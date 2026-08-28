#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"

#if AS_CAN_GENERATE_JIT

#include "AngelscriptEngine.h"

void FAngelscriptStaticJITGenerationProfile::ApplyToEngineConfig(
	FAngelscriptEngineConfig& InOutConfig) const
{
	InOutConfig.Purpose = EAngelscriptEnginePurpose::StaticJITGeneration;
	InOutConfig.bCollectStaticJITCompatibilityBinds = true;
	InOutConfig.bRequireCanonicalASTForStaticJITGeneration =
		RequiresCanonicalASTSnapshot();
	// A TypedAST generation Engine must compile the source graph with the same
	// sealed AST that the backend leases. Retaining a canonical sidecar beside
	// LEGACY-published Bytecode would create two authorities and make generation
	// provenance depend on a graph that did not publish the executable module.
	InOutConfig.bUseCanonicalStagedCompiler = RequiresCanonicalASTSnapshot();
	// A TypedAST generation snapshot owns only the public V1 AST lease. Make
	// retention part of the frozen profile itself so every direct generation
	// host (not merely the project-source-graph wrapper) satisfies the same
	// precondition before source compilation begins.
	if (RequiresCanonicalASTSnapshot())
	{
		InOutConfig.InitialASTRetentionPolicy = asAST_RETAIN_SNAPSHOT;
	}
}

#endif
