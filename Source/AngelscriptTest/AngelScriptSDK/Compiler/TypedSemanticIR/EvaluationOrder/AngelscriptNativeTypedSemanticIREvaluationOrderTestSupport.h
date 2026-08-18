#pragma once

#include "CoreMinimal.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

namespace TypedSemanticIREvaluationOrderTestSupport
{
	inline const asSTypedSemanticExpression* ResolveExpression(
		const asCTypedSemanticFunction& HIR,
		const asTypedSemanticExpressionId Id)
	{
		return Id.IsValid() && Id.value < HIR.expressions.GetLength()
			? &HIR.expressions[Id.value]
			: nullptr;
	}

	inline const asSTypedSemanticExpression* FindResolvedCall(
		const asCTypedSemanticFunction& HIR,
		const int TargetFunctionId)
	{
		for (asUINT Index = 0; Index < HIR.expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression = HIR.expressions[Index];
			if (Expression.kind == asETypedSemanticExpressionKind::ResolvedCall
				&& Expression.resolvedFunctionId == TargetFunctionId)
			{
				return &Expression;
			}
		}
		return nullptr;
	}

	inline const asSTypedSemanticExpression* FindResolvedCallByName(
		const asCTypedSemanticFunction& HIR,
		const asCScriptEngine& Engine,
		const char* const TargetName)
	{
		for (asUINT Index = 0; Index < HIR.expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression = HIR.expressions[Index];
			if (Expression.kind != asETypedSemanticExpressionKind::ResolvedCall)
			{
				continue;
			}
			const asCScriptFunction* const Target =
				Engine.GetScriptFunction(Expression.resolvedFunctionId);
			if (Target != nullptr && Target->name == TargetName)
			{
				return &Expression;
			}
		}
		return nullptr;
	}

	inline const asSTypedSemanticExpression* FindConstructorCall(
		const asCTypedSemanticFunction& HIR,
		const asCScriptEngine& Engine)
	{
		for (asUINT Index = 0; Index < HIR.expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression = HIR.expressions[Index];
			if (Expression.kind != asETypedSemanticExpressionKind::ResolvedCall)
			{
				continue;
			}
			const asCScriptFunction* const Target =
				Engine.GetScriptFunction(Expression.resolvedFunctionId);
			if (Target != nullptr && Target->traits.GetTrait(asTRAIT_CONSTRUCTOR))
			{
				return &Expression;
			}
		}
		return nullptr;
	}

	inline const asSTypedSemanticStatement* FindStatement(
		const asCTypedSemanticFunction& HIR,
		const asETypedSemanticStatementKind Kind,
		const asETypedSemanticExpressionKind ValueKind =
			asETypedSemanticExpressionKind::Unsupported)
	{
		for (asUINT Index = 0; Index < HIR.statements.GetLength(); ++Index)
		{
			const asSTypedSemanticStatement& Statement = HIR.statements[Index];
			if (Statement.kind != Kind || !Statement.valueExpression.IsValid())
			{
				continue;
			}
			if (ValueKind == asETypedSemanticExpressionKind::Unsupported
				|| HIR.expressions[Statement.valueExpression.value].kind == ValueKind)
			{
				return &Statement;
			}
		}
		return nullptr;
	}

	inline int32 FindFormalBinding(
		const asSTypedSemanticExpression& Call,
		const int32 FormalIndex)
	{
		for (asUINT BindingIndex = 0;
			BindingIndex < Call.formalParameterIndices.GetLength();
			++BindingIndex)
		{
			if (Call.formalParameterIndices[BindingIndex] == FormalIndex)
			{
				return static_cast<int32>(BindingIndex);
			}
		}
		return INDEX_NONE;
	}

	inline const asSTypedSemanticExpression* UnwrapConversions(
		const asCTypedSemanticFunction& HIR,
		const asTypedSemanticExpressionId ExpressionId)
	{
		const asSTypedSemanticExpression* Expression =
			ResolveExpression(HIR, ExpressionId);
		while (Expression != nullptr
			&& Expression->kind == asETypedSemanticExpressionKind::Conversion
			&& Expression->operands.GetLength() == 1)
		{
			Expression = ResolveExpression(HIR, Expression->operands[0]);
		}
		return Expression;
	}

	inline bool TryReadMarkerStage(
		const asCTypedSemanticFunction& HIR,
		const asSTypedSemanticExpression& Call,
		int32& OutStage)
	{
		const int32 Binding = FindFormalBinding(Call, 0);
		if (Binding == INDEX_NONE)
		{
			return false;
		}
		const asSTypedSemanticExpression* const Literal =
			UnwrapConversions(HIR, Call.operands[Binding]);
		if (Literal == nullptr
			|| Literal->kind != asETypedSemanticExpressionKind::Literal)
		{
			return false;
		}
		OutStage = FCStringAnsi::Atoi(Literal->literal.AddressOf());
		return true;
	}

	inline bool CollectMarkerStages(
		const asCTypedSemanticFunction& HIR,
		const asTypedSemanticExpressionId ExpressionId,
		const int MarkerFunctionId,
		TArray<int32>& OutStages,
		const asUINT Depth = 0)
	{
		const asSTypedSemanticExpression* const Expression =
			ResolveExpression(HIR, ExpressionId);
		if (Expression == nullptr || Depth > HIR.expressions.GetLength())
		{
			return false;
		}

		if (Expression->kind == asETypedSemanticExpressionKind::ResolvedCall
			&& Expression->resolvedFunctionId == MarkerFunctionId)
		{
			int32 Stage = 0;
			if (!TryReadMarkerStage(HIR, *Expression, Stage))
			{
				return false;
			}
			OutStages.Add(Stage);
			return true;
		}

		if (Expression->evaluationSteps.GetLength() != 0)
		{
			for (asUINT StepIndex = 0;
				StepIndex < Expression->evaluationSteps.GetLength();
				++StepIndex)
			{
				if (!CollectMarkerStages(
					HIR,
					Expression->evaluationSteps[StepIndex].expression,
					MarkerFunctionId,
					OutStages,
					Depth + 1))
				{
					return false;
				}
			}
			return true;
		}

		for (asUINT OperandIndex = 0;
			OperandIndex < Expression->operands.GetLength();
			++OperandIndex)
		{
			if (!CollectMarkerStages(
				HIR,
				Expression->operands[OperandIndex],
				MarkerFunctionId,
				OutStages,
				Depth + 1))
			{
				return false;
			}
		}
		return true;
	}

	inline bool HasExactStages(
		const TArray<int32>& Actual,
		const TArray<int32>& Expected)
	{
		if (Actual.Num() != Expected.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Actual.Num(); ++Index)
		{
			if (Actual[Index] != Expected[Index])
			{
				return false;
			}
		}
		return true;
	}

	inline TArray<int32> MakeExpectedStages(
		const int32 Count,
		const bool bReverse)
	{
		TArray<int32> Result;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Result.Add(bReverse ? Count - Index : Index + 1);
		}
		return Result;
	}
}
