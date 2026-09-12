#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "frontend/AST/as_ast_cast.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace BodyControlFlowTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	static AS_NAMESPACE_QUALIFIER asCCompoundStmt* BodyOf(const FSessionRun& Run, const TCHAR* Name)
	{
		AS_NAMESPACE_QUALIFIER asCFunctionDecl* Function = Run.Function(Name);
		return Function ? AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Function->GetBody()) : nullptr;
	}

	static AS_NAMESPACE_QUALIFIER asCDeclStmt* DeclarationAt(AS_NAMESPACE_QUALIFIER asCCompoundStmt& Body, int32 Index)
	{
		return Body.GetStatements().IsValidIndex(Index)
			? AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(Body.GetStatements()[Index]) : nullptr;
	}

	TEST_CLASS_WITH_FLAGS(
		BodiesControlFlow,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(LocalAutoUsesTheInitializerCanonicalQualType)
		{
			FSessionRun Run({{TEXT("AutoLocals.as"),
				TEXT("class Item {} int F(Item@ Input) { auto A = 1; const auto B = A; auto@ Copy = Input; return B; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			AS_NAMESPACE_QUALIFIER asCCompoundStmt* Body = BodyOf(Run, TEXT("F"));
			ASSERT_THAT(IsNotNull(Body));
			auto* A = DeclarationAt(*Body, 0);
			auto* B = DeclarationAt(*Body, 1);
			auto* Copy = DeclarationAt(*Body, 2);
			ASSERT_THAT(IsNotNull(A));
			ASSERT_THAT(IsNotNull(B));
			ASSERT_THAT(IsNotNull(Copy));
			ASSERT_THAT(IsTrue(A->GetDeclaration()->GetResolvedQualType().IsValid()));
			ASSERT_THAT(IsFalse(A->GetDeclaration()->GetResolvedQualType().IsConst()));
			ASSERT_THAT(IsTrue(B->GetDeclaration()->GetResolvedQualType().IsConst()));
			ASSERT_THAT(IsTrue(Copy->GetDeclaration()->GetResolvedQualType().IsHandle()));
			ASSERT_THAT(IsFalse(Copy->GetDeclaration()->GetResolvedQualType().IsReference()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(LocalAutoRequiresAConcreteTypedInitializer)
		{
			for (const TCHAR* Body :
				{TEXT("auto Missing;"), TEXT("auto Value = null;"), TEXT("auto Values = {1, 2};")})
			{
				FSessionRun Run({{TEXT("InvalidAuto.as"),
					FString::Printf(TEXT("int F() { %s return 0; }"), Body)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsNotNull(Run.BodyFragment(TEXT("F"))));
				ASSERT_THAT(IsTrue(Run.BodyFragment(TEXT("F"))->HasErrors()));
			}
		}

		TEST_METHOD(ForRetainsMultipleInitializersAndIncrementsInAuthoredOrder)
		{
			FSessionRun Run({{TEXT("ForPhases.as"),
				TEXT("int F() { for (int First = 0, Second = 1; First < 3; First++, Second += First) { if (Second > 4) continue; } return 0; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			AS_NAMESPACE_QUALIFIER asCCompoundStmt* Body = BodyOf(Run, TEXT("F"));
			ASSERT_THAT(IsNotNull(Body));
			auto* For = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCForStmt>(Body->GetStatements()[0]);
			ASSERT_THAT(IsNotNull(For));
			ASSERT_THAT(AreEqual(2, For->GetInitializers().Num()));
			ASSERT_THAT(AreEqual(2, For->GetIncrements().Num()));
			auto* First = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(For->GetInitializers()[0])->GetDeclaration();
			auto* Second = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(For->GetInitializers()[1])->GetDeclaration();
			auto* FirstIncrement = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCUnaryOperator>(For->GetIncrements()[0]);
			auto* SecondIncrement = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(For->GetIncrements()[1]);
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(FirstIncrement->GetOperand())->GetDecl() == First));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(SecondIncrement->GetLeft())->GetDecl() == Second));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(SecondIncrement->GetRight())->GetDecl() == First));
			auto* ControlledBody = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(For->GetBody());
			auto* If = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIfStmt>(ControlledBody->GetStatements()[0]);
			auto* ContinueScope = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(If->GetThen());
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCContinueStmt>(ContinueScope->GetStatements()[0])->GetTarget() == For));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(SwitchRetainsNormalizedCasesAndExactControlTargets)
		{
			FSessionRun Run({{TEXT("Switch.as"),
				TEXT("int F(int Value) { switch (Value) { case 1: Value += 1; fallthrough; case 2: break; default: return Value; } return Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Switch = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCSwitchStmt>(BodyOf(Run, TEXT("F"))->GetStatements()[0]);
			ASSERT_THAT(IsNotNull(Switch));
			ASSERT_THAT(AreEqual(3, Switch->GetClauses().Num()));
			auto* First = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCaseStmt>(Switch->GetClauses()[0]);
			auto* Second = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCaseStmt>(Switch->GetClauses()[1]);
			ASSERT_THAT(IsTrue(First->HasResolvedValue()));
			ASSERT_THAT(AreEqual(static_cast<uint64>(1), First->GetResolvedValueBits()));
			ASSERT_THAT(AreEqual(static_cast<uint64>(2), Second->GetResolvedValueBits()));
			auto* Fallthrough = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFallthroughStmt>(First->GetStatements().Last());
			ASSERT_THAT(IsTrue(Fallthrough->GetTarget() == Second));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBreakStmt>(Second->GetStatements().Last())->GetTarget() == Switch));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(DefaultOnlyAndNonFinalDefaultRemainAuthoredClauses)
		{
			FSessionRun Run({{TEXT("DefaultOrder.as"),
				TEXT("int Only(int Value) { switch (Value) { default: break; } return Value; } "
					"int Ordered(int Value) { switch (Value) { default: fallthrough; case 7: break; } return Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Only = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCSwitchStmt>(BodyOf(Run, TEXT("Only"))->GetStatements()[0]);
			auto* Ordered = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCSwitchStmt>(BodyOf(Run, TEXT("Ordered"))->GetStatements()[0]);
			ASSERT_THAT(AreEqual(1, Only->GetClauses().Num()));
			ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDefaultStmt>(Only->GetClauses()[0])));
			ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDefaultStmt>(Ordered->GetClauses()[0])));
			auto* Fallthrough = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFallthroughStmt>(Ordered->GetClauses()[0]->GetStatements()[0]);
			ASSERT_THAT(IsTrue(Fallthrough->GetTarget() == Ordered->GetClauses()[1]));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(SwitchRejectsDuplicateLabelsInvalidFallthroughAndUnscopedDeclarations)
		{
			for (const TCHAR* Clauses :
				{TEXT("case 1: break; case 1: break;"),
				 TEXT("case 1: fallthrough;"),
				 TEXT("case 1: int Local = 0; break;"),
				 TEXT("default: break; default: break;")})
			{
				FSessionRun Run({{TEXT("InvalidSwitch.as"),
					FString::Printf(TEXT("int F(int Value) { switch (Value) { %s } return Value; }"), Clauses)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsTrue(Run.BodyFragment(TEXT("F"))->HasErrors()));
			}
		}

		TEST_METHOD(ContinueSkipsSwitchAndTargetsTheNearestLoop)
		{
			FSessionRun Run({{TEXT("NestedControl.as"),
				TEXT("int F(int Value) { while (true) { switch (Value) { case 0: continue; default: break; } break; } return Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* While = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCWhileStmt>(BodyOf(Run, TEXT("F"))->GetStatements()[0]);
			auto* WhileBody = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(While->GetBody());
			auto* Switch = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCSwitchStmt>(WhileBody->GetStatements()[0]);
			auto* Continue = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCContinueStmt>(Switch->GetClauses()[0]->GetStatements()[0]);
			auto* SwitchBreak = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBreakStmt>(Switch->GetClauses()[1]->GetStatements()[0]);
			auto* LoopBreak = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBreakStmt>(WhileBody->GetStatements()[1]);
			ASSERT_THAT(IsTrue(Continue->GetTarget() == While));
			ASSERT_THAT(IsTrue(SwitchBreak->GetTarget() == Switch));
			ASSERT_THAT(IsTrue(LoopBreak->GetTarget() == While));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(ForeachOwnsVariablesAndAuthenticatesTheSelectedProtocol)
		{
			FSessionRun Run({{TEXT("Foreach.as"), TEXT(
				"class Range { int opForBegin() const; bool opForEnd(int Iterator) const; "
				"void opForNext(int Iterator) const; int opForValue(int Iterator) const; int opForKey(int Iterator) const; } "
				"int F(Range Values) { int Sum = 0; foreach (auto Value, auto Key : Values) { Sum += Value + Key; } return Sum; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Foreach = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCForeachStmt>(BodyOf(Run, TEXT("F"))->GetStatements()[1]);
			ASSERT_THAT(IsNotNull(Foreach->GetValueStatement()));
			ASSERT_THAT(IsNotNull(Foreach->GetKeyStatement()));
			ASSERT_THAT(IsTrue(Foreach->GetValueStatement()->GetDeclaration()->GetResolvedQualType().IsValid()));
			ASSERT_THAT(IsTrue(Foreach->GetKeyStatement()->GetDeclaration()->GetResolvedQualType().IsValid()));
			const AS_NAMESPACE_QUALIFIER asSForeachProtocol& Protocol = Foreach->GetProtocol();
			ASSERT_THAT(AreEqual(FString(TEXT("opForBegin")), Protocol.BeginFunction->GetName()));
			ASSERT_THAT(AreEqual(FString(TEXT("opForEnd")), Protocol.EndFunction->GetName()));
			ASSERT_THAT(AreEqual(FString(TEXT("opForNext")), Protocol.NextFunction->GetName()));
			ASSERT_THAT(AreEqual(FString(TEXT("opForValue")), Protocol.ValueFunction->GetName()));
			ASSERT_THAT(AreEqual(FString(TEXT("opForKey")), Protocol.KeyFunction->GetName()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(ForeachHandleConstAdditionAnalyzesAndVerifies)
		{
			FSessionRun Run({{TEXT("ForeachConstHandle.as"), TEXT(
				"class Item {} class Range { int opForBegin() const; bool opForEnd(int I) const; "
				"void opForNext(int I) const; Item@ opForValue(int I) const; Item@ opForKey(int I) const; } "
				"void F(Range Items) { foreach (const Item@ Value, const Item@ Key : Items) {} }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST(), Run.Session->GetVerificationError()));
		}

		TEST_METHOD(ForeachHandleConstRemovalFailsDuringAnalysis)
		{
			FSessionRun Run({{TEXT("ForeachConstRemoval.as"), TEXT(
				"class Item {} class Range { int opForBegin() const; bool opForEnd(int I) const; "
				"void opForNext(int I) const; const Item@ opForValue(int I) const; const Item@ opForKey(int I) const; } "
				"void F(Range Items) { foreach (Item@ Value, Item@ Key : Items) {} }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Run.BodyFragment(TEXT("F"))->HasErrors()));
		}

		TEST_METHOD(ForeachRejectsIncompleteOrMismatchedProtocols)
		{
			for (const TCHAR* Members :
				{TEXT("int opForBegin() const; bool opForEnd(int I) const; void opForNext(int I) const; int opForValue(int I) const;"),
				 TEXT("int opForBegin() const; int opForEnd(int I) const; void opForNext(int I) const; int opForValue(int I) const; int opForKey(int I) const;")})
			{
				FSessionRun Run({{TEXT("InvalidForeach.as"), FString::Printf(TEXT(
					"class Range { %s } int F(Range Values) { foreach (auto Value, auto Key : Values) { break; } return 0; }"), Members)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsTrue(Run.BodyFragment(TEXT("F"))->HasErrors()));
			}
		}

		TEST_METHOD(ForeachIterationLocalsAreDestroyedOnContinueAndBreak)
		{
			FSessionRun Run({{TEXT("ForeachLifetime.as"), TEXT(
				"class Item {} class Range { int opForBegin() const; bool opForEnd(int Iterator) const; "
				"void opForNext(int Iterator) const; Item opForValue(int Iterator) const; } "
				"int F(Range Values) { foreach (auto Value : Values) { if (true) continue; break; } return 0; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const AS_NAMESPACE_QUALIFIER asCBodyFragment* Fragment = Run.BodyFragment(TEXT("F"));
			ASSERT_THAT(IsNotNull(Fragment));
			const auto Constructed = Fragment->GetLifetimePlan().GetConstructedLocals();
			ASSERT_THAT(AreEqual(1, Constructed.Num()));
			const auto Exits = Fragment->GetLifetimePlan().GetExitCleanups();
			ASSERT_THAT(IsTrue(Exits.Num() >= 3));
			ASSERT_THAT(AreEqual(1, Exits[0].Obligations.Num()));
			ASSERT_THAT(AreEqual(Constructed[0].StableLocalKey, Exits[0].Obligations[0].StableLocalKey));
			ASSERT_THAT(AreEqual(1, Exits[1].Obligations.Num()));
			ASSERT_THAT(AreEqual(Constructed[0].StableLocalKey, Exits[1].Obligations[0].StableLocalKey));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
