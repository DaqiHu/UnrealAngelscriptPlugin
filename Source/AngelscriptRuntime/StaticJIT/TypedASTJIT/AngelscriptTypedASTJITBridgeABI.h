#pragma once

#include "CoreMinimal.h"

#include "Core/Artifacts/AngelscriptArtifactIdentity.h"
#include "StaticJIT/StaticJITConfig.h"

#if AS_CAN_GENERATE_JIT

class asIScriptFunction;

/**
 * AS-visible scalar marshalling shape for the VM bridge.
 *
 * This deliberately excludes the registered native calling convention and
 * callable pointer ABI. It proves only what InvokeBound can pack through
 * Prepare, SetArg and Execute. The stable environment reference separately
 * proves which current-Engine function may receive those values.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITVMBridgeABIIdentity
{
	FString Canonical;
	FAngelscriptHash256 Hash;

	static FAngelscriptTypedASTJITVMBridgeABIIdentity FromCanonical(
		FStringView Canonical);
	bool IsValid() const;

	friend bool operator==(
		const FAngelscriptTypedASTJITVMBridgeABIIdentity& Left,
		const FAngelscriptTypedASTJITVMBridgeABIIdentity& Right)
	{
		return Left.Canonical == Right.Canonical
			&& Left.Hash == Right.Hash;
	}
};

ANGELSCRIPTRUNTIME_API bool TryBuildAngelscriptTypedASTJITVMBridgeABIIdentity(
	const asIScriptFunction& ScriptFunction,
	FAngelscriptTypedASTJITVMBridgeABIIdentity& OutIdentity,
	FString* OutError = nullptr);

#endif // AS_CAN_GENERATE_JIT
