#pragma once

#include "CoreMinimal.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_buildartifact.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

namespace TypedSemanticIRArgumentOriginTestSupport
{
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

	inline bool AreSpansEqual(
		const asSTypedSemanticSourceSpan& Left,
		const asSTypedSemanticSourceSpan& Right)
	{
		return Left.section == Right.section
			&& Left.sourceOffset == Right.sourceOffset
			&& Left.sourceLength == Right.sourceLength
			&& Left.row == Right.row
			&& Left.column == Right.column;
	}

	inline int32 CountSignatureDependencies(
		const asCScriptFunction& Caller,
		const asCScriptFunction& Callee)
	{
		if (Caller.scriptData == nullptr)
		{
			return 0;
		}
		int32 Count = 0;
		for (asUINT Index = 0;
			Index < Caller.scriptData->artifactDependencies.GetLength();
			++Index)
		{
			const asSBuildArtifactDependency& Dependency =
				Caller.scriptData->artifactDependencies[Index];
			Count += Dependency.kind
					== asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE
				&& Dependency.referenceKind
					== asBUILD_ARTIFACT_REFERENCE_FUNCTION
				&& Dependency.function == &Callee
				? 1
				: 0;
		}
		return Count;
	}
}
