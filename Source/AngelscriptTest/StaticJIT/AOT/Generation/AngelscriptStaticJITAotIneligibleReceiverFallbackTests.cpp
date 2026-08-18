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
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITModel.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotIneligibleReceiverFallbackTests,
	"Angelscript.TestModule.StaticJIT.AOT.Generation.IneligibleReceiverFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FIneligibleCase
	{
		const TCHAR* FunctionName = nullptr;
		const TCHAR* Kind = nullptr;
	};

	static bool SourceDeclaresRoot(
		const FString& ScriptSource,
		const TCHAR* FunctionName)
	{
		return ScriptSource.Contains(FunctionName, ESearchCase::CaseSensitive);
	}

#if AS_CAN_GENERATE_JIT
	struct FGenerationSession
	{
		FString Root;
		TUniquePtr<FAngelscriptEngine> Engine;
	};

	TUniquePtr<FGenerationSession> CreateGenerationSession()
	{
		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		FString ProfileError;
		if (!TestRunner->TestTrue(
			TEXT("Ineligible-receiver generation should resolve the typed-ast profile"),
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::TypedAST(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				GenerationProfile,
				&ProfileError)))
		{
			TestRunner->AddError(ProfileError);
			return nullptr;
		}

		TUniquePtr<FGenerationSession> Session = MakeUnique<FGenerationSession>();
		Session->Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/StaticJITAotIneligibleReceiver"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		FPaths::NormalizeDirectoryName(Session->Root);
		const FString ScriptRoot = FPaths::Combine(Session->Root, TEXT("Script"));
		const FString ScriptPath = FPaths::Combine(
			ScriptRoot,
			AngelscriptStaticJITAotFixture::GetIneligibleReceiverSourceFilename());
		if (!TestRunner->TestTrue(
				TEXT("Ineligible-receiver generation should create a Script root"),
				IFileManager::Get().MakeDirectory(*ScriptRoot, true))
			|| !TestRunner->TestTrue(
				TEXT("Ineligible-receiver generation should write fixture source"),
				FFileHelper::SaveStringToFile(
					AngelscriptStaticJITAotFixture::
						GetIneligibleReceiverScriptSource(),
					*ScriptPath)))
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
		if (!TestRunner->TestNotNull(
				TEXT("Ineligible-receiver generation should create an Engine"),
				Session->Engine.Get())
			|| !TestRunner->TestTrue(
				TEXT("Ineligible-receiver fixture source should compile"),
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
			TEXT("typed-ast-aot-ineligible-receiver-provider"));
		ProviderWriter.WriteString(TEXT("IneligibleReceiverFallback"));
		Settings.ProviderId.Hash = ProviderWriter.FinalizeHash();
		Settings.ProviderName = TEXT("TypedASTJIT AOT Ineligible Receiver");
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

	bool GenerateFixture(
		FAngelscriptEngine& Engine,
		FAngelscriptStaticJITGeneratorOutput& OutOutput)
	{
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Engine.GetStaticJITGenerationSnapshot();
		if (!TestRunner->TestTrue(
			TEXT("Ineligible-receiver generation must publish a complete snapshot"),
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
		if (!TestRunner->TestTrue(
			TEXT("Ineligible-receiver generation must have compiled modules"),
			CompiledModules.Num() > 0))
		{
			return false;
		}

		FAngelscriptStaticJITGenerationProfile RequestProfile;
		FString RequestProfileError;
		if (!TestRunner->TestTrue(
			TEXT("Ineligible-receiver generation should rebuild the typed-ast profile"),
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::TypedAST(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				RequestProfile,
				&RequestProfileError)))
		{
			TestRunner->AddError(RequestProfileError);
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

		FString GenerationError;
		const bool bGenerated = GenerateStaticJITProviderArtifacts(
			Request, OutOutput, &GenerationError);
		if (!TestRunner->TestTrue(GenerationError, bGenerated))
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

	static bool IsExpectedMixinReason(
		const EAngelscriptTypedASTJITFallbackReason Reason)
	{
		return Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver
			|| Reason == EAngelscriptTypedASTJITFallbackReason::InvalidEffectiveReceiver
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedCall
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedType
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionTrait
			|| Reason == EAngelscriptTypedASTJITFallbackReason::MissingTypedHIR;
	}

	static bool IsExpectedExternalReason(
		const EAngelscriptTypedASTJITFallbackReason Reason)
	{
		return Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver
			|| Reason == EAngelscriptTypedASTJITFallbackReason::InvalidEffectiveReceiver
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedType
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionTrait
			|| Reason == EAngelscriptTypedASTJITFallbackReason::MissingTypedHIR;
	}

	static bool IsExpectedLifecycleReason(
		const EAngelscriptTypedASTJITFallbackReason Reason)
	{
		return Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime
			|| Reason == EAngelscriptTypedASTJITFallbackReason::MissingTypedHIR
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionKind
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedCall
			|| Reason == EAngelscriptTypedASTJITFallbackReason::InvalidCleanupPlan
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionTrait
			|| Reason == EAngelscriptTypedASTJITFallbackReason::UnsupportedType;
	}
#endif

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

	TEST_METHOD(FixtureDeclaresMixinExternalAndGeneratedLifecycleRoots)
	{
		const FString& ScriptSource =
			AngelscriptStaticJITAotFixture::GetIneligibleReceiverScriptSource();
		ASSERT_THAT(IsTrue(SourceDeclaresRoot(ScriptSource, TEXT("mixin")),
			TEXT("AOT ineligible fixture must declare a mixin receiver")));
		ASSERT_THAT(IsTrue(
			SourceDeclaresRoot(ScriptSource, TEXT("MixinReceiverRoot")),
			TEXT("AOT ineligible fixture must declare MixinReceiverRoot")));
		ASSERT_THAT(IsTrue(
			SourceDeclaresRoot(ScriptSource, TEXT("external_implicit_this")),
			TEXT("AOT ineligible fixture must declare an external implicit receiver")));
		ASSERT_THAT(IsTrue(
			SourceDeclaresRoot(ScriptSource, TEXT("ExternalReceiverRoot")),
			TEXT("AOT ineligible fixture must declare ExternalReceiverRoot")));
		ASSERT_THAT(IsTrue(
			SourceDeclaresRoot(ScriptSource, TEXT("GeneratedLifecycleRoot")),
			TEXT("AOT ineligible fixture must declare GeneratedLifecycleRoot")));
	}

	TEST_METHOD(IneligibleRootsStayVmExecutable)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource =
			AngelscriptStaticJITAotFixture::GetIneligibleReceiverScriptSource()
			+ ASTEST_AS(R"AS(
			int RunMixinReceiverRoot()
			{
				UAotIneligibleMixinHost Host = UAotIneligibleMixinHost();
				return Host.MixinReceiverRoot(5);
			}

			int RunExternalReceiverRoot()
			{
				UAotIneligibleMixinHost Host = UAotIneligibleMixinHost();
				return ExternalReceiverRoot(Host, 5);
			}

			int RunGeneratedLifecycleRoot()
			{
				UAotGeneratedLifecycleHost Host = UAotGeneratedLifecycleHost();
				return Host.GeneratedLifecycleRoot();
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			*AngelscriptStaticJITAotFixture::GetIneligibleReceiverModuleName().ToString(),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(),
			TEXT("Ineligible receiver/lifecycle fixture should compile for VM")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		int32 VmSuccessCount = 0;
		if (ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int RunMixinReceiverRoot()"),
			TEXT("mixin ineligible fixture"),
			AngelscriptStaticJITAotFixture::GetExpectedMixinReceiverRootResult()))
		{
			++VmSuccessCount;
		}
		if (ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int RunExternalReceiverRoot()"),
			TEXT("external-receiver ineligible fixture"),
			AngelscriptStaticJITAotFixture::GetExpectedExternalReceiverRootResult()))
		{
			++VmSuccessCount;
		}
		if (ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int RunGeneratedLifecycleRoot()"),
			TEXT("generated-lifecycle ineligible fixture"),
			AngelscriptStaticJITAotFixture::GetExpectedGeneratedLifecycleRootResult()))
		{
			++VmSuccessCount;
		}
		ASSERT_THAT(AreEqual(3, VmSuccessCount,
			TEXT("Mixin, external-receiver, and generated-lifecycle fixtures must stay VM executable")));
	}

#if AS_CAN_GENERATE_JIT
	TEST_METHOD(IneligibleRootsFallbackToBytecodeNotTypedAst)
	{
		TUniquePtr<FGenerationSession> Session = CreateGenerationSession();
		ASSERT_THAT(IsNotNull(Session.Get(),
			TEXT("Ineligible-receiver generation session should start")));
		if (!Session.IsValid())
		{
			return;
		}

		FAngelscriptEngineScope Scope(*Session->Engine);
		FAngelscriptStaticJITGeneratorOutput Output;
		ASSERT_THAT(IsTrue(GenerateFixture(*Session->Engine, Output),
			TEXT("Ineligible-receiver fixture should generate with typed-ast requested")));

		const FIneligibleCase Cases[] = {
			{TEXT("MixinReceiverRoot"), TEXT("mixin")},
			{TEXT("ExternalReceiverRoot"), TEXT("external-receiver")},
			{TEXT("GeneratedLifecycleRoot"), TEXT("generated-lifecycle")},
		};
		int32 BytecodeFallbackCount = 0;
		int32 TypedSelectionCount = 0;
		for (const FIneligibleCase& Case : Cases)
		{
			const FAngelscriptStaticJITFunctionResult* const Result =
				FindNamedRootResult(*Session->Engine, Output, Case.FunctionName);
			ASSERT_THAT(IsNotNull(Result,
				*FString::Printf(TEXT(
					"AOT generation must retain the ineligible %s root %s"),
					Case.Kind, Case.FunctionName)));
			if (Result == nullptr)
			{
				continue;
			}

			if (Result->ActualBackendId == FAngelscriptStaticJITBackendId::TypedAST())
			{
				++TypedSelectionCount;
			}

			ASSERT_THAT(AreNotEqual(
				FAngelscriptStaticJITBackendId::TypedAST(),
				Result->ActualBackendId,
				*FString::Printf(TEXT(
					"Ineligible %s root %s must not select TypedASTJIT; actual=%s detail=%s"),
					Case.Kind,
					Case.FunctionName,
					*Result->ActualBackendId.ToString(),
					*Result->Detail)));
			ASSERT_THAT(AreEqual(
				FAngelscriptStaticJITBackendId::Bytecode(),
				Result->ActualBackendId,
				*FString::Printf(TEXT(
					"Ineligible %s root %s must fall back to BytecodeJIT; actual=%s"),
					Case.Kind,
					Case.FunctionName,
					*Result->ActualBackendId.ToString())));

			const EAngelscriptTypedASTJITFallbackReason Reason =
				ExtractFallbackReason(*Result);
			const bool bExpectedReason =
				(FCString::Strcmp(Case.FunctionName, TEXT("MixinReceiverRoot")) == 0
					&& IsExpectedMixinReason(Reason))
				|| (FCString::Strcmp(Case.FunctionName, TEXT("ExternalReceiverRoot")) == 0
					&& IsExpectedExternalReason(Reason))
				|| (FCString::Strcmp(Case.FunctionName, TEXT("GeneratedLifecycleRoot")) == 0
					&& IsExpectedLifecycleReason(Reason));
			ASSERT_THAT(IsTrue(bExpectedReason,
				*FString::Printf(TEXT(
					"Ineligible %s root %s must report a typed fallback reason: Reason=%s Detail=%s"),
					Case.Kind,
					Case.FunctionName,
					LexToString(Reason),
					*Result->Detail)));
			if (Result->ActualBackendId == FAngelscriptStaticJITBackendId::Bytecode())
			{
				++BytecodeFallbackCount;
			}
		}

		ASSERT_THAT(AreEqual(0, TypedSelectionCount,
			TEXT("No ineligible mixin/external/lifecycle root may publish TypedASTJIT")));
		ASSERT_THAT(AreEqual(3, BytecodeFallbackCount,
			TEXT("Mixin, external-receiver, and generated-lifecycle counters must prove BytecodeJIT fallback")));
	}
#endif
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
