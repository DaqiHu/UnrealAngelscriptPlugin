#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

namespace TypedSemanticIRMixinTestSupport
{
	struct FExecutionObservation
	{
		int State = asERROR;
		int32 ReturnValue = 0;
		FString Exception;
	};

	inline FExecutionObservation ExecuteGlobalInt(
		asIScriptEngine* ScriptEngine,
		asIScriptFunction* Function)
	{
		FExecutionObservation Observation;
		if (ScriptEngine == nullptr || Function == nullptr)
		{
			return Observation;
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		if (Context == nullptr)
		{
			return Observation;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int PrepareResult = Context->Prepare(Function);
		if (PrepareResult < 0)
		{
			Observation.State = PrepareResult;
			return Observation;
		}

		Observation.State = Context->Execute();
		if (Observation.State == asEXECUTION_FINISHED)
		{
			Observation.ReturnValue =
				static_cast<int32>(Context->GetReturnDWord());
		}
		else if (Observation.State == asEXECUTION_EXCEPTION)
		{
			Observation.Exception = UTF8_TO_TCHAR(
				Context->GetExceptionString() != nullptr
					? Context->GetExceptionString()
					: "");
		}
		return Observation;
	}

	inline const asSTypedSemanticExpression* FindResolvedCall(
		const asCTypedSemanticFunction& HIR,
		const int TargetFunctionId)
	{
		for (asUINT Index = 0; Index < HIR.expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression =
				HIR.expressions[Index];
			if (Expression.kind
					== asETypedSemanticExpressionKind::ResolvedCall
				&& Expression.resolvedFunctionId == TargetFunctionId)
			{
				return &Expression;
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

	inline const asSTypedSemanticExpression* ResolveExpression(
		const asCTypedSemanticFunction& HIR,
		const asTypedSemanticExpressionId Id)
	{
		return Id.IsValid() && Id.value < HIR.expressions.GetLength()
			? &HIR.expressions[Id.value]
			: nullptr;
	}

	inline bool ExpressionIsIntLiteral(
		const asCTypedSemanticFunction& HIR,
		const asTypedSemanticExpressionId Id,
		const char* ExpectedLiteral)
	{
		const asSTypedSemanticExpression* Expression =
			ResolveExpression(HIR, Id);
		while (Expression != nullptr
			&& Expression->kind
				== asETypedSemanticExpressionKind::Conversion
			&& Expression->operands.GetLength() == 1)
		{
			Expression = ResolveExpression(HIR, Expression->operands[0]);
		}
		return Expression != nullptr
			&& Expression->kind == asETypedSemanticExpressionKind::Literal
			&& Expression->literal == ExpectedLiteral;
	}

	inline bool CallHasIntArguments(
		const asCTypedSemanticFunction& HIR,
		const asTypedSemanticExpressionId Id,
		const int TargetFunctionId,
		const char* First,
		const char* Second)
	{
		const asSTypedSemanticExpression* const Call =
			ResolveExpression(HIR, Id);
		if (Call == nullptr
			|| Call->kind != asETypedSemanticExpressionKind::ResolvedCall
			|| Call->resolvedFunctionId != TargetFunctionId)
		{
			return false;
		}

		const int32 FirstBinding = FindFormalBinding(*Call, 0);
		const int32 SecondBinding = FindFormalBinding(*Call, 1);
		return FirstBinding != INDEX_NONE && SecondBinding != INDEX_NONE
			&& ExpressionIsIntLiteral(
				HIR,
				Call->operands[FirstBinding],
				First)
			&& ExpressionIsIntLiteral(
				HIR,
				Call->operands[SecondBinding],
				Second);
	}
}
