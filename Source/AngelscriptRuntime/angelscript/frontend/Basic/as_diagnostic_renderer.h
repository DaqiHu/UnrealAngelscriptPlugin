/*
   Maintained Unreal AngelScript fork — diagnostic presentation adapters.
*/

#pragma once

#include "as_config.h"

#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"

BEGIN_AS_NAMESPACE

struct asSDiagnosticRecord;

class asCDiagnosticResult;

class ANGELSCRIPTRUNTIME_API asCDiagnosticRenderer
{
public:
	static FString Render(FStringView Format, const asSDiagnosticRecord& Record);
	static FString SerializeStable(TConstArrayView<asSDiagnosticRecord> Records);
	FString Format(const asCDiagnosticResult& Result, int32 GroupIndex) const;
	FString SerializeJson(const asCDiagnosticResult& Result) const;
};

END_AS_NAMESPACE
