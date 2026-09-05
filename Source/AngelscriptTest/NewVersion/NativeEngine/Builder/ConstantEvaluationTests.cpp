#include "CQTest.h"
#include "frontend/as_ast_context.h"
#include "frontend/as_constant_evaluator.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace AngelscriptIntegralConstantTests
{












TEST_CLASS_WITH_FLAGS(IntegralConstants, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		struct FGraph
		{
			TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot = MakeShared<asCSourceSnapshot, ESPMode::ThreadSafe>();
			asCASTContext Context{Snapshot};
			asCIntegerLiteral* Integer(int64 Value, asEPrimitiveType Kind = asEPrimitiveType::Int32)
			{
				auto* Node = Context.Create<asCIntegerLiteral>(Value, asCSourceRange{});
				Node->SetExpressionSemantics(Context.GetBuiltinType(Kind), asEValueCategory::PRValue); return Node;
			}
			asCBinaryOperator* Binary(asEBinaryOperator Op, asCExpr* Left, asCExpr* Right, asEPrimitiveType Kind = asEPrimitiveType::Int32)
			{
				auto* Node = Context.Create<asCBinaryOperator>(Op, Left, Right, asCSourceRange{}, asCSourceRange{});
				Node->SetExpressionSemantics(Context.GetBuiltinType(Kind), asEValueCategory::PRValue); return Node;
			}
		};
	public:
		TEST_METHOD(TypedArithmeticAndBitwiseValuesAreEvaluatedWithoutAnEngine)
		{
			FGraph G;
			auto* Product = G.Binary(asEBinaryOperator::Multiply, G.Integer(6), G.Integer(7));
			auto* Shift = G.Binary(asEBinaryOperator::ShiftLeft, G.Integer(1), G.Integer(4));
			auto Value = asEvaluateIntegralConstant(*G.Binary(asEBinaryOperator::BitwiseOr, Product, Shift));
			ASSERT_THAT(IsTrue(Value.Status == asEConstantEvaluationStatus::Succeeded));
			ASSERT_THAT(AreEqual(uint64(58), Value.Bits)); ASSERT_THAT(AreEqual(uint8(32), Value.BitWidth));
			ASSERT_THAT(IsTrue(Value.bSigned));
		}
		TEST_METHOD(ShortCircuitDoesNotEvaluateAnUnreachableDivision)
		{
			FGraph G;
			auto* Division = G.Binary(asEBinaryOperator::Divide, G.Integer(1), G.Integer(0));
			auto Value = asEvaluateIntegralConstant(*G.Binary(asEBinaryOperator::LogicalAnd, G.Integer(0), Division, asEPrimitiveType::Bool));
			ASSERT_THAT(IsTrue(Value.Status == asEConstantEvaluationStatus::Succeeded));
			ASSERT_THAT(AreEqual(uint64(0), Value.Bits));
			ASSERT_THAT(IsTrue(asEvaluateIntegralConstant(*Division).Status == asEConstantEvaluationStatus::DivideByZero));
		}
		TEST_METHOD(SignedOverflowAndInvalidShiftFailAtTheActualOperator)
		{
			FGraph G;
			auto* Overflow = G.Binary(asEBinaryOperator::Add, G.Integer(MAX_int32), G.Integer(1));
			const auto Value = asEvaluateIntegralConstant(*Overflow);
			ASSERT_THAT(IsTrue(Value.Status == asEConstantEvaluationStatus::Overflow)); ASSERT_THAT(IsTrue(Value.FailureNode == Overflow));
			ASSERT_THAT(IsTrue(asEvaluateIntegralConstant(*G.Binary(asEBinaryOperator::ShiftLeft, G.Integer(1), G.Integer(32))).Status == asEConstantEvaluationStatus::InvalidShift));
		}
		TEST_METHOD(UnsignedArithmeticWrapsAtItsDeclaredWidth)
		{
			FGraph G;
			const auto Value = asEvaluateIntegralConstant(*G.Binary(asEBinaryOperator::Add, G.Integer(MAX_uint32, asEPrimitiveType::UInt32), G.Integer(1, asEPrimitiveType::UInt32), asEPrimitiveType::UInt32));
			ASSERT_THAT(IsTrue(Value.Status == asEConstantEvaluationStatus::Succeeded)); ASSERT_THAT(AreEqual(uint64(0), Value.Bits));
		}
		TEST_METHOD(CheckedCaseConversionPreservesOutputOnFailure)
		{
			FGraph G; asCQualType Byte;
			asCQualType::TryCreate(G.Context.GetBuiltinType(asEPrimitiveType::UInt8), asETypeQualifiers::None, Byte);
			uint64 Bits = 99;
			ASSERT_THAT(IsFalse(asTryConvertIntegralConstant(asEvaluateIntegralConstant(*G.Integer(256)), Byte, Bits)));
			ASSERT_THAT(AreEqual(uint64(99), Bits));
			ASSERT_THAT(IsTrue(asTryConvertIntegralConstant(asEvaluateIntegralConstant(*G.Integer(255)), Byte, Bits)));
			ASSERT_THAT(AreEqual(uint64(255), Bits));
		}
		TEST_METHOD(UnresolvedEnumReferenceBecomesConcreteAfterResolution)
		{
			FGraph G;
			auto* Constant = G.Context.Create<asCEnumConstantDecl>(TEXT("Forward"), 0, true, asCSourceRange{}); Constant->MarkValueUnresolved();
			auto* Ref = G.Context.Create<asCDeclRefExpr>(Constant, asCSourceRange{}); Ref->SetExpressionSemantics(G.Context.GetBuiltinType(asEPrimitiveType::Int32), asEValueCategory::PRValue);
			ASSERT_THAT(IsTrue(asEvaluateIntegralConstant(*Ref).Status == asEConstantEvaluationStatus::UnresolvedReference));
			Constant->SetResolvedValue(42);
			ASSERT_THAT(AreEqual(uint64(42), asEvaluateIntegralConstant(*Ref).Bits));
		}
		TEST_METHOD(LogicalAndArithmeticRightShiftKeepMaintainedASMeanings)
		{
			FGraph G;
			const auto Logical = asEvaluateIntegralConstant(*G.Binary(asEBinaryOperator::ShiftRight, G.Integer(-2), G.Integer(1)));
			const auto Arithmetic = asEvaluateIntegralConstant(*G.Binary(asEBinaryOperator::ShiftRightArithmetic, G.Integer(-2), G.Integer(1)));
			const auto UnsignedArithmetic = asEvaluateIntegralConstant(*G.Binary(
				asEBinaryOperator::ShiftRightArithmetic,
				G.Integer(static_cast<int64>(0x80000000u), asEPrimitiveType::UInt32),
				G.Integer(1), asEPrimitiveType::UInt32));
			ASSERT_THAT(AreEqual(uint64(0x7fffffff), Logical.Bits));
			ASSERT_THAT(AreEqual(uint64(0xffffffff), Arithmetic.Bits));
			// AngelScript's >>> selects an arithmetic DWORD operation even when
			// the stored bit pattern has an unsigned declared type.
			ASSERT_THAT(AreEqual(uint64(0xc0000000), UnsignedArithmetic.Bits));
		}
		TEST_METHOD(CrossWidthDenominatorIsNormalizedBeforeCheckedDivision)
		{
			FGraph G;
			// This deliberately inconsistent typed graph is not publishable, but
			// the pure query must still reject it without reaching native 1 / 0.
			const auto Value = asEvaluateIntegralConstant(*G.Binary(
				asEBinaryOperator::Divide,
				G.Integer(1, asEPrimitiveType::Int8),
				G.Integer(256, asEPrimitiveType::Int16),
				asEPrimitiveType::Int8));
			ASSERT_THAT(IsTrue(Value.Status == asEConstantEvaluationStatus::DivideByZero));
		}
		TEST_METHOD(Int64MinimumDivisionAndNegationRejectOverflowWithoutNativeUB)
		{
			FGraph G;
			auto* Minimum = G.Integer(MIN_int64, asEPrimitiveType::Int64);
			const auto Division = asEvaluateIntegralConstant(*G.Binary(
				asEBinaryOperator::Divide, Minimum,
				G.Integer(-1, asEPrimitiveType::Int64), asEPrimitiveType::Int64));
			auto* Negation = G.Context.Create<asCUnaryOperator>(
				asEUnaryOperator::Minus, G.Integer(MIN_int64, asEPrimitiveType::Int64),
				asCSourceRange{}, asCSourceRange{});
			Negation->SetExpressionSemantics(
				G.Context.GetBuiltinType(asEPrimitiveType::Int64), asEValueCategory::PRValue);
			const auto Negated = asEvaluateIntegralConstant(*Negation);
			ASSERT_THAT(IsTrue(Division.Status == asEConstantEvaluationStatus::Overflow));
			ASSERT_THAT(IsTrue(Negated.Status == asEConstantEvaluationStatus::Overflow));
		}
		TEST_METHOD(SignedLeftShiftUsesTheMaintainedFixedWidthBitPattern)
		{
			FGraph G;
			const auto HighBit = asEvaluateIntegralConstant(*G.Binary(asEBinaryOperator::ShiftLeft, G.Integer(1), G.Integer(31)));
			const auto Negative = asEvaluateIntegralConstant(*G.Binary(asEBinaryOperator::ShiftLeft, G.Integer(-2), G.Integer(1)));
			ASSERT_THAT(IsTrue(HighBit.Status == asEConstantEvaluationStatus::Succeeded && Negative.Status == asEConstantEvaluationStatus::Succeeded));
			ASSERT_THAT(AreEqual(uint64(0x80000000), HighBit.Bits)); ASSERT_THAT(AreEqual(uint64(0xfffffffc), Negative.Bits));
		}
		TEST_METHOD(FloatingConstantArithmeticCanFeedAnExplicitIntegralCast)
		{
			FGraph G;
			auto* A = G.Context.Create<asCFloatingLiteral>(21.25, asCSourceRange{});
			auto* B = G.Context.Create<asCFloatingLiteral>(2.0, asCSourceRange{});
			A->SetExpressionSemantics(G.Context.GetBuiltinType(asEPrimitiveType::Float64), asEValueCategory::PRValue);
			B->SetExpressionSemantics(G.Context.GetBuiltinType(asEPrimitiveType::Float64), asEValueCategory::PRValue);
			auto* Product = G.Binary(asEBinaryOperator::Multiply, A, B, asEPrimitiveType::Float64);
			auto* Cast = G.Context.Create<asCExplicitCastExpr>(Product, asSTypeSyntax{}, asCSourceRange{});
			Cast->SetExpressionSemantics(G.Context.GetBuiltinType(asEPrimitiveType::Int32), asEValueCategory::PRValue);
			const auto Value = asEvaluateIntegralConstant(*Cast);
			ASSERT_THAT(IsTrue(Value.Status == asEConstantEvaluationStatus::Succeeded)); ASSERT_THAT(AreEqual(uint64(42), Value.Bits));
		}
		TEST_METHOD(ConstFloatingReferenceArithmeticCanFeedBuiltinIntegralConstruction)
		{
			FGraph G;
			auto* ScaleValue = G.Context.Create<asCFloatingLiteral>(2.0, asCSourceRange{});
			ScaleValue->SetExpressionSemantics(
				G.Context.GetBuiltinType(asEPrimitiveType::Float64), asEValueCategory::PRValue);
			auto* Scale = G.Context.Create<asCVarDecl>(asEDeclKind::Var, TEXT("Scale"), asCSourceRange{});
			asCQualType ConstFloat;
			ASSERT_THAT(IsTrue(asCQualType::TryCreate(
				G.Context.GetBuiltinType(asEPrimitiveType::Float64), asETypeQualifiers::Const, ConstFloat)));
			ASSERT_THAT(IsTrue(Scale->SetResolvedQualType(ConstFloat)));
			ASSERT_THAT(IsTrue(Scale->SetInitializer(ScaleValue)));
			auto* ScaleReference = G.Context.Create<asCDeclRefExpr>(Scale, asCSourceRange{});
			ScaleReference->SetExpressionQualType(ConstFloat, asEValueCategory::LValue);
			auto* Value = G.Context.Create<asCFloatingLiteral>(21.25, asCSourceRange{});
			Value->SetExpressionSemantics(
				G.Context.GetBuiltinType(asEPrimitiveType::Float64), asEValueCategory::PRValue);
			auto* Product = G.Binary(
				asEBinaryOperator::Multiply, Value, ScaleReference, asEPrimitiveType::Float64);
			asSTypeSyntax IntSyntax;
			IntSyntax.NameParts.Add(TEXT("int"));
			auto* Construct = G.Context.Create<asCConstructExpr>(
				MoveTemp(IntSyntax), TArray<asCExpr*>{Product}, nullptr, asCSourceRange{});
			Construct->SetExpressionSemantics(
				G.Context.GetBuiltinType(asEPrimitiveType::Int32), asEValueCategory::PRValue);
			const auto Result = asEvaluateIntegralConstant(*Construct);
			ASSERT_THAT(IsTrue(Result.Status == asEConstantEvaluationStatus::Succeeded));
			ASSERT_THAT(AreEqual(uint64(42), Result.Bits));
		}
		TEST_METHOD(NestedBuiltinFloatingConstructionFeedsCheckedIntegralConstruction)
		{
			FGraph G;
			auto* Scale = G.Context.Create<asCFloatingLiteral>(2.0, asCSourceRange{});
			Scale->SetExpressionSemantics(
				G.Context.GetBuiltinType(asEPrimitiveType::Float64), asEValueCategory::PRValue);
			asSTypeSyntax FloatSyntax;
			FloatSyntax.NameParts.Add(TEXT("float64"));
			auto* FloatingConstruct = G.Context.Create<asCConstructExpr>(
				MoveTemp(FloatSyntax), TArray<asCExpr*>{G.Integer(21)}, nullptr, asCSourceRange{});
			FloatingConstruct->SetExpressionSemantics(
				G.Context.GetBuiltinType(asEPrimitiveType::Float64), asEValueCategory::PRValue);
			auto* Product = G.Binary(
				asEBinaryOperator::Multiply, FloatingConstruct, Scale, asEPrimitiveType::Float64);
			asSTypeSyntax IntSyntax;
			IntSyntax.NameParts.Add(TEXT("int"));
			auto* IntegralConstruct = G.Context.Create<asCConstructExpr>(
				MoveTemp(IntSyntax), TArray<asCExpr*>{Product}, nullptr, asCSourceRange{});
			IntegralConstruct->SetExpressionSemantics(
				G.Context.GetBuiltinType(asEPrimitiveType::Int32), asEValueCategory::PRValue);
			const auto Result = asEvaluateIntegralConstant(*IntegralConstruct);
			ASSERT_THAT(IsTrue(Result.Status == asEConstantEvaluationStatus::Succeeded));
			ASSERT_THAT(AreEqual(uint64(42), Result.Bits));

			asSTypeSyntax EmptySyntax;
			auto* WrongArity = G.Context.Create<asCConstructExpr>(
				EmptySyntax, TArray<asCExpr*>{}, nullptr, asCSourceRange{});
			WrongArity->SetExpressionSemantics(
				G.Context.GetBuiltinType(asEPrimitiveType::Int32), asEValueCategory::PRValue);
			auto* UserConstructor = G.Context.Create<asCFunctionDecl>(TEXT("int"), asCSourceRange{});
			auto* UserCall = G.Context.Create<asCConstructExpr>(
				MoveTemp(EmptySyntax), TArray<asCExpr*>{G.Integer(1)}, UserConstructor, asCSourceRange{});
			UserCall->SetExpressionSemantics(
				G.Context.GetBuiltinType(asEPrimitiveType::Int32), asEValueCategory::PRValue);
			ASSERT_THAT(IsTrue(asEvaluateIntegralConstant(*WrongArity).Status == asEConstantEvaluationStatus::NotConstant));
			ASSERT_THAT(IsTrue(asEvaluateIntegralConstant(*UserCall).Status == asEConstantEvaluationStatus::NotConstant));
		}
		TEST_METHOD(FloatingConversionChecksRangeBeforeNativeConversion)
		{
			FGraph G;
			auto* Literal = G.Context.Create<asCFloatingLiteral>(2147483648.0, asCSourceRange{});
			Literal->SetExpressionSemantics(G.Context.GetBuiltinType(asEPrimitiveType::Float64), asEValueCategory::PRValue);
			auto* Cast = G.Context.Create<asCExplicitCastExpr>(Literal, asSTypeSyntax{}, asCSourceRange{});
			Cast->SetExpressionSemantics(G.Context.GetBuiltinType(asEPrimitiveType::Int32), asEValueCategory::PRValue);
			const auto Value = asEvaluateIntegralConstant(*Cast);
			ASSERT_THAT(IsTrue(Value.Status == asEConstantEvaluationStatus::Overflow && Value.FailureNode == Cast));
		}
	};
}
#endif
