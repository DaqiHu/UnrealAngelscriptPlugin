#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeTypedSemanticIRCallRewriteTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRCompileOutTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.CallRewrites.CompileOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CompilerRewritesPublishFinalValuesWithoutExecutableCalls)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIRCallRewriteTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-CALL-REWRITES",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		NormalInvocationCount = 0;
		ErasedInvocationCount = 0;
		ReplacedInvocationCount = 0;
		MethodChainInvocationCount = 0;

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
		const FRegisteredRewriteFunctions Registered =
			RegisterRewriteFunctions(*ScriptEngine);
		ASSERT_THAT(IsTrue(Registered.IsValid(),
			TEXT("The rewrite fixture must register all four dispositions")));
		if (!Registered.IsValid())
		{
			return;
		}

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedSemanticIRCallRewrites",
			ASTEST_AS_ANSI(R"AS(
				int ScalarRewriteEntry(int Value)
				{
					RewriteErased(MissingErasedArgument);
					return RewriteReplaced(
						Value + 1,
						MissingReplacementArgument)
						+ RewriteNormal(2);
				}

				int MethodChainRewriteEntry()
				{
					FCallRewriteChainProbe Value;
					Value.Value = 37;
					return Value.Chain(MissingMethodChainArgument).Value;
				}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(),
			TEXT("Compile-out arguments that bytecode never evaluates must not be resolved as source expressions")));
		if (!Module.IsValid())
		{
			return;
		}

		asCScriptFunction* const ScalarEntry =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module, "int ScalarRewriteEntry(int Value)"));
		asCScriptFunction* const MethodChainEntry =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module, "int MethodChainRewriteEntry()"));
		ASSERT_THAT(IsNotNull(ScalarEntry));
		ASSERT_THAT(IsNotNull(MethodChainEntry));
		if (ScalarEntry == nullptr || MethodChainEntry == nullptr)
		{
			return;
		}

		const asCTypedSemanticFunction* const ScalarHIR =
			ScalarEntry->GetTypedSemanticFunction();
		const asCTypedSemanticFunction* const MethodChainHIR =
			MethodChainEntry->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(ScalarHIR,
			TEXT("An erased expression statement must retain verified final HIR")));
		ASSERT_THAT(IsNotNull(MethodChainHIR,
			TEXT("A method-chain rewrite must retain verified final HIR")));
		if (ScalarHIR == nullptr || MethodChainHIR == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*ScalarHIR).IsValid()));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*MethodChainHIR).IsValid()));

		const asSTypedSemanticExpression* const Erased =
			FindCallRewrite(
				*ScalarHIR,
				asETypedSemanticCompileOutKind::CompileOutEntirely);
		const asSTypedSemanticExpression* const Replaced =
			FindCallRewrite(
				*ScalarHIR,
				asETypedSemanticCompileOutKind::ReplaceWithFirstParam);
		const asSTypedSemanticExpression* const MethodChain =
			FindCallRewrite(
				*MethodChainHIR,
				asETypedSemanticCompileOutKind::CompileOutAsMethodChain);
		ASSERT_THAT(IsNotNull(Erased,
			TEXT("CompileOutEntirely must become a non-call final void marker")));
		ASSERT_THAT(IsNotNull(Replaced,
			TEXT("ReplaceWithFirstParam must retain the selected first value")));
		ASSERT_THAT(IsNotNull(MethodChain,
			TEXT("CompileOutAsMethodChain must retain the final chain value")));
		ASSERT_THAT(IsTrue(Erased == nullptr
			|| Erased->type.GetTokenType() == ttVoid));
		ASSERT_THAT(IsTrue(Replaced == nullptr
			|| Replaced->operands.GetLength() == 1));
		ASSERT_THAT(IsTrue(Replaced == nullptr
			|| (Replaced->evaluationSequence.GetLength() == 1
				&& Replaced->evaluationSequence[0] == 0
				&& Replaced->argumentOrigins.GetLength() == 1
				&& Replaced->argumentOrigins[0]
					== asETypedSemanticArgumentOrigin::SourcePositional)));
		ASSERT_THAT(IsTrue(MethodChain == nullptr
			|| MethodChain->operands.GetLength() == 1));
		ASSERT_THAT(IsTrue(MethodChain == nullptr
			|| (MethodChain->receiverExpression.IsValid()
				&& MethodChain->receiverExpression == MethodChain->operands[0]
				&& MethodChain->argumentOrigins.GetLength() == 1
				&& MethodChain->argumentOrigins[0]
					== asETypedSemanticArgumentOrigin::Receiver)));

		ASSERT_THAT(IsTrue(HasResolvedCallTo(*ScalarHIR, Registered.Normal),
			TEXT("CompileCalls must remain an executable resolved call")));
		ASSERT_THAT(IsFalse(HasResolvedCallTo(*ScalarHIR, Registered.Erased),
			TEXT("CompileOutEntirely must never be exposed as an executable HIR call")));
		ASSERT_THAT(IsFalse(HasResolvedCallTo(*ScalarHIR, Registered.Replaced),
			TEXT("ReplaceWithFirstParam must never retain the replaced call")));
		ASSERT_THAT(IsFalse(HasResolvedCallTo(
			*MethodChainHIR, Registered.MethodChain),
			TEXT("CompileOutAsMethodChain must never retain the compiled-out call")));

		int32 ScalarValue = 0;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteGlobalIntWithInt(
				*ScriptEngine, *ScalarEntry, 10, ScalarValue)));
		ASSERT_THAT(AreEqual(16, ScalarValue,
			TEXT("The rewritten value and ordinary native call must match VM bytecode")));
		int32 MethodChainValue = 0;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteGlobalIntWithInt(
				*ScriptEngine, *MethodChainEntry, 0, MethodChainValue)));
		ASSERT_THAT(AreEqual(37, MethodChainValue));
		ASSERT_THAT(AreEqual(1, NormalInvocationCount));
		ASSERT_THAT(AreEqual(0, ErasedInvocationCount));
		ASSERT_THAT(AreEqual(0, ReplacedInvocationCount));
		ASSERT_THAT(AreEqual(0, MethodChainInvocationCount));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
