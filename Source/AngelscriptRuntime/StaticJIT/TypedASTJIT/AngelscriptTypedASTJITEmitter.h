#pragma once

#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITAnalyzer.h"

/** Stable C++ spelling shared by the pure body and Provider entry adapters. */
ANGELSCRIPTRUNTIME_API bool TrySpellAngelscriptTypedASTJITCppType(
	const asCDataType& Type,
	FString& OutType);

/** Emits one immutable definition for every exact bridge target in the view. */
ANGELSCRIPTRUNTIME_API bool EmitAngelscriptTypedASTJITBridgeCallSiteRows(
	TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls,
	FString& OutDefinitions,
	FString* OutError = nullptr);

class asCASTContext;
