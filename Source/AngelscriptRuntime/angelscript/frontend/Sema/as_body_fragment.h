/* Maintained Unreal AngelScript fork — isolated deferred-body semantic product. */
#pragma once

#include "frontend/Sema/as_body_lifetime.h"

BEGIN_AS_NAMESPACE

struct asSDeferredBodyWorkItem
	{
		asCFunctionDecl* Function = nullptr;
		asSStableKey StableFunctionKey;
		asCSourceRange BodyRange;

		bool IsValid() const
		{
			return Function && !Function->IsInvalid() && !StableFunctionKey.IsZero() && BodyRange.IsOrdered();
		}
	};

	class ANGELSCRIPTRUNTIME_API asCBodyFragment
	{
	public:
		explicit asCBodyFragment(asSDeferredBodyWorkItem InWork = {}) : Work(MoveTemp(InWork)) {}
		const asSDeferredBodyWorkItem& GetWorkItem() const { return Work; }
		asCCompoundStmt* GetBody() const { return Body; }
		bool SetBody(asCCompoundStmt* InBody) { if (!InBody || Body) return false; Body = InBody; return true; }
		bool HasErrors() const { return bHasErrors; }
		void MarkError(FString Diagnostic)
		{
			bHasErrors = true;
			if (!Diagnostic.IsEmpty()) StableDiagnostics.Add(MoveTemp(Diagnostic));
		}
		TConstArrayView<FString> GetStableDiagnostics() const { return MakeArrayView(StableDiagnostics); }
		asCBodyLifetimePlan& GetLifetimePlan() { return LifetimePlan; }
		const asCBodyLifetimePlan& GetLifetimePlan() const { return LifetimePlan; }

	private:
		asSDeferredBodyWorkItem Work;
		asCCompoundStmt* Body = nullptr;
		bool bHasErrors = false;
		TArray<FString> StableDiagnostics;
		asCBodyLifetimePlan LifetimePlan;
	};

END_AS_NAMESPACE
