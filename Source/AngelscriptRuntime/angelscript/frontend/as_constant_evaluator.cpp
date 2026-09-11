#include "frontend/as_constant_evaluator.h"
#include "frontend/as_ast_context.h"
#include "frontend/as_ast_cast.h"
#include "Containers/Set.h"
#include <cmath>

BEGIN_AS_NAMESPACE
namespace asConstantEvaluation
	{
		struct FShape { uint8 Width = 0; bool Signed = false; };
		uint64 Mask(uint8 Width) { return Width == 64 ? MAX_uint64 : (uint64(1) << Width) - 1; }
		int64 SignedValue(uint64 Bits, uint8 Width)
		{
			Bits &= Mask(Width);
			if (!(Bits & (uint64(1) << (Width - 1)))) return static_cast<int64>(Bits);
			const uint64 Magnitude = ((~Bits) & Mask(Width)) + 1;
			return Magnitude == (uint64(1) << 63) ? MIN_int64 : -static_cast<int64>(Magnitude);
		}
		uint64 Magnitude(int64 Value) { return Value < 0 ? uint64(-(Value + 1)) + 1 : uint64(Value); }
		int64 Minimum(uint8 Width) { return Width == 64 ? MIN_int64 : -(int64(1) << (Width - 1)); }
		int64 Maximum(uint8 Width) { return Width == 64 ? MAX_int64 : (int64(1) << (Width - 1)) - 1; }
		bool Shape(asCQualType Type, FShape& Out)
		{
			if (!Type.IsValid() || Type.IsHandle() || Type.GetType()->IsInvalid()) return false;
			if (const auto* Nominal = dyn_cast<asCNominalType>(Type.GetType()))
			{
				asSNominalTypeDescriptor Descriptor;
				if (!Nominal->GetOwnerContext() || Nominal->GetOwnerContext()->GetTypeContext()->GetIdentityRegistry().ResolveNominalType(
					Nominal->GetDeclarationIdentity(), Descriptor) != asETypeIdentityStatus::Succeeded || Descriptor.Kind != asETypeDeclKind::Enum) return false;
				Out = {32, true}; return true;
			}
			const auto* Builtin = dyn_cast<asCBuiltinType>(Type.GetType());
			if (!Builtin) return false;
			switch (Builtin->GetPrimitive())
			{
			case asEPrimitiveType::Bool: Out = {1, false}; break;
			case asEPrimitiveType::Int8: Out = {8, true}; break;
			case asEPrimitiveType::UInt8: Out = {8, false}; break;
			case asEPrimitiveType::Int16: Out = {16, true}; break;
			case asEPrimitiveType::UInt16: Out = {16, false}; break;
			case asEPrimitiveType::Int32: Out = {32, true}; break;
			case asEPrimitiveType::UInt32: Out = {32, false}; break;
			case asEPrimitiveType::Int64: Out = {64, true}; break;
			case asEPrimitiveType::UInt64: Out = {64, false}; break;
			default: return false;
			}
			return true;
		}
		asSIntegralConstantEvaluation Success(uint64 Bits, FShape Type)
		{
			return {asEConstantEvaluationStatus::Succeeded, Bits & Mask(Type.Width), Type.Width, Type.Signed, nullptr};
		}
		asSIntegralConstantEvaluation Fail(asEConstantEvaluationStatus Status, const asCExpr& Expr)
		{
			return {Status, 0, 0, false, &Expr};
		}
		uint64 Extend(const asSIntegralConstantEvaluation& Value)
		{
			return Value.bSigned ? static_cast<uint64>(SignedValue(Value.Bits, Value.BitWidth)) : Value.Bits;
		}
		bool Multiply(uint64 A, uint64 B, FShape Type, uint64& Out)
		{
			if (!Type.Signed) { Out = A * B; return true; }
			const int64 Left = SignedValue(A, Type.Width), Right = SignedValue(B, Type.Width);
			const bool Negative = (Left < 0) != (Right < 0);
			const uint64 X = Magnitude(Left), Y = Magnitude(Right), Limit = uint64(Maximum(Type.Width)) + uint64(Negative);
			if (X && Y > Limit / X) return false;
			Out = Negative ? uint64(0) - X * Y : X * Y; return true;
		}
		struct FEvaluator
		{
			TSet<const asCExpr*> Visiting;
			int32 Remaining = 100000;
			bool IsFloating(const asCExpr* Expr) const
			{
				const auto* Type = Expr ? dyn_cast<asCBuiltinType>(Expr->GetExpressionType()) : nullptr;
				return Type && (Type->GetPrimitive() == asEPrimitiveType::Float32 || Type->GetPrimitive() == asEPrimitiveType::Float64);
			}
			asSIntegralConstantEvaluation EvaluateFloating(const asCExpr& Expr, double& Out, int32 Depth)
			{
				if (!IsFloating(&Expr))
				{
					auto Result = Evaluate(Expr, Depth);
					if (Result.Status == asEConstantEvaluationStatus::Succeeded) Out = Result.bSigned ? double(SignedValue(Result.Bits, Result.BitWidth)) : double(Result.Bits);
					return Result;
				}
				if (Expr.IsInvalid()) return Fail(asEConstantEvaluationStatus::InvalidType, Expr);
				if (Depth > 256 || --Remaining < 0) return Fail(asEConstantEvaluationStatus::BudgetExceeded, Expr);
				if (Visiting.Contains(&Expr)) return Fail(asEConstantEvaluationStatus::UnresolvedReference, Expr);
				Visiting.Add(&Expr);
				struct FPop { TSet<const asCExpr*>& Set; const asCExpr* Node; ~FPop() { Set.Remove(Node); } } Pop{Visiting, &Expr};
				auto Finish = [&](double Value)
				{
					if (cast<asCBuiltinType>(Expr.GetExpressionType())->GetPrimitive() == asEPrimitiveType::Float32) Value = double(float(Value));
					if (!std::isfinite(Value)) return Fail(asEConstantEvaluationStatus::Overflow, Expr);
					Out = Value; return Success(0, {1, false});
				};
				auto Recurse = [&](const asCExpr* Child, double& Value) { return Child ? EvaluateFloating(*Child, Value, Depth + 1) : Fail(asEConstantEvaluationStatus::NotConstant, Expr); };
				if (const auto* Literal = dyn_cast<asCFloatingLiteral>(&Expr)) return Finish(Literal->GetValue());
				const asCExpr* Operand = nullptr;
				if (const auto* Paren = dyn_cast<asCParenExpr>(&Expr)) Operand = Paren->GetSubExpr();
				else if (const auto* ImplicitCast = dyn_cast<asCImplicitCastExpr>(&Expr)) Operand = ImplicitCast->GetOperand();
				else if (const auto* ExplicitCast = dyn_cast<asCExplicitCastExpr>(&Expr)) Operand = ExplicitCast->GetOperand();
				else if (const auto* Construct = dyn_cast<asCConstructExpr>(&Expr))
				{
					if (Construct->GetConstructor() || !isa<asCBuiltinType>(Expr.GetExpressionType())
						|| Construct->GetArguments().Num() != 1)
						return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
					Operand = Construct->GetArguments()[0];
				}
				else if (const auto* Reference = dyn_cast<asCDeclRefExpr>(&Expr))
				{
					const auto* Variable = dyn_cast<asCVarDecl>(Reference->GetDecl());
					if (!Variable || !Variable->GetResolvedQualType().IsConst()) return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
					Operand = Variable->GetInitializer();
					if (!Operand) return Fail(asEConstantEvaluationStatus::UnresolvedReference, Expr);
				}
				else if (const auto* Conditional = dyn_cast<asCConditionalOperator>(&Expr))
				{
					if (!Conditional->GetCondition()) return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
					const auto Condition = Evaluate(*Conditional->GetCondition(), Depth + 1);
					if (Condition.Status != asEConstantEvaluationStatus::Succeeded) return Condition;
					Operand = Condition.Bits ? Conditional->GetTrueExpr() : Conditional->GetFalseExpr();
				}
				if (Operand)
				{
					double Value = 0; auto Result = Recurse(Operand, Value);
					return Result.Status == asEConstantEvaluationStatus::Succeeded ? Finish(Value) : Result;
				}
				if (const auto* Unary = dyn_cast<asCUnaryOperator>(&Expr))
				{
					if (Unary->GetOperator() != asEUnaryOperator::Plus && Unary->GetOperator() != asEUnaryOperator::Minus) return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
					double Value = 0; auto Result = Recurse(Unary->GetOperand(), Value);
					return Result.Status == asEConstantEvaluationStatus::Succeeded ? Finish(Unary->GetOperator() == asEUnaryOperator::Minus ? -Value : Value) : Result;
				}
				if (const auto* Binary = dyn_cast<asCBinaryOperator>(&Expr))
				{
					if (Binary->GetOperator() >= asEBinaryOperator::Assign) return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
					double A = 0, B = 0; auto Left = Recurse(Binary->GetLeft(), A);
					if (Left.Status != asEConstantEvaluationStatus::Succeeded) return Left;
					auto Right = Recurse(Binary->GetRight(), B); if (Right.Status != asEConstantEvaluationStatus::Succeeded) return Right;
					switch (Binary->GetOperator())
					{
					case asEBinaryOperator::Add: return Finish(A + B);
					case asEBinaryOperator::Subtract: return Finish(A - B);
					case asEBinaryOperator::Multiply: return Finish(A * B);
					case asEBinaryOperator::Divide: return B == 0 ? Fail(asEConstantEvaluationStatus::DivideByZero, Expr) : Finish(A / B);
					case asEBinaryOperator::Modulo: return B == 0 ? Fail(asEConstantEvaluationStatus::DivideByZero, Expr) : Finish(std::fmod(A, B));
					case asEBinaryOperator::Power: return Finish(std::pow(A, B));
					default: return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
					}
				}
				return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
			}
			asSIntegralConstantEvaluation Evaluate(const asCExpr& Expr, int32 Depth = 0)
			{
				if (Depth > 256 || --Remaining < 0) return Fail(asEConstantEvaluationStatus::BudgetExceeded, Expr);
				if (Visiting.Contains(&Expr)) return Fail(asEConstantEvaluationStatus::UnresolvedReference, Expr);
				FShape Type;
				if (Expr.IsInvalid() || !Shape(Expr.GetExpressionQualType(), Type)) return Fail(asEConstantEvaluationStatus::InvalidType, Expr);
				Visiting.Add(&Expr);
				struct FPop { TSet<const asCExpr*>& Set; const asCExpr* Expr; ~FPop() { Set.Remove(Expr); } } Pop{Visiting, &Expr};
				auto Recurse = [&](const asCExpr* Child) { return Child ? Evaluate(*Child, Depth + 1) : Fail(asEConstantEvaluationStatus::NotConstant, Expr); };
				auto Cast = [&](asSIntegralConstantEvaluation Value)
				{
					if (Value.Status != asEConstantEvaluationStatus::Succeeded) return Value;
					return Success(Type.Width == 1 ? uint64(Value.Bits != 0) : Extend(Value), Type);
				};
				auto Convert = [&](const asCExpr* Operand)
				{
					if (!IsFloating(Operand)) return Cast(Recurse(Operand));
					double Value = 0; auto Result = EvaluateFloating(*Operand, Value, Depth + 1);
					if (Result.Status != asEConstantEvaluationStatus::Succeeded) return Result;
					if (Type.Width == 1) return Success(Value != 0, Type);
					Value = std::trunc(Value);
					const double Upper = std::ldexp(1.0, Type.Signed ? Type.Width - 1 : Type.Width);
					if (Value < (Type.Signed ? -Upper : 0.0) || Value >= Upper) return Fail(asEConstantEvaluationStatus::Overflow, Expr);
					return Success(Type.Signed ? uint64(int64(Value)) : uint64(Value), Type);
				};
				if (const auto* Integer = dyn_cast<asCIntegerLiteral>(&Expr)) return Success(Integer->GetUnsignedValue(), Type);
				if (const auto* Boolean = dyn_cast<asCBooleanLiteral>(&Expr)) return Success(Boolean->GetValue() ? 1 : 0, Type);
				if (const auto* Paren = dyn_cast<asCParenExpr>(&Expr)) return Cast(Recurse(Paren->GetSubExpr()));
				if (const auto* Implicit = dyn_cast<asCImplicitCastExpr>(&Expr)) return Convert(Implicit->GetOperand());
				if (const auto* Explicit = dyn_cast<asCExplicitCastExpr>(&Expr)) return Convert(Explicit->GetOperand());
				if (const auto* Construct = dyn_cast<asCConstructExpr>(&Expr))
				{
					// Primitive `int(value)` syntax is represented by Sema as a
					// constructor expression even though it has no callable
					// constructor. Keep actual user constructors non-constant.
					if (Construct->GetConstructor() || !isa<asCBuiltinType>(Expr.GetExpressionType())
						|| Construct->GetArguments().Num() != 1)
						return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
					return Convert(Construct->GetArguments()[0]);
				}
				if (const auto* Reference = dyn_cast<asCDeclRefExpr>(&Expr))
				{
					if (const auto* Enum = dyn_cast<asCEnumConstantDecl>(Reference->GetDecl()))
						return Enum->HasResolvedValue() ? Success(static_cast<uint64>(Enum->GetValue()), Type) : Fail(asEConstantEvaluationStatus::UnresolvedReference, Expr);
					if (const auto* Variable = dyn_cast<asCVarDecl>(Reference->GetDecl()); Variable && Variable->GetResolvedQualType().IsConst())
						return Variable->GetInitializer() ? Cast(Recurse(Variable->GetInitializer())) : Fail(asEConstantEvaluationStatus::UnresolvedReference, Expr);
					return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
				}
				if (const auto* Conditional = dyn_cast<asCConditionalOperator>(&Expr))
				{
					const auto Condition = Recurse(Conditional->GetCondition());
					if (Condition.Status != asEConstantEvaluationStatus::Succeeded) return Condition;
					return Cast(Recurse(Condition.Bits ? Conditional->GetTrueExpr() : Conditional->GetFalseExpr()));
				}
				if (const auto* Unary = dyn_cast<asCUnaryOperator>(&Expr))
				{
					const auto Value = Recurse(Unary->GetOperand());
					if (Value.Status != asEConstantEvaluationStatus::Succeeded) return Value;
					switch (Unary->GetOperator())
					{
					case asEUnaryOperator::Plus: return Cast(Value);
					case asEUnaryOperator::LogicalNot: return Success(Value.Bits == 0, Type);
					case asEUnaryOperator::BitwiseNot: return Success(~Extend(Value), Type);
					case asEUnaryOperator::Minus:
						if (Type.Signed && SignedValue(Extend(Value), Type.Width) == Minimum(Type.Width)) return Fail(asEConstantEvaluationStatus::Overflow, Expr);
						return Success(uint64(0) - Extend(Value), Type);
					default: return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
					}
				}
				const auto* Binary = dyn_cast<asCBinaryOperator>(&Expr);
				if (!Binary) return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
				const auto Op = Binary->GetOperator();
				if (Op >= asEBinaryOperator::Assign) return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
				const asCExpr* LeftExpr = Binary->GetLeft();
				const asCExpr* RightExpr = Binary->GetRight();
				if (!LeftExpr || !RightExpr) return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
				if (IsFloating(LeftExpr) || IsFloating(RightExpr))
				{
					double A = 0, B = 0; auto Left = EvaluateFloating(*LeftExpr, A, Depth + 1);
					if (Left.Status != asEConstantEvaluationStatus::Succeeded) return Left;
					auto Right = EvaluateFloating(*RightExpr, B, Depth + 1); if (Right.Status != asEConstantEvaluationStatus::Succeeded) return Right;
					switch (Op)
					{
					case asEBinaryOperator::Equal: return Success(A == B, Type);
					case asEBinaryOperator::NotEqual: return Success(A != B, Type);
					case asEBinaryOperator::Less: return Success(A < B, Type);
					case asEBinaryOperator::Greater: return Success(A > B, Type);
					case asEBinaryOperator::LessEqual: return Success(A <= B, Type);
					case asEBinaryOperator::GreaterEqual: return Success(A >= B, Type);
					default: return Fail(asEConstantEvaluationStatus::InvalidType, Expr);
					}
				}
				const auto Left = Recurse(LeftExpr);
				if (Left.Status != asEConstantEvaluationStatus::Succeeded) return Left;
				if ((Op == asEBinaryOperator::LogicalAnd && !Left.Bits) || (Op == asEBinaryOperator::LogicalOr && Left.Bits)) return Success(Left.Bits != 0, Type);
				const auto Right = Recurse(RightExpr);
				if (Right.Status != asEConstantEvaluationStatus::Succeeded) return Right;
				const uint64 A = Extend(Left), B = Extend(Right);
				const int64 SignedA = SignedValue(Left.Bits, Left.BitWidth), SignedB = SignedValue(Right.Bits, Right.BitWidth);
				const bool BothSigned = Left.bSigned && Right.bSigned;
				switch (Op)
				{
				case asEBinaryOperator::LogicalAnd: return Success(Left.Bits && Right.Bits, Type);
				case asEBinaryOperator::LogicalOr: return Success(Left.Bits || Right.Bits, Type);
				case asEBinaryOperator::LogicalXor: return Success(bool(Left.Bits) != bool(Right.Bits), Type);
				case asEBinaryOperator::Equal: return Success(A == B, Type);
				case asEBinaryOperator::NotEqual: return Success(A != B, Type);
				case asEBinaryOperator::Less: return Success(BothSigned ? SignedA < SignedB : A < B, Type);
				case asEBinaryOperator::Greater: return Success(BothSigned ? SignedA > SignedB : A > B, Type);
				case asEBinaryOperator::LessEqual: return Success(BothSigned ? SignedA <= SignedB : A <= B, Type);
				case asEBinaryOperator::GreaterEqual: return Success(BothSigned ? SignedA >= SignedB : A >= B, Type);
				default: break;
				}
				const uint64 TypedA = A & Mask(Type.Width), TypedB = B & Mask(Type.Width);
				const int64 X = SignedValue(TypedA, Type.Width), Y = SignedValue(TypedB, Type.Width);
				uint64 Value = 0;
				switch (Op)
				{
				case asEBinaryOperator::Add:
					if (Type.Signed && ((Y > 0 && X > Maximum(Type.Width) - Y) || (Y < 0 && X < Minimum(Type.Width) - Y))) return Fail(asEConstantEvaluationStatus::Overflow, Expr);
					Value = TypedA + TypedB; break;
				case asEBinaryOperator::Subtract:
					if (Type.Signed && ((Y < 0 && X > Maximum(Type.Width) + Y) || (Y > 0 && X < Minimum(Type.Width) + Y))) return Fail(asEConstantEvaluationStatus::Overflow, Expr);
					Value = TypedA - TypedB; break;
				case asEBinaryOperator::Multiply:
					if (!Multiply(TypedA, TypedB, Type, Value)) return Fail(asEConstantEvaluationStatus::Overflow, Expr);
					break;
				case asEBinaryOperator::Divide: case asEBinaryOperator::Modulo:
					if (!TypedB) return Fail(asEConstantEvaluationStatus::DivideByZero, Expr);
					if (Type.Signed && X == Minimum(Type.Width) && Y == -1) return Fail(asEConstantEvaluationStatus::Overflow, Expr);
					Value = Type.Signed ? uint64(Op == asEBinaryOperator::Divide ? X / Y : X % Y) : (Op == asEBinaryOperator::Divide ? TypedA / TypedB : TypedA % TypedB); break;
				case asEBinaryOperator::BitwiseAnd: Value = TypedA & TypedB; break;
				case asEBinaryOperator::BitwiseOr: Value = TypedA | TypedB; break;
				case asEBinaryOperator::BitwiseXor: Value = TypedA ^ TypedB; break;
				case asEBinaryOperator::ShiftLeft: case asEBinaryOperator::ShiftRight: case asEBinaryOperator::ShiftRightArithmetic:
					if ((Right.bSigned && SignedB < 0) || B >= Type.Width) return Fail(asEConstantEvaluationStatus::InvalidShift, Expr);
					if (Op == asEBinaryOperator::ShiftLeft)
					{
						// AS shifts fixed-width bit patterns (the old compiler uses
						// DWORD/QWORD), including signed negative operands. Do not
						// import C++ signed-left-shift undefined behavior here.
						Value = TypedA << B;
					}
					else
					{
						Value = TypedA >> B;
						const uint64 SignBit = uint64(1) << (Type.Width - 1);
						if (Op == asEBinaryOperator::ShiftRightArithmetic && (TypedA & SignBit) && B)
							Value |= Mask(Type.Width) ^ (Mask(Type.Width) >> B);
					}
					break;
				case asEBinaryOperator::Power:
					if (Right.bSigned && SignedB < 0) return Fail(asEConstantEvaluationStatus::InvalidType, Expr);
					Value = 1;
					for (uint64 Exponent = B, Base = TypedA; Exponent; Exponent >>= 1)
					{
						if ((Exponent & 1) && !Multiply(Value, Base, Type, Value)) return Fail(asEConstantEvaluationStatus::Overflow, Expr);
						if (Exponent > 1 && !Multiply(Base, Base, Type, Base)) return Fail(asEConstantEvaluationStatus::Overflow, Expr);
					}
					break;
				default: return Fail(asEConstantEvaluationStatus::NotConstant, Expr);
				}
				return Success(Value, Type);
			}
		};
	}

	asSIntegralConstantEvaluation asEvaluateIntegralConstant(const asCExpr& Expression)
	{
		return asConstantEvaluation::FEvaluator{}.Evaluate(Expression);
	}
	bool asTryConvertIntegralConstant(const asSIntegralConstantEvaluation& Input, asCQualType Target, uint64& OutBits)
	{
		using namespace asConstantEvaluation;
		FShape Type;
		if (Input.Status != asEConstantEvaluationStatus::Succeeded || !Input.BitWidth || Input.BitWidth > 64 || !Shape(Target, Type)) return false;
		const int64 Signed = SignedValue(Input.Bits, Input.BitWidth);
		if (Type.Signed)
		{
			if (Input.bSigned ? (Signed < Minimum(Type.Width) || Signed > Maximum(Type.Width)) : Input.Bits > uint64(Maximum(Type.Width))) return false;
		}
		else if ((Input.bSigned && Signed < 0) || (Input.bSigned ? uint64(Signed) : Input.Bits) > Mask(Type.Width)) return false;
		OutBits = Extend(Input) & Mask(Type.Width); return true;
	}
END_AS_NAMESPACE
