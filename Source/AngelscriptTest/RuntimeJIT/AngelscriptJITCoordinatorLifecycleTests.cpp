#include "AngelscriptEngine.h"
#include "AngelscriptTestEngineAcquisition.h"
#include "CQTest.h"
#include "RuntimeJIT/AngelscriptJITCoordinator.h"

#include "Misc/CommandLine.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptJITCoordinatorLifecycleTests,
	"Angelscript.TestModule.RuntimeJIT.Coordinator.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	class FScopedCommandLine final
	{
	public:
		explicit FScopedCommandLine(const TCHAR* Value)
			: Previous(FCommandLine::Get())
		{
			FCommandLine::Set(Value);
		}

		~FScopedCommandLine()
		{
			FCommandLine::Set(*Previous);
		}

	private:
		FString Previous;
	};

	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(EngineOwnsAndInstallsOneStableCoordinator)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		asIScriptEngine* const ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));

		FAngelscriptJITCoordinator* const First =
			Engine.GetJITCoordinator();
		FAngelscriptJITCoordinator* const Second =
			Engine.GetJITCoordinator();
		ASSERT_THAT(IsNotNull(First));
		ASSERT_THAT(IsTrue(First == Second));
		ASSERT_THAT(IsTrue(ScriptEngine->GetJITCompiler() == First));
		ASSERT_THAT(IsTrue(First->GetOwnerScriptEngine() == ScriptEngine));
	}

	TEST_METHOD(ExecutionModesSelectOnlyTheRequestedTier)
	{
		using namespace FAngelscriptJITConfiguration;
		ASSERT_THAT(IsTrue(SelectTier(
			EAngelscriptJITExecutionMode::Auto, true, true).Tier
			== EAngelscriptJITExecutionTier::StaticAOT));
		ASSERT_THAT(IsTrue(SelectTier(
			EAngelscriptJITExecutionMode::Auto, false, true).Tier
			== EAngelscriptJITExecutionTier::Runtime));
		ASSERT_THAT(IsTrue(SelectTier(
			EAngelscriptJITExecutionMode::VMOnly, true, true).Tier
			== EAngelscriptJITExecutionTier::VM));
		ASSERT_THAT(IsTrue(SelectTier(
			EAngelscriptJITExecutionMode::StaticAOTOnly, true, true).Tier
			== EAngelscriptJITExecutionTier::StaticAOT));
		ASSERT_THAT(IsTrue(SelectTier(
			EAngelscriptJITExecutionMode::RuntimeOnly, true, true).Tier
			== EAngelscriptJITExecutionTier::Runtime));
		ASSERT_THAT(IsFalse(SelectTier(
			static_cast<EAngelscriptJITExecutionMode>(255),
			true,
			true).bConfigurationValid));
	}

	TEST_METHOD(ProcessConfigurationParsesExactJitOptionsAndRejectsUnknowns)
	{
		{
			FScopedCommandLine CommandLine(
				TEXT("-as-jit-mode=runtime "
					"-as-runtime-jit-backend=angelsea-mir "
					"-as-runtime-jit-compile=eager-background"));
			const FAngelscriptEngineConfig Config =
				FAngelscriptEngineConfig::FromCurrentProcess();
			ASSERT_THAT(IsTrue(Config.JITExecutionMode ==
				EAngelscriptJITExecutionMode::RuntimeOnly));
			ASSERT_THAT(AreEqual(
				FString(TEXT("angelsea-mir")),
				Config.RuntimeJITBackend.ToString()));
			ASSERT_THAT(IsTrue(Config.RuntimeJITCompilePolicy ==
				EAngelscriptRuntimeJITCompilePolicy::EagerBackground));
			ASSERT_THAT(IsTrue(Config.bJITExecutionModeValid));
			ASSERT_THAT(IsTrue(Config.bRuntimeJITConfigurationValid));
		}

		{
			FScopedCommandLine CommandLine(
				TEXT("-as-jit-mode=surprise "
					"-as-runtime-jit-compile=eventually"));
			const FAngelscriptEngineConfig Config =
				FAngelscriptEngineConfig::FromCurrentProcess();
			ASSERT_THAT(IsFalse(Config.bJITExecutionModeValid));
			ASSERT_THAT(IsFalse(Config.bRuntimeJITConfigurationValid));
			ASSERT_THAT(IsFalse(Config.JITConfigurationError.IsEmpty()));
		}

		{
			FScopedCommandLine CommandLine(
				TEXT("-as-jit-mode=auto "
					"-as-runtime-jit-compile=eventually"));
			const FAngelscriptEngineConfig Config =
				FAngelscriptEngineConfig::FromCurrentProcess();
			ASSERT_THAT(IsTrue(Config.bJITExecutionModeValid));
			ASSERT_THAT(IsFalse(Config.bRuntimeJITConfigurationValid));
			ASSERT_THAT(IsTrue(Config.JITExecutionMode ==
				EAngelscriptJITExecutionMode::Auto));
		}

		{
			FScopedCommandLine CommandLine(
				TEXT("-as-jit-mode=auto "
					"-as-runtime-jit-backend=not/a/backend"));
			const FAngelscriptEngineConfig Config =
				FAngelscriptEngineConfig::FromCurrentProcess();
			ASSERT_THAT(IsTrue(Config.bJITExecutionModeValid));
			ASSERT_THAT(IsFalse(Config.bRuntimeJITConfigurationValid));
			ASSERT_THAT(IsTrue(Config.JITExecutionMode ==
				EAngelscriptJITExecutionMode::Auto));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT
