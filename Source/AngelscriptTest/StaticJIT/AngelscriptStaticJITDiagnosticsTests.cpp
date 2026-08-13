#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AngelscriptJITBindingContext.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/StaticJITDiagnostics.h"

#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

#if AS_WITH_STATIC_JIT_DIAGNOSTICS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotDiagnosticTests,
	"Angelscript.TestModule.StaticJIT.AOT.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static TUniquePtr<AngelscriptStaticJITAotGeneration::
		FStaticJITAotFixtureEngineSession> SharedFixtureSession;

	struct FScopedClearedCurrentEngineStack
	{
		FScopedClearedCurrentEngineStack()
			: SavedStack(FAngelscriptEngineContextStack::SnapshotAndClear())
		{
		}

		~FScopedClearedCurrentEngineStack()
		{
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		}

	private:
		TArray<FAngelscriptEngine*> SavedStack;
	};

	static FAngelscriptEngine* GetSharedAotFixtureForDiagnosticsTest(
			FAutomationTestBase& Test)
	{
		if (!Test.TestNotNull(
				TEXT("StaticJIT diagnostics section should own one shared isolated source fixture session"),
				SharedFixtureSession.Get()))
		{
			return nullptr;
		}
		if (!Test.TestTrue(
				TEXT("StaticJIT diagnostics shared source fixture should publish current Provider routes"),
				SharedFixtureSession->IsValid()))
		{
			Test.AddError(SharedFixtureSession->Error);
			return nullptr;
		}
		return SharedFixtureSession->Engine.Get();
	}

	static IConsoleCommand* FindDumpDiagnosticsCommand()
	{
		return static_cast<IConsoleCommand*>(IConsoleManager::Get().FindConsoleObject(TEXT("as.StaticJIT.DumpDiagnostics")));
	}

	static bool ExecuteDumpDiagnosticsCommand(FAutomationTestBase& Test, const TArray<FString>& Args, const TCHAR* Context)
	{
		IConsoleCommand* Command = FindDumpDiagnosticsCommand();
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should find the StaticJIT diagnostics dump command"), Context),
				Command))
		{
			return false;
		}

		FOutputDeviceNull OutputDevice;
		return Test.TestTrue(
			*FString::Printf(TEXT("%s should execute the StaticJIT diagnostics dump command"), Context),
			Command->Execute(Args, nullptr, OutputDevice));
	}

	static bool RunDiagnosticsResolveFixtureState(FAutomationTestBase& Test);
	static bool RunDiagnosticsSchemaCompleteness(FAutomationTestBase& Test);
	static bool RunDiagnosticsMultipleProviderConflictSerialization(
		FAutomationTestBase& Test);
	static bool RunDiagnosticsConsoleCommandRegistered(FAutomationTestBase& Test);
	static bool RunDiagnosticsConsoleCommandJsonOutput(FAutomationTestBase& Test);

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

	TEST_METHOD(ResolveFixtureState)
	{
		ASSERT_THAT(IsTrue(RunDiagnosticsResolveFixtureState(*TestRunner)));
	}

	TEST_METHOD(ConsoleCommandRegistered)
	{
		ASSERT_THAT(IsTrue(RunDiagnosticsConsoleCommandRegistered(*TestRunner)));
	}

	TEST_METHOD(SchemaCompleteness)
	{
		ASSERT_THAT(IsTrue(RunDiagnosticsSchemaCompleteness(*TestRunner)));
	}

	TEST_METHOD(MultipleProviderConflictSerialization)
	{
		ASSERT_THAT(IsTrue(
			RunDiagnosticsMultipleProviderConflictSerialization(*TestRunner)));
	}

	TEST_METHOD(ConsoleCommandJsonOutput)
	{
		ASSERT_THAT(IsTrue(RunDiagnosticsConsoleCommandJsonOutput(*TestRunner)));
	}
};

bool FAngelscriptStaticJITAotDiagnosticTests::RunDiagnosticsResolveFixtureState(FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr =
		GetSharedAotFixtureForDiagnosticsTest(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);

	asCScriptFunction* EntryFunction = nullptr;
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(AngelscriptStaticJITAotFixture::GetModuleName().ToString());
		if (ModuleDesc.IsValid() && ModuleDesc->ScriptModule != nullptr)
		{
			FTCHARToUTF8 EntryDecl(*AngelscriptStaticJITAotFixture::GetEntryDeclaration());
			EntryFunction = static_cast<asCScriptFunction*>(ModuleDesc->ScriptModule->GetFunctionByDecl(EntryDecl.Get()));
		}
	}

	if (!Test.TestNotNull(TEXT("StaticJIT diagnostics red test should resolve the fixture entry function"), EntryFunction))
	{
		return false;
	}

	FAngelscriptStableFunctionKey FunctionKey;
	if (!Test.TestTrue(
			TEXT("StaticJIT diagnostics should resolve the fixture stable function key"),
			Engine.TryResolveStableFunctionKey(EntryFunction, FunctionKey)))
	{
		return false;
	}

	Test.TestTrue(TEXT("StaticJIT diagnostics should see jitFunction attachment"), FStaticJITDiagnostics::HasJitFunction(EntryFunction));
	const asSJITFunctionBinding Binding = EntryFunction->GetJITBinding();
	Test.TestNotNull(
		TEXT("StaticJIT diagnostics should see current binding context"),
		FAngelscriptJITBindingContext::FromBindingUserData(Binding.UserData));
	FAngelscriptFunctionRoute Route;
	Test.TestTrue(
		TEXT("StaticJIT diagnostics should resolve the current generated route"),
		Engine.ResolveFunctionRoute(FunctionKey, Route) && Route.IsNative());

	const FStaticJITDiagnostics::FSnapshot Snapshot = FStaticJITDiagnostics::CaptureSnapshot(&Engine);
	Test.TestTrue(TEXT("StaticJIT diagnostics snapshot should report a current engine"), Snapshot.bHasCurrentEngine);
	Test.TestTrue(TEXT("StaticJIT diagnostics snapshot should report a script engine"), Snapshot.bHasScriptEngine);
	Test.TestTrue(TEXT("StaticJIT diagnostics should enumerate copied Provider catalogs"), Snapshot.Providers.Num() > 0);
	Test.TestTrue(TEXT("StaticJIT diagnostics should enumerate current Engine routes"), Snapshot.Routes.Num() > 0);
	Test.TestTrue(TEXT("StaticJIT diagnostics should report the selected target profile"), Snapshot.TargetProfile != EAngelscriptJITTargetProfile::Invalid);
	const FStaticJITDiagnostics::FJsonResult FirstJson =
		FStaticJITDiagnostics::CaptureJson(&Engine);
	const FStaticJITDiagnostics::FJsonResult SecondJson =
		FStaticJITDiagnostics::CaptureJson(&Engine);
	Test.TestTrue(TEXT("StaticJIT diagnostics should emit machine-readable JSON"), FirstJson.IsSuccess());
	Test.TestEqual(TEXT("StaticJIT diagnostics JSON should be deterministic for unchanged state"), FirstJson.Json, SecondJson.Json);
	Test.TestTrue(TEXT("StaticJIT diagnostics JSON should expose schema revision"), FirstJson.Json.Contains(TEXT("\"schemaVersion\"")));
	Test.TestFalse(TEXT("StaticJIT diagnostics JSON should not retain the legacy global database"), FirstJson.Json.Contains(TEXT("FJITDatabase")));
	Test.TestFalse(TEXT("StaticJIT diagnostics JSON should not retain the legacy whole-cache GUID"), FirstJson.Json.Contains(TEXT("PrecompiledDataGuid")));
	return true;
}

bool FAngelscriptStaticJITAotDiagnosticTests::RunDiagnosticsSchemaCompleteness(
	FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr =
		GetSharedAotFixtureForDiagnosticsTest(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}

	FAngelscriptEngineScope EngineScope(*EnginePtr);
	const FStaticJITDiagnostics::FJsonResult Result =
		FStaticJITDiagnostics::CaptureJson(EnginePtr);
	if (!Test.TestTrue(
		TEXT("StaticJIT complete diagnostics schema should serialize"),
		Result.IsSuccess()))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= Test.TestTrue(
		TEXT("StaticJIT diagnostics should publish schema revision 2"),
		Result.Json.Contains(TEXT("\"schemaVersion\":2")));
	bPassed &= Test.TestTrue(
		TEXT("StaticJIT diagnostics should publish the Provider ABI revision"),
		Result.Json.Contains(TEXT("\"providerAbiRevision\":")));
	bPassed &= Test.TestTrue(
		TEXT("StaticJIT diagnostics should group generated ownership by AS module"),
		Result.Json.Contains(TEXT("\"generatedModuleSource\":\"Unmapped/Module."))
		&& Result.Json.Contains(TEXT("\"functionCount\":"))
		&& Result.Json.Contains(TEXT("\"moduleArtifactDigest\":")));
	bPassed &= Test.TestTrue(
		TEXT("StaticJIT diagnostics should include exact Provider candidates for current routes"),
		Result.Json.Contains(TEXT("\"candidates\":[{"))
		&& Result.Json.Contains(TEXT("\"matchResult\":\"Exact\"")));
	bPassed &= Test.TestTrue(
		TEXT("StaticJIT diagnostics should expose resolved stable reference slots"),
		Result.Json.Contains(TEXT("\"resolved\":true")));
	bPassed &= Test.TestTrue(
		TEXT("StaticJIT diagnostics should keep process addresses out of JSON"),
		!Result.Json.Contains(TEXT("0x"), ESearchCase::IgnoreCase));
	return bPassed;
}

bool FAngelscriptStaticJITAotDiagnosticTests::
RunDiagnosticsMultipleProviderConflictSerialization(FAutomationTestBase& Test)
{
	const auto MakeHash = [](const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("static-jit-diagnostics-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	};
	const auto MakeProvider = [&MakeHash](
		const FStringView ProviderSeed,
		const FStringView ModuleSeed,
		const FStringView FunctionSeed)
	{
		FStaticJITDiagnostics::FProviderRecord Provider;
		Provider.ProviderId.Hash = MakeHash(ProviderSeed);
		Provider.ProviderGeneration.Hash = MakeHash(
			FString::Printf(TEXT("%.*s.Generation"),
				ProviderSeed.Len(), ProviderSeed.GetData()));
		Provider.ArtifactSetDigest = MakeHash(TEXT("ArtifactSet"));
		Provider.ArtifactProfile.Hash = MakeHash(TEXT("Profile"));
		Provider.NativeEnvironmentFingerprint = MakeHash(TEXT("Environment"));
		Provider.ProviderName = FString(ProviderSeed);
		Provider.OwnerModuleName = FString::Printf(
			TEXT("%.*sModule"), ProviderSeed.Len(), ProviderSeed.GetData());
		Provider.CompatibleEntryCount = 1;
		Provider.ConflictingEntryCount = 1;

		FStaticJITDiagnostics::FProviderModuleRecord& Module =
			Provider.Modules.AddDefaulted_GetRef();
		Module.ModuleKey.Hash = MakeHash(ModuleSeed);
		Module.GeneratedModuleSource =
			FAngelscriptJITGeneration::BuildModuleSourceRelativePath(
				Module.ModuleKey,
				EAngelscriptJITTargetProfile::EditorDevelopment);
		Module.ModuleArtifactDigest = MakeHash(
			FString::Printf(TEXT("%.*s.Artifact"),
				ModuleSeed.Len(), ModuleSeed.GetData()));
		Module.FunctionCount = 1;

		FStaticJITDiagnostics::FProviderEntryRecord& Entry =
			Provider.Entries.AddDefaulted_GetRef();
		Entry.ModuleKey = Module.ModuleKey;
		Entry.FunctionKey.Hash = MakeHash(FunctionSeed);
		Entry.ExecutionHash = MakeHash(TEXT("Execution"));
		Entry.DebugHash = MakeHash(TEXT("Debug"));
		Entry.ArtifactProfile = Provider.ArtifactProfile;
		Entry.EntryAbiHash = MakeHash(TEXT("EntryAbi"));
		Entry.NativeEnvironmentFingerprint =
			Provider.NativeEnvironmentFingerprint;
		Entry.bHasVMEntry = true;
		Entry.bHasRawEntry = true;
		Entry.bHasParmsEntry = true;
		Entry.GeneratedModuleSource = Module.GeneratedModuleSource;
		return Provider;
	};

	FStaticJITDiagnostics::FSnapshot Snapshot;
	Snapshot.bHasCurrentEngine = true;
	Snapshot.bHasScriptEngine = true;
	Snapshot.TargetProfile =
		EAngelscriptJITTargetProfile::EditorDevelopment;
	Snapshot.ProviderPublicationOrdinal = 19;
	Snapshot.RouteGeneration = 23;
	Snapshot.VmRouteCount = 1;
	Snapshot.Providers.Add(MakeProvider(
		TEXT("ProviderB"), TEXT("SharedModule"), TEXT("SharedFunction")));
	Snapshot.Providers.Add(MakeProvider(
		TEXT("ProviderA"), TEXT("SharedModule"), TEXT("SharedFunction")));

	FStaticJITDiagnostics::FRouteRecord& Route =
		Snapshot.Routes.AddDefaulted_GetRef();
	Route.ModuleKey.Hash = MakeHash(TEXT("SharedModule"));
	Route.Identity.FunctionKey.Hash = MakeHash(TEXT("SharedFunction"));
	Route.Identity.Content.Execution = MakeHash(TEXT("Execution"));
	Route.Identity.Content.Debug = MakeHash(TEXT("Debug"));
	Route.Identity.Profile.Hash = MakeHash(TEXT("Profile"));
	Route.CanonicalDeclaration = TEXT("void SharedFunction()");
	Route.NumericFunctionId = 99;
	Route.MatchResult =
		EAngelscriptArtifactMatchResult::AmbiguousExactProvider;
	Route.bHasVerifiedArtifactIdentity = true;
	for (const FStaticJITDiagnostics::FProviderRecord& Provider
		: Snapshot.Providers)
	{
		FStaticJITDiagnostics::FRouteCandidateRecord& Candidate =
			Route.Candidates.AddDefaulted_GetRef();
		Candidate.ProviderId = Provider.ProviderId;
		Candidate.ProviderGeneration = Provider.ProviderGeneration;
		Candidate.ProviderEntryIndex = 0;
		Candidate.MatchResult = EAngelscriptArtifactMatchResult::Exact;
	}

	const FStaticJITDiagnostics::FJsonResult First =
		FStaticJITDiagnostics::Serialize(Snapshot);
	const FStaticJITDiagnostics::FJsonResult Second =
		FStaticJITDiagnostics::Serialize(Snapshot);
	bool bPassed = true;
	bPassed &= Test.TestTrue(
		TEXT("Synthetic multi-Provider conflict diagnostics should serialize"),
		First.IsSuccess());
	bPassed &= Test.TestEqual(
		TEXT("Synthetic multi-Provider conflict diagnostics should be deterministic"),
		First.Json,
		Second.Json);
	const FString ExactCandidateMarker = TEXT("\"matchResult\":\"Exact\"");
	const FString WithoutExactCandidates = First.Json.Replace(
		*ExactCandidateMarker, TEXT(""));
	const int32 ExactCandidateCount =
		(First.Json.Len() - WithoutExactCandidates.Len())
		/ ExactCandidateMarker.Len();
	bPassed &= Test.TestEqual(
		TEXT("Synthetic multi-Provider diagnostics should include both exact candidates"),
		ExactCandidateCount,
		2);
	bPassed &= Test.TestTrue(
		TEXT("Synthetic multi-Provider diagnostics should explain VM ambiguity"),
		First.Json.Contains(TEXT("\"selectedRoute\":\"Vm\""))
		&& First.Json.Contains(
			TEXT("\"matchResult\":\"AmbiguousExactProvider\"")));
	const FString ProviderAHex = MakeHash(TEXT("ProviderA")).ToHexString();
	const FString ProviderBHex = MakeHash(TEXT("ProviderB")).ToHexString();
	const bool bProviderOrderIsStable = ProviderAHex < ProviderBHex
		? First.Json.Find(ProviderAHex) < First.Json.Find(ProviderBHex)
		: First.Json.Find(ProviderBHex) < First.Json.Find(ProviderAHex);
	bPassed &= Test.TestTrue(
		TEXT("Synthetic multi-Provider diagnostics should sort ProviderIds independent of insertion order"),
		bProviderOrderIsStable);
	return bPassed;
}

bool FAngelscriptStaticJITAotDiagnosticTests::RunDiagnosticsConsoleCommandRegistered(FAutomationTestBase& Test)
{
	FScopedAngelscriptEngineResolutionSuppressionForTesting NoCurrentEngineScope;
	FScopedClearedCurrentEngineStack ClearedStackScope;
	const FStaticJITDiagnostics::FSnapshot Snapshot = FStaticJITDiagnostics::CaptureSnapshot();
	Test.TestFalse(TEXT("StaticJIT diagnostics command test should run with no current engine"), Snapshot.bHasCurrentEngine);
	FStaticJITDiagnostics::FSnapshot EmptySnapshot;
	const FStaticJITDiagnostics::FJsonResult EmptyJson =
		FStaticJITDiagnostics::Serialize(EmptySnapshot);
	Test.TestTrue(
		TEXT("StaticJIT diagnostics should serialize a safe no-engine/no-provider snapshot"),
		EmptyJson.IsSuccess());
	Test.TestTrue(
		TEXT("StaticJIT diagnostics empty snapshot should expose an empty Provider catalog"),
		EmptyJson.Json.Contains(TEXT("\"providers\":[]")));
	Test.TestTrue(
		TEXT("StaticJIT diagnostics empty snapshot should expose an empty Engine route set"),
		EmptyJson.Json.Contains(TEXT("\"routes\":[]")));

	return ExecuteDumpDiagnosticsCommand(
		Test, {}, TEXT("StaticJIT diagnostics JSON dump without current engine"));
}

bool FAngelscriptStaticJITAotDiagnosticTests::RunDiagnosticsConsoleCommandJsonOutput(FAutomationTestBase& Test)
{
	FAngelscriptEngine* const EnginePtr =
		GetSharedAotFixtureForDiagnosticsTest(Test);
	if (EnginePtr == nullptr)
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	FAngelscriptEngineScope EngineScope(Engine);
	const FString OutputFilename = FPaths::CreateTempFilename(
		*FPaths::ProjectIntermediateDir(),
		TEXT("StaticJITDiagnostics-"),
		TEXT(".json"));
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*OutputFilename, false, true);
	};

	bool bPassed = true;
	bPassed &= ExecuteDumpDiagnosticsCommand(Test, {}, TEXT("StaticJIT diagnostics dump with current engine"));
	bPassed &= ExecuteDumpDiagnosticsCommand(
		Test,
		{ OutputFilename },
		TEXT("StaticJIT diagnostics dump to explicit JSON file"));
	FString WrittenJson;
	bPassed &= Test.TestTrue(
		TEXT("StaticJIT diagnostics command should write the requested output file"),
		FFileHelper::LoadFileToString(WrittenJson, *OutputFilename));
	bPassed &= Test.TestTrue(
		TEXT("StaticJIT diagnostics command output should contain Provider catalogs"),
		WrittenJson.Contains(TEXT("\"providers\"")));
	bPassed &= Test.TestTrue(
		TEXT("StaticJIT diagnostics command output should contain Engine routes"),
		WrittenJson.Contains(TEXT("\"routes\"")));

	const FStaticJITDiagnostics::FSnapshot Snapshot =
		FStaticJITDiagnostics::CaptureSnapshot(&Engine);
	if (!Test.TestTrue(
		TEXT("StaticJIT diagnostics query fixture should expose at least one route"),
		!Snapshot.Routes.IsEmpty()))
	{
		return false;
	}
	const FString QueryOutputFilename = FPaths::CreateTempFilename(
		*FPaths::ProjectIntermediateDir(),
		TEXT("StaticJITDiagnostics-Function-"),
		TEXT(".json"));
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*QueryOutputFilename, false, true);
	};
	const FString FunctionKey =
		Snapshot.Routes[0].Identity.FunctionKey.Hash.ToHexString();
	bPassed &= ExecuteDumpDiagnosticsCommand(
		Test,
		{
			FString::Printf(TEXT("-Function=%s"), *FunctionKey),
			FString::Printf(TEXT("-Output=%s"), *QueryOutputFilename),
		},
		TEXT("StaticJIT diagnostics dump for one stable function key"));
	FString QueryJson;
	bPassed &= Test.TestTrue(
		TEXT("StaticJIT diagnostics function query should write JSON"),
		FFileHelper::LoadFileToString(QueryJson, *QueryOutputFilename));
	bPassed &= Test.TestTrue(
		TEXT("StaticJIT diagnostics function query should retain the requested stable key"),
		QueryJson.Contains(FunctionKey));
	bPassed &= Test.TestTrue(
		TEXT("StaticJIT diagnostics function query should exclude unrelated routes"),
		QueryJson.Contains(TEXT("\"queryMatched\":true")));
	return bPassed;
}

#endif

#endif // WITH_ANGELSCRIPT_UNITTESTS
