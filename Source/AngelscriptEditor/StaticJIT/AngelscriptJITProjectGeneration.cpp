#include "StaticJIT/AngelscriptJITProjectGeneration.h"

#include "AngelscriptEngine.h"
#include "Artifacts/AngelscriptArtifactIdentity.h"
#include "Artifacts/AngelscriptFunctionRoute.h"
#include "StaticJIT/AngelscriptJITGeneratedFileStore.h"
#include "StaticJIT/AngelscriptJITProjectScaffold.h"
#include "StaticJIT/AngelscriptJITSourceAuthority.h"
#include "StaticJIT/AngelscriptNativeFormCatalog.h"
#include "StaticJIT/AngelscriptProjectSourceGraph.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"

#include "StartAngelscriptHeaders.h"
#include "angelscript.h"
#include "as_module.h"
#include "EndAngelscriptHeaders.h"

#include "HAL/FileManager.h"
#include "GenericPlatform/GenericPlatformProperties.h"
#include "Misc/EngineVersion.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

namespace AngelscriptEditor::StaticJIT
{
	namespace
	{
		FString FullDirectory(const FStringView Directory)
		{
			FString Result(Directory);
			Result.TrimStartAndEndInline();
			if (Result.IsEmpty())
			{
				return {};
			}
			Result = FPaths::ConvertRelativePathToFull(Result);
			FPaths::NormalizeDirectoryName(Result);
			return Result;
		}

		bool IsSameOrNested(const FString& Candidate, const FString& Parent)
		{
			return Candidate == Parent
				|| FPaths::IsUnderDirectory(Candidate, Parent);
		}

		const TCHAR* DifferenceKindName(
			const EAngelscriptJITGeneratedFileDifference Kind)
		{
			switch (Kind)
			{
			case EAngelscriptJITGeneratedFileDifference::Missing:
				return TEXT("Missing");
			case EAngelscriptJITGeneratedFileDifference::ContentMismatch:
				return TEXT("ContentMismatch");
			case EAngelscriptJITGeneratedFileDifference::UnexpectedOwnedFile:
				return TEXT("UnexpectedOwnedFile");
			case EAngelscriptJITGeneratedFileDifference::OwnershipConflict:
				return TEXT("OwnershipConflict");
			case EAngelscriptJITGeneratedFileDifference::TargetProfileMismatch:
				return TEXT("TargetProfileMismatch");
			case EAngelscriptJITGeneratedFileDifference::ProviderIdMismatch:
				return TEXT("ProviderIdMismatch");
			case EAngelscriptJITGeneratedFileDifference::InvalidInventory:
				return TEXT("InvalidInventory");
			case EAngelscriptJITGeneratedFileDifference::InvalidExpectedOutput:
				return TEXT("InvalidExpectedOutput");
			case EAngelscriptJITGeneratedFileDifference::ReadFailure:
				return TEXT("ReadFailure");
			default:
				return TEXT("Unknown");
			}
		}

		FString OneLine(FString Value)
		{
			Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));
			Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
			return Value;
		}

		FString BuildVerificationReport(
			const FAngelscriptJITGenerationOutput& Output,
			const FAngelscriptJITGeneratedFileComparison& Comparison)
		{
			FString Report;
			Report += FString::Printf(TEXT("Status=%s\n"),
				Comparison.IsCurrent() ? TEXT("Current") : TEXT("Stale"));
			Report += FString::Printf(TEXT("ProviderId=%s\n"),
				*Output.ProviderId.Hash.ToHexString());
			Report += FString::Printf(TEXT("ProviderGeneration=%s\n"),
				*Output.ProviderGeneration.Hash.ToHexString());
			Report += FString::Printf(TEXT("Profile=%s\n"),
				FAngelscriptJITTargetProfiles::ToString(Output.TargetProfile));
			Report += FString::Printf(TEXT("ProviderAbiRevision=%u\n"),
				FAngelscriptJITProviderAbi::Revision);
			Report += FString::Printf(TEXT("ArtifactSetDigest=%s\n"),
				*Output.ArtifactSetDigest.ToHexString());
			Report += FString::Printf(TEXT("ArtifactProfile=%s\n"),
				*Output.ArtifactProfile.Hash.ToHexString());
			Report += FString::Printf(TEXT("NativeEnvironment=%s\n"),
				*Output.NativeEnvironmentFingerprint.ToHexString());
			Report += FString::Printf(TEXT("FunctionCount=%d\n"),
				Output.Functions.Num());
			Report += FString::Printf(TEXT("ModuleCount=%d\n"),
				Output.Modules.Num());
			for (const FAngelscriptJITGeneratedModule& Module : Output.Modules)
			{
				Report += FString::Printf(
					TEXT("Module StableModuleKey=%s CanonicalModuleName=%s VirtualSourcePath=%s Source=%s ContentHash=%s FunctionCount=%u\n"),
					*Module.ModuleKey.Hash.ToHexString(),
					*OneLine(Module.CanonicalModuleName),
					*OneLine(Module.VirtualSourcePath),
					*Module.SourceRelativePath,
					*Module.ContentHash.ToHexString(),
					Module.FunctionCount);
			}
			for (const FAngelscriptJITGeneratedFunction& Function :
				Output.Functions)
			{
				Report += FString::Printf(
					TEXT("Function StableFunctionKey=%s ModuleKey=%s Declaration=%s Source=%s:%u:%u ExecutionHash=%s DebugHash=%s EntryAbiHash=%s ModuleSource=%s Symbol=%s ReferenceCount=%d\n"),
					*Function.FunctionKey.Hash.ToHexString(),
					*Function.ModuleKey.Hash.ToHexString(),
					*OneLine(Function.CanonicalDeclaration),
					*OneLine(Function.SourceSection),
					Function.SourceLine,
					Function.SourceColumn,
					*Function.ExecutionHash.ToHexString(),
					*Function.DebugHash.ToHexString(),
					*Function.EntryAbiHash.ToHexString(),
					*Function.ModuleSourceRelativePath,
					*Function.SymbolPrefix,
					Function.ReferenceSlots.Num());
			}
			Report += FString::Printf(TEXT("DifferenceCount=%d\n"),
				Comparison.Differences.Num());
			for (const FAngelscriptJITGeneratedFileDifferenceRecord& Difference :
				Comparison.Differences)
			{
				Report += FString::Printf(
					TEXT("Difference Kind=%s Path=%s ExpectedHash=%s ActualHash=%s Detail=%s\n"),
					DifferenceKindName(Difference.Kind),
					*Difference.RelativePath,
					*Difference.ExpectedHash.ToHexString(),
					*Difference.ActualHash.ToHexString(),
					*OneLine(Difference.Detail));
			}
			return Report;
		}

		void PopulateModuleSourceSetChanges(
			const FAngelscriptJITGenerationOutput& Output,
			const FAngelscriptJITGeneratedFileComparison& Comparison,
			FAngelscriptJITProjectGenerationResult& OutResult)
		{
			for (const FAngelscriptJITGeneratedFileDifferenceRecord& Difference :
				Comparison.Differences)
			{
				if (Difference.Kind
					== EAngelscriptJITGeneratedFileDifference::Missing
					&& Output.Modules.ContainsByPredicate(
						[Path = Difference.RelativePath](
							const FAngelscriptJITGeneratedModule& Module)
						{
							return Module.SourceRelativePath == Path;
						}))
				{
					OutResult.AddedModuleSources.Add(Difference.RelativePath);
				}
				else if (Difference.Kind
						== EAngelscriptJITGeneratedFileDifference::UnexpectedOwnedFile
					&& Difference.RelativePath.EndsWith(TEXT(".jit.cpp")))
				{
					OutResult.RemovedModuleSources.Add(Difference.RelativePath);
				}
			}
			OutResult.AddedModuleSources.Sort();
			OutResult.RemovedModuleSources.Sort();
		}

		struct FLegacyProfileOutputRoot
		{
			FString Directory;
			FString DisplayRoot;
		};

		TArray<FLegacyProfileOutputRoot> LegacyProfileOutputDirectories(
			const FStringView OutputDirectory,
			const EAngelscriptJITTargetProfile TargetProfile)
		{
			TArray<FLegacyProfileOutputRoot> Result;
			const FString OutputRoot = FullDirectory(OutputDirectory);
			if (OutputRoot.IsEmpty()
				|| FPaths::GetCleanFilename(OutputRoot)
					!= FAngelscriptJITTargetProfiles::ToString(TargetProfile))
			{
				return Result;
			}
			const FString GeneratedRoot = FPaths::GetPath(OutputRoot);
			if (FPaths::GetCleanFilename(GeneratedRoot) != TEXT("Generated"))
			{
				return Result;
			}
			const FString ModuleRoot = FPaths::GetPath(GeneratedRoot);
			const FString ProfileName =
				FAngelscriptJITTargetProfiles::ToString(TargetProfile);
			FLegacyProfileOutputRoot& Direct = Result.AddDefaulted_GetRef();
			Direct.Directory = FPaths::Combine(
				ModuleRoot, TEXT("Private/Generated"), ProfileName);
			Direct.DisplayRoot = FPaths::Combine(
				TEXT("Private/Generated"), ProfileName);
			FPaths::NormalizeDirectoryName(Direct.Directory);
			FPaths::NormalizeFilename(Direct.DisplayRoot);

			FLegacyProfileOutputRoot& Profiles = Result.AddDefaulted_GetRef();
			Profiles.Directory = FPaths::Combine(
				ModuleRoot, TEXT("Private/Generated/Profiles"), ProfileName);
			Profiles.DisplayRoot = FPaths::Combine(
				TEXT("Private/Generated/Profiles"), ProfileName);
			FPaths::NormalizeDirectoryName(Profiles.Directory);
			FPaths::NormalizeFilename(Profiles.DisplayRoot);
			return Result;
		}

		FString LegacyDisplayPath(
			const FLegacyProfileOutputRoot& LegacyRoot,
			const FStringView RelativePath)
		{
			FString Result = FPaths::Combine(
				LegacyRoot.DisplayRoot,
				FString(RelativePath));
			FPaths::NormalizeFilename(Result);
			return Result;
		}

		void RemoveEmptyLegacyPrivateWrappers(
			const TConstArrayView<FLegacyProfileOutputRoot> LegacyRoots)
		{
			if (LegacyRoots.IsEmpty())
			{
				return;
			}
			const FString PrivateGeneratedRoot = FPaths::GetPath(
				LegacyRoots[0].Directory);
			const FString PrivateRoot = FPaths::GetPath(PrivateGeneratedRoot);
			IFileManager::Get().DeleteDirectory(
				*FPaths::Combine(PrivateGeneratedRoot, TEXT("Profiles")),
				false, false);
			IFileManager::Get().DeleteDirectory(
				*PrivateGeneratedRoot, false, false);
			IFileManager::Get().DeleteDirectory(*PrivateRoot, false, false);
		}

		void AddLegacyDifference(
			FAngelscriptJITGeneratedFileComparison& Comparison,
			const EAngelscriptJITGeneratedFileDifference Kind,
			const FString& RelativePath,
			const FString& Detail)
		{
			FAngelscriptJITGeneratedFileDifferenceRecord& Difference =
				Comparison.Differences.AddDefaulted_GetRef();
			Difference.Kind = Kind;
			Difference.RelativePath = RelativePath;
			Difference.Detail = Detail;
		}
	}

	FString FAngelscriptJITProjectGeneration::GetProfileOutputDirectory(
		const FStringView ProjectFile,
		const EAngelscriptJITTargetProfile TargetProfile)
	{
		FString Descriptor(ProjectFile);
		Descriptor.TrimStartAndEndInline();
		if (Descriptor.IsEmpty()
			|| !FAngelscriptJITTargetProfiles::GetTraits(
				TargetProfile).IsSet())
		{
			return {};
		}
		Descriptor = FPaths::ConvertRelativePathToFull(Descriptor);
		FPaths::NormalizeFilename(Descriptor);
		FString Result = FPaths::Combine(
			FPaths::GetPath(Descriptor),
			TEXT("Source/AngelscriptJIT/Generated"),
			FAngelscriptJITTargetProfiles::ToString(TargetProfile));
		Result = FPaths::ConvertRelativePathToFull(Result);
		FPaths::NormalizeDirectoryName(Result);
		return Result;
	}

	bool FAngelscriptJITProjectGeneration::PrepareProjectProfileOutput(
		const FAngelscriptJITGenerationOutput& RuntimeOutput,
		FAngelscriptJITGenerationOutput& OutProjectOutput,
		FString* OutError)
	{
		OutProjectOutput = RuntimeOutput;
		for (FAngelscriptJITGeneratedFile& File : OutProjectOutput.Files)
		{
			if (File.Kind
				== EAngelscriptJITGeneratedFileKind::ProviderSource)
			{
				File.RelativePath = TEXT("Provider.generated.inl");
			}
		}
		return FAngelscriptJITGeneration::RefreshOwnedFileInventory(
			OutProjectOutput, OutError);
	}

	namespace CanonicalASTGenerate_Private
	{
		void CountCatalogRecipes(
			FAngelscriptEngine& Engine,
			const EAngelscriptJITTargetProfile TargetProfile,
			FAngelscriptCanonicalASTGenerateResult& Result)
		{
#if AS_CAN_GENERATE_JIT
			asIScriptEngine* const ScriptEngine = Engine.GetScriptEngine();
			if (ScriptEngine == nullptr)
			{
				return;
			}
			FAngelscriptNativeFormCatalog& Catalog =
				FAngelscriptNativeFormCatalog::Get();
			auto Visit = [&](asIScriptFunction* Function)
			{
				if (Function == nullptr)
				{
					return;
				}
				const FAngelscriptNativeFormCatalogKey Key =
					FAngelscriptNativeFormCatalog::MakeKey(
						*Function, TargetProfile);
				if (Catalog.Find(Key) != nullptr)
				{
					++Result.CatalogRecipeHits;
				}
				else
				{
					++Result.CatalogRecipeMisses;
				}
			};
			for (asUINT Index = 0;
				Index < ScriptEngine->GetGlobalFunctionCount();
				++Index)
			{
				Visit(ScriptEngine->GetGlobalFunctionByIndex(Index));
			}
			for (asUINT TypeIndex = 0;
				TypeIndex < ScriptEngine->GetObjectTypeCount();
				++TypeIndex)
			{
				asITypeInfo* const Type =
					ScriptEngine->GetObjectTypeByIndex(TypeIndex);
				if (Type == nullptr)
				{
					continue;
				}
				for (asUINT MethodIndex = 0;
					MethodIndex < Type->GetMethodCount();
					++MethodIndex)
				{
					Visit(Type->GetMethodByIndex(MethodIndex));
				}
			}
#else
			(void)Engine;
			(void)TargetProfile;
			(void)Result;
#endif
		}

		bool WriteOwnedLease(
			const FStringView OutputDirectory,
			const FStringView ProjectFile,
			const EAngelscriptJITTargetProfile TargetProfile,
			const FAngelscriptCanonicalASTGenerateResult& Result,
			TArray<FString>& OutWritten,
			FString* OutError)
		{
			FString Directory(OutputDirectory);
			Directory.TrimStartAndEndInline();
			if (Directory.IsEmpty())
			{
				return true;
			}
			FAngelscriptJITGenerationOutput Output;
			Output.TargetProfile = TargetProfile;
			Output.ProviderId =
				FAngelscriptJITProjectScaffold::BuildProviderId(ProjectFile);
			FAngelscriptArtifactCanonicalWriter GenerationWriter(
				TEXT("canonical-ast-lease-generation"));
			GenerationWriter.WriteString(FString(ProjectFile));
			GenerationWriter.WriteString(
				FAngelscriptJITTargetProfiles::ToString(TargetProfile));
			GenerationWriter.WriteUInt32(
				static_cast<uint32>(FMath::Max(0, Result.LeasedASTSnapshotCount)));
			GenerationWriter.WriteUInt32(
				static_cast<uint32>(FMath::Max(0, Result.CatalogRecipeHits)));
			Output.ProviderGeneration.Hash = GenerationWriter.FinalizeHash();
			FString Lease = TEXT(
				"// @angelscript-jit-owned revision=2 kind=ProviderManifest\n");
			Lease += FString::Printf(
				TEXT("{\"leasedSnapshots\":%d,\"catalogHits\":%d,\"catalogMisses\":%d}\n"),
				Result.LeasedASTSnapshotCount,
				Result.CatalogRecipeHits,
				Result.CatalogRecipeMisses);
			FAngelscriptJITGeneratedFile& LeaseFile =
				Output.Files.AddDefaulted_GetRef();
			LeaseFile.RelativePath = TEXT("CanonicalASTLease.generated.json");
			LeaseFile.Content = MoveTemp(Lease);
			LeaseFile.Kind =
				EAngelscriptJITGeneratedFileKind::ProviderManifest;
			FAngelscriptJITGeneratedFile& Inventory =
				Output.Files.AddDefaulted_GetRef();
			Inventory.RelativePath = TEXT("OwnedFiles.generated.json");
			Inventory.Kind =
				EAngelscriptJITGeneratedFileKind::OwnedFileInventory;
			FString RefreshError;
			if (!FAngelscriptJITGeneration::RefreshOwnedFileInventory(
					Output, &RefreshError))
			{
				if (OutError != nullptr)
				{
					*OutError = RefreshError;
				}
				return false;
			}
			const FAngelscriptJITGeneratedFilePublication Publication =
				FAngelscriptJITGeneratedFileStore::Publish(
					Directory, Output);
			if (!Publication.bSuccess)
			{
				if (OutError != nullptr)
				{
					*OutError = Publication.Error;
				}
				return false;
			}
			OutWritten = Publication.WrittenFiles;
			OutWritten.Append(Publication.UnchangedFiles);
			return true;
		}

		FAngelscriptCanonicalASTGenerateResult FinishGenerate(
			FAngelscriptCanonicalASTGenerateResult Result,
			const int32 GenerationEnginesBefore,
			const int32 LiveBefore)
		{
#if WITH_ANGELSCRIPT_UNITTESTS
			Result.CreatedGenerationEngineCount =
				FAngelscriptEngine::GetStaticJITGenerationEngineCreateCountForTests()
				- GenerationEnginesBefore;
			Result.LiveGenerationEngineCount =
				FAngelscriptEngine::GetStaticJITGenerationEngineLiveCountForTests();
			Result.MaxLiveGenerationEngines = FMath::Max(
				Result.MaxLiveGenerationEngines,
				FMath::Max(0,
					FAngelscriptEngine::GetStaticJITGenerationEnginePeakLiveCountForTests()
					- LiveBefore));
#else
			(void)GenerationEnginesBefore;
			(void)LiveBefore;
#endif
			return Result;
		}
	}

	FAngelscriptCanonicalASTGenerateResult
	FAngelscriptJITProjectGeneration::GenerateMatchingProfile(
		FAngelscriptEngine& PrimaryEngine,
		const FStringView ProjectFile,
		const EAngelscriptJITTargetProfile TargetProfile)
	{
		FAngelscriptCanonicalASTGenerateRequest Request;
		Request.PrimaryEngine = &PrimaryEngine;
		Request.ProjectFile = FString(ProjectFile);
		Request.TargetProfile = TargetProfile;
		return GenerateMatchingProfile(Request);
	}

	FAngelscriptCanonicalASTGenerateResult
	FAngelscriptJITProjectGeneration::GenerateMatchingProfile(
		const FAngelscriptCanonicalASTGenerateRequest& Request)
	{
		using namespace CanonicalASTGenerate_Private;
		FAngelscriptCanonicalASTGenerateResult Result;
#if WITH_ANGELSCRIPT_UNITTESTS
		const int32 GenerationEnginesBefore =
			FAngelscriptEngine::GetStaticJITGenerationEngineCreateCountForTests();
		const int32 LiveBefore =
			FAngelscriptEngine::GetStaticJITGenerationEngineLiveCountForTests();
#else
		const int32 GenerationEnginesBefore = 0;
		const int32 LiveBefore = 0;
#endif
		if (Request.PrimaryEngine == nullptr)
		{
			Result.Code =
				EAngelscriptCanonicalASTGenerateCode::AuthoritativeEngineStale;
			Result.Detail = TEXT("AuthoritativeEngineStale");
			return FinishGenerate(Result, GenerationEnginesBefore, LiveBefore);
		}
		FAngelscriptEngine& PrimaryEngine = *Request.PrimaryEngine;
		PrimaryEngine.PushHotReloadApplicationFreeze();
		ON_SCOPE_EXIT
		{
			PrimaryEngine.PopHotReloadApplicationFreeze();
		};

		const FAngelscriptJITSourceAuthoritySnapshot Authority =
			FAngelscriptJITSourceAuthority::Capture(
				PrimaryEngine, Request.ProjectFile, Request.TargetProfile);
		if (!Authority.IsCurrent())
		{
			Result.Code =
				EAngelscriptCanonicalASTGenerateCode::AuthoritativeEngineStale;
			Result.Detail = Authority.Detail.IsEmpty()
				? TEXT("AuthoritativeEngineStale")
				: Authority.Detail;
			if (Request.OnHoldingLeases)
			{
				Result.bHotReloadFrozenWhileHeld =
					PrimaryEngine.IsHotReloadApplicationFrozen();
				Request.OnHoldingLeases();
			}
			return FinishGenerate(Result, GenerationEnginesBefore, LiveBefore);
		}

		const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules =
			PrimaryEngine.GetActiveModules();
		TArray<asIASTSnapshot*> Leases;
		ON_SCOPE_EXIT
		{
			for (asIASTSnapshot* Snapshot : Leases)
			{
				if (Snapshot != nullptr)
				{
					Snapshot->Release();
				}
			}
		};
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
		{
			if (Module->bCompileError || Module->ScriptModule == nullptr)
			{
				continue;
			}
			asIScriptModule* const ScriptModule =
				reinterpret_cast<asIScriptModule*>(Module->ScriptModule);
			asIASTSnapshot* const Snapshot =
				ScriptModule->AcquireASTSnapshot(asAST_API_VERSION_1);
			if (Snapshot == nullptr)
			{
				Result.Code =
					EAngelscriptCanonicalASTGenerateCode::ASTSnapshotRequired;
				Result.Detail = TEXT("ASTSnapshotRequired");
				if (Request.OnHoldingLeases)
				{
					Result.bHotReloadFrozenWhileHeld =
						PrimaryEngine.IsHotReloadApplicationFrozen();
					Request.OnHoldingLeases();
				}
				return FinishGenerate(
					Result, GenerationEnginesBefore, LiveBefore);
			}
			Leases.Add(Snapshot);
		}
		if (Leases.IsEmpty())
		{
			Result.Code =
				EAngelscriptCanonicalASTGenerateCode::ASTSnapshotRequired;
			Result.Detail = TEXT("ASTSnapshotRequired");
			if (Request.OnHoldingLeases)
			{
				Result.bHotReloadFrozenWhileHeld =
					PrimaryEngine.IsHotReloadApplicationFrozen();
				Request.OnHoldingLeases();
			}
			return FinishGenerate(Result, GenerationEnginesBefore, LiveBefore);
		}

		CountCatalogRecipes(
			PrimaryEngine, Request.TargetProfile, Result);
		Result.LeasedASTSnapshotCount = Leases.Num();
		Result.bHotReloadFrozenWhileHeld =
			PrimaryEngine.IsHotReloadApplicationFrozen();
		if (Request.OnHoldingLeases)
		{
			Request.OnHoldingLeases();
		}

		Result.bSuccess = true;
		Result.Code = EAngelscriptCanonicalASTGenerateCode::Succeeded;
		Result.bCompiledSource = false;
		FString WriteError;
		if (!WriteOwnedLease(
				Request.OutputDirectory,
				Request.ProjectFile,
				Request.TargetProfile,
				Result,
				Result.WrittenOwnedFiles,
				&WriteError))
		{
			Result.bSuccess = false;
			Result.Detail = WriteError;
			return FinishGenerate(Result, GenerationEnginesBefore, LiveBefore);
		}
		return FinishGenerate(Result, GenerationEnginesBefore, LiveBefore);
	}

	FAngelscriptCanonicalASTGenerateResult
	FAngelscriptJITProjectGeneration::GenerateContainedProfile(
		FAngelscriptEngine& PrimaryEngine,
		const FStringView ProjectFile,
		const EAngelscriptJITTargetProfile TargetProfile,
		const FStringView OutputDirectory)
	{
		using namespace CanonicalASTGenerate_Private;
		FAngelscriptCanonicalASTGenerateResult Result;
#if WITH_ANGELSCRIPT_UNITTESTS
		const int32 GenerationEnginesBefore =
			FAngelscriptEngine::GetStaticJITGenerationEngineCreateCountForTests();
		const int32 LiveBefore =
			FAngelscriptEngine::GetStaticJITGenerationEngineLiveCountForTests();
#else
		const int32 GenerationEnginesBefore = 0;
		const int32 LiveBefore = 0;
#endif
		PrimaryEngine.PushHotReloadApplicationFreeze();
		ON_SCOPE_EXIT
		{
			PrimaryEngine.PopHotReloadApplicationFreeze();
		};
		Result.bHotReloadFrozenWhileHeld =
			PrimaryEngine.IsHotReloadApplicationFrozen();

		FAngelscriptJITGenerationOutput GeneratedOutput;
		FString Error;
		if (!BuildProjectArtifacts(
				ProjectFile, TargetProfile, GeneratedOutput, &Error))
		{
			Result.Code =
				EAngelscriptCanonicalASTGenerateCode::AuthoritativeEngineStale;
			Result.Detail = Error;
			Result.bCompiledSource = true;
			return FinishGenerate(Result, GenerationEnginesBefore, LiveBefore);
		}
		Result.bCompiledSource = true;
		Result.bSuccess = true;
		Result.Code = EAngelscriptCanonicalASTGenerateCode::Succeeded;
		if (!OutputDirectory.IsEmpty())
		{
			FAngelscriptJITGenerationOutput ProjectOutput;
			if (!PrepareProjectProfileOutput(
					GeneratedOutput, ProjectOutput, &Error))
			{
				Result.bSuccess = false;
				Result.Detail = Error;
				return FinishGenerate(
					Result, GenerationEnginesBefore, LiveBefore);
			}
			const FAngelscriptJITProjectGenerationResult Publication =
				Generate(OutputDirectory, ProjectOutput);
			if (!Publication.bSuccess)
			{
				Result.bSuccess = false;
				Result.Detail = Publication.Error;
				return FinishGenerate(
					Result, GenerationEnginesBefore, LiveBefore);
			}
			Result.WrittenOwnedFiles = Publication.Publication.WrittenFiles;
		}
		return FinishGenerate(Result, GenerationEnginesBefore, LiveBefore);
	}

	TArray<FAngelscriptCanonicalASTGenerateResult>
	FAngelscriptJITProjectGeneration::GenerateProfilesSequential(
		FAngelscriptEngine& PrimaryEngine,
		const FStringView ProjectFile,
		const TConstArrayView<EAngelscriptJITTargetProfile> Profiles,
		const FStringView OutputRoot)
	{
		TArray<FAngelscriptCanonicalASTGenerateResult> Results;
		const EAngelscriptJITTargetProfile PrimaryProfile =
			FAngelscriptNativeFormCatalog::InferTargetProfile(PrimaryEngine);
		int32 MaxLive = 0;
		for (const EAngelscriptJITTargetProfile Profile : Profiles)
		{
			FString ProfileOutput;
			if (!OutputRoot.IsEmpty())
			{
				ProfileOutput = FPaths::Combine(
					FString(OutputRoot),
					FAngelscriptJITTargetProfiles::ToString(Profile));
			}
			FAngelscriptCanonicalASTGenerateResult ProfileResult =
				Profile == PrimaryProfile
				? GenerateMatchingProfile(
					PrimaryEngine, ProjectFile, Profile)
				: GenerateContainedProfile(
					PrimaryEngine, ProjectFile, Profile, ProfileOutput);
			MaxLive = FMath::Max(
				MaxLive, ProfileResult.MaxLiveGenerationEngines);
			ProfileResult.MaxLiveGenerationEngines = MaxLive;
			Results.Add(MoveTemp(ProfileResult));
		}
		return Results;
	}

	bool FAngelscriptJITProjectGeneration::BuildProjectArtifacts(
		const FStringView ProjectFile,
		const EAngelscriptJITTargetProfile TargetProfile,
		FAngelscriptJITGenerationOutput& OutGeneratedOutput,
		FString* OutError)
	{
		return BuildProjectArtifacts(ProjectFile, TargetProfile, nullptr,
			OutGeneratedOutput, OutError);
	}

	bool FAngelscriptJITProjectGeneration::BuildProjectArtifacts(
		const FStringView ProjectFile,
		const EAngelscriptJITTargetProfile TargetProfile,
		TSharedPtr<IAngelscriptSourceProvider> SourceProviderOverride,
		FAngelscriptJITGenerationOutput& OutGeneratedOutput,
		FString* OutError)
	{
		OutGeneratedOutput = {};
		auto Fail = [OutError](const FString& Message)
		{
			if (OutError != nullptr)
			{
				*OutError = Message;
			}
			return false;
		};

#if !AS_CAN_GENERATE_JIT
		return Fail(
			TEXT("Project StaticJIT generation is unavailable in this host build."));
#else
		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		FString ProfileError;
		if (!FAngelscriptStaticJITGenerationProfile::TryCreate(
			FAngelscriptStaticJITBackendId::TypedAST(),
			TargetProfile,
			GenerationProfile, &ProfileError))
		{
			return Fail(ProfileError);
		}
		FAngelscriptProjectSourceGraphCompileRequest CompileRequest;
		CompileRequest.ProjectFile = FString(ProjectFile);
		CompileRequest.TargetProfile = TargetProfile;
		CompileRequest.CaptureProfile = GenerationProfile.CaptureProfile;
		CompileRequest.ASTRetentionPolicy =
			GenerationProfile.CaptureProfile
				== EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST
			? 1
			: 0;
		CompileRequest.SourceProviderOverride = MoveTemp(SourceProviderOverride);
		return FAngelscriptProjectSourceGraph::Compile(
			CompileRequest,
			[&OutGeneratedOutput, &GenerationProfile, TargetProfile](
				const FAngelscriptProjectSourceGraphCompileView& View,
				FString* ConsumerError)
			{
				FAngelscriptJITProviderGenerationSettings Settings;
				Settings.TargetProfile = TargetProfile;
				Settings.ProviderId =
					FAngelscriptJITProjectScaffold::BuildProviderId(
						View.ProjectFile);
				Settings.ProviderName = FPaths::GetBaseFilename(
					FString(View.ProjectFile)) + TEXT(" StaticJIT");
				Settings.OwnerModuleName =
					FAngelscriptJITProjectScaffold::ModuleName;
				Settings.KnownNativeCallModules = {
					FName(TEXT("Core")),
					FName(TEXT("AngelscriptRuntime")),
				};
				Settings.PublicNativeCallDependencyModules =
					Settings.KnownNativeCallModules;
				Settings.ArtifactProfile = View.Snapshot->ArtifactProfile;
				Settings.NativeEnvironmentFingerprint =
					FAngelscriptJITTargetProfiles::
						BuildCurrentNativeEnvironmentFingerprint(
							TargetProfile);

				FAngelscriptStaticJITProviderGenerationRequest Request;
				Request.BackendId = FAngelscriptStaticJITBackendId::TypedAST();
				Request.Settings = Settings;
				Request.TypedASTJITCapabilityProfile =
					GenerationProfile.TypedASTJITCapabilityProfile;
				Request.TypedASTJITExecutionRequirements =
					GenerationProfile.TypedASTJITExecutionRequirements;
				Request.CompiledSourceGraph = View.Modules;
				Request.GenerationSnapshot = View.Snapshot;
				FAngelscriptStaticJITGeneratorOutput GeneratorOutput;
				if (!GenerateStaticJITProviderArtifacts(
					Request, GeneratorOutput, ConsumerError))
				{
					return false;
				}
				if (!GeneratorOutput.bHasProviderOutput)
				{
					if (ConsumerError != nullptr)
					{
						*ConsumerError = TEXT(
							"Project StaticJIT generation produced no eligible Provider output.");
					}
					return false;
				}
				OutGeneratedOutput = MoveTemp(
					GeneratorOutput.ProviderOutput);
				return true;
			},
			OutError);
#endif
	}

	FAngelscriptJITProjectGenerationResult
	FAngelscriptJITProjectGeneration::Generate(
		const FStringView OutputDirectory,
		const FAngelscriptJITGenerationOutput& GeneratedOutput)
	{
		FAngelscriptJITProjectGenerationResult Result;
		const TArray<FLegacyProfileOutputRoot> LegacyRoots =
			LegacyProfileOutputDirectories(
			OutputDirectory, GeneratedOutput.TargetProfile);
		TArray<FAngelscriptJITOwnedOutputInspection> LegacyInspections;
		LegacyInspections.Reserve(LegacyRoots.Num());
		for (const FLegacyProfileOutputRoot& LegacyRoot : LegacyRoots)
		{
			const FAngelscriptJITOwnedOutputInspection Inspection =
				FAngelscriptJITGeneratedFileStore::InspectOwnedOutput(
					LegacyRoot.Directory, GeneratedOutput.TargetProfile,
					GeneratedOutput.ProviderId);
			LegacyInspections.Add(Inspection);
			if (!Inspection.bValid)
			{
				Result.Comparison = FAngelscriptJITGeneratedFileStore::Compare(
					OutputDirectory, GeneratedOutput);
				AddLegacyDifference(Result.Comparison,
					EAngelscriptJITGeneratedFileDifference::InvalidInventory,
					LegacyDisplayPath(LegacyRoot,
						TEXT("OwnedFiles.generated.json")),
					Inspection.Error);
				Result.Error = FString::Printf(
					TEXT("Refusing StaticJIT profile migration from '%s': %s"),
					*LegacyRoot.DisplayRoot, *Inspection.Error);
				return Result;
			}
		}
		Result.Publication = FAngelscriptJITGeneratedFileStore::Publish(
			OutputDirectory, GeneratedOutput);
		Result.Comparison = Result.Publication.Comparison;
		Result.AddedModuleSources = Result.Publication.AddedModuleSources;
		Result.RemovedModuleSources = Result.Publication.RemovedModuleSources;
		Result.Error = Result.Publication.Error;
		Result.bSuccess = Result.Publication.bSuccess;
		for (int32 LegacyIndex = 0;
			Result.bSuccess && LegacyIndex < LegacyRoots.Num(); ++LegacyIndex)
		{
			if (!LegacyInspections[LegacyIndex].bExists)
			{
				continue;
			}
			const FLegacyProfileOutputRoot& LegacyRoot =
				LegacyRoots[LegacyIndex];
			const FAngelscriptJITOwnedOutputRetirement Retired =
				FAngelscriptJITGeneratedFileStore::RetireOwnedOutput(
					LegacyRoot.Directory, GeneratedOutput.TargetProfile,
					GeneratedOutput.ProviderId);
			if (!Retired.bSuccess)
			{
				Result.bSuccess = false;
				Result.Error = FString::Printf(
					TEXT("Generated the new StaticJIT profile but could not retire legacy '%s': %s"),
					*LegacyRoot.DisplayRoot, *Retired.Error);
				return Result;
			}
			for (const FString& RelativePath : Retired.RemovedModuleSources)
			{
				Result.RemovedModuleSources.Add(
					LegacyDisplayPath(LegacyRoot,
						RelativePath));
			}
		}
		if (Result.bSuccess)
		{
			Result.RemovedModuleSources.Sort();
			RemoveEmptyLegacyPrivateWrappers(LegacyRoots);
		}
		return Result;
	}

	FAngelscriptJITProjectGenerationResult
	FAngelscriptJITProjectGeneration::Verify(
		const FStringView OutputDirectory,
		const FStringView TemporaryRootDirectory,
		const FAngelscriptJITGenerationOutput& GeneratedOutput)
	{
		FAngelscriptJITProjectGenerationResult Result;
		const FString OutputRoot = FullDirectory(OutputDirectory);
		const FString TemporaryRoot = FullDirectory(TemporaryRootDirectory);
		if (TemporaryRoot.IsEmpty()
			|| OutputRoot.IsEmpty()
			|| IsSameOrNested(TemporaryRoot, OutputRoot)
			|| IsSameOrNested(OutputRoot, TemporaryRoot))
		{
			Result.Error =
				TEXT("StaticJIT Verify requires separate output and temporary roots.");
			return Result;
		}

		Result.VerificationDirectory = FPaths::Combine(
			TemporaryRoot,
			TEXT("AngelscriptJITVerify"),
			GeneratedOutput.ProviderId.Hash.ToHexString(),
			FAngelscriptJITTargetProfiles::ToString(
				GeneratedOutput.TargetProfile),
			GeneratedOutput.ProviderGeneration.Hash.ToHexString(),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		FPaths::NormalizeDirectoryName(Result.VerificationDirectory);
		if (!FPaths::IsUnderDirectory(
			Result.VerificationDirectory, TemporaryRoot))
		{
			Result.Error = TEXT("StaticJIT Verify temporary path escaped its root.");
			return Result;
		}

		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(
				*Result.VerificationDirectory, false, true);
		};

		const FAngelscriptJITGeneratedFilePublication Staged =
			FAngelscriptJITGeneratedFileStore::Publish(
				Result.VerificationDirectory, GeneratedOutput);
		if (!Staged.bSuccess)
		{
			Result.Error = FString::Printf(
				TEXT("StaticJIT Verify could not materialize isolated expected output: %s"),
				*Staged.Error);
			return Result;
		}
		const FAngelscriptJITGeneratedFileComparison StagedComparison =
			FAngelscriptJITGeneratedFileStore::Compare(
				Result.VerificationDirectory, GeneratedOutput);
		if (!StagedComparison.IsCurrent())
		{
			Result.Error =
				TEXT("StaticJIT Verify isolated expected output was not deterministic.");
			return Result;
		}

		Result.Comparison = FAngelscriptJITGeneratedFileStore::Compare(
			OutputRoot, GeneratedOutput);
		PopulateModuleSourceSetChanges(
			GeneratedOutput, Result.Comparison, Result);
		const TArray<FLegacyProfileOutputRoot> LegacyRoots =
			LegacyProfileOutputDirectories(
				OutputRoot, GeneratedOutput.TargetProfile);
		for (const FLegacyProfileOutputRoot& LegacyRoot : LegacyRoots)
		{
			const FAngelscriptJITOwnedOutputInspection LegacyInspection =
				FAngelscriptJITGeneratedFileStore::InspectOwnedOutput(
					LegacyRoot.Directory, GeneratedOutput.TargetProfile,
					GeneratedOutput.ProviderId);
			if (!LegacyInspection.bValid)
			{
				AddLegacyDifference(Result.Comparison,
					EAngelscriptJITGeneratedFileDifference::InvalidInventory,
					LegacyDisplayPath(LegacyRoot,
						TEXT("OwnedFiles.generated.json")),
					LegacyInspection.Error);
			}
			else if (LegacyInspection.bExists)
			{
				for (const FString& RelativePath : LegacyInspection.OwnedFiles)
				{
					AddLegacyDifference(Result.Comparison,
						EAngelscriptJITGeneratedFileDifference::UnexpectedOwnedFile,
						LegacyDisplayPath(LegacyRoot,
							RelativePath),
						TEXT("Legacy Private generated output remains and requires Generate migration."));
				}
				for (const FString& RelativePath : LegacyInspection.ModuleSources)
				{
					Result.RemovedModuleSources.Add(
						LegacyDisplayPath(LegacyRoot,
							RelativePath));
				}
			}
		}
		Result.RemovedModuleSources.Sort();
		Result.bSuccess = Result.Comparison.IsCurrent();
		Result.Report = BuildVerificationReport(
			GeneratedOutput, Result.Comparison);
		if (!Result.bSuccess)
		{
			Result.Error = Result.Report;
		}
		return Result;
	}
}
