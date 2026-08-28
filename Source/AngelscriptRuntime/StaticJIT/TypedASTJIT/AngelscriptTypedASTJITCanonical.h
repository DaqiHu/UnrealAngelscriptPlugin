#pragma once

#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITAnalyzer.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITDependencies.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEligibility.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"

#if AS_CAN_GENERATE_JIT

struct FAngelscriptStaticJITCompiledFunctionView;

ANGELSCRIPTRUNTIME_API asASTDeclId
FindAngelscriptTypedASTJITCanonicalFunctionDecl(
	const asCASTContext& Context,
	const char* Name);

ANGELSCRIPTRUNTIME_API bool
FunctionHasTypedASTJITCanonicalBody(
	const FAngelscriptStaticJITCompiledFunctionView& Function);

ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITEligibility
EvaluateAngelscriptTypedASTJITEligibilityFromCanonical(
	const FAngelscriptTypedASTJITEligibilityInput& Input);

ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITAnalysis
AnalyzeTypedASTJITFunction(
	const asCASTContext& Context,
	asASTDeclId FunctionDecl,
	const FAngelscriptTypedASTJITFunctionShape& Shape,
	TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
		NativeCalls = {},
	TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan>
		ScriptCalls = {},
	TConstArrayView<FAngelscriptTypedASTJITGlobalDependency>
		GlobalDependencies = {});

/**
 * Authenticates one exact same-generation sealed Canonical lifetime protocol
 * and copies only its pointer-free stable-key/ABI-key summary. Unsupported
 * non-empty cleanup families remain authenticated but fail closed at native
 * eligibility until their object-frame ABI and exit emission are implemented.
 */
ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITCanonicalLifetimeFacts
AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
	const asCASTContext& Context,
	asASTDeclId FunctionDecl);

ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITEmission
EmitTypedASTJITFunction(
	const asCASTContext& Context,
	asASTDeclId FunctionDecl,
	const FAngelscriptTypedASTJITFunctionShape& Shape,
	const FAngelscriptTypedASTJITEmitOptions& Options);

ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITDependencyAnalysis
AnalyzeAngelscriptTypedASTJITDependencies(
	const asCASTContext& Context,
	asASTDeclId FunctionDecl,
	const FAngelscriptTypedASTJITFunctionShape& Shape,
	const FAngelscriptStaticJITCompiledFunctionView& Function,
	const FAngelscriptStaticJITCompiledSourceGraph& Graph,
	TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
		NativeCalls = {},
	TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan>
		ScriptCalls = {},
	TConstArrayView<FAngelscriptTypedASTJITGlobalDependency>
		GlobalDependencies = {});

ANGELSCRIPTRUNTIME_API bool
AngelscriptTypedASTJITCanonicalHasLoopBackedge(
	const asCASTContext& Context,
	asASTDeclId FunctionDecl);

ANGELSCRIPTRUNTIME_API void
CollectAngelscriptTypedASTJITCanonicalCallExprs(
	const asCASTContext& Context,
	asASTDeclId FunctionDecl,
	TArray<asASTExprId>& OutCalls);

#endif
