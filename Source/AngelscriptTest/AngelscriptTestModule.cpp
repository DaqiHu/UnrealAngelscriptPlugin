#include "Core/AngelscriptTestModule.h"

#if WITH_ANGELSCRIPT_TESTS
#include "Framework/Catalog/AngelscriptTestCode.h"
#include "CoreGlobals.h"
#include "Misc/CoreDelegates.h"
#endif

#if WITH_ANGELSCRIPT_UNITTESTS
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"
#include "AngelscriptTestEnginePool.h"
#endif

#include "Logging/LogMacros.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

IMPLEMENT_MODULE(FAngelscriptTestModule, AngelscriptTest);

DEFINE_LOG_CATEGORY_STATIC(LogAngelscriptTest, Log, All);

#if WITH_ANGELSCRIPT_UNITTESTS
// Definition for the log category declared in AngelscriptPreprocessorTestHelpers.h.
// Default verbosity is NoLogging; enable on demand via -LogCmds or
// LogPreprocessorDump.SetVerbosity(...) inside a TEST_METHOD.
DEFINE_LOG_CATEGORY(LogPreprocessorDump);
#endif

void FAngelscriptTestModule::StartupModule()
{
#if WITH_ANGELSCRIPT_TESTS
	TestCodeActivationBarrier = MakeUnique<AngelscriptTest::FAngelscriptTestCodeActivationBarrier>(
		[this]() { ActivateTestCode(); });
	TestCodeActivationBarrier->Install(
		IsEngineStartupModuleLoadingComplete(),
		FCoreDelegates::OnAllModuleLoadingPhasesComplete);
#endif
#if WITH_ANGELSCRIPT_UNITTESTS
	const bool bPrewarmEngine = FParse::Param(FCommandLine::Get(), TEXT("AngelscriptTestPrewarmEngine"));
	StartupTestEnginePool(bPrewarmEngine);
#endif
	UE_LOG(LogAngelscriptTest, Log, TEXT("AngelscriptTest replacement module shell started."));
}

void FAngelscriptTestModule::ShutdownModule()
{
#if WITH_ANGELSCRIPT_TESTS
	if (TestCodeActivationBarrier.IsValid())
	{
		TestCodeActivationBarrier->Shutdown();
		TestCodeActivationBarrier.Reset();
	}
#endif
#if WITH_ANGELSCRIPT_UNITTESTS
	ShutdownTestEnginePool();
#endif
	UE_LOG(LogAngelscriptTest, Log, TEXT("AngelscriptTest replacement module shell shut down."));
}

#if WITH_ANGELSCRIPT_TESTS
void FAngelscriptTestModule::ActivateTestCode()
{
	const AngelscriptTest::FAngelscriptTestStatus Status =
		AngelscriptTest::FAngelscriptTestCode::GetInstance().ActivateRegistrations();
	for (const AngelscriptTest::FAngelscriptTestError& Error : Status.GetErrors())
	{
		UE_LOG(
			LogAngelscriptTest,
			Error,
			TEXT("Test code registration failed [%s] %s (%s:%d)"),
			*Error.Code,
			*Error.Message,
			*Error.SourcePath,
			Error.SourceLine);
	}
}
#endif
