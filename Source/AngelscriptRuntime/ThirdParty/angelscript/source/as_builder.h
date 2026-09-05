#ifndef AS_BUILDER_H
#define AS_BUILDER_H

#include "as_config.h"
#include "frontend/as_builder_stages.h"
#include "as_scriptengine.h"
#include "as_module.h"

#include "Misc/MemStack.h"

#include <memory>

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCBuilder
{
public:
	FMemStackBase MemStack;

	asCBuilder(asCScriptEngine *engine, asCModule *module);
	asCBuilder(TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot,
		asCDiagnosticsEngine& Diagnostics, const asSBuilderOptions& Options);
	bool RunStage(asEBuilderStage Stage);
	bool RunThrough(asEBuilderStage Stage = asEBuilderStage::DefinitionsFrozen);
	asEBuilderStage GetStage() const;
	const asSBuilderStageResult* GetStageResult(asEBuilderStage Stage) const;
	const asCCompilationSession* GetCompilationSession() const;
	TConstArrayView<asSSemanticSourceInput> GetLexedSources() const;
	TConstArrayView<asSDirectiveProcessResult> GetPreprocessedSources() const;
	std::shared_ptr<asCMetadataImage> GetDefinitions() const;
	static FAngelscriptPreprocessResult CompileDeclarations(
		const asCSourceManager& SourceManager,
		const asCPreprocessorConfiguration& Configuration,
		asCDiagnosticsEngine& Diagnostics,
		TConstArrayView<asSSemanticSourceInput> Sources, FString StableScope, int32 WorkerCount = 1);
	~asCBuilder();

	bool silent = false;
	asCScriptEngine *engine = nullptr;
	asCModule *module = nullptr;

private:
	std::shared_ptr<asCBuilderState> frontendState;
};

END_AS_NAMESPACE

#endif
