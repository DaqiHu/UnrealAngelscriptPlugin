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

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRResolvedCallTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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
	TEST_METHOD(VoidDiscardAndResolvedCallsRetainExpressionIdentity)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-VOID-DISCARD-CALL",
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
			"TypedSemanticIRVoidDiscardCalls",
			ASTEST_AS_ANSI(R"AS(
				void VoidTarget()
				{
				}

				int ValueTarget(int Value)
				{
					return Value + 1;
				}

				void DiscardValue(int Value)
				{
					ValueTarget(Value);
				}

				void DiscardVoid()
				{
					VoidTarget();
				}

				int CallInitializer(int Value)
				{
					int Result = ValueTarget(Value);
					return Result;
				}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		struct FDiscardCase
		{
			const char* Declaration;
			bool bReturnsVoid;
			int32 OperandCount;
		};
		const FDiscardCase DiscardCases[] = {
			{"void DiscardValue(int Value)", false, 1},
			{"void DiscardVoid()", true, 0},
		};

		for (const FDiscardCase& Case : DiscardCases)
		{
			asCScriptFunction* const Function =
				static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
					Module,
					Case.Declaration));
			const asCTypedSemanticFunction* const HIR = Function != nullptr
				? Function->GetTypedSemanticFunction()
				: nullptr;
			ASSERT_THAT(IsNotNull(HIR));
			if (HIR == nullptr)
			{
				continue;
			}
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
			const asSTypedSemanticStatement* const Statement =
				FindStatement(*HIR, asETypedSemanticStatementKind::Expression);
			ASSERT_THAT(IsNotNull(Statement,
				TEXT("A discarded call should retain one expression statement")));
			if (Statement == nullptr || !Statement->valueExpression.IsValid())
			{
				continue;
			}
			const asSTypedSemanticExpression& Call =
				HIR->expressions[Statement->valueExpression.value];
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::ResolvedCall,
				Call.kind));
			ASSERT_THAT(AreEqual(
				Case.OperandCount,
				static_cast<int32>(Call.operands.GetLength())));
			ASSERT_THAT(AreEqual(
				Case.bReturnsVoid,
				Call.type.GetTokenType() == ttVoid));
		}

		asCScriptFunction* const InitializerFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module,
				"int CallInitializer(int Value)"));
		const asCTypedSemanticFunction* const InitializerHIR =
			InitializerFunction != nullptr
				? InitializerFunction->GetTypedSemanticFunction()
				: nullptr;
		ASSERT_THAT(IsNotNull(InitializerHIR));
		if (InitializerHIR != nullptr)
		{
			ASSERT_THAT(IsTrue(
				VerifyTypedSemanticFunction(*InitializerHIR).IsValid()));
			const asSTypedSemanticStatement* const Local =
				FindStatement(
					*InitializerHIR,
					asETypedSemanticStatementKind::LocalDeclaration);
			ASSERT_THAT(IsNotNull(Local));
			if (Local != nullptr && Local->initializerExpression.IsValid())
			{
				const asSTypedSemanticExpression& Call =
					InitializerHIR->expressions[
						Local->initializerExpression.value];
				ASSERT_THAT(AreEqual(
					asETypedSemanticExpressionKind::ResolvedCall,
					Call.kind));
				ASSERT_THAT(IsTrue(Call.type.IsIntegerType()));
			}
		}
	}
};

#endif
