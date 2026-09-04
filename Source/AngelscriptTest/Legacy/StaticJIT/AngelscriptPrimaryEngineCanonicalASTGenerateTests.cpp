#include "CQTest.h"

#include "../AngelScriptSDK/Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "AngelscriptEngine.h"
#include "Core/AngelscriptBinds.h"
#include "StaticJIT/AngelscriptJITProjectGeneration.h"
#include "StaticJIT/AngelscriptNativeFormCatalog.h"
#include "StaticJIT/BytecodeJIT/StaticJITBinds.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "angelscript.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptPrimaryEngineCanonicalASTGenerateTests,
	"Angelscript.TestModule.StaticJIT.PrimaryCanonicalASTGenerate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FString MakeProject(FString& OutRoot)
	{
		OutRoot = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/PrimaryCanonicalASTGenerate"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		check(IFileManager::Get().MakeDirectory(*OutRoot, true));
		const FString ProjectFile = FPaths::Combine(
			OutRoot, TEXT("PrimaryCanonicalAST.uproject"));
		check(FFileHelper::SaveStringToFile(
			TEXT("{\n  \"FileVersion\": 3,\n  \"Modules\": []\n}\n"),
			*ProjectFile,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		const FString ScriptRoot = FPaths::Combine(OutRoot, TEXT("Script"));
		check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		check(FFileHelper::SaveStringToFile(
			TEXT("int _PrimaryCanonicalASTGlobal = 40;\n")
			TEXT("\n")
			TEXT("UFUNCTION()\n")
			TEXT("int PrimaryCanonicalASTEntry()\n")
			TEXT("{\n")
			TEXT("\treturn _PrimaryCanonicalASTGlobal + 2;\n")
			TEXT("}\n"),
			*FPaths::Combine(ScriptRoot, TEXT("PrimaryCanonicalAST.as")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		return ProjectFile;
	}

	static FString MakeFactoryProject(FString& OutRoot)
	{
		OutRoot = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/PrimaryCanonicalASTFactory"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		check(IFileManager::Get().MakeDirectory(*OutRoot, true));
		const FString ProjectFile = FPaths::Combine(
			OutRoot, TEXT("PrimaryCanonicalASTFactory.uproject"));
		check(FFileHelper::SaveStringToFile(
			TEXT("{\n  \"FileVersion\": 3,\n  \"Modules\": []\n}\n"),
			*ProjectFile,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		const FString ScriptRoot = FPaths::Combine(OutRoot, TEXT("Script"));
		check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		check(FFileHelper::SaveStringToFile(
			TEXT("class FPrimaryPreparedRef\n")
			TEXT("{\n")
			TEXT("\tint Value;\n")
			TEXT("\tFPrimaryPreparedRef(int InValue) { Value = InValue; }\n")
			TEXT("\tint Read() { return Value; }\n")
			TEXT("}\n")
			TEXT("\n")
			TEXT("UFUNCTION()\n")
			TEXT("int PrimaryPreparedFactoryEntry()\n")
			TEXT("{\n")
			TEXT("\tFPrimaryPreparedRef Value = FPrimaryPreparedRef(42);\n")
			TEXT("\treturn Value.Read();\n")
			TEXT("}\n")
			TEXT("\n")
			TEXT("UFUNCTION()\n")
			TEXT("int PrimaryPreparedDefaultFactoryEntry()\n")
			TEXT("{\n")
			TEXT("\tFPrimaryPreparedRef Value;\n")
			TEXT("\treturn Value.Read();\n")
			TEXT("}\n"),
			*FPaths::Combine(ScriptRoot, TEXT("PrimaryCanonicalASTFactory.as")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		return ProjectFile;
	}

	static int32 CatalogHeaderInlineProbe(int32 Value)
	{
		return Value + 1;
	}

	static TUniquePtr<FAngelscriptEngine> CreatePrimaryEngine(
		const FString& ProjectRoot,
		const bool bSkipInitialCompile,
		const int32 RetentionPolicy,
		const bool bCollectNativeForms = false,
		const bool bUseCanonicalStagedCompiler = true)
	{
		FAngelscriptEngineConfig Config;
		Config.Purpose = EAngelscriptEnginePurpose::Runtime;
		Config.bUseCanonicalStagedCompiler = bUseCanonicalStagedCompiler;
		Config.InitialASTRetentionPolicy = RetentionPolicy;
		Config.bCollectStaticJITCompatibilityBinds = bCollectNativeForms;
		Config.bSkipInitialCompile = bSkipInitialCompile;
		Config.bSkipThreadedInitialize = true;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bRunningCommandlet = false;
		Config.bIsUnattended = true;
		Config.bForcePreprocessEditorCode = true;
		Config.bDisableCacheV2Persistence = true;
		Config.bForceSerialCacheV2Preparation = true;
		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
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
		return FAngelscriptEngine::Create(Config, Dependencies);
	}

public:
	BEFORE_EACH()
	{
		FAngelscriptEngine::ResetStaticJITGenerationEngineCreateCountForTests();
		FAngelscriptNativeFormCatalog::Get().ResetForTests();
	}

	TEST_METHOD(EngineConfigSelectsWholeModuleCompilerBeforeInitialization)
	{
		FString Root;
		MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		TUniquePtr<FAngelscriptEngine> Legacy = CreatePrimaryEngine(
			Root, true, 0, false, false);
		ASSERT_THAT(IsTrue(Legacy.IsValid()));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asCOMPILER_PIPELINE_LEGACY),
			static_cast<int32>(
				static_cast<asCScriptEngine*>(Legacy->GetScriptEngine())
					->GetCompilerPipeline()),
			TEXT("ordinary UE hosts must keep the whole module on LEGACY during staged migration")));

		Legacy.Reset();

		TUniquePtr<FAngelscriptEngine> Canonical = CreatePrimaryEngine(
			Root, true, 0, false, true);
		ASSERT_THAT(IsTrue(Canonical.IsValid()));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asCOMPILER_PIPELINE_CANONICAL),
			static_cast<int32>(
				static_cast<asCScriptEngine*>(Canonical->GetScriptEngine())
					->GetCompilerPipeline()),
			TEXT("the primary cutover fixture must select CANONICAL for the whole engine")));
	}

	TEST_METHOD(PrimaryInitialCompileUsesCanonicalCodeGenPublisher)
	{
		FString Root;
		MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		TUniquePtr<FAngelscriptEngine> Primary = CreatePrimaryEngine(
			Root, false, 1);
		ASSERT_THAT(IsTrue(Primary.IsValid()));
		ASSERT_THAT(IsTrue(Primary->DidInitialCompileSucceed()));

		asCModule* SourceModule = nullptr;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module
			: Primary->GetActiveModules())
		{
			if (Module->ScriptModule != nullptr
				&& Module->ScriptModule->GetFunctionByDecl(
					"int PrimaryCanonicalASTEntry()") != nullptr)
			{
				SourceModule = Module->ScriptModule;
				break;
			}
		}

		ASSERT_THAT(IsNotNull(SourceModule,
			TEXT("the primary source fixture must belong to one active module")));
		if (SourceModule == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN),
			static_cast<int32>(SourceModule->GetLastBytecodePublisher()),
			TEXT("the UE staged primary compile must publish Bytecode from the sealed canonical AST")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			SourceModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("the UE staged primary compile must not invoke asCCompiler")));
		const int GlobalIndex = SourceModule->GetGlobalVarIndexByName(
			"_PrimaryCanonicalASTGlobal");
		ASSERT_THAT(IsTrue(GlobalIndex >= 0,
			TEXT("the real staged module must publish its prepared global")));
		if (GlobalIndex >= 0)
		{
			ASSERT_THAT(IsNotNull(
				SourceModule->scriptGlobalsList[static_cast<asUINT>(GlobalIndex)]
					->GetInitFunc(),
				TEXT("Stage 3 must publish the Canonical property-owned initializer")));
		}
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner,
				Primary->GetScriptEngine(),
				SourceModule,
				"int PrimaryCanonicalASTEntry()",
				Value)
			&& Value == 42,
			TEXT("the real Stage 4 lifecycle must execute the Canonical global initializer")));
	}

	TEST_METHOD(PrimaryInitialCompileExecutesPreparedConstructorsAndFactories)
	{
		FString Root;
		MakeFactoryProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		TUniquePtr<FAngelscriptEngine> Primary = CreatePrimaryEngine(
			Root, false, 1);
		ASSERT_THAT(IsTrue(Primary.IsValid()));
		ASSERT_THAT(IsTrue(Primary->DidInitialCompileSucceed()));

		asCModule* SourceModule = nullptr;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module
			: Primary->GetActiveModules())
		{
			if (Module->ScriptModule != nullptr
				&& Module->ScriptModule->GetFunctionByDecl(
					"int PrimaryPreparedFactoryEntry()") != nullptr)
			{
				SourceModule = Module->ScriptModule;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(SourceModule,
			TEXT("the real primary host must publish the prepared constructor fixture")));
		if (SourceModule == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN),
			static_cast<int32>(SourceModule->GetLastBytecodePublisher())));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			SourceModule->GetLastLegacyCompilerInvocationCount()));

		int32 Value = -1;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner,
				Primary->GetScriptEngine(),
				SourceModule,
				"int PrimaryPreparedFactoryEntry()",
				Value)
			&& Value == 42,
			TEXT("the real staged host must execute the authored constructor through its prepared factory")));
		Value = -1;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner,
				Primary->GetScriptEngine(),
				SourceModule,
				"int PrimaryPreparedDefaultFactoryEntry()",
				Value)
			&& Value == 0,
			TEXT("the real staged host must execute the generated default constructor and its sealed field initializer")));
	}

	TEST_METHOD(MatchingProfileLeasesPrimaryAstWithoutGenerationEngine)
	{
		using namespace AngelscriptEditor::StaticJIT;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		TUniquePtr<FAngelscriptEngine> Primary = CreatePrimaryEngine(
			Root, false, 1);
		ASSERT_THAT(IsTrue(Primary.IsValid()));
		ASSERT_THAT(IsTrue(Primary->DidInitialCompileSucceed()));
		ASSERT_THAT(AreEqual(0,
			FAngelscriptEngine::GetStaticJITGenerationEngineCreateCountForTests()));

		const FAngelscriptCanonicalASTGenerateResult Result =
			FAngelscriptJITProjectGeneration::GenerateMatchingProfile(
				*Primary,
				ProjectFile,
				EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(Result.bSuccess, Result.Detail));
		ASSERT_THAT(AreEqual(
			EAngelscriptCanonicalASTGenerateCode::Succeeded,
			Result.Code));
		ASSERT_THAT(IsTrue(Result.LeasedASTSnapshotCount > 0));
		ASSERT_THAT(IsFalse(Result.bCompiledSource));
		ASSERT_THAT(AreEqual(0, Result.CreatedGenerationEngineCount));
		ASSERT_THAT(AreEqual(0,
			FAngelscriptEngine::GetStaticJITGenerationEngineCreateCountForTests()));
	}

	TEST_METHOD(DiscardPolicyFailsAstSnapshotRequiredWithoutSiblingEngine)
	{
		using namespace AngelscriptEditor::StaticJIT;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		TUniquePtr<FAngelscriptEngine> Primary = CreatePrimaryEngine(
			Root, false, 0);
		ASSERT_THAT(IsTrue(Primary.IsValid()));
		ASSERT_THAT(IsTrue(Primary->DidInitialCompileSucceed()));

		const FAngelscriptCanonicalASTGenerateResult Result =
			FAngelscriptJITProjectGeneration::GenerateMatchingProfile(
				*Primary,
				ProjectFile,
				EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsFalse(Result.bSuccess));
		ASSERT_THAT(AreEqual(
			EAngelscriptCanonicalASTGenerateCode::ASTSnapshotRequired,
			Result.Code));
		ASSERT_THAT(IsTrue(Result.Detail.Contains(TEXT("ASTSnapshotRequired"))));
		ASSERT_THAT(AreEqual(0, Result.CreatedGenerationEngineCount));
		ASSERT_THAT(AreEqual(0,
			FAngelscriptEngine::GetStaticJITGenerationEngineCreateCountForTests()));
	}

	TEST_METHOD(StalePrimaryFailsClosedWithoutSiblingFallback)
	{
		using namespace AngelscriptEditor::StaticJIT;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		TUniquePtr<FAngelscriptEngine> Primary = CreatePrimaryEngine(
			Root, true, 1);
		ASSERT_THAT(IsTrue(Primary.IsValid()));

		const FAngelscriptCanonicalASTGenerateResult Result =
			FAngelscriptJITProjectGeneration::GenerateMatchingProfile(
				*Primary,
				ProjectFile,
				EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsFalse(Result.bSuccess));
		ASSERT_THAT(AreEqual(
			EAngelscriptCanonicalASTGenerateCode::AuthoritativeEngineStale,
			Result.Code));
		ASSERT_THAT(AreEqual(0, Result.CreatedGenerationEngineCount));
		ASSERT_THAT(AreEqual(0,
			FAngelscriptEngine::GetStaticJITGenerationEngineCreateCountForTests()));
	}

	TEST_METHOD(MatchingGenerateFreezesHotReloadAndLeavesQueuedChanges)
	{
		using namespace AngelscriptEditor::StaticJIT;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		TUniquePtr<FAngelscriptEngine> Primary = CreatePrimaryEngine(
			Root, false, 1);
		ASSERT_THAT(IsTrue(Primary.IsValid()));
		FAngelscriptEngine::FFilenamePair Queued;
		Queued.AbsolutePath = FPaths::Combine(Root, TEXT("Script/Queued.as"));
		Queued.RelativePath = TEXT("Queued.as");
		Queued.VirtualPath = TEXT("/Angelscript/Game/Queued.as");
		Primary->FileChangesDetectedForReload.AddUnique(Queued);
		bool bSawFrozenQueue = false;
		FAngelscriptCanonicalASTGenerateRequest Request;
		Request.PrimaryEngine = Primary.Get();
		Request.ProjectFile = ProjectFile;
		Request.TargetProfile = EAngelscriptJITTargetProfile::EditorDevelopment;
		Request.OnHoldingLeases = [&]()
		{
			bSawFrozenQueue = Primary->IsHotReloadApplicationFrozen();
			ASSERT_THAT(IsFalse(Primary->TryProcessQueuedHotReload()));
			ASSERT_THAT(AreEqual(1, Primary->FileChangesDetectedForReload.Num()));
		};
		const FAngelscriptCanonicalASTGenerateResult Result =
			FAngelscriptJITProjectGeneration::GenerateMatchingProfile(Request);
		ASSERT_THAT(IsTrue(Result.bSuccess, Result.Detail));
		ASSERT_THAT(IsTrue(Result.bHotReloadFrozenWhileHeld));
		ASSERT_THAT(IsTrue(bSawFrozenQueue));
		ASSERT_THAT(IsFalse(Primary->IsHotReloadApplicationFrozen()));
		ASSERT_THAT(AreEqual(1, Primary->FileChangesDetectedForReload.Num()));
	}

	TEST_METHOD(MatchingGenerateLooksUpNativeFormCatalogWithoutSiblingEngine)
	{
		using namespace AngelscriptEditor::StaticJIT;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		TUniquePtr<FAngelscriptEngine> Primary = CreatePrimaryEngine(
			Root, false, 1, true);
		ASSERT_THAT(IsTrue(Primary.IsValid()));
		FAngelscriptBinds Binds(*Primary);
		FAngelscriptBoundFunction Function = Binds.BindGlobalFunctionForTarget(
			"int NativeFormCatalogGenerateProbe(int)",
			&CatalogHeaderInlineProbe);
		Function.NativeFunctionHeader(
			"CatalogHeaderInlineProbe",
			"NativeFormCatalogGenerateProbe.h",
			true);
		const FAngelscriptCanonicalASTGenerateResult Result =
			FAngelscriptJITProjectGeneration::GenerateMatchingProfile(
				*Primary,
				ProjectFile,
				EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(Result.bSuccess, Result.Detail));
		ASSERT_THAT(IsTrue(Result.CatalogRecipeHits > 0));
		ASSERT_THAT(AreEqual(0, Result.CreatedGenerationEngineCount));
	}

	TEST_METHOD(MatchingGenerateWritesOnlyOwnedOutput)
	{
		using namespace AngelscriptEditor::StaticJIT;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		TUniquePtr<FAngelscriptEngine> Primary = CreatePrimaryEngine(
			Root, false, 1);
		ASSERT_THAT(IsTrue(Primary.IsValid()));
		const FString OutputDirectory = FPaths::Combine(
			Root, TEXT("Generated/EditorDevelopment"));
		ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(
			*OutputDirectory, true)));
		const FString Sentinel = FPaths::Combine(
			OutputDirectory, TEXT("sentinel.txt"));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			TEXT("keep"), *Sentinel)));
		FAngelscriptCanonicalASTGenerateRequest Request;
		Request.PrimaryEngine = Primary.Get();
		Request.ProjectFile = ProjectFile;
		Request.TargetProfile = EAngelscriptJITTargetProfile::EditorDevelopment;
		Request.OutputDirectory = OutputDirectory;
		const FAngelscriptCanonicalASTGenerateResult Result =
			FAngelscriptJITProjectGeneration::GenerateMatchingProfile(Request);
		ASSERT_THAT(IsTrue(Result.bSuccess, Result.Detail));
		ASSERT_THAT(IsTrue(Result.WrittenOwnedFiles.Num() > 0));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*Sentinel)));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*FPaths::Combine(
			OutputDirectory, TEXT("OwnedFiles.generated.json")))));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(*FPaths::Combine(
			OutputDirectory, TEXT("Provider.generated.cpp")))));
	}

	TEST_METHOD(EarlyFailStagesDoNotWriteOwnedOutputOrMutatePrimary)
	{
		using namespace AngelscriptEditor::StaticJIT;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		TUniquePtr<FAngelscriptEngine> DiscardPrimary = CreatePrimaryEngine(
			Root, false, 0);
		ASSERT_THAT(IsTrue(DiscardPrimary.IsValid()));
		const int32 ModulesBefore = DiscardPrimary->GetActiveModules().Num();
		const FString OutputDirectory = FPaths::Combine(
			Root, TEXT("Generated/FailClosed"));
		FAngelscriptCanonicalASTGenerateRequest Request;
		Request.PrimaryEngine = DiscardPrimary.Get();
		Request.ProjectFile = ProjectFile;
		Request.TargetProfile = EAngelscriptJITTargetProfile::EditorDevelopment;
		Request.OutputDirectory = OutputDirectory;
		const FAngelscriptCanonicalASTGenerateResult Result =
			FAngelscriptJITProjectGeneration::GenerateMatchingProfile(Request);
		ASSERT_THAT(IsFalse(Result.bSuccess));
		ASSERT_THAT(AreEqual(
			EAngelscriptCanonicalASTGenerateCode::ASTSnapshotRequired,
			Result.Code));
		ASSERT_THAT(AreEqual(0, Result.WrittenOwnedFiles.Num()));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*OutputDirectory)));
		ASSERT_THAT(AreEqual(ModulesBefore,
			DiscardPrimary->GetActiveModules().Num()));
		ASSERT_THAT(AreEqual(0, Result.CreatedGenerationEngineCount));
	}

	TEST_METHOD(SequentialProfilesUseAtMostOneContainedEngine)
	{
		using namespace AngelscriptEditor::StaticJIT;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		TUniquePtr<FAngelscriptEngine> Primary = CreatePrimaryEngine(
			Root, false, 1);
		ASSERT_THAT(IsTrue(Primary.IsValid()));
		FAngelscriptEngine::ResetStaticJITGenerationEngineCreateCountForTests();
		const TArray<EAngelscriptJITTargetProfile> Profiles = {
			EAngelscriptJITTargetProfile::EditorDevelopment,
			EAngelscriptJITTargetProfile::GameShipping,
		};
		const TArray<FAngelscriptCanonicalASTGenerateResult> Results =
			FAngelscriptJITProjectGeneration::GenerateProfilesSequential(
				*Primary,
				ProjectFile,
				Profiles,
				FPaths::Combine(Root, TEXT("Generated")));
		ASSERT_THAT(AreEqual(2, Results.Num()));
		ASSERT_THAT(IsTrue(Results[0].bSuccess, Results[0].Detail));
		ASSERT_THAT(AreEqual(0, Results[0].CreatedGenerationEngineCount));
		ASSERT_THAT(IsTrue(Results[1].bSuccess || !Results[1].Detail.IsEmpty()));
		ASSERT_THAT(IsTrue(Results[1].MaxLiveGenerationEngines <= 1));
		ASSERT_THAT(AreEqual(0,
			FAngelscriptEngine::GetStaticJITGenerationEngineLiveCountForTests()));
	}

	TEST_METHOD(PrimaryRetainedAstLeaseRemainsCurrentBeforeMatchingGeneration)
	{
		using namespace AngelscriptEditor::StaticJIT;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		TUniquePtr<FAngelscriptEngine> Primary = CreatePrimaryEngine(
			Root, false, 1);
		ASSERT_THAT(IsTrue(Primary.IsValid()));
		asCModule* SourceModule = nullptr;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module
			: Primary->GetActiveModules())
		{
			if (Module->ScriptModule != nullptr
				&& Module->ScriptModule->GetFunctionByDecl(
					"int PrimaryCanonicalASTEntry()") != nullptr)
			{
				SourceModule = Module->ScriptModule;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(SourceModule,
			TEXT("the retained primary module must remain discoverable")));
		if (SourceModule == nullptr)
		{
			return;
		}
		asIASTSnapshot* const Snapshot =
			SourceModule->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Snapshot,
			TEXT("matching generation requires a retained Canonical snapshot")));
		if (Snapshot == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Snapshot->IsCurrentGeneration()));
		ASSERT_THAT(IsTrue(Snapshot->GetGenerationKey() != nullptr
			&& Snapshot->GetGenerationKey()[0] != '\0'));
		Snapshot->Release();
		FAngelscriptCanonicalASTGenerateRequest GenerateRequest;
		GenerateRequest.PrimaryEngine = Primary.Get();
		GenerateRequest.ProjectFile = ProjectFile;
		GenerateRequest.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		const FAngelscriptCanonicalASTGenerateResult Generate =
			FAngelscriptJITProjectGeneration::GenerateMatchingProfile(
				GenerateRequest);
		ASSERT_THAT(IsTrue(Generate.bSuccess, Generate.Detail));
	}
};

#endif
