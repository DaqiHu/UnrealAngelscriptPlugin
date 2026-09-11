/* Maintained Unreal AngelScript fork — isolated typed declaration fragment. */
#pragma once

#include "frontend/as_decl.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCDeclarationFragment
	{
	public:
		explicit asCDeclarationFragment(FString InLogicalSourceKey = {}) : LogicalSourceKey(MoveTemp(InLogicalSourceKey)) {}
		const FString& GetLogicalSourceKey() const { return LogicalSourceKey; }
		bool AddTopLevelDecl(asCDecl* Decl) { if (!Decl) return false; TopLevelDecls.Add(Decl); return true; }
		TConstArrayView<asCDecl*> GetTopLevelDecls() const { return MakeArrayView(TopLevelDecls); }
		bool HasErrors() const { return bHasErrors; }
		void MarkError() { bHasErrors = true; }

	private:
		FString LogicalSourceKey;
		TArray<asCDecl*> TopLevelDecls;
		bool bHasErrors = false;
	};

END_AS_NAMESPACE
