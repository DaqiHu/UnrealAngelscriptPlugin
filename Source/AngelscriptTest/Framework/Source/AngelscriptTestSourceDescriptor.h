#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

namespace AngelscriptTest
{
struct ANGELSCRIPTTEST_API FAngelscriptTestSourceDescriptor
{
	struct FPoint
	{
		FString Name;
		int32 Offset = 0;
	};

	struct FBreakpoint
	{
		FString Name;
		int32 Offset = 0;
	};

	struct FRange
	{
		FString Name;
		int32 Begin = 0;
		int32 End = 0;
	};

	struct FOriginSpan
	{
		int32 CleanBegin = 0;
		int32 AuthoredBegin = 0;
		int32 Length = 0;
	};

	struct FAnnotations
	{
		TArray<FPoint> Points;
		TArray<FBreakpoint> Breakpoints;
		TArray<FRange> Ranges;
	};

	FString AuthoredPath;
	int32 AuthoredLine = 0;
	int32 AuthoredEnd = 0;
	FAnnotations Annotations;
	TArray<FOriginSpan> OriginSpans;
};
}
