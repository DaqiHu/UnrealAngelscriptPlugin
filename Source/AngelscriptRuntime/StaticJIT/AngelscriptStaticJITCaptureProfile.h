#pragma once

#include "CoreTypes.h"

#include "StaticJIT/StaticJITConfig.h"

#if AS_CAN_GENERATE_JIT

/** Source-compile data retained for one frozen StaticJIT generation task. */
enum class EAngelscriptStaticJITCaptureProfile : uint8
{
	Bytecode = 0,
	VerifiedTypedHIR,
};

#endif
