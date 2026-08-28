#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheExactStartup.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Cache/AngelscriptCacheSourceDiscovery.h"
#include "Cache/AngelscriptCacheStore.h"

#include "CQTest.h"
#include "Compilation/AngelscriptCompilationEvents.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Hash/Blake3.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "Shared/AngelscriptTestFixture.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_context.h"
#include "source/as_ast_diagnostics.h"
#include "source/as_ast_public_view.h"
#include "source/as_ast_sidecar.h"
#include "source/as_decl.h"
#include "source/as_module.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheExactWarmStartupTests_Private
{
	static constexpr const TCHAR* RelativeSourcePath = TEXT("ExactWarm.as");
	static constexpr const TCHAR* ModuleName = TEXT("ExactWarm");

	class FScopedDiskRoot final
	{
	public:
		FScopedDiskRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheExactWarmStartup"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheExactWarmStartup/")));
			check(IFileManager::Get().MakeDirectory(*Root, true));
		}

		~FScopedDiskRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheExactWarmStartup/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		FString MakeSourceRoot(const TCHAR* Leaf) const
		{
			const FString Result = Root / Leaf;
			check(IFileManager::Get().MakeDirectory(*Result, true));
			return Result;
		}

		FString Root;
	};

	static FString MakeSource(const int32 Answer)
	{
		return FString::Printf(TEXT(R"AS(
enum EExactWarmState
{
	Ready = 7,
}

int Answer()
{
	return %d;
}
)AS"), Answer);
	}

	static FString MakeEnumTypedSource()
	{
		return TEXT(R"AS(
enum EExactWarmState
{
	Ready = 7,
}

int Answer()
{
	EExactWarmState LocalState = EExactWarmState::Ready;
	return LocalState == EExactWarmState::Ready ? 42 : 0;
}
)AS");
	}

	static bool WriteSource(
		const FString& SourceRoot,
		const FString& Source)
	{
		return FFileHelper::SaveStringToFile(
			Source,
			*(SourceRoot / RelativeSourcePath),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2ExactWarmStartupTest"),
			TEXT("VmExecutionCodec=2"),
		};
		Options.Compatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
				Compatibility);

		FAngelscriptContextDescriptor Context;
		Context.CanonicalInputs = {
			TEXT("SourceMount=Game"),
			TEXT("DebugSidecar=Enabled"),
		};
		Options.Context =
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(Context);
		Options.Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Options.Compatibility, Options.Context);
		Options.CanonicalCompileOptions = {
			TEXT("AutomaticImports=false"),
		};
		return Options;
	}

	static FAngelscriptCacheProductionSourceDiscoveryConfig
	MakeDiscoveryConfig(const FAngelscriptCacheCleanCaptureOptions& Options)
	{
		FAngelscriptCacheProductionSourceDiscoveryConfig Config;
		Config.Profile = Options.Profile;
		Config.DiscoveryPolicyVersion = 1;
		Config.bObserveLegacyGlobalPreprocessHooks = false;
		FAngelscriptCacheDirectOptionInput& CompileOption =
			Config.Options.AddDefaulted_GetRef();
		CompileOption.Kind = EAngelscriptCacheDirectOptionKind::Compiler;
		CompileOption.CanonicalKey = TEXT("AutomaticImports");
		CompileOption.CanonicalValue = TEXT("false");
		return Config;
	}

	static bool Discover(
		FAutomationTestBase& Test,
		const FString& SourceRoot,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		FAngelscriptCacheProductionSourceDiscoveryResult& OutDiscovery)
	{
		FAngelscriptDiskSourceProvider Provider;
		const TArray<FAngelscriptSourceRoot> Roots{
			FAngelscriptSourceRoot::FromGameRoot(SourceRoot)};
		const FAngelscriptCacheSourceDiscoveryStatus Status =
			FAngelscriptCacheSourceDiscovery::DiscoverProductionSources(
				Provider,
				Roots,
				false,
				false,
				MakeDiscoveryConfig(Options),
				{},
				OutDiscovery);
		Test.AddInfo(FString::Printf(
			TEXT("Exact warm discovery: Success=%d Sources=%d Modules=%d Direct=%s Detail=%s"),
			Status.IsSuccess() ? 1 : 0,
			OutDiscovery.LoadedSourceCount,
			OutDiscovery.Modules.Num(),
			*OutDiscovery.DirectPlan.DirectInputDigest.ToHexString(),
			*Status.Detail));
		return Status.IsSuccess();
	}

	static TSharedPtr<FAngelscriptModuleDesc> CompileDiskModule(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& SourceRoot,
		const bool bRetainCanonicalAST = true)
	{
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(
			RelativeSourcePath,
			SourceRoot / RelativeSourcePath);
		if (!Preprocessor.Preprocess())
		{
			Test.AddError(TEXT("Exact warm producer preprocessing failed"));
			return {};
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(
			Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(
			Engine.GetScriptEngine());
		Engine.NextCompileASTRetentionPolicy = static_cast<int32>(
			bRetainCanonicalAST ? asAST_RETAIN_SNAPSHOT : asAST_DISCARD_AFTER_CODEGEN);
		const ECompileResult CompileResult = Engine.CompileModules(
			ECompileType::Initial,
			Preprocessor.GetModulesToCompile(),
			CompiledModules);
		if (CompileResult == ECompileResult::Error
			|| CompileResult == ECompileResult::ErrorNeedFullReload
			|| CompiledModules.Num() != 1)
		{
			Test.AddError(FString::Printf(
				TEXT("Exact warm producer compile failed: Result=%u Modules=%d"),
				static_cast<uint32>(CompileResult),
				CompiledModules.Num()));
			return {};
		}
		return CompiledModules[0];
	}

	struct FPreparedFixture
	{
		FAngelscriptCachePreparedColdGeneration Generation;
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptHash256 ProducerDirectDigest;
		int32 ASTBodySidecarRecordCount = 0;
	};

	using FColdArtifactMutation = bool (*)(FAngelscriptCacheCleanModuleArtifacts&);
	using FAstPayloadMutation = bool (*)(TArray<uint8>&);

	static bool BuildPreparedRecord(
		const EAngelscriptCacheRecordKind Kind,
		TArray<uint8>&& Payload,
		FAngelscriptPreparedRecord& OutRecord)
	{
		OutRecord = {};
		OutRecord.CanonicalPayload = MoveTemp(Payload);
		return FAngelscriptCacheRecordArchive::TryBuildRecordId(
			Kind, OutRecord.CanonicalPayload, OutRecord.RecordId).IsSuccess();
	}

	static bool ReplacePreparedRecord(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const FAngelscriptCacheRecordId& OldRecordId,
		FAngelscriptPreparedRecord&& NewRecord)
	{
		for (FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			if (Record.RecordId == OldRecordId)
			{
				Record = MoveTemp(NewRecord);
				return true;
			}
		}
		return false;
	}

	// Deliberately construct a record graph that is self-consistent at Cache V2
	// graph-admission level, while the linked AST DTO itself has forbidden
	// trailing bytes. ExactStartup must reach its strict AST decode boundary and
	// reject before it activates a target module.
	static bool RewriteAstSidecarAndRepairGraph(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const FAstPayloadMutation PayloadMutation)
	{
		if (PayloadMutation == nullptr)
		{
			return false;
		}
		FAngelscriptPreparedRecord* ASTRecord = Artifacts.Records.FindByPredicate(
			[](const FAngelscriptPreparedRecord& Record)
			{
				return Record.RecordId.Kind
					== EAngelscriptCacheRecordKind::ASTBodySidecar;
			});
		const FAngelscriptPreparedRecord* FunctionRecord =
			Artifacts.Records.FindByPredicate(
				[](const FAngelscriptPreparedRecord& Record)
				{
					return Record.RecordId.Kind
						== EAngelscriptCacheRecordKind::FunctionBody;
				});
		const FAngelscriptPreparedRecord* SnapshotRecord =
			Artifacts.Records.FindByPredicate(
				[](const FAngelscriptPreparedRecord& Record)
				{
					return Record.RecordId.Kind
						== EAngelscriptCacheRecordKind::ModuleSnapshot;
				});
		if (ASTRecord == nullptr || FunctionRecord == nullptr
			|| SnapshotRecord == nullptr)
		{
			return false;
		}

		const FAngelscriptCacheRecordId OldASTRecordId = ASTRecord->RecordId;
		const FAngelscriptCacheRecordId OldFunctionRecordId = FunctionRecord->RecordId;
		const FAngelscriptCacheRecordId OldSnapshotRecordId = SnapshotRecord->RecordId;
		TArray<uint8> MutatedAstPayload = ASTRecord->CanonicalPayload;
		if (!PayloadMutation(MutatedAstPayload))
		{
			return false;
		}
		FAngelscriptPreparedRecord NewASTRecord;
		if (!BuildPreparedRecord(EAngelscriptCacheRecordKind::ASTBodySidecar,
			MoveTemp(MutatedAstPayload), NewASTRecord))
		{
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedFunction;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedSnapshot;
		if (!FAngelscriptDecodedCacheRecord::TryDecode(
				OldFunctionRecordId, FunctionRecord->CanonicalPayload,
				Limits, Budget, DecodedFunction).IsSuccess()
			|| !DecodedFunction.IsSet()
			|| !FAngelscriptDecodedCacheRecord::TryDecode(
				OldSnapshotRecordId, SnapshotRecord->CanonicalPayload,
				Limits, Budget, DecodedSnapshot).IsSuccess()
			|| !DecodedSnapshot.IsSet())
		{
			return false;
		}
		const FAngelscriptCachedFunctionBody* OriginalFunction =
			DecodedFunction.GetValue()->TryGetFunctionBody();
		const FAngelscriptCachedModuleSnapshot* OriginalSnapshot =
			DecodedSnapshot.GetValue()->TryGetModuleSnapshot();
		if (OriginalFunction == nullptr || OriginalSnapshot == nullptr)
		{
			return false;
		}

		FAngelscriptCachedFunctionBody Function = *OriginalFunction;
		Function.ASTBodySidecar = NewASTRecord.RecordId;
		TArray<uint8> FunctionPayload;
		if (!FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody(
				Function, FunctionPayload).IsSuccess())
		{
			return false;
		}
		FAngelscriptPreparedRecord NewFunctionRecord;
		if (!BuildPreparedRecord(EAngelscriptCacheRecordKind::FunctionBody,
			MoveTemp(FunctionPayload), NewFunctionRecord))
		{
			return false;
		}

		FAngelscriptCachedModuleSnapshot Snapshot = *OriginalSnapshot;
		if (Snapshot.FunctionBodies.Num() != 1
			|| Snapshot.FunctionBodies[0].RecordId != OldFunctionRecordId
			|| !(Snapshot.FunctionBodies[0].FunctionKey
				== Function.Identity.FunctionKey))
		{
			return false;
		}
		Snapshot.FunctionBodies[0].RecordId = NewFunctionRecord.RecordId;
		TArray<uint8> SnapshotPayload;
		if (!FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot(
				Snapshot, SnapshotPayload).IsSuccess())
		{
			return false;
		}
		FAngelscriptPreparedRecord NewSnapshotRecord;
		if (!BuildPreparedRecord(EAngelscriptCacheRecordKind::ModuleSnapshot,
			MoveTemp(SnapshotPayload), NewSnapshotRecord))
		{
			return false;
		}

		Artifacts.ModuleSnapshot.RecordId = NewSnapshotRecord.RecordId;
		return ReplacePreparedRecord(Artifacts, OldASTRecordId,
				MoveTemp(NewASTRecord))
			&& ReplacePreparedRecord(Artifacts, OldFunctionRecordId,
				MoveTemp(NewFunctionRecord))
			&& ReplacePreparedRecord(Artifacts, OldSnapshotRecordId,
				MoveTemp(NewSnapshotRecord));
	}

	static bool AppendTrailingByteToAstSidecar(TArray<uint8>& Payload)
	{
		Payload.Add(0xa5u);
		return true;
	}

	static bool TruncateAstSidecar(TArray<uint8>& Payload)
	{
		if (Payload.IsEmpty())
		{
			return false;
		}
		Payload.Pop(EAllowShrinking::No);
		return true;
	}

	static bool MutateAstSidecarProfile(TArray<uint8>& Payload)
	{
		// AST sidecars begin with `CAST`, version, a length-prefixed owner, then
		// a length-prefixed profile. Mutate one profile-hash nibble without
		// breaking the self-describing header, so the restore owner/profile guard
		// rather than generic graph admission owns this rejection.
		if (Payload.Num() < 16)
		{
			return false;
		}
		const auto ReadLittleEndianUInt32 = [&Payload](const int32 Offset)
		{
			return static_cast<uint32>(Payload[Offset])
				| (static_cast<uint32>(Payload[Offset + 1]) << 8u)
				| (static_cast<uint32>(Payload[Offset + 2]) << 16u)
				| (static_cast<uint32>(Payload[Offset + 3]) << 24u);
		};
		const uint32 OwnerLength = ReadLittleEndianUInt32(8);
		const uint64 ProfileLengthOffset = 12ull + OwnerLength;
		if (ProfileLengthOffset + 4ull > static_cast<uint64>(Payload.Num()))
		{
			return false;
		}
		const uint32 ProfileLength = ReadLittleEndianUInt32(
			static_cast<int32>(ProfileLengthOffset));
		const uint64 ProfileOffset = ProfileLengthOffset + 4ull;
		constexpr ANSICHAR Prefix[] = "profile:";
		constexpr uint64 PrefixLength = UE_ARRAY_COUNT(Prefix) - 1u;
		if (ProfileLength <= PrefixLength
			|| ProfileOffset + ProfileLength > static_cast<uint64>(Payload.Num())
			|| FMemory::Memcmp(Payload.GetData() + ProfileOffset,
				Prefix, PrefixLength) != 0)
		{
			return false;
		}
		uint8& FirstHashCharacter = Payload[
			static_cast<int32>(ProfileOffset + PrefixLength)];
		FirstHashCharacter = FirstHashCharacter == static_cast<uint8>('0')
			? static_cast<uint8>('1') : static_cast<uint8>('0');
		return true;
	}

	static bool MutateAstSidecarUnremappableType(TArray<uint8>& Payload)
	{
		// Walk only the encoded type table. The source text, declarations, and
		// record graph remain intact; this simulates an otherwise valid persisted
		// Canonical AST whose stable named type cannot be resolved by the target
		// Engine during exact startup.
		constexpr ANSICHAR Magic[] = "CAST";
		constexpr ANSICHAR OriginalType[] = "EExactWarmState";
		if (Payload.Num() < 12
			|| FMemory::Memcmp(Payload.GetData(), Magic,
				UE_ARRAY_COUNT(Magic) - 1) != 0)
		{
			return false;
		}

		int32 Offset = 8; // magic + version
		const auto ReadUInt32 = [&Payload, &Offset](uint32& OutValue)
		{
			if (Offset < 0 || Offset > Payload.Num() - 4)
			{
				return false;
			}
			OutValue = static_cast<uint32>(Payload[Offset])
				| (static_cast<uint32>(Payload[Offset + 1]) << 8u)
				| (static_cast<uint32>(Payload[Offset + 2]) << 16u)
				| (static_cast<uint32>(Payload[Offset + 3]) << 24u);
			Offset += 4;
			return true;
		};
		const auto SkipBytes = [&Payload, &Offset, &ReadUInt32]()
		{
			uint32 Length = 0;
			if (!ReadUInt32(Length)
				|| static_cast<uint64>(Offset) + Length
					> static_cast<uint64>(Payload.Num()))
			{
				return false;
			}
			Offset += static_cast<int32>(Length);
			return true;
		};
		if (!SkipBytes() || !SkipBytes()) // owner and profile
		{
			return false;
		}

		uint32 SourceCount = 0;
		if (!ReadUInt32(SourceCount))
		{
			return false;
		}
		for (uint32 SourceIndex = 0; SourceIndex < SourceCount; ++SourceIndex)
		{
			uint32 Ignored = 0;
			if (!SkipBytes() || !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !SkipBytes()) // logical key, origin, line offset, source bytes
			{
				return false;
			}
		}

		uint32 TypeCount = 0;
		if (!ReadUInt32(TypeCount))
		{
			return false;
		}
		bool bMutated = false;
		for (uint32 TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
		{
			uint32 Ignored = 0;
			uint32 StableKeyLength = 0;
			if (!ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !ReadUInt32(StableKeyLength)
				|| static_cast<uint64>(Offset) + StableKeyLength
					> static_cast<uint64>(Payload.Num()))
			{
				return false;
			}
			if (StableKeyLength == UE_ARRAY_COUNT(OriginalType) - 1
				&& FMemory::Memcmp(Payload.GetData() + Offset, OriginalType,
					StableKeyLength) == 0)
			{
				if (bMutated)
				{
					return false;
				}
				Payload[Offset] = static_cast<uint8>('X');
				bMutated = true;
			}
			Offset += static_cast<int32>(StableKeyLength);
		}
		return bMutated;
	}

	static bool MutateAstSidecarUnremappableDeclaration(
		TArray<uint8>& Payload,
		const ANSICHAR* const OriginalStableKey,
		const uint32 OriginalStableKeyLength,
		const ANSICHAR* const OriginalName = nullptr,
		const uint32 OriginalNameLength = 0)
	{
		// Preserve the Cache V2 graph, sidecar layout, source text, and VM
		// declaration. Only replace one source-owned declaration stable key so
		// ExactStartup must prove retained declaration identity before publication.
		constexpr ANSICHAR Magic[] = "CAST";
		if (OriginalStableKey == nullptr || OriginalStableKeyLength == 0
			|| (OriginalName != nullptr && OriginalNameLength == 0)
			|| Payload.Num() < 12
			|| FMemory::Memcmp(Payload.GetData(), Magic,
				UE_ARRAY_COUNT(Magic) - 1) != 0)
		{
			return false;
		}

		int32 Offset = 8; // magic + version
		const auto ReadUInt32 = [&Payload, &Offset](uint32& OutValue)
		{
			if (Offset < 0 || Offset > Payload.Num() - 4)
			{
				return false;
			}
			OutValue = static_cast<uint32>(Payload[Offset])
				| (static_cast<uint32>(Payload[Offset + 1]) << 8u)
				| (static_cast<uint32>(Payload[Offset + 2]) << 16u)
				| (static_cast<uint32>(Payload[Offset + 3]) << 24u);
			Offset += 4;
			return true;
		};
		const auto SkipBytes = [&Payload, &Offset, &ReadUInt32]()
		{
			uint32 Length = 0;
			if (!ReadUInt32(Length)
				|| static_cast<uint64>(Offset) + Length
					> static_cast<uint64>(Payload.Num()))
			{
				return false;
			}
			Offset += static_cast<int32>(Length);
			return true;
		};
		const auto SkipUInt32Array = [&ReadUInt32](uint32& OutCount)
		{
			if (!ReadUInt32(OutCount))
			{
				return false;
			}
			for (uint32 Index = 0; Index < OutCount; ++Index)
			{
				uint32 Ignored = 0;
				if (!ReadUInt32(Ignored))
				{
					return false;
				}
			}
			return true;
		};
		const auto SkipStringArray = [&ReadUInt32, &SkipBytes](uint32& OutCount)
		{
			if (!ReadUInt32(OutCount))
			{
				return false;
			}
			for (uint32 Index = 0; Index < OutCount; ++Index)
			{
				if (!SkipBytes())
				{
					return false;
				}
			}
			return true;
		};

		if (!SkipBytes() || !SkipBytes()) // owner and profile
		{
			return false;
		}

		uint32 SourceCount = 0;
		if (!ReadUInt32(SourceCount))
		{
			return false;
		}
		for (uint32 SourceIndex = 0; SourceIndex < SourceCount; ++SourceIndex)
		{
			uint32 Ignored = 0;
			if (!SkipBytes() || !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !SkipBytes())
			{
				return false;
			}
		}

		uint32 TypeCount = 0;
		if (!ReadUInt32(TypeCount))
		{
			return false;
		}
		for (uint32 TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
		{
			uint32 Ignored = 0;
			if (!ReadUInt32(Ignored) || !ReadUInt32(Ignored) || !SkipBytes())
			{
				return false;
			}
		}

		uint32 DeclCount = 0;
		if (!ReadUInt32(DeclCount))
		{
			return false;
		}
		bool bMutatedStableKey = false;
		bool bMutatedName = OriginalName == nullptr;
		for (uint32 DeclIndex = 0; DeclIndex < DeclCount; ++DeclIndex)
		{
			uint32 Ignored = 0;
			uint32 ArrayCount = 0;
			// kind, parent, source range, type, qualifiers
			for (uint32 Field = 0; Field < 8; ++Field)
			{
				if (!ReadUInt32(Ignored))
				{
					return false;
				}
			}
			uint32 NameLength = 0;
			if (!ReadUInt32(NameLength)
				|| static_cast<uint64>(Offset) + NameLength
					> static_cast<uint64>(Payload.Num()))
			{
				return false;
			}
			if (OriginalName != nullptr && NameLength == OriginalNameLength
				&& FMemory::Memcmp(Payload.GetData() + Offset, OriginalName,
					NameLength) == 0)
			{
				if (bMutatedName)
				{
					return false;
				}
				Payload[Offset] = static_cast<uint8>('X');
				bMutatedName = true;
			}
			Offset += static_cast<int32>(NameLength);
			uint32 StableKeyLength = 0;
			if (!ReadUInt32(StableKeyLength)
				|| static_cast<uint64>(Offset) + StableKeyLength
					> static_cast<uint64>(Payload.Num()))
			{
				return false;
			}
			if (StableKeyLength == OriginalStableKeyLength
				&& FMemory::Memcmp(Payload.GetData() + Offset, OriginalStableKey,
					StableKeyLength) == 0)
			{
				if (bMutatedStableKey)
				{
					return false;
				}
				Payload[Offset] = static_cast<uint8>('X');
				bMutatedStableKey = true;
			}
			Offset += static_cast<int32>(StableKeyLength);
			if (!SkipUInt32Array(ArrayCount) || !SkipUInt32Array(ArrayCount)
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !SkipBytes() || !SkipBytes()
				|| !SkipStringArray(ArrayCount) || !SkipUInt32Array(ArrayCount)
				|| !SkipUInt32Array(ArrayCount)
				// V3 declaration layout: byte offset, byte size, byte
				// alignment, constant-present flag, and 64-bit constant bits.
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored))
			{
				return false;
			}
		}
		return bMutatedStableKey && bMutatedName;
	}

	static bool MutateAstSidecarUnremappableFunctionDeclaration(TArray<uint8>& Payload)
	{
		constexpr ANSICHAR OriginalStableKey[] = "Answer()";
		constexpr ANSICHAR OriginalName[] = "Answer";
		return MutateAstSidecarUnremappableDeclaration(
			Payload, OriginalStableKey, UE_ARRAY_COUNT(OriginalStableKey) - 1,
			OriginalName, UE_ARRAY_COUNT(OriginalName) - 1);
	}

	static bool MutateAstSidecarDetachedEnumDeclaration(TArray<uint8>& Payload)
	{
		constexpr ANSICHAR OriginalStableKey[] = "EExactWarmState";
		return MutateAstSidecarUnremappableDeclaration(
			Payload, OriginalStableKey, UE_ARRAY_COUNT(OriginalStableKey) - 1);
	}

	static bool MutateAstSidecarVerifierInvalidStatementOwner(TArray<uint8>& Payload)
	{
		// Keep every sidecar byte structurally readable and every reference in
		// bounds, but give a declaration's actual body statement a different valid
		// declaration owner. Only the post-decode sealed-AST verifier can reject this
		// decl.body <-> stmt.owner disagreement.
		constexpr ANSICHAR Magic[] = "CAST";
		if (Payload.Num() < 12
			|| FMemory::Memcmp(Payload.GetData(), Magic,
				UE_ARRAY_COUNT(Magic) - 1) != 0)
		{
			return false;
		}

		int32 Offset = 8; // magic + version
		const auto ReadUInt32 = [&Payload, &Offset](uint32& OutValue)
		{
			if (Offset < 0 || Offset > Payload.Num() - 4)
			{
				return false;
			}
			OutValue = static_cast<uint32>(Payload[Offset])
				| (static_cast<uint32>(Payload[Offset + 1]) << 8u)
				| (static_cast<uint32>(Payload[Offset + 2]) << 16u)
				| (static_cast<uint32>(Payload[Offset + 3]) << 24u);
			Offset += 4;
			return true;
		};
		const auto SkipBytes = [&Payload, &Offset, &ReadUInt32]()
		{
			uint32 Length = 0;
			if (!ReadUInt32(Length)
				|| static_cast<uint64>(Offset) + Length
					> static_cast<uint64>(Payload.Num()))
			{
				return false;
			}
			Offset += static_cast<int32>(Length);
			return true;
		};
		const auto SkipUInt32Array = [&ReadUInt32](uint32& OutCount)
		{
			if (!ReadUInt32(OutCount))
			{
				return false;
			}
			for (uint32 Index = 0; Index < OutCount; ++Index)
			{
				uint32 Ignored = 0;
				if (!ReadUInt32(Ignored))
				{
					return false;
				}
			}
			return true;
		};
		const auto SkipStringArray = [&ReadUInt32, &SkipBytes](uint32& OutCount)
		{
			if (!ReadUInt32(OutCount))
			{
				return false;
			}
			for (uint32 Index = 0; Index < OutCount; ++Index)
			{
				if (!SkipBytes())
				{
					return false;
				}
			}
			return true;
		};
		if (!SkipBytes() || !SkipBytes()) // owner and profile
		{
			return false;
		}

		uint32 SourceCount = 0;
		if (!ReadUInt32(SourceCount))
		{
			return false;
		}
		for (uint32 SourceIndex = 0; SourceIndex < SourceCount; ++SourceIndex)
		{
			uint32 Ignored = 0;
			if (!SkipBytes() || !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !SkipBytes())
			{
				return false;
			}
		}

		uint32 TypeCount = 0;
		if (!ReadUInt32(TypeCount))
		{
			return false;
		}
		for (uint32 TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
		{
			uint32 Ignored = 0;
			if (!ReadUInt32(Ignored) || !ReadUInt32(Ignored) || !SkipBytes())
			{
				return false;
			}
		}

		uint32 DeclCount = 0;
		if (!ReadUInt32(DeclCount) || DeclCount < 2)
		{
			return false;
		}
		TArray<uint32> DeclBodies;
		DeclBodies.Reserve(static_cast<int32>(DeclCount));
		for (uint32 DeclIndex = 0; DeclIndex < DeclCount; ++DeclIndex)
		{
			uint32 Ignored = 0;
			uint32 ArrayCount = 0;
			uint32 Body = 0;
			// kind, parent, source range, type, qualifiers
			for (uint32 Field = 0; Field < 8; ++Field)
			{
				if (!ReadUInt32(Ignored))
				{
					return false;
				}
			}
			if (!SkipBytes() || !SkipBytes()
				|| !SkipUInt32Array(ArrayCount) || !SkipUInt32Array(ArrayCount)
				// body, traits, accessor kind, accessor field
				|| !ReadUInt32(Body) || !ReadUInt32(Ignored)
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !SkipBytes() || !SkipBytes()
				|| !SkipStringArray(ArrayCount) || !SkipUInt32Array(ArrayCount)
				|| !SkipUInt32Array(ArrayCount)
				// V3 declaration layout: byte offset, byte size, byte
				// alignment, constant-present flag, and 64-bit constant bits.
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored))
			{
				return false;
			}
			DeclBodies.Add(Body);
		}

		uint32 StatementCount = 0;
		if (!ReadUInt32(StatementCount) || StatementCount == 0)
		{
			return false;
		}
		for (uint32 StatementIndex = 1; StatementIndex <= StatementCount;
			++StatementIndex)
		{
			uint32 Ignored = 0;
			uint32 OriginalOwner = 0;
			uint32 ArrayCount = 0;
			if (!ReadUInt32(Ignored)) // kind
			{
				return false;
			}
			const int32 OwnerOffset = Offset;
			if (!ReadUInt32(OriginalOwner)
				// source range, target, expr, decl, safe-point role
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !ReadUInt32(Ignored) || !ReadUInt32(Ignored)
				|| !SkipUInt32Array(ArrayCount))
			{
				return false;
			}
			const int32 DeclBodyIndex = DeclBodies.IndexOfByPredicate(
				[StatementIndex](const uint32 Body)
				{
					return Body == StatementIndex;
				});
			if (DeclBodyIndex == INDEX_NONE
				|| OriginalOwner != static_cast<uint32>(DeclBodyIndex + 1))
			{
				continue;
			}
			const uint32 ReplacementOwner = OriginalOwner == 1 ? 2 : 1;
			Payload[OwnerOffset] = static_cast<uint8>(ReplacementOwner & 0xffu);
			Payload[OwnerOffset + 1] = static_cast<uint8>((ReplacementOwner >> 8u) & 0xffu);
			Payload[OwnerOffset + 2] = static_cast<uint8>((ReplacementOwner >> 16u) & 0xffu);
			Payload[OwnerOffset + 3] = static_cast<uint8>((ReplacementOwner >> 24u) & 0xffu);
			return true;
		}
		return false;
	}

	static bool AppendTrailingByteToAstSidecarAndRepairGraph(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts)
	{
		return RewriteAstSidecarAndRepairGraph(
			Artifacts, &AppendTrailingByteToAstSidecar);
	}

	static bool MutateAstSidecarProfileAndRepairGraph(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts)
	{
		return RewriteAstSidecarAndRepairGraph(
			Artifacts, &MutateAstSidecarProfile);
	}

	static bool TruncateAstSidecarAndRepairGraph(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts)
	{
		return RewriteAstSidecarAndRepairGraph(Artifacts, &TruncateAstSidecar);
	}

	static bool MutateAstSidecarUnremappableTypeAndRepairGraph(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts)
	{
		return RewriteAstSidecarAndRepairGraph(
			Artifacts, &MutateAstSidecarUnremappableType);
	}

	static bool MutateAstSidecarUnremappableDeclarationAndRepairGraph(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts)
	{
		return RewriteAstSidecarAndRepairGraph(
			Artifacts, &MutateAstSidecarUnremappableFunctionDeclaration);
	}

	static bool MutateAstSidecarDetachedEnumDeclarationAndRepairGraph(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts)
	{
		return RewriteAstSidecarAndRepairGraph(
			Artifacts, &MutateAstSidecarDetachedEnumDeclaration);
	}

	static bool MutateAstSidecarVerifierInvalidStatementOwnerAndRepairGraph(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts)
	{
		return RewriteAstSidecarAndRepairGraph(
			Artifacts, &MutateAstSidecarVerifierInvalidStatementOwner);
	}

	static bool PrepareColdFixture(
		FAutomationTestBase& Test,
		const FString& SourceRoot,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		FPreparedFixture& OutFixture,
		const bool bRetainCanonicalAST = true,
		const FColdArtifactMutation Mutation = nullptr)
	{
		FAngelscriptCacheProductionSourceDiscoveryResult Discovery;
		if (!Discover(Test, SourceRoot, Options, Discovery))
		{
			return false;
		}
		OutFixture.ProducerDirectDigest =
			Discovery.DirectPlan.DirectInputDigest;

		FAngelscriptCachedSourceIndex PersistedSourceIndex;
		const FAngelscriptCacheValidationResult CandidateResult =
			FAngelscriptCacheSourcePlanner::BuildPersistedDependencyCandidate(
				Discovery.DirectPlan,
				Options.Profile,
				{},
				{},
				{},
				PersistedSourceIndex);
		if (!CandidateResult.IsSuccess())
		{
			Test.AddError(FString::Printf(
				TEXT("Exact warm SourceIndex candidate failed: Error=%u"),
				static_cast<uint32>(CandidateResult.Error)));
			return false;
		}

		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		{
			FAngelscriptTestFixture Producer(
				Test, ETestEngineMode::IsolatedFull);
			if (!Producer.IsValid())
			{
				return false;
			}
			TSharedPtr<FAngelscriptModuleDesc> Module = CompileDiskModule(
				Test, Producer.GetEngine(), SourceRoot, bRetainCanonicalAST);
			if (!Module.IsValid())
			{
				return false;
			}
			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(),
					Options,
					PersistedSourceIndex,
					Artifacts);
			Test.AddInfo(FString::Printf(
				TEXT("Exact warm producer capture: Error=%u Records=%d Graph=%u Snapshot=%s Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
				*Artifacts.SourceSnapshot.ToHexString(),
				*Capture.Detail));
			if (!Capture.IsSuccess())
			{
				return false;
			}
			for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
			{
				OutFixture.ASTBodySidecarRecordCount +=
					Record.RecordId.Kind == EAngelscriptCacheRecordKind::ASTBodySidecar
						? 1 : 0;
			}
			OutFixture.ModuleKey = Artifacts.ModuleKey;
		}
		if (Mutation != nullptr && !Mutation(Artifacts))
		{
			Test.AddError(TEXT("Exact warm fixture could not apply its requested AST mutation"));
			return false;
		}

		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		const FAngelscriptCacheCleanCaptureResult Prepare =
			PrepareAngelscriptCacheColdGeneration(
				Artifacts,
				Options,
				PackPolicy,
				Codec,
				OutFixture.Generation);
		Test.AddInfo(FString::Printf(
			TEXT("Exact warm cold generation: Error=%u Generation=%s Packs=%d Detail=%s"),
			static_cast<uint32>(Prepare.Error),
			*OutFixture.Generation.EncodedManifest.ComputedGenerationId.ToHexString(),
			OutFixture.Generation.Packs.Num(),
			*Prepare.Detail));
		return Prepare.IsSuccess();
	}

	static bool PublishAndOpen(
		FAutomationTestBase& Test,
		const FString& DiskRoot,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FPreparedFixture& Fixture,
		FAngelscriptCacheStorePaths& OutPaths,
		TUniquePtr<FAngelscriptCacheReadSession>& OutSession)
	{
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
			CreateAngelscriptCacheNamespaceLockOps();
		if (!FileOps.IsValid() || !LockOps.IsValid()
			|| !BuildAngelscriptCacheStorePaths(
				DiskRoot / TEXT("CacheV2"),
				Options.Compatibility,
				Options.Context,
				*FileOps,
				OutPaths).IsSuccess()
			|| !EnsureAngelscriptCacheStoreDirectories(
				OutPaths, *FileOps).IsSuccess())
		{
			return false;
		}

		const TOptional<FAngelscriptCacheWriterToken> WriterToken =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("8501-aabbccddeeff00112233445566778899"));
		if (!WriterToken.IsSet())
		{
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget PublicationBudget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		const FAngelscriptCacheStoreResult Publication =
			PublishAngelscriptCacheGeneration(
				OutPaths,
				EAngelscriptCachePublicationDisposition::Current,
				{},
				Fixture.Generation.Packs,
				Fixture.Generation.Manifest,
				Fixture.Generation.EncodedManifest,
				WriterToken.GetValue(),
				Limits,
				PublicationBudget,
				FPlatformTime::Seconds() + 2.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps);
		if (Publication.Error != EAngelscriptCacheStoreError::None
			|| Publication.CommitState
				!= EAngelscriptCacheStoreCommitState::CurrentCommitted)
		{
			return false;
		}

		FAngelscriptCacheReadSelection Selection;
		Selection.Compatibility = Options.Compatibility;
		Selection.Context = Options.Context;
		Selection.Profile = Options.Profile;
		Selection.SourceSnapshot = Fixture.Generation.Manifest.SourceSnapshot;
		const FAngelscriptCacheStoreResult Open =
			OpenBestAngelscriptCacheReadSession(
				OutPaths,
				Selection,
				Limits,
				FPlatformTime::Seconds() + 2.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps,
				OutSession);
		Test.AddInfo(FString::Printf(
			TEXT("Exact warm Store reopen: OpenError=%u Generation=%s PinnedPacks=%d"),
			static_cast<uint32>(Open.Error),
			*Fixture.Generation.EncodedManifest.ComputedGenerationId.ToHexString(),
			OutSession.IsValid() ? OutSession->GetPinnedPackCount() : 0));
		return Open.Error == EAngelscriptCacheStoreError::None
			&& OutSession.IsValid();
	}

	static FAngelscriptHash256 SnapshotPhysicalStore(
		const FString& NamespaceRoot,
		int32& OutFileCount)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(
			Files, *NamespaceRoot, TEXT("*"), true, false);
		Files.Sort();
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-v2-exact-warm-physical-store-v1"));
		Writer.WriteUInt32(static_cast<uint32>(Files.Num()));
		for (const FString& File : Files)
		{
			TArray<uint8> Bytes;
			check(FFileHelper::LoadFileToArray(Bytes, *File));
			FString Relative = File;
			FPaths::MakePathRelativeTo(Relative, *NamespaceRoot);
			Writer.WriteString(Relative);
			Writer.WriteUInt64(static_cast<uint64>(Bytes.Num()));
			Writer.WriteHash(FAngelscriptHash256{FBlake3::HashBuffer(
				Bytes.GetData(), static_cast<uint64>(Bytes.Num()))});
		}
		OutFileCount = Files.Num();
		return Writer.FinalizeHash();
	}

	static int32 CountFrontendEvents(
		const TArray<FAngelscriptCompilationEvent>& Events)
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

	static const FAngelscriptCachedFunctionBody* FindOnlyFunctionBody(
		const FAngelscriptValidatedGeneration& Generation)
	{
		const FAngelscriptCachedFunctionBody* Result = nullptr;
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: Generation.ReachableRecords)
		{
			if (const FAngelscriptCachedFunctionBody* Candidate =
				Record->TryGetFunctionBody())
			{
				if (Result != nullptr)
				{
					return nullptr;
				}
				Result = Candidate;
			}
		}
		return Result;
	}

	static const FAngelscriptDecodedCacheRecord* FindOnlyCanonicalAstSidecar(
		const FAngelscriptValidatedGeneration& Generation)
	{
		const FAngelscriptCachedFunctionBody* const Body =
			FindOnlyFunctionBody(Generation);
		if (Body == nullptr || !Body->ASTBodySidecar.IsSet())
		{
			return nullptr;
		}
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: Generation.ReachableRecords)
		{
			if (Record->GetRecordId() == Body->ASTBodySidecar.GetValue()
				&& Record->TryGetASTBodySidecar() != nullptr)
			{
				return &Record.Get();
			}
		}
		return nullptr;
	}

	static void AssertZeroWork(
		FAutomationTestBase& Test,
		const FAngelscriptCacheExactStartupResult& Result,
		const TArray<FAngelscriptCompilationEvent>& Events)
	{
		Test.TestEqual(TEXT("warm preprocess calls"),
			Result.Counters.PreprocessCalls, uint32(0));
		Test.TestEqual(TEXT("warm parse calls"),
			Result.Counters.ParseCalls, uint32(0));
		Test.TestEqual(TEXT("warm module compiler calls"),
			Result.Counters.ModuleCompilerCalls, uint32(0));
		Test.TestEqual(TEXT("warm function compiler calls"),
			Result.Counters.FunctionCompilerCalls, uint32(0));
		Test.TestEqual(TEXT("warm publication attempts"),
			Result.Counters.PublicationAttempts, uint32(0));
		Test.TestEqual(TEXT("observed warm frontend events"),
			CountFrontendEvents(Events), 0);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheExactWarmStartupTests,
	"Angelscript.TestModule.Cache.ExactWarmStartup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(RetainedCanonicalAstIsCapturedAsOneReachableSidecarRecord)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceA"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared)));
		ASSERT_THAT(AreEqual(1, Prepared.ASTBodySidecarRecordCount,
			TEXT("retained complete Canonical AST must be captured once as a Cache V2 sidecar record")));
	}

	TEST_METHOD(RetainedCanonicalAstPolicyRestoresPublishedSnapshotWithoutFrontendWork)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceA"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared)));
		ASSERT_THAT(AreEqual(1, Prepared.ASTBodySidecarRecordCount));

		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session)));
		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		Consumer.GetEngine().NextCompileASTRetentionPolicy = static_cast<int32>(
			asAST_RETAIN_SNAPSHOT);
		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(
			*TestRunner, SourceRoot, Options, Current)));

		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(),
				Options.Profile, Current, {}, {});
		ASSERT_THAT(IsTrue(Warm.IsRestored(), *Warm.Detail));
		TSharedPtr<FAngelscriptModuleDesc> Restored =
			Consumer.GetEngine().GetModuleByModuleName(ModuleName);
		ASSERT_THAT(IsTrue(Restored.IsValid()));
		asIASTSnapshot* const Snapshot = Restored->ScriptModule->AcquireASTSnapshot(
			asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Snapshot,
			TEXT("retained exact startup must publish the verified Cache V2 Canonical AST")));
		if (Snapshot != nullptr)
		{
			const asCASTSnapshot* const ConcreteSnapshot =
				static_cast<const asCASTSnapshot*>(Snapshot);
			ASSERT_THAT(AreEqual(FString(TEXT("cache-v2-restore")),
				FString(UTF8_TO_TCHAR(ConcreteSnapshot->GetProvenance())),
				TEXT("a restored snapshot must retain cache provenance outside semantic identity")));
			asSASTDiagnosticRequest DiagnosticRequest;
			DiagnosticRequest.operation = asAST_DIAGNOSTIC_VERIFY;
			DiagnosticRequest.moduleName = "ExactWarm";
			asCString DiagnosticOutput;
			ASSERT_THAT(AreEqual(asSUCCESS, asCASTRunDiagnostics(
				Consumer.GetEngine().GetScriptEngine(), DiagnosticRequest, DiagnosticOutput)));
			ASSERT_THAT(IsTrue(FString(UTF8_TO_TCHAR(DiagnosticOutput.AddressOf())).Contains(
				TEXT("provenance=cache-v2-restore")),
				TEXT("host-neutral diagnostics must identify Cache V2 restored provenance")));

			asSASTDeclView TranslationUnit = {};
			TranslationUnit.structSize = sizeof(TranslationUnit);
			ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(
				Snapshot->GetTranslationUnitDecl(), &TranslationUnit),
				TEXT("restored snapshot must expose its translation unit through AST V1")));
			ASSERT_THAT(IsTrue(TranslationUnit.childCount > 0,
				TEXT("the restored AST must retain declarations, not merely an empty translation unit")));

			bool bFoundAnswer = false;
			for (asUINT ChildIndex = 0; ChildIndex < TranslationUnit.childCount; ++ChildIndex)
			{
				asASTDeclId ChildId;
				if (Snapshot->GetDeclChild(TranslationUnit.id, ChildIndex, &ChildId) != 0)
				{
					continue;
				}
				asSASTDeclView Child = {};
				Child.structSize = sizeof(Child);
				if (Snapshot->GetDecl(ChildId, &Child) != 0
					|| FString(UTF8_TO_TCHAR(Child.name != nullptr ? Child.name : "")) != TEXT("Answer"))
				{
					continue;
				}

				bFoundAnswer = true;
				ASSERT_THAT(AreEqual(static_cast<uint8>(asAST_DECL_FUNCTION), Child.kind,
					TEXT("the restored Answer declaration must retain its function kind")));
				ASSERT_THAT(IsTrue(Child.body.IsValid(),
					TEXT("the restored function declaration must retain its body edge")));
				asSASTStmtView Body = {};
				Body.structSize = sizeof(Body);
				ASSERT_THAT(AreEqual(0, Snapshot->GetStmt(Child.body, &Body),
					TEXT("the restored Answer body must be traversable")));
				ASSERT_THAT(IsTrue(Body.childCount > 0,
					TEXT("the restored Answer body must retain its return statement")));
				asASTStmtId ReturnId;
				ASSERT_THAT(AreEqual(0, Snapshot->GetStmtChild(Body.id, 0, &ReturnId),
					TEXT("the restored Answer body must expose its first statement")));
				asSASTStmtView Return = {};
				Return.structSize = sizeof(Return);
				ASSERT_THAT(AreEqual(0, Snapshot->GetStmt(ReturnId, &Return),
					TEXT("the restored return statement must be traversable")));
				ASSERT_THAT(AreEqual(static_cast<uint8>(asAST_STMT_RETURN), Return.kind,
					TEXT("the restored Answer body must retain return semantics")));
				ASSERT_THAT(IsTrue(Return.expr.IsValid(),
					TEXT("the restored return statement must retain its expression edge")));
				asSASTExprView ReturnExpr = {};
				ReturnExpr.structSize = sizeof(ReturnExpr);
				ASSERT_THAT(AreEqual(0, Snapshot->GetExpr(Return.expr, &ReturnExpr),
					TEXT("the restored return expression must be traversable")));
				ASSERT_THAT(AreEqual(static_cast<uint8>(asAST_EXPR_INTEGER_LITERAL), ReturnExpr.kind,
					TEXT("the restored return expression must retain its literal node")));
				ASSERT_THAT(AreEqual(FString(TEXT("42")), FString(UTF8_TO_TCHAR(
					ReturnExpr.literal != nullptr ? ReturnExpr.literal : "")),
					TEXT("the restored return expression must retain its literal payload")));

				const asCASTContext* const RestoredContext =
					ConcreteSnapshot->GetContext();
				const asCDecl* const InternalAnswer = RestoredContext != nullptr
					? RestoredContext->GetDecl(ChildId) : nullptr;
				asCScriptFunction* const RuntimeAnswer =
					static_cast<asCScriptFunction*>(
						Restored->ScriptModule->GetFunctionByDecl("int Answer()"));
				ASSERT_THAT(IsNotNull(InternalAnswer,
					TEXT("Cache V2 Answer declaration remains available in the sealed internal AST")));
				ASSERT_THAT(IsNotNull(RuntimeAnswer,
					TEXT("Cache V2 Answer runtime skeleton remains available after restore")));
				if (InternalAnswer != nullptr && RuntimeAnswer != nullptr)
				{
					ASSERT_THAT(AreEqual(
						FString(UTF8_TO_TCHAR(InternalAnswer->stableKey.AddressOf())),
						FString(UTF8_TO_TCHAR(
							RuntimeAnswer->canonicalASTStableDeclKey.AddressOf())),
						TEXT("verified Cache V2 restore must bind the live function to the exact sealed declaration identity")));
				}
				break;
			}
			ASSERT_THAT(IsTrue(bFoundAnswer,
				TEXT("Cache V2 restore must retain Answer(), not only a synthetic translation unit")));
			Snapshot->Release();
		}
	}

	TEST_METHOD(RetainedCanonicalAstReencodesByteExactSidecarWithoutFrontendWork)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceByteExact"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared)));

		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session)));
		const FAngelscriptDecodedCacheRecord* const PersistedSidecar =
			FindOnlyCanonicalAstSidecar(Session->GetGeneration());
		ASSERT_THAT(IsNotNull(PersistedSidecar,
			TEXT("a retain-policy generation must have exactly one linked AST sidecar record")));
		if (PersistedSidecar == nullptr)
		{
			return;
		}
		const TConstArrayView<uint8> PersistedBytes =
			PersistedSidecar->GetCanonicalPayload();
		ASSERT_THAT(IsTrue(!PersistedBytes.IsEmpty(),
			TEXT("the persisted AST DTO must contain real canonical bytes")));

		FAngelscriptTestFixture Consumer(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		Consumer.GetEngine().NextCompileASTRetentionPolicy = static_cast<int32>(
			asAST_RETAIN_SNAPSHOT);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};

		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(), Options.Profile, Current, {}, {});
		ASSERT_THAT(IsTrue(Warm.IsRestored(), *Warm.Detail));
		AssertZeroWork(*TestRunner, Warm, Events);

		TSharedPtr<FAngelscriptModuleDesc> Restored =
			Consumer.GetEngine().GetModuleByModuleName(ModuleName);
		ASSERT_THAT(IsTrue(Restored.IsValid()));
		asIASTSnapshot* const Snapshot = Restored.IsValid()
			? Restored->ScriptModule->AcquireASTSnapshot(asAST_API_VERSION_1) : nullptr;
		ASSERT_THAT(IsNotNull(Snapshot,
			TEXT("exact restore must publish the same verified canonical graph it read from Cache V2")));
		if (Snapshot == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Snapshot->Release();
		};

		const asCASTSnapshot* const ConcreteSnapshot =
			static_cast<const asCASTSnapshot*>(Snapshot);
		const asCASTContext* const RestoredContext =
			ConcreteSnapshot != nullptr ? ConcreteSnapshot->GetContext() : nullptr;
		ASSERT_THAT(IsNotNull(RestoredContext,
			TEXT("the public retained snapshot must keep its sealed Context lease")));
		if (RestoredContext == nullptr)
		{
			return;
		}
		const FString Owner = FString::Printf(TEXT("module:%s"),
			*Prepared.ModuleKey.Hash.ToHexString());
		const FString Profile = FString::Printf(TEXT("profile:%s"),
			*Session->GetGeneration().Manifest.Profile.Hash.ToHexString());
		const auto OwnerUtf8 = StringCast<UTF8CHAR>(*Owner);
		const auto ProfileUtf8 = StringCast<UTF8CHAR>(*Profile);
		asSASTSidecarHeader Header;
		Header.functionKey = reinterpret_cast<const char*>(OwnerUtf8.Get());
		Header.profile = reinterpret_cast<const char*>(ProfileUtf8.Get());
		Header.version = FAngelscriptCacheASTBodySidecar::SchemaVersion;
		asCArray<asBYTE> Reencoded;
		ASSERT_THAT(AreEqual(static_cast<int>(asAST_SIDECAR_OK),
			asCASTEncodeSidecar(*RestoredContext, Header, Reencoded),
			TEXT("a restored sealed AST must re-encode as a valid Cache V2 AST DTO")));
		ASSERT_THAT(AreEqual(PersistedBytes.Num(),
			static_cast<int32>(Reencoded.GetLength()),
			TEXT("restored AST DTO byte count must exactly match the persisted sidecar")));
		ASSERT_THAT(IsTrue(PersistedBytes.Num() == static_cast<int32>(Reencoded.GetLength())
			&& FMemory::Memcmp(PersistedBytes.GetData(), Reencoded.AddressOf(),
				PersistedBytes.Num()) == 0,
			TEXT("ExactStartup must restore the full pointer-free AST DTO byte-exactly, not synthesize an equivalent-looking partial graph")));
	}

	TEST_METHOD(RetainedPolicyRejectsMissingAstSidecarBeforeEngineMutation)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceWithoutAst"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared, false),
			TEXT("a discard-policy producer must still create a valid VM Cache V2 generation")));
		ASSERT_THAT(AreEqual(0, Prepared.ASTBodySidecarRecordCount,
			TEXT("discard-policy capture must not fabricate a Canonical AST sidecar")));

		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session)));

		FAngelscriptTestFixture Consumer(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		Consumer.GetEngine().NextCompileASTRetentionPolicy = static_cast<int32>(
			asAST_RETAIN_SNAPSHOT);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener = FAngelscriptCompilationEvents::RegisterListener(
			[&Events](const FAngelscriptCompilationEvent& Event)
			{
				Events.Add(Event);
			});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};

		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(), Options.Profile, Current, {}, {});

		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupDisposition::Rejected,
			Warm.Disposition,
			TEXT("retain-policy exact startup must reject a generation without the required AST sidecar")));
		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupReason::RestoreRejected,
			Warm.Reason,
			TEXT("missing retained AST is a restore rejection, never a silent legacy/frontend fallback")));
		ASSERT_THAT(IsTrue(Warm.Detail.Contains(TEXT("no Canonical AST sidecar")),
			*Warm.Detail));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(ModuleName).IsValid(),
			TEXT("rejected retained restore must not activate a partial module")));
		AssertZeroWork(*TestRunner, Warm, Events);
	}

	TEST_METHOD(RetainedPolicyRejectsTrailingAstSidecarBeforeEngineMutation)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceWithTrailingAst"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared, true,
			&AppendTrailingByteToAstSidecarAndRepairGraph),
			TEXT("the fixture must retain a graph-linked AST sidecar with a deliberately trailing DTO byte")));
		ASSERT_THAT(AreEqual(1, Prepared.ASTBodySidecarRecordCount));

		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session),
			TEXT("Cache V2 admission must accept the self-consistent record graph so ExactStartup owns the strict AST decode")));

		FAngelscriptTestFixture Consumer(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		Consumer.GetEngine().NextCompileASTRetentionPolicy = static_cast<int32>(
			asAST_RETAIN_SNAPSHOT);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};

		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(), Options.Profile, Current, {}, {});

		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupDisposition::Rejected,
			Warm.Disposition,
			TEXT("retain-policy exact startup must reject a trailing Canonical AST DTO")));
		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupReason::RestoreRejected,
			Warm.Reason,
			TEXT("trailing AST bytes are a restore rejection, never a silent frontend fallback")));
		ASSERT_THAT(IsTrue(Warm.Detail.Contains(TEXT("strictly decoded")),
			*Warm.Detail));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(ModuleName).IsValid(),
			TEXT("a strict AST decode rejection must not activate a partial module")));
		AssertZeroWork(*TestRunner, Warm, Events);
	}

	TEST_METHOD(RetainedPolicyRejectsTruncatedAstSidecarBeforeEngineMutation)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceWithTruncatedAst"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared, true,
			&TruncateAstSidecarAndRepairGraph),
			TEXT("the fixture must retain a graph-linked AST sidecar whose DTO body is truncated")));

		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session),
			TEXT("Cache V2 graph admission must accept the complete header so ExactStartup owns strict AST body validation")));

		FAngelscriptTestFixture Consumer(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		Consumer.GetEngine().NextCompileASTRetentionPolicy = static_cast<int32>(
			asAST_RETAIN_SNAPSHOT);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};

		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(), Options.Profile, Current, {}, {});

		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupDisposition::Rejected,
			Warm.Disposition,
			TEXT("retain-policy exact startup must reject a truncated Canonical AST DTO")));
		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupReason::RestoreRejected,
			Warm.Reason,
			TEXT("truncated AST bytes are a restore rejection, never a silent frontend fallback")));
		ASSERT_THAT(IsTrue(Warm.Detail.Contains(TEXT("strictly decoded")),
			*Warm.Detail));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(ModuleName).IsValid(),
			TEXT("a truncated AST decode rejection must not activate a partial module")));
		AssertZeroWork(*TestRunner, Warm, Events);
	}

	TEST_METHOD(RetainedPolicyRejectsAstSidecarProfileMismatchBeforeEngineMutation)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceWithWrongAstProfile"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared, true,
			&MutateAstSidecarProfileAndRepairGraph),
			TEXT("the fixture must retain a graph-linked AST sidecar whose header profile differs from its generation")));

		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session),
			TEXT("Cache V2 graph admission must preserve the sidecar header for ExactStartup owner/profile validation")));

		FAngelscriptTestFixture Consumer(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		Consumer.GetEngine().NextCompileASTRetentionPolicy = static_cast<int32>(
			asAST_RETAIN_SNAPSHOT);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};

		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(), Options.Profile, Current, {}, {});

		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupDisposition::Rejected,
			Warm.Disposition,
			TEXT("retain-policy exact startup must reject an AST sidecar with a mismatched profile header")));
		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupReason::RestoreRejected,
			Warm.Reason,
			TEXT("AST owner/profile mismatch is a restore rejection, never a frontend fallback")));
		ASSERT_THAT(IsTrue(Warm.Detail.Contains(TEXT("owner or profile")),
			*Warm.Detail));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(ModuleName).IsValid(),
			TEXT("an AST owner/profile mismatch must not activate a partial module")));
		AssertZeroWork(*TestRunner, Warm, Events);
	}

	TEST_METHOD(RetainedPolicyRejectsUnremappableAstTypeBeforeEngineActivation)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceWithUnremappableAstType"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeEnumTypedSource())));
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared, true,
			&MutateAstSidecarUnremappableTypeAndRepairGraph),
			TEXT("the fixture must keep a graph-valid AST sidecar whose named type has no target-Engine stable resolution")));

		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session),
			TEXT("Cache V2 graph admission must preserve the structurally valid but target-unresolvable AST DTO")));

		FAngelscriptTestFixture Consumer(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		Consumer.GetEngine().NextCompileASTRetentionPolicy = static_cast<int32>(
			asAST_RETAIN_SNAPSHOT);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};

		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(), Options.Profile, Current, {}, {});

		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupDisposition::Rejected,
			Warm.Disposition,
			TEXT("retain-policy exact startup must reject a Canonical AST named type that the target Engine cannot resolve")));
		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupReason::RestoreRejected,
			Warm.Reason,
			TEXT("an unremappable Canonical AST type is a restore rejection, never a frontend fallback")));
		ASSERT_THAT(IsTrue(Warm.Detail.Contains(TEXT("could not resolve Canonical AST type")),
			*Warm.Detail));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(ModuleName).IsValid(),
			TEXT("an unremappable Canonical AST type must not activate a partial module")));
		AssertZeroWork(*TestRunner, Warm, Events);
	}

	TEST_METHOD(RetainedPolicyRejectsUnremappableAstDeclarationBeforeEngineActivation)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(
			TEXT("SourceWithUnremappableAstDeclaration"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared, true,
			&MutateAstSidecarUnremappableDeclarationAndRepairGraph),
			TEXT("the fixture must preserve Cache V2 graph and VM declaration validity while making the retained Canonical AST function name/key self-consistent but unresolvable against the target skeleton")));

		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session),
			TEXT("Cache V2 graph admission must preserve the structurally valid AST declaration-key mismatch so exact startup owns the rejection")));

		FAngelscriptTestFixture Consumer(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		Consumer.GetEngine().NextCompileASTRetentionPolicy = static_cast<int32>(
			asAST_RETAIN_SNAPSHOT);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};

		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(), Options.Profile, Current, {}, {});

		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupDisposition::Rejected,
			Warm.Disposition,
			TEXT("retain-policy exact startup must reject a Canonical AST declaration key that cannot bind to the target Engine's restored function skeleton")));
		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupReason::RestoreRejected,
			Warm.Reason,
			TEXT("an unremappable Canonical AST declaration is a restore rejection, never a frontend fallback")));
		ASSERT_THAT(IsTrue(Warm.Detail.Contains(
			TEXT("could not resolve Canonical AST declaration")), *Warm.Detail));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(ModuleName).IsValid(),
			TEXT("an unremappable Canonical AST declaration must not activate a partial module")));
		AssertZeroWork(*TestRunner, Warm, Events);
	}

	TEST_METHOD(RetainedPolicyRejectsDetachedEnumDeclarationIdentityBeforeEngineActivation)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(
			TEXT("SourceWithDetachedEnumAstDeclaration"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared, true,
			&MutateAstSidecarDetachedEnumDeclarationAndRepairGraph),
			TEXT("the fixture must preserve the Cache V2 graph and the target Engine enum type while detaching the sidecar enum declaration identity")));

		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session),
			TEXT("Cache V2 graph admission must preserve the structurally valid enum declaration identity mismatch so ExactStartup owns its semantic rejection")));

		FAngelscriptTestFixture Consumer(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		Consumer.GetEngine().NextCompileASTRetentionPolicy = static_cast<int32>(
			asAST_RETAIN_SNAPSHOT);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};

		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(), Options.Profile, Current, {}, {});

		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupDisposition::Rejected,
			Warm.Disposition,
			TEXT("retain-policy ExactStartup must reject a detached non-body enum declaration identity even though no local script-function skeleton owns it")));
		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupReason::RestoreRejected,
			Warm.Reason,
			TEXT("a detached enum declaration identity is a restore rejection, never a frontend fallback")));
		ASSERT_THAT(IsTrue(Warm.Detail.Contains(
			TEXT("could not resolve Canonical AST declaration")), *Warm.Detail));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(ModuleName).IsValid(),
			TEXT("a detached enum declaration identity must not activate a partial module")));
		AssertZeroWork(*TestRunner, Warm, Events);
	}

	TEST_METHOD(RetainedPolicyRejectsVerifierInvalidAstGraphBeforeEngineActivation)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceWithVerifierInvalidAstGraph"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared, true,
			&MutateAstSidecarVerifierInvalidStatementOwnerAndRepairGraph),
			TEXT("the fixture must preserve a graph-valid Cache V2 record set while making only the sealed AST decl.body/stmt.owner relation invalid")));

		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session),
			TEXT("Cache V2 graph admission must preserve the structurally readable sidecar so exact startup reaches AST Seal verification")));

		FAngelscriptTestFixture Consumer(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		Consumer.GetEngine().NextCompileASTRetentionPolicy = static_cast<int32>(
			asAST_RETAIN_SNAPSHOT);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};

		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(), Options.Profile, Current, {}, {});

		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupDisposition::Rejected,
			Warm.Disposition,
			TEXT("retain-policy exact startup must reject a Cache V2 sidecar that only sealed-AST verification can prove inconsistent")));
		ASSERT_THAT(AreEqual(EAngelscriptCacheExactStartupReason::RestoreRejected,
			Warm.Reason,
			TEXT("a verifier-invalid Canonical AST graph is a restore rejection, never a frontend fallback")));
		ASSERT_THAT(IsTrue(Warm.Detail.Contains(TEXT("strictly decoded")),
			*Warm.Detail));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(ModuleName).IsValid(),
			TEXT("a verifier-invalid Canonical AST graph must not activate a partial module")));
		AssertZeroWork(*TestRunner, Warm, Events);
	}

	TEST_METHOD(UnchangedSecondLaunchRestoresWithZeroFrontendWorkAndNoPublication)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceA"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared)));

		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session)));
		int32 BeforeFileCount = 0;
		const FAngelscriptHash256 BeforeStore = SnapshotPhysicalStore(
			Paths.NamespaceRoot, BeforeFileCount);

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};

		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(
			*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(),
				Session->GetGeneration(),
				Options.Profile,
				Current,
				{},
				{});
		TestRunner->AddInfo(FString::Printf(
			TEXT("Exact warm restore: Disposition=%u Reason=%u Modules=%u Types=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Warm.Disposition),
			static_cast<uint32>(Warm.Reason),
			Warm.RestoredModuleCount,
			Warm.RestoredTypeCount,
			Warm.RestoredFunctionCount,
			*Warm.Detail));
		ASSERT_THAT(IsTrue(Warm.IsRestored(), *Warm.Detail));
		AssertZeroWork(*TestRunner, Warm, Events);

		TSharedPtr<FAngelscriptModuleDesc> Restored =
			Consumer.GetEngine().GetModuleByModuleName(ModuleName);
		ASSERT_THAT(IsTrue(Restored.IsValid()));
		asIASTSnapshot* const DiscardedSnapshot =
			Restored->ScriptModule->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNull(DiscardedSnapshot,
			TEXT("the default discard policy must not publish a cached Canonical AST snapshot")));
		if (DiscardedSnapshot != nullptr)
		{
			DiscardedSnapshot->Release();
		}
		ASSERT_THAT(AreEqual(1, Restored->Code.Num()));
		ASSERT_THAT(AreEqual(
			FPaths::ConvertRelativePathToFull(SourceRoot / RelativeSourcePath),
			FPaths::ConvertRelativePathToFull(Restored->Code[0].AbsoluteFilename)));
		ASSERT_THAT(IsTrue(Restored->Code[0].Code.IsEmpty(),
			TEXT("raw source must not masquerade as preprocessed module code")));

		asIScriptFunction* Answer =
			Restored->ScriptModule->GetFunctionByDecl("int Answer()");
		ASSERT_THAT(IsNotNull(Answer));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(Consumer.ExecuteInt(*Answer, Value)));
		ASSERT_THAT(AreEqual(42, Value));
		const FAngelscriptCachedFunctionBody* FunctionBody =
			FindOnlyFunctionBody(Session->GetGeneration());
		ASSERT_THAT(IsNotNull(FunctionBody));
		FAngelscriptFunctionRoute Route;
		ASSERT_THAT(IsTrue(Consumer.GetEngine().ResolveFunctionRoute(
			FunctionBody->Identity.FunctionKey, Route)));
		ASSERT_THAT(AreEqual(Answer->GetId(), Route.NumericFunctionId));

		int32 AfterFileCount = 0;
		const FAngelscriptHash256 AfterStore = SnapshotPhysicalStore(
			Paths.NamespaceRoot, AfterFileCount);
		ASSERT_THAT(AreEqual(BeforeFileCount, AfterFileCount));
		ASSERT_THAT(AreEqual(
			BeforeStore.ToHexString(), AfterStore.ToHexString()));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Exact warm accepted: Generation=%s SourceSnapshot=%s FunctionId=%d Result=%d StoreFiles=%d StoreDigest=%s"),
			*Session->GetGenerationId().ToHexString(),
			*Session->GetGeneration().Manifest.SourceSnapshot.ToHexString(),
			Route.NumericFunctionId,
			Value,
			AfterFileCount,
			*AfterStore.ToHexString()));
	}

	TEST_METHOD(ChangedRawSourceIsSafeMissBeforeEngineMutation)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceA"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared)));
		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session)));
		int32 BeforeFileCount = 0;
		const FAngelscriptHash256 BeforeStore = SnapshotPhysicalStore(
			Paths.NamespaceRoot, BeforeFileCount);
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(99))));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};
		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(
			*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(),
				Options.Profile, Current, {}, {});
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheExactStartupDisposition::Miss,
			Warm.Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheExactStartupReason::DirectInputMismatch,
			Warm.Reason));
		AssertZeroWork(*TestRunner, Warm, Events);
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(
			ModuleName).IsValid()));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetEnum(
			TEXT("EExactWarmState")).IsValid()));
		int32 AfterFileCount = 0;
		const FAngelscriptHash256 AfterStore = SnapshotPhysicalStore(
			Paths.NamespaceRoot, AfterFileCount);
		ASSERT_THAT(AreEqual(BeforeFileCount, AfterFileCount));
		ASSERT_THAT(AreEqual(
			BeforeStore.ToHexString(), AfterStore.ToHexString()));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Exact warm changed-source miss: Reason=%u DirectBefore=%s DirectNow=%s Detail=%s"),
			static_cast<uint32>(Warm.Reason),
			*Prepared.ProducerDirectDigest.ToHexString(),
			*Current.DirectPlan.DirectInputDigest.ToHexString(),
			*Warm.Detail));
	}

	TEST_METHOD(MismatchedTransientProjectionIsSafeMissBeforeActivation)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceA"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared)));
		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session)));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(
			*TestRunner, SourceRoot, Options, Current)));
		ASSERT_THAT(AreEqual(1, Current.CurrentSources.Num()));
		ASSERT_THAT(IsTrue(!Current.CurrentSources[0].RawSourceBytes.IsEmpty()));
		Current.CurrentSources[0].RawSourceBytes[0] ^= 0x1;

		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(),
				Options.Profile, Current, {}, {});
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheExactStartupDisposition::Miss,
			Warm.Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheExactStartupReason::CurrentSourceProjectionMismatch,
			Warm.Reason));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(
			ModuleName).IsValid()));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetEnum(
			TEXT("EExactWarmState")).IsValid()));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Exact warm transient projection miss: Reason=%u Detail=%s"),
			static_cast<uint32>(Warm.Reason), *Warm.Detail));
	}

	TEST_METHOD(RelocatedAbsoluteRootStillRestoresFromSameGeneration)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRootA = Disk.MakeSourceRoot(TEXT("SourceA"));
		const FString SourceRootB = Disk.MakeSourceRoot(TEXT("SourceB"));
		const FString Source = MakeSource(42);
		ASSERT_THAT(IsTrue(WriteSource(SourceRootA, Source)));
		ASSERT_THAT(IsTrue(WriteSource(SourceRootB, Source)));
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRootA, Options, Prepared)));
		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session)));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(
			*TestRunner, SourceRootB, Options, Current)));
		ASSERT_THAT(AreEqual(
			Prepared.ProducerDirectDigest.ToHexString(),
			Current.DirectPlan.DirectInputDigest.ToHexString()));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(),
				Options.Profile, Current, {}, {});
		ASSERT_THAT(IsTrue(Warm.IsRestored(), *Warm.Detail));
		TSharedPtr<FAngelscriptModuleDesc> Restored =
			Consumer.GetEngine().GetModuleByModuleName(ModuleName);
		ASSERT_THAT(IsTrue(Restored.IsValid()));
		ASSERT_THAT(AreEqual(1, Restored->Code.Num()));
		ASSERT_THAT(AreEqual(
			FPaths::ConvertRelativePathToFull(SourceRootB / RelativeSourcePath),
			FPaths::ConvertRelativePathToFull(Restored->Code[0].AbsoluteFilename)));
		ASSERT_THAT(IsFalse(Restored->Code[0].AbsoluteFilename.Contains(
			TEXT("SourceA"))));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Exact warm relocation: Direct=%s OldRoot=%s CurrentPath=%s"),
			*Current.DirectPlan.DirectInputDigest.ToHexString(),
			*SourceRootA,
			*Restored->Code[0].AbsoluteFilename));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
