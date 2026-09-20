#include "Framework/Source/AngelscriptTestAnnotations.h"
#include "Framework/Source/AngelscriptTestSource.h"
#include "Framework/Source/AngelscriptTestSourceParseResult.h"

namespace AngelscriptTest
{
namespace
{
struct FOpenRange
{
	FString Name;
	int32 Begin = 0;
};

bool Matches(TConstArrayView<uint8> Bytes, int32 Offset, const ANSICHAR* Text)
{
	const int32 Length = FCStringAnsi::Strlen(Text);
	return Offset >= 0 && Offset + Length <= Bytes.Num()
		&& FMemory::Memcmp(Bytes.GetData() + Offset, Text, Length) == 0;
}

int32 FindMarkerEnd(TConstArrayView<uint8> Bytes, int32 Begin)
{
	for (int32 Index = Begin; Index + 3 <= Bytes.Num(); ++Index)
	{
		if (Matches(Bytes, Index, " */"))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FAngelscriptTestError MakeAnnotationError(
	FStringView Code,
	FStringView Message,
	const FAngelscriptTestSource& Source,
	int32 InputByteOffset)
{
	return {
		.Code = FString(Code),
		.Message = FString(Message),
		.SourcePath = FString(Source.GetOriginMap().GetHostFile()),
		.SourceLine = Source.GetOriginMap().GetHostLine(),
		.ByteOffset = Source.GetOriginMap().MapToOriginalByteOffset(InputByteOffset),
	};
}

bool ParseMarkerWords(
	TConstArrayView<uint8> Bytes,
	int32 Begin,
	int32 End,
	FString& OutDirective,
	FString& OutName)
{
	int32 Separator = INDEX_NONE;
	for (int32 Index = Begin; Index < End; ++Index)
	{
		if (Bytes[Index] == static_cast<uint8>(' '))
		{
			Separator = Index;
			break;
		}
		if (Bytes[Index] < 0x21 || Bytes[Index] > 0x7e)
		{
			return false;
		}
	}

	if (Separator <= Begin || Separator + 1 >= End)
	{
		return false;
	}
	for (int32 Index = Separator + 1; Index < End; ++Index)
	{
		if (Bytes[Index] < 0x21 || Bytes[Index] > 0x7e)
		{
			return false;
		}
	}

	OutDirective.Reset();
	OutDirective.Reserve(Separator - Begin);
	OutName.Reset();
	OutName.Reserve(End - Separator - 1);
	for (int32 Index = Begin; Index < Separator; ++Index)
	{
		OutDirective.AppendChar(static_cast<TCHAR>(Bytes[Index]));
	}
	for (int32 Index = Separator + 1; Index < End; ++Index)
	{
		OutName.AppendChar(static_cast<TCHAR>(Bytes[Index]));
	}
	return true;
}

void CopyMappedByte(
	const FAngelscriptTestSource& Source,
	int32 InputOffset,
	TArray<uint8>& OutBytes,
	TArray<int32>& OutOffsets)
{
	OutBytes.Add(Source.GetBytes()[InputOffset]);
	OutOffsets.Add(Source.GetOriginMap().MapToOriginalByteOffset(InputOffset));
}
}

FAngelscriptTestSourceParseResult FAngelscriptTestAnnotations::Parse(
	FAngelscriptTestSource Source)
{
	const TConstArrayView<uint8> Input = Source.GetBytes();
	TArray<uint8> CleanBytes;
	TArray<int32> CleanOffsets;
	CleanBytes.Reserve(Input.Num());
	CleanOffsets.Reserve(Input.Num() + 1);
	FAngelscriptTestAnnotations ParsedAnnotations;
	TArray<FAngelscriptTestError> Errors;
	TArray<FOpenRange> OpenRanges;

	for (int32 Index = 0; Index < Input.Num();)
	{
		if (Matches(Input, Index, "/** @@"))
		{
			const int32 MarkerEnd = FindMarkerEnd(Input, Index + 6);
			if (MarkerEnd == INDEX_NONE)
			{
				Errors.Add(MakeAnnotationError(
					TEXT("MalformedAnnotationMarker"),
					TEXT("Escaped annotation text must end with ' */'."),
					Source,
					Index));
				break;
			}

			const int32 AfterMarker = MarkerEnd + 3;
			for (int32 CopyIndex = Index; CopyIndex < AfterMarker; ++CopyIndex)
			{
				if (CopyIndex != Index + 5)
				{
					CopyMappedByte(Source, CopyIndex, CleanBytes, CleanOffsets);
				}
			}
			Index = AfterMarker;
			continue;
		}

		if (!Matches(Input, Index, "/** @"))
		{
			CopyMappedByte(Source, Index, CleanBytes, CleanOffsets);
			++Index;
			continue;
		}

		const int32 MarkerEnd = FindMarkerEnd(Input, Index + 5);
		if (MarkerEnd == INDEX_NONE)
		{
			Errors.Add(MakeAnnotationError(
				TEXT("MalformedAnnotationMarker"),
				TEXT("Annotation markers must end with ' */'."),
				Source,
				Index));
			break;
		}

		FString Directive;
		FString Name;
		if (!ParseMarkerWords(Input, Index + 5, MarkerEnd, Directive, Name))
		{
			Errors.Add(MakeAnnotationError(
				TEXT("MalformedAnnotationMarker"),
				TEXT("Annotation markers require one directive and one non-whitespace name."),
				Source,
				Index));
			Index = MarkerEnd + 3;
			continue;
		}

		const int32 CleanOffset = CleanBytes.Num();
		if (Directive == TEXT("point"))
		{
			if (ParsedAnnotations.Points.Contains(Name))
			{
				Errors.Add(MakeAnnotationError(
					TEXT("DuplicatePoint"),
					TEXT("Point names must be unique within one source version."),
					Source,
					Index));
			}
			else
			{
				ParsedAnnotations.Points.Add(MoveTemp(Name), CleanOffset);
			}
		}
		else if (Directive == TEXT("breakpoint"))
		{
			if (ParsedAnnotations.Breakpoints.Contains(Name))
			{
				Errors.Add(MakeAnnotationError(
					TEXT("DuplicateBreakpoint"),
					TEXT("Breakpoint names must be unique within one source version."),
					Source,
					Index));
			}
			else
			{
				ParsedAnnotations.Breakpoints.Add(MoveTemp(Name), CleanOffset);
			}
		}
		else if (Directive == TEXT("range-begin"))
		{
			const bool bDuplicate = ParsedAnnotations.Ranges.Contains(Name)
				|| OpenRanges.ContainsByPredicate([&Name](const FOpenRange& Range)
				{
					return Range.Name == Name;
				});
			if (bDuplicate)
			{
				Errors.Add(MakeAnnotationError(
					TEXT("DuplicateRange"),
					TEXT("Range names must be unique within one source version."),
					Source,
					Index));
			}
			else
			{
				OpenRanges.Add({MoveTemp(Name), CleanOffset});
			}
		}
		else if (Directive == TEXT("range-end"))
		{
			const int32 MatchingIndex = OpenRanges.IndexOfByPredicate([&Name](const FOpenRange& Range)
			{
				return Range.Name == Name;
			});
			if (MatchingIndex == INDEX_NONE)
			{
				Errors.Add(MakeAnnotationError(
					TEXT("MissingRangeBegin"),
					TEXT("A range end must match an earlier open range."),
					Source,
					Index));
			}
			else if (MatchingIndex != OpenRanges.Num() - 1)
			{
				Errors.Add(MakeAnnotationError(
					TEXT("CrossingRange"),
					TEXT("Ranges may nest but must not cross."),
					Source,
					Index));
				OpenRanges.RemoveAt(MatchingIndex);
			}
			else
			{
				const FOpenRange OpenRange = MoveTemp(OpenRanges.Last());
				OpenRanges.Pop(EAllowShrinking::No);
				ParsedAnnotations.Ranges.Add(
					OpenRange.Name,
					{OpenRange.Begin, CleanOffset});
			}
		}
		else
		{
			Errors.Add(MakeAnnotationError(
				TEXT("UnknownAnnotationDirective"),
				TEXT("The annotation directive is not supported by v1."),
				Source,
				Index));
		}

		Index = MarkerEnd + 3;
	}

	for (const FOpenRange& OpenRange : OpenRanges)
	{
		Errors.Add(MakeAnnotationError(
			TEXT("MissingRangeEnd"),
			TEXT("Every range begin must have a matching range end."),
			Source,
			Input.Num()));
	}

	if (!Errors.IsEmpty())
	{
		return FAngelscriptTestSourceParseResult(MoveTemp(Errors));
	}

	CleanOffsets.Add(Source.GetOriginMap().MapToOriginalByteOffset(Input.Num()));
	FAngelscriptTestSourceOriginMap OriginMap;
	OriginMap.HostFile = FString(Source.GetOriginMap().GetHostFile());
	OriginMap.HostLine = Source.GetOriginMap().GetHostLine();
	OriginMap.OriginalByteOffsets = MoveTemp(CleanOffsets);
	return FAngelscriptTestSourceParseResult(FAngelscriptTestSource::FromParsedData(
		MoveTemp(CleanBytes), MoveTemp(ParsedAnnotations), MoveTemp(OriginMap)));
}

int32 FAngelscriptTestAnnotations::Num() const
{
	return Points.Num() + Breakpoints.Num() + Ranges.Num();
}

const int32* FAngelscriptTestAnnotations::GetPoint(FStringView Name) const &
{
	return Points.Find(FString(Name));
}

const int32* FAngelscriptTestAnnotations::GetBreakpoint(FStringView Name) const &
{
	return Breakpoints.Find(FString(Name));
}

const FAngelscriptTestSourceRange* FAngelscriptTestAnnotations::GetRange(FStringView Name) const &
{
	return Ranges.Find(FString(Name));
}

FAngelscriptTestSourceParseResult::FAngelscriptTestSourceParseResult(
	FAngelscriptTestSource InSource)
	: Source(MoveTemp(InSource))
{
}

FAngelscriptTestSourceParseResult::FAngelscriptTestSourceParseResult(
	TArray<FAngelscriptTestError> InErrors)
	: Errors(MoveTemp(InErrors))
{
}

bool FAngelscriptTestSourceParseResult::IsSuccess() const
{
	return Source.IsSet() && Errors.IsEmpty();
}

const FAngelscriptTestSource* FAngelscriptTestSourceParseResult::GetSource() const &
{
	return Source.IsSet() ? &Source.GetValue() : nullptr;
}

const TArray<FAngelscriptTestError>& FAngelscriptTestSourceParseResult::GetErrors() const &
{
	return Errors;
}
}
