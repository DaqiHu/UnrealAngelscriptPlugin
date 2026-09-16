/* Maintained Unreal AngelScript fork — bytecode emission status and failure facts. */
#pragma once

#include "frontend/Basic/as_diagnostics.h"
#include "frontend/Basic/as_stable_key.h"

BEGIN_AS_NAMESPACE

enum class asEByteCodeEmissionStatus : asBYTE
{
	Succeeded = 0,
	NotReady,
	InvalidInput,
	UnsupportedLowering,
	ResourceLimit
};

struct asSByteCodeEmissionFailure
{
	asSStableKey FunctionKey;
	uint16 NodeKind = 0;
	int32 Line = 0;
	int32 Column = 0;
	FString StableSourceKey;
	FString StableFragmentKey;
	TArray<asSDiagnosticRecord> Diagnostics;
};

struct asSByteCodeEmissionResult
{
	asEByteCodeEmissionStatus EmissionStatus = asEByteCodeEmissionStatus::InvalidInput;
	asSByteCodeEmissionFailure Failure;
};

END_AS_NAMESPACE
