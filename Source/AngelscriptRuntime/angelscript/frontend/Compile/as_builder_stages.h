/* Maintained Unreal AngelScript fork — engine-independent frontend stages. */
#pragma once

#include "frontend/Compile/as_compilation_session.h"
#include "as_module_definition_set.h"

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
		TMap<FString, bool> PreprocessorFlags;
		TSharedPtr<asCTypeContext, ESPMode::ThreadSafe> TypeContext;
		TArray<asCModuleDefinitionSet*> Dependencies;
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
		TOptional<asEMetadataResult> MetadataStatus;
	};
END_AS_NAMESPACE
