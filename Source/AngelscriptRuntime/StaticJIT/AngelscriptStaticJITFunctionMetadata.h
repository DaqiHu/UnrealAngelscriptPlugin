#pragma once

#include "CoreMinimal.h"

#if AS_CAN_GENERATE_JIT

/**
 * Pointer-free role assigned only from the authoritative literal-asset pair
 * recorded by the preprocessor. Function names are never role authority.
 */
enum class EAngelscriptStaticJITLiteralAssetFunctionRole : uint8
{
	None = 0,
	Getter,
	Initializer,
};

#endif // AS_CAN_GENERATE_JIT
