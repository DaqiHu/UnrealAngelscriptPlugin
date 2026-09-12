/* Maintained Unreal AngelScript fork — internal emit that still produces a bytecode image. */
#pragma once

#include "as_bytecode_emitter.h"
#include "as_bytecode_image.h"

BEGIN_AS_NAMESPACE

ANGELSCRIPTRUNTIME_API asSByteCodeEmissionResult asEmitByteCodeImage(
	const asCCompilationSession* Session,
	const asCModuleDefinitionSet* FrozenDefinitions,
	const asSByteCodeEmissionOptions& Options,
	TUniquePtr<asCByteCodeImage>& OutImage);

END_AS_NAMESPACE
