/* Maintained Unreal AngelScript fork — engine-independent frontend stages. */
#pragma once

#include "frontend/Basic/as_diagnostics.h"
#include "frontend/Compile/as_compilation_session.h"
#include "as_bytecode_emission.h"
#include "as_definitions.h"
#include "Templates/SharedPointer.h"

BEGIN_AS_NAMESPACE
class asCBuilderState;
	struct FAngelscriptPreprocessResult;
	struct asSSemanticSourceInput
	{
		FString LogicalSourceKey;
		TArray<asCToken> RawTokens;
	};
	enum class asEBuilderStage : uint8
	{
		SourceReady, Lexed, Preprocessed, DeclarationsCollected, DeclarationsResolved,
		BodiesAnalyzed, ASTVerified, DefinitionsBuilt, LayoutsFinalized, DefinitionsFrozen, ByteCodeEmitted, Failed
	};

	struct asSBuilderOptions
	{
		FString StableScope;
		int32 WorkerCount = 1;
		int32 LexBatchSize = 4;
		asSDiagnosticOptions Diagnostics;
		TMap<FString, bool> PreprocessorFlags;
		TSharedPtr<asCTypeContext, ESPMode::ThreadSafe> TypeContext;
		TArray<asCDefinitions*> Dependencies;
	};

	// An immutable observation of a completed attempt, not another AST or an
	// input format. Concrete phase products are owned by the Builder/session.
	struct asSBuilderStageResult
	{
		asEBuilderStage Stage = asEBuilderStage::SourceReady;
		bool Succeeded = false;
		FString Error;
		FString Text;
		FString Json;
		TArray<asSDiagnosticRecord> Diagnostics;
		TOptional<asEDefinitionResult> MetadataStatus;
		TSharedPtr<const asCDiagnosticResult, ESPMode::ThreadSafe> DiagnosticResult;
		bool bHasEmission = false;
		asEByteCodeEmissionStatus EmissionStatus = asEByteCodeEmissionStatus::InvalidInput;
		asSByteCodeEmissionFailure EmissionFailure;
	};

	ANGELSCRIPTRUNTIME_API void asIngestByteCodeEmissionResult(
		asSBuilderStageResult& Stage,
		const asSByteCodeEmissionResult& Emitted);
END_AS_NAMESPACE
