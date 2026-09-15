#ifndef AS_BUILDER_H
#define AS_BUILDER_H

#include "as_config.h"
#include "as_builder_callbacks.h"
#include "frontend/Basic/as_source_manager.h"
#include "frontend/Compile/as_builder_stages.h"
#include "as_compile_output.h"
#include "as_scriptengine.h"
#include "Templates/UniquePtr.h"
#include "as_module.h"
#include "unreal/AngelscriptSource.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "Misc/MemStack.h"
#include "Templates/SharedPointer.h"

#include <memory>

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCBuilder
{
public:
	FMemStackBase MemStack;

	asCBuilder(asCScriptEngine *engine, asCModule *module);
	asCBuilder(
		TConstArrayView<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources,
		const asSBuilderOptions& Options,
		TConstArrayView<asIBuilderCallbacks*> Callbacks = {});
	bool RunStage(asEBuilderStage Stage);
	bool RunThrough(asEBuilderStage Stage = asEBuilderStage::ByteCodeEmitted);
	asEBuilderStage GetStage() const;
	const asSBuilderStageResult* GetStageResult(asEBuilderStage Stage) const;
	const asCSourceManager* GetSourceManager() const;
	const asCCompilationSession* GetCompilationSession() const;
	asCCompilationSession* GetCompilationSession();
	TConstArrayView<asSSemanticSourceInput> GetLexedSources() const;
	TConstArrayView<asSDirectiveProcessResult> GetPreprocessedSources() const;
	TSharedRef<const asCDiagnosticResult, ESPMode::ThreadSafe> CaptureDiagnosticResult() const;
	const asCModuleDefinitionSet* GetModuleDefinitionSet() const;
	asCModuleDefinitionSet* GetModuleDefinitionSet();
	TUniquePtr<asCModuleDefinitionSet> TakeModuleDefinitionSet();
	const asCCompileOutput* GetCompileOutput() const;
	TUniquePtr<asCCompileOutput> TakeCompileOutput();
#if WITH_ANGELSCRIPT_TESTS
	static void DebugResetDeclarationsResolvedEntries();
	static int32 DebugGetDeclarationsResolvedEntries();
	void DebugForceLexFalse(const FString& LogicalSourceKey);
	void DebugForceAcquisitionFailure(const FString& LogicalSourceKey);
	int32 DebugGetPeakWorkers() const;
	int32 DebugGetProcessInvocationCount() const;
	uint32 DebugGetLexCallerThreadId() const;
	TArray<FString> DebugGetProcessedLogicalKeys() const;
	TArray<uint32> DebugGetWorkThreadIds() const;
#endif
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
