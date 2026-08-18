#include "CQTest.h"

#include "AngelscriptEngine.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptStaticJITBytecodeAccessAudit.h"
#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITModel.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptTypedASTJITScriptFunctionCorpusTests_Private
{
	static bool ExecuteNamedInt(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const TCHAR* Declaration,
		const int32 Expected)
	{
		return ExpectGlobalInt(
			Test,
			Engine,
			Module,
			Declaration,
			Declaration,
			Expected);
	}

#if AS_CAN_GENERATE_JIT
	struct FCorpusGenerationSession
	{
		FString Root;
		TUniquePtr<FAngelscriptEngine> Engine;
	};

	static TUniquePtr<FCorpusGenerationSession> CreateCorpusGenerationSession(
		FAutomationTestBase& Test,
		const TCHAR* FolderName,
		const TCHAR* FileStem,
		const FString& ScriptSource)
	{
		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		FString ProfileError;
		if (!Test.TestTrue(
			TEXT("Script corpus generation should resolve the typed-ast profile"),
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::TypedAST(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				GenerationProfile,
				&ProfileError)))
		{
			Test.AddError(ProfileError);
			return nullptr;
		}

		TUniquePtr<FCorpusGenerationSession> Session =
			MakeUnique<FCorpusGenerationSession>();
		Session->Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/TypedASTJITScriptCorpus"),
			FolderName,
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		FPaths::NormalizeDirectoryName(Session->Root);
		const FString ScriptRoot = FPaths::Combine(Session->Root, TEXT("Script"));
		const FString ScriptPath = FPaths::Combine(
			ScriptRoot, FString(FileStem) + TEXT(".as"));
		if (!Test.TestTrue(
				TEXT("Script corpus generation should create a Script root"),
				IFileManager::Get().MakeDirectory(*ScriptRoot, true))
			|| !Test.TestTrue(
				TEXT("Script corpus generation should write adapted source"),
				FFileHelper::SaveStringToFile(ScriptSource, *ScriptPath)))
		{
			return nullptr;
		}

		FAngelscriptEngineConfig Config;
		Config.Purpose = EAngelscriptEnginePurpose::StaticJITGeneration;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.bRunningCommandlet = true;
		Config.bIsUnattended = true;
		Config.bDisableCacheV2Persistence = true;
		Config.bForceSerialCacheV2Preparation = true;
		Config.CacheV2RootOverride = FPaths::Combine(
			Session->Root, TEXT("CacheV2"));
		GenerationProfile.ApplyToEngineConfig(Config);

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		const FString ProjectRoot = Session->Root;
		Dependencies.GetProjectDir = [ProjectRoot]()
		{
			return ProjectRoot;
		};
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		Dependencies.HandleStartupCompileFailureExit =
			[](const FAngelscriptStartupCompileFailureExitRequest&)
			{
			};

		Session->Engine = FAngelscriptEngine::Create(Config, Dependencies);
		if (!Test.TestNotNull(
				TEXT("Script corpus generation should create an Engine"),
				Session->Engine.Get())
			|| !Test.TestTrue(
				TEXT("Script corpus generation source should compile"),
				Session->Engine->DidInitialCompileSucceed()))
		{
			return nullptr;
		}
		return Session;
	}

	static FAngelscriptJITProviderGenerationSettings MakeSettings(
		const FAngelscriptStaticJITGenerationSnapshot& Snapshot)
	{
		FAngelscriptJITProviderGenerationSettings Settings;
		Settings.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		FAngelscriptArtifactCanonicalWriter ProviderWriter(
			TEXT("typed-ast-script-corpus-provider"));
		ProviderWriter.WriteString(TEXT("ScriptFunctionCorpus"));
		Settings.ProviderId.Hash = ProviderWriter.FinalizeHash();
		Settings.ProviderName = TEXT("TypedASTJIT Script Function Corpus");
		Settings.OwnerModuleName = TEXT("AngelscriptTest");
		Settings.KnownNativeCallModules = {
			FName(TEXT("Core")),
			FName(TEXT("CoreUObject")),
			FName(TEXT("AngelscriptRuntime")),
		};
		Settings.PublicNativeCallDependencyModules = {
			FName(TEXT("Core")),
			FName(TEXT("CoreUObject")),
			FName(TEXT("AngelscriptRuntime")),
		};
		Settings.ArtifactProfile = Snapshot.ArtifactProfile;
		Settings.NativeEnvironmentFingerprint =
			FAngelscriptJITTargetProfiles::
				BuildCurrentNativeEnvironmentFingerprint(Settings.TargetProfile);
		return Settings;
	}

	static bool GenerateCorpus(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		FAngelscriptStaticJITGeneratorOutput& OutOutput)
	{
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Engine.GetStaticJITGenerationSnapshot();
		if (!Test.TestTrue(
			TEXT("Script corpus generation must publish a complete snapshot"),
			Snapshot.IsValid() && Snapshot->IsComplete()))
		{
			return false;
		}

		TArray<asIScriptModule*> CompiledModules;
		CompiledModules.Reserve(Snapshot->Modules.Num());
		for (const FAngelscriptStaticJITGenerationModule& Module
			: Snapshot->Modules)
		{
			if (Module.EngineLocalModule != nullptr)
			{
				CompiledModules.Add(Module.EngineLocalModule);
			}
		}
		if (!Test.TestTrue(
			TEXT("Script corpus generation must have compiled modules"),
			CompiledModules.Num() > 0))
		{
			return false;
		}

		FAngelscriptStaticJITGenerationProfile RequestProfile;
		FString RequestProfileError;
		if (!Test.TestTrue(
			TEXT("Script corpus generation should rebuild the typed-ast profile"),
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::TypedAST(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				RequestProfile,
				&RequestProfileError)))
		{
			Test.AddError(RequestProfileError);
			return false;
		}

		FAngelscriptStaticJITProviderGenerationRequest Request;
		Request.BackendId = FAngelscriptStaticJITBackendId::TypedAST();
		Request.Settings = MakeSettings(*Snapshot);
		Request.TypedASTJITCapabilityProfile =
			RequestProfile.TypedASTJITCapabilityProfile;
		Request.TypedASTJITExecutionRequirements =
			RequestProfile.TypedASTJITExecutionRequirements;
		Request.CompiledSourceGraph = CompiledModules;
		Request.EmitModuleSet = CompiledModules;
		Request.GenerationSnapshot = Snapshot;

		FAngelscriptStaticJITBytecodeAccessAudit::ResetForTesting();
		FString GenerationError;
		const bool bGenerated = GenerateStaticJITProviderArtifacts(
			Request, OutOutput, &GenerationError);
		if (!Test.TestTrue(GenerationError, bGenerated))
		{
			return false;
		}
		return true;
	}

	static const FAngelscriptStaticJITFunctionResult* FindNamedRootResult(
		FAngelscriptEngine& Engine,
		const FAngelscriptStaticJITGeneratorOutput& Output,
		const TCHAR* FunctionName)
	{
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Engine.GetStaticJITGenerationSnapshot();
		if (!Snapshot.IsValid())
		{
			return nullptr;
		}
		const FAngelscriptStaticJITGenerationFunction* const SnapshotFunction =
			Snapshot->Functions.FindByPredicate(
				[FunctionName](
					const FAngelscriptStaticJITGenerationFunction& Candidate)
				{
					return Candidate.CanonicalDeclaration.Contains(FunctionName);
				});
		if (SnapshotFunction == nullptr)
		{
			return nullptr;
		}
		return Output.Functions.FindByPredicate(
			[SnapshotFunction](
				const FAngelscriptStaticJITFunctionResult& Candidate)
			{
				return Candidate.FunctionKey == SnapshotFunction->FunctionKey;
			});
	}

	static EAngelscriptTypedASTJITFallbackReason ExtractFallbackReason(
		const FAngelscriptStaticJITFunctionResult& Result)
	{
		if (Result.TypedDiagnostic.IsSet())
		{
			return Result.TypedDiagnostic->FallbackReason;
		}
		for (const FAngelscriptStaticJITBackendAttemptDiagnostic& Attempt
			: Result.BackendAttempts)
		{
			if (Attempt.TypedDiagnostic.IsSet()
				&& Attempt.TypedDiagnostic->FallbackReason
					!= EAngelscriptTypedASTJITFallbackReason::None)
			{
				return Attempt.TypedDiagnostic->FallbackReason;
			}
		}
		return EAngelscriptTypedASTJITFallbackReason::None;
	}
#endif
}

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITScriptFunctionCorpusTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.ScriptCorpus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(AdaptedScriptScalarFixturesMatchVmResults)
	{
		using namespace AngelscriptTypedASTJITScriptFunctionCorpusTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int HandlesFixtureValue()
			{
				const int Value = 9;
				return Value;
			}

			int EnumsFixtureValue()
			{
				const int One = 1;
				return One;
			}

			int InheritanceFixtureValue()
			{
				const int BaseValue = 3;
				const int DerivedValue = BaseValue + 4;
				return DerivedValue;
			}

			int GameplayTagsFixtureValue()
			{
				return 1;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTypedASTJITScriptCorpus_ScalarVm"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(),
			TEXT("Adapted scalar Script/Tests fixtures should compile")));
		asIScriptModule& Module = ModuleScope.GetModule();
		ASSERT_THAT(IsTrue(ExecuteNamedInt(
			*TestRunner, Engine, Module, TEXT("int HandlesFixtureValue()"), 9)));
		ASSERT_THAT(IsTrue(ExecuteNamedInt(
			*TestRunner, Engine, Module, TEXT("int EnumsFixtureValue()"), 1)));
		ASSERT_THAT(IsTrue(ExecuteNamedInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int InheritanceFixtureValue()"),
			7)));
		ASSERT_THAT(IsTrue(ExecuteNamedInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int GameplayTagsFixtureValue()"),
			1)));
	}

	TEST_METHOD(AdaptedScriptClassFixturesMatchVmResults)
	{
		using namespace AngelscriptTypedASTJITScriptFunctionCorpusTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			class FPhase2ActorLifecycleProbe
			{
				int Step()
				{
					return 21;
				}
			}

			int ActorLifecycleFixtureValue()
			{
				FPhase2ActorLifecycleProbe Probe =
					FPhase2ActorLifecycleProbe();
				return Probe.Step();
			}

			class FPhase2SystemUtilsFixture
			{
				int Read()
				{
					return 13;
				}
			}

			int SystemUtilsFixtureValue()
			{
				FPhase2SystemUtilsFixture Fixture =
					FPhase2SystemUtilsFixture();
				return Fixture.Read();
			}

			class FPhase2ExampleActorFixture
			{
				int Value = 42;
			}

			int ExampleActorFixtureValue()
			{
				FPhase2ExampleActorFixture Fixture =
					FPhase2ExampleActorFixture();
				return Fixture.Value;
			}

			class FPhase2MathFixture
			{
				int Value = 10;
			}

			int MathNamespaceFixtureValue()
			{
				FPhase2MathFixture Fixture = FPhase2MathFixture();
				return Fixture.Value;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTypedASTJITScriptCorpus_ClassVm"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(),
			TEXT("Adapted class Script/Tests fixtures should compile")));
		asIScriptModule& Module = ModuleScope.GetModule();
		ASSERT_THAT(IsTrue(ExecuteNamedInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int ActorLifecycleFixtureValue()"),
			21)));
		ASSERT_THAT(IsTrue(ExecuteNamedInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int SystemUtilsFixtureValue()"),
			13)));
		ASSERT_THAT(IsTrue(ExecuteNamedInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int ExampleActorFixtureValue()"),
			42)));
		ASSERT_THAT(IsTrue(ExecuteNamedInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int MathNamespaceFixtureValue()"),
			10)));
	}

#if AS_CAN_GENERATE_JIT
	TEST_METHOD(AdaptedScriptScalarUFunctionsSelectTypedBackend)
	{
		using namespace AngelscriptTypedASTJITScriptFunctionCorpusTests_Private;
		const FString ScriptSource = ASTEST_AS(R"AS(
			UFUNCTION()
			int HandlesFixtureValue()
			{
				const int Value = 9;
				return Value;
			}

			UFUNCTION()
			int EnumsFixtureValue()
			{
				const int One = 1;
				return One;
			}

			UFUNCTION()
			int InheritanceFixtureValue()
			{
				const int BaseValue = 3;
				const int DerivedValue = BaseValue + 4;
				return DerivedValue;
			}

			UFUNCTION()
			int GameplayTagsFixtureValue()
			{
				return 1;
			}
			)AS");

		TUniquePtr<FCorpusGenerationSession> Session =
			CreateCorpusGenerationSession(
				*TestRunner,
				TEXT("ScalarTyped"),
				TEXT("TypedASTJITScriptCorpusScalar"),
				ScriptSource);
		ASSERT_THAT(IsNotNull(Session.Get()));
		if (!Session.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Session->Engine);

		FAngelscriptStaticJITGeneratorOutput Output;
		ASSERT_THAT(IsTrue(GenerateCorpus(*TestRunner, *Session->Engine, Output)));
		const FAngelscriptStaticJITBytecodeAccessObservation Observation =
			FAngelscriptStaticJITBytecodeAccessAudit::GetObservationForTesting();
		ON_SCOPE_EXIT
		{
			FAngelscriptStaticJITBytecodeAccessAudit::ResetForTesting();
		};

		const TCHAR* const RequiredNames[] = {
			TEXT("HandlesFixtureValue"),
			TEXT("EnumsFixtureValue"),
			TEXT("InheritanceFixtureValue"),
			TEXT("GameplayTagsFixtureValue"),
		};
		for (const TCHAR* const Name : RequiredNames)
		{
			const FAngelscriptStaticJITFunctionResult* const Result =
				FindNamedRootResult(*Session->Engine, Output, Name);
			ASSERT_THAT(IsNotNull(Result,
				*FString::Printf(TEXT(
					"Generation must retain adapted Script function %s"),
					Name)));
			if (Result == nullptr)
			{
				return;
			}
			ASSERT_THAT(AreEqual(
				FAngelscriptStaticJITBackendId::TypedAST(),
				Result->ActualBackendId,
				*FString::Printf(TEXT(
					"Adapted Script scalar UFUNCTION %s must select TypedASTJIT: Backend=%s Detail=%s"),
					Name,
					*Result->ActualBackendId.ToString(),
					*Result->Detail)));
		}

		ASSERT_THAT(AreEqual(0, Observation.BytecodeAnalysisDuringTypedASTCount,
			TEXT("Typed eligibility must not invoke BytecodeJIT analysis")));
		ASSERT_THAT(AreEqual(
			0, Observation.BytecodeReferenceScanDuringTypedASTCount,
			TEXT("Typed reference collection must not scan bytecode")));
		ASSERT_THAT(AreEqual(0, Observation.GetByteCodeDuringTypedASTCount,
			TEXT("Typed generation must not read the VM instruction stream")));
		ASSERT_THAT(AreEqual(0, Observation.BytecodeDispatchDuringTypedASTCount,
			TEXT("Typed emission must not dispatch FAngelscriptBytecode")));
	}

	TEST_METHOD(AdaptedScriptClassAndArrayFixturesFallbackFromTypedBackend)
	{
		using namespace AngelscriptTypedASTJITScriptFunctionCorpusTests_Private;
		const FString ScriptSource = ASTEST_AS(R"AS(
			class FPhase2ActorLifecycleProbe
			{
				int Step()
				{
					return 21;
				}
			}

			UFUNCTION()
			int ActorLifecycleFixtureValue()
			{
				FPhase2ActorLifecycleProbe Probe;
				return Probe.Step();
			}

			class FPhase2SystemUtilsFixture
			{
				int Read()
				{
					return 13;
				}
			}

			UFUNCTION()
			int SystemUtilsFixtureValue()
			{
				FPhase2SystemUtilsFixture Fixture;
				return Fixture.Read();
			}

			class FPhase2ExampleActorFixture
			{
				int Value = 42;
			}

			UFUNCTION()
			int ExampleActorFixtureValue()
			{
				FPhase2ExampleActorFixture Fixture;
				return Fixture.Value;
			}

			class FPhase2MathFixture
			{
				int Value = 10;
			}

			UFUNCTION()
			int MathNamespaceFixtureValue()
			{
				FPhase2MathFixture Fixture;
				return Fixture.Value;
			}

			UFUNCTION()
			int ExampleArrayFixtureValue()
			{
				TArray<int> LocalIntArray;
				LocalIntArray.Add(2);
				LocalIntArray.Add(8);
				Log("Array contained values");
				return LocalIntArray.Num();
			}
			)AS");

		TUniquePtr<FCorpusGenerationSession> Session =
			CreateCorpusGenerationSession(
				*TestRunner,
				TEXT("ClassFallback"),
				TEXT("TypedASTJITScriptCorpusFallback"),
				ScriptSource);
		ASSERT_THAT(IsNotNull(Session.Get()));
		if (!Session.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Session->Engine);

		FAngelscriptStaticJITGeneratorOutput Output;
		ASSERT_THAT(IsTrue(GenerateCorpus(*TestRunner, *Session->Engine, Output)));
		FAngelscriptStaticJITBytecodeAccessAudit::ResetForTesting();

		const TCHAR* const FallbackNames[] = {
			TEXT("ActorLifecycleFixtureValue"),
			TEXT("SystemUtilsFixtureValue"),
			TEXT("ExampleActorFixtureValue"),
			TEXT("MathNamespaceFixtureValue"),
			TEXT("ExampleArrayFixtureValue"),
		};
		for (const TCHAR* const Name : FallbackNames)
		{
			const FAngelscriptStaticJITFunctionResult* const Result =
				FindNamedRootResult(*Session->Engine, Output, Name);
			ASSERT_THAT(IsNotNull(Result,
				*FString::Printf(TEXT(
					"Generation must retain adapted fallback Script function %s"),
					Name)));
			if (Result == nullptr)
			{
				return;
			}
			ASSERT_THAT(AreNotEqual(
				FAngelscriptStaticJITBackendId::TypedAST(),
				Result->ActualBackendId,
				*FString::Printf(TEXT(
					"Adapted class/array Script UFUNCTION %s must not select TypedASTJIT"),
					Name)));
			const EAngelscriptTypedASTJITFallbackReason Reason =
				ExtractFallbackReason(*Result);
			const bool bExpectedReason =
				Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime
				|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedType
				|| Reason == EAngelscriptTypedASTJITFallbackReason::MissingTypedHIR
				|| Reason == EAngelscriptTypedASTJITFallbackReason::InvalidCleanupPlan
				|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedStatement
				|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression
				|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedCall;
			ASSERT_THAT(IsTrue(bExpectedReason,
				*FString::Printf(TEXT(
					"Adapted class/array Script UFUNCTION %s must fallback with a typed lifetime/type reason: Reason=%s Detail=%s"),
					Name,
					LexToString(Reason),
					*Result->Detail)));
		}
	}
#endif
};

#endif
