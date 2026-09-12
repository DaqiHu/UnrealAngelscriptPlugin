/* Maintained Unreal AngelScript fork — structural typed AST verification. */
#pragma once

#include "frontend/AST/as_ast_fwd.h"

#include "Containers/UnrealString.h"

BEGIN_AS_NAMESPACE

enum class asEASTVerificationStatus : uint8
	{
		Succeeded,
		NullRoot,
		ForeignNode,
		InvalidSourceRange,
		MissingRequiredChild,
		DuplicateOwningEdge,
		OwningCycle,
		RecoveryReachable,
		InvalidCrossReference,
		MissingSemanticReference,
		InvalidSemanticReference,
		InvalidDeclarationContext,
		InvalidAttributeTarget,
		InvalidPayload,
		UnhandledKind,
	};

	struct asSASTVerificationResult
	{
		asEASTVerificationStatus Status = asEASTVerificationStatus::Succeeded;
		FString Diagnostic;
		uint32 LocalNodeID = 0;
	};

	class ANGELSCRIPTRUNTIME_API asCASTVerifier
	{
	public:
		static bool Verify(const asCASTContext& Context, const asCASTNode* Root, asSASTVerificationResult& OutResult);
	};

END_AS_NAMESPACE
