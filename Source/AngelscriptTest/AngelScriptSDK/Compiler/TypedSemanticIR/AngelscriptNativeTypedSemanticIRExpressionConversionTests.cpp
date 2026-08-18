#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRExpressionConversionTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FReturnExpressionView
	{
		const asCTypedSemanticFunction* HIR = nullptr;
		const asSTypedSemanticStatement* Return = nullptr;
		const asSTypedSemanticExpression* Expression = nullptr;
	};

	static FReturnExpressionView GetSingleReturnExpression(
		asCScriptFunction* Function)
	{
		FReturnExpressionView Result;
		Result.HIR = Function != nullptr
			? Function->GetTypedSemanticFunction()
			: nullptr;
		if (Result.HIR == nullptr)
		{
			return Result;
		}

		for (asUINT StatementIndex = 0;
			StatementIndex < Result.HIR->statements.GetLength();
			++StatementIndex)
		{
			const asSTypedSemanticStatement& Statement =
				Result.HIR->statements[StatementIndex];
			if (Statement.kind != asETypedSemanticStatementKind::Return)
			{
				continue;
			}
			if (Result.Return != nullptr)
			{
				Result.Return = nullptr;
				Result.Expression = nullptr;
				return Result;
			}
			Result.Return = &Statement;
		}

		if (Result.Return != nullptr
			&& Result.Return->valueExpression.IsValid()
			&& Result.Return->valueExpression.value
				< Result.HIR->expressions.GetLength())
		{
			Result.Expression = &Result.HIR->expressions[
				Result.Return->valueExpression.value];
		}
		return Result;
	}

	static const asSTypedSemanticStatement* FindStatement(
		const asCTypedSemanticFunction& HIR,
		const asETypedSemanticStatementKind Kind)
	{
		for (asUINT StatementIndex = 0;
			StatementIndex < HIR.statements.GetLength();
			++StatementIndex)
		{
			if (HIR.statements[StatementIndex].kind == Kind)
			{
				return &HIR.statements[StatementIndex];
			}
		}
		return nullptr;
	}

public:
	TEST_METHOD(ExplicitImplicitAndPowerConversionsRetainFinalCompilerTypes)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-EXPRESSION-CONVERSIONS",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScriptEngine->SetTypedSemanticIRCapture(true)));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedSemanticIRExpressionConversions",
			ASTEST_AS_ANSI(R"AS(
				float32 ImplicitReturn(int Value)
				{
					return Value;
				}

				int64 ExplicitCast(int Value)
				{
					return int64(Value);
				}

				float64 PowerConversion(float64 Base, float32 Exponent)
				{
					return Base ** Exponent;
				}

				float32 MixedAdd(float32 Left, int Right)
				{
					return Left + Right;
				}

				bool MixedCompare(float32 Left, int Right)
				{
					return Left < Right;
				}

				int64 MixedBitwise(int64 Left, int Right)
				{
					return Left & Right;
				}

				float32 FloatTarget(float32 Value)
				{
					return Value;
				}

				float32 CallConversion(int Value)
				{
					return FloatTarget(Value);
				}

				float32 LocalConversion(int Value)
				{
					float32 Local = Value;
					return Local;
				}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		asCScriptFunction* const ImplicitFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module,
				"float32 ImplicitReturn(int Value)"));
		const FReturnExpressionView Implicit =
			GetSingleReturnExpression(ImplicitFunction);
		ASSERT_THAT(IsNotNull(Implicit.HIR));
		ASSERT_THAT(IsNotNull(Implicit.Return));
		ASSERT_THAT(IsNotNull(Implicit.Expression,
			TEXT("An implicit return conversion must be the captured return value")));
		if (Implicit.HIR != nullptr && Implicit.Expression != nullptr)
		{
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*Implicit.HIR).IsValid()));
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::Conversion,
				Implicit.Expression->kind));
			ASSERT_THAT(IsTrue(Implicit.Expression->type.IsFloat32Type()));
			ASSERT_THAT(AreEqual(
				1,
				static_cast<int32>(Implicit.Expression->operands.GetLength())));
			if (Implicit.Expression->operands.GetLength() == 1)
			{
				const asSTypedSemanticExpression& Operand =
					Implicit.HIR->expressions[
						Implicit.Expression->operands[0].value];
				ASSERT_THAT(AreEqual(
					asETypedSemanticExpressionKind::Symbol,
					Operand.kind));
				ASSERT_THAT(IsTrue(Operand.type.IsIntegerType()));
			}
		}

		asCScriptFunction* const ExplicitFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module,
				"int64 ExplicitCast(int Value)"));
		const FReturnExpressionView Explicit =
			GetSingleReturnExpression(ExplicitFunction);
		ASSERT_THAT(IsNotNull(Explicit.HIR));
		ASSERT_THAT(IsNotNull(Explicit.Expression));
		if (Explicit.HIR != nullptr && Explicit.Expression != nullptr)
		{
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*Explicit.HIR).IsValid()));
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::Conversion,
				Explicit.Expression->kind));
			ASSERT_THAT(IsTrue(
				Explicit.Expression->type.IsIntegerType()
					&& Explicit.Expression->type.GetSizeInMemoryDWords() == 2));
		}

		asCScriptFunction* const PowerFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module,
				"float64 PowerConversion(float64 Base, float32 Exponent)"));
		const FReturnExpressionView Power =
			GetSingleReturnExpression(PowerFunction);
		ASSERT_THAT(IsNotNull(Power.HIR));
		ASSERT_THAT(IsNotNull(Power.Expression));
		if (Power.HIR != nullptr && Power.Expression != nullptr)
		{
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*Power.HIR).IsValid()));
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::Binary,
				Power.Expression->kind));
			ASSERT_THAT(AreEqual(
				FString(TEXT("**")),
				FString(UTF8_TO_TCHAR(
					Power.Expression->operatorToken.AddressOf()))));
			ASSERT_THAT(IsTrue(Power.Expression->type.IsFloat64Type()));
			ASSERT_THAT(AreEqual(
				2,
				static_cast<int32>(Power.Expression->operands.GetLength())));
			if (Power.Expression->operands.GetLength() == 2)
			{
				const asSTypedSemanticExpression& Base =
					Power.HIR->expressions[
						Power.Expression->operands[0].value];
				const asSTypedSemanticExpression& Exponent =
					Power.HIR->expressions[
						Power.Expression->operands[1].value];
				ASSERT_THAT(IsTrue(Base.type.IsFloat64Type()));
				ASSERT_THAT(AreEqual(
					asETypedSemanticExpressionKind::Conversion,
					Exponent.kind));
				ASSERT_THAT(IsTrue(Exponent.type.IsFloat64Type()));
			}
		}

		struct FMixedBinaryCase
		{
			const char* Declaration;
			const TCHAR* Operator;
			bool bBooleanResult;
			bool bWideIntegerOperand;
		};
		const FMixedBinaryCase MixedBinaryCases[] = {
			{"float32 MixedAdd(float32 Left, int Right)", TEXT("+"), false, false},
			{"bool MixedCompare(float32 Left, int Right)", TEXT("<"), true, false},
			{"int64 MixedBitwise(int64 Left, int Right)", TEXT("&"), false, true},
		};
		for (const FMixedBinaryCase& Case : MixedBinaryCases)
		{
			asCScriptFunction* const Function =
				static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
					Module,
					Case.Declaration));
			const FReturnExpressionView View =
				GetSingleReturnExpression(Function);
			ASSERT_THAT(IsNotNull(View.HIR));
			ASSERT_THAT(IsNotNull(View.Expression));
			if (View.HIR == nullptr || View.Expression == nullptr)
			{
				continue;
			}
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*View.HIR).IsValid()));
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::Binary,
				View.Expression->kind));
			ASSERT_THAT(AreEqual(
				FString(Case.Operator),
				FString(UTF8_TO_TCHAR(
					View.Expression->operatorToken.AddressOf()))));
			ASSERT_THAT(AreEqual(
				2,
				static_cast<int32>(View.Expression->operands.GetLength())));
			if (View.Expression->operands.GetLength() != 2)
			{
				continue;
			}
			const asSTypedSemanticExpression& ConvertedRight =
				View.HIR->expressions[
					View.Expression->operands[1].value];
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::Conversion,
				ConvertedRight.kind));
			ASSERT_THAT(AreEqual(
				1,
				static_cast<int32>(ConvertedRight.operands.GetLength())));
			ASSERT_THAT(AreEqual(
				Case.bBooleanResult,
				View.Expression->type.IsBooleanType()));
			ASSERT_THAT(AreEqual(
				Case.bWideIntegerOperand,
				ConvertedRight.type.IsIntegerType()
					&& ConvertedRight.type.GetSizeInMemoryDWords() == 2));
			ASSERT_THAT(AreEqual(
				!Case.bWideIntegerOperand,
				ConvertedRight.type.IsFloat32Type()));
		}

		asCScriptFunction* const CallFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module,
				"float32 CallConversion(int Value)"));
		const FReturnExpressionView Call =
			GetSingleReturnExpression(CallFunction);
		ASSERT_THAT(IsNotNull(Call.HIR));
		ASSERT_THAT(IsNotNull(Call.Expression));
		if (Call.HIR != nullptr && Call.Expression != nullptr)
		{
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*Call.HIR).IsValid()));
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::ResolvedCall,
				Call.Expression->kind));
			ASSERT_THAT(AreEqual(
				1,
				static_cast<int32>(Call.Expression->operands.GetLength())));
			if (Call.Expression->operands.GetLength() == 1)
			{
				const asSTypedSemanticExpression& Argument =
					Call.HIR->expressions[
						Call.Expression->operands[0].value];
				ASSERT_THAT(AreEqual(
					asETypedSemanticExpressionKind::Conversion,
					Argument.kind));
				ASSERT_THAT(IsTrue(Argument.type.IsFloat32Type()));
			}
		}

		asCScriptFunction* const LocalFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module,
				"float32 LocalConversion(int Value)"));
		const asCTypedSemanticFunction* const LocalHIR =
			LocalFunction != nullptr
				? LocalFunction->GetTypedSemanticFunction()
				: nullptr;
		ASSERT_THAT(IsNotNull(LocalHIR));
		if (LocalHIR != nullptr)
		{
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*LocalHIR).IsValid()));
			const asSTypedSemanticStatement* const Local =
				FindStatement(
					*LocalHIR,
					asETypedSemanticStatementKind::LocalDeclaration);
			ASSERT_THAT(IsNotNull(Local));
			if (Local != nullptr && Local->initializerExpression.IsValid())
			{
				const asSTypedSemanticExpression& Initializer =
					LocalHIR->expressions[
						Local->initializerExpression.value];
				ASSERT_THAT(AreEqual(
					asETypedSemanticExpressionKind::Conversion,
					Initializer.kind));
				ASSERT_THAT(IsTrue(Initializer.type.IsFloat32Type()));
			}
		}
	}

};

#endif
