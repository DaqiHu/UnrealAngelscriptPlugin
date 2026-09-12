#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"

#include "AngelscriptTestEngineAcquisition.h"
#include "AngelscriptTestJITProbes.h"
#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "Compilation/AngelscriptCompilationEvents.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AngelscriptJITGeneratedFileStore.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"
#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCanonical.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITProviderEmitter.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/AST/as_ast_context.h"
#include "as_module.h"
#include "as_scriptfunction.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptStaticJITAotGeneration
{
	namespace
	{
		static FString GetTestJITModuleRoot()
		{
			return FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("Plugins/Angelscript/Source/AngelscriptTestJIT")));
		}

		static FString GetGeneratedProfileRoot()
		{
			return FPaths::Combine(
				GetTestJITModuleRoot(), TEXT("Generated/EditorDevelopment"));
		}

		static TArray<FString> GetLegacyGeneratedProfileRoots()
		{
			return {
				FPaths::Combine(GetTestJITModuleRoot(),
					TEXT("Private/Generated/EditorDevelopment")),
				FPaths::Combine(GetTestJITModuleRoot(),
					TEXT("Private/Generated/Profiles/EditorDevelopment")),
			};
		}

		static void RemoveEmptyLegacyPrivateWrappers()
		{
			const FString PrivateRoot = FPaths::Combine(
				GetTestJITModuleRoot(), TEXT("Private"));
			const FString GeneratedRoot = FPaths::Combine(
				PrivateRoot, TEXT("Generated"));
			IFileManager::Get().DeleteDirectory(
				*FPaths::Combine(GeneratedRoot, TEXT("Profiles")),
				false, false);
			IFileManager::Get().DeleteDirectory(
				*GeneratedRoot, false, false);
			IFileManager::Get().DeleteDirectory(
				*PrivateRoot, false, false);
		}

		static TUniquePtr<FAngelscriptEngine> CreateUncompiledFixtureEngine(
			const FString& FixtureRoot,
			FString& OutError,
			const bool bEnableCacheV2Persistence,
			const FAngelscriptStaticJITGenerationProfile* GenerationProfile,
			const EAngelscriptEnginePurpose Purpose,
			const EStaticJITAotFixtureComposition Composition,
			const AngelscriptStaticJITAotFixture::
				ETypedASTPrivateBridgeRegistration PrivateBridgeRegistration,
			const bool bForceManualImports,
			const bool bRequireCanonicalCompilerIdentity)
		{
			FAngelscriptEngineConfig Config;
			Config.Purpose = Purpose;
			if (GenerationProfile != nullptr)
			{
				GenerationProfile->ApplyToEngineConfig(Config);
			}
			// Provider execution identity includes the emitted VM artifact. A
			// runtime fixture adopting a committed TypedASTJIT Provider must
			// therefore compile with the same CANONICAL pipeline as generation,
			// without pretending that the runtime Engine is itself a generation
			// host or changing the product-default LEGACY selection.
			if (bRequireCanonicalCompilerIdentity)
			{
				Config.bUseCanonicalStagedCompiler = true;
			}
			Config.bIsEditor = true;
			Config.bDevelopmentMode = true;
			Config.bSkipThreadedInitialize = true;
			Config.bRunningCommandlet = true;
			Config.bIsUnattended = true;
			Config.CacheV2RootOverride = FPaths::Combine(
				FixtureRoot, TEXT("CacheV2"));
			// This fixture parameter is an explicit test-host selection, not only
			// a disk-persistence allowance. Keep the Cache V2 producer/restore
			// proof runnable while the product setting remains default-off, and
			// keep ordinary Provider adoption genuinely Cache-free.
			Config.bOverrideCacheV2Enablement = true;
			Config.bEnableCacheV2 = bEnableCacheV2Persistence;
			Config.bDisableCacheV2Persistence =
				!bEnableCacheV2Persistence;
			// Automation invokes this synchronously on the game thread. Serial
			// preparation avoids starving TaskGraph writer workers while preserving
			// the exact same deterministic capture bytes and Store contract.
			Config.bForceSerialCacheV2Preparation = true;

			FAngelscriptEngineDependencies Dependencies =
				FAngelscriptEngineDependencies::CreateDefault();
			Dependencies.GetProjectDir = [FixtureRoot]()
			{
				return FixtureRoot;
			};
			Dependencies.GetEnabledPluginScriptRoots = []()
			{
				return TArray<FString>();
			};
			Dependencies.GetEnabledPluginScriptRootDescriptors = []()
			{
				return TArray<FAngelscriptPluginScriptRoot>();
			};

			TUniquePtr<FAngelscriptEngine> Engine =
				CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
			if (!Engine.IsValid())
			{
				OutError = TEXT(
					"Failed to create the isolated AngelscriptTestJIT fixture Engine.");
				return nullptr;
			}
			if (bForceManualImports)
			{
				// This is a deliberately separate legacy-profile fixture. The normal
				// project/generation path keeps the target's bAutomaticImports value;
				// only the import-slot differential needs CALLBND and mutable
				// BindImportedFunction/UnbindImportedFunction semantics.
				Engine->SetAutomaticImportMethodForTesting(false);
				if (Engine->GetScriptEngine()->SetEngineProperty(
						asEP_AUTOMATIC_IMPORTS, 0) < 0
					|| Engine->GetScriptEngine()->GetEngineProperty(
						asEP_AUTOMATIC_IMPORTS) != 0)
				{
					OutError = TEXT(
						"AngelscriptTestJIT fixture could not select the manual imported-function-slot profile.");
					return nullptr;
				}
			}
			if (!AngelscriptStaticJITAotFixture::RegisterObjectLastNativeSurface(
				*Engine->GetScriptEngine()))
			{
				OutError = TEXT(
					"AngelscriptTestJIT fixture could not register its native probe surface.");
				return nullptr;
			}
			if (Composition == EStaticJITAotFixtureComposition::Complete
				&& !AngelscriptStaticJITAotFixture::
					RegisterTypedASTPrivateBridgeSurface(
						*Engine, PrivateBridgeRegistration))
			{
				OutError = TEXT(
					"AngelscriptTestJIT fixture could not register its provider-private TypedAST bridge surface.");
				return nullptr;
			}

			return Engine;
		}

		static int32 CountFrontendEvents(
			const TConstArrayView<FAngelscriptCompilationEvent> Events)
		{
			int32 Count = 0;
			for (const FAngelscriptCompilationEvent& Event : Events)
			{
				switch (Event.Type)
				{
				case EAngelscriptCompilationEventType::PreprocessProcessChunks:
				case EAngelscriptCompilationEventType::PreprocessPostProcessCode:
				case EAngelscriptCompilationEventType::CompileModuleParse:
				case EAngelscriptCompilationEventType::CompileModuleCompileCode:
					++Count;
					break;
				default:
					break;
				}
			}
			return Count;
		}

		static TUniquePtr<FAngelscriptEngine> CreateFixtureEngine(
			FString& OutFixtureRoot,
			FStaticJITAotGenerationResult& Result,
			const bool bEnableCacheV2Persistence,
			const FAngelscriptStaticJITGenerationProfile* GenerationProfile,
			const EAngelscriptEnginePurpose Purpose,
			const EStaticJITAotFixtureComposition Composition,
			const AngelscriptStaticJITAotFixture::
				ETypedASTPrivateBridgeRegistration PrivateBridgeRegistration,
			const FString* const TypedFinalScalarSourceOverride = nullptr,
			const bool bForceManualImports = false,
			const bool bRequireCanonicalCompilerIdentity = false)
		{
			OutFixtureRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptTestJITGeneration"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(OutFixtureRoot);
			if (!OutFixtureRoot.Contains(
				TEXT("/Saved/Automation/AngelscriptTestJITGeneration/")))
			{
				Result.Error = TEXT(
					"AngelscriptTestJIT fixture root escaped its isolated Saved directory.");
				return nullptr;
			}

			const FString ScriptRoot = FPaths::Combine(
				OutFixtureRoot, TEXT("Script"));
			const FString& TypedFinalScalarSource =
				TypedFinalScalarSourceOverride != nullptr
					? *TypedFinalScalarSourceOverride
					: AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarScriptSource();
			bool bSourcesWritten =
				IFileManager::Get().MakeDirectory(*ScriptRoot, true)
				&& FFileHelper::SaveStringToFile(
					AngelscriptStaticJITAotFixture::GetImportProviderScriptSource(),
					*FPaths::Combine(
						ScriptRoot,
						AngelscriptStaticJITAotFixture::
							GetImportProviderSourceFilename()))
				&& FFileHelper::SaveStringToFile(
					AngelscriptStaticJITAotFixture::GetScriptSource(),
					*FPaths::Combine(
						ScriptRoot,
						AngelscriptStaticJITAotFixture::GetSourceFilename()))
				&& FFileHelper::SaveStringToFile(
					AngelscriptStaticJITAotFixture::
						GetTypedControlFlowScriptSource(),
					*FPaths::Combine(
						ScriptRoot,
						AngelscriptStaticJITAotFixture::
							GetTypedControlFlowSourceFilename()))
				&& FFileHelper::SaveStringToFile(
					TypedFinalScalarSource,
					*FPaths::Combine(
						ScriptRoot,
						AngelscriptStaticJITAotFixture::
							GetTypedFinalScalarSourceFilename()));
			if (Composition == EStaticJITAotFixtureComposition::Complete)
			{
				bSourcesWritten = bSourcesWritten
					&& FFileHelper::SaveStringToFile(
						AngelscriptStaticJITAotFixture::GetTypedPrintScriptSource(),
						*FPaths::Combine(
							ScriptRoot,
							AngelscriptStaticJITAotFixture::
								GetTypedPrintSourceFilename()))
					&& FFileHelper::SaveStringToFile(
						AngelscriptStaticJITAotFixture::
							GetTypedPrivateBridgeScriptSource(),
						*FPaths::Combine(
							ScriptRoot,
							AngelscriptStaticJITAotFixture::
								GetTypedPrivateBridgeSourceFilename()))
					&& FFileHelper::SaveStringToFile(
						AngelscriptStaticJITAotFixture::
							GetTypedRecursionScriptSource(),
						*FPaths::Combine(
							ScriptRoot,
							AngelscriptStaticJITAotFixture::
								GetTypedRecursionSourceFilename()));
			}
			if (!bSourcesWritten)
			{
				Result.Error = TEXT(
					"AngelscriptTestJIT could not materialize its committed fixture sources.");
				return nullptr;
			}

			TUniquePtr<FAngelscriptEngine> Engine =
				CreateUncompiledFixtureEngine(
					OutFixtureRoot, Result.Error,
					bEnableCacheV2Persistence,
					GenerationProfile, Purpose, Composition,
					PrivateBridgeRegistration, bForceManualImports,
					bRequireCanonicalCompilerIdentity);
			if (!Engine.IsValid())
			{
				return nullptr;
			}

			{
				FAngelscriptEngineScope EngineScope(*Engine);
				Engine->InitialCompile();
			}

			const bool bCoreModulesValid =
				Engine->GetModuleByModuleName(
					AngelscriptStaticJITAotFixture::
						GetImportProviderModuleName().ToString()).IsValid()
				&& Engine->GetModuleByModuleName(
					AngelscriptStaticJITAotFixture::
						GetModuleName().ToString()).IsValid()
				&& Engine->GetModuleByModuleName(
					AngelscriptStaticJITAotFixture::
						GetTypedControlFlowModuleName().ToString()).IsValid()
				&& Engine->GetModuleByModuleName(
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarModuleName().ToString()).IsValid();
			const bool bCallModulesValid =
				Composition != EStaticJITAotFixtureComposition::Complete
				|| (Engine->GetModuleByModuleName(
						AngelscriptStaticJITAotFixture::
							GetTypedPrintModuleName().ToString()).IsValid()
					&& Engine->GetModuleByModuleName(
						AngelscriptStaticJITAotFixture::
							GetTypedPrivateBridgeModuleName().ToString()).IsValid()
					&& Engine->GetModuleByModuleName(
						AngelscriptStaticJITAotFixture::
							GetTypedRecursionModuleName().ToString()).IsValid());
			if (!bCoreModulesValid || !bCallModulesValid)
			{
				Result.Error = TEXT(
					"AngelscriptTestJIT isolated fixture source compilation failed.");
				return nullptr;
			}
			const TSharedPtr<FAngelscriptModuleDesc> PrimaryModule =
				Engine->GetModuleByModuleName(
					AngelscriptStaticJITAotFixture::GetModuleName().ToString());
			const asCModule* const PrimaryScriptModule =
				PrimaryModule.IsValid()
					? static_cast<const asCModule*>(PrimaryModule->ScriptModule)
					: nullptr;
			if (PrimaryModule.IsValid() && PrimaryScriptModule != nullptr)
			{
				UE_LOG(Angelscript, Verbose,
					TEXT("[TestJITGeneration] Primary capture shape: Classes=%d Enums=%d Delegates=%d ImportedModules=%d PostInitFunctions=%d ObjectTypes=%u ScriptEnums=%u Typedefs=%u ImportedFunctions=%u"),
					PrimaryModule->Classes.Num(), PrimaryModule->Enums.Num(),
					PrimaryModule->Delegates.Num(),
					PrimaryModule->ImportedModules.Num(),
					PrimaryModule->PostInitFunctions.Num(),
					PrimaryScriptModule->GetObjectTypeCount(),
					PrimaryScriptModule->GetEnumCount(),
					PrimaryScriptModule->GetTypedefCount(),
					PrimaryScriptModule->GetImportedFunctionCount());
			}
			return Engine;
		}

		static bool AppendTypedASTScalarProbe(
			FAngelscriptEngine& Engine,
			const TConstArrayView<FAngelscriptStaticJITFunctionResult>
				BackendFunctionResults,
			FAngelscriptJITGenerationOutput& InOutOutput,
			FStaticJITAotGenerationResult& Result,
			const bool bObserveEmitterFailureFallback)
		{
			const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc =
				Engine.GetModuleByModuleName(
					AngelscriptStaticJITAotFixture::GetModuleName().ToString());
			if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
			{
				Result.Error = TEXT(
					"TypedASTJIT probe could not resolve its compiled fixture module.");
				return false;
			}

			FTCHARToUTF8 DeclarationUtf8(
				*AngelscriptStaticJITAotFixture::
					GetSemanticScalarBranchDeclaration());
			asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
				ModuleDesc->ScriptModule->GetFunctionByDecl(DeclarationUtf8.Get()));
			if (Function == nullptr)
			{
				Result.Error = TEXT(
					"TypedASTJIT probe could not resolve SemanticScalarBranch by exact declaration.");
				return false;
			}

			const asCModule* const CanonicalModule =
				static_cast<const asCModule*>(ModuleDesc->ScriptModule);
			const asCASTContext* const CanonicalAST =
				CanonicalModule != nullptr
					? CanonicalModule->GetCanonicalASTContext()
					: nullptr;
			const asASTDeclId CanonicalFunctionDecl =
				CanonicalAST != nullptr
					? FindAngelscriptTypedASTJITCanonicalFunctionDecl(
						*CanonicalAST, Function->name.AddressOf())
					: asASTDeclId();
			const asCDecl* const CanonicalFunction =
				CanonicalAST != nullptr
					? CanonicalAST->GetDecl(CanonicalFunctionDecl)
					: nullptr;
			if (CanonicalAST == nullptr
				|| !CanonicalAST->IsPublishable()
				|| CanonicalFunction == nullptr
				|| !CanonicalFunction->body.IsValid())
			{
				Result.Error = TEXT(
					"TypedASTJIT probe source compile published no sealed Canonical AST body.");
				return false;
			}

			FAngelscriptTypedASTJITFunctionShape Shape;
			Shape.SymbolName =
				TEXT("AS_Test_Semantic_SemanticScalarBranch");
			Shape.ReturnType = Function->returnType;
			Shape.InvocationKind = Function->objectType != nullptr
				? EAngelscriptTypedASTJITInvocationKind::InstanceMethod
				: EAngelscriptTypedASTJITInvocationKind::Global;
			Shape.ReceiverKind = Function->objectType != nullptr
				? EAngelscriptTypedASTJITReceiverKind::NativeObjectThis
				: EAngelscriptTypedASTJITReceiverKind::None;
			Shape.bReturnsOnStack = Function->DoesReturnOnStack();
			for (asUINT Index = 0;
				Index < Function->parameterTypes.GetLength(); ++Index)
			{
				Shape.ParameterTypes.Add(Function->parameterTypes[Index]);
			}

			const FAngelscriptTypedASTJITEmission Emission =
				EmitTypedASTJITFunction(
					*CanonicalAST, CanonicalFunctionDecl, Shape, {});
			if (!Emission.bSuccess)
			{
				Result.Error = FString::Printf(
					TEXT("TypedASTJIT scalar fixture unexpectedly fell back: Reason=%u Detail=%s"),
					static_cast<uint32>(Emission.FailureReason),
					*Emission.FailureDetail);
				return false;
			}

			TArray<FString> ScalarProbeRequiredIncludes =
				Emission.RequiredIncludes;
			FString ScalarProbeDefinitions = Emission.Definition;
			auto AppendTypedProbe = [&Engine,
				&ScalarProbeRequiredIncludes,
				&ScalarProbeDefinitions,
				&Result](
					const FName& ModuleName,
					const FString& Declaration,
					const FString& SymbolName,
					const TCHAR* Label)
			{
				const TSharedPtr<FAngelscriptModuleDesc> ProbeModuleDesc =
					Engine.GetModuleByModuleName(ModuleName.ToString());
				FTCHARToUTF8 DeclarationUtf8(*Declaration);
				asCScriptFunction* const ProbeFunction =
					ProbeModuleDesc.IsValid()
						&& ProbeModuleDesc->ScriptModule != nullptr
					? static_cast<asCScriptFunction*>(
						ProbeModuleDesc->ScriptModule->GetFunctionByDecl(
							DeclarationUtf8.Get()))
					: nullptr;
				if (ProbeFunction == nullptr)
				{
					Result.Error = FString::Printf(
						TEXT("TypedASTJIT probe could not resolve %s by exact declaration in module %s: %s"),
						Label, *ModuleName.ToString(), *Declaration);
					return false;
				}

				const asCModule* const CanonicalProbeModule =
					ProbeModuleDesc.IsValid()
						? static_cast<const asCModule*>(
							ProbeModuleDesc->ScriptModule)
						: nullptr;
				const asCASTContext* const ProbeAST =
					CanonicalProbeModule != nullptr
						? CanonicalProbeModule->GetCanonicalASTContext()
						: nullptr;
				const asASTDeclId ProbeFunctionDecl =
					ProbeAST != nullptr
						? FindAngelscriptTypedASTJITCanonicalFunctionDecl(
							*ProbeAST, ProbeFunction->name.AddressOf())
						: asASTDeclId();
				const asCDecl* const ProbeCanonicalFunction =
					ProbeAST != nullptr
						? ProbeAST->GetDecl(ProbeFunctionDecl)
						: nullptr;
				if (ProbeAST == nullptr
					|| !ProbeAST->IsPublishable()
					|| ProbeCanonicalFunction == nullptr
					|| !ProbeCanonicalFunction->body.IsValid())
				{
					Result.Error = FString::Printf(
						TEXT("TypedASTJIT %s source compile published no sealed Canonical AST body."),
						Label);
					return false;
				}

				FAngelscriptTypedASTJITFunctionShape ProbeShape;
				ProbeShape.SymbolName = SymbolName;
				ProbeShape.ReturnType = ProbeFunction->returnType;
				ProbeShape.InvocationKind = ProbeFunction->objectType != nullptr
					? EAngelscriptTypedASTJITInvocationKind::InstanceMethod
					: EAngelscriptTypedASTJITInvocationKind::Global;
				ProbeShape.ReceiverKind = ProbeFunction->objectType != nullptr
					? EAngelscriptTypedASTJITReceiverKind::NativeObjectThis
					: EAngelscriptTypedASTJITReceiverKind::None;
				ProbeShape.bReturnsOnStack = ProbeFunction->DoesReturnOnStack();
				for (asUINT Index = 0;
					Index < ProbeFunction->parameterTypes.GetLength(); ++Index)
				{
					ProbeShape.ParameterTypes.Add(
						ProbeFunction->parameterTypes[Index]);
				}

				const FAngelscriptTypedASTJITEmission ProbeEmission =
					EmitTypedASTJITFunction(
						*ProbeAST, ProbeFunctionDecl, ProbeShape, {});
				if (!ProbeEmission.bSuccess)
				{
					Result.Error = FString::Printf(
						TEXT("TypedASTJIT %s fixture unexpectedly fell back: Reason=%u Detail=%s"),
						Label,
						static_cast<uint32>(ProbeEmission.FailureReason),
						*ProbeEmission.FailureDetail);
					return false;
				}

				ScalarProbeRequiredIncludes.Append(
					ProbeEmission.RequiredIncludes);
				ScalarProbeDefinitions += TEXT("\n")
					+ ProbeEmission.Definition;
				return true;
			};

			if (!AppendTypedProbe(
					AngelscriptStaticJITAotFixture::GetModuleName(),
					AngelscriptStaticJITAotFixture::
						GetSemanticMutationParityDeclaration(),
					TEXT("AS_Test_Semantic_SemanticMutationParity"),
					TEXT("mutation"))
				|| !AppendTypedProbe(
					AngelscriptStaticJITAotFixture::GetModuleName(),
					AngelscriptStaticJITAotFixture::
						GetSemanticPowerFloatDeclaration(),
					TEXT("AS_Test_Semantic_SemanticPowerFloat"),
					TEXT("POWf"))
				|| !AppendTypedProbe(
					AngelscriptStaticJITAotFixture::GetModuleName(),
					AngelscriptStaticJITAotFixture::
						GetSemanticPowerDoubleDeclaration(),
					TEXT("AS_Test_Semantic_SemanticPowerDouble"),
					TEXT("POWd"))
				|| !AppendTypedProbe(
					AngelscriptStaticJITAotFixture::GetModuleName(),
					AngelscriptStaticJITAotFixture::
						GetSemanticPowerDoubleIntDeclaration(),
					TEXT("AS_Test_Semantic_SemanticPowerDoubleInt"),
					TEXT("POWdi"))
				|| !AppendTypedProbe(
					AngelscriptStaticJITAotFixture::
						GetTypedControlFlowModuleName(),
					AngelscriptStaticJITAotFixture::
						GetSemanticStructuredControlFlowDeclaration(),
					TEXT("AS_Test_Semantic_SemanticStructuredControlFlow"),
					TEXT("structured control flow"))
				|| !AppendTypedProbe(
					AngelscriptStaticJITAotFixture::
						GetTypedControlFlowModuleName(),
					AngelscriptStaticJITAotFixture::
						GetSemanticExhaustiveEnumDeclaration(),
					TEXT("AS_Test_Semantic_SemanticExhaustiveEnum"),
					TEXT("exhaustive enum switch")))
			{
				return false;
			}

			auto BuildProbeIncludes = [](
				const TConstArrayView<FString> RequiredIncludes,
				const bool bRequireStaticJITHeader)
			{
				TSet<FString> Includes;
				Includes.Add(TEXT("AngelscriptTestJITProbes.h"));
				if (bRequireStaticJITHeader)
				{
					Includes.Add(TEXT("StaticJIT/StaticJITHeader.h"));
				}
				for (const FString& Include : RequiredIncludes)
				{
					Includes.Add(Include);
				}
				TArray<FString> SortedIncludes = Includes.Array();
				SortedIncludes.Sort([](const FString& Left,
					const FString& Right)
				{
					return Left.Compare(Right, ESearchCase::CaseSensitive) < 0;
				});
				FString Directives;
				for (const FString& Include : SortedIncludes)
				{
					Directives += FString::Printf(
						TEXT("#include \"%s\"\n"), *Include);
				}
				return Directives;
			};
			const FString ScalarProbeIncludes =
				BuildProbeIncludes(ScalarProbeRequiredIncludes, false);

			Result.TypedASTGeneratedCpp = FString::Printf(
				TEXT("// @angelscript-jit-owned revision=2 kind=module-source\n")
				TEXT("/*\n")
				TEXT(" * AngelScript TypedASTJIT Test Probe\n")
				TEXT(" * Declarations:\n")
				TEXT(" *   - %s\n")
				TEXT(" *   - %s\n")
				TEXT(" *   - %s\n")
				TEXT(" *   - %s\n")
				TEXT(" *   - %s\n")
				TEXT(" *   - %s\n")
				TEXT(" *   - %s\n")
				TEXT(" * Sources     : /Angelscript/Game/ASStaticJITAotFixture.as\n")
				TEXT(" *               /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as\n")
				TEXT(" * Scope       : test-only; not a Provider entry\n")
				TEXT(" */\n")
				TEXT("%s\n")
				TEXT("namespace\n")
				TEXT("{\n")
				TEXT("%s")
				TEXT("\n\tstruct FTypedASTScalarProbeRegistration\n")
				TEXT("\t{\n")
				TEXT("\t\tFTypedASTScalarProbeRegistration()\n")
				TEXT("\t\t{\n")
				TEXT("\t\t\tAngelscriptTestJITProbes::RegisterTypedASTScalarBranchProbe(\n")
				TEXT("\t\t\t\t&AS_Test_Semantic_SemanticScalarBranch);\n")
				TEXT("\t\t\tAngelscriptTestJITProbes::RegisterTypedASTMutationPowerProbes(\n")
				TEXT("\t\t\t\t&AS_Test_Semantic_SemanticMutationParity,\n")
				TEXT("\t\t\t\t&AS_Test_Semantic_SemanticPowerFloat,\n")
				TEXT("\t\t\t\t&AS_Test_Semantic_SemanticPowerDouble,\n")
				TEXT("\t\t\t\t&AS_Test_Semantic_SemanticPowerDoubleInt);\n")
				TEXT("\t\t\tAngelscriptTestJITProbes::RegisterTypedASTControlFlowProbes(\n")
				TEXT("\t\t\t\t&AS_Test_Semantic_SemanticStructuredControlFlow,\n")
				TEXT("\t\t\t\t&AS_Test_Semantic_SemanticExhaustiveEnum);\n")
				TEXT("\t\t}\n")
				TEXT("\t};\n\n")
				TEXT("\tFTypedASTScalarProbeRegistration GTypedASTScalarProbeRegistration;\n")
				TEXT("}\n"),
				*AngelscriptStaticJITAotFixture::
					GetSemanticScalarBranchDeclaration(),
				*AngelscriptStaticJITAotFixture::
					GetSemanticMutationParityDeclaration(),
				*AngelscriptStaticJITAotFixture::
					GetSemanticPowerFloatDeclaration(),
				*AngelscriptStaticJITAotFixture::
					GetSemanticPowerDoubleDeclaration(),
				*AngelscriptStaticJITAotFixture::
					GetSemanticPowerDoubleIntDeclaration(),
				*AngelscriptStaticJITAotFixture::
					GetSemanticStructuredControlFlowDeclaration(),
				*AngelscriptStaticJITAotFixture::
					GetSemanticExhaustiveEnumDeclaration(),
				*ScalarProbeIncludes,
				*ScalarProbeDefinitions);

			const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
				ESPMode::ThreadSafe> GenerationSnapshot =
					Engine.GetStaticJITGenerationSnapshot();
			auto RequireProductionTypedASTResult = [&Engine,
				&GenerationSnapshot,
				&BackendFunctionResults,
				&InOutOutput,
				bObserveEmitterFailureFallback,
				&Result](
					const FName& ModuleName,
					const FString& Declaration,
					const TCHAR* Label,
					const FString& ReceiverTypeName)
			{
				const TSharedPtr<FAngelscriptModuleDesc> RequiredModule =
					Engine.GetModuleByModuleName(ModuleName.ToString());
				FTCHARToUTF8 DeclarationUtf8(*Declaration);
				asCScriptFunction* RequiredFunction = nullptr;
				if (RequiredModule.IsValid()
					&& RequiredModule->ScriptModule != nullptr)
				{
					if (ReceiverTypeName.IsEmpty())
					{
						RequiredFunction = static_cast<asCScriptFunction*>(
							RequiredModule->ScriptModule->GetFunctionByDecl(
								DeclarationUtf8.Get()));
					}
					else
					{
						FTCHARToUTF8 ReceiverTypeNameUtf8(*ReceiverTypeName);
						asITypeInfo* const ReceiverType =
							RequiredModule->ScriptModule->GetTypeInfoByName(
								ReceiverTypeNameUtf8.Get());
						RequiredFunction = ReceiverType != nullptr
							? static_cast<asCScriptFunction*>(
								ReceiverType->GetMethodByDecl(
									DeclarationUtf8.Get()))
							: nullptr;
					}
				}
				const FAngelscriptStaticJITGenerationFunction* const
					CapturedRequiredFunction = RequiredFunction != nullptr
						&& GenerationSnapshot.IsValid()
					? GenerationSnapshot->Functions.FindByPredicate(
						[RequiredFunction](const auto& Candidate)
						{
							return Candidate.EngineLocalFunction
								== RequiredFunction;
						})
					: nullptr;
				const FAngelscriptStaticJITFunctionResult* const BackendResult =
					CapturedRequiredFunction != nullptr
					? BackendFunctionResults.FindByPredicate(
						[CapturedRequiredFunction](const auto& Candidate)
						{
							return Candidate.FunctionKey
								== CapturedRequiredFunction->FunctionKey;
						})
					: nullptr;
				if (bObserveEmitterFailureFallback
					&& !Result.EmitterFailureFallback.bCaptured)
				{
					FStaticJITAotEmitterFailureFallbackObservation& Observation =
						Result.EmitterFailureFallback;
					const FAngelscriptStaticJITBackendAttemptDiagnostic* const
						TypedAttempt = BackendResult != nullptr
						? BackendResult->BackendAttempts.FindByPredicate(
							[](const auto& Attempt)
							{
								return Attempt.BackendId
									== FAngelscriptStaticJITBackendId::TypedAST();
							})
						: nullptr;
					const FAngelscriptStaticJITBackendAttemptDiagnostic* const
						BytecodeAttempt = BackendResult != nullptr
						? BackendResult->BackendAttempts.FindByPredicate(
							[](const auto& Attempt)
							{
								return Attempt.BackendId
									== FAngelscriptStaticJITBackendId::Bytecode();
							})
						: nullptr;
					const FAngelscriptJITGeneratedFunction* const
						GeneratedFunction = CapturedRequiredFunction != nullptr
						? InOutOutput.Functions.FindByPredicate(
							[CapturedRequiredFunction](const auto& Candidate)
							{
								return Candidate.FunctionKey
									== CapturedRequiredFunction->FunctionKey;
							})
						: nullptr;

					Observation.bCaptured = RequiredFunction != nullptr
						&& CapturedRequiredFunction != nullptr
						&& BackendResult != nullptr
						&& TypedAttempt != nullptr;
					Observation.FunctionKey = CapturedRequiredFunction != nullptr
						? CapturedRequiredFunction->FunctionKey.Hash.ToHexString()
						: FString();
					Observation.FinalBackendId = BackendResult != nullptr
						? BackendResult->ActualBackendId.ToString() : FString();
					Observation.bFinalFunctionEmitted = BackendResult != nullptr
						&& BackendResult->Disposition
							== EAngelscriptStaticJITFunctionDisposition::Emitted;
					Observation.bBytecodeAttemptEmitted = BytecodeAttempt != nullptr
						&& BytecodeAttempt->Disposition
							== EAngelscriptStaticJITFunctionDisposition::Emitted;
					Observation.BytecodeAttemptDetail = BytecodeAttempt != nullptr
						? BytecodeAttempt->Detail : FString();
					if (TypedAttempt != nullptr)
					{
						Observation.TypedAttemptDetail = TypedAttempt->Detail;
						if (TypedAttempt->TypedDiagnostic.IsSet())
						{
							const FAngelscriptStaticJITTypedFunctionDiagnostic&
								TypedDiagnostic = TypedAttempt->TypedDiagnostic.GetValue();
							Observation.TypedAttemptDetail = FString::Printf(
								TEXT("%s: %s"),
								LexToString(TypedDiagnostic.FallbackReason),
								*TypedDiagnostic.Detail);
							Observation.bTypedEligibilityPassed =
								TypedDiagnostic.CallClosure.IsSet()
								&& TypedDiagnostic.CallClosure->bEligible;
						}
					}
					if (CapturedRequiredFunction != nullptr)
					{
						for (const FAngelscriptJITGeneratedFunction& Candidate
							: InOutOutput.Functions)
						{
							if (Candidate.FunctionKey
								== CapturedRequiredFunction->FunctionKey)
							{
								++Observation.ProviderFunctionCountForKey;
							}
						}
					}
					if (GeneratedFunction != nullptr)
					{
						const FAngelscriptJITGeneratedFile* const ModuleSource =
							InOutOutput.Files.FindByPredicate(
								[GeneratedFunction](const auto& Candidate)
								{
									return Candidate.RelativePath
										== GeneratedFunction->ModuleSourceRelativePath;
								});
						Observation.bTypedBodyPresent = ModuleSource != nullptr
							&& ModuleSource->Content.Contains(
								GeneratedFunction->SymbolPrefix + TEXT("_TypedBody"),
								ESearchCase::CaseSensitive);
					}
				}
				if (BackendResult != nullptr
					&& BackendResult->Disposition
						== EAngelscriptStaticJITFunctionDisposition::Emitted
					&& BackendResult->ActualBackendId
						== FAngelscriptStaticJITBackendId::TypedAST())
				{
					return true;
				}

				TArray<FString> AttemptDetails;
				if (BackendResult != nullptr)
				{
					for (const FAngelscriptStaticJITBackendAttemptDiagnostic& Attempt
						: BackendResult->BackendAttempts)
					{
						const FString TypedFallback =
							Attempt.TypedDiagnostic.IsSet()
							? FString::Printf(TEXT(":%s"),
								LexToString(Attempt.TypedDiagnostic->FallbackReason))
							: FString();
						AttemptDetails.Add(FString::Printf(
							TEXT("%s:%u%s:%s"),
							*Attempt.BackendId.ToString(),
							static_cast<uint32>(Attempt.Disposition),
							*TypedFallback,
							*Attempt.Detail));
					}
				}
				Result.Error = FString::Printf(TEXT(
					"%s did not use the production TypedAST backend: Function=%s Backend=%s Disposition=%u Detail=%s Attempts=[%s]"),
					Label,
					RequiredFunction != nullptr
						? UTF8_TO_TCHAR(RequiredFunction->GetDeclaration())
						: TEXT("<missing>"),
					BackendResult != nullptr
						? *BackendResult->ActualBackendId.ToString()
						: TEXT("<missing>"),
					BackendResult != nullptr
						? static_cast<uint32>(BackendResult->Disposition)
						: MAX_uint32,
					BackendResult != nullptr
						? *BackendResult->Detail : TEXT("<missing>"),
					*FString::Join(AttemptDetails, TEXT(" | ")));
				return false;
			};
			if (!RequireProductionTypedASTResult(
					AngelscriptStaticJITAotFixture::
						GetTypedControlFlowModuleName(),
					AngelscriptStaticJITAotFixture::
						GetSemanticStructuredControlFlowDeclaration(),
					TEXT("TypedASTJIT structured-control-flow UFUNCTION fixture"),
					FString())
				|| !RequireProductionTypedASTResult(
					AngelscriptStaticJITAotFixture::
						GetTypedControlFlowModuleName(),
					AngelscriptStaticJITAotFixture::
						GetSemanticExhaustiveEnumDeclaration(),
					TEXT("TypedASTJIT exhaustive-enum UFUNCTION fixture"),
					FString())
				|| !RequireProductionTypedASTResult(
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarModuleName(),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarRawDeclaration(),
					TEXT("TypedASTJIT final scalar Raw UFUNCTION fixture"),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarReceiverClassName().ToString())
				|| !RequireProductionTypedASTResult(
					AngelscriptStaticJITAotFixture::GetTypedRecursionModuleName(),
					AngelscriptStaticJITAotFixture::
						GetTypedASTSelfRecursionDeclaration(),
					TEXT("TypedASTJIT self-recursion AOT fixture"),
					FString())
				|| !RequireProductionTypedASTResult(
					AngelscriptStaticJITAotFixture::GetTypedRecursionModuleName(),
					AngelscriptStaticJITAotFixture::
						GetTypedASTMutualRecursionDeclaration(),
					TEXT("TypedASTJIT mutual-recursion AOT fixture"),
					FString())
				|| !RequireProductionTypedASTResult(
					AngelscriptStaticJITAotFixture::GetTypedRecursionModuleName(),
					AngelscriptStaticJITAotFixture::
						GetTypedASTExceptionEntryDeclaration(),
					TEXT("TypedASTJIT checked-exception public entry fixture"),
					FString()))
			{
				return false;
			}
			const FAngelscriptStaticJITGenerationFunction* const
				CapturedFunction = GenerationSnapshot.IsValid()
					? GenerationSnapshot->Functions.FindByPredicate(
						[Function](const auto& Candidate)
						{
							return Candidate.EngineLocalFunction == Function;
						})
					: nullptr;
			if (CapturedFunction == nullptr
				|| !CapturedFunction->bHasVMEntry
				|| !CapturedFunction->bHasRawEntry
				|| !CapturedFunction->bHasParmsEntry)
			{
				Result.Error = TEXT(
					"TypedASTJIT Provider probe requires the exact frozen raw, VM, and Parms entry set.");
				return false;
			}
			const FAngelscriptJITGeneratedFunction* const GeneratedScalarFunction =
				InOutOutput.Functions.FindByPredicate(
					[CapturedFunction](const auto& Candidate)
					{
						return Candidate.FunctionKey
							== CapturedFunction->FunctionKey;
					});
			if (GeneratedScalarFunction == nullptr
				|| GeneratedScalarFunction->CanonicalDeclaration.IsEmpty()
				|| !GeneratedScalarFunction->SourceSection.StartsWith(
					TEXT("/Angelscript/"), ESearchCase::CaseSensitive)
				|| GeneratedScalarFunction->SourceLine == 0)
			{
				TArray<FString> DeclarationCandidates;
				for (const FAngelscriptJITGeneratedFunction& Candidate
					: InOutOutput.Functions)
				{
					if (Candidate.CanonicalDeclaration.Contains(
							TEXT("SemanticScalarBranch"),
							ESearchCase::CaseSensitive))
					{
						DeclarationCandidates.Add(FString::Printf(TEXT(
							"Key=%s Declaration='%s' Source='%s' Line=%u"),
							*Candidate.FunctionKey.Hash.ToHexString(),
							*Candidate.CanonicalDeclaration,
							*Candidate.SourceSection,
							Candidate.SourceLine));
					}
				}
				Result.Error = FString::Printf(TEXT(
					"TypedASTJIT Provider probe requires the authoritative generated declaration and logical source metadata. CapturedKey=%s OutputFunctionCount=%d ExactMatch=%s DeclarationCandidates=[%s]"),
					*CapturedFunction->FunctionKey.Hash.ToHexString(),
					InOutOutput.Functions.Num(),
					GeneratedScalarFunction != nullptr
						? *FString::Printf(TEXT(
							"Declaration='%s' Source='%s' Line=%u"),
							*GeneratedScalarFunction->CanonicalDeclaration,
							*GeneratedScalarFunction->SourceSection,
							GeneratedScalarFunction->SourceLine)
						: TEXT("<missing>"),
					*FString::Join(DeclarationCandidates, TEXT(" | ")));
				return false;
			}

			FAngelscriptTypedASTJITFunctionShape ProviderShape = Shape;
			ProviderShape.SymbolName = TEXT("{SYMBOL_PREFIX}_TypedBody");
			FAngelscriptStaticJITGenerationProfile ProviderGenerationProfile;
			if (!FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::TypedAST(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				ProviderGenerationProfile,
				&Result.Error))
			{
				return false;
			}
			FAngelscriptTypedASTJITEmitOptions ProviderEmitOptions;
			ProviderEmitOptions.InstrumentationProfile =
				&ProviderGenerationProfile.TypedASTJITCapabilityProfile;
			const FAngelscriptTypedASTJITEmission ProviderBody =
				EmitTypedASTJITFunction(
					*CanonicalAST,
					CanonicalFunctionDecl,
					ProviderShape,
					ProviderEmitOptions);
			FAngelscriptTypedASTJITProviderEntryPlan EntryPlan;
			FString ProviderError;
			if (!ProviderBody.bSuccess
				|| !BuildAngelscriptTypedASTJITProviderEntryPlan(
					*Function,
					CapturedFunction->bHasVMEntry,
					CapturedFunction->bHasRawEntry,
					CapturedFunction->bHasParmsEntry,
					EntryPlan, &ProviderError))
			{
				Result.Error = ProviderBody.bSuccess
					? MoveTemp(ProviderError)
					: ProviderBody.FailureDetail;
				return false;
			}
			EntryPlan.CanonicalDeclaration =
				GeneratedScalarFunction->CanonicalDeclaration;
			EntryPlan.SourceSection = GeneratedScalarFunction->SourceSection;
			EntryPlan.SourceLine = GeneratedScalarFunction->SourceLine;
			EntryPlan.InstrumentationProfile =
				ProviderGenerationProfile.TypedASTJITCapabilityProfile;
			FAngelscriptTypedASTJITProviderEmission ProviderEmission =
				EmitAngelscriptTypedASTJITProviderFunction(
					EntryPlan, ProviderBody);
			if (!ProviderEmission.bSuccess)
			{
				Result.Error = MoveTemp(ProviderEmission.FailureDetail);
				return false;
			}
			const FString ProviderProbeIncludes = BuildProbeIncludes(
				ProviderEmission.RequiredIncludes, true);

			const FString ProviderSymbolPrefix =
				TEXT("AS_Test_TypedASTProvider_SemanticScalarBranch");
			const FString RawSymbol = ProviderSymbolPrefix + TEXT("_Raw");
			const FString VMSymbol = ProviderSymbolPrefix + TEXT("_VM");
			const FString ParmsSymbol = ProviderSymbolPrefix + TEXT("_Parms");
			ProviderEmission.ImplementationTemplate.ReplaceInline(
				TEXT("{SYMBOL_PREFIX}"), *ProviderSymbolPrefix,
				ESearchCase::CaseSensitive);
			ProviderEmission.ImplementationTemplate.ReplaceInline(
				TEXT("{RAW_SYMBOL}"), *RawSymbol,
				ESearchCase::CaseSensitive);
			ProviderEmission.ImplementationTemplate.ReplaceInline(
				TEXT("{VM_SYMBOL}"), *VMSymbol,
				ESearchCase::CaseSensitive);
			ProviderEmission.ImplementationTemplate.ReplaceInline(
				TEXT("{PARMS_SYMBOL}"), *ParmsSymbol,
				ESearchCase::CaseSensitive);
			ProviderEmission.ImplementationTemplate.ReplaceInline(
				TEXT("{RAW_ENTRY_COMMENT}"),
				TEXT("// AS Function : int SemanticScalarBranch(const int, const int)\n// JIT Entry   : Raw\n"),
				ESearchCase::CaseSensitive);
			ProviderEmission.ImplementationTemplate.ReplaceInline(
				TEXT("{VM_ENTRY_COMMENT}"),
				TEXT("// AS Function : int SemanticScalarBranch(const int, const int)\n// JIT Entry   : VM\n"),
				ESearchCase::CaseSensitive);
			ProviderEmission.ImplementationTemplate.ReplaceInline(
				TEXT("{PARMS_ENTRY_COMMENT}"),
				TEXT("// AS Function : int SemanticScalarBranch(const int, const int)\n// JIT Entry   : Parms\n"),
				ESearchCase::CaseSensitive);

			FString ProviderProbeCpp = FString::Printf(
				TEXT("// @angelscript-jit-owned revision=2 kind=module-source\n")
				TEXT("/*\n")
				TEXT(" * AngelScript TypedASTJIT Provider Entry Probe\n")
				TEXT(" * Declaration : %s\n")
				TEXT(" * Source      : /Angelscript/Game/ASStaticJITAotFixture.as\n")
				TEXT(" * Entry ABI   : %s\n")
				TEXT(" * Scope       : test-only compiled raw/VM/Parms carrier\n")
				TEXT(" */\n")
				TEXT("%s\n")
				TEXT("namespace\n")
				TEXT("{\n")
				TEXT("%s")
				TEXT("\n\tstruct FTypedASTScalarProviderEntryRegistration\n")
				TEXT("\t{\n")
				TEXT("\t\tFTypedASTScalarProviderEntryRegistration()\n")
				TEXT("\t\t{\n")
				TEXT("\t\t\tAngelscriptTestJITProbes::RegisterTypedASTScalarProviderEntries(\n")
				TEXT("\t\t\t\t&%s,\n")
				TEXT("\t\t\t\treinterpret_cast<asJITFunction_Raw>(&%s),\n")
				TEXT("\t\t\t\t&%s);\n")
				TEXT("\t\t}\n")
				TEXT("\t};\n\n")
				TEXT("\tFTypedASTScalarProviderEntryRegistration GTypedASTScalarProviderEntryRegistration;\n")
				TEXT("}\n"),
				*AngelscriptStaticJITAotFixture::
					GetSemanticScalarBranchDeclaration(),
				*CapturedFunction->EntryAbiHash.ToHexString(),
				*ProviderProbeIncludes,
				*ProviderEmission.ImplementationTemplate,
				*VMSymbol, *RawSymbol, *ParmsSymbol);

			FTCHARToUTF8 CapabilityDeclarationUtf8(
				*AngelscriptStaticJITAotFixture::
					GetTypedASTCapabilityShowcaseDeclaration());
			asCScriptFunction* const CapabilityFunction =
				static_cast<asCScriptFunction*>(
					ModuleDesc->ScriptModule->GetFunctionByDecl(
						CapabilityDeclarationUtf8.Get()));
			if (CapabilityFunction == nullptr)
			{
				Result.Error = TEXT(
					"TypedASTJIT capability probe could not resolve its exact UFUNCTION root.");
				return false;
			}
			const FAngelscriptStaticJITGenerationFunction* const
				CapturedCapabilityFunction = GenerationSnapshot.IsValid()
					? GenerationSnapshot->Functions.FindByPredicate(
						[CapabilityFunction](const auto& Candidate)
						{
							return Candidate.EngineLocalFunction
								== CapabilityFunction;
						})
					: nullptr;
			if (CapturedCapabilityFunction == nullptr)
			{
				Result.Error = TEXT(
					"TypedASTJIT capability probe is absent from the frozen generation snapshot.");
				return false;
			}

			const FAngelscriptStaticJITFunctionResult* const
				CapabilityBackendResult = BackendFunctionResults.FindByPredicate(
					[CapturedCapabilityFunction](const auto& Candidate)
					{
						return Candidate.FunctionKey
							== CapturedCapabilityFunction->FunctionKey;
					});
			if (CapabilityBackendResult == nullptr
				|| CapabilityBackendResult->Disposition
					!= EAngelscriptStaticJITFunctionDisposition::Emitted
				|| CapabilityBackendResult->ActualBackendId
					!= FAngelscriptStaticJITBackendId::TypedAST())
			{
				TArray<FString> AttemptDetails;
				TArray<FString> CanonicalDeclCandidates;
				if (CapturedCapabilityFunction->SealedAST != nullptr)
				{
					const asCASTContext& SealedAST =
						*CapturedCapabilityFunction->SealedAST;
					for (asUINT DeclIndex = 1;
						DeclIndex <= SealedAST.GetDeclCount();
						++DeclIndex)
					{
						const asCDecl* const Decl =
							SealedAST.GetDecl(asASTDeclId(DeclIndex));
						if (Decl != nullptr
							&& Decl->name.Equals(CapabilityFunction->name))
						{
							const asCDecl* const Parent =
								SealedAST.GetDecl(Decl->parent);
							CanonicalDeclCandidates.Add(FString::Printf(
								TEXT("id=%u kind=%u parent=%u parentKey='%s' file=%u begin=%u end=%u body=%u traits=0x%08x key='%s'"),
								Decl->id.value,
								static_cast<uint32>(Decl->kind),
								Decl->parent.value,
								Parent != nullptr
									? UTF8_TO_TCHAR(Parent->stableKey.AddressOf())
									: TEXT(""),
								Decl->range.begin.fileID,
								Decl->range.begin.offset,
								Decl->range.end.offset,
								Decl->body.value,
								Decl->traits,
								UTF8_TO_TCHAR(Decl->stableKey.AddressOf())));
						}
					}
				}
				if (CapabilityBackendResult != nullptr)
				{
					for (const FAngelscriptStaticJITBackendAttemptDiagnostic& Attempt
						: CapabilityBackendResult->BackendAttempts)
					{
						AttemptDetails.Add(FString::Printf(
							TEXT("%s:%u:%s"),
							*Attempt.BackendId.ToString(),
							static_cast<uint32>(Attempt.Disposition),
							*Attempt.Detail));
					}
				}
				Result.Error = CapabilityBackendResult == nullptr
					? TEXT(
						"TypedASTJIT capability probe has no backend result.")
					: FString::Printf(TEXT(
						"TypedASTJIT capability probe did not use the production TypedAST backend: Backend=%s Disposition=%u Detail=%s Attempts=[%s] CanonicalDeclId=%u RuntimeStableKey='%s' CanonicalCandidates=[%s]"),
						*CapabilityBackendResult->ActualBackendId.ToString(),
						static_cast<uint32>(
							CapabilityBackendResult->Disposition),
						*CapabilityBackendResult->Detail,
						*FString::Join(AttemptDetails, TEXT(" | ")),
						CapturedCapabilityFunction->CanonicalFunctionDeclValue,
						UTF8_TO_TCHAR(CapabilityFunction->
							canonicalASTStableDeclKey.AddressOf()),
						*FString::Join(CanonicalDeclCandidates, TEXT(" | ")));
				return false;
			}

			const FAngelscriptJITGeneratedFunction* const
				GeneratedCapabilityFunction =
					InOutOutput.Functions.FindByPredicate(
						[CapturedCapabilityFunction](const auto& Candidate)
						{
							return Candidate.FunctionKey
								== CapturedCapabilityFunction->FunctionKey;
						});
			if (GeneratedCapabilityFunction == nullptr
				|| !GeneratedCapabilityFunction->bHasVMEntry
				|| !GeneratedCapabilityFunction->bHasRawEntry
				|| !GeneratedCapabilityFunction->bHasParmsEntry
				|| GeneratedCapabilityFunction->RawEntryDeclaration.IsEmpty())
			{
				Result.Error = TEXT(
					"TypedASTJIT capability probe requires the production raw, VM, and Parms entry set.");
				return false;
			}
			const FAngelscriptJITGeneratedFile* const CapabilityModuleSource =
				InOutOutput.Files.FindByPredicate(
					[GeneratedCapabilityFunction](const auto& Candidate)
					{
						return Candidate.RelativePath
							== GeneratedCapabilityFunction->
								ModuleSourceRelativePath;
					});
			const FString CapabilityTypedBodySymbol =
				GeneratedCapabilityFunction->SymbolPrefix + TEXT("_TypedBody");
			if (CapabilityModuleSource == nullptr
				|| !CapabilityModuleSource->Content.Contains(
					CapabilityTypedBodySymbol, ESearchCase::CaseSensitive)
				|| !CapabilityModuleSource->Content.Contains(
					TEXT("IsRunningCommandlet()"),
					ESearchCase::CaseSensitive)
				|| CapabilityModuleSource->Content.Contains(
					TEXT("FAngelscriptJITExecutionContext"),
					ESearchCase::CaseSensitive))
			{
				Result.Error = TEXT(
					"TypedASTJIT capability probe production source is missing its typed body/direct native call or retained a forbidden execution context.");
				return false;
			}

			FTCHARToUTF8 ExportedSymbolDeclarationUtf8(
				*AngelscriptStaticJITAotFixture::
					GetTypedASTExportedSymbolShowcaseDeclaration());
			asCScriptFunction* const ExportedSymbolFunction =
				static_cast<asCScriptFunction*>(
					ModuleDesc->ScriptModule->GetFunctionByDecl(
						ExportedSymbolDeclarationUtf8.Get()));
			const FAngelscriptStaticJITGenerationFunction* const
				CapturedExportedSymbolFunction =
					ExportedSymbolFunction != nullptr && GenerationSnapshot.IsValid()
						? GenerationSnapshot->Functions.FindByPredicate(
							[ExportedSymbolFunction](const auto& Candidate)
							{
								return Candidate.EngineLocalFunction
									== ExportedSymbolFunction;
							})
						: nullptr;
			const FAngelscriptStaticJITFunctionResult* const
				ExportedSymbolBackendResult =
					CapturedExportedSymbolFunction != nullptr
						? BackendFunctionResults.FindByPredicate(
							[CapturedExportedSymbolFunction](
								const auto& Candidate)
							{
								return Candidate.FunctionKey
									== CapturedExportedSymbolFunction->FunctionKey;
							})
						: nullptr;
			if (ExportedSymbolBackendResult == nullptr
				|| ExportedSymbolBackendResult->Disposition
					!= EAngelscriptStaticJITFunctionDisposition::Emitted
				|| ExportedSymbolBackendResult->ActualBackendId
					!= FAngelscriptStaticJITBackendId::TypedAST())
			{
				TArray<FString> AttemptDetails;
				if (ExportedSymbolBackendResult != nullptr)
				{
					for (const FAngelscriptStaticJITBackendAttemptDiagnostic& Attempt
						: ExportedSymbolBackendResult->BackendAttempts)
					{
						const FString TypedFallback = Attempt.TypedDiagnostic.IsSet()
							? FString::Printf(
								TEXT(":%s"),
								LexToString(Attempt.TypedDiagnostic->FallbackReason))
							: FString();
						AttemptDetails.Add(FString::Printf(
							TEXT("%s:%u%s:%s"),
							*Attempt.BackendId.ToString(),
							static_cast<uint32>(Attempt.Disposition),
							*TypedFallback,
							*Attempt.Detail));
					}
				}
				TArray<FString> MatchingInventory;
				TArray<FString> MatchingTargets;
				if (GenerationSnapshot.IsValid())
				{
					for (const FAngelscriptStaticJITNativeCallInventoryEntry& Entry
						: GenerationSnapshot->NativeCallInventory)
					{
						if (!Entry.CanonicalDeclaration.Contains(TEXT("DaysInMonth"))
							&& !Entry.NativeCallableDisplay.Contains(TEXT("DaysInMonth")))
						{
							continue;
						}
						MatchingInventory.Add(FString::Printf(
							TEXT("id=%d decl='%s' ns='%s' receiver='%s' descriptor=%s native='%s' stableRef=%s nativeABI=%s nativeABIError='%s' scalarABI=%s scalarABIError='%s'"),
							Entry.EngineLocalFunctionId,
							*Entry.CanonicalDeclaration,
							*Entry.NamespaceCanonicalName,
							*Entry.ReceiverCanonicalName,
							Entry.bHasExternalDescriptor ? TEXT("yes") : TEXT("no"),
							*Entry.NativeCallableDisplay,
							Entry.bHasStableReference ? TEXT("yes") : TEXT("no"),
							Entry.bHasExpectedNativeABI ? TEXT("yes") : TEXT("no"),
							*Entry.NativeABIError,
							Entry.bHasExpectedScalarABI ? TEXT("yes") : TEXT("no"),
							*Entry.ScalarABIError));
					}
					for (const FAngelscriptStaticJITNativeCallTarget& Target
						: GenerationSnapshot->NativeCallTargets)
					{
						if (Target.CanonicalDeclaration.Contains(TEXT("DaysInMonth"))
							|| Target.RegisteredTargetDisplay.Contains(TEXT("DaysInMonth")))
						{
							MatchingTargets.Add(FString::Printf(
								TEXT("id=%d decl='%s' target='%s' symbol='%s'"),
								Target.EngineLocalFunctionId,
								*Target.CanonicalDeclaration,
								*Target.RegisteredTargetDisplay,
								*Target.Descriptor.Symbol));
						}
					}
				}
				Result.Error = FString::Printf(TEXT(
					"TypedASTJIT Core exported-symbol probe did not use the production TypedAST backend: Function=%s Backend=%s Disposition=%u Detail=%s Attempts=[%s] CanonicalDeclId=%u RuntimeStableKey='%s' MatchingInventory=[%s] MatchingTargets=[%s]"),
					ExportedSymbolFunction != nullptr
						? UTF8_TO_TCHAR(
							ExportedSymbolFunction->GetDeclaration())
						: TEXT("<missing>"),
					ExportedSymbolBackendResult != nullptr
						? *ExportedSymbolBackendResult->ActualBackendId.ToString()
						: TEXT("<missing>"),
					ExportedSymbolBackendResult != nullptr
						? static_cast<uint32>(
							ExportedSymbolBackendResult->Disposition)
						: MAX_uint32,
					ExportedSymbolBackendResult != nullptr
						? *ExportedSymbolBackendResult->Detail
						: TEXT("<missing>"),
					*FString::Join(AttemptDetails, TEXT(" | ")),
					CapturedExportedSymbolFunction != nullptr
						? CapturedExportedSymbolFunction->CanonicalFunctionDeclValue
						: 0,
					ExportedSymbolFunction != nullptr
						? UTF8_TO_TCHAR(ExportedSymbolFunction->
							canonicalASTStableDeclKey.AddressOf())
						: TEXT("<missing>"),
					*FString::Join(MatchingInventory, TEXT(" | ")),
					*FString::Join(MatchingTargets, TEXT(" | ")));
				return false;
			}

			const FAngelscriptJITGeneratedFunction* const
				GeneratedExportedSymbolFunction =
					InOutOutput.Functions.FindByPredicate(
						[CapturedExportedSymbolFunction](const auto& Candidate)
						{
							return Candidate.FunctionKey
								== CapturedExportedSymbolFunction->FunctionKey;
						});
			const FAngelscriptJITGeneratedFile* const
				ExportedSymbolModuleSource =
					GeneratedExportedSymbolFunction != nullptr
						? InOutOutput.Files.FindByPredicate(
							[GeneratedExportedSymbolFunction](
								const auto& Candidate)
							{
								return Candidate.RelativePath
									== GeneratedExportedSymbolFunction->
										ModuleSourceRelativePath;
							})
						: nullptr;
			const FAngelscriptJITGenerationNativeCallSite* const
				ExportedSymbolCallSite =
					GeneratedExportedSymbolFunction != nullptr
						? GeneratedExportedSymbolFunction->NativeCallSites.
							FindByPredicate([](const auto& Candidate)
							{
								return Candidate.Route
									== EAngelscriptJITNativeCallSiteRoute::
										CurrentNativeBinding
									&& Candidate.EmittedCppCallee.StartsWith(TEXT(
										"AngelscriptTypedASTJIT::InvokeBoundNative<"));
							})
						: nullptr;
			if (GeneratedExportedSymbolFunction == nullptr
				|| !GeneratedExportedSymbolFunction->bHasVMEntry
				|| !GeneratedExportedSymbolFunction->bHasRawEntry
				|| !GeneratedExportedSymbolFunction->bHasParmsEntry
				|| ExportedSymbolModuleSource == nullptr
				|| !ExportedSymbolModuleSource->Content.Contains(TEXT(
					"#include \"StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h\""),
					ESearchCase::CaseSensitive)
				|| !ExportedSymbolModuleSource->Content.Contains(
					TEXT("AngelscriptTypedASTJIT::InvokeBoundNative<"),
					ESearchCase::CaseSensitive)
				|| ExportedSymbolModuleSource->Content.Contains(
					TEXT("FAngelscriptJITExecutionContext"),
					ESearchCase::CaseSensitive)
				|| ExportedSymbolCallSite == nullptr
				|| ExportedSymbolCallSite->RegisteredTarget
					!= TEXT("FDateTime::DaysInMonth")
				|| ExportedSymbolCallSite->RuntimeDllCore
					!= TEXT("AngelscriptTypedASTJIT::ResolveBoundNative")
				|| ExportedSymbolCallSite->MetadataRowSymbol.IsEmpty()
				|| ExportedSymbolCallSite->StableTargetKey.IsZero()
				|| ExportedSymbolCallSite->ExpectedAbi.IsZero()
				|| ExportedSymbolCallSite->ReferenceSlotIndex < 0)
			{
				Result.Error = TEXT(
					"TypedASTJIT Core exported-symbol production source is missing its current-Engine FDateTime::DaysInMonth native slot/call-site mapping, entry set, or retained a frozen direct target.");
				return false;
			}

			const TSharedPtr<FAngelscriptModuleDesc> PrintModuleDesc =
				Engine.GetModuleByModuleName(
					AngelscriptStaticJITAotFixture::
						GetTypedPrintModuleName().ToString());
			asCScriptFunction* const PrintFunction =
				PrintModuleDesc.IsValid()
					&& PrintModuleDesc->ScriptModule != nullptr
					? static_cast<asCScriptFunction*>(
						PrintModuleDesc->ScriptModule->GetFunctionByName(
							"TypedASTPrintShowcase"))
					: nullptr;
			const FAngelscriptStaticJITGenerationFunction* const
				CapturedPrintFunction =
					PrintFunction != nullptr && GenerationSnapshot.IsValid()
						? GenerationSnapshot->Functions.FindByPredicate(
							[PrintFunction](const auto& Candidate)
							{
								return Candidate.EngineLocalFunction
									== PrintFunction;
							})
						: nullptr;
			const FAngelscriptStaticJITFunctionResult* const
				PrintBackendResult =
					CapturedPrintFunction != nullptr
						? BackendFunctionResults.FindByPredicate(
							[CapturedPrintFunction](const auto& Candidate)
							{
								return Candidate.FunctionKey
									== CapturedPrintFunction->FunctionKey;
							})
						: nullptr;
			if (PrintBackendResult == nullptr
				|| PrintBackendResult->Disposition
					!= EAngelscriptStaticJITFunctionDisposition::Emitted
				|| PrintBackendResult->ActualBackendId
					!= FAngelscriptStaticJITBackendId::TypedAST())
			{
				TArray<FString> AttemptDetails;
				if (PrintBackendResult != nullptr)
				{
					for (const FAngelscriptStaticJITBackendAttemptDiagnostic& Attempt
						: PrintBackendResult->BackendAttempts)
					{
						AttemptDetails.Add(FString::Printf(
							TEXT("%s:%u:%s"),
							*Attempt.BackendId.ToString(),
							static_cast<uint32>(Attempt.Disposition),
							*Attempt.Detail));
					}
				}
				Result.Error = FString::Printf(TEXT(
					"TypedASTJIT Print probe did not use the production TypedAST backend: Function=%s Backend=%s Disposition=%u Detail=%s Attempts=[%s]"),
					PrintFunction != nullptr
						? UTF8_TO_TCHAR(PrintFunction->GetDeclaration())
						: TEXT("<missing>"),
					PrintBackendResult != nullptr
						? *PrintBackendResult->ActualBackendId.ToString()
						: TEXT("<missing>"),
					PrintBackendResult != nullptr
						? static_cast<uint32>(PrintBackendResult->Disposition)
						: MAX_uint32,
					PrintBackendResult != nullptr
						? *PrintBackendResult->Detail : TEXT("<missing>"),
					*FString::Join(AttemptDetails, TEXT(" | ")));
				return false;
			}
			const FAngelscriptJITGeneratedFunction* const GeneratedPrintFunction =
				InOutOutput.Functions.FindByPredicate(
					[CapturedPrintFunction](const auto& Candidate)
					{
						return Candidate.FunctionKey
							== CapturedPrintFunction->FunctionKey;
					});
			const FAngelscriptJITGeneratedFile* const PrintModuleSource =
				GeneratedPrintFunction != nullptr
					? InOutOutput.Files.FindByPredicate(
						[GeneratedPrintFunction](const auto& Candidate)
						{
							return Candidate.RelativePath
								== GeneratedPrintFunction->ModuleSourceRelativePath;
						})
					: nullptr;
			if (GeneratedPrintFunction == nullptr
				|| PrintModuleSource == nullptr
				|| !PrintModuleSource->Content.Contains(
					TEXT("AngelscriptStaticJITNativeCallables::Print("),
					ESearchCase::CaseSensitive)
				|| PrintModuleSource->Content.Contains(
					TEXT("FAngelscriptJITExecutionContext"),
					ESearchCase::CaseSensitive))
			{
				Result.Error = TEXT(
					"TypedASTJIT Print production source is missing its exact exported Runtime call or retained a forbidden execution context.");
				return false;
			}

			FTCHARToUTF8 PrivateBridgeDeclarationUtf8(
				*AngelscriptStaticJITAotFixture::
					GetTypedASTPrivateBridgeShowcaseDeclaration());
			const TSharedPtr<FAngelscriptModuleDesc> PrivateBridgeModuleDesc =
				Engine.GetModuleByModuleName(
					AngelscriptStaticJITAotFixture::
						GetTypedPrivateBridgeModuleName().ToString());
			asCScriptFunction* const PrivateBridgeFunction =
				PrivateBridgeModuleDesc.IsValid()
					&& PrivateBridgeModuleDesc->ScriptModule != nullptr
					? static_cast<asCScriptFunction*>(
						PrivateBridgeModuleDesc->ScriptModule->GetFunctionByDecl(
							PrivateBridgeDeclarationUtf8.Get()))
					: nullptr;
			const FAngelscriptStaticJITGenerationFunction* const
				CapturedPrivateBridgeFunction =
					PrivateBridgeFunction != nullptr && GenerationSnapshot.IsValid()
						? GenerationSnapshot->Functions.FindByPredicate(
							[PrivateBridgeFunction](const auto& Candidate)
							{
								return Candidate.EngineLocalFunction
									== PrivateBridgeFunction;
							})
						: nullptr;
			const FAngelscriptStaticJITFunctionResult* const
				PrivateBridgeBackendResult =
					CapturedPrivateBridgeFunction != nullptr
						? BackendFunctionResults.FindByPredicate(
							[CapturedPrivateBridgeFunction](const auto& Candidate)
							{
								return Candidate.FunctionKey
									== CapturedPrivateBridgeFunction->FunctionKey;
							})
						: nullptr;
			if (PrivateBridgeBackendResult == nullptr
				|| PrivateBridgeBackendResult->Disposition
					!= EAngelscriptStaticJITFunctionDisposition::Emitted
				|| PrivateBridgeBackendResult->ActualBackendId
					!= FAngelscriptStaticJITBackendId::TypedAST())
			{
				TArray<FString> AttemptDetails;
				if (PrivateBridgeBackendResult != nullptr)
				{
					for (const FAngelscriptStaticJITBackendAttemptDiagnostic& Attempt
						: PrivateBridgeBackendResult->BackendAttempts)
					{
						AttemptDetails.Add(FString::Printf(
							TEXT("%s:%u:%s"),
							*Attempt.BackendId.ToString(),
							static_cast<uint32>(Attempt.Disposition),
							*Attempt.Detail));
					}
				}
				Result.Error = FString::Printf(TEXT(
					"TypedASTJIT provider-private bridge probe did not use the production TypedAST backend: Function=%s Backend=%s Disposition=%u Detail=%s Attempts=[%s]"),
					PrivateBridgeFunction != nullptr
						? UTF8_TO_TCHAR(PrivateBridgeFunction->GetDeclaration())
						: TEXT("<missing>"),
					PrivateBridgeBackendResult != nullptr
						? *PrivateBridgeBackendResult->ActualBackendId.ToString()
						: TEXT("<missing>"),
					PrivateBridgeBackendResult != nullptr
						? static_cast<uint32>(
							PrivateBridgeBackendResult->Disposition)
						: MAX_uint32,
					PrivateBridgeBackendResult != nullptr
						? *PrivateBridgeBackendResult->Detail : TEXT("<missing>"),
					*FString::Join(AttemptDetails, TEXT(" | ")));
				return false;
			}

			const FAngelscriptJITGeneratedFunction* const
				GeneratedPrivateBridgeFunction =
					InOutOutput.Functions.FindByPredicate(
						[CapturedPrivateBridgeFunction](const auto& Candidate)
						{
							return Candidate.FunctionKey
								== CapturedPrivateBridgeFunction->FunctionKey;
						});
			const FAngelscriptJITGeneratedFile* const PrivateBridgeModuleSource =
				GeneratedPrivateBridgeFunction != nullptr
					? InOutOutput.Files.FindByPredicate(
						[GeneratedPrivateBridgeFunction](const auto& Candidate)
						{
							return Candidate.RelativePath
								== GeneratedPrivateBridgeFunction->
									ModuleSourceRelativePath;
						})
					: nullptr;
			const FAngelscriptJITGenerationNativeCallSite* const
				PrivateCurrentNativeCallSite =
					GeneratedPrivateBridgeFunction != nullptr
						? GeneratedPrivateBridgeFunction->NativeCallSites.
							FindByPredicate([](const auto& Candidate)
							{
								return Candidate.Route
									== EAngelscriptJITNativeCallSiteRoute::
										CurrentNativeBinding
									&& Candidate.CanonicalDeclaration.Contains(
										TEXT("TypedASTProviderPrivateAdd"),
										ESearchCase::CaseSensitive)
									&& Candidate.EmittedCppCallee.StartsWith(TEXT(
										"AngelscriptTypedASTJIT::InvokeBoundNative<"));
							})
						: nullptr;
			if (GeneratedPrivateBridgeFunction == nullptr
				|| PrivateBridgeModuleSource == nullptr
				|| PrivateCurrentNativeCallSite == nullptr
				|| !PrivateBridgeModuleSource->Content.Contains(
					GeneratedPrivateBridgeFunction->SymbolPrefix
						+ TEXT("_TypedBody"), ESearchCase::CaseSensitive)
				|| !PrivateBridgeModuleSource->Content.Contains(
					TEXT("AngelscriptTypedASTJIT::InvokeBoundNative<"),
					ESearchCase::CaseSensitive)
				|| PrivateBridgeModuleSource->Content.Contains(
					TEXT("FAngelscriptJITExecutionContext"),
					ESearchCase::CaseSensitive))
			{
				Result.Error = TEXT(
					"TypedASTJIT provider-private safe-native source is missing its typed body/current-slot InvokeBoundNative call or retained a forbidden execution context.");
				return false;
			}

			FTCHARToUTF8 OrdinaryBridgeDeclarationUtf8(
				*AngelscriptStaticJITAotFixture::
					GetTypedASTOrdinaryBridgeSqrtDeclaration());
			asCScriptFunction* const OrdinaryBridgeFunction =
				PrivateBridgeModuleDesc.IsValid()
					&& PrivateBridgeModuleDesc->ScriptModule != nullptr
					? static_cast<asCScriptFunction*>(
						PrivateBridgeModuleDesc->ScriptModule->GetFunctionByDecl(
							OrdinaryBridgeDeclarationUtf8.Get()))
					: nullptr;
			const FAngelscriptStaticJITGenerationFunction* const
				CapturedOrdinaryBridgeFunction =
					OrdinaryBridgeFunction != nullptr && GenerationSnapshot.IsValid()
						? GenerationSnapshot->Functions.FindByPredicate(
							[OrdinaryBridgeFunction](const auto& Candidate)
							{
								return Candidate.EngineLocalFunction
									== OrdinaryBridgeFunction;
							})
						: nullptr;
			const FAngelscriptStaticJITFunctionResult* const
				OrdinaryBridgeBackendResult =
					CapturedOrdinaryBridgeFunction != nullptr
						? BackendFunctionResults.FindByPredicate(
							[CapturedOrdinaryBridgeFunction](const auto& Candidate)
							{
								return Candidate.FunctionKey
									== CapturedOrdinaryBridgeFunction->FunctionKey;
							})
						: nullptr;
			if (OrdinaryBridgeBackendResult == nullptr
				|| OrdinaryBridgeBackendResult->Disposition
					!= EAngelscriptStaticJITFunctionDisposition::Emitted
				|| OrdinaryBridgeBackendResult->ActualBackendId
					!= FAngelscriptStaticJITBackendId::TypedAST())
			{
				Result.Error = FString::Printf(TEXT(
					"TypedASTJIT ordinary no-export scalar bridge probe did not use the production TypedAST backend: Function=%s Backend=%s Disposition=%u Detail=%s"),
					OrdinaryBridgeFunction != nullptr
						? UTF8_TO_TCHAR(OrdinaryBridgeFunction->GetDeclaration())
						: TEXT("<missing>"),
					OrdinaryBridgeBackendResult != nullptr
						? *OrdinaryBridgeBackendResult->ActualBackendId.ToString()
						: TEXT("<missing>"),
					OrdinaryBridgeBackendResult != nullptr
						? static_cast<uint32>(OrdinaryBridgeBackendResult->Disposition)
						: MAX_uint32,
					OrdinaryBridgeBackendResult != nullptr
						? *OrdinaryBridgeBackendResult->Detail : TEXT("<missing>"));
				return false;
			}
			const FAngelscriptJITGeneratedFunction* const
				GeneratedOrdinaryBridgeFunction =
					InOutOutput.Functions.FindByPredicate(
						[CapturedOrdinaryBridgeFunction](const auto& Candidate)
						{
							return Candidate.FunctionKey
								== CapturedOrdinaryBridgeFunction->FunctionKey;
						});
			const FAngelscriptJITGeneratedFile* const OrdinaryBridgeModuleSource =
				GeneratedOrdinaryBridgeFunction != nullptr
					? InOutOutput.Files.FindByPredicate(
						[GeneratedOrdinaryBridgeFunction](const auto& Candidate)
						{
							return Candidate.RelativePath
								== GeneratedOrdinaryBridgeFunction->
									ModuleSourceRelativePath;
						})
					: nullptr;
			if (GeneratedOrdinaryBridgeFunction == nullptr
				|| OrdinaryBridgeModuleSource == nullptr
				|| !OrdinaryBridgeModuleSource->Content.Contains(
					TEXT("AngelscriptTypedASTJIT::InvokeBound<float, float>"),
					ESearchCase::CaseSensitive)
				|| !OrdinaryBridgeModuleSource->Content.Contains(
					TEXT("FMath::Sqrt"), ESearchCase::CaseSensitive))
			{
				Result.Error = TEXT(
					"TypedASTJIT ordinary no-export scalar bridge source is missing its typed InvokeBound<float, float> call or readable registered FMath::Sqrt target.");
				return false;
			}

			FString CapabilityRawDeclaration =
				GeneratedCapabilityFunction->RawEntryDeclaration;
			CapabilityRawDeclaration.RemoveFromEnd(TEXT(";"));
			ProviderProbeCpp += FString::Printf(
				TEXT("\n/*\n")
				TEXT(" * AngelScript TypedASTJIT Production Capability Carrier\n")
				TEXT(" * Declaration : %s\n")
				TEXT(" * Entry ABI   : %s\n")
				TEXT(" * Body owner  : %s\n")
				TEXT(" * Native call : IsRunningCommandlet (production call closure)\n")
				TEXT(" */\n")
				TEXT("extern %s;\n")
				TEXT("extern void %s(FScriptExecution&, asDWORD*, asQWORD*);\n")
				TEXT("extern void %s(FScriptExecution&, void*, void*);\n\n")
				TEXT("namespace\n")
				TEXT("{\n")
				TEXT("\tstruct FTypedASTCapabilityProviderEntryRegistration\n")
				TEXT("\t{\n")
				TEXT("\t\tFTypedASTCapabilityProviderEntryRegistration()\n")
				TEXT("\t\t{\n")
				TEXT("\t\t\tAngelscriptTestJITProbes::RegisterTypedASTCapabilityProviderEntries(\n")
				TEXT("\t\t\t\t&%s,\n")
				TEXT("\t\t\t\treinterpret_cast<asJITFunction_Raw>(&%s),\n")
				TEXT("\t\t\t\t&%s);\n")
				TEXT("\t\t}\n")
				TEXT("\t};\n\n")
				TEXT("\tFTypedASTCapabilityProviderEntryRegistration GTypedASTCapabilityProviderEntryRegistration;\n")
				TEXT("}\n"),
				*AngelscriptStaticJITAotFixture::
					GetTypedASTCapabilityShowcaseDeclaration(),
				*GeneratedCapabilityFunction->EntryAbiHash.ToHexString(),
				*GeneratedCapabilityFunction->ModuleSourceRelativePath,
				*CapabilityRawDeclaration,
				*GeneratedCapabilityFunction->VMEntrySymbol,
				*GeneratedCapabilityFunction->ParmsEntrySymbol,
				*GeneratedCapabilityFunction->VMEntrySymbol,
				*GeneratedCapabilityFunction->RawEntrySymbol,
				*GeneratedCapabilityFunction->ParmsEntrySymbol);

			InOutOutput.Files.Add({
				TEXT("TypedASTJITScalarProbe.generated.cpp"),
				Result.TypedASTGeneratedCpp,
				EAngelscriptJITGeneratedFileKind::ModuleSource});
			InOutOutput.Files.Add({
				TEXT("TypedASTJITProviderProbe.generated.cpp"),
				ProviderProbeCpp,
				EAngelscriptJITGeneratedFileKind::ModuleSource});
			Result.TypedASTGeneratedCpp += TEXT("\n") + ProviderProbeCpp;
			InOutOutput.Files.Sort(
				[](const FAngelscriptJITGeneratedFile& Left,
					const FAngelscriptJITGeneratedFile& Right)
				{
					return Left.RelativePath.Compare(
						Right.RelativePath,
						ESearchCase::IgnoreCase) < 0;
				});
			if (!FAngelscriptJITGeneration::RefreshOwnedFileInventory(
					InOutOutput, &Result.Error))
			{
				return false;
			}
			return true;
		}

		static bool ExtractGeneratedFunctionBlock(
			const FString& ModuleSource,
			const FAngelscriptJITGeneratedFunction& Function,
			FString& OutBlock,
			FString& OutError)
		{
			static const FString FunctionMarker(
				TEXT("/*\n * AngelScript Static JIT Function\n"));
			const FString KeyMarker = FString::Printf(
				TEXT(" * StableFunctionKey  : %s"),
				*Function.FunctionKey.Hash.ToHexString());
			const int32 KeyIndex = ModuleSource.Find(
				KeyMarker, ESearchCase::CaseSensitive);
			const int32 BlockStart = KeyIndex != INDEX_NONE
				? ModuleSource.Find(
					FunctionMarker,
					ESearchCase::CaseSensitive,
					ESearchDir::FromEnd,
					KeyIndex)
				: INDEX_NONE;
			if (BlockStart == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT(
					"Isolated differential generation could not locate function %s in its emitted module source."),
					*Function.FunctionKey.Hash.ToHexString());
				return false;
			}

			int32 BlockEnd = ModuleSource.Find(
				FunctionMarker,
				ESearchCase::CaseSensitive,
				ESearchDir::FromStart,
				BlockStart + FunctionMarker.Len());
			if (BlockEnd == INDEX_NONE)
			{
				BlockEnd = ModuleSource.Find(
					TEXT("#endif\n#endif"),
					ESearchCase::CaseSensitive,
					ESearchDir::FromStart,
					BlockStart + FunctionMarker.Len());
			}
			if (BlockEnd == INDEX_NONE || BlockEnd <= BlockStart)
			{
				OutError = FString::Printf(TEXT(
					"Isolated differential generation could not bound function %s in its emitted module source."),
					*Function.FunctionKey.Hash.ToHexString());
				return false;
			}

			OutBlock = ModuleSource.Mid(BlockStart, BlockEnd - BlockStart);
			OutBlock.TrimEndInline();
			return true;
		}

		static bool AppendIsolatedDifferentialArtifact(
			const FAngelscriptStaticJITBackendId BackendId,
			const TCHAR* const BackendLabel,
			const FName TargetModuleName,
			const FName ReceiverTypeName,
			const ANSICHAR* const TargetFunctionName,
			const bool bExpectedParmsEntry,
			const TCHAR* const UniqueSymbolPrefix,
			const TCHAR* const RelativePath,
			FAngelscriptJITGenerationOutput& InOutOutput,
			FStaticJITAotGenerationResult& Result,
			const TCHAR* const ExpectedTypedFallbackReason = nullptr,
			const bool bForceManualImports = false,
			FString* const OutExpectedTypedFallbackDetail = nullptr,
			const TCHAR* const HarnessRejectionPrefix = nullptr)
		{
			FAngelscriptStaticJITGenerationProfile GenerationProfile;
			FString ProfileError;
			if (!FAngelscriptStaticJITGenerationProfile::TryCreate(
				BackendId,
				EAngelscriptJITTargetProfile::EditorDevelopment,
				GenerationProfile,
				&ProfileError))
			{
				Result.Error = MoveTemp(ProfileError);
				return false;
			}

			FStaticJITAotFixtureEngineSession Session;
			Session.Composition = EStaticJITAotFixtureComposition::Complete;
			FStaticJITAotGenerationResult CreationResult;
			Session.Engine = CreateFixtureEngine(
				Session.FixtureRoot,
				CreationResult,
				false,
				&GenerationProfile,
				EAngelscriptEnginePurpose::StaticJITGeneration,
				EStaticJITAotFixtureComposition::Complete,
				AngelscriptStaticJITAotFixture::
					ETypedASTPrivateBridgeRegistration::Primary,
				nullptr,
				bForceManualImports);
			Session.Error = MoveTemp(CreationResult.Error);
			if (!Session.IsValid())
			{
				Result.Error = Session.Error.IsEmpty()
					? FString::Printf(TEXT(
						"Isolated %s differential generation could not create its fresh Engine."),
						BackendLabel)
					: Session.Error;
				return false;
			}

			FAngelscriptEngineScope EngineScope(*Session.Engine);
			TArray<asIScriptModule*> CompiledSourceGraph;
			asIScriptModule* TargetModule = nullptr;
			const FName FixtureModuleNames[] = {
				AngelscriptStaticJITAotFixture::GetImportProviderModuleName(),
				AngelscriptStaticJITAotFixture::GetModuleName(),
				AngelscriptStaticJITAotFixture::GetTypedControlFlowModuleName(),
				AngelscriptStaticJITAotFixture::GetTypedFinalScalarModuleName(),
				AngelscriptStaticJITAotFixture::GetTypedPrintModuleName(),
				AngelscriptStaticJITAotFixture::GetTypedPrivateBridgeModuleName(),
				AngelscriptStaticJITAotFixture::GetTypedRecursionModuleName(),
			};
			for (const FName ModuleName : FixtureModuleNames)
			{
				const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc =
					Session.Engine->GetModuleByModuleName(ModuleName.ToString());
				if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
				{
					Result.Error = FString::Printf(TEXT(
						"Isolated %s differential generation is missing compiled module '%s'."),
						BackendLabel, *ModuleName.ToString());
					return false;
				}
				CompiledSourceGraph.Add(ModuleDesc->ScriptModule);
				if (ModuleName == TargetModuleName)
				{
					TargetModule = ModuleDesc->ScriptModule;
				}
			}

			const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
				ESPMode::ThreadSafe> GenerationSnapshot =
					Session.Engine->GetStaticJITGenerationSnapshot();
			if (!GenerationSnapshot.IsValid()
				|| !GenerationSnapshot->IsComplete()
				|| TargetModule == nullptr)
			{
				Result.Error = FString::Printf(TEXT(
					"Isolated %s differential generation produced no complete same-compilation snapshot or target module."),
					BackendLabel);
				return false;
			}
			const FAngelscriptStaticJITGenerationModule* const SnapshotModule =
				GenerationSnapshot->Modules.FindByPredicate(
					[TargetModule](const auto& Candidate)
					{
						return Candidate.EngineLocalModule == TargetModule;
					});
			FTCHARToUTF8 ReceiverNameUtf8(*ReceiverTypeName.ToString());
			asITypeInfo* const ReceiverType = ReceiverTypeName.IsNone()
				? nullptr
				: TargetModule->GetTypeInfoByName(ReceiverNameUtf8.Get());
			asIScriptFunction* EngineLocalTargetFunction = nullptr;
			int32 MatchingTargetMethodCount = 0;
			if (ReceiverTypeName.IsNone())
			{
				for (asUINT FunctionIndex = 0;
					FunctionIndex < TargetModule->GetFunctionCount();
					++FunctionIndex)
				{
					asIScriptFunction* const Candidate =
						TargetModule->GetFunctionByIndex(FunctionIndex);
					if (Candidate != nullptr
						&& Candidate->GetObjectType() == nullptr
						&& FCStringAnsi::Strcmp(
							Candidate->GetName(), TargetFunctionName) == 0)
					{
						EngineLocalTargetFunction = Candidate;
						++MatchingTargetMethodCount;
					}
				}
			}
			else if (ReceiverType != nullptr)
			{
				for (asUINT MethodIndex = 0;
					MethodIndex < ReceiverType->GetMethodCount();
					++MethodIndex)
				{
					asIScriptFunction* const Candidate =
						ReceiverType->GetMethodByIndex(MethodIndex);
					if (Candidate != nullptr
						&& FCStringAnsi::Strcmp(
							Candidate->GetName(),
							TargetFunctionName) == 0)
					{
						EngineLocalTargetFunction = Candidate;
						++MatchingTargetMethodCount;
					}
				}
			}
			const FAngelscriptStaticJITGenerationFunction* const SnapshotFunction =
				SnapshotModule != nullptr
					&& EngineLocalTargetFunction != nullptr
					&& MatchingTargetMethodCount == 1
				? GenerationSnapshot->Functions.FindByPredicate(
					[EngineLocalTargetFunction](const auto& Candidate)
					{
						return Candidate.EngineLocalFunction
							== EngineLocalTargetFunction;
					})
				: nullptr;
			if (SnapshotFunction == nullptr)
			{
				TArray<FString> AvailableDeclarations;
				if (SnapshotModule != nullptr)
				{
					for (const FAngelscriptStaticJITGenerationFunction& Candidate
						: GenerationSnapshot->Functions)
					{
						if (Candidate.ModuleKey == SnapshotModule->ModuleKey)
						{
							AvailableDeclarations.Add(
								Candidate.CanonicalDeclaration);
						}
					}
				}
				AvailableDeclarations.Sort();
				Result.Error = FString::Printf(TEXT(
					"Isolated %s differential generation could not resolve the unique Engine-local target '%s' to an exact target FunctionKey; receiver=%s functionMatches=%d available target-module declarations: %s."),
					BackendLabel,
					UTF8_TO_TCHAR(TargetFunctionName),
					ReceiverTypeName.IsNone()
						? TEXT("global")
						: (ReceiverType != nullptr ? TEXT("resolved") : TEXT("missing")),
					MatchingTargetMethodCount,
					*FString::Join(AvailableDeclarations, TEXT(", ")));
				return false;
			}

			FAngelscriptJITProviderGenerationSettings Settings;
			Settings.TargetProfile =
				EAngelscriptJITTargetProfile::EditorDevelopment;
			Settings.ProviderId = AngelscriptTestJITProbes::GetStableProviderId();
			Settings.ProviderName = FString::Printf(
				TEXT("AngelscriptTestJIT isolated %s differential artifact"),
				BackendLabel);
			Settings.OwnerModuleName = TEXT("AngelscriptTestJIT");
			Settings.KnownNativeCallModules = {
				FName(TEXT("Core")),
				FName(TEXT("CoreUObject")),
				FName(TEXT("AngelscriptRuntime")),
				FName(TEXT("AngelscriptTest")),
			};
			Settings.PublicNativeCallDependencyModules = {
				FName(TEXT("Core")),
				FName(TEXT("CoreUObject")),
				FName(TEXT("AngelscriptRuntime")),
			};
			Settings.ArtifactProfile = GenerationSnapshot->ArtifactProfile;
			Settings.NativeEnvironmentFingerprint =
				FAngelscriptJITTargetProfiles::
					BuildCurrentNativeEnvironmentFingerprint(
						Settings.TargetProfile);

			asIScriptModule* EmitModules[] = {TargetModule};
			FAngelscriptStaticJITProviderGenerationRequest Request;
			Request.BackendId = BackendId;
			Request.Settings = Settings;
			Request.TypedASTJITCapabilityProfile =
				GenerationProfile.TypedASTJITCapabilityProfile;
			Request.TypedASTJITExecutionRequirements =
				GenerationProfile.TypedASTJITExecutionRequirements;
			Request.CompiledSourceGraph = CompiledSourceGraph;
			Request.EmitModuleSet = MakeArrayView(EmitModules);
			Request.GenerationSnapshot = GenerationSnapshot;

			FAngelscriptStaticJITGeneratorOutput GeneratorOutput;
			FString GenerationError;
			if (!GenerateStaticJITProviderArtifacts(
				Request, GeneratorOutput, &GenerationError))
			{
				Result.Error = FString::Printf(TEXT(
					"Isolated %s differential generation failed: %s"),
					BackendLabel, *GenerationError);
				return false;
			}
			const FAngelscriptStaticJITFunctionResult* const FunctionResult =
				GeneratorOutput.Functions.FindByPredicate(
					[SnapshotFunction](const auto& Candidate)
					{
						return Candidate.FunctionKey
							== SnapshotFunction->FunctionKey;
					});
			const FAngelscriptJITGeneratedFunction* const GeneratedFunction =
				GeneratorOutput.bHasProviderOutput
				? GeneratorOutput.ProviderOutput.Functions.FindByPredicate(
					[SnapshotFunction](const auto& Candidate)
					{
						return Candidate.FunctionKey
							== SnapshotFunction->FunctionKey;
					})
				: nullptr;
			if (ExpectedTypedFallbackReason != nullptr)
			{
				const FAngelscriptStaticJITBackendAttemptDiagnostic* const
					TypedAttempt = FunctionResult != nullptr
					? FunctionResult->BackendAttempts.FindByPredicate(
						[](const auto& Attempt)
						{
							return Attempt.BackendId
								== FAngelscriptStaticJITBackendId::TypedAST();
						})
					: nullptr;
				const FAngelscriptStaticJITBackendAttemptDiagnostic* const
					BytecodeAttempt = FunctionResult != nullptr
					? FunctionResult->BackendAttempts.FindByPredicate(
						[](const auto& Attempt)
						{
							return Attempt.BackendId
								== FAngelscriptStaticJITBackendId::Bytecode();
						})
					: nullptr;
				const FString ExpectedReasonPrefix =
					FString(ExpectedTypedFallbackReason) + TEXT(":");
				if (BackendId != FAngelscriptStaticJITBackendId::TypedAST()
					|| FunctionResult == nullptr
					|| FunctionResult->Disposition
						!= EAngelscriptStaticJITFunctionDisposition::Emitted
					|| FunctionResult->ActualBackendId
						!= FAngelscriptStaticJITBackendId::Bytecode()
					|| GeneratedFunction == nullptr
					|| TypedAttempt == nullptr
					|| TypedAttempt->Disposition
						!= EAngelscriptStaticJITFunctionDisposition::Unsupported
					|| !TypedAttempt->Detail.StartsWith(
						ExpectedReasonPrefix, ESearchCase::CaseSensitive)
					|| BytecodeAttempt == nullptr
					|| BytecodeAttempt->Disposition
						!= EAngelscriptStaticJITFunctionDisposition::Emitted)
				{
					Result.Error = FString::Printf(TEXT(
						"Isolated TypedASTJIT differential target did not fail closed through '%s' before Bytecode fallback: disposition=%u actual='%s' generated=%s typedAttempt='%s' bytecodeDisposition=%u."),
						ExpectedTypedFallbackReason,
						FunctionResult != nullptr
							? static_cast<uint32>(FunctionResult->Disposition) : 0u,
						FunctionResult != nullptr
							? *FunctionResult->ActualBackendId.ToString() : TEXT("<missing>"),
						GeneratedFunction != nullptr ? TEXT("true") : TEXT("false"),
						TypedAttempt != nullptr
							? *TypedAttempt->Detail : TEXT("<missing>"),
						BytecodeAttempt != nullptr
							? static_cast<uint32>(BytecodeAttempt->Disposition) : 0u);
					return false;
				}

				if (OutExpectedTypedFallbackDetail != nullptr)
				{
					FString NormalizedFallbackDetail;
					if (HarnessRejectionPrefix == nullptr)
					{
						NormalizedFallbackDetail = TypedAttempt->Detail;
					}
					else
					{
						NormalizedFallbackDetail = FString::Printf(
							TEXT("%s: %s"),
							HarnessRejectionPrefix,
							*TypedAttempt->Detail);
					}

					FString PhysicalScriptRoot = FPaths::Combine(
						Session.FixtureRoot, TEXT("Script"));
					FPaths::NormalizeFilename(PhysicalScriptRoot);
					NormalizedFallbackDetail.ReplaceInline(
						*PhysicalScriptRoot,
						TEXT("/Angelscript/Game"),
						ESearchCase::CaseSensitive);
					FString NormalizedFixtureRoot = Session.FixtureRoot;
					FPaths::NormalizeFilename(NormalizedFixtureRoot);
					if (NormalizedFallbackDetail.Contains(
						NormalizedFixtureRoot,
						ESearchCase::CaseSensitive))
					{
						Result.Error = FString::Printf(TEXT(
							"%s differential rejection retained its transient fixture root after source normalization: %s"),
							HarnessRejectionPrefix != nullptr
								? HarnessRejectionPrefix
								: ExpectedTypedFallbackReason,
							*NormalizedFallbackDetail);
						return false;
					}
					*OutExpectedTypedFallbackDetail =
						MoveTemp(NormalizedFallbackDetail);
				}
				return true;
			}
			if (FunctionResult == nullptr
				|| FunctionResult->Disposition
					!= EAngelscriptStaticJITFunctionDisposition::Emitted
				|| FunctionResult->ActualBackendId != BackendId
				|| GeneratedFunction == nullptr
				|| !GeneratedFunction->bHasVMEntry
				|| !GeneratedFunction->bHasRawEntry
				|| GeneratedFunction->bHasParmsEntry
					!= bExpectedParmsEntry)
			{
				TArray<FString> AttemptDetails;
				if (FunctionResult != nullptr)
				{
					for (const FAngelscriptStaticJITBackendAttemptDiagnostic& Attempt
						: FunctionResult->BackendAttempts)
					{
						AttemptDetails.Add(FString::Printf(TEXT("%s=%u:%s"),
							*Attempt.BackendId.ToString(),
							static_cast<uint32>(Attempt.Disposition),
							*Attempt.Detail));
					}
				}
				Result.Error = FString::Printf(TEXT(
					"Isolated %s differential target emitted an unexpected entry plan: disposition=%u actual='%s' vm=%s raw=%s parms=%s expectedParms=%s detail='%s' attempts={%s}."),
					BackendLabel,
					FunctionResult != nullptr
						? static_cast<uint32>(FunctionResult->Disposition) : 0u,
					FunctionResult != nullptr
						? *FunctionResult->ActualBackendId.ToString() : TEXT("<missing>"),
					GeneratedFunction != nullptr && GeneratedFunction->bHasVMEntry
						? TEXT("true") : TEXT("false"),
					GeneratedFunction != nullptr && GeneratedFunction->bHasRawEntry
						? TEXT("true") : TEXT("false"),
					GeneratedFunction != nullptr && GeneratedFunction->bHasParmsEntry
						? TEXT("true") : TEXT("false"),
					bExpectedParmsEntry ? TEXT("true") : TEXT("false"),
					FunctionResult != nullptr
						? *FunctionResult->Detail : TEXT("<missing>"),
					*FString::Join(AttemptDetails, TEXT(", ")));
				return false;
			}

			using ECleanupPlanState = AngelscriptTestJITProbes::
				EIsolatedDifferentialCleanupPlanState;
			ECleanupPlanState CleanupPlanState =
				ECleanupPlanState::NotCaptured;
			uint32 CleanupActionCount = 0;
			if (BackendId == FAngelscriptStaticJITBackendId::TypedAST())
			{
				const asCDecl* const CanonicalFunctionDecl =
					SnapshotFunction->SealedAST != nullptr
						&& SnapshotFunction->SealedAST->IsPublishable()
						&& SnapshotFunction->CanonicalFunctionDeclValue != 0
					? SnapshotFunction->SealedAST->GetDecl(asASTDeclId(
						SnapshotFunction->CanonicalFunctionDeclValue))
					: nullptr;
				if (CanonicalFunctionDecl == nullptr
					|| !CanonicalFunctionDecl->body.IsValid())
				{
					Result.Error = FString::Printf(TEXT(
						"Isolated TypedASTJIT differential target '%s' did not retain its exact sealed canonical AST body."),
						*SnapshotFunction->CanonicalDeclaration);
					return false;
				}

				// A successful production Canonical Typed AST emission is the proof
				// for this scalar-only test slice: eligibility rejects non-scalar
				// lifetime state, while construct/materialize/cleanup wrappers are
				// accepted only when they transparently own one scalar value. The
				// production snapshot intentionally no longer retains HIR cleanup
				// plans, so the probe records the equivalent zero-action result from
				// the canonical eligibility/emission contract.
				CleanupPlanState = ECleanupPlanState::VerifiedEmpty;
			}

			const FAngelscriptJITGeneratedFile* const ModuleSource =
				GeneratorOutput.ProviderOutput.Files.FindByPredicate(
					[GeneratedFunction](const auto& Candidate)
					{
						return Candidate.RelativePath
							== GeneratedFunction->ModuleSourceRelativePath;
					});
			FString FunctionBlock;
			if (ModuleSource == nullptr
				|| !ExtractGeneratedFunctionBlock(
					ModuleSource->Content,
					*GeneratedFunction,
					FunctionBlock,
					Result.Error))
			{
				if (ModuleSource == nullptr)
				{
					Result.Error = FString::Printf(TEXT(
						"Isolated %s differential target has no generated module source."),
						BackendLabel);
				}
				return false;
			}

			const FString OriginalPrefix = GeneratedFunction->SymbolPrefix;
			FunctionBlock.ReplaceInline(
				*OriginalPrefix,
				UniqueSymbolPrefix,
				ESearchCase::CaseSensitive);
			const FString VMEntrySymbol = GeneratedFunction->VMEntrySymbol.Replace(
				*OriginalPrefix,
				UniqueSymbolPrefix,
				ESearchCase::CaseSensitive);
			const FString RawEntrySymbol = GeneratedFunction->RawEntrySymbol.Replace(
				*OriginalPrefix,
				UniqueSymbolPrefix,
				ESearchCase::CaseSensitive);
			const FString ParmsEntrySymbol =
				GeneratedFunction->ParmsEntrySymbol.Replace(
					*OriginalPrefix,
					UniqueSymbolPrefix,
					ESearchCase::CaseSensitive);
			const FString ParmsEntryExpression =
				GeneratedFunction->bHasParmsEntry
					? TEXT("&") + ParmsEntrySymbol
					: FString(TEXT("nullptr"));
			const FString RegistrationType = FString::Printf(
				TEXT("F%sDifferentialEntryRegistration"), BackendLabel);
			const FString RegistrationInstance = FString::Printf(
				TEXT("G%sDifferentialEntryRegistration"), BackendLabel);
			const TCHAR* const CleanupPlanStateName =
				CleanupPlanState == ECleanupPlanState::VerifiedEmpty
					? TEXT("VerifiedEmpty") : TEXT("NotCaptured");
			FString ReferenceSlotsDeclaration;
			FString ReferenceSlotsExpression(TEXT("nullptr"));
			const FString ReferenceSlotsName =
				RegistrationType + TEXT("ReferenceSlots");
			if (!GeneratedFunction->ReferenceSlots.IsEmpty())
			{
				ReferenceSlotsExpression = ReferenceSlotsName;
				ReferenceSlotsDeclaration = FString::Printf(
					TEXT("\tstatic const FAngelscriptJITReferenceSlot %s[%d] =\n\t{\n"),
					*ReferenceSlotsName,
					GeneratedFunction->ReferenceSlots.Num());
				for (const FAngelscriptJITReferenceSlot& Slot
					: GeneratedFunction->ReferenceSlots)
				{
					ReferenceSlotsDeclaration += FString::Printf(
						TEXT("\t\t{sizeof(FAngelscriptJITReferenceSlot), %uu, static_cast<EAngelscriptJITReferenceSlotFlags>(%u), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(%u), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT(\"%s\")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT(\"%s\")))}}},\n"),
						Slot.SlotIndex,
						static_cast<uint32>(Slot.Flags),
						static_cast<uint32>(Slot.Reference.Kind),
						*Slot.Reference.StableKey.ToHexString(),
						*Slot.Reference.ExpectedAbi.ToHexString());
				}
				ReferenceSlotsDeclaration += TEXT("\t};\n\n");
			}
			const FString GeneratedSource = FString::Printf(
				TEXT("// @angelscript-jit-owned revision=2 kind=module-source\n")
				TEXT("/*\n")
				TEXT(" * AngelScript Static JIT Isolated Differential Artifact\n")
				TEXT(" * Backend            : %s\n")
				TEXT(" * Declaration        : %s\n")
				TEXT(" * StableFunctionKey  : %s\n")
				TEXT(" * OriginalSymbolSet  : %s\n")
				TEXT(" * TestSymbolSet      : %s\n")
				TEXT(" * CleanupPlan       : %s (%u actions)\n")
				TEXT(" * Publication        : test-only entry accessors; no Provider registration\n")
				TEXT(" */\n\n")
				TEXT("#if WITH_EDITOR && UE_BUILD_DEVELOPMENT\n")
				TEXT("#include \"StaticJIT/StaticJITConfig.h\"\n")
				TEXT("#ifndef AS_SKIP_JITTED_CODE\n")
				TEXT("#include \"AngelscriptTestJITProbes.h\"\n")
				TEXT("#include \"CoreGlobals.h\"\n")
				TEXT("#include \"CoreMinimal.h\"\n")
				TEXT("#include \"StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h\"\n")
				TEXT("#include \"StaticJIT/StaticJITHeader.h\"\n")
				TEXT("#include \"StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h\"\n")
				TEXT("#include \"StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h\"\n\n")
				TEXT("%s\n\n")
				TEXT("namespace\n")
				TEXT("{\n")
				TEXT("%s")
				TEXT("\tstruct %s\n")
				TEXT("\t{\n")
				TEXT("\t\t%s()\n")
				TEXT("\t\t{\n")
				TEXT("\t\t\tAngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(\n")
				TEXT("\t\t\t\tAngelscriptTestJITProbes::EIsolatedDifferentialBackend::%s,\n")
				TEXT("\t\t\t\t&%s,\n")
				TEXT("\t\t\t\treinterpret_cast<asJITFunction_Raw>(&%s),\n")
				TEXT("\t\t\t\t%s,\n")
				TEXT("\t\t\t\tTEXT(\"%s\"),\n")
				TEXT("\t\t\t\tTEXT(\"%s\"),\n")
				TEXT("\t\t\t\tTEXT(\"%s\"),\n")
				TEXT("\t\t\t\tTEXT(\"%s\"),\n")
				TEXT("\t\t\t\t%uu,\n")
				TEXT("\t\t\t\t%uu,\n")
				TEXT("\t\t\t\tAngelscriptTestJITProbes::EIsolatedDifferentialCleanupPlanState::%s,\n")
				TEXT("\t\t\t\t%uu,\n")
				TEXT("\t\t\t\t%s,\n")
				TEXT("\t\t\t\t%uu);\n")
				TEXT("\t\t}\n")
				TEXT("\t};\n\n")
				TEXT("\t%s %s;\n")
				TEXT("}\n")
				TEXT("#endif\n")
				TEXT("#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT\n"),
				BackendLabel,
				*GeneratedFunction->CanonicalDeclaration,
				*GeneratedFunction->FunctionKey.Hash.ToHexString(),
				*OriginalPrefix,
				UniqueSymbolPrefix,
				CleanupPlanStateName,
				CleanupActionCount,
				*FunctionBlock,
				*ReferenceSlotsDeclaration,
				*RegistrationType,
				*RegistrationType,
				BackendLabel,
				*VMEntrySymbol,
				*RawEntrySymbol,
				*ParmsEntryExpression,
				UniqueSymbolPrefix,
				*GeneratedFunction->FunctionKey.Hash.ToHexString(),
				*GeneratedFunction->CanonicalDeclaration,
				*GeneratedFunction->SourceSection,
				GeneratedFunction->SourceLine,
				GeneratedFunction->SourceColumn,
				CleanupPlanStateName,
				CleanupActionCount,
				*ReferenceSlotsExpression,
				GeneratedFunction->ReferenceSlots.Num(),
				*RegistrationType,
				*RegistrationInstance);

			InOutOutput.Files.Add({
				RelativePath,
				GeneratedSource,
				EAngelscriptJITGeneratedFileKind::ModuleSource});
			Result.TypedASTGeneratedCpp += TEXT("\n") + GeneratedSource;
			if (FCStringAnsi::Strcmp(
					TargetFunctionName, "DifferentialScalarValue") == 0
				&& BackendId == FAngelscriptStaticJITBackendId::Bytecode())
			{
				Result.IsolatedBytecodeDifferentialSymbolPrefix =
					UniqueSymbolPrefix;
				Result.IsolatedBytecodeDifferentialIdentity = {
					GeneratedFunction->FunctionKey.Hash.ToHexString(),
					GeneratedFunction->ExecutionHash.ToHexString(),
					GeneratedFunction->DebugHash.ToHexString(),
					Settings.ArtifactProfile.Hash.ToHexString(),
					GeneratedFunction->EntryAbiHash.ToHexString(),
				};
			}
			else if (FCStringAnsi::Strcmp(
				TargetFunctionName, "DifferentialScalarValue") == 0)
			{
				Result.IsolatedTypedASTDifferentialSymbolPrefix =
					UniqueSymbolPrefix;
				Result.IsolatedTypedASTDifferentialIdentity = {
					GeneratedFunction->FunctionKey.Hash.ToHexString(),
					GeneratedFunction->ExecutionHash.ToHexString(),
					GeneratedFunction->DebugHash.ToHexString(),
					Settings.ArtifactProfile.Hash.ToHexString(),
					GeneratedFunction->EntryAbiHash.ToHexString(),
				};
			}
			return true;
		}

		static bool AppendIsolatedDifferentialArtifacts(
			FAngelscriptJITGenerationOutput& InOutOutput,
			FStaticJITAotGenerationResult& Result)
		{
			if (!AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::Bytecode(),
					TEXT("BytecodeJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarModuleName(),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarReceiverClassName(),
					"DifferentialScalarValue",
					true,
					TEXT("AS_Test_Differential_Bytecode_DifferentialScalarValue"),
					TEXT("DifferentialBytecodeJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::TypedAST(),
					TEXT("TypedASTJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarModuleName(),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarReceiverClassName(),
					"DifferentialScalarValue",
					true,
					TEXT("AS_Test_Differential_TypedAST_DifferentialScalarValue"),
					TEXT("DifferentialTypedASTJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::Bytecode(),
					TEXT("BytecodeJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedControlFlowModuleName(),
					NAME_None,
					"SemanticExhaustiveEnum",
					false,
					TEXT("AS_Test_Differential_Bytecode_SemanticExhaustiveEnum"),
					TEXT("DifferentialControlFlowBytecodeJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::TypedAST(),
					TEXT("TypedASTJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedControlFlowModuleName(),
					NAME_None,
					"SemanticExhaustiveEnum",
					true,
					TEXT("AS_Test_Differential_TypedAST_SemanticExhaustiveEnum"),
					TEXT("DifferentialControlFlowTypedASTJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::Bytecode(),
					TEXT("BytecodeJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedControlFlowModuleName(),
					NAME_None,
					"SemanticCheckedDivide",
					false,
					TEXT("AS_Test_Differential_Bytecode_SemanticCheckedDivide"),
					TEXT("DifferentialCheckedIntegerBytecodeJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::TypedAST(),
					TEXT("TypedASTJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedControlFlowModuleName(),
					NAME_None,
					"SemanticCheckedDivide",
					true,
					TEXT("AS_Test_Differential_TypedAST_SemanticCheckedDivide"),
					TEXT("DifferentialCheckedIntegerTypedASTJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::Bytecode(),
					TEXT("BytecodeJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedControlFlowModuleName(),
					NAME_None,
					"SemanticCloneableState",
					false,
					TEXT("AS_Test_Differential_Bytecode_SemanticCloneableState"),
					TEXT("DifferentialCloneableStateBytecodeJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::TypedAST(),
					TEXT("TypedASTJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedControlFlowModuleName(),
					NAME_None,
					"SemanticCloneableState",
					true,
					TEXT("AS_Test_Differential_TypedAST_SemanticCloneableState"),
					TEXT("DifferentialCloneableStateTypedASTJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::Bytecode(),
					TEXT("BytecodeJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarModuleName(),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarReceiverClassName(),
					"SemanticNumericBoundary",
					true,
					TEXT("AS_Test_Differential_Bytecode_SemanticNumericBoundary"),
					TEXT("DifferentialNumericBoundaryBytecodeJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::TypedAST(),
					TEXT("TypedASTJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarModuleName(),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarReceiverClassName(),
					"SemanticNumericBoundary",
					true,
					TEXT("AS_Test_Differential_TypedAST_SemanticNumericBoundary"),
					TEXT("DifferentialNumericBoundaryTypedASTJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::Bytecode(),
					TEXT("BytecodeJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarModuleName(),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarReceiverClassName(),
					"SemanticPowerBoundary",
					true,
					TEXT("AS_Test_Differential_Bytecode_SemanticPowerBoundary"),
					TEXT("DifferentialPowerBoundaryBytecodeJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::TypedAST(),
					TEXT("TypedASTJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarModuleName(),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarReceiverClassName(),
					"SemanticPowerBoundary",
					true,
					TEXT("AS_Test_Differential_TypedAST_SemanticPowerBoundary"),
					TEXT("DifferentialPowerBoundaryTypedASTJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::Bytecode(),
					TEXT("BytecodeJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarModuleName(),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarReceiverClassName(),
					"SemanticImportedBinding",
					true,
					TEXT("AS_Test_Differential_Bytecode_SemanticImportedBinding"),
					TEXT("DifferentialImportedBindingBytecodeJIT.generated.cpp"),
					InOutOutput,
					Result,
					nullptr,
					true)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::TypedAST(),
					TEXT("TypedASTJIT"),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarModuleName(),
					AngelscriptStaticJITAotFixture::
						GetTypedFinalScalarReceiverClassName(),
					"SemanticImportedBinding",
					true,
					TEXT("AS_Test_Differential_TypedAST_SemanticImportedBinding"),
					TEXT("DifferentialImportedBindingTypedASTJIT.generated.cpp"),
					InOutOutput,
					Result,
					TEXT("UnsupportedImportedRoute"),
					true,
					&Result.IsolatedTypedASTImportedFallbackDetail))
			{
				return false;
			}

			if (!AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::Bytecode(),
					TEXT("BytecodeJIT"),
					AngelscriptStaticJITAotFixture::GetModuleName(),
					NAME_None,
					"TypedASTCapabilityShowcase",
					false,
					TEXT("AS_Test_Differential_Bytecode_TypedASTCapabilityShowcase"),
					TEXT("DifferentialInlineBytecodeJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::TypedAST(),
					TEXT("TypedASTJIT"),
					AngelscriptStaticJITAotFixture::GetModuleName(),
					NAME_None,
					"TypedASTCapabilityShowcase",
					true,
					TEXT("AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase"),
					TEXT("DifferentialInlineTypedASTJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::Bytecode(),
					TEXT("BytecodeJIT"),
					AngelscriptStaticJITAotFixture::GetTypedPrintModuleName(),
					NAME_None,
					"TypedASTPrintShowcase",
					false,
					TEXT("AS_Test_Differential_Bytecode_TypedASTPrintShowcase"),
					TEXT("DifferentialExportedBytecodeJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::TypedAST(),
					TEXT("TypedASTJIT"),
					AngelscriptStaticJITAotFixture::GetTypedPrintModuleName(),
					NAME_None,
					"TypedASTPrintShowcase",
					true,
					TEXT("AS_Test_Differential_TypedAST_TypedASTPrintShowcase"),
					TEXT("DifferentialExportedTypedASTJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::Bytecode(),
					TEXT("BytecodeJIT"),
					AngelscriptStaticJITAotFixture::GetTypedPrivateBridgeModuleName(),
					NAME_None,
					"TypedASTRuntimeThunkShowcase",
					false,
					TEXT("AS_Test_Differential_Bytecode_TypedASTRuntimeThunkShowcase"),
					TEXT("DifferentialRuntimeThunkBytecodeJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::TypedAST(),
					TEXT("TypedASTJIT"),
					AngelscriptStaticJITAotFixture::GetTypedPrivateBridgeModuleName(),
					NAME_None,
					"TypedASTRuntimeThunkShowcase",
					true,
					TEXT("AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase"),
					TEXT("DifferentialRuntimeThunkTypedASTJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::Bytecode(),
					TEXT("BytecodeJIT"),
					AngelscriptStaticJITAotFixture::GetTypedPrivateBridgeModuleName(),
					NAME_None,
					"TypedASTPrivateBridgeShowcase",
					false,
					TEXT("AS_Test_Differential_Bytecode_TypedASTPrivateBridgeShowcase"),
					TEXT("DifferentialPrivateBridgeBytecodeJIT.generated.cpp"),
					InOutOutput,
					Result)
				|| !AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::TypedAST(),
					TEXT("TypedASTJIT"),
					AngelscriptStaticJITAotFixture::GetTypedPrivateBridgeModuleName(),
					NAME_None,
					"TypedASTPrivateBridgeShowcase",
					true,
					TEXT("AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase"),
					TEXT("DifferentialPrivateBridgeTypedASTJIT.generated.cpp"),
					InOutOutput,
					Result))
			{
				return false;
			}

			if (!AppendIsolatedDifferentialArtifact(
					FAngelscriptStaticJITBackendId::TypedAST(),
					TEXT("TypedASTJIT"),
					AngelscriptStaticJITAotFixture::GetModuleName(),
					AngelscriptStaticJITAotFixture::GetGeneratedClassName(),
					"ObjectLifetimeEntryForAOT",
					true,
					TEXT("AS_Test_Differential_TypedAST_ObjectLifetimeEntryForAOT"),
					TEXT("DifferentialRejectedObjectLifetimeTypedASTJIT.generated.cpp"),
					InOutOutput,
					Result,
					TEXT("UnsupportedSignature"),
					false,
					&Result.IsolatedTypedASTNonCloneableFallbackDetail,
					TEXT("NonCloneableEffectRejected")))
			{
				return false;
			}

			InOutOutput.Files.Sort(
				[](const FAngelscriptJITGeneratedFile& Left,
					const FAngelscriptJITGeneratedFile& Right)
				{
					return Left.RelativePath.Compare(
						Right.RelativePath,
						ESearchCase::IgnoreCase) < 0;
				});
			return FAngelscriptJITGeneration::RefreshOwnedFileInventory(
				InOutOutput, &Result.Error);
		}

		static bool BuildExpectedOutput(
			FAngelscriptEngine& Engine,
			const FString& FixtureRoot,
			const FAngelscriptStaticJITGenerationProfile& GenerationProfile,
			const EStaticJITAotGenerationTestVariant Variant,
			FAngelscriptJITGenerationOutput& OutOutput,
			FStaticJITAotGenerationResult& Result)
		{
#if !AS_CAN_GENERATE_JIT
			Result.Error = TEXT(
				"AngelscriptTestJIT generation is unavailable in this build.");
			return false;
#else
			TArray<asIScriptModule*> ScriptModules;
			const FName FixtureModuleNames[] = {
				AngelscriptStaticJITAotFixture::GetImportProviderModuleName(),
				AngelscriptStaticJITAotFixture::GetModuleName(),
				AngelscriptStaticJITAotFixture::GetTypedControlFlowModuleName(),
				AngelscriptStaticJITAotFixture::GetTypedFinalScalarModuleName(),
				AngelscriptStaticJITAotFixture::GetTypedPrintModuleName(),
				AngelscriptStaticJITAotFixture::GetTypedPrivateBridgeModuleName(),
				AngelscriptStaticJITAotFixture::GetTypedRecursionModuleName(),
			};
			for (const FName ModuleName : FixtureModuleNames)
			{
				const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc =
					Engine.GetModuleByModuleName(ModuleName.ToString());
				if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
				{
					Result.Error = FString::Printf(
						TEXT("AngelscriptTestJIT fixture module '%s' is unavailable after compile."),
						*ModuleName.ToString());
					return false;
				}
				ScriptModules.Add(ModuleDesc->ScriptModule);
			}

			const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
				ESPMode::ThreadSafe> GenerationSnapshot =
				Engine.GetStaticJITGenerationSnapshot();
			if (!GenerationSnapshot.IsValid()
				|| !GenerationSnapshot->IsComplete())
			{
				Result.Error = TEXT(
					"AngelscriptTestJIT fixture compile produced no complete generation snapshot.");
				return false;
			}

			FAngelscriptJITProviderGenerationSettings Settings;
			Settings.TargetProfile =
				EAngelscriptJITTargetProfile::EditorDevelopment;
			Settings.ProviderId =
				AngelscriptTestJITProbes::GetStableProviderId();
			Settings.ProviderName = TEXT("AngelscriptTestJIT Fixtures");
			Settings.OwnerModuleName = TEXT("AngelscriptTestJIT");
			Settings.KnownNativeCallModules = {
				FName(TEXT("Core")),
				FName(TEXT("CoreUObject")),
				FName(TEXT("AngelscriptRuntime")),
				// The provider-private probe is registered by AngelscriptTest.
				// Generated code reaches it through the Runtime binding slot, so
				// the provider module must be known but is deliberately not a
				// public dependency of the AngelscriptTestJIT consumer module.
				FName(TEXT("AngelscriptTest")),
			};
			Settings.PublicNativeCallDependencyModules = {
				FName(TEXT("Core")),
				FName(TEXT("CoreUObject")),
				FName(TEXT("AngelscriptRuntime")),
			};
			Settings.ArtifactProfile = GenerationSnapshot->ArtifactProfile;
			Settings.NativeEnvironmentFingerprint =
				FAngelscriptJITTargetProfiles::
					BuildCurrentNativeEnvironmentFingerprint(
						Settings.TargetProfile);

			FAngelscriptStaticJITProviderGenerationRequest Request;
			Request.BackendId =
				FAngelscriptStaticJITBackendId::TypedAST();
			Request.Settings = Settings;
			Request.TypedASTJITCapabilityProfile =
				GenerationProfile.TypedASTJITCapabilityProfile;
			Request.TypedASTJITExecutionRequirements =
				GenerationProfile.TypedASTJITExecutionRequirements;
			Request.CompiledSourceGraph = ScriptModules;
			Request.GenerationSnapshot = GenerationSnapshot;
			FAngelscriptStaticJITGeneratorOutput GeneratorOutput;
			FString GenerationError;
			if (!GenerateStaticJITProviderArtifacts(
				Request, GeneratorOutput, &GenerationError))
			{
				Result.Error = MoveTemp(GenerationError);
				return false;
			}
			if (!GeneratorOutput.bHasProviderOutput)
			{
				Result.Error = TEXT(
					"AngelscriptTestJIT generation produced no eligible Provider output.");
				return false;
			}
			OutOutput = MoveTemp(GeneratorOutput.ProviderOutput);
			if (!AppendTypedASTScalarProbe(
				Engine, GeneratorOutput.Functions,
				OutOutput, Result,
				Variant == EStaticJITAotGenerationTestVariant::
					AdvertiseUnsupportedEmitterCapability))
			{
				return false;
			}
			if (!AppendIsolatedDifferentialArtifacts(OutOutput, Result))
			{
				return false;
			}
			Result.ProviderId = OutOutput.ProviderId.Hash.ToHexString();
			Result.ProviderGeneration =
				OutOutput.ProviderGeneration.Hash.ToHexString();
			Result.ArtifactSetDigest =
				OutOutput.ArtifactSetDigest.ToHexString();
			for (const FAngelscriptJITGeneratedModule& Module : OutOutput.Modules)
			{
				Result.GeneratedModules.Add(FString::Printf(
					TEXT("%s [%s] -> %s (%u functions)"),
					*Module.CanonicalModuleName,
					*Module.ModuleKey.Hash.ToHexString(),
					*Module.SourceRelativePath,
					Module.FunctionCount));
			}
			for (const FAngelscriptJITGeneratedFunction& Function :
				OutOutput.Functions)
			{
				Result.GeneratedFunctionKeys.Add(
					Function.FunctionKey.Hash.ToHexString());
			}
			return true;
#endif
		}

		static FString DescribeDifferences(
			const FAngelscriptJITGeneratedFileComparison& Comparison)
		{
			TArray<FString> Lines;
			for (const FAngelscriptJITGeneratedFileDifferenceRecord& Difference :
				Comparison.Differences)
			{
				Lines.Add(FString::Printf(TEXT("%s: %s"),
					*Difference.RelativePath, *Difference.Detail));
			}
			return FString::Join(Lines, TEXT("; "));
		}
	}

	FStaticJITAotFixtureEngineSession::~FStaticJITAotFixtureEngineSession()
	{
		Engine.Reset();
		if (FixtureRoot.Contains(
			TEXT("/Saved/Automation/AngelscriptTestJITGeneration/")))
		{
			IFileManager::Get().DeleteDirectory(*FixtureRoot, false, true);
		}
	}

	FStaticJITAotCacheV2RestoreResult
	FStaticJITAotFixtureEngineSession::RestoreFreshEngineFromCacheV2()
	{
		FStaticJITAotCacheV2RestoreResult Result;
		const auto Fail = [&Result](const FString& Detail)
		{
			Result.Detail = Detail;
			return Result;
		};

		if (!IsValid())
		{
			return Fail(TEXT(
				"StaticJIT Cache V2 restore requires a valid source fixture Engine."));
		}
		if (Composition != EStaticJITAotFixtureComposition::Core)
		{
			return Fail(TEXT(
				"StaticJIT Cache V2 restore requires the core-only fixture composition; JIT-only native-call modules are intentionally outside this Cache proof."));
		}

		const FAngelscriptCacheFlushApiResult Flush =
			FlushAngelscriptCacheToStore(Engine.Get(), 5.0);
		if (!Flush.IsSuccess() || !Flush.Flush.Current.IsSuccess())
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Cache V2 producer flush failed: ApiError=%u LifecycleError=%u Detail=%s"),
				static_cast<uint32>(Flush.Error),
				static_cast<uint32>(Flush.Flush.Error),
				*Flush.Detail));
		}
		const FAngelscriptHash256 ProducerGeneration =
			Flush.Flush.Current.GenerationId;
		const FAngelscriptCacheService* const ProducerCacheService =
			Engine->GetCacheService();
		const FAngelscriptCacheLifecyclePublications ProducerPublications =
			ProducerCacheService != nullptr
				? ProducerCacheService->GetLifecyclePublications()
				: FAngelscriptCacheLifecyclePublications{};
		if (!ProducerPublications.Current.IsValid()
			|| ProducerPublications.Current->Modules.IsEmpty())
		{
			return Fail(TEXT(
				"StaticJIT Cache V2 producer did not expose the immutable current publication used by the successful flush."));
		}

		TArray<FAngelscriptStableModuleKey> ExpectedModuleKeys;
		TArray<FAngelscriptStableFunctionKey> ExpectedFunctionKeys;
		TArray<FAngelscriptStableFunctionKey> ExpectedProviderFunctionKeys;
		int32 ExpectedProviderExcludedDestructorCount = 0;
		for (const FAngelscriptCacheCleanModuleArtifacts& Module
			: ProducerPublications.Current->Modules)
		{
			if (Module.ModuleKey.Hash.IsZero()
				|| ExpectedModuleKeys.Contains(Module.ModuleKey))
			{
				return Fail(TEXT(
					"StaticJIT Cache V2 producer publication contained an invalid or duplicate module key."));
			}
			ExpectedModuleKeys.Add(Module.ModuleKey);
			for (const FAngelscriptFunctionArtifactIdentity& Identity
				: Module.ValidatedFunctionArtifactIdentities)
			{
				if (Identity.FunctionKey.Hash.IsZero()
					|| ExpectedFunctionKeys.Contains(Identity.FunctionKey))
				{
					return Fail(TEXT(
						"StaticJIT Cache V2 producer publication contained an invalid or duplicate function key."));
				}
				ExpectedFunctionKeys.Add(Identity.FunctionKey);
			}

			const TSharedPtr<FAngelscriptModuleDesc> SourceModule =
				Engine->GetModuleByModuleName(Module.CanonicalModuleName);
			asCModule* const ScriptModule = SourceModule.IsValid()
				? SourceModule->ScriptModule : nullptr;
			if (ScriptModule == nullptr)
			{
				return Fail(FString::Printf(TEXT(
					"StaticJIT Cache V2 proof could not resolve producer module '%s' while classifying Provider-compatible functions."),
					*Module.CanonicalModuleName));
			}
			for (asUINT FunctionIndex = 0;
				FunctionIndex < ScriptModule->scriptFunctions.GetLength();
				++FunctionIndex)
			{
				asCScriptFunction* const Function =
					ScriptModule->scriptFunctions[FunctionIndex];
				if (Function == nullptr || Function->module != ScriptModule)
				{
					continue;
				}
				FAngelscriptStableFunctionKey FunctionKey;
				if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
					Module.ModuleKey, *Function, FunctionKey)
					|| !Module.ValidatedFunctionArtifactIdentities.ContainsByPredicate(
						[&FunctionKey](const auto& Candidate)
						{
							return Candidate.FunctionKey == FunctionKey;
						}))
				{
					continue;
				}
				if (Function->traits.GetTrait(asTRAIT_DESTRUCTOR))
				{
					++ExpectedProviderExcludedDestructorCount;
					continue;
				}
				ExpectedProviderFunctionKeys.Add(FunctionKey);
			}
		}
		Result.ExpectedCandidateModuleCount = ExpectedModuleKeys.Num();
		Result.ExpectedRestoredFunctionCount = ExpectedFunctionKeys.Num();
		if (Result.ExpectedRestoredFunctionCount <= 0)
		{
			return Fail(TEXT(
				"StaticJIT Cache V2 producer publication did not contain any validated function artifacts."));
		}

		// The import provider and primary fixture are the stable minimum of this
		// proof. Additional fixture modules participate only when Cache V2 actually
		// froze them. In particular, a module that materializes a statics class is
		// compiled by the fresh Engine until that shape becomes Cache-compatible.
		const FName RequiredCacheModuleNames[] = {
			AngelscriptStaticJITAotFixture::GetImportProviderModuleName(),
			AngelscriptStaticJITAotFixture::GetModuleName(),
		};
		for (const FName ModuleName : RequiredCacheModuleNames)
		{
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Engine->GetModuleByModuleName(ModuleName.ToString());
			FAngelscriptStableModuleKey ModuleKey;
			FString ModuleKeyError;
			if (!Module.IsValid()
				|| !FAngelscriptCacheStableSymbolIdentity::TryBuildModuleKey(
					*Module, ModuleKey, &ModuleKeyError))
			{
				return Fail(FString::Printf(TEXT(
					"StaticJIT Cache V2 proof could not resolve required module '%s': %s"),
					*ModuleName.ToString(), *ModuleKeyError));
			}
			if (!ExpectedModuleKeys.Contains(ModuleKey))
			{
				return Fail(FString::Printf(TEXT(
					"StaticJIT Cache V2 producer publication omitted required module '%s'."),
					*ModuleName.ToString()));
			}
		}
		int32 CommittedProviderFunctionCount = 0;
		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> ProviderSnapshot =
				FAngelscriptJITProviderRegistry::Get().GetSnapshot();
		if (ProviderSnapshot.IsValid())
		{
			for (const TSharedPtr<const FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>& Provider : ProviderSnapshot->Providers)
			{
				if (Provider.IsValid()
					&& Provider->ProviderId
						== AngelscriptTestJITProbes::GetStableProviderId())
				{
					for (const FAngelscriptJITProviderCatalogEntry& Entry
						: Provider->Entries)
					{
						if (ExpectedProviderFunctionKeys.Contains(
							Entry.FunctionKey))
						{
							++CommittedProviderFunctionCount;
						}
					}
					break;
				}
			}
		}
		if (CommittedProviderFunctionCount
			!= ExpectedProviderFunctionKeys.Num())
		{
			return Fail(FString::Printf(TEXT(
				"StaticJIT Cache V2 proof matched only %d/%d Provider-compatible committed function artifacts; %d cached destructor artifacts remain explicit VM fallback during LEGACY adoption."),
				CommittedProviderFunctionCount,
				ExpectedProviderFunctionKeys.Num(),
				ExpectedProviderExcludedDestructorCount));
		}

		// The producer and all of its Engine-local function/reference objects are
		// gone before the consumer is allocated. Only source files and the
		// persisted Cache V2 generation remain below FixtureRoot.
		Engine.Reset();

		FString CreateError;
		TUniquePtr<FAngelscriptEngine> RestoredEngine =
			CreateUncompiledFixtureEngine(
				FixtureRoot, CreateError, true, nullptr,
				EAngelscriptEnginePurpose::Runtime, Composition,
				PrivateBridgeRegistration, false, true);
		if (!RestoredEngine.IsValid())
		{
			Error = CreateError;
			return Fail(CreateError);
		}
		FAngelscriptCacheService* const CacheService =
			RestoredEngine->GetCacheService();
		if (CacheService != nullptr)
		{
			// Preserve the complete core-module lookup sequence plus the subsequent
			// stable-route publication events so a late failure still names its
			// original function-level rejection.
			CacheService->ConfigureDecisionTrace(true, 512);
		}

		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		{
			ON_SCOPE_EXIT
			{
				FAngelscriptCompilationEvents::UnregisterListener(Listener);
			};
			FAngelscriptEngineScope EngineScope(*RestoredEngine);
			RestoredEngine->InitialCompile();
		}
		Result.FrontendEventCount = CountFrontendEvents(Events);

		const TSharedPtr<FAngelscriptModuleDesc> ImportModule =
			RestoredEngine->GetModuleByModuleName(
				AngelscriptStaticJITAotFixture::
					GetImportProviderModuleName().ToString());
		const TSharedPtr<FAngelscriptModuleDesc> PrimaryModule =
			RestoredEngine->GetModuleByModuleName(
				AngelscriptStaticJITAotFixture::GetModuleName().ToString());
		const FAngelscriptCacheFunctionReuseSummary FunctionReuse =
			CacheService != nullptr
				? CacheService->CaptureFunctionReuseSummary()
				: FAngelscriptCacheFunctionReuseSummary{};
		Result.bConsumedPersistedCandidate = FunctionReuse.bPresent;
		Result.CandidateModuleCount = static_cast<int32>(
			FunctionReuse.CandidateModuleCount);
		Result.RestoredFunctionCount = static_cast<int32>(
			FunctionReuse.RestoredFunctionCount);
		Result.CompiledMissCount = static_cast<int32>(
			FunctionReuse.CompiledMissCount);
		Result.NotCacheableCount = static_cast<int32>(
			FunctionReuse.NotCacheableCount);

		const FAngelscriptCacheLifecyclePublications Publications =
			CacheService != nullptr
				? CacheService->GetLifecyclePublications()
				: FAngelscriptCacheLifecyclePublications{};
		const FAngelscriptHash256 CandidateGeneration =
			FunctionReuse.CandidateGenerationId;
		Result.GenerationId = CandidateGeneration.ToHexString();
		TArray<FString> DecisionParts;
		if (CacheService != nullptr)
		{
			const FAngelscriptCacheDecisionTraceSnapshot Trace =
				CacheService->CaptureDecisionTrace();
			for (const FAngelscriptCacheDecisionEvent& Event : Trace.Events)
			{
				const bool bStartupDecision =
					Event.Stage
						== EAngelscriptCacheDecisionStage::StartupSelection
					|| Event.Stage
						== EAngelscriptCacheDecisionStage::StartupRestore;
				const bool bFailedFunctionDecision =
					Event.Stage
						== EAngelscriptCacheDecisionStage::FunctionLookup
					&& Event.Outcome
						!= EAngelscriptCacheDecisionOutcome::Restored;
				if (!bStartupDecision && !bFailedFunctionDecision)
				{
					continue;
				}
				DecisionParts.Add(FString::Printf(
					TEXT("stage=%u outcome=%u domain=%u reason=%u expected=%s current=%s detail=%s"),
					static_cast<uint32>(Event.Stage),
					static_cast<uint32>(Event.Outcome),
					static_cast<uint32>(Event.ReasonDomain),
					Event.ReasonCode,
					Event.ExpectedCoordinate.IsSet()
						? *Event.ExpectedCoordinate->ToHexString() : TEXT("none"),
					Event.CurrentCoordinate.IsSet()
						? *Event.CurrentCoordinate->ToHexString() : TEXT("none"),
					*Event.Detail));
			}
		}
		const FString DecisionDetail = DecisionParts.IsEmpty()
			? TEXT("no startup or failed function decision events")
			: FString::Join(DecisionParts, TEXT(" | "));

		// The primary fixture deliberately contains reflected classes and many
		// functions. Cache V2's executable exact-start vertical currently accepts
		// only its narrow enum/global shape, so this production-shaped module uses
		// the normal typed hybrid path: select the persisted Generation, rebuild
		// current module/class authority, and restore every cacheable function body
		// into the fresh Engine's own function objects. Do not misreport that as an
		// exact whole-module bLoadedIncrementalCache restoration.
		if (!ImportModule.IsValid() || !PrimaryModule.IsValid()
			|| !Result.bConsumedPersistedCandidate
			|| Result.CandidateModuleCount
				!= Result.ExpectedCandidateModuleCount
			|| Result.RestoredFunctionCount
				!= Result.ExpectedRestoredFunctionCount
			|| CandidateGeneration != ProducerGeneration
			|| !Publications.Current.IsValid()
			|| Publications.Current->bRestoredFromStore)
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Cache V2 fresh Engine reuse rejected: Producer=%s Candidate=%s Consumed=%d Modules=%d/%d RestoredFunctions=%d/%d CompiledMisses=%d NotCacheable=%d ExactModules=%d FrontendEvents=%d Decisions=[%s]"),
				*ProducerGeneration.ToHexString(),
				*CandidateGeneration.ToHexString(),
				Result.bConsumedPersistedCandidate ? 1 : 0,
				Result.CandidateModuleCount,
				Result.ExpectedCandidateModuleCount,
				Result.RestoredFunctionCount,
				Result.ExpectedRestoredFunctionCount,
				Result.CompiledMissCount,
				Result.NotCacheableCount,
				Publications.Current.IsValid()
					&& Publications.Current->bRestoredFromStore ? 1 : 0,
				Result.FrontendEventCount,
				*DecisionDetail));
		}

		Engine = MoveTemp(RestoredEngine);
		Error.Reset();
		Result.bSuccess = true;
		Result.Detail = FString::Printf(
			TEXT("StaticJIT Cache V2 fresh Engine reused: Generation=%s Modules=%d/%d RestoredFunctions=%d/%d CompiledMisses=%d NotCacheable=%d FrontendEvents=%d"),
			*Result.GenerationId,
			Result.CandidateModuleCount,
			Result.ExpectedCandidateModuleCount,
			Result.RestoredFunctionCount,
			Result.ExpectedRestoredFunctionCount,
			Result.CompiledMissCount,
			Result.NotCacheableCount,
			Result.FrontendEventCount);
		return Result;
	}

	TUniquePtr<FStaticJITAotFixtureEngineSession>
	CreateFixtureEngineSession(
		const bool bEnableCacheV2Persistence,
		const EStaticJITAotFixtureComposition Composition,
		const AngelscriptStaticJITAotFixture::
			ETypedASTPrivateBridgeRegistration PrivateBridgeRegistration)
	{
		TUniquePtr<FStaticJITAotFixtureEngineSession> Session =
			MakeUnique<FStaticJITAotFixtureEngineSession>();
		Session->Composition = Composition;
		Session->PrivateBridgeRegistration = PrivateBridgeRegistration;
		FStaticJITAotGenerationResult CreationResult;
		Session->Engine = CreateFixtureEngine(
			Session->FixtureRoot, CreationResult,
			bEnableCacheV2Persistence, nullptr,
			EAngelscriptEnginePurpose::Runtime, Composition,
			PrivateBridgeRegistration,
			nullptr,
			false,
			true);
		Session->Error = MoveTemp(CreationResult.Error);
		return Session;
	}

	TUniquePtr<FStaticJITAotFixtureEngineSession>
	CreateInterpreterFixtureEngineSession()
	{
		TUniquePtr<FStaticJITAotFixtureEngineSession> Session =
			MakeUnique<FStaticJITAotFixtureEngineSession>();
		Session->Composition = EStaticJITAotFixtureComposition::Core;
		FStaticJITAotGenerationResult CreationResult;
		Session->Engine = CreateFixtureEngine(
			Session->FixtureRoot, CreationResult,
			false, nullptr,
			EAngelscriptEnginePurpose::StaticJITGeneration,
			EStaticJITAotFixtureComposition::Core,
			AngelscriptStaticJITAotFixture::
				ETypedASTPrivateBridgeRegistration::Primary,
			nullptr,
			true);
		Session->Error = MoveTemp(CreationResult.Error);
		return Session;
	}

	TUniquePtr<FStaticJITAotFixtureEngineSession>
	CreateCompleteInterpreterFixtureEngineSession()
	{
		TUniquePtr<FStaticJITAotFixtureEngineSession> Session =
			MakeUnique<FStaticJITAotFixtureEngineSession>();
		Session->Composition = EStaticJITAotFixtureComposition::Complete;
		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		if (!FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::TypedAST(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				GenerationProfile,
				&Session->Error))
		{
			return Session;
		}
		FStaticJITAotGenerationResult CreationResult;
		Session->Engine = CreateFixtureEngine(
			Session->FixtureRoot,
			CreationResult,
			false,
			&GenerationProfile,
			EAngelscriptEnginePurpose::StaticJITGeneration,
			EStaticJITAotFixtureComposition::Complete,
			AngelscriptStaticJITAotFixture::
				ETypedASTPrivateBridgeRegistration::Primary);
		Session->Error = MoveTemp(CreationResult.Error);
		return Session;
	}

	TUniquePtr<FStaticJITAotFixtureEngineSession>
	CreateFoldedGlobalInvalidationFixtureEngineSession()
	{
		TUniquePtr<FStaticJITAotFixtureEngineSession> Session =
			MakeUnique<FStaticJITAotFixtureEngineSession>();
		Session->Composition = EStaticJITAotFixtureComposition::Core;
		FString ChangedSource =
			AngelscriptStaticJITAotFixture::GetTypedFinalScalarScriptSource();
		const int32 ReplacementCount = ChangedSource.ReplaceInline(
			TEXT("const int TypedASTDifferentialFoldedGlobal = 7001;"),
			TEXT("const int TypedASTDifferentialFoldedGlobal = 9001;"),
			ESearchCase::CaseSensitive);
		if (ReplacementCount != 1)
		{
			Session->Error = FString::Printf(TEXT(
				"Folded-global invalidation fixture expected exactly one source replacement, found %d."),
				ReplacementCount);
			return Session;
		}

		FStaticJITAotGenerationResult CreationResult;
		Session->Engine = CreateFixtureEngine(
			Session->FixtureRoot,
			CreationResult,
			false,
			nullptr,
			EAngelscriptEnginePurpose::Runtime,
			EStaticJITAotFixtureComposition::Core,
			AngelscriptStaticJITAotFixture::
				ETypedASTPrivateBridgeRegistration::Primary,
			&ChangedSource,
			false,
			true);
		Session->Error = MoveTemp(CreationResult.Error);
		return Session;
	}

	static FStaticJITAotGenerationResult RunInternal(
		const EStaticJITAotGenerationMode Mode,
		const EStaticJITAotGenerationTestVariant Variant)
	{
		FStaticJITAotGenerationResult Result;
		if (Variant != EStaticJITAotGenerationTestVariant::Default
			&& Mode != EStaticJITAotGenerationMode::Verify)
		{
			Result.Error = TEXT(
				"StaticJIT AOT test generation variants are read-only and require Verify mode.");
			return Result;
		}
		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		if (!FAngelscriptStaticJITGenerationProfile::TryCreate(
			FAngelscriptStaticJITBackendId::TypedAST(),
			EAngelscriptJITTargetProfile::EditorDevelopment,
			GenerationProfile, &Result.Error))
		{
			return Result;
		}
		using ECapability = EAngelscriptTypedASTJITDirectCapability;
		FString VariantError;
		switch (Variant)
		{
		case EStaticJITAotGenerationTestVariant::Default:
			break;
		case EStaticJITAotGenerationTestVariant::RecursionOnlyProfile:
			if (!BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
				ECapability::RecursionBudget,
				GenerationProfile.TypedASTJITCapabilityProfile,
				&VariantError))
			{
				Result.Error = MoveTemp(VariantError);
				return Result;
			}
			break;
		case EStaticJITAotGenerationTestVariant::
			RequireCoverageWithoutCapability:
			if (!BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
				ECapability::Coverage,
				GenerationProfile.TypedASTJITExecutionRequirements,
				&VariantError))
			{
				Result.Error = MoveTemp(VariantError);
				return Result;
			}
			break;
		case EStaticJITAotGenerationTestVariant::
			AdvertiseUnsupportedEmitterCapability:
			if (!BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
				ECapability::FramePosition
					| ECapability::RecursionBudget
					| ECapability::Coverage,
				GenerationProfile.TypedASTJITCapabilityProfile,
				&VariantError))
			{
				Result.Error = MoveTemp(VariantError);
				return Result;
			}
			break;
		default:
			Result.Error = TEXT(
				"StaticJIT AOT generation received an unknown test variant.");
			return Result;
		}
		TUniquePtr<FStaticJITAotFixtureEngineSession> Session =
			MakeUnique<FStaticJITAotFixtureEngineSession>();
		Session->Composition = EStaticJITAotFixtureComposition::Complete;
		FStaticJITAotGenerationResult CreationResult;
		Session->Engine = CreateFixtureEngine(
			Session->FixtureRoot, CreationResult, false,
			&GenerationProfile,
			EAngelscriptEnginePurpose::StaticJITGeneration,
			EStaticJITAotFixtureComposition::Complete,
			AngelscriptStaticJITAotFixture::
				ETypedASTPrivateBridgeRegistration::Primary);
		Session->Error = MoveTemp(CreationResult.Error);
		if (!Session.IsValid() || !Session->IsValid())
		{
			Result.Error = Session.IsValid()
				? Session->Error
				: TEXT("Failed to allocate the TestJIT fixture Engine session.");
			return Result;
		}

		FAngelscriptEngineScope EngineScope(*Session->Engine);
		FAngelscriptJITGenerationOutput Output;
		if (!BuildExpectedOutput(
			*Session->Engine, Session->FixtureRoot, GenerationProfile, Variant,
			Output, Result))
		{
			return Result;
		}

		const FString OutputRoot = GetGeneratedProfileRoot();
		const TArray<FString> LegacyRoots = GetLegacyGeneratedProfileRoots();
		TArray<FAngelscriptJITOwnedOutputInspection> LegacyInspections;
		LegacyInspections.Reserve(LegacyRoots.Num());
		for (const FString& LegacyRoot : LegacyRoots)
		{
			const FAngelscriptJITOwnedOutputInspection Inspection =
				FAngelscriptJITGeneratedFileStore::InspectOwnedOutput(
					LegacyRoot, Output.TargetProfile, Output.ProviderId);
			LegacyInspections.Add(Inspection);
			if (!Inspection.bValid)
			{
				Result.Error = FString::Printf(
					TEXT("AngelscriptTestJIT legacy output '%s' cannot be migrated: %s"),
					*LegacyRoot, *Inspection.Error);
				return Result;
			}
		}
		if (Mode == EStaticJITAotGenerationMode::Generate)
		{
			const FAngelscriptJITGeneratedFilePublication Publication =
				FAngelscriptJITGeneratedFileStore::Publish(OutputRoot, Output);
			Result.bSuccess = Publication.bSuccess;
			Result.Error = Publication.Error;
			Result.WrittenFiles = Publication.WrittenFiles;
			Result.UnchangedFiles = Publication.UnchangedFiles;
			Result.RemovedFiles = Publication.RemovedFiles;
			for (int32 LegacyIndex = 0;
				Result.bSuccess && LegacyIndex < LegacyRoots.Num(); ++LegacyIndex)
			{
				if (!LegacyInspections[LegacyIndex].bExists)
				{
					continue;
				}
				const FAngelscriptJITOwnedOutputRetirement Retired =
					FAngelscriptJITGeneratedFileStore::RetireOwnedOutput(
						LegacyRoots[LegacyIndex], Output.TargetProfile,
						Output.ProviderId);
				Result.bSuccess = Retired.bSuccess;
				Result.Error = Retired.Error;
				Result.RemovedFiles.Append(Retired.RemovedFiles);
				Result.RemovedFiles.Sort();
			}
			if (Result.bSuccess)
			{
				RemoveEmptyLegacyPrivateWrappers();
			}
			return Result;
		}

		const FAngelscriptJITGeneratedFileComparison Comparison =
			FAngelscriptJITGeneratedFileStore::Compare(OutputRoot, Output);
		const bool bLegacyCurrent = LegacyInspections.ContainsByPredicate(
			[](const FAngelscriptJITOwnedOutputInspection& Inspection)
			{
				return !Inspection.bValid || Inspection.bExists;
			}) == false;
		Result.bSuccess = Comparison.IsCurrent() && bLegacyCurrent;
		if (!Result.bSuccess)
		{
			Result.Error = TEXT("AngelscriptTestJIT Verify found stale output: ")
				+ DescribeDifferences(Comparison);
			if (!bLegacyCurrent)
			{
				Result.Error +=
					TEXT("; legacy Private generated output remains or is invalid");
			}
		}
		return Result;
	}

	FStaticJITAotGenerationResult Run(const EStaticJITAotGenerationMode Mode)
	{
		return RunInternal(
			Mode, EStaticJITAotGenerationTestVariant::Default);
	}

	FStaticJITAotGenerationResult RunBytecodeIsolationProbe()
	{
		FStaticJITAotGenerationResult Result;
		FAngelscriptJITGenerationOutput ScratchOutput;
		if (!AppendIsolatedDifferentialArtifact(
				FAngelscriptStaticJITBackendId::Bytecode(),
				TEXT("BytecodeJIT"),
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarModuleName(),
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarReceiverClassName(),
				"DifferentialScalarValue",
				true,
				TEXT("AS_Test_BytecodeIsolation_Bytecode"),
				TEXT("BytecodeIsolationBytecode.generated.cpp"),
				ScratchOutput,
				Result)
			|| !AppendIsolatedDifferentialArtifact(
				FAngelscriptStaticJITBackendId::TypedAST(),
				TEXT("TypedASTJIT"),
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarModuleName(),
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarReceiverClassName(),
				"DifferentialScalarValue",
				true,
				TEXT("AS_Test_BytecodeIsolation_TypedAST"),
				TEXT("BytecodeIsolationTypedAST.generated.cpp"),
				ScratchOutput,
				Result))
		{
			return Result;
		}

		Result.bSuccess = true;
		return Result;
	}

	static int32 FindGeneratedFileBytes(
		const FAngelscriptJITGenerationOutput& Output,
		const TCHAR* const RelativePath)
	{
		const FAngelscriptJITGeneratedFile* const File = Output.Files.FindByPredicate(
			[RelativePath](const FAngelscriptJITGeneratedFile& Candidate)
			{
				return Candidate.RelativePath == RelativePath;
			});
		return File != nullptr ? File->Content.Len() : 0;
	}

	FStaticJITAotFirstVersionBenchmark RunFirstVersionBenchmark()
	{
		FStaticJITAotFirstVersionBenchmark Benchmark;
		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		if (!FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::TypedAST(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				GenerationProfile,
				&Benchmark.Error))
		{
			return Benchmark;
		}

		const uint64 WorkingSetBefore =
			FPlatformMemory::GetStats().UsedPhysical;
		const double CanonicalASTStart = FPlatformTime::Seconds();
		FStaticJITAotGenerationResult CanonicalASTResult;
		FString CanonicalASTRoot;
		TUniquePtr<FAngelscriptEngine> CanonicalASTEngine = CreateFixtureEngine(
			CanonicalASTRoot,
			CanonicalASTResult,
			false,
			&GenerationProfile,
			EAngelscriptEnginePurpose::StaticJITGeneration,
			EStaticJITAotFixtureComposition::Core,
			AngelscriptStaticJITAotFixture::
				ETypedASTPrivateBridgeRegistration::Primary);
		Benchmark.CanonicalASTBuildMilliseconds =
			(FPlatformTime::Seconds() - CanonicalASTStart) * 1000.0;
		const uint64 WorkingSetAfter =
			FPlatformMemory::GetStats().UsedPhysical;
		Benchmark.CanonicalASTWorkingSetDeltaBytes =
			static_cast<int64>(WorkingSetAfter) -
			static_cast<int64>(WorkingSetBefore);
		if (!CanonicalASTEngine.IsValid())
		{
			Benchmark.Error = CanonicalASTResult.Error.IsEmpty()
				? TEXT("First-version Canonical AST generation Engine failed.")
				: CanonicalASTResult.Error;
			return Benchmark;
		}
		CanonicalASTEngine.Reset();

		FAngelscriptJITGenerationOutput ScratchOutput;
		FStaticJITAotGenerationResult IsolatedResult;
		const double BytecodeStart = FPlatformTime::Seconds();
		if (!AppendIsolatedDifferentialArtifact(
				FAngelscriptStaticJITBackendId::Bytecode(),
				TEXT("BytecodeJIT"),
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarModuleName(),
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarReceiverClassName(),
				"DifferentialScalarValue",
				true,
				TEXT("AS_Test_Benchmark_Bytecode"),
				TEXT("BenchmarkBytecode.generated.cpp"),
				ScratchOutput,
				IsolatedResult))
		{
			Benchmark.Error = IsolatedResult.Error;
			return Benchmark;
		}
		Benchmark.IsolatedBytecodeGenerationMilliseconds =
			(FPlatformTime::Seconds() - BytecodeStart) * 1000.0;
		Benchmark.IsolatedBytecodeGeneratedCppBytes = FindGeneratedFileBytes(
			ScratchOutput, TEXT("BenchmarkBytecode.generated.cpp"));

		const double TypedStart = FPlatformTime::Seconds();
		if (!AppendIsolatedDifferentialArtifact(
				FAngelscriptStaticJITBackendId::TypedAST(),
				TEXT("TypedASTJIT"),
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarModuleName(),
				AngelscriptStaticJITAotFixture::
					GetTypedFinalScalarReceiverClassName(),
				"DifferentialScalarValue",
				true,
				TEXT("AS_Test_Benchmark_TypedAST"),
				TEXT("BenchmarkTypedAST.generated.cpp"),
				ScratchOutput,
				IsolatedResult))
		{
			Benchmark.Error = IsolatedResult.Error;
			return Benchmark;
		}
		Benchmark.IsolatedTypedGenerationMilliseconds =
			(FPlatformTime::Seconds() - TypedStart) * 1000.0;
		Benchmark.IsolatedTypedGeneratedCppBytes = FindGeneratedFileBytes(
			ScratchOutput, TEXT("BenchmarkTypedAST.generated.cpp"));
		Benchmark.bSuccess = true;
		return Benchmark;
	}

	FStaticJITAotGenerationResult RunForTesting(
		const EStaticJITAotGenerationMode Mode,
		const EStaticJITAotGenerationTestVariant Variant)
	{
		return RunInternal(Mode, Variant);
	}
}
