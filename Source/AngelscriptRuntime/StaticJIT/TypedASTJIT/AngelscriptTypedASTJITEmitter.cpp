#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"
#include "StaticJIT/AngelscriptStaticJITEntryPlan.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITExecutionProfile.h"

#include "Internationalization/Regex.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	bool TryFindParameterSymbolId(
		const asCTypedSemanticFunction& Function,
		const int32 ParameterOrdinal,
		asTypedSemanticSymbolId& OutSymbol)
	{
		int32 CurrentOrdinal = 0;
		for (asUINT SymbolIndex = 0;
			SymbolIndex < Function.symbols.GetLength(); ++SymbolIndex)
		{
			const asSTypedSemanticSymbol& Symbol =
				Function.symbols[SymbolIndex];
			if (Symbol.kind != asETypedSemanticSymbolKind::Parameter)
			{
				continue;
			}
			if (CurrentOrdinal == ParameterOrdinal)
			{
				OutSymbol = Symbol.id;
				return true;
			}
			++CurrentOrdinal;
		}
		OutSymbol = {};
		return false;
	}

	const TCHAR* NativeCallSiteRouteName(
		const EAngelscriptJITNativeCallSiteRoute Route)
	{
		switch (Route)
		{
		case EAngelscriptJITNativeCallSiteRoute::DirectExported:
			return TEXT("DirectExported");
		case EAngelscriptJITNativeCallSiteRoute::DirectInline:
			return TEXT("DirectInline");
		case EAngelscriptJITNativeCallSiteRoute::RuntimeThunk:
			return TEXT("RuntimeThunk");
		case EAngelscriptJITNativeCallSiteRoute::Bridge:
			return TEXT("Bridge");
		case EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding:
			return TEXT("CurrentNativeBinding");
		default:
			return TEXT("Invalid");
		}
	}

	FString OneLineNativeCallComment(FString Value)
	{
		Value.ReplaceInline(TEXT("\r"), TEXT(" "));
		Value.ReplaceInline(TEXT("\n"), TEXT(" "));
		return Value;
	}

	FAngelscriptTypedASTJITEmission MakeTypedASTEmissionFailure(
		const EAngelscriptTypedASTJITFallbackReason Reason,
		const asSTypedSemanticSourceSpan& Span,
		const FString& Detail)
	{
		FAngelscriptTypedASTJITEmission Result;
		Result.FailureReason = Reason;
		Result.FailureSpan = Span;
		Result.FailureDetail = Detail;
		return Result;
	}

	FString MakeTypedASTIndent(const int32 Depth)
	{
		FString Result;
		for (int32 Index = 0; Index < Depth; ++Index)
		{
			Result += TEXT("\t");
		}
		return Result;
	}

	FString EscapeTypedASTCppString(FStringView Value)
	{
		FString Result;
		Result.Reserve(Value.Len());
		for (const TCHAR Character : Value)
		{
			switch (Character)
			{
			case TEXT('\\'):
				Result += TEXT("\\\\");
				break;
			case TEXT('"'):
				Result += TEXT("\\\"");
				break;
			case TEXT('\n'):
				Result += TEXT("\\n");
				break;
			case TEXT('\r'):
				Result += TEXT("\\r");
				break;
			case TEXT('\t'):
				Result += TEXT("\\t");
				break;
			default:
				Result.AppendChar(Character);
				break;
			}
		}
		return Result;
	}

	bool TrySpellTypedASTCppTypeInternal(
		const asCDataType& Type,
		FString& OutType)
	{
		return TrySpellAngelscriptStaticJITBodyCppType(Type, OutType);
	}

	bool IsTypedASTIntegerLike(const asCDataType& Type)
	{
		if (Type.IsEnumType())
		{
			return true;
		}
		switch (Type.GetTokenType())
		{
		case ttInt8:
		case ttUInt8:
		case ttInt16:
		case ttUInt16:
		case ttInt:
		case ttUInt:
		case ttInt64:
		case ttUInt64:
			return true;
		default:
			return false;
		}
	}

	bool IsTypedASTUnsignedIntegerLike(const asCDataType& Type)
	{
		return !Type.IsEnumType() && Type.IsUnsignedType();
	}

	class FTypedASTScalarEmitter
	{
	public:
		explicit FTypedASTScalarEmitter(
			const asCTypedSemanticFunction& InFunction,
			const TConstArrayView<
				FAngelscriptTypedASTJITNativeCallEmissionPlan> InNativeCalls,
			const TConstArrayView<
				FAngelscriptTypedASTJITScriptCallEmissionPlan> InScriptCalls,
			const FString& InReturnCppType,
			const bool bInEmitFramePosition)
			: Function(InFunction)
			, NativeCalls(InNativeCalls)
			, ScriptCalls(InScriptCalls)
			, ReturnCppType(InReturnCppType)
			, bEmitFramePosition(bInEmitFramePosition)
		{
			bRequiresExecutionState = !ScriptCalls.IsEmpty()
				|| NativeCalls.ContainsByPredicate(
				[](const FAngelscriptTypedASTJITNativeCallEmissionPlan& Plan)
				{
					return Plan.Route
						== EAngelscriptTypedASTJITCallEmissionRoute::Bridge
						|| Plan.Route
							== EAngelscriptTypedASTJITCallEmissionRoute::CurrentNativeBinding;
				})
				|| ContainsExceptionCapableIntegralOperator()
				|| ContainsExceptionCapablePowerOperator()
				|| ContainsInvalidEnumSwitchEdge();
		}

		bool EmitExpression(
			const asTypedSemanticExpressionId Id,
			const int32 Depth,
			FString& OutExpression)
		{
			if (!Id.IsValid() || Id.value >= Function.expressions.GetLength())
			{
				return Fail(TEXT("Expression ID is outside the verified arena"));
			}
			const asSTypedSemanticExpression& Expression =
				Function.expressions[Id.value];
			switch (Expression.kind)
			{
			case asETypedSemanticExpressionKind::Symbol:
				OutExpression = FString::Printf(
					TEXT("as_sem_s%u"), Expression.symbol.value);
				return true;
			case asETypedSemanticExpressionKind::FoldedGlobalConstant:
			{
				FString Type;
				if (!TrySpellTypedASTCppTypeInternal(Expression.type, Type))
				{
					return Fail(TEXT(
						"Folded global constant type has no reviewed C++ spelling"));
				}
				bRequiresScalarOps = true;
				OutExpression = FString::Printf(TEXT(
					"AngelscriptTypedASTJIT::FromCanonicalBits<%s>(UINT64_C(%llu))"),
					*Type,
					static_cast<unsigned long long>(Expression.literalBits));
				return true;
			}
			case asETypedSemanticExpressionKind::Literal:
			{
				const FString Literal = UTF8_TO_TCHAR(Expression.literal.AddressOf());
				FString Type;
				if (Literal.IsEmpty())
				{
					return Fail(TEXT("Literal spelling is empty"));
				}
				if (!TrySpellTypedASTCppTypeInternal(Expression.type, Type))
				{
					return Fail(TEXT("Literal type has no reviewed C++ spelling"));
				}
				if (Expression.type.IsBooleanType()
					|| Expression.type.IsFloatType()
					|| Expression.type.IsDoubleType()
					|| Expression.type.IsEnumType())
				{
					bRequiresScalarOps = true;
					OutExpression = FString::Printf(TEXT(
						"AngelscriptTypedASTJIT::FromCanonicalBits<%s>(UINT64_C(%llu))"),
						*Type,
						static_cast<unsigned long long>(
							Expression.literalBits));
					return true;
				}
				for (const TCHAR Character : Literal)
				{
					if (!FChar::IsDigit(Character))
					{
						return Fail(TEXT("Literal spelling is not a reviewed decimal integer"));
					}
				}
				OutExpression = FString::Printf(
					TEXT("%s(%s)"), *Type, *Literal);
				return true;
			}
			case asETypedSemanticExpressionKind::Conversion:
			{
				FString Operand;
				FString Type;
				if (Expression.operands.GetLength() != 1
					|| !EmitExpression(Expression.operands[0], Depth, Operand)
					|| !TrySpellTypedASTCppTypeInternal(Expression.type, Type))
				{
					return false;
				}
				const asCDataType& SourceType =
					Function.expressions[Expression.operands[0].value].type;
				const bool bIntegerConversion =
					IsTypedASTIntegerLike(SourceType)
					&& IsTypedASTIntegerLike(Expression.type);
				const bool bRequiresWrappingNarrow = bIntegerConversion
					&& SourceType.GetSizeInMemoryBytes()
						> Expression.type.GetSizeInMemoryBytes();
				const bool bRequiresBitDomainConversion = bIntegerConversion
					&& !bRequiresWrappingNarrow
					&& IsTypedASTUnsignedIntegerLike(SourceType)
						!= IsTypedASTUnsignedIntegerLike(Expression.type);
				if (bRequiresWrappingNarrow)
				{
					bRequiresScalarOps = true;
					OutExpression = FString::Printf(
						TEXT("AngelscriptTypedASTJIT::WrapNarrow<%s>(%s)"),
						*Type,
						*Operand);
				}
				else if (bRequiresBitDomainConversion)
				{
					bRequiresScalarOps = true;
					OutExpression = FString::Printf(
						TEXT("AngelscriptTypedASTJIT::ConvertInteger<%s>(%s)"),
						*Type,
						*Operand);
				}
				else
				{
					OutExpression = FString::Printf(
						TEXT("static_cast<%s>(%s)"), *Type, *Operand);
				}
				return true;
			}
			case asETypedSemanticExpressionKind::Assignment:
			{
				const asSTypedSemanticExpression& TargetExpression =
					Function.expressions[Expression.mutationTarget.value];
				if (TargetExpression.kind
					!= asETypedSemanticExpressionKind::Symbol)
				{
					return Fail(TEXT("Scalar mutation target is not a symbol"));
				}

				FString Type;
				if (!TrySpellTypedASTCppTypeInternal(Expression.type, Type))
				{
					return Fail(TEXT("Scalar mutation type has no reviewed C++ spelling"));
				}

				const FString Prefix = MakeTypedASTIndent(Depth);
				const FString BodyPrefix = MakeTypedASTIndent(Depth + 1);
				const FString TargetName = FString::Printf(
					TEXT("as_sem_s%u"), TargetExpression.symbol.value);
				const FString TargetTemporary = FString::Printf(
					TEXT("as_sem_e%u_target"), Id.value);
				OutExpression = FString::Printf(
					TEXT("([&]() -> %s\n%s{\n"), *Type, *Prefix);

				const bool bIncrementOrDecrement =
					Expression.operatorToken == "++"
					|| Expression.operatorToken == "--";
				FString ValueTemporary;
				if (!bIncrementOrDecrement)
				{
					FString Value;
					if (!EmitExpression(
						Expression.mutationValue,
						Depth + 1,
						Value))
					{
						return false;
					}
					ValueTemporary = FString::Printf(
						TEXT("as_sem_e%u_value"), Id.value);
					OutExpression += FString::Printf(
						TEXT("%sconst %s %s = %s;\n"),
						*BodyPrefix,
						*Type,
						*ValueTemporary,
						*Value);
					AppendExpressionExceptionReturnCheck(
						Depth + 1,
						Expression.mutationValue,
						Type,
						OutExpression);
				}
				OutExpression += FString::Printf(
					TEXT("%s%s& %s = %s;\n"),
					*BodyPrefix,
					*Type,
					*TargetTemporary,
					*TargetName);

				if (Expression.operatorToken == "=")
				{
					OutExpression += FString::Printf(
						TEXT("%s%s = %s;\n"),
						*BodyPrefix,
						*TargetTemporary,
						*ValueTemporary);
					OutExpression += FString::Printf(
						TEXT("%sreturn %s;\n%s}())"),
						*BodyPrefix,
						*TargetTemporary,
						*Prefix);
					return true;
				}

				const TCHAR* Helper = nullptr;
				if (Expression.operatorToken == "+="
					|| Expression.operatorToken == "++")
				{
					Helper = TEXT("WrapAdd");
				}
				else if (Expression.operatorToken == "-="
					|| Expression.operatorToken == "--")
				{
					Helper = TEXT("WrapSubtract");
				}
				else if (Expression.operatorToken == "*=")
				{
					Helper = TEXT("WrapMultiply");
				}
				if (Helper == nullptr)
				{
					return Fail(TEXT("Mutation operator reached emission without a reviewed helper"));
				}

				bRequiresScalarOps = true;
				FString HelperLeft = TargetTemporary;
				FString HelperRight = ValueTemporary;
				FString ReturnValue = TargetTemporary;
				if (bIncrementOrDecrement)
				{
					HelperRight = TEXT("int32(1)");
					if (Expression.mutationReturnsOldValue)
					{
						const FString OldTemporary = FString::Printf(
							TEXT("as_sem_e%u_old"), Id.value);
						OutExpression += FString::Printf(
							TEXT("%sconst %s %s = %s;\n"),
							*BodyPrefix,
							*Type,
							*OldTemporary,
							*TargetTemporary);
						HelperLeft = OldTemporary;
						ReturnValue = OldTemporary;
					}
				}
				OutExpression += FString::Printf(
					TEXT("%s%s = AngelscriptTypedASTJIT::%s<%s>(%s, %s);\n"),
					*BodyPrefix,
					*TargetTemporary,
					Helper,
					*Type,
					*HelperLeft,
					*HelperRight);
				OutExpression += FString::Printf(
					TEXT("%sreturn %s;\n%s}())"),
					*BodyPrefix,
					*ReturnValue,
					*Prefix);
				return true;
			}
			case asETypedSemanticExpressionKind::Unary:
			{
				FString Operand;
				FString Type;
				if (Expression.operands.GetLength() != 1
					|| !EmitExpression(Expression.operands[0], Depth, Operand)
					|| !TrySpellTypedASTCppTypeInternal(Expression.type, Type))
				{
					return false;
				}

				const TCHAR* Helper = nullptr;
				if (Expression.operatorToken == "-")
				{
					Helper = TEXT("WrapNegate");
				}
				else if (Expression.operatorToken == "~")
				{
					Helper = TEXT("BitwiseNot");
				}
				if (Helper == nullptr)
				{
					return Fail(TEXT("Unary operator reached emission without a reviewed helper"));
				}

				bRequiresScalarOps = true;
				OutExpression = FString::Printf(
					TEXT("AngelscriptTypedASTJIT::%s<%s>(%s)"),
					Helper,
					*Type,
					*Operand);
				return true;
			}
			case asETypedSemanticExpressionKind::Binary:
			case asETypedSemanticExpressionKind::ShortCircuit:
			{
				FString Left;
				FString Right;
				if (Expression.operands.GetLength() != 2
					|| !EmitExpression(Expression.operands[0], Depth, Left)
					|| !EmitExpression(Expression.operands[1], Depth, Right))
				{
					return false;
				}
				if (Expression.kind
					== asETypedSemanticExpressionKind::ShortCircuit)
				{
					OutExpression = FString::Printf(
						TEXT("(%s %s %s)"),
						*Left,
						UTF8_TO_TCHAR(Expression.operatorToken.AddressOf()),
						*Right);
					return true;
				}

				FString ResultType;
				FString LeftType;
				FString RightType;
				if (!TrySpellTypedASTCppTypeInternal(
						Expression.type, ResultType)
					|| !TrySpellTypedASTCppTypeInternal(
						Function.expressions[Expression.operands[0].value].type,
						LeftType)
					|| !TrySpellTypedASTCppTypeInternal(
						Function.expressions[Expression.operands[1].value].type,
						RightType))
				{
					return Fail(TEXT(
						"Binary operand or result has no reviewed C++ spelling"));
				}

				// C++ does not provide the AngelScript evaluation-order contract for
				// ordinary binary operands or function-call arguments. Materialize
				// both operands in HIR order before emitting the native operation so
				// mutations and calls cannot be reordered by the host compiler.
				const FString Prefix = MakeTypedASTIndent(Depth);
				const FString BodyPrefix = MakeTypedASTIndent(Depth + 1);
				const FString LeftTemporary = FString::Printf(
					TEXT("as_sem_e%u_left"), Id.value);
				const FString RightTemporary = FString::Printf(
					TEXT("as_sem_e%u_right"), Id.value);
				FString Operation;
				if (Expression.kind == asETypedSemanticExpressionKind::Binary
					&& Expression.operatorToken == "**")
				{
					const asCDataType& LeftDataType =
						Function.expressions[Expression.operands[0].value].type;
					const asCDataType& RightDataType =
						Function.expressions[Expression.operands[1].value].type;
					const bool bFloatPower =
						Expression.type.GetTokenType() == ttFloat32
						&& LeftDataType.GetTokenType() == ttFloat32
						&& RightDataType.GetTokenType() == ttFloat32;
					const bool bDoublePower =
						Expression.type.GetTokenType() == ttFloat64
						&& LeftDataType.GetTokenType() == ttFloat64
						&& RightDataType.GetTokenType() == ttFloat64;
					const bool bDoubleIntPower =
						Expression.type.GetTokenType() == ttFloat64
						&& LeftDataType.GetTokenType() == ttFloat64
						&& RightDataType.GetTokenType() == ttInt;
					if (!bFloatPower && !bDoublePower && !bDoubleIntPower)
					{
						return Fail(TEXT(
							"Power reached emission without a reviewed compiler-selected shape"));
					}
					bRequiresScalarOps = true;
					Operation = FString::Printf(
						TEXT("AngelscriptTypedASTJIT::CheckedPower<%s>(Execution, %s, %s)"),
						*ResultType,
						*LeftTemporary,
						*RightTemporary);
				}
				else
				{
					const TCHAR* Helper = nullptr;
					if (Expression.operatorToken == "+")
					{
						Helper = TEXT("WrapAdd");
					}
					else if (Expression.operatorToken == "-")
					{
						Helper = TEXT("WrapSubtract");
					}
					else if (Expression.operatorToken == "*")
					{
						Helper = TEXT("WrapMultiply");
					}
					else if (Expression.operatorToken == "/")
					{
						Helper = TEXT("CheckedDivide");
					}
					else if (Expression.operatorToken == "%")
					{
						Helper = TEXT("CheckedRemainder");
					}
					else if (Expression.operatorToken == "<<")
					{
						Helper = TEXT("ShiftLeft");
					}
					else if (Expression.operatorToken == ">>")
					{
						Helper = TEXT("LogicalShiftRight");
					}
					else if (Expression.operatorToken == ">>>")
					{
						Helper = TEXT("ArithmeticShiftRight");
					}

					if (Helper != nullptr)
					{
						bRequiresScalarOps = true;
						const bool bCheckedIntegralOperator =
							Expression.operatorToken == "/"
							|| Expression.operatorToken == "%";
						Operation = bCheckedIntegralOperator
							? FString::Printf(
								TEXT("AngelscriptTypedASTJIT::%s<%s>(Execution, %s, %s)"),
								Helper,
								*ResultType,
								*LeftTemporary,
								*RightTemporary)
							: FString::Printf(
								TEXT("AngelscriptTypedASTJIT::%s<%s>(%s, %s)"),
								Helper,
								*ResultType,
								*LeftTemporary,
								*RightTemporary);
					}
					else
					{
						Operation = FString::Printf(
							TEXT("(%s %s %s)"),
							*LeftTemporary,
							UTF8_TO_TCHAR(Expression.operatorToken.AddressOf()),
							*RightTemporary);
					}
				}

				OutExpression = FString::Printf(
					TEXT("([&]() -> %s\n%s{\n")
					TEXT("%sconst %s %s = %s;\n"),
					*ResultType,
					*Prefix,
					*BodyPrefix,
					*LeftType,
					*LeftTemporary,
					*Left);
				AppendExpressionExceptionReturnCheck(
					Depth + 1,
					Expression.operands[0],
					ResultType,
					OutExpression);
				OutExpression += FString::Printf(
					TEXT("%sconst %s %s = %s;\n"),
					*BodyPrefix,
					*RightType,
					*RightTemporary,
					*Right);
				AppendExpressionExceptionReturnCheck(
					Depth + 1,
					Expression.operands[1],
					ResultType,
					OutExpression);
				OutExpression += FString::Printf(
					TEXT("%sreturn %s;\n%s}())"),
					*BodyPrefix,
					*Operation,
					*Prefix);
				return true;
			}
			case asETypedSemanticExpressionKind::CallRewrite:
				switch (Expression.callRewriteKind)
				{
				case asETypedSemanticCompileOutKind::CompileOutEntirely:
					OutExpression = TEXT("(void)0");
					return true;
				case asETypedSemanticCompileOutKind::ReplaceWithFirstParam:
				case asETypedSemanticCompileOutKind::CompileOutAsMethodChain:
					if (Expression.operands.GetLength() != 1)
					{
						return Fail(TEXT(
							"Retained call rewrite reached emission without one final value"));
					}
					return EmitExpression(
						Expression.operands[0], Depth, OutExpression);
				default:
					return Fail(TEXT(
						"Executable call disposition reached CallRewrite emission"));
				}
			case asETypedSemanticExpressionKind::ResolvedCall:
			{
				const FAngelscriptTypedASTJITNativeCallEmissionPlan* const NativePlan =
					NativeCalls.FindByPredicate(
						[Id](const auto& Candidate)
						{
							return Candidate.Expression == Id;
						});
				const FAngelscriptTypedASTJITScriptCallEmissionPlan* const ScriptPlan =
					ScriptCalls.FindByPredicate(
						[Id](const auto& Candidate)
						{
							return Candidate.Expression == Id;
						});
				const bool bNativeCall = NativePlan != nullptr
					&& NativePlan->EngineLocalTargetFunctionId
						== Expression.resolvedFunctionId;
				const bool bScriptCall = ScriptPlan != nullptr
					&& ScriptPlan->EngineLocalTargetFunctionId
						== Expression.resolvedFunctionId;
				if (bNativeCall == bScriptCall)
				{
					return Fail(TEXT(
						"Resolved call reached emission without exactly one validated native or script-helper plan"));
				}
				const bool bBridge = bNativeCall && NativePlan->Route
					== EAngelscriptTypedASTJITCallEmissionRoute::Bridge;
				const bool bCurrentNativeBinding = bNativeCall
					&& NativePlan->Route
						== EAngelscriptTypedASTJITCallEmissionRoute::CurrentNativeBinding;
				const bool bBoundNative = bBridge || bCurrentNativeBinding;
				if (bBoundNative)
				{
					RequiredIncludes.AddUnique(TEXT(
						"StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"));
				}
				else if (bNativeCall)
				{
					RequiredIncludes.AddUnique(NativePlan->Include);
				}
				else
				{
					RequiredIncludes.AddUnique(TEXT(
						"StaticJIT/StaticJITHeader.h"));
				}
				if (bNativeCall && !bBoundNative && !bEmitFramePosition
					&& Expression.operands.GetLength() == 0)
				{
					if (!RecordNativeCallSite(
						Id, *NativePlan, NativePlan->Symbol))
					{
						return false;
					}
					OutExpression = NativePlan->Symbol + TEXT("()");
					return true;
				}

				FString ReturnType;
				if (!TrySpellTypedASTCppTypeInternal(Expression.type, ReturnType))
				{
					return Fail(TEXT(
						"Direct native-call return type has no reviewed C++ spelling"));
				}
				const FString Prefix = MakeTypedASTIndent(Depth);
				const FString BodyPrefix = MakeTypedASTIndent(Depth + 1);
				OutExpression = FString::Printf(
					TEXT("([&]() -> %s\n%s{\n"),
					*ReturnType, *Prefix);
				AppendPositionHook(
					Expression.safePointRole,
					Expression.span,
					Depth + 1,
					OutExpression);
				TArray<FString> FormalTemporaries;
				FormalTemporaries.SetNum(Expression.operands.GetLength());
				TArray<FString> FormalTypes;
				FormalTypes.SetNum(Expression.operands.GetLength());
				for (asUINT EvaluationIndex = 0;
					EvaluationIndex < Expression.evaluationSteps.GetLength();
					++EvaluationIndex)
				{
					const asSTypedSemanticEvaluationStep& EvaluationStep =
						Expression.evaluationSteps[EvaluationIndex];
					const int32 FormalIndex =
						EvaluationStep.formalParameterIndex;
					if (FormalIndex < 0)
					{
						return Fail(TEXT(
							"Direct call emission does not yet support an independent receiver evaluation"));
					}
					asUINT BindingIndex = Expression.operands.GetLength();
					for (asUINT CandidateIndex = 0;
						CandidateIndex < Expression.operands.GetLength();
						++CandidateIndex)
					{
						if (Expression.formalParameterIndices[CandidateIndex]
							== FormalIndex)
						{
							BindingIndex = CandidateIndex;
							break;
						}
					}
					if (BindingIndex >= Expression.operands.GetLength())
					{
						return Fail(TEXT(
							"Direct native-call evaluation sequence is incomplete"));
					}

					const asTypedSemanticExpressionId OperandId =
						EvaluationStep.expression;
					if (OperandId != Expression.operands[BindingIndex])
					{
						return Fail(TEXT(
							"Direct call evaluation step disagrees with its formal operand"));
					}
					FString Operand;
					FString OperandType;
					if (!EmitExpression(OperandId, Depth + 1, Operand)
						|| !TrySpellTypedASTCppTypeInternal(
							Function.expressions[OperandId.value].type,
							OperandType))
					{
						return false;
					}
					if (FormalIndex < 0
						|| FormalIndex >= FormalTemporaries.Num())
					{
						return Fail(TEXT(
							"Direct native-call formal binding is outside its argument list"));
					}
					FString& Temporary = FormalTemporaries[FormalIndex];
					FormalTypes[FormalIndex] = OperandType;
					Temporary = FString::Printf(
						TEXT("as_sem_e%u_arg%d"), Id.value, FormalIndex);
					OutExpression += FString::Printf(
						TEXT("%sconst %s %s = %s;\n"),
						*BodyPrefix, *OperandType, *Temporary, *Operand);
				}

				if (bScriptCall)
				{
					if (Expression.type.GetTokenType() == ttVoid)
					{
						OutExpression += FString::Printf(TEXT(
							"%s%s(Execution%s%s);\n"
							"%sif (UNLIKELY(Execution.bExceptionThrown))\n"
							"%s{\n"
							"%s\treturn;\n"
							"%s}\n"
							"%sreturn;\n%s}())"),
							*BodyPrefix,
							*ScriptPlan->Symbol,
							FormalTemporaries.IsEmpty()
								? TEXT("") : TEXT(", "),
							*FString::Join(FormalTemporaries, TEXT(", ")),
							*BodyPrefix,
							*BodyPrefix,
							*BodyPrefix,
							*BodyPrefix,
							*BodyPrefix,
							*Prefix);
						return true;
					}
					OutExpression += FString::Printf(TEXT(
						"%sconst %s as_sem_e%u_result = %s(Execution%s%s);\n"
						"%sif (UNLIKELY(Execution.bExceptionThrown))\n"
						"%s{\n"
						"%s\treturn %s{};\n"
						"%s}\n"
						"%sreturn as_sem_e%u_result;\n%s}())"),
						*BodyPrefix,
						*ReturnType,
						Id.value,
						*ScriptPlan->Symbol,
						FormalTemporaries.IsEmpty() ? TEXT("") : TEXT(", "),
						*FString::Join(FormalTemporaries, TEXT(", ")),
						*BodyPrefix,
						*BodyPrefix,
						*BodyPrefix,
						*ReturnType,
						*BodyPrefix,
						*BodyPrefix,
						Id.value,
						*Prefix);
					return true;
				}

				if (!bBoundNative)
				{
					if (!RecordNativeCallSite(
						Id, *NativePlan, NativePlan->Symbol))
					{
						return false;
					}
					OutExpression += FString::Printf(
						TEXT("%sreturn %s(%s);\n%s}())"),
						*BodyPrefix,
						*NativePlan->Symbol,
						*FString::Join(FormalTemporaries, TEXT(", ")),
						*Prefix);
					return true;
				}

				TArray<FString> TemplateTypes;
				TemplateTypes.Reserve(FormalTypes.Num() + 1);
				TemplateTypes.Add(ReturnType);
				TemplateTypes.Append(FormalTypes);
				const TCHAR* const BoundInvocation = bCurrentNativeBinding
					? TEXT("InvokeBoundNative") : TEXT("InvokeBound");
				const FString EmittedCppCallee = FString::Printf(
					TEXT("AngelscriptTypedASTJIT::%s<%s>"),
					BoundInvocation,
					*FString::Join(TemplateTypes, TEXT(", ")));
				if (!RecordNativeCallSite(
					Id, *NativePlan, EmittedCppCallee))
				{
					return false;
				}
				FString BridgeArguments = TEXT("Execution, ")
					+ NativePlan->CallSiteSymbol;
				if (!FormalTemporaries.IsEmpty())
				{
					BridgeArguments += TEXT(", ")
						+ FString::Join(FormalTemporaries, TEXT(", "));
				}
				if (Expression.type.GetTokenType() == ttVoid)
				{
					OutExpression += FString::Printf(TEXT(
						"%sAngelscriptTypedASTJIT::%s<%s>(\n"
						"%s\t%s);\n"
						"%sif (UNLIKELY(Execution.bExceptionThrown))\n"
						"%s{\n"
						"%s\treturn;\n"
						"%s}\n"
						"%sreturn;\n%s}())"),
						*BodyPrefix,
						BoundInvocation,
						*FString::Join(TemplateTypes, TEXT(", ")),
						*BodyPrefix,
						*BridgeArguments,
						*BodyPrefix,
						*BodyPrefix,
						*BodyPrefix,
						*BodyPrefix,
						*BodyPrefix,
						*Prefix);
					return true;
				}
				OutExpression += FString::Printf(TEXT(
					"%sconst %s as_sem_e%u_result = "
					"AngelscriptTypedASTJIT::%s<%s>(\n"
					"%s\t%s);\n"
					"%sif (UNLIKELY(Execution.bExceptionThrown))\n"
					"%s{\n"
					"%s\treturn %s{};\n"
					"%s}\n"
					"%sreturn as_sem_e%u_result;\n%s}())"),
					*BodyPrefix,
					*ReturnType,
					Id.value,
					BoundInvocation,
					*FString::Join(TemplateTypes, TEXT(", ")),
					*BodyPrefix,
					*BridgeArguments,
					*BodyPrefix,
					*BodyPrefix,
					*BodyPrefix,
					*ReturnType,
					*BodyPrefix,
					*BodyPrefix,
					Id.value,
					*Prefix);
				return true;
			}
			default:
				return Fail(TEXT("Expression kind reached emission without analysis support"));
			}
		}

		bool EmitLoopCondition(
			const asTypedSemanticExpressionId ConditionId,
			const asETypedSemanticSafePointRole SafePointRole,
			const asSTypedSemanticSourceSpan& Span,
			const int32 Depth,
			FString& OutCondition)
		{
			FString Condition = TEXT("true");
			if (ConditionId.IsValid()
				&& !EmitExpression(ConditionId, Depth + 1, Condition))
			{
				return false;
			}
			if (!bRequiresExecutionState && !bEmitFramePosition)
			{
				OutCondition = MoveTemp(Condition);
				return true;
			}

			const FString Prefix = MakeTypedASTIndent(Depth);
			const FString BodyPrefix = MakeTypedASTIndent(Depth + 1);
			OutCondition = TEXT("[&]() -> bool\n") + Prefix + TEXT("{\n");
			AppendPositionHook(
				SafePointRole, Span, Depth + 1, OutCondition);
			if (bRequiresExecutionState)
			{
				OutCondition += FString::Printf(
					TEXT("%sif (UNLIKELY(Execution.bExceptionThrown))\n")
					TEXT("%s{\n")
					TEXT("%s\treturn false;\n")
					TEXT("%s}\n"),
					*BodyPrefix,
					*BodyPrefix,
					*BodyPrefix,
					*BodyPrefix);
			}
			OutCondition += FString::Printf(
				TEXT("%sconst bool as_sem_loop_condition = %s;\n"),
				*BodyPrefix,
				*Condition);
			OutCondition += bRequiresExecutionState
				? FString::Printf(
					TEXT("%sreturn !UNLIKELY(Execution.bExceptionThrown) ")
					TEXT("&& as_sem_loop_condition;\n"),
					*BodyPrefix)
				: FString::Printf(
					TEXT("%sreturn as_sem_loop_condition;\n"),
					*BodyPrefix);
			OutCondition += Prefix + TEXT("}()");
			return true;
		}

		bool EmitForIncrement(
			const asSTypedSemanticStatement& Statement,
			const int32 Depth,
			FString& OutIncrement)
		{
			if (!Statement.incrementStatement.IsValid())
			{
				OutIncrement.Reset();
				return true;
			}
			const asSTypedSemanticStatement& IncrementBlock =
				Function.statements[Statement.incrementStatement.value];
			if (!bRequiresExecutionState && !bEmitFramePosition)
			{
				TArray<FString> Expressions;
				Expressions.Reserve(
					static_cast<int32>(Statement.incrementExpressions.GetLength()));
				for (asUINT Index = 0;
					Index < Statement.incrementExpressions.GetLength(); ++Index)
				{
					FString Expression;
					if (!EmitExpression(
						Statement.incrementExpressions[Index],
						Depth,
						Expression))
					{
						return false;
					}
					Expressions.Add(MoveTemp(Expression));
				}
				OutIncrement = FString::Join(Expressions, TEXT(", "));
				return true;
			}

			const FString Prefix = MakeTypedASTIndent(Depth);
			const FString BodyPrefix = MakeTypedASTIndent(Depth + 1);
			OutIncrement = TEXT("[&]()\n") + Prefix + TEXT("{\n");
			for (asUINT Index = 0;
				Index < IncrementBlock.children.GetLength(); ++Index)
			{
				const asSTypedSemanticStatement& Child =
					Function.statements[IncrementBlock.children[Index].value];
				AppendPositionHook(
					Child.safePointRole,
					Child.span,
					Depth + 1,
					OutIncrement);
				FString Expression;
				if (!EmitExpression(
						Child.valueExpression, Depth + 1, Expression))
				{
					return false;
				}
				OutIncrement += FString::Printf(
					TEXT("%s%s;\n"), *BodyPrefix, *Expression);
				if (bRequiresExecutionState)
				{
					OutIncrement += FString::Printf(
						TEXT("%sif (UNLIKELY(Execution.bExceptionThrown))\n")
						TEXT("%s{\n")
						TEXT("%s\treturn;\n")
						TEXT("%s}\n"),
						*BodyPrefix,
						*BodyPrefix,
						*BodyPrefix,
						*BodyPrefix);
				}
			}
			OutIncrement += Prefix + TEXT("}()");
			return true;
		}

		bool EmitSwitchCase(
			const asTypedSemanticStatementId CaseId,
			const asETypedSemanticSwitchNormalization Normalization,
			const int32 Depth,
			FString& OutDefinition)
		{
			const asSTypedSemanticStatement& Case =
				Function.statements[CaseId.value];
			const FString Prefix = MakeTypedASTIndent(Depth);
			if (Case.isDefaultCase)
			{
				OutDefinition += Prefix + TEXT("default:\n");
			}
			else
			{
				const TCHAR* NormalizedType = Normalization
					== asETypedSemanticSwitchNormalization::UnsignedInt32
						? TEXT("uint32") : TEXT("int32");
				OutDefinition += FString::Printf(
					TEXT("%scase %s(%lld):\n"),
					*Prefix,
					NormalizedType,
					static_cast<long long>(Case.caseConstantValue));
			}
			AppendPositionHook(
				Case.safePointRole,
				Case.span,
				Depth + 1,
				OutDefinition);
			if (!EmitStatement(
				Case.bodyStatement, Depth + 1, OutDefinition))
			{
				return false;
			}
			if (!Case.fallsThrough)
			{
				OutDefinition += MakeTypedASTIndent(Depth + 1)
					+ TEXT("break;\n");
			}
			return true;
		}

		bool EmitStatement(
			const asTypedSemanticStatementId Id,
			const int32 Depth,
			FString& OutDefinition)
		{
			if (!Id.IsValid() || Id.value >= Function.statements.GetLength())
			{
				return Fail(TEXT("Statement ID is outside the verified arena"));
			}
			const asSTypedSemanticStatement& Statement =
				Function.statements[Id.value];
			const FString Prefix = MakeTypedASTIndent(Depth);
			switch (Statement.kind)
			{
			case asETypedSemanticStatementKind::Block:
				OutDefinition += Prefix + TEXT("{\n");
				AppendPositionHook(
					Statement.safePointRole,
					Statement.span,
					Depth + 1,
					OutDefinition);
				for (asUINT ChildIndex = 0;
					ChildIndex < Statement.children.GetLength();
					++ChildIndex)
				{
					if (!EmitStatement(
						Statement.children[ChildIndex],
						Depth + 1,
						OutDefinition))
					{
						return false;
					}
				}
				OutDefinition += Prefix + TEXT("}\n");
				return true;
			case asETypedSemanticStatementKind::LocalDeclaration:
			{
				AppendPositionHook(
					Statement.safePointRole,
					Statement.span,
					Depth,
					OutDefinition);
				FString Initializer;
				FString Type;
				if (!EmitExpression(
					Statement.initializerExpression, Depth, Initializer))
				{
					return false;
				}
				if (!Statement.symbol.IsValid()
					|| Statement.symbol.value >= Function.symbols.GetLength()
					|| !TrySpellTypedASTCppTypeInternal(
						Function.symbols[Statement.symbol.value].type, Type))
				{
					return Fail(TEXT("Local declaration type is outside the reviewed scalar set"));
				}
				OutDefinition += FString::Printf(
					TEXT("%s%s%s as_sem_s%u = %s;\n"),
					*Prefix,
					IsSymbolMutated(Statement.symbol) ? TEXT("") : TEXT("const "),
					*Type,
					Statement.symbol.value,
					*Initializer);
				AppendExceptionReturnCheck(Depth, OutDefinition);
				return true;
			}
			case asETypedSemanticStatementKind::Expression:
			{
				AppendPositionHook(
					Statement.safePointRole,
					Statement.span,
					Depth,
					OutDefinition);
				FString Value;
				if (!EmitExpression(Statement.valueExpression, Depth, Value))
				{
					return false;
				}
				OutDefinition += FString::Printf(
					TEXT("%s%s;\n"), *Prefix, *Value);
				AppendExceptionReturnCheck(Depth, OutDefinition);
				return true;
			}
			case asETypedSemanticStatementKind::If:
			{
				AppendPositionHook(
					Statement.safePointRole,
					Statement.span,
					Depth,
					OutDefinition);
				FString Condition;
				if (!EmitExpression(Statement.conditionExpression, Depth, Condition))
				{
					return false;
				}
				if (bRequiresExecutionState)
				{
					const FString ConditionTemporary = FString::Printf(
						TEXT("as_sem_if_condition_%u"), Id.value);
					OutDefinition += FString::Printf(
						TEXT("%sconst bool %s = %s;\n"),
						*Prefix,
						*ConditionTemporary,
						*Condition);
					AppendExceptionReturnCheck(Depth, OutDefinition);
					Condition = ConditionTemporary;
				}
				OutDefinition += FString::Printf(
					TEXT("%sif (%s)\n"), *Prefix, *Condition);
				if (!EmitStatement(Statement.thenStatement, Depth, OutDefinition))
				{
					return false;
				}
				if (Statement.elseStatement.IsValid())
				{
					OutDefinition += Prefix + TEXT("else\n");
					if (!EmitStatement(Statement.elseStatement, Depth, OutDefinition))
					{
						return false;
					}
				}
				return true;
			}
			case asETypedSemanticStatementKind::Return:
			{
				AppendPositionHook(
					Statement.safePointRole,
					Statement.span,
					Depth,
					OutDefinition);
				if (!Statement.valueExpression.IsValid())
				{
					if (ReturnCppType != TEXT("void"))
					{
						return Fail(TEXT(
							"Non-void return reached emission without a value"));
					}
					OutDefinition += Prefix + TEXT("return;\n");
					return true;
				}
				if (ReturnCppType == TEXT("void"))
				{
					return Fail(TEXT(
						"Void return reached emission with an unexpected value"));
				}

				FString Value;
				if (!EmitExpression(Statement.valueExpression, Depth, Value))
				{
					return false;
				}
				OutDefinition += FString::Printf(
					TEXT("%sreturn %s;\n"), *Prefix, *Value);
				return true;
			}
			case asETypedSemanticStatementKind::For:
			{
				OutDefinition += Prefix + TEXT("{\n");
				if (Statement.initializerStatement.IsValid()
					&& !EmitStatement(
						Statement.initializerStatement,
						Depth + 1,
						OutDefinition))
				{
					return false;
				}
				FString Condition;
				if (!EmitLoopCondition(
					Statement.conditionExpression,
					Statement.safePointRole,
					Statement.span,
					Depth + 2,
					Condition))
				{
					return false;
				}
				FString Increment;
				if (!EmitForIncrement(Statement, Depth + 2, Increment))
				{
					return false;
				}
				OutDefinition += FString::Printf(
					TEXT("%sfor (; %s; %s)\n"),
					*MakeTypedASTIndent(Depth + 1),
					*Condition,
					*Increment);
				if (!EmitStatement(
					Statement.bodyStatement, Depth + 1, OutDefinition))
				{
					return false;
				}
				AppendExceptionReturnCheck(Depth + 1, OutDefinition);
				OutDefinition += Prefix + TEXT("}\n");
				return true;
			}
			case asETypedSemanticStatementKind::While:
			{
				FString Condition;
				if (!EmitLoopCondition(
					Statement.conditionExpression,
					Statement.safePointRole,
					Statement.span,
					Depth + 1,
					Condition))
				{
					return false;
				}
				OutDefinition += FString::Printf(
					TEXT("%swhile (%s)\n"), *Prefix, *Condition);
				if (!EmitStatement(
					Statement.bodyStatement, Depth, OutDefinition))
				{
					return false;
				}
				AppendExceptionReturnCheck(Depth, OutDefinition);
				return true;
			}
			case asETypedSemanticStatementKind::DoWhile:
			{
				OutDefinition += Prefix + TEXT("do\n");
				if (!EmitStatement(
					Statement.bodyStatement, Depth, OutDefinition))
				{
					return false;
				}
				FString Condition;
				if (!EmitLoopCondition(
					Statement.conditionExpression,
					Statement.safePointRole,
					Statement.span,
					Depth + 1,
					Condition))
				{
					return false;
				}
				OutDefinition += FString::Printf(
					TEXT("%swhile (%s);\n"), *Prefix, *Condition);
				AppendExceptionReturnCheck(Depth, OutDefinition);
				return true;
			}
			case asETypedSemanticStatementKind::Switch:
			{
				if (!Statement.switchHasInvalidEnumValueEdge)
				{
					AppendPositionHook(
						Statement.safePointRole,
						Statement.span,
						Depth,
						OutDefinition);
				}
				FString Selector;
				if (!EmitExpression(
					Statement.conditionExpression, Depth, Selector))
				{
					return false;
				}
				const TCHAR* NormalizedType = Statement.switchNormalization
					== asETypedSemanticSwitchNormalization::UnsignedInt32
						? TEXT("uint32") : TEXT("int32");
				const FString SelectorTemporary = FString::Printf(
					TEXT("as_sem_switch_selector_%u"), Id.value);
				OutDefinition += FString::Printf(
					TEXT("%sconst %s %s = static_cast<%s>(%s);\n"),
					*Prefix,
					NormalizedType,
					*SelectorTemporary,
					NormalizedType,
					*Selector);
				AppendExceptionReturnCheck(Depth, OutDefinition);
				OutDefinition += FString::Printf(
					TEXT("%sswitch (%s)\n%s{\n"),
					*Prefix,
					*SelectorTemporary,
					*Prefix);
				for (asUINT CaseIndex = 0;
					CaseIndex < Statement.children.GetLength(); ++CaseIndex)
				{
					if (!EmitSwitchCase(
						Statement.children[CaseIndex],
						Statement.switchNormalization,
						Depth + 1,
						OutDefinition))
					{
						return false;
					}
				}
				if (Statement.switchHasInvalidEnumValueEdge)
				{
					RequiredIncludes.AddUnique(
						TEXT("StaticJIT/StaticJITHeader.h"));
					OutDefinition += MakeTypedASTIndent(Depth + 1)
						+ TEXT("default:\n");
					AppendPositionHook(
						Statement.safePointRole,
						Statement.span,
						Depth + 2,
						OutDefinition);
					OutDefinition += MakeTypedASTIndent(Depth + 2)
						+ TEXT("FStaticJITFunction::SetSwitchValueInvalidException(Execution);\n");
					OutDefinition += MakeTypedASTIndent(Depth + 2)
						+ MakeFunctionFailureReturn() + TEXT("\n");
				}
				OutDefinition += Prefix + TEXT("}\n");
				return true;
			}
			case asETypedSemanticStatementKind::Case:
				return Fail(TEXT(
					"Case statement reached emission outside its owning switch"));
			case asETypedSemanticStatementKind::Break:
				AppendPositionHook(
					Statement.safePointRole,
					Statement.span,
					Depth,
					OutDefinition);
				OutDefinition += Prefix + TEXT("break;\n");
				return true;
			case asETypedSemanticStatementKind::Continue:
				AppendPositionHook(
					Statement.safePointRole,
					Statement.span,
					Depth,
					OutDefinition);
				OutDefinition += Prefix + TEXT("continue;\n");
				return true;
			default:
				return Fail(TEXT("Statement kind reached emission without analysis support"));
			}
		}

		bool EmitRootChildren(FString& OutDefinition)
		{
			const asSTypedSemanticStatement& Root =
				Function.statements[Function.rootStatement.value];
			for (asUINT ChildIndex = 0;
				ChildIndex < Root.children.GetLength();
				++ChildIndex)
			{
				if (!EmitStatement(
					Root.children[ChildIndex],
					1,
					OutDefinition))
				{
					return false;
				}
			}
			return true;
		}

		const FString& GetFailureDetail() const
		{
			return FailureDetail;
		}

		bool RequiresScalarOps() const
		{
			return bRequiresScalarOps;
		}

		bool RequiresExecutionState() const
		{
			return bRequiresExecutionState;
		}

		const TArray<FString>& GetRequiredIncludes() const
		{
			return RequiredIncludes;
		}

		const TArray<FAngelscriptTypedASTJITNativeCallSiteMetadata>&
			GetNativeCallSites() const
		{
			return EmittedNativeCallSites;
		}

		bool IsSymbolMutated(const asTypedSemanticSymbolId Symbol) const
		{
			for (asUINT Index = 0; Index < Function.expressions.GetLength(); ++Index)
			{
				const asSTypedSemanticExpression& Expression =
					Function.expressions[Index];
				if (Expression.kind
						!= asETypedSemanticExpressionKind::Assignment
					|| !Expression.mutationTarget.IsValid())
				{
					continue;
				}
				const asSTypedSemanticExpression& Target =
					Function.expressions[Expression.mutationTarget.value];
				if (Target.kind == asETypedSemanticExpressionKind::Symbol
					&& Target.symbol == Symbol)
				{
					return true;
				}
			}
			return false;
		}

	private:
		void AppendPositionHook(
			const asETypedSemanticSafePointRole Role,
			const asSTypedSemanticSourceSpan& Span,
			const int32 Depth,
			FString& OutDefinition) const
		{
			if (!bEmitFramePosition
				|| Role == asETypedSemanticSafePointRole::None
				|| Span.row == 0)
			{
				return;
			}
			OutDefinition += FString::Printf(
				TEXT("%sTypedFrame.SetSourcePosition(%u, %u);\n"),
				*MakeTypedASTIndent(Depth),
				Span.row,
				Span.column);
		}

		FString MakeFunctionFailureReturn() const
		{
			return ReturnCppType == TEXT("void")
				? FString(TEXT("return;"))
				: FString::Printf(
					TEXT("return %s{};"), *ReturnCppType);
		}

		void AppendExceptionReturnCheck(
			const int32 Depth,
			FString& OutDefinition) const
		{
			if (!bRequiresExecutionState)
			{
				return;
			}
			const FString Prefix = MakeTypedASTIndent(Depth);
			OutDefinition += FString::Printf(
				TEXT("%sif (UNLIKELY(Execution.bExceptionThrown))\n")
				TEXT("%s{\n")
				TEXT("%s\t%s\n")
				TEXT("%s}\n"),
				*Prefix,
				*Prefix,
				*Prefix,
				*MakeFunctionFailureReturn(),
				*Prefix);
		}

		void AppendExpressionExceptionReturnCheck(
			const int32 Depth,
			const asTypedSemanticExpressionId EvaluatedExpression,
			const FString& ExpressionCppType,
			FString& OutExpression) const
		{
			if (!bRequiresExecutionState
				|| !ContainsExceptionCapableScalarOperator(
					EvaluatedExpression))
			{
				return;
			}
			const FString Prefix = MakeTypedASTIndent(Depth);
			OutExpression += FString::Printf(
				TEXT("%sif (UNLIKELY(Execution.bExceptionThrown))\n")
				TEXT("%s{\n")
				TEXT("%s\treturn %s{};\n")
				TEXT("%s}\n"),
				*Prefix,
				*Prefix,
				*Prefix,
				*ExpressionCppType,
				*Prefix);
		}

		bool ContainsExceptionCapableScalarOperator(
			const asTypedSemanticExpressionId Root) const
		{
			if (!Root.IsValid()
				|| Root.value >= Function.expressions.GetLength())
			{
				return false;
			}

			TBitArray<> Visited(
				false,
				static_cast<int32>(Function.expressions.GetLength()));
			TArray<asTypedSemanticExpressionId, TInlineAllocator<8>> Pending;
			Pending.Add(Root);
			while (!Pending.IsEmpty())
			{
				const asTypedSemanticExpressionId Current = Pending.Pop();
				if (!Current.IsValid()
					|| Current.value >= Function.expressions.GetLength()
					|| Visited[Current.value])
				{
					continue;
				}
				Visited[Current.value] = true;
				const asSTypedSemanticExpression& Expression =
					Function.expressions[Current.value];
				if (Expression.kind == asETypedSemanticExpressionKind::Binary
					&& (Expression.operatorToken == "/"
						|| Expression.operatorToken == "%"
						|| Expression.operatorToken == "**"))
				{
					return true;
				}
				for (asUINT OperandIndex = 0;
					OperandIndex < Expression.operands.GetLength();
					++OperandIndex)
				{
					Pending.Add(Expression.operands[OperandIndex]);
				}
				if (Expression.mutationValue.IsValid())
				{
					Pending.Add(Expression.mutationValue);
				}
			}
			return false;
		}

		bool RecordNativeCallSite(
			const asTypedSemanticExpressionId Expression,
			const FAngelscriptTypedASTJITNativeCallEmissionPlan& Plan,
			const FString& EmittedCppCallee)
		{
			FAngelscriptTypedASTJITNativeCallSiteMetadata Candidate;
			Candidate.Expression = Expression;
			Candidate.Route = Plan.Route
				== EAngelscriptTypedASTJITCallEmissionRoute::Bridge
					? EAngelscriptJITNativeCallSiteRoute::Bridge
					: Plan.NativeCallSiteRoute;
			Candidate.CanonicalDeclaration = Plan.CanonicalDeclaration;
			Candidate.EmittedCppCallee = EmittedCppCallee;
			Candidate.RegisteredTarget = Plan.RegisteredTarget.IsEmpty()
				? Plan.Symbol : Plan.RegisteredTarget;
			if (Candidate.Route == EAngelscriptJITNativeCallSiteRoute::Bridge
				|| Candidate.Route
					== EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding)
			{
				Candidate.RuntimeDllCore =
					Candidate.Route == EAngelscriptJITNativeCallSiteRoute::Bridge
						? TEXT("AngelscriptTypedASTJIT::InvokeBoundViaVM")
						: TEXT("AngelscriptTypedASTJIT::ResolveBoundNative");
				Candidate.MetadataRowSymbol = Plan.CallSiteSymbol;
				Candidate.StableTargetKey = Plan.StableTargetKey.Hash;
				Candidate.ExpectedAbi = Plan.ExpectedABI;
				Candidate.ReferenceSlotIndex = Plan.ReferenceSlotIndex;
			}

			if (const FAngelscriptTypedASTJITNativeCallSiteMetadata* Existing =
				EmittedNativeCallSites.FindByPredicate(
					[Expression](const auto& CallSite)
					{
						return CallSite.Expression == Expression;
					}))
			{
				if (Existing->Route != Candidate.Route
					|| Existing->CanonicalDeclaration
						!= Candidate.CanonicalDeclaration
					|| Existing->EmittedCppCallee
						!= Candidate.EmittedCppCallee
					|| Existing->RuntimeDllCore != Candidate.RuntimeDllCore
					|| Existing->MetadataRowSymbol
						!= Candidate.MetadataRowSymbol
					|| Existing->RegisteredTarget
						!= Candidate.RegisteredTarget
					|| !(Existing->StableTargetKey
						== Candidate.StableTargetKey)
					|| !(Existing->ExpectedAbi == Candidate.ExpectedAbi)
					|| Existing->ReferenceSlotIndex
						!= Candidate.ReferenceSlotIndex)
				{
					return Fail(TEXT(
						"One native expression emitted conflicting call-site metadata"));
				}
				return true;
			}
			EmittedNativeCallSites.Add(MoveTemp(Candidate));
			return true;
		}

		bool Fail(const FString& Detail)
		{
			FailureDetail = Detail;
			return false;
		}

		bool ContainsExceptionCapableIntegralOperator() const
		{
			for (asUINT Index = 0;
				Index < Function.expressions.GetLength();
				++Index)
			{
				const asSTypedSemanticExpression& Expression =
					Function.expressions[Index];
				if (Expression.kind == asETypedSemanticExpressionKind::Binary
					&& (Expression.operatorToken == "/"
						|| Expression.operatorToken == "%"))
				{
					return true;
				}
			}
			return false;
		}

		bool ContainsExceptionCapablePowerOperator() const
		{
			for (asUINT Index = 0;
				Index < Function.expressions.GetLength(); ++Index)
			{
				const asSTypedSemanticExpression& Expression =
					Function.expressions[Index];
				if (Expression.kind == asETypedSemanticExpressionKind::Binary
					&& Expression.operatorToken == "**")
				{
					return true;
				}
			}
			return false;
		}

		bool ContainsInvalidEnumSwitchEdge() const
		{
			for (asUINT Index = 0;
				Index < Function.statements.GetLength(); ++Index)
			{
				const asSTypedSemanticStatement& Statement =
					Function.statements[Index];
				if (Statement.kind
						== asETypedSemanticStatementKind::Switch
					&& Statement.switchHasInvalidEnumValueEdge)
				{
					return true;
				}
			}
			return false;
		}

		const asCTypedSemanticFunction& Function;
		TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
			NativeCalls;
		TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan>
			ScriptCalls;
		FString ReturnCppType;
		FString FailureDetail;
		TArray<FString> RequiredIncludes;
		TArray<FAngelscriptTypedASTJITNativeCallSiteMetadata>
			EmittedNativeCallSites;
		bool bRequiresScalarOps = false;
		bool bRequiresExecutionState = false;
		bool bEmitFramePosition = false;
	};

	bool ContainsTypedASTForbiddenToken(const FString& Text, FString& OutToken)
	{
		const TCHAR* Forbidden[] = {
			TEXT("GetByteCode"),
			TEXT("FAngelscriptBytecode"),
			TEXT("FStaticJITContext"),
			TEXT("FAngelscriptJITExecutionContext"),
			TEXT("FAngelscriptJITProvider"),
		};
		for (const TCHAR* Token : Forbidden)
		{
			if (Text.Contains(Token, ESearchCase::IgnoreCase))
			{
				OutToken = Token;
				return true;
			}
		}

		FRegexMatcher PointerLike(
			FRegexPattern(TEXT("0[xX][0-9a-fA-F]{8,}")),
			Text);
		if (PointerLike.FindNext())
		{
			OutToken = TEXT("pointer-like hexadecimal literal");
			return true;
		}
		return false;
	}
}

bool TrySpellAngelscriptTypedASTJITCppType(
	const asCDataType& Type,
	FString& OutType)
{
	return TrySpellTypedASTCppTypeInternal(Type, OutType);
}

bool EmitAngelscriptTypedASTJITBridgeCallSiteRows(
	const TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
		NativeCalls,
	FString& OutDefinitions,
	FString* OutError)
{
	OutDefinitions.Reset();
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	auto Fail = [OutError](FString Detail)
	{
		if (OutError != nullptr)
		{
			*OutError = MoveTemp(Detail);
		}
		return false;
	};

	TMap<FString, const FAngelscriptTypedASTJITNativeCallEmissionPlan*>
		EmittedBridgeCallSites;
	for (const FAngelscriptTypedASTJITNativeCallEmissionPlan& NativeCall
		: NativeCalls)
	{
		if (NativeCall.Route
				!= EAngelscriptTypedASTJITCallEmissionRoute::Bridge
			&& NativeCall.Route
				!= EAngelscriptTypedASTJITCallEmissionRoute::CurrentNativeBinding)
		{
			continue;
		}
		if (const FAngelscriptTypedASTJITNativeCallEmissionPlan* const* Existing =
			EmittedBridgeCallSites.Find(NativeCall.CallSiteSymbol))
		{
			if ((*Existing)->CanonicalDeclaration
					!= NativeCall.CanonicalDeclaration
				|| (*Existing)->RegisteredTarget
					!= NativeCall.RegisteredTarget
				|| !((*Existing)->StableTargetKey
					== NativeCall.StableTargetKey)
				|| !((*Existing)->ExpectedABI == NativeCall.ExpectedABI)
				|| (*Existing)->ReferenceSlotIndex
					!= NativeCall.ReferenceSlotIndex)
			{
				return Fail(TEXT(
					"Bridge call-site identifier aliases different immutable metadata"));
			}
			continue;
		}
		EmittedBridgeCallSites.Add(NativeCall.CallSiteSymbol, &NativeCall);
		OutDefinitions += FString::Printf(TEXT(
			"static const FAngelscriptTypedASTJITBoundCallSite %s = {\n"
			"\tTEXT(\"%s\"),\n"
			"\tTEXT(\"%s\"),\n"
			"\tFAngelscriptStableFunctionKey{FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT(\"%s\"))) }},\n"
			"\tFAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT(\"%s\"))) },\n"
			"\t%d\n"
			"};\n\n"),
			*NativeCall.CallSiteSymbol,
			*EscapeTypedASTCppString(NativeCall.CanonicalDeclaration),
			*EscapeTypedASTCppString(NativeCall.RegisteredTarget),
			*NativeCall.StableTargetKey.Hash.ToHexString(),
			*NativeCall.ExpectedABI.ToHexString(),
			NativeCall.ReferenceSlotIndex);
	}
	return true;
}

FAngelscriptTypedASTJITEmission EmitTypedASTJITFunction(
	const asCTypedSemanticFunction& Function,
	const FAngelscriptTypedASTJITFunctionShape& Shape,
	const FAngelscriptTypedASTJITEmitOptions& Options)
{
	using ECapability = EAngelscriptTypedASTJITDirectCapability;
	constexpr ECapability ApprovedEmissionCapabilities =
		ECapability::FramePosition | ECapability::RecursionBudget;
	const FAngelscriptTypedASTJITGenerationCapabilityProfile* const
		InstrumentationProfile = Options.InstrumentationProfile;
	if (InstrumentationProfile != nullptr)
	{
		if (!InstrumentationProfile->IsValid())
		{
			return MakeTypedASTEmissionFailure(
				EAngelscriptTypedASTJITFallbackReason::EmitterFailure,
				{},
				TEXT("InvalidInstrumentationProfile: generated runtime hooks require one valid immutable capability profile"));
		}
		const uint16 UnsupportedBits = static_cast<uint16>(
			InstrumentationProfile->AvailableCapabilities)
			& ~static_cast<uint16>(ApprovedEmissionCapabilities);
		if (UnsupportedBits != 0)
		{
			return MakeTypedASTEmissionFailure(
				EAngelscriptTypedASTJITFallbackReason::EmitterFailure,
				{},
				FString::Printf(TEXT(
					"UnsupportedInstrumentationCapabilities: Profile=%s Unsupported=0x%04x"),
					*LexToString(
						InstrumentationProfile->AvailableCapabilities),
					UnsupportedBits));
		}
	}
	const bool bEmitFramePosition = InstrumentationProfile != nullptr
		&& EnumHasAnyFlags(
			InstrumentationProfile->AvailableCapabilities,
			ECapability::FramePosition);

	const FAngelscriptTypedASTJITAnalysis Analysis =
		AnalyzeTypedASTJITFunction(
			Function, Shape, Options.NativeCalls, Options.ScriptCalls,
			Options.GlobalDependencies);
	if (!Analysis.bEligible)
	{
		return MakeTypedASTEmissionFailure(
			Analysis.FailureReason,
			Analysis.FailureSpan,
			Analysis.FailureDetail);
	}

	FAngelscriptTypedASTJITEmission Candidate;
	if (InstrumentationProfile != nullptr)
	{
		Candidate.InstrumentationProfileIdentityHash =
			InstrumentationProfile->IdentityHash;
	}
	Candidate.bRequiresInstrumentationFrame = bEmitFramePosition;
	Candidate.RequiredIncludes.Add(TEXT("CoreMinimal.h"));
	FString ReturnType;
	if (!TrySpellAngelscriptTypedASTJITCppType(Shape.ReturnType, ReturnType))
	{
		return MakeTypedASTEmissionFailure(
			EAngelscriptTypedASTJITFallbackReason::EmitterFailure,
			{},
			TEXT("Return type is outside the reviewed scalar C++ spelling set"));
	}
	FTypedASTScalarEmitter Emitter(
		Function,
		Options.NativeCalls,
		Options.ScriptCalls,
		ReturnType,
		bEmitFramePosition);
	Candidate.bRequiresExecutionState = Emitter.RequiresExecutionState();
	if (Candidate.bRequiresExecutionState)
	{
		Candidate.RequiredIncludes.Add(TEXT(
			"StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"));
	}
	if (Candidate.bRequiresInstrumentationFrame)
	{
		Candidate.RequiredIncludes.Add(TEXT("StaticJIT/StaticJITHeader.h"));
	}
	const FString FunctionStart = FString::Printf(
		TEXT("static %s %s(\n"),
		*ReturnType,
		*Shape.SymbolName);
	Candidate.Declaration = FunctionStart;
	if (Options.bEmitBridgeCallSiteRows)
	{
		FString BridgeCallSiteRows;
		FString BridgeCallSiteError;
		if (!EmitAngelscriptTypedASTJITBridgeCallSiteRows(
				Options.NativeCalls,
				BridgeCallSiteRows,
				&BridgeCallSiteError))
		{
			return MakeTypedASTEmissionFailure(
				EAngelscriptTypedASTJITFallbackReason::EmitterFailure,
				{},
				MoveTemp(BridgeCallSiteError));
		}
		Candidate.Definition += MoveTemp(BridgeCallSiteRows);
	}
	Candidate.Definition += FunctionStart;
	const bool bHasHiddenParameters = Candidate.bRequiresExecutionState
		|| Candidate.bRequiresInstrumentationFrame;
	auto AppendHiddenParameter = [&](const TCHAR* Parameter)
	{
		const bool bHasAnotherHiddenParameter =
			Candidate.bRequiresExecutionState
			&& Candidate.bRequiresInstrumentationFrame
			&& FCString::Strcmp(Parameter,
				TEXT("FScriptExecution& Execution")) == 0;
		const bool bNeedsComma = bHasAnotherHiddenParameter
			|| !Shape.ParameterTypes.IsEmpty();
		FString Line = TEXT("\t");
		Line += Parameter;
		Line += bNeedsComma ? TEXT(",\n") : TEXT(")");
		Candidate.Declaration += Line;
		Candidate.Definition += Line;
	};
	if (Candidate.bRequiresExecutionState)
	{
		AppendHiddenParameter(TEXT("FScriptExecution& Execution"));
	}
	if (Candidate.bRequiresInstrumentationFrame)
	{
		AppendHiddenParameter(
			TEXT("FScopeTypedASTJITExecutionFrame& TypedFrame"));
	}
	if (Shape.ParameterTypes.IsEmpty())
	{
		if (bHasHiddenParameters)
		{
			Candidate.Declaration += TEXT(";\n");
			Candidate.Definition += TEXT("\n{\n");
		}
		else
		{
			Candidate.Declaration.RemoveFromEnd(TEXT("\n"));
			Candidate.Declaration += TEXT(");\n");
			Candidate.Definition.RemoveFromEnd(TEXT("\n"));
			Candidate.Definition += TEXT(")\n{\n");
		}
	}
	else
	{
		for (int32 Index = 0; Index < Shape.ParameterTypes.Num(); ++Index)
		{
			FString ParameterType;
			asTypedSemanticSymbolId ParameterSymbol;
			if (!TrySpellAngelscriptTypedASTJITCppType(
					Shape.ParameterTypes[Index], ParameterType)
				|| !TryFindParameterSymbolId(
					Function, Index, ParameterSymbol))
			{
				return MakeTypedASTEmissionFailure(
					EAngelscriptTypedASTJITFallbackReason::EmitterFailure,
					{},
						TEXT("Parameter type or compiler-owned symbol mapping is outside the reviewed scalar/borrowed-input C++ set"));
			}
			const FString Parameter = FString::Printf(
				TEXT("\t%s%s as_sem_s%d%s\n"),
				Emitter.IsSymbolMutated(ParameterSymbol)
					? TEXT("") : TEXT("const "),
				*ParameterType,
				static_cast<int32>(ParameterSymbol.value),
				Index + 1 == Shape.ParameterTypes.Num() ? TEXT(")") : TEXT(","));
			Candidate.Declaration += Parameter;
			Candidate.Definition += Parameter;
		}
		Candidate.Declaration.RemoveFromEnd(TEXT(")\n"));
		Candidate.Declaration += TEXT(");\n");
		Candidate.Definition += TEXT("{\n");
	}

	if (!Emitter.EmitRootChildren(Candidate.Definition))
	{
		return MakeTypedASTEmissionFailure(
			EAngelscriptTypedASTJITFallbackReason::EmitterFailure,
			{},
			Emitter.GetFailureDetail());
	}
	Candidate.NativeCallSites = Emitter.GetNativeCallSites();
	Candidate.NativeCallSites.Sort([](
		const FAngelscriptTypedASTJITNativeCallSiteMetadata& Left,
		const FAngelscriptTypedASTJITNativeCallSiteMetadata& Right)
	{
		return Left.Expression.value < Right.Expression.value;
	});
	FString NativeCallComments;
	for (int32 CallSiteIndex = 0;
		CallSiteIndex < Candidate.NativeCallSites.Num();
		++CallSiteIndex)
	{
		const FAngelscriptTypedASTJITNativeCallSiteMetadata& CallSite =
			Candidate.NativeCallSites[CallSiteIndex];
		NativeCallComments += FString::Printf(TEXT(
			"/*\n"
			" * AngelScript Native Call Site %d\n"
			" * AS Declaration      : %s\n"
			" * Emitted C++ Callee  : %s\n"
			" * Route               : %s\n"
			" * Effective Target    : %s\n"
			" * Runtime DLL Core    : %s\n"
			" * Metadata Row        : %s\n"
			" * Reference Slot      : %d\n"
			" */\n"),
			CallSiteIndex,
			*OneLineNativeCallComment(CallSite.CanonicalDeclaration),
			*OneLineNativeCallComment(CallSite.EmittedCppCallee),
			NativeCallSiteRouteName(CallSite.Route),
			*OneLineNativeCallComment(CallSite.RegisteredTarget),
			CallSite.RuntimeDllCore.IsEmpty()
				? TEXT("<none>") : *CallSite.RuntimeDllCore,
			CallSite.MetadataRowSymbol.IsEmpty()
				? TEXT("<none>") : *CallSite.MetadataRowSymbol,
			CallSite.ReferenceSlotIndex);
	}
	Candidate.Definition = NativeCallComments + Candidate.Definition;
	if (Emitter.RequiresScalarOps())
	{
		Candidate.RequiredIncludes.Add(
			TEXT("StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"));
	}
	for (const FString& Include : Emitter.GetRequiredIncludes())
	{
		Candidate.RequiredIncludes.AddUnique(Include);
	}
	Candidate.Definition += TEXT("}\n");

	FString ForbiddenToken;
	if (ContainsTypedASTForbiddenToken(
		Candidate.Declaration + Candidate.Definition,
		ForbiddenToken))
	{
		return MakeTypedASTEmissionFailure(
			EAngelscriptTypedASTJITFallbackReason::EmitterFailure,
			{},
			FString::Printf(
				TEXT("Generated output contains forbidden token: %s"),
				*ForbiddenToken));
	}

	Candidate.bSuccess = true;
	return Candidate;
}
