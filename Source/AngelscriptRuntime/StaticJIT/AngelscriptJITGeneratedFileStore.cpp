#include "StaticJIT/AngelscriptJITGeneratedFileStore.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace AngelscriptJITGeneratedFileStore_Private
{
	static constexpr TCHAR InventoryPath[] =
		TEXT("OwnedFiles.generated.json");
	static constexpr TCHAR TextOwnershipPrefix[] =
		TEXT("// @angelscript-jit-owned revision=2 kind=");
	static constexpr TCHAR JsonOwnershipPrefix[] =
		TEXT("{\n  \"ownershipRevision\": 2,");
	static constexpr TCHAR LegacyTextOwnershipPrefix[] =
		TEXT("// @angelscript-jit-owned revision=1 kind=");
	static constexpr TCHAR LegacyJsonOwnershipPrefix[] =
		TEXT("{\n  \"ownershipRevision\": 1,");

	struct FInventory
	{
		uint32 OwnershipRevision = 0;
		EAngelscriptJITTargetProfile TargetProfile =
			EAngelscriptJITTargetProfile::Invalid;
		FString ProviderId;
		TArray<FString> Files;
	};

	struct FPendingWrite
	{
		FString RelativePath;
		FString Destination;
		FString Temporary;
		bool bInventory = false;
	};

	static bool PathLess(const FString& Left, const FString& Right)
	{
		return FAngelscriptArtifactCanonicalWriter::
			CompareCanonicalUtf8Strings(Left, Right) < 0;
	}

	static FString NormalizeRoot(const FStringView RootDirectory)
	{
		FString Result(RootDirectory);
		Result.TrimStartAndEndInline();
		if (Result.IsEmpty())
		{
			return {};
		}
		Result = FPaths::ConvertRelativePathToFull(Result);
		FPaths::NormalizeDirectoryName(Result);
		return Result;
	}

	static bool IsUsableRoot(const FString& Root)
	{
		return !Root.IsEmpty()
			&& !FPaths::IsSamePath(Root, FPaths::RootDir())
			&& !FPaths::IsSamePath(Root, FPaths::ProjectDir())
			&& !FPaths::IsSamePath(Root, FPaths::EngineDir());
	}

	static bool NormalizeRelativePath(
		const FStringView RelativePath,
		FString& OutNormalized)
	{
		OutNormalized = FString(RelativePath);
		FPaths::NormalizeFilename(OutNormalized);
		if (OutNormalized.IsEmpty()
			|| !FPaths::IsRelative(OutNormalized)
			|| OutNormalized.Contains(TEXT(":"))
			|| OutNormalized == TEXT("..")
			|| OutNormalized.StartsWith(TEXT("../"))
			|| OutNormalized.Contains(TEXT("/../")))
		{
			return false;
		}
		if (!FPaths::CollapseRelativeDirectories(OutNormalized)
			|| OutNormalized == TEXT(".")
			|| OutNormalized.StartsWith(TEXT("../")))
		{
			return false;
		}
		return true;
	}

	static FString MakeAbsolutePath(
		const FString& Root,
		const FString& RelativePath)
	{
		FString Result = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(Root, RelativePath));
		FPaths::NormalizeFilename(Result);
		return Result;
	}

	static TArray<uint8> ToUtf8Bytes(const FString& Content)
	{
		FTCHARToUTF8 Converted(*Content, Content.Len());
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()),
			Converted.Length());
		return Bytes;
	}

	static FString FromUtf8Bytes(const TArray<uint8>& Bytes)
	{
		if (Bytes.IsEmpty())
		{
			return {};
		}
		FUTF8ToTCHAR Converted(
			reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		return FString(Converted.Length(), Converted.Get());
	}

	static FAngelscriptHash256 HashBytes(const TArray<uint8>& Bytes)
	{
		return FAngelscriptHash256{FBlake3::HashBuffer(
			Bytes.GetData(), static_cast<uint64>(Bytes.Num()))};
	}

	static bool StartsWithJsonOwnershipPrefix(
		const FString& Content,
		const FStringView Prefix)
	{
		const FString CanonicalPrefix(Prefix);
		if (Content.StartsWith(CanonicalPrefix, ESearchCase::CaseSensitive))
		{
			return true;
		}
		FString CrlfPrefix = CanonicalPrefix;
		CrlfPrefix.ReplaceInline(
			TEXT("\n"), TEXT("\r\n"), ESearchCase::CaseSensitive);
		return Content.StartsWith(CrlfPrefix, ESearchCase::CaseSensitive);
	}

	static bool HasCurrentOwnershipMarker(const FString& Content)
	{
		return Content.StartsWith(TextOwnershipPrefix,
				ESearchCase::CaseSensitive)
			|| StartsWithJsonOwnershipPrefix(
				Content, JsonOwnershipPrefix);
	}

	static bool StartsWithTextOwnershipMarkerLine(
		const FString& Content,
		const FString& Marker)
	{
		if (Marker.IsEmpty()
			|| !Content.StartsWith(Marker, ESearchCase::CaseSensitive))
		{
			return false;
		}
		if (Content.Len() == Marker.Len())
		{
			return true;
		}
		const TCHAR Terminator = Content[Marker.Len()];
		return Terminator == TEXT('\n') || Terminator == TEXT('\r');
	}

	static bool HasExpectedLegacyOwnershipMarker(
		const FAngelscriptJITGeneratedFile& Expected,
		const FString& ActualContent)
	{
		if (Expected.Content.StartsWith(TextOwnershipPrefix,
			ESearchCase::CaseSensitive))
		{
			int32 End = INDEX_NONE;
			if (!Expected.Content.FindChar(TEXT('\n'), End))
			{
				return false;
			}
			FString LegacyMarker = Expected.Content.Left(End);
			LegacyMarker.RemoveFromEnd(TEXT("\r"));
			LegacyMarker.ReplaceInline(
				TEXT("revision=2"), TEXT("revision=1"),
				ESearchCase::CaseSensitive);
			return StartsWithTextOwnershipMarkerLine(
				ActualContent, LegacyMarker);
		}
		return StartsWithJsonOwnershipPrefix(
				Expected.Content, JsonOwnershipPrefix)
			&& StartsWithJsonOwnershipPrefix(
				ActualContent, LegacyJsonOwnershipPrefix);
	}

	static bool IsRecognizedLegacyRemovalFile(
		const FStringView RelativePath,
		const FString& Content)
	{
		const bool bSlice = RelativePath.StartsWith(
				TEXT("Slices/"), ESearchCase::CaseSensitive)
			&& RelativePath.EndsWith(
				TEXT(".jit.hpp"), ESearchCase::CaseSensitive)
			&& Content.StartsWith(
				TEXT("// @angelscript-jit-owned revision=1 kind=function-slice\n"),
				ESearchCase::CaseSensitive);
		const bool bBucket = RelativePath.StartsWith(
				TEXT("Buckets/JITBucket_"), ESearchCase::CaseSensitive)
			&& (RelativePath.EndsWith(
					TEXT(".generated.inl"), ESearchCase::CaseSensitive)
				|| RelativePath.EndsWith(
					TEXT(".cpp"), ESearchCase::CaseSensitive))
			&& Content.StartsWith(
				TEXT("// @angelscript-jit-owned revision=1 kind=bucket\n"),
				ESearchCase::CaseSensitive);
		return bSlice || bBucket;
	}

	static FString ExpectedOwnershipMarker(
		const FAngelscriptJITGeneratedFile& Expected)
	{
		if (Expected.Content.StartsWith(TextOwnershipPrefix,
			ESearchCase::CaseSensitive))
		{
			int32 End = INDEX_NONE;
			if (Expected.Content.FindChar(TEXT('\n'), End))
			{
				FString Marker = Expected.Content.Left(End);
				Marker.RemoveFromEnd(TEXT("\r"));
				return Marker;
			}
		}
		if (StartsWithJsonOwnershipPrefix(
			Expected.Content, JsonOwnershipPrefix))
		{
			return JsonOwnershipPrefix;
		}
		return {};
	}

	static bool HasExpectedOwnershipMarker(
		const FAngelscriptJITGeneratedFile& Expected,
		const FString& ActualContent)
	{
		const FString Marker = ExpectedOwnershipMarker(Expected);
		return Expected.Content.StartsWith(
				TextOwnershipPrefix, ESearchCase::CaseSensitive)
			? StartsWithTextOwnershipMarkerLine(ActualContent, Marker)
			: !Marker.IsEmpty()
				&& StartsWithJsonOwnershipPrefix(ActualContent, Marker);
	}

	static void AddDifference(
		FAngelscriptJITGeneratedFileComparison& Comparison,
		const EAngelscriptJITGeneratedFileDifference Kind,
		FString RelativePath,
		FString Detail,
		const TArray<uint8>* ExpectedBytes = nullptr,
		const TArray<uint8>* ActualBytes = nullptr)
	{
		FAngelscriptJITGeneratedFileDifferenceRecord& Difference =
			Comparison.Differences.AddDefaulted_GetRef();
		Difference.Kind = Kind;
		Difference.RelativePath = MoveTemp(RelativePath);
		Difference.Detail = MoveTemp(Detail);
		if (ExpectedBytes != nullptr)
		{
			Difference.ExpectedHash = HashBytes(*ExpectedBytes);
		}
		if (ActualBytes != nullptr)
		{
			Difference.ActualHash = HashBytes(*ActualBytes);
		}
	}

	static bool ParseInventory(
		const FString& Content,
		const bool bAllowLegacyRemovalInventory,
		FInventory& OutInventory,
		FString& OutError)
	{
		OutInventory = {};
		const bool bCurrentMarker = StartsWithJsonOwnershipPrefix(
			Content, JsonOwnershipPrefix);
		const bool bLegacyMarker = StartsWithJsonOwnershipPrefix(
			Content, LegacyJsonOwnershipPrefix);
		if (!bCurrentMarker
			&& !(bAllowLegacyRemovalInventory && bLegacyMarker))
		{
			OutError = TEXT("Owned inventory has no recognized ownership marker.");
			return false;
		}
		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader =
			TJsonReaderFactory<>::Create(Content);
		if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
		{
			OutError = TEXT("Owned inventory is not valid JSON.");
			return false;
		}
		double Revision = 0.0;
		FString TargetProfile;
		if (!Json->TryGetNumberField(TEXT("ownershipRevision"), Revision)
			|| (Revision != static_cast<double>(
					FAngelscriptJITGeneration::OwnershipRevision)
				&& !(bAllowLegacyRemovalInventory && Revision == 1.0))
			|| !Json->TryGetStringField(TEXT("targetProfile"), TargetProfile)
			|| !Json->TryGetStringField(TEXT("providerId"),
				OutInventory.ProviderId)
			|| !FAngelscriptJITTargetProfiles::TryParse(
				TargetProfile, OutInventory.TargetProfile)
			|| !FAngelscriptJITTargetProfiles::GetTraits(
				OutInventory.TargetProfile).IsSet())
		{
			OutError = TEXT("Owned inventory identity fields are invalid.");
			return false;
		}
		OutInventory.OwnershipRevision = static_cast<uint32>(Revision);
		const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
		if (!Json->TryGetArrayField(TEXT("files"), Files) || Files == nullptr)
		{
			OutError = TEXT("Owned inventory has no file array.");
			return false;
		}
		TSet<FString> Seen;
		for (const TSharedPtr<FJsonValue>& Value : *Files)
		{
			FString RelativePath;
			FString Normalized;
			if (!Value.IsValid() || !Value->TryGetString(RelativePath)
				|| !NormalizeRelativePath(RelativePath, Normalized)
				|| Seen.Contains(Normalized))
			{
				OutError = TEXT("Owned inventory contains an invalid file path.");
				return false;
			}
			Seen.Add(Normalized);
			OutInventory.Files.Add(MoveTemp(Normalized));
		}
		OutInventory.Files.Sort(PathLess);
		return true;
	}

	static bool ValidateExpectedOutput(
		const FAngelscriptJITGenerationOutput& Expected,
		FAngelscriptJITGeneratedFileComparison& OutComparison,
		TMap<FString, const FAngelscriptJITGeneratedFile*>& OutFiles)
	{
		if (!Expected.ProviderId.IsValid()
			|| !Expected.ProviderGeneration.IsValid()
			|| !FAngelscriptJITTargetProfiles::GetTraits(
				Expected.TargetProfile).IsSet()
			|| Expected.Files.IsEmpty())
		{
			AddDifference(OutComparison,
				EAngelscriptJITGeneratedFileDifference::InvalidExpectedOutput,
				FString(), TEXT("Generated output identity is incomplete."));
			return false;
		}

		for (const FAngelscriptJITGeneratedFile& File : Expected.Files)
		{
			FString RelativePath;
			if (!NormalizeRelativePath(File.RelativePath, RelativePath)
				|| OutFiles.Contains(RelativePath)
				|| ExpectedOwnershipMarker(File).IsEmpty())
			{
				AddDifference(OutComparison,
					EAngelscriptJITGeneratedFileDifference::InvalidExpectedOutput,
					File.RelativePath,
					TEXT("Generated output has an invalid path, duplicate, or ownership marker."));
				return false;
			}
			OutFiles.Add(MoveTemp(RelativePath), &File);
		}

		if (Expected.OwnedFiles.Num() != OutFiles.Num())
		{
			AddDifference(OutComparison,
				EAngelscriptJITGeneratedFileDifference::InvalidExpectedOutput,
				InventoryPath,
				TEXT("Generated owned-file inventory count does not match files."));
			return false;
		}
		for (const FString& OwnedFile : Expected.OwnedFiles)
		{
			FString RelativePath;
			if (!NormalizeRelativePath(OwnedFile, RelativePath)
				|| !OutFiles.Contains(RelativePath))
			{
				AddDifference(OutComparison,
					EAngelscriptJITGeneratedFileDifference::InvalidExpectedOutput,
					OwnedFile,
					TEXT("Generated owned-file inventory does not match files."));
				return false;
			}
		}

		const FAngelscriptJITGeneratedFile* const* InventoryFile =
			OutFiles.Find(InventoryPath);
		FInventory ParsedInventory;
		FString InventoryError;
		if (InventoryFile == nullptr
			|| !ParseInventory((*InventoryFile)->Content, false,
				ParsedInventory, InventoryError)
			|| ParsedInventory.TargetProfile != Expected.TargetProfile
			|| ParsedInventory.ProviderId
				!= Expected.ProviderId.Hash.ToHexString()
			|| ParsedInventory.Files != Expected.OwnedFiles)
		{
			AddDifference(OutComparison,
				EAngelscriptJITGeneratedFileDifference::InvalidExpectedOutput,
				InventoryPath,
				InventoryError.IsEmpty()
					? TEXT("Generated owned-file inventory identity does not match output.")
					: InventoryError);
			return false;
		}
		return true;
	}

	static bool IsBlockingDifference(
		const EAngelscriptJITGeneratedFileDifference Kind)
	{
		return Kind != EAngelscriptJITGeneratedFileDifference::Missing
			&& Kind != EAngelscriptJITGeneratedFileDifference::ContentMismatch
			&& Kind
				!= EAngelscriptJITGeneratedFileDifference::UnexpectedOwnedFile;
	}

	static bool IsModuleSourcePath(const FStringView RelativePath)
	{
		return RelativePath.EndsWith(TEXT(".jit.cpp"),
				ESearchCase::CaseSensitive);
	}

	static void SortDifferences(
		TArray<FAngelscriptJITGeneratedFileDifferenceRecord>& Differences)
	{
		Differences.Sort([](
			const FAngelscriptJITGeneratedFileDifferenceRecord& Left,
			const FAngelscriptJITGeneratedFileDifferenceRecord& Right)
		{
			const int32 PathOrder =
				FAngelscriptArtifactCanonicalWriter::
					CompareCanonicalUtf8Strings(
						Left.RelativePath, Right.RelativePath);
			return PathOrder != 0
				? PathOrder < 0
				: static_cast<uint8>(Left.Kind)
					< static_cast<uint8>(Right.Kind);
		});
	}
}

FAngelscriptJITGeneratedFileComparison
FAngelscriptJITGeneratedFileStore::Compare(
	const FStringView RootDirectory,
	const FAngelscriptJITGenerationOutput& Expected)
{
	using namespace AngelscriptJITGeneratedFileStore_Private;
	FAngelscriptJITGeneratedFileComparison Result;
	Result.RootDirectory = NormalizeRoot(RootDirectory);
	if (!IsUsableRoot(Result.RootDirectory))
	{
		AddDifference(Result,
			EAngelscriptJITGeneratedFileDifference::InvalidExpectedOutput,
			FString(), TEXT("Generated output root is empty or too broad."));
		return Result;
	}

	TMap<FString, const FAngelscriptJITGeneratedFile*> ExpectedFiles;
	if (!ValidateExpectedOutput(Expected, Result, ExpectedFiles))
	{
		SortDifferences(Result.Differences);
		return Result;
	}

	FInventory ActualInventory;
	const FString InventoryFilename = MakeAbsolutePath(
		Result.RootDirectory, InventoryPath);
	const bool bInventoryExists =
		IFileManager::Get().FileExists(*InventoryFilename);
	if (bInventoryExists)
	{
		TArray<uint8> InventoryBytes;
		if (!FFileHelper::LoadFileToArray(
			InventoryBytes, *InventoryFilename))
		{
			AddDifference(Result,
				EAngelscriptJITGeneratedFileDifference::ReadFailure,
				InventoryPath, TEXT("Could not read owned-file inventory."));
		}
		else
		{
			FString InventoryError;
			if (!ParseInventory(FromUtf8Bytes(InventoryBytes), true,
				ActualInventory, InventoryError))
			{
				AddDifference(Result,
					EAngelscriptJITGeneratedFileDifference::InvalidInventory,
					InventoryPath, MoveTemp(InventoryError), nullptr,
					&InventoryBytes);
			}
			else
			{
				if (ActualInventory.TargetProfile != Expected.TargetProfile)
				{
					AddDifference(Result,
						EAngelscriptJITGeneratedFileDifference::TargetProfileMismatch,
						InventoryPath,
						FString::Printf(
							TEXT("Output root owns %s, expected %s."),
							FAngelscriptJITTargetProfiles::ToString(
								ActualInventory.TargetProfile),
							FAngelscriptJITTargetProfiles::ToString(
								Expected.TargetProfile)));
				}
				if (ActualInventory.ProviderId
					!= Expected.ProviderId.Hash.ToHexString())
				{
					AddDifference(Result,
						EAngelscriptJITGeneratedFileDifference::ProviderIdMismatch,
						InventoryPath,
						TEXT("Output root belongs to a different ProviderId."));
				}
			}
		}
	}

	for (const TPair<FString,
		const FAngelscriptJITGeneratedFile*>& Pair : ExpectedFiles)
	{
		const FString Filename = MakeAbsolutePath(
			Result.RootDirectory, Pair.Key);
		if (!IFileManager::Get().FileExists(*Filename))
		{
			const TArray<uint8> ExpectedBytes =
				ToUtf8Bytes(Pair.Value->Content);
			AddDifference(Result,
				EAngelscriptJITGeneratedFileDifference::Missing,
				Pair.Key, TEXT("Expected generated file is missing."),
				&ExpectedBytes);
			continue;
		}
		TArray<uint8> ActualBytes;
		if (!FFileHelper::LoadFileToArray(ActualBytes, *Filename))
		{
			AddDifference(Result,
				EAngelscriptJITGeneratedFileDifference::ReadFailure,
				Pair.Key, TEXT("Could not read generated file."));
			continue;
		}
		const TArray<uint8> ExpectedBytes = ToUtf8Bytes(Pair.Value->Content);
		if (ActualBytes == ExpectedBytes)
		{
			continue;
		}
		const FString ActualContent = FromUtf8Bytes(ActualBytes);
		const bool bLegacyOwnedTarget =
			ActualInventory.OwnershipRevision == 1
			&& ActualInventory.Files.Contains(Pair.Key)
			&& HasExpectedLegacyOwnershipMarker(
				*Pair.Value, ActualContent);
		AddDifference(Result,
			HasExpectedOwnershipMarker(*Pair.Value, ActualContent)
				|| bLegacyOwnedTarget
				? EAngelscriptJITGeneratedFileDifference::ContentMismatch
				: EAngelscriptJITGeneratedFileDifference::OwnershipConflict,
			Pair.Key,
			HasExpectedOwnershipMarker(*Pair.Value, ActualContent)
				|| bLegacyOwnedTarget
				? TEXT("Owned generated bytes are stale.")
				: TEXT("Target exists without the expected ownership marker."),
			&ExpectedBytes, &ActualBytes);
	}

	if (bInventoryExists
		&& ActualInventory.TargetProfile
			!= EAngelscriptJITTargetProfile::Invalid)
	{
		for (const FString& ActualOwnedFile : ActualInventory.Files)
		{
			if (ExpectedFiles.Contains(ActualOwnedFile))
			{
				continue;
			}
			const FString Filename = MakeAbsolutePath(
				Result.RootDirectory, ActualOwnedFile);
			if (!IFileManager::Get().FileExists(*Filename))
			{
				continue;
			}
			TArray<uint8> ActualBytes;
			if (!FFileHelper::LoadFileToArray(ActualBytes, *Filename))
			{
				AddDifference(Result,
					EAngelscriptJITGeneratedFileDifference::ReadFailure,
					ActualOwnedFile,
					TEXT("Could not read prior owned file."));
				continue;
			}
			const FString ActualContent = FromUtf8Bytes(ActualBytes);
			const bool bOwnedRemoval =
				ActualInventory.OwnershipRevision == 1
					? IsRecognizedLegacyRemovalFile(
						ActualOwnedFile, ActualContent)
					: HasCurrentOwnershipMarker(ActualContent);
			AddDifference(Result,
				bOwnedRemoval
					? EAngelscriptJITGeneratedFileDifference::UnexpectedOwnedFile
					: EAngelscriptJITGeneratedFileDifference::OwnershipConflict,
				ActualOwnedFile,
				TEXT("Prior inventory contains a file absent from current output."),
				nullptr, &ActualBytes);
		}
	}

	SortDifferences(Result.Differences);
	return Result;
}

FAngelscriptJITGeneratedFilePublication
FAngelscriptJITGeneratedFileStore::Publish(
	const FStringView RootDirectory,
	const FAngelscriptJITGenerationOutput& Expected)
{
	using namespace AngelscriptJITGeneratedFileStore_Private;
	FAngelscriptJITGeneratedFilePublication Result;
	Result.Comparison = Compare(RootDirectory, Expected);
	for (const FAngelscriptJITGeneratedFileDifferenceRecord& Difference :
		Result.Comparison.Differences)
	{
		if (Difference.Kind
			== EAngelscriptJITGeneratedFileDifference::Missing)
		{
			const FAngelscriptJITGeneratedFile* ExpectedFile =
				Expected.Files.FindByPredicate(
					[Path = Difference.RelativePath](
						const FAngelscriptJITGeneratedFile& File)
					{
						return File.RelativePath == Path;
					});
			if (ExpectedFile != nullptr
				&& ExpectedFile->Kind
					== EAngelscriptJITGeneratedFileKind::ModuleSource)
			{
				Result.AddedModuleSources.Add(Difference.RelativePath);
			}
		}
		else if (Difference.Kind
				== EAngelscriptJITGeneratedFileDifference::UnexpectedOwnedFile
			&& IsModuleSourcePath(Difference.RelativePath))
		{
			Result.RemovedModuleSources.Add(Difference.RelativePath);
		}
	}
	Result.AddedModuleSources.Sort(PathLess);
	Result.RemovedModuleSources.Sort(PathLess);
	for (const FAngelscriptJITGeneratedFileDifferenceRecord& Difference :
		Result.Comparison.Differences)
	{
		if (IsBlockingDifference(Difference.Kind))
		{
			Result.Error = FString::Printf(
				TEXT("Refusing generated output publication at '%s': %s (%s)."),
				*Result.Comparison.RootDirectory,
				*Difference.RelativePath, *Difference.Detail);
			return Result;
		}
	}

	const FString& Root = Result.Comparison.RootDirectory;
	if (!IFileManager::Get().DirectoryExists(*Root)
		&& !IFileManager::Get().MakeDirectory(*Root, true))
	{
		Result.Error = FString::Printf(
			TEXT("Could not create generated output root '%s'."), *Root);
		return Result;
	}

	TArray<FPendingWrite> PendingWrites;
	for (const FAngelscriptJITGeneratedFile& File : Expected.Files)
	{
		const FString Destination = MakeAbsolutePath(Root, File.RelativePath);
		const TArray<uint8> ExpectedBytes = ToUtf8Bytes(File.Content);
		TArray<uint8> ActualBytes;
		if (IFileManager::Get().FileExists(*Destination)
			&& FFileHelper::LoadFileToArray(ActualBytes, *Destination)
			&& ActualBytes == ExpectedBytes)
		{
			Result.UnchangedFiles.Add(File.RelativePath);
			continue;
		}

		const FString Parent = FPaths::GetPath(Destination);
		if (!IFileManager::Get().DirectoryExists(*Parent)
			&& !IFileManager::Get().MakeDirectory(*Parent, true))
		{
			Result.Error = FString::Printf(
				TEXT("Could not create generated output directory '%s'."),
				*Parent);
			break;
		}
		FPendingWrite& Pending = PendingWrites.AddDefaulted_GetRef();
		Pending.RelativePath = File.RelativePath;
		Pending.Destination = Destination;
		Pending.Temporary = Destination + TEXT(".asjit-tmp-")
			+ FGuid::NewGuid().ToString(EGuidFormats::Digits);
		Pending.bInventory = File.Kind
			== EAngelscriptJITGeneratedFileKind::OwnedFileInventory;
		if (!FFileHelper::SaveArrayToFile(ExpectedBytes, *Pending.Temporary))
		{
			Result.Error = FString::Printf(
				TEXT("Could not stage generated file '%s'."),
				*File.RelativePath);
			break;
		}
	}

	if (!Result.Error.IsEmpty())
	{
		for (const FPendingWrite& Pending : PendingWrites)
		{
			IFileManager::Get().Delete(*Pending.Temporary, false, true);
		}
		return Result;
	}

	PendingWrites.Sort([](const FPendingWrite& Left,
		const FPendingWrite& Right)
	{
		if (Left.bInventory != Right.bInventory)
		{
			return !Left.bInventory;
		}
		return PathLess(Left.RelativePath, Right.RelativePath);
	});

	for (const FPendingWrite& Pending : PendingWrites)
	{
		if (Pending.bInventory)
		{
			continue;
		}
		if (!IFileManager::Get().Move(*Pending.Destination,
			*Pending.Temporary, true, true))
		{
			Result.Error = FString::Printf(
				TEXT("Could not atomically replace generated file '%s'."),
				*Pending.RelativePath);
			break;
		}
		Result.WrittenFiles.Add(Pending.RelativePath);
	}

	if (Result.Error.IsEmpty())
	{
		for (const FAngelscriptJITGeneratedFileDifferenceRecord& Difference :
			Result.Comparison.Differences)
		{
			if (Difference.Kind
				!= EAngelscriptJITGeneratedFileDifference::UnexpectedOwnedFile)
			{
				continue;
			}
			const FString Filename = MakeAbsolutePath(Root,
				Difference.RelativePath);
			if (!IFileManager::Get().Delete(*Filename, false, true))
			{
				Result.Error = FString::Printf(
					TEXT("Could not remove stale owned file '%s'."),
					*Difference.RelativePath);
				break;
			}
			Result.RemovedFiles.Add(Difference.RelativePath);
		}
	}

	if (Result.Error.IsEmpty())
	{
		for (const FPendingWrite& Pending : PendingWrites)
		{
			if (!Pending.bInventory)
			{
				continue;
			}
			if (!IFileManager::Get().Move(*Pending.Destination,
				*Pending.Temporary, true, true))
			{
				Result.Error = FString::Printf(
					TEXT("Could not atomically publish owned-file inventory '%s'."),
					*Pending.RelativePath);
				break;
			}
			Result.WrittenFiles.Add(Pending.RelativePath);
		}
	}

	for (const FPendingWrite& Pending : PendingWrites)
	{
		if (IFileManager::Get().FileExists(*Pending.Temporary))
		{
			IFileManager::Get().Delete(*Pending.Temporary, false, true);
		}
	}
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}

	Result.WrittenFiles.Sort(PathLess);
	Result.UnchangedFiles.Sort(PathLess);
	Result.RemovedFiles.Sort(PathLess);
	Result.bSuccess = true;
	return Result;
}

FAngelscriptJITOwnedOutputInspection
FAngelscriptJITGeneratedFileStore::InspectOwnedOutput(
	const FStringView RootDirectory,
	const EAngelscriptJITTargetProfile ExpectedProfile,
	const FAngelscriptJITProviderId& ExpectedProviderId)
{
	using namespace AngelscriptJITGeneratedFileStore_Private;
	FAngelscriptJITOwnedOutputInspection Result;
	Result.RootDirectory = NormalizeRoot(RootDirectory);
	if (!IsUsableRoot(Result.RootDirectory)
		|| !FAngelscriptJITTargetProfiles::GetTraits(ExpectedProfile).IsSet()
		|| !ExpectedProviderId.IsValid())
	{
		Result.Error = TEXT("Owned output inspection received an invalid root or identity.");
		return Result;
	}

	const FString InventoryFilename = MakeAbsolutePath(
		Result.RootDirectory, InventoryPath);
	Result.bExists = IFileManager::Get().FileExists(*InventoryFilename);
	if (!Result.bExists)
	{
		Result.bValid = true;
		return Result;
	}

	TArray<uint8> InventoryBytes;
	FInventory Inventory;
	if (!FFileHelper::LoadFileToArray(InventoryBytes, *InventoryFilename))
	{
		Result.Error = TEXT("Could not read the owned output inventory.");
		return Result;
	}
	if (!ParseInventory(FromUtf8Bytes(InventoryBytes), false,
		Inventory, Result.Error))
	{
		return Result;
	}
	if (Inventory.OwnershipRevision
			!= FAngelscriptJITGeneration::OwnershipRevision
		|| Inventory.TargetProfile != ExpectedProfile
		|| Inventory.ProviderId != ExpectedProviderId.Hash.ToHexString()
		|| !Inventory.Files.Contains(InventoryPath))
	{
		Result.Error =
			TEXT("Owned output inventory is not revision 2 or belongs to another profile/provider.");
		return Result;
	}

	for (const FString& RelativePath : Inventory.Files)
	{
		const FString Filename = MakeAbsolutePath(
			Result.RootDirectory, RelativePath);
		if (!IFileManager::Get().FileExists(*Filename))
		{
			continue;
		}
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
		{
			Result.Error = FString::Printf(
				TEXT("Could not read inventory-owned path '%s'."),
				*RelativePath);
			return Result;
		}
		if (!HasCurrentOwnershipMarker(FromUtf8Bytes(Bytes)))
		{
			Result.Error = FString::Printf(
				TEXT("Inventory path '%s' no longer has a revision-2 ownership marker."),
				*RelativePath);
			return Result;
		}
	}

	Result.OwnedFiles = Inventory.Files;
	for (const FString& RelativePath : Result.OwnedFiles)
	{
		if (IsModuleSourcePath(RelativePath))
		{
			Result.ModuleSources.Add(RelativePath);
		}
	}
	Result.ModuleSources.Sort(PathLess);
	Result.bValid = true;
	return Result;
}

FAngelscriptJITOwnedOutputRetirement
FAngelscriptJITGeneratedFileStore::RetireOwnedOutput(
	const FStringView RootDirectory,
	const EAngelscriptJITTargetProfile ExpectedProfile,
	const FAngelscriptJITProviderId& ExpectedProviderId)
{
	using namespace AngelscriptJITGeneratedFileStore_Private;
	FAngelscriptJITOwnedOutputRetirement Result;
	const FAngelscriptJITOwnedOutputInspection Inspection =
		InspectOwnedOutput(RootDirectory, ExpectedProfile, ExpectedProviderId);
	if (!Inspection.bValid)
	{
		Result.Error = Inspection.Error;
		return Result;
	}
	if (!Inspection.bExists)
	{
		Result.bSuccess = true;
		return Result;
	}

	TArray<FString> RemovalOrder = Inspection.OwnedFiles;
	RemovalOrder.Sort([](const FString& Left, const FString& Right)
	{
		if (Left == InventoryPath || Right == InventoryPath)
		{
			return Right == InventoryPath && Left != InventoryPath;
		}
		return PathLess(Left, Right);
	});
	TSet<FString> CandidateDirectories;
	for (const FString& RelativePath : RemovalOrder)
	{
		const FString Filename = MakeAbsolutePath(
			Inspection.RootDirectory, RelativePath);
		if (IFileManager::Get().FileExists(*Filename)
			&& !IFileManager::Get().Delete(*Filename, false, true))
		{
			Result.Error = FString::Printf(
				TEXT("Could not retire inventory-owned path '%s'."),
				*RelativePath);
			return Result;
		}
		Result.RemovedFiles.Add(RelativePath);
		if (IsModuleSourcePath(RelativePath))
		{
			Result.RemovedModuleSources.Add(RelativePath);
		}

		FString Directory = FPaths::GetPath(Filename);
		while (!Directory.IsEmpty()
			&& FPaths::IsUnderDirectory(
				Directory, Inspection.RootDirectory))
		{
			CandidateDirectories.Add(Directory);
			Directory = FPaths::GetPath(Directory);
		}
	}

	TArray<FString> Directories = CandidateDirectories.Array();
	Directories.Sort([](const FString& Left, const FString& Right)
	{
		return Left.Len() > Right.Len();
	});
	for (const FString& Directory : Directories)
	{
		IFileManager::Get().DeleteDirectory(*Directory, false, false);
	}
	IFileManager::Get().DeleteDirectory(
		*Inspection.RootDirectory, false, false);

	Result.RemovedFiles.Sort(PathLess);
	Result.RemovedModuleSources.Sort(PathLess);
	Result.bSuccess = true;
	return Result;
}
