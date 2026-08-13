#include "CQTest.h"

#include "AngelscriptTestEngineAcquisition.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestJITProbes.h"
#include "AngelscriptTestMacros.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASFunction.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AngelscriptJITBindingContext.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptJITReferenceResolver.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/StaticJITDiagnostics.h"
#include "StaticJIT/StaticJITHeader.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_context.h"
#include "source/as_objecttype.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

struct FAngelscriptStaticJITAotUASFunctionDispatchTests;
struct FAngelscriptStaticJITAotMultiEngineTests;

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotTests,
	"Angelscript.TestModule.StaticJIT.AOT",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	friend struct FAngelscriptStaticJITAotUASFunctionDispatchTests;
	friend struct FAngelscriptStaticJITAotMultiEngineTests;

	struct FPrimitiveArgParams
	{
		int32 Value = 0;
	};

	struct FTwoPrimitiveParams
	{
		int32 A = 0;
		int32 B = 0;
		int32 ReturnValue = 0;
	};

	struct FReferenceParams
	{
		int32 Value = 0;
		int32 ReturnValue = 0;
	};

	struct FStaticWorldContextParams
	{
		UObject* WorldContextObject = nullptr;
		int32 Value = 0;
		int32 ReturnValue = 0;
	};

	struct FDoubleInt64ConversionResults
	{
		int64 SignedValue = 0;
		uint64 UnsignedValue = 0;
	};

	inline static TUniquePtr<AngelscriptStaticJITAotGeneration::
		FStaticJITAotFixtureEngineSession> SharedFixtureSession;

	static void DiscardActiveModules(FAngelscriptEngine& Engine)
	{
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
		{
			Engine.DiscardModule(*Module->ModuleName);
		}
	}

	static bool FunctionContainsOpcode(asCScriptFunction& Function, const asEBCInstr ExpectedOpcode)
	{
		asUINT BytecodeLength = 0;
		const asDWORD* Bytecode = Function.GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (Opcode == ExpectedOpcode)
			{
				return true;
			}

			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				break;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0)
			{
				break;
			}

			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return false;
	}

	static bool ExecuteDoubleArgumentFunction(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		asCScriptFunction& Function,
		const double Input,
		const TCHAR* Label,
		asQWORD& OutRawResult)
	{
		FNoDiscardAsserter LocalAssert(Test);
		asIScriptContext* Context = ScriptEngine.CreateContext();
		if (!LocalAssert.IsNotNull(Context, *FString::Printf(TEXT("%s should create an execution context"), Label)))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		if (!LocalAssert.AreEqual(
				static_cast<int32>(asSUCCESS),
				Context->Prepare(&Function),
				*FString::Printf(TEXT("%s should prepare the conversion function"), Label))
			|| !LocalAssert.AreEqual(
				static_cast<int32>(asSUCCESS),
				Context->SetArgDouble(0, Input),
				*FString::Printf(TEXT("%s should bind the runtime double argument"), Label))
			|| !LocalAssert.AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*FString::Printf(TEXT("%s should finish conversion execution"), Label)))
		{
			return false;
		}

		OutRawResult = Context->GetReturnQWord();
		return true;
	}

	static asCScriptFunction* FindEntryFunction(FAngelscriptEngine& Engine)
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(AngelscriptStaticJITAotFixture::GetModuleName().ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			return nullptr;
		}

		FTCHARToUTF8 EntryDecl(*AngelscriptStaticJITAotFixture::GetEntryDeclaration());
		return static_cast<asCScriptFunction*>(ModuleDesc->ScriptModule->GetFunctionByDecl(EntryDecl.Get()));
	}

	static FString GetFunctionNameFromDeclaration(const FString& Declaration)
	{
		int32 OpenParenIndex = INDEX_NONE;
		if (!Declaration.FindChar(TEXT('('), OpenParenIndex))
		{
			return FString();
		}

		const FString Prefix = Declaration.Left(OpenParenIndex).TrimStartAndEnd();
		int32 NameSeparatorIndex = INDEX_NONE;
		if (!Prefix.FindLastChar(TEXT(' '), NameSeparatorIndex))
		{
			return Prefix;
		}

		return Prefix.Mid(NameSeparatorIndex + 1).TrimStartAndEnd();
	}

	static FString GetAvailableMethodDeclarations(asITypeInfo& TypeInfo)
	{
		FString AvailableMethods;
		const asUINT MethodCount = TypeInfo.GetMethodCount();
		for (asUINT MethodIndex = 0; MethodIndex < MethodCount; ++MethodIndex)
		{
			asIScriptFunction* CandidateFunction = TypeInfo.GetMethodByIndex(MethodIndex);
			if (CandidateFunction == nullptr)
			{
				continue;
			}

			if (!AvailableMethods.IsEmpty())
			{
				AvailableMethods += TEXT(", ");
			}

			AvailableMethods += UTF8_TO_TCHAR(CandidateFunction->GetDeclaration());
		}

		return AvailableMethods.IsEmpty() ? TEXT("<none>") : AvailableMethods;
	}

	static asCScriptFunction* FindMethodFunction(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const FString& Declaration)
	{
		UASClass* GeneratedClass = Cast<UASClass>(FindGeneratedClass(&Engine, AngelscriptStaticJITAotFixture::GetGeneratedClassName()));
		if (GeneratedClass == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT should resolve generated class '%s' before looking up method '%s'."),
				*AngelscriptStaticJITAotFixture::GetGeneratedClassName().ToString(),
				*Declaration));
			return nullptr;
		}

		asITypeInfo* TypeInfo = static_cast<asITypeInfo*>(GeneratedClass->ScriptTypePtr);
		if (TypeInfo == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT generated class '%s' has no AngelScript type before looking up method '%s'."),
				*GeneratedClass->GetName(),
				*Declaration));
			return nullptr;
		}

		FTCHARToUTF8 MethodDecl(*Declaration);
		asIScriptFunction* Function = TypeInfo->GetMethodByDecl(MethodDecl.Get());
		if (Function == nullptr)
		{
			const FString MethodName = GetFunctionNameFromDeclaration(Declaration);
			if (!MethodName.IsEmpty())
			{
				FTCHARToUTF8 MethodNameUtf8(*MethodName);
				const asUINT MethodCount = TypeInfo->GetMethodCount();
				for (asUINT MethodIndex = 0; MethodIndex < MethodCount; ++MethodIndex)
				{
					asIScriptFunction* CandidateFunction = TypeInfo->GetMethodByIndex(MethodIndex);
					if (CandidateFunction != nullptr && FCStringAnsi::Strcmp(CandidateFunction->GetName(), MethodNameUtf8.Get()) == 0)
					{
						Function = CandidateFunction;
						break;
					}
				}
			}
		}

		if (Function == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT should resolve method '%s'; available methods on '%s': %s"),
				*Declaration,
				UTF8_TO_TCHAR(TypeInfo->GetName()),
				*GetAvailableMethodDeclarations(*TypeInfo)));
		}

		return static_cast<asCScriptFunction*>(Function);
	}

	static FString GetAvailableGlobalFunctionDeclarations(asIScriptModule& Module)
	{
		FString AvailableFunctions;
		const asUINT FunctionCount = Module.GetFunctionCount();
		for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
		{
			asIScriptFunction* CandidateFunction = Module.GetFunctionByIndex(FunctionIndex);
			if (CandidateFunction == nullptr)
			{
				continue;
			}

			if (!AvailableFunctions.IsEmpty())
			{
				AvailableFunctions += TEXT(", ");
			}

			AvailableFunctions += UTF8_TO_TCHAR(CandidateFunction->GetDeclaration());
		}

		return AvailableFunctions.IsEmpty() ? TEXT("<none>") : AvailableFunctions;
	}

	static asCScriptFunction* FindGlobalFunction(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const FString& Declaration)
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(AngelscriptStaticJITAotFixture::GetModuleName().ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT should resolve module '%s' before looking up global function '%s'."),
				*AngelscriptStaticJITAotFixture::GetModuleName().ToString(),
				*Declaration));
			return nullptr;
		}

		FTCHARToUTF8 FunctionDecl(*Declaration);
		asIScriptFunction* Function = ModuleDesc->ScriptModule->GetFunctionByDecl(FunctionDecl.Get());
		if (Function == nullptr)
		{
			const FString FunctionName = GetFunctionNameFromDeclaration(Declaration);
			if (!FunctionName.IsEmpty())
			{
				FTCHARToUTF8 FunctionNameUtf8(*FunctionName);
				const asUINT FunctionCount = ModuleDesc->ScriptModule->GetFunctionCount();
				for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
				{
					asIScriptFunction* CandidateFunction = ModuleDesc->ScriptModule->GetFunctionByIndex(FunctionIndex);
					if (CandidateFunction != nullptr && FCStringAnsi::Strcmp(CandidateFunction->GetName(), FunctionNameUtf8.Get()) == 0)
					{
						Function = CandidateFunction;
						break;
					}
				}
			}
		}

		if (Function == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT should resolve global function '%s'; available functions in module '%s': %s"),
				*Declaration,
				*AngelscriptStaticJITAotFixture::GetModuleName().ToString(),
				*GetAvailableGlobalFunctionDeclarations(*ModuleDesc->ScriptModule)));
		}

		return static_cast<asCScriptFunction*>(Function);
	}

	static bool RequireJitEntries(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asCScriptFunction* Function, const TCHAR* Label, uint32& OutFunctionId)
	{
		if (!Test.TestNotNull(*FString::Printf(TEXT("StaticJIT.AOT %s should resolve a script function"), Label), Function))
		{
			return false;
		}

		OutFunctionId = static_cast<uint32>(Function->GetId());
		FAngelscriptStableFunctionKey FunctionKey;
		if (!Test.TestTrue(
				*FString::Printf(
					TEXT("StaticJIT.AOT %s should expose a stable function key"),
					Label),
				Engine.TryResolveStableFunctionKey(Function, FunctionKey)))
		{
			return false;
		}

		const asSJITFunctionBinding Binding = Function->GetJITBinding();
		FAngelscriptFunctionRoute Route;
		return Test.TestTrue(
				*FString::Printf(
					TEXT("StaticJIT.AOT %s should resolve its current route"), Label),
				Engine.ResolveFunctionRoute(FunctionKey, Route))
			&& Test.TestTrue(
				*FString::Printf(
					TEXT("StaticJIT.AOT %s should select generated Native code"), Label),
				Route.IsNative())
			&& Test.TestNotNull(*FString::Printf(TEXT("StaticJIT.AOT %s should attach VM entry"), Label), Binding.VMEntry)
			&& Test.TestNotNull(*FString::Printf(TEXT("StaticJIT.AOT %s should attach Raw entry"), Label), Binding.RawEntry)
			&& Test.TestNotNull(*FString::Printf(TEXT("StaticJIT.AOT %s should attach Parms entry"), Label), Binding.ParmsEntry)
			&& Test.TestNotNull(
				*FString::Printf(
					TEXT("StaticJIT.AOT %s should retain current binding context"), Label),
				FAngelscriptJITBindingContext::FromBindingUserData(
					Binding.UserData));
	}

	static bool RequireFunctionClass(
		FAutomationTestBase& Test,
		UClass* OwnerClass,
		const FName FunctionName,
		UClass* ExpectedFunctionClass,
		const TCHAR* Label)
	{
		UASFunction* const Function = Cast<UASFunction>(FindGeneratedFunction(
			OwnerClass,
			FunctionName));
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("StaticJIT.AOT %s should expose reflected function '%s'"), Label, *FunctionName.ToString()),
				Function))
		{
			return false;
		}

		return Test.TestTrue(
			*FString::Printf(
				TEXT("StaticJIT.AOT %s should select %s (actual: %s)"),
				Label,
				*GetNameSafe(ExpectedFunctionClass),
				*GetNameSafe(Function->GetClass())),
			Function->GetClass() == ExpectedFunctionClass);
	}

	static FAngelscriptEngine* GetSharedAotFixtureEngine(
			FAutomationTestBase& Test)
	{
		if (!Test.TestNotNull(
				TEXT("StaticJIT.AOT test section should own one shared isolated source fixture session"),
				SharedFixtureSession.Get()))
		{
			return nullptr;
		}
		if (!Test.TestTrue(
				TEXT("StaticJIT.AOT shared source fixture session should compile and publish current Provider routes"),
				SharedFixtureSession->IsValid()))
		{
			Test.AddError(SharedFixtureSession->Error);
			return nullptr;
		}
		return SharedFixtureSession->Engine.Get();
	}

	static bool RunGeneratedOutputVerify(FAutomationTestBase& Test);
	static bool RunRuntimeRegistration(FAutomationTestBase& Test);
	static bool RunRuntimeExecution(FAutomationTestBase& Test);
	static bool RunNestedExceptionPropagation(FAutomationTestBase& Test);
	static bool RunNestedReferenceLifetimeAndRecursion(FAutomationTestBase& Test);
	static bool RunCurrentVirtualOverride(FAutomationTestBase& Test);
	static bool RunUASFunctionJitEntryAttachment(FAutomationTestBase& Test);
	static bool RunUASFunctionRuntimeCallEvent(FAutomationTestBase& Test);
	static bool RunStaticWorldContextRuntimeCallEvent(FAutomationTestBase& Test);
	static bool RunMultiEngineSequentialLoad(FAutomationTestBase& Test);

public:
	BEFORE_ALL()
	{
		SharedFixtureSession =
			AngelscriptStaticJITAotGeneration::CreateFixtureEngineSession(false);
	}

	AFTER_ALL()
	{
		SharedFixtureSession.Reset();
	}

	TEST_METHOD(GeneratedOutputVerify)
	{
		ASSERT_THAT(IsTrue(RunGeneratedOutputVerify(*TestRunner)));
	}

	TEST_METHOD(RuntimeRegistersGeneratedFunction)
	{
		ASSERT_THAT(IsTrue(RunRuntimeRegistration(*TestRunner)));
	}

	TEST_METHOD(CurrentSourceEngineResolvesEveryCommittedProviderReferenceSlot)
	{
		FAngelscriptEngine* const SourceEngine =
			GetSharedAotFixtureEngine(*TestRunner);
		ASSERT_THAT(IsNotNull(SourceEngine));
		if (SourceEngine == nullptr)
		{
			return;
		}

		FAngelscriptEngineScope SourceScope(*SourceEngine);

		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Providers =
			FAngelscriptJITProviderRegistry::Get().GetSnapshot();
		ASSERT_THAT(IsTrue(Providers.IsValid()));
		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>* const Provider = Providers.IsValid()
			? Providers->Providers.FindByPredicate(
				[](const TSharedPtr<const FAngelscriptJITProviderCatalog,
					ESPMode::ThreadSafe>& Candidate)
				{
					return Candidate.IsValid()
						&& Candidate->ProviderId
							== AngelscriptTestJITProbes::GetStableProviderId();
				})
			: nullptr;
		ASSERT_THAT(IsNotNull(Provider));
		if (Provider == nullptr || !Provider->IsValid())
		{
			return;
		}

		TArray<FAngelscriptArtifactReference> RequiredReferences;
		for (const FAngelscriptJITProviderCatalogEntry& Entry
			: (*Provider)->Entries)
		{
			for (const FAngelscriptJITReferenceSlot& Slot : Entry.ReferenceSlots)
			{
				RequiredReferences.AddUnique(Slot.Reference);
			}
		}
		const FAngelscriptJITCurrentEngineReferenceBuildResult BuildResult =
			FAngelscriptJITCurrentEngineReferenceResolverBuilder::Build(
				*SourceEngine, RequiredReferences);
		TestRunner->AddInfo(BuildResult.Detail);
		ASSERT_THAT(IsTrue(BuildResult.IsUsable(), BuildResult.Detail));
		ASSERT_THAT(AreEqual(RequiredReferences.Num(),
			static_cast<int32>(BuildResult.ResolvedReferenceCount),
			BuildResult.Detail));
		if (!BuildResult.IsUsable())
		{
			return;
		}

		for (const FAngelscriptJITProviderCatalogEntry& Entry
			: (*Provider)->Entries)
		{
			const FAngelscriptJITReferenceTableBuildResult TableResult =
				FAngelscriptJITResolvedReferenceTableBuilder::Build(
					Entry.ReferenceSlots, *BuildResult.Resolver);
			ASSERT_THAT(IsTrue(TableResult.IsExact(),
				FString::Printf(
					TEXT("Provider entry %s failed at slot %d with result %u"),
					*Entry.FunctionKey.Hash.ToHexString(),
					TableResult.FailedSlotIndex,
					static_cast<uint32>(TableResult.Result))));
		}
	}

	TEST_METHOD(CurrentSourceEnginePublishesAndExecutesCommittedProviderRoute)
	{
		FAngelscriptEngine* const SourceEnginePtr =
			GetSharedAotFixtureEngine(*TestRunner);
		ASSERT_THAT(IsNotNull(SourceEnginePtr));
		if (SourceEnginePtr == nullptr)
		{
			return;
		}

		FAngelscriptEngine& SourceEngine = *SourceEnginePtr;
		FAngelscriptEngineScope SourceScope(SourceEngine);

		const TSharedPtr<FAngelscriptModuleDesc> Module =
			SourceEngine.GetModuleByModuleName(
				AngelscriptStaticJITAotFixture::
					GetImportProviderModuleName().ToString());
		ASSERT_THAT(IsTrue(Module.IsValid()
			&& Module->ScriptModule != nullptr));
		if (!Module.IsValid() || Module->ScriptModule == nullptr)
		{
			return;
		}

		asIScriptFunction* const Function =
			Module->ScriptModule->GetFunctionByDecl(
				"int IndependentModuleEntryForAOT()");
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		const asSJITFunctionBinding Binding = Function->GetJITBinding();
		ASSERT_THAT(IsNotNull(Binding.VMEntry));
		ASSERT_THAT(IsNotNull(Binding.RawEntry));
		ASSERT_THAT(IsNotNull(
			FAngelscriptJITBindingContext::FromBindingUserData(
				Binding.UserData)));

		FAngelscriptFunctionRoute Route;
		FAngelscriptStableFunctionKey FunctionKey;
		ASSERT_THAT(IsTrue(SourceEngine.TryResolveStableFunctionKey(
			Function, FunctionKey)));
		ASSERT_THAT(IsTrue(SourceEngine.ResolveFunctionRoute(
			FunctionKey, Route)));
		ASSERT_THAT(IsTrue(Route.IsNative()));

		asIScriptContext* const Context =
			SourceEngine.GetScriptEngine()->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute()));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::GetExpectedImportResult(),
			static_cast<int32>(Context->GetReturnDWord())));
	}

	TEST_METHOD(FreshCacheV2EnginePublishesAndExecutesCommittedProviderRoute)
	{
		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> Session =
			AngelscriptStaticJITAotGeneration::CreateFixtureEngineSession();
		ASSERT_THAT(IsTrue(Session.IsValid()));
		ASSERT_THAT(IsTrue(
			Session.IsValid() && Session->IsValid(),
			Session.IsValid()
				? Session->Error
				: TEXT("StaticJIT Cache V2 proof could not allocate its isolated source session.")));
		if (!Session.IsValid() || !Session->IsValid())
		{
			return;
		}

		FAngelscriptStableFunctionKey SourceFunctionKey;
		{
			FAngelscriptEngineScope SourceScope(*Session->Engine);
			const TSharedPtr<FAngelscriptModuleDesc> SourceModule =
				Session->Engine->GetModuleByModuleName(
					AngelscriptStaticJITAotFixture::
						GetImportProviderModuleName().ToString());
			asIScriptFunction* const SourceFunction =
				SourceModule.IsValid() && SourceModule->ScriptModule != nullptr
					? SourceModule->ScriptModule->GetFunctionByDecl(
						"int IndependentModuleEntryForAOT()")
					: nullptr;
			ASSERT_THAT(IsNotNull(SourceFunction));
			ASSERT_THAT(IsTrue(SourceFunction != nullptr
				&& Session->Engine->TryResolveStableFunctionKey(
					SourceFunction, SourceFunctionKey)));
		}

		const AngelscriptStaticJITAotGeneration::
			FStaticJITAotCacheV2RestoreResult Restore =
				Session->RestoreFreshEngineFromCacheV2();
		TestRunner->AddInfo(Restore.Detail);
		ASSERT_THAT(IsTrue(Restore.bSuccess, Restore.Detail));
		ASSERT_THAT(IsTrue(
			Restore.bConsumedPersistedCandidate, Restore.Detail));
		ASSERT_THAT(AreEqual(2, Restore.CandidateModuleCount, Restore.Detail));
		ASSERT_THAT(IsTrue(Restore.RestoredFunctionCount > 0, Restore.Detail));
		if (!Restore.bSuccess || !Session->IsValid())
		{
			return;
		}

		FAngelscriptEngine& RestoredEngine = *Session->Engine;
		FAngelscriptEngineScope RestoredScope(RestoredEngine);
		ON_SCOPE_EXIT
		{
			DiscardActiveModules(RestoredEngine);
		};

		const TSharedPtr<FAngelscriptModuleDesc> RestoredModule =
			RestoredEngine.GetModuleByModuleName(
				AngelscriptStaticJITAotFixture::
					GetImportProviderModuleName().ToString());
		ASSERT_THAT(IsTrue(RestoredModule.IsValid()
			&& RestoredModule->ScriptModule != nullptr));
		if (!RestoredModule.IsValid()
			|| RestoredModule->ScriptModule == nullptr)
		{
			return;
		}

		asIScriptFunction* const RestoredFunction =
			RestoredModule->ScriptModule->GetFunctionByDecl(
				"int IndependentModuleEntryForAOT()");
		ASSERT_THAT(IsNotNull(RestoredFunction));
		if (RestoredFunction == nullptr)
		{
			return;
		}

		FAngelscriptStableFunctionKey RestoredFunctionKey;
		ASSERT_THAT(IsTrue(RestoredEngine.TryResolveStableFunctionKey(
			RestoredFunction, RestoredFunctionKey)));
		ASSERT_THAT(IsTrue(RestoredFunctionKey == SourceFunctionKey));

		const asSJITFunctionBinding Binding =
			RestoredFunction->GetJITBinding();
		ASSERT_THAT(IsNotNull(Binding.VMEntry));
		ASSERT_THAT(IsNotNull(Binding.RawEntry));
		ASSERT_THAT(IsNotNull(
			FAngelscriptJITBindingContext::FromBindingUserData(
				Binding.UserData)));

		FAngelscriptFunctionRoute Route;
		ASSERT_THAT(IsTrue(RestoredEngine.ResolveFunctionRoute(
			RestoredFunctionKey, Route)));
		ASSERT_THAT(IsTrue(Route.IsNative()));

		asIScriptContext* const Context =
			RestoredEngine.GetScriptEngine()->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Context->Prepare(RestoredFunction)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute()));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::GetExpectedImportResult(),
			static_cast<int32>(Context->GetReturnDWord())));
	}

	TEST_METHOD(RuntimeExecuteUsesGeneratedEntry)
	{
		ASSERT_THAT(IsTrue(RunRuntimeExecution(*TestRunner)));
	}

	TEST_METHOD(NestedGeneratedExceptionPropagates)
	{
		ASSERT_THAT(IsTrue(RunNestedExceptionPropagation(*TestRunner)));
	}

	TEST_METHOD(NestedReferenceLifetimeAndRecursionMatchInterpreter)
	{
		ASSERT_THAT(IsTrue(RunNestedReferenceLifetimeAndRecursion(*TestRunner)));
	}

	TEST_METHOD(CurrentVirtualOverrideUsesGeneratedRoute)
	{
		ASSERT_THAT(IsTrue(RunCurrentVirtualOverride(*TestRunner)));
	}

	TEST_METHOD(DoubleInt64ConversionsMatchInterpreter)
	{
		constexpr double SignedInput = -4294967296.75;
		constexpr double UnsignedInput = 4294967296.75;
		constexpr int64 ExpectedSigned = -4294967296LL;
		constexpr uint64 ExpectedUnsigned = 4294967296ULL;
		FDoubleInt64ConversionResults InterpreterResults;

		TestRunner->AddInfo(FString::Printf(
			TEXT("[AS-STATICJIT-AOT-SOURCE-BEGIN][DoubleInt64ConversionsMatchInterpreter]\n%s[AS-STATICJIT-AOT-SOURCE-END][DoubleInt64ConversionsMatchInterpreter]"),
			*AngelscriptStaticJITAotFixture::GetScriptSource()));

		{
			TUniquePtr<FAngelscriptEngine> InterpreterEngine = CreateIsolatedFullEngine();
			ASSERT_THAT(IsNotNull(InterpreterEngine.Get(), TEXT("StaticJIT.AOT parity should create an isolated interpreter engine")));
			FAngelscriptEngineScope InterpreterScope(*InterpreterEngine);
			ON_SCOPE_EXIT
			{
				DiscardActiveModules(*InterpreterEngine);
			};
			ASSERT_THAT(IsTrue(
				AngelscriptStaticJITAotFixture::RegisterObjectLastNativeSurface(
					*InterpreterEngine->GetScriptEngine()),
				TEXT("StaticJIT.AOT parity should register the fixture object-last native surface")));
			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(
					InterpreterEngine.Get(),
					AngelscriptStaticJITAotFixture::GetImportProviderModuleName(),
					AngelscriptStaticJITAotFixture::GetImportProviderSourceFilename(),
					AngelscriptStaticJITAotFixture::GetImportProviderScriptSource()),
				TEXT("StaticJIT.AOT parity should compile the import provider for interpreter execution")));

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(
					InterpreterEngine.Get(),
					AngelscriptStaticJITAotFixture::GetModuleName(),
					AngelscriptStaticJITAotFixture::GetSourceFilename(),
					AngelscriptStaticJITAotFixture::GetScriptSource()),
				TEXT("StaticJIT.AOT parity should compile the fixture for interpreter execution")));

			asCScriptFunction* SignedFunction = FindGlobalFunction(
				*TestRunner,
				*InterpreterEngine,
				AngelscriptStaticJITAotFixture::GetDoubleToInt64Declaration());
			asCScriptFunction* UnsignedFunction = FindGlobalFunction(
				*TestRunner,
				*InterpreterEngine,
				AngelscriptStaticJITAotFixture::GetDoubleToUint64Declaration());
			ASSERT_THAT(IsNotNull(SignedFunction, TEXT("Interpreter should resolve the signed double conversion")));
			ASSERT_THAT(IsNotNull(UnsignedFunction, TEXT("Interpreter should resolve the unsigned double conversion")));

			const int32 DoubleTypeId = InterpreterEngine->GetScriptEngine()->GetTypeIdByDecl("double");
			int32 SignedParameterTypeId = asINVALID_TYPE;
			int32 UnsignedParameterTypeId = asINVALID_TYPE;
			ASSERT_THAT(AreEqual(static_cast<asUINT>(1), SignedFunction->GetParamCount(), TEXT("Signed interpreter conversion should expose one parameter")));
			ASSERT_THAT(AreEqual(static_cast<asUINT>(1), UnsignedFunction->GetParamCount(), TEXT("Unsigned interpreter conversion should expose one parameter")));
			ASSERT_THAT(AreEqual(asSUCCESS, SignedFunction->GetParam(0, &SignedParameterTypeId), TEXT("Signed interpreter parameter metadata should be readable")));
			ASSERT_THAT(AreEqual(asSUCCESS, UnsignedFunction->GetParam(0, &UnsignedParameterTypeId), TEXT("Unsigned interpreter parameter metadata should be readable")));
			ASSERT_THAT(AreEqual(DoubleTypeId, SignedParameterTypeId, TEXT("Signed interpreter conversion should accept double")));
			ASSERT_THAT(AreEqual(DoubleTypeId, UnsignedParameterTypeId, TEXT("Unsigned interpreter conversion should accept double")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asTYPEID_INT64), SignedFunction->GetReturnTypeId(), TEXT("Signed interpreter conversion should return int64")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asTYPEID_UINT64), UnsignedFunction->GetReturnTypeId(), TEXT("Unsigned interpreter conversion should return uint64")));

			const asSJITFunctionBinding SignedBinding = SignedFunction->GetJITBinding();
			const asSJITFunctionBinding UnsignedBinding = UnsignedFunction->GetJITBinding();
			ASSERT_THAT(IsNull(SignedBinding.VMEntry, TEXT("Signed interpreter conversion should not attach a VM entry")));
			ASSERT_THAT(IsNull(SignedBinding.RawEntry, TEXT("Signed interpreter conversion should not attach a Raw entry")));
			ASSERT_THAT(IsNull(SignedBinding.ParmsEntry, TEXT("Signed interpreter conversion should not attach a Parms entry")));
			ASSERT_THAT(IsNull(UnsignedBinding.VMEntry, TEXT("Unsigned interpreter conversion should not attach a VM entry")));
			ASSERT_THAT(IsNull(UnsignedBinding.RawEntry, TEXT("Unsigned interpreter conversion should not attach a Raw entry")));
			ASSERT_THAT(IsNull(UnsignedBinding.ParmsEntry, TEXT("Unsigned interpreter conversion should not attach a Parms entry")));
			ASSERT_THAT(IsTrue(FunctionContainsOpcode(*SignedFunction, asBC_dTOi64), TEXT("Signed interpreter conversion should retain asBC_dTOi64 bytecode")));
			ASSERT_THAT(IsTrue(FunctionContainsOpcode(*UnsignedFunction, asBC_dTOu64), TEXT("Unsigned interpreter conversion should retain asBC_dTOu64 bytecode")));

			asQWORD SignedRawResult = 0;
			asQWORD UnsignedRawResult = 0;
			ASSERT_THAT(IsTrue(ExecuteDoubleArgumentFunction(
				*TestRunner,
				*InterpreterEngine->GetScriptEngine(),
				*SignedFunction,
				SignedInput,
				TEXT("Signed interpreter conversion"),
				SignedRawResult)));
			ASSERT_THAT(IsTrue(ExecuteDoubleArgumentFunction(
				*TestRunner,
				*InterpreterEngine->GetScriptEngine(),
				*UnsignedFunction,
				UnsignedInput,
				TEXT("Unsigned interpreter conversion"),
				UnsignedRawResult)));
			FMemory::Memcpy(&InterpreterResults.SignedValue, &SignedRawResult, sizeof(InterpreterResults.SignedValue));
			InterpreterResults.UnsignedValue = static_cast<uint64>(UnsignedRawResult);
			ASSERT_THAT(AreEqual(ExpectedSigned, InterpreterResults.SignedValue, TEXT("Interpreter signed conversion should truncate toward zero exactly")));
			ASSERT_THAT(AreEqual(ExpectedUnsigned, InterpreterResults.UnsignedValue, TEXT("Interpreter unsigned conversion should preserve the 64-bit value exactly")));
			DiscardActiveModules(*InterpreterEngine);
			ASSERT_THAT(AreEqual(0, InterpreterEngine->GetActiveModules().Num(), TEXT("Interpreter parity phase should discard every active module")));
		}

		{
			FAngelscriptEngine* const AotEngine =
				GetSharedAotFixtureEngine(*TestRunner);
			ASSERT_THAT(IsNotNull(AotEngine));
			if (AotEngine == nullptr)
			{
				return;
			}
			FAngelscriptEngineScope AotScope(*AotEngine);

			asCScriptFunction* SignedFunction = FindGlobalFunction(
				*TestRunner,
				*AotEngine,
				AngelscriptStaticJITAotFixture::GetDoubleToInt64Declaration());
			asCScriptFunction* UnsignedFunction = FindGlobalFunction(
				*TestRunner,
				*AotEngine,
				AngelscriptStaticJITAotFixture::GetDoubleToUint64Declaration());
			ASSERT_THAT(IsNotNull(SignedFunction, TEXT("Loaded AOT should resolve the signed double conversion")));
			ASSERT_THAT(IsNotNull(UnsignedFunction, TEXT("Loaded AOT should resolve the unsigned double conversion")));

			const int32 DoubleTypeId = AotEngine->GetScriptEngine()->GetTypeIdByDecl("double");
			int32 SignedParameterTypeId = asINVALID_TYPE;
			int32 UnsignedParameterTypeId = asINVALID_TYPE;
			ASSERT_THAT(AreEqual(static_cast<asUINT>(1), SignedFunction->GetParamCount(), TEXT("Signed loaded-AOT conversion should expose one parameter")));
			ASSERT_THAT(AreEqual(static_cast<asUINT>(1), UnsignedFunction->GetParamCount(), TEXT("Unsigned loaded-AOT conversion should expose one parameter")));
			ASSERT_THAT(AreEqual(asSUCCESS, SignedFunction->GetParam(0, &SignedParameterTypeId), TEXT("Signed loaded-AOT parameter metadata should be readable")));
			ASSERT_THAT(AreEqual(asSUCCESS, UnsignedFunction->GetParam(0, &UnsignedParameterTypeId), TEXT("Unsigned loaded-AOT parameter metadata should be readable")));
			ASSERT_THAT(AreEqual(DoubleTypeId, SignedParameterTypeId, TEXT("Signed loaded-AOT conversion should accept double")));
			ASSERT_THAT(AreEqual(DoubleTypeId, UnsignedParameterTypeId, TEXT("Unsigned loaded-AOT conversion should accept double")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asTYPEID_INT64), SignedFunction->GetReturnTypeId(), TEXT("Signed loaded-AOT conversion should return int64")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asTYPEID_UINT64), UnsignedFunction->GetReturnTypeId(), TEXT("Unsigned loaded-AOT conversion should return uint64")));

			uint32 SignedFunctionId = 0;
			uint32 UnsignedFunctionId = 0;
			ASSERT_THAT(IsTrue(RequireJitEntries(
				*TestRunner,
				*AotEngine,
				SignedFunction,
				TEXT("signed double-to-int64 conversion"),
				SignedFunctionId)));
			ASSERT_THAT(IsTrue(RequireJitEntries(
				*TestRunner,
				*AotEngine,
				UnsignedFunction,
				TEXT("unsigned double-to-uint64 conversion"),
				UnsignedFunctionId)));

			asQWORD SignedRawResult = 0;
			asQWORD UnsignedRawResult = 0;
			ASSERT_THAT(IsTrue(ExecuteDoubleArgumentFunction(
				*TestRunner,
				*AotEngine->GetScriptEngine(),
				*SignedFunction,
				SignedInput,
				TEXT("Signed loaded-AOT conversion"),
				SignedRawResult)));
			ASSERT_THAT(IsTrue(ExecuteDoubleArgumentFunction(
				*TestRunner,
				*AotEngine->GetScriptEngine(),
				*UnsignedFunction,
				UnsignedInput,
				TEXT("Unsigned loaded-AOT conversion"),
				UnsignedRawResult)));

			FDoubleInt64ConversionResults AotResults;
			FMemory::Memcpy(&AotResults.SignedValue, &SignedRawResult, sizeof(AotResults.SignedValue));
			AotResults.UnsignedValue = static_cast<uint64>(UnsignedRawResult);
			ASSERT_THAT(AreEqual(ExpectedSigned, AotResults.SignedValue, TEXT("Loaded AOT signed conversion should truncate toward zero exactly")));
			ASSERT_THAT(AreEqual(ExpectedUnsigned, AotResults.UnsignedValue, TEXT("Loaded AOT unsigned conversion should preserve the 64-bit value exactly")));
			ASSERT_THAT(AreEqual(InterpreterResults.SignedValue, AotResults.SignedValue, TEXT("Loaded AOT signed conversion should match the interpreter exactly")));
			ASSERT_THAT(AreEqual(InterpreterResults.UnsignedValue, AotResults.UnsignedValue, TEXT("Loaded AOT unsigned conversion should match the interpreter exactly")));
		}
	}

	TEST_METHOD(ObjectLastNativeConstructorUsesGeneratedEntry)
	{
		TestRunner->AddInfo(FString::Printf(
			TEXT("[AS-STATICJIT-AOT-SOURCE-BEGIN][ObjectLastNativeConstructorUsesGeneratedEntry]\n%s[AS-STATICJIT-AOT-SOURCE-END][ObjectLastNativeConstructorUsesGeneratedEntry]"),
			*AngelscriptStaticJITAotFixture::GetScriptSource()));

		FAngelscriptEngine* const AotEngine =
			GetSharedAotFixtureEngine(*TestRunner);
		ASSERT_THAT(IsNotNull(AotEngine));
		if (AotEngine == nullptr)
		{
			return;
		}
		FAngelscriptEngineScope AotScope(*AotEngine);

		asCScriptFunction* const Function = FindGlobalFunction(
			*TestRunner,
			*AotEngine,
			AngelscriptStaticJITAotFixture::GetObjectLastNativeEntryDeclaration());
		ASSERT_THAT(IsNotNull(
			Function,
			TEXT("StaticJIT.AOT object-last test should resolve its exact generated function")));
		if (Function == nullptr)
		{
			return;
		}

		uint32 FunctionId = 0;
		ASSERT_THAT(IsTrue(
			RequireJitEntries(
				*TestRunner,
				*AotEngine,
				Function,
				TEXT("object-last native constructor"),
				FunctionId),
			TEXT("StaticJIT.AOT object-last function should expose every generated entry")));

		AngelscriptStaticJITAotFixture::ResetObjectLastNativeObservation();
		asIScriptContext* const Context =
			AotEngine->GetScriptEngine()->CreateContext();
		ASSERT_THAT(IsNotNull(
			Context,
			TEXT("StaticJIT.AOT object-last test should create an execution context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Context->Prepare(Function),
			TEXT("StaticJIT.AOT object-last test should prepare its generated function")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			TEXT("StaticJIT.AOT object-last test should execute generated code")));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::GetExpectedObjectLastNativeResult(),
			static_cast<int32>(Context->GetReturnDWord()),
			TEXT("StaticJIT.AOT object-last test should preserve the constructed object result")));
		ASSERT_THAT(AreEqual(
			1,
			AngelscriptStaticJITAotFixture::GetObjectLastNativeCallCount(),
			TEXT("StaticJIT.AOT object-last native constructor should execute exactly once")));
		ASSERT_THAT(AreEqual(
			39,
			AngelscriptStaticJITAotFixture::GetObjectLastNativeLeftSentinel(),
			TEXT("StaticJIT.AOT object-last constructor should receive the first explicit argument")));
		ASSERT_THAT(AreEqual(
			97,
			AngelscriptStaticJITAotFixture::GetObjectLastNativeRightSentinel(),
			TEXT("StaticJIT.AOT object-last constructor should receive the final explicit argument")));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::GetExpectedObjectLastNativeResult(),
			AngelscriptStaticJITAotFixture::GetObjectLastNativeObjectValue(),
			TEXT("StaticJIT.AOT object-last constructor should receive destination storage after explicit arguments")));
	}
};

bool FAngelscriptStaticJITAotTests::RunGeneratedOutputVerify(FAutomationTestBase& Test)
{
	using namespace AngelscriptStaticJITAotGeneration;
	const FStaticJITAotGenerationResult Result = Run(EStaticJITAotGenerationMode::Verify);
	if (!Test.TestTrue(TEXT("StaticJIT.AOT generated output should match the fixture"), Result.bSuccess))
	{
		Test.AddError(Result.Error);
	}
	return Result.bSuccess;
}

bool FAngelscriptStaticJITAotTests::RunRuntimeRegistration(FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);

	asCScriptFunction* EntryFunction = FindEntryFunction(Engine);
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT should resolve the fixture entry function"), EntryFunction))
	{
		return false;
	}

	uint32 TransientFunctionId = 0;
	return RequireJitEntries(
		Test, Engine, EntryFunction, TEXT("fixture entry"), TransientFunctionId);
}

bool FAngelscriptStaticJITAotTests::RunRuntimeExecution(FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);

	asCScriptFunction* EntryFunction = FindEntryFunction(Engine);
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT should resolve the fixture entry function before execution"), EntryFunction))
	{
		return false;
	}

	uint32 TransientFunctionId = 0;
	if (!RequireJitEntries(
			Test, Engine, EntryFunction, TEXT("fixture entry"),
			TransientFunctionId))
	{
		return false;
	}

	asIScriptContext* Context = Engine.GetScriptEngine()->CreateContext();
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT should create an execution context"), Context))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		Context->Release();
	};

	if (!Test.TestEqual(TEXT("StaticJIT.AOT should prepare the fixture entry function"), Context->Prepare(EntryFunction), asSUCCESS))
	{
		return false;
	}

	if (!Test.TestEqual(TEXT("StaticJIT.AOT should execute the fixture entry function"), Context->Execute(), asEXECUTION_FINISHED))
	{
		return false;
	}

	Test.TestEqual(TEXT("StaticJIT.AOT should return the expected fixture result"), static_cast<int32>(Context->GetReturnDWord()), AngelscriptStaticJITAotFixture::GetExpectedEntryResult());
	return true;
}

bool FAngelscriptStaticJITAotTests::RunNestedExceptionPropagation(FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);

	asCScriptFunction* const EntryFunction = FindGlobalFunction(
		Test,
		Engine,
		AngelscriptStaticJITAotFixture::GetExceptionEntryDeclaration());
	if (!Test.TestNotNull(
		TEXT("StaticJIT.AOT nested exception should resolve the generated entry"),
		EntryFunction))
	{
		return false;
	}

	asIScriptContext* const Context = Engine.CreateContext();
	if (!Test.TestNotNull(
		TEXT("StaticJIT.AOT nested exception should create an execution context"),
		Context))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		Context->Release();
	};

	if (!Test.TestEqual(
		TEXT("StaticJIT.AOT nested exception should prepare the generated entry"),
		Context->Prepare(EntryFunction),
		asSUCCESS))
	{
		return false;
	}

	Test.AddExpectedError(
		AngelscriptStaticJITAotFixture::GetExpectedExceptionText(),
		EAutomationExpectedErrorFlags::Contains,
		1);
	Test.AddExpectedError(
		TEXT("Angelscript: ASStaticJITAotFixture"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	Test.AddExpectedError(
		TEXT("void FailForAOT() | Line"),
		EAutomationExpectedErrorFlags::Contains,
		2,
		false);
	return Test.TestEqual(
		TEXT("StaticJIT.AOT nested generated exception should reach the outer context"),
		Context->Execute(),
		asEXECUTION_EXCEPTION);
}

bool FAngelscriptStaticJITAotTests::RunNestedReferenceLifetimeAndRecursion(FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);

	struct FCase
	{
		const FString& Declaration;
		const TCHAR* Label;
		int32 Expected;
	};
	const FCase Cases[] =
	{
		{ AngelscriptStaticJITAotFixture::GetReferenceEntryDeclaration(), TEXT("reference writeback"), AngelscriptStaticJITAotFixture::GetExpectedReferenceEntryResult() },
		{ AngelscriptStaticJITAotFixture::GetObjectLifetimeEntryDeclaration(), TEXT("object lifetime"), AngelscriptStaticJITAotFixture::GetExpectedObjectLifetimeResult() },
		{ AngelscriptStaticJITAotFixture::GetRecursionEntryDeclaration(), TEXT("recursion"), AngelscriptStaticJITAotFixture::GetExpectedRecursionResult() },
		{ AngelscriptStaticJITAotFixture::GetImportEntryDeclaration(), TEXT("current Engine import"), AngelscriptStaticJITAotFixture::GetExpectedImportResult() },
	};

	for (const FCase& Case : Cases)
	{
		asCScriptFunction* const EntryFunction = FindGlobalFunction(
			Test,
			Engine,
			Case.Declaration);
		uint32 FunctionId = 0;
		if (!RequireJitEntries(
			Test,
			Engine,
			EntryFunction,
			Case.Label,
			FunctionId))
		{
			return false;
		}

		asIScriptContext* const Context = Engine.CreateContext();
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("StaticJIT.AOT %s should create an execution context"), Case.Label),
			Context))
		{
			return false;
		}
		const int32 PrepareResult = Context->Prepare(EntryFunction);
		const int32 ExecuteResult = PrepareResult >= 0
			? Context->Execute()
			: PrepareResult;
		const int32 Actual = ExecuteResult == asEXECUTION_FINISHED
			? static_cast<int32>(Context->GetReturnDWord())
			: 0;
		Context->Release();

		if (!Test.TestEqual(
			*FString::Printf(TEXT("StaticJIT.AOT %s should finish"), Case.Label),
			ExecuteResult,
			asEXECUTION_FINISHED)
			|| !Test.TestEqual(
				*FString::Printf(TEXT("StaticJIT.AOT %s should preserve interpreter behavior"), Case.Label),
				Actual,
				Case.Expected))
		{
			return false;
		}
	}

	return true;
}

bool FAngelscriptStaticJITAotTests::RunCurrentVirtualOverride(FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);

	UASClass* const ParentClass = Cast<UASClass>(FindGeneratedClass(
		&Engine,
		AngelscriptStaticJITAotFixture::GetVirtualParentClassName()));
	UASClass* const ChildClass = Cast<UASClass>(FindGeneratedClass(
		&Engine,
		AngelscriptStaticJITAotFixture::GetVirtualChildClassName()));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT virtual route should restore the parent class"), ParentClass)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT virtual route should restore the child class"), ChildClass))
	{
		return false;
	}

	asITypeInfo* const ParentType = static_cast<asITypeInfo*>(ParentClass->ScriptTypePtr);
	asITypeInfo* const ChildType = static_cast<asITypeInfo*>(ChildClass->ScriptTypePtr);
	FTCHARToUTF8 CallDeclaration(*AngelscriptStaticJITAotFixture::GetVirtualCallDeclaration());
	FTCHARToUTF8 OverrideDeclaration(*AngelscriptStaticJITAotFixture::GetVirtualOverrideDeclaration());
	FTCHARToUTF8 OverrideImplementationDeclaration(
		*AngelscriptStaticJITAotFixture::GetVirtualOverrideImplementationDeclaration());
	asCScriptFunction* const CallFunction = ParentType != nullptr
		? static_cast<asCScriptFunction*>(ParentType->GetMethodByDecl(CallDeclaration.Get()))
		: nullptr;
	asCScriptFunction* const ParentVirtualFunction = ParentType != nullptr
		? static_cast<asCScriptFunction*>(ParentType->GetMethodByDecl(OverrideDeclaration.Get()))
		: nullptr;
	asCScriptFunction* const OverrideFunction = ChildType != nullptr
		? static_cast<asCScriptFunction*>(ChildType->GetMethodByDecl(OverrideImplementationDeclaration.Get()))
		: nullptr;
	uint32 CallFunctionId = 0;
	uint32 ParentVirtualFunctionId = 0;
	uint32 OverrideFunctionId = 0;
	if (!RequireJitEntries(Test, Engine, CallFunction, TEXT("parent virtual caller"), CallFunctionId)
		|| !RequireJitEntries(Test, Engine, ParentVirtualFunction, TEXT("parent virtual method"), ParentVirtualFunctionId)
		|| !RequireJitEntries(Test, Engine, OverrideFunction, TEXT("child current virtual override"), OverrideFunctionId))
	{
		return false;
	}

	UASFunction* const ReflectedCall = Cast<UASFunction>(FindGeneratedFunction(
		ParentClass,
		TEXT("CallVirtualValue")));
	UASFunction* const ReflectedVirtual = Cast<UASFunction>(FindGeneratedFunction(
		ParentClass,
		TEXT("GetVirtualValue")));
	UObject* const ChildInstance = NewObject<UObject>(
		GetTransientPackage(),
		ChildClass,
		TEXT("StaticJITAotVirtualChildInstance"));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT virtual route should expose the reflected parent caller"), ReflectedCall)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT virtual route should expose the reflected virtual method"), ReflectedVirtual)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT virtual route should instantiate the child"), ChildInstance))
	{
		return false;
	}

	FStructOnScope Params(ReflectedCall);
	FIntProperty* const ReturnProperty = FindFProperty<FIntProperty>(
		ReflectedCall,
		FName(TEXT("ReturnValue")));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT virtual route should expose the int return property"), ReturnProperty))
	{
		return false;
	}

	ReflectedCall->RuntimeCallEvent(ChildInstance, Params.GetStructMemory());
	const int32 ReturnValue = ReturnProperty->GetPropertyValue_InContainer(
		Params.GetStructMemory());
	const bool bGeneratedCallerPassed = Test.TestEqual(
		TEXT("StaticJIT.AOT parent generated caller should resolve the current child override"),
		ReturnValue,
		AngelscriptStaticJITAotFixture::GetExpectedVirtualOverrideResult());
	if (!bGeneratedCallerPassed)
	{
		return false;
	}

	FStructOnScope VirtualParams(ReflectedVirtual);
	FIntProperty* const VirtualReturnProperty = CastField<FIntProperty>(
		ReflectedVirtual->GetReturnProperty());
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT reflected virtual method should expose its return property"), VirtualReturnProperty))
	{
		return false;
	}

	ReflectedVirtual->RuntimeCallEvent(ChildInstance, VirtualParams.GetStructMemory());
	return Test.TestEqual(
		TEXT("StaticJIT.AOT reflected parent virtual wrapper should dispatch to the current child override"),
		VirtualReturnProperty->GetPropertyValue_InContainer(VirtualParams.GetStructMemory()),
		AngelscriptStaticJITAotFixture::GetExpectedVirtualOverrideResult());
}

bool FAngelscriptStaticJITAotTests::RunUASFunctionJitEntryAttachment(FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);

	UClass* const GeneratedClass = FindGeneratedClass(
		&Engine,
		AngelscriptStaticJITAotFixture::GetGeneratedClassName());
	UClass* const VirtualParentClass = FindGeneratedClass(
		&Engine,
		AngelscriptStaticJITAotFixture::GetVirtualParentClassName());
	UClass* const StaticsClass = FindGeneratedClass(
		&Engine,
		TEXT("UModule_ASStaticJITAotFixtureStatics"));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should generate the fixture class"), GeneratedClass)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should generate the virtual parent class"), VirtualParentClass)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should generate the statics class"), StaticsClass))
	{
		return false;
	}

	const TArray<TPair<FString, const TCHAR*>> MethodCases =
	{
		{ AngelscriptStaticJITAotFixture::GetMethodNoParamsDeclaration(), TEXT("no-param method") },
		{ AngelscriptStaticJITAotFixture::GetMethodPrimitiveArgDeclaration(), TEXT("primitive argument method") },
		{ AngelscriptStaticJITAotFixture::GetMethodPrimitiveReturnDeclaration(), TEXT("primitive return method") },
		{ AngelscriptStaticJITAotFixture::GetMethodReferenceArgDeclaration(), TEXT("reference argument method") },
		{ AngelscriptStaticJITAotFixture::GetMethodReferenceDeclaration(), TEXT("reference writeback method") },
		{ AngelscriptStaticJITAotFixture::GetMethodObjectReturnDeclaration(), TEXT("object return method") },
		{ AngelscriptStaticJITAotFixture::GetMethodMultiArgDeclaration(), TEXT("multi-argument generic method") },
		{ AngelscriptStaticJITAotFixture::GetMethodThreadSafeDeclaration(), TEXT("thread-safe generic method") },
	};

	for (const TPair<FString, const TCHAR*>& MethodCase : MethodCases)
	{
		uint32 FunctionId = 0;
		if (!RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, MethodCase.Key), MethodCase.Value, FunctionId))
		{
			return false;
		}
	}

	uint32 StaticFunctionId = 0;
	if (!RequireJitEntries(
			Test,
			Engine,
			FindGlobalFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetStaticWorldContextDeclaration()),
			TEXT("static world-context function"),
			StaticFunctionId))
	{
		return false;
	}

	struct FWrapperCase
	{
		UClass* OwnerClass = nullptr;
		FName FunctionName;
		UClass* ExpectedClass = nullptr;
		const TCHAR* Label = TEXT("");
	};

	const TArray<FWrapperCase> WrapperCases =
	{
		{ GeneratedClass, TEXT("ResetStoredValue"), UASFunction_NoParams::StaticClass(), TEXT("reloadable specialized no-param wrapper") },
		{ GeneratedClass, TEXT("StorePrimitiveArg"), UASFunction_DWordArg::StaticClass(), TEXT("reloadable specialized primitive-argument wrapper") },
		{ GeneratedClass, TEXT("ReturnPrimitive"), UASFunction_DWordReturn::StaticClass(), TEXT("reloadable specialized primitive-return wrapper") },
		{ GeneratedClass, TEXT("IncrementReference"), UASFunction_ReferenceArg::StaticClass(), TEXT("reloadable specialized reference-argument wrapper") },
		{ GeneratedClass, TEXT("BumpReference"), UASFunction_NotThreadSafe::StaticClass(), TEXT("reloadable generic reference-plus-return wrapper") },
		{ GeneratedClass, TEXT("ReturnSelfObject"), UASFunction_ObjectReturn::StaticClass(), TEXT("reloadable specialized object-return wrapper") },
		{ GeneratedClass, TEXT("AddTwo"), UASFunction_NotThreadSafe::StaticClass(), TEXT("reloadable generic multi-argument wrapper") },
		{ GeneratedClass, TEXT("ThreadSafeReturn"), UASFunction::StaticClass(), TEXT("reloadable generic thread-safe wrapper") },
		{ VirtualParentClass, TEXT("GetVirtualValue"), UASFunction_NotThreadSafe::StaticClass(), TEXT("generic virtual/non-final wrapper") },
		{ StaticsClass, TEXT("StaticWorldContextCheck"), UASFunction_NotThreadSafe::StaticClass(), TEXT("reloadable generic static world-context wrapper") },
	};

	for (const FWrapperCase& WrapperCase : WrapperCases)
	{
		if (!RequireFunctionClass(
				Test,
				WrapperCase.OwnerClass,
				WrapperCase.FunctionName,
				WrapperCase.ExpectedClass,
				WrapperCase.Label))
		{
			return false;
		}
	}

	return true;
}

bool FAngelscriptStaticJITAotTests::RunUASFunctionRuntimeCallEvent(FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);

	UClass* GeneratedClass = FindGeneratedClass(&Engine, AngelscriptStaticJITAotFixture::GetGeneratedClassName());
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should generate the fixture class"), GeneratedClass))
	{
		return false;
	}

	UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("StaticJITAotFunctionCarrierInstance"));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should instantiate the fixture class"), Instance))
	{
		return false;
	}

	UASFunction* ResetStoredValueFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("ResetStoredValue")));
	UASFunction* StorePrimitiveArgFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("StorePrimitiveArg")));
	UASFunction* ReturnPrimitiveFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("ReturnPrimitive")));
	UASFunction* IncrementReferenceFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("IncrementReference")));
	UASFunction* BumpReferenceFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("BumpReference")));
	UASFunction* ReturnSelfObjectFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("ReturnSelfObject")));
	UASFunction* AddTwoFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("AddTwo")));
	UASFunction* ThreadSafeReturnFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("ThreadSafeReturn")));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose ResetStoredValue"), ResetStoredValueFunction)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose StorePrimitiveArg"), StorePrimitiveArgFunction)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose ReturnPrimitive"), ReturnPrimitiveFunction)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose IncrementReference"), IncrementReferenceFunction)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose BumpReference"), BumpReferenceFunction)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose ReturnSelfObject"), ReturnSelfObjectFunction)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose AddTwo"), AddTwoFunction)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose ThreadSafeReturn"), ThreadSafeReturnFunction))
	{
		return false;
	}

	uint32 ResetStoredValueId = 0;
	uint32 StorePrimitiveArgId = 0;
	uint32 ReturnPrimitiveId = 0;
	uint32 IncrementReferenceId = 0;
	uint32 BumpReferenceId = 0;
	uint32 ReturnSelfObjectId = 0;
	uint32 AddTwoId = 0;
	uint32 ThreadSafeReturnId = 0;
	if (!RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetMethodNoParamsDeclaration()), TEXT("no-param method"), ResetStoredValueId)
		|| !RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetMethodPrimitiveArgDeclaration()), TEXT("primitive argument method"), StorePrimitiveArgId)
		|| !RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetMethodPrimitiveReturnDeclaration()), TEXT("primitive return method"), ReturnPrimitiveId)
		|| !RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetMethodReferenceArgDeclaration()), TEXT("reference argument method"), IncrementReferenceId)
		|| !RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetMethodReferenceDeclaration()), TEXT("reference writeback method"), BumpReferenceId)
		|| !RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetMethodObjectReturnDeclaration()), TEXT("object return method"), ReturnSelfObjectId)
		|| !RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetMethodMultiArgDeclaration()), TEXT("multi-argument generic method"), AddTwoId)
		|| !RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetMethodThreadSafeDeclaration()), TEXT("thread-safe generic method"), ThreadSafeReturnId))
	{
		return false;
	}

	FIntProperty* StoredValueProperty = FindFProperty<FIntProperty>(GeneratedClass, TEXT("StoredValue"));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose StoredValue"), StoredValueProperty))
	{
		return false;
	}

	ResetStoredValueFunction->RuntimeCallEvent(Instance, nullptr);
	Test.TestEqual(TEXT("RuntimeCallEvent JIT no-param method should update object state"), StoredValueProperty->GetPropertyValue_InContainer(Instance), AngelscriptStaticJITAotFixture::GetExpectedNoParamsStoredValue());

	FPrimitiveArgParams PrimitiveArgParams;
	PrimitiveArgParams.Value = 41;
	StorePrimitiveArgFunction->RuntimeCallEvent(Instance, &PrimitiveArgParams);
	Test.TestEqual(TEXT("RuntimeCallEvent JIT primitive arg should update object state"), StoredValueProperty->GetPropertyValue_InContainer(Instance), AngelscriptStaticJITAotFixture::GetExpectedPrimitiveArgStoredValue());

	FStructOnScope PrimitiveReturnParams(ReturnPrimitiveFunction);
	ReturnPrimitiveFunction->RuntimeCallEvent(Instance, PrimitiveReturnParams.GetStructMemory());
	FIntProperty* PrimitiveReturnProperty = CastField<FIntProperty>(ReturnPrimitiveFunction->GetReturnProperty());
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose primitive return property"), PrimitiveReturnProperty))
	{
		return false;
	}
	Test.TestEqual(TEXT("RuntimeCallEvent JIT primitive return should write reflected return value"), PrimitiveReturnProperty->GetPropertyValue_InContainer(PrimitiveReturnParams.GetStructMemory()), AngelscriptStaticJITAotFixture::GetExpectedPrimitiveReturnValue());

	FPrimitiveArgParams ReferenceArgParams;
	ReferenceArgParams.Value = 13;
	IncrementReferenceFunction->RuntimeCallEvent(Instance, &ReferenceArgParams);
	Test.TestEqual(TEXT("RuntimeCallEvent JIT specialized reference arg should write back parameter memory"), ReferenceArgParams.Value, AngelscriptStaticJITAotFixture::GetExpectedReferenceArgValue());

	FReferenceParams ReferenceParams;
	ReferenceParams.Value = 13;
	BumpReferenceFunction->RuntimeCallEvent(Instance, &ReferenceParams);
	Test.TestEqual(TEXT("RuntimeCallEvent JIT reference arg should write back parameter memory"), ReferenceParams.Value, AngelscriptStaticJITAotFixture::GetExpectedReferenceReturnValue());
	Test.TestEqual(TEXT("RuntimeCallEvent JIT reference arg should write reflected return value"), ReferenceParams.ReturnValue, AngelscriptStaticJITAotFixture::GetExpectedReferenceReturnValue());

	FStructOnScope ObjectReturnParams(ReturnSelfObjectFunction);
	ReturnSelfObjectFunction->RuntimeCallEvent(Instance, ObjectReturnParams.GetStructMemory());
	FObjectProperty* ObjectReturnProperty = CastField<FObjectProperty>(ReturnSelfObjectFunction->GetReturnProperty());
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose object return property"), ObjectReturnProperty))
	{
		return false;
	}
	Test.TestTrue(TEXT("RuntimeCallEvent JIT object return should preserve object identity"), ObjectReturnProperty->GetObjectPropertyValue_InContainer(ObjectReturnParams.GetStructMemory()) == Instance);

	FTwoPrimitiveParams MultiArgParams;
	MultiArgParams.A = 17;
	MultiArgParams.B = 25;
	AddTwoFunction->RuntimeCallEvent(Instance, &MultiArgParams);
	Test.TestEqual(TEXT("RuntimeCallEvent JIT generic multi-argument method should preserve its result"), MultiArgParams.ReturnValue, AngelscriptStaticJITAotFixture::GetExpectedMultiArgResult());

	FStructOnScope ThreadSafeParams(ThreadSafeReturnFunction);
	ThreadSafeReturnFunction->RuntimeCallEvent(Instance, ThreadSafeParams.GetStructMemory());
	FIntProperty* const ThreadSafeReturnProperty = CastField<FIntProperty>(ThreadSafeReturnFunction->GetReturnProperty());
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT thread-safe generic dispatch should expose its return property"), ThreadSafeReturnProperty))
	{
		return false;
	}
	Test.TestEqual(TEXT("RuntimeCallEvent thread-safe generic method should preserve its result"), ThreadSafeReturnProperty->GetPropertyValue_InContainer(ThreadSafeParams.GetStructMemory()), AngelscriptStaticJITAotFixture::GetExpectedThreadSafeResult());

	return true;
}

bool FAngelscriptStaticJITAotTests::RunStaticWorldContextRuntimeCallEvent(FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);

	UClass* StaticsClass = FindGeneratedClass(&Engine, TEXT("UModule_ASStaticJITAotFixtureStatics"));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should generate the fixture statics class"), StaticsClass))
	{
		return false;
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, AngelscriptStaticJITAotFixture::GetGeneratedClassName());
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should generate the fixture class for world context"), GeneratedClass))
	{
		return false;
	}

	UASFunction* StaticWorldContextFunction = Cast<UASFunction>(FindGeneratedFunction(StaticsClass, TEXT("StaticWorldContextCheck")));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose StaticWorldContextCheck"), StaticWorldContextFunction))
	{
		return false;
	}

	uint32 StaticWorldContextId = 0;
	if (!RequireJitEntries(
			Test,
			Engine,
			FindGlobalFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetStaticWorldContextDeclaration()),
			TEXT("static world-context function"),
			StaticWorldContextId))
	{
		return false;
	}

	UObject* WorldContextObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("StaticJITAotWorldContextObject"));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should instantiate a concrete world-context object"), WorldContextObject))
	{
		return false;
	}

	FStaticWorldContextParams Params;
	Params.WorldContextObject = WorldContextObject;
	Params.Value = 31;
	StaticWorldContextFunction->RuntimeCallEvent(StaticsClass->GetDefaultObject(), &Params);

	Test.TestEqual(TEXT("RuntimeCallEvent JIT static world-context should preserve script result"), Params.ReturnValue, AngelscriptStaticJITAotFixture::GetExpectedStaticWorldContextResult());
	return true;
}

bool FAngelscriptStaticJITAotTests::RunMultiEngineSequentialLoad(FAutomationTestBase& Test)
{
	for (int32 Index = 0; Index < 2; ++Index)
	{
		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> Session =
				AngelscriptStaticJITAotGeneration::
					CreateFixtureEngineSession(false);
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("StaticJIT.AOT multi-engine pass %d should create an isolated source fixture session"),
					Index),
				Session.Get())
			|| !Test.TestTrue(
				*FString::Printf(
					TEXT("StaticJIT.AOT multi-engine pass %d should publish current Provider routes"),
					Index),
				Session.IsValid() && Session->IsValid()))
		{
			if (Session.IsValid())
			{
				Test.AddError(Session->Error);
			}
			return false;
		}
		FAngelscriptEngine& Engine = *Session->Engine;
		FAngelscriptEngineScope EngineScope(Engine);

		asCScriptFunction* EntryFunction = FindEntryFunction(Engine);
		if (!Test.TestNotNull(*FString::Printf(TEXT("StaticJIT.AOT multi-engine pass %d should resolve the fixture entry function"), Index), EntryFunction))
		{
			return false;
		}

		uint32 TransientFunctionId = 0;
		if (!RequireJitEntries(
				Test, Engine, EntryFunction,
				*FString::Printf(TEXT("multi-engine pass %d fixture entry"), Index),
				TransientFunctionId))
		{
			return false;
		}
	}

	return true;
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotUASFunctionDispatchTests,
	"Angelscript.TestModule.StaticJIT.AOT.UASFunctionDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		FAngelscriptStaticJITAotTests::SharedFixtureSession =
			AngelscriptStaticJITAotGeneration::CreateFixtureEngineSession(false);
	}

	AFTER_ALL()
	{
		FAngelscriptStaticJITAotTests::SharedFixtureSession.Reset();
	}

	TEST_METHOD(ExposesJitEntries)
	{
		ASSERT_THAT(IsTrue(FAngelscriptStaticJITAotTests::RunUASFunctionJitEntryAttachment(*TestRunner)));
	}

	TEST_METHOD(RuntimeCallEventUsesGeneratedJit)
	{
		ASSERT_THAT(IsTrue(FAngelscriptStaticJITAotTests::RunUASFunctionRuntimeCallEvent(*TestRunner)));
	}

	TEST_METHOD(StaticWorldContextUsesGeneratedJit)
	{
		ASSERT_THAT(IsTrue(FAngelscriptStaticJITAotTests::RunStaticWorldContextRuntimeCallEvent(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotMultiEngineTests,
	"Angelscript.TestModule.StaticJIT.AOT.MultiEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SequentialLoadsKeepGeneratedRegistryVisible)
	{
		ASSERT_THAT(IsTrue(FAngelscriptStaticJITAotTests::RunMultiEngineSequentialLoad(*TestRunner)));
	}
};

#endif
