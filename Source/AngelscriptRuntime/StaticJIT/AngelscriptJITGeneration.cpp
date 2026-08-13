#include "StaticJIT/AngelscriptJITGeneration.h"

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
			TEXT(" */\n"),
			*OneLineCommentValue(Function.CanonicalDeclaration),
			*OneLineCommentValue(Function.SourceSection),
			Function.SourceLine,
			Function.SourceColumn,
			*Function.FunctionKey.Hash.ToHexString(),
			*Function.ExecutionHash.ToHexString(),
			*Function.DebugHash.ToHexString(),
			*Function.EntryAbiHash.ToHexString());
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

	static FString BuildProviderSource(
		const FAngelscriptJITGenerationRequest& Request,
		const FAngelscriptJITGenerationOutput& Output)
	{
		FString Source = FString(OwnershipPrefix)
			+ TEXT("provider-source\n")
			+ TEXT("#include \"Provider.generated.h\"\n\n");
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
			Source += FString::Printf(
				TEXT("\tGEntries[%d] = {sizeof(FAngelscriptJITArtifactEntry), static_cast<EAngelscriptJITArtifactEntryFlags>(%u), {%s}, {%s}, %s, %s, {%s}, %s, %s, {%s, %s, %s}, %s, %d, 0};\n"),
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
				Function.ReferenceSlots.Num());
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
		Source += FString::Printf(TEXT("\t\t%u\n"), Output.Functions.Num());
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
	TArray<FAngelscriptJITArtifactEntry> DigestEntries;
	TArray<FString> Implementations;
	TArray<uint32> ModuleFunctionCounts;
	DigestReferences.SetNum(Functions.Num());
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

		FAngelscriptJITArtifactEntry& DigestEntry =
			DigestEntries[FunctionIndex];
		DigestEntry.Flags = Generated.Flags;
		DigestEntry.ModuleKey = Generated.ModuleKey;
		DigestEntry.FunctionKey = Generated.FunctionKey;
		DigestEntry.ExecutionHash = Generated.ExecutionHash;
		DigestEntry.DebugHash = Generated.DebugHash;
		DigestEntry.ArtifactProfile = Request.ArtifactProfile;
		DigestEntry.EntryAbiHash = Generated.EntryAbiHash;
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
		FAngelscriptJITProviderManifest::BuildArtifactSetDigest(DigestEntries);
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
	OutOutput.ProviderGeneration =
		FAngelscriptJITProviderManifest::BuildProviderGeneration(DigestView);

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
