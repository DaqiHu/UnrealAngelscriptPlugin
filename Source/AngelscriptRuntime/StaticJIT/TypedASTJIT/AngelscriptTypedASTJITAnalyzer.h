#pragma once

#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITModel.h"

ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITAnalysis AnalyzeTypedASTJITFunction(
	const asCTypedSemanticFunction& Function,
	const FAngelscriptTypedASTJITFunctionShape& Shape,
	TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
		NativeCalls = {},
	TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan>
		ScriptCalls = {},
	TConstArrayView<FAngelscriptTypedASTJITGlobalDependency>
		GlobalDependencies = {});
