/* Maintained Unreal AngelScript fork — request-local cursor parse context. */
#pragma once

#include "frontend/Sema/as_tooling_types.h"

BEGIN_AS_NAMESPACE

struct ANGELSCRIPTRUNTIME_API asSCursorContext
{
	asEAnalysisStatus Status = asEAnalysisStatus::Unavailable;
	TArray<FString> VisibleNames;
	TArray<asSCursorVisibleSymbol> VisibleSymbols;
	FString ExpectedTypeName;
	FString ReceiverTypeName;
	bool bReceiverReadOnly = false;
	FString InnermostCallName;
	TArray<FString> CallArgumentNames;
	TArray<FString> CallArgumentTypeNames;
	TArray<bool> CallArgumentIsInitList;
	int32 ActiveParameter = INDEX_NONE;
	bool bInnermostCallArgumentIsInitList = false;
	asCSourceRange Replacement;

	bool IsNameVisible(const FString& Name) const
	{
		return VisibleNames.Contains(Name);
	}
};

END_AS_NAMESPACE
