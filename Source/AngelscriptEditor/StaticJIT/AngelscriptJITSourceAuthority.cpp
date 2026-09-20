#include "StaticJIT/AngelscriptJITSourceAuthority.h"

#include "AngelscriptEngine.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "Containers/Utf8String.h"
#include "Misc/Paths.h"

namespace AngelscriptEditor::StaticJIT
{
	namespace SourceAuthority_Private
	{
		FAngelscriptSource CopyHostSource(const FAngelscriptSource& Source)
		{
			FAngelscriptSource Copy;
			Copy.VirtualPath = Source.VirtualPath;
			Copy.Path = FString(Source.GetPath());
			Copy.ModuleName = Source.ModuleName;
			Copy.RelativeFilename = Source.RelativeFilename;
			Copy.AbsoluteFilename = Source.AbsoluteFilename;
			Copy.SourceText = Source.GetSourceText();
			Copy.SourceKind = Source.SourceKind;
			Copy.bHasSourceText = Source.bHasSourceText;
			return Copy;
		}

		struct FFrozenSource
		{
			FAngelscriptSource Source;
			FString Text;
			TArray<uint8> Bytes;
			FAngelscriptSourceState State;
			bool bHasState = false;
			FAngelscriptSourceProviderDescriptor Descriptor;
			bool bHasDescriptor = false;
		};

		class FFrozenSourceProvider final : public IAngelscriptSourceProvider
		{
		public:
			FFrozenSourceProvider(
				FString InScriptRoot,
				TArray<FFrozenSource> InSources)
				: ScriptRoot(MoveTemp(InScriptRoot))
				, Sources(MoveTemp(InSources))
			{
			}

			virtual void FindSources(
				const TArray<FAngelscriptSourceRoot>& ScriptRoots,
				const bool bSkipDevelopmentScripts,
				const bool bSkipEditorScripts,
				TArray<FAngelscriptSource>& OutSources) override
			{
				if (bSkipDevelopmentScripts || bSkipEditorScripts
					|| ScriptRoots.Num() != 1
					|| ScriptRoots[0].SourceKind != EAngelscriptSourceKind::Game
					|| !FPaths::IsSamePath(
						ScriptRoots[0].AbsolutePath, ScriptRoot))
				{
					return;
				}
				for (const FFrozenSource& Frozen : Sources)
				{
					OutSources.Add(CopyHostSource(Frozen.Source));
				}
			}

			virtual bool LoadSourceText(
				const FAngelscriptSource& Source,
				FString& OutSourceText) override
			{
				const FFrozenSource* Frozen = Find(Source);
				if (Frozen == nullptr)
				{
					return false;
				}
				OutSourceText = Frozen->Text;
				return true;
			}

			virtual bool QuerySourceState(
				const FAngelscriptSource& Source,
				FAngelscriptSourceState& OutState) override
			{
				const FFrozenSource* Frozen = Find(Source);
				if (Frozen == nullptr || !Frozen->bHasState)
				{
					return false;
				}
				OutState = Frozen->State;
				return true;
			}

			virtual bool LoadSourceBytes(
				const FAngelscriptSource& Source,
				TArray<uint8>& OutSourceBytes) override
			{
				const FFrozenSource* Frozen = Find(Source);
				if (Frozen == nullptr)
				{
					return false;
				}
				OutSourceBytes = Frozen->Bytes;
				return true;
			}

			virtual bool QuerySourceDescriptor(
				const FAngelscriptSource& Source,
				FAngelscriptSourceProviderDescriptor& OutDescriptor) override
			{
				const FFrozenSource* Frozen = Find(Source);
				if (Frozen == nullptr || !Frozen->bHasDescriptor)
				{
					OutDescriptor = {};
					return false;
				}
				OutDescriptor = Frozen->Descriptor;
				return true;
			}

		private:
			const FFrozenSource* Find(const FAngelscriptSource& Source) const
			{
				return Sources.FindByPredicate(
					[&Source](const FFrozenSource& Candidate)
					{
						return Candidate.Source.VirtualPath.ToString()
							== Source.VirtualPath.ToString();
					});
			}

			FString ScriptRoot;
			TArray<FFrozenSource> Sources;
		};

		struct FCompiledSource
		{
			FString VirtualPath;
			FString ModuleName;
			FAngelscriptHash256 SourceTextHash;
		};

		FAngelscriptJITSourceAuthoritySnapshot Failure(
			const EAngelscriptJITSourceAuthorityCode Code,
			FString Detail)
		{
			FAngelscriptJITSourceAuthoritySnapshot Result;
			Result.Code = Code;
			Result.Detail = MoveTemp(Detail);
			return Result;
		}

		bool ReadFlag(
			const FAngelscriptPreprocessorContext& Context,
			const TCHAR* Name)
		{
			const bool* Value = Context.PreprocessorFlags.Find(Name);
			return Value != nullptr && *Value;
		}

		FAngelscriptHash256 BuildProfileIdentity(
			const FAngelscriptPreprocessorContext& Context)
		{
			TArray<FString> FlagNames;
			Context.PreprocessorFlags.GetKeys(FlagNames);
			FlagNames.Sort([](const FString& A, const FString& B)
			{
				return FAngelscriptArtifactCanonicalWriter::
					CompareCanonicalUtf8Strings(A, B) < 0;
			});
			FAngelscriptArtifactCanonicalWriter Writer(
				TEXT("jit-editor-source-profile-v1"));
			Writer.WriteUInt32(static_cast<uint32>(FlagNames.Num()));
			for (const FString& FlagName : FlagNames)
			{
				Writer.WriteString(FlagName);
				Writer.WriteBool(Context.PreprocessorFlags.FindChecked(FlagName));
			}
			Writer.WriteBool(Context.bUseAutomaticImportMethod);
			Writer.WriteBool(Context.bWarnOnManualImportStatements);
			Writer.WriteBool(Context.bDefaultFunctionBlueprintCallable);
			Writer.WriteUInt8(static_cast<uint8>(Context.DefaultPropertyEditSpecifier));
			Writer.WriteUInt8(static_cast<uint8>(
				Context.DefaultPropertyEditSpecifierForStructs));
			Writer.WriteUInt8(static_cast<uint8>(
				Context.DefaultPropertyBlueprintSpecifier));
			Writer.WriteUInt8(static_cast<uint8>(Context.StaticClassDeprecation));
			Writer.WriteBool(Context.bScriptFloatIsFloat64);
			return Writer.FinalizeHash();
		}
	}

	FAngelscriptJITSourceAuthoritySnapshot FAngelscriptJITSourceAuthority::Capture(
		FAngelscriptEngine& PrimaryEngine,
		const FStringView ProjectFile,
		const EAngelscriptJITTargetProfile TargetProfile)
	{
		using namespace SourceAuthority_Private;
		if (!PrimaryEngine.IsInitialCompileFinished())
		{
			return Failure(EAngelscriptJITSourceAuthorityCode::EngineUnavailable,
				TEXT("The authoritative AngelScript Engine has not finished its initial compile."));
		}
		if (!PrimaryEngine.DidInitialCompileSucceed())
		{
			return Failure(EAngelscriptJITSourceAuthorityCode::InitialCompileFailed,
				TEXT("The authoritative AngelScript Engine initial compile failed."));
		}
		if (PrimaryEngine.GetPurpose() != EAngelscriptEnginePurpose::Runtime)
		{
			return Failure(EAngelscriptJITSourceAuthorityCode::WrongEnginePurpose,
				TEXT("Only the primary Runtime Engine can authorize an Editor refresh."));
		}
		const TOptional<FAngelscriptJITTargetProfileTraits> Traits =
			FAngelscriptJITTargetProfiles::GetTraits(TargetProfile);
		if (!Traits.IsSet()
			|| TargetProfile != EAngelscriptJITTargetProfile::EditorDevelopment)
		{
			return Failure(EAngelscriptJITSourceAuthorityCode::ProfileMismatch,
				TEXT("Editor refresh requires the EditorDevelopment target profile."));
		}

		FAngelscriptPreprocessorContext Context;
		{
			FAngelscriptEngineScope EngineScope(PrimaryEngine);
			Context = FAngelscriptPreprocessorContext::
				CreateFromCurrentEngineContext();
		}
		if (PrimaryEngine.GetRuntimeConfig().bSimulateCooked
			|| !PrimaryEngine.ShouldUseEditorScripts()
			|| ReadFlag(Context, TEXT("EDITOR")) != Traits->bEditor
			|| ReadFlag(Context, TEXT("EDITORONLY_DATA")) != Traits->bEditorOnlyData
			|| ReadFlag(Context, TEXT("RELEASE")) != Traits->bRelease
			|| ReadFlag(Context, TEXT("TEST")) != Traits->bTest)
		{
			return Failure(EAngelscriptJITSourceAuthorityCode::ProfileMismatch,
				FString::Printf(
					TEXT("The primary Engine preprocessor profile does not match EditorDevelopment: Purpose=%u IsEditor=%s RunningCommandlet=%s SimulateCooked=%s UseEditorScripts=%s Flags=[EDITOR:%s/%s EDITORONLY_DATA:%s/%s RELEASE:%s/%s TEST:%s/%s]."),
					static_cast<uint32>(PrimaryEngine.GetPurpose()),
					PrimaryEngine.GetRuntimeConfig().bIsEditor ? TEXT("true") : TEXT("false"),
					PrimaryEngine.GetRuntimeConfig().bRunningCommandlet ? TEXT("true") : TEXT("false"),
					PrimaryEngine.GetRuntimeConfig().bSimulateCooked ? TEXT("true") : TEXT("false"),
					PrimaryEngine.ShouldUseEditorScripts() ? TEXT("true") : TEXT("false"),
					ReadFlag(Context, TEXT("EDITOR")) ? TEXT("true") : TEXT("false"),
					Traits->bEditor ? TEXT("true") : TEXT("false"),
					ReadFlag(Context, TEXT("EDITORONLY_DATA")) ? TEXT("true") : TEXT("false"),
					Traits->bEditorOnlyData ? TEXT("true") : TEXT("false"),
					ReadFlag(Context, TEXT("RELEASE")) ? TEXT("true") : TEXT("false"),
					Traits->bRelease ? TEXT("true") : TEXT("false"),
					ReadFlag(Context, TEXT("TEST")) ? TEXT("true") : TEXT("false"),
					Traits->bTest ? TEXT("true") : TEXT("false")));
		}

		FString Descriptor(ProjectFile);
		Descriptor.TrimStartAndEndInline();
		if (Descriptor.IsEmpty())
		{
			Descriptor = FPaths::GetProjectFilePath();
		}
		Descriptor = FPaths::ConvertRelativePathToFull(Descriptor);
		FPaths::NormalizeFilename(Descriptor);
		FString ScriptRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::GetPath(Descriptor), TEXT("Script")));
		FPaths::NormalizeDirectoryName(ScriptRoot);

		TOptional<FAngelscriptSourceRoot> MatchingRoot;
		for (const FAngelscriptSourceRoot& Root
			: PrimaryEngine.GetEffectiveScriptRootDescriptors())
		{
			if (Root.SourceKind == EAngelscriptSourceKind::Game
				&& FPaths::IsSamePath(Root.AbsolutePath, ScriptRoot))
			{
				if (MatchingRoot.IsSet())
				{
					return Failure(EAngelscriptJITSourceAuthorityCode::Unproven,
						TEXT("The primary Engine exposes duplicate project Script roots."));
				}
				MatchingRoot = Root;
			}
		}
		if (!MatchingRoot.IsSet())
		{
			return Failure(EAngelscriptJITSourceAuthorityCode::ProjectRootMismatch,
				TEXT("The requested project Script root is not the primary Engine Game root."));
		}

		IAngelscriptSourceProvider* Provider = PrimaryEngine.GetSourceProvider();
		if (Provider == nullptr)
		{
			return Failure(EAngelscriptJITSourceAuthorityCode::SourceDiscoveryFailed,
				TEXT("The primary Engine has no authoritative source provider."));
		}
		TArray<FAngelscriptSource> Sources;
		Provider->FindSources({MatchingRoot.GetValue()}, false, false, Sources);
		Sources.Sort([](const FAngelscriptSource& A, const FAngelscriptSource& B)
		{
			return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(
				A.VirtualPath.ToString(), B.VirtualPath.ToString()) < 0;
		});
		if (Sources.IsEmpty())
		{
			return Failure(EAngelscriptJITSourceAuthorityCode::SourceDiscoveryFailed,
				TEXT("The project source provider discovered no EditorDevelopment scripts."));
		}

		TArray<FCompiledSource> CompiledSources;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module
			: PrimaryEngine.GetActiveModules())
		{
			for (const FAngelscriptModuleDesc::FCodeSection& Section : Module->Code)
			{
				if (Section.VirtualPath.StartsWith(
					FString(FAngelscriptVirtualPath::GameRoot) + TEXT("/")))
				{
					CompiledSources.Add({Section.VirtualPath, Module->ModuleName,
						Section.SourceTextHash});
				}
			}
		}
		CompiledSources.Sort([](const FCompiledSource& A, const FCompiledSource& B)
		{
			return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(
				A.VirtualPath, B.VirtualPath) < 0;
		});
		if (CompiledSources.Num() != Sources.Num())
		{
			return Failure(EAngelscriptJITSourceAuthorityCode::InventoryMismatch,
				FString::Printf(
					TEXT("The current project source inventory has %d files but the primary Engine compiled %d."),
					Sources.Num(), CompiledSources.Num()));
		}

		TArray<FFrozenSource> FrozenSources;
		FrozenSources.Reserve(Sources.Num());
		FAngelscriptArtifactCanonicalWriter InventoryWriter(
			TEXT("jit-editor-source-inventory-v1"));
		FAngelscriptArtifactCanonicalWriter ContentWriter(
			TEXT("jit-editor-source-content-v1"));
		InventoryWriter.WriteUInt32(static_cast<uint32>(Sources.Num()));
		ContentWriter.WriteUInt32(static_cast<uint32>(Sources.Num()));
		for (int32 Index = 0; Index < Sources.Num(); ++Index)
		{
			FAngelscriptSource& Source = Sources[Index];
			const FCompiledSource& Compiled = CompiledSources[Index];
			const FString& VirtualPath = Source.VirtualPath.ToString();
			if (Compiled.VirtualPath != VirtualPath
				|| Compiled.ModuleName != Source.ModuleName)
			{
				return Failure(EAngelscriptJITSourceAuthorityCode::InventoryMismatch,
					FString::Printf(TEXT("The primary Engine source inventory does not match %s."),
						*VirtualPath));
			}
			if (Compiled.SourceTextHash.IsZero())
			{
				return Failure(EAngelscriptJITSourceAuthorityCode::Unproven,
					FString::Printf(TEXT("The primary Engine has no compile-input identity for %s."),
						*VirtualPath));
			}

			FFrozenSource Frozen;
			Frozen.Source = CopyHostSource(Source);
			if (Source.bHasSourceText)
			{
				Frozen.Text = FString(Source.GetSourceText());
			}
			else if (!Provider->LoadSourceText(Source, Frozen.Text))
			{
				return Failure(EAngelscriptJITSourceAuthorityCode::SourceDiscoveryFailed,
					FString::Printf(TEXT("Unable to read authoritative source text for %s."),
						*VirtualPath));
			}
			if (!Provider->LoadSourceBytes(Source, Frozen.Bytes))
			{
				return Failure(EAngelscriptJITSourceAuthorityCode::SourceDiscoveryFailed,
					FString::Printf(TEXT("Unable to read authoritative source bytes for %s."),
						*VirtualPath));
			}
			const FAngelscriptHash256 CurrentHash =
				BuildAngelscriptSourceTextHash(Frozen.Text);
			if (!(CurrentHash == Compiled.SourceTextHash))
			{
				return Failure(EAngelscriptJITSourceAuthorityCode::ContentMismatch,
					FString::Printf(TEXT("Source content changed after the primary Engine compiled %s."),
						*VirtualPath));
			}
			Frozen.bHasState = Provider->QuerySourceState(Source, Frozen.State);
			Frozen.bHasDescriptor = Provider->QuerySourceDescriptor(
				Source, Frozen.Descriptor);
			Frozen.Source.SourceText = FUtf8String(Frozen.Text);
			Frozen.Source.bHasSourceText = true;

			InventoryWriter.WriteString(VirtualPath);
			InventoryWriter.WriteString(Source.ModuleName);
			ContentWriter.WriteString(VirtualPath);
			ContentWriter.WriteHash(CurrentHash);
			FrozenSources.Add(MoveTemp(Frozen));
		}

		FAngelscriptJITSourceAuthoritySnapshot Result;
		Result.Code = EAngelscriptJITSourceAuthorityCode::Current;
		Result.Detail = TEXT("The primary Engine source inventory, content, and EditorDevelopment profile are current.");
		Result.ProfileIdentity = BuildProfileIdentity(Context);
		Result.InventoryIdentity = InventoryWriter.FinalizeHash();
		Result.ContentIdentity = ContentWriter.FinalizeHash();
		Result.SourceCount = static_cast<uint32>(FrozenSources.Num());
		Result.FrozenSourceProvider = MakeShared<FFrozenSourceProvider>(
			MoveTemp(ScriptRoot), MoveTemp(FrozenSources));
		return Result;
	}
}
