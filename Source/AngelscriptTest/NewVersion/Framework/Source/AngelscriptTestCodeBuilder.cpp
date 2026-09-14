#include "NewVersion/Framework/Source/AngelscriptTestCodeBuilder.h"
#include "NewVersion/Framework/Source/AngelscriptTestSourceResult.h"

#include "Containers/Map.h"
#include "Containers/Set.h"

namespace AngelscriptTest
{
namespace
{
FAngelscriptTestError MakeBuilderError(
	FStringView Code,
	FStringView Message,
	FStringView FileTag,
	FStringView VersionTag = FStringView())
{
	return {
		.Code = FString(Code),
		.Message = FString(Message),
		.FileTag = FString(FileTag),
		.VersionTag = FString(VersionTag),
	};
}

bool IsNonEmptyText(FStringView Value)
{
	for (const TCHAR Character : Value)
	{
		if (!FChar::IsWhitespace(Character))
		{
			return true;
		}
	}
	return false;
}

bool IsValidFileTag(FStringView Tag)
{
	if (Tag.IsEmpty() || Tag[0] == TEXT('/') || Tag[Tag.Len() - 1] == TEXT('/'))
	{
		return false;
	}

	int32 SegmentStart = 0;
	for (int32 Index = 0; Index <= Tag.Len(); ++Index)
	{
		if (Index < Tag.Len() && Tag[Index] == TEXT('\\'))
		{
			return false;
		}
		if (Index < Tag.Len() && Tag[Index] != TEXT('/'))
		{
			continue;
		}

		const FStringView Segment = Tag.Mid(SegmentStart, Index - SegmentStart);
		if (Segment.IsEmpty() || Segment == TEXT(".") || Segment == TEXT("..")
			|| Segment.Find(TEXT(":")) != INDEX_NONE)
		{
			return false;
		}
		SegmentStart = Index + 1;
	}

	return true;
}

bool IsValidVersionTag(FStringView Tag)
{
	if (Tag.IsEmpty())
	{
		return false;
	}

	for (const TCHAR Character : Tag)
	{
		if (FChar::IsWhitespace(Character) || Character == TEXT('/') || Character == TEXT('\\'))
		{
			return false;
		}
	}
	return true;
}

void ValidateTopics(
	TConstArrayView<FString> Topics,
	FStringView FileTag,
	FStringView VersionTag,
	TArray<FAngelscriptTestError>& OutErrors)
{
	for (const FString& Topic : Topics)
	{
		if (!IsNonEmptyText(Topic))
		{
			OutErrors.Add(MakeBuilderError(
				TEXT("EmptyTopic"),
				TEXT("Topics must not be empty or whitespace-only."),
				FileTag,
				VersionTag));
		}
	}
}
}

struct FAngelscriptTestSourceCase::FData
{
	FData(
		TSharedRef<const FAngelscriptTestFileMeta, ESPMode::ThreadSafe> InFileMeta,
		FAngelscriptTestVersionMeta InVersionMeta,
		FAngelscriptTestSource InSource)
		: FileMeta(MoveTemp(InFileMeta))
		, VersionMeta(MoveTemp(InVersionMeta))
		, Source(MoveTemp(InSource))
	{
	}

	TSharedRef<const FAngelscriptTestFileMeta, ESPMode::ThreadSafe> FileMeta;
	FAngelscriptTestVersionMeta VersionMeta;
	FAngelscriptTestSource Source;
};

FAngelscriptTestSourceCase::FAngelscriptTestSourceCase(
	TSharedRef<const FAngelscriptTestFileMeta, ESPMode::ThreadSafe> FileMeta,
	FAngelscriptTestVersionMeta VersionMeta,
	FAngelscriptTestSource Source)
	: Data(MakeShared<FData, ESPMode::ThreadSafe>(
		MoveTemp(FileMeta), MoveTemp(VersionMeta), MoveTemp(Source)))
{
}

const FAngelscriptTestFileMeta& FAngelscriptTestSourceCase::GetFileMeta() const &
{
	return *Data->FileMeta;
}

const FAngelscriptTestVersionMeta& FAngelscriptTestSourceCase::GetVersionMeta() const &
{
	return Data->VersionMeta;
}

FAngelscriptTestSource FAngelscriptTestSourceCase::GetSource() const
{
	return Data->Source;
}

FAngelscriptTestSourceResult::FAngelscriptTestSourceResult(FAngelscriptTestSourceCase InSourceCase)
	: SourceCase(MoveTemp(InSourceCase))
{
}

FAngelscriptTestSourceResult::FAngelscriptTestSourceResult(FAngelscriptTestError InError)
	: Error(MoveTemp(InError))
{
}

bool FAngelscriptTestSourceResult::IsSuccess() const
{
	return SourceCase.IsSet();
}

const FAngelscriptTestSourceCase* FAngelscriptTestSourceResult::GetCase() const &
{
	return SourceCase.IsSet() ? &SourceCase.GetValue() : nullptr;
}

const FAngelscriptTestError* FAngelscriptTestSourceResult::GetError() const &
{
	return Error.IsSet() ? &Error.GetValue() : nullptr;
}

struct FAngelscriptTestCodeBuildResult::FPayload
{
	TArray<FAngelscriptTestSourceCase> SourceCases;
};

FAngelscriptTestCodeBuildResult::~FAngelscriptTestCodeBuildResult() = default;
FAngelscriptTestCodeBuildResult::FAngelscriptTestCodeBuildResult(
	FAngelscriptTestCodeBuildResult&& Other) noexcept = default;
FAngelscriptTestCodeBuildResult& FAngelscriptTestCodeBuildResult::operator=(
	FAngelscriptTestCodeBuildResult&& Other) noexcept = default;

FAngelscriptTestCodeBuildResult::FAngelscriptTestCodeBuildResult(
	TArray<FAngelscriptTestSourceCase> SourceCases)
	: Payload(MakeUnique<FPayload>())
{
	Payload->SourceCases = MoveTemp(SourceCases);
}

FAngelscriptTestCodeBuildResult::FAngelscriptTestCodeBuildResult(
	TArray<FAngelscriptTestError> InErrors)
	: Errors(MoveTemp(InErrors))
{
}

bool FAngelscriptTestCodeBuildResult::IsSuccess() const
{
	return Payload.IsValid() && Errors.IsEmpty();
}

const TArray<FAngelscriptTestError>& FAngelscriptTestCodeBuildResult::GetErrors() const &
{
	return Errors;
}

const TArray<FAngelscriptTestSourceCase>&
FAngelscriptTestCodeBuildResult::GetCasesForAdmission() const &
{
	check(Payload.IsValid());
	return Payload->SourceCases;
}

TArray<FAngelscriptTestSourceCase> FAngelscriptTestCodeBuildResult::TakeCasesForAdmission()
{
	check(Payload.IsValid());
	return MoveTemp(Payload->SourceCases);
}

struct FAngelscriptTestCodeBuilder::FData
{
	struct FPendingNode
	{
		FPendingNode(
			FAngelscriptTestVersionMeta InVersionMeta,
			FAngelscriptTestSource InSource,
			bool bInRoot)
			: VersionMeta(MoveTemp(InVersionMeta))
			, Source(MoveTemp(InSource))
			, bRoot(bInRoot)
		{
		}

		FAngelscriptTestVersionMeta VersionMeta;
		FAngelscriptTestSource Source;
		bool bRoot = false;
	};

	explicit FData(const FAngelscriptTestFileMeta& InFileMeta)
		: FileMeta(MakeShared<FAngelscriptTestFileMeta, ESPMode::ThreadSafe>(InFileMeta))
	{
		if (FileMeta->Version != TEXT("v1"))
		{
			Errors.Add(MakeBuilderError(
				TEXT("UnsupportedFileVersion"),
				TEXT("File metadata Version must be v1."),
				FileMeta->Tag));
		}
		if (!IsValidFileTag(FileMeta->Tag))
		{
			Errors.Add(MakeBuilderError(
				TEXT("InvalidFileTag"),
				TEXT("File Tag must be a relative slash-normalized logical path."),
				FileMeta->Tag));
		}
		if (!IsNonEmptyText(FileMeta->Summary))
		{
			Errors.Add(MakeBuilderError(
				TEXT("EmptyFileSummary"),
				TEXT("File Summary must not be empty."),
				FileMeta->Tag));
		}
		ValidateTopics(FileMeta->Topics, FileMeta->Tag, FStringView(), Errors);
	}

	TSharedRef<const FAngelscriptTestFileMeta, ESPMode::ThreadSafe> FileMeta;
	TArray<FPendingNode> Nodes;
	TArray<FAngelscriptTestError> Errors;
	bool bConsumed = false;
};

void FAngelscriptTestCodeBuilder::AddNode(
	FAngelscriptTestVersionMeta VersionMeta,
	FAngelscriptTestSource Source,
	bool bRoot)
{
	if (Data->bConsumed)
	{
		if (!Data->Errors.ContainsByPredicate([](const FAngelscriptTestError& Error)
		{
			return Error.Code == TEXT("BuilderConsumed");
		}))
		{
			Data->Errors.Add(MakeBuilderError(
				TEXT("BuilderConsumed"),
				TEXT("The builder has already been consumed."),
				Data->FileMeta->Tag,
				VersionMeta.Tag));
		}
		return;
	}

	const FString VersionTag = VersionMeta.Tag;
	if (!IsValidVersionTag(VersionMeta.Tag))
	{
		Data->Errors.Add(MakeBuilderError(
			TEXT("InvalidVersionTag"),
			TEXT("Version Tag must be a nonempty token without whitespace or slashes."),
			Data->FileMeta->Tag,
			VersionTag));
	}
	if (!IsNonEmptyText(VersionMeta.Summary))
	{
		Data->Errors.Add(MakeBuilderError(
			TEXT("EmptyVersionSummary"),
			TEXT("Version Summary must not be empty."),
			Data->FileMeta->Tag,
			VersionTag));
	}
	ValidateTopics(VersionMeta.Topics, Data->FileMeta->Tag, VersionTag, Data->Errors);

	if (bRoot)
	{
		if (VersionMeta.Tag != TEXT("root"))
		{
			Data->Errors.Add(MakeBuilderError(
				TEXT("RootTagRequired"),
				TEXT("The root version Tag must be root."),
				Data->FileMeta->Tag,
				VersionTag));
		}
		if (VersionMeta.Parent.IsSet())
		{
			Data->Errors.Add(MakeBuilderError(
				TEXT("RootParentForbidden"),
				TEXT("The root version must not declare a Parent."),
				Data->FileMeta->Tag,
				VersionTag));
		}
	}
	else
	{
		if (VersionMeta.Tag == TEXT("root"))
		{
			Data->Errors.Add(MakeBuilderError(
				TEXT("RootMustUseAddRoot"),
				TEXT("The root version must be added with AddRoot."),
				Data->FileMeta->Tag,
				VersionTag));
		}
		if (!VersionMeta.Parent.IsSet())
		{
			Data->Errors.Add(MakeBuilderError(
				TEXT("VersionParentRequired"),
				TEXT("Every non-root version must declare a Parent."),
				Data->FileMeta->Tag,
				VersionTag));
		}
		else if (!IsValidVersionTag(VersionMeta.Parent.GetValue()))
		{
			Data->Errors.Add(MakeBuilderError(
				TEXT("InvalidParentTag"),
				TEXT("Parent must be a valid same-file version Tag."),
				Data->FileMeta->Tag,
				VersionTag));
		}
	}

	if (!VersionTag.IsEmpty()
		&& Data->Nodes.ContainsByPredicate([&VersionTag](const FData::FPendingNode& Node)
		{
			return Node.VersionMeta.Tag == VersionTag;
		}))
	{
		Data->Errors.Add(MakeBuilderError(
			TEXT("DuplicateVersionTag"),
			TEXT("Each version Tag must be unique within a file."),
			Data->FileMeta->Tag,
			VersionTag));
	}

	Data->Nodes.Emplace(MoveTemp(VersionMeta), MoveTemp(Source), bRoot);
}

FAngelscriptTestCodeBuilder::FAngelscriptTestCodeBuilder(const FAngelscriptTestFileMeta& FileMeta)
	: Data(MakeUnique<FData>(FileMeta))
{
}

FAngelscriptTestCodeBuilder::~FAngelscriptTestCodeBuilder() = default;

void FAngelscriptTestCodeBuilder::AddRoot(
	FAngelscriptTestVersionMeta VersionMeta,
	FAngelscriptTestSource Source)
{
	AddNode(MoveTemp(VersionMeta), MoveTemp(Source), true);
}

void FAngelscriptTestCodeBuilder::AddVersion(
	FAngelscriptTestVersionMeta VersionMeta,
	FAngelscriptTestSource Source)
{
	AddNode(MoveTemp(VersionMeta), MoveTemp(Source), false);
}

FAngelscriptTestCodeBuildResult FAngelscriptTestCodeBuilder::Build()
{
	if (Data->bConsumed)
	{
		TArray<FAngelscriptTestError> ConsumedErrors;
		ConsumedErrors.Add(MakeBuilderError(
			TEXT("BuilderConsumed"),
			TEXT("The builder has already been consumed."),
			Data->FileMeta->Tag));
		return FAngelscriptTestCodeBuildResult(MoveTemp(ConsumedErrors));
	}

	Data->bConsumed = true;
	TArray<FAngelscriptTestError> BuildErrors = Data->Errors;

	TMap<FString, int32> TagCounts;
	TMap<FString, int32> UniqueNodeIndices;
	int32 RootCount = 0;
	for (int32 Index = 0; Index < Data->Nodes.Num(); ++Index)
	{
		const FData::FPendingNode& Node = Data->Nodes[Index];
		RootCount += Node.bRoot ? 1 : 0;
		int32& Count = TagCounts.FindOrAdd(Node.VersionMeta.Tag);
		++Count;
		if (Count == 1)
		{
			UniqueNodeIndices.Add(Node.VersionMeta.Tag, Index);
		}
		else
		{
			UniqueNodeIndices.Remove(Node.VersionMeta.Tag);
		}
	}

	if (RootCount == 0)
	{
		BuildErrors.Add(MakeBuilderError(
			TEXT("MissingRoot"),
			TEXT("A file must contain exactly one root version."),
			Data->FileMeta->Tag));
	}
	else if (RootCount > 1)
	{
		BuildErrors.Add(MakeBuilderError(
			TEXT("MultipleRoots"),
			TEXT("A file must contain exactly one root version."),
			Data->FileMeta->Tag));
	}

	for (const FData::FPendingNode& Node : Data->Nodes)
	{
		if (Node.bRoot || !Node.VersionMeta.Parent.IsSet())
		{
			continue;
		}

		const FString& ParentTag = Node.VersionMeta.Parent.GetValue();
		const int32* ParentCount = TagCounts.Find(ParentTag);
		if (ParentCount == nullptr)
		{
			BuildErrors.Add(MakeBuilderError(
				TEXT("MissingParent"),
				TEXT("The declared Parent does not exist in this file."),
				Data->FileMeta->Tag,
				Node.VersionMeta.Tag));
		}
		// A duplicate parent is already a root cause. Do not manufacture a
		// dependent missing-parent or cycle diagnostic from ambiguous input.
	}

	TSet<FString> ResolvedTags;
	for (const TPair<FString, int32>& Entry : UniqueNodeIndices)
	{
		if (ResolvedTags.Contains(Entry.Key))
		{
			continue;
		}

		TArray<FString> Path;
		TMap<FString, int32> PathPositions;
		FString CurrentTag = Entry.Key;
		while (!ResolvedTags.Contains(CurrentTag))
		{
			if (const int32* ExistingPosition = PathPositions.Find(CurrentTag))
			{
				BuildErrors.Add(MakeBuilderError(
					TEXT("VersionCycle"),
					TEXT("Version Parent relationships must not form a cycle."),
					Data->FileMeta->Tag,
					Path[*ExistingPosition]));
				break;
			}

			const int32* NodeIndex = UniqueNodeIndices.Find(CurrentTag);
			if (NodeIndex == nullptr)
			{
				break;
			}

			PathPositions.Add(CurrentTag, Path.Num());
			Path.Add(CurrentTag);
			const FData::FPendingNode& Node = Data->Nodes[*NodeIndex];
			if (Node.bRoot || !Node.VersionMeta.Parent.IsSet())
			{
				break;
			}

			const FString& ParentTag = Node.VersionMeta.Parent.GetValue();
			const int32* ParentCount = TagCounts.Find(ParentTag);
			if (ParentCount == nullptr || *ParentCount != 1)
			{
				break;
			}
			CurrentTag = ParentTag;
		}

		for (const FString& Tag : Path)
		{
			ResolvedTags.Add(Tag);
		}
	}

	if (!BuildErrors.IsEmpty())
	{
		return FAngelscriptTestCodeBuildResult(MoveTemp(BuildErrors));
	}

	TArray<int32> SortedIndices;
	SortedIndices.Reserve(Data->Nodes.Num());
	for (int32 Index = 0; Index < Data->Nodes.Num(); ++Index)
	{
		SortedIndices.Add(Index);
	}
	SortedIndices.Sort([this](int32 Left, int32 Right)
	{
		return Data->Nodes[Left].VersionMeta.Tag.Compare(
			Data->Nodes[Right].VersionMeta.Tag,
			ESearchCase::CaseSensitive) < 0;
	});

	TArray<FAngelscriptTestSourceCase> SourceCases;
	SourceCases.Reserve(SortedIndices.Num());
	for (const int32 Index : SortedIndices)
	{
		const FData::FPendingNode& Node = Data->Nodes[Index];
		SourceCases.Add(FAngelscriptTestSourceCase(Data->FileMeta, Node.VersionMeta, Node.Source));
	}
	return FAngelscriptTestCodeBuildResult(MoveTemp(SourceCases));
}
}
