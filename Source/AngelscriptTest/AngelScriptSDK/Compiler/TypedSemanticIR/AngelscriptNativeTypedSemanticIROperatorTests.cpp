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

TEST_CLASS_WITH_FLAGS(FTypedSemanticIROperatorTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const asSTypedSemanticExpression* GetSingleReturnExpression(
		asCScriptFunction* Function,
		const asCTypedSemanticFunction*& OutHIR)
	{
		OutHIR = Function != nullptr
			? Function->GetTypedSemanticFunction()
			: nullptr;
		if (OutHIR == nullptr)
		{
			return nullptr;
		}

		const asSTypedSemanticStatement* Return = nullptr;
		for (asUINT StatementIndex = 0;
			StatementIndex < OutHIR->statements.GetLength();
			++StatementIndex)
		{
			const asSTypedSemanticStatement& Statement =
				OutHIR->statements[StatementIndex];
			if (Statement.kind != asETypedSemanticStatementKind::Return)
			{
				continue;
			}
			if (Return != nullptr)
			{
				return nullptr;
			}
			Return = &Statement;
		}

		if (Return == nullptr
			|| !Return->valueExpression.IsValid()
			|| Return->valueExpression.value >= OutHIR->expressions.GetLength())
		{
			return nullptr;
		}
		return &OutHIR->expressions[Return->valueExpression.value];
	}

public:
	TEST_METHOD(LiteralsEnumsParametersAndLocalsRetainExactTypes)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SCALAR-LEAVES",
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
			"TypedSemanticIRScalarLeaves",
			ASTEST_AS_ANSI(R"AS(
				enum EScalarValue
				{
					Zero = 0,
					Two = 2,
				}

				int PrimitiveLiteral()
				{
					return 42;
				}

				EScalarValue EnumLiteral()
				{
					return EScalarValue::Two;
				}

				int SymbolLocal(int Input)
				{
					int Local = Input;
					return Local;
				}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		const asCTypedSemanticFunction* LiteralHIR = nullptr;
		const asSTypedSemanticExpression* const Literal =
			GetSingleReturnExpression(
				static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
					Module,
					"int PrimitiveLiteral()")),
				LiteralHIR);
		ASSERT_THAT(IsNotNull(LiteralHIR));
		ASSERT_THAT(IsNotNull(Literal));
		if (LiteralHIR != nullptr && Literal != nullptr)
		{
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*LiteralHIR).IsValid()));
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::Literal,
				Literal->kind));
			ASSERT_THAT(IsTrue(Literal->type.IsIntegerType()));
			ASSERT_THAT(AreEqual(
				FString(TEXT("42")),
				FString(UTF8_TO_TCHAR(Literal->literal.AddressOf()))));
		}

		const asCTypedSemanticFunction* EnumHIR = nullptr;
		const asSTypedSemanticExpression* const Enum =
			GetSingleReturnExpression(
				static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
					Module,
					"EScalarValue EnumLiteral()")),
				EnumHIR);
		ASSERT_THAT(IsNotNull(EnumHIR));
		ASSERT_THAT(IsNotNull(Enum));
		if (EnumHIR != nullptr && Enum != nullptr)
		{
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*EnumHIR).IsValid()));
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::Literal,
				Enum->kind));
			ASSERT_THAT(IsTrue(Enum->type.IsEnumType()));
			ASSERT_THAT(AreEqual(
				FString(TEXT("2")),
				FString(UTF8_TO_TCHAR(Enum->literal.AddressOf()))));
		}

		asCScriptFunction* const SymbolFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module,
				"int SymbolLocal(int Input)"));
		const asCTypedSemanticFunction* SymbolHIR = nullptr;
		const asSTypedSemanticExpression* const Symbol =
			GetSingleReturnExpression(SymbolFunction, SymbolHIR);
		ASSERT_THAT(IsNotNull(SymbolHIR));
		ASSERT_THAT(IsNotNull(Symbol));
		if (SymbolHIR != nullptr && Symbol != nullptr)
		{
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*SymbolHIR).IsValid()));
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::Symbol,
				Symbol->kind));
			ASSERT_THAT(IsTrue(Symbol->type.IsIntegerType()));
			ASSERT_THAT(IsTrue(Symbol->symbol.IsValid()));
			if (Symbol->symbol.IsValid())
			{
				ASSERT_THAT(AreEqual(
					asETypedSemanticSymbolKind::Local,
					SymbolHIR->symbols[Symbol->symbol.value].kind));
				ASSERT_THAT(AreEqual(
					FString(TEXT("Local")),
					FString(UTF8_TO_TCHAR(
						SymbolHIR->symbols[Symbol->symbol.value]
							.name.AddressOf()))));
			}

			const asSTypedSemanticStatement* Local = nullptr;
			for (asUINT StatementIndex = 0;
				StatementIndex < SymbolHIR->statements.GetLength();
				++StatementIndex)
			{
				if (SymbolHIR->statements[StatementIndex].kind
					== asETypedSemanticStatementKind::LocalDeclaration)
				{
					Local = &SymbolHIR->statements[StatementIndex];
					break;
				}
			}
			ASSERT_THAT(IsNotNull(Local));
			if (Local != nullptr && Local->initializerExpression.IsValid())
			{
				const asSTypedSemanticExpression& Initializer =
					SymbolHIR->expressions[
						Local->initializerExpression.value];
				ASSERT_THAT(AreEqual(
					asETypedSemanticExpressionKind::Symbol,
					Initializer.kind));
				ASSERT_THAT(AreEqual(
					asETypedSemanticSymbolKind::Parameter,
					SymbolHIR->symbols[
						Initializer.symbol.value].kind));
			}
		}
	}

	TEST_METHOD(PrimitiveOperatorFamiliesRetainExactTokenOrderAndResultType)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-PRIMITIVE-OPERATORS",
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
			"TypedSemanticIRPrimitiveOperators",
			ASTEST_AS_ANSI(R"AS(
				int OpAdd(int A, int B) { return A + B; }
				int OpSubtract(int A, int B) { return A - B; }
				int OpMultiply(int A, int B) { return A * B; }
				int OpDivide(int A, int B) { return A / B; }
				int OpModulo(int A, int B) { return A % B; }
				bool OpEqual(int A, int B) { return A == B; }
				bool OpNotEqual(int A, int B) { return A != B; }
				bool OpLess(int A, int B) { return A < B; }
				bool OpLessEqual(int A, int B) { return A <= B; }
				bool OpGreater(int A, int B) { return A > B; }
				bool OpGreaterEqual(int A, int B) { return A >= B; }
				int OpBitAnd(int A, int B) { return A & B; }
				int OpBitOr(int A, int B) { return A | B; }
				int OpBitXor(int A, int B) { return A ^ B; }
				int OpShiftLeft(int A, int B) { return A << B; }
				int OpShiftRight(int A, int B) { return A >> B; }
				bool OpLogicalAnd(bool A, bool B) { return A && B; }
				bool OpLogicalOr(bool A, bool B) { return A || B; }
				int OpUnaryPlus(int A) { return +A; }
				int OpUnaryMinus(int A) { return -A; }
				int OpBitNot(int A) { return ~A; }
				bool OpLogicalNot(bool A) { return !A; }
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		enum class EExpectedType : uint8
		{
			Integer,
			Boolean,
		};
		struct FOperatorCase
		{
			const char* Declaration;
			const TCHAR* Token;
			asETypedSemanticExpressionKind Kind;
			EExpectedType Type;
			int32 OperandCount;
		};
		const FOperatorCase Cases[] = {
			{"int OpAdd(int A, int B)", TEXT("+"), asETypedSemanticExpressionKind::Binary, EExpectedType::Integer, 2},
			{"int OpSubtract(int A, int B)", TEXT("-"), asETypedSemanticExpressionKind::Binary, EExpectedType::Integer, 2},
			{"int OpMultiply(int A, int B)", TEXT("*"), asETypedSemanticExpressionKind::Binary, EExpectedType::Integer, 2},
			{"int OpDivide(int A, int B)", TEXT("/"), asETypedSemanticExpressionKind::Binary, EExpectedType::Integer, 2},
			{"int OpModulo(int A, int B)", TEXT("%"), asETypedSemanticExpressionKind::Binary, EExpectedType::Integer, 2},
			{"bool OpEqual(int A, int B)", TEXT("=="), asETypedSemanticExpressionKind::Binary, EExpectedType::Boolean, 2},
			{"bool OpNotEqual(int A, int B)", TEXT("!="), asETypedSemanticExpressionKind::Binary, EExpectedType::Boolean, 2},
			{"bool OpLess(int A, int B)", TEXT("<"), asETypedSemanticExpressionKind::Binary, EExpectedType::Boolean, 2},
			{"bool OpLessEqual(int A, int B)", TEXT("<="), asETypedSemanticExpressionKind::Binary, EExpectedType::Boolean, 2},
			{"bool OpGreater(int A, int B)", TEXT(">"), asETypedSemanticExpressionKind::Binary, EExpectedType::Boolean, 2},
			{"bool OpGreaterEqual(int A, int B)", TEXT(">="), asETypedSemanticExpressionKind::Binary, EExpectedType::Boolean, 2},
			{"int OpBitAnd(int A, int B)", TEXT("&"), asETypedSemanticExpressionKind::Binary, EExpectedType::Integer, 2},
			{"int OpBitOr(int A, int B)", TEXT("|"), asETypedSemanticExpressionKind::Binary, EExpectedType::Integer, 2},
			{"int OpBitXor(int A, int B)", TEXT("^"), asETypedSemanticExpressionKind::Binary, EExpectedType::Integer, 2},
			{"int OpShiftLeft(int A, int B)", TEXT("<<"), asETypedSemanticExpressionKind::Binary, EExpectedType::Integer, 2},
			{"int OpShiftRight(int A, int B)", TEXT(">>"), asETypedSemanticExpressionKind::Binary, EExpectedType::Integer, 2},
			{"bool OpLogicalAnd(bool A, bool B)", TEXT("&&"), asETypedSemanticExpressionKind::ShortCircuit, EExpectedType::Boolean, 2},
			{"bool OpLogicalOr(bool A, bool B)", TEXT("||"), asETypedSemanticExpressionKind::ShortCircuit, EExpectedType::Boolean, 2},
			{"int OpUnaryPlus(int A)", TEXT("+"), asETypedSemanticExpressionKind::Unary, EExpectedType::Integer, 1},
			{"int OpUnaryMinus(int A)", TEXT("-"), asETypedSemanticExpressionKind::Unary, EExpectedType::Integer, 1},
			{"int OpBitNot(int A)", TEXT("~"), asETypedSemanticExpressionKind::Unary, EExpectedType::Integer, 1},
			{"bool OpLogicalNot(bool A)", TEXT("!"), asETypedSemanticExpressionKind::Unary, EExpectedType::Boolean, 1},
		};

		for (const FOperatorCase& Case : Cases)
		{
			asCScriptFunction* const Function =
				static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
					Module,
					Case.Declaration));
			const asCTypedSemanticFunction* HIR = nullptr;
			const asSTypedSemanticExpression* const Expression =
				GetSingleReturnExpression(Function, HIR);
			const FString CaseName = UTF8_TO_TCHAR(Case.Declaration);
			ASSERT_THAT(IsNotNull(HIR,
				*FString::Printf(TEXT("%s should publish typed HIR"), *CaseName)));
			ASSERT_THAT(IsNotNull(Expression,
				*FString::Printf(TEXT("%s should retain one return expression"), *CaseName)));
			if (HIR == nullptr || Expression == nullptr)
			{
				continue;
			}
			ASSERT_THAT(IsTrue(
				VerifyTypedSemanticFunction(*HIR).IsValid(),
				*FString::Printf(TEXT("%s HIR should verify"), *CaseName)));
			ASSERT_THAT(AreEqual(Case.Kind, Expression->kind));
			ASSERT_THAT(AreEqual(
				FString(Case.Token),
				FString(UTF8_TO_TCHAR(
					Expression->operatorToken.AddressOf()))));
			ASSERT_THAT(AreEqual(
				Case.OperandCount,
				static_cast<int32>(Expression->operands.GetLength())));
			ASSERT_THAT(AreEqual(
				Case.Type == EExpectedType::Integer,
				Expression->type.IsIntegerType()));
			ASSERT_THAT(AreEqual(
				Case.Type == EExpectedType::Boolean,
				Expression->type.IsBooleanType()));
			for (asUINT OperandIndex = 0;
				OperandIndex < Expression->operands.GetLength();
				++OperandIndex)
			{
				ASSERT_THAT(IsTrue(
					Expression->operands[OperandIndex].value
						< Expression->id.value,
					TEXT("Operands should retain source order and precede their parent")));
			}
		}
	}
};

#endif
