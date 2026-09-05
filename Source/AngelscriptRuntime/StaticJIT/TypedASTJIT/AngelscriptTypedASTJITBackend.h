#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/Backends/AngelscriptStaticJITBackend.h"

#if AS_CAN_GENERATE_JIT

#if WITH_ANGELSCRIPT_UNITTESTS
/** Pointer-free lifecycle audit for the most recent TypedAST generation task. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITTaskObservation
{
	int32 CreatedInstanceCount = 0;
	int32 GenerateInvocationCount = 0;
	int32 DestroyedInstanceCount = 0;
	int32 AcceptedTaskCount = 0;
	int32 RejectedTaskCount = 0;
	int32 LastModuleCount = 0;
	int32 LastFunctionCount = 0;
	int32 LastVerifiedCanonicalASTCount = 0;
	int32 LastUFunctionRootCount = 0;
	int32 LastCallClosurePlanCount = 0;
	int32 LastEligibleCallClosurePlanCount = 0;
	int32 LastPlannedCallCount = 0;
	int32 LastDirectScriptCallCount = 0;
	int32 LastInternalSemanticHelperCallCount = 0;
	int32 LastBridgeCallCount = 0;
	int32 LastUnsupportedCallCount = 0;
	int32 StructuredFunctionDiagnosticCount = 0;
	int32 StructuredRootClosureDiagnosticCount = 0;
	int32 StructuredHelperDiagnosticCount = 0;
	int32 StructuredVerifiedCanonicalASTDiagnosticCount = 0;
	int32 StructuredExceptionPayloadDiagnosticCount = 0;
	int32 StructuredCallDiagnosticCount = 0;
	int32 StructuredEmittedCallDiagnosticCount = 0;
	int32 StructuredBridgeCallDiagnosticCount = 0;
	bool bLastTaskUsedOnlySameCompilationAST = false;
	FString LastFailure;
};
#endif

/**
 * One synchronous Typed Semantic StaticJIT generation task.
 *
 * The instance is deliberately one-shot. It accepts only Engine-local
 * functions and the exact sealed Canonical AST still owned by each function
 * from the same compilation. It never serializes or retains Engine/AST
 * pointers after Generate() returns.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJIT final
	: public IAngelscriptStaticJITBackend
{
public:
	FAngelscriptTypedASTJIT();
	~FAngelscriptTypedASTJIT() override;

	bool Generate(
		const FAngelscriptStaticJITBackendRequest& Request,
		FAngelscriptStaticJITBackendOutput& OutOutput,
		FString* OutError) override;

#if WITH_ANGELSCRIPT_UNITTESTS
	static void ResetTaskObservationForTesting();
	static FAngelscriptTypedASTJITTaskObservation
		GetTaskObservationForTesting();
	static FAngelscriptStaticJITTypedCallDiagnostic
		BuildSourceProvenanceDiagnosticForTesting(
			const asSSourceSpan& SourceSpan);
#endif

private:
	bool bTaskConsumed = false;
};

#endif // AS_CAN_GENERATE_JIT
