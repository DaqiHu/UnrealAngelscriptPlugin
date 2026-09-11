#include "CQTest.h"

#include "AngelscriptTestEngineAcquisition.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestJITProbes.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASFunction.h"
#include "Cache/AngelscriptCacheService.h"
#include "Core/AngelscriptUhtCoverageTestTypes.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AOT/UASFunctionDispatch/AngelscriptStaticJITAotUASFunctionDispatchTestBridge.h"
#include "StaticJIT/AngelscriptJITBindingContext.h"
#include "StaticJIT/AngelscriptJITExecutionContext.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptJITProviderRouter.h"
#include "StaticJIT/AngelscriptJITReferenceResolver.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h"
#include "StaticJIT/StaticJITDiagnostics.h"
#include "StaticJIT/StaticJITHeader.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITBackend.h"
#include "AngelScriptSDK/Support/AngelscriptNativeDebugTestSupport.h"
#include "Testing/AngelscriptScriptTestRunner.h"

#include "Editor.h"
#include "StartAngelscriptHeaders.h"
#include "as_bytecode.h"
#include "as_context.h"
#include "as_module.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#include "UObject/StructOnScope.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#include <cmath>
#include <limits>

#if WITH_ANGELSCRIPT_UNITTESTS

struct FAngelscriptStaticJITAotUASFunctionDispatchTestAccess;
struct FAngelscriptStaticJITAotMultiEngineTests;

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotTests,
	"Angelscript.TestModule.StaticJIT.AOT",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	friend struct FAngelscriptStaticJITAotUASFunctionDispatchTestAccess;
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

	struct FExceptionRouteObservation
	{
		FString Label;
		int32 ExecuteStatus = asERROR;
		FString Message;
		FString FunctionDeclaration;
		FString RawSection;
		FString Section;
		int32 Line = INDEX_NONE;
		int32 Column = INDEX_NONE;
		int32 CallbackCount = 0;
		bool bObservedJITRecord = false;
		bool bRecordReported = false;
		bool bRecordAdopted = false;
		FString RouteOrigin;
		FString BackendOrigin;
		int32 TriggerCallCount = INDEX_NONE;
		int32 LaterSideEffectCount = 0;

		FString Describe() const
		{
			return FString::Printf(TEXT(
				"%s: Status=%d Message='%s' Function='%s' RawSection='%s' Section='%s' Line=%d Column=%d CallbackCount=%d JITRecord=%s Reported=%s Adopted=%s Route='%s' Backend='%s' TriggerCalls=%d LaterSideEffects=%d"),
				*Label,
				ExecuteStatus,
				*Message,
				*FunctionDeclaration,
				*RawSection,
				*Section,
				Line,
				Column,
				CallbackCount,
				bObservedJITRecord ? TEXT("true") : TEXT("false"),
				bRecordReported ? TEXT("true") : TEXT("false"),
				bRecordAdopted ? TEXT("true") : TEXT("false"),
				*RouteOrigin,
				*BackendOrigin,
				TriggerCallCount,
				LaterSideEffectCount);
		}
	};

	static FString ResolveStableExceptionSection(
		FAngelscriptEngine& Engine,
		asIScriptFunction* Function,
		const FStringView RawSection)
	{
		if (Function == nullptr || Function->GetModule() == nullptr)
		{
			return FString(RawSection);
		}

		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Engine.GetModule(Function->GetModule());
		if (!Module.IsValid())
		{
			return FString(RawSection);
		}

		for (const FAngelscriptModuleDesc::FCodeSection& CodeSection
			: Module->Code)
		{
			if (RawSection.Equals(
					CodeSection.AbsoluteFilename,
					ESearchCase::CaseSensitive)
				|| RawSection.Equals(
					CodeSection.VirtualPath,
					ESearchCase::CaseSensitive)
				|| RawSection.Equals(
					CodeSection.RelativeFilename,
					ESearchCase::CaseSensitive)
				|| (!CodeSection.AbsoluteFilename.IsEmpty()
					&& RawSection.Equals(
						CodeSection.AbsoluteFilename,
						ESearchCase::IgnoreCase)))
			{
				return CodeSection.VirtualPath.IsEmpty()
					? FString(RawSection) : CodeSection.VirtualPath;
			}
		}
		return FString(RawSection);
	}

	struct FExceptionRouteCallbackCapture
	{
		int32 Count = 0;
		bool bObservedJITRecord = false;
		bool bRecordReported = false;
		bool bRecordAdopted = false;
		FString RouteOrigin;
		FString BackendOrigin;
	};

	static void CaptureExceptionRoute(
		asIScriptContext* Context,
		void* UserData)
	{
		FExceptionRouteCallbackCapture* const Capture =
			static_cast<FExceptionRouteCallbackCapture*>(UserData);
		if (Context == nullptr || Capture == nullptr)
		{
			return;
		}

		++Capture->Count;
		asCThreadLocalData* const ThreadLocalData =
			asCThreadManager::GetLocalData();
		FScriptExecution* const Execution = ThreadLocalData != nullptr
			? ThreadLocalData->activeExecution : nullptr;
		const asSJITFailureRecord* const Failure = Execution != nullptr
			? Execution->FailureRecord : nullptr;
		if (Failure != nullptr && Failure->bHasFailure)
		{
			Capture->bObservedJITRecord = true;
			Capture->bRecordReported = Failure->bReported;
			Capture->bRecordAdopted = Failure->bAdopted;
			Capture->RouteOrigin = UTF8_TO_TCHAR(
				Failure->RouteOrigin.AddressOf());
			Capture->BackendOrigin = UTF8_TO_TCHAR(
				Failure->BackendOrigin.AddressOf());
		}
	}

	static bool ObserveExceptionContext(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asCScriptFunction* Function,
		const TConstArrayView<asDWORD> Arguments,
		const FString& Label,
		FExceptionRouteObservation& OutObservation)
	{
		OutObservation = {};
		OutObservation.Label = Label;
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should resolve its function"), *Label),
				Function))
		{
			return false;
		}

		asIScriptContext* const Context = Engine.CreateContext();
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should create a context"), *Label),
				Context))
		{
			return false;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		FExceptionRouteCallbackCapture Capture;
		if (!Test.TestEqual(
				*FString::Printf(TEXT("%s should install its exception callback"),
					*Label),
				Context->SetExceptionCallback(
					asFUNCTION(CaptureExceptionRoute),
					&Capture,
					asCALL_CDECL),
				asSUCCESS)
			|| !Test.TestEqual(
				*FString::Printf(TEXT("%s should prepare its function"), *Label),
				Context->Prepare(Function),
				asSUCCESS))
		{
			return false;
		}

		for (int32 ArgumentIndex = 0;
			ArgumentIndex < Arguments.Num();
			++ArgumentIndex)
		{
			if (!Test.TestEqual(
					*FString::Printf(TEXT("%s should set argument %d"),
						*Label, ArgumentIndex),
					Context->SetArgDWord(
						ArgumentIndex, Arguments[ArgumentIndex]),
					asSUCCESS))
			{
				return false;
			}
		}

		{
			FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
			OutObservation.ExecuteStatus = Context->Execute();
		}

		OutObservation.Message = UTF8_TO_TCHAR(
			Context->GetExceptionString() != nullptr
				? Context->GetExceptionString() : "");
		asIScriptFunction* const ExceptionFunction =
			Context->GetExceptionFunction();
		if (ExceptionFunction != nullptr)
		{
			OutObservation.FunctionDeclaration = UTF8_TO_TCHAR(
				ExceptionFunction->GetDeclaration(false, false, false));
		}
		const char* Section = nullptr;
		OutObservation.Line = Context->GetExceptionLineNumber(
			&OutObservation.Column, &Section);
		OutObservation.RawSection = UTF8_TO_TCHAR(
			Section != nullptr ? Section : "");
		OutObservation.Section = ResolveStableExceptionSection(
			Engine, ExceptionFunction, OutObservation.RawSection);
		OutObservation.CallbackCount = Capture.Count;
		OutObservation.bObservedJITRecord = Capture.bObservedJITRecord;
		OutObservation.bRecordReported = Capture.bRecordReported;
		OutObservation.bRecordAdopted = Capture.bRecordAdopted;
		OutObservation.RouteOrigin = MoveTemp(Capture.RouteOrigin);
		OutObservation.BackendOrigin = MoveTemp(Capture.BackendOrigin);
		if (!OutObservation.bObservedJITRecord
			&& OutObservation.CallbackCount > 0)
		{
			OutObservation.RouteOrigin = TEXT("vm");
			OutObservation.BackendOrigin = TEXT("vm");
		}
		return true;
	}

	static AngelscriptTestJITProbes::FIsolatedDifferentialRouteResult
		MakeInterpreterFailureResult(
			FAngelscriptEngine& Engine,
			asCScriptFunction& Function,
			const FExceptionRouteObservation& Observation)
	{
		using namespace AngelscriptTestJITProbes;
		FIsolatedDifferentialRouteResult Result;
		Result.Backend =
			EIsolatedDifferentialExecutionBackend::InterpreterVM;
		Result.Route = EIsolatedDifferentialEntryRoute::VM;
		Result.ExecuteStatus = Observation.ExecuteStatus;
		Result.bExceptionThrown =
			Observation.ExecuteStatus == asEXECUTION_EXCEPTION;
		Result.Failure.bHasFailure = Result.bExceptionThrown;
		Result.Failure.bFromJITRecord = Observation.bObservedJITRecord;
		Result.Failure.bReported = Observation.CallbackCount > 0;
		Result.Failure.bAdopted = Observation.bRecordAdopted;
		Result.Failure.FunctionId = Function.GetId();
		Result.Failure.Message = Observation.Message;
		FAngelscriptStableFunctionKey FunctionKey;
		if (Engine.TryResolveStableFunctionKey(&Function, FunctionKey))
		{
			Result.Failure.StableFunctionIdentity =
				FunctionKey.Hash.ToHexString();
		}
		Result.Failure.FunctionDeclaration =
			Observation.FunctionDeclaration;
		Result.Failure.Section = Observation.Section;
		Result.Failure.Row = Observation.Line;
		Result.Failure.Column = Observation.Column;
		Result.Failure.RouteOrigin = Observation.RouteOrigin;
		Result.Failure.BackendOrigin = Observation.BackendOrigin;
		return Result;
	}

	static AngelscriptTestJITProbes::FIsolatedDifferentialRouteResult
		MakeDirectFailureResult(
			const AngelscriptTestJITProbes::
				EIsolatedDifferentialExecutionBackend Backend,
			const AngelscriptTestJITProbes::
				EIsolatedDifferentialEntryRoute Route,
			const AngelscriptTestJITProbes::
				FIsolatedDifferentialEntryView& Entry,
			const FScriptExecution& Execution,
			const int32 Value)
	{
		using namespace AngelscriptTestJITProbes;
		FIsolatedDifferentialRouteResult Result;
		Result.Backend = Backend;
		Result.Route = Route;
		Result.Value = Value;
		Result.bExceptionThrown = Execution.bExceptionThrown;
		Result.ExecuteStatus = Result.bExceptionThrown
			? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED;
		Result.CleanupPlanState = Entry.CleanupPlanState;
		Result.CleanupActionCount = Entry.CleanupActionCount;

		const asSJITFailureRecord* const Failure = Execution.FailureRecord;
		if (Failure == nullptr || !Failure->bHasFailure)
		{
			return Result;
		}

		Result.Failure.bHasFailure = true;
		Result.Failure.bFromJITRecord = true;
		Result.Failure.bReported = Failure->bReported;
		Result.Failure.bAdopted = Failure->bAdopted;
		Result.Failure.FunctionId = Failure->FunctionId;
		Result.Failure.Message = UTF8_TO_TCHAR(
			Failure->Message.AddressOf());
		Result.Failure.StableFunctionIdentity = UTF8_TO_TCHAR(
			Failure->StableFunctionIdentity.AddressOf());
		Result.Failure.Section = UTF8_TO_TCHAR(
			Failure->SectionName.AddressOf());
		Result.Failure.Row = Failure->Line;
		Result.Failure.Column = Failure->Column;
		Result.Failure.RouteOrigin = UTF8_TO_TCHAR(
			Failure->RouteOrigin.AddressOf());
		Result.Failure.BackendOrigin = UTF8_TO_TCHAR(
			Failure->BackendOrigin.AddressOf());
		asIScriptFunction* const FailureFunction =
			Execution.jitEngine != nullptr && Failure->FunctionId >= 0
				? Execution.jitEngine->GetFunctionById(Failure->FunctionId)
				: Execution.jitFunction;
		if (FailureFunction != nullptr)
		{
			Result.Failure.FunctionDeclaration = UTF8_TO_TCHAR(
				FailureFunction->GetDeclaration(false, false, false));
		}
		return Result;
	}

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

	static asCScriptFunction* FindGlobalFunctionInModule(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName& ModuleName,
		const FString& Declaration)
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc =
			Engine.GetModuleByModuleName(ModuleName.ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT should resolve module '%s' before looking up global function '%s'."),
				*ModuleName.ToString(),
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
				*ModuleName.ToString(),
				*GetAvailableGlobalFunctionDeclarations(*ModuleDesc->ScriptModule)));
		}

		return static_cast<asCScriptFunction*>(Function);
	}

	static TSharedPtr<const FAngelscriptJITResolvedReferenceTable,
		ESPMode::ThreadSafe> ResolveIsolatedEntryReferences(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const AngelscriptTestJITProbes::FIsolatedDifferentialEntryView& Entry)
	{
		if (Entry.ReferenceSlots.IsEmpty())
		{
			return nullptr;
		}

		TArray<FAngelscriptArtifactReference> RequiredReferences;
		RequiredReferences.Reserve(Entry.ReferenceSlots.Num());
		for (const FAngelscriptJITReferenceSlot& Slot : Entry.ReferenceSlots)
		{
			RequiredReferences.Add(Slot.Reference);
		}
		const FAngelscriptJITCurrentEngineReferenceBuildResult ResolverResult =
			FAngelscriptJITCurrentEngineReferenceResolverBuilder::Build(
				Engine, RequiredReferences);
		if (!Test.TestTrue(
				*FString::Printf(TEXT(
					"Isolated entry '%s' should resolve its current-Engine references: %s"),
					*Entry.CanonicalDeclaration,
					*ResolverResult.Detail),
				ResolverResult.IsUsable()))
		{
			return nullptr;
		}
		const FAngelscriptJITReferenceTableBuildResult TableResult =
			FAngelscriptJITResolvedReferenceTableBuilder::Build(
				Entry.ReferenceSlots, *ResolverResult.Resolver);
		if (!Test.TestTrue(
				*FString::Printf(TEXT(
					"Isolated entry '%s' should build an exact reference table"),
					*Entry.CanonicalDeclaration),
				TableResult.IsExact()))
		{
			return nullptr;
		}
		return TableResult.Table;
	}

	static bool ExecuteInterpreterInt(
		FAutomationTestBase& Test,
		asCScriptFunction& Function,
		const TCHAR* const Label,
		TFunctionRef<bool(asIScriptContext&)> SetArguments,
		int32& OutValue)
	{
		asIScriptContext* const Context = Function.GetEngine()->CreateContext();
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should create an interpreter context"),
					Label),
				Context))
		{
			return false;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		if (!Test.TestEqual(
				*FString::Printf(TEXT("%s should prepare the interpreter body"),
					Label),
				Context->Prepare(&Function),
				asSUCCESS))
		{
			return false;
		}
		static_cast<asCContext*>(Context)->m_executeVirtualCall = false;
		if (!SetArguments(*Context))
		{
			Test.AddError(FString::Printf(
				TEXT("%s could not initialize interpreter arguments"), Label));
			return false;
		}
		const int ExecuteResult = Context->Execute();
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			const char* const ExceptionString = Context->GetExceptionString();
			Test.AddError(FString::Printf(
				TEXT("%s interpreter returned %d instead of %d: %s"),
				Label,
				ExecuteResult,
				asEXECUTION_FINISHED,
				ExceptionString != nullptr
					? UTF8_TO_TCHAR(ExceptionString)
					: TEXT("<no exception string>")));
			return false;
		}
		OutValue = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

	static asCScriptFunction* FindMethodFunctionInModule(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName& ModuleName,
		const FName& TypeName,
		const FString& Declaration)
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc =
			Engine.GetModuleByModuleName(ModuleName.ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT should resolve module '%s' before looking up method '%s'."),
				*ModuleName.ToString(), *Declaration));
			return nullptr;
		}

		FTCHARToUTF8 TypeNameUtf8(*TypeName.ToString());
		asITypeInfo* const TypeInfo =
			ModuleDesc->ScriptModule->GetTypeInfoByName(TypeNameUtf8.Get());
		if (TypeInfo == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT should resolve type '%s' in module '%s' before looking up method '%s'."),
				*TypeName.ToString(), *ModuleName.ToString(), *Declaration));
			return nullptr;
		}

		FTCHARToUTF8 MethodDecl(*Declaration);
		asIScriptFunction* Function = TypeInfo->GetMethodByDecl(MethodDecl.Get());
		if (Function == nullptr)
		{
			const FString FunctionName = GetFunctionNameFromDeclaration(Declaration);
			FTCHARToUTF8 FunctionNameUtf8(*FunctionName);
			for (asUINT MethodIndex = 0;
				MethodIndex < TypeInfo->GetMethodCount(); ++MethodIndex)
			{
				asIScriptFunction* const Candidate =
					TypeInfo->GetMethodByIndex(MethodIndex);
				if (Candidate != nullptr
					&& FCStringAnsi::Strcmp(
						Candidate->GetName(), FunctionNameUtf8.Get()) == 0)
				{
					Function = Candidate;
					break;
				}
			}
		}
		if (Function == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT should resolve method '%s' on '%s'; available methods: %s"),
				*Declaration, *TypeName.ToString(),
				*GetAvailableMethodDeclarations(*TypeInfo)));
		}
		return static_cast<asCScriptFunction*>(Function);
	}

	static asCScriptFunction* FindGlobalFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& Declaration)
	{
		return FindGlobalFunctionInModule(
			Test,
			Engine,
			AngelscriptStaticJITAotFixture::GetModuleName(),
			Declaration);
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
		const bool bRouteResolved = Engine.ResolveFunctionRoute(
			FunctionKey, Route);
		const bool bNativeRoute = bRouteResolved && Route.IsNative();
		if (!bNativeRoute)
		{
			const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
				ESPMode::ThreadSafe> Snapshot = Engine.GetFunctionRouteSnapshot();
			Test.AddError(FString::Printf(TEXT(
				"StaticJIT.AOT %s route diagnostic: Resolved=%s Native=%s Key=%s ModuleKey=%s Execution=%s Function=%p Id=%d Declaration='%s' RouteGeneration=%llu RouteFunction=%p RouteId=%d Selected=%u Match=%u Verified=%s SnapshotGeneration=%llu SnapshotNative=%u SnapshotVM=%u Binding={VM=%p Raw=%p Parms=%p UserData=%p}"),
				Label,
				bRouteResolved ? TEXT("true") : TEXT("false"),
				bNativeRoute ? TEXT("true") : TEXT("false"),
				*FunctionKey.Hash.ToHexString(),
				*Route.ModuleKey.Hash.ToHexString(),
				*Route.Identity.Content.Execution.ToHexString(),
				Function,
				Function->GetId(),
				UTF8_TO_TCHAR(Function->GetDeclaration()),
				Route.Generation.Value,
				Route.Function,
				Route.NumericFunctionId,
				static_cast<uint32>(Route.SelectedExecutionRoute),
				static_cast<uint32>(Route.MatchResult),
				Route.bHasVerifiedArtifactIdentity
					? TEXT("true") : TEXT("false"),
				Snapshot.IsValid() ? Snapshot->Generation.Value : 0,
				Snapshot.IsValid() ? Snapshot->NativeRouteCount : 0,
				Snapshot.IsValid() ? Snapshot->VmRouteCount : 0,
				Binding.VMEntry,
				Binding.RawEntry,
				Binding.ParmsEntry,
				Binding.UserData));
		}
		return Test.TestTrue(
				*FString::Printf(
					TEXT("StaticJIT.AOT %s should resolve its current route"), Label),
				bRouteResolved)
			&& Test.TestTrue(
				*FString::Printf(
					TEXT("StaticJIT.AOT %s should select generated Native code"), Label),
				bNativeRoute)
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

	static bool RunTypedASTScalarParity(FAutomationTestBase& Test);
	static bool RunTypedASTStructuredControlFlowParity(
		FAutomationTestBase& Test);
	static bool RunRuntimeRegistration(FAutomationTestBase& Test);
	static bool RunRuntimeExecution(FAutomationTestBase& Test);
	static bool RunNestedExceptionPropagation(FAutomationTestBase& Test);
	static bool RunNestedReferenceLifetimeAndRecursion(FAutomationTestBase& Test);
	static bool RunCurrentVirtualOverride(FAutomationTestBase& Test);
	static bool RunUASFunctionJitEntryAttachment(FAutomationTestBase& Test);
	static bool RunTypedUASFunctionIdentityAndParmsDispatch(
		FAutomationTestBase& Test);
	static bool RunTypedFinalScalarRawDispatch(FAutomationTestBase& Test);
	static bool RunUASFunctionRuntimeCallEvent(FAutomationTestBase& Test);
	static bool RunRpcProcessEventRoute(FAutomationTestBase& Test);
	static bool RunStaticWorldContextRuntimeCallEvent(FAutomationTestBase& Test);
	static bool RunBytecodeFallbackGeneratedWorldContext(
		FAutomationTestBase& Test);
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

	TEST_METHOD(TypedASTScalarProbeMatchesInterpreterAndBytecodeJIT)
	{
		ASSERT_THAT(IsTrue(RunTypedASTScalarParity(*TestRunner)));
	}

	TEST_METHOD(TypedASTStructuredControlFlowMatchesInterpreterAndBytecodeJIT)
	{
		ASSERT_THAT(IsTrue(
			RunTypedASTStructuredControlFlowParity(*TestRunner)));
	}

	TEST_METHOD(TypedASTGeneratedProviderEntriesCompileLinkAndExecute)
	{
		ASSERT_THAT(IsTrue(
			AngelscriptTestJITProbes::HasTypedASTScalarProviderEntries(),
			TEXT("The generated TypedASTJIT Provider probe should register raw, VM, and Parms entries")));

		struct FCase
		{
			int32 A;
			int32 B;
			int32 Expected;
		};
		const FCase Cases[] = {
			{3, 4, 6},
			{6, 7, 26},
			{-5, 2, -4},
			{10, 0, 9},
		};
		for (const FCase& Case : Cases)
		{
			ASSERT_THAT(AreEqual(
				AngelscriptTestJITProbes::ExecuteTypedASTScalarProviderRaw(
					Case.A, Case.B),
				Case.Expected,
				TEXT("The compiled TypedASTJIT raw entry should preserve scalar semantics")));
			ASSERT_THAT(AreEqual(
				AngelscriptTestJITProbes::ExecuteTypedASTScalarProviderVM(
					Case.A, Case.B),
				Case.Expected,
				TEXT("The compiled TypedASTJIT VM entry should preserve scalar frame mapping")));
			ASSERT_THAT(AreEqual(
				AngelscriptTestJITProbes::ExecuteTypedASTScalarProviderParms(
					Case.A, Case.B),
				Case.Expected,
				TEXT("The compiled TypedASTJIT Parms entry should preserve reflected parameter layout")));
		}
	}

	TEST_METHOD(IsolatedBackendsUseDistinctEntriesAndOneProductionRegistration)
	{
		const bool bHasBytecodeEntries =
			AngelscriptTestJITProbes::
				HasIsolatedBytecodeDifferentialEntries();
		const bool bHasTypedEntries =
			AngelscriptTestJITProbes::
				HasIsolatedTypedASTDifferentialEntries();
		ASSERT_THAT(IsTrue(
			bHasBytecodeEntries,
			TEXT("An independent BytecodeJIT generation task should register its test-only VM, Raw, and Parms entries")));
		ASSERT_THAT(IsTrue(
			bHasTypedEntries,
			TEXT("An independent TypedASTJIT generation task should register its test-only VM, Raw, and Parms entries")));
		if (!bHasBytecodeEntries || !bHasTypedEntries)
		{
			return;
		}

		const FString BytecodePrefix =
			AngelscriptTestJITProbes::
				GetIsolatedBytecodeDifferentialSymbolPrefix();
		const FString TypedPrefix =
			AngelscriptTestJITProbes::
				GetIsolatedTypedASTDifferentialSymbolPrefix();
		ASSERT_THAT(AreEqual(
			BytecodePrefix,
			FString(TEXT("AS_Test_Differential_Bytecode_DifferentialScalarValue"))));
		ASSERT_THAT(AreEqual(
			TypedPrefix,
			FString(TEXT("AS_Test_Differential_TypedAST_DifferentialScalarValue"))));
		ASSERT_THAT(IsTrue(
			BytecodePrefix != TypedPrefix,
			TEXT("Independent backends must retain content-distinct generated symbol sets")));
		const FString BytecodeFunctionKey =
			AngelscriptTestJITProbes::
				GetIsolatedBytecodeDifferentialFunctionKey();
		const FString TypedFunctionKey =
			AngelscriptTestJITProbes::
				GetIsolatedTypedASTDifferentialFunctionKey();
		ASSERT_THAT(AreEqual(BytecodeFunctionKey, TypedFunctionKey,
			TEXT("Independent backend artifacts must describe the same stable AS function")));
		ASSERT_THAT(AreEqual(BytecodeFunctionKey.Len(), 64,
			TEXT("The generated stable function key must be a complete hash")));
		ASSERT_THAT(IsTrue(
			AngelscriptTestJITProbes::
				GetIsolatedBytecodeDifferentialRawEntry()
				!= AngelscriptTestJITProbes::
					GetIsolatedTypedASTDifferentialRawEntry(),
			TEXT("Independent backends must compile distinct raw entry addresses")));

		struct FCase
		{
			int32 A;
			int32 B;
			int32 Expected;
		};
		const FCase Cases[] = {
			{3, 4, 6},
			{6, 7, 18},
			{-5, 2, -20},
			{10, 0, 23},
		};
		for (const FCase& Case : Cases)
		{
			ASSERT_THAT(AreEqual(
				AngelscriptTestJITProbes::
					ExecuteIsolatedBytecodeDifferentialRaw(Case.A, Case.B),
				Case.Expected));
			ASSERT_THAT(AreEqual(
				AngelscriptTestJITProbes::
					ExecuteIsolatedBytecodeDifferentialVM(Case.A, Case.B),
				Case.Expected));
			ASSERT_THAT(AreEqual(
				AngelscriptTestJITProbes::
					ExecuteIsolatedBytecodeDifferentialParms(Case.A, Case.B),
				Case.Expected));
			ASSERT_THAT(AreEqual(
				AngelscriptTestJITProbes::
					ExecuteIsolatedTypedASTDifferentialRaw(Case.A, Case.B),
				Case.Expected));
			ASSERT_THAT(AreEqual(
				AngelscriptTestJITProbes::
					ExecuteIsolatedTypedASTDifferentialVM(Case.A, Case.B),
				Case.Expected));
			ASSERT_THAT(AreEqual(
				AngelscriptTestJITProbes::
					ExecuteIsolatedTypedASTDifferentialParms(Case.A, Case.B),
				Case.Expected));
		}

		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Registry =
				FAngelscriptJITProviderRegistry::Get().GetSnapshot();
		ASSERT_THAT(IsTrue(Registry.IsValid()));
		int32 MatchingProductionEntries = 0;
		if (Registry.IsValid())
		{
			for (const TSharedPtr<const FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>& Provider : Registry->Providers)
			{
				if (!Provider.IsValid()
					|| Provider->ProviderId
						!= AngelscriptTestJITProbes::GetStableProviderId())
				{
					continue;
				}
				for (const FAngelscriptJITProviderCatalogEntry& Entry
					: Provider->Entries)
				{
					MatchingProductionEntries +=
						Entry.FunctionKey.Hash.ToHexString()
							== BytecodeFunctionKey ? 1 : 0;
				}
			}
		}
		ASSERT_THAT(AreEqual(
			MatchingProductionEntries,
			1,
			TEXT("Test-only differential artifacts must not register competing production Provider entries")));
	}

	TEST_METHOD(IsolatedImportedBindingTracksBindRebindUnbindWithoutTypedStaleCall)
	{
		using namespace AngelscriptTestJITProbes;
		const FStringView CanonicalDeclaration(
			TEXT("int SemanticImportedBinding(const int)"));
		FIsolatedDifferentialEntryView Bytecode;
		FIsolatedDifferentialEntryView Typed;
		const bool bFoundBytecode = FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::BytecodeJIT,
			CanonicalDeclaration,
			Bytecode);
		const bool bFoundTyped = FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::TypedASTJIT,
			CanonicalDeclaration,
			Typed);
		ASSERT_THAT(IsTrue(bFoundBytecode,
			TEXT("The imported-route differential requires an independently generated BytecodeJIT entry")));
		ASSERT_THAT(IsFalse(bFoundTyped,
			TEXT("TypedASTJIT must not publish a stale direct call while imported-slot lowering is unsupported")));
		if (!bFoundBytecode)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(Bytecode.VMEntry));
		ASSERT_THAT(IsNotNull(Bytecode.RawEntry));
		ASSERT_THAT(IsNotNull(Bytecode.ParmsEntry));

		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> InterpreterSession =
				AngelscriptStaticJITAotGeneration::
					CreateInterpreterFixtureEngineSession();
		ASSERT_THAT(IsNotNull(InterpreterSession.Get()));
		ASSERT_THAT(IsTrue(
			InterpreterSession.IsValid() && InterpreterSession->IsValid(),
			InterpreterSession.IsValid()
				? InterpreterSession->Error
				: TEXT("The imported-route interpreter session was not allocated")));
		if (!InterpreterSession.IsValid() || !InterpreterSession->IsValid())
		{
			return;
		}

		FAngelscriptEngine& Engine = *InterpreterSession->Engine;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const ConsumerFunction =
			FindMethodFunctionInModule(
				*TestRunner,
				Engine,
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarModuleName(),
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarReceiverClassName(),
				AngelscriptStaticJITAotFixture::
					GetSemanticImportedBindingDeclaration());
		const TSharedPtr<FAngelscriptModuleDesc> ConsumerModuleDesc =
			Engine.GetModuleByModuleName(
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarModuleName().ToString());
		const TSharedPtr<FAngelscriptModuleDesc> PrimaryProviderDesc =
			Engine.GetModuleByModuleName(
				AngelscriptStaticJITAotFixture::
					GetImportProviderModuleName().ToString());
		asIScriptModule* const ConsumerModule =
			ConsumerModuleDesc.IsValid()
				? ConsumerModuleDesc->ScriptModule : nullptr;
		asIScriptFunction* const PrimaryProviderFunction =
			PrimaryProviderDesc.IsValid()
				&& PrimaryProviderDesc->ScriptModule != nullptr
				? PrimaryProviderDesc->ScriptModule->GetFunctionByDecl(
					"int ImportedValueForAOT(int)")
				: nullptr;
		ASSERT_THAT(IsNotNull(ConsumerFunction));
		ASSERT_THAT(IsNotNull(ConsumerModule));
		ASSERT_THAT(IsNotNull(PrimaryProviderFunction));
		if (ConsumerFunction == nullptr || ConsumerModule == nullptr
			|| PrimaryProviderFunction == nullptr)
		{
			return;
		}

		const int32 ImportIndex =
			ConsumerModule->GetImportedFunctionIndexByDecl(
				"int ImportedValueForAOT(int)");
		ASSERT_THAT(IsTrue(ImportIndex >= 0,
			TEXT("The final-scalar consumer must retain one mutable imported slot")));
		if (ImportIndex < 0)
		{
			return;
		}

		FScopedAngelscriptModule ReplacementProvider(
			*TestRunner,
			Engine,
			TEXT("ASStaticJITAotImportReplacement"),
			ASTEST_AS(R"AS(
				int ImportedValueForAOT(int Value)
				{
					return Value + 177;
				}
			)AS"));
		ASSERT_THAT(IsTrue(ReplacementProvider.IsValid()));
		if (!ReplacementProvider.IsValid())
		{
			return;
		}
		asIScriptFunction* const ReplacementProviderFunction =
			ReplacementProvider.GetModule().GetFunctionByDecl(
				"int ImportedValueForAOT(int)");
		ASSERT_THAT(IsNotNull(ReplacementProviderFunction));
		if (ReplacementProviderFunction == nullptr)
		{
			return;
		}

		TArray<FAngelscriptArtifactReference> RequiredReferences;
		RequiredReferences.Reserve(Bytecode.ReferenceSlots.Num());
		for (const FAngelscriptJITReferenceSlot& Slot
			: Bytecode.ReferenceSlots)
		{
			RequiredReferences.Add(Slot.Reference);
		}
		ASSERT_THAT(IsTrue(!Bytecode.ReferenceSlots.IsEmpty(),
			TEXT("The generated import differential must expose its exact Provider reference slots")));
		const FAngelscriptJITCurrentEngineReferenceBuildResult ResolverResult =
			FAngelscriptJITCurrentEngineReferenceResolverBuilder::Build(
				Engine, RequiredReferences);
		ASSERT_THAT(IsTrue(ResolverResult.IsUsable(), ResolverResult.Detail));
		if (!ResolverResult.IsUsable())
		{
			return;
		}
		const FAngelscriptJITReferenceTableBuildResult ReferenceTableResult =
			FAngelscriptJITResolvedReferenceTableBuilder::Build(
				Bytecode.ReferenceSlots, *ResolverResult.Resolver);
		ASSERT_THAT(IsTrue(ReferenceTableResult.IsExact(),
			TEXT("The manual-import Engine must resolve the generated slot descriptors exactly")));
		if (!ReferenceTableResult.IsExact())
		{
			return;
		}
		const TSharedPtr<const FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> ReferenceTable = ReferenceTableResult.Table;

		TStrongObjectPtr<UAngelscriptUhtCoverageTestObject> Receiver(
			NewObject<UAngelscriptUhtCoverageTestObject>(GetTransientPackage()));
		struct FExecutionResult
		{
			int32 Status = asERROR;
			int32 Value = 0;
			FString Message;
		};
		auto ExecuteInterpreter = [ConsumerFunction, &Receiver](
			const int32 Input)
		{
			FExecutionResult Result;
			asIScriptContext* const Context =
				ConsumerFunction->GetEngine()->CreateContext();
			if (Context == nullptr)
			{
				return Result;
			}
			ON_SCOPE_EXIT { Context->Release(); };
			if (Context->Prepare(ConsumerFunction) < 0)
			{
				return Result;
			}
			static_cast<asCContext*>(Context)->m_executeVirtualCall = false;
			if (Context->SetObject(Receiver.Get()) < 0
				|| Context->SetArgDWord(
					0, static_cast<asDWORD>(Input)) < 0)
			{
				return Result;
			}
			{
				FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(
					true);
				Result.Status = Context->Execute();
			}
			if (Result.Status == asEXECUTION_FINISHED)
			{
				Result.Value = static_cast<int32>(Context->GetReturnDWord());
			}
			Result.Message = UTF8_TO_TCHAR(
				Context->GetExceptionString() != nullptr
					? Context->GetExceptionString() : "");
			return Result;
		};

		auto ExecuteGenerated = [&Engine, ConsumerFunction, &Receiver, &Bytecode,
			ReferenceTable](
			const EIsolatedDifferentialEntryRoute Route,
			const int32 Input)
		{
			FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			Execution.jitEngine = Engine.GetScriptEngine();
			Execution.jitFunction = ConsumerFunction;
			Execution.jitReferenceTable = ReferenceTable.Get();
			int32 Value = 0;
			if (Route == EIsolatedDifferentialEntryRoute::Raw)
			{
				using FRawEntry = int32(*)(
					FScriptExecution&, void*, asDWORD);
				Value = reinterpret_cast<FRawEntry>(Bytecode.RawEntry)(
					Execution,
					Receiver.Get(),
					static_cast<asDWORD>(Input));
			}
			else if (Route == EIsolatedDifferentialEntryRoute::VM)
			{
				struct alignas(void*) FFrame
				{
					void* Object = nullptr;
					int32 Input = 0;
				};
				FFrame Frame{Receiver.Get(), Input};
				asQWORD ReturnValue = 0;
				Bytecode.VMEntry(
					Execution,
					reinterpret_cast<asDWORD*>(&Frame),
					&ReturnValue);
				Value = static_cast<int32>(
					static_cast<asDWORD>(ReturnValue));
			}
			else
			{
				struct FParms
				{
					int32 Input = 0;
					int32 ReturnValue = 0;
				};
				FParms Parms{Input, 0};
				Bytecode.ParmsEntry(Execution, Receiver.Get(), &Parms);
				Value = Parms.ReturnValue;
			}

			FExecutionResult Result;
			Result.Status = Execution.bExceptionThrown
				? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED;
			Result.Value = Value;
			if (Execution.FailureRecord != nullptr
				&& Execution.FailureRecord->bHasFailure)
			{
				Result.Message = UTF8_TO_TCHAR(
					Execution.FailureRecord->Message.AddressOf());
			}
			return Result;
		};

		const EIsolatedDifferentialEntryRoute GeneratedRoutes[] = {
			EIsolatedDifferentialEntryRoute::Raw,
			EIsolatedDifferentialEntryRoute::VM,
			EIsolatedDifferentialEntryRoute::Parms,
		};
		auto VerifyBound = [this, ConsumerModule, ImportIndex,
			&ExecuteInterpreter, &ExecuteGenerated, &GeneratedRoutes](
			asIScriptFunction* const ProviderFunction,
			const int32 Expected,
			const TCHAR* const Label)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ConsumerModule->BindImportedFunction(
					static_cast<asUINT>(ImportIndex), ProviderFunction),
				Label));
			const FExecutionResult Interpreter = ExecuteInterpreter(5);
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Interpreter.Status,
				Label));
			ASSERT_THAT(AreEqual(Expected, Interpreter.Value, Label));
			for (const EIsolatedDifferentialEntryRoute Route : GeneratedRoutes)
			{
				const FExecutionResult Generated = ExecuteGenerated(Route, 5);
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_FINISHED),
					Generated.Status,
					Label));
				ASSERT_THAT(AreEqual(Expected, Generated.Value, Label));
			}
		};

		VerifyBound(PrimaryProviderFunction, 82, TEXT("primary bind"));
		VerifyBound(ReplacementProviderFunction, 182, TEXT("replacement bind"));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ConsumerModule->UnbindImportedFunction(
				static_cast<asUINT>(ImportIndex))));
		const FExecutionResult UnboundInterpreter = ExecuteInterpreter(5);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_EXCEPTION),
			UnboundInterpreter.Status));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Unbound function called")),
			UnboundInterpreter.Message));
		for (const EIsolatedDifferentialEntryRoute Route : GeneratedRoutes)
		{
			const FExecutionResult Generated = ExecuteGenerated(Route, 5);
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_EXCEPTION),
				Generated.Status));
			ASSERT_THAT(AreEqual(
				FString(TEXT("Unbound function called")),
				Generated.Message));
			ASSERT_THAT(AreEqual(0, Generated.Value,
				TEXT("An unbound generated route must not expose a stale provider result")));
		}
		VerifyBound(PrimaryProviderFunction, 82, TEXT("rebind after unbind"));
	}

	TEST_METHOD(IsolatedNativeAndPrivateRowsAreGeneratedButNonCloneableEffectsAreNot)
	{
		using namespace AngelscriptTestJITProbes;
		const FStringView ExpectedDeclarations[] = {
			TEXT("int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)"),
			TEXT("int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)"),
			TEXT("int TypedASTRuntimeThunkShowcase(const FString&in)"),
			TEXT("int TypedASTPrivateBridgeShowcase(const int, const int)"),
		};
		for (const FStringView Declaration : ExpectedDeclarations)
		{
			for (const EIsolatedDifferentialBackend Backend : {
				EIsolatedDifferentialBackend::BytecodeJIT,
				EIsolatedDifferentialBackend::TypedASTJIT})
			{
				FIsolatedDifferentialEntryView Entry;
				ASSERT_THAT(IsTrue(FindIsolatedDifferentialEntry(
					Backend, Declaration, Entry),
					*FString::Printf(TEXT(
						"The isolated differential registry must contain backend=%u declaration='%.*s'"),
						static_cast<uint32>(Backend),
						Declaration.Len(), Declaration.GetData())));
				ASSERT_THAT(IsNotNull(Entry.VMEntry));
				ASSERT_THAT(IsNotNull(Entry.RawEntry));
			}
		}

		for (const EIsolatedDifferentialBackend Backend : {
			EIsolatedDifferentialBackend::BytecodeJIT,
			EIsolatedDifferentialBackend::TypedASTJIT})
		{
			FIsolatedDifferentialEntryView RejectedEntry;
			ASSERT_THAT(IsFalse(FindIsolatedDifferentialEntry(
				Backend,
				TEXT("int ObjectLifetimeEntryForAOT()"),
				RejectedEntry),
				TEXT("A non-cloneable object-lifetime effect must be rejected before the differential runner can execute it")));
		}
	}

	TEST_METHOD(IsolatedNativeAndPrivateRoutesMatchFreshInterpreterState)
	{
		using namespace AngelscriptTestJITProbes;
		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> InterpreterSession =
				AngelscriptStaticJITAotGeneration::
					CreateCompleteInterpreterFixtureEngineSession();
		ASSERT_THAT(IsNotNull(InterpreterSession.Get()));
		ASSERT_THAT(IsTrue(
			InterpreterSession.IsValid() && InterpreterSession->IsValid(),
			InterpreterSession.IsValid()
				? InterpreterSession->Error
				: TEXT("The complete interpreter session was not allocated")));
		if (!InterpreterSession.IsValid() || !InterpreterSession->IsValid())
		{
			return;
		}

		FAngelscriptEngine& Engine = *InterpreterSession->Engine;
		UWorld* const WorldContext = GEditor != nullptr
			? GEditor->GetEditorWorldContext().World()
			: nullptr;
		ASSERT_THAT(IsNotNull(WorldContext));
		if (WorldContext == nullptr)
		{
			return;
		}
		FAngelscriptEngineScope EngineScope(Engine, WorldContext);
		struct FEntryPair
		{
			FIsolatedDifferentialEntryView Bytecode;
			FIsolatedDifferentialEntryView Typed;
		};
		auto FindPair = [this](const FStringView Declaration,
			FEntryPair& OutPair) -> bool
		{
			const bool bFoundBytecode = FindIsolatedDifferentialEntry(
				EIsolatedDifferentialBackend::BytecodeJIT,
				Declaration,
				OutPair.Bytecode);
			const bool bFoundTyped = FindIsolatedDifferentialEntry(
				EIsolatedDifferentialBackend::TypedASTJIT,
				Declaration,
				OutPair.Typed);
			TestRunner->TestTrue(
				*FString::Printf(TEXT(
					"BytecodeJIT should register isolated entry '%.*s'"),
					Declaration.Len(), Declaration.GetData()),
				bFoundBytecode);
			TestRunner->TestTrue(
				*FString::Printf(TEXT(
					"TypedASTJIT should register isolated entry '%.*s'"),
					Declaration.Len(), Declaration.GetData()),
				bFoundTyped);
			if (bFoundBytecode && bFoundTyped)
			{
				TestRunner->TestEqual(
					*FString::Printf(TEXT(
						"Isolated entry '%.*s' should share one stable function key"),
						Declaration.Len(), Declaration.GetData()),
					OutPair.Bytecode.StableFunctionKey,
					OutPair.Typed.StableFunctionKey);
			}
			return bFoundBytecode && bFoundTyped;
		};

		const EIsolatedDifferentialEntryRoute NativeRoutes[] = {
			EIsolatedDifferentialEntryRoute::Raw,
			EIsolatedDifferentialEntryRoute::VM,
		};
		const EIsolatedDifferentialEntryRoute TypedRoutes[] = {
			EIsolatedDifferentialEntryRoute::Raw,
			EIsolatedDifferentialEntryRoute::VM,
			EIsolatedDifferentialEntryRoute::Parms,
		};

		// HeaderInline: both generated backends must match a fresh interpreter
		// result, while TypedASTJIT owns the reflected Parms entry.
		FEntryPair Capability;
		const FStringView CapabilityDeclaration(TEXT(
			"int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)"));
		if (!FindPair(CapabilityDeclaration, Capability))
		{
			return;
		}
		asCScriptFunction* const CapabilityFunction = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::GetModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedASTCapabilityShowcaseDeclaration());
		ASSERT_THAT(IsNotNull(CapabilityFunction));
		if (CapabilityFunction == nullptr)
		{
			return;
		}
		int32 CapabilityInterpreter = 0;
		ASSERT_THAT(IsTrue(ExecuteInterpreterInt(
			*TestRunner,
			*CapabilityFunction,
			TEXT("HeaderInline capability oracle"),
			[](asIScriptContext& Context)
			{
				return Context.SetArgDWord(0, 6u) >= 0
					&& Context.SetArgDWord(1, 7u) >= 0
					&& Context.SetArgByte(2, 1u) >= 0
					&& Context.SetArgByte(3, 1u) >= 0;
			},
			CapabilityInterpreter)));
		auto ExecuteCapability = [&Engine, CapabilityFunction](
			const FIsolatedDifferentialEntryView& Entry,
			const TSharedPtr<const FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>& ReferenceTable,
			const EIsolatedDifferentialEntryRoute Route)
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			Execution.jitEngine = Engine.GetScriptEngine();
			Execution.jitFunction = CapabilityFunction;
			Execution.jitReferenceTable = ReferenceTable.Get();
			if (Route == EIsolatedDifferentialEntryRoute::Raw)
			{
				using FRawEntry = int32(*)(
					FScriptExecution&, asDWORD, asDWORD, asBYTE, asBYTE);
				return reinterpret_cast<FRawEntry>(Entry.RawEntry)(
					Execution, 6u, 7u, 1u, 1u);
			}
			if (Route == EIsolatedDifferentialEntryRoute::VM)
			{
				asDWORD Frame[4] = {6u, 7u, 1u, 1u};
				asQWORD ReturnValue = 0;
				Entry.VMEntry(Execution, Frame, &ReturnValue);
				return static_cast<int32>(static_cast<asDWORD>(ReturnValue));
			}
			struct alignas(void*) FParms
			{
				int32 A = 0;
				int32 B = 0;
				bool bEnabled = false;
				bool bExtraCheck = false;
				uint8 Padding[6] = {};
				void* GeneratedWorldContext = nullptr;
				int32 ReturnValue = 0;
			};
			static_assert(STRUCT_OFFSET(FParms, GeneratedWorldContext) == 16);
			static_assert(STRUCT_OFFSET(FParms, ReturnValue) == 24);
			FParms Parms{6, 7, true, true};
			Entry.ParmsEntry(Execution, nullptr, &Parms);
			return Parms.ReturnValue;
		};
		const auto CapabilityBytecodeReferences = ResolveIsolatedEntryReferences(
			*TestRunner, Engine, Capability.Bytecode);
		const auto CapabilityTypedReferences = ResolveIsolatedEntryReferences(
			*TestRunner, Engine, Capability.Typed);
		for (const EIsolatedDifferentialEntryRoute Route : NativeRoutes)
		{
			ASSERT_THAT(AreEqual(
				CapabilityInterpreter,
				ExecuteCapability(
					Capability.Bytecode,
					CapabilityBytecodeReferences,
					Route)));
		}
		for (const EIsolatedDifferentialEntryRoute Route : TypedRoutes)
		{
			ASSERT_THAT(AreEqual(
				CapabilityInterpreter,
				ExecuteCapability(
					Capability.Typed,
					CapabilityTypedReferences,
					Route)));
		}

		// DirectExported: reset the exported Print counter for every route so
		// equivalence cannot come from shared state accumulated by an earlier run.
		FEntryPair Print;
		const FStringView PrintDeclaration(TEXT(
			"int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)"));
		if (!FindPair(PrintDeclaration, Print))
		{
			return;
		}
		asCScriptFunction* const PrintFunction = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::GetTypedPrintModuleName(),
			AngelscriptStaticJITAotFixture::GetTypedASTPrintShowcaseDeclaration());
		ASSERT_THAT(IsNotNull(PrintFunction));
		if (PrintFunction == nullptr)
		{
			return;
		}
		FString PrintText(TEXT("isolated exported Print"));
		const double PrintDuration = 0.0;
		FLinearColor PrintColor(0.125f, 0.25f, 0.5f, 1.0f);
		AngelscriptStaticJITNativeCallables::ResetPrintCallCountForTesting();
		int32 PrintInterpreter = 0;
		ASSERT_THAT(IsTrue(ExecuteInterpreterInt(
			*TestRunner,
			*PrintFunction,
			TEXT("DirectExported Print oracle"),
			[&PrintText, PrintDuration, &PrintColor](asIScriptContext& Context)
			{
				return Context.SetArgAddress(0, &PrintText) >= 0
					&& Context.SetArgDouble(1, PrintDuration) >= 0
					&& Context.SetArgAddress(2, &PrintColor) >= 0;
			},
			PrintInterpreter)));
		ASSERT_THAT(AreEqual(314, PrintInterpreter));
		ASSERT_THAT(AreEqual(
			1,
			AngelscriptStaticJITNativeCallables::GetPrintCallCountForTesting()));
		auto ExecutePrint = [&Engine, PrintFunction, &PrintText, PrintDuration,
			&PrintColor](
			const FIsolatedDifferentialEntryView& Entry,
			const TSharedPtr<const FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>& ReferenceTable,
			const EIsolatedDifferentialEntryRoute Route)
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			Execution.jitEngine = Engine.GetScriptEngine();
			Execution.jitFunction = PrintFunction;
			Execution.jitReferenceTable = ReferenceTable.Get();
			if (Route == EIsolatedDifferentialEntryRoute::Raw)
			{
				using FRawEntry = int32(*)(
					FScriptExecution&, FString*, double, FLinearColor*);
				return reinterpret_cast<FRawEntry>(Entry.RawEntry)(
					Execution, &PrintText, PrintDuration, &PrintColor);
			}
			if (Route == EIsolatedDifferentialEntryRoute::VM)
			{
				alignas(8) asDWORD Frame[6] = {};
				*reinterpret_cast<FString**>(&Frame[0]) = &PrintText;
				*reinterpret_cast<double*>(&Frame[2]) = PrintDuration;
				*reinterpret_cast<FLinearColor**>(&Frame[4]) = &PrintColor;
				asQWORD ReturnValue = 0;
				Entry.VMEntry(Execution, Frame, &ReturnValue);
				return static_cast<int32>(static_cast<asDWORD>(ReturnValue));
			}
			struct FParms
			{
				FString Text;
				double Duration = 0.0;
				FLinearColor Color = FLinearColor::White;
				void* GeneratedWorldContext = nullptr;
				int32 ReturnValue = 0;
			};
			FParms Parms{PrintText, PrintDuration, PrintColor, nullptr, 0};
			Entry.ParmsEntry(Execution, nullptr, &Parms);
			return Parms.ReturnValue;
		};
		const auto PrintBytecodeReferences = ResolveIsolatedEntryReferences(
			*TestRunner, Engine, Print.Bytecode);
		const auto PrintTypedReferences = ResolveIsolatedEntryReferences(
			*TestRunner, Engine, Print.Typed);
		auto VerifyPrintRoute = [this, PrintInterpreter, &ExecutePrint](
			const FIsolatedDifferentialEntryView& Entry,
			const TSharedPtr<const FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>& Table,
			const EIsolatedDifferentialEntryRoute Route)
		{
			AngelscriptStaticJITNativeCallables::ResetPrintCallCountForTesting();
			ASSERT_THAT(AreEqual(PrintInterpreter, ExecutePrint(Entry, Table, Route)));
			ASSERT_THAT(AreEqual(
				1,
				AngelscriptStaticJITNativeCallables::GetPrintCallCountForTesting()));
		};
		for (const EIsolatedDifferentialEntryRoute Route : NativeRoutes)
		{
			VerifyPrintRoute(Print.Bytecode, PrintBytecodeReferences, Route);
		}
		for (const EIsolatedDifferentialEntryRoute Route : TypedRoutes)
		{
			VerifyPrintRoute(Print.Typed, PrintTypedReferences, Route);
		}

		// RuntimeThunk: interpreter/Bytecode consume the current registered
		// target, while TypedASTJIT intentionally calls the exported DLL wrapper.
		FEntryPair RuntimeThunk;
		const FStringView RuntimeThunkDeclaration(
			TEXT("int TypedASTRuntimeThunkShowcase(const FString&in)"));
		if (!FindPair(RuntimeThunkDeclaration, RuntimeThunk))
		{
			return;
		}
		asCScriptFunction* const RuntimeThunkFunction = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::GetTypedPrivateBridgeModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedASTRuntimeThunkShowcaseDeclaration());
		ASSERT_THAT(IsNotNull(RuntimeThunkFunction));
		if (RuntimeThunkFunction == nullptr)
		{
			return;
		}
		FString RuntimeThunkText(TEXT("runtime-thunk"));
		AngelscriptStaticJITAotFixture::ResetTypedASTRuntimeThunkObservation();
		int32 RuntimeThunkInterpreter = 0;
		ASSERT_THAT(IsTrue(ExecuteInterpreterInt(
			*TestRunner,
			*RuntimeThunkFunction,
			TEXT("RuntimeThunk interpreter oracle"),
			[&RuntimeThunkText](asIScriptContext& Context)
			{
				return Context.SetArgAddress(0, &RuntimeThunkText) >= 0;
			},
			RuntimeThunkInterpreter)));
		ASSERT_THAT(AreEqual(
			1,
			AngelscriptStaticJITAotFixture::
				GetTypedASTRuntimeThunkRegisteredCallCount()));
		ASSERT_THAT(AreEqual(
			0,
			AngelscriptStaticJITNativeCallables::
				GetRuntimeThunkProbeCountForTesting()));
		auto ExecuteRuntimeThunk = [&Engine, RuntimeThunkFunction,
			&RuntimeThunkText](
			const FIsolatedDifferentialEntryView& Entry,
			const TSharedPtr<const FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>& ReferenceTable,
			const EIsolatedDifferentialEntryRoute Route)
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			Execution.jitEngine = Engine.GetScriptEngine();
			Execution.jitFunction = RuntimeThunkFunction;
			Execution.jitReferenceTable = ReferenceTable.Get();
			if (Route == EIsolatedDifferentialEntryRoute::Raw)
			{
				using FRawEntry = int32(*)(FScriptExecution&, FString*);
				return reinterpret_cast<FRawEntry>(Entry.RawEntry)(
					Execution, &RuntimeThunkText);
			}
			if (Route == EIsolatedDifferentialEntryRoute::VM)
			{
				alignas(void*) asDWORD Frame[AS_PTR_SIZE] = {};
				*reinterpret_cast<FString**>(&Frame[0]) = &RuntimeThunkText;
				asQWORD ReturnValue = 0;
				Entry.VMEntry(Execution, Frame, &ReturnValue);
				return static_cast<int32>(static_cast<asDWORD>(ReturnValue));
			}
			struct FParms
			{
				FString Text;
				void* GeneratedWorldContext = nullptr;
				int32 ReturnValue = 0;
			};
			FParms Parms{RuntimeThunkText, nullptr, 0};
			Entry.ParmsEntry(Execution, nullptr, &Parms);
			return Parms.ReturnValue;
		};
		const auto RuntimeThunkBytecodeReferences =
			ResolveIsolatedEntryReferences(*TestRunner, Engine, RuntimeThunk.Bytecode);
		const auto RuntimeThunkTypedReferences =
			ResolveIsolatedEntryReferences(*TestRunner, Engine, RuntimeThunk.Typed);
		for (const EIsolatedDifferentialEntryRoute Route : NativeRoutes)
		{
			AngelscriptStaticJITAotFixture::ResetTypedASTRuntimeThunkObservation();
			ASSERT_THAT(AreEqual(
				RuntimeThunkInterpreter,
				ExecuteRuntimeThunk(
					RuntimeThunk.Bytecode,
					RuntimeThunkBytecodeReferences,
					Route)));
			ASSERT_THAT(AreEqual(
				1,
				AngelscriptStaticJITAotFixture::
					GetTypedASTRuntimeThunkRegisteredCallCount()));
			ASSERT_THAT(AreEqual(
				0,
				AngelscriptStaticJITNativeCallables::
					GetRuntimeThunkProbeCountForTesting()));
		}
		for (const EIsolatedDifferentialEntryRoute Route : TypedRoutes)
		{
			AngelscriptStaticJITAotFixture::ResetTypedASTRuntimeThunkObservation();
			ASSERT_THAT(AreEqual(
				RuntimeThunkInterpreter,
				ExecuteRuntimeThunk(
					RuntimeThunk.Typed,
					RuntimeThunkTypedReferences,
					Route)));
			ASSERT_THAT(AreEqual(
				0,
				AngelscriptStaticJITAotFixture::
					GetTypedASTRuntimeThunkRegisteredCallCount()));
			ASSERT_THAT(AreEqual(
				1,
				AngelscriptStaticJITNativeCallables::
					GetRuntimeThunkProbeCountForTesting()));
		}

		// Provider-private bridge: every route reads the current Engine slot and
		// therefore reaches the registered primary function exactly once.
		FEntryPair PrivateBridge;
		const FStringView PrivateBridgeDeclaration(
			TEXT("int TypedASTPrivateBridgeShowcase(const int, const int)"));
		if (!FindPair(PrivateBridgeDeclaration, PrivateBridge))
		{
			return;
		}
		asCScriptFunction* const PrivateBridgeFunction = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::GetTypedPrivateBridgeModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedASTPrivateBridgeShowcaseDeclaration());
		ASSERT_THAT(IsNotNull(PrivateBridgeFunction));
		if (PrivateBridgeFunction == nullptr)
		{
			return;
		}
		AngelscriptStaticJITAotFixture::ResetTypedASTPrivateBridgeObservation();
		int32 PrivateBridgeInterpreter = 0;
		ASSERT_THAT(IsTrue(ExecuteInterpreterInt(
			*TestRunner,
			*PrivateBridgeFunction,
			TEXT("provider-private interpreter oracle"),
			[](asIScriptContext& Context)
			{
				return Context.SetArgDWord(0, 19u) >= 0
					&& Context.SetArgDWord(1, 23u) >= 0;
			},
			PrivateBridgeInterpreter)));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::GetExpectedTypedASTPrivateBridgeResult(),
			PrivateBridgeInterpreter));
		ASSERT_THAT(AreEqual(
			1,
			AngelscriptStaticJITAotFixture::GetTypedASTPrivateBridgeCallCount()));
		auto ExecutePrivateBridge = [&Engine, PrivateBridgeFunction](
			const FIsolatedDifferentialEntryView& Entry,
			const TSharedPtr<const FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>& ReferenceTable,
			const EIsolatedDifferentialEntryRoute Route)
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			Execution.jitEngine = Engine.GetScriptEngine();
			Execution.jitFunction = PrivateBridgeFunction;
			Execution.jitReferenceTable = ReferenceTable.Get();
			if (Route == EIsolatedDifferentialEntryRoute::Raw)
			{
				using FRawEntry = int32(*)(
					FScriptExecution&, asDWORD, asDWORD);
				return reinterpret_cast<FRawEntry>(Entry.RawEntry)(
					Execution, 19u, 23u);
			}
			if (Route == EIsolatedDifferentialEntryRoute::VM)
			{
				asDWORD Frame[2] = {19u, 23u};
				asQWORD ReturnValue = 0;
				Entry.VMEntry(Execution, Frame, &ReturnValue);
				return static_cast<int32>(static_cast<asDWORD>(ReturnValue));
			}
			struct alignas(void*) FParms
			{
				int32 A = 0;
				int32 B = 0;
				void* GeneratedWorldContext = nullptr;
				int32 ReturnValue = 0;
			};
			static_assert(STRUCT_OFFSET(FParms, GeneratedWorldContext) == 8);
			static_assert(STRUCT_OFFSET(FParms, ReturnValue) == 16);
			FParms Parms{19, 23, nullptr, 0};
			Entry.ParmsEntry(Execution, nullptr, &Parms);
			return Parms.ReturnValue;
		};
		const auto PrivateBytecodeReferences = ResolveIsolatedEntryReferences(
			*TestRunner, Engine, PrivateBridge.Bytecode);
		const auto PrivateTypedReferences = ResolveIsolatedEntryReferences(
			*TestRunner, Engine, PrivateBridge.Typed);
		auto VerifyPrivateRoute = [this, PrivateBridgeInterpreter,
			&ExecutePrivateBridge](
			const FIsolatedDifferentialEntryView& Entry,
			const TSharedPtr<const FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>& Table,
			const EIsolatedDifferentialEntryRoute Route)
		{
			AngelscriptStaticJITAotFixture::ResetTypedASTPrivateBridgeObservation();
			ASSERT_THAT(AreEqual(
				PrivateBridgeInterpreter,
				ExecutePrivateBridge(Entry, Table, Route)));
			ASSERT_THAT(AreEqual(
				1,
				AngelscriptStaticJITAotFixture::
					GetTypedASTPrivateBridgeCallCount()));
			ASSERT_THAT(AreEqual(
				0,
				AngelscriptStaticJITAotFixture::
					GetTypedASTPrivateBridgeReplacementCallCount()));
		};
		for (const EIsolatedDifferentialEntryRoute Route : NativeRoutes)
		{
			VerifyPrivateRoute(
				PrivateBridge.Bytecode, PrivateBytecodeReferences, Route);
		}
		for (const EIsolatedDifferentialEntryRoute Route : TypedRoutes)
		{
			VerifyPrivateRoute(
				PrivateBridge.Typed, PrivateTypedReferences, Route);
		}
	}

	TEST_METHOD(IsolatedEnumBackendsPreserveRawVmAndReflectedParms)
	{
		using namespace AngelscriptTestJITProbes;
		const FStringView Declaration(
			TEXT("int SemanticExhaustiveEnum(const int)"));
		FIsolatedDifferentialEntryView Bytecode;
		FIsolatedDifferentialEntryView Typed;
		ASSERT_THAT(IsTrue(FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::BytecodeJIT,
			Declaration,
			Bytecode),
			TEXT("The independent BytecodeJIT artifact should register the exhaustive-enum function")));
		ASSERT_THAT(IsTrue(FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::TypedASTJIT,
			Declaration,
			Typed),
			TEXT("The independent TypedASTJIT artifact should register the exhaustive-enum function")));
		if (Bytecode.RawEntry == nullptr || Typed.RawEntry == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(Bytecode.StableFunctionKey,
			Typed.StableFunctionKey));
		ASSERT_THAT(IsTrue(Bytecode.RawEntry != Typed.RawEntry));
		ASSERT_THAT(IsTrue(Bytecode.VMEntry != nullptr));
		ASSERT_THAT(IsTrue(Bytecode.ParmsEntry == nullptr,
			TEXT("BytecodeJIT must fail closed instead of exposing a Parms entry that cannot represent the generated WorldContext suffix")));
		ASSERT_THAT(IsTrue(Typed.VMEntry != nullptr));
		ASSERT_THAT(IsTrue(Typed.ParmsEntry != nullptr,
			TEXT("TypedASTJIT should expose the reflected Parms layout including the generated WorldContext suffix")));

		using FRawEntry = int32(*)(FScriptExecution&, asDWORD);
		struct alignas(void*) FParms
		{
			int32 RawMode = 0;
			uint32 PointerAlignmentPadding = 0;
			void* GeneratedWorldContext = nullptr;
			int32 ReturnValue = 0;
		};
		static_assert(STRUCT_OFFSET(FParms, RawMode) == 0);
		static_assert(STRUCT_OFFSET(FParms, GeneratedWorldContext) == 8);
		static_assert(STRUCT_OFFSET(FParms, ReturnValue) == 16);

		auto ExecuteRaw = [](const FIsolatedDifferentialEntryView& Entry,
			const int32 RawMode)
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			return reinterpret_cast<FRawEntry>(Entry.RawEntry)(
				Execution, static_cast<asDWORD>(RawMode));
		};
		auto ExecuteVM = [](const FIsolatedDifferentialEntryView& Entry,
			const int32 RawMode)
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			asDWORD Frame = static_cast<asDWORD>(RawMode);
			asQWORD Result = 0;
			Entry.VMEntry(Execution, &Frame, &Result);
			return static_cast<int32>(static_cast<asDWORD>(Result));
		};
		auto ExecuteParms = [](const FIsolatedDifferentialEntryView& Entry,
			const int32 RawMode)
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			FParms Parms;
			Parms.RawMode = RawMode;
			Entry.ParmsEntry(Execution, nullptr, &Parms);
			return Parms.ReturnValue;
		};

		struct FCase
		{
			int32 RawMode;
			int32 Expected;
		};
		const FCase Cases[] = {{0, 10}, {1, 20}, {2, 30}};
		for (const FCase& Case : Cases)
		{
			ASSERT_THAT(AreEqual(ExecuteRaw(Bytecode, Case.RawMode), Case.Expected));
			ASSERT_THAT(AreEqual(ExecuteVM(Bytecode, Case.RawMode), Case.Expected));
			ASSERT_THAT(AreEqual(ExecuteRaw(Typed, Case.RawMode), Case.Expected));
			ASSERT_THAT(AreEqual(ExecuteVM(Typed, Case.RawMode), Case.Expected));
			ASSERT_THAT(AreEqual(ExecuteParms(Typed, Case.RawMode), Case.Expected));
		}
	}

	TEST_METHOD(IsolatedCheckedIntegerBackendsExposeIndependentEntries)
	{
		using namespace AngelscriptTestJITProbes;
		const FStringView EnumDeclaration(
			TEXT("int SemanticExhaustiveEnum(const int)"));
		const FStringView DivideDeclaration(
			TEXT("int SemanticCheckedDivide(const int, const int)"));
		FIsolatedDifferentialEntryView BytecodeEnum;
		FIsolatedDifferentialEntryView TypedEnum;
		FIsolatedDifferentialEntryView BytecodeDivide;
		FIsolatedDifferentialEntryView TypedDivide;
		ASSERT_THAT(IsTrue(FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::BytecodeJIT,
			EnumDeclaration,
			BytecodeEnum)));
		ASSERT_THAT(IsTrue(FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::TypedASTJIT,
			EnumDeclaration,
			TypedEnum)));
		ASSERT_THAT(IsTrue(FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::BytecodeJIT,
			DivideDeclaration,
			BytecodeDivide),
			TEXT("The independent BytecodeJIT artifact should register the checked-integer function")));
		ASSERT_THAT(IsTrue(FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::TypedASTJIT,
			DivideDeclaration,
			TypedDivide),
			TEXT("The independent TypedASTJIT artifact should register the checked-integer function")));
		if (BytecodeEnum.RawEntry == nullptr
			|| TypedEnum.RawEntry == nullptr
			|| BytecodeDivide.RawEntry == nullptr
			|| TypedDivide.RawEntry == nullptr)
		{
			return;
		}

		const FString ExpectedSection(
			TEXT("/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as"));
		for (const FIsolatedDifferentialEntryView* const Entry : {
			&BytecodeEnum, &TypedEnum, &BytecodeDivide, &TypedDivide})
		{
			ASSERT_THAT(AreEqual(Entry->SourceSection, ExpectedSection));
			ASSERT_THAT(IsTrue(Entry->SourceLine > 0));
			ASSERT_THAT(IsTrue(Entry->SourceColumn > 0));
			ASSERT_THAT(AreEqual(Entry->CleanupActionCount, 0u));
		}
		ASSERT_THAT(AreEqual(
			BytecodeEnum.CleanupPlanState,
			EIsolatedDifferentialCleanupPlanState::NotCaptured));
		ASSERT_THAT(AreEqual(
			BytecodeDivide.CleanupPlanState,
			EIsolatedDifferentialCleanupPlanState::NotCaptured));
		ASSERT_THAT(AreEqual(
			TypedEnum.CleanupPlanState,
			EIsolatedDifferentialCleanupPlanState::VerifiedEmpty));
		ASSERT_THAT(AreEqual(
			TypedDivide.CleanupPlanState,
			EIsolatedDifferentialCleanupPlanState::VerifiedEmpty));
		ASSERT_THAT(IsNull(BytecodeDivide.ParmsEntry));
		ASSERT_THAT(IsNotNull(TypedDivide.ParmsEntry));

		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> InterpreterSession =
				AngelscriptStaticJITAotGeneration::
					CreateInterpreterFixtureEngineSession();
		ASSERT_THAT(IsNotNull(InterpreterSession.Get()));
		ASSERT_THAT(IsTrue(
			InterpreterSession.IsValid() && InterpreterSession->IsValid(),
			InterpreterSession.IsValid()
				? InterpreterSession->Error
				: TEXT("The isolated interpreter session was not allocated")));
		if (!InterpreterSession.IsValid() || !InterpreterSession->IsValid())
		{
			return;
		}

		FAngelscriptEngine& Engine = *InterpreterSession->Engine;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const EnumFunction = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::GetTypedControlFlowModuleName(),
			AngelscriptStaticJITAotFixture::
				GetSemanticExhaustiveEnumDeclaration());
		asCScriptFunction* const DivideFunction = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::GetTypedControlFlowModuleName(),
			AngelscriptStaticJITAotFixture::
				GetSemanticCheckedDivideDeclaration());
		ASSERT_THAT(IsNotNull(EnumFunction));
		ASSERT_THAT(IsNotNull(DivideFunction));
		if (EnumFunction == nullptr || DivideFunction == nullptr)
		{
			return;
		}

		auto ExecuteOneArgument = [&Engine](
			const FIsolatedDifferentialEntryView& Entry,
			const EIsolatedDifferentialExecutionBackend Backend,
			const EIsolatedDifferentialEntryRoute Route,
			asCScriptFunction& Function,
			const int32 Argument)
		{
			FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			Execution.jitEngine = Engine.GetScriptEngine();
			Execution.jitFunction = &Function;
			int32 Value = 0;
			if (Route == EIsolatedDifferentialEntryRoute::Raw)
			{
				using FRawEntry = int32(*)(FScriptExecution&, asDWORD);
				Value = reinterpret_cast<FRawEntry>(Entry.RawEntry)(
					Execution, static_cast<asDWORD>(Argument));
			}
			else if (Route == EIsolatedDifferentialEntryRoute::VM)
			{
				asDWORD Frame = static_cast<asDWORD>(Argument);
				asQWORD ReturnValue = 0;
				Entry.VMEntry(Execution, &Frame, &ReturnValue);
				Value = static_cast<int32>(
					static_cast<asDWORD>(ReturnValue));
			}
			else
			{
				struct alignas(void*) FParms
				{
					int32 Argument = 0;
					uint32 PointerAlignmentPadding = 0;
					void* GeneratedWorldContext = nullptr;
					int32 ReturnValue = 0;
				};
				FParms Parms;
				Parms.Argument = Argument;
				Entry.ParmsEntry(Execution, nullptr, &Parms);
				Value = Parms.ReturnValue;
			}
			return MakeDirectFailureResult(
				Backend, Route, Entry, Execution, Value);
		};

		auto ExecuteTwoArguments = [&Engine](
			const FIsolatedDifferentialEntryView& Entry,
			const EIsolatedDifferentialExecutionBackend Backend,
			const EIsolatedDifferentialEntryRoute Route,
			asCScriptFunction& Function,
			const int32 Numerator,
			const int32 Denominator)
		{
			FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			Execution.jitEngine = Engine.GetScriptEngine();
			Execution.jitFunction = &Function;
			int32 Value = 0;
			if (Route == EIsolatedDifferentialEntryRoute::Raw)
			{
				using FRawEntry = int32(*)(
					FScriptExecution&, asDWORD, asDWORD);
				Value = reinterpret_cast<FRawEntry>(Entry.RawEntry)(
					Execution,
					static_cast<asDWORD>(Numerator),
					static_cast<asDWORD>(Denominator));
			}
			else if (Route == EIsolatedDifferentialEntryRoute::VM)
			{
				asDWORD Frame[] = {
					static_cast<asDWORD>(Numerator),
					static_cast<asDWORD>(Denominator),
				};
				asQWORD ReturnValue = 0;
				Entry.VMEntry(Execution, Frame, &ReturnValue);
				Value = static_cast<int32>(
					static_cast<asDWORD>(ReturnValue));
			}
			else
			{
				struct alignas(void*) FParms
				{
					int32 Numerator = 0;
					int32 Denominator = 0;
					void* GeneratedWorldContext = nullptr;
					int32 ReturnValue = 0;
				};
				FParms Parms;
				Parms.Numerator = Numerator;
				Parms.Denominator = Denominator;
				Entry.ParmsEntry(Execution, nullptr, &Parms);
				Value = Parms.ReturnValue;
			}
			return MakeDirectFailureResult(
				Backend, Route, Entry, Execution, Value);
		};

		FExceptionRouteObservation EnumInterpreterObservation;
		const asDWORD EnumArguments[] = {99};
		ASSERT_THAT(IsTrue(ObserveExceptionContext(
			*TestRunner,
			Engine,
			EnumFunction,
			EnumArguments,
			TEXT("isolated interpreter exhaustive-enum failure"),
			EnumInterpreterObservation)));
		TArray<FIsolatedDifferentialRouteResult> EnumResults{
			MakeInterpreterFailureResult(
				Engine, *EnumFunction, EnumInterpreterObservation),
			ExecuteOneArgument(BytecodeEnum,
				EIsolatedDifferentialExecutionBackend::BytecodeJIT,
				EIsolatedDifferentialEntryRoute::Raw,
				*EnumFunction, 99),
			ExecuteOneArgument(BytecodeEnum,
				EIsolatedDifferentialExecutionBackend::BytecodeJIT,
				EIsolatedDifferentialEntryRoute::VM,
				*EnumFunction, 99),
			ExecuteOneArgument(TypedEnum,
				EIsolatedDifferentialExecutionBackend::TypedASTJIT,
				EIsolatedDifferentialEntryRoute::Raw,
				*EnumFunction, 99),
			ExecuteOneArgument(TypedEnum,
				EIsolatedDifferentialExecutionBackend::TypedASTJIT,
				EIsolatedDifferentialEntryRoute::VM,
				*EnumFunction, 99),
			ExecuteOneArgument(TypedEnum,
				EIsolatedDifferentialExecutionBackend::TypedASTJIT,
				EIsolatedDifferentialEntryRoute::Parms,
				*EnumFunction, 99),
		};

		FExceptionRouteObservation DivideInterpreterObservation;
		const asDWORD DivideArguments[] = {12, 0};
		ASSERT_THAT(IsTrue(ObserveExceptionContext(
			*TestRunner,
			Engine,
			DivideFunction,
			DivideArguments,
			TEXT("isolated interpreter checked-integer failure"),
			DivideInterpreterObservation)));
		TArray<FIsolatedDifferentialRouteResult> DivideResults{
			MakeInterpreterFailureResult(
				Engine, *DivideFunction, DivideInterpreterObservation),
			ExecuteTwoArguments(BytecodeDivide,
				EIsolatedDifferentialExecutionBackend::BytecodeJIT,
				EIsolatedDifferentialEntryRoute::Raw,
				*DivideFunction, 12, 0),
			ExecuteTwoArguments(BytecodeDivide,
				EIsolatedDifferentialExecutionBackend::BytecodeJIT,
				EIsolatedDifferentialEntryRoute::VM,
				*DivideFunction, 12, 0),
			ExecuteTwoArguments(TypedDivide,
				EIsolatedDifferentialExecutionBackend::TypedASTJIT,
				EIsolatedDifferentialEntryRoute::Raw,
				*DivideFunction, 12, 0),
			ExecuteTwoArguments(TypedDivide,
				EIsolatedDifferentialExecutionBackend::TypedASTJIT,
				EIsolatedDifferentialEntryRoute::VM,
				*DivideFunction, 12, 0),
			ExecuteTwoArguments(TypedDivide,
				EIsolatedDifferentialExecutionBackend::TypedASTJIT,
				EIsolatedDifferentialEntryRoute::Parms,
				*DivideFunction, 12, 0),
		};

		auto ValidateFailureRows = [this](
			const TConstArrayView<FIsolatedDifferentialRouteResult> Results,
			const FStringView ExpectedMessage,
			const FStringView ExpectedFunctionKey)
		{
			ASSERT_THAT(AreEqual(Results.Num(), 6));
			const FIsolatedDifferentialFailurePayload& Baseline =
				Results[0].Failure;
			ASSERT_THAT(IsTrue(Baseline.Row > 0));
			ASSERT_THAT(IsTrue(Baseline.Column > 0));
			ASSERT_THAT(IsTrue(
				FStringView(Baseline.Message).Equals(
					ExpectedMessage, ESearchCase::CaseSensitive)));
			ASSERT_THAT(IsTrue(
				FStringView(Baseline.StableFunctionIdentity).Equals(
					ExpectedFunctionKey, ESearchCase::CaseSensitive)));
			for (const FIsolatedDifferentialRouteResult& Result : Results)
			{
				const bool bInterpreter = Result.Backend ==
					EIsolatedDifferentialExecutionBackend::InterpreterVM;
				const bool bTyped = Result.Backend ==
					EIsolatedDifferentialExecutionBackend::TypedASTJIT;
				ASSERT_THAT(AreEqual(
					Result.ExecuteStatus, asEXECUTION_EXCEPTION));
				ASSERT_THAT(IsTrue(Result.bExceptionThrown));
				ASSERT_THAT(AreEqual(Result.Value, 0));
				ASSERT_THAT(IsTrue(Result.Failure.bHasFailure));
				ASSERT_THAT(AreEqual(
					Result.Failure.bFromJITRecord, !bInterpreter));
				ASSERT_THAT(IsTrue(Result.Failure.bReported));
				ASSERT_THAT(IsFalse(Result.Failure.bAdopted));
				ASSERT_THAT(AreEqual(
					Result.Failure.Message, Baseline.Message));
				ASSERT_THAT(AreEqual(
					Result.Failure.StableFunctionIdentity,
					Baseline.StableFunctionIdentity));
				ASSERT_THAT(AreEqual(
					Result.Failure.FunctionDeclaration,
					Baseline.FunctionDeclaration));
				ASSERT_THAT(AreEqual(
					Result.Failure.Section, Baseline.Section));
				ASSERT_THAT(AreEqual(
					Result.Failure.Row, Baseline.Row));
				ASSERT_THAT(AreEqual(
					Result.Failure.Column, Baseline.Column));
				ASSERT_THAT(AreEqual(
					Result.Failure.RouteOrigin,
					bInterpreter ? FString(TEXT("vm"))
						: FString(TEXT("generated-native-call"))));
				ASSERT_THAT(AreEqual(
					Result.Failure.BackendOrigin,
					bInterpreter ? FString(TEXT("vm"))
						: FString(TEXT("static-jit"))));
				ASSERT_THAT(AreEqual(
					Result.CleanupPlanState,
					bTyped
						? EIsolatedDifferentialCleanupPlanState::
							VerifiedEmpty
						: EIsolatedDifferentialCleanupPlanState::
							NotCaptured));
				ASSERT_THAT(AreEqual(Result.CleanupActionCount, 0u));
				ASSERT_THAT(AreEqual(Result.CleanupTrace.Num(), 0));
			}
		};

		ValidateFailureRows(
			EnumResults,
			TEXT("Invalid enum value passed to switch"),
			BytecodeEnum.StableFunctionKey);
		ValidateFailureRows(
			DivideResults,
			TEXT("Divide by zero"),
			BytecodeDivide.StableFunctionKey);
	}

	TEST_METHOD(IsolatedCloneableStatePreservesEvaluationAndControlFlow)
	{
		using namespace AngelscriptTestJITProbes;
		const FStringView Declaration(
			TEXT("int SemanticCloneableState(const int, const int, const bool, const bool)"));
		FIsolatedDifferentialEntryView Bytecode;
		FIsolatedDifferentialEntryView Typed;
		const bool bFoundBytecode = FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::BytecodeJIT,
			Declaration,
			Bytecode);
		const bool bFoundTyped = FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::TypedASTJIT,
			Declaration,
			Typed);
		ASSERT_THAT(IsTrue(
			bFoundBytecode,
			TEXT("The independent BytecodeJIT artifact should register the cloneable-state function")));
		ASSERT_THAT(IsTrue(
			bFoundTyped,
			TEXT("The independent TypedASTJIT artifact should register the cloneable-state function")));
		if (!bFoundBytecode || !bFoundTyped)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			Bytecode.StableFunctionKey,
			Typed.StableFunctionKey));
		ASSERT_THAT(IsNotNull(Bytecode.VMEntry));
		ASSERT_THAT(IsNotNull(Bytecode.RawEntry));
		ASSERT_THAT(IsNull(Bytecode.ParmsEntry));
		ASSERT_THAT(IsNotNull(Typed.VMEntry));
		ASSERT_THAT(IsNotNull(Typed.RawEntry));
		ASSERT_THAT(IsNotNull(Typed.ParmsEntry));

		struct FCloneableDifferentialState
		{
			int32 Seed = 0;
			int32 Mode = 0;
			bool bLeft = false;
			bool bRight = false;
			int32 ObservedValue = MIN_int32;

			FCloneableDifferentialState CloneForRoute() const
			{
				FCloneableDifferentialState Clone = *this;
				Clone.ObservedValue = MIN_int32;
				return Clone;
			}
		};
		struct FCase
		{
			FCloneableDifferentialState InitialState;
			int32 Expected;
		};
		const FCase Cases[] = {
			{{0, 0, false, false}, 3123},
			{{5, 0, false, false}, 8678},
			{{4, 1, false, true}, 40},
			{{4, 1, true, false}, 50},
			{{4, 1, true, true}, 51},
			{{2, 2, false, false}, 766},
			{{-4, 2, false, false}, 100},
			{{5, 3, false, false}, 40},
			{{5, 14, false, false}, 553},
			{{-1, 15, false, false}, -10},
			{{0, 15, false, false}, 0},
			{{5, 15, true, true}, 11},
			{{5, 15, true, false}, 12},
			{{5, 15, false, true}, 13},
			{{5, 15, false, false}, 14},
			{{0, 16, false, false}, 21},
			{{1, 16, false, false}, 21},
			{{2, 16, false, false}, 32},
		};

		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> InterpreterSession =
				AngelscriptStaticJITAotGeneration::
					CreateInterpreterFixtureEngineSession();
		ASSERT_THAT(IsNotNull(InterpreterSession.Get()));
		ASSERT_THAT(IsTrue(
			InterpreterSession.IsValid() && InterpreterSession->IsValid(),
			InterpreterSession.IsValid()
				? InterpreterSession->Error
				: TEXT("The cloneable-state interpreter session was not allocated")));
		if (!InterpreterSession.IsValid() || !InterpreterSession->IsValid())
		{
			return;
		}

		FAngelscriptEngine& Engine = *InterpreterSession->Engine;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const InterpreterFunction =
			FindGlobalFunctionInModule(
				*TestRunner,
				Engine,
				AngelscriptStaticJITAotFixture::
					GetTypedControlFlowModuleName(),
				AngelscriptStaticJITAotFixture::
					GetSemanticCloneableStateDeclaration());
		ASSERT_THAT(IsNotNull(InterpreterFunction));
		if (InterpreterFunction == nullptr)
		{
			return;
		}

		auto ExecuteInterpreter = [InterpreterFunction](
			FCloneableDifferentialState State)
		{
			asIScriptContext* const Context =
				InterpreterFunction->GetEngine()->CreateContext();
			if (Context == nullptr)
			{
				return State;
			}
			ON_SCOPE_EXIT { Context->Release(); };
			if (Context->Prepare(InterpreterFunction) >= 0
				&& Context->SetArgDWord(
					0, static_cast<asDWORD>(State.Seed)) >= 0
				&& Context->SetArgDWord(
					1, static_cast<asDWORD>(State.Mode)) >= 0
				&& Context->SetArgByte(2, State.bLeft ? 1 : 0) >= 0
				&& Context->SetArgByte(3, State.bRight ? 1 : 0) >= 0
				&& Context->Execute() == asEXECUTION_FINISHED)
			{
				State.ObservedValue = static_cast<int32>(
					Context->GetReturnDWord());
			}
			return State;
		};

		auto ExecuteGenerated = [&Engine, InterpreterFunction](
			const FIsolatedDifferentialEntryView& Entry,
			const EIsolatedDifferentialEntryRoute Route,
			FCloneableDifferentialState State)
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			Execution.jitEngine = Engine.GetScriptEngine();
			Execution.jitFunction = InterpreterFunction;
			if (Route == EIsolatedDifferentialEntryRoute::Raw)
			{
				using FRawEntry = int32(*)(FScriptExecution&,
					asDWORD, asDWORD, asBYTE, asBYTE);
				State.ObservedValue =
					reinterpret_cast<FRawEntry>(Entry.RawEntry)(
						Execution,
						static_cast<asDWORD>(State.Seed),
						static_cast<asDWORD>(State.Mode),
						State.bLeft ? 1 : 0,
						State.bRight ? 1 : 0);
			}
			else if (Route == EIsolatedDifferentialEntryRoute::VM)
			{
				asDWORD Frame[] = {
					static_cast<asDWORD>(State.Seed),
					static_cast<asDWORD>(State.Mode),
					State.bLeft ? 1u : 0u,
					State.bRight ? 1u : 0u,
				};
				asQWORD ReturnValue = 0;
				Entry.VMEntry(Execution, Frame, &ReturnValue);
				State.ObservedValue = static_cast<int32>(
					static_cast<asDWORD>(ReturnValue));
			}
			else
			{
				struct alignas(void*) FParms
				{
					int32 Seed = 0;
					int32 Mode = 0;
					bool bLeft = false;
					bool bRight = false;
					uint8 Padding[6] = {};
					void* GeneratedWorldContext = nullptr;
					int32 ReturnValue = 0;
				};
				static_assert(STRUCT_OFFSET(FParms, Seed) == 0);
				static_assert(STRUCT_OFFSET(FParms, Mode) == 4);
				static_assert(STRUCT_OFFSET(FParms, bLeft) == 8);
				static_assert(STRUCT_OFFSET(FParms, bRight) == 9);
				static_assert(
					STRUCT_OFFSET(FParms, GeneratedWorldContext) == 16);
				static_assert(STRUCT_OFFSET(FParms, ReturnValue) == 24);
				FParms Parms;
				Parms.Seed = State.Seed;
				Parms.Mode = State.Mode;
				Parms.bLeft = State.bLeft;
				Parms.bRight = State.bRight;
				Entry.ParmsEntry(Execution, nullptr, &Parms);
				State.ObservedValue = Parms.ReturnValue;
			}
			return State;
		};

		for (const FCase& Case : Cases)
		{
			const FCloneableDifferentialState Interpreter =
				ExecuteInterpreter(Case.InitialState.CloneForRoute());
			const FCloneableDifferentialState BytecodeRaw =
				ExecuteGenerated(
					Bytecode,
					EIsolatedDifferentialEntryRoute::Raw,
					Case.InitialState.CloneForRoute());
			const FCloneableDifferentialState BytecodeVM =
				ExecuteGenerated(
					Bytecode,
					EIsolatedDifferentialEntryRoute::VM,
					Case.InitialState.CloneForRoute());
			const FCloneableDifferentialState TypedRaw =
				ExecuteGenerated(
					Typed,
					EIsolatedDifferentialEntryRoute::Raw,
					Case.InitialState.CloneForRoute());
			const FCloneableDifferentialState TypedVM =
				ExecuteGenerated(
					Typed,
					EIsolatedDifferentialEntryRoute::VM,
					Case.InitialState.CloneForRoute());
			const FCloneableDifferentialState TypedParms =
				ExecuteGenerated(
					Typed,
					EIsolatedDifferentialEntryRoute::Parms,
					Case.InitialState.CloneForRoute());

			ASSERT_THAT(AreEqual(Case.Expected, Interpreter.ObservedValue));
			ASSERT_THAT(AreEqual(Case.Expected, BytecodeRaw.ObservedValue));
			ASSERT_THAT(AreEqual(Case.Expected, BytecodeVM.ObservedValue));
			ASSERT_THAT(AreEqual(Case.Expected, TypedRaw.ObservedValue));
			ASSERT_THAT(AreEqual(Case.Expected, TypedVM.ObservedValue));
			ASSERT_THAT(AreEqual(Case.Expected, TypedParms.ObservedValue));
			ASSERT_THAT(AreEqual(MIN_int32,
				Case.InitialState.ObservedValue,
				TEXT("Each differential route must consume a clone instead of sharing the source state")));
		}
	}

	TEST_METHOD(IsolatedNumericBoundariesMatchInterpreter)
	{
		using namespace AngelscriptTestJITProbes;
		const FStringView Declaration(
			TEXT("int SemanticNumericBoundary(const int, const int, const int, const int)"));
		FIsolatedDifferentialEntryView Bytecode;
		FIsolatedDifferentialEntryView Typed;
		const bool bFoundBytecode = FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::BytecodeJIT,
			Declaration,
			Bytecode);
		const bool bFoundTyped = FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::TypedASTJIT,
			Declaration,
			Typed);
		ASSERT_THAT(IsTrue(bFoundBytecode,
			TEXT("The independent BytecodeJIT artifact should register the numeric-boundary function")));
		ASSERT_THAT(IsTrue(bFoundTyped,
			TEXT("The independent TypedASTJIT artifact should register the numeric-boundary function")));
		if (!bFoundBytecode || !bFoundTyped)
		{
			return;
		}

		ASSERT_THAT(AreEqual(Bytecode.StableFunctionKey,
			Typed.StableFunctionKey));
		ASSERT_THAT(IsNotNull(Bytecode.VMEntry));
		ASSERT_THAT(IsNotNull(Bytecode.RawEntry));
		ASSERT_THAT(IsNotNull(Bytecode.ParmsEntry));
		ASSERT_THAT(IsNotNull(Typed.VMEntry));
		ASSERT_THAT(IsNotNull(Typed.RawEntry));
		ASSERT_THAT(IsNotNull(Typed.ParmsEntry));

		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> InterpreterSession =
				AngelscriptStaticJITAotGeneration::
					CreateInterpreterFixtureEngineSession();
		ASSERT_THAT(IsNotNull(InterpreterSession.Get()));
		ASSERT_THAT(IsTrue(
			InterpreterSession.IsValid() && InterpreterSession->IsValid(),
			InterpreterSession.IsValid()
				? InterpreterSession->Error
				: TEXT("The numeric-boundary interpreter session was not allocated")));
		if (!InterpreterSession.IsValid() || !InterpreterSession->IsValid())
		{
			return;
		}

		FAngelscriptEngine& Engine = *InterpreterSession->Engine;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const InterpreterFunction =
			FindMethodFunctionInModule(
				*TestRunner,
				Engine,
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarModuleName(),
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarReceiverClassName(),
				AngelscriptStaticJITAotFixture::
					GetSemanticNumericBoundaryDeclaration());
		ASSERT_THAT(IsNotNull(InterpreterFunction));
		if (InterpreterFunction == nullptr)
		{
			return;
		}
		TStrongObjectPtr<UAngelscriptUhtCoverageTestObject> Receiver(
			NewObject<UAngelscriptUhtCoverageTestObject>(GetTransientPackage()));

		struct FInterpreterResult
		{
			int32 Value = 0;
			int32 Status = asERROR;
			FString Message;
		};
		auto ExecuteInterpreter = [InterpreterFunction, &Receiver](
			const int32 Left,
			const int32 Right,
			const int32 ShiftCount,
			const int32 Mode)
		{
			FInterpreterResult Result;
			asIScriptContext* const Context =
				InterpreterFunction->GetEngine()->CreateContext();
			if (Context == nullptr)
			{
				return Result;
			}
			ON_SCOPE_EXIT { Context->Release(); };
			if (Context->Prepare(InterpreterFunction) < 0)
			{
				return Result;
			}
			static_cast<asCContext*>(Context)->m_executeVirtualCall = false;
			if (Context->SetObject(Receiver.Get()) < 0
				|| Context->SetArgDWord(0, static_cast<asDWORD>(Left)) < 0
				|| Context->SetArgDWord(1, static_cast<asDWORD>(Right)) < 0
				|| Context->SetArgDWord(
					2, static_cast<asDWORD>(ShiftCount)) < 0
				|| Context->SetArgDWord(3, static_cast<asDWORD>(Mode)) < 0)
			{
				return Result;
			}
			{
				FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
				Result.Status = Context->Execute();
			}
			if (Result.Status == asEXECUTION_FINISHED)
			{
				Result.Value = static_cast<int32>(Context->GetReturnDWord());
			}
			Result.Message = UTF8_TO_TCHAR(
				Context->GetExceptionString() != nullptr
					? Context->GetExceptionString() : "");
			return Result;
		};

		auto ExecuteGenerated = [&Engine, InterpreterFunction, &Receiver](
			const FIsolatedDifferentialEntryView& Entry,
			const EIsolatedDifferentialExecutionBackend Backend,
			const EIsolatedDifferentialEntryRoute Route,
			const int32 Left,
			const int32 Right,
			const int32 ShiftCount,
			const int32 Mode)
		{
			FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			Execution.jitEngine = Engine.GetScriptEngine();
			Execution.jitFunction = InterpreterFunction;
			int32 Value = 0;
			if (Route == EIsolatedDifferentialEntryRoute::Raw)
			{
				using FRawEntry = int32(*)(FScriptExecution&, void*,
					asDWORD, asDWORD, asDWORD, asDWORD);
				Value = reinterpret_cast<FRawEntry>(Entry.RawEntry)(
					Execution,
					Receiver.Get(),
					static_cast<asDWORD>(Left),
					static_cast<asDWORD>(Right),
					static_cast<asDWORD>(ShiftCount),
					static_cast<asDWORD>(Mode));
			}
			else if (Route == EIsolatedDifferentialEntryRoute::VM)
			{
				struct alignas(void*) FFrame
				{
					void* Object = nullptr;
					int32 Left = 0;
					int32 Right = 0;
					int32 ShiftCount = 0;
					int32 Mode = 0;
				};
				FFrame Frame{Receiver.Get(), Left, Right, ShiftCount, Mode};
				asQWORD ReturnValue = 0;
				Entry.VMEntry(Execution,
					reinterpret_cast<asDWORD*>(&Frame), &ReturnValue);
				Value = static_cast<int32>(static_cast<asDWORD>(ReturnValue));
			}
			else
			{
				struct FParms
				{
					int32 Left = 0;
					int32 Right = 0;
					int32 ShiftCount = 0;
					int32 Mode = 0;
					int32 ReturnValue = 0;
				};
				FParms Parms{Left, Right, ShiftCount, Mode, 0};
				Entry.ParmsEntry(Execution, Receiver.Get(), &Parms);
				Value = Parms.ReturnValue;
			}
			return MakeDirectFailureResult(
				Backend, Route, Entry, Execution, Value);
		};

		struct FRoute
		{
			const FIsolatedDifferentialEntryView* Entry;
			EIsolatedDifferentialExecutionBackend Backend;
			EIsolatedDifferentialEntryRoute Route;
		};
		const FRoute Routes[] = {
			{&Bytecode, EIsolatedDifferentialExecutionBackend::BytecodeJIT,
				EIsolatedDifferentialEntryRoute::Raw},
			{&Bytecode, EIsolatedDifferentialExecutionBackend::BytecodeJIT,
				EIsolatedDifferentialEntryRoute::VM},
			{&Bytecode, EIsolatedDifferentialExecutionBackend::BytecodeJIT,
				EIsolatedDifferentialEntryRoute::Parms},
			{&Typed, EIsolatedDifferentialExecutionBackend::TypedASTJIT,
				EIsolatedDifferentialEntryRoute::Raw},
			{&Typed, EIsolatedDifferentialExecutionBackend::TypedASTJIT,
				EIsolatedDifferentialEntryRoute::VM},
			{&Typed, EIsolatedDifferentialExecutionBackend::TypedASTJIT,
				EIsolatedDifferentialEntryRoute::Parms},
		};

		struct FSuccessCase
		{
			int32 Left;
			int32 Right;
			int32 ShiftCount;
			int32 Mode;
			int32 Expected;
			const TCHAR* Label;
		};
		const FSuccessCase SuccessCases[] = {
			{MAX_int32, 1, 0, 0, MIN_int32, TEXT("wrapping add")},
			{MIN_int32, 1, 0, 1, MAX_int32, TEXT("wrapping subtract")},
			{0x40000000, 4, 0, 2, 0, TEXT("wrapping multiply")},
			{-7, 3, 0, 3, -2, TEXT("truncating divide")},
			{-7, 3, 0, 4, -1, TEXT("signed remainder")},
			{1, 0, -1, 5, MIN_int32, TEXT("negative masked left shift")},
			{1, 0, 32, 5, 1, TEXT("word-size masked left shift")},
			{1, 0, 33, 5, 2, TEXT("large masked left shift")},
			{-1, 0, 1, 6, MAX_int32, TEXT("logical right shift")},
			{-1, 0, 32, 6, -1, TEXT("masked logical right shift")},
			{MIN_int32, 0, 1, 7, -1073741824,
				TEXT("arithmetic sign-fill shift")},
			{-1, 0, 100000, 7, -1, TEXT("large masked arithmetic shift")},
			{MIN_int32, 0, 0, 8, MIN_int32,
				TEXT("signed integer widening")},
			{0, 0, 0, 9, 0, TEXT("false normalization")},
			{-1, 0, 0, 9, 1, TEXT("true normalization")},
			{41, 0, 0, 10, 7042, TEXT("folded pure global")},
		};
		for (const FSuccessCase& Case : SuccessCases)
		{
			const FInterpreterResult Interpreter = ExecuteInterpreter(
				Case.Left, Case.Right, Case.ShiftCount, Case.Mode);
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,
				Interpreter.Status, Case.Label));
			ASSERT_THAT(AreEqual(Case.Expected,
				Interpreter.Value, Case.Label));
			for (const FRoute& Route : Routes)
			{
				const FIsolatedDifferentialRouteResult Result =
					ExecuteGenerated(*Route.Entry, Route.Backend, Route.Route,
						Case.Left, Case.Right, Case.ShiftCount, Case.Mode);
				ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,
					Result.ExecuteStatus, Case.Label));
				ASSERT_THAT(IsFalse(Result.bExceptionThrown, Case.Label));
				ASSERT_THAT(AreEqual(Case.Expected, Result.Value, Case.Label));
			}
		}

		const int32 OverflowModes[] = {3, 4};
		for (const int32 Mode : OverflowModes)
		{
			const FInterpreterResult Interpreter = ExecuteInterpreter(
				MIN_int32, -1, 0, Mode);
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION,
				Interpreter.Status));
			ASSERT_THAT(AreEqual(
				FString(TEXT("Overflow in integer division")),
				Interpreter.Message));
			for (const FRoute& Route : Routes)
			{
				const FIsolatedDifferentialRouteResult Result =
					ExecuteGenerated(*Route.Entry, Route.Backend, Route.Route,
						MIN_int32, -1, 0, Mode);
				ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION,
					Result.ExecuteStatus));
				ASSERT_THAT(IsTrue(Result.bExceptionThrown));
				ASSERT_THAT(AreEqual(0, Result.Value));
				ASSERT_THAT(AreEqual(
					FString(TEXT("Overflow in integer division")),
					Result.Failure.Message));
			}
		}
	}

	TEST_METHOD(IsolatedPowerBoundariesMatchInterpreter)
	{
		using namespace AngelscriptTestJITProbes;
		const FStringView Declaration(
			TEXT("float SemanticPowerBoundary(const float, const float)"));
		FIsolatedDifferentialEntryView Bytecode;
		FIsolatedDifferentialEntryView Typed;
		const bool bFoundBytecode = FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::BytecodeJIT, Declaration, Bytecode);
		const bool bFoundTyped = FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::TypedASTJIT, Declaration, Typed);
		ASSERT_THAT(IsTrue(bFoundBytecode));
		ASSERT_THAT(IsTrue(bFoundTyped));
		if (!bFoundBytecode || !bFoundTyped)
		{
			return;
		}
		ASSERT_THAT(AreEqual(Bytecode.StableFunctionKey,
			Typed.StableFunctionKey));
		ASSERT_THAT(IsNotNull(Bytecode.VMEntry));
		ASSERT_THAT(IsNotNull(Bytecode.RawEntry));
		ASSERT_THAT(IsNotNull(Bytecode.ParmsEntry));
		ASSERT_THAT(IsNotNull(Typed.VMEntry));
		ASSERT_THAT(IsNotNull(Typed.RawEntry));
		ASSERT_THAT(IsNotNull(Typed.ParmsEntry));

		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> InterpreterSession =
				AngelscriptStaticJITAotGeneration::
					CreateInterpreterFixtureEngineSession();
		ASSERT_THAT(IsNotNull(InterpreterSession.Get()));
		ASSERT_THAT(IsTrue(
			InterpreterSession.IsValid() && InterpreterSession->IsValid(),
			InterpreterSession.IsValid()
				? InterpreterSession->Error
				: TEXT("The power-boundary interpreter session was not allocated")));
		if (!InterpreterSession.IsValid() || !InterpreterSession->IsValid())
		{
			return;
		}

		FAngelscriptEngine& Engine = *InterpreterSession->Engine;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const InterpreterFunction =
			FindMethodFunctionInModule(
				*TestRunner,
				Engine,
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarModuleName(),
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarReceiverClassName(),
				AngelscriptStaticJITAotFixture::
					GetSemanticPowerBoundaryDeclaration());
		ASSERT_THAT(IsNotNull(InterpreterFunction));
		if (InterpreterFunction == nullptr)
		{
			return;
		}
		TStrongObjectPtr<UAngelscriptUhtCoverageTestObject> Receiver(
			NewObject<UAngelscriptUhtCoverageTestObject>(GetTransientPackage()));

		struct FPowerResult
		{
			double Value = 0.0;
			int32 Status = asERROR;
			FString Message;
		};
		auto ExecuteInterpreter = [InterpreterFunction, &Receiver](
			const double Base,
			const double Exponent)
		{
			FPowerResult Result;
			asIScriptContext* const Context =
				InterpreterFunction->GetEngine()->CreateContext();
			if (Context == nullptr)
			{
				return Result;
			}
			ON_SCOPE_EXIT { Context->Release(); };
			if (Context->Prepare(InterpreterFunction) < 0)
			{
				return Result;
			}
			static_cast<asCContext*>(Context)->m_executeVirtualCall = false;
			if (Context->SetObject(Receiver.Get()) < 0
				|| Context->SetArgDouble(0, Base) < 0
				|| Context->SetArgDouble(1, Exponent) < 0)
			{
				return Result;
			}
			{
				FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
				Result.Status = Context->Execute();
			}
			if (Result.Status == asEXECUTION_FINISHED)
			{
				Result.Value = Context->GetReturnDouble();
			}
			Result.Message = UTF8_TO_TCHAR(
				Context->GetExceptionString() != nullptr
					? Context->GetExceptionString() : "");
			return Result;
		};

		auto ExecuteGenerated = [&Engine, InterpreterFunction, &Receiver](
			const FIsolatedDifferentialEntryView& Entry,
			const EIsolatedDifferentialEntryRoute Route,
			const double Base,
			const double Exponent)
		{
			FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
			FPowerResult Result;
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			Execution.jitEngine = Engine.GetScriptEngine();
			Execution.jitFunction = InterpreterFunction;
			if (Route == EIsolatedDifferentialEntryRoute::Raw)
			{
				using FRawEntry = double(*)(FScriptExecution&, void*,
					double, double);
				Result.Value = reinterpret_cast<FRawEntry>(Entry.RawEntry)(
					Execution, Receiver.Get(), Base, Exponent);
			}
			else if (Route == EIsolatedDifferentialEntryRoute::VM)
			{
				struct alignas(double) FFrame
				{
					void* Object = nullptr;
					double Base = 0.0;
					double Exponent = 0.0;
				};
				FFrame Frame{Receiver.Get(), Base, Exponent};
				asQWORD ReturnValue = 0;
				Entry.VMEntry(Execution,
					reinterpret_cast<asDWORD*>(&Frame), &ReturnValue);
				FMemory::Memcpy(&Result.Value,
					&ReturnValue, sizeof(Result.Value));
			}
			else
			{
				struct FParms
				{
					double Base = 0.0;
					double Exponent = 0.0;
					double ReturnValue = 0.0;
				};
				FParms Parms{Base, Exponent, 0.0};
				Entry.ParmsEntry(Execution, Receiver.Get(), &Parms);
				Result.Value = Parms.ReturnValue;
			}
			Result.Status = Execution.bExceptionThrown
				? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED;
			if (Execution.FailureRecord != nullptr
				&& Execution.FailureRecord->bHasFailure)
			{
				Result.Message = UTF8_TO_TCHAR(
					Execution.FailureRecord->Message.AddressOf());
			}
			return Result;
		};

		struct FRoute
		{
			const FIsolatedDifferentialEntryView* Entry;
			EIsolatedDifferentialEntryRoute Route;
		};
		const FRoute Routes[] = {
			{&Bytecode, EIsolatedDifferentialEntryRoute::Raw},
			{&Bytecode, EIsolatedDifferentialEntryRoute::VM},
			{&Bytecode, EIsolatedDifferentialEntryRoute::Parms},
			{&Typed, EIsolatedDifferentialEntryRoute::Raw},
			{&Typed, EIsolatedDifferentialEntryRoute::VM},
			{&Typed, EIsolatedDifferentialEntryRoute::Parms},
		};
		auto DoubleBits = [](const double Value)
		{
			uint64 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			return Bits;
		};
		enum class EExpectedPowerValue : uint8
		{
			ExactBits,
			NaN,
		};
		struct FSuccessCase
		{
			double Base;
			double Exponent;
			double Expected;
			EExpectedPowerValue Kind;
			const TCHAR* Label;
		};
		const FSuccessCase SuccessCases[] = {
			{-0.0, 3.0, -0.0, EExpectedPowerValue::ExactBits,
				TEXT("negative zero")},
			{std::numeric_limits<double>::denorm_min(), 1.0,
				std::numeric_limits<double>::denorm_min(),
				EExpectedPowerValue::ExactBits, TEXT("subnormal")},
			{std::numeric_limits<double>::quiet_NaN(), 1.0, 0.0,
				EExpectedPowerValue::NaN, TEXT("NaN")},
			{std::numeric_limits<double>::infinity(), -1.0, 0.0,
				EExpectedPowerValue::ExactBits, TEXT("positive infinity reciprocal")},
			{-1.0, 0.5, 0.0, EExpectedPowerValue::NaN,
				TEXT("negative base fractional exponent")},
			{-std::numeric_limits<double>::infinity(), 3.0,
				-std::numeric_limits<double>::infinity(),
				EExpectedPowerValue::ExactBits, TEXT("negative infinity")},
		};
		auto ValueMatches = [DoubleBits](
			const FSuccessCase& Case,
			const double Value)
		{
			return Case.Kind == EExpectedPowerValue::NaN
				? FMath::IsNaN(Value)
				: DoubleBits(Case.Expected) == DoubleBits(Value);
		};
		for (const FSuccessCase& Case : SuccessCases)
		{
			const FPowerResult Interpreter =
				ExecuteInterpreter(Case.Base, Case.Exponent);
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,
				Interpreter.Status, Case.Label));
			ASSERT_THAT(IsTrue(
				ValueMatches(Case, Interpreter.Value), Case.Label));
			for (const FRoute& Route : Routes)
			{
				const FPowerResult Result = ExecuteGenerated(
					*Route.Entry, Route.Route, Case.Base, Case.Exponent);
				ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,
					Result.Status, Case.Label));
				ASSERT_THAT(IsTrue(
					ValueMatches(Case, Result.Value), Case.Label));
			}
		}

		const FPowerResult InterpreterOverflow =
			ExecuteInterpreter(2.0, 1024.0);
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION,
			InterpreterOverflow.Status));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Overflow in exponent operation")),
			InterpreterOverflow.Message));
		for (const FRoute& Route : Routes)
		{
			const FPowerResult Result = ExecuteGenerated(
				*Route.Entry, Route.Route, 2.0, 1024.0);
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Result.Status));
			ASSERT_THAT(AreEqual(
				FString(TEXT("Overflow in exponent operation")),
				Result.Message));
		}
	}

	TEST_METHOD(FoldedPureGlobalChangeRejectsCommittedProvider)
	{
		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> Session =
				AngelscriptStaticJITAotGeneration::
					CreateFoldedGlobalInvalidationFixtureEngineSession();
		ASSERT_THAT(IsNotNull(Session.Get()));
		ASSERT_THAT(IsTrue(
			Session.IsValid() && Session->IsValid(),
			Session.IsValid()
				? Session->Error
				: TEXT("The folded-global invalidation session was not allocated")));
		if (!Session.IsValid() || !Session->IsValid())
		{
			return;
		}

		FAngelscriptEngine& Engine = *Session->Engine;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const Function = FindMethodFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::
				GetTypedFinalScalarModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedFinalScalarReceiverClassName(),
			AngelscriptStaticJITAotFixture::
				GetSemanticNumericBoundaryDeclaration());
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		FAngelscriptStableFunctionKey FunctionKey;
		ASSERT_THAT(IsTrue(Engine.TryResolveStableFunctionKey(
			Function, FunctionKey)));
		if (FunctionKey.Hash.IsZero())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Providers =
				FAngelscriptJITProviderRegistry::Get().GetSnapshot();
		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>* const Provider = Providers.IsValid()
			? Providers->Providers.FindByPredicate(
				[](const auto& Candidate)
				{
					return Candidate.IsValid()
						&& Candidate->ProviderId
							== AngelscriptTestJITProbes::GetStableProviderId();
				})
			: nullptr;
		ASSERT_THAT(IsNotNull(Provider));
		const FAngelscriptJITProviderCatalogEntry* const CommittedEntry =
			Provider != nullptr && Provider->IsValid()
				? (*Provider)->Entries.FindByPredicate(
					[&FunctionKey](const auto& Candidate)
					{
						return Candidate.FunctionKey == FunctionKey;
					})
				: nullptr;
		ASSERT_THAT(IsNotNull(CommittedEntry));
		if (CommittedEntry == nullptr)
		{
			return;
		}

		FAngelscriptFunctionRoute Route;
		ASSERT_THAT(IsTrue(Engine.ResolveFunctionRoute(FunctionKey, Route)));
		ASSERT_THAT(IsTrue(Route.bHasVerifiedArtifactIdentity));
		ASSERT_THAT(IsTrue(Route.Identity.FunctionKey == FunctionKey));
		ASSERT_THAT(IsFalse(
			Route.Identity.Content.Execution == CommittedEntry->ExecutionHash,
			TEXT("Changing only the folded pure-global value must change the function execution identity")));
		const FAngelscriptJITProviderRouteRefreshResult DiagnosticRefresh =
			FAngelscriptJITProviderRouter::Refresh(Engine);
		ASSERT_THAT(IsTrue(
			DiagnosticRefresh.IsApplied(), DiagnosticRefresh.Detail));
		const FAngelscriptJITProviderRouteRecord* const ProviderRoute =
			DiagnosticRefresh.Routes.FindByPredicate(
				[&FunctionKey](
					const FAngelscriptJITProviderRouteRecord& Candidate)
				{
					return Candidate.FunctionKey == FunctionKey;
				});
		ASSERT_THAT(IsNotNull(ProviderRoute));
		ASSERT_THAT(IsTrue(
			ProviderRoute != nullptr
				&& ProviderRoute->MatchResult
				== EAngelscriptArtifactMatchResult::ContentMismatch,
			TEXT("The committed 7001 Provider entry should be rejected as a content mismatch for current value 9001")));
		ASSERT_THAT(IsTrue(
			ProviderRoute != nullptr && !ProviderRoute->bBindingPublished,
			TEXT("A content-mismatched Provider entry must not publish a binding")));
		ASSERT_THAT(IsFalse(Route.IsNative(),
			TEXT("A stale folded-global Provider must not become the current native route")));

		const asSJITFunctionBinding Binding = Function->GetJITBinding();
		ASSERT_THAT(IsNull(Binding.VMEntry));
		ASSERT_THAT(IsNull(Binding.RawEntry));
		ASSERT_THAT(IsNull(Binding.ParmsEntry));
		ASSERT_THAT(IsNull(
			FAngelscriptJITBindingContext::FromBindingUserData(
				Binding.UserData)));

		UClass* const GeneratedClass = FindGeneratedClass(
			&Engine,
			AngelscriptStaticJITAotFixture::
				GetTypedFinalScalarReceiverClassName());
		ASSERT_THAT(IsNotNull(GeneratedClass));
		if (GeneratedClass == nullptr)
		{
			return;
		}
		TStrongObjectPtr<UObject> Receiver(NewObject<UObject>(
			GetTransientPackage(), GeneratedClass));
		ASSERT_THAT(IsNotNull(Receiver.Get()));

		asIScriptContext* const Context =
			Engine.GetScriptEngine()->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr || Receiver.IsValid() == false)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetObject(Receiver.Get())));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Context->SetArgDWord(0, static_cast<asDWORD>(41))));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Context->SetArgDWord(1, static_cast<asDWORD>(0))));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Context->SetArgDWord(2, static_cast<asDWORD>(0))));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Context->SetArgDWord(3, static_cast<asDWORD>(10))));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(
			9042,
			static_cast<int32>(Context->GetReturnDWord()),
			TEXT("The current VM route must observe the changed folded global instead of executing stale generated C++")));
	}

	TEST_METHOD(TypedASTCapabilityShowcaseEntriesCompileLinkAndExecute)
	{
		const bool bHasEntries =
			AngelscriptTestJITProbes::HasTypedASTCapabilityProviderEntries();
		ASSERT_THAT(IsTrue(
			bHasEntries,
			TEXT("The generated TypedASTJIT capability showcase should register raw, VM, and Parms entries")));
		if (!bHasEntries)
		{
			return;
		}

		struct FCase
		{
			int32 A;
			int32 B;
			bool bEnabled;
			bool bExtraCheck;
			int32 Expected;
		};
		const FCase Cases[] = {
			{3, 4, false, true, 0},
			{3, 4, true, false, 1},
			{6, 7, true, true, 61},
			{10, 0, true, true, 49},
		};
		for (const FCase& Case : Cases)
		{
			ASSERT_THAT(AreEqual(
				AngelscriptTestJITProbes::ExecuteTypedASTCapabilityProviderRaw(
					Case.A, Case.B, Case.bEnabled, Case.bExtraCheck),
				Case.Expected,
				TEXT("The compiled capability raw entry should preserve combined scalar semantics")));
			ASSERT_THAT(AreEqual(
				AngelscriptTestJITProbes::ExecuteTypedASTCapabilityProviderVM(
					Case.A, Case.B, Case.bEnabled, Case.bExtraCheck),
				Case.Expected,
				TEXT("The compiled capability VM entry should preserve exact frame mapping")));
			ASSERT_THAT(AreEqual(
				AngelscriptTestJITProbes::ExecuteTypedASTCapabilityProviderParms(
					Case.A, Case.B, Case.bEnabled, Case.bExtraCheck),
				Case.Expected,
				TEXT("The compiled capability Parms entry should preserve reflected scalar layout")));
		}
	}

	TEST_METHOD(TypedASTCoreExportedSymbolCompilesLinksAndExecutes)
	{
		FAngelscriptEngine* const EnginePtr =
			GetSharedAotFixtureEngine(*TestRunner);
		ASSERT_THAT(IsNotNull(EnginePtr));
		if (EnginePtr == nullptr)
		{
			return;
		}

		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const Function = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::GetModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedASTExportedSymbolShowcaseDeclaration());
		uint32 FunctionId = 0;
		const bool bHasJitEntries = RequireJitEntries(
			*TestRunner,
			Engine,
			Function,
			TEXT("TypedAST Core exported FDateTime::DaysInMonth call"),
			FunctionId);
		ASSERT_THAT(IsTrue(bHasJitEntries));
		if (!bHasJitEntries || Function == nullptr)
		{
			return;
		}

		const asSJITFunctionBinding Binding = Function->GetJITBinding();
		using FRawEntry = int32(*)(FScriptExecution&, int32, int32);
		FScriptExecution RawExecution(asCThreadManager::GetLocalData());
		int32 RawResult = 0;
		ASSERT_THAT(IsTrue(FAngelscriptJITExecutionContext::InvokeRaw(
			RawExecution,
			Engine.GetScriptEngine(),
			Function,
			Binding,
			[&RawResult](asJITFunction_Raw RawEntry,
				FScriptExecution& Execution)
			{
				RawResult = reinterpret_cast<FRawEntry>(RawEntry)(
					Execution, 2024, 2);
			})));
		ASSERT_THAT(AreEqual(
			29,
			RawResult,
			TEXT("The generated raw entry should call the current Engine's native Bind")));

		asDWORD Frame[2] = {2024, 2};
		asQWORD VMResult = 0;
		FScriptExecution VMExecution(asCThreadManager::GetLocalData());
		ASSERT_THAT(IsTrue(FAngelscriptJITExecutionContext::InvokeVM(
			VMExecution,
			Engine.GetScriptEngine(),
			Function,
			Binding,
			Frame,
			&VMResult)));
		ASSERT_THAT(AreEqual(
			29,
			static_cast<int32>(static_cast<asDWORD>(VMResult)),
			TEXT("The generated VM entry should preserve exported-symbol scalar arguments")));

		struct FDaysInMonthParms
		{
			int32 Year = 0;
			int32 Month = 0;
			void* GeneratedWorldContext = nullptr;
			int32 ReturnValue = 0;
		};
		FDaysInMonthParms Parms{2024, 2, nullptr, 0};
		FScriptExecution ParmsExecution(asCThreadManager::GetLocalData());
		ASSERT_THAT(IsTrue(FAngelscriptJITExecutionContext::InvokeParms(
			ParmsExecution,
			Engine.GetScriptEngine(),
			Function,
			Binding,
			nullptr,
			&Parms)));
		ASSERT_THAT(AreEqual(
			29,
			Parms.ReturnValue,
			TEXT("The generated Parms entry should call the same current native Bind")));
	}

	TEST_METHOD(TypedASTPrintExportedRuntimeCallableCompilesLinksAndExecutes)
	{
		FAngelscriptEngine* const EnginePtr =
			GetSharedAotFixtureEngine(*TestRunner);
		ASSERT_THAT(IsNotNull(EnginePtr));
		if (EnginePtr == nullptr)
		{
			return;
		}

		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const Function = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::GetTypedPrintModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedASTPrintShowcaseDeclaration());
		uint32 FunctionId = 0;
		const bool bHasJitEntries = RequireJitEntries(
			*TestRunner,
			Engine,
			Function,
			TEXT("TypedAST exported Runtime Print call"),
			FunctionId);
		if (!bHasJitEntries && Function != nullptr)
		{
			FAngelscriptStableFunctionKey FunctionKey;
			if (Engine.TryResolveStableFunctionKey(Function, FunctionKey))
			{
				const FStaticJITDiagnostics::FSnapshot Diagnostics =
					FStaticJITDiagnostics::CaptureSnapshot(&Engine);
				const FStaticJITDiagnostics::FRouteRecord* const Route =
					Diagnostics.Routes.FindByPredicate(
						[&FunctionKey](const auto& Candidate)
						{
							return Candidate.Identity.FunctionKey == FunctionKey;
						});
				if (Route != nullptr)
				{
					TestRunner->AddInfo(FString::Printf(
						TEXT("TypedAST Print route diagnostic: key=%s execution=%s debug=%s route=%u match=%u candidates=%d"),
						*FunctionKey.Hash.ToHexString(),
						*Route->Identity.Content.Execution.ToHexString(),
						*Route->Identity.Content.Debug.ToHexString(),
						static_cast<uint32>(Route->SelectedExecutionRoute),
						static_cast<uint32>(Route->MatchResult),
						Route->Candidates.Num()));
					for (const auto& Candidate : Route->Candidates)
					{
						TestRunner->AddInfo(FString::Printf(
							TEXT("TypedAST Print provider candidate: entry=%d match=%u"),
							Candidate.ProviderEntryIndex,
							static_cast<uint32>(Candidate.MatchResult)));
					}
				}
			}
		}
		ASSERT_THAT(IsTrue(bHasJitEntries));
		if (!bHasJitEntries || Function == nullptr)
		{
			return;
		}

		const asSJITFunctionBinding Binding = Function->GetJITBinding();
		using FRawEntry = int32(*)(
			FScriptExecution&, FString*, double, FLinearColor*);
		const FRawEntry RawEntry = reinterpret_cast<FRawEntry>(
			Binding.RawEntry);
		ASSERT_THAT(IsNotNull(RawEntry));

		FString Text(TEXT("Typed AOT exported Print execution proof"));
		const double Duration = 0.0;
		FLinearColor Color(0.125f, 0.25f, 0.5f, 1.0f);
		AngelscriptStaticJITNativeCallables::ResetPrintCallCountForTesting();

		FScriptExecution RawExecution(asCThreadManager::GetLocalData());
		ASSERT_THAT(AreEqual(
			314,
			RawEntry(RawExecution, &Text, Duration, &Color),
			TEXT("The generated raw entry should execute the typed Print body")));
		ASSERT_THAT(AreEqual(
			1,
			AngelscriptStaticJITNativeCallables::GetPrintCallCountForTesting(),
			TEXT("The generated raw entry should call the exported Runtime Print symbol once")));

		alignas(8) asDWORD Frame[6] = {};
		*reinterpret_cast<FString**>(&Frame[0]) = &Text;
		*reinterpret_cast<double*>(&Frame[2]) = Duration;
		*reinterpret_cast<FLinearColor**>(&Frame[4]) = &Color;
		asQWORD VMResult = 0;
		FScriptExecution VMExecution(asCThreadManager::GetLocalData());
		Binding.VMEntry(VMExecution, Frame, &VMResult);
		ASSERT_THAT(AreEqual(
			314,
			static_cast<int32>(static_cast<asDWORD>(VMResult)),
			TEXT("The generated VM entry should preserve borrowed managed arguments")));
		ASSERT_THAT(AreEqual(
			2,
			AngelscriptStaticJITNativeCallables::GetPrintCallCountForTesting(),
			TEXT("The generated VM entry should call the same exported Runtime Print symbol")));

		struct FPrintParms
		{
			FString Text;
			double Duration = 0.0;
			FLinearColor Color = FLinearColor::White;
			void* GeneratedWorldContext = nullptr;
			int32 ReturnValue = 0;
		};
		FPrintParms Parms{Text, Duration, Color, nullptr, 0};
		FScriptExecution ParmsExecution(asCThreadManager::GetLocalData());
		Binding.ParmsEntry(ParmsExecution, nullptr, &Parms);
		ASSERT_THAT(AreEqual(
			314,
			Parms.ReturnValue,
			TEXT("The generated Parms entry should preserve reflected managed layout")));
		ASSERT_THAT(AreEqual(
			3,
			AngelscriptStaticJITNativeCallables::GetPrintCallCountForTesting(),
			TEXT("All generated entry kinds should call the exported Runtime Print symbol")));
	}

	TEST_METHOD(TypedASTProviderPrivateCallUsesCurrentEngineReferenceSlot)
	{
		FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(*TestRunner);
		ASSERT_THAT(IsNotNull(EnginePtr));
		if (EnginePtr == nullptr)
		{
			return;
		}
		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const Function = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::
				GetTypedPrivateBridgeModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedASTPrivateBridgeShowcaseDeclaration());
		uint32 FunctionId = 0;
		ASSERT_THAT(IsTrue(RequireJitEntries(
			*TestRunner,
			Engine,
			Function,
			TEXT("TypedAST provider-private bridge"),
			FunctionId)));
		if (Function == nullptr || Function->GetJITBinding().VMEntry == nullptr)
		{
			return;
		}

		const FAngelscriptJITBindingContext* const BindingContext =
			FAngelscriptJITBindingContext::FromBindingUserData(
				Function->GetJITBinding().UserData);
		ASSERT_THAT(IsNotNull(BindingContext));
		ASSERT_THAT(IsNotNull(
			BindingContext != nullptr
				? BindingContext->GetReferenceTable() : nullptr));

		asIScriptContext* const Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 19)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(1, 23)));
		AngelscriptStaticJITAotFixture::ResetTypedASTPrivateBridgeObservation();
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::
				GetExpectedTypedASTPrivateBridgeResult(),
			static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(
			1,
			AngelscriptStaticJITAotFixture::
				GetTypedASTPrivateBridgeCallCount()));
	}

	TEST_METHOD(TypedASTOrdinaryBindingUsesCurrentEngineReferenceSlot)
	{
		FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(*TestRunner);
		ASSERT_THAT(IsNotNull(EnginePtr));
		if (EnginePtr == nullptr)
		{
			return;
		}
		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const Function = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::
				GetTypedPrivateBridgeModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedASTOrdinaryBridgeSqrtDeclaration());
		uint32 FunctionId = 0;
		ASSERT_THAT(IsTrue(RequireJitEntries(
			*TestRunner,
			Engine,
			Function,
			TEXT("TypedAST ordinary no-export scalar bridge"),
			FunctionId)));
		if (Function == nullptr || Function->GetJITBinding().VMEntry == nullptr)
		{
			return;
		}

		const FAngelscriptJITBindingContext* const BindingContext =
			FAngelscriptJITBindingContext::FromBindingUserData(
				Function->GetJITBinding().UserData);
		ASSERT_THAT(IsNotNull(BindingContext));
		ASSERT_THAT(IsNotNull(
			BindingContext != nullptr
				? BindingContext->GetReferenceTable() : nullptr));

		asIScriptContext* const Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgFloat(0, 144.0f)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsNear(
			12.0f,
			Context->GetReturnFloat(),
			0.0001f,
			TEXT("The ordinary Math::Sqrt binding should execute through the generated current-Engine bridge")));
	}

	TEST_METHOD(TypedASTNestedCheckedExceptionPublishesOnceAndSuppressesLaterNativeCall)
	{
		using namespace AngelscriptNativeTestSupport;

		FAngelscriptEngine* const EnginePtr =
			GetSharedAotFixtureEngine(*TestRunner);
		ASSERT_THAT(IsNotNull(EnginePtr));
		if (EnginePtr == nullptr)
		{
			return;
		}

		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const Helper = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::GetTypedRecursionModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedASTExceptionHelperDeclaration());
		asCScriptFunction* const Entry = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::GetTypedRecursionModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedASTExceptionEntryDeclaration());
		uint32 EntryId = 0;
		const bool bHasTypedEntries = RequireJitEntries(
			*TestRunner,
			Engine,
			Entry,
			TEXT("TypedAST checked-exception public entry"),
			EntryId);
		ASSERT_THAT(IsTrue(bHasTypedEntries));
		if (!bHasTypedEntries || Helper == nullptr || Entry == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		FNativeDebugRecorder Recorder;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->SetExceptionCallback(
				asFUNCTION(CaptureNativeException),
				&Recorder,
				asCALL_CDECL)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Entry)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 12)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(1, 0)));
		AngelscriptStaticJITAotFixture::ResetTypedASTPrivateBridgeObservation();
		int32 ExecuteResult = asERROR;
		{
			FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
			ExecuteResult = Context->Execute();
		}

		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, ExecuteResult));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Divide by zero")),
			FString(UTF8_TO_TCHAR(Context->GetExceptionString()))));
		int32 Column = INDEX_NONE;
		const char* Section = nullptr;
		const int32 Line = Context->GetExceptionLineNumber(&Column, &Section);
		asIScriptFunction* const ExceptionFunction =
			Context->GetExceptionFunction();
		const int32 CallbackCount =
			Recorder.Num(ENativeDebugEventKind::Exception);
		const int32 LateNativeCallCount =
			AngelscriptStaticJITAotFixture::GetTypedASTPrivateBridgeCallCount();
		const FString ActualSection = UTF8_TO_TCHAR(
			Section != nullptr ? Section : "");
		const bool bNestedExceptionContractMatches =
			ExceptionFunction == Helper
			&& Line == AngelscriptStaticJITAotFixture::
				GetExpectedTypedASTExceptionLine()
			&& ActualSection == TEXT(
				"/Angelscript/Game/ASStaticJITTypedRecursionFixture.as")
			&& CallbackCount == 1
			&& LateNativeCallCount == 0;
		if (!bNestedExceptionContractMatches)
		{
			TestRunner->AddError(FString::Printf(TEXT(
				"Nested TypedAST exception mismatch: Function=%s ExpectedFunction=%s Line=%d ExpectedLine=%d Column=%d Section=%s CallbackCount=%d LateNativeCallCount=%d"),
				ExceptionFunction != nullptr
					? UTF8_TO_TCHAR(ExceptionFunction->GetDeclaration())
					: TEXT("<null>"),
				UTF8_TO_TCHAR(Helper->GetDeclaration()),
				Line,
				AngelscriptStaticJITAotFixture::
					GetExpectedTypedASTExceptionLine(),
				Column,
				*ActualSection,
				CallbackCount,
				LateNativeCallCount));
		}
	}

	TEST_METHOD(ExceptionRouteMatrixCharacterizesExecutionPaths)
	{
		using namespace AngelscriptStaticJITAotGeneration;

		FAngelscriptEngine* const RuntimeEnginePtr =
			GetSharedAotFixtureEngine(*TestRunner);
		ASSERT_THAT(IsNotNull(RuntimeEnginePtr));
		if (RuntimeEnginePtr == nullptr)
		{
			return;
		}

		TUniquePtr<FStaticJITAotFixtureEngineSession> InterpreterSession =
			CreateInterpreterFixtureEngineSession();
		ASSERT_THAT(IsTrue(InterpreterSession.IsValid()));
		ASSERT_THAT(IsTrue(
			InterpreterSession.IsValid() && InterpreterSession->IsValid(),
			InterpreterSession.IsValid()
				? InterpreterSession->Error
				: TEXT("Could not allocate the interpreter-only Engine session.")));
		if (!InterpreterSession.IsValid() || !InterpreterSession->IsValid())
		{
			return;
		}

		FAngelscriptEngine& RuntimeEngine = *RuntimeEnginePtr;
		FAngelscriptEngine& InterpreterEngine = *InterpreterSession->Engine;
		asCScriptFunction* const VMThrow = FindGlobalFunction(
			*TestRunner,
			InterpreterEngine,
			AngelscriptStaticJITAotFixture::GetExceptionThrowDeclaration());
		asCScriptFunction* const BytecodeThrow = FindGlobalFunction(
			*TestRunner,
			RuntimeEngine,
			AngelscriptStaticJITAotFixture::GetExceptionThrowDeclaration());
		asCScriptFunction* const BytecodeNested = FindGlobalFunction(
			*TestRunner,
			RuntimeEngine,
			AngelscriptStaticJITAotFixture::GetExceptionEntryDeclaration());
		asCScriptFunction* const TypedHelper = FindGlobalFunctionInModule(
			*TestRunner,
			RuntimeEngine,
			AngelscriptStaticJITAotFixture::GetTypedRecursionModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedASTExceptionHelperDeclaration());
		asCScriptFunction* const TypedEntry = FindGlobalFunctionInModule(
			*TestRunner,
			RuntimeEngine,
			AngelscriptStaticJITAotFixture::GetTypedRecursionModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedASTExceptionEntryDeclaration());
		asCScriptFunction* const TypedBridgeException =
			FindGlobalFunctionInModule(
				*TestRunner,
				RuntimeEngine,
				AngelscriptStaticJITAotFixture::
					GetTypedPrivateBridgeModuleName(),
				AngelscriptStaticJITAotFixture::
					GetTypedASTPrivateBridgeExceptionDeclaration());
		asCScriptFunction* const PublicUASFunctionEntry =
			FindGlobalFunctionInModule(
				*TestRunner,
				RuntimeEngine,
				AngelscriptStaticJITAotFixture::
					GetTypedPrivateBridgeModuleName(),
				AngelscriptStaticJITAotFixture::
					GetTypedASTPublicUASFunctionExceptionEntryDeclaration());
		UClass* const TypedBridgeStaticsClass = FindGeneratedClass(
			&RuntimeEngine,
			AngelscriptStaticJITAotFixture::
				GetTypedPrivateBridgeStaticsClassName());
		UASFunction* const PublicUASFunction =
			TypedBridgeStaticsClass != nullptr
				? Cast<UASFunction>(FindGeneratedFunction(
					TypedBridgeStaticsClass,
					TEXT("TypedASTPrivateBridgeException")))
				: nullptr;

		uint32 BytecodeThrowId = 0;
		uint32 BytecodeNestedId = 0;
		uint32 TypedEntryId = 0;
		uint32 TypedBridgeExceptionId = 0;
		uint32 PublicUASFunctionEntryId = 0;
		const bool bRoutesReady =
			RequireJitEntries(
				*TestRunner,
				RuntimeEngine,
				BytecodeThrow,
				TEXT("top-level BytecodeJIT exception route"),
				BytecodeThrowId)
			&& RequireJitEntries(
				*TestRunner,
				RuntimeEngine,
				BytecodeNested,
				TEXT("nested BytecodeJIT exception route"),
				BytecodeNestedId)
			&& RequireJitEntries(
				*TestRunner,
				RuntimeEngine,
				TypedEntry,
				TEXT("TypedASTJIT helper exception route"),
				TypedEntryId)
			&& RequireJitEntries(
				*TestRunner,
				RuntimeEngine,
				TypedBridgeException,
				TEXT("TypedASTJIT provider-private VM bridge exception route"),
				TypedBridgeExceptionId)
			&& RequireJitEntries(
				*TestRunner,
				RuntimeEngine,
				PublicUASFunctionEntry,
				TEXT("public UASFunction Parms exception entry route"),
				PublicUASFunctionEntryId);
		ASSERT_THAT(IsTrue(bRoutesReady));
		ASSERT_THAT(IsNotNull(VMThrow));
		ASSERT_THAT(IsNotNull(TypedHelper));
		ASSERT_THAT(IsNotNull(TypedBridgeStaticsClass));
		ASSERT_THAT(IsNotNull(PublicUASFunction));
		ASSERT_THAT(IsTrue(
			PublicUASFunction != nullptr
				&& PublicUASFunction->ScriptFunction == TypedBridgeException,
			TEXT("The public reflected UASFunction must wrap the exact current TypedAST root.")));
		if (!bRoutesReady || VMThrow == nullptr || TypedHelper == nullptr
			|| TypedBridgeException == nullptr || PublicUASFunction == nullptr
			|| PublicUASFunction->ScriptFunction != TypedBridgeException)
		{
			return;
		}

		const TArray<asDWORD> NoArguments;
		const TArray<asDWORD> DivideArguments{12u, 0u};
		const TArray<asDWORD> BridgeArguments{17u};
		TArray<FExceptionRouteObservation> Observations;
		Observations.SetNum(6);
		{
			FAngelscriptEngineScope InterpreterScope(InterpreterEngine);
			ASSERT_THAT(IsTrue(ObserveExceptionContext(
				*TestRunner,
				InterpreterEngine,
				VMThrow,
				NoArguments,
				TEXT("VM top-level"),
				Observations[0])));
		}
		{
			FAngelscriptEngineScope RuntimeScope(RuntimeEngine);
			ASSERT_THAT(IsTrue(ObserveExceptionContext(
				*TestRunner,
				RuntimeEngine,
				BytecodeThrow,
				NoArguments,
				TEXT("BytecodeJIT top-level"),
				Observations[1])));
			ASSERT_THAT(IsTrue(ObserveExceptionContext(
				*TestRunner,
				RuntimeEngine,
				BytecodeNested,
				NoArguments,
				TEXT("BytecodeJIT nested direct"),
				Observations[2])));
			AngelscriptStaticJITAotFixture::
				ResetTypedASTPrivateBridgeObservation();
			ASSERT_THAT(IsTrue(ObserveExceptionContext(
				*TestRunner,
				RuntimeEngine,
				TypedEntry,
				DivideArguments,
				TEXT("TypedASTJIT helper"),
				Observations[3])));
			Observations[3].LaterSideEffectCount =
				AngelscriptStaticJITAotFixture::
					GetTypedASTPrivateBridgeCallCount();
			AngelscriptStaticJITAotFixture::
				ResetTypedASTPrivateBridgeObservation();
			ASSERT_THAT(IsTrue(ObserveExceptionContext(
				*TestRunner,
				RuntimeEngine,
				TypedBridgeException,
				BridgeArguments,
				TEXT("TypedASTJIT provider-private VM bridge"),
				Observations[4])));
			Observations[4].TriggerCallCount =
				AngelscriptStaticJITAotFixture::
					GetTypedASTPrivateBridgeExceptionCallCount();
			Observations[4].LaterSideEffectCount =
				AngelscriptStaticJITAotFixture::
					GetTypedASTPrivateBridgeCallCount();

			const asSJITFunctionBinding PublicBinding =
				TypedBridgeException->GetJITBinding();
			const FAngelscriptJITBindingContext* const PublicBindingContext =
				FAngelscriptJITBindingContext::FromBindingUserData(
					PublicBinding.UserData);
			asCScriptFunction* ResolvedPublicScriptFunction =
				TypedBridgeException;
			UObject* const TypedBridgeStaticsObject =
				TypedBridgeStaticsClass->GetDefaultObject();
			UASClass* const TypedBridgeASClass =
				UASClass::GetFirstASClass(TypedBridgeStaticsObject);
			if (TypedBridgeException->vfTableIdx >= 0
				&& TypedBridgeASClass != nullptr)
			{
				asCObjectType* const ObjectType =
					static_cast<asCObjectType*>(
						TypedBridgeASClass->ScriptTypePtr);
				if (ObjectType != nullptr
					&& TypedBridgeException->vfTableIdx
						< static_cast<int32>(
							ObjectType->virtualFunctionTable.GetLength()))
				{
					ResolvedPublicScriptFunction =
						ObjectType->virtualFunctionTable[
							TypedBridgeException->vfTableIdx];
				}
			}
			const asSJITFunctionBinding ResolvedPublicBinding =
				ResolvedPublicScriptFunction != nullptr
					? ResolvedPublicScriptFunction->GetJITBinding()
					: asSJITFunctionBinding();
			const FAngelscriptJITBindingContext* const
				ResolvedPublicBindingContext =
					FAngelscriptJITBindingContext::FromBindingUserData(
						ResolvedPublicBinding.UserData);
			ASSERT_THAT(IsNotNull(PublicBindingContext));
			const uint64 ParmsExecutionCountBefore = PublicBindingContext != nullptr
				? PublicBindingContext->GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::Parms)
				: 0;
			AngelscriptStaticJITAotFixture::
				ResetTypedASTPrivateBridgeObservation();
			ASSERT_THAT(IsTrue(ObserveExceptionContext(
				*TestRunner,
				RuntimeEngine,
				PublicUASFunctionEntry,
				BridgeArguments,
				TEXT("public UASFunction Parms entry"),
				Observations[5])));
			Observations[5].TriggerCallCount =
				AngelscriptStaticJITAotFixture::
					GetTypedASTPublicUASFunctionInvokeCallCount();
			Observations[5].LaterSideEffectCount =
				AngelscriptStaticJITAotFixture::
					GetTypedASTPrivateBridgeCallCount();
			if (PublicBindingContext != nullptr)
			{
				const uint64 ParmsExecutionCountAfter =
					PublicBindingContext->GetExecutionCount(
						FAngelscriptJITBindingContext::EEntryKind::Parms);
				ASSERT_THAT(AreEqual(
					ParmsExecutionCountBefore + 1,
					ParmsExecutionCountAfter,
					FString::Printf(
						TEXT("The public UASFunction route must invoke the current TypedAST Parms entry exactly once. WrapperClass=%s NotBlueprintThreadSafe=%s Static=%s VftableIndex=%d ResolvedSame=%s HasParmsEntry=%s ResolvedHasParmsEntry=%s BindingContext=%p ResolvedBindingContext=%p EntryIndex=%d ResolvedEntryIndex=%d Before=%llu After=%llu TriggerCalls=%d Observation={%s}"),
						*GetNameSafe(PublicUASFunction->GetClass()),
						PublicUASFunction->HasMetaData(
							TEXT("NotBlueprintThreadSafe")) ? TEXT("true") : TEXT("false"),
						PublicUASFunction->HasAnyFunctionFlags(FUNC_Static)
							? TEXT("true") : TEXT("false"),
						TypedBridgeException->vfTableIdx,
						ResolvedPublicScriptFunction == TypedBridgeException
							? TEXT("true") : TEXT("false"),
						PublicBinding.ParmsEntry != nullptr ? TEXT("true") : TEXT("false"),
						ResolvedPublicBinding.ParmsEntry != nullptr
							? TEXT("true") : TEXT("false"),
						PublicBindingContext,
						ResolvedPublicBindingContext,
						PublicBindingContext != nullptr
							? PublicBindingContext->GetEntryIndex() : INDEX_NONE,
						ResolvedPublicBindingContext != nullptr
							? ResolvedPublicBindingContext->GetEntryIndex() : INDEX_NONE,
						ParmsExecutionCountBefore,
						ParmsExecutionCountAfter,
						AngelscriptStaticJITAotFixture::
							GetTypedASTPublicUASFunctionInvokeCallCount(),
						*Observations[5].Describe())));
			}
		}

		struct FExpected
		{
			FString Message;
			FString FunctionDeclaration;
			FString Section;
			int32 Line = INDEX_NONE;
			int32 Column = INDEX_NONE;
			bool bExpectJITRecord = false;
			bool bExpectAdopted = false;
			FString RouteOrigin;
			FString BackendOrigin;
			int32 TriggerCallCount = INDEX_NONE;
		};
		const FString BaseSection(
			TEXT("/Angelscript/Game/ASStaticJITAotFixture.as"));
		const FString TypedSection(
			TEXT("/Angelscript/Game/ASStaticJITTypedRecursionFixture.as"));
		const FString TestJITBackend(TEXT("AngelscriptTestJIT Fixtures"));
		const TArray<FExpected> Expected{
			{
				AngelscriptStaticJITAotFixture::GetExpectedExceptionText(),
				TEXT("void FailForAOT()"),
				BaseSection,
				AngelscriptStaticJITAotFixture::GetExpectedNestedThrowLine(),
				2,
				false,
				false,
				TEXT("vm"),
				TEXT("vm"),
			},
			{
				AngelscriptStaticJITAotFixture::GetExpectedExceptionText(),
				TEXT("void FailForAOT()"),
				BaseSection,
				AngelscriptStaticJITAotFixture::GetExpectedNestedThrowLine(),
				2,
				true,
				false,
				TEXT("generated-native-call"),
				TestJITBackend,
			},
			{
				AngelscriptStaticJITAotFixture::GetExpectedExceptionText(),
				TEXT("void FailForAOT()"),
				BaseSection,
				AngelscriptStaticJITAotFixture::GetExpectedNestedThrowLine(),
				2,
				true,
				false,
				TEXT("generated-native-call"),
				TestJITBackend,
			},
			{
				TEXT("Divide by zero"),
				TEXT("int TypedASTExceptionHelper(const int, const int)"),
				TypedSection,
				AngelscriptStaticJITAotFixture::
					GetExpectedTypedASTExceptionLine(),
				2,
				true,
				false,
				TEXT("generated-native-call"),
				TestJITBackend,
			},
			{
				AngelscriptStaticJITAotFixture::
					GetExpectedTypedASTPrivateBridgeExceptionText(),
				TEXT("int TypedASTProviderPrivateFail(int)"),
				FString(),
				0,
				0,
				true,
				true,
				TEXT("context-adoption"),
				TEXT("vm"),
				1,
			},
			{
				AngelscriptStaticJITAotFixture::
					GetExpectedTypedASTPrivateBridgeExceptionText(),
				TEXT("int TypedASTProviderPrivateFail(int)"),
				FString(),
				0,
				0,
				true,
				true,
				TEXT("context-adoption"),
				TEXT("vm"),
				1,
			},
		};

		TArray<FString> Mismatches;
		for (int32 CaseIndex = 0; CaseIndex < Observations.Num(); ++CaseIndex)
		{
			const FExceptionRouteObservation& Actual = Observations[CaseIndex];
			TestRunner->AddInfo(Actual.Describe());
			const FExpected& Wanted = Expected[CaseIndex];
			const bool bMatches =
				Actual.ExecuteStatus == asEXECUTION_EXCEPTION
				&& Actual.Message == Wanted.Message
				&& Actual.FunctionDeclaration == Wanted.FunctionDeclaration
				&& Actual.Section == Wanted.Section
				&& Actual.Line == Wanted.Line
				&& Actual.Column == Wanted.Column
				&& Actual.CallbackCount == 1
				&& Actual.bObservedJITRecord == Wanted.bExpectJITRecord
				&& (!Wanted.bExpectJITRecord || Actual.bRecordReported)
				&& Actual.bRecordAdopted == Wanted.bExpectAdopted
				&& Actual.RouteOrigin == Wanted.RouteOrigin
				&& Actual.BackendOrigin == Wanted.BackendOrigin
				&& (Wanted.TriggerCallCount == INDEX_NONE
					|| Actual.TriggerCallCount == Wanted.TriggerCallCount)
				&& Actual.LaterSideEffectCount == 0;
			if (!bMatches)
			{
				Mismatches.Add(Actual.Describe());
			}
		}

		if (!Mismatches.IsEmpty())
		{
			TestRunner->AddError(FString::Printf(TEXT(
				"Exception route matrix mismatch (%d/%d):\n%s"),
				Mismatches.Num(),
				Observations.Num(),
				*FString::Join(Mismatches, TEXT("\n"))));
		}
	}

	TEST_METHOD(TypedASTProviderPrivateCallRefreshesForReplacementEngineAndUnbindsToVM)
	{
		using namespace AngelscriptStaticJITAotGeneration;
		TUniquePtr<FStaticJITAotFixtureEngineSession> Session =
			CreateFixtureEngineSession(
				false,
				EStaticJITAotFixtureComposition::Complete,
				AngelscriptStaticJITAotFixture::
					ETypedASTPrivateBridgeRegistration::Replacement);
		ASSERT_THAT(IsTrue(Session.IsValid()));
		ASSERT_THAT(IsTrue(
			Session.IsValid() && Session->IsValid(),
			Session.IsValid()
				? Session->Error
				: TEXT("Could not allocate the replacement Engine session.")));
		if (!Session.IsValid() || !Session->IsValid())
		{
			return;
		}

		FAngelscriptEngine& Engine = *Session->Engine;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const Function = FindGlobalFunctionInModule(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::GetTypedPrivateBridgeModuleName(),
			AngelscriptStaticJITAotFixture::
				GetTypedASTPrivateBridgeShowcaseDeclaration());
		uint32 FunctionId = 0;
		ASSERT_THAT(IsTrue(RequireJitEntries(
			*TestRunner,
			Engine,
			Function,
			TEXT("TypedAST replacement Engine private bridge"),
			FunctionId)));
		if (Function == nullptr || Function->GetJITBinding().VMEntry == nullptr)
		{
			return;
		}

		const asSJITFunctionBinding ProviderBinding = Function->GetJITBinding();
		const FAngelscriptJITBindingContext* const ProviderContext =
			FAngelscriptJITBindingContext::FromBindingUserData(
				ProviderBinding.UserData);
		ASSERT_THAT(IsNotNull(ProviderContext));
		ASSERT_THAT(IsNotNull(
			ProviderContext != nullptr
				? ProviderContext->GetReferenceTable() : nullptr));

		asIScriptContext* const Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		AngelscriptStaticJITAotFixture::
			ResetTypedASTPrivateBridgeObservation();
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 19)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(1, 23)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::
				GetExpectedTypedASTPrivateBridgeReplacementResult(),
			static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(
			0,
			AngelscriptStaticJITAotFixture::
				GetTypedASTPrivateBridgeCallCount()));
		ASSERT_THAT(AreEqual(
			1,
			AngelscriptStaticJITAotFixture::
				GetTypedASTPrivateBridgeReplacementCallCount()));

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Function->SetJITBinding(asSJITFunctionBinding())));
		ASSERT_THAT(IsNull(
			FAngelscriptJITBindingContext::FromBindingUserData(
				ProviderBinding.UserData)));
		ASSERT_THAT(IsNull(Function->GetJITBinding().VMEntry));

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare()));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 19)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(1, 23)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::
				GetExpectedTypedASTPrivateBridgeReplacementResult(),
			static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(
			2,
			AngelscriptStaticJITAotFixture::
				GetTypedASTPrivateBridgeReplacementCallCount()));
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
		ASSERT_THAT(IsTrue(
			SourceEngine.GetRuntimeConfig().bUseCanonicalStagedCompiler,
			TEXT("A committed TypedASTJIT Provider must be adopted by source compiled through the same CANONICAL compiler pipeline that produced its execution identity; this fixture-local requirement does not change the product-default LEGACY route")));
		ASSERT_THAT(IsFalse(
			SourceEngine.IsCacheV2Enabled(),
			TEXT("Current-source Provider adoption must remain independent of the product-default-disabled Cache V2 lifecycle")));
		FAngelscriptCacheService* const CacheService =
			SourceEngine.GetCacheService();
		ASSERT_THAT(IsNotNull(CacheService));
		if (CacheService == nullptr)
		{
			return;
		}
		const FAngelscriptCacheLifecyclePublications CachePublications =
			CacheService->GetLifecyclePublications();
		ASSERT_THAT(IsFalse(CachePublications.Current.IsValid()));
		ASSERT_THAT(IsFalse(CachePublications.PendingColdStart.IsValid()));
		ASSERT_THAT(IsFalse(CachePublications.LatestSuccessful.IsValid()));

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

		const FAngelscriptJITProviderRouteRefreshResult DiagnosticRefresh =
			FAngelscriptJITProviderRouter::Refresh(SourceEngine);
		TestRunner->AddInfo(DiagnosticRefresh.Detail);
		ASSERT_THAT(IsTrue(
			DiagnosticRefresh.IsApplied(), DiagnosticRefresh.Detail));

		const asSJITFunctionBinding Binding = Function->GetJITBinding();
		ASSERT_THAT(IsNotNull(Binding.VMEntry, DiagnosticRefresh.Detail));
		ASSERT_THAT(IsNotNull(Binding.RawEntry, DiagnosticRefresh.Detail));
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
			AngelscriptStaticJITAotGeneration::CreateFixtureEngineSession(
				true,
				AngelscriptStaticJITAotGeneration::
					EStaticJITAotFixtureComposition::Core);
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
		ASSERT_THAT(AreEqual(
			Restore.ExpectedCandidateModuleCount,
			Restore.CandidateModuleCount,
			Restore.Detail));
		ASSERT_THAT(AreEqual(
			Restore.ExpectedRestoredFunctionCount,
			Restore.RestoredFunctionCount,
			Restore.Detail));
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

bool FAngelscriptStaticJITAotTests::RunTypedASTScalarParity(
	FAutomationTestBase& Test)
{
	TUniquePtr<AngelscriptStaticJITAotGeneration::
		FStaticJITAotFixtureEngineSession> InterpreterSession =
			AngelscriptStaticJITAotGeneration::
				CreateInterpreterFixtureEngineSession();
	if (!Test.TestNotNull(
			TEXT("TypedASTJIT scalar parity should create an isolated interpreter Engine"),
			InterpreterSession.Get())
		|| !Test.TestTrue(
			TEXT("TypedASTJIT scalar parity interpreter Engine should compile the fixture"),
			InterpreterSession.IsValid() && InterpreterSession->IsValid()))
	{
		if (InterpreterSession.IsValid())
		{
			Test.AddError(InterpreterSession->Error);
		}
		return false;
	}

	FAngelscriptEngine* const BytecodeEngine = GetSharedAotFixtureEngine(Test);
	if (BytecodeEngine == nullptr)
	{
		return false;
	}

	auto Execute = [&Test](
		FAngelscriptEngine& Engine,
		asCScriptFunction& Function,
		const int32 A,
		const int32 B,
		const TCHAR* Label,
		int32& OutResult)
	{
		FAngelscriptEngineScope Scope(Engine);
		asIScriptContext* const Context = Engine.GetScriptEngine()->CreateContext();
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should create a script context"), Label),
				Context))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		if (!Test.TestEqual(
				*FString::Printf(TEXT("%s should prepare the scalar function"), Label),
				Context->Prepare(&Function), asSUCCESS)
			|| !Test.TestEqual(
				*FString::Printf(TEXT("%s should bind A"), Label),
				Context->SetArgDWord(0, static_cast<asDWORD>(A)), asSUCCESS)
			|| !Test.TestEqual(
				*FString::Printf(TEXT("%s should bind B"), Label),
				Context->SetArgDWord(1, static_cast<asDWORD>(B)), asSUCCESS)
			|| !Test.TestEqual(
				*FString::Printf(TEXT("%s should finish"), Label),
				Context->Execute(), asEXECUTION_FINISHED))
		{
			return false;
		}

		OutResult = static_cast<int32>(Context->GetReturnDWord());
		return true;
	};

	asCScriptFunction* const InterpreterFunction = FindGlobalFunction(
		Test,
		*InterpreterSession->Engine,
		AngelscriptStaticJITAotFixture::GetSemanticScalarBranchDeclaration());
	asCScriptFunction* const BytecodeFunction = FindGlobalFunction(
		Test,
		*BytecodeEngine,
		AngelscriptStaticJITAotFixture::GetSemanticScalarBranchDeclaration());
	if (InterpreterFunction == nullptr || BytecodeFunction == nullptr)
	{
		return false;
	}
	if (!Test.TestNull(
			TEXT("The isolated interpreter function should have no JIT VM entry"),
			InterpreterFunction->GetJITBinding().VMEntry))
	{
		return false;
	}
	uint32 BytecodeFunctionId = 0;
	if (!RequireJitEntries(
			Test, *BytecodeEngine, BytecodeFunction,
			TEXT("scalar BytecodeJIT function"), BytecodeFunctionId))
	{
		return false;
	}

	struct FCase
	{
		int32 A;
		int32 B;
		int32 Expected;
	};
	const FCase Cases[] = {
		{3, 4, 6},
		{6, 7, 26},
		{-5, 2, -4},
		{10, 0, 9},
	};

	AngelscriptTestJITProbes::ResetTypedASTScalarBranchProbeCount();
	for (const FCase& Case : Cases)
	{
		int32 InterpreterResult = 0;
		int32 BytecodeResult = 0;
		if (!Execute(
				*InterpreterSession->Engine, *InterpreterFunction,
				Case.A, Case.B, TEXT("interpreter"), InterpreterResult)
			|| !Execute(
				*BytecodeEngine, *BytecodeFunction,
				Case.A, Case.B, TEXT("BytecodeJIT"), BytecodeResult))
		{
			return false;
		}
		const int32 TypedASTResult =
			AngelscriptTestJITProbes::ExecuteTypedASTScalarBranch(
				Case.A, Case.B);
		if (!Test.TestEqual(TEXT("Interpreter scalar result"),
				InterpreterResult, Case.Expected)
			|| !Test.TestEqual(TEXT("BytecodeJIT scalar result"),
				BytecodeResult, Case.Expected)
			|| !Test.TestEqual(TEXT("TypedASTJIT scalar result"),
				TypedASTResult, Case.Expected))
		{
			return false;
		}
	}

	if (!Test.TestEqual(
			TEXT("The compiled TypedASTJIT branch probe should execute exactly once per row"),
			AngelscriptTestJITProbes::GetTypedASTScalarBranchProbeCount(),
			static_cast<int32>(UE_ARRAY_COUNT(Cases))))
	{
		return false;
	}

	asCScriptFunction* const InterpreterMutation = FindGlobalFunction(
		Test, *InterpreterSession->Engine,
		AngelscriptStaticJITAotFixture::
			GetSemanticMutationParityDeclaration());
	asCScriptFunction* const BytecodeMutation = FindGlobalFunction(
		Test, *BytecodeEngine,
		AngelscriptStaticJITAotFixture::
			GetSemanticMutationParityDeclaration());
	asCScriptFunction* const InterpreterPowerFloat = FindGlobalFunction(
		Test, *InterpreterSession->Engine,
		AngelscriptStaticJITAotFixture::GetSemanticPowerFloatDeclaration());
	asCScriptFunction* const BytecodePowerFloat = FindGlobalFunction(
		Test, *BytecodeEngine,
		AngelscriptStaticJITAotFixture::GetSemanticPowerFloatDeclaration());
	asCScriptFunction* const InterpreterPowerDouble = FindGlobalFunction(
		Test, *InterpreterSession->Engine,
		AngelscriptStaticJITAotFixture::GetSemanticPowerDoubleDeclaration());
	asCScriptFunction* const BytecodePowerDouble = FindGlobalFunction(
		Test, *BytecodeEngine,
		AngelscriptStaticJITAotFixture::GetSemanticPowerDoubleDeclaration());
	asCScriptFunction* const InterpreterPowerDoubleInt = FindGlobalFunction(
		Test, *InterpreterSession->Engine,
		AngelscriptStaticJITAotFixture::
			GetSemanticPowerDoubleIntDeclaration());
	asCScriptFunction* const BytecodePowerDoubleInt = FindGlobalFunction(
		Test, *BytecodeEngine,
		AngelscriptStaticJITAotFixture::
			GetSemanticPowerDoubleIntDeclaration());
	if (InterpreterMutation == nullptr || BytecodeMutation == nullptr
		|| InterpreterPowerFloat == nullptr || BytecodePowerFloat == nullptr
		|| InterpreterPowerDouble == nullptr || BytecodePowerDouble == nullptr
		|| InterpreterPowerDoubleInt == nullptr
		|| BytecodePowerDoubleInt == nullptr)
	{
		return false;
	}

	const TPair<asCScriptFunction*, const TCHAR*> BytecodeFunctions[] = {
		{BytecodeMutation, TEXT("mutation BytecodeJIT function")},
		{BytecodePowerFloat, TEXT("POWf BytecodeJIT function")},
		{BytecodePowerDouble, TEXT("POWd BytecodeJIT function")},
		{BytecodePowerDoubleInt, TEXT("POWdi BytecodeJIT function")},
	};
	for (const TPair<asCScriptFunction*, const TCHAR*>& Pair
		: BytecodeFunctions)
	{
		uint32 TransientFunctionId = 0;
		if (!RequireJitEntries(
				Test, *BytecodeEngine, Pair.Key, Pair.Value,
				TransientFunctionId))
		{
			return false;
		}
	}

	if (!Test.TestTrue(
			TEXT("The generated TestJIT DLL should register mutation, POWf, POWd, and POWdi probes"),
			AngelscriptTestJITProbes::HasTypedASTMutationPowerProbes()))
	{
		return false;
	}

	enum class EPowerProbeShape : uint8
	{
		Float,
		Double,
		DoubleInt,
	};
	auto ExecutePower = [&Test](
		FAngelscriptEngine& Engine,
		asCScriptFunction& Function,
		const double Base,
		const double Exponent,
		const EPowerProbeShape PowerShape,
		const TCHAR* Label,
		double& OutResult)
	{
		FAngelscriptEngineScope Scope(Engine);
		asIScriptContext* const Context =
			Engine.GetScriptEngine()->CreateContext();
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should create a script context"), Label),
				Context))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int32 PrepareResult = Context->Prepare(&Function);
		int32 BaseResult = asERROR;
		int32 ExponentResult = asERROR;
		if (PowerShape == EPowerProbeShape::Float)
		{
			BaseResult = Context->SetArgFloat(0, static_cast<float>(Base));
			ExponentResult = Context->SetArgFloat(
				1, static_cast<float>(Exponent));
		}
		else
		{
			BaseResult = Context->SetArgDouble(0, Base);
			ExponentResult = PowerShape == EPowerProbeShape::Double
				? Context->SetArgDouble(1, Exponent)
				: Context->SetArgDWord(
					1, static_cast<asDWORD>(static_cast<int32>(Exponent)));
		}
		if (!Test.TestEqual(
				*FString::Printf(TEXT("%s should prepare"), Label),
				PrepareResult, asSUCCESS)
			|| !Test.TestEqual(
				*FString::Printf(TEXT("%s should bind Base"), Label),
				BaseResult, asSUCCESS)
			|| !Test.TestEqual(
				*FString::Printf(TEXT("%s should bind Exponent"), Label),
				ExponentResult, asSUCCESS)
			|| !Test.TestEqual(
				*FString::Printf(TEXT("%s should finish"), Label),
				Context->Execute(), asEXECUTION_FINISHED))
		{
			return false;
		}

		OutResult = PowerShape == EPowerProbeShape::Float
			? static_cast<double>(Context->GetReturnFloat())
			: Context->GetReturnDouble();
		return true;
	};

	AngelscriptTestJITProbes::ResetTypedASTMutationPowerProbeCount();
	const FCase MutationCases[] = {
		{99, 2, 7606},
		{-5, -4, 1000},
		{1, 10, 16414},
	};
	for (const FCase& Case : MutationCases)
	{
		int32 InterpreterResult = 0;
		int32 BytecodeResult = 0;
		if (!Execute(
				*InterpreterSession->Engine, *InterpreterMutation,
				Case.A, Case.B, TEXT("mutation interpreter"),
				InterpreterResult)
			|| !Execute(
				*BytecodeEngine, *BytecodeMutation,
				Case.A, Case.B, TEXT("mutation BytecodeJIT"),
				BytecodeResult))
		{
			return false;
		}
		const int32 TypedResult =
			AngelscriptTestJITProbes::ExecuteTypedASTMutation(
				Case.A, Case.B);
		if (!Test.TestEqual(TEXT("Interpreter mutation result"),
				InterpreterResult, Case.Expected)
			|| !Test.TestEqual(TEXT("BytecodeJIT mutation result"),
				BytecodeResult, Case.Expected)
			|| !Test.TestEqual(TEXT("TypedASTJIT mutation result"),
				TypedResult, Case.Expected))
		{
			return false;
		}
	}

	struct FPowerCase
	{
		double Base;
		double Exponent;
		double Expected;
	};
	const FPowerCase FloatCases[] = {
		{2.0, 3.0, 8.0},
		{4.0, 2.0, 16.0},
		{0.5, 2.0, 0.25},
	};
	const FPowerCase DoubleCases[] = {
		{2.0, 4.0, 16.0},
		{8.0, 2.0, 64.0},
		{0.25, 2.0, 0.0625},
	};
	const FPowerCase DoubleIntCases[] = {
		{2.0, 10.0, 1024.0},
		{2.0, -3.0, 0.125},
		{-2.0, 3.0, -8.0},
	};
	auto RunPowerCases = [&Test,
		&ExecutePower,
		&InterpreterSession,
		BytecodeEngine](
			const TConstArrayView<FPowerCase> PowerCases,
			asCScriptFunction& InterpreterFunction,
			asCScriptFunction& BytecodeFunction,
			const EPowerProbeShape PowerShape,
			const TCHAR* Label)
	{
		for (const FPowerCase& Case : PowerCases)
		{
			double InterpreterResult = 0.0;
			double BytecodeResult = 0.0;
			if (!ExecutePower(
					*InterpreterSession->Engine, InterpreterFunction,
					Case.Base, Case.Exponent, PowerShape,
					*FString::Printf(TEXT("%s interpreter"), Label),
					InterpreterResult)
				|| !ExecutePower(
					*BytecodeEngine, BytecodeFunction,
					Case.Base, Case.Exponent, PowerShape,
					*FString::Printf(TEXT("%s BytecodeJIT"), Label),
					BytecodeResult))
			{
				return false;
			}

			double TypedResult = 0.0;
			if (PowerShape == EPowerProbeShape::Float)
			{
				TypedResult = static_cast<double>(
					AngelscriptTestJITProbes::ExecuteTypedASTPowerFloat(
						static_cast<float>(Case.Base),
						static_cast<float>(Case.Exponent)));
			}
			else if (PowerShape == EPowerProbeShape::Double)
			{
				TypedResult =
					AngelscriptTestJITProbes::ExecuteTypedASTPowerDouble(
						Case.Base, Case.Exponent);
			}
			else
			{
				TypedResult =
					AngelscriptTestJITProbes::ExecuteTypedASTPowerDoubleInt(
						Case.Base, static_cast<int32>(Case.Exponent));
			}

			if (!Test.TestEqual(
					*FString::Printf(TEXT("%s interpreter result"), Label),
					InterpreterResult, Case.Expected)
				|| !Test.TestEqual(
					*FString::Printf(TEXT("%s BytecodeJIT result"), Label),
					BytecodeResult, Case.Expected)
				|| !Test.TestEqual(
					*FString::Printf(TEXT("%s TypedASTJIT result"), Label),
					TypedResult, Case.Expected))
			{
				return false;
			}
		}
		return true;
	};

	if (!RunPowerCases(
			FloatCases, *InterpreterPowerFloat, *BytecodePowerFloat,
			EPowerProbeShape::Float, TEXT("POWf"))
		|| !RunPowerCases(
			DoubleCases, *InterpreterPowerDouble, *BytecodePowerDouble,
			EPowerProbeShape::Double, TEXT("POWd"))
		|| !RunPowerCases(
			DoubleIntCases, *InterpreterPowerDoubleInt,
			*BytecodePowerDoubleInt,
			EPowerProbeShape::DoubleInt, TEXT("POWdi")))
	{
		return false;
	}

	return Test.TestEqual(
		TEXT("The compiled TypedASTJIT mutation/power probes should execute exactly once per row"),
		AngelscriptTestJITProbes::GetTypedASTMutationPowerProbeCount(),
		static_cast<int32>(UE_ARRAY_COUNT(MutationCases)
			+ UE_ARRAY_COUNT(FloatCases)
			+ UE_ARRAY_COUNT(DoubleCases)
			+ UE_ARRAY_COUNT(DoubleIntCases)));
}

bool FAngelscriptStaticJITAotTests::RunTypedASTStructuredControlFlowParity(
	FAutomationTestBase& Test)
{
	TUniquePtr<AngelscriptStaticJITAotGeneration::
		FStaticJITAotFixtureEngineSession> InterpreterSession =
			AngelscriptStaticJITAotGeneration::
				CreateInterpreterFixtureEngineSession();
	if (!Test.TestNotNull(
			TEXT("TypedASTJIT control-flow parity should create an isolated interpreter Engine"),
			InterpreterSession.Get())
		|| !Test.TestTrue(
			TEXT("TypedASTJIT control-flow parity interpreter Engine should compile the fixture"),
			InterpreterSession.IsValid() && InterpreterSession->IsValid()))
	{
		if (InterpreterSession.IsValid())
		{
			Test.AddError(InterpreterSession->Error);
		}
		return false;
	}

	FAngelscriptEngine* const BytecodeEngine = GetSharedAotFixtureEngine(Test);
	if (BytecodeEngine == nullptr)
	{
		return false;
	}

	asCScriptFunction* const InterpreterStructured = FindGlobalFunctionInModule(
		Test, *InterpreterSession->Engine,
		AngelscriptStaticJITAotFixture::GetTypedControlFlowModuleName(),
		AngelscriptStaticJITAotFixture::
			GetSemanticStructuredControlFlowDeclaration());
	asCScriptFunction* const BytecodeStructured = FindGlobalFunctionInModule(
		Test, *BytecodeEngine,
		AngelscriptStaticJITAotFixture::GetTypedControlFlowModuleName(),
		AngelscriptStaticJITAotFixture::
			GetSemanticStructuredControlFlowDeclaration());
	asCScriptFunction* const InterpreterEnum = FindGlobalFunctionInModule(
		Test, *InterpreterSession->Engine,
		AngelscriptStaticJITAotFixture::GetTypedControlFlowModuleName(),
		AngelscriptStaticJITAotFixture::
			GetSemanticExhaustiveEnumDeclaration());
	asCScriptFunction* const BytecodeEnum = FindGlobalFunctionInModule(
		Test, *BytecodeEngine,
		AngelscriptStaticJITAotFixture::GetTypedControlFlowModuleName(),
		AngelscriptStaticJITAotFixture::
			GetSemanticExhaustiveEnumDeclaration());
	if (InterpreterStructured == nullptr || BytecodeStructured == nullptr
		|| InterpreterEnum == nullptr || BytecodeEnum == nullptr)
	{
		return false;
	}

	if (!Test.TestNull(
			TEXT("The isolated interpreter structured function should have no JIT VM entry"),
			InterpreterStructured->GetJITBinding().VMEntry)
		|| !Test.TestNull(
			TEXT("The isolated interpreter enum function should have no JIT VM entry"),
			InterpreterEnum->GetJITBinding().VMEntry))
	{
		return false;
	}
	const TPair<asCScriptFunction*, const TCHAR*> BytecodeFunctions[] = {
		{BytecodeStructured, TEXT("structured-control-flow BytecodeJIT function")},
		{BytecodeEnum, TEXT("exhaustive-enum BytecodeJIT function")},
	};
	for (const TPair<asCScriptFunction*, const TCHAR*>& Pair
		: BytecodeFunctions)
	{
		uint32 TransientFunctionId = 0;
		if (!RequireJitEntries(
				Test, *BytecodeEngine, Pair.Key, Pair.Value,
				TransientFunctionId))
		{
			return false;
		}
	}

	if (!Test.TestTrue(
			TEXT("The generated TestJIT DLL should register structured-control-flow and exhaustive-enum probes"),
			AngelscriptTestJITProbes::HasTypedASTControlFlowProbes()))
	{
		return false;
	}

	auto ExecuteIntPair = [&Test](
		FAngelscriptEngine& Engine,
		asCScriptFunction& Function,
		const int32 First,
		const int32 Second,
		const TCHAR* Label,
		int32& OutResult)
	{
		FAngelscriptEngineScope Scope(Engine);
		asIScriptContext* const Context =
			Engine.GetScriptEngine()->CreateContext();
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should create a script context"), Label),
				Context))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		if (!Test.TestEqual(
				*FString::Printf(TEXT("%s should prepare"), Label),
				Context->Prepare(&Function), asSUCCESS)
			|| !Test.TestEqual(
				*FString::Printf(TEXT("%s should bind the first argument"), Label),
				Context->SetArgDWord(0, static_cast<asDWORD>(First)),
				asSUCCESS)
			|| !Test.TestEqual(
				*FString::Printf(TEXT("%s should bind the second argument"), Label),
				Context->SetArgDWord(1, static_cast<asDWORD>(Second)),
				asSUCCESS)
			|| !Test.TestEqual(
				*FString::Printf(TEXT("%s should finish"), Label),
				Context->Execute(), asEXECUTION_FINISHED))
		{
			return false;
		}
		OutResult = static_cast<int32>(Context->GetReturnDWord());
		return true;
	};

	struct FStructuredCase
	{
		int32 Limit;
		int32 Selector;
		int32 Expected;
	};
	const FStructuredCase StructuredCases[] = {
		{0, 0, 105},
		{1, 1, 228},
		{4, 2, 84},
		{4, 3, 78},
		{6, 99, 77},
	};

	AngelscriptTestJITProbes::ResetTypedASTControlFlowProbeCount();
	for (const FStructuredCase& Case : StructuredCases)
	{
		int32 InterpreterResult = 0;
		int32 BytecodeResult = 0;
		if (!ExecuteIntPair(
				*InterpreterSession->Engine, *InterpreterStructured,
				Case.Limit, Case.Selector,
				TEXT("structured-control-flow interpreter"),
				InterpreterResult)
			|| !ExecuteIntPair(
				*BytecodeEngine, *BytecodeStructured,
				Case.Limit, Case.Selector,
				TEXT("structured-control-flow BytecodeJIT"),
				BytecodeResult))
		{
			return false;
		}
		const int32 TypedResult =
			AngelscriptTestJITProbes::ExecuteTypedASTStructuredControlFlow(
				Case.Limit, Case.Selector);
		if (!Test.TestEqual(
				TEXT("Interpreter structured-control-flow result"),
				InterpreterResult, Case.Expected)
			|| !Test.TestEqual(
				TEXT("BytecodeJIT structured-control-flow result"),
				BytecodeResult, Case.Expected)
			|| !Test.TestEqual(
				TEXT("TypedASTJIT structured-control-flow result"),
				TypedResult, Case.Expected))
		{
			return false;
		}
	}

	auto ExecuteEnum = [&Test](
		FAngelscriptEngine& Engine,
		asCScriptFunction& Function,
		const int32 Mode,
		const TCHAR* Label,
		int32& OutStatus,
		int32& OutResult,
		FString& OutException)
	{
		FAngelscriptEngineScope Scope(Engine);
		asIScriptContext* const Context =
			Engine.GetScriptEngine()->CreateContext();
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should create a script context"), Label),
				Context))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		if (!Test.TestEqual(
				*FString::Printf(TEXT("%s should prepare"), Label),
				Context->Prepare(&Function), asSUCCESS)
			|| !Test.TestEqual(
				*FString::Printf(TEXT("%s should bind the enum carrier"), Label),
				Context->SetArgDWord(0, static_cast<asDWORD>(Mode)), asSUCCESS))
		{
			return false;
		}
		{
			FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
			OutStatus = Context->Execute();
		}
		OutResult = static_cast<int32>(Context->GetReturnDWord());
		OutException = UTF8_TO_TCHAR(
			Context->GetExceptionString() != nullptr
				? Context->GetExceptionString() : "");
		return true;
	};

	struct FEnumCase
	{
		int32 Mode;
		int32 Expected;
	};
	const FEnumCase EnumCases[] = {
		{0, 10},
		{1, 20},
		{2, 30},
	};
	for (const FEnumCase& Case : EnumCases)
	{
		int32 InterpreterStatus = asERROR;
		int32 InterpreterResult = 0;
		FString InterpreterException;
		int32 BytecodeStatus = asERROR;
		int32 BytecodeResult = 0;
		FString BytecodeException;
		if (!ExecuteEnum(
				*InterpreterSession->Engine, *InterpreterEnum, Case.Mode,
				TEXT("exhaustive-enum interpreter"),
				InterpreterStatus, InterpreterResult, InterpreterException)
			|| !ExecuteEnum(
				*BytecodeEngine, *BytecodeEnum, Case.Mode,
				TEXT("exhaustive-enum BytecodeJIT"),
				BytecodeStatus, BytecodeResult, BytecodeException))
		{
			return false;
		}
		bool bTypedException = true;
		int32 TypedResult = 0;
		{
			FAngelscriptEngineScope Scope(*BytecodeEngine);
			TypedResult =
				AngelscriptTestJITProbes::ExecuteTypedASTExhaustiveEnum(
					Case.Mode, bTypedException);
		}
		if (!Test.TestEqual(TEXT("Interpreter exhaustive-enum status"),
				InterpreterStatus, asEXECUTION_FINISHED)
			|| !Test.TestEqual(TEXT("BytecodeJIT exhaustive-enum status"),
				BytecodeStatus, asEXECUTION_FINISHED)
			|| !Test.TestEqual(TEXT("Interpreter exhaustive-enum result"),
				InterpreterResult, Case.Expected)
			|| !Test.TestEqual(TEXT("BytecodeJIT exhaustive-enum result"),
				BytecodeResult, Case.Expected)
			|| !Test.TestEqual(TEXT("TypedASTJIT exhaustive-enum result"),
				TypedResult, Case.Expected)
			|| !Test.TestFalse(
				TEXT("TypedASTJIT valid enum values should not set an exception"),
				bTypedException))
		{
			return false;
		}
	}

	constexpr int32 InvalidMode = 99;
	int32 InterpreterInvalidStatus = asERROR;
	int32 InterpreterInvalidResult = 0;
	FString InterpreterInvalidException;
	int32 BytecodeInvalidStatus = asERROR;
	int32 BytecodeInvalidResult = 0;
	FString BytecodeInvalidException;
	if (!ExecuteEnum(
			*InterpreterSession->Engine, *InterpreterEnum, InvalidMode,
			TEXT("invalid exhaustive-enum interpreter"),
			InterpreterInvalidStatus, InterpreterInvalidResult,
			InterpreterInvalidException)
		|| !ExecuteEnum(
			*BytecodeEngine, *BytecodeEnum, InvalidMode,
			TEXT("invalid exhaustive-enum BytecodeJIT"),
			BytecodeInvalidStatus, BytecodeInvalidResult,
			BytecodeInvalidException))
	{
		return false;
	}
	bool bTypedInvalidException = false;
	int32 TypedInvalidResult = 99;
	{
		FAngelscriptEngineScope Scope(*BytecodeEngine);
		FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
		TypedInvalidResult =
			AngelscriptTestJITProbes::ExecuteTypedASTExhaustiveEnum(
				InvalidMode, bTypedInvalidException);
	}
	const FString ExpectedInvalidException(
		TEXT("Invalid enum value passed to switch"));
	return Test.TestEqual(
			TEXT("Interpreter invalid exhaustive-enum status"),
			InterpreterInvalidStatus, asEXECUTION_EXCEPTION)
		&& Test.TestEqual(
			TEXT("BytecodeJIT invalid exhaustive-enum status"),
			BytecodeInvalidStatus, asEXECUTION_EXCEPTION)
		&& Test.TestEqual(
			TEXT("Interpreter invalid exhaustive-enum message"),
			InterpreterInvalidException, ExpectedInvalidException)
		&& Test.TestEqual(
			TEXT("BytecodeJIT invalid exhaustive-enum message"),
			BytecodeInvalidException, ExpectedInvalidException)
		&& Test.TestTrue(
			TEXT("TypedASTJIT invalid exhaustive-enum value should set the execution exception"),
			bTypedInvalidException)
		&& Test.TestEqual(
			TEXT("TypedASTJIT invalid exhaustive-enum value should return the deterministic failure value"),
			TypedInvalidResult, 0)
		&& Test.TestEqual(
			TEXT("The compiled TypedASTJIT control-flow probes should execute exactly once per row"),
			AngelscriptTestJITProbes::GetTypedASTControlFlowProbeCount(),
			static_cast<int32>(UE_ARRAY_COUNT(StructuredCases)
				+ UE_ARRAY_COUNT(EnumCases) + 1));
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
	Test.AddExpectedError(
		TEXT("int ExceptionEntryForAOT() | Line"),
		EAutomationExpectedErrorFlags::Contains,
		2,
		false);
	if (!Test.TestEqual(
			TEXT("StaticJIT.AOT nested generated exception should reach the outer context"),
			Context->Execute(),
			asEXECUTION_EXCEPTION))
	{
		return false;
	}

	const asIScriptFunction* const ExceptionFunction =
		Context->GetExceptionFunction();
	const char* const ExceptionString = Context->GetExceptionString();
	if (!Test.TestEqual(
			TEXT("StaticJIT.AOT should preserve the first nested exception message"),
			FString(UTF8_TO_TCHAR(
				ExceptionString != nullptr ? ExceptionString : "")),
			AngelscriptStaticJITAotFixture::GetExpectedExceptionText())
		|| !Test.TestNotNull(
			TEXT("StaticJIT.AOT should preserve the nested throwing function"),
			ExceptionFunction)
		|| (ExceptionFunction != nullptr
			&& !Test.TestEqual(
				TEXT("StaticJIT.AOT should report the nested throwing declaration"),
				FString(UTF8_TO_TCHAR(
					ExceptionFunction->GetDeclaration(false, false, false))),
				FString(TEXT("void FailForAOT()")))))
	{
		return false;
	}

	int32 ExceptionColumn = INDEX_NONE;
	const char* ExceptionSection = nullptr;
	const int32 ExceptionLine = Context->GetExceptionLineNumber(
		&ExceptionColumn, &ExceptionSection);
	return Test.TestEqual(
			TEXT("StaticJIT.AOT should preserve the processed nested exception section"),
			FString(UTF8_TO_TCHAR(
				ExceptionSection != nullptr ? ExceptionSection : "")),
			FString(TEXT("/Angelscript/Game/ASStaticJITAotFixture.as")))
		&& Test.TestEqual(
			TEXT("StaticJIT.AOT should preserve the nested throwing line"),
			ExceptionLine,
			AngelscriptStaticJITAotFixture::GetExpectedNestedThrowLine())
		&& Test.TestEqual(
			TEXT("StaticJIT.AOT should preserve the exact nested VM column"),
			ExceptionColumn,
			2);
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
		{ AngelscriptStaticJITAotFixture::GetMethodRpcImplementationDeclaration(), TEXT("RPC implementation method") },
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
		{ GeneratedClass, TEXT("ClientStoreRpcValue"), UASFunction_NotThreadSafe::StaticClass(), TEXT("generic RPC/ProcessEvent wrapper") },
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

bool FAngelscriptStaticJITAotTests::RunTypedUASFunctionIdentityAndParmsDispatch(
	FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);

	UClass* const StaticsClass = FindGeneratedClass(
		&Engine,
		AngelscriptStaticJITAotFixture::
			GetTypedControlFlowStaticsClassName());
	asCScriptFunction* const StructuredFunction = FindGlobalFunctionInModule(
		Test,
		Engine,
		AngelscriptStaticJITAotFixture::GetTypedControlFlowModuleName(),
		AngelscriptStaticJITAotFixture::
			GetSemanticStructuredControlFlowDeclaration());
	asCScriptFunction* const EnumFunction = FindGlobalFunctionInModule(
		Test,
		Engine,
		AngelscriptStaticJITAotFixture::GetTypedControlFlowModuleName(),
		AngelscriptStaticJITAotFixture::
			GetSemanticExhaustiveEnumDeclaration());
	UASFunction* const StructuredWrapper = StaticsClass != nullptr
		? Cast<UASFunction>(FindGeneratedFunction(
			StaticsClass, TEXT("SemanticStructuredControlFlow")))
		: nullptr;
	UASFunction* const EnumWrapper = StaticsClass != nullptr
		? Cast<UASFunction>(FindGeneratedFunction(
			StaticsClass, TEXT("SemanticExhaustiveEnum")))
		: nullptr;
	if (!Test.TestNotNull(
			TEXT("TypedASTJIT reflected control-flow fixture should generate its statics class"),
			StaticsClass)
		|| !Test.TestNotNull(
			TEXT("TypedASTJIT structured root should expose a UASFunction"),
			StructuredWrapper)
		|| !Test.TestNotNull(
			TEXT("TypedASTJIT enum root should expose a UASFunction"),
			EnumWrapper)
		|| StructuredFunction == nullptr || EnumFunction == nullptr)
	{
		return false;
	}

	uint32 StructuredFunctionId = 0;
	uint32 EnumFunctionId = 0;
	if (!RequireJitEntries(
			Test, Engine, StructuredFunction,
			TEXT("production TypedASTJIT structured UFUNCTION root"),
			StructuredFunctionId)
		|| !RequireJitEntries(
			Test, Engine, EnumFunction,
			TEXT("production TypedASTJIT enum UFUNCTION root"),
			EnumFunctionId))
	{
		return false;
	}

	const TPair<UASFunction*, asCScriptFunction*> Wrappers[] = {
		{StructuredWrapper, StructuredFunction},
		{EnumWrapper, EnumFunction},
	};
	for (const TPair<UASFunction*, asCScriptFunction*>& Pair : Wrappers)
	{
		if (!Test.TestTrue(
				TEXT("TypedASTJIT UFUNCTION wrapper should retain its exact current asCScriptFunction"),
				Pair.Key->ScriptFunction == Pair.Value)
			|| !Test.TestTrue(
				TEXT("TypedASTJIT should reuse the existing reloadable generic UASFunction wrapper"),
				Pair.Key->GetClass()
					== UASFunction_NotThreadSafe::StaticClass())
			|| !Test.TestTrue(
				TEXT("TypedASTJIT reflected root should retain AngelScript function origin"),
				IsAngelscriptGenerated(
					static_cast<const UFunction*>(Pair.Key))))
		{
			return false;
		}
	}

	// Use the loop-free exhaustive-enum root for the direct Parms route. The
	// structured root contains loops and therefore correctly selects the VM
	// while the editor timeout requirement is active because TypedASTJIT does
	// not advertise LoopTimeout support yet.
	FIntProperty* const ModeProperty = FindFProperty<FIntProperty>(
		EnumWrapper, TEXT("RawMode"));
	FIntProperty* const ReturnProperty = FindFProperty<FIntProperty>(
		EnumWrapper, TEXT("ReturnValue"));
	FObjectPropertyBase* const WorldContextProperty =
		FindFProperty<FObjectPropertyBase>(
			EnumWrapper, TEXT("_World_Context"));
	if (!Test.TestNotNull(
			TEXT("TypedASTJIT UASFunction should retain the RawMode property"),
			ModeProperty)
		|| !Test.TestNotNull(
			TEXT("TypedASTJIT UASFunction should retain the return property"),
			ReturnProperty)
		|| !Test.TestNotNull(
			TEXT("TypedASTJIT static UASFunction should retain its synthetic world-context property"),
			WorldContextProperty)
		|| !Test.TestTrue(
			TEXT("TypedASTJIT static UASFunction should identify its synthetic world-context layout"),
			EnumWrapper->bIsWorldContextGenerated
				&& EnumWrapper->WorldContextOffsetInParms
					== WorldContextProperty->GetOffset_ForUFunction())
		|| !Test.TestTrue(
			TEXT("TypedASTJIT UASFunction RawMode should retain AngelScript property origin"),
			IsAngelscriptGenerated(ModeProperty))
		|| !Test.TestTrue(
			TEXT("TypedASTJIT UASFunction return should retain AngelScript property origin"),
			IsAngelscriptGenerated(ReturnProperty)))
	{
		return false;
	}

	UFunction* const NativeFunction =
		UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(
			TEXT("RequiresWorldContext"));
	FProperty* const NativeValueProperty = NativeFunction != nullptr
		? FindFProperty<FProperty>(NativeFunction, TEXT("Value"))
		: nullptr;
	FProperty* const NativeReturnProperty = NativeFunction != nullptr
		? NativeFunction->GetReturnProperty()
		: nullptr;
	if (!Test.TestNotNull(
			TEXT("Native origin control should resolve its UFunction"),
			NativeFunction)
		|| !Test.TestNotNull(
			TEXT("Native origin control should resolve its parameter"),
			NativeValueProperty)
		|| !Test.TestNotNull(
			TEXT("Native origin control should resolve its return property"),
			NativeReturnProperty)
		|| !Test.TestFalse(
			TEXT("Native UFunction must not be classified as AngelScript-generated"),
			IsAngelscriptGenerated(NativeFunction))
		|| !Test.TestFalse(
			TEXT("Native UFunction parameter must not be classified as AngelScript-generated"),
			IsAngelscriptGenerated(NativeValueProperty))
		|| !Test.TestFalse(
			TEXT("Native UFunction return must not be classified as AngelScript-generated"),
			IsAngelscriptGenerated(NativeReturnProperty)))
	{
		return false;
	}

	const asSJITFunctionBinding Binding = EnumFunction->GetJITBinding();
	const FAngelscriptJITBindingContext* const BindingContext =
		FAngelscriptJITBindingContext::FromBindingUserData(Binding.UserData);
	if (!Test.TestNotNull(
			TEXT("TypedASTJIT UASFunction should retain its binding context"),
			BindingContext))
	{
		return false;
	}
	const uint64 VMCountBefore = BindingContext->GetExecutionCount(
		FAngelscriptJITBindingContext::EEntryKind::VM);
	const uint64 RawCountBefore = BindingContext->GetExecutionCount(
		FAngelscriptJITBindingContext::EEntryKind::Raw);
	const uint64 ParmsCountBefore = BindingContext->GetExecutionCount(
		FAngelscriptJITBindingContext::EEntryKind::Parms);

	FStructOnScope Params(EnumWrapper);
	ModeProperty->SetPropertyValue_InContainer(Params.GetStructMemory(), 2);
	UObject* const StaticsObject = StaticsClass->GetDefaultObject();
	WorldContextProperty->SetObjectPropertyValue_InContainer(
		Params.GetStructMemory(), StaticsObject);
	EnumWrapper->RuntimeCallEvent(
		StaticsObject, Params.GetStructMemory());
	if (!Test.TestEqual(
			*FString::Printf(TEXT(
				"TypedASTJIT UASFunction RuntimeCallEvent should return the exhaustive-enum native result (RawModeOffset=%d WorldContextOffset=%d ReturnOffset=%d ParmsSize=%d)"),
				ModeProperty->GetOffset_ForUFunction(),
				WorldContextProperty->GetOffset_ForUFunction(),
				ReturnProperty->GetOffset_ForUFunction(),
				EnumWrapper->ParmsSize),
			ReturnProperty->GetPropertyValue_InContainer(
				Params.GetStructMemory()),
			30)
		|| !Test.TestEqual(
			TEXT("TypedASTJIT UASFunction RuntimeCallEvent must not enter VMEntry"),
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::VM),
			VMCountBefore)
		|| !Test.TestEqual(
			TEXT("TypedASTJIT UASFunction RuntimeCallEvent must not enter RawEntry"),
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::Raw),
			RawCountBefore)
		|| !Test.TestEqual(
			TEXT("TypedASTJIT UASFunction RuntimeCallEvent should enter ParmsEntry exactly once"),
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::Parms),
			ParmsCountBefore + 1))
	{
		return false;
	}

	// Retire a separate Engine's module so the shared runtime fixture remains
	// current for the rest of this test class. UObject type identity is the
	// durable script-origin marker; retiring the script module must not mutate a
	// held UASFunction into a native UFunction classification.
	TUniquePtr<AngelscriptStaticJITAotGeneration::
		FStaticJITAotFixtureEngineSession> RetirementSession =
			AngelscriptStaticJITAotGeneration::CreateFixtureEngineSession(
				false,
				AngelscriptStaticJITAotGeneration::
					EStaticJITAotFixtureComposition::Core);
	if (!Test.TestNotNull(
			TEXT("TypedASTJIT stale-origin proof should create an isolated Engine"),
			RetirementSession.Get())
		|| !Test.TestTrue(
			TEXT("TypedASTJIT stale-origin proof should compile its isolated Engine"),
			RetirementSession.IsValid() && RetirementSession->IsValid()))
	{
		if (RetirementSession.IsValid())
		{
			Test.AddError(RetirementSession->Error);
		}
		return false;
	}
	FAngelscriptEngine& RetirementEngine = *RetirementSession->Engine;
	FAngelscriptEngineScope RetirementScope(RetirementEngine);
	UClass* const RetirementStaticsClass = FindGeneratedClass(
		&RetirementEngine,
		AngelscriptStaticJITAotFixture::
			GetTypedControlFlowStaticsClassName());
	TStrongObjectPtr<UASFunction> StaleFunction(
		RetirementStaticsClass != nullptr
			? Cast<UASFunction>(FindGeneratedFunction(
				RetirementStaticsClass,
				TEXT("SemanticStructuredControlFlow")))
			: nullptr);
	FProperty* const StaleLimitProperty = StaleFunction.IsValid()
		? FindFProperty<FProperty>(StaleFunction.Get(), TEXT("Limit"))
		: nullptr;
	if (!Test.TestTrue(
			TEXT("TypedASTJIT stale-origin proof should retain its UASFunction"),
			StaleFunction.IsValid())
		|| !Test.TestNotNull(
			TEXT("TypedASTJIT stale-origin proof should retain its parameter"),
			StaleLimitProperty))
	{
		return false;
	}
	RetirementEngine.DiscardModule(
		*AngelscriptStaticJITAotFixture::
			GetTypedControlFlowModuleName().ToString());
	return Test.TestTrue(
			TEXT("Retired TypedASTJIT UASFunction should remain AngelScript-generated"),
			IsAngelscriptGenerated(
				static_cast<const UFunction*>(StaleFunction.Get())))
		&& Test.TestTrue(
			TEXT("Retired TypedASTJIT UASFunction parameter should retain AngelScript origin"),
			IsAngelscriptGenerated(StaleLimitProperty));
}


bool FAngelscriptStaticJITAotTests::RunTypedFinalScalarRawDispatch(
	FAutomationTestBase& Test)
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
		AngelscriptStaticJITAotFixture::
			GetTypedFinalScalarReceiverClassName());
	UASFunction* const Wrapper = GeneratedClass != nullptr
		? Cast<UASFunction>(FindGeneratedFunction(
			GeneratedClass, TEXT("TypedFinalRawValue")))
		: nullptr;
	asCScriptFunction* const ScriptFunction = Wrapper != nullptr
		? static_cast<asCScriptFunction*>(Wrapper->ScriptFunction)
		: nullptr;
	FProperty* const ReturnProperty = Wrapper != nullptr
		? Wrapper->GetReturnProperty() : nullptr;
	if (!Test.TestNotNull(
			TEXT("TypedASTJIT final scalar fixture should generate its receiver class"),
			GeneratedClass)
		|| !Test.TestNotNull(
			TEXT("TypedASTJIT final scalar fixture should expose a UASFunction"),
			Wrapper)
		|| !Test.TestNotNull(
			TEXT("TypedASTJIT final scalar fixture should retain its exact script function"),
			ScriptFunction)
		|| !Test.TestNotNull(
			TEXT("TypedASTJIT final scalar fixture should retain its return property"),
			ReturnProperty))
	{
		return false;
	}

	uint32 FunctionId = 0;
	if (!RequireJitEntries(
			Test,
			Engine,
			ScriptFunction,
			TEXT("production TypedASTJIT final scalar Raw UFUNCTION root"),
			FunctionId))
	{
		return false;
	}
	if (!Test.TestTrue(
			TEXT("Reloadable final scalar dispatch should reuse the existing optimized DWord-return wrapper"),
			Wrapper->GetClass() == UASFunction_DWordReturn::StaticClass())
		|| !Test.TestTrue(
			TEXT("TypedASTJIT final scalar wrapper should retain AngelScript function origin"),
			IsAngelscriptGenerated(static_cast<const UFunction*>(Wrapper)))
		|| !Test.TestTrue(
			TEXT("TypedASTJIT final scalar return should retain AngelScript property origin"),
			IsAngelscriptGenerated(ReturnProperty)))
	{
		return false;
	}

	const asSJITFunctionBinding Binding = ScriptFunction->GetJITBinding();
	const FAngelscriptJITBindingContext* const BindingContext =
		FAngelscriptJITBindingContext::FromBindingUserData(Binding.UserData);
	if (!Test.TestNotNull(
			TEXT("TypedASTJIT final scalar Raw route should retain its binding context"),
			BindingContext))
	{
		return false;
	}

	const uint64 VMCountBefore = BindingContext->GetExecutionCount(
		FAngelscriptJITBindingContext::EEntryKind::VM);
	const uint64 RawCountBefore = BindingContext->GetExecutionCount(
		FAngelscriptJITBindingContext::EEntryKind::Raw);
	const uint64 ParmsCountBefore = BindingContext->GetExecutionCount(
		FAngelscriptJITBindingContext::EEntryKind::Parms);
	TStrongObjectPtr<UObject> Instance(NewObject<UObject>(
		GetTransientPackage(), GeneratedClass));
	FStructOnScope Params(Wrapper);
	Wrapper->RuntimeCallEvent(Instance.Get(), Params.GetStructMemory());

	return Test.TestEqual(
			TEXT("TypedASTJIT optimized final scalar wrapper should return the native Typed body result"),
			CastFieldChecked<FIntProperty>(ReturnProperty)->
				GetPropertyValue_InContainer(Params.GetStructMemory()),
			84)
		&& Test.TestEqual(
			TEXT("TypedASTJIT optimized final scalar wrapper must not enter VMEntry"),
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::VM),
			VMCountBefore)
		&& Test.TestEqual(
			TEXT("TypedASTJIT optimized final scalar wrapper should enter Raw exactly once"),
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::Raw),
			RawCountBefore + 1)
		&& Test.TestEqual(
			TEXT("TypedASTJIT optimized final scalar wrapper must not enter ParmsEntry"),
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::Parms),
			ParmsCountBefore);
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

bool FAngelscriptStaticJITAotTests::RunRpcProcessEventRoute(
	FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);

	UClass* const GeneratedClass = FindGeneratedClass(
		&Engine, AngelscriptStaticJITAotFixture::GetGeneratedClassName());
	asCScriptFunction* const RpcImplementation = FindMethodFunction(
		Test,
		Engine,
		AngelscriptStaticJITAotFixture::
			GetMethodRpcImplementationDeclaration());
	UASFunction* const RpcWrapper = GeneratedClass != nullptr
		? Cast<UASFunction>(FindGeneratedFunction(
			GeneratedClass, TEXT("ClientStoreRpcValue")))
		: nullptr;
	if (!Test.TestNotNull(
			TEXT("StaticJIT.AOT RPC route should generate the fixture class"),
			GeneratedClass)
		|| RpcImplementation == nullptr
		|| !Test.TestNotNull(
			TEXT("StaticJIT.AOT RPC route should expose its public UASFunction"),
			RpcWrapper))
	{
		return false;
	}

	const asSJITFunctionBinding Binding = RpcImplementation->GetJITBinding();
	const FAngelscriptJITBindingContext* const BindingContext =
		FAngelscriptJITBindingContext::FromBindingUserData(Binding.UserData);
	FIntProperty* const ValueProperty = FindFProperty<FIntProperty>(
		RpcWrapper, TEXT("Value"));
	FIntProperty* const StoredValueProperty = FindFProperty<FIntProperty>(
		GeneratedClass, TEXT("StoredValue"));
	if (!Test.TestTrue(
			TEXT("StaticJIT.AOT RPC wrapper should retain Net+Client routing flags"),
			RpcWrapper->HasAllFunctionFlags(FUNC_Net | FUNC_NetClient))
		|| !Test.TestTrue(
			TEXT("StaticJIT.AOT RPC wrapper must remain the generic route-aware UASFunction"),
			RpcWrapper->GetClass()
				== UASFunction_NotThreadSafe::StaticClass())
		|| !Test.TestTrue(
			TEXT("StaticJIT.AOT RPC wrapper should retain its current implementation function"),
			RpcWrapper->ScriptFunction == RpcImplementation)
		|| !Test.TestNotNull(
			TEXT("StaticJIT.AOT RPC implementation should retain VMEntry"),
			Binding.VMEntry)
		|| !Test.TestNotNull(
			TEXT("StaticJIT.AOT RPC implementation should retain RawEntry"),
			Binding.RawEntry)
		|| !Test.TestNotNull(
			TEXT("StaticJIT.AOT RPC implementation should retain ParmsEntry"),
			Binding.ParmsEntry)
		|| !Test.TestNotNull(
			TEXT("StaticJIT.AOT RPC implementation should retain its binding context"),
			BindingContext)
		|| !Test.TestNotNull(
			TEXT("StaticJIT.AOT RPC wrapper should expose Value"),
			ValueProperty)
		|| !Test.TestNotNull(
			TEXT("StaticJIT.AOT RPC fixture should expose StoredValue"),
			StoredValueProperty))
	{
		return false;
	}

	TStrongObjectPtr<UObject> Instance(NewObject<UObject>(
		GetTransientPackage(), GeneratedClass));
	if (!Test.TestNotNull(
			TEXT("StaticJIT.AOT RPC route should instantiate its receiver"),
			Instance.Get()))
	{
		return false;
	}

	const uint64 VMCountBefore = BindingContext->GetExecutionCount(
		FAngelscriptJITBindingContext::EEntryKind::VM);
	const uint64 RawCountBefore = BindingContext->GetExecutionCount(
		FAngelscriptJITBindingContext::EEntryKind::Raw);
	const uint64 ParmsCountBefore = BindingContext->GetExecutionCount(
		FAngelscriptJITBindingContext::EEntryKind::Parms);
	FStructOnScope Params(RpcWrapper);
	ValueProperty->SetPropertyValue_InContainer(
		Params.GetStructMemory(),
		AngelscriptStaticJITAotFixture::GetExpectedRpcStoredValue());
	Instance->ProcessEvent(RpcWrapper, Params.GetStructMemory());

	return Test.TestEqual(
			TEXT("StaticJIT.AOT RPC ProcessEvent route should execute the current implementation"),
			StoredValueProperty->GetPropertyValue_InContainer(Instance.Get()),
			AngelscriptStaticJITAotFixture::GetExpectedRpcStoredValue())
		&& Test.TestEqual(
			TEXT("StaticJIT.AOT RPC ProcessEvent route should enter VM exactly once"),
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::VM),
			VMCountBefore + 1)
		&& Test.TestEqual(
			TEXT("StaticJIT.AOT RPC ProcessEvent route must not raw-direct the implementation"),
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::Raw),
			RawCountBefore)
		&& Test.TestEqual(
			TEXT("StaticJIT.AOT RPC ProcessEvent route must not bypass routing through ParmsEntry"),
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::Parms),
			ParmsCountBefore);
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

bool FAngelscriptStaticJITAotTests::
	RunBytecodeFallbackGeneratedWorldContext(FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr = GetSharedAotFixtureEngine(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);

	UClass* const StaticsClass = FindGeneratedClass(
		&Engine, TEXT("UModule_ASStaticJITAotFixtureStatics"));
	asCScriptFunction* const ScriptFunction = FindGlobalFunction(
		Test, Engine,
		AngelscriptStaticJITAotFixture::
			GetBytecodeFallbackGeneratedWorldContextDeclaration());
	UASFunction* const Wrapper = StaticsClass != nullptr
		? Cast<UASFunction>(FindGeneratedFunction(
			StaticsClass, TEXT("BytecodeFallbackGeneratedWorldContext")))
		: nullptr;
	if (!Test.TestNotNull(
			TEXT("Bytecode fallback world-context fixture should generate its statics class"),
			StaticsClass)
		|| ScriptFunction == nullptr
		|| !Test.TestNotNull(
			TEXT("Bytecode fallback world-context fixture should expose a UASFunction"),
			Wrapper))
	{
		return false;
	}

	const asSJITFunctionBinding Binding = ScriptFunction->GetJITBinding();
	const FAngelscriptJITBindingContext* const BindingContext =
		FAngelscriptJITBindingContext::FromBindingUserData(Binding.UserData);
	FIntProperty* const ValueProperty = FindFProperty<FIntProperty>(
		Wrapper, TEXT("Value"));
	FObjectPropertyBase* const WorldContextProperty =
		FindFProperty<FObjectPropertyBase>(Wrapper, TEXT("_World_Context"));
	FIntProperty* const ReturnProperty = CastField<FIntProperty>(
		Wrapper->GetReturnProperty());
	if (!Test.TestNotNull(
			TEXT("Bytecode fallback should retain its VM entry"),
			Binding.VMEntry)
		|| !Test.TestNotNull(
			TEXT("Bytecode fallback should retain its Raw entry"),
			Binding.RawEntry)
		|| !Test.TestNull(
			TEXT("Bytecode fallback with a generated WorldContext suffix must fail closed instead of publishing an incompatible Parms entry"),
			Binding.ParmsEntry)
		|| !Test.TestNotNull(
			TEXT("Bytecode fallback should retain its binding context"),
			BindingContext)
		|| !Test.TestNotNull(
			TEXT("Bytecode fallback should retain its reflected Value property"),
			ValueProperty)
		|| !Test.TestNotNull(
			TEXT("Bytecode fallback should retain its generated WorldContext property"),
			WorldContextProperty)
		|| !Test.TestNotNull(
			TEXT("Bytecode fallback should retain its reflected return property"),
			ReturnProperty)
		|| !Test.TestTrue(
			TEXT("Bytecode fallback should identify the synthetic WorldContext suffix"),
			Wrapper->bIsWorldContextGenerated
				&& Wrapper->WorldContextOffsetInParms
					== WorldContextProperty->GetOffset_ForUFunction()))
	{
		return false;
	}

	// RuntimeCallEvent without a Parms entry uses the ordinary AS Context to
	// marshal the reflected parameter layout. asCContext then acquires the
	// current Binding and enters VMEntry through the shared execution context,
	// so the exact VM entry invocation is observable once while Parms remains
	// unused.
	const uint64 VMWrapperCountBefore = BindingContext->GetExecutionCount(
		FAngelscriptJITBindingContext::EEntryKind::VM);
	const uint64 RawWrapperCountBefore = BindingContext->GetExecutionCount(
		FAngelscriptJITBindingContext::EEntryKind::Raw);
	const uint64 ParmsCountBefore = BindingContext->GetExecutionCount(
		FAngelscriptJITBindingContext::EEntryKind::Parms);
	FStructOnScope Params(Wrapper);
	ValueProperty->SetPropertyValue_InContainer(
		Params.GetStructMemory(), 31);
	WorldContextProperty->SetObjectPropertyValue_InContainer(
		Params.GetStructMemory(), StaticsClass->GetDefaultObject());
	Wrapper->RuntimeCallEvent(
		StaticsClass->GetDefaultObject(), Params.GetStructMemory());

	return Test.TestEqual(
			TEXT("Bytecode fallback RuntimeCallEvent should preserve the return value after the generated WorldContext suffix"),
			ReturnProperty->GetPropertyValue_InContainer(
				Params.GetStructMemory()),
			AngelscriptStaticJITAotFixture::
				GetExpectedBytecodeFallbackGeneratedWorldContextResult())
		&& Test.TestEqual(
			TEXT("Bytecode fallback AS Context dispatch should enter the current VM entry exactly once"),
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::VM),
			VMWrapperCountBefore + 1)
		&& Test.TestEqual(
			TEXT("Bytecode fallback AS Context dispatch must not enter the explicit Raw wrapper"),
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::Raw),
			RawWrapperCountBefore)
		&& Test.TestEqual(
			TEXT("Bytecode fallback RuntimeCallEvent must not enter Parms"),
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::Parms),
			ParmsCountBefore);
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

struct FAngelscriptStaticJITAotUASFunctionDispatchTestAccess
{
	static void SetUpFixture()
	{
		FAngelscriptStaticJITAotTests::SharedFixtureSession =
			AngelscriptStaticJITAotGeneration::CreateFixtureEngineSession(false);
	}

	static void TearDownFixture()
	{
		FAngelscriptStaticJITAotTests::SharedFixtureSession.Reset();
	}

	static bool RunJitEntryAttachment(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotTests::RunUASFunctionJitEntryAttachment(
			Test);
	}

	static bool RunRuntimeCallEvent(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotTests::RunUASFunctionRuntimeCallEvent(
			Test);
	}

	static bool RunRpcProcessEventRoute(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotTests::RunRpcProcessEventRoute(Test);
	}

	static bool RunTypedIdentityAndParmsDispatch(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotTests::
			RunTypedUASFunctionIdentityAndParmsDispatch(Test);
	}

	static bool RunTypedFinalScalarRawDispatch(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotTests::
			RunTypedFinalScalarRawDispatch(Test);
	}

	static bool RunStaticWorldContextRuntimeCallEvent(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotTests::
			RunStaticWorldContextRuntimeCallEvent(Test);
	}

	static bool RunBytecodeFallbackGeneratedWorldContext(
		FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotTests::
			RunBytecodeFallbackGeneratedWorldContext(Test);
	}
};

namespace AngelscriptStaticJITAotUASFunctionDispatchTestBridge
{
	void SetUpFixture()
	{
		FAngelscriptStaticJITAotUASFunctionDispatchTestAccess::SetUpFixture();
	}

	void TearDownFixture()
	{
		FAngelscriptStaticJITAotUASFunctionDispatchTestAccess::
			TearDownFixture();
	}

	bool RunJitEntryAttachment(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotUASFunctionDispatchTestAccess::
			RunJitEntryAttachment(Test);
	}

	bool RunRuntimeCallEvent(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotUASFunctionDispatchTestAccess::
			RunRuntimeCallEvent(Test);
	}

	bool RunRpcProcessEventRoute(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotUASFunctionDispatchTestAccess::
			RunRpcProcessEventRoute(Test);
	}

	bool RunTypedIdentityAndParmsDispatch(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotUASFunctionDispatchTestAccess::
			RunTypedIdentityAndParmsDispatch(Test);
	}

	bool RunTypedFinalScalarRawDispatch(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotUASFunctionDispatchTestAccess::
			RunTypedFinalScalarRawDispatch(Test);
	}

	bool RunStaticWorldContextRuntimeCallEvent(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotUASFunctionDispatchTestAccess::
			RunStaticWorldContextRuntimeCallEvent(Test);
	}

	bool RunBytecodeFallbackGeneratedWorldContext(FAutomationTestBase& Test)
	{
		return FAngelscriptStaticJITAotUASFunctionDispatchTestAccess::
			RunBytecodeFallbackGeneratedWorldContext(Test);
	}
}

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
