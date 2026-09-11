#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/as_ast_context.h"
#include "source/as_ast_traversal.h"
#include "as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
const TCHAR* NodeClassName(asEASTNodeClass NodeClass)
{
	switch (NodeClass)
	{
	case asAST_NODE_DECL: return TEXT("D");
	case asAST_NODE_TYPE: return TEXT("T");
	case asAST_NODE_STMT: return TEXT("S");
	case asAST_NODE_EXPR: return TEXT("E");
	default: return TEXT("?");
	}
}

class FRecordingASTVisitor final : public asIASTConstVisitor
{
public:
	TArray<FString> PreOrder;
	TArray<FString> PostOrder;
	TArray<asEASTEdgeRole> EdgeRoles;

	bool EnterNode(const asCASTContext&, const asSASTNodeRef& Node, const asSASTTraversalEdge*, asUINT) override
	{
		PreOrder.Add(FString::Printf(TEXT("%s%u"), NodeClassName(Node.nodeClass), Node.value));
		return true;
	}

	bool VisitEdge(const asCASTContext&, const asSASTTraversalEdge& Edge, asUINT) override
	{
		EdgeRoles.Add(Edge.role);
		return true;
	}

	bool LeaveNode(const asCASTContext&, const asSASTNodeRef& Node, const asSASTTraversalEdge*, asUINT) override
	{
		PostOrder.Add(FString::Printf(TEXT("%s%u"), NodeClassName(Node.nodeClass), Node.value));
		return true;
	}
};

class FSkipReceiverASTVisitor final : public asIASTConstVisitor
{
public:
	TArray<FString> PreOrder;
	TArray<asEASTEdgeRole> EdgeRoles;

	bool EnterNode(const asCASTContext&, const asSASTNodeRef& Node, const asSASTTraversalEdge*, asUINT) override
	{
		PreOrder.Add(FString::Printf(TEXT("%s%u"), NodeClassName(Node.nodeClass), Node.value));
		return true;
	}

	bool VisitEdge(const asCASTContext&, const asSASTTraversalEdge& Edge, asUINT) override
	{
		EdgeRoles.Add(Edge.role);
		return true;
	}

	bool ShouldTraverseEdge(const asCASTContext&, const asSASTTraversalEdge& Edge, asUINT) override
	{
		return Edge.role != asAST_EDGE_EXPR_RECEIVER;
	}

	bool LeaveNode(const asCASTContext&, const asSASTNodeRef&, const asSASTTraversalEdge*, asUINT) override
	{
		return true;
	}
};

void BuildSmallSealedGraph(
	asCASTContext& Context,
	asASTDeclId& OutTu,
	asASTDeclId& OutFunction,
	asASTStmtId& OutBody,
	asASTStmtId& OutReturn,
	asASTExprId& OutLiteral,
	asASTTypeRef& OutIntType)
{
	OutTu = Context.CreateTranslationUnit("Traversal");
	const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
	OutIntType = IntType.type;
	OutFunction = Context.CreateDecl(asAST_DECL_FUNCTION, OutTu, asCSourceRange(), "F");
	Context.SetDeclType(OutFunction, IntType);
	OutBody = Context.CreateStmt(asAST_STMT_BLOCK, OutFunction, asCSourceRange());
	Context.SetBody(OutFunction, OutBody);
	OutReturn = Context.CreateStmt(asAST_STMT_RETURN, OutFunction, asCSourceRange());
	Context.AddStmtChild(OutBody, OutReturn);
	OutLiteral = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
	Context.SetLiteral(OutLiteral, "7");
	Context.SetLiteralBits(OutLiteral, 7);
	Context.SetStmtExpr(OutReturn, OutLiteral);
	Context.Seal();
}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTTraversalTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.Traversal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(VisitsEveryNodeClassWithDeterministicPrePostOrderAndNamedEdges)
	{
		asCASTContext Context;
		asASTDeclId Tu;
		asASTDeclId Function;
		asASTStmtId Body;
		asASTStmtId Return;
		asASTExprId Literal;
		asASTTypeRef IntType;
		BuildSmallSealedGraph(Context, Tu, Function, Body, Return, Literal, IntType);
		ASSERT_THAT(IsTrue(Context.IsPublishable(), TEXT("fixture must publish a Frozen/Publishable context")));

		FRecordingASTVisitor Visitor;
		asSASTTraversalResult Result;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_OK,
			asCASTTraverse(Context, asSASTNodeRef::FromDecl(Tu), Visitor, asSASTTraversalOptions(), Result),
			TEXT("translation-unit traversal should complete")));

		const TArray<FString> ExpectedPre = {TEXT("D1"), TEXT("D2"), TEXT("S1"), TEXT("S2"), TEXT("E1")};
		const TArray<FString> ExpectedPost = {TEXT("E1"), TEXT("S2"), TEXT("S1"), TEXT("D2"), TEXT("D1")};
		ASSERT_THAT(AreEqual(ExpectedPre, Visitor.PreOrder, TEXT("pre-order must be deterministic across node classes")));
		ASSERT_THAT(AreEqual(ExpectedPost, Visitor.PostOrder, TEXT("post-order must mirror structural ownership")));
		ASSERT_THAT(AreEqual((asUINT)5, Result.nodesEntered, TEXT("type references are reported but not followed by default")));

		const TArray<asEASTEdgeRole> ExpectedRoles = {
			asAST_EDGE_DECL_CHILD,
			asAST_EDGE_DECL_BODY,
			asAST_EDGE_STMT_CHILD,
			asAST_EDGE_STMT_EXPR,
			asAST_EDGE_EXPR_TYPE,
			asAST_EDGE_STMT_OWNER,
			asAST_EDGE_STMT_OWNER,
			asAST_EDGE_DECL_TYPE,
			asAST_EDGE_DECL_PARENT,
		};
		ASSERT_THAT(AreEqual(ExpectedRoles, Visitor.EdgeRoles, TEXT("all structural and reference edges need stable names and order")));
		ASSERT_THAT(AreEqual((asUINT)ExpectedRoles.Num(), Result.edgesVisited, TEXT("edge accounting must include non-followed references")));

		FRecordingASTVisitor TypeVisitor;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_OK,
			asCASTTraverse(Context, asSASTNodeRef::FromType(IntType), TypeVisitor, asSASTTraversalOptions(), Result),
			TEXT("a type must also be a first-class traversal root")));
		ASSERT_THAT(AreEqual(TArray<FString>({TEXT("T1")}), TypeVisitor.PreOrder, TEXT("type root should be visited")));
	}

	TEST_METHOD(BuildsOnDemandStructuralParentsAndIncomingReferenceEdgesForSealedContext)
	{
		asCASTContext Context;
		asASTDeclId Tu;
		asASTDeclId Function;
		asASTStmtId Body;
		asASTStmtId Return;
		asASTExprId Literal;
		asASTTypeRef IntType;
		BuildSmallSealedGraph(Context, Tu, Function, Body, Return, Literal, IntType);

		asCASTParentEdgeIndex Index;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_OK, Index.Build(Context), TEXT("sealed graph should accept an on-demand index")));
		ASSERT_THAT(IsTrue(Index.IsBuilt(), TEXT("successful build should retain an immutable index view")));

		asSASTTraversalEdge Parent;
		ASSERT_THAT(IsTrue(Index.GetStructuralParent(asSASTNodeRef::FromDecl(Function), Parent), TEXT("function needs its TU parent")));
		ASSERT_THAT(IsTrue(Parent.source == asSASTNodeRef::FromDecl(Tu), TEXT("function structural parent should be the TU")));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_DECL_CHILD, (int)Parent.role, TEXT("parent edge should preserve its role")));

		ASSERT_THAT(IsTrue(Index.GetStructuralParent(asSASTNodeRef::FromStmt(Body), Parent), TEXT("body needs its function parent")));
		ASSERT_THAT(IsTrue(Parent.source == asSASTNodeRef::FromDecl(Function), TEXT("body structural parent should be its function")));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_DECL_BODY, (int)Parent.role, TEXT("function-body ownership must stay distinguishable")));

		ASSERT_THAT(IsTrue(Index.GetStructuralParent(asSASTNodeRef::FromExpr(Literal), Parent), TEXT("literal needs its return-stmt parent")));
		ASSERT_THAT(IsTrue(Parent.source == asSASTNodeRef::FromStmt(Return), TEXT("literal parent should be its containing statement")));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_EXPR, (int)Parent.role, TEXT("statement expression ownership must stay distinguishable")));

		ASSERT_THAT(AreEqual((asUINT)2, Index.GetIncomingEdgeCount(asSASTNodeRef::FromType(IntType)),
			TEXT("one decl and one expr should reference the exact int type")));
		asSASTTraversalEdge Incoming;
		ASSERT_THAT(IsTrue(Index.GetIncomingEdge(asSASTNodeRef::FromType(IntType), 0, Incoming), TEXT("first incoming type edge should be queryable")));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_DECL_TYPE, (int)Incoming.role, TEXT("incoming edges should retain deterministic source order")));
		ASSERT_THAT(IsTrue(Index.GetIncomingEdge(asSASTNodeRef::FromType(IntType), 1, Incoming), TEXT("second incoming type edge should be queryable")));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_EXPR_TYPE, (int)Incoming.role, TEXT("expr type reference should be independently queryable")));
	}

	TEST_METHOD(VisitsCompleteContextInventoryOnceIncludingUnfollowedTypes)
	{
		asCASTContext Context;
		asASTDeclId Tu;
		asASTDeclId Function;
		asASTStmtId Body;
		asASTStmtId Return;
		asASTExprId Literal;
		asASTTypeRef IntType;
		BuildSmallSealedGraph(Context, Tu, Function, Body, Return, Literal, IntType);

		FRecordingASTVisitor Visitor;
		asSASTTraversalResult Result;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_OK,
			asCASTTraverseAll(Context, Visitor, asSASTTraversalOptions(), Result),
			TEXT("complete-context traversal should cover structural roots and unowned inventory")));
		const TArray<FString> ExpectedPre = {TEXT("D1"), TEXT("D2"), TEXT("S1"), TEXT("S2"), TEXT("E1"), TEXT("T1")};
		ASSERT_THAT(AreEqual(ExpectedPre, Visitor.PreOrder, TEXT("complete traversal should visit each node once in deterministic component order")));
		ASSERT_THAT(AreEqual((asUINT)6, Result.nodesEntered, TEXT("complete traversal should include the referenced type once")));
		ASSERT_THAT(AreEqual((asUINT)9, Result.edgesVisited, TEXT("skipped completed roots must not duplicate edge visits")));
	}

	TEST_METHOD(ConsumerCanObserveButSkipASelectedStructuralEdge)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("FilteredTraversal");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTExprId Receiver = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId Argument = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId Call = Context.CreateExpr(asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		Context.SetExprReceiver(Call, Receiver);
		Context.AddExprChild(Call, Argument);

		FSkipReceiverASTVisitor Visitor;
		asSASTTraversalResult Result;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_OK,
			asCASTTraverse(Context, asSASTNodeRef::FromExpr(Call), Visitor, asSASTTraversalOptions(), Result),
			TEXT("consumer edge filter should not abort traversal")));
		ASSERT_THAT(AreEqual(TArray<FString>({TEXT("E3"), TEXT("E2")}), Visitor.PreOrder,
			TEXT("receiver edge is visible but only the argument subtree is descended")));
		ASSERT_THAT(IsTrue(Visitor.EdgeRoles.Contains(asAST_EDGE_EXPR_RECEIVER),
			TEXT("skipping descent must not hide the named edge from diagnostics")));
	}

	TEST_METHOD(AccessSpecifierReferenceIsNamedIndexedAndWrongKindRejected)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("AccessTraversal");
		const asASTDeclId Record = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "Vault");
		const asASTDeclId Specifier = Context.CreateDecl(
			asAST_DECL_ACCESS_SPECIFIER, Record, asCSourceRange(), "Internal");
		ASSERT_THAT(AreEqual(0, Context.SetDeclTraits(Specifier, asAST_TRAIT_PRIVATE),
			TEXT("access specifier fixture needs exactly one base permission")));
		const asASTDeclId Field = Context.CreateDecl(
			asAST_DECL_VAR, Record, asCSourceRange(), "Value");
		ASSERT_THAT(AreEqual(0, Context.SetDeclAccessSpecifier(Field, Specifier),
			TEXT("field should accept an access-specifier reference")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("well-typed access reference should seal")));

		FRecordingASTVisitor Visitor;
		asSASTTraversalResult Result;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_OK,
			asCASTTraverse(Context, asSASTNodeRef::FromDecl(Field), Visitor,
				asSASTTraversalOptions(), Result),
			TEXT("member traversal should expose its access reference")));
		ASSERT_THAT(IsTrue(Visitor.EdgeRoles.Contains(asAST_EDGE_DECL_ACCESS_SPECIFIER),
			TEXT("member-to-access reference needs a stable named edge")));

		asCASTParentEdgeIndex Index;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_OK, Index.Build(Context),
			TEXT("sealed access graph should build an edge index")));
		bool bFoundIncoming = false;
		for (asUINT EdgeIndex = 0;
			EdgeIndex < Index.GetIncomingEdgeCount(asSASTNodeRef::FromDecl(Specifier));
			++EdgeIndex)
		{
			asSASTTraversalEdge Edge;
			if (Index.GetIncomingEdge(asSASTNodeRef::FromDecl(Specifier), EdgeIndex, Edge)
				&& Edge.role == asAST_EDGE_DECL_ACCESS_SPECIFIER
				&& Edge.source == asSASTNodeRef::FromDecl(Field))
			{
				bFoundIncoming = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bFoundIncoming,
			TEXT("parent-edge index must retain the exact access-reference source")));

		asCASTContext WrongKind;
		const asASTDeclId WrongTu = WrongKind.CreateTranslationUnit("WrongAccessTraversal");
		const asASTDeclId WrongRecord = WrongKind.CreateDecl(
			asAST_DECL_CLASS, WrongTu, asCSourceRange(), "Vault");
		const asASTDeclId WrongField = WrongKind.CreateDecl(
			asAST_DECL_VAR, WrongRecord, asCSourceRange(), "Value");
		ASSERT_THAT(AreEqual(0, WrongKind.SetDeclAccessSpecifier(WrongField, WrongRecord),
			TEXT("construction may retain malformed edge for verifier coverage")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, WrongKind.Seal(),
			TEXT("verifier must reject access edges to a non-specifier declaration")));
	}

	TEST_METHOD(RejectsUnsealedParentIndexAndBoundsMalformedTraversal)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("MalformedTraversal");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTExprId A = Context.CreateExpr(asAST_EXPR_UNARY, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId B = Context.CreateExpr(asAST_EXPR_UNARY, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		Context.AddExprChild(A, B);
		Context.AddExprChild(B, A);

		asCASTParentEdgeIndex Index;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_UNSEALED_CONTEXT, Index.Build(Context),
			TEXT("parent index must never imply immutability for a graph still under construction")));

		FRecordingASTVisitor Visitor;
		asSASTTraversalResult Result;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_CYCLE_DETECTED,
			asCASTTraverse(Context, asSASTNodeRef::FromExpr(A), Visitor, asSASTTraversalOptions(), Result),
			TEXT("malformed structural cycles must fail boundedly")));
		ASSERT_THAT(AreEqual((asUINT)2, Result.nodesEntered, TEXT("cycle target must not be entered twice")));
		ASSERT_THAT(IsTrue(Result.offendingEdge.role == asAST_EDGE_EXPR_CHILD, TEXT("cycle result should name the offending edge")));

		asSASTTraversalOptions BudgetOptions;
		BudgetOptions.maxNodes = 1;
		FRecordingASTVisitor BudgetVisitor;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_NODE_BUDGET_EXCEEDED,
			asCASTTraverse(Context, asSASTNodeRef::FromExpr(A), BudgetVisitor, BudgetOptions, Result),
			TEXT("node budget should stop a malformed or unexpectedly large graph")));
		ASSERT_THAT(AreEqual((asUINT)1, Result.nodesEntered, TEXT("budget should count only entered nodes")));

		asSASTTraversalOptions DepthOptions;
		DepthOptions.maxDepth = 0;
		FRecordingASTVisitor DepthVisitor;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_DEPTH_LIMIT_EXCEEDED,
			asCASTTraverse(Context, asSASTNodeRef::FromExpr(A), DepthVisitor, DepthOptions, Result),
			TEXT("depth guard should stop before descending beyond the configured root depth")));
		ASSERT_THAT(AreEqual((asUINT)1, Result.nodesEntered, TEXT("depth-rejected child must not be entered")));

		asCASTContext Dangling;
		Dangling.CreateTranslationUnit("DanglingTraversal");
		const asCQualType DanglingInt = Dangling.InternPrimitive(ttInt, 0);
		const asASTExprId Root = Dangling.CreateExpr(asAST_EXPR_UNARY, DanglingInt, asAST_VALUE_PRVALUE, asCSourceRange());
		Dangling.AddExprChild(Root, asASTExprId(99));
		FRecordingASTVisitor DanglingVisitor;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_INVALID_NODE,
			asCASTTraverse(Dangling, asSASTNodeRef::FromExpr(Root), DanglingVisitor, asSASTTraversalOptions(), Result),
			TEXT("dangling child must return a diagnostic status instead of dereferencing garbage")));
		ASSERT_THAT(IsTrue(Result.offendingNode == asSASTNodeRef::FromExpr(asASTExprId(99)), TEXT("invalid-node result should preserve the target ID")));

		asCASTContext ZeroSlot;
		ZeroSlot.CreateTranslationUnit("ZeroSlotTraversal");
		const asCQualType ZeroInt = ZeroSlot.InternPrimitive(ttInt, 0);
		const asASTExprId ZeroRoot = ZeroSlot.CreateExpr(asAST_EXPR_UNARY, ZeroInt, asAST_VALUE_PRVALUE, asCSourceRange());
		ZeroSlot.AddExprChild(ZeroRoot, asASTExprId());
		FRecordingASTVisitor ZeroVisitor;
		ASSERT_THAT(AreEqual((int)asAST_TRAVERSAL_INVALID_NODE,
			asCASTTraverse(ZeroSlot, asSASTNodeRef::FromExpr(ZeroRoot), ZeroVisitor, asSASTTraversalOptions(), Result),
			TEXT("an invalid ID stored in an array edge is malformed, not an absent optional edge")));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_EXPR_CHILD, (int)Result.offendingEdge.role,
			TEXT("zero-slot failure should still identify the array edge role")));
	}
};

#endif
