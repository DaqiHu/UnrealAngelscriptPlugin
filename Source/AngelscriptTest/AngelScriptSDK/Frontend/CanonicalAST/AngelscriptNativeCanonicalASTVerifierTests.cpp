#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_context.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_lifetime.h"
#include "source/as_ast_verifier.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
struct FLocalLifetimeViewFixture
{
	asASTDeclId Destructor;
	asASTDeclId FirstLocal;
	asASTDeclId SecondLocal;
	asASTStmtId Region;
	asASTExprId FirstAssignment;
	asASTExprId SecondAssignment;
	asASTExprId FirstInitializerValue;
	asASTExprId SecondInitializerValue;
};

static FLocalLifetimeViewFixture BuildLocalLifetimeViewFixture(
	asCASTContext& Context)
{
	FLocalLifetimeViewFixture Fixture;
	const asASTDeclId Tu = Context.CreateTranslationUnit("LifetimeView");
	const asCQualType ValueType = Context.InternNamedType(
		asAST_TYPE_VALUE_OBJECT, "FValue", 0);
	const asASTDeclId ValueClass = Context.CreateDecl(
		asAST_DECL_CLASS, Tu, asCSourceRange(), "FValue");
	Context.SetStableKey(ValueClass, "FValue");
	Fixture.Destructor = Context.CreateDecl(
		asAST_DECL_DESTRUCTOR, ValueClass, asCSourceRange(), "FValue");
	const asASTDeclId Function = Context.CreateDecl(
		asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
	Fixture.Region = Context.CreateStmt(
		asAST_STMT_BLOCK, Function, asCSourceRange());
	Context.SetBody(Function, Fixture.Region);

	for( asUINT LocalIndex = 0; LocalIndex < 2; ++LocalIndex )
	{
		const bool IsFirst = LocalIndex == 0;
		const asASTDeclId Local = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(),
			IsFirst ? "First" : "Second");
		Context.SetDeclType(Local, ValueType);
		const asASTStmtId DeclStmt = Context.CreateStmt(
			asAST_STMT_DECL, Function, asCSourceRange());
		Context.SetStmtDecl(DeclStmt, Local);
		Context.AddStmtChild(Fixture.Region, DeclStmt);

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType,
			asAST_VALUE_LVALUE, asCSourceRange());
		Context.SetResolvedDecl(Target, Local);
		const asASTExprId InitializerValue = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, ValueType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		Context.SetLiteral(InitializerValue, IsFirst ? "1" : "2");
		const asASTExprId Assignment = Context.CreateExpr(
			asAST_EXPR_ASSIGN, ValueType,
			asAST_VALUE_LVALUE, asCSourceRange());
		Context.SetLiteral(Assignment, "=");
		Context.AddExprChild(Assignment, Target);
		Context.AddExprChild(Assignment, InitializerValue);
		Context.AddDeclInit(Local, Assignment);
		const asASTStmtId ExprStmt = Context.CreateStmt(
			asAST_STMT_EXPR, Function, asCSourceRange());
		Context.SetStmtExpr(ExprStmt, Assignment);
		Context.AddStmtChild(Fixture.Region, ExprStmt);

		if( IsFirst )
		{
			Fixture.FirstLocal = Local;
			Fixture.FirstAssignment = Assignment;
			Fixture.FirstInitializerValue = InitializerValue;
		}
		else
		{
			Fixture.SecondLocal = Local;
			Fixture.SecondAssignment = Assignment;
			Fixture.SecondInitializerValue = InitializerValue;
		}
	}
	return Fixture;
}

static asSASTLifetimeRecord MakeLocalLifetimeRecord(
	const FLocalLifetimeViewFixture& Fixture,
	bool First,
	asUINT Order)
{
	asSASTLifetimeRecord Record;
	Record.subjectKind = asAST_LIFETIME_SUBJECT_LOCAL;
	Record.subject = asSASTLifetimeNodeId::FromDecl(
		First ? Fixture.FirstLocal : Fixture.SecondLocal);
	Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
	Record.actionTarget = asSASTLifetimeNodeId::FromDecl(Fixture.Destructor);
	Record.activationPoint = asSASTLifetimeNodeId::FromExpr(
		First ? Fixture.FirstAssignment : Fixture.SecondAssignment);
	Record.semanticRegion = asSASTLifetimeNodeId::FromStmt(Fixture.Region);
	Record.phase = asAST_LIFETIME_PHASE_LEXICAL_SCOPE;
	Record.supportedExitMask = asAST_LIFETIME_EXIT_ALL;
	Record.constructionStep = 0;
	Record.order = Order;
	Record.completeObjectCommit = false;
	return Record;
}

static asASTStmtId AppendNormalScopeExit(
	asCASTContext& Context,
	const asSASTLifetimeRecord& Record)
{
	const asASTDeclId Local(Record.subject.value);
	const asCDecl* const LocalDecl = Context.GetDecl(Local);
	const asASTExprId Target = Context.CreateExpr(
		asAST_EXPR_DECL_REF, LocalDecl->type,
		asAST_VALUE_LVALUE, asCSourceRange());
	Context.SetResolvedDecl(Target, Local);
	const asASTExprId Cleanup = Context.CreateExpr(
		asAST_EXPR_CLEANUP, LocalDecl->type,
		asAST_VALUE_PRVALUE, asCSourceRange());
	Context.SetLiteral(Cleanup, "scope-exit");
	Context.SetResolvedDecl(Cleanup, asASTDeclId(Record.actionTarget.value));
	Context.AddExprChild(Cleanup, Target);
	const asASTStmtId CleanupStmt = Context.CreateStmt(
		asAST_STMT_EXPR, LocalDecl->parent, asCSourceRange());
	Context.SetStmtExpr(CleanupStmt, Cleanup);
	Context.AddStmtChild(
		asASTStmtId(Record.semanticRegion.value), CleanupStmt);
	return CleanupStmt;
}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTVerifierTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.Verifier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ExposesNamedAuthenticatedCompatibilityCleanupActions)
	{
		asCASTContext Context;
		const FLocalLifetimeViewFixture Fixture =
			BuildLocalLifetimeViewFixture(Context);
		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF,
			Context.GetDecl(Fixture.FirstLocal)->type,
			asAST_VALUE_LVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(Target, Fixture.FirstLocal)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP,
			Context.GetDecl(Fixture.FirstLocal)->type,
			asAST_VALUE_PRVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Cleanup, "scope-exit")));
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(Cleanup, Fixture.Destructor)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Target)));
		const asASTStmtId CleanupStmt = Context.CreateStmt(
			asAST_STMT_EXPR,
			Context.GetDecl(Fixture.FirstLocal)->parent,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(CleanupStmt, Cleanup)));
		ASSERT_THAT(AreEqual(0,
			Context.AddStmtChild(Fixture.Region, CleanupStmt)));
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(
			MakeLocalLifetimeRecord(Fixture, true, 0))));
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asCASTLifetimeView View;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, View)));
		const asSASTLifetimeCleanupBinding* const Binding =
			View.FindCleanupBinding(CleanupStmt);
		ASSERT_THAT(IsNotNull(Binding,
			TEXT("a compatibility statement must resolve through a named authenticated binding")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), Binding->recordIndex));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_LIFETIME_ACTION_DESTROY_VALUE),
			static_cast<int32>(Binding->actionKind)));
		ASSERT_THAT(IsTrue(Binding->subject ==
			asSASTLifetimeNodeId::FromDecl(Fixture.FirstLocal)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_LIFETIME_COMPAT_NORMAL_EXIT),
			static_cast<int32>(Binding->role)));
	}

	TEST_METHOD(RejectsLifetimeProtocolCompatibilityEncodingDisagreement)
	{
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture =
				BuildLocalLifetimeViewFixture(Context);
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(
				MakeLocalLifetimeRecord(Fixture, true, 0))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("a protocol action without its compatibility statement must fail closed")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture =
				BuildLocalLifetimeViewFixture(Context);
			AppendNormalScopeExit(Context,
				MakeLocalLifetimeRecord(Fixture, true, 0));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("a compatibility statement without its protocol action must fail closed")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture =
				BuildLocalLifetimeViewFixture(Context);
			const asSASTLifetimeRecord Record =
				MakeLocalLifetimeRecord(Fixture, true, 0);
			AppendNormalScopeExit(Context, Record);
			AppendNormalScopeExit(Context, Record);
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Record)));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("duplicate compatibility statements for one protocol action must fail closed")));
		}
	}

	TEST_METHOD(BuildsDeterministicLifetimeViewWithCommittedLiveSets)
	{
		asCASTContext Context;
		const FLocalLifetimeViewFixture Fixture =
			BuildLocalLifetimeViewFixture(Context);
		const asSASTLifetimeRecord First =
			MakeLocalLifetimeRecord(Fixture, true, 0);
		const asSASTLifetimeRecord Second =
			MakeLocalLifetimeRecord(Fixture, false, 1);
		AppendNormalScopeExit(Context, Second);
		AppendNormalScopeExit(Context, First);
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(First)));
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Second)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("the exact local lifetime fixture must reach Frozen publication")));

		asCASTLifetimeView FirstView;
		asCASTLifetimeView SecondView;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, FirstView)));
		asCString DiagnosticDump;
		asCASTDump(Context, DiagnosticDump);
		ASSERT_THAT(IsTrue(DiagnosticDump.GetLength() != 0,
			TEXT("the compatibility dump is only a diagnostic side effect")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, SecondView)));

		ASSERT_THAT(AreEqual(static_cast<asUINT>(2), FirstView.GetRecordCount()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1), FirstView.GetScopeEdgeCount()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2), FirstView.GetCommitPointCount()));
		ASSERT_THAT(AreEqual(
			FirstView.GetStructuralDigest(),
			SecondView.GetStructuralDigest(),
			TEXT("rebuilding from one Frozen snapshot must be deterministic")));
		ASSERT_THAT(IsTrue(
			FirstView.GetStructuralDigest() != Context.GetLifetimeProtocolHash(),
			TEXT("the derived-view digest must have its own typed domain")));
		for( asUINT Index = 0; Index < FirstView.GetRecordCount(); ++Index )
		{
			ASSERT_THAT(IsTrue(
				*FirstView.GetRecord(Index) == *SecondView.GetRecord(Index),
				TEXT("typed derived records must reconstruct identically")));
		}

		const asSASTLifetimeCommitPoint* FirstCommit =
			FirstView.GetCommitPoint(0);
		const asSASTLifetimeCommitPoint* SecondCommit =
			FirstView.GetCommitPoint(1);
		ASSERT_THAT(IsNotNull(FirstCommit));
		ASSERT_THAT(IsNotNull(SecondCommit));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			FirstCommit->liveBefore.GetLength(),
			TEXT("the first local is not live while its initializer is running")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			FirstCommit->liveAfter.GetLength()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			SecondCommit->liveBefore.GetLength(),
			TEXT("only the first committed local is live before Second commits")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			SecondCommit->liveBefore[0]));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2),
			SecondCommit->liveAfter.GetLength()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			SecondCommit->reverseLiveCleanup[0],
			TEXT("normal cleanup starts at the newest committed local")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			SecondCommit->reverseLiveCleanup[1]));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			SecondCommit->abortCleanup.GetLength(),
			TEXT("initializer failure must exclude the failed current object")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			SecondCommit->abortCleanup[0]));
	}

	TEST_METHOD(DerivesNestedScopeEdgesAndOuterLiveSet)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("NestedLifetimeView");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FValue", 0);
		const asASTDeclId ValueClass = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FValue");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(ValueClass, "FValue")));
		const asASTDeclId Destructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, ValueClass, asCSourceRange(), "FValue");
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTStmtId OuterRegion = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Function, OuterRegion)));

		auto AddLocal = [&](const asASTStmtId Region, const char* Name,
			const char* Literal, asUINT Order) -> asSASTLifetimeRecord
		{
			const asASTDeclId Local = Context.CreateDecl(
				asAST_DECL_VAR, Function, asCSourceRange(), Name);
			Context.SetDeclType(Local, ValueType);
			const asASTStmtId DeclStmt = Context.CreateStmt(
				asAST_STMT_DECL, Function, asCSourceRange());
			Context.SetStmtDecl(DeclStmt, Local);
			Context.AddStmtChild(Region, DeclStmt);

			const asASTExprId Target = Context.CreateExpr(
				asAST_EXPR_DECL_REF, ValueType,
				asAST_VALUE_LVALUE, asCSourceRange());
			Context.SetResolvedDecl(Target, Local);
			const asASTExprId Initializer = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, ValueType,
				asAST_VALUE_PRVALUE, asCSourceRange());
			Context.SetLiteral(Initializer, Literal);
			const asASTExprId Assignment = Context.CreateExpr(
				asAST_EXPR_ASSIGN, ValueType,
				asAST_VALUE_LVALUE, asCSourceRange());
			Context.SetLiteral(Assignment, "=");
			Context.AddExprChild(Assignment, Target);
			Context.AddExprChild(Assignment, Initializer);
			Context.AddDeclInit(Local, Assignment);
			const asASTStmtId ExprStmt = Context.CreateStmt(
				asAST_STMT_EXPR, Function, asCSourceRange());
			Context.SetStmtExpr(ExprStmt, Assignment);
			Context.AddStmtChild(Region, ExprStmt);

			asSASTLifetimeRecord Record;
			Record.subjectKind = asAST_LIFETIME_SUBJECT_LOCAL;
			Record.subject = asSASTLifetimeNodeId::FromDecl(Local);
			Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
			Record.actionTarget = asSASTLifetimeNodeId::FromDecl(Destructor);
			Record.activationPoint = asSASTLifetimeNodeId::FromExpr(Assignment);
			Record.semanticRegion = asSASTLifetimeNodeId::FromStmt(Region);
			Record.phase = asAST_LIFETIME_PHASE_LEXICAL_SCOPE;
			Record.supportedExitMask = asAST_LIFETIME_EXIT_ALL;
			Record.order = Order;
			return Record;
		};

		const asSASTLifetimeRecord OuterRecord =
			AddLocal(OuterRegion, "Outer", "1", 0);
		const asASTStmtId InnerRegion = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(OuterRegion, InnerRegion)));
		const asSASTLifetimeRecord InnerRecord =
			AddLocal(InnerRegion, "Inner", "2", 1);
		AppendNormalScopeExit(Context, InnerRecord);
		AppendNormalScopeExit(Context, OuterRecord);
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(OuterRecord)));
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(InnerRecord)));
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asCASTLifetimeView View;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, View)));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2), View.GetScopeEdgeCount()));
		const asSASTLifetimeScopeEdge* OuterEdge = nullptr;
		const asSASTLifetimeScopeEdge* InnerEdge = nullptr;
		for( asUINT Index = 0; Index < View.GetScopeEdgeCount(); ++Index )
		{
			const asSASTLifetimeScopeEdge* Edge = View.GetScopeEdge(Index);
			if( Edge->region == asSASTLifetimeNodeId::FromStmt(OuterRegion) )
				OuterEdge = Edge;
			if( Edge->region == asSASTLifetimeNodeId::FromStmt(InnerRegion) )
				InnerEdge = Edge;
		}
		ASSERT_THAT(IsNotNull(OuterEdge));
		ASSERT_THAT(IsNotNull(InnerEdge));
		ASSERT_THAT(IsFalse(OuterEdge->parentRegion.IsValid()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), OuterEdge->depth));
		ASSERT_THAT(IsTrue(
			InnerEdge->parentRegion == asSASTLifetimeNodeId::FromStmt(OuterRegion)));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1), InnerEdge->depth));

		const asSASTLifetimeCommitPoint* InnerCommit = View.GetCommitPoint(1);
		ASSERT_THAT(IsNotNull(InnerCommit));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			InnerCommit->liveBefore.GetLength()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), InnerCommit->liveBefore[0],
			TEXT("the committed outer local must be live before Inner initializes")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2),
			InnerCommit->liveAfter.GetLength()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			InnerCommit->reverseLiveCleanup[0]));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			InnerCommit->reverseLiveCleanup[1]));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			InnerCommit->abortCleanup.GetLength()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			InnerCommit->abortCleanup[0],
			TEXT("failed Inner initialization cleans only the committed outer local")));
	}

	TEST_METHOD(RejectsForgedLifetimeSubjectActionActivationOrderAndPhase)
	{
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.subject = asSASTLifetimeNodeId::FromDecl(Fixture.Destructor);
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("a destructor declaration cannot masquerade as a local subject")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.actionKind = asAST_LIFETIME_ACTION_RELEASE_REFERENCE;
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("a value local cannot replace its exact destructor with release")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.activationPoint = asSASTLifetimeNodeId::FromExpr(Fixture.FirstInitializerValue);
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("cleanup may not activate while the current initializer can still fail")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			Context.AddLifetimeRecord(MakeLocalLifetimeRecord(Fixture, false, 0));
			Context.AddLifetimeRecord(MakeLocalLifetimeRecord(Fixture, true, 1));
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("storage indices cannot forge source commit order inside one region")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.phase = asAST_LIFETIME_PHASE_FULL_EXPRESSION;
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("a lexical local may not be reclassified as a temporary phase")));
		}
	}

	TEST_METHOD(RejectsMissingDuplicateForeignAndEarlyLifetimeActions)
	{
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.actionTarget = asSASTLifetimeNodeId();
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("a committed subject requires one exact cleanup action")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			Context.AddLifetimeRecord(MakeLocalLifetimeRecord(Fixture, true, 0));
			asSASTLifetimeRecord Duplicate = MakeLocalLifetimeRecord(Fixture, true, 1);
			Context.AddLifetimeRecord(Duplicate);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("one commit cannot publish the same cleanup action twice")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.subject.snapshotOwner = 0xA57u;
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("snapshot-local lifetime facts reject public or foreign owner tokens")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.phase = asAST_LIFETIME_PHASE_COMPLETE_OBJECT;
			Record.completeObjectCommit = true;
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("complete-object destruction is unreachable before a distinct complete commit")));
		}
	}

	TEST_METHOD(RejectsWrongLifetimeProtocolRevisionBeforePlanning)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("WrongLifetimeRevision");
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			Context.SetLifetimeProtocolRevision(
				asAST_LIFETIME_PROTOCOL_REVISION + 1),
			TEXT("the malformed-protocol fixture must be authored while Building")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			Context.AdvanceLifecycle(
				asAST_CONTEXT_BUILDING,
				asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("structural Sema finalization precedes lifetime protocol authentication")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
			Context.AdvanceLifecycle(
				asAST_CONTEXT_SEMA_FINALIZED,
				asAST_CONTEXT_LIFETIME_PLANNED),
			TEXT("an unknown lifetime protocol revision must fail closed")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_CONTEXT_SEMA_FINALIZED),
			static_cast<int32>(Context.GetLifecycleState()),
			TEXT("failed authentication must not claim LifetimePlanned")));
		ASSERT_THAT(IsFalse(Context.IsPublishable(),
			TEXT("a wrong-revision protocol must never reach a backend or snapshot reader")));
	}

	TEST_METHOD(RejectsDanglingWrongKindAndInvalidRange)
	{
		asCASTContext Empty;
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, asCASTVerify(Empty, Result),
			TEXT("missing translation unit should be dangling-id")));

		asCASTContext Context;
		Context.CreateTranslationUnit("V");
		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Context.GetTranslationUnit(), asCSourceRange(), "F");
		ASSERT_THAT(IsTrue(Fn.IsValid(), TEXT("function decl should exist")));
		ASSERT_THAT(AreEqual(0, Context.AddDeclChild(Fn, asASTDeclId(99)), TEXT("foreign child attach")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("unknown child ID should fail closed")));

		asCASTContext Kinds;
		Kinds.CreateTranslationUnit("K");
		const asASTDeclId Broken = Kinds.CreateDecl(asAST_DECL_FUNCTION, Kinds.GetTranslationUnit(), asCSourceRange(), "G");
		ASSERT_THAT(AreEqual(0, Kinds.SetDeclKind(Broken, asAST_DECL_INVALID), TEXT("kind corruption")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Kinds, Result),
			TEXT("invalid kind should fail closed")));
	}

	TEST_METHOD(RejectsInvalidSourceRange)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("R");
		asCSourceRange Range;
		Range.begin = asCSourceLocation(1, 0);
		Range.end = asCSourceLocation(1, 4);
		Context.CreateDecl(asAST_DECL_FUNCTION, Tu, Range, "F");
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_RANGE, asCASTVerify(Context, Result),
			TEXT("range into a missing FileID should fail")));
	}

	TEST_METHOD(RejectsStmtIndexMismatch)
	{
		asCASTContext Stmts;
		Stmts.CreateTranslationUnit("StmtV");
		const asASTStmtId Block = Stmts.CreateStmt(asAST_STMT_BLOCK, Stmts.GetTranslationUnit(), asCSourceRange());
		ASSERT_THAT(IsTrue(Block.IsValid(), TEXT("block stmt should exist")));
		ASSERT_THAT(AreEqual(0, Stmts.SetStmtId(Block, asASTStmtId(99)), TEXT("stmt id corruption")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_FOREIGN_ID, asCASTVerify(Stmts, Result),
			TEXT("stmt id must match its table index")));
	}

	TEST_METHOD(RejectsExprIndexMismatch)
	{
		asCASTContext Exprs;
		Exprs.CreateTranslationUnit("ExprV");
		const asCQualType IntType = Exprs.InternPrimitive(ttInt, 0);
		const asASTExprId Lit = Exprs.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(IsTrue(Lit.IsValid(), TEXT("literal expr should exist")));
		ASSERT_THAT(AreEqual(0, Exprs.SetExprId(Lit, asASTExprId(99)), TEXT("expr id corruption")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_FOREIGN_ID, asCASTVerify(Exprs, Result),
			TEXT("expr id must match its table index")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("expr-id"), TEXT("detail token must be expr-id")));
	}

	TEST_METHOD(RejectsParentChildNotBidirectional)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ParentV");
		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		ASSERT_THAT(IsNotNull(Context.GetDecl(Tu), TEXT("translation unit should exist")));
		ASSERT_THAT(IsTrue(Fn.IsValid(), TEXT("function child should exist")));
		ASSERT_THAT(AreEqual(0, Context.ClearDeclChildren(Tu), TEXT("drop reverse child list")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("child parent must be listed on the parent")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("decl-parent-child"), TEXT("detail token must be decl-parent-child")));
	}

	TEST_METHOD(RejectsBreakTargetNotAncestor)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("BreakV");
		const asASTStmtId LoopA = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId LoopB = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId Brk = Context.CreateStmt(asAST_STMT_BREAK, Tu, asCSourceRange());
		ASSERT_THAT(IsNotNull(Context.GetStmt(LoopA), TEXT("loop A should exist")));
		ASSERT_THAT(IsNotNull(Context.GetStmt(Brk), TEXT("break should exist")));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(LoopA, Brk), TEXT("break under loop A")));
		ASSERT_THAT(AreEqual(0, Context.SetTarget(Brk, LoopB), TEXT("break targets sibling loop B")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Context, Result),
			TEXT("break must target an ancestor loop or switch")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("break-ancestor"), TEXT("detail token must be break-ancestor")));
	}

	TEST_METHOD(RejectsExprMissingExactType)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("TypeV");
		const asASTExprId Lit = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, asCQualType(), asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(IsTrue(Lit.IsValid(), TEXT("untyped literal should still be allocated")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_TYPE, asCASTVerify(Context, Result),
			TEXT("non-error expr must carry an exact interned type")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("expr-type"), TEXT("detail token must be expr-type")));
	}

	TEST_METHOD(RejectsExprOperandWrongKind)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("OperandV");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTExprId Bin = Context.CreateExpr(asAST_EXPR_BINARY, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(IsTrue(Bin.IsValid(), TEXT("binary expr should exist")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Bin, asASTExprId(99)), TEXT("dangling operand")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("expr operands must resolve as expr ids")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("expr-operand"), TEXT("detail token must be expr-operand")));
	}

	TEST_METHOD(RejectsStmtMultiOwner)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("StmtMulti");
		const asASTStmtId LoopA = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId LoopB = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId Inner = Context.CreateStmt(asAST_STMT_BLOCK, Tu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(LoopA, Inner), TEXT("inner under loop A")));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(LoopB, Inner), TEXT("same inner under loop B")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("a stmt must not have two parents")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("stmt-multi-owner"), TEXT("detail token must be stmt-multi-owner")));
	}

	TEST_METHOD(RejectsStmtCycle)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("StmtCycle");
		const asASTStmtId BlockA = Context.CreateStmt(asAST_STMT_BLOCK, Tu, asCSourceRange());
		const asASTStmtId BlockB = Context.CreateStmt(asAST_STMT_BLOCK, Tu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(BlockA, BlockB), TEXT("A owns B")));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(BlockB, BlockA), TEXT("B owns A")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("stmt child lists must be acyclic")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("stmt-cycle"), TEXT("detail token must be stmt-cycle")));
	}

	TEST_METHOD(RejectsExprCycle)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("ExprCycle");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTExprId UnaryA = Context.CreateExpr(asAST_EXPR_UNARY, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId UnaryB = Context.CreateExpr(asAST_EXPR_UNARY, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(UnaryA, UnaryB), TEXT("A owns B")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(UnaryB, UnaryA), TEXT("B owns A")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("expr child lists must be acyclic before a backend can traverse them")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("expr-cycle"), TEXT("detail token must be expr-cycle")));
	}

	TEST_METHOD(RejectsExprReceiverCycle)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("ExprReceiverCycle");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTExprId CallA = Context.CreateExpr(asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId CallB = Context.CreateExpr(asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetExprReceiver(CallA, CallB), TEXT("A receives B")));
		ASSERT_THAT(AreEqual(0, Context.SetExprReceiver(CallB, CallA), TEXT("B receives A")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("effective receiver traversal must not form a cycle")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("expr-cycle"), TEXT("detail token must be expr-cycle")));
	}

	TEST_METHOD(RejectsGeneratedGetterInitWithoutExactCopyConstructionPlan)
	{
		asCASTContext Context;
		const asASTDeclId Tu =
			Context.CreateTranslationUnit("GeneratedGetterCopyPlan");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FVerifierNativeValue", 0);
		const asASTDeclId Owner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FOwner");
		const asASTDeclId Field = Context.CreateDecl(
			asAST_DECL_VAR, Owner, asCSourceRange(), "Inner");
		const asASTDeclId Getter = Context.CreateDecl(
			asAST_DECL_METHOD, Owner, asCSourceRange(), "GetInner");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Field, ValueType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Getter, ValueType)));
		ASSERT_THAT(AreEqual(0,
			Context.AddDeclTrait(Getter, asAST_TRAIT_GENERATED)));
		ASSERT_THAT(AreEqual(0,
			Context.SetGeneratedAccessor(Getter, asAST_ACCESSOR_GET, Field)));

		// A field reference has the right storage identity and type, but it is not
		// an object-return lifetime plan. Publishing it would let a backend invent
		// raw-copy or copy-constructor semantics after Sema.
		const asASTExprId BareField = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(BareField, Field)));
		ASSERT_THAT(AreEqual(0, Context.AddDeclInit(Getter, BareField)));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			static_cast<int>(asAST_VERIFY_INVALID_CHILD),
			asCASTVerify(Context, Result),
			TEXT("a generated getter init must be an exact sealed copy-construction plan")));
		ASSERT_THAT(IsTrue(
			Result.detail.Equals("accessor-get-copy-plan"),
			TEXT("the malformed getter plan needs a stable verifier token")));
	}

	TEST_METHOD(AcceptsGeneratedGetterExactCopyConstructionPlan)
	{
		asCASTContext Context;
		const asASTDeclId Tu =
			Context.CreateTranslationUnit("ValidGeneratedGetterCopyPlan");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FVerifierNativeValue", 0);
		const asCQualType VoidType = Context.InternPrimitive(ttVoid, 0);

		const asASTDeclId ValueOwner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FVerifierNativeValue");
		const asASTDeclId CopyConstructor = Context.CreateDecl(
			asAST_DECL_CONSTRUCTOR, ValueOwner, asCSourceRange(),
			"FVerifierNativeValue");
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(CopyConstructor, VoidType)));
		const asASTDeclId CopyParameter = Context.CreateDecl(
			asAST_DECL_PARAM, CopyConstructor, asCSourceRange(), "Other");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(
			CopyParameter,
			asCQualType(
				ValueType.type,
				asAST_QUAL_CONST | asAST_QUAL_REFERENCE | asAST_QUAL_IN))));

		const asASTDeclId Owner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FOwner");
		const asASTDeclId Field = Context.CreateDecl(
			asAST_DECL_VAR, Owner, asCSourceRange(), "Inner");
		const asASTDeclId Getter = Context.CreateDecl(
			asAST_DECL_METHOD, Owner, asCSourceRange(), "GetInner");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Field, ValueType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Getter, ValueType)));
		ASSERT_THAT(AreEqual(0,
			Context.AddDeclTrait(Getter, asAST_TRAIT_GENERATED)));
		ASSERT_THAT(AreEqual(0,
			Context.SetGeneratedAccessor(Getter, asAST_ACCESSOR_GET, Field)));

		const asASTExprId Source = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Source, Field)));
		const asASTExprId Construct = Context.CreateExpr(
			asAST_EXPR_CONSTRUCT, ValueType, asAST_VALUE_PRVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(Construct, CopyConstructor)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Construct, Source)));
		const asASTExprId Materialize = Context.CreateExpr(
			asAST_EXPR_MATERIALIZE_TEMPORARY, ValueType,
			asAST_VALUE_XVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.AddExprChild(Materialize, Construct)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP, ValueType, asAST_VALUE_PRVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.AddExprChild(Cleanup, Materialize)));
		ASSERT_THAT(AreEqual(0, Context.AddDeclInit(Getter, Cleanup)));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			static_cast<int>(asAST_VERIFY_OK),
			asCASTVerify(Context, Result),
			TEXT("the exact field-to-copy-constructor plan must remain publishable")));
	}

	TEST_METHOD(RejectsGeneratedSetterInitWithoutExactAssignmentPlan)
	{
		asCASTContext Context;
		const asASTDeclId Tu =
			Context.CreateTranslationUnit("GeneratedSetterAssignmentPlan");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FVerifierSetterValue", 0);
		const asCQualType VoidType = Context.InternPrimitive(ttVoid, 0);
		const asASTDeclId Owner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FOwner");
		const asASTDeclId Field = Context.CreateDecl(
			asAST_DECL_VAR, Owner, asCSourceRange(), "Inner");
		const asASTDeclId Setter = Context.CreateDecl(
			asAST_DECL_METHOD, Owner, asCSourceRange(), "SetInner");
		const asASTDeclId Parameter = Context.CreateDecl(
			asAST_DECL_PARAM, Setter, asCSourceRange(), "value");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Field, ValueType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Setter, VoidType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Parameter, ValueType)));
		ASSERT_THAT(AreEqual(0,
			Context.AddDeclTrait(Setter, asAST_TRAIT_GENERATED)));
		ASSERT_THAT(AreEqual(0,
			Context.SetGeneratedAccessor(Setter, asAST_ACCESSOR_SET, Field)));

		const asASTExprId BareParameter = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(BareParameter, Parameter)));
		ASSERT_THAT(AreEqual(0, Context.AddDeclInit(Setter, BareParameter)));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			static_cast<int>(asAST_VERIFY_INVALID_CHILD),
			asCASTVerify(Context, Result),
			TEXT("a generated setter init must be an exact sealed assignment plan")));
		ASSERT_THAT(IsTrue(
			Result.detail.Equals("accessor-set-assignment-plan"),
			TEXT("the malformed setter plan needs a stable verifier token")));
	}

	TEST_METHOD(AcceptsGeneratedSetterExactAssignmentPlan)
	{
		asCASTContext Context;
		const asASTDeclId Tu =
			Context.CreateTranslationUnit("ValidGeneratedSetterAssignmentPlan");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FVerifierSetterValue", 0);
		const asCQualType ConstRefValueType(
			ValueType.type,
			asAST_QUAL_CONST | asAST_QUAL_REFERENCE | asAST_QUAL_IN);
		const asCQualType VoidType = Context.InternPrimitive(ttVoid, 0);

		const asASTDeclId ValueOwner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FVerifierSetterValue");
		const asASTDeclId AssignOperator = Context.CreateDecl(
			asAST_DECL_METHOD, ValueOwner, asCSourceRange(), "opAssign");
		const asASTDeclId AssignParameter = Context.CreateDecl(
			asAST_DECL_PARAM, AssignOperator, asCSourceRange(), "Other");
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(AssignOperator, ConstRefValueType)));
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(AssignParameter, ConstRefValueType)));

		const asASTDeclId Owner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FOwner");
		const asASTDeclId Field = Context.CreateDecl(
			asAST_DECL_VAR, Owner, asCSourceRange(), "Inner");
		const asASTDeclId Setter = Context.CreateDecl(
			asAST_DECL_METHOD, Owner, asCSourceRange(), "SetInner");
		const asASTDeclId Parameter = Context.CreateDecl(
			asAST_DECL_PARAM, Setter, asCSourceRange(), "value");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Field, ValueType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Setter, VoidType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Parameter, ValueType)));
		ASSERT_THAT(AreEqual(0,
			Context.AddDeclTrait(Setter, asAST_TRAIT_GENERATED)));
		ASSERT_THAT(AreEqual(0,
			Context.SetGeneratedAccessor(Setter, asAST_ACCESSOR_SET, Field)));

		const asASTExprId Lhs = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		const asASTExprId Rhs = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		const asASTExprId Assign = Context.CreateExpr(
			asAST_EXPR_ASSIGN, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Lhs, Field)));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Rhs, Parameter)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Assign, Lhs)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Assign, Rhs)));
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(Assign, AssignOperator)));
		ASSERT_THAT(AreEqual(0, Context.AddDeclInit(Setter, Assign)));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			static_cast<int>(asAST_VERIFY_OK),
			asCASTVerify(Context, Result),
			TEXT("the verifier must accept the exact generated setter assignment plan")));
	}

	TEST_METHOD(RejectsFallthroughOutsideSwitch)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("FallV");
		const asASTStmtId Block = Context.CreateStmt(asAST_STMT_BLOCK, Tu, asCSourceRange());
		const asASTStmtId Fall = Context.CreateStmt(asAST_STMT_FALLTHROUGH, Tu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Block, Fall), TEXT("fallthrough under a bare block")));
		asSAstVerifyResult Result;
		ASSERT_THAT(IsTrue(asCASTVerify(Context, Result) != (int)asAST_VERIFY_OK,
			TEXT("fallthrough must sit under a switch")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("fallthrough-switch"), TEXT("detail token must be fallthrough-switch")));
	}

	TEST_METHOD(RejectsUnsealedPublication)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("PubV");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTStmtId Body = Context.CreateStmt(asAST_STMT_RETURN, Fn, asCSourceRange());
		const asASTExprId Lit = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Fn, Body), TEXT("set body")));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Body, Lit), TEXT("return literal")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerify(Context, Result),
			TEXT("asCASTVerify must still accept an unsealed construction graph so Seal() can run")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION, asCASTVerifyPublication(Context, Result),
			TEXT("publication of an unsealed graph must fail closed")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("unsealed-publication"), TEXT("detail token must be unsealed-publication")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_BUILDING, asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("fixture must reach SemaFinalized")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION, asCASTVerifyPublication(Context, Result),
			TEXT("SemaFinalized must not be publishable")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_SEMA_FINALIZED, asAST_CONTEXT_LIFETIME_PLANNED),
			TEXT("fixture must reach LifetimePlanned")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION, asCASTVerifyPublication(Context, Result),
			TEXT("LifetimePlanned must not be publishable")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_LIFETIME_PLANNED, asAST_CONTEXT_FROZEN_PUBLISHABLE),
			TEXT("fixture must reach Frozen/Publishable")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerifyPublication(Context, Result),
			TEXT("publication of a Frozen/Publishable verified graph must succeed")));
	}

	TEST_METHOD(RejectsCleanupResolvedDeclNotDestructor)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("CleanupV");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "NotDtor");
		const asASTExprId Inner = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId Cleanup = Context.CreateExpr(asAST_EXPR_CLEANUP, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Inner), TEXT("cleanup arity 1")));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Cleanup, Fn), TEXT("point cleanup at a non-destructor")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Context, Result),
			TEXT("CLEANUP resolvedDecl if set must be a destructor")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("cleanup-dtor"), TEXT("detail token must be cleanup-dtor")));
	}

	TEST_METHOD(RejectsScopeExitCleanupBoundToWrongValueTypeDestructor)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ScopeExitCleanupTypeV");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FExpectedScopeValue", 0);
		const asASTDeclId ExpectedClass = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FExpectedScopeValue");
		const asASTDeclId WrongClass = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FWrongScopeValue");
		const asASTDeclId WrongDestructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, WrongClass, asCSourceRange(), "FWrongScopeValue");
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTDeclId Local = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(), "Local");
		ASSERT_THAT(IsTrue(ExpectedClass.IsValid()));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Local, ValueType)));

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Target, Local)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP, ValueType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Target)));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Cleanup, WrongDestructor)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Cleanup, "scope-exit")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_INVALID_TYPE,
			asCASTVerify(Context, Result),
			TEXT("scope-exit cleanup must reject a destructor from a different value type")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("scope-exit-cleanup-type"),
			TEXT("detail token must identify the forged scope-exit destructor owner")));
	}

	TEST_METHOD(RejectsScopeReleaseCleanupForNonOwningReference)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ScopeReleaseNonOwningV");
		const asCQualType NonOwningReference = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT, "CScopedRef", asAST_QUAL_REFERENCE);
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTDeclId Local = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(), "Borrowed");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Local, NonOwningReference)));

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, NonOwningReference, asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Target, Local)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP, NonOwningReference, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Target)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Cleanup, "scope-release")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_INVALID_TYPE,
			asCASTVerify(Context, Result),
			TEXT("scope-release cleanup must reject a borrowed non-owning reference")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("scope-release-cleanup-type"),
			TEXT("detail token must identify the forged non-owning release target")));
	}

	TEST_METHOD(RejectsScopeReleaseCleanupWithDestructorBinding)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ScopeReleaseDestructorV");
		const asCQualType OwningHandle = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT,
			"CScopedRef",
			asAST_QUAL_HANDLE | asAST_QUAL_AUTO_HANDLE);
		const asASTDeclId RefClass = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "CScopedRef");
		const asASTDeclId Destructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, RefClass, asCSourceRange(), "CScopedRef");
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTDeclId Local = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(), "Owned");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Local, OwningHandle)));

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, OwningHandle, asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Target, Local)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP, OwningHandle, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Target)));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Cleanup, Destructor)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Cleanup, "scope-release")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_WRONG_KIND,
			asCASTVerify(Context, Result),
			TEXT("scope-release cleanup must not bind a value-object destructor route")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("scope-release-cleanup-decl"),
			TEXT("detail token must identify the forged release destructor binding")));
	}

	TEST_METHOD(RejectsScopeReleaseCleanupForUninitializedOwningHandle)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ScopeReleaseUninitializedV");
		const asCQualType OwningHandle = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT, "CScopedRef", asAST_QUAL_HANDLE);
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTDeclId Local = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(), "NeverInitialized");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Local, OwningHandle)));

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, OwningHandle, asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Target, Local)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP, OwningHandle, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Target)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Cleanup, "scope-release")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_INVALID_CHILD,
			asCASTVerify(Context, Result),
			TEXT("scope-release must reject a local with no sealed initialization fact")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("scope-release-cleanup-uninitialized"),
			TEXT("detail token must identify the missing live-state initializer")));
	}

	TEST_METHOD(RejectsBreakSkippedNearerLoop)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("SkipV");
		const asASTStmtId Outer = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId Inner = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId Brk = Context.CreateStmt(asAST_STMT_BREAK, Tu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Outer, Inner), TEXT("inner under outer")));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Inner, Brk), TEXT("break under inner")));
		ASSERT_THAT(AreEqual(0, Context.SetTarget(Brk, Outer), TEXT("break skips inner, targets outer")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Context, Result),
			TEXT("break must target the nearest enclosing loop or switch, not a skipped outer loop")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("break-skipped-nearer"),
			TEXT("detail token must be break-skipped-nearer")));

		asCASTContext Dangling;
		const asASTDeclId DTu = Dangling.CreateTranslationUnit("DangleV");
		const asASTStmtId Loop = Dangling.CreateStmt(asAST_STMT_WHILE, DTu, asCSourceRange());
		const asASTStmtId DBrk = Dangling.CreateStmt(asAST_STMT_BREAK, DTu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Dangling.AddStmtChild(Loop, DBrk), TEXT("break under loop")));
		ASSERT_THAT(AreEqual(0, Dangling.SetTarget(DBrk, asASTStmtId(99)), TEXT("dangling target id")));
		asSAstVerifyResult DanglingResult;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, asCASTVerify(Dangling, DanglingResult),
			TEXT("break target must resolve in the stmt table")));
		ASSERT_THAT(IsTrue(
			DanglingResult.detail.Equals("stmt-target") || DanglingResult.detail.Equals("ctrl-target"),
			TEXT("detail token must stay stmt-target or ctrl-target")));
	}

	TEST_METHOD(RejectsContinueSkippedNearerLoop)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ContSkipV");
		const asASTStmtId Outer = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId Inner = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId Cont = Context.CreateStmt(asAST_STMT_CONTINUE, Tu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Outer, Inner), TEXT("inner under outer")));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Inner, Cont), TEXT("continue under inner")));
		ASSERT_THAT(AreEqual(0, Context.SetTarget(Cont, Outer), TEXT("continue skips inner, targets outer")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Context, Result),
			TEXT("continue must target the nearest enclosing loop, not a skipped outer loop")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("continue-skipped-nearer"),
			TEXT("detail token must be continue-skipped-nearer")));
	}

	TEST_METHOD(RejectsDuplicateCaseAndDefaultNotLast)
	{
		asCASTContext Dup;
		const asASTDeclId Tu = Dup.CreateTranslationUnit("DupCaseV");
		const asCQualType IntType = Dup.InternPrimitive(ttInt, 0);
		const asASTStmtId Sw = Dup.CreateStmt(asAST_STMT_SWITCH, Tu, asCSourceRange());
		const asASTStmtId CaseA = Dup.CreateStmt(asAST_STMT_CASE, Tu, asCSourceRange());
		const asASTStmtId CaseB = Dup.CreateStmt(asAST_STMT_CASE, Tu, asCSourceRange());
		const asASTExprId LitA = Dup.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId LitB = Dup.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Dup.SetLiteral(LitA, "1")));
		ASSERT_THAT(AreEqual(0, Dup.SetLiteral(LitB, "1")));
		ASSERT_THAT(AreEqual(0, Dup.SetLiteralBits(LitA, 1)));
		ASSERT_THAT(AreEqual(0, Dup.SetLiteralBits(LitB, 1)));
		ASSERT_THAT(AreEqual(0, Dup.SetStmtExpr(CaseA, LitA)));
		ASSERT_THAT(AreEqual(0, Dup.SetStmtExpr(CaseB, LitB)));
		ASSERT_THAT(AreEqual(0, Dup.AddStmtChild(Sw, CaseA)));
		ASSERT_THAT(AreEqual(0, Dup.AddStmtChild(Sw, CaseB)));
		asSAstVerifyResult DupResult;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Dup, DupResult),
			TEXT("duplicate normalized case constants must fail")));
		ASSERT_THAT(IsTrue(DupResult.detail.Equals("duplicate-case"),
			TEXT("detail token must be duplicate-case")));

		asCASTContext Order;
		const asASTDeclId OTu = Order.CreateTranslationUnit("DefOrderV");
		const asASTStmtId OSw = Order.CreateStmt(asAST_STMT_SWITCH, OTu, asCSourceRange());
		const asASTStmtId DefaultCase = Order.CreateStmt(asAST_STMT_CASE, OTu, asCSourceRange());
		const asASTStmtId LaterCase = Order.CreateStmt(asAST_STMT_CASE, OTu, asCSourceRange());
		const asASTExprId LaterLit = Order.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Order.SetLiteral(LaterLit, "2")));
		ASSERT_THAT(AreEqual(0, Order.SetLiteralBits(LaterLit, 2)));
		ASSERT_THAT(AreEqual(0, Order.SetStmtExpr(LaterCase, LaterLit)));
		ASSERT_THAT(AreEqual(0, Order.AddStmtChild(OSw, DefaultCase), TEXT("default first (no expr)")));
		ASSERT_THAT(AreEqual(0, Order.AddStmtChild(OSw, LaterCase), TEXT("valued case after default")));
		asSAstVerifyResult OrderResult;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Order, OrderResult),
			TEXT("default must be last in source order (current dialect TXT_DEFAULT_MUST_BE_LAST)")));
		ASSERT_THAT(IsTrue(OrderResult.detail.Equals("default-order"),
			TEXT("detail token must be default-order")));
	}

	TEST_METHOD(RejectsFallthroughTargetNotNextCase)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("FallTargetV");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTStmtId Sw = Context.CreateStmt(asAST_STMT_SWITCH, Tu, asCSourceRange());
		const asASTStmtId Case0 = Context.CreateStmt(asAST_STMT_CASE, Tu, asCSourceRange());
		const asASTStmtId Case1 = Context.CreateStmt(asAST_STMT_CASE, Tu, asCSourceRange());
		const asASTStmtId Fall = Context.CreateStmt(asAST_STMT_FALLTHROUGH, Tu, asCSourceRange());
		const asASTExprId Lit0 = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId Lit1 = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Lit0, 0)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Lit1, 1)));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Case0, Lit0)));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Case1, Lit1)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Sw, Case0)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Sw, Case1)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Case0, Fall)));
		ASSERT_THAT(AreEqual(0, Context.SetTarget(Fall, Sw), TEXT("fallthrough targets the switch, not next case")));
		asSAstVerifyResult Result;
		ASSERT_THAT(IsTrue(asCASTVerify(Context, Result) != (int)asAST_VERIFY_OK,
			TEXT("fallthrough target must be the next ordered case")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("fallthrough-target"),
			TEXT("detail token must be fallthrough-target")));
	}

	TEST_METHOD(RejectsSealedPublicationCallWithoutResolvedDecl)
	{
		asCASTContext Missing;
		const asASTDeclId Tu = Missing.CreateTranslationUnit("PubCallMissing");
		const asCQualType IntType = Missing.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Missing.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTStmtId Body = Missing.CreateStmt(asAST_STMT_RETURN, Fn, asCSourceRange());
		const asASTExprId Call = Missing.CreateExpr(asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Missing.SetBody(Fn, Body), TEXT("set body")));
		ASSERT_THAT(AreEqual(0, Missing.SetStmtExpr(Body, Call), TEXT("return CALL without callee")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerify(Missing, Result),
			TEXT("HARD NO: unsealed asCASTVerify must succeed without CALL resolvedDecl")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION, asCASTVerifyPublication(Missing, Result),
			TEXT("unsealed publication stays UNSEALED_PUBLICATION, not call-decl")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("unsealed-publication"),
			TEXT("detail token must stay unsealed-publication")));
		ASSERT_THAT(AreEqual(0, Missing.Seal(),
			TEXT("Seal() uses asCASTVerify; CALL without callee must still seal for diagnostics")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, asCASTVerifyPublication(Missing, Result),
			TEXT("sealed publication must refuse CALL without resolvedDecl")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("call-decl"), TEXT("detail token must be call-decl")));

		asCASTContext Present;
		const asASTDeclId PresentTu = Present.CreateTranslationUnit("PubCallPresent");
		const asCQualType PresentInt = Present.InternPrimitive(ttInt, 0);
		const asASTDeclId Callee = Present.CreateDecl(asAST_DECL_FUNCTION, PresentTu, asCSourceRange(), "Callee");
		const asASTDeclId PresentFn = Present.CreateDecl(asAST_DECL_FUNCTION, PresentTu, asCSourceRange(), "F");
		const asASTStmtId PresentBody = Present.CreateStmt(asAST_STMT_RETURN, PresentFn, asCSourceRange());
		const asASTExprId PresentCall = Present.CreateExpr(asAST_EXPR_CALL, PresentInt, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Present.SetBody(PresentFn, PresentBody), TEXT("set present body")));
		ASSERT_THAT(AreEqual(0, Present.SetStmtExpr(PresentBody, PresentCall), TEXT("return CALL")));
		ASSERT_THAT(AreEqual(0, Present.SetResolvedDecl(PresentCall, Callee), TEXT("publication target present")));
		ASSERT_THAT(AreEqual(0, Present.SetExprCallDispatch(
			PresentCall, asAST_CALL_DISPATCH_DIRECT), TEXT("publication dispatch present")));
		ASSERT_THAT(AreEqual(0, Present.Seal(), TEXT("CALL with resolvedDecl must seal")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerifyPublication(Present, Result),
			TEXT("sealed CALL with a resolving target must publish")));
	}

	TEST_METHOD(RejectsSealedPublicationConstructWithoutResolvedDecl)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("PubConstructMissing");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		Context.CreateExpr(asAST_EXPR_CONSTRUCT, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerify(Context, Result),
			TEXT("HARD NO: unsealed CONSTRUCT without resolvedDecl must asCASTVerify")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("CONSTRUCT without callee must still seal for diagnostics")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, asCASTVerifyPublication(Context, Result),
			TEXT("sealed publication must refuse CONSTRUCT without resolvedDecl")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("construct-decl"), TEXT("detail token must be construct-decl")));
	}

	TEST_METHOD(RejectsSealedPublicationDeclRefWithoutResolvedDecl)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("PubDeclRefMissing");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		Context.CreateExpr(asAST_EXPR_DECL_REF, IntType, asAST_VALUE_LVALUE, asCSourceRange());
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerify(Context, Result),
			TEXT("HARD NO: unsealed DECL_REF without resolvedDecl must asCASTVerify")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("DECL_REF without target must still seal for diagnostics")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, asCASTVerifyPublication(Context, Result),
			TEXT("sealed publication must refuse DECL_REF without resolvedDecl")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("declref-decl"), TEXT("detail token must be declref-decl")));
	}
};

#endif
