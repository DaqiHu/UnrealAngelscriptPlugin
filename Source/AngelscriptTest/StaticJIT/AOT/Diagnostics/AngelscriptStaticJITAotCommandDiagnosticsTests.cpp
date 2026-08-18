#include "CQTest.h"

#include "StaticJIT/StaticJITDiagnostics.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotCommandDiagnosticTests,
	"Angelscript.TestModule.StaticJIT.AOT.Diagnostics.Command",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InstalledProviderGenerationDiagnosticsAppearInStableCommandJson)
	{
		const FStaticJITDiagnostics::FSnapshot Snapshot =
			FStaticJITDiagnostics::CaptureSnapshot();
		const FStaticJITDiagnostics::FProviderRecord* const TestJITProvider =
			Snapshot.Providers.FindByPredicate([](const auto& Provider)
			{
				return Provider.OwnerModuleName == TEXT("AngelscriptTestJIT");
			});
		ASSERT_THAT(IsNotNull(TestJITProvider));

		const FStaticJITDiagnostics::FJsonResult First =
			FStaticJITDiagnostics::Serialize(Snapshot);
		const FStaticJITDiagnostics::FJsonResult Second =
			FStaticJITDiagnostics::Serialize(Snapshot);
		ASSERT_THAT(IsTrue(First.IsSuccess(), First.Detail));
		ASSERT_THAT(AreEqual(First.Json, Second.Json));
		ASSERT_THAT(IsTrue(First.Json.Contains(TEXT("\"schemaVersion\":9"))));
		ASSERT_THAT(IsTrue(First.Json.Contains(
			TEXT("\"generationDiagnostics\":{\"schemaRevision\":1"))));
		ASSERT_THAT(IsTrue(First.Json.Contains(
			TEXT("\"requestKind\":\"StaticJITArtifact\""))));
		ASSERT_THAT(IsTrue(First.Json.Contains(
			TEXT("\"requestedBackend\":\"typed-ast\""))));
		ASSERT_THAT(IsTrue(First.Json.Contains(
			TEXT("\"captureState\":\"VerifiedTypedHIR\""))));
		ASSERT_THAT(IsTrue(First.Json.Contains(TEXT("\"backendAttempts\""))));
		ASSERT_THAT(IsTrue(First.Json.Contains(TEXT("\"callClosure\""))));
		ASSERT_THAT(IsTrue(First.Json.Contains(TEXT("\"mapping\""))));

		const FString OutputFilename = FPaths::CreateTempFilename(
			*FPaths::ProjectIntermediateDir(),
			TEXT("StaticJITAotCommandDiagnostics-"),
			TEXT(".json"));
		ON_SCOPE_EXIT
		{
			IFileManager::Get().Delete(*OutputFilename, false, true);
		};

		IConsoleCommand* const Command = static_cast<IConsoleCommand*>(
			IConsoleManager::Get().FindConsoleObject(
				TEXT("as.StaticJIT.DumpDiagnostics")));
		ASSERT_THAT(IsNotNull(Command));
		FOutputDeviceNull OutputDevice;
		ASSERT_THAT(IsTrue(Command->Execute(
			{FString::Printf(TEXT("-Output=%s"), *OutputFilename)},
			nullptr,
			OutputDevice)));

		FString CommandJson;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			CommandJson, *OutputFilename)));
		ASSERT_THAT(IsTrue(CommandJson.Contains(TEXT("\"generationDiagnostics\""))));
		ASSERT_THAT(IsTrue(CommandJson.Contains(
			TEXT("\"requestedBackend\":\"typed-ast\""))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS
