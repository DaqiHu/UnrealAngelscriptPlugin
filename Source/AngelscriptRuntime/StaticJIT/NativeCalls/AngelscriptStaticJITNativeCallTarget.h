#pragma once

#include "CoreMinimal.h"

#include "Core/Artifacts/AngelscriptArtifactReference.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallDescriptor.h"
#include "StaticJIT/StaticJITConfig.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITBridgeABI.h"

#if AS_CAN_GENERATE_JIT

/** Pointer-free target facts copied out of one generation Engine. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITNativeCallTarget
{
	int32 EngineLocalFunctionId = INDEX_NONE;
	FString CanonicalDeclaration;
	/** Stable current-Engine adoption identity; never an executable pointer. */
	FAngelscriptArtifactReference Reference;
	FAngelscriptStaticJITNativeABIIdentity ExpectedNativeABI;
	FAngelscriptStaticJITScalarABIIdentity ExpectedScalarABI;
	FAngelscriptTypedASTJITVMBridgeABIIdentity ExpectedVMBridgeABI;
	/** Diagnostic-only registered C++ spelling; never dispatch identity. */
	FString RegisteredTargetDisplay;
	/** Actual installed ABI was proven safe for current-slot native dispatch. */
	bool bCurrentNativeCallable = false;
	bool bScalarBridgeCallable = false;
	FAngelscriptStaticJITNativeCallDescriptor Descriptor;
};

#endif // AS_CAN_GENERATE_JIT
