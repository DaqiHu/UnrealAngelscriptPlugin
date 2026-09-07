/* Maintained Unreal AngelScript fork — bounded canonical AST to symbolic bytecode. */
#pragma once

#include "as_bytecode_image_builder.h"
#include "as_builder.h"
#include "frontend/as_diagnostics.h"
#include "frontend/as_stable_key.h"

BEGIN_AS_NAMESPACE

class asCCompilationSession;
class asCMetadataImage;

enum class asEByteCodeEmissionStatus : asBYTE
{
	Succeeded = 0,
	NotReady,
	InvalidInput,
	UnsupportedLowering,
	ResourceLimit
};

struct asSByteCodeEmissionOptions
{
	asUINT PointerByteSize = sizeof(void*);
	asUINT MaxFrameDWords = 64;
	asUINT MaxInstructions = 65535;
	asUINT MaxStrings = 4096;
	asUINT MaxFunctions = 4096;
	bool bFloatIsFloat64 = true;
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
	TUniquePtr<asCByteCodeImage> Image;
	asEByteCodeImageStatus Status = asEByteCodeImageStatus::InvalidArgument;
	asEByteCodeEmissionStatus EmissionStatus = asEByteCodeEmissionStatus::InvalidInput;
	asSByteCodeEmissionFailure Failure;
};

class ANGELSCRIPTRUNTIME_API asCByteCodeEmitter
{
public:
	static asSByteCodeEmissionResult Emit(
		const asCCompilationSession* Session,
		const asCMetadataImage* FrozenDefinitions,
		const asSByteCodeEmissionOptions& Options);
	static asSByteCodeEmissionResult Emit(asCBuilder& Builder, asCScriptFunction& Function);
};

END_AS_NAMESPACE
