#pragma once

#include "CoreMinimal.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

namespace TypedSemanticIRCallTargetTestSupport
{
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

	inline const asSTypedSemanticExpression* FindResolvedCall(
		const asCTypedSemanticFunction& HIR,
		const asETypedSemanticCallTargetKind TargetKind)
	{
		for (asUINT Index = 0; Index < HIR.expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression = HIR.expressions[Index];
			if (Expression.kind == asETypedSemanticExpressionKind::ResolvedCall
				&& Expression.callTargetKind == TargetKind)
			{
				return &Expression;
			}
		}
		return nullptr;
	}

	inline bool AreTargetProvenancesEqual(
		const asSTypedSemanticFunctionTargetProvenance& Left,
		const asSTypedSemanticFunctionTargetProvenance& Right)
	{
		return Left.canonicalDeclaration == Right.canonicalDeclaration
			&& Left.declarationModule == Right.declarationModule
			&& Left.declaredTraitBits == Right.declaredTraitBits
			&& Left.bodyAvailability == Right.bodyAvailability
			&& Left.bodyOwnership == Right.bodyOwnership
			&& Left.bodyOwnerModule == Right.bodyOwnerModule
			&& Left.importedSourceModule == Right.importedSourceModule
			&& Left.importedCanonicalSignature
				== Right.importedCanonicalSignature;
	}

	inline int ExecuteIntNoArgs(
		asIScriptEngine& Engine,
		asIScriptFunction& Function,
		int32& OutValue)
	{
		asIScriptContext* const Context = Engine.CreateContext();
		if (Context == nullptr)
		{
			return asOUT_OF_MEMORY;
		}

		int Result = Context->Prepare(&Function);
		if (Result >= 0)
		{
			Result = Context->Execute();
			if (Result == asEXECUTION_FINISHED)
			{
				OutValue = static_cast<int32>(Context->GetReturnDWord());
			}
		}
		Context->Release();
		return Result;
	}
}
