/* Maintained Unreal AngelScript fork — source-language body lifetime facts. */
#pragma once

#include "frontend/AST/as_expr.h"

BEGIN_AS_NAMESPACE

struct asSBodyConstructedLocal
	{
		asCVarDecl* Declaration = nullptr;
		FString StableLocalKey;
		int32 ScopeDepth = 0;
	};

	struct asSBodyCleanupObligation
	{
		FString StableLocalKey;
		int32 ScopeDepth = 0;
	};

	struct asSBodyExitCleanup
	{
		asCStmt* Exit = nullptr;
		asCASTNode* Target = nullptr;
		TArray<asSBodyCleanupObligation> Obligations;
	};

	class ANGELSCRIPTRUNTIME_API asCBodyLifetimePlan
	{
	public:
		bool Analyze(asCCompoundStmt* Body);
		TConstArrayView<asSBodyConstructedLocal> GetConstructedLocals() const { return MakeArrayView(ConstructedLocals); }
		TConstArrayView<asSBodyExitCleanup> GetExitCleanups() const { return MakeArrayView(ExitCleanups); }
		FString SerializeStable() const;

	private:
		TArray<asSBodyConstructedLocal> ConstructedLocals;
		TArray<asSBodyExitCleanup> ExitCleanups;
	};

END_AS_NAMESPACE
