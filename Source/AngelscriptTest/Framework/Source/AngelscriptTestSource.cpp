#include "Framework/Source/AngelscriptTestSource.h"
#include "Framework/Source/AngelscriptTestSourceTypes.h"

#include "Containers/StringConv.h"

namespace AngelscriptTest
{
namespace
{
bool IsHorizontalWhitespace(uint8 Byte)
{
	return Byte == static_cast<uint8>(' ') || Byte == static_cast<uint8>('\t');
}

bool IsHorizontalWhitespaceOnly(TConstArrayView<uint8> Bytes, int32 Begin, int32 End)
{
	for (int32 Index = Begin; Index < End; ++Index)
	{
		if (!IsHorizontalWhitespace(Bytes[Index]))
		{
			return false;
		}
	}

	return true;
}

int32 GetLineBreakLength(TConstArrayView<uint8> Bytes, int32 Index, int32 End)
{
	if (Index >= End)
	{
		return 0;
	}

	if (Bytes[Index] == static_cast<uint8>('\r'))
	{
		return Index + 1 < End && Bytes[Index + 1] == static_cast<uint8>('\n') ? 2 : 1;
	}

	return Bytes[Index] == static_cast<uint8>('\n') ? 1 : 0;
}

void CopyExactBytes(
	TConstArrayView<uint8> Bytes,
	TArray<uint8>& OutBytes,
	TArray<int32>& OutOriginalByteOffsets)
{
	OutBytes.Reserve(Bytes.Num());
	OutOriginalByteOffsets.Reserve(Bytes.Num() + 1);
	for (int32 Index = 0; Index < Bytes.Num(); ++Index)
	{
		OutBytes.Add(Bytes[Index]);
		OutOriginalByteOffsets.Add(Index);
	}
	OutOriginalByteOffsets.Add(Bytes.Num());
}

void FindLiteralEnvelope(TConstArrayView<uint8> Bytes, int32& OutBegin, int32& OutEnd)
{
	OutBegin = 0;
	OutEnd = Bytes.Num();

	int32 FirstLineEnd = 0;
	while (FirstLineEnd < OutEnd && GetLineBreakLength(Bytes, FirstLineEnd, OutEnd) == 0)
	{
		++FirstLineEnd;
	}

	const int32 OpeningLineBreakLength = GetLineBreakLength(Bytes, FirstLineEnd, OutEnd);
	const bool bRemovedOpeningEnvelope = OpeningLineBreakLength > 0
		&& IsHorizontalWhitespaceOnly(Bytes, 0, FirstLineEnd);
	if (bRemovedOpeningEnvelope)
	{
		OutBegin = FirstLineEnd + OpeningLineBreakLength;
	}

	int32 LastLineBreak = INDEX_NONE;
	int32 LastLineStart = OutBegin;
	for (int32 Index = OutBegin; Index < OutEnd;)
	{
		const int32 LineBreakLength = GetLineBreakLength(Bytes, Index, OutEnd);
		if (LineBreakLength == 0)
		{
			++Index;
			continue;
		}

		LastLineBreak = Index;
		Index += LineBreakLength;
		LastLineStart = Index;
	}

	if (LastLineBreak != INDEX_NONE
		&& IsHorizontalWhitespaceOnly(Bytes, LastLineStart, OutEnd))
	{
		OutEnd = LastLineBreak;
	}
	else if (bRemovedOpeningEnvelope
		&& IsHorizontalWhitespaceOnly(Bytes, OutBegin, OutEnd))
	{
		// A completely empty multiline literal has one line break followed by
		// only the indentation that places its closing delimiter.
		OutEnd = OutBegin;
	}
}

int32 FindCommonIndent(TConstArrayView<uint8> Bytes, int32 Begin, int32 End, int32& OutPrefixStart)
{
	OutPrefixStart = INDEX_NONE;
	int32 CommonLength = 0;
	bool bFoundContentLine = false;

	for (int32 LineStart = Begin; LineStart <= End;)
	{
		int32 LineEnd = LineStart;
		while (LineEnd < End && GetLineBreakLength(Bytes, LineEnd, End) == 0)
		{
			++LineEnd;
		}

		int32 IndentLength = 0;
		while (LineStart + IndentLength < LineEnd
			&& IsHorizontalWhitespace(Bytes[LineStart + IndentLength]))
		{
			++IndentLength;
		}

		if (LineStart + IndentLength < LineEnd)
		{
			if (!bFoundContentLine)
			{
				OutPrefixStart = LineStart;
				CommonLength = IndentLength;
				bFoundContentLine = true;
			}
			else
			{
				CommonLength = FMath::Min(CommonLength, IndentLength);
				int32 MatchingLength = 0;
				while (MatchingLength < CommonLength
					&& Bytes[OutPrefixStart + MatchingLength] == Bytes[LineStart + MatchingLength])
				{
					++MatchingLength;
				}
				CommonLength = MatchingLength;
			}
		}

		if (LineEnd >= End)
		{
			break;
		}
		LineStart = LineEnd + GetLineBreakLength(Bytes, LineEnd, End);
	}

	return CommonLength;
}

bool StartsWithIndent(
	TConstArrayView<uint8> Bytes,
	int32 LineStart,
	int32 LineEnd,
	int32 PrefixStart,
	int32 PrefixLength)
{
	if (PrefixLength == 0 || LineEnd - LineStart < PrefixLength)
	{
		return PrefixLength == 0;
	}

	for (int32 Index = 0; Index < PrefixLength; ++Index)
	{
		if (Bytes[LineStart + Index] != Bytes[PrefixStart + Index])
		{
			return false;
		}
	}

	return true;
}

void CopyNormalizedLiteral(
	TConstArrayView<uint8> Bytes,
	TArray<uint8>& OutBytes,
	TArray<int32>& OutOriginalByteOffsets)
{
	int32 Begin = 0;
	int32 End = 0;
	FindLiteralEnvelope(Bytes, Begin, End);

	int32 PrefixStart = INDEX_NONE;
	const int32 PrefixLength = FindCommonIndent(Bytes, Begin, End, PrefixStart);
	OutBytes.Reserve(End - Begin);
	OutOriginalByteOffsets.Reserve(End - Begin + 1);

	for (int32 LineStart = Begin; LineStart < End;)
	{
		int32 LineEnd = LineStart;
		while (LineEnd < End && GetLineBreakLength(Bytes, LineEnd, End) == 0)
		{
			++LineEnd;
		}

		const int32 ContentStart = StartsWithIndent(
			Bytes, LineStart, LineEnd, PrefixStart, PrefixLength)
			? LineStart + PrefixLength
			: LineStart;
		for (int32 Index = ContentStart; Index < LineEnd; ++Index)
		{
			OutBytes.Add(Bytes[Index]);
			OutOriginalByteOffsets.Add(Index);
		}

		const int32 LineBreakLength = GetLineBreakLength(Bytes, LineEnd, End);
		for (int32 Index = 0; Index < LineBreakLength; ++Index)
		{
			OutBytes.Add(Bytes[LineEnd + Index]);
			OutOriginalByteOffsets.Add(LineEnd + Index);
		}
		LineStart = LineEnd + LineBreakLength;
	}

	OutOriginalByteOffsets.Add(End);
}
}

struct FAngelscriptTestSource::FData
{
	TArray<uint8> Bytes;
	FAngelscriptTestAnnotations Annotations;
	FAngelscriptTestSourceOriginMap OriginMap;
};

FAngelscriptTestStatus::FAngelscriptTestStatus(TArray<FAngelscriptTestError> InErrors)
	: Errors(MoveTemp(InErrors))
{
}

FAngelscriptTestStatus FAngelscriptTestStatus::Success()
{
	return FAngelscriptTestStatus({});
}

FAngelscriptTestStatus FAngelscriptTestStatus::Failure(FAngelscriptTestError Error)
{
	TArray<FAngelscriptTestError> Errors;
	Errors.Add(MoveTemp(Error));
	return FAngelscriptTestStatus(MoveTemp(Errors));
}

FAngelscriptTestStatus FAngelscriptTestStatus::Failure(TArray<FAngelscriptTestError> Errors)
{
	return FAngelscriptTestStatus(MoveTemp(Errors));
}

bool FAngelscriptTestStatus::IsSuccess() const
{
	return Errors.IsEmpty();
}

const TArray<FAngelscriptTestError>& FAngelscriptTestStatus::GetErrors() const &
{
	return Errors;
}

FStringView FAngelscriptTestSourceOriginMap::GetHostFile() const &
{
	return HostFile;
}

int32 FAngelscriptTestSourceOriginMap::GetHostLine() const
{
	return HostLine;
}

int32 FAngelscriptTestSourceOriginMap::MapToOriginalByteOffset(int32 SourceByteOffset) const
{
	return OriginalByteOffsets.IsValidIndex(SourceByteOffset)
		? OriginalByteOffsets[SourceByteOffset]
		: INDEX_NONE;
}

FAngelscriptTestSource::FAngelscriptTestSource(TSharedRef<const FData, ESPMode::ThreadSafe> InData)
	: Data(MoveTemp(InData))
{
}

FAngelscriptTestSource FAngelscriptTestSource::FromBytes(TConstArrayView<uint8> Bytes)
{
	TSharedRef<FData, ESPMode::ThreadSafe> NewData = MakeShared<FData, ESPMode::ThreadSafe>();
	CopyExactBytes(Bytes, NewData->Bytes, NewData->OriginMap.OriginalByteOffsets);
	return FAngelscriptTestSource(NewData);
}

FAngelscriptTestSource FAngelscriptTestSource::FromLiteral(
	TConstArrayView<uint8> Bytes,
	const ANSICHAR* HostFile,
	int32 HostLine,
	bool bNormalize)
{
	TSharedRef<FData, ESPMode::ThreadSafe> NewData = MakeShared<FData, ESPMode::ThreadSafe>();
	NewData->OriginMap.HostFile = HostFile != nullptr ? FString(UTF8_TO_TCHAR(HostFile)) : FString();
	NewData->OriginMap.HostLine = HostLine;

	if (bNormalize)
	{
		CopyNormalizedLiteral(Bytes, NewData->Bytes, NewData->OriginMap.OriginalByteOffsets);
	}
	else
	{
		CopyExactBytes(Bytes, NewData->Bytes, NewData->OriginMap.OriginalByteOffsets);
	}

	return FAngelscriptTestSource(NewData);
}

FAngelscriptTestSource FAngelscriptTestSource::FromParsedData(
	TArray<uint8> Bytes,
	FAngelscriptTestAnnotations Annotations,
	FAngelscriptTestSourceOriginMap OriginMap)
{
	TSharedRef<FData, ESPMode::ThreadSafe> NewData = MakeShared<FData, ESPMode::ThreadSafe>();
	NewData->Bytes = MoveTemp(Bytes);
	NewData->Annotations = MoveTemp(Annotations);
	NewData->OriginMap = MoveTemp(OriginMap);
	return FAngelscriptTestSource(NewData);
}

TConstArrayView<uint8> FAngelscriptTestSource::GetBytes() const &
{
	return Data->Bytes;
}

const FAngelscriptTestAnnotations& FAngelscriptTestSource::GetAnnotations() const &
{
	return Data->Annotations;
}

const FAngelscriptTestSourceOriginMap& FAngelscriptTestSource::GetOriginMap() const &
{
	return Data->OriginMap;
}
}
