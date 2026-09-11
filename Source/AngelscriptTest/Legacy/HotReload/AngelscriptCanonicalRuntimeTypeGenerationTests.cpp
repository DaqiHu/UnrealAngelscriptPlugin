#include "AngelscriptTestMacros.h"
#include "../AngelScriptSDK/Support/AngelscriptNativeCoreTestSupport.h"

#include "CQTest.h"
#include "Async/Async.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_public_view.h"
#include "as_module.h"
#include "as_runtime_type_binding.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalRuntimeTypeGenerationTests,
	"Angelscript.TestModule.HotReload.CanonicalAST.RuntimeTypeGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ExecuteInt(
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		int32& OutValue)
	{
		if (Engine == nullptr || Function == nullptr)
		{
			return false;
		}
		asIScriptContext* const Context = Engine->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		const int PrepareResult = Context->Prepare(Function);
		const int ExecuteResult = PrepareResult >= 0
			? Context->Execute() : PrepareResult;
		if (ExecuteResult == asEXECUTION_FINISHED)
		{
			OutValue = static_cast<int32>(Context->GetReturnDWord());
		}
		Context->Release();
		return ExecuteResult == asEXECUTION_FINISHED;
	}

	static int AddSection(
		asCModule* Module,
		const char* SectionName,
		const char* Source)
	{
		return Module != nullptr && Source != nullptr
			? Module->AddScriptSection(
				SectionName, Source, FCStringAnsi::Strlen(Source), 0)
			: asINVALID_ARG;
	}

	static const asSResolvedRuntimeTypeBinding* FindTypeBinding(
		const asCASTSnapshot& Snapshot,
		const char* TypeName)
	{
		for (asUINT Index = 0;
			Index < Snapshot.GetRuntimeTypeBindingCountForDiagnostics();
			++Index)
		{
			const asSResolvedRuntimeTypeBinding* const Binding =
				Snapshot.GetRuntimeTypeBindingForDiagnostics(Index);
			if (Binding != nullptr && Binding->typeInfo != nullptr
				&& FCStringAnsi::Strcmp(Binding->typeInfo->GetName(), TypeName) == 0)
			{
				return Binding;
			}
		}
		return nullptr;
	}

public:
	TEST_METHOD(SameModuleReplacementRetiresGenerationBehindSnapshotLease)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = static_cast<asCModule*>(
			ScriptEngine->GetModule(
				"CanonicalRuntimeTypeGeneration",
				asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));

		const char* const SourceA = R"AS(
			class FGenerationPayload
			{
				int Value;
			}

			int GenerationEntry()
			{
				FGenerationPayload Payload;
				Payload.Value = 11;
				return Payload.Value;
			}
		)AS";
		ASSERT_THAT(AreEqual(asSUCCESS,
			AddSection(Module, "GenerationA.as", SourceA)));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build(),
			TEXT("generation A must build through the canonical aggregate transaction")));

		asCASTSnapshot* const SnapshotA = static_cast<asCASTSnapshot*>(
			Module->AcquireASTSnapshot(asAST_API_VERSION_1));
		ASSERT_THAT(IsNotNull(SnapshotA));
		if (SnapshotA == nullptr)
		{
			return;
		}
		asITypeInfo* const TypeA = Module->GetTypeInfoByName(
			"FGenerationPayload");
		ASSERT_THAT(IsNotNull(TypeA));
		if (TypeA == nullptr)
		{
			SnapshotA->Release();
			return;
		}
		const int TypeIdA = TypeA->GetTypeId();
		const asUINT SizeA = TypeA->GetSize();
		const asSResolvedRuntimeTypeBinding* const BindingA =
			FindTypeBinding(*SnapshotA, "FGenerationPayload");
		const int BindingTypeIdA = BindingA != nullptr
			? ScriptEngine->GetTypeIdFromDataType(BindingA->dataType)
			: -1;
		ASSERT_THAT(IsNotNull(BindingA,
			TEXT("generation A snapshot must retain its frozen aggregate Runtime binding table")));
		ASSERT_THAT(IsTrue(BindingA != nullptr
			&& BindingA->typeInfo == TypeA
			&& BindingTypeIdA >= 0
			&& BindingA->dataType.IsObjectHandle()
			&& BindingA->publicTypeId == BindingTypeIdA,
			TEXT("generation-local binding must project A's exact TypeInfo and handle-qualified public TypeId")));

		const char* const SourceB = R"AS(
			class FGenerationPayload
			{
				int Padding;
				int Value;
			}

			int GenerationEntry()
			{
				FGenerationPayload Payload;
				Payload.Padding = 7;
				Payload.Value = 29;
				return Payload.Value;
			}
		)AS";
		ASSERT_THAT(AreEqual(asSUCCESS,
			AddSection(Module, "GenerationB.as", SourceB)));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build(),
			TEXT("generation B must atomically replace A")));

		asCASTSnapshot* const SnapshotB = static_cast<asCASTSnapshot*>(
			Module->AcquireASTSnapshot(asAST_API_VERSION_1));
		ASSERT_THAT(IsNotNull(SnapshotB));
		ASSERT_THAT(IsFalse(SnapshotA->IsCurrentGeneration()));
		ASSERT_THAT(IsTrue(SnapshotA->IsRuntimeTypeGenerationRetiredForDiagnostics(),
			TEXT("A lease must own the retired executable/type generation")));
		ASSERT_THAT(IsTrue(SnapshotB != nullptr
			&& SnapshotB->IsCurrentGeneration()));

		asITypeInfo* const TypeB = Module->GetTypeInfoByName(
			"FGenerationPayload");
		ASSERT_THAT(IsNotNull(TypeB));
		const int TypeIdB = TypeB != nullptr ? TypeB->GetTypeId() : -1;
		ASSERT_THAT(IsTrue(TypeB != nullptr
			&& TypeB != TypeA
			&& TypeIdB != TypeIdA
			&& TypeB->GetSize() > SizeA,
			TEXT("same nominal type in B must publish a distinct ABI revision and projection")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->GetTypeInfoById(TypeIdA) == TypeA,
			TEXT("A's public TypeId map dependency must remain live while A is leased")));
		const asSResolvedRuntimeTypeBinding* const BindingAAfterB =
			FindTypeBinding(*SnapshotA, "FGenerationPayload");
		ASSERT_THAT(IsTrue(BindingAAfterB != nullptr
			&& BindingAAfterB->typeInfo == TypeA
			&& BindingAAfterB->publicTypeId == BindingTypeIdA,
			TEXT("publishing B must never retarget an immutable A binding slot")));

		asIScriptFunction* const RetiredEntry =
			SnapshotA->GetRetiredRuntimeFunctionByDeclForDiagnostics(
				"int GenerationEntry()");
		int32 ValueA = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(ScriptEngine, RetiredEntry, ValueA)
			&& ValueA == 11,
			TEXT("leased generation A must remain executable after B publishes")));
		int32 ValueB = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(
			ScriptEngine,
			Module->GetFunctionByDecl("int GenerationEntry()"),
			ValueB) && ValueB == 29,
			TEXT("new readers must execute generation B")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->GetTypeInfoById(TypeIdA) == TypeA,
			TEXT("completed A execution must not release resources before its snapshot lease")));

		SnapshotA->Release();
		ASSERT_THAT(IsNull(ScriptEngine->GetTypeInfoById(TypeIdA),
			TEXT("A resources and TypeId map entry release after its last execution/snapshot lease")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->GetTypeInfoById(TypeIdB) == TypeB,
			TEXT("retiring A must not disturb current generation B")));
		if (SnapshotB != nullptr)
		{
			SnapshotB->Release();
		}
	}

	TEST_METHOD(FailedResolutionOrSnapshotPreparationKeepsGenerationACurrent)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = static_cast<asCModule*>(
			ScriptEngine->GetModule(
				"CanonicalRuntimeTypeGenerationFailure",
				asGM_ALWAYS_CREATE));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(asSUCCESS, AddSection(Module,
			"GenerationA.as",
			"class FGenerationFailurePayload { int Value; } "
			"int GenerationFailureEntry() { FGenerationFailurePayload V; V.Value = 17; return V.Value; }")));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build()));

		asCASTSnapshot* const SnapshotA = static_cast<asCASTSnapshot*>(
			Module->AcquireASTSnapshot(asAST_API_VERSION_1));
		ASSERT_THAT(IsNotNull(SnapshotA));
		if (SnapshotA == nullptr)
		{
			return;
		}
		const FString GenerationKeyA = UTF8_TO_TCHAR(
			SnapshotA->GetGenerationKey());
		asITypeInfo* const TypeA = Module->GetTypeInfoByName(
			"FGenerationFailurePayload");
		const int TypeIdA = TypeA != nullptr ? TypeA->GetTypeId() : -1;
		const asUINT BindingCountA =
			SnapshotA->GetRuntimeTypeBindingCountForDiagnostics();
		const asSResolvedRuntimeTypeBinding* const BindingA =
			FindTypeBinding(*SnapshotA, "FGenerationFailurePayload");
		ASSERT_THAT(IsTrue(BindingA != nullptr
			&& BindingA->typeInfo == TypeA
			&& BindingA->publicTypeId == ScriptEngine->GetTypeIdFromDataType(
				BindingA->dataType),
			TEXT("generation A must expose its exact frozen Runtime binding before failure injection")));

		ASSERT_THAT(AreEqual(asSUCCESS, AddSection(Module,
			"GenerationBadResolution.as",
			"class FGenerationFailurePayload { int Padding; int Value; } "
			"int GenerationFailureEntry() { FGenerationFailurePayload V; V.Missing = 31; return V.Value; }")));
		ASSERT_THAT(IsTrue(Module->Build() < 0,
			TEXT("unresolved generation B must fail before publication")));
		asCASTSnapshot* const AfterResolutionFailure =
			static_cast<asCASTSnapshot*>(Module->AcquireASTSnapshot(
				asAST_API_VERSION_1));
		ASSERT_THAT(IsTrue(AfterResolutionFailure == SnapshotA
			&& AfterResolutionFailure->IsCurrentGeneration()
			&& GenerationKeyA == UTF8_TO_TCHAR(
				AfterResolutionFailure->GetGenerationKey())
			&& ScriptEngine->GetTypeInfoById(TypeIdA) == TypeA,
			TEXT("failed Runtime resolution must keep every A projection current")));
		ASSERT_THAT(IsTrue(
			AfterResolutionFailure->GetRuntimeTypeBindingCountForDiagnostics() == BindingCountA
			&& FindTypeBinding(*AfterResolutionFailure,
				"FGenerationFailurePayload") == BindingA,
			TEXT("failed Runtime resolution must preserve A's exact immutable binding table")));
		AfterResolutionFailure->Release();

		ASSERT_THAT(AreEqual(asSUCCESS, AddSection(Module,
			"GenerationSnapshotFailure.as",
			"class FGenerationFailurePayload { int Padding; int Value; } "
			"int GenerationFailureEntry() { FGenerationFailurePayload V; V.Value = 31; return V.Value; }")));
		Module->SetCanonicalSnapshotPreparationFailureForTesting(
			asOUT_OF_MEMORY);
		ASSERT_THAT(AreEqual(asOUT_OF_MEMORY, Module->Build(),
			TEXT("injected snapshot preparation failure must abort the aggregate transaction")));
		asCASTSnapshot* const AfterSnapshotFailure =
			static_cast<asCASTSnapshot*>(Module->AcquireASTSnapshot(
				asAST_API_VERSION_1));
		ASSERT_THAT(IsTrue(AfterSnapshotFailure == SnapshotA
			&& AfterSnapshotFailure->IsCurrentGeneration()
			&& GenerationKeyA == UTF8_TO_TCHAR(
				AfterSnapshotFailure->GetGenerationKey())
			&& ScriptEngine->GetTypeInfoById(TypeIdA) == TypeA,
			TEXT("failed snapshot preparation must leave A executable, mapped and current")));
		ASSERT_THAT(IsTrue(
			AfterSnapshotFailure->GetRuntimeTypeBindingCountForDiagnostics() == BindingCountA
			&& FindTypeBinding(*AfterSnapshotFailure,
				"FGenerationFailurePayload") == BindingA,
			TEXT("failed snapshot preparation must preserve A's exact immutable binding table")));
		int32 ValueA = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(
			ScriptEngine,
			Module->GetFunctionByDecl("int GenerationFailureEntry()"),
			ValueA) && ValueA == 17));
		AfterSnapshotFailure->Release();
		SnapshotA->Release();
	}

	TEST_METHOD(DiscardAndRecreateKeepsGenerationABehindItsSnapshotLease)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		const char* const ModuleName = "CanonicalRuntimeTypeDiscard";
		asCModule* const ModuleA = static_cast<asCModule*>(
			ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(ModuleA));
		ASSERT_THAT(AreEqual(asSUCCESS,
			ModuleA->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(asSUCCESS, AddSection(ModuleA,
			"DiscardGenerationA.as",
			"class FDiscardPayload { int Value; } "
			"int DiscardEntry() { FDiscardPayload V; V.Value = 41; return V.Value; }")));
		ASSERT_THAT(AreEqual(asSUCCESS, ModuleA->Build()));

		asCASTSnapshot* const SnapshotA = static_cast<asCASTSnapshot*>(
			ModuleA->AcquireASTSnapshot(asAST_API_VERSION_1));
		ASSERT_THAT(IsNotNull(SnapshotA));
		if (SnapshotA == nullptr)
		{
			return;
		}
		asITypeInfo* const TypeA = ModuleA->GetTypeInfoByName(
			"FDiscardPayload");
		ASSERT_THAT(IsNotNull(TypeA));
		const int TypeIdA = TypeA != nullptr ? TypeA->GetTypeId() : -1;

		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->DiscardModule(ModuleName)));
		ScriptEngine->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(IsFalse(SnapshotA->IsCurrentGeneration()));
		ASSERT_THAT(IsTrue(
			SnapshotA->IsRuntimeTypeGenerationRetiredForDiagnostics(),
			TEXT("discarded module storage must transfer to the leased retired carrier")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->GetTypeInfoById(TypeIdA) == TypeA,
			TEXT("discard must keep A's public TypeId map dependency while A is leased")));

		asCModule* const ModuleB = static_cast<asCModule*>(
			ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(ModuleB));
		ASSERT_THAT(AreEqual(asSUCCESS,
			ModuleB->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(asSUCCESS, AddSection(ModuleB,
			"DiscardGenerationB.as",
			"class FDiscardPayload { int Padding; int Value; } "
			"int DiscardEntry() { FDiscardPayload V; V.Value = 73; return V.Value; }")));
		ASSERT_THAT(AreEqual(asSUCCESS, ModuleB->Build(),
			TEXT("the discarded generation must leave no authoritative type/function lookup entries")));
		asITypeInfo* const TypeB = ModuleB->GetTypeInfoByName(
			"FDiscardPayload");
		ASSERT_THAT(IsTrue(TypeB != nullptr
			&& TypeB != TypeA
			&& TypeB->GetTypeId() != TypeIdA));

		int32 ValueA = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(
			ScriptEngine,
			SnapshotA->GetRetiredRuntimeFunctionByDeclForDiagnostics(
				"int DiscardEntry()"),
			ValueA) && ValueA == 41));
		int32 ValueB = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(
			ScriptEngine,
			ModuleB->GetFunctionByDecl("int DiscardEntry()"),
			ValueB) && ValueB == 73));

		SnapshotA->Release();
		ASSERT_THAT(IsNull(ScriptEngine->GetTypeInfoById(TypeIdA)));
	}

	TEST_METHOD(EmptyCanonicalRebuildRetiresTheLastExecutableGeneration)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = static_cast<asCModule*>(
			ScriptEngine->GetModule(
				"CanonicalRuntimeTypeEmptyRebuild",
				asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(asSUCCESS, AddSection(Module,
			"EmptyRebuildGenerationA.as",
			"class FEmptyRebuildPayload { int Value; } "
			"int EmptyRebuildEntry() { return 59; }")));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build()));

		asCASTSnapshot* const SnapshotA = static_cast<asCASTSnapshot*>(
			Module->AcquireASTSnapshot(asAST_API_VERSION_1));
		ASSERT_THAT(IsNotNull(SnapshotA));
		if (SnapshotA == nullptr)
		{
			return;
		}
		asITypeInfo* const TypeA = Module->GetTypeInfoByName(
			"FEmptyRebuildPayload");
		ASSERT_THAT(IsNotNull(TypeA));
		const int TypeIdA = TypeA != nullptr ? TypeA->GetTypeId() : -1;

		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build(),
			TEXT("a canonical build with no queued sections must publish an empty current module")));
		ASSERT_THAT(IsFalse(SnapshotA->IsCurrentGeneration()));
		ASSERT_THAT(IsTrue(
			SnapshotA->IsRuntimeTypeGenerationRetiredForDiagnostics()));
		asIASTSnapshot* const CurrentSnapshot =
			Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNull(CurrentSnapshot,
			TEXT("an empty module must not leave generation A advertised as current")));
		ASSERT_THAT(IsNull(Module->GetTypeInfoByName(
			"FEmptyRebuildPayload")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl(
			"int EmptyRebuildEntry()")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->GetTypeInfoById(TypeIdA) == TypeA));
		int32 ValueA = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(
			ScriptEngine,
			SnapshotA->GetRetiredRuntimeFunctionByDeclForDiagnostics(
				"int EmptyRebuildEntry()"),
			ValueA) && ValueA == 59));

		SnapshotA->Release();
		ASSERT_THAT(IsNull(ScriptEngine->GetTypeInfoById(TypeIdA)));
	}

	TEST_METHOD(PreparedExecutionKeepsRetiredGenerationAliveAfterSnapshotRelease)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = static_cast<asCModule*>(
			ScriptEngine->GetModule(
				"CanonicalRuntimeTypeExecutionLease",
				asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(asSUCCESS, AddSection(Module,
			"ExecutionLeaseGenerationA.as",
			"class FExecutionLeasePayload { int Value; } "
			"int ExecutionLeaseEntry() { FExecutionLeasePayload V; V.Value = 83; return V.Value; }")));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build()));

		asCASTSnapshot* const SnapshotA = static_cast<asCASTSnapshot*>(
			Module->AcquireASTSnapshot(asAST_API_VERSION_1));
		ASSERT_THAT(IsNotNull(SnapshotA));
		if (SnapshotA == nullptr)
		{
			return;
		}
		asITypeInfo* const TypeA = Module->GetTypeInfoByName(
			"FExecutionLeasePayload");
		ASSERT_THAT(IsNotNull(TypeA));
		const int TypeIdA = TypeA != nullptr ? TypeA->GetTypeId() : -1;

		ASSERT_THAT(AreEqual(asSUCCESS, AddSection(Module,
			"ExecutionLeaseGenerationB.as",
			"class FExecutionLeasePayload { int Padding; int Value; } "
			"int ExecutionLeaseEntry() { return 97; }")));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build()));

		asIScriptFunction* const RetiredEntry =
			SnapshotA->GetRetiredRuntimeFunctionByDeclForDiagnostics(
				"int ExecutionLeaseEntry()");
		ASSERT_THAT(IsNotNull(RetiredEntry));
		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr || RetiredEntry == nullptr)
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
			SnapshotA->Release();
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(RetiredEntry)));

		SnapshotA->Release();
		ASSERT_THAT(IsTrue(
			ScriptEngine->GetTypeInfoById(TypeIdA) == TypeA,
			TEXT("a prepared execution must lease the complete retired generation after its snapshot releases")));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(83, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(IsTrue(
			ScriptEngine->GetTypeInfoById(TypeIdA) == TypeA,
			TEXT("completed execution remains prepared and keeps its generation lease until Unprepare")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare()));
		ASSERT_THAT(IsNull(ScriptEngine->GetTypeInfoById(TypeIdA),
			TEXT("the final execution lease releases the retired generation and its public TypeId map")));
		Context->Release();
	}

	TEST_METHOD(ConcurrentAcquireNeverSeesRetiredGenerationAdvertisedAsCurrent)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = static_cast<asCModule*>(
			ScriptEngine->GetModule(
				"CanonicalRuntimeTypeAtomicPublication",
				asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(asSUCCESS, AddSection(Module,
			"AtomicPublication.as", "int AtomicPublicationEntry() { return 0; }")));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build()));

		FThreadSafeBool bStop = false;
		FThreadSafeBool bAcquired = false;
		FThreadSafeBool bSawRetiredCurrentGeneration = false;
		TFuture<void> Reader = Async(EAsyncExecution::Thread,
			[Module, &bStop, &bAcquired, &bSawRetiredCurrentGeneration]()
			{
				while (!bStop)
				{
					asCASTSnapshot* const Snapshot =
						static_cast<asCASTSnapshot*>(Module->AcquireASTSnapshot(
							asAST_API_VERSION_1));
					if (Snapshot == nullptr)
					{
						continue;
					}
					bAcquired = true;
					const bool bCurrentBefore = Snapshot->IsCurrentGeneration();
					const bool bRetired =
						Snapshot->IsRuntimeTypeGenerationRetiredForDiagnostics();
					const bool bCurrentAfter = Snapshot->IsCurrentGeneration();
					Snapshot->Release();
					if (bCurrentBefore && bCurrentAfter && bRetired)
					{
						bSawRetiredCurrentGeneration = true;
						break;
					}
				}
			});

		bool bBuildFailed = false;
		for (int32 Generation = 1; Generation <= 128; ++Generation)
		{
			const char* const Source = (Generation & 1) == 0
				? "int AtomicPublicationEntry() { return 2; }"
				: "int AtomicPublicationEntry() { return 1; }";
			if (AddSection(Module, "AtomicPublication.as", Source) != asSUCCESS
				|| Module->Build() != asSUCCESS)
			{
				bBuildFailed = true;
				break;
			}
		}
		bStop = true;
		Reader.Wait();

		ASSERT_THAT(IsTrue((bool)bAcquired));
		ASSERT_THAT(IsFalse(bBuildFailed));
		ASSERT_THAT(IsFalse((bool)bSawRetiredCurrentGeneration,
			TEXT("executable retirement and snapshot-current exchange must share one publication critical section")));
	}
};

#endif
