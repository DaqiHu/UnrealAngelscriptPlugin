#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"

namespace AngelscriptTest::Generate
{
enum class EGeneratedCaseOutcome : uint8
{
	ReturnValue,
	CompileReject,
	RuntimeException,
};

enum class EGeneratedCaseExecution : uint8
{
	Standalone,
	RequiresHostSetup,
	SourceOnly,
};

struct FGeneratedCaseInfo
{
	FString CaseId;
	FString EntryDeclaration;
	EGeneratedCaseOutcome Outcome = EGeneratedCaseOutcome::ReturnValue;
	EGeneratedCaseExecution Execution = EGeneratedCaseExecution::SourceOnly;
	TOptional<int32> ExpectedReturn;
	FString ExpectedException;
	bool bLimitedObservation = false;
	FString ExecutionNotes;
};

// Naming assumed: MakeCanonicalEntryName — shared CaseId-to-Entry conversion used by every product.
inline FString MakeCanonicalEntryName(const FStringView CaseId)
{
	FString Result(TEXT("Entry"));
	FString Token;
	auto FlushToken = [&Result, &Token]()
	{
		if (Token.IsEmpty())
		{
			return;
		}
		Token[0] = FChar::ToUpper(Token[0]);
		for (int32 Index = 1; Index < Token.Len(); ++Index)
		{
			Token[Index] = FChar::ToLower(Token[Index]);
		}
		Result += Token;
		Token.Reset();
	};

	for (const TCHAR Character : CaseId)
	{
		if (Character == TEXT('-') || Character == TEXT('_'))
		{
			FlushToken();
		}
		else
		{
			Token.AppendChar(Character);
		}
	}
	FlushToken();
	return Result;
}

inline bool IsValidGeneratedFunctionName(const FStringView FunctionName)
{
	if (FunctionName.IsEmpty())
	{
		return false;
	}
	const TCHAR First = FunctionName[0];
	if (!FChar::IsAlpha(First) && First != TEXT('_'))
	{
		return false;
	}
	for (int32 Index = 1; Index < FunctionName.Len(); ++Index)
	{
		const TCHAR Character = FunctionName[Index];
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			return false;
		}
	}
	return true;
}
}
