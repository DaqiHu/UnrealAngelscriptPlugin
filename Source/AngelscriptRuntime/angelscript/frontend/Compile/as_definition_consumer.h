/* Maintained Unreal AngelScript fork — typed AST to real detached definitions. */
#pragma once

#include "as_definitions.h"
#include "frontend/Basic/as_source_location.h"

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
		TUniquePtr<asCDefinitions> Definitions;
		TArray<asSDefinitionDiagnostic> Diagnostics;
		bool Succeeded() const { return Status == asEDefinitionConsumerStatus::Succeeded && Definitions.IsValid(); }
	};
	class ANGELSCRIPTRUNTIME_API asCDefinitionConsumer final
	{
	public:
		// Returns complete semantic edges in a Building definition set. Layout
		// validation and Freeze remain explicit subsequent stages; no AST pointer is retained.
		asSDefinitionConsumerResult Consume(const asCCompilationSession& Session,
			TConstArrayView<asCDefinitions*> Dependencies = {}) const;
	};
END_AS_NAMESPACE
