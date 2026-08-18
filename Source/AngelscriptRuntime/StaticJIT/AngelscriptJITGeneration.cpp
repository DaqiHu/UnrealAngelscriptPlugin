#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITProviderDiagnostics.h"

#include "HAL/PlatformProperties.h"
#include "Misc/EngineVersion.h"

namespace AngelscriptJITGeneration_Private
{
	static constexpr TCHAR OwnershipPrefix[] =
		TEXT("// @angelscript-jit-owned revision=2 kind=");

	static void DigestVMEntry(FScriptExecution&, asDWORD*, asQWORD*)
	{
	}

	static void DigestRawEntry()
	{
	}

	static void DigestParmsEntry(FScriptExecution&, void*, void*)
	{
	}

	static bool HashLess(
		const FAngelscriptHash256& Left,
		const FAngelscriptHash256& Right)
	{
		return Left < Right;
	}

	static bool FunctionLess(
		const FAngelscriptJITGenerationFunction& Left,
		const FAngelscriptJITGenerationFunction& Right)
	{
		if (!(Left.ModuleKey == Right.ModuleKey))
		{
			return HashLess(Left.ModuleKey.Hash, Right.ModuleKey.Hash);
		}
		return HashLess(Left.FunctionKey.Hash, Right.FunctionKey.Hash);
	}

	static bool ModuleLess(
		const FAngelscriptJITGenerationModule& Left,
		const FAngelscriptJITGenerationModule& Right)
	{
		return HashLess(Left.ModuleKey.Hash, Right.ModuleKey.Hash);
	}

	static bool ReferenceLess(
		const FAngelscriptJITGenerationReference& Left,
		const FAngelscriptJITGenerationReference& Right)
	{
		if (Left.Reference != Right.Reference)
		{
			return Left.Reference < Right.Reference;
		}
		return static_cast<uint32>(Left.Flags)
			< static_cast<uint32>(Right.Flags);
	}

	static bool PathLess(const FString& Left, const FString& Right)
	{
		return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(
			Left, Right) < 0;
	}

	static bool FileLess(
		const FAngelscriptJITGeneratedFile& Left,
		const FAngelscriptJITGeneratedFile& Right)
	{
		return PathLess(Left.RelativePath, Right.RelativePath);
	}

	static FString EscapeJson(const FString& Value)
	{
		FString Result;
		Result.Reserve(Value.Len());
		for (const TCHAR Character : Value)
		{
			switch (Character)
			{
			case TEXT('\\'):
				Result += TEXT("\\\\");
				break;
			case TEXT('"'):
				Result += TEXT("\\\"");
				break;
			case TEXT('\n'):
				Result += TEXT("\\n");
				break;
			case TEXT('\r'):
				Result += TEXT("\\r");
				break;
			case TEXT('\t'):
				Result += TEXT("\\t");
				break;
			default:
				Result.AppendChar(Character);
				break;
			}
		}
		return Result;
	}

	static const TCHAR* ReferenceKindName(
		const EAngelscriptArtifactReferenceKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptArtifactReferenceKind::ScriptModule:
			return TEXT("ScriptModule");
		case EAngelscriptArtifactReferenceKind::ScriptType:
			return TEXT("ScriptType");
		case EAngelscriptArtifactReferenceKind::ScriptFunction:
			return TEXT("ScriptFunction");
		case EAngelscriptArtifactReferenceKind::ScriptGlobal:
			return TEXT("ScriptGlobal");
		case EAngelscriptArtifactReferenceKind::ScriptProperty:
			return TEXT("ScriptProperty");
		case EAngelscriptArtifactReferenceKind::ScriptImport:
			return TEXT("ScriptImport");
		case EAngelscriptArtifactReferenceKind::EnvironmentSymbol:
			return TEXT("EnvironmentSymbol");
		case EAngelscriptArtifactReferenceKind::CanonicalName:
			return TEXT("CanonicalName");
		case EAngelscriptArtifactReferenceKind::StringLiteral:
			return TEXT("StringLiteral");
		case EAngelscriptArtifactReferenceKind::RuntimeHelper:
			return TEXT("RuntimeHelper");
		default:
			return TEXT("Invalid");
		}
	}

	static const TCHAR* NativeCallSiteRouteName(
		const EAngelscriptJITNativeCallSiteRoute Route)
	{
		switch (Route)
		{
		case EAngelscriptJITNativeCallSiteRoute::DirectExported:
			return TEXT("DirectExported");
		case EAngelscriptJITNativeCallSiteRoute::DirectInline:
			return TEXT("DirectInline");
		case EAngelscriptJITNativeCallSiteRoute::RuntimeThunk:
			return TEXT("RuntimeThunk");
		case EAngelscriptJITNativeCallSiteRoute::Bridge:
			return TEXT("Bridge");
		case EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding:
			return TEXT("CurrentNativeBinding");
		default:
			return TEXT("Invalid");
		}
	}

	static FString HashExpression(const FAngelscriptHash256& Hash)
	{
		return FString::Printf(
			TEXT("FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT(\"%s\")))}"),
			*Hash.ToHexString());
	}

	static FAngelscriptHash256 HashUtf8Content(const FString& Content)
	{
		FTCHARToUTF8 Converted(*Content, Content.Len());
		return FAngelscriptHash256{FBlake3::HashBuffer(
			reinterpret_cast<const uint8*>(Converted.Get()),
			static_cast<uint64>(Converted.Length()))};
	}

	static const TCHAR* ProfileGuard(
		const EAngelscriptJITTargetProfile Profile)
	{
		switch (Profile)
		{
		case EAngelscriptJITTargetProfile::EditorDevelopment:
			return TEXT("WITH_EDITOR && UE_BUILD_DEVELOPMENT");
		case EAngelscriptJITTargetProfile::GameDevelopment:
			return TEXT("!WITH_EDITOR && UE_BUILD_DEVELOPMENT");
		case EAngelscriptJITTargetProfile::GameShipping:
			return TEXT("!WITH_EDITOR && UE_BUILD_SHIPPING");
		default:
			return TEXT("0");
		}
	}

	static FString OneLineCommentValue(const FString& Value)
	{
		FString Result = Value;
		Result.ReplaceInline(TEXT("\r"), TEXT(" "));
		Result.ReplaceInline(TEXT("\n"), TEXT(" "));
		return Result.TrimStartAndEnd();
	}

	static FString BuildEntryComment(
		const FAngelscriptJITGenerationFunction& Function,
		const TCHAR* EntryKind)
	{
		return FString::Printf(
			TEXT("// AS Function : %s\n")
			TEXT("// AS Source   : %s:%u:%u\n")
			TEXT("// JIT Entry   : %s\n"),
			*OneLineCommentValue(Function.CanonicalDeclaration),
			*OneLineCommentValue(Function.SourceSection),
			Function.SourceLine,
			Function.SourceColumn,
			EntryKind);
	}

	static FString BuildFunctionMetadataBlock(
		const FAngelscriptJITGeneratedFunction& Function)
	{
		return FString::Printf(
			TEXT("/*\n")
			TEXT(" * AngelScript Static JIT Function\n")
			TEXT(" * Declaration       : %s\n")
			TEXT(" * Source             : %s:%u:%u\n")
			TEXT(" * StableFunctionKey  : %s\n")
			TEXT(" * ExecutionHash      : %s\n")
			TEXT(" * DebugHash          : %s\n")
			TEXT(" * EntryAbiHash       : %s\n")
			TEXT(" * ExecutionCaps      : %u\n")
			TEXT(" * RuntimeTriggers    : %u\n")
			TEXT(" * CapabilityHash     : %s\n")
			TEXT(" */\n"),
			*OneLineCommentValue(Function.CanonicalDeclaration),
			*OneLineCommentValue(Function.SourceSection),
			Function.SourceLine,
			Function.SourceColumn,
			*Function.FunctionKey.Hash.ToHexString(),
			*Function.ExecutionHash.ToHexString(),
			*Function.DebugHash.ToHexString(),
			*Function.EntryAbiHash.ToHexString(),
			static_cast<uint32>(Function.AvailableExecutionCapabilities),
			static_cast<uint32>(
				Function.RuntimeExecutionRequirementTriggers),
			*Function.ExecutionCapabilityProfileHash.ToHexString());
	}

	static FString BuildModuleMetadataBlock(
		const FAngelscriptJITGenerationRequest& Request,
		const FAngelscriptJITGeneratedModule& Module)
	{
		return FString::Printf(
			TEXT("/*\n")
			TEXT(" * AngelScript Static JIT Module\n")
			TEXT(" * VirtualSourcePath   : %s\n")
			TEXT(" * CanonicalModuleName : %s\n")
			TEXT(" * TargetProfile       : %s\n")
			TEXT(" * StableModuleKey     : %s\n")
			TEXT(" * ProviderId          : %s\n")
			TEXT(" * ArtifactProfile     : %s\n")
			TEXT(" * NativeEnvironment   : %s\n")
			TEXT(" * FunctionCount       : %u\n")
			TEXT(" */\n\n"),
			*OneLineCommentValue(Module.VirtualSourcePath),
			*OneLineCommentValue(Module.CanonicalModuleName),
			FAngelscriptJITTargetProfiles::ToString(Request.TargetProfile),
			*Module.ModuleKey.Hash.ToHexString(),
			*Request.ProviderId.Hash.ToHexString(),
			*Request.ArtifactProfile.Hash.ToHexString(),
			*Request.NativeEnvironmentFingerprint.ToHexString(),
			Module.FunctionCount);
	}

	static bool TryBuildReadableModulePathBase(
		const FString& VirtualSourcePath,
		FString& OutDirectory,
		FString& OutStem,
		FString& OutError)
	{
		FString Normalized = VirtualSourcePath;
		FPaths::NormalizeFilename(Normalized);
		if (Normalized.Contains(TEXT(".."))
			|| !Normalized.EndsWith(TEXT(".as"), ESearchCase::IgnoreCase))
		{
			OutError = FString::Printf(
				TEXT("JIT module virtual source path is invalid: %s."),
				*VirtualSourcePath);
			return false;
		}

		FString RelativeSource;
		static const FString GamePrefix = TEXT("/Angelscript/Game/");
		static const FString PluginPrefix = TEXT("/Angelscript/Plugin/");
		static const FString MemoryPrefix = TEXT("/Angelscript/Memory/");
		if (Normalized.StartsWith(GamePrefix, ESearchCase::CaseSensitive))
		{
			RelativeSource = Normalized.Mid(GamePrefix.Len());
		}
		else if (Normalized.StartsWith(PluginPrefix, ESearchCase::CaseSensitive)
			|| Normalized.StartsWith(MemoryPrefix, ESearchCase::CaseSensitive))
		{
			RelativeSource = Normalized.Mid(
				FStringView(TEXT("/Angelscript/")).Len());
		}
		else
		{
			OutError = FString::Printf(
				TEXT("JIT module virtual source path uses an unsupported mount: %s."),
				*VirtualSourcePath);
			return false;
		}

		if (RelativeSource.IsEmpty()
			|| !FPaths::IsRelative(RelativeSource)
			|| RelativeSource.StartsWith(TEXT("/"))
			|| RelativeSource.Contains(TEXT(":")))
		{
			OutError = FString::Printf(
				TEXT("JIT module virtual source path cannot map to a safe relative path: %s."),
				*VirtualSourcePath);
			return false;
		}

		OutDirectory = FPaths::GetPath(RelativeSource);
		OutStem = FPaths::GetBaseFilename(RelativeSource);
		if (OutStem.IsEmpty())
		{
			OutError = FString::Printf(
				TEXT("JIT module virtual source path has no source stem: %s."),
				*VirtualSourcePath);
			return false;
		}
		return true;
	}

	static bool BuildModuleSourceRelativePaths(
		const TArray<FAngelscriptJITGenerationModule>& Modules,
		const EAngelscriptJITTargetProfile TargetProfile,
		TArray<FString>& OutPaths,
		FString& OutError)
	{
		TArray<FString> Directories;
		TArray<FString> Stems;
		TArray<int32> PrefixLengths;
		Directories.SetNum(Modules.Num());
		Stems.SetNum(Modules.Num());
		PrefixLengths.Init(8, Modules.Num());
		for (int32 Index = 0; Index < Modules.Num(); ++Index)
		{
			if (!TryBuildReadableModulePathBase(
				Modules[Index].VirtualSourcePath,
				Directories[Index], Stems[Index], OutError))
			{
				return false;
			}
		}

		for (;;)
		{
			TMap<FString, TArray<int32>> BasenameGroups;
			for (int32 Index = 0; Index < Modules.Num(); ++Index)
			{
				const FString Basename = FString::Printf(TEXT("%s.%s.%s.jit.cpp"),
					*Stems[Index],
					*Modules[Index].ModuleKey.Hash.ToHexString().Left(
						PrefixLengths[Index]),
					FAngelscriptJITTargetProfiles::ToString(TargetProfile));
				BasenameGroups.FindOrAdd(Basename.ToLower()).Add(Index);
			}

			bool bExtended = false;
			for (const TPair<FString, TArray<int32>>& Group : BasenameGroups)
			{
				if (Group.Value.Num() < 2)
				{
					continue;
				}
				for (const int32 Index : Group.Value)
				{
					if (PrefixLengths[Index] >= 64)
					{
						OutError = FString::Printf(
							TEXT("JIT module output basename remains ambiguous for %s."),
							*Modules[Index].VirtualSourcePath);
						return false;
					}
					PrefixLengths[Index] = FMath::Min(64,
						PrefixLengths[Index] + 4);
				}
				bExtended = true;
			}
			if (!bExtended)
			{
				break;
			}
		}

		OutPaths.SetNum(Modules.Num());
		for (int32 Index = 0; Index < Modules.Num(); ++Index)
		{
			const FString Basename = FString::Printf(TEXT("%s.%s.%s.jit.cpp"),
				*Stems[Index],
				*Modules[Index].ModuleKey.Hash.ToHexString().Left(
					PrefixLengths[Index]),
				FAngelscriptJITTargetProfiles::ToString(TargetProfile));
			OutPaths[Index] = Directories[Index].IsEmpty()
				? Basename : FPaths::Combine(Directories[Index], Basename);
			FPaths::NormalizeFilename(OutPaths[Index]);
		}
		return true;
	}

	static FString BuildManifestJson(
		const FAngelscriptJITGenerationRequest& Request,
		const FAngelscriptJITGenerationOutput& Output)
	{
		FString Json;
		Json += TEXT("{\n");
		Json += FString::Printf(TEXT("  \"ownershipRevision\": %u,\n"),
			FAngelscriptJITGeneration::OwnershipRevision);
		Json += FString::Printf(TEXT("  \"schemaRevision\": %u,\n"),
			FAngelscriptJITGeneration::SchemaRevision);
		Json += FString::Printf(TEXT("  \"providerAbiRevision\": %u,\n"),
			FAngelscriptJITProviderAbi::Revision);
		Json += FString::Printf(TEXT("  \"targetProfile\": \"%s\",\n"),
			FAngelscriptJITTargetProfiles::ToString(Output.TargetProfile));
		Json += FString::Printf(TEXT("  \"providerId\": \"%s\",\n"),
			*Output.ProviderId.Hash.ToHexString());
		Json += FString::Printf(TEXT("  \"providerGeneration\": \"%s\",\n"),
			*Output.ProviderGeneration.Hash.ToHexString());
		Json += FString::Printf(TEXT("  \"artifactSetDigest\": \"%s\",\n"),
			*Output.ArtifactSetDigest.ToHexString());
		Json += FString::Printf(TEXT("  \"artifactProfile\": \"%s\",\n"),
			*Output.ArtifactProfile.Hash.ToHexString());
		Json += FString::Printf(TEXT("  \"nativeEnvironment\": \"%s\",\n"),
			*Output.NativeEnvironmentFingerprint.ToHexString());
		Json += FString::Printf(TEXT("  \"providerName\": \"%s\",\n"),
			*EscapeJson(Request.ProviderName));
		Json += FString::Printf(TEXT("  \"ownerModuleName\": \"%s\",\n"),
			*EscapeJson(Request.OwnerModuleName));
		Json += TEXT("  \"modules\": [\n");
		for (int32 ModuleIndex = 0;
			ModuleIndex < Output.Modules.Num();
			++ModuleIndex)
		{
			const FAngelscriptJITGeneratedModule& Module =
				Output.Modules[ModuleIndex];
			Json += FString::Printf(
				TEXT("    {\"moduleKey\": \"%s\", \"canonicalName\": \"%s\", \"virtualSourcePath\": \"%s\", \"source\": \"%s\", \"contentHash\": \"%s\", \"functionCount\": %u}%s\n"),
				*Module.ModuleKey.Hash.ToHexString(),
				*EscapeJson(Module.CanonicalModuleName),
				*EscapeJson(Module.VirtualSourcePath),
				*EscapeJson(Module.SourceRelativePath),
				*Module.ContentHash.ToHexString(),
				Module.FunctionCount,
				ModuleIndex + 1 < Output.Modules.Num()
					? TEXT(",") : TEXT(""));
		}
		Json += TEXT("  ],\n");
		Json += TEXT("  \"functions\": [\n");
		for (int32 FunctionIndex = 0;
			FunctionIndex < Output.Functions.Num();
			++FunctionIndex)
		{
			const FAngelscriptJITGeneratedFunction& Function =
				Output.Functions[FunctionIndex];
			Json += TEXT("    {\n");
			Json += FString::Printf(TEXT("      \"moduleKey\": \"%s\",\n"),
				*Function.ModuleKey.Hash.ToHexString());
			Json += FString::Printf(TEXT("      \"functionKey\": \"%s\",\n"),
				*Function.FunctionKey.Hash.ToHexString());
			Json += FString::Printf(TEXT("      \"executionHash\": \"%s\",\n"),
				*Function.ExecutionHash.ToHexString());
			Json += FString::Printf(TEXT("      \"debugHash\": \"%s\",\n"),
				*Function.DebugHash.ToHexString());
			Json += FString::Printf(TEXT("      \"entryAbiHash\": \"%s\",\n"),
				*Function.EntryAbiHash.ToHexString());
			Json += FString::Printf(
				TEXT("      \"canonicalDeclaration\": \"%s\",\n"),
				*EscapeJson(Function.CanonicalDeclaration));
			Json += FString::Printf(
				TEXT("      \"sourceSection\": \"%s\",\n"),
				*EscapeJson(Function.SourceSection));
			Json += FString::Printf(TEXT("      \"sourceLine\": %u,\n"),
				Function.SourceLine);
			Json += FString::Printf(TEXT("      \"sourceColumn\": %u,\n"),
				Function.SourceColumn);
			Json += FString::Printf(TEXT("      \"flags\": %u,\n"),
				static_cast<uint32>(Function.Flags));
			Json += FString::Printf(
				TEXT("      \"moduleSource\": \"%s\",\n"),
				*Function.ModuleSourceRelativePath);
			Json += FString::Printf(TEXT("      \"symbol\": \"%s\",\n"),
				*Function.SymbolPrefix);
			Json += FString::Printf(
				TEXT("      \"entryPoints\": {\"vm\": %s, \"raw\": %s, \"parms\": %s},\n"),
				Function.bHasVMEntry ? TEXT("true") : TEXT("false"),
				Function.bHasRawEntry ? TEXT("true") : TEXT("false"),
				Function.bHasParmsEntry ? TEXT("true") : TEXT("false"));
			Json += TEXT("      \"nativeCallSites\": [\n");
			for (int32 CallSiteIndex = 0;
				CallSiteIndex < Function.NativeCallSites.Num();
				++CallSiteIndex)
			{
				const FAngelscriptJITGenerationNativeCallSite& CallSite =
					Function.NativeCallSites[CallSiteIndex];
				Json += FString::Printf(TEXT(
					"        {\"callSiteIndex\": %d, \"route\": \"%s\", \"canonicalDeclaration\": \"%s\", \"emittedCppCallee\": \"%s\", \"runtimeDllCore\": \"%s\", \"metadataRowSymbol\": \"%s\", \"registeredTarget\": \"%s\", \"stableTargetKey\": \"%s\", \"expectedAbi\": \"%s\", \"referenceSlotIndex\": %d}%s\n"),
					CallSiteIndex,
					NativeCallSiteRouteName(CallSite.Route),
					*EscapeJson(CallSite.CanonicalDeclaration),
					*EscapeJson(CallSite.EmittedCppCallee),
					*EscapeJson(CallSite.RuntimeDllCore),
					*EscapeJson(CallSite.MetadataRowSymbol),
					*EscapeJson(CallSite.RegisteredTarget),
					*CallSite.StableTargetKey.ToHexString(),
					*CallSite.ExpectedAbi.ToHexString(),
					CallSite.ReferenceSlotIndex,
					CallSiteIndex + 1 < Function.NativeCallSites.Num()
						? TEXT(",") : TEXT(""));
			}
			Json += TEXT("      ],\n");
			Json += TEXT("      \"references\": [\n");
			for (int32 ReferenceIndex = 0;
				ReferenceIndex < Function.ReferenceSlots.Num();
				++ReferenceIndex)
			{
				const FAngelscriptJITReferenceSlot& Slot =
					Function.ReferenceSlots[ReferenceIndex];
				Json += FString::Printf(
					TEXT("        {\"slot\": %u, \"flags\": %u, \"kind\": \"%s\", \"stableKey\": \"%s\", \"expectedAbi\": \"%s\"}%s\n"),
					Slot.SlotIndex,
					static_cast<uint32>(Slot.Flags),
					ReferenceKindName(Slot.Reference.Kind),
					*Slot.Reference.StableKey.ToHexString(),
					*Slot.Reference.ExpectedAbi.ToHexString(),
					ReferenceIndex + 1 < Function.ReferenceSlots.Num()
						? TEXT(",") : TEXT(""));
			}
			Json += TEXT("      ],\n");
			Json += TEXT("      \"semanticDependencies\": [\n");
			for (int32 DependencyIndex = 0;
				DependencyIndex < Function.SemanticDependencies.Num();
				++DependencyIndex)
			{
				const FAngelscriptArtifactSemanticDependency& Dependency =
					Function.SemanticDependencies[DependencyIndex];
				const FString Expected =
					Dependency.ExpectedContentOrValue.IsSet()
						? Dependency.ExpectedContentOrValue.GetValue().
							ToHexString()
						: FString();
				Json += FString::Printf(
					TEXT("        {\"kind\": %u, \"targetKind\": %u, \"stableKey\": \"%s\", \"expectedAbi\": \"%s\", \"expectedContentOrValue\": \"%s\"}%s\n"),
					static_cast<uint32>(Dependency.Kind),
					static_cast<uint32>(Dependency.Target.Kind),
					*Dependency.Target.StableKey.ToHexString(),
					*Dependency.Target.ExpectedAbi.ToHexString(),
					*Expected,
					DependencyIndex + 1 < Function.SemanticDependencies.Num()
						? TEXT(",") : TEXT(""));
			}
			Json += TEXT("      ]\n");
			Json += FunctionIndex + 1 < Output.Functions.Num()
				? TEXT("    },\n") : TEXT("    }\n");
		}
		Json += TEXT("  ]\n");
		Json += TEXT("}\n");
		return Json;
	}

	static FString BuildProviderHeader()
	{
		return FString(OwnershipPrefix)
			+ TEXT("provider-header\n")
			+ TEXT("#pragma once\n\n")
			+ TEXT("#include \"StaticJIT/AngelscriptJITProvider.h\"\n\n")
			+ TEXT("FAngelscriptJITProviderView GetGeneratedAngelscriptJITProviderView();\n");
	}

	struct FProviderDiagnosticFlatView
	{
		TArray<const TCHAR*> FallbackChain;
		TArray<FAngelscriptJITProviderFunctionDiagnostic> Functions;
		TArray<FAngelscriptJITProviderBackendAttemptDiagnostic> BackendAttempts;
		TArray<FAngelscriptJITProviderTypedSemanticFunctionDiagnostic>
			SemanticFunctions;
		TArray<FAngelscriptJITProviderTypedCallDiagnostic> Calls;
		FAngelscriptJITProviderDiagnostics View;
	};

	static FAngelscriptJITProviderDiagnosticSource MakeDiagnosticSource(
		const FAngelscriptJITGenerationDiagnosticSource& Source)
	{
		FAngelscriptJITProviderDiagnosticSource Result;
		Result.Section = *Source.Section;
		Result.Row = Source.Row;
		Result.Column = Source.Column;
		return Result;
	}

	static bool NormalizeProviderDiagnostics(
		const TConstArrayView<FAngelscriptJITGeneratedFunction> Functions,
		const FAngelscriptJITGenerationDiagnostics& Source,
		FAngelscriptJITGenerationDiagnostics& OutDiagnostics,
		FString& OutError)
	{
		if (Source.Functions.Num() != Functions.Num())
		{
			OutError = FString::Printf(TEXT(
				"Provider diagnostics contain %d function rows for %d emitted entries."),
				Source.Functions.Num(), Functions.Num());
			return false;
		}

		OutDiagnostics = Source;
		OutDiagnostics.Functions.Reset();
		OutDiagnostics.Functions.Reserve(Functions.Num());
		TArray<uint8> Used;
		Used.SetNumZeroed(Source.Functions.Num());
		for (const FAngelscriptJITGeneratedFunction& Function : Functions)
		{
			int32 MatchIndex = INDEX_NONE;
			for (int32 CandidateIndex = 0;
				CandidateIndex < Source.Functions.Num(); ++CandidateIndex)
			{
				if (Source.Functions[CandidateIndex].FunctionKey
					== Function.FunctionKey)
				{
					if (MatchIndex != INDEX_NONE)
					{
						OutError = FString::Printf(TEXT(
							"Provider diagnostics contain duplicate function key %s."),
							*Function.FunctionKey.Hash.ToHexString());
						return false;
					}
					MatchIndex = CandidateIndex;
				}
			}
			if (MatchIndex == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT(
					"Provider diagnostics are missing emitted function key %s."),
					*Function.FunctionKey.Hash.ToHexString());
				return false;
			}
			Used[MatchIndex] = 1;
			OutDiagnostics.Functions.Add(Source.Functions[MatchIndex]);
		}
		for (int32 Index = 0; Index < Used.Num(); ++Index)
		{
			if (Used[Index] == 0)
			{
				OutError = FString::Printf(TEXT(
					"Provider diagnostics contain non-emitted function key %s."),
					*Source.Functions[Index].FunctionKey.Hash.ToHexString());
				return false;
			}
		}
		return true;
	}

	static bool BuildProviderDiagnosticFlatView(
		const FAngelscriptJITGenerationDiagnostics& Diagnostics,
		FProviderDiagnosticFlatView& OutFlat,
		FString& OutError)
	{
		OutFlat = {};
		OutFlat.FallbackChain.Reserve(Diagnostics.FallbackChain.Num());
		for (const FString& BackendId : Diagnostics.FallbackChain)
		{
			OutFlat.FallbackChain.Add(*BackendId);
		}

		OutFlat.Functions.Reserve(Diagnostics.Functions.Num());
		for (const FAngelscriptJITGenerationFunctionDiagnostic& Function
			: Diagnostics.Functions)
		{
			FAngelscriptJITProviderFunctionDiagnostic& FlatFunction =
				OutFlat.Functions.AddDefaulted_GetRef();
			FlatFunction.FunctionKey = Function.FunctionKey;
			FlatFunction.ActualBackendId = *Function.ActualBackendId;
			FlatFunction.Disposition = *Function.Disposition;
			FlatFunction.Detail = *Function.Detail;
			FlatFunction.BackendAttemptStart =
				static_cast<uint32>(OutFlat.BackendAttempts.Num());
			FlatFunction.BackendAttemptCount =
				static_cast<uint32>(Function.BackendAttempts.Num());

			for (const FAngelscriptJITGenerationBackendAttemptDiagnostic& Attempt
				: Function.BackendAttempts)
			{
				FAngelscriptJITProviderBackendAttemptDiagnostic& FlatAttempt =
					OutFlat.BackendAttempts.AddDefaulted_GetRef();
				FlatAttempt.BackendId = *Attempt.BackendId;
				FlatAttempt.Disposition = *Attempt.Disposition;
				FlatAttempt.Detail = *Attempt.Detail;
				if (Attempt.bHasTypedDiagnostic)
				{
					FlatAttempt.Flags |=
						EAngelscriptJITProviderDiagnosticAttemptFlags::
							HasTypedDiagnostic;
				}
				if (!Attempt.SemanticFunctions.IsEmpty())
				{
					FlatAttempt.Flags |=
						EAngelscriptJITProviderDiagnosticAttemptFlags::
							HasSemanticFunction;
				}
				if (!Attempt.Calls.IsEmpty())
				{
					FlatAttempt.Flags |=
						EAngelscriptJITProviderDiagnosticAttemptFlags::
							HasCallClosure;
				}
				FlatAttempt.FallbackReason = *Attempt.FallbackReason;
				FlatAttempt.TypedDetail = *Attempt.TypedDetail;
				FlatAttempt.ProcessedSource =
					MakeDiagnosticSource(Attempt.ProcessedSource);
				FlatAttempt.SemanticFunctionStart =
					static_cast<uint32>(OutFlat.SemanticFunctions.Num());
				FlatAttempt.SemanticFunctionCount =
					static_cast<uint32>(Attempt.SemanticFunctions.Num());
				if (Attempt.PrimarySemanticFunctionIndex != INDEX_NONE)
				{
					if (!Attempt.SemanticFunctions.IsValidIndex(
						Attempt.PrimarySemanticFunctionIndex))
					{
						OutError = FString::Printf(TEXT(
							"Diagnostic backend %s has invalid primary semantic index %d."),
							*Attempt.BackendId,
							Attempt.PrimarySemanticFunctionIndex);
						return false;
					}
					FlatAttempt.PrimarySemanticFunctionIndex =
						static_cast<int32>(FlatAttempt.SemanticFunctionStart)
						+ Attempt.PrimarySemanticFunctionIndex;
				}
				FlatAttempt.CallStart =
					static_cast<uint32>(OutFlat.Calls.Num());
				FlatAttempt.CallCount =
					static_cast<uint32>(Attempt.Calls.Num());
				FlatAttempt.RequiredCapabilities =
					Attempt.RequiredCapabilities;
				FlatAttempt.AvailableCapabilities =
					Attempt.AvailableCapabilities;
				FlatAttempt.ExecutionRequirementsIdentity =
					Attempt.ExecutionRequirementsIdentity;
				FlatAttempt.AvailableCapabilityProfileIdentity =
					Attempt.AvailableCapabilityProfileIdentity;
				FlatAttempt.ProfileKind = *Attempt.ProfileKind;
				FlatAttempt.ExceptionControlState =
					*Attempt.ExceptionControlState;
				FlatAttempt.ExceptionPayloadState =
					*Attempt.ExceptionPayloadState;
				FlatAttempt.bRecursionGuardAvailable =
					Attempt.bRecursionGuardAvailable ? 1u : 0u;

				for (const auto& Semantic : Attempt.SemanticFunctions)
				{
					FAngelscriptJITProviderTypedSemanticFunctionDiagnostic&
						FlatSemantic =
							OutFlat.SemanticFunctions.AddDefaulted_GetRef();
					FlatSemantic.FunctionKey = Semantic.FunctionKey;
					FlatSemantic.Role = *Semantic.Role;
					FlatSemantic.RawTraitBits = Semantic.RawTraitBits;
					FlatSemantic.UnknownTraitBits = Semantic.UnknownTraitBits;
					FlatSemantic.InvocationKind = *Semantic.InvocationKind;
					FlatSemantic.ReceiverKind = *Semantic.ReceiverKind;
					FlatSemantic.ReceiverParameterIndex =
						Semantic.ReceiverParameterIndex;
					FlatSemantic.HIRState = *Semantic.HIRState;
					FlatSemantic.CleanupPlanState = *Semantic.CleanupPlanState;
					FlatSemantic.bHasSuspendState =
						Semantic.bHasSuspendState ? 1u : 0u;
					FlatSemantic.bHasExceptionCleanup =
						Semantic.bHasExceptionCleanup ? 1u : 0u;
					FlatSemantic.bCleanupPlanCoversAllTransfers =
						Semantic.bCleanupPlanCoversAllTransfers ? 1u : 0u;
					FlatSemantic.ComponentIndex = Semantic.ComponentIndex;
					FlatSemantic.bRecursiveComponent =
						Semantic.bRecursiveComponent ? 1u : 0u;
					FlatSemantic.EligibilityReason =
						*Semantic.EligibilityReason;
					FlatSemantic.EligibilityDetail =
						*Semantic.EligibilityDetail;
					FlatSemantic.EligibilitySource =
						MakeDiagnosticSource(Semantic.EligibilitySource);
				}

				for (const auto& Call : Attempt.Calls)
				{
					FAngelscriptJITProviderTypedCallDiagnostic& FlatCall =
						OutFlat.Calls.AddDefaulted_GetRef();
					FlatCall.CallerFunctionKey = Call.CallerFunctionKey;
					FlatCall.CalleeFunctionKey = Call.CalleeFunctionKey;
					FlatCall.ExpressionIndex = Call.ExpressionIndex;
					FlatCall.ProcessedSource =
						MakeDiagnosticSource(Call.ProcessedSource);
					if (Call.AuthoredSource.IsSet())
					{
						FlatCall.SourceFlags |=
							EAngelscriptJITProviderDiagnosticSourceFlags::
								HasAuthoredSource;
						FlatCall.AuthoredSource = MakeDiagnosticSource(
							Call.AuthoredSource.GetValue());
					}
					if (Call.GeneratedSource.IsSet())
					{
						FlatCall.SourceFlags |=
							EAngelscriptJITProviderDiagnosticSourceFlags::
								HasGeneratedSource;
						FlatCall.GeneratedSource = MakeDiagnosticSource(
							Call.GeneratedSource.GetValue());
					}
					FlatCall.TargetKind = *Call.TargetKind;
					FlatCall.Disposition = *Call.Disposition;
					FlatCall.CalleeEligibilityReason =
						*Call.CalleeEligibilityReason;
					FlatCall.CalleeEligibilityDetail =
						*Call.CalleeEligibilityDetail;
					FlatCall.CalleeEligibilitySource =
						MakeDiagnosticSource(Call.CalleeEligibilitySource);
					FlatCall.bHasNativeLinkageValidation =
						Call.bHasNativeLinkageValidation ? 1u : 0u;
					FlatCall.NativeLinkageValidation =
						*Call.NativeLinkageValidation;
					FlatCall.NativeLinkageValidationDetail =
						*Call.NativeLinkageValidationDetail;
					FlatCall.CanonicalDeclaration =
						*Call.CanonicalDeclaration;
					FlatCall.EmittedRoute = Call.EmittedRoute;
					FlatCall.EmittedCppCallee = *Call.EmittedCppCallee;
					FlatCall.RuntimeDllCore = *Call.RuntimeDllCore;
					FlatCall.MetadataRowSymbol = *Call.MetadataRowSymbol;
					FlatCall.RegisteredTarget = *Call.RegisteredTarget;
					FlatCall.DirectCppSymbol = *Call.DirectCppSymbol;
					FlatCall.StableTargetKey = Call.StableTargetKey;
					FlatCall.ExpectedAbi = Call.ExpectedAbi;
					FlatCall.ReferenceSlotIndex = Call.ReferenceSlotIndex;
				}
			}
		}

		OutFlat.View.RequestKind = *Diagnostics.RequestKind;
		OutFlat.View.RequestedBackendId = *Diagnostics.RequestedBackendId;
		OutFlat.View.CaptureState = *Diagnostics.CaptureState;
		OutFlat.View.FallbackChain = OutFlat.FallbackChain.GetData();
		OutFlat.View.FallbackChainCount =
			static_cast<uint32>(OutFlat.FallbackChain.Num());
		OutFlat.View.SourceCompilePassCount =
			Diagnostics.SourceCompilePassCount;
		OutFlat.View.CompiledModuleCount = Diagnostics.CompiledModuleCount;
		OutFlat.View.CompiledFunctionCount = Diagnostics.CompiledFunctionCount;
		OutFlat.View.CompiledTypeCount = Diagnostics.CompiledTypeCount;
		OutFlat.View.CompiledGlobalCount = Diagnostics.CompiledGlobalCount;
		OutFlat.View.CompiledDescriptorCount =
			Diagnostics.CompiledDescriptorCount;
		OutFlat.View.CompiledDependencyCount =
			Diagnostics.CompiledDependencyCount;
		OutFlat.View.CompiledExternalNativeCallCount =
			Diagnostics.CompiledExternalNativeCallCount;
		OutFlat.View.EmitModuleCount = Diagnostics.EmitModuleCount;
		OutFlat.View.bHasVerifiedTypedHIR =
			Diagnostics.bHasVerifiedTypedHIR ? 1u : 0u;
		OutFlat.View.TaskError = *Diagnostics.TaskError;
		OutFlat.View.Functions = OutFlat.Functions.GetData();
		OutFlat.View.FunctionCount =
			static_cast<uint32>(OutFlat.Functions.Num());
		OutFlat.View.BackendAttempts = OutFlat.BackendAttempts.GetData();
		OutFlat.View.BackendAttemptCount =
			static_cast<uint32>(OutFlat.BackendAttempts.Num());
		OutFlat.View.SemanticFunctions = OutFlat.SemanticFunctions.GetData();
		OutFlat.View.SemanticFunctionCount =
			static_cast<uint32>(OutFlat.SemanticFunctions.Num());
		OutFlat.View.Calls = OutFlat.Calls.GetData();
		OutFlat.View.CallCount = static_cast<uint32>(OutFlat.Calls.Num());
		OutFlat.View.DiagnosticDigest =
			FAngelscriptJITProviderManifest::BuildDiagnosticsDigest(OutFlat.View);
		return true;
	}

	static void AppendDiagnosticSourceAssignment(
		FString& Source,
		const FString& Target,
		const FAngelscriptJITProviderDiagnosticSource& Value)
	{
		Source += FString::Printf(TEXT(
			"\t%s = {sizeof(FAngelscriptJITProviderDiagnosticSource), TEXT(\"%s\"), %u, %u};\n"),
			*Target,
			*EscapeJson(Value.Section != nullptr ? Value.Section : TEXT("")),
			Value.Row,
			Value.Column);
	}

	static void AppendProviderDiagnosticsSource(
		FString& Source,
		const FAngelscriptJITGenerationOutput& Output)
	{
		if (!Output.Diagnostics.IsSet())
		{
			return;
		}

		FProviderDiagnosticFlatView Flat;
		FString Error;
		const bool bBuilt = BuildProviderDiagnosticFlatView(
			Output.Diagnostics.GetValue(), Flat, Error);
		checkf(bBuilt, TEXT("Validated Provider diagnostics failed to flatten: %s"),
			*Error);
		check(Flat.View.DiagnosticDigest == Output.DiagnosticDigest);

		if (!Flat.FallbackChain.IsEmpty())
		{
			Source += FString::Printf(TEXT(
				"\tstatic thread_local const TCHAR* GDiagnosticFallbackChain[%d];\n"),
				Flat.FallbackChain.Num());
			for (int32 Index = 0; Index < Flat.FallbackChain.Num(); ++Index)
			{
				Source += FString::Printf(TEXT(
					"\tGDiagnosticFallbackChain[%d] = TEXT(\"%s\");\n"),
					Index,
					*EscapeJson(Flat.FallbackChain[Index]));
			}
		}

		if (!Flat.SemanticFunctions.IsEmpty())
		{
			Source += FString::Printf(TEXT(
				"\tstatic thread_local FAngelscriptJITProviderTypedSemanticFunctionDiagnostic GDiagnosticSemanticFunctions[%d];\n"),
				Flat.SemanticFunctions.Num());
			for (int32 Index = 0; Index < Flat.SemanticFunctions.Num(); ++Index)
			{
				const auto& Semantic = Flat.SemanticFunctions[Index];
				const FString Prefix = FString::Printf(
					TEXT("GDiagnosticSemanticFunctions[%d]"), Index);
				Source += FString::Printf(TEXT("\t%s = {};\n"), *Prefix);
				Source += FString::Printf(TEXT("\t%s.FunctionKey = {%s};\n"),
					*Prefix, *HashExpression(Semantic.FunctionKey.Hash));
				Source += FString::Printf(TEXT("\t%s.Role = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Semantic.Role));
				Source += FString::Printf(TEXT("\t%s.RawTraitBits = %u;\n"),
					*Prefix, Semantic.RawTraitBits);
				Source += FString::Printf(TEXT("\t%s.UnknownTraitBits = %u;\n"),
					*Prefix, Semantic.UnknownTraitBits);
				Source += FString::Printf(TEXT("\t%s.InvocationKind = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Semantic.InvocationKind));
				Source += FString::Printf(TEXT("\t%s.ReceiverKind = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Semantic.ReceiverKind));
				Source += FString::Printf(TEXT("\t%s.ReceiverParameterIndex = %d;\n"),
					*Prefix, Semantic.ReceiverParameterIndex);
				Source += FString::Printf(TEXT("\t%s.HIRState = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Semantic.HIRState));
				Source += FString::Printf(TEXT("\t%s.CleanupPlanState = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Semantic.CleanupPlanState));
				Source += FString::Printf(TEXT("\t%s.bHasSuspendState = %u;\n"),
					*Prefix, Semantic.bHasSuspendState);
				Source += FString::Printf(TEXT("\t%s.bHasExceptionCleanup = %u;\n"),
					*Prefix, Semantic.bHasExceptionCleanup);
				Source += FString::Printf(TEXT(
					"\t%s.bCleanupPlanCoversAllTransfers = %u;\n"),
					*Prefix, Semantic.bCleanupPlanCoversAllTransfers);
				Source += FString::Printf(TEXT("\t%s.ComponentIndex = %d;\n"),
					*Prefix, Semantic.ComponentIndex);
				Source += FString::Printf(TEXT("\t%s.bRecursiveComponent = %u;\n"),
					*Prefix, Semantic.bRecursiveComponent);
				Source += FString::Printf(TEXT(
					"\t%s.EligibilityReason = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Semantic.EligibilityReason));
				Source += FString::Printf(TEXT(
					"\t%s.EligibilityDetail = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Semantic.EligibilityDetail));
				AppendDiagnosticSourceAssignment(Source,
					Prefix + TEXT(".EligibilitySource"),
					Semantic.EligibilitySource);
			}
		}

		if (!Flat.Calls.IsEmpty())
		{
			Source += FString::Printf(TEXT(
				"\tstatic thread_local FAngelscriptJITProviderTypedCallDiagnostic GDiagnosticCalls[%d];\n"),
				Flat.Calls.Num());
			for (int32 Index = 0; Index < Flat.Calls.Num(); ++Index)
			{
				const auto& Call = Flat.Calls[Index];
				const FString Prefix = FString::Printf(
					TEXT("GDiagnosticCalls[%d]"), Index);
				Source += FString::Printf(TEXT("\t%s = {};\n"), *Prefix);
				Source += FString::Printf(TEXT("\t%s.CallerFunctionKey = {%s};\n"),
					*Prefix, *HashExpression(Call.CallerFunctionKey.Hash));
				Source += FString::Printf(TEXT("\t%s.CalleeFunctionKey = {%s};\n"),
					*Prefix, *HashExpression(Call.CalleeFunctionKey.Hash));
				Source += FString::Printf(TEXT("\t%s.ExpressionIndex = %u;\n"),
					*Prefix, Call.ExpressionIndex);
				Source += FString::Printf(TEXT(
					"\t%s.SourceFlags = static_cast<EAngelscriptJITProviderDiagnosticSourceFlags>(%u);\n"),
					*Prefix, static_cast<uint32>(Call.SourceFlags));
				AppendDiagnosticSourceAssignment(Source,
					Prefix + TEXT(".ProcessedSource"), Call.ProcessedSource);
				AppendDiagnosticSourceAssignment(Source,
					Prefix + TEXT(".AuthoredSource"), Call.AuthoredSource);
				AppendDiagnosticSourceAssignment(Source,
					Prefix + TEXT(".GeneratedSource"), Call.GeneratedSource);
				Source += FString::Printf(TEXT("\t%s.TargetKind = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Call.TargetKind));
				Source += FString::Printf(TEXT("\t%s.Disposition = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Call.Disposition));
				Source += FString::Printf(TEXT(
					"\t%s.CalleeEligibilityReason = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Call.CalleeEligibilityReason));
				Source += FString::Printf(TEXT(
					"\t%s.CalleeEligibilityDetail = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Call.CalleeEligibilityDetail));
				AppendDiagnosticSourceAssignment(Source,
					Prefix + TEXT(".CalleeEligibilitySource"),
					Call.CalleeEligibilitySource);
				Source += FString::Printf(TEXT(
					"\t%s.bHasNativeLinkageValidation = %u;\n"),
					*Prefix, Call.bHasNativeLinkageValidation);
				Source += FString::Printf(TEXT(
					"\t%s.NativeLinkageValidation = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Call.NativeLinkageValidation));
				Source += FString::Printf(TEXT(
					"\t%s.NativeLinkageValidationDetail = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Call.NativeLinkageValidationDetail));
				Source += FString::Printf(TEXT(
					"\t%s.CanonicalDeclaration = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Call.CanonicalDeclaration));
				Source += FString::Printf(TEXT(
					"\t%s.EmittedRoute = static_cast<EAngelscriptJITNativeCallSiteRoute>(%u);\n"),
					*Prefix, static_cast<uint32>(Call.EmittedRoute));
				Source += FString::Printf(TEXT(
					"\t%s.EmittedCppCallee = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Call.EmittedCppCallee));
				Source += FString::Printf(TEXT("\t%s.RuntimeDllCore = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Call.RuntimeDllCore));
				Source += FString::Printf(TEXT(
					"\t%s.MetadataRowSymbol = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Call.MetadataRowSymbol));
				Source += FString::Printf(TEXT("\t%s.RegisteredTarget = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Call.RegisteredTarget));
				Source += FString::Printf(TEXT("\t%s.DirectCppSymbol = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Call.DirectCppSymbol));
				Source += FString::Printf(TEXT("\t%s.StableTargetKey = %s;\n"),
					*Prefix, *HashExpression(Call.StableTargetKey));
				Source += FString::Printf(TEXT("\t%s.ExpectedAbi = %s;\n"),
					*Prefix, *HashExpression(Call.ExpectedAbi));
				Source += FString::Printf(TEXT("\t%s.ReferenceSlotIndex = %d;\n"),
					*Prefix, Call.ReferenceSlotIndex);
			}
		}

		if (!Flat.BackendAttempts.IsEmpty())
		{
			Source += FString::Printf(TEXT(
				"\tstatic thread_local FAngelscriptJITProviderBackendAttemptDiagnostic GDiagnosticBackendAttempts[%d];\n"),
				Flat.BackendAttempts.Num());
			for (int32 Index = 0; Index < Flat.BackendAttempts.Num(); ++Index)
			{
				const auto& Attempt = Flat.BackendAttempts[Index];
				const FString Prefix = FString::Printf(
					TEXT("GDiagnosticBackendAttempts[%d]"), Index);
				Source += FString::Printf(TEXT("\t%s = {};\n"), *Prefix);
				Source += FString::Printf(TEXT("\t%s.BackendId = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Attempt.BackendId));
				Source += FString::Printf(TEXT("\t%s.Disposition = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Attempt.Disposition));
				Source += FString::Printf(TEXT("\t%s.Detail = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Attempt.Detail));
				Source += FString::Printf(TEXT(
					"\t%s.Flags = static_cast<EAngelscriptJITProviderDiagnosticAttemptFlags>(%u);\n"),
					*Prefix, static_cast<uint32>(Attempt.Flags));
				Source += FString::Printf(TEXT("\t%s.FallbackReason = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Attempt.FallbackReason));
				Source += FString::Printf(TEXT("\t%s.TypedDetail = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Attempt.TypedDetail));
				AppendDiagnosticSourceAssignment(Source,
					Prefix + TEXT(".ProcessedSource"), Attempt.ProcessedSource);
				Source += FString::Printf(TEXT(
					"\t%s.PrimarySemanticFunctionIndex = %d;\n"),
					*Prefix, Attempt.PrimarySemanticFunctionIndex);
				Source += FString::Printf(TEXT("\t%s.SemanticFunctionStart = %u;\n"),
					*Prefix, Attempt.SemanticFunctionStart);
				Source += FString::Printf(TEXT("\t%s.SemanticFunctionCount = %u;\n"),
					*Prefix, Attempt.SemanticFunctionCount);
				Source += FString::Printf(TEXT("\t%s.CallStart = %u;\n"),
					*Prefix, Attempt.CallStart);
				Source += FString::Printf(TEXT("\t%s.CallCount = %u;\n"),
					*Prefix, Attempt.CallCount);
				Source += FString::Printf(TEXT(
					"\t%s.RequiredCapabilities = static_cast<EAngelscriptJITExecutionCapability>(%u);\n"),
					*Prefix, static_cast<uint32>(Attempt.RequiredCapabilities));
				Source += FString::Printf(TEXT(
					"\t%s.AvailableCapabilities = static_cast<EAngelscriptJITExecutionCapability>(%u);\n"),
					*Prefix, static_cast<uint32>(Attempt.AvailableCapabilities));
				Source += FString::Printf(TEXT(
					"\t%s.ExecutionRequirementsIdentity = %s;\n"),
					*Prefix, *HashExpression(Attempt.ExecutionRequirementsIdentity));
				Source += FString::Printf(TEXT(
					"\t%s.AvailableCapabilityProfileIdentity = %s;\n"),
					*Prefix,
					*HashExpression(Attempt.AvailableCapabilityProfileIdentity));
				Source += FString::Printf(TEXT("\t%s.ProfileKind = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Attempt.ProfileKind));
				Source += FString::Printf(TEXT(
					"\t%s.ExceptionControlState = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Attempt.ExceptionControlState));
				Source += FString::Printf(TEXT(
					"\t%s.ExceptionPayloadState = TEXT(\"%s\");\n"),
					*Prefix, *EscapeJson(Attempt.ExceptionPayloadState));
				Source += FString::Printf(TEXT(
					"\t%s.bRecursionGuardAvailable = %u;\n"),
					*Prefix, Attempt.bRecursionGuardAvailable);
			}
		}

		Source += FString::Printf(TEXT(
			"\tstatic thread_local FAngelscriptJITProviderFunctionDiagnostic GDiagnosticFunctions[%d];\n"),
			Flat.Functions.Num());
		for (int32 Index = 0; Index < Flat.Functions.Num(); ++Index)
		{
			const auto& Function = Flat.Functions[Index];
			const FString Prefix = FString::Printf(
				TEXT("GDiagnosticFunctions[%d]"), Index);
			Source += FString::Printf(TEXT("\t%s = {};\n"), *Prefix);
			Source += FString::Printf(TEXT("\t%s.FunctionKey = {%s};\n"),
				*Prefix, *HashExpression(Function.FunctionKey.Hash));
			Source += FString::Printf(TEXT("\t%s.ActualBackendId = TEXT(\"%s\");\n"),
				*Prefix, *EscapeJson(Function.ActualBackendId));
			Source += FString::Printf(TEXT("\t%s.Disposition = TEXT(\"%s\");\n"),
				*Prefix, *EscapeJson(Function.Disposition));
			Source += FString::Printf(TEXT("\t%s.Detail = TEXT(\"%s\");\n"),
				*Prefix, *EscapeJson(Function.Detail));
			Source += FString::Printf(TEXT("\t%s.BackendAttemptStart = %u;\n"),
				*Prefix, Function.BackendAttemptStart);
			Source += FString::Printf(TEXT("\t%s.BackendAttemptCount = %u;\n"),
				*Prefix, Function.BackendAttemptCount);
		}

		const FAngelscriptJITProviderDiagnostics& Diagnostics = Flat.View;
		Source += TEXT("\tstatic thread_local FAngelscriptJITProviderDiagnostics GProviderDiagnostics;\n");
		Source += TEXT("\tGProviderDiagnostics = {};\n");
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.DiagnosticDigest = %s;\n"),
			*HashExpression(Diagnostics.DiagnosticDigest));
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.RequestKind = TEXT(\"%s\");\n"),
			*EscapeJson(Diagnostics.RequestKind));
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.RequestedBackendId = TEXT(\"%s\");\n"),
			*EscapeJson(Diagnostics.RequestedBackendId));
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.CaptureState = TEXT(\"%s\");\n"),
			*EscapeJson(Diagnostics.CaptureState));
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.FallbackChain = %s;\n"),
			Flat.FallbackChain.IsEmpty()
				? TEXT("nullptr") : TEXT("GDiagnosticFallbackChain"));
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.FallbackChainCount = %u;\n"),
			Diagnostics.FallbackChainCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.SourceCompilePassCount = %u;\n"),
			Diagnostics.SourceCompilePassCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.CompiledModuleCount = %u;\n"),
			Diagnostics.CompiledModuleCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.CompiledFunctionCount = %u;\n"),
			Diagnostics.CompiledFunctionCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.CompiledTypeCount = %u;\n"),
			Diagnostics.CompiledTypeCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.CompiledGlobalCount = %u;\n"),
			Diagnostics.CompiledGlobalCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.CompiledDescriptorCount = %u;\n"),
			Diagnostics.CompiledDescriptorCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.CompiledDependencyCount = %u;\n"),
			Diagnostics.CompiledDependencyCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.CompiledExternalNativeCallCount = %u;\n"),
			Diagnostics.CompiledExternalNativeCallCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.EmitModuleCount = %u;\n"),
			Diagnostics.EmitModuleCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.bHasVerifiedTypedHIR = %u;\n"),
			Diagnostics.bHasVerifiedTypedHIR);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.TaskError = TEXT(\"%s\");\n"),
			*EscapeJson(Diagnostics.TaskError));
		Source += TEXT("\tGProviderDiagnostics.Functions = GDiagnosticFunctions;\n");
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.FunctionCount = %u;\n"),
			Diagnostics.FunctionCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.BackendAttempts = %s;\n"),
			Flat.BackendAttempts.IsEmpty()
				? TEXT("nullptr") : TEXT("GDiagnosticBackendAttempts"));
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.BackendAttemptCount = %u;\n"),
			Diagnostics.BackendAttemptCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.SemanticFunctions = %s;\n"),
			Flat.SemanticFunctions.IsEmpty()
				? TEXT("nullptr") : TEXT("GDiagnosticSemanticFunctions"));
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.SemanticFunctionCount = %u;\n"),
			Diagnostics.SemanticFunctionCount);
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.Calls = %s;\n"),
			Flat.Calls.IsEmpty() ? TEXT("nullptr") : TEXT("GDiagnosticCalls"));
		Source += FString::Printf(TEXT(
			"\tGProviderDiagnostics.CallCount = %u;\n"),
			Diagnostics.CallCount);
	}

	static FString BuildProviderSource(
		const FAngelscriptJITGenerationRequest& Request,
		const FAngelscriptJITGenerationOutput& Output)
	{
		FString Source = FString(OwnershipPrefix)
			+ TEXT("provider-source\n")
			+ TEXT("#include \"Provider.generated.h\"\n\n");
		if (Output.Diagnostics.IsSet())
		{
			Source += TEXT(
				"#include \"StaticJIT/AngelscriptJITProviderDiagnostics.h\"\n\n");
		}
		Source += Request.ProviderSourcePreamble;
		if (!Request.ProviderSourcePreamble.IsEmpty()
			&& !Request.ProviderSourcePreamble.EndsWith(TEXT("\n")))
		{
			Source += TEXT("\n");
		}
		for (const FAngelscriptJITGeneratedFunction& Function : Output.Functions)
		{
			if (Function.bHasVMEntry)
			{
				Source += FString::Printf(
					TEXT("extern void %s(FScriptExecution&, asDWORD*, asQWORD*);\n"),
					*Function.VMEntrySymbol);
			}
			if (Function.bHasRawEntry)
			{
				if (!Function.RawEntryDeclaration.IsEmpty())
				{
					Source += TEXT("extern ") + Function.RawEntryDeclaration;
					if (!Source.EndsWith(TEXT(";")))
					{
						Source += TEXT(";");
					}
					Source += TEXT("\n");
				}
				else
				{
					Source += FString::Printf(TEXT("extern void %s();\n"),
						*Function.RawEntrySymbol);
				}
			}
			if (Function.bHasParmsEntry)
			{
				Source += FString::Printf(
					TEXT("extern void %s(FScriptExecution&, void*, void*);\n"),
					*Function.ParmsEntrySymbol);
			}
		}
		Source += TEXT("\n");

		Source += TEXT("FAngelscriptJITProviderView GetGeneratedAngelscriptJITProviderView()\n{\n");
		for (int32 FunctionIndex = 0;
			FunctionIndex < Output.Functions.Num();
			++FunctionIndex)
		{
			const FAngelscriptJITGeneratedFunction& Function =
				Output.Functions[FunctionIndex];
			if (Function.ReferenceSlots.IsEmpty())
			{
				continue;
			}
			Source += FString::Printf(
				TEXT("\tstatic thread_local FAngelscriptJITReferenceSlot GReferences_%d[%d];\n"),
				FunctionIndex,
				Function.ReferenceSlots.Num());
			for (int32 ReferenceIndex = 0;
				ReferenceIndex < Function.ReferenceSlots.Num();
				++ReferenceIndex)
			{
				const FAngelscriptJITReferenceSlot& Slot =
					Function.ReferenceSlots[ReferenceIndex];
				Source += FString::Printf(
					TEXT("\tGReferences_%d[%d] = {sizeof(FAngelscriptJITReferenceSlot), %u, static_cast<EAngelscriptJITReferenceSlotFlags>(%u), 0, {static_cast<EAngelscriptArtifactReferenceKind>(%u), %s, %s}};\n"),
					FunctionIndex,
					ReferenceIndex,
					Slot.SlotIndex,
					static_cast<uint32>(Slot.Flags),
					static_cast<uint8>(Slot.Reference.Kind),
					*HashExpression(Slot.Reference.StableKey),
					*HashExpression(Slot.Reference.ExpectedAbi));
			}
		}
		for (int32 FunctionIndex = 0;
			FunctionIndex < Output.Functions.Num();
			++FunctionIndex)
		{
			const FAngelscriptJITGeneratedFunction& Function =
				Output.Functions[FunctionIndex];
			if (Function.NativeCallSites.IsEmpty())
			{
				continue;
			}
			Source += FString::Printf(
				TEXT("\tstatic thread_local FAngelscriptJITNativeCallSite GNativeCallSites_%d[%d];\n"),
				FunctionIndex,
				Function.NativeCallSites.Num());
			for (int32 CallSiteIndex = 0;
				CallSiteIndex < Function.NativeCallSites.Num();
				++CallSiteIndex)
			{
				const FAngelscriptJITGenerationNativeCallSite& CallSite =
					Function.NativeCallSites[CallSiteIndex];
				const FString RuntimeCoreExpression =
					CallSite.RuntimeDllCore.IsEmpty()
						? TEXT("nullptr")
						: FString::Printf(TEXT("TEXT(\"%s\")"),
							*EscapeJson(CallSite.RuntimeDllCore));
				const FString MetadataRowExpression =
					CallSite.MetadataRowSymbol.IsEmpty()
						? TEXT("nullptr")
						: FString::Printf(TEXT("TEXT(\"%s\")"),
							*EscapeJson(CallSite.MetadataRowSymbol));
				Source += FString::Printf(TEXT(
					"\tGNativeCallSites_%d[%d] = {sizeof(FAngelscriptJITNativeCallSite), %d, static_cast<EAngelscriptJITNativeCallSiteRoute>(%u), TEXT(\"%s\"), TEXT(\"%s\"), %s, %s, TEXT(\"%s\"), %s, %s, %d, 0};\n"),
					FunctionIndex,
					CallSiteIndex,
					CallSiteIndex,
					static_cast<uint32>(CallSite.Route),
					*EscapeJson(CallSite.CanonicalDeclaration),
					*EscapeJson(CallSite.EmittedCppCallee),
					*RuntimeCoreExpression,
					*MetadataRowExpression,
					*EscapeJson(CallSite.RegisteredTarget),
					*HashExpression(CallSite.StableTargetKey),
					*HashExpression(CallSite.ExpectedAbi),
					CallSite.ReferenceSlotIndex);
			}
		}
		AppendProviderDiagnosticsSource(Source, Output);
		int32 SemanticDependencyCount = 0;
		for (const FAngelscriptJITGeneratedFunction& Function : Output.Functions)
		{
			SemanticDependencyCount += Function.SemanticDependencies.Num();
		}
		if (SemanticDependencyCount > 0)
		{
			Source += FString::Printf(
				TEXT("\tstatic thread_local FAngelscriptJITSemanticDependencyRow GSemanticDependencies[%d];\n"),
				SemanticDependencyCount);
			int32 EmittedDependencyIndex = 0;
			for (const FAngelscriptJITGeneratedFunction& Function
				: Output.Functions)
			{
				for (const FAngelscriptArtifactSemanticDependency& Dependency
					: Function.SemanticDependencies)
				{
					FAngelscriptJITSemanticDependencyRow Row;
					FAngelscriptJITProviderManifest::FillSemanticDependencyRow(
						Dependency,
						static_cast<uint32>(EmittedDependencyIndex),
						Row);
					Source += FString::Printf(
						TEXT("\tGSemanticDependencies[%d] = {sizeof(FAngelscriptJITSemanticDependencyRow), %u, static_cast<EAngelscriptArtifactSemanticDependencyKind>(%u), static_cast<EAngelscriptArtifactReferenceKind>(%u), static_cast<EAngelscriptJITSemanticDependencyFlags>(%u), 0, %s, %s, %s};\n"),
						EmittedDependencyIndex,
						Row.RowIndex,
						static_cast<uint32>(Row.Kind),
						static_cast<uint32>(Row.TargetKind),
						static_cast<uint32>(Row.Flags),
						*HashExpression(Row.TargetStableKey),
						*HashExpression(Row.ExpectedAbi),
						*HashExpression(Row.ExpectedContentOrValue));
					++EmittedDependencyIndex;
				}
			}
		}
		Source += FString::Printf(
			TEXT("\tstatic thread_local FAngelscriptJITArtifactEntry GEntries[%d];\n"),
			Output.Functions.Num());
		for (int32 FunctionIndex = 0;
			FunctionIndex < Output.Functions.Num();
			++FunctionIndex)
		{
			const FAngelscriptJITGeneratedFunction& Function =
				Output.Functions[FunctionIndex];
			const FString VMExpression = Function.bHasVMEntry
				? TEXT("&") + Function.VMEntrySymbol : TEXT("nullptr");
			const FString RawExpression = Function.bHasRawEntry
				? TEXT("(asJITFunction_Raw)(void*)&") + Function.RawEntrySymbol
				: TEXT("nullptr");
			const FString ParmsExpression = Function.bHasParmsEntry
				? TEXT("&") + Function.ParmsEntrySymbol : TEXT("nullptr");
			const FString ReferenceExpression = Function.ReferenceSlots.IsEmpty()
				? TEXT("nullptr")
				: FString::Printf(TEXT("GReferences_%d"), FunctionIndex);
			const FString NativeCallSiteExpression =
				Function.NativeCallSites.IsEmpty()
					? TEXT("nullptr")
					: FString::Printf(TEXT("GNativeCallSites_%d"), FunctionIndex);
			Source += FString::Printf(
				TEXT("\tGEntries[%d] = {sizeof(FAngelscriptJITArtifactEntry), static_cast<EAngelscriptJITArtifactEntryFlags>(%u), {%s}, {%s}, %s, %s, {%s}, %s, %s, {%s, %s, %s}, %s, %d, %s, %d, 0, static_cast<EAngelscriptJITExecutionCapability>(%u), %s, static_cast<EAngelscriptJITExecutionCapability>(%u), %u, %u};\n"),
				FunctionIndex,
				static_cast<uint32>(Function.Flags),
				*HashExpression(Function.ModuleKey.Hash),
				*HashExpression(Function.FunctionKey.Hash),
				*HashExpression(Function.ExecutionHash),
				*HashExpression(Function.DebugHash),
				*HashExpression(Output.ArtifactProfile.Hash),
				*HashExpression(Function.EntryAbiHash),
				*HashExpression(Output.NativeEnvironmentFingerprint),
				*VMExpression,
				*RawExpression,
				*ParmsExpression,
				*ReferenceExpression,
				Function.ReferenceSlots.Num(),
				*NativeCallSiteExpression,
				Function.NativeCallSites.Num(),
				static_cast<uint32>(
					Function.AvailableExecutionCapabilities),
				*HashExpression(Function.ExecutionCapabilityProfileHash),
				static_cast<uint32>(
					Function.RuntimeExecutionRequirementTriggers),
				Function.SemanticDependencyStartIndex,
				Function.SemanticDependencyCount);
		}
		Source += TEXT("\n");
		Source += TEXT("\treturn\n\t{\n");
		Source += TEXT("\t\tsizeof(FAngelscriptJITProviderView),\n");
		Source += FString::Printf(TEXT("\t\t%u,\n"),
			FAngelscriptJITProviderAbi::Revision);
		Source += FString::Printf(TEXT("\t\t{%s},\n"),
			*HashExpression(Output.ProviderId.Hash));
		Source += FString::Printf(TEXT("\t\t{%s},\n"),
			*HashExpression(Output.ProviderGeneration.Hash));
		Source += FString::Printf(TEXT("\t\t%s,\n"),
			*HashExpression(Output.ArtifactSetDigest));
		Source += FString::Printf(TEXT("\t\t{%s},\n"),
			*HashExpression(Output.ArtifactProfile.Hash));
		Source += FString::Printf(TEXT("\t\t%s,\n"),
			*HashExpression(Output.NativeEnvironmentFingerprint));
		Source += FString::Printf(TEXT("\t\tTEXT(\"%s\"),\n"),
			*EscapeJson(Request.ProviderName));
		Source += FString::Printf(TEXT("\t\tTEXT(\"%s\"),\n"),
			*EscapeJson(Request.OwnerModuleName));
		Source += TEXT("\t\tGEntries,\n");
		Source += FString::Printf(TEXT("\t\t%u,\n"), Output.Functions.Num());
		Source += Output.Diagnostics.IsSet()
			? TEXT("\t\t&GProviderDiagnostics,\n")
			: TEXT("\t\tnullptr,\n");
		if (SemanticDependencyCount > 0)
		{
			Source += TEXT("\t\tGSemanticDependencies,\n");
		}
		else
		{
			Source += TEXT("\t\tnullptr,\n");
		}
		Source += FString::Printf(TEXT("\t\t%u\n"),
			static_cast<uint32>(SemanticDependencyCount));
		Source += TEXT("\t};\n}\n");
		return Source;
	}

	static FString BuildOwnedFileInventory(
		const FAngelscriptJITGenerationOutput& Output)
	{
		FString Json = TEXT("{\n");
		Json += FString::Printf(TEXT("  \"ownershipRevision\": %u,\n"),
			FAngelscriptJITGeneration::OwnershipRevision);
		Json += FString::Printf(TEXT("  \"targetProfile\": \"%s\",\n"),
			FAngelscriptJITTargetProfiles::ToString(Output.TargetProfile));
		Json += FString::Printf(TEXT("  \"providerId\": \"%s\",\n"),
			*Output.ProviderId.Hash.ToHexString());
		Json += FString::Printf(TEXT("  \"providerGeneration\": \"%s\",\n"),
			*Output.ProviderGeneration.Hash.ToHexString());
		Json += TEXT("  \"files\": [\n");
		for (int32 Index = 0; Index < Output.OwnedFiles.Num(); ++Index)
		{
			Json += FString::Printf(TEXT("    \"%s\"%s\n"),
				*EscapeJson(Output.OwnedFiles[Index]),
				Index + 1 < Output.OwnedFiles.Num() ? TEXT(",") : TEXT(""));
		}
		Json += TEXT("  ]\n}\n");
		return Json;
	}
}

const TCHAR* FAngelscriptJITTargetProfiles::ToString(
	const EAngelscriptJITTargetProfile Profile)
{
	switch (Profile)
	{
	case EAngelscriptJITTargetProfile::EditorDevelopment:
		return TEXT("EditorDevelopment");
	case EAngelscriptJITTargetProfile::GameDevelopment:
		return TEXT("GameDevelopment");
	case EAngelscriptJITTargetProfile::GameShipping:
		return TEXT("GameShipping");
	case EAngelscriptJITTargetProfile::All:
		return TEXT("All");
	default:
		return TEXT("Invalid");
	}
}

bool FAngelscriptJITTargetProfiles::TryParse(
	const FStringView Value,
	EAngelscriptJITTargetProfile& OutProfile)
{
	OutProfile = EAngelscriptJITTargetProfile::Invalid;
	if (Value.Equals(TEXT("EditorDevelopment"), ESearchCase::IgnoreCase))
	{
		OutProfile = EAngelscriptJITTargetProfile::EditorDevelopment;
	}
	else if (Value.Equals(TEXT("GameDevelopment"), ESearchCase::IgnoreCase))
	{
		OutProfile = EAngelscriptJITTargetProfile::GameDevelopment;
	}
	else if (Value.Equals(TEXT("GameShipping"), ESearchCase::IgnoreCase))
	{
		OutProfile = EAngelscriptJITTargetProfile::GameShipping;
	}
	else if (Value.Equals(TEXT("All"), ESearchCase::IgnoreCase))
	{
		OutProfile = EAngelscriptJITTargetProfile::All;
	}
	return OutProfile != EAngelscriptJITTargetProfile::Invalid;
}

TOptional<FAngelscriptJITTargetProfileTraits>
FAngelscriptJITTargetProfiles::GetTraits(
	const EAngelscriptJITTargetProfile Profile)
{
	FAngelscriptJITTargetProfileTraits Traits;
	switch (Profile)
	{
	case EAngelscriptJITTargetProfile::EditorDevelopment:
		Traits.bEditor = true;
		Traits.bEditorOnlyData = true;
		Traits.bTest = true;
		Traits.bIncludeDevelopmentScripts = true;
		Traits.bIncludeEditorScripts = true;
		return Traits;
	case EAngelscriptJITTargetProfile::GameDevelopment:
		Traits.bTest = true;
		Traits.bUseCookedBindings = true;
		return Traits;
	case EAngelscriptJITTargetProfile::GameShipping:
		Traits.bRelease = true;
		Traits.bUseCookedBindings = true;
		return Traits;
	default:
		return {};
	}
}

FAngelscriptHash256
FAngelscriptJITTargetProfiles::BuildNativeEnvironmentFingerprint(
	const EAngelscriptJITTargetProfile Profile,
	const TConstArrayView<FString> CanonicalInputs)
{
	const TOptional<FAngelscriptJITTargetProfileTraits> Traits =
		GetTraits(Profile);
	if (!Traits.IsSet())
	{
		return {};
	}

	TArray<FString> SortedInputs(CanonicalInputs);
	SortedInputs.Sort([](const FString& Left, const FString& Right)
	{
		return FAngelscriptArtifactCanonicalWriter::
			CompareCanonicalUtf8Strings(Left, Right) < 0;
	});
	TArray<FString> UniqueInputs;
	UniqueInputs.Reserve(SortedInputs.Num());
	for (FString& Input : SortedInputs)
	{
		if (UniqueInputs.IsEmpty() || UniqueInputs.Last() != Input)
		{
			UniqueInputs.Add(MoveTemp(Input));
		}
	}

	FAngelscriptArtifactCanonicalWriter Writer(
		TEXT("jit-native-environment-v1"));
	Writer.WriteUInt32(FAngelscriptJITGeneration::SchemaRevision);
	Writer.WriteUInt32(FAngelscriptJITProviderAbi::Revision);
	Writer.WriteString(ToString(Profile));
	Writer.WriteBool(Traits->bEditor);
	Writer.WriteBool(Traits->bEditorOnlyData);
	Writer.WriteBool(Traits->bRelease);
	Writer.WriteBool(Traits->bTest);
	Writer.WriteBool(Traits->bUseCookedBindings);
	Writer.WriteBool(Traits->bIncludeDevelopmentScripts);
	Writer.WriteBool(Traits->bIncludeEditorScripts);
	Writer.WriteUInt32(static_cast<uint32>(UniqueInputs.Num()));
	for (const FString& Input : UniqueInputs)
	{
		Writer.WriteString(Input);
	}
	return Writer.FinalizeHash();
}

FAngelscriptHash256
FAngelscriptJITTargetProfiles::BuildCurrentNativeEnvironmentFingerprint(
	const EAngelscriptJITTargetProfile Profile)
{
	const TOptional<FAngelscriptJITTargetProfileTraits> Traits =
		GetTraits(Profile);
	if (!Traits.IsSet())
	{
		return {};
	}

	// The commandlet generates all profiles inside an Editor process. Game
	// providers must therefore name the target platform family ("Windows"),
	// not the host executable flavor ("WindowsEditor"), so the generated
	// identity is identical to the one recomputed by the packaged process.
	const ANSICHAR* TargetPlatformName = Traits->bEditor
		? FPlatformProperties::PlatformName()
		: FPlatformProperties::IniPlatformName();
	const TArray<FString> NativeInputs = {
		FString::Printf(TEXT("Platform=%hs"),
			TargetPlatformName),
		FString::Printf(TEXT("PointerBits=%u"),
			static_cast<uint32>(sizeof(void*) * 8)),
		TEXT("Engine=") + FEngineVersion::Current().ToString(),
		Traits->bUseCookedBindings
			? TEXT("Bindings=Cooked") : TEXT("Bindings=Editor"),
	};
	return BuildNativeEnvironmentFingerprint(Profile, NativeInputs);
}

FString FAngelscriptJITGeneration::BuildSymbolPrefix(
	const FAngelscriptStableFunctionKey& FunctionKey,
	const FAngelscriptHash256& ExecutionHash)
{
	return FString::Printf(TEXT("ASJIT_%s_%s"),
		*FunctionKey.Hash.ToHexString(), *ExecutionHash.ToHexString());
}

FString FAngelscriptJITGeneration::BuildModuleSourceRelativePath(
	const FAngelscriptStableModuleKey& ModuleKey,
	const EAngelscriptJITTargetProfile TargetProfile)
{
	// Provider catalogs do not carry source paths. Keep this diagnostic-only
	// fallback readable and outside every emitted mount-derived directory.
	return FString::Printf(TEXT("Unmapped/Module.%s.%s.jit.cpp"),
		*ModuleKey.Hash.ToHexString().Left(8),
		FAngelscriptJITTargetProfiles::ToString(TargetProfile));
}

FAngelscriptHash256 FAngelscriptJITGeneration::BuildEntryAbiHash(
	const FStringView CanonicalDeclaration,
	const bool bHasVMEntry,
	const bool bHasRawEntry,
	const bool bHasParmsEntry)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("jit-entry-abi-v1"));
	Writer.WriteUInt32(FAngelscriptJITProviderAbi::Revision);
	Writer.WriteString(CanonicalDeclaration);
	Writer.WriteBool(bHasVMEntry);
	Writer.WriteBool(bHasRawEntry);
	Writer.WriteBool(bHasParmsEntry);
	return Writer.FinalizeHash();
}

bool FAngelscriptJITGeneration::Generate(
	const FAngelscriptJITGenerationRequest& Request,
	FAngelscriptJITGenerationOutput& OutOutput,
	FString* OutError)
{
	using namespace AngelscriptJITGeneration_Private;
	OutOutput = {};
	auto Fail = [OutError](const FString& Error)
	{
		if (OutError != nullptr)
		{
			*OutError = Error;
		}
		return false;
	};
	if (!Request.ProviderId.IsValid())
	{
		return Fail(TEXT("JIT generation requires a non-zero ProviderId."));
	}
	if (!FAngelscriptJITTargetProfiles::GetTraits(
		Request.TargetProfile).IsSet())
	{
		return Fail(TEXT("JIT generation requires one concrete target profile."));
	}
	if (Request.ProviderName.IsEmpty() || Request.OwnerModuleName.IsEmpty())
	{
		return Fail(TEXT("JIT generation requires provider and owner module names."));
	}
	if (Request.ArtifactProfile.Hash.IsZero()
		|| Request.NativeEnvironmentFingerprint.IsZero())
	{
		return Fail(TEXT("JIT generation requires non-zero profile and environment identities."));
	}
	if (Request.Functions.IsEmpty())
	{
		return Fail(TEXT("JIT generation requires at least one function."));
	}
	if (Request.Modules.IsEmpty())
	{
		return Fail(TEXT("JIT generation requires at least one script module."));
	}

	TArray<FAngelscriptJITGenerationModule> Modules = Request.Modules;
	Modules.Sort(ModuleLess);
	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
	{
		const FAngelscriptJITGenerationModule& Module = Modules[ModuleIndex];
		if (Module.ModuleKey.Hash.IsZero()
			|| Module.CanonicalModuleName.TrimStartAndEnd().IsEmpty()
			|| Module.VirtualSourcePath.TrimStartAndEnd().IsEmpty())
		{
			return Fail(FString::Printf(
				TEXT("Generated module %d has an incomplete stable identity."),
				ModuleIndex));
		}
		if (ModuleIndex > 0
			&& Module.ModuleKey == Modules[ModuleIndex - 1].ModuleKey)
		{
			return Fail(FString::Printf(
				TEXT("Duplicate generated module identity %s."),
				*Module.ModuleKey.Hash.ToHexString()));
		}
	}
	TArray<FString> ModuleSourceRelativePaths;
	FString ModulePathError;
	if (!BuildModuleSourceRelativePaths(
		Modules, Request.TargetProfile,
		ModuleSourceRelativePaths, ModulePathError))
	{
		return Fail(ModulePathError);
	}

	TArray<FAngelscriptJITGenerationFunction> Functions = Request.Functions;
	Functions.Sort(FunctionLess);
	OutOutput.TargetProfile = Request.TargetProfile;
	OutOutput.ProviderId = Request.ProviderId;
	OutOutput.ArtifactProfile = Request.ArtifactProfile;
	OutOutput.NativeEnvironmentFingerprint =
		Request.NativeEnvironmentFingerprint;

	TArray<TArray<FAngelscriptJITReferenceSlot>> DigestReferences;
	TArray<TArray<FAngelscriptJITNativeCallSite>> DigestNativeCallSites;
	TArray<FAngelscriptJITSemanticDependencyRow> DigestSemanticDependencies;
	TArray<FAngelscriptJITArtifactEntry> DigestEntries;
	TArray<FString> Implementations;
	TArray<uint32> ModuleFunctionCounts;
	DigestReferences.SetNum(Functions.Num());
	DigestNativeCallSites.SetNum(Functions.Num());
	DigestEntries.SetNum(Functions.Num());
	Implementations.SetNum(Functions.Num());
	ModuleFunctionCounts.SetNumZeroed(Modules.Num());
	for (int32 FunctionIndex = 0;
		FunctionIndex < Functions.Num();
		++FunctionIndex)
	{
		const FAngelscriptJITGenerationFunction& Function =
			Functions[FunctionIndex];
		if (FunctionIndex > 0
			&& Function.ModuleKey == Functions[FunctionIndex - 1].ModuleKey
			&& Function.FunctionKey == Functions[FunctionIndex - 1].FunctionKey)
		{
			return Fail(FString::Printf(
				TEXT("Duplicate generated function identity %s."),
				*Function.FunctionKey.Hash.ToHexString()));
		}
		if (Function.ModuleKey.Hash.IsZero()
			|| Function.FunctionKey.Hash.IsZero()
			|| Function.ExecutionHash.IsZero()
			|| Function.DebugHash.IsZero()
			|| Function.EntryAbiHash.IsZero()
			|| Function.CanonicalDeclaration.TrimStartAndEnd().IsEmpty()
			|| Function.SourceSection.TrimStartAndEnd().IsEmpty()
			|| Function.SourceLine == 0)
		{
			return Fail(FString::Printf(
				TEXT("Generated function %d has an incomplete stable identity: FunctionKey=%s Declaration='%s' Source='%s' Line=%u Column=%u."),
				FunctionIndex,
				*Function.FunctionKey.Hash.ToHexString(),
				*OneLineCommentValue(Function.CanonicalDeclaration),
				*OneLineCommentValue(Function.SourceSection),
				Function.SourceLine,
				Function.SourceColumn));
		}
		const bool bHasExecutionCapabilityProfile = EnumHasAnyFlags(
			Function.Flags,
			EAngelscriptJITArtifactEntryFlags::HasExecutionCapabilityProfile);
		const uint32 AvailableExecutionCapabilities = static_cast<uint32>(
			Function.AvailableExecutionCapabilities);
		const uint32 RuntimeExecutionRequirementTriggers =
			static_cast<uint32>(
				Function.RuntimeExecutionRequirementTriggers);
		if ((AvailableExecutionCapabilities
				& ~FAngelscriptJITExecutionCapabilityMask::All) != 0
			|| (RuntimeExecutionRequirementTriggers
				& ~FAngelscriptJITExecutionCapabilityMask::All) != 0
			|| (!bHasExecutionCapabilityProfile
				&& (AvailableExecutionCapabilities != 0
					|| RuntimeExecutionRequirementTriggers != 0))
			|| (bHasExecutionCapabilityProfile
				== Function.ExecutionCapabilityProfileHash.IsZero()))
		{
			return Fail(FString::Printf(TEXT(
				"Generated function %s has an invalid execution capability profile."),
				*Function.FunctionKey.Hash.ToHexString()));
		}
		const int32 ModuleIndex = Modules.IndexOfByPredicate(
			[ModuleKey = Function.ModuleKey](
				const FAngelscriptJITGenerationModule& Candidate)
			{
				return Candidate.ModuleKey == ModuleKey;
			});
		if (ModuleIndex == INDEX_NONE)
		{
			return Fail(FString::Printf(
				TEXT("Generated function %s names undeclared module %s."),
				*Function.FunctionKey.Hash.ToHexString(),
				*Function.ModuleKey.Hash.ToHexString()));
		}
		++ModuleFunctionCounts[ModuleIndex];
		if (!Function.bHasVMEntry && !Function.bHasRawEntry
			&& !Function.bHasParmsEntry)
		{
			return Fail(FString::Printf(
				TEXT("Generated function %s has no entry points."),
				*Function.FunctionKey.Hash.ToHexString()));
		}
		if ((Function.bHasVMEntry
				&& !Function.ImplementationTemplate.Contains(
					TEXT("{VM_ENTRY_COMMENT}")))
			|| (Function.bHasRawEntry
				&& !Function.ImplementationTemplate.Contains(
					TEXT("{RAW_ENTRY_COMMENT}")))
			|| (Function.bHasParmsEntry
				&& !Function.ImplementationTemplate.Contains(
					TEXT("{PARMS_ENTRY_COMMENT}"))))
		{
			return Fail(FString::Printf(
				TEXT("Generated function %s is missing an entry-point source comment placeholder."),
				*Function.FunctionKey.Hash.ToHexString()));
		}

		FAngelscriptJITGeneratedFunction& Generated =
			OutOutput.Functions.Emplace_GetRef();
		Generated.Flags = Function.Flags;
		Generated.ModuleKey = Function.ModuleKey;
		Generated.FunctionKey = Function.FunctionKey;
		Generated.ExecutionHash = Function.ExecutionHash;
		Generated.DebugHash = Function.DebugHash;
		Generated.EntryAbiHash = Function.EntryAbiHash;
		Generated.AvailableExecutionCapabilities =
			Function.AvailableExecutionCapabilities;
		Generated.ExecutionCapabilityProfileHash =
			Function.ExecutionCapabilityProfileHash;
		Generated.RuntimeExecutionRequirementTriggers =
			Function.RuntimeExecutionRequirementTriggers;
		Generated.bHasVMEntry = Function.bHasVMEntry;
		Generated.bHasRawEntry = Function.bHasRawEntry;
		Generated.bHasParmsEntry = Function.bHasParmsEntry;
		Generated.SymbolPrefix = BuildSymbolPrefix(
			Function.FunctionKey, Function.ExecutionHash);
		Generated.VMEntrySymbol = Function.VMEntrySymbol.IsEmpty()
			? Generated.SymbolPrefix + TEXT("_VM") : Function.VMEntrySymbol;
		Generated.RawEntrySymbol = Function.RawEntrySymbol.IsEmpty()
			? Generated.SymbolPrefix + TEXT("_Raw") : Function.RawEntrySymbol;
		Generated.ParmsEntrySymbol = Function.ParmsEntrySymbol.IsEmpty()
			? Generated.SymbolPrefix + TEXT("_Parms") : Function.ParmsEntrySymbol;
		Generated.RawEntryDeclaration = Function.RawEntryDeclaration;
		Generated.ModuleSourceRelativePath =
			ModuleSourceRelativePaths[ModuleIndex];
		Generated.CanonicalDeclaration = Function.CanonicalDeclaration;
		Generated.SourceSection = Function.SourceSection;
		Generated.SourceLine = Function.SourceLine;
		Generated.SourceColumn = Function.SourceColumn;

		TArray<FAngelscriptJITGenerationReference> References =
			Function.References;
		References.Sort(ReferenceLess);
		for (int32 ReferenceIndex = 0;
			ReferenceIndex < References.Num();
			++ReferenceIndex)
		{
			const FAngelscriptJITGenerationReference& Reference =
				References[ReferenceIndex];
			if (!Reference.Reference.IsValid()
				|| Reference.Reference.ExpectedAbi.IsZero())
			{
				return Fail(FString::Printf(
					TEXT("Generated function %s has invalid reference %d."),
					*Function.FunctionKey.Hash.ToHexString(),
					ReferenceIndex));
			}
			FAngelscriptJITReferenceSlot& Slot =
				Generated.ReferenceSlots.Emplace_GetRef();
			Slot.SlotIndex = static_cast<uint32>(ReferenceIndex);
			Slot.Flags = Reference.Flags;
			Slot.Reference = Reference.Reference;
		}
		DigestReferences[FunctionIndex] = Generated.ReferenceSlots;
		Generated.NativeCallSites = Function.NativeCallSites;
		DigestNativeCallSites[FunctionIndex].Reserve(
			Generated.NativeCallSites.Num());
		for (int32 CallSiteIndex = 0;
			CallSiteIndex < Generated.NativeCallSites.Num();
			++CallSiteIndex)
		{
			const FAngelscriptJITGenerationNativeCallSite& SourceCallSite =
				Generated.NativeCallSites[CallSiteIndex];
			FAngelscriptJITNativeCallSite& DigestCallSite =
				DigestNativeCallSites[FunctionIndex].Emplace_GetRef();
			DigestCallSite.CallSiteIndex =
				static_cast<uint32>(CallSiteIndex);
			DigestCallSite.Route = SourceCallSite.Route;
			DigestCallSite.CanonicalDeclaration =
				*SourceCallSite.CanonicalDeclaration;
			DigestCallSite.EmittedCppCallee =
				*SourceCallSite.EmittedCppCallee;
			DigestCallSite.RuntimeDllCore =
				SourceCallSite.RuntimeDllCore.IsEmpty()
					? nullptr : *SourceCallSite.RuntimeDllCore;
			DigestCallSite.MetadataRowSymbol =
				SourceCallSite.MetadataRowSymbol.IsEmpty()
					? nullptr : *SourceCallSite.MetadataRowSymbol;
			DigestCallSite.RegisteredTarget =
				*SourceCallSite.RegisteredTarget;
			DigestCallSite.StableTargetKey =
				SourceCallSite.StableTargetKey;
			DigestCallSite.ExpectedAbi = SourceCallSite.ExpectedAbi;
			DigestCallSite.ReferenceSlotIndex =
				SourceCallSite.ReferenceSlotIndex;
		}

		FAngelscriptJITArtifactEntry& DigestEntry =
			DigestEntries[FunctionIndex];
		DigestEntry.Flags = Generated.Flags;
		DigestEntry.ModuleKey = Generated.ModuleKey;
		DigestEntry.FunctionKey = Generated.FunctionKey;
		DigestEntry.ExecutionHash = Generated.ExecutionHash;
		DigestEntry.DebugHash = Generated.DebugHash;
		DigestEntry.ArtifactProfile = Request.ArtifactProfile;
		DigestEntry.EntryAbiHash = Generated.EntryAbiHash;
		DigestEntry.AvailableExecutionCapabilities =
			Generated.AvailableExecutionCapabilities;
		DigestEntry.ExecutionCapabilityProfileHash =
			Generated.ExecutionCapabilityProfileHash;
		DigestEntry.RuntimeExecutionRequirementTriggers =
			Generated.RuntimeExecutionRequirementTriggers;
		DigestEntry.NativeEnvironmentFingerprint =
			Request.NativeEnvironmentFingerprint;
		DigestEntry.EntryPoints.VMEntry = Generated.bHasVMEntry
			? &DigestVMEntry : nullptr;
		DigestEntry.EntryPoints.RawEntry = Generated.bHasRawEntry
			? &DigestRawEntry : nullptr;
		DigestEntry.EntryPoints.ParmsEntry = Generated.bHasParmsEntry
			? &DigestParmsEntry : nullptr;
		DigestEntry.ReferenceSlots = DigestReferences[FunctionIndex].GetData();
		DigestEntry.ReferenceSlotCount =
			DigestReferences[FunctionIndex].Num();
		DigestEntry.NativeCallSites =
			DigestNativeCallSites[FunctionIndex].GetData();
		DigestEntry.NativeCallSiteCount =
			DigestNativeCallSites[FunctionIndex].Num();

		const FAngelscriptJITSemanticDependencyCanonicalResult Canonical =
			FAngelscriptJITProviderManifest::CanonicalizeSemanticDependencies(
				Function.SemanticDependencies);
		if (!Canonical.IsValid())
		{
			return Fail(FString::Printf(TEXT(
				"Generated function %s has invalid semantic dependencies (%u)."),
				*Function.FunctionKey.Hash.ToHexString(),
				static_cast<uint32>(Canonical.Code)));
		}
		Generated.SemanticDependencies = Canonical.Rows;
		Generated.SemanticDependencyStartIndex =
			static_cast<uint32>(DigestSemanticDependencies.Num());
		Generated.SemanticDependencyCount =
			static_cast<uint32>(Canonical.Rows.Num());
		for (int32 DependencyIndex = 0;
			DependencyIndex < Canonical.Rows.Num();
			++DependencyIndex)
		{
			FAngelscriptJITSemanticDependencyRow& Row =
				DigestSemanticDependencies.Emplace_GetRef();
			FAngelscriptJITProviderManifest::FillSemanticDependencyRow(
				Canonical.Rows[DependencyIndex],
				static_cast<uint32>(DigestSemanticDependencies.Num() - 1),
				Row);
		}
		DigestEntry.SemanticDependencyStartIndex =
			Generated.SemanticDependencyStartIndex;
		DigestEntry.SemanticDependencyCount =
			Generated.SemanticDependencyCount;

		FString Implementation = Function.ImplementationTemplate;
		Implementation.ReplaceInline(TEXT("{SYMBOL_PREFIX}"),
			*Generated.SymbolPrefix, ESearchCase::CaseSensitive);
		Implementation.ReplaceInline(TEXT("{VM_SYMBOL}"),
			*Generated.VMEntrySymbol,
			ESearchCase::CaseSensitive);
		Implementation.ReplaceInline(TEXT("{RAW_SYMBOL}"),
			*Generated.RawEntrySymbol,
			ESearchCase::CaseSensitive);
		Implementation.ReplaceInline(TEXT("{PARMS_SYMBOL}"),
			*Generated.ParmsEntrySymbol,
			ESearchCase::CaseSensitive);
		Implementation.ReplaceInline(TEXT("{VM_ENTRY_COMMENT}"),
			*BuildEntryComment(Function, TEXT("VM")),
			ESearchCase::CaseSensitive);
		Implementation.ReplaceInline(TEXT("{RAW_ENTRY_COMMENT}"),
			*BuildEntryComment(Function, TEXT("Raw")),
			ESearchCase::CaseSensitive);
		Implementation.ReplaceInline(TEXT("{PARMS_ENTRY_COMMENT}"),
			*BuildEntryComment(Function, TEXT("Parms")),
			ESearchCase::CaseSensitive);
		Implementation = BuildFunctionMetadataBlock(Generated)
			+ Implementation;
		if (!Implementation.EndsWith(TEXT("\n")))
		{
			Implementation += TEXT("\n");
		}
		Implementations[FunctionIndex] = MoveTemp(Implementation);
	}

	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
	{
		if (ModuleFunctionCounts[ModuleIndex] == 0)
		{
			return Fail(FString::Printf(
				TEXT("Generated module %s owns no generated functions."),
				*Modules[ModuleIndex].ModuleKey.Hash.ToHexString()));
		}

		FAngelscriptJITGeneratedModule& GeneratedModule =
			OutOutput.Modules.Emplace_GetRef();
		GeneratedModule.ModuleKey = Modules[ModuleIndex].ModuleKey;
		GeneratedModule.CanonicalModuleName =
			Modules[ModuleIndex].CanonicalModuleName;
		GeneratedModule.VirtualSourcePath =
			Modules[ModuleIndex].VirtualSourcePath;
		GeneratedModule.SourceRelativePath =
			ModuleSourceRelativePaths[ModuleIndex];
		GeneratedModule.FunctionCount = ModuleFunctionCounts[ModuleIndex];

		FAngelscriptJITGeneratedFile& ModuleSource =
			OutOutput.Files.Emplace_GetRef();
		ModuleSource.RelativePath = GeneratedModule.SourceRelativePath;
		ModuleSource.Kind = EAngelscriptJITGeneratedFileKind::ModuleSource;
		const TCHAR* const Guard = ProfileGuard(Request.TargetProfile);
		ModuleSource.Content = FString(OwnershipPrefix)
			+ TEXT("module-source\n")
			+ BuildModuleMetadataBlock(Request, GeneratedModule)
			+ FString::Printf(TEXT("#if %s\n"), Guard)
			+ Request.ModuleSourcePreamble;
		if (!Request.ModuleSourcePreamble.IsEmpty()
			&& !Request.ModuleSourcePreamble.EndsWith(TEXT("\n")))
		{
			ModuleSource.Content += TEXT("\n");
		}
		for (int32 FunctionIndex = 0;
			FunctionIndex < OutOutput.Functions.Num();
			++FunctionIndex)
		{
			if (OutOutput.Functions[FunctionIndex].ModuleKey
				== GeneratedModule.ModuleKey)
			{
				ModuleSource.Content += Implementations[FunctionIndex];
			}
		}
		ModuleSource.Content += Request.ModuleSourceFooter;
		if (!Request.ModuleSourceFooter.IsEmpty()
			&& !Request.ModuleSourceFooter.EndsWith(TEXT("\n")))
		{
			ModuleSource.Content += TEXT("\n");
		}
		ModuleSource.Content += FString::Printf(
			TEXT("#endif // %s\n"), Guard);
		GeneratedModule.ContentHash = HashUtf8Content(ModuleSource.Content);
	}

	OutOutput.ArtifactSetDigest =
		FAngelscriptJITProviderManifest::BuildArtifactSetDigest(
			DigestEntries,
			DigestSemanticDependencies);
	FAngelscriptJITProviderView DigestView;
	DigestView.ProviderId = Request.ProviderId;
	DigestView.ArtifactSetDigest = OutOutput.ArtifactSetDigest;
	DigestView.ArtifactProfile = Request.ArtifactProfile;
	DigestView.NativeEnvironmentFingerprint =
		Request.NativeEnvironmentFingerprint;
	DigestView.ProviderName = *Request.ProviderName;
	DigestView.OwnerModuleName = *Request.OwnerModuleName;
	DigestView.Entries = DigestEntries.GetData();
	DigestView.EntryCount = DigestEntries.Num();
	DigestView.SemanticDependencies = DigestSemanticDependencies.Num() > 0
		? DigestSemanticDependencies.GetData() : nullptr;
	DigestView.SemanticDependencyCount =
		static_cast<uint32>(DigestSemanticDependencies.Num());
	OutOutput.ProviderGeneration =
		FAngelscriptJITProviderManifest::BuildProviderGeneration(DigestView);
	DigestView.ProviderGeneration = OutOutput.ProviderGeneration;
	FProviderDiagnosticFlatView DiagnosticFlatView;
	if (Request.TargetProfile != EAngelscriptJITTargetProfile::GameShipping
		&& Request.Diagnostics.IsSet())
	{
		FAngelscriptJITGenerationDiagnostics NormalizedDiagnostics;
		FString DiagnosticError;
		if (!NormalizeProviderDiagnostics(
			OutOutput.Functions,
			Request.Diagnostics.GetValue(),
			NormalizedDiagnostics,
			DiagnosticError))
		{
			return Fail(DiagnosticError);
		}
		OutOutput.Diagnostics = MoveTemp(NormalizedDiagnostics);
		if (!BuildProviderDiagnosticFlatView(
			OutOutput.Diagnostics.GetValue(),
			DiagnosticFlatView,
			DiagnosticError))
		{
			return Fail(DiagnosticError);
		}
		OutOutput.DiagnosticDigest =
			DiagnosticFlatView.View.DiagnosticDigest;
		DigestView.Diagnostics = &DiagnosticFlatView.View;
	}
	const FAngelscriptJITProviderValidationResult DigestValidation =
		FAngelscriptJITProviderManifest::Validate(DigestView);
	if (!DigestValidation.IsValid())
	{
		return Fail(FString::Printf(TEXT(
			"Generated Provider view failed validation: Code=%u Entry=%d Reference=%d NativeCallSite=%d DiagnosticFunction=%d DiagnosticAttempt=%d DiagnosticSemantic=%d DiagnosticCall=%d."),
			static_cast<uint32>(DigestValidation.Code),
			DigestValidation.EntryIndex,
			DigestValidation.ReferenceIndex,
			DigestValidation.NativeCallSiteIndex,
			DigestValidation.DiagnosticFunctionIndex,
			DigestValidation.DiagnosticAttemptIndex,
			DigestValidation.DiagnosticSemanticIndex,
			DigestValidation.DiagnosticCallIndex));
	}

	OutOutput.Files.Add({TEXT("Provider.generated.h"),
		BuildProviderHeader(),
		EAngelscriptJITGeneratedFileKind::ProviderHeader});
	OutOutput.Files.Add({TEXT("Provider.generated.cpp"),
		BuildProviderSource(Request, OutOutput),
		EAngelscriptJITGeneratedFileKind::ProviderSource});
	OutOutput.Files.Add({TEXT("ProviderManifest.generated.json"),
		BuildManifestJson(Request, OutOutput),
		EAngelscriptJITGeneratedFileKind::ProviderManifest});
	OutOutput.Files.Add({TEXT("OwnedFiles.generated.json"), TEXT(""),
		EAngelscriptJITGeneratedFileKind::OwnedFileInventory});

	OutOutput.Files.Sort(FileLess);
	OutOutput.OwnedFiles.Reserve(OutOutput.Files.Num());
	for (const FAngelscriptJITGeneratedFile& File : OutOutput.Files)
	{
		OutOutput.OwnedFiles.Add(File.RelativePath);
	}
	FAngelscriptJITGeneratedFile* Inventory =
		OutOutput.Files.FindByPredicate([](const FAngelscriptJITGeneratedFile& File)
		{
			return File.Kind
				== EAngelscriptJITGeneratedFileKind::OwnedFileInventory;
		});
	check(Inventory != nullptr);
	Inventory->Content = BuildOwnedFileInventory(OutOutput);
	return true;
}

bool FAngelscriptJITGeneration::RefreshOwnedFileInventory(
	FAngelscriptJITGenerationOutput& InOutOutput,
	FString* OutError)
{
	auto Fail = [OutError](const FString& Message)
	{
		if (OutError != nullptr)
		{
			*OutError = Message;
		}
		return false;
	};

	FAngelscriptJITGeneratedFile* Inventory = nullptr;
	TSet<FString> SeenPaths;
	InOutOutput.OwnedFiles.Reset();
	for (FAngelscriptJITGeneratedFile& File : InOutOutput.Files)
	{
		FPaths::NormalizeFilename(File.RelativePath);
		if (File.RelativePath.IsEmpty()
			|| !FPaths::IsRelative(File.RelativePath)
			|| File.RelativePath == TEXT("..")
			|| File.RelativePath.StartsWith(TEXT("../"))
			|| File.RelativePath.Contains(TEXT("/../"))
			|| SeenPaths.Contains(File.RelativePath))
		{
			return Fail(
				TEXT("JIT owned-file inventory refresh found an invalid or duplicate path."));
		}
		SeenPaths.Add(File.RelativePath);
		InOutOutput.OwnedFiles.Add(File.RelativePath);
		if (File.Kind == EAngelscriptJITGeneratedFileKind::OwnedFileInventory)
		{
			if (Inventory != nullptr)
			{
				return Fail(
					TEXT("JIT owned-file inventory refresh found several inventory files."));
			}
			Inventory = &File;
		}
	}
	if (Inventory == nullptr)
	{
		return Fail(
			TEXT("JIT owned-file inventory refresh found no inventory file."));
	}
	InOutOutput.OwnedFiles.Sort(AngelscriptJITGeneration_Private::PathLess);
	Inventory->Content = AngelscriptJITGeneration_Private::
		BuildOwnedFileInventory(InOutOutput);
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	return true;
}

TArray<FString> FAngelscriptJITGeneration::FindRemovedOwnedFiles(
	const FAngelscriptJITGenerationOutput& Previous,
	const FAngelscriptJITGenerationOutput& Current)
{
	using namespace AngelscriptJITGeneration_Private;
	TSet<FString> CurrentFiles(Current.OwnedFiles);
	TArray<FString> Removed;
	for (const FString& PreviousFile : Previous.OwnedFiles)
	{
		if (!CurrentFiles.Contains(PreviousFile))
		{
			Removed.Add(PreviousFile);
		}
	}
	Removed.Sort(PathLess);
	return Removed;
}
