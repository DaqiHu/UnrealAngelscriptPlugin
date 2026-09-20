#pragma once

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace AngelscriptTest::Generate::TestSupport
{
inline FString GoldPath(const TCHAR* Relative)
{
	return FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("Angelscript/Source/AngelscriptTest/FrameworkTests/Gold"),
		Relative);
}

inline FString LoadGold(const TCHAR* Relative)
{
	FString Gold;
	FFileHelper::LoadFileToString(Gold, *GoldPath(Relative));
	return Gold;
}

inline int32 CountOccurrences(const FString& Haystack, const FString& Needle)
{
	int32 Count = 0;
	for (int32 SearchFrom = 0;
		(SearchFrom = Haystack.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom)) != INDEX_NONE;
		++SearchFrom)
	{
		++Count;
	}
	return Count;
}

inline bool HasTrailingWhitespace(const FString& Source)
{
	for (int32 Index = 0; Index < Source.Len(); ++Index)
	{
		if ((Source[Index] == TEXT(' ') || Source[Index] == TEXT('\t'))
			&& Index + 1 < Source.Len()
			&& (Source[Index + 1] == TEXT('\n') || Source[Index + 1] == TEXT('\r')))
		{
			return true;
		}
	}
	return Source.EndsWith(TEXT(" ")) || Source.EndsWith(TEXT("\t"));
}

inline bool ExtractFunction(const FString& Source, const FString& FunctionName, FString& OutBody)
{
	const FString Header = FString::Printf(TEXT("int %s()"), *FunctionName);
	int32 Start = INDEX_NONE;
	if (Source.StartsWith(Header))
	{
		Start = 0;
	}
	else
	{
		const FString Anchored = TEXT("\n") + Header;
		const int32 Found = Source.Find(Anchored, ESearchCase::CaseSensitive);
		if (Found == INDEX_NONE)
		{
			return false;
		}
		Start = Found + 1;
	}

	const int32 Open = Source.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
	if (Open == INDEX_NONE)
	{
		return false;
	}
	int32 Depth = 0;
	int32 Close = INDEX_NONE;
	for (int32 Index = Open; Index < Source.Len(); ++Index)
	{
		if (Source[Index] == TEXT('{'))
		{
			++Depth;
		}
		else if (Source[Index] == TEXT('}'))
		{
			--Depth;
			if (Depth == 0)
			{
				Close = Index;
				break;
			}
		}
	}
	if (Close == INDEX_NONE)
	{
		return false;
	}
	OutBody = Source.Mid(Start, Close + 1 - Start);
	return true;
}
}
