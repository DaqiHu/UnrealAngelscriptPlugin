#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITAnalyzer.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	bool IsTypedASTReviewedScalar(const asCDataType& Type)
	{
		if (!Type.IsValid() || Type.IsReference()
			|| Type.IsObjectHandle())
		{
			return false;
		}
		if (Type.IsEnumType())
		{
			return Type.GetSizeInMemoryBytes() == 1
				|| Type.GetSizeInMemoryBytes() == 2
				|| Type.GetSizeInMemoryBytes() == 4
				|| Type.GetSizeInMemoryBytes() == 8;
		}
		if (Type.GetTypeInfo() != nullptr)
		{
			return false;
		}

		switch (Type.GetTokenType())
		{
		case ttBool:
		case ttInt8:
		case ttUInt8:
		case ttInt16:
		case ttUInt16:
		case ttInt:
		case ttUInt:
		case ttInt64:
		case ttUInt64:
		case ttFloat32:
		case ttFloat64:
			return true;
		default:
			return false;
		}
	}

	bool IsTypedASTReviewedBorrowedInput(const asCDataType& Type)
	{
		if (!Type.IsValid() || !Type.IsReference() || !Type.IsReadOnly()
			|| Type.IsObjectHandle() || Type.GetTypeInfo() == nullptr)
		{
			return false;
		}
		const asCString& Name = Type.GetTypeInfo()->name;
		return Name == "FString" || Name == "FLinearColor";
	}

	bool IsTypedASTReviewedVoid(const asCDataType& Type)
	{
		return Type.IsValid() && !Type.IsReference()
			&& !Type.IsObjectHandle() && Type.GetTypeInfo() == nullptr
			&& Type.GetTokenType() == ttVoid;
	}

	int32 GetTypedASTReviewedIntegerWidth(const asCDataType& Type)
	{
		switch (Type.GetTokenType())
		{
		case ttInt8:
		case ttUInt8:
			return 8;
		case ttInt16:
		case ttUInt16:
			return 16;
		case ttInt:
		case ttUInt:
			return 32;
		case ttInt64:
		case ttUInt64:
			return 64;
		default:
			return 0;
		}
	}

	bool IsTypedASTReviewedInteger(const asCDataType& Type)
	{
		return Type.IsValid() && !Type.IsReference()
			&& !Type.IsObjectHandle() && Type.GetTypeInfo() == nullptr
			&& GetTypedASTReviewedIntegerWidth(Type) != 0;
	}

	bool IsTypedASTReviewedFloating(const asCDataType& Type)
	{
		return Type.IsValid() && !Type.IsReference()
			&& !Type.IsObjectHandle() && Type.GetTypeInfo() == nullptr
			&& (Type.GetTokenType() == ttFloat32
				|| Type.GetTokenType() == ttFloat64);
	}

	bool IsTypedASTReviewedNumeric(const asCDataType& Type)
	{
		return IsTypedASTReviewedInteger(Type)
			|| IsTypedASTReviewedFloating(Type);
	}

	bool IsTypedASTSameValueType(
		const asCDataType& Left,
		const asCDataType& Right)
	{
		return Left.IsEqualExceptRefAndConst(Right);
	}

	bool IsTypedASTReviewedIntegerConversion(
		const asCDataType& Source,
		const asCDataType& Target)
	{
		return IsTypedASTReviewedInteger(Source)
			&& IsTypedASTReviewedInteger(Target);
	}

	bool IsTypedASTReviewedFloatingConversion(
		const asCDataType& Source,
		const asCDataType& Target)
	{
		// The maintained VM implements both float-width conversions with the
		// corresponding native C++ conversion. Focused differential coverage
		// freezes signed zero, subnormal, infinity, NaN and out-of-range behavior
		// for the supported UE toolchain. Float/integer conversion remains behind
		// the separate non-portable numeric-conversion boundary.
		if (!IsTypedASTReviewedFloating(Source)
			|| !IsTypedASTReviewedFloating(Target))
		{
			return false;
		}
		return (Source.GetTokenType() == ttFloat32
				&& Target.GetTokenType() == ttFloat64)
			|| (Source.GetTokenType() == ttFloat64
				&& Target.GetTokenType() == ttFloat32);
	}

	bool IsTypedASTNonPortableFloatIntegerConversion(
		const asCDataType& Source,
		const asCDataType& Target)
	{
		return (IsTypedASTReviewedFloating(Source)
				&& IsTypedASTReviewedInteger(Target))
			|| (IsTypedASTReviewedInteger(Source)
				&& IsTypedASTReviewedFloating(Target));
	}

	bool IsTypedASTReviewedSignedIntegerForEnum(
		const asCDataType& IntegerType)
	{
		if (!IntegerType.IsValid() || IntegerType.IsReference()
			|| IntegerType.IsObjectHandle()
			|| IntegerType.GetTypeInfo() != nullptr)
		{
			return false;
		}

		switch (IntegerType.GetTokenType())
		{
		case ttInt8:
		case ttInt16:
		case ttInt:
		case ttInt64:
			return true;
		default:
			return false;
		}
	}

	bool IsTypedASTReviewedEnumIntegerConversion(
		const asCDataType& Source,
		const asCDataType& Target)
	{
		const bool bSourceIsEnum = Source.IsEnumType();
		const bool bTargetIsEnum = Target.IsEnumType();
		if (bSourceIsEnum == bTargetIsEnum)
		{
			return false;
		}

		const asCDataType& EnumType = bSourceIsEnum ? Source : Target;
		const asCDataType& IntegerType = bSourceIsEnum ? Target : Source;
		return IsTypedASTReviewedScalar(EnumType)
			&& IsTypedASTReviewedSignedIntegerForEnum(IntegerType);
	}

	bool IsTypedASTReviewedPowerShape(
		const asCTypedSemanticFunction& Function,
		const asSTypedSemanticExpression& Expression)
	{
		if (Expression.kind != asETypedSemanticExpressionKind::Binary
			|| Expression.operatorToken != "**"
			|| Expression.operands.GetLength() != 2)
		{
			return false;
		}
		const asCDataType& Left =
			Function.expressions[Expression.operands[0].value].type;
		const asCDataType& Right =
			Function.expressions[Expression.operands[1].value].type;
		if (Expression.type.GetTokenType() == ttFloat32)
		{
			return Left.GetTokenType() == ttFloat32
				&& Right.GetTokenType() == ttFloat32;
		}
		if (Expression.type.GetTokenType() != ttFloat64
			|| Left.GetTokenType() != ttFloat64)
		{
			return false;
		}
		return Right.GetTokenType() == ttFloat64
			|| Right.GetTokenType() == ttInt;
	}

	bool IsTypedASTReviewedPowerConversion(
		const asCTypedSemanticFunction& Function,
		const asTypedSemanticExpressionId ConversionId,
		const asCDataType& Source,
		const asCDataType& Target)
	{
		if (!IsTypedASTReviewedNumeric(Source))
		{
			return false;
		}
		const bool bReviewedTarget =
			Target.GetTokenType() == ttFloat32
			|| Target.GetTokenType() == ttFloat64
			|| (Target.GetTokenType() == ttInt
				&& IsTypedASTReviewedInteger(Source));
		if (!bReviewedTarget)
		{
			return false;
		}

		for (asUINT Index = 0; Index < Function.expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Candidate =
				Function.expressions[Index];
			if (!IsTypedASTReviewedPowerShape(Function, Candidate))
			{
				continue;
			}
			for (asUINT OperandIndex = 0;
				OperandIndex < Candidate.operands.GetLength();
				++OperandIndex)
			{
				if (Candidate.operands[OperandIndex] == ConversionId)
				{
					return true;
				}
			}
		}
		return false;
	}

	FAngelscriptTypedASTJITAnalysis MakeTypedASTAnalysisFailure(
		const EAngelscriptTypedASTJITFallbackReason Reason,
		const asSTypedSemanticSourceSpan& Span,
		const FString& Detail)
	{
		FAngelscriptTypedASTJITAnalysis Result;
		Result.FailureReason = Reason;
		Result.FailureSpan = Span;
		Result.FailureDetail = Detail;
		return Result;
	}

	FAngelscriptTypedASTJITAnalysis MakeTypedASTAnalysisFailure(
		const EAngelscriptTypedASTJITFallbackReason Reason,
		const FString& Detail)
	{
		return MakeTypedASTAnalysisFailure(Reason, {}, Detail);
	}

	const FAngelscriptTypedASTJITNativeCallEmissionPlan* FindNativeCallPlan(
		const TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
			NativeCalls,
		const asTypedSemanticExpressionId Expression)
	{
		return NativeCalls.FindByPredicate(
			[Expression](const auto& Candidate)
			{
				return Candidate.Expression == Expression;
			});
	}

	const FAngelscriptTypedASTJITScriptCallEmissionPlan* FindScriptCallPlan(
		const TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan>
			ScriptCalls,
		const asTypedSemanticExpressionId Expression)
	{
		return ScriptCalls.FindByPredicate(
			[Expression](const auto& Candidate)
			{
				return Candidate.Expression == Expression;
			});
	}

	int32 FindParameterOrdinal(
		const asCTypedSemanticFunction& Function,
		const asTypedSemanticSymbolId SymbolId)
	{
		int32 Ordinal = 0;
		for (asUINT SymbolIndex = 0;
			SymbolIndex < Function.symbols.GetLength(); ++SymbolIndex)
		{
			const asSTypedSemanticSymbol& Symbol =
				Function.symbols[SymbolIndex];
			if (Symbol.kind != asETypedSemanticSymbolKind::Parameter)
			{
				continue;
			}
			if (Symbol.id == SymbolId)
			{
				return Ordinal;
			}
			++Ordinal;
		}
		return INDEX_NONE;
	}
}

FAngelscriptTypedASTJITAnalysis AnalyzeTypedASTJITFunction(
	const asCTypedSemanticFunction& Function,
	const FAngelscriptTypedASTJITFunctionShape& Shape,
	const TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
		NativeCalls,
	const TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan>
		ScriptCalls,
	const TConstArrayView<FAngelscriptTypedASTJITGlobalDependency>
		GlobalDependencies)
{
	const asSTypedSemanticVerificationResult Verification =
		VerifyTypedSemanticFunction(Function);
	if (!Verification.IsValid())
	{
		return MakeTypedASTAnalysisFailure(
			Verification.error
				== asETypedSemanticVerificationError::InvalidCleanupPlan
				? EAngelscriptTypedASTJITFallbackReason::InvalidCleanupPlan
				: EAngelscriptTypedASTJITFallbackReason::InvalidTypedHIR,
			Verification.span,
			UTF8_TO_TCHAR(Verification.detail.AddressOf()));
	}

	const asSTypedSemanticSourceSpan RootSpan =
		Function.statements[Function.rootStatement.value].span;
	if (Function.header.hasSuspendState)
	{
		return MakeTypedASTAnalysisFailure(
			EAngelscriptTypedASTJITFallbackReason::SuspendOrExceptionState,
			RootSpan,
			TEXT("The scalar emitter cannot own suspend or resumable state"));
	}
	if (Function.header.cleanupPlanState
		!= asETypedSemanticCleanupPlanState::VerifiedEmpty)
	{
		const EAngelscriptTypedASTJITFallbackReason Reason =
			Function.header.cleanupPlanState
				== asETypedSemanticCleanupPlanState::Unverified
			? EAngelscriptTypedASTJITFallbackReason::InvalidCleanupPlan
			: (Function.header.cleanupPlanState
				== asETypedSemanticCleanupPlanState::CompilerExceptionRegion
				? EAngelscriptTypedASTJITFallbackReason::UnsupportedExceptionRegion
				: EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime);
		return MakeTypedASTAnalysisFailure(
			Reason,
			RootSpan,
			TEXT("The scalar emitter requires one compiler-verified empty cleanup plan"));
	}

	if (Shape.SymbolName.IsEmpty())
	{
		return MakeTypedASTAnalysisFailure(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature,
			TEXT("The resolved C++ symbol name is empty"));
	}
	if (!IsTypedASTReviewedScalar(Shape.ReturnType)
		&& !IsTypedASTReviewedVoid(Shape.ReturnType))
	{
		return MakeTypedASTAnalysisFailure(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
			TEXT("The reviewed TypedASTJIT slice supports only void or primitive scalar returns"));
	}
	for (const asCDataType& ParameterType : Shape.ParameterTypes)
	{
		if (!IsTypedASTReviewedScalar(ParameterType)
			&& !IsTypedASTReviewedBorrowedInput(ParameterType))
		{
			return MakeTypedASTAnalysisFailure(
				EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
				TEXT("The reviewed TypedASTJIT slice supports primitive scalars and exact read-only FString/FLinearColor inputs"));
		}
	}

	if (Function.header.invocationKind != Shape.InvocationKind)
	{
		return MakeTypedASTAnalysisFailure(
			EAngelscriptTypedASTJITFallbackReason::SemanticDependencyMismatch,
			TEXT("The resolved shape does not match the verified HIR invocation kind"));
	}
	if (Function.header.invocationKind
		== asETypedSemanticInvocationKind::GlobalInitializer)
	{
		return MakeTypedASTAnalysisFailure(
			EAngelscriptTypedASTJITFallbackReason::
				UnsupportedGlobalInitializer,
			RootSpan,
			TEXT("The initial TypedASTJIT profile does not emit global initializer bodies"));
	}
	const bool bGlobalInvocation = Shape.InvocationKind
		== asETypedSemanticInvocationKind::Global;
	const bool bFinalInstanceInvocation = Shape.InvocationKind
		== asETypedSemanticInvocationKind::InstanceMethod;
	if (!bGlobalInvocation && !bFinalInstanceInvocation)
	{
		return MakeTypedASTAnalysisFailure(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionKind,
			TEXT("The reviewed TypedASTJIT slice supports global functions and final instance methods only"));
	}
	if (Function.header.receiver.kind != Shape.ReceiverKind)
	{
		return MakeTypedASTAnalysisFailure(
			EAngelscriptTypedASTJITFallbackReason::InvalidEffectiveReceiver,
			TEXT("The resolved shape does not match the verified HIR receiver"));
	}
	if (bGlobalInvocation
		&& (Shape.ReceiverKind != asETypedSemanticReceiverKind::None
			|| Shape.ReceiverParameterIndex != INDEX_NONE
			|| Shape.bHasNativeObjectSlot))
	{
		return MakeTypedASTAnalysisFailure(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver,
			TEXT("A global TypedASTJIT body must not carry an effective receiver"));
	}
	if (bFinalInstanceInvocation
		&& (Shape.ReceiverKind
				!= asETypedSemanticReceiverKind::NativeObjectThis
			|| Shape.ReceiverParameterIndex != INDEX_NONE
			|| !Shape.bHasNativeObjectSlot))
	{
		return MakeTypedASTAnalysisFailure(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver,
			TEXT("A final instance TypedASTJIT body requires the native object-this slot"));
	}
	if (Function.header.returnsOnStack != Shape.bReturnsOnStack
		|| Shape.bReturnsOnStack)
	{
		return MakeTypedASTAnalysisFailure(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature,
			TEXT("Return-on-stack functions are outside the scalar slice"));
	}

	for (int32 PlanIndex = 0; PlanIndex < NativeCalls.Num(); ++PlanIndex)
	{
		const FAngelscriptTypedASTJITNativeCallEmissionPlan& Plan =
			NativeCalls[PlanIndex];
		if (!Plan.Expression.IsValid()
			|| Plan.Expression.value >= Function.expressions.GetLength()
			|| Plan.EngineLocalTargetFunctionId < 0)
		{
			return MakeTypedASTAnalysisFailure(
				EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
				TEXT("A native-call emission plan has incomplete exact target metadata"));
		}
		if (Plan.Route == EAngelscriptTypedASTJITCallEmissionRoute::Direct)
		{
			if (Plan.Symbol.IsEmpty() || Plan.Include.IsEmpty())
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
					TEXT("A direct native-call plan requires an exact symbol and include"));
			}
		}
		else if (Plan.Route == EAngelscriptTypedASTJITCallEmissionRoute::Bridge
			|| Plan.Route
				== EAngelscriptTypedASTJITCallEmissionRoute::CurrentNativeBinding)
		{
			if (Plan.CallSiteSymbol.IsEmpty()
				|| Plan.CanonicalDeclaration.IsEmpty()
				|| Plan.StableTargetKey.Hash.IsZero()
				|| Plan.ExpectedABI.IsZero()
				|| Plan.ReferenceSlotIndex < 0)
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
					TEXT("A current-Engine native-call plan requires a named row, stable identity, ABI, and adopted slot"));
			}
		}
		else
		{
			return MakeTypedASTAnalysisFailure(
				EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
				TEXT("A native-call emission plan selected an unknown route"));
		}
		for (int32 PreviousIndex = 0;
			PreviousIndex < PlanIndex; ++PreviousIndex)
		{
			if (NativeCalls[PreviousIndex].Expression == Plan.Expression)
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
					TEXT("A native-call expression has more than one emission plan"));
			}
		}
	}

	FAngelscriptTypedASTJITAnalysis Result;
	int32 ParameterOrdinal = 0;
	for (asUINT Index = 0; Index < Function.symbols.GetLength(); ++Index)
	{
		const asSTypedSemanticSymbol& Symbol = Function.symbols[Index];
		if (bFinalInstanceInvocation
			&& Symbol.id == Function.header.receiver.symbol)
		{
			if (Symbol.kind != asETypedSemanticSymbolKind::Receiver
				|| Symbol.type != Function.header.receiver.type)
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::InvalidEffectiveReceiver,
					Symbol.span,
					TEXT("The native receiver symbol disagrees with the verified receiver header"));
			}
			continue;
		}
		const bool bParameterSymbol =
			Symbol.kind == asETypedSemanticSymbolKind::Parameter;
		if (!IsTypedASTReviewedScalar(Symbol.type)
			&& !(bParameterSymbol
				&& IsTypedASTReviewedBorrowedInput(Symbol.type)))
		{
			return MakeTypedASTAnalysisFailure(
				EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
				Symbol.span,
				TEXT("The reviewed TypedASTJIT slice supports scalar symbols plus exact borrowed managed parameters"));
		}
		if (bParameterSymbol)
		{
			if (ParameterOrdinal >= Shape.ParameterTypes.Num()
				|| !Symbol.type.IsEqualExceptRefAndConst(
					Shape.ParameterTypes[ParameterOrdinal]))
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature,
					Symbol.span,
					TEXT("The HIR parameter symbols do not match the resolved shape"));
			}
			++ParameterOrdinal;
		}
		else if (Symbol.kind != asETypedSemanticSymbolKind::Local)
		{
			return MakeTypedASTAnalysisFailure(
				EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver,
				Symbol.span,
				TEXT("Receiver symbols are outside the first scalar slice"));
		}
		Result.OrderedSymbols.Add(Symbol.id);
	}
	if (ParameterOrdinal != Shape.ParameterTypes.Num())
	{
		return MakeTypedASTAnalysisFailure(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature,
			TEXT("The HIR parameter count does not match the resolved shape"));
	}

	for (asUINT Index = 0; Index < Function.expressions.GetLength(); ++Index)
	{
		const asSTypedSemanticExpression& Expression = Function.expressions[Index];
		if (bFinalInstanceInvocation
			&& Expression.symbol.IsValid()
			&& Expression.symbol == Function.header.receiver.symbol)
		{
			return MakeTypedASTAnalysisFailure(
				EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver,
				Expression.span,
				TEXT("The reviewed instance slice preserves object identity but does not read native receiver storage"));
		}
		switch (Expression.kind)
		{
		case asETypedSemanticExpressionKind::FoldedGlobalConstant:
		{
			FString DependencyError;
			if (!ValidateAngelscriptTypedASTJITFoldedGlobalDependency(
					GlobalDependencies,
					Expression.resolvedGlobalPropertyId,
					&DependencyError))
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::
						SemanticDependencyMismatch,
					Expression.span, DependencyError);
			}
			if (!IsTypedASTReviewedScalar(Expression.type))
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::
						UnsupportedType,
					Expression.span,
					TEXT("Folded global constant is outside the reviewed scalar type set"));
			}
			break;
		}
		case asETypedSemanticExpressionKind::GlobalStorage:
			return MakeTypedASTAnalysisFailure(
				EAngelscriptTypedASTJITFallbackReason::
					UnsupportedGlobalStorage,
				Expression.span,
				TEXT("The initial TypedASTJIT profile does not read or mutate runtime global storage"));
		case asETypedSemanticExpressionKind::Literal:
			if (!IsTypedASTReviewedScalar(Expression.type))
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
					Expression.span,
					TEXT("The reviewed scalar emitter accepts primitive scalar values only"));
			}
			break;
		case asETypedSemanticExpressionKind::Symbol:
		{
			const asSTypedSemanticSymbol& Symbol =
				Function.symbols[Expression.symbol.value];
			if (!IsTypedASTReviewedScalar(Expression.type)
				&& !(Symbol.kind == asETypedSemanticSymbolKind::Parameter
					&& IsTypedASTReviewedBorrowedInput(Expression.type)))
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
					Expression.span,
					TEXT("Only exact borrowed managed parameters may appear as non-scalar symbol expressions"));
			}
			break;
		}
		case asETypedSemanticExpressionKind::Conversion:
		{
			if (Expression.operands.GetLength() != 1)
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::InvalidTypedHIR,
					Expression.span,
					TEXT("Scalar conversion requires exactly one operand"));
			}
			const asCDataType& ConversionSource =
				Function.expressions[Expression.operands[0].value].type;
			if (!IsTypedASTReviewedIntegerConversion(
					ConversionSource,
					Expression.type)
				&& !IsTypedASTReviewedFloatingConversion(
					ConversionSource,
					Expression.type)
				&& !IsTypedASTReviewedEnumIntegerConversion(
					ConversionSource,
					Expression.type)
				&& !IsTypedASTReviewedPowerConversion(
					Function,
					asTypedSemanticExpressionId(Index),
					ConversionSource,
					Expression.type))
			{
				const bool bNonPortableNumericConversion =
					IsTypedASTNonPortableFloatIntegerConversion(
						ConversionSource,
						Expression.type);
				return MakeTypedASTAnalysisFailure(
					bNonPortableNumericConversion
						? EAngelscriptTypedASTJITFallbackReason::
							NonPortableNumericConversion
						: EAngelscriptTypedASTJITFallbackReason::
							UnsupportedExpression,
					Expression.span,
					FString::Printf(TEXT(
						"Only reviewed primitive integer conversions, float32/float64 width conversions, signed integer/enum, or compiler-selected power conversions are supported: SourceEnum=%d SourceToken=%d SourceBytes=%d SourceUnsigned=%d TargetEnum=%d TargetToken=%d TargetBytes=%d TargetUnsigned=%d"),
						ConversionSource.IsEnumType() ? 1 : 0,
						static_cast<int32>(ConversionSource.GetTokenType()),
						ConversionSource.GetSizeInMemoryBytes(),
						ConversionSource.IsUnsignedType() ? 1 : 0,
						Expression.type.IsEnumType() ? 1 : 0,
						static_cast<int32>(Expression.type.GetTokenType()),
						Expression.type.GetSizeInMemoryBytes(),
						Expression.type.IsUnsignedType() ? 1 : 0));
			}
			break;
		}
		case asETypedSemanticExpressionKind::Assignment:
		{
			const asSTypedSemanticExpression& Target =
				Function.expressions[Expression.mutationTarget.value];
			if (Target.kind != asETypedSemanticExpressionKind::Symbol)
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
					Expression.span,
					TEXT("The first scalar mutation slice supports symbol targets only"));
			}
			if (!IsTypedASTReviewedScalar(Expression.type)
				|| !IsTypedASTSameValueType(Target.type, Expression.type))
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
					Expression.span,
					TEXT("Scalar mutation target and result types must match"));
			}

			const bool bIncrementOrDecrement =
				Expression.operatorToken == "++"
				|| Expression.operatorToken == "--";
			if (bIncrementOrDecrement)
			{
				if (!IsTypedASTReviewedInteger(Expression.type))
				{
					return MakeTypedASTAnalysisFailure(
						EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
						Expression.span,
						TEXT("Increment and decrement require a reviewed integer type"));
				}
				break;
			}

			if (Expression.operatorToken != "="
				&& Expression.operatorToken != "+="
				&& Expression.operatorToken != "-="
				&& Expression.operatorToken != "*=")
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
					Expression.span,
					TEXT("Assignment operator is outside the reviewed scalar slice"));
			}

			const asCDataType& ValueType =
				Function.expressions[Expression.mutationValue.value].type;
			if (!IsTypedASTSameValueType(ValueType, Expression.type))
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
					Expression.span,
					TEXT("Scalar assignment value must match its target type"));
			}
			if (Expression.operatorToken != "="
				&& !IsTypedASTReviewedInteger(Expression.type))
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
					Expression.span,
					TEXT("Compound arithmetic assignment requires a reviewed integer type"));
			}
			break;
		}
		case asETypedSemanticExpressionKind::Unary:
		{
			if (Expression.operands.GetLength() != 1)
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::InvalidTypedHIR,
					Expression.span,
					TEXT("Scalar unary expression requires exactly one operand"));
			}
			const asCDataType& OperandType =
				Function.expressions[Expression.operands[0].value].type;
			if ((Expression.operatorToken != "-"
					&& Expression.operatorToken != "~")
				|| !IsTypedASTReviewedInteger(Expression.type)
				|| !IsTypedASTSameValueType(OperandType, Expression.type))
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
					Expression.span,
					TEXT("Unary operator or operand type is outside the reviewed integer slice"));
			}
			break;
		}
		case asETypedSemanticExpressionKind::Binary:
		{
			if (Expression.operands.GetLength() != 2)
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::InvalidTypedHIR,
					Expression.span,
					TEXT("Scalar binary expression requires exactly two operands"));
			}
			const asCDataType& LeftType =
				Function.expressions[Expression.operands[0].value].type;
			const asCDataType& RightType =
				Function.expressions[Expression.operands[1].value].type;
			if (Expression.operatorToken == "**")
			{
				if (!IsTypedASTReviewedPowerShape(Function, Expression))
				{
					return MakeTypedASTAnalysisFailure(
						EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
						Expression.span,
						TEXT("Power is limited to compiler-selected POWf, POWd, and POWdi shapes"));
				}
			}
			else if (Expression.operatorToken == "+"
				|| Expression.operatorToken == "-"
				|| Expression.operatorToken == "*"
				|| Expression.operatorToken == "/"
				|| Expression.operatorToken == "%")
			{
				if (!IsTypedASTReviewedInteger(Expression.type)
					|| !IsTypedASTSameValueType(LeftType, Expression.type)
					|| !IsTypedASTSameValueType(RightType, Expression.type))
				{
					return MakeTypedASTAnalysisFailure(
						EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
						Expression.span,
						TEXT("Integer arithmetic operands and result must have one reviewed type"));
				}
			}
			else if (Expression.operatorToken == "<<"
				|| Expression.operatorToken == ">>"
				|| Expression.operatorToken == ">>>")
			{
				if (!IsTypedASTReviewedInteger(Expression.type)
					|| !IsTypedASTSameValueType(LeftType, Expression.type)
					|| !IsTypedASTReviewedInteger(RightType))
				{
					return MakeTypedASTAnalysisFailure(
						EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
						Expression.span,
						TEXT("Shift value/result must match and the count must be an integer"));
				}
			}
			else if (Expression.operatorToken == "<"
				|| Expression.operatorToken == "<="
				|| Expression.operatorToken == ">"
				|| Expression.operatorToken == ">="
				|| Expression.operatorToken == "=="
				|| Expression.operatorToken == "!=")
			{
				if (!Expression.type.IsBooleanType()
					|| !IsTypedASTReviewedScalar(LeftType)
					|| !IsTypedASTSameValueType(LeftType, RightType))
				{
					return MakeTypedASTAnalysisFailure(
						EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
						Expression.span,
						TEXT("Comparison operands must match and produce bool"));
				}
			}
			else
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
					Expression.span,
					TEXT("Binary operator is outside the scalar slice"));
			}
			break;
		}
		case asETypedSemanticExpressionKind::ShortCircuit:
			if (Expression.operands.GetLength() != 2)
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::InvalidTypedHIR,
					Expression.span,
					TEXT("Short-circuit expression requires exactly two operands"));
			}
			if ((Expression.operatorToken != "&&"
					&& Expression.operatorToken != "||")
				|| !Expression.type.IsBooleanType())
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
					Expression.span,
					TEXT("Short-circuit emission requires a bool && or || expression"));
			}
			for (asUINT OperandIndex = 0;
				OperandIndex < Expression.operands.GetLength();
				++OperandIndex)
			{
				const asTypedSemanticExpressionId OperandId =
					Expression.operands[OperandIndex];
				if (!Function.expressions[OperandId.value].type.IsBooleanType())
				{
					return MakeTypedASTAnalysisFailure(
						EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
						Expression.span,
						TEXT("Short-circuit operands must be bool"));
				}
			}
			break;
		case asETypedSemanticExpressionKind::ResolvedCall:
		{
			const FAngelscriptTypedASTJITNativeCallEmissionPlan* const NativePlan =
				FindNativeCallPlan(NativeCalls, Expression.id);
			const FAngelscriptTypedASTJITScriptCallEmissionPlan* const ScriptPlan =
				FindScriptCallPlan(ScriptCalls, Expression.id);
			const bool bNativePlanValid = NativePlan != nullptr
				&& Expression.callTargetKind
					== asETypedSemanticCallTargetKind::SystemFunction
				&& Expression.resolvedFunctionId
					== NativePlan->EngineLocalTargetFunctionId;
			const bool bScriptPlanValid = ScriptPlan != nullptr
				&& Expression.callTargetKind
					== asETypedSemanticCallTargetKind::ScriptFunction
				&& Expression.resolvedFunctionId
					== ScriptPlan->EngineLocalTargetFunctionId
				&& !ScriptPlan->Symbol.IsEmpty();
			if (bNativePlanValid == bScriptPlanValid)
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
					Expression.span,
					TEXT("Resolved call requires exactly one exact native or direct script-helper emission plan"));
			}
			if (!IsTypedASTReviewedScalar(Expression.type)
				&& !IsTypedASTReviewedVoid(Expression.type))
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
					Expression.span,
					TEXT("Direct native-call return is outside the reviewed scalar/void slice"));
			}
			for (asUINT OperandIndex = 0;
				OperandIndex < Expression.operands.GetLength(); ++OperandIndex)
			{
				const asTypedSemanticExpressionId Operand =
					Expression.operands[OperandIndex];
				if (!IsTypedASTReviewedScalar(
						Function.expressions[Operand.value].type)
					&& !(NativePlan != nullptr
						&& NativePlan->Route
							== EAngelscriptTypedASTJITCallEmissionRoute::Direct
						&& IsTypedASTReviewedBorrowedInput(
							Function.expressions[Operand.value].type)))
				{
					return MakeTypedASTAnalysisFailure(
						EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
						Expression.span,
						TEXT("Native-call argument is outside the reviewed scalar/borrowed-input slice"));
				}
			}
			break;
		}
		case asETypedSemanticExpressionKind::CallRewrite:
			if (Expression.callRewriteKind
					!= asETypedSemanticCompileOutKind::CompileOutEntirely
				&& !IsTypedASTReviewedScalar(Expression.type))
			{
				return MakeTypedASTAnalysisFailure(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
					Expression.span,
					TEXT("Retained call-rewrite value is outside the reviewed scalar slice"));
			}
			break;
		case asETypedSemanticExpressionKind::Unsupported:
			if (Expression.unsupportedKind
					== asETypedSemanticUnsupportedKind::Reference
				&& Expression.symbol.IsValid()
				&& FindParameterOrdinal(Function, Expression.symbol)
					!= INDEX_NONE
				&& Expression.symbol.value < Function.symbols.GetLength()
				&& Function.symbols[Expression.symbol.value].kind
					== asETypedSemanticSymbolKind::Parameter
				&& IsTypedASTReviewedBorrowedInput(Expression.type)
				&& IsTypedASTReviewedBorrowedInput(
					Function.symbols[Expression.symbol.value].type)
				&& Expression.type.IsEqualExceptRefAndConst(
					Function.symbols[Expression.symbol.value].type))
			{
				break;
			}
			return MakeTypedASTAnalysisFailure(
				EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
				Expression.span,
				TEXT("Expression kind is outside the scalar slice"));
		default:
			return MakeTypedASTAnalysisFailure(
				EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
				Expression.span,
				TEXT("Expression kind is outside the scalar slice"));
		}
	}

	for (asUINT Index = 0; Index < Function.statements.GetLength(); ++Index)
	{
		const asSTypedSemanticStatement& Statement = Function.statements[Index];
		switch (Statement.kind)
		{
		case asETypedSemanticStatementKind::Block:
		case asETypedSemanticStatementKind::LocalDeclaration:
		case asETypedSemanticStatementKind::Expression:
		case asETypedSemanticStatementKind::If:
		case asETypedSemanticStatementKind::Return:
		case asETypedSemanticStatementKind::For:
		case asETypedSemanticStatementKind::While:
		case asETypedSemanticStatementKind::DoWhile:
		case asETypedSemanticStatementKind::Switch:
		case asETypedSemanticStatementKind::Case:
		case asETypedSemanticStatementKind::Break:
		case asETypedSemanticStatementKind::Continue:
			break;
		default:
			return MakeTypedASTAnalysisFailure(
				EAngelscriptTypedASTJITFallbackReason::UnsupportedStatement,
				Statement.span,
				TEXT("Statement kind is outside the scalar slice"));
		}
		Result.OrderedStatements.Add(Statement.id);
	}

	Result.bEligible = true;
	return Result;
}
