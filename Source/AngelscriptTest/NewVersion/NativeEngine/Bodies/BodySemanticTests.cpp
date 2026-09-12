#include "CQTest.h"

#include "../NativeEngineTestSupport.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/Sema/as_body_fragment.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/Basic/as_identifier_table.h"
#include "frontend/Lexer/as_tokenizer.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace BodySemanticTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	TEST_CLASS_WITH_FLAGS(BodiesCore, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(DeferredBodyRetainsSelectedConditionalBranch)
		{
			const FString Source = TEXT(
				"int Selected()\n"
				"{\n"
				"#if FEATURE\n"
				"    return 11;\n"
				"#else\n"
				"    return 22;\n"
				"#endif\n"
				"}\n");
			const TPair<bool, int64> Cases[] = {{true, 11}, {false, 22}};
			for (const auto& Case : Cases)
			{
				FSessionRun Run({{TEXT("ConditionalBody.as"), Source}}, {{TEXT("FEATURE"), Case.Key}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
				auto* Function = Run.Function(TEXT("Selected"));
				ASSERT_THAT(IsNotNull(Function));
				auto* Body = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Function->GetBody());
				ASSERT_THAT(IsNotNull(Body, TEXT("The selected body must not disappear after preprocessing failure.")));
				ASSERT_THAT(AreEqual(1, Body->GetStatements().Num()));
				auto* Return = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0]);
				ASSERT_THAT(IsNotNull(Return));
				auto* Value = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Return->GetValue());
				ASSERT_THAT(IsNotNull(Value));
				ASSERT_THAT(AreEqual(Case.Value, Value->GetValue()));
				ASSERT_THAT(AreEqual(asUINT(0), Run.Diagnostics.GetPendingCount()));
			}
		}

		TEST_METHOD(DeferredBodyPreservesOuterConditionalContext)
		{
			FSessionRun Run({{TEXT("OuterConditional.as"), TEXT(
				"#if FEATURE\n"
				"int Selected()\n"
				"{\n"
				"#endif\n"
				"    return 33;\n"
				"#if FEATURE\n"
				"}\n"
				"#endif\n")}}, {{TEXT("FEATURE"), true}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Function = Run.Function(TEXT("Selected"));
			ASSERT_THAT(IsNotNull(Function));
			auto* Body = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Function->GetBody());
			ASSERT_THAT(IsNotNull(Body));
			ASSERT_THAT(AreEqual(1, Body->GetStatements().Num()));
			auto* Return = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0]);
			ASSERT_THAT(IsNotNull(Return));
			auto* Value = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Return->GetValue());
			ASSERT_THAT(IsNotNull(Value));
			ASSERT_THAT(AreEqual(33ll, Value->GetValue()));
			ASSERT_THAT(AreEqual(asUINT(0), Run.Diagnostics.GetPendingCount()));
		}

		TEST_METHOD(DeferredIdentifiersSurviveCallerOwners)
		{
			FSessionRun Run({{TEXT("CallerLifetime.as"), TEXT(
				"int Callee() { return 7; }\n"
				"int Caller() { return Callee(); }\n")}});
			// FSessionRun has already destroyed all caller-owned identifier tables.
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Function = Run.Function(TEXT("Caller"));
			ASSERT_THAT(IsNotNull(Function));
			auto* Body = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Function->GetBody());
			ASSERT_THAT(IsNotNull(Body));
			auto* Return = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0]);
			ASSERT_THAT(IsNotNull(Return));
			auto* Call = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Return->GetValue());
			ASSERT_THAT(IsNotNull(Call));
			auto* Reference = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Call->GetCallee());
			ASSERT_THAT(IsNotNull(Reference));
			ASSERT_THAT(IsTrue(Reference->GetDecl() == Run.Function(TEXT("Callee"))));
		}

		TEST_METHOD(DeferredTokensPreserveOriginalUnicodePolicy)
		{
			FSessionRun Run({{TEXT("UnicodeBody.as"), TEXT(
				"int \u6570\u503c() { return 7; }\n"
				"int Caller() { return \u6570\u503c(); }\n")}}, {},
				AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AllowUnicode);
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Function = Run.Function(TEXT("Caller"));
			ASSERT_THAT(IsNotNull(Function));
			auto* Body = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Function->GetBody());
			ASSERT_THAT(IsNotNull(Body));
			auto* Return = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0]);
			ASSERT_THAT(IsNotNull(Return));
			auto* Call = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Return->GetValue());
			ASSERT_THAT(IsNotNull(Call));
			auto* Reference = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Call->GetCallee());
			ASSERT_THAT(IsNotNull(Reference));
			ASSERT_THAT(IsTrue(Reference->GetDecl() == Run.Function(TEXT("\u6570\u503c"))));
			ASSERT_THAT(AreEqual(asUINT(0), Run.Diagnostics.GetPendingCount()));
		}

		TEST_METHOD(DeclarationsRemainInspectableBeforeBodyResume)
		{
			FSessionRun Run({{TEXT("Resume.as"), TEXT("int Value() { return 42; }\n")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			auto* Function = Run.Function(TEXT("Value"));
			ASSERT_THAT(IsNotNull(Function));
			const AS_NAMESPACE_QUALIFIER asSStableKey DeclarationKey = Function->GetStableIdentity();
			const FString DeclarationProjection = Run.Session->SerializeStableProjection();
			ASSERT_THAT(IsNull(Function->GetBody()));
			ASSERT_THAT(IsFalse(Run.Session->GetASTContext().IsSealed()));
			ASSERT_THAT(IsFalse(Run.Session->AreBodiesAnalyzed()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Function == Run.Function(TEXT("Value"))));
			ASSERT_THAT(IsTrue(DeclarationKey == Function->GetStableIdentity()));
			ASSERT_THAT(AreEqual(DeclarationProjection, Run.Session->SerializeStableProjection()));
			ASSERT_THAT(IsNotNull(Function->GetBody()));
			ASSERT_THAT(IsFalse(Run.Session->GetASTContext().IsSealed()));
			ASSERT_THAT(IsTrue(Run.Session->AreBodiesAnalyzed()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
			ASSERT_THAT(IsTrue(Run.Session->GetASTContext().IsSealed()));
		}

		TEST_METHOD(RejectedBodyInputMarksOwningFragment)
		{
			FSessionRun Run({{TEXT("RejectedBody.as"), TEXT("int Value() { return 42; }\n")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			auto* Function = Run.Function(TEXT("Value"));
			ASSERT_THAT(IsNotNull(Function));
			AS_NAMESPACE_QUALIFIER asSDeferredBodyWorkItem Work;
			Work.Function = Function;
			Work.StableFunctionKey = Function->GetStableIdentity();
			Work.BodyRange = Function->GetDeferredBodyRange();
			AS_NAMESPACE_QUALIFIER asCBodyFragment Fragment(MoveTemp(Work));
			AS_NAMESPACE_QUALIFIER asCParser Parser(Run.Session->GetSema());
			AS_NAMESPACE_QUALIFIER asSDirectiveProcessResult InvalidInput;
			ASSERT_THAT(IsFalse(Parser.ParseBody(InvalidInput, Fragment)));
			ASSERT_THAT(IsTrue(Fragment.HasErrors(), TEXT("A rejected parser input must invalidate its owning body result.")));
			ASSERT_THAT(IsFalse(Fragment.GetStableDiagnostics().IsEmpty()));
			ASSERT_THAT(IsNull(Fragment.GetBody()));
		}

		TEST_METHOD(BodyAnalysisRequiresResolvedDeclarations)
		{
			FSessionRun Run({{TEXT("Body.as"), TEXT("int F() { return 1; }\n")}});
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asECompilationPhase::Collecting, Run.Session->GetPhase()));
		}

		TEST_METHOD(LaterFileCallResolvesCanonicalDeclaration)
		{
			FSessionRun Run({
				{TEXT("A.as"), TEXT("int Caller() { return Callee(); }\n")},
				{TEXT("B.as"), TEXT("int Callee() { return 7; }\n")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("Caller"))->GetBody());
			auto* Return = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0]);
			auto* Call = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Return->GetValue());
			auto* Callee = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Call->GetCallee());
			ASSERT_THAT(IsTrue(Callee->GetDecl() == Run.Function(TEXT("Callee"))));
		}

		TEST_METHOD(MutualRecursionUsesTheFrozenOverloadEnvironment)
		{
			FSessionRun Run({{TEXT("Mutual.as"), TEXT("int A() { return B(); } int B() { return A(); }\n")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(4)));
			auto GetDirectCallee = [](AS_NAMESPACE_QUALIFIER asCFunctionDecl* Function)
			{
				auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Function->GetBody());
				auto* Return = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0]);
				auto* Call = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Return->GetValue());
				return AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Call->GetCallee())->GetDecl();
			};
			ASSERT_THAT(IsTrue(GetDirectCallee(Run.Function(TEXT("A"))) == Run.Function(TEXT("B"))));
			ASSERT_THAT(IsTrue(GetDirectCallee(Run.Function(TEXT("B"))) == Run.Function(TEXT("A"))));
		}

		TEST_METHOD(ConcreteNodesCarryTypeAndValueCategory)
		{
			FSessionRun Run({{TEXT("Literal.as"), TEXT("int Value() { return 42; }\n")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("Value"))->GetBody());
			auto* Return = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0]);
			auto* Literal = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Return->GetValue());
			ASSERT_THAT(IsTrue(Body && Return && Literal));
			ASSERT_THAT(AreEqual(42ll, Literal->GetValue()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue, Literal->GetValueCategory()));
			ASSERT_THAT(IsTrue(Literal->GetExpressionType() != nullptr));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeKind::Builtin, Literal->GetExpressionType()->GetKind()));
		}

		TEST_METHOD(OverloadConversionIsAnExplicitTypedNode)
		{
			// This scenario proves an int-to-Float32 conversion, independently of
			// the TypeContext's configured width for the authored float alias.
			FSessionRun Run({{TEXT("Convert.as"), TEXT("float32 Target(float32 X); float32 Caller() { return Target(1); }\n")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("Caller"))->GetBody());
			auto* Return = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0]);
			auto* Call = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Return->GetValue());
			auto* Conversion = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCImplicitCastExpr>(Call->GetArguments()[0]);
			ASSERT_THAT(IsTrue(Conversion != nullptr));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEImplicitConversionKind::Numeric, Conversion->GetConversionKind()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Conversion->GetExpressionType())->GetPrimitive()));
		}

		TEST_METHOD(ExplicitASTVerificationRejectsSubsequentMutation)
		{
			FSessionRun Run({{TEXT("Frozen.as"), TEXT("int F() { return 1; }\n")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			AS_NAMESPACE_QUALIFIER asCFunctionDecl* Function = Run.Function(TEXT("F"));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asECompilationPhase::BodiesAnalyzed, Run.Session->GetPhase()));
			ASSERT_THAT(IsFalse(Run.Session->GetASTContext().IsSealed()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asECompilationPhase::ASTVerified, Run.Session->GetPhase()));
			ASSERT_THAT(IsTrue(Run.Session->GetASTContext().IsSealed()));
			ASSERT_THAT(IsFalse(Function->SetStableIdentity(Function->GetStableIdentity())));
		}
	};


	TEST_CLASS_WITH_FLAGS(BodiesExpressions, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
	private:
		static AS_NAMESPACE_QUALIFIER asCCompoundStmt* FunctionBody(const FSessionRun& Run)
		{
			return AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
		}
		static AS_NAMESPACE_QUALIFIER asCExpr* ReturnValue(const FSessionRun& Run)
		{
			return AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(FunctionBody(Run)->GetStatements().Last())->GetValue();
		}

	public:
		TEST_METHOD(MultiplicationBindsInsideAdditionWithExactOperatorRange)
		{
			const FString Source = TEXT("int F() { return 1 + 2 * 3; }");
			FSessionRun Run({{TEXT("Precedence.as"), Source}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Add = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(ReturnValue(Run));
			ASSERT_THAT(IsNotNull(Add));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBinaryOperator::Add, Add->GetOperator()));
			auto* Multiply = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(Add->GetRight());
			ASSERT_THAT(IsNotNull(Multiply));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBinaryOperator::Multiply, Multiply->GetOperator()));
			ASSERT_THAT(AreEqual(2ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Multiply->GetLeft())->GetValue()));
			ASSERT_THAT(AreEqual(static_cast<asUINT>(Source.Find(TEXT("+"))), Add->GetOperatorRange().Begin.ByteOffset));
			ASSERT_THAT(AreEqual(asUINT(1), Add->GetOperatorRange().Length()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(ArithmeticAndPowerRemainLeftAssociative)
		{
			for (const TCHAR* Expression : {TEXT("8 - 3 - 1"), TEXT("2 ** 3 ** 2")})
			{
				FSessionRun Run({{TEXT("Associativity.as"), FString::Printf(TEXT("int F() { return %s; }"), Expression)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
				auto* Outer = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(ReturnValue(Run));
				ASSERT_THAT(IsNotNull(Outer));
				auto* Inner = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(Outer->GetLeft());
				ASSERT_THAT(IsNotNull(Inner, TEXT("The left tree catches a right-associative power regression.")));
				ASSERT_THAT(AreEqual(Outer->GetOperator(), Inner->GetOperator()));
				ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Outer->GetRight())));
			}
		}

		TEST_METHOD(AngelScriptBitwisePrecedenceIsNotCppPrecedence)
		{
			FSessionRun Run({{TEXT("ASPrecedence.as"), TEXT("bool F() { return 1 | 2 == 3; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Equal = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(ReturnValue(Run));
			ASSERT_THAT(IsNotNull(Equal));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBinaryOperator::Equal, Equal->GetOperator()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBinaryOperator::BitwiseOr, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(Equal->GetLeft())->GetOperator()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Bool, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Equal->GetExpressionType())->GetPrimitive()));
		}

		TEST_METHOD(AssignmentIsRightAssociativeAndRetainsLvalue)
		{
			FSessionRun Run({{TEXT("Assignment.as"), TEXT("int F() { int A; int B; A = B = 3; return A; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Assign = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(FunctionBody(Run)->GetStatements()[2]);
			auto* RightAssign = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(Assign->GetRight());
			ASSERT_THAT(IsNotNull(RightAssign));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBinaryOperator::Assign, RightAssign->GetOperator()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEValueCategory::LValue, Assign->GetValueCategory()));
			auto* B = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(FunctionBody(Run)->GetStatements()[1])->GetDeclaration();
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(RightAssign->GetLeft())->GetDecl() == B));
		}

		TEST_METHOD(ConditionalNestingIsRightAssociativeAndTyped)
		{
			FSessionRun Run({{TEXT("Conditional.as"), TEXT("int F() { return true ? 1 : false ? 2 : 3; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Outer = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCConditionalOperator>(ReturnValue(Run));
			ASSERT_THAT(IsNotNull(Outer));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBooleanLiteral>(Outer->GetCondition())->GetValue()));
			auto* Inner = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCConditionalOperator>(Outer->GetFalseExpr());
			ASSERT_THAT(IsNotNull(Inner));
			ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBooleanLiteral>(Inner->GetCondition())->GetValue()));
			ASSERT_THAT(AreEqual(3ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Inner->GetFalseExpr())->GetValue()));
		}

		TEST_METHOD(ConditionalBetweenHandlesKeepsHandleQualifiers)
		{
			FSessionRun Run({{TEXT("HandleConditional.as"), TEXT(
				"class Item {} Item@ F(bool Pick, Item@ A, Item@ B) { return Pick ? A : B; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Conditional = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCConditionalOperator>(ReturnValue(Run));
			ASSERT_THAT(IsNotNull(Conditional));
			ASSERT_THAT(IsTrue(Conditional->GetExpressionQualType().IsHandle()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST(), Run.Session->GetVerificationError()));
			FSessionRun ConstWiden({{TEXT("ConstHandleConditional.as"), TEXT(
				"class Item {} const Item@ F(bool Pick, Item@ A, const Item@ B) { return Pick ? A : B; }")}});
			ASSERT_THAT(IsTrue(ConstWiden.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(ConstWiden.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(ConstWiden.Session->VerifyAST(), ConstWiden.Session->GetVerificationError()));
			FSessionRun ConstRemoval({{TEXT("ConstHandleConditionalRemoval.as"), TEXT(
				"class Item {} Item@ F(bool Pick, const Item@ A, const Item@ B) { return Pick ? A : B; }")}});
			ASSERT_THAT(IsTrue(ConstRemoval.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(ConstRemoval.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(ParenthesesAndPrefixPostfixAreDistinctTypedNodes)
		{
			FSessionRun Run({{TEXT("Unary.as"), TEXT("int F() { int A = 1; ++A; A++; return -(A + 2); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Prefix = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCUnaryOperator>(FunctionBody(Run)->GetStatements()[1]);
			auto* Postfix = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCUnaryOperator>(FunctionBody(Run)->GetStatements()[2]);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEUnaryOperator::PreIncrement, Prefix->GetOperator()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEValueCategory::LValue, Prefix->GetValueCategory()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEUnaryOperator::PostIncrement, Postfix->GetOperator()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue, Postfix->GetValueCategory()));
			auto* Minus = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCUnaryOperator>(ReturnValue(Run));
			auto* Paren = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCParenExpr>(Minus->GetOperand());
			ASSERT_THAT(IsNotNull(Paren));
			ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(Paren->GetSubExpr())));
		}

		TEST_METHOD(StrictIntegerRadicesAndWidthsPreserveValues)
		{
			struct FCase { const TCHAR* Literal; const TCHAR* Type; AS_NAMESPACE_QUALIFIER asEPrimitiveType Primitive; uint64 Value; };
			const FCase Cases[] = {
				{TEXT("2147483647"), TEXT("int"), AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, 2147483647ull},
				{TEXT("2147483648"), TEXT("int64"), AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int64, 2147483648ull},
				{TEXT("18446744073709551615"), TEXT("uint64"), AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt64, MAX_uint64},
				{TEXT("0x2A"), TEXT("uint"), AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt32, 42},
				{TEXT("0b101010"), TEXT("uint"), AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt32, 42},
				{TEXT("0o52"), TEXT("uint"), AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt32, 42},
				{TEXT("0d42"), TEXT("uint"), AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt32, 42},
				{TEXT("0x100000000"), TEXT("uint64"), AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt64, 4294967296ull}
			};
			for (const FCase& Case : Cases)
			{
				FSessionRun Run({{TEXT("Integer.as"), FString::Printf(TEXT("%s F() { return %s; }"), Case.Type, Case.Literal)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
				auto* Literal = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(ReturnValue(Run));
				ASSERT_THAT(IsNotNull(Literal));
				ASSERT_THAT(AreEqual(Case.Value, Literal->GetUnsignedValue()));
				ASSERT_THAT(AreEqual(Case.Primitive, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Literal->GetExpressionType())->GetPrimitive()));
			}
		}

		TEST_METHOD(DecimalExponentAndFloatSuffixPreserveFloatingCategory)
		{
			struct FCase { const TCHAR* Literal; const TCHAR* Type; AS_NAMESPACE_QUALIFIER asEPrimitiveType Primitive; double Value; };
			const FCase Cases[] = {
				{TEXT(".5"), TEXT("float64"), AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float64, 0.5},
				{TEXT("1."), TEXT("float64"), AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float64, 1.0},
				{TEXT("2.5e-1"), TEXT("float64"), AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float64, 0.25},
				{TEXT("1e+2f"), TEXT("float32"), AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32, 100.0}
			};
			for (const FCase& Case : Cases)
			{
				FSessionRun Run({{TEXT("Floating.as"), FString::Printf(TEXT("%s F() { return %s; }"), Case.Type, Case.Literal)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
				auto* Literal = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFloatingLiteral>(ReturnValue(Run));
				ASSERT_THAT(IsNotNull(Literal));
				// These fixtures are exactly representable; verify decoding without a tolerance.
				ASSERT_THAT(IsTrue(Case.Value == Literal->GetValue()));
				ASSERT_THAT(AreEqual(Case.Primitive, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Literal->GetExpressionType())->GetPrimitive()));
			}
		}

		TEST_METHOD(MalformedAndOverflowingNumbersDiagnoseWithoutLosingLaterReturn)
		{
			for (const TCHAR* Literal : {TEXT("123abc"), TEXT("0b102"), TEXT("0x"), TEXT("1e+"), TEXT("1.2.3"),
				TEXT("18446744073709551616"), TEXT("1e999"), TEXT("1e99f"), TEXT("1_0"), TEXT("42u")})
			{
				FSessionRun Run({{TEXT("InvalidLiteral.as"), FString::Printf(TEXT("int F() { %s; return 7; }"), Literal)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("invalid-numeric-literal"))));
				ASSERT_THAT(AreEqual(7ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(ReturnValue(Run))->GetValue()));
				ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			}
		}

		TEST_METHOD(NumericCommonTypeCreatesExplicitConversions)
		{
			FSessionRun Run({{TEXT("Promotion.as"), TEXT("float64 F() { return 1 + 2.5; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Add = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(ReturnValue(Run));
			auto* Converted = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCImplicitCastExpr>(Add->GetLeft());
			ASSERT_THAT(IsNotNull(Converted));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEImplicitConversionKind::Numeric, Converted->GetConversionKind()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float64, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Add->GetExpressionType())->GetPrimitive()));
		}

		TEST_METHOD(ReadonlyAndRvalueMutationAreRejected)
		{
			for (const TCHAR* Statement : {TEXT("const int A = 1; A = 2;"), TEXT("const int A = 1; (A)++;"), TEXT("1 = 2;"), TEXT("++1;")})
			{
				FSessionRun Run({{TEXT("Readonly.as"), FString::Printf(TEXT("int F() { %s return 7; }"), Statement)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("modifiable-lvalue"))));
				ASSERT_THAT(AreEqual(7ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(ReturnValue(Run))->GetValue()));
			}
		}

		TEST_METHOD(BooleanAndIntegralOperatorsRejectUnrelatedTypes)
		{
			for (const TCHAR* Expression : {TEXT("1 && 2"), TEXT("~1.5"), TEXT("1.5 << 2"), TEXT("true + false"), TEXT("1 ? 2 : 3")})
			{
				FSessionRun Run({{TEXT("OperatorTypes.as"), FString::Printf(TEXT("void F() { %s; }"), Expression)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsTrue(Run.BodyFragment(TEXT("F"))->HasErrors()));
			}
		}

		TEST_METHOD(OutArgumentsRequireExactModifiableStorage)
		{
			FSessionRun Valid({{TEXT("Out.as"), TEXT("void Fill(int&out X); void F() { int A; Fill(A); }")}});
			ASSERT_THAT(IsTrue(Valid.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Valid.Session->AnalyzeBodies(1)));
			for (const TCHAR* Argument : {TEXT("1"), TEXT("1.5")})
			{
				FSessionRun Invalid({{TEXT("OutInvalid.as"), FString::Printf(TEXT("void Fill(int&out X); void F() { Fill(%s); }"), Argument)}});
				ASSERT_THAT(IsTrue(Invalid.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Invalid.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsTrue(Invalid.Session->SerializeStableBodyDiagnostics().Contains(TEXT("unresolved-call:Fill"))));
			}
		}

		TEST_METHOD(ReturnTypeContractRejectsMissingVoidAndBooleanMismatches)
		{
			for (const TCHAR* Source : {TEXT("int F() { return; }"), TEXT("void F() { return 1; }"), TEXT("int F() { return true; }")})
			{
				FSessionRun Run({{TEXT("ReturnType.as"), Source}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("incompatible-return-value"))));
			}
		}

		TEST_METHOD(ArgumentTrailingCommaAndExcessiveDepthRecover)
		{
			FSessionRun Trailing({{TEXT("Trailing.as"), TEXT("int G(int X); int F() { G(1,); return 7; }")}});
			ASSERT_THAT(IsTrue(Trailing.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Trailing.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Trailing.Session->SerializeStableBodyDiagnostics().Contains(TEXT("call-trailing-comma"))));
			FString Source = TEXT("int F() { ");
			for (int32 Index = 0; Index < 300; ++Index) Source += TEXT("!");
			Source += TEXT("true; return 7; }");
			FSessionRun Deep({{TEXT("Depth.as"), Source}});
			ASSERT_THAT(IsTrue(Deep.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Deep.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Deep.Session->SerializeStableBodyDiagnostics().Contains(TEXT("expression-depth-limit"))));
			ASSERT_THAT(AreEqual(7ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(ReturnValue(Deep))->GetValue()));
		}
	};

	TEST_CLASS_WITH_FLAGS(BodiesStructuredControlFlow, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(ForOwnsTypedInitConditionIncrementAndContinueTarget)
		{
			FSessionRun Run({{TEXT("For.as"), TEXT("int F() { int Sum = 0; for (int I = 0; I < 3; ++I) { Sum += I; continue; } return Sum; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(4)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* For = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCForStmt>(Body->GetStatements()[1]);
			ASSERT_THAT(IsNotNull(For));
			ASSERT_THAT(AreEqual(1, For->GetInitializers().Num()));
			auto* Init = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(For->GetInitializers()[0]);
			auto* Condition = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(For->GetCondition());
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBinaryOperator::Less, Condition->GetOperator()));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Condition->GetLeft())->GetDecl() == Init->GetDeclaration()));
			ASSERT_THAT(AreEqual(1, For->GetIncrements().Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEUnaryOperator::PreIncrement, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCUnaryOperator>(For->GetIncrements()[0])->GetOperator()));
			auto* LoopBody = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(For->GetBody());
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCContinueStmt>(LoopBody->GetStatements()[1])->GetTarget() == For));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(DoAndEmptyForHaveExplicitBodiesAndTypedBreakTargets)
		{
			FSessionRun Run({{TEXT("Do.as"), TEXT("void F() { do { break; } while(false); for (;;) ; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* Do = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDoStmt>(Body->GetStatements()[0]);
			ASSERT_THAT(IsNotNull(Do));
			ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBooleanLiteral>(Do->GetCondition())->GetValue()));
			auto* DoBody = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Do->GetBody());
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBreakStmt>(DoBody->GetStatements()[0])->GetTarget() == Do));
			auto* For = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCForStmt>(Body->GetStatements()[1]);
			ASSERT_THAT(AreEqual(0, For->GetInitializers().Num()));
			ASSERT_THAT(IsNull(For->GetCondition()));
			ASSERT_THAT(AreEqual(0, For->GetIncrements().Num()));
			ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCNullStmt>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(For->GetBody())->GetStatements()[0])));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(DanglingElseBindsNearestIf)
		{
			FSessionRun Run({{TEXT("If.as"), TEXT("int F(bool A, bool B) { if(A) if(B) return 1; else return 2; return 3; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* Outer = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIfStmt>(Body->GetStatements()[0]);
			ASSERT_THAT(IsNull(Outer->GetElse()));
			auto* ThenScope = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Outer->GetThen());
			auto* Inner = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIfStmt>(ThenScope->GetStatements()[0]);
			ASSERT_THAT(IsNotNull(Inner->GetElse()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(LoopAndBranchLocalsDoNotLeakToEnclosingScope)
		{
			for (const TCHAR* Statement : {TEXT("for (int I = 0; I < 1; ++I) ;"), TEXT("if(true) int I = 0;")})
			{
				FSessionRun Run({{TEXT("Scope.as"), FString::Printf(TEXT("int F() { %s return I; }"), Statement)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("unresolved-reference:I"))));
			}
		}

		TEST_METHOD(InnerLocalShadowsParameterAndDuplicateLocalIsRejected)
		{
			FSessionRun Run({{TEXT("Shadow.as"), TEXT("int F(int X) { { int X = 3; return X; } return X; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* Inner = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Body->GetStatements()[0]);
			auto* Local = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(Inner->GetStatements()[0])->GetDeclaration();
			auto* InnerRef = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Inner->GetStatements()[1])->GetValue());
			auto* OuterRef = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[1])->GetValue());
			ASSERT_THAT(IsTrue(InnerRef->GetDecl() == Local));
			ASSERT_THAT(IsTrue(OuterRef->GetDecl() == Run.Function(TEXT("F"))->GetParameters()[0]));
			FSessionRun Duplicate({{TEXT("Duplicate.as"), TEXT("void F() { int X; int X; }")}});
			ASSERT_THAT(IsTrue(Duplicate.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Duplicate.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Duplicate.Session->SerializeStableBodyDiagnostics().Contains(TEXT("duplicate-local:X"))));
		}

		TEST_METHOD(ContinuePreservesForInitializerButBreakDestroysIt)
		{
			FSessionRun Run({{TEXT("ForLifetime.as"), TEXT("class Owned {}; void F() { for(Owned Init; true;) { Owned Body; if(true) continue; break; } }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const auto Exits = Run.BodyFragment(TEXT("F"))->GetLifetimePlan().GetExitCleanups();
			ASSERT_THAT(AreEqual(2, Exits.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEStmtKind::Continue, Exits[0].Exit->GetKind()));
			ASSERT_THAT(AreEqual(1, Exits[0].Obligations.Num()));
			ASSERT_THAT(IsTrue(Exits[0].Obligations[0].StableLocalKey.Contains(TEXT(":Body"))));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEStmtKind::Break, Exits[1].Exit->GetKind()));
			ASSERT_THAT(AreEqual(2, Exits[1].Obligations.Num()));
			ASSERT_THAT(IsTrue(Exits[1].Obligations[0].StableLocalKey.Contains(TEXT(":Body"))));
			ASSERT_THAT(IsTrue(Exits[1].Obligations[1].StableLocalKey.Contains(TEXT(":Init"))));
		}

		TEST_METHOD(NumericConditionsAreRejectedRatherThanImplicitlyBoolified)
		{
			for (const TCHAR* Statement : {TEXT("if(1) ;"), TEXT("while(1) ;"), TEXT("for(;1;) ;"), TEXT("do ; while(1);")})
			{
				FSessionRun Run({{TEXT("Condition.as"), FString::Printf(TEXT("void F() { %s }"), Statement)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("condition-requires-bool"))));
			}
		}

		TEST_METHOD(LocalInitializerConvertsButIncompatibleInitializerFails)
		{
			FSessionRun Run({{TEXT("Init.as"), TEXT("void F() { float32 X = 3; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* Init = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(Body->GetStatements()[0])->GetInitializer();
			ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCImplicitCastExpr>(Init)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Init->GetExpressionType())->GetPrimitive()));
			FSessionRun Invalid({{TEXT("InitInvalid.as"), TEXT("void F() { bool X = 3; }")}});
			ASSERT_THAT(IsTrue(Invalid.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Invalid.Session->AnalyzeBodies(1)));
		}
	};



	TEST_CLASS_WITH_FLAGS(BodiesPostfix, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
	private:
		static AS_NAMESPACE_QUALIFIER asCCompoundStmt* Body(const FSessionRun& Run)
		{
			return AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
		}
		static AS_NAMESPACE_QUALIFIER asCExpr* Returned(const FSessionRun& Run)
		{
			return AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body(Run)->GetStatements().Last())->GetValue();
		}
	public:
		TEST_METHOD(MemberChainResolvesEachActualDeclaration)
		{
			FSessionRun Run({{TEXT("Members.as"), TEXT("class Leaf { int Value; } class Root { Leaf Child; } int F(Root Obj) { return Obj.Child.Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Value = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCMemberExpr>(Returned(Run));
			ASSERT_THAT(IsNotNull(Value));
			ASSERT_THAT(AreEqual(FString(TEXT("Value")), Value->GetMember()->GetName()));
			auto* Child = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCMemberExpr>(Value->GetBase());
			ASSERT_THAT(IsNotNull(Child));
			ASSERT_THAT(AreEqual(FString(TEXT("Child")), Child->GetMember()->GetName()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEValueCategory::LValue, Value->GetValueCategory()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(MemberCallUsesTypedReceiverAndSelectedOverload)
		{
			FSessionRun Run({{TEXT("Methods.as"), TEXT("class Item { int Read(int X); float64 Read(float64 X); } int F(Item Obj) { return Obj.Read(3); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Call = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Returned(Run));
			auto* Member = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCMemberExpr>(Call->GetCallee());
			ASSERT_THAT(IsNotNull(Member));
			auto* Method = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Member->GetMember());
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Method->GetResolvedReturnType())->GetPrimitive()));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Member->GetBase())->GetDecl() == Run.Function(TEXT("F"))->GetParameters()[0]));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(ConstReceiverRejectsFieldMutationAndMutableMethod)
		{
			for (const TCHAR* Expression : {TEXT("Obj.Value = 1"), TEXT("Obj.Mutate()")})
			{
				FSessionRun Run({{TEXT("ConstMember.as"), FString::Printf(TEXT("class Item { int Value; void Mutate(); } void F(const Item Obj) { %s; }"), Expression)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsTrue(Run.BodyFragment(TEXT("F"))->HasErrors()));
			}
		}

		TEST_METHOD(ConstMethodCanBeCalledOnReadonlyReceiver)
		{
			FSessionRun Run({{TEXT("ConstMethod.as"), TEXT("class Item { int Read() const; } int F(const Item Obj) { return Obj.Read(); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(MutableReceiverPrefersNonConstOverloadOfEqualRank)
		{
			FSessionRun Mutable({{TEXT("MutableRead.as"), TEXT(
				"class Item { int Read(); int Read() const; } int F(Item Obj) { return Obj.Read(); }")}});
			ASSERT_THAT(IsTrue(Mutable.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Mutable.Session->AnalyzeBodies(1)));
			auto* MutableCall = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Returned(Mutable));
			auto* MutableMember = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCMemberExpr>(MutableCall->GetCallee());
			ASSERT_THAT(IsNotNull(MutableMember));
			auto* MutableMethod = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(MutableMember->GetMember());
			ASSERT_THAT(IsFalse(MutableMethod->IsConstMethod()));
			ASSERT_THAT(IsTrue(Mutable.Session->VerifyAST(), Mutable.Session->GetVerificationError()));
			FSessionRun ConstReceiver({{TEXT("ConstRead.as"), TEXT(
				"class Item { int Read(); int Read() const; } int F(const Item Obj) { return Obj.Read(); }")}});
			ASSERT_THAT(IsTrue(ConstReceiver.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(ConstReceiver.Session->AnalyzeBodies(1)));
			auto* ConstCall = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Returned(ConstReceiver));
			auto* ConstMember = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCMemberExpr>(ConstCall->GetCallee());
			ASSERT_THAT(IsNotNull(ConstMember));
			auto* ConstMethod = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(ConstMember->GetMember());
			ASSERT_THAT(IsTrue(ConstMethod->IsConstMethod()));
			ASSERT_THAT(IsTrue(ConstReceiver.Session->VerifyAST(), ConstReceiver.Session->GetVerificationError()));
		}

		TEST_METHOD(MutableReceiverPrefersNonConstIndexOverloadOfEqualRank)
		{
			FSessionRun Mutable({{TEXT("MutableIndex.as"), TEXT(
				"class Item { int opIndex(int I); int opIndex(int I) const; } int F(Item Obj) { return Obj[0]; }")}});
			ASSERT_THAT(IsTrue(Mutable.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Mutable.Session->AnalyzeBodies(1)));
			auto* Index = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCArraySubscriptExpr>(Returned(Mutable));
			ASSERT_THAT(IsNotNull(Index));
			ASSERT_THAT(IsNotNull(Index->GetOperatorFunction()));
			ASSERT_THAT(IsFalse(Index->GetOperatorFunction()->IsConstMethod()));
			ASSERT_THAT(IsTrue(Mutable.Session->VerifyAST(), Mutable.Session->GetVerificationError()));
			FSessionRun ConstReceiver({{TEXT("ConstIndex.as"), TEXT(
				"class Item { int opIndex(int I); int opIndex(int I) const; } int F(const Item Obj) { return Obj[0]; }")}});
			ASSERT_THAT(IsTrue(ConstReceiver.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(ConstReceiver.Session->AnalyzeBodies(1)));
			auto* ConstIndex = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCArraySubscriptExpr>(Returned(ConstReceiver));
			ASSERT_THAT(IsNotNull(ConstIndex));
			ASSERT_THAT(IsTrue(ConstIndex->GetOperatorFunction()->IsConstMethod()));
			ASSERT_THAT(IsTrue(ConstReceiver.Session->VerifyAST(), ConstReceiver.Session->GetVerificationError()));
		}

		TEST_METHOD(UnknownMemberRecoversLaterReturn)
		{
			FSessionRun Run({{TEXT("MissingMember.as"), TEXT("class Item {} int F(Item Obj) { Obj.Missing; return 9; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("unresolved-member:Missing"))));
			ASSERT_THAT(AreEqual(9ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Returned(Run))->GetValue()));
		}

		TEST_METHOD(IndexOperatorRetainsSelectedMethodAndReferenceCategory)
		{
			FSessionRun Run({{TEXT("Index.as"), TEXT("class Buffer { int& opIndex(int Index); } int F(Buffer Values) { return Values[2]; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Index = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCArraySubscriptExpr>(Returned(Run));
			ASSERT_THAT(IsNotNull(Index));
			ASSERT_THAT(IsNotNull(Index->GetOperatorFunction()));
			ASSERT_THAT(AreEqual(FString(TEXT("opIndex")), Index->GetOperatorFunction()->GetName()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEValueCategory::LValue, Index->GetValueCategory()));
			ASSERT_THAT(IsTrue(Index->GetExpressionQualType().IsReference()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(InvalidIndexReceiverAndArgumentAreDiagnosed)
		{
			for (const TCHAR* Expression : {TEXT("3[0]"), TEXT("Values[true]")})
			{
				FSessionRun Run({{TEXT("IndexInvalid.as"), FString::Printf(TEXT("class Buffer { int opIndex(int Index); } void F(Buffer Values) { %s; }"), Expression)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			}
		}

		TEST_METHOD(ExplicitCastRetainsAuthoredTargetAndOperand)
		{
			FSessionRun Run({{TEXT("Cast.as"), TEXT("float64 F() { return Cast<float64>(1); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Cast = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCExplicitCastExpr>(Returned(Run));
			ASSERT_THAT(IsNotNull(Cast));
			ASSERT_THAT(AreEqual(FString(TEXT("float64")), Cast->GetTypeSyntax().GetQualifiedName()));
			ASSERT_THAT(AreEqual(1ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Cast->GetOperand())->GetValue()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float64, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Cast->GetExpressionType())->GetPrimitive()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(PrimitiveConstructionIsNotAnUnresolvedFunctionCall)
		{
			FSessionRun Run({{TEXT("PrimitiveConstruct.as"), TEXT("float32 F() { return float32(2); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Construct = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCConstructExpr>(Returned(Run));
			ASSERT_THAT(IsNotNull(Construct));
			ASSERT_THAT(IsNull(Construct->GetConstructor()));
			ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCImplicitCastExpr>(Construct->GetArguments()[0])));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(PrimitiveConversionRequiresOnePositionalArgumentAndRecovers)
		{
			for (const TCHAR* Expression : {TEXT("int()"), TEXT("float64()"), TEXT("bool()"), TEXT("int(1, 2)"), TEXT("int(Value: 1)")})
			{
				FSessionRun Run({{TEXT("PrimitiveArguments.as"), FString::Printf(TEXT("int F() { %s; return 9; }"), Expression)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1), Expression));
				ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("invalid-primitive-construction"))));
				ASSERT_THAT(AreEqual(9ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Returned(Run))->GetValue()));
				ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			}
		}

		TEST_METHOD(RecordDefaultConstructionStillSelectsItsActualConstructor)
		{
			FSessionRun Run({{TEXT("RecordDefaultConstruct.as"), TEXT("class Value { Value(); } Value F() { return Value(); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Construct = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCConstructExpr>(Returned(Run));
			ASSERT_THAT(IsNotNull(Construct));
			ASSERT_THAT(AreEqual(0, Construct->GetArguments().Num()));
			auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->Lookup(TEXT("Value"))[0]);
			ASSERT_THAT(IsTrue(Construct->GetConstructor() == AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Record->Lookup(TEXT("Value"))[0])));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(DirectLocalInitializationResolvesRealConstructor)
		{
			FSessionRun Run({{TEXT("Construct.as"), TEXT("class Value { Value(int X); int Read(); } int F() { Value Local(7); return Local.Read(); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Declaration = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(Body(Run)->GetStatements()[0]);
			auto* Construct = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCConstructExpr>(Declaration->GetInitializer());
			ASSERT_THAT(IsNotNull(Construct));
			ASSERT_THAT(IsNotNull(Construct->GetConstructor()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFunctionKind::Constructor, Construct->GetConstructor()->GetFunctionKind()));
			ASSERT_THAT(AreEqual(7ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Construct->GetArguments()[0])->GetValue()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(NoMatchingConstructorDoesNotProduceSuccessfulBody)
		{
			FSessionRun Run({{TEXT("ConstructInvalid.as"), TEXT("class Value { Value(int X); } void F() { Value Local(true); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("unresolved-constructor"))));
		}

		TEST_METHOD(DefaultArgumentReferencesOneAuthoredInitializerTree)
		{
			FSessionRun Run({{TEXT("Defaults.as"), TEXT("int Add(int Value = 6 * 7); int F() { return Add(); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Parameter = Run.Function(TEXT("Add"))->GetParameters()[0];
			ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(Parameter->GetInitializer())));
			auto* Call = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Returned(Run));
			auto* Default = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDefaultArgumentExpr>(Call->GetArguments()[0]);
			ASSERT_THAT(IsNotNull(Default));
			ASSERT_THAT(IsTrue(Default->GetParameter() == Parameter));
			ASSERT_THAT(AreEqual(INDEX_NONE, Call->GetArgumentSourceOrdinals()[0]));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(NamedArgumentsReorderByParameterWithoutLosingAuthoredOrder)
		{
			FSessionRun Run({{TEXT("Named.as"), TEXT("int Add(int A, int B); int F() { return Add(B: 4, A: 3); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Call = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Returned(Run));
			ASSERT_THAT(AreEqual(3ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Call->GetArguments()[0])->GetValue()));
			ASSERT_THAT(AreEqual(4ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Call->GetArguments()[1])->GetValue()));
			ASSERT_THAT(AreEqual(1, Call->GetArgumentSourceOrdinals()[0]));
			ASSERT_THAT(AreEqual(0, Call->GetArgumentSourceOrdinals()[1]));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(DuplicateUnknownAndMisorderedNamedArgumentsReject)
		{
			for (const TCHAR* Arguments : {TEXT("A: 1, A: 2"), TEXT("Unknown: 1, B: 2"), TEXT("B: 1, 2")})
			{
				FSessionRun Run({{TEXT("NamedInvalid.as"), FString::Printf(TEXT("int Add(int A, int B); int F() { return Add(%s); }"), Arguments)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			}
		}

		TEST_METHOD(DelegateVariableCallUsesCanonicalSignatureWithoutEngine)
		{
			FSessionRun Run({{TEXT("DelegateCall.as"), TEXT("delegate int Callback(int X); int F(Callback Handler) { return Handler(2); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Call = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Returned(Run));
			auto* Callee = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Call->GetCallee());
			ASSERT_THAT(IsTrue(Callee->GetDecl() == Run.Function(TEXT("F"))->GetParameters()[0]));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Call->GetExpressionType())->GetPrimitive()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(NullReturnAcquiresActualHandleTypeThroughConversion)
		{
			FSessionRun Run({{TEXT("Null.as"), TEXT("class Object {} Object@ F() { return nullptr; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Conversion = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCImplicitCastExpr>(Returned(Run));
			ASSERT_THAT(IsNotNull(Conversion));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEImplicitConversionKind::NullToHandle, Conversion->GetConversionKind()));
			ASSERT_THAT(IsTrue(Conversion->GetExpressionQualType().IsHandle()));
			ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCNullPtrLiteral>(Conversion->GetOperand())));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(NullCannotMasqueradeAsPrimitiveOrUntypedSuccess)
		{
			for (const TCHAR* Source : {TEXT("int F() { return nullptr; }"), TEXT("void F() { nullptr; }")})
			{
				FSessionRun Run({{TEXT("NullInvalid.as"), Source}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			}
		}

		TEST_METHOD(StringEscapesUseDeclaredProviderAndDecodedValue)
		{
			FSessionRun Run({{TEXT("String.as"), TEXT("class FString {} FString F() { return \"line\\n\\u0041\"; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Literal = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCStringLiteral>(Returned(Run));
			ASSERT_THAT(IsNotNull(Literal));
			ASSERT_THAT(AreEqual(FString(TEXT("line\nA")), Literal->GetValue()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeKind::Nominal, Literal->GetExpressionType()->GetKind()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(MalformedEscapeAndMissingStringProviderAreErrors)
		{
			for (const TCHAR* Source : {TEXT("class FString {} FString F() { return \"\\q\"; }"), TEXT("void F() { \"text\"; }")})
			{
				FSessionRun Run({{TEXT("StringInvalid.as"), Source}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			}
		}
	};


	TEST_CLASS_WITH_FLAGS(BodiesFinalization, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(NestedExitsCarryTypedTargetsAndReverseCleanup)
		{
			FSessionRun Run({{TEXT("Lifetime.as"), TEXT("class Owned {}; int F() { Owned A; Owned B; while(true) { Owned C; break; } return 0; }\n")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(4)));
			const AS_NAMESPACE_QUALIFIER asCBodyFragment* Fragment = Run.BodyFragment(TEXT("F"));
			ASSERT_THAT(IsTrue(Fragment != nullptr));
			const TConstArrayView<AS_NAMESPACE_QUALIFIER asSBodyExitCleanup> Exits = Fragment->GetLifetimePlan().GetExitCleanups();
			ASSERT_THAT(AreEqual(2, Exits.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEStmtKind::Break, Exits[0].Exit->GetKind()));
			ASSERT_THAT(AreEqual(1, Exits[0].Obligations.Num()));
			ASSERT_THAT(IsTrue(Exits[0].Obligations[0].StableLocalKey.Contains(TEXT(":C"))));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEStmtKind::Return, Exits[1].Exit->GetKind()));
			ASSERT_THAT(AreEqual(2, Exits[1].Obligations.Num()));
			ASSERT_THAT(IsTrue(Exits[1].Obligations[0].StableLocalKey.Contains(TEXT(":B"))));
			ASSERT_THAT(IsTrue(Exits[1].Obligations[1].StableLocalKey.Contains(TEXT(":A"))));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* While = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCWhileStmt>(Body->GetStatements()[2]);
			auto* WhileBody = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(While->GetBody());
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBreakStmt>(WhileBody->GetStatements()[1])->GetTarget() == While));
		}

		TEST_METHOD(InvalidConstructionDoesNotInventCleanup)
		{
			FSessionRun Run({{TEXT("Invalid.as"), TEXT("int F() { Missing A; return 0; }\n")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			const AS_NAMESPACE_QUALIFIER asCBodyFragment* Fragment = Run.BodyFragment(TEXT("F"));
			ASSERT_THAT(IsTrue(Fragment != nullptr));
			ASSERT_THAT(IsTrue(Fragment->HasErrors()));
			ASSERT_THAT(AreEqual(0, Fragment->GetLifetimePlan().GetConstructedLocals().Num()));
			ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("unresolved-local-type:Missing"))));
		}

		TEST_METHOD(RecoveryExpressionKeepsTheLaterStatement)
		{
			FSessionRun Run({{TEXT("Recover.as"), TEXT("int F() { return Missing(; return 2; }\n")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			ASSERT_THAT(AreEqual(2, Body->GetStatements().Num()));
			auto* First = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0]);
			auto* Recovery = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCRecoveryExpr>(First->GetValue());
			ASSERT_THAT(IsTrue(Recovery != nullptr));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeKind::Error, Recovery->GetExpressionType()->GetKind()));
			auto* Second = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[1]);
			ASSERT_THAT(AreEqual(2ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Second->GetValue())->GetValue()));
		}

		TEST_METHOD(MalformedBodyDoesNotEraseALaterFunction)
		{
			FSessionRun Run({{TEXT("Functions.as"), TEXT("int Bad() { ???; } int Good() { return 9; }\n")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(4)));
			ASSERT_THAT(IsTrue(Run.BodyFragment(TEXT("Bad"))->HasErrors()));
			ASSERT_THAT(IsFalse(Run.BodyFragment(TEXT("Good"))->HasErrors()));
			auto* GoodBody = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("Good"))->GetBody());
			auto* Return = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(GoodBody->GetStatements()[0]);
			ASSERT_THAT(AreEqual(9ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Return->GetValue())->GetValue()));
		}

		TEST_METHOD(ReversedInputsProduceEqualBodyResults)
		{
			const TPair<FString, FString> A(TEXT("A.as"), TEXT("int Caller() { return Callee(); }\n"));
			const TPair<FString, FString> B(TEXT("B.as"), TEXT("int Callee() { return 3; }\n"));
			FSessionRun First({A, B});
			FSessionRun Second({B, A});
			First.ResolveDeclarations(); Second.ResolveDeclarations();
			First.Session->AnalyzeBodies(1); Second.Session->AnalyzeBodies(1);
			ASSERT_THAT(AreEqual(First.Session->SerializeStableBodyProjection(), Second.Session->SerializeStableBodyProjection()));
			ASSERT_THAT(AreEqual(First.Session->SerializeStableBodyDiagnostics(), Second.Session->SerializeStableBodyDiagnostics()));
		}

		TEST_METHOD(WorkerCountDoesNotChangeBodyResults)
		{
			const TArray<TPair<FString, FString>> Sources = {
				{TEXT("A.as"), TEXT("int A() { return B(); }\n")},
				{TEXT("B.as"), TEXT("int B() { return 4; }\n")}};
			FSessionRun One(Sources);
			FSessionRun Four(Sources);
			One.ResolveDeclarations(); Four.ResolveDeclarations();
			One.Session->AnalyzeBodies(1); Four.Session->AnalyzeBodies(4);
			ASSERT_THAT(AreEqual(One.Session->SerializeStableBodyProjection(), Four.Session->SerializeStableBodyProjection()));
			ASSERT_THAT(AreEqual(One.Session->SerializeStableBodyDiagnostics(), Four.Session->SerializeStableBodyDiagnostics()));
		}

		TEST_METHOD(DeclarationErrorsSkipInvalidBodiesButAnalyzeValidOnes)
		{
			FSessionRun Run({{TEXT("Mixed.as"), TEXT("Missing Bad() { return 1; } int Good() { return 5; }\n")}});
			Run.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Run.Function(TEXT("Bad")) == nullptr));
			ASSERT_THAT(IsTrue(Run.Function(TEXT("Good"))->GetBody() != nullptr));
			ASSERT_THAT(IsFalse(Run.Session->AreBodiesAnalyzed()));
		}
	};
}

#endif
