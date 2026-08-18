#include "CQTest.h"

#include "AngelscriptTestMacros.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITCompileOutTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.CompileOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asSTypedSemanticSourceSpan MakeSpan()
	{
		asSTypedSemanticSourceSpan Span;
		Span.section = "typed_ast_compile_out.as";
		Span.sourceLength = 1;
		return Span;
	}

	static void PopulateCompileOutTarget(
		asSTypedSemanticExpression& Expression,
		const char* const CanonicalDeclaration,
		const int32 FunctionId)
	{
		Expression.span = MakeSpan();
		Expression.callTargetKind =
			asETypedSemanticCallTargetKind::SystemFunction;
		Expression.resolvedFunctionId = FunctionId;
		Expression.targetProvenance.canonicalDeclaration =
			CanonicalDeclaration;
		Expression.targetProvenance.bodyAvailability =
			asETypedSemanticBodyAvailability::RegisteredNative;
		Expression.targetProvenance.bodyOwnership =
			asETypedSemanticBodyOwnership::NativeRuntime;
	}

	static void MarkVerifiedEmptyCleanup(
		asCTypedSemanticFunction& Function)
	{
		asSTypedSemanticCleanupPlan EmptyCleanup;
		Function.header.universalCleanupPlan =
			Function.AddCleanupPlan(EmptyCleanup);
		Function.header.cleanupPlanState =
			asETypedSemanticCleanupPlanState::VerifiedEmpty;
		Function.header.cleanupPlanCoversAllTransfers = true;
		for (asUINT Index = 0; Index < Function.statements.GetLength(); ++Index)
		{
			asSTypedSemanticStatement& Statement = Function.statements[Index];
			if (Statement.kind == asETypedSemanticStatementKind::Return)
			{
				Statement.cleanupPlan =
					Function.header.universalCleanupPlan;
			}
		}
	}

	static asCTypedSemanticFunction MakeCompileOutFunction()
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int CompileOutProbe()";
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);

		asSTypedSemanticExpression VoidRewrite;
		VoidRewrite.kind = asETypedSemanticExpressionKind::CallRewrite;
		VoidRewrite.type = asCDataType::CreatePrimitive(ttVoid, false);
		VoidRewrite.callRewriteKind =
			asETypedSemanticCompileOutKind::CompileOutEntirely;
		PopulateCompileOutTarget(
			VoidRewrite, "void CompiledOutSideEffect()", 41);
		const asTypedSemanticExpressionId VoidRewriteId =
			Function.AddExpression(VoidRewrite);

		asSTypedSemanticExpression Literal;
		Literal.kind = asETypedSemanticExpressionKind::Literal;
		Literal.type = IntType;
		Literal.span = MakeSpan();
		Literal.literal = "7";
		const asTypedSemanticExpressionId LiteralId =
			Function.AddExpression(Literal);

		asSTypedSemanticExpression ValueRewrite;
		ValueRewrite.kind = asETypedSemanticExpressionKind::CallRewrite;
		ValueRewrite.type = IntType;
		ValueRewrite.callRewriteKind =
			asETypedSemanticCompileOutKind::ReplaceWithFirstParam;
		PopulateCompileOutTarget(
			ValueRewrite, "int ReplacedTarget(int)", 42);
		ValueRewrite.operands.PushLast(LiteralId);
		ValueRewrite.argumentOrigins.PushLast(
			asETypedSemanticArgumentOrigin::SourcePositional);
		ValueRewrite.sourceArgumentOrdinals.PushLast(-1);
		ValueRewrite.evaluationSequence.PushLast(0);
		asSTypedSemanticEvaluationStep EvaluationStep;
		EvaluationStep.role =
			asETypedSemanticEvaluationRole::Argument;
		EvaluationStep.expression = LiteralId;
		EvaluationStep.formalParameterIndex = -1;
		EvaluationStep.sourceArgumentOrdinal = -1;
		ValueRewrite.evaluationSteps.PushLast(EvaluationStep);
		const asTypedSemanticExpressionId ValueRewriteId =
			Function.AddExpression(ValueRewrite);

		asSTypedSemanticStatement CompileOutStatement;
		CompileOutStatement.kind =
			asETypedSemanticStatementKind::Expression;
		CompileOutStatement.span = MakeSpan();
		CompileOutStatement.valueExpression = VoidRewriteId;
		const asTypedSemanticStatementId CompileOutStatementId =
			Function.AddStatement(CompileOutStatement);

		asSTypedSemanticStatement ReturnStatement;
		ReturnStatement.kind = asETypedSemanticStatementKind::Return;
		ReturnStatement.span = MakeSpan();
		ReturnStatement.safePointRole =
			asETypedSemanticSafePointRole::Return;
		ReturnStatement.valueExpression = ValueRewriteId;
		ReturnStatement.exitedScopeCount = 1;
		const asTypedSemanticStatementId ReturnStatementId =
			Function.AddStatement(ReturnStatement);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(CompileOutStatementId);
		Root.children.PushLast(ReturnStatementId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeMethodChainFunction()
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int MethodChainProbe()";
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);

		asSTypedSemanticExpression Receiver;
		Receiver.kind = asETypedSemanticExpressionKind::Literal;
		Receiver.type = IntType;
		Receiver.span = MakeSpan();
		Receiver.literal = "37";
		const asTypedSemanticExpressionId ReceiverId =
			Function.AddExpression(Receiver);

		asSTypedSemanticExpression MethodChain;
		MethodChain.kind = asETypedSemanticExpressionKind::CallRewrite;
		MethodChain.type = IntType;
		MethodChain.callRewriteKind =
			asETypedSemanticCompileOutKind::CompileOutAsMethodChain;
		PopulateCompileOutTarget(
			MethodChain, "int ChainProbe::Chain()", 43);
		MethodChain.receiverExpression = ReceiverId;
		MethodChain.operands.PushLast(ReceiverId);
		MethodChain.argumentOrigins.PushLast(
			asETypedSemanticArgumentOrigin::Receiver);
		MethodChain.sourceArgumentOrdinals.PushLast(-1);
		MethodChain.evaluationSequence.PushLast(0);
		asSTypedSemanticEvaluationStep EvaluationStep;
		EvaluationStep.role =
			asETypedSemanticEvaluationRole::Receiver;
		EvaluationStep.expression = ReceiverId;
		EvaluationStep.formalParameterIndex = -1;
		EvaluationStep.sourceArgumentOrdinal = -1;
		MethodChain.evaluationSteps.PushLast(EvaluationStep);
		const asTypedSemanticExpressionId MethodChainId =
			Function.AddExpression(MethodChain);

		asSTypedSemanticStatement ReturnStatement;
		ReturnStatement.kind = asETypedSemanticStatementKind::Return;
		ReturnStatement.span = MakeSpan();
		ReturnStatement.safePointRole =
			asETypedSemanticSafePointRole::Return;
		ReturnStatement.valueExpression = MethodChainId;
		ReturnStatement.exitedScopeCount = 1;
		const asTypedSemanticStatementId ReturnStatementId =
			Function.AddStatement(ReturnStatement);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ReturnStatementId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

public:
	TEST_METHOD(RewritesReturnTheRetainedValueWithoutCallingTheTarget)
	{
		const asCTypedSemanticFunction Function = MakeCompileOutFunction();
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("ASJIT_CompileOutProbe");
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);

		const FAngelscriptTypedASTJITEmission First =
			EmitTypedASTJITFunction(Function, Shape, {});
		const FAngelscriptTypedASTJITEmission Second =
			EmitTypedASTJITFunction(Function, Shape, {});

		ASSERT_THAT(IsTrue(First.bSuccess, First.FailureDetail));
		ASSERT_THAT(AreEqual(First.Definition, Second.Definition,
			TEXT("Call-rewrite output must be byte deterministic")));
		ASSERT_THAT(IsTrue(First.Definition.Contains(
			TEXT("\t(void)0;\n"), ESearchCase::CaseSensitive),
			TEXT("CompileOutEntirely must emit only its final void marker")));
		ASSERT_THAT(IsTrue(First.Definition.Contains(
			TEXT("\treturn int32(7);\n"), ESearchCase::CaseSensitive),
			TEXT("ReplaceWithFirstParam must return its retained scalar value")));
		ASSERT_THAT(IsFalse(First.Definition.Contains(
			TEXT("CompiledOutSideEffect"), ESearchCase::CaseSensitive),
			TEXT("CompileOutEntirely must not emit its registered target")));
		ASSERT_THAT(IsFalse(First.Definition.Contains(
			TEXT("ReplacedTarget"), ESearchCase::CaseSensitive),
			TEXT("ReplaceWithFirstParam must not emit its registered target")));
		ASSERT_THAT(AreEqual(0, First.NativeCallSites.Num(),
			TEXT("Final call rewrites must not publish native call-site metadata")));
		ASSERT_THAT(IsFalse(First.Definition.Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(MethodChainReturnsOnlyTheRetainedReceiver)
	{
		const asCTypedSemanticFunction Function = MakeMethodChainFunction();
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("ASJIT_MethodChainProbe");
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);

		const FAngelscriptTypedASTJITEmission First =
			EmitTypedASTJITFunction(Function, Shape, {});
		const FAngelscriptTypedASTJITEmission Second =
			EmitTypedASTJITFunction(Function, Shape, {});

		ASSERT_THAT(IsTrue(First.bSuccess, First.FailureDetail));
		ASSERT_THAT(AreEqual(First.Definition, Second.Definition,
			TEXT("Method-chain rewrite output must be byte deterministic")));
		ASSERT_THAT(IsTrue(First.Definition.Contains(
			TEXT("\treturn int32(37);\n"), ESearchCase::CaseSensitive),
			TEXT("Method-chain compile-out must return its exact receiver")));
		ASSERT_THAT(IsFalse(First.Definition.Contains(
			TEXT("ChainProbe::Chain"), ESearchCase::CaseSensitive),
			TEXT("Method-chain compile-out must not invoke its target")));
		ASSERT_THAT(AreEqual(0, First.NativeCallSites.Num(),
			TEXT("Method-chain compile-out must not publish native call metadata")));
		ASSERT_THAT(IsFalse(First.Definition.Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
