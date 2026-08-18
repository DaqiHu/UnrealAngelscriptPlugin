#pragma once

#include "Cache/AngelscriptCacheDiagnostics.h"

#include "AngelscriptEngine.h"
#include "AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITProvider.h"
#include "StaticJIT/AngelscriptJITProviderMatcher.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptJITProviderRouter.h"
#include "StaticJIT/AngelscriptJITReferenceResolver.h"
#include "StaticJIT/BytecodeJIT/AngelscriptBytecodeJIT.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptTypedASTJITProviderDependencyTestSupport
{
	inline FAngelscriptHash256 MakeHash(const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("typed-ast-provider-dependency-runtime"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	inline void ProviderVmEntry(
		FScriptExecution&,
		asDWORD*,
		asQWORD* OutValue)
	{
		if (OutValue != nullptr)
		{
			*OutValue = 700;
		}
	}

	inline asDWORD ProviderRawEntry(FScriptExecution&, void*)
	{
		return 700;
	}

	inline void ProviderParmsEntry(FScriptExecution&, void*, void*)
	{
	}

	class FExactResolver final : public IAngelscriptJITReferenceResolver
	{
	public:
		FAngelscriptJITReferenceResolution Resolve(
			const FAngelscriptArtifactReference&) const override
		{
			return {EAngelscriptJITReferenceResolutionCode::Exact, this};
		}
	};

	class FScopedProjectRoot final
	{
	public:
		explicit FScopedProjectRoot(const TCHAR* FolderName)
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				FString::Printf(TEXT("Automation/%s"), FolderName),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(FString::Printf(
				TEXT("/Saved/Automation/%s/"), FolderName)));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
			SourceFileName = FString(FolderName) + TEXT(".as");
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(TEXT("/Saved/Automation/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source, *(ScriptRoot / SourceFileName));
		}

		FString ModuleName() const
		{
			return FPaths::GetBaseFilename(SourceFileName);
		}

		FString Root;
		FString ScriptRoot;
		FString CacheRoot;
		FString SourceFileName;
	};

	inline TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FScopedProjectRoot& Project)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.CacheV2RootOverride = Project.CacheRoot;

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [&Project]()
		{
			return Project.Root;
		};
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	}

	inline const FAngelscriptFunctionRoute* FindRoute(
		const FAngelscriptFunctionRouteSnapshot& Snapshot,
		const FString& CanonicalDeclaration)
	{
		return Snapshot.FunctionRoutes.FindByPredicate(
			[&CanonicalDeclaration](const FAngelscriptFunctionRoute& Route)
			{
				return Route.CanonicalDeclaration == CanonicalDeclaration;
			});
	}

	inline const FAngelscriptFunctionRoute* FindRouteByName(
		const FAngelscriptFunctionRouteSnapshot& Snapshot,
		const ANSICHAR* FunctionName)
	{
		return Snapshot.FunctionRoutes.FindByPredicate(
			[FunctionName](const FAngelscriptFunctionRoute& Route)
			{
				return Route.Function != nullptr
					&& FCStringAnsi::Strcmp(
						Route.Function->GetName(), FunctionName) == 0;
			});
	}

	inline const FAngelscriptJITProviderRouteRecord* FindRecord(
		const FAngelscriptJITProviderRouteRefreshResult& Result,
		const FAngelscriptStableFunctionKey& FunctionKey)
	{
		return Result.Routes.FindByPredicate(
			[&FunctionKey](const FAngelscriptJITProviderRouteRecord& Record)
			{
				return Record.FunctionKey == FunctionKey;
			});
	}

	inline FAngelscriptHash256 MakeEntryAbi(
		const FAngelscriptFunctionRoute& Route)
	{
		const bool bHasParmsEntry =
			FAngelscriptBytecodeJIT::CanGenerateParmsEntry(Route.Function);
		return FAngelscriptJITGeneration::BuildEntryAbiHash(
			Route.CanonicalDeclaration,
			true,
			true,
			bHasParmsEntry);
	}

	inline FAngelscriptJITFunctionMatchInput MakeMatchInput(
		const FAngelscriptFunctionRoute& Route)
	{
		FAngelscriptJITFunctionMatchInput Input;
		Input.ModuleKey = Route.ModuleKey;
		Input.Identity = Route.Identity;
		Input.EntryAbiHash = MakeEntryAbi(Route);
		Input.NativeEnvironmentFingerprint =
			FAngelscriptJITTargetProfiles::
				BuildCurrentNativeEnvironmentFingerprint(
					EAngelscriptJITTargetProfile::EditorDevelopment);
		Input.RequiredEntryPoints =
			EAngelscriptJITRequiredEntryPoints::VM
			| EAngelscriptJITRequiredEntryPoints::Raw;
		if (FAngelscriptBytecodeJIT::CanGenerateParmsEntry(Route.Function))
		{
			Input.RequiredEntryPoints |=
				EAngelscriptJITRequiredEntryPoints::Parms;
		}
		return Input;
	}

	inline FAngelscriptArtifactSemanticDependency MakeDependency(
		const EAngelscriptArtifactSemanticDependencyKind Kind,
		const EAngelscriptArtifactReferenceKind TargetKind,
		const FAngelscriptHash256& TargetKey,
		const FAngelscriptHash256& ExpectedAbi,
		const TOptional<FAngelscriptHash256>& Expected = {})
	{
		FAngelscriptArtifactSemanticDependency Dependency;
		Dependency.Kind = Kind;
		Dependency.Target.Kind = TargetKind;
		Dependency.Target.StableKey = TargetKey;
		Dependency.Target.ExpectedAbi = ExpectedAbi;
		Dependency.ExpectedContentOrValue = Expected;
		return Dependency;
	}

	inline FAngelscriptArtifactSemanticDependency MakeFunctionContent(
		const FAngelscriptFunctionRoute& Helper)
	{
		return MakeDependency(
			EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
			EAngelscriptArtifactReferenceKind::ScriptFunction,
			Helper.Identity.FunctionKey.Hash,
			MakeEntryAbi(Helper),
			Helper.Identity.Content.Execution);
	}

	inline FAngelscriptArtifactSemanticDependency MakeSignature(
		const FAngelscriptFunctionRoute& Target)
	{
		return MakeDependency(
			EAngelscriptArtifactSemanticDependencyKind::Signature,
			EAngelscriptArtifactReferenceKind::ScriptFunction,
			Target.Identity.FunctionKey.Hash,
			MakeEntryAbi(Target));
	}

	inline const FAngelscriptArtifactSemanticDependency* FindSemanticDependency(
		const FAngelscriptFunctionRouteSnapshot& Snapshot,
		const EAngelscriptArtifactSemanticDependencyKind Kind)
	{
		for (const FAngelscriptFunctionArtifactReferenceSet& Set
			: Snapshot.ValidatedFunctionArtifactReferenceSets)
		{
			for (const FAngelscriptArtifactSemanticDependency& Dependency
				: Set.SemanticDependencies)
			{
				if (Dependency.Kind == Kind
					&& Dependency.ExpectedContentOrValue.IsSet())
				{
					return &Dependency;
				}
			}
		}
		return nullptr;
	}

	class FDependencyProvider final : public IAngelscriptJITArtifactProvider
	{
	public:
		explicit FDependencyProvider(
			const FAngelscriptFunctionRouteSnapshot& Routes,
			const TCHAR* ProviderSeed = TEXT("provider"))
		{
			View.ProviderId.Hash = MakeHash(ProviderSeed);
			View.ProviderName = TEXT("TypedASTJITDependencyProvider");
			View.OwnerModuleName = TEXT("AngelscriptTest");
			View.NativeEnvironmentFingerprint =
				FAngelscriptJITTargetProfiles::
					BuildCurrentNativeEnvironmentFingerprint(
						EAngelscriptJITTargetProfile::EditorDevelopment);

			for (const FAngelscriptFunctionRoute& Route : Routes.FunctionRoutes)
			{
				if (!Route.bHasVerifiedArtifactIdentity
					|| Route.Function == nullptr)
				{
					continue;
				}
				FAngelscriptJITArtifactEntry& Entry =
					Entries.AddDefaulted_GetRef();
				Entry.ModuleKey = Route.ModuleKey;
				Entry.FunctionKey = Route.Identity.FunctionKey;
				Entry.ExecutionHash = Route.Identity.Content.Execution;
				Entry.DebugHash = Route.Identity.Content.Debug;
				Entry.ArtifactProfile = Route.Identity.Profile;
				Entry.NativeEnvironmentFingerprint =
					View.NativeEnvironmentFingerprint;
				Entry.EntryAbiHash = MakeEntryAbi(Route);
				Entry.EntryPoints.VMEntry = &ProviderVmEntry;
				Entry.EntryPoints.RawEntry =
					reinterpret_cast<asJITFunction_Raw>(&ProviderRawEntry);
				Entry.EntryPoints.ParmsEntry =
					FAngelscriptBytecodeJIT::CanGenerateParmsEntry(Route.Function)
						? &ProviderParmsEntry : nullptr;
			}
			Entries.Sort([](
				const FAngelscriptJITArtifactEntry& Left,
				const FAngelscriptJITArtifactEntry& Right)
			{
				if (!(Left.ModuleKey == Right.ModuleKey))
				{
					return Left.ModuleKey.Hash < Right.ModuleKey.Hash;
				}
				return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
			});
			if (Entries.IsEmpty())
			{
				return;
			}
			View.ArtifactProfile = Entries[0].ArtifactProfile;
			EntryDependencies.SetNum(Entries.Num());
			Rebuild();
		}

		int32 FindEntryIndex(
			const FAngelscriptStableFunctionKey& FunctionKey) const
		{
			return Entries.IndexOfByPredicate(
				[&FunctionKey](const FAngelscriptJITArtifactEntry& Entry)
				{
					return Entry.FunctionKey == FunctionKey;
				});
		}

		void SetEntryDependencies(
			const FAngelscriptStableFunctionKey& FunctionKey,
			TArray<FAngelscriptArtifactSemanticDependency> Rows)
		{
			const int32 EntryIndex = FindEntryIndex(FunctionKey);
			check(EntryIndex != INDEX_NONE);
			const FAngelscriptJITSemanticDependencyCanonicalResult Canonical =
				FAngelscriptJITProviderManifest::
					CanonicalizeSemanticDependencies(Rows);
			check(Canonical.IsValid());
			EntryDependencies[EntryIndex] = Canonical.Rows;
			Rebuild();
		}

		void Rebuild()
		{
			DependencyRows.Reset();
			for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
			{
				FAngelscriptJITArtifactEntry& Entry = Entries[EntryIndex];
				Entry.SemanticDependencyStartIndex =
					static_cast<uint32>(DependencyRows.Num());
				Entry.SemanticDependencyCount = static_cast<uint32>(
					EntryDependencies[EntryIndex].Num());
				for (const FAngelscriptArtifactSemanticDependency& Dependency
					: EntryDependencies[EntryIndex])
				{
					FAngelscriptJITSemanticDependencyRow& Row =
						DependencyRows.AddDefaulted_GetRef();
					FAngelscriptJITProviderManifest::FillSemanticDependencyRow(
						Dependency,
						static_cast<uint32>(DependencyRows.Num() - 1),
						Row);
				}
			}
			View.Entries = Entries.GetData();
			View.EntryCount = static_cast<uint32>(Entries.Num());
			View.SemanticDependencies = DependencyRows.Num() > 0
				? DependencyRows.GetData() : nullptr;
			View.SemanticDependencyCount =
				static_cast<uint32>(DependencyRows.Num());
			View.ArtifactSetDigest =
				FAngelscriptJITProviderManifest::BuildArtifactSetDigest(
					Entries,
					MakeArrayView(DependencyRows));
			View.ProviderGeneration =
				FAngelscriptJITProviderManifest::BuildProviderGeneration(View);
			check(FAngelscriptJITProviderManifest::Validate(View).IsValid());
		}

		const FAngelscriptJITProviderView* GetAngelscriptJITProviderView()
			const override
		{
			return &View;
		}

		TArray<FAngelscriptJITArtifactEntry> Entries;
		TArray<TArray<FAngelscriptArtifactSemanticDependency>> EntryDependencies;
		TArray<FAngelscriptJITSemanticDependencyRow> DependencyRows;
		FAngelscriptJITProviderView View;
	};

	inline bool ExecuteInt(
		FAngelscriptEngine& Engine,
		const FString& ModuleName,
		const FString& Declaration,
		int32& OutValue)
	{
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Engine.GetModuleByModuleName(ModuleName);
		if (!Module.IsValid() || Module->ScriptModule == nullptr)
		{
			return false;
		}
		asIScriptFunction* const Function =
			Module->ScriptModule->GetFunctionByDecl(
				TCHAR_TO_UTF8(*Declaration));
		if (Function == nullptr)
		{
			return false;
		}
		asIScriptContext* const Context =
			Engine.GetScriptEngine()->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		const bool bSuccess = Context->Prepare(Function) >= 0
			&& Context->Execute() == asEXECUTION_FINISHED;
		if (bSuccess)
		{
			OutValue = static_cast<int32>(Context->GetReturnDWord());
		}
		Context->Release();
		return bSuccess;
	}

	inline bool IsNativeProviderBinding(
		const FAngelscriptFunctionRoute& Route)
	{
		return Route.SelectedExecutionRoute
				== EAngelscriptFunctionExecutionRoute::Native
			&& Route.Function != nullptr
			&& Route.Function->GetJITBinding().VMEntry == &ProviderVmEntry;
	}
}

#endif
