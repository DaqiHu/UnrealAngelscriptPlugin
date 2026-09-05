/* Maintained Unreal AngelScript fork — typed AST to real detached definitions. */
#pragma once

#include "as_metadata_image.h"
#include "frontend/as_source_location.h"

BEGIN_AS_NAMESPACE
class asCCompilationSession;
	enum class asEDefinitionConsumerStatus : uint8
	{
		Succeeded, InvalidPhase, InvalidAST, InvalidIdentity, UnsupportedDefinition,
		InvalidType, InvalidDefinition, InvalidDependency
	};
	struct asSDefinitionDiagnostic
	{
		asCSourceRange Range;
		FString Message;
	};
	struct asSDefinitionConsumerResult
	{
		asEDefinitionConsumerStatus Status = asEDefinitionConsumerStatus::InvalidPhase;
		std::shared_ptr<asCMetadataImage> Image;
		TArray<asSDefinitionDiagnostic> Diagnostics;
		bool Succeeded() const { return Status == asEDefinitionConsumerStatus::Succeeded && Image != nullptr; }
	};
	class ANGELSCRIPTRUNTIME_API asCDefinitionConsumer final
	{
	public:
		// Returns complete semantic edges in a Building image. Layout validation
		// and Freeze remain explicit subsequent stages; no AST pointer is retained.
		asSDefinitionConsumerResult Consume(const asCCompilationSession& Session,
			TConstArrayView<std::shared_ptr<asCMetadataImage>> Dependencies = {}) const;
	};
END_AS_NAMESPACE
