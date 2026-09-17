#include "Framework/Source/AngelscriptTestSourceParser.h"

#include "Framework/Source/AngelscriptTestAnnotations.h"
#include "Framework/Source/AngelscriptTestCodeBuilder.h"
#include "Framework/Source/AngelscriptTestSourceParseResult.h"

#include "Containers/Map.h"
#include "Containers/StringConv.h"

namespace AngelscriptTest
{
namespace
{
struct FNormalizedContainer
{
	TArray<uint8> Bytes;
	TArray<int32> RawOffsets;
};

struct FLine
{
	int32 Start = 0;
	int32 End = 0;
	int32 Next = 0;
	int32 Number = 0;
};

struct FHeader
{
	FString Version;
	FString Begin;
	TOptional<FString> Parent;
	FString Summary;
	TArray<FString> Topics;
	bool bSawVersion = false;
	bool bSawBegin = false;
	bool bSawParent = false;
	bool bSawSummary = false;
};

bool IsContinuationByte(uint8 Byte)
{
	return Byte >= 0x80 && Byte <= 0xbf;
}

bool IsValidUtf8(TConstArrayView<uint8> Bytes, int32 Begin, int32& OutInvalidOffset)
{
	for (int32 Index = Begin; Index < Bytes.Num();)
	{
		const uint8 First = Bytes[Index];
		if (First <= 0x7f)
		{
			++Index;
			continue;
		}

		int32 Length = 0;
		if (First >= 0xc2 && First <= 0xdf)
		{
			Length = 2;
		}
		else if (First >= 0xe0 && First <= 0xef)
		{
			Length = 3;
		}
		else if (First >= 0xf0 && First <= 0xf4)
		{
			Length = 4;
		}
		else
		{
			OutInvalidOffset = Index;
			return false;
		}

		if (Index + Length > Bytes.Num())
		{
			OutInvalidOffset = Index;
			return false;
		}
		for (int32 Continuation = 1; Continuation < Length; ++Continuation)
		{
			if (!IsContinuationByte(Bytes[Index + Continuation]))
			{
				OutInvalidOffset = Index + Continuation;
				return false;
			}
		}

		if ((First == 0xe0 && Bytes[Index + 1] < 0xa0)
			|| (First == 0xed && Bytes[Index + 1] > 0x9f)
			|| (First == 0xf0 && Bytes[Index + 1] < 0x90)
			|| (First == 0xf4 && Bytes[Index + 1] > 0x8f))
		{
			OutInvalidOffset = Index;
			return false;
		}
		Index += Length;
	}

	return true;
}

FAngelscriptTestError MakeParserError(
	FStringView Code,
	FStringView Message,
	FStringView FileTag,
	int32 SourceLine,
	int32 ByteOffset,
	FStringView VersionTag = FStringView())
{
	return {
		.Code = FString(Code),
		.Message = FString(Message),
		.FileTag = FString(FileTag),
		.VersionTag = FString(VersionTag),
		.SourcePath = FString(FileTag) + TEXT(".as"),
		.SourceLine = SourceLine,
		.ByteOffset = ByteOffset,
	};
}

FNormalizedContainer NormalizeContainer(TConstArrayView<uint8> RawBytes, int32 Begin)
{
	FNormalizedContainer Result;
	Result.Bytes.Reserve(RawBytes.Num() - Begin);
	Result.RawOffsets.Reserve(RawBytes.Num() - Begin + 1);
	for (int32 Index = Begin; Index < RawBytes.Num();)
	{
		if (RawBytes[Index] == static_cast<uint8>('\r'))
		{
			Result.Bytes.Add(static_cast<uint8>('\n'));
			Result.RawOffsets.Add(Index);
			Index += Index + 1 < RawBytes.Num()
				&& RawBytes[Index + 1] == static_cast<uint8>('\n')
				? 2
				: 1;
		}
		else
		{
			Result.Bytes.Add(RawBytes[Index]);
			Result.RawOffsets.Add(Index);
			++Index;
		}
	}
	Result.RawOffsets.Add(RawBytes.Num());
	return Result;
}

TArray<FLine> SplitLines(TConstArrayView<uint8> Bytes)
{
	TArray<FLine> Lines;
	int32 LineNumber = 1;
	for (int32 Start = 0; Start < Bytes.Num(); ++LineNumber)
	{
		int32 End = Start;
		while (End < Bytes.Num() && Bytes[End] != static_cast<uint8>('\n'))
		{
			++End;
		}
		const int32 Next = End < Bytes.Num() ? End + 1 : End;
		Lines.Add({Start, End, Next, LineNumber});
		Start = Next;
	}
	return Lines;
}

void TrimHorizontal(TConstArrayView<uint8> Bytes, int32& Begin, int32& End)
{
	while (Begin < End && (Bytes[Begin] == static_cast<uint8>(' ')
		|| Bytes[Begin] == static_cast<uint8>('\t')))
	{
		++Begin;
	}
	while (End > Begin && (Bytes[End - 1] == static_cast<uint8>(' ')
		|| Bytes[End - 1] == static_cast<uint8>('\t')))
	{
		--End;
	}
}

bool EqualsAscii(TConstArrayView<uint8> Bytes, int32 Begin, int32 End, const ANSICHAR* Text)
{
	const int32 Length = FCStringAnsi::Strlen(Text);
	return End - Begin == Length
		&& FMemory::Memcmp(Bytes.GetData() + Begin, Text, Length) == 0;
}

FString Utf8SliceToString(TConstArrayView<uint8> Bytes, int32 Begin, int32 End)
{
	const FUTF8ToTCHAR Converted(
		reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Begin),
		End - Begin);
	return FString(Converted.Length(), Converted.Get());
}

bool IsBlankLine(TConstArrayView<uint8> Bytes, const FLine& Line)
{
	int32 Begin = Line.Start;
	int32 End = Line.End;
	TrimHorizontal(Bytes, Begin, End);
	return Begin == End;
}

bool IsEndMarker(TConstArrayView<uint8> Bytes, const FLine& Line)
{
	int32 Begin = Line.Start;
	int32 End = Line.End;
	TrimHorizontal(Bytes, Begin, End);
	return EqualsAscii(Bytes, Begin, End, "/** @end */");
}

bool ParseHeader(
	TConstArrayView<uint8> Bytes,
	TConstArrayView<FLine> Lines,
	int32& InOutLineIndex,
	FStringView FileTag,
	const TArray<int32>& RawOffsets,
	FHeader& OutHeader,
	TArray<FAngelscriptTestError>& OutErrors)
{
	if (!Lines.IsValidIndex(InOutLineIndex))
	{
		OutErrors.Add(MakeParserError(
			TEXT("MissingMetadataHeader"),
			TEXT("Expected a Doxygen metadata header."),
			FileTag,
			Lines.Num() + 1,
			RawOffsets.Last()));
		return false;
	}

	const FLine& OpeningLine = Lines[InOutLineIndex];
	int32 OpeningBegin = OpeningLine.Start;
	int32 OpeningEnd = OpeningLine.End;
	TrimHorizontal(Bytes, OpeningBegin, OpeningEnd);
	if (!EqualsAscii(Bytes, OpeningBegin, OpeningEnd, "/**"))
	{
		OutErrors.Add(MakeParserError(
			TEXT("MalformedMetadataHeader"),
			TEXT("A metadata header must begin with a standalone '/**' line."),
			FileTag,
			OpeningLine.Number,
			RawOffsets[OpeningBegin]));
		return false;
	}

	++InOutLineIndex;
	for (; Lines.IsValidIndex(InOutLineIndex); ++InOutLineIndex)
	{
		const FLine& Line = Lines[InOutLineIndex];
		int32 Begin = Line.Start;
		int32 End = Line.End;
		TrimHorizontal(Bytes, Begin, End);
		if (EqualsAscii(Bytes, Begin, End, "*/"))
		{
			++InOutLineIndex;
			return true;
		}

		if (Begin < End && Bytes[Begin] == static_cast<uint8>('*'))
		{
			++Begin;
			if (Begin < End && Bytes[Begin] == static_cast<uint8>(' '))
			{
				++Begin;
			}
		}
		while (Begin < End && (Bytes[Begin] == static_cast<uint8>(' ')
			|| Bytes[Begin] == static_cast<uint8>('\t')))
		{
			++Begin;
		}

		if (Begin >= End)
		{
			continue;
		}
		if (Bytes[Begin] != static_cast<uint8>('@'))
		{
			continue;
		}

		int32 DirectiveEnd = Begin + 1;
		while (DirectiveEnd < End
			&& Bytes[DirectiveEnd] != static_cast<uint8>(' ')
			&& Bytes[DirectiveEnd] != static_cast<uint8>('\t'))
		{
			++DirectiveEnd;
		}
		int32 ValueBegin = DirectiveEnd;
		while (ValueBegin < End && (Bytes[ValueBegin] == static_cast<uint8>(' ')
			|| Bytes[ValueBegin] == static_cast<uint8>('\t')))
		{
			++ValueBegin;
		}
		int32 ValueEnd = End;
		TrimHorizontal(Bytes, ValueBegin, ValueEnd);
		const FString Directive = Utf8SliceToString(Bytes, Begin + 1, DirectiveEnd);
		const FString Value = Utf8SliceToString(Bytes, ValueBegin, ValueEnd);

		if (Directive == TEXT("version"))
		{
			if (OutHeader.bSawVersion)
			{
				OutErrors.Add(MakeParserError(
					TEXT("DuplicateMetadataDirective"),
					TEXT("@version may appear only once in a metadata header."),
					FileTag,
					Line.Number,
					RawOffsets[Begin]));
			}
			else
			{
				OutHeader.Version = Value;
				OutHeader.bSawVersion = true;
			}
		}
		else if (Directive == TEXT("begin"))
		{
			if (OutHeader.bSawBegin)
			{
				OutErrors.Add(MakeParserError(
					TEXT("DuplicateMetadataDirective"),
					TEXT("@begin may appear only once in a metadata header."),
					FileTag,
					Line.Number,
					RawOffsets[Begin]));
			}
			else
			{
				OutHeader.Begin = Value;
				OutHeader.bSawBegin = true;
			}
		}
		else if (Directive == TEXT("parent"))
		{
			if (OutHeader.bSawParent)
			{
				OutErrors.Add(MakeParserError(
					TEXT("DuplicateMetadataDirective"),
					TEXT("@parent may appear only once in a metadata header."),
					FileTag,
					Line.Number,
					RawOffsets[Begin]));
			}
			else
			{
				OutHeader.Parent = Value;
				OutHeader.bSawParent = true;
			}
		}
		else if (Directive == TEXT("summary"))
		{
			if (OutHeader.bSawSummary)
			{
				OutErrors.Add(MakeParserError(
					TEXT("DuplicateMetadataDirective"),
					TEXT("@summary may appear only once in a metadata header."),
					FileTag,
					Line.Number,
					RawOffsets[Begin]));
			}
			else
			{
				OutHeader.Summary = Value;
				OutHeader.bSawSummary = true;
			}
		}
		else if (Directive == TEXT("topic"))
		{
			OutHeader.Topics.Add(Value);
		}
		else
		{
			OutErrors.Add(MakeParserError(
				TEXT("UnknownMetadataDirective"),
				TEXT("The metadata directive is not supported by v1."),
				FileTag,
				Line.Number,
				RawOffsets[Begin]));
		}
	}

	OutErrors.Add(MakeParserError(
		TEXT("UnterminatedMetadataHeader"),
		TEXT("A metadata header must end with a standalone '*/' line."),
		FileTag,
		OpeningLine.Number,
		RawOffsets[OpeningBegin]));
	return false;
}

}

FAngelscriptTestCodeBuildResult FAngelscriptTestSourceParser::Parse(
	FStringView FileTag,
	TConstArrayView<uint8> RawBytes)
{
	const int32 BomLength = RawBytes.Num() >= 3
		&& RawBytes[0] == 0xef && RawBytes[1] == 0xbb && RawBytes[2] == 0xbf
		? 3
		: 0;
	int32 InvalidUtf8Offset = INDEX_NONE;
	if (!IsValidUtf8(RawBytes, BomLength, InvalidUtf8Offset))
	{
		TArray<FAngelscriptTestError> Errors;
		Errors.Add(MakeParserError(
			TEXT("InvalidUtf8"),
			TEXT("The v1 source container must be valid UTF-8 text."),
			FileTag,
			1,
			InvalidUtf8Offset));
		return FAngelscriptTestCodeBuildResult(MoveTemp(Errors));
	}

	const FNormalizedContainer Container = NormalizeContainer(RawBytes, BomLength);
	const TArray<FLine> Lines = SplitLines(Container.Bytes);
	TArray<FAngelscriptTestError> Errors;
	int32 LineIndex = 0;
	while (Lines.IsValidIndex(LineIndex) && IsBlankLine(Container.Bytes, Lines[LineIndex]))
	{
		++LineIndex;
	}

	FHeader FileHeader;
	if (!ParseHeader(
		Container.Bytes,
		Lines,
		LineIndex,
		FileTag,
		Container.RawOffsets,
		FileHeader,
		Errors))
	{
		return FAngelscriptTestCodeBuildResult(MoveTemp(Errors));
	}
	if (!FileHeader.bSawVersion)
	{
		Errors.Add(MakeParserError(
			TEXT("MissingFileVersion"),
			TEXT("The file header requires @version v1."),
			FileTag,
			1,
			Container.RawOffsets[0]));
	}
	if (!FileHeader.bSawSummary)
	{
		Errors.Add(MakeParserError(
			TEXT("MissingFileSummary"),
			TEXT("The file header requires @summary."),
			FileTag,
			1,
			Container.RawOffsets[0]));
	}
	if (FileHeader.bSawParent)
	{
		Errors.Add(MakeParserError(
			TEXT("FileParentForbidden"),
			TEXT("The file header cannot declare @parent."),
			FileTag,
			1,
			Container.RawOffsets[0]));
	}
	if (FileHeader.bSawBegin)
	{
		Errors.Add(MakeParserError(
			TEXT("FileBeginForbidden"),
			TEXT("The file header cannot declare @begin."),
			FileTag,
			1,
			Container.RawOffsets[0]));
	}

	FAngelscriptTestFileMeta FileMeta{
		.Version = MoveTemp(FileHeader.Version),
		.Tag = FString(FileTag),
		.Summary = MoveTemp(FileHeader.Summary),
		.Topics = MoveTemp(FileHeader.Topics),
	};
	FAngelscriptTestCodeBuilder Builder(FileMeta);
	struct FVersionLocation
	{
		int32 SourceLine = 0;
		int32 ByteOffset = 0;
	};
	TMap<FString, FVersionLocation> VersionLocations;

	while (true)
	{
		while (Lines.IsValidIndex(LineIndex) && IsBlankLine(Container.Bytes, Lines[LineIndex]))
		{
			++LineIndex;
		}
		if (!Lines.IsValidIndex(LineIndex))
		{
			break;
		}

		FHeader VersionHeader;
		const int32 HeaderLineIndex = LineIndex;
		if (!ParseHeader(
			Container.Bytes,
			Lines,
			LineIndex,
			FileTag,
			Container.RawOffsets,
			VersionHeader,
			Errors))
		{
			break;
		}
		if (VersionHeader.bSawBegin && VersionHeader.bSawVersion)
		{
			Errors.Add(MakeParserError(
				TEXT("CaseIdentityConflict"),
				TEXT("A case header uses @begin or @version, not both."),
				FileTag,
				Lines[HeaderLineIndex].Number,
				Container.RawOffsets[Lines[HeaderLineIndex].Start]));
		}
		if (!VersionHeader.bSawBegin && !VersionHeader.bSawVersion)
		{
			Errors.Add(MakeParserError(
				TEXT("MissingVersionTag"),
				TEXT("A version header requires @begin or @version."),
				FileTag,
				Lines[HeaderLineIndex].Number,
				Container.RawOffsets[Lines[HeaderLineIndex].Start]));
		}
		if (VersionHeader.bSawBegin)
		{
			VersionHeader.Version = VersionHeader.Begin;
		}
		if (!VersionHeader.bSawSummary)
		{
			Errors.Add(MakeParserError(
				TEXT("MissingVersionSummary"),
				TEXT("A version header requires @summary."),
				FileTag,
				Lines[HeaderLineIndex].Number,
				Container.RawOffsets[Lines[HeaderLineIndex].Start],
				VersionHeader.Version));
		}

		const int32 BodyLine = Lines.IsValidIndex(LineIndex)
			? Lines[LineIndex].Number
			: Lines.Num() + 1;
		const int32 BodyBegin = Lines.IsValidIndex(LineIndex)
			? Lines[LineIndex].Start
			: Container.Bytes.Num();
		int32 EndMarkerLineIndex = LineIndex;
		while (Lines.IsValidIndex(EndMarkerLineIndex)
			&& !IsEndMarker(Container.Bytes, Lines[EndMarkerLineIndex]))
		{
			++EndMarkerLineIndex;
		}
		if (!Lines.IsValidIndex(EndMarkerLineIndex))
		{
			Errors.Add(MakeParserError(
				TEXT("MissingVersionEnd"),
				TEXT("Every version body must end with a standalone '/** @end */' line."),
				FileTag,
				BodyLine,
				Container.RawOffsets[BodyBegin],
				VersionHeader.Version));
			break;
		}

		const int32 BodyEnd = Lines[EndMarkerLineIndex].Start;
		TArray<uint8> BodyBytes;
		TArray<int32> BodyOffsets;
		BodyBytes.Reserve(BodyEnd - BodyBegin);
		BodyOffsets.Reserve(BodyEnd - BodyBegin + 1);
		for (int32 Index = BodyBegin; Index < BodyEnd; ++Index)
		{
			BodyBytes.Add(Container.Bytes[Index]);
			BodyOffsets.Add(Container.RawOffsets[Index]);
		}
		BodyOffsets.Add(Container.RawOffsets[BodyEnd]);
		FAngelscriptTestSourceOriginMap BodyOriginMap;
		BodyOriginMap.HostFile = FString(FileTag) + TEXT(".as");
		BodyOriginMap.HostLine = BodyLine;
		BodyOriginMap.OriginalByteOffsets = MoveTemp(BodyOffsets);
		FAngelscriptTestSource BodySource = FAngelscriptTestSource::FromParsedData(
			MoveTemp(BodyBytes), FAngelscriptTestAnnotations(), MoveTemp(BodyOriginMap));
		auto ParsedSource = FAngelscriptTestAnnotations::Parse(BodySource);
		if (ParsedSource.IsSuccess())
		{
			BodySource = *ParsedSource.GetSource();
		}
		else
		{
			for (FAngelscriptTestError AnnotationError : ParsedSource.GetErrors())
			{
				AnnotationError.FileTag = FString(FileTag);
				AnnotationError.VersionTag = VersionHeader.Version;
				Errors.Add(MoveTemp(AnnotationError));
			}
		}

		FVersionLocation& Location = VersionLocations.FindOrAdd(VersionHeader.Version);
		Location.SourceLine = BodyLine;
		Location.ByteOffset = Container.RawOffsets.IsValidIndex(BodyBegin)
			? Container.RawOffsets[BodyBegin]
			: 0;

		FAngelscriptTestVersionMeta VersionMeta{
			.Tag = MoveTemp(VersionHeader.Version),
			.Parent = MoveTemp(VersionHeader.Parent),
			.Summary = MoveTemp(VersionHeader.Summary),
			.Topics = MoveTemp(VersionHeader.Topics),
		};
		Builder.AddVersion(MoveTemp(VersionMeta), MoveTemp(BodySource));
		LineIndex = EndMarkerLineIndex + 1;
	}

	auto BuildResult = Builder.Build();
	if (!BuildResult.IsSuccess())
	{
		Errors.Append(BuildResult.GetErrors());
	}
	for (FAngelscriptTestError& Error : Errors)
	{
		if (Error.Code == TEXT("VersionCycle")
			|| Error.Code == TEXT("MissingRoot")
			|| Error.Code == TEXT("MultipleRoots"))
		{
			Error.SourcePath = FString(FileTag) + TEXT(".as");
			Error.SourceLine = 1;
			Error.ByteOffset = 0;
		}
		else if (Error.Code == TEXT("DuplicateVersionTag")
			|| Error.Code == TEXT("MissingParent"))
		{
			Error.SourcePath = FString(FileTag) + TEXT(".as");
			if (const FVersionLocation* Location = VersionLocations.Find(Error.VersionTag))
			{
				Error.SourceLine = Location->SourceLine;
				Error.ByteOffset = Location->ByteOffset;
			}
		}
	}
	if (!Errors.IsEmpty())
	{
		return FAngelscriptTestCodeBuildResult(MoveTemp(Errors));
	}
	return BuildResult;
}
}
