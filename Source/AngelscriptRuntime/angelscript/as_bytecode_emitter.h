/* Maintained Unreal AngelScript fork — bounded canonical AST to symbolic bytecode. */
#pragma once

#include "as_builder.h"
#include "as_bytecode_emission.h"
#include "frontend/Basic/as_diagnostics.h"
#include "frontend/Basic/as_stable_key.h"

BEGIN_AS_NAMESPACE

class asCByteCodeImage;
class asCCompilationSession;
class asCModuleDefinitionSet;

struct asSByteCodeEmissionOptions
{
	asUINT PointerByteSize = sizeof(void*);
	asUINT MaxFrameDWords = 64;
	asUINT MaxInstructions = 65535;
	asUINT MaxStrings = 4096;
	asUINT MaxFunctions = 4096;
	bool bFloatIsFloat64 = true;
};

class ANGELSCRIPTRUNTIME_API asCByteCodeEmitter
{
public:
	static asSByteCodeEmissionResult Emit(
		const asCCompilationSession* Session,
		const asCModuleDefinitionSet* FrozenDefinitions,
		const asSByteCodeEmissionOptions& Options);
	static asSByteCodeEmissionResult Emit(asCBuilder& Builder);
	static asSByteCodeEmissionResult Emit(asCBuilder& Builder, asCScriptFunction& Function);
};

ANGELSCRIPTRUNTIME_API asSByteCodeEmissionResult asEmitByteCodeImage(
	const asCCompilationSession* Session,
	const asCModuleDefinitionSet* FrozenDefinitions,
	const asSByteCodeEmissionOptions& Options,
	TUniquePtr<asCByteCodeImage>& OutImage);

END_AS_NAMESPACE
