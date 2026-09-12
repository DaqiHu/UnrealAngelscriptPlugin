/* Maintained Unreal AngelScript fork — typed integral constant evaluation. */
#pragma once

#include "frontend/AST/as_type.h"

BEGIN_AS_NAMESPACE
enum class asEConstantEvaluationStatus : uint8
	{
		Succeeded, NotConstant, UnresolvedReference, InvalidType, Overflow, DivideByZero, InvalidShift, BudgetExceeded
	};
	struct asSIntegralConstantEvaluation
	{
		asEConstantEvaluationStatus Status = asEConstantEvaluationStatus::NotConstant;
		uint64 Bits = 0;
		uint8 BitWidth = 0;
		bool bSigned = false;
		const asCExpr* FailureNode = nullptr;
	};
	// Pure semantic queries: no Engine, diagnostics, source reparse or AST mutation.
	ANGELSCRIPTRUNTIME_API asSIntegralConstantEvaluation asEvaluateIntegralConstant(const asCExpr& Expression);
	ANGELSCRIPTRUNTIME_API bool asTryConvertIntegralConstant(const asSIntegralConstantEvaluation& Input, asCQualType Target, uint64& OutBits);
END_AS_NAMESPACE
