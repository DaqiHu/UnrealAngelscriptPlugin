#include "Framework/Source/AngelscriptTestCodeBuilder.h"
#include "Framework/Source/AngelscriptTestAnnotations.h"
#include "Framework/Source/AngelscriptTestSourceOriginMap.h"
#include "Framework/Source/AngelscriptTestSourceResult.h"

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Misc/Optional.h"

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

void ValidateSourceDescriptor(
	const FAngelscriptTestSource& Source,
	const FAngelscriptTestSourceDescriptor& Descriptor,
	FStringView FileTag,
	FStringView VersionTag,
	TArray<FAngelscriptTestError>& OutErrors)
{
	const int32 CleanNum = Source.GetBytes().Num();
	for (const FAngelscriptTestSourceDescriptor::FPoint& Point : Descriptor.Annotations.Points)
	{
		if (Point.Offset < 0 || Point.Offset > CleanNum)
		{
			OutErrors.Add(MakeBuilderError(
				TEXT("InvalidAnnotationOffset"),
				TEXT("A Point offset must lie in [0, clean Num]."),
				FileTag,
				VersionTag));
		}
	}
	for (const FAngelscriptTestSourceDescriptor::FBreakpoint& Breakpoint : Descriptor.Annotations.Breakpoints)
	{
		if (Breakpoint.Offset < 0 || Breakpoint.Offset > CleanNum)
		{
			OutErrors.Add(MakeBuilderError(
				TEXT("InvalidAnnotationOffset"),
				TEXT("A Breakpoint offset must lie in [0, clean Num]."),
				FileTag,
				VersionTag));
		}
	}
	for (const FAngelscriptTestSourceDescriptor::FRange& Range : Descriptor.Annotations.Ranges)
	{
		if (Range.Begin > Range.End || Range.Begin < 0 || Range.End > CleanNum)
		{
			OutErrors.Add(MakeBuilderError(
				TEXT("InvalidSourceRange"),
				TEXT("A Range must be half-open and lie within clean Num."),
				FileTag,
				VersionTag));
		}
	}

	TArray<FAngelscriptTestSourceDescriptor::FOriginSpan> Spans = Descriptor.OriginSpans;
	Spans.Sort([](const FAngelscriptTestSourceDescriptor::FOriginSpan& Left,
		const FAngelscriptTestSourceDescriptor::FOriginSpan& Right)
	{
		return Left.CleanBegin < Right.CleanBegin;
	});

	int32 Cursor = 0;
	int32 MappedAuthoredEnd = 0;
	for (const FAngelscriptTestSourceDescriptor::FOriginSpan& Span : Spans)
	{
		if (Span.Length <= 0 || Span.CleanBegin < 0 || Span.AuthoredBegin < 0
			|| Span.CleanBegin + Span.Length > CleanNum)
		{
			OutErrors.Add(MakeBuilderError(
				TEXT("InvalidOriginSpan"),
				TEXT("Origin spans must have positive length and stay within clean Num."),
				FileTag,
				VersionTag));
			continue;
		}
		if (Span.CleanBegin < Cursor)
		{
			OutErrors.Add(MakeBuilderError(
				TEXT("InvalidOriginSpan"),
				TEXT("Origin spans must not overlap."),
				FileTag,
				VersionTag));
			continue;
		}
		if (Span.CleanBegin > Cursor)
		{
			OutErrors.Add(MakeBuilderError(
				TEXT("IncompleteOriginCoverage"),
				TEXT("Origin spans must cover every clean byte without gaps."),
				FileTag,
				VersionTag));
		}
		Cursor = Span.CleanBegin + Span.Length;
		MappedAuthoredEnd = FMath::Max(MappedAuthoredEnd, Span.AuthoredBegin + Span.Length);
	}
	if (Cursor < CleanNum)
	{
		OutErrors.Add(MakeBuilderError(
			TEXT("IncompleteOriginCoverage"),
			TEXT("Origin spans must cover every clean byte without gaps."),
			FileTag,
			VersionTag));
	}
	if (Descriptor.AuthoredEnd < 0 || Descriptor.AuthoredEnd < MappedAuthoredEnd)
	{
		OutErrors.Add(MakeBuilderError(
			TEXT("InvalidAuthoredEnd"),
			TEXT("AuthoredEnd must stay in authored bounds and not reverse a continuous mapping."),
			FileTag,
			VersionTag));
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
			bool bInRoot,
			TOptional<FAngelscriptTestSourceDescriptor> InDescriptor = {})
			: VersionMeta(MoveTemp(InVersionMeta))
			, Source(MoveTemp(InSource))
			, bRoot(bInRoot)
			, Descriptor(MoveTemp(InDescriptor))
		{
		}

		FAngelscriptTestVersionMeta VersionMeta;
		FAngelscriptTestSource Source;
		bool bRoot = false;
		TOptional<FAngelscriptTestSourceDescriptor> Descriptor;
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
	bool bRoot,
	TOptional<FAngelscriptTestSourceDescriptor> Descriptor)
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
	else if (VersionMeta.Parent.IsSet() && !IsValidVersionTag(VersionMeta.Parent.GetValue()))
	{
		Data->Errors.Add(MakeBuilderError(
			TEXT("InvalidParentTag"),
			TEXT("Parent must be a valid same-file version Tag."),
			Data->FileMeta->Tag,
			VersionTag));
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

	Data->Nodes.Emplace(MoveTemp(VersionMeta), MoveTemp(Source), bRoot, MoveTemp(Descriptor));
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

void FAngelscriptTestCodeBuilder::AddRoot(
	FAngelscriptTestVersionMeta VersionMeta,
	FAngelscriptTestSource Source,
	FAngelscriptTestSourceDescriptor Descriptor)
{
	AddNode(MoveTemp(VersionMeta), MoveTemp(Source), true, MoveTemp(Descriptor));
}

void FAngelscriptTestCodeBuilder::AddVersion(
	FAngelscriptTestVersionMeta VersionMeta,
	FAngelscriptTestSource Source)
{
	AddNode(MoveTemp(VersionMeta), MoveTemp(Source), false);
}

void FAngelscriptTestCodeBuilder::AddVersion(
	FAngelscriptTestVersionMeta VersionMeta,
	FAngelscriptTestSource Source,
	FAngelscriptTestSourceDescriptor Descriptor)
{
	AddNode(MoveTemp(VersionMeta), MoveTemp(Source), false, MoveTemp(Descriptor));
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
	for (int32 Index = 0; Index < Data->Nodes.Num(); ++Index)
	{
		const FData::FPendingNode& Node = Data->Nodes[Index];
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

	if (Data->Nodes.Num() == 0)
	{
		BuildErrors.Add(MakeBuilderError(
			TEXT("MissingRoot"),
			TEXT("A file must contain at least one version."),
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

	for (FData::FPendingNode& Node : Data->Nodes)
	{
		if (Node.Descriptor.IsSet())
		{
			ValidateSourceDescriptor(
				Node.Source,
				Node.Descriptor.GetValue(),
				Data->FileMeta->Tag,
				Node.VersionMeta.Tag,
				BuildErrors);
		}
	}

	if (!BuildErrors.IsEmpty())
	{
		return FAngelscriptTestCodeBuildResult(MoveTemp(BuildErrors));
	}

	for (FData::FPendingNode& Node : Data->Nodes)
	{
		if (Node.Descriptor.IsSet())
		{
			Node.Source = ApplySourceDescriptor(MoveTemp(Node.Source), Node.Descriptor.GetValue());
		}
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

FAngelscriptTestSource FAngelscriptTestCodeBuilder::ApplySourceDescriptor(
	FAngelscriptTestSource Source,
	const FAngelscriptTestSourceDescriptor& Descriptor)
{
	FAngelscriptTestAnnotations Annotations;
	for (const FAngelscriptTestSourceDescriptor::FPoint& Point : Descriptor.Annotations.Points)
	{
		Annotations.Points.Add(Point.Name, Point.Offset);
	}
	for (const FAngelscriptTestSourceDescriptor::FBreakpoint& Breakpoint : Descriptor.Annotations.Breakpoints)
	{
		Annotations.Breakpoints.Add(Breakpoint.Name, Breakpoint.Offset);
	}
	for (const FAngelscriptTestSourceDescriptor::FRange& Range : Descriptor.Annotations.Ranges)
	{
		Annotations.Ranges.Add(Range.Name, {Range.Begin, Range.End});
	}

	FAngelscriptTestSourceOriginMap OriginMap;
	OriginMap.HostFile = Descriptor.AuthoredPath;
	OriginMap.HostLine = Descriptor.AuthoredLine;
	const int32 CleanNum = Source.GetBytes().Num();
	OriginMap.OriginalByteOffsets.Init(0, CleanNum + 1);
	for (const FAngelscriptTestSourceDescriptor::FOriginSpan& Span : Descriptor.OriginSpans)
	{
		for (int32 Index = 0; Index < Span.Length; ++Index)
		{
			OriginMap.OriginalByteOffsets[Span.CleanBegin + Index] = Span.AuthoredBegin + Index;
		}
	}
	OriginMap.OriginalByteOffsets[CleanNum] = Descriptor.AuthoredEnd;

	TArray<uint8> Bytes;
	Bytes.Append(Source.GetBytes().GetData(), CleanNum);
	return FAngelscriptTestSource::FromParsedData(
		MoveTemp(Bytes),
		MoveTemp(Annotations),
		MoveTemp(OriginMap));
}
}
