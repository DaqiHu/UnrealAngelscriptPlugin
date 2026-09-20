#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptEngineExtensionRegistry.h"
#include "Core/AngelscriptRuntimeModule.h"
#include "Core/AngelscriptSubsystem.h"
#include "Features/IModularFeatures.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "StaticJIT/AngelscriptJITProvider.h"

#ifndef WITH_ANGELSCRIPT_UNITTESTS
#error WITH_ANGELSCRIPT_UNITTESTS must be defined by AngelscriptRuntime.Build.cs.
#endif

#ifndef WITH_ANGELSCRIPT_TESTS
#error WITH_ANGELSCRIPT_TESTS must be defined by AngelscriptRuntime.Build.cs.
#endif

static_assert(WITH_ANGELSCRIPT_UNITTESTS == 0, "The checked-in baseline must exclude legacy AngelScript tests.");
static_assert(WITH_ANGELSCRIPT_TESTS == 1, "The checked-in baseline must include replacement AngelScript tests.");

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags BaselineFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	bool HasRegisteredTestWithPrefix(const TCHAR* Prefix)
	{
		TArray<FAutomationTestInfo> Tests;
		FAutomationTestFramework::Get().GetValidTestNames(Tests);
		return Tests.ContainsByPredicate(
			[Prefix](const FAutomationTestInfo& Test)
			{
				return Test.GetFullTestPath().StartsWith(Prefix);
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptRuntimeDormantByDefaultTest,
	"Angelscript.UnitTest.Baseline.RuntimeDormantByDefault",
	BaselineFlags)

bool FAngelscriptRuntimeDormantByDefaultTest::RunTest(const FString& Parameters)
{
	TestFalse(
		TEXT("The reconstruction baseline hard-disables legacy runtime activation"),
		FAngelscriptRuntimeModule::IsLegacyRuntimeEnabled());

	UAngelscriptSubsystem* Subsystem = UAngelscriptSubsystem::Get();
	TestNotNull(TEXT("The AngelScript engine subsystem shell remains available"), Subsystem);
	if (Subsystem != nullptr)
	{
		TestNull(TEXT("The dormant subsystem does not own or adopt an engine"), Subsystem->GetEngine());
		TestFalse(TEXT("The dormant subsystem is not allowed to tick"), Subsystem->IsAllowedToTick());
		Subsystem->EnsurePrimaryEngineInitialized();
		TestNull(TEXT("Explicit subsystem initialization cannot bypass the dormant setting"), Subsystem->GetEngine());
	}

	TestNull(
		TEXT("Default startup does not publish an ambient AngelScript engine"),
		FAngelscriptEngine::TryGetCurrentEngine());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptOptionalIntegrationsDormantByDefaultTest,
	"Angelscript.UnitTest.Baseline.OptionalIntegrationsDormantByDefault",
	BaselineFlags)

bool FAngelscriptOptionalIntegrationsDormantByDefaultTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("No legacy JIT artifact provider is registered"),
		IModularFeatures::Get().GetModularFeatureImplementationCount(
			IAngelscriptJITArtifactProvider::FeatureName()),
		0);
	TestEqual(
		TEXT("No optional legacy engine extension is registered"),
		FAngelscriptEngineExtensionRegistry::Get().NumExtensions(),
		0);
	TestNull(
		TEXT("The legacy editor StaticJIT refresh command is not registered"),
		IConsoleManager::Get().FindConsoleObject(TEXT("as.StaticJIT.EditorRefresh")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLegacySuiteExcludedByDefaultTest,
	"Angelscript.UnitTest.Baseline.LegacySuiteExcludedByDefault",
	BaselineFlags)

bool FAngelscriptLegacySuiteExcludedByDefaultTest::RunTest(const FString& Parameters)
{
	TestFalse(
		TEXT("The legacy Angelscript.TestModule namespace is not registered"),
		HasRegisteredTestWithPrefix(TEXT("Angelscript.TestModule")));
	TestFalse(
		TEXT("The legacy Angelscript.CppTests namespace is not registered"),
		HasRegisteredTestWithPrefix(TEXT("Angelscript.CppTests")));
	TestFalse(
		TEXT("The legacy Angelscript.Editor namespace is not registered"),
		HasRegisteredTestWithPrefix(TEXT("Angelscript.Editor")));
	return true;
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
