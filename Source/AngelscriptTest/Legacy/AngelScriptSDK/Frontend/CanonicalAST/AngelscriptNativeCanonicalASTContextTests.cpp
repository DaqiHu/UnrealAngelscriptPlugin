#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_context.h"
#include "source/as_ast_lifetime.h"
#include "source/as_ast_public_view.h"
#include "source/as_ast_verifier.h"
#include "source/as_memory.h"
#include "EndAngelscriptHeaders.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalASTContextTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.Context",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(LifetimeProtocolRecordsHaveDeterministicValueIdentity)
	{
		static_assert(std::is_trivially_copyable<asSASTLifetimeNodeId>::value,
			"snapshot-local lifetime node identity must remain pointer-free value data");
		static_assert(std::is_trivially_copyable<asSASTLifetimeRecord>::value,
			"canonical lifetime records must remain pointer-free value data");

		asSASTLifetimeRecord First;
		First.subjectKind = asAST_LIFETIME_SUBJECT_LOCAL;
		First.subject = asSASTLifetimeNodeId::FromDecl(asASTDeclId(4));
		First.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
		First.actionTarget = asSASTLifetimeNodeId::FromDecl(asASTDeclId(8));
		First.activationPoint = asSASTLifetimeNodeId::FromExpr(asASTExprId(6));
		First.semanticRegion = asSASTLifetimeNodeId::FromStmt(asASTStmtId(3));
		First.phase = asAST_LIFETIME_PHASE_LEXICAL_SCOPE;
		First.supportedExitMask = asAST_LIFETIME_EXIT_NORMAL
			| asAST_LIFETIME_EXIT_RETURN
			| asAST_LIFETIME_EXIT_EXCEPTION;
		First.constructionStep = 2;
		First.order = 5;
		First.completeObjectCommit = false;

		const asSASTLifetimeRecord Same = First;
		ASSERT_THAT(IsTrue(First == Same,
			TEXT("field-identical lifetime records must compare equal")));
		ASSERT_THAT(AreEqual(
			asASTHashLifetimeRecord(First),
			asASTHashLifetimeRecord(Same),
			TEXT("field-identical lifetime records must hash identically")));

		asSASTLifetimeRecord Different = First;
		Different.order = First.order + 1;
		ASSERT_THAT(IsTrue(First != Different,
			TEXT("construction order participates in typed record identity")));
		ASSERT_THAT(IsTrue(
			asASTHashLifetimeRecord(First) != asASTHashLifetimeRecord(Different),
			TEXT("construction order participates in the structural record hash")));
	}

	TEST_METHOD(ContextOwnsAndFreezesRevisionedLifetimeProtocol)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("LifetimeProtocol");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FValue", 0);
		const asASTDeclId ValueClass = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FValue");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(ValueClass, "FValue")));
		const asASTDeclId Destructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, ValueClass, asCSourceRange(), "FValue");
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTStmtId Region = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Function, Region)));
		const asASTDeclId Local = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(), "Local");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Local, ValueType)));
		const asASTStmtId DeclStmt = Context.CreateStmt(
			asAST_STMT_DECL, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetStmtDecl(DeclStmt, Local)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Region, DeclStmt)));

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Target, Local)));
		const asASTExprId Initializer = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, ValueType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Initializer, "1")));
		const asASTExprId Assignment = Context.CreateExpr(
			asAST_EXPR_ASSIGN, ValueType, asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Assignment, "=")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Assignment, Target)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Assignment, Initializer)));
		ASSERT_THAT(AreEqual(0, Context.AddDeclInit(Local, Assignment)));
		const asASTStmtId ExprStmt = Context.CreateStmt(
			asAST_STMT_EXPR, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(ExprStmt, Assignment)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Region, ExprStmt)));

		asSASTLifetimeRecord Record;
		Record.subjectKind = asAST_LIFETIME_SUBJECT_LOCAL;
		Record.subject = asSASTLifetimeNodeId::FromDecl(Local);
		Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
		Record.actionTarget = asSASTLifetimeNodeId::FromDecl(Destructor);
		Record.activationPoint = asSASTLifetimeNodeId::FromExpr(Assignment);
		Record.semanticRegion = asSASTLifetimeNodeId::FromStmt(Region);
		Record.phase = asAST_LIFETIME_PHASE_LEXICAL_SCOPE;
		Record.supportedExitMask = asAST_LIFETIME_EXIT_NORMAL
			| asAST_LIFETIME_EXIT_RETURN
			| asAST_LIFETIME_EXIT_EXCEPTION;
		Record.constructionStep = 0;
		Record.order = 0;
		Record.completeObjectCommit = false;

		const asASTExprId CleanupTarget = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType,
			asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(CleanupTarget, Local)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP, ValueType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Cleanup, "scope-exit")));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Cleanup, Destructor)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, CleanupTarget)));
		const asASTStmtId CleanupStmt = Context.CreateStmt(
			asAST_STMT_EXPR, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(CleanupStmt, Cleanup)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Region, CleanupStmt)));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			Context.AddLifetimeRecord(Record),
			TEXT("Building Sema must be able to author an exact lifetime record")));
		ASSERT_THAT(AreEqual(
			static_cast<asDWORD>(asAST_LIFETIME_PROTOCOL_REVISION),
			Context.GetLifetimeProtocolRevision(),
			TEXT("a fresh Context owns the current lifetime protocol revision")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1),
			Context.GetLifetimeRecordCount(),
			TEXT("the record must be stored by the Context rather than a backend")));
		ASSERT_THAT(IsTrue(*Context.GetLifetimeRecord(0) == Record,
			TEXT("Context storage must preserve every typed lifetime fact")));
		ASSERT_THAT(AreEqual(
			asASTHashLifetimeProtocol(
				asAST_LIFETIME_PROTOCOL_REVISION, &Record, 1),
			Context.GetLifetimeProtocolHash(),
			TEXT("Context protocol identity must use the typed structural hash")));

		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("a current, structurally valid protocol must seal")));
		ASSERT_THAT(IsTrue(Context.IsPublishable()));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_POST_SEAL_MUTATION),
			Context.AddLifetimeRecord(Record),
			TEXT("a Frozen snapshot must reject lifetime protocol mutation")));
	}

	TEST_METHOD(ContextOwnerCannotBeCopiedOrMoved)
	{
		ASSERT_THAT(IsFalse(std::is_copy_constructible<asCASTContext>::value,
			TEXT("copy construction would duplicate ownership of the same arena blocks")));
		ASSERT_THAT(IsFalse(std::is_copy_assignable<asCASTContext>::value,
			TEXT("copy assignment would duplicate ownership of the same arena blocks")));
		ASSERT_THAT(IsFalse(std::is_move_constructible<asCASTContext>::value,
			TEXT("an implicit copy fallback must not make the arena owner appear movable")));
		ASSERT_THAT(IsFalse(std::is_move_assignable<asCASTContext>::value,
			TEXT("an implicit copy fallback must not make the arena owner move-assignable")));
	}

	TEST_METHOD(ContextRejectsPublicOwnerTokensForEveryNodeClass)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("OwnerAdmission");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTStmtId Body = Context.CreateStmt(
			asAST_STMT_RETURN, Fn, asCSourceRange());
		const asASTExprId Value = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL,
			IntType,
			asAST_VALUE_PRVALUE,
			asCSourceRange());

		const asDWORD ForeignOwner = 0xA57u;
		const asASTDeclId ForeignDecl(Fn.value, ForeignOwner);
		const asASTStmtId ForeignStmt(Body.value, ForeignOwner);
		const asASTExprId ForeignExpr(Value.value, ForeignOwner);
		const asASTTypeRef ForeignType(IntType.type.value, ForeignOwner);

		ASSERT_THAT(IsNull(Context.GetDecl(ForeignDecl),
			TEXT("a public declaration ID must not resolve through an internal Context lookup")));
		ASSERT_THAT(IsNull(Context.GetStmt(ForeignStmt),
			TEXT("a public statement ID must not resolve through an internal Context lookup")));
		ASSERT_THAT(IsNull(Context.GetExpr(ForeignExpr),
			TEXT("a public expression ID must not resolve through an internal Context lookup")));
		ASSERT_THAT(IsNull(Context.GetType(ForeignType),
			TEXT("a public type ID must not resolve through an internal Context lookup")));
		ASSERT_THAT(IsFalse(Context.OwnsDecl(ForeignDecl),
			TEXT("OwnsDecl must apply the same owner-token admission rule as GetDecl")));
		ASSERT_THAT(IsFalse(Context.OwnsStmt(ForeignStmt),
			TEXT("OwnsStmt must apply the same owner-token admission rule as GetStmt")));
		ASSERT_THAT(IsFalse(Context.OwnsExpr(ForeignExpr),
			TEXT("OwnsExpr must apply the same owner-token admission rule as GetExpr")));
	}

	TEST_METHOD(ContextLifecycleTransitionsAreOrderedAndOneShot)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("Lifecycle");
		ASSERT_THAT(IsTrue(Tu.IsValid(), TEXT("the lifecycle fixture must begin with a valid graph")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_CONTEXT_BUILDING),
			static_cast<int32>(Context.GetLifecycleState()),
			TEXT("a fresh Context must start in Building")));
		ASSERT_THAT(IsFalse(Context.IsPublishable(),
			TEXT("Building must not satisfy the shared publication admission predicate")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_INVALID_LIFECYCLE_TRANSITION),
			Context.AdvanceLifecycle(
				asAST_CONTEXT_BUILDING,
				asAST_CONTEXT_LIFETIME_PLANNED),
			TEXT("Building must not skip SemaFinalized")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_CONTEXT_BUILDING),
			static_cast<int32>(Context.GetLifecycleState()),
			TEXT("a skipped transition must leave the state unchanged")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			Context.AdvanceLifecycle(
				asAST_CONTEXT_BUILDING,
				asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("Building must advance exactly once to SemaFinalized")));
		ASSERT_THAT(IsTrue(!Context.CreateDecl(
			asAST_DECL_VAR, Tu, asCSourceRange(), "TooLate").IsValid(),
			TEXT("ordinary AST mutation must stop once Sema is finalized")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_INVALID_LIFECYCLE_TRANSITION),
			Context.AdvanceLifecycle(
				asAST_CONTEXT_BUILDING,
				asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("a repeated transition carrying a stale expected state must fail")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_INVALID_LIFECYCLE_TRANSITION),
			Context.AdvanceLifecycle(
				asAST_CONTEXT_SEMA_FINALIZED,
				asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("a repeated target state must fail")));
		ASSERT_THAT(IsFalse(Context.IsPublishable(),
			TEXT("SemaFinalized must not satisfy publication admission")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			Context.AdvanceLifecycle(
				asAST_CONTEXT_SEMA_FINALIZED,
				asAST_CONTEXT_LIFETIME_PLANNED),
			TEXT("SemaFinalized must advance exactly once to LifetimePlanned")));
		ASSERT_THAT(IsFalse(Context.IsPublishable(),
			TEXT("LifetimePlanned must not satisfy publication admission")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_INVALID_LIFECYCLE_TRANSITION),
			Context.AdvanceLifecycle(
				asAST_CONTEXT_SEMA_FINALIZED,
				asAST_CONTEXT_LIFETIME_PLANNED),
			TEXT("a stale planner transition must fail without changing state")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			Context.AdvanceLifecycle(
				asAST_CONTEXT_LIFETIME_PLANNED,
				asAST_CONTEXT_FROZEN_PUBLISHABLE),
			TEXT("LifetimePlanned must freeze only after structural verification")));
		ASSERT_THAT(IsTrue(Context.IsPublishable(),
			TEXT("only Frozen/Publishable satisfies the shared admission predicate")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_INVALID_LIFECYCLE_TRANSITION),
			Context.AdvanceLifecycle(
				asAST_CONTEXT_FROZEN_PUBLISHABLE,
				asAST_CONTEXT_FROZEN_PUBLISHABLE),
			TEXT("Frozen/Publishable is terminal and cannot be repeated")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_POST_SEAL_MUTATION),
			Context.SetDeclName(Tu, "Mutated"),
			TEXT("post-freeze mutation must remain rejected")));
	}

	TEST_METHOD(SealCompatibilityCompletesTheOrderedLifecycleOnlyOnce)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("SealCompatibility");
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			Context.Seal(),
			TEXT("Seal must execute all adjacent lifecycle stages for existing callers")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_CONTEXT_FROZEN_PUBLISHABLE),
			static_cast<int32>(Context.GetLifecycleState()),
			TEXT("successful Seal must end at Frozen/Publishable")));
		ASSERT_THAT(IsTrue(Context.IsPublishable(),
			TEXT("successful Seal must satisfy the shared admission predicate")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_INVALID_LIFECYCLE_TRANSITION),
			Context.Seal(),
			TEXT("Seal must not hide a repeated terminal transition")));
	}

	TEST_METHOD(ArenaOwnershipSealAndForeignIds)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("SampleModule");
		ASSERT_THAT(IsTrue(Tu.IsValid(), TEXT("context should own a translation-unit root")));
		ASSERT_THAT(IsTrue(asASTIsa(Context.GetDecl(Tu), asAST_DECL_TRANSLATION_UNIT), TEXT("TU kind should match")));

		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		ASSERT_THAT(IsTrue(Fn.IsValid(), TEXT("function decl should be allocated from the context arena")));
		ASSERT_THAT(IsTrue(Context.OwnsDecl(Fn), TEXT("creator context should own the decl ID")));
		const asCDecl* const LiveDecl = Context.GetDecl(Fn);
		ASSERT_THAT(IsTrue(Context.ArenaContains(LiveDecl),
			TEXT("decls must live in the context arena, not a unique heap asNEW")));

		asCASTContext Other;
		ASSERT_THAT(IsTrue(!Other.OwnsDecl(Fn), TEXT("foreign context must reject the other snapshot's IDs")));

		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("well-formed graph should seal")));
		ASSERT_THAT(IsTrue(Context.IsPublishable(), TEXT("Seal must reach Frozen/Publishable")));
		const asASTDeclId AfterSeal = Context.CreateDecl(asAST_DECL_VAR, Tu, asCSourceRange(), "X");
		ASSERT_THAT(IsTrue(!AfterSeal.IsValid(), TEXT("post-seal mutation must be rejected")));
		ASSERT_THAT(IsTrue(Context.ArenaContains(LiveDecl),
			TEXT("arena storage must outlive seal")));
		ASSERT_THAT(IsNotNull(Context.GetDecl(Fn),
			TEXT("sealed const GetDecl still traverses the function")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_POST_SEAL_MUTATION, Context.SetBody(Fn, asASTStmtId()),
			TEXT("construction APIs reject post-seal mutation")));
	}

	TEST_METHOD(BulkDestructionClearsTables)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("DestroyMe");
		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Context.GetTranslationUnit(), asCSourceRange(), "F");
		ASSERT_THAT(IsTrue(Context.ArenaContains(Context.GetDecl(Fn)),
			TEXT("function decl must be arena-owned before destruction")));
		ASSERT_THAT(IsTrue(Context.GetArenaUsedBytes() > 0,
			TEXT("arena must report nonzero used bytes while nodes live")));
	}

	TEST_METHOD(GetDeclAndSourceManagerAreConstOnly)
	{
		static_assert(std::is_same<decltype(std::declval<asCASTContext&>().GetDecl(asASTDeclId())), const asCDecl*>::value,
			"asCASTContext::GetDecl must be const-only");
		static_assert(std::is_same<decltype(std::declval<asCASTContext&>().GetStmt(asASTStmtId())), const asCStmt*>::value,
			"asCASTContext::GetStmt must be const-only");
		static_assert(std::is_same<decltype(std::declval<asCASTContext&>().GetExpr(asASTExprId())), const asCExpr*>::value,
			"asCASTContext::GetExpr must be const-only");
		static_assert(std::is_same<decltype(std::declval<asCASTContext&>().GetSourceManager()), const asCSourceManager&>::value,
			"asCASTContext::GetSourceManager must be const-only");
	}

	TEST_METHOD(SnapshotGetContextIsConstOnly)
	{
		static_assert(std::is_same<decltype(std::declval<asCASTSnapshot&>().GetContext()), const asCASTContext*>::value,
			"asCASTSnapshot::GetContext must be const-only");

		asCASTContext* Owned = asNEW(asCASTContext)();
		ASSERT_THAT(IsNotNull(Owned, TEXT("snapshot must own a heap context")));
		Owned->CreateTranslationUnit("Snap");
		ASSERT_THAT(AreEqual(0, Owned->Seal(), TEXT("snapshot context should seal")));
		asCASTSnapshot Snap(Owned, "Snap", "gen-1");
		const asCASTContext* Ctx = Snap.GetContext();
		ASSERT_THAT(IsNotNull(Ctx, TEXT("lease exposes sealed context")));
		ASSERT_THAT(IsTrue(Ctx->IsPublishable(), TEXT("snapshot context is Frozen/Publishable")));
		ASSERT_THAT(IsNotNull(Ctx->GetDecl(Ctx->GetTranslationUnit()), TEXT("const traversal still sees the TU")));
		const asCSourceManager& Manager = Ctx->GetSourceManager();
		ASSERT_THAT(AreEqual((asUINT)0, Manager.GetSectionCount(), TEXT("sealed snapshot SourceManager is const-observable")));
	}

	TEST_METHOD(ConstructionApisWriteAndSealGate)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("CtorApi");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Fn, IntType), TEXT("SetDeclType should write on an unsealed context")));
		const asASTStmtId Body = Context.CreateStmt(asAST_STMT_RETURN, Fn, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Fn, Body), TEXT("SetBody should attach the function body")));
		const asASTExprId Lit = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Lit, "7"), TEXT("SetLiteral should write")));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Lit, 7), TEXT("SetLiteralBits should write")));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Body, Lit), TEXT("SetStmtExpr should attach the return value")));
		const char* Section = "int F() { return 7; }";
		const asASTFileID File = Context.AddSourceSection("ctor.as", asAST_SOURCE_AUTHORED, Section, 21, 0);
		ASSERT_THAT(IsTrue(File != 0, TEXT("AddSourceSection should allocate a FileID while unsealed")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("construction-API graph should verify and seal")));

		const asCDecl* SealedFn = static_cast<const asCASTContext&>(Context).GetDecl(Fn);
		ASSERT_THAT(IsNotNull(SealedFn, TEXT("const GetDecl must still see the function after seal")));
		ASSERT_THAT(IsTrue(SealedFn->body == Body, TEXT("SetBody must survive seal")));
		ASSERT_THAT(AreEqual((asUINT)1, Context.GetSourceManager().GetSectionCount(),
			TEXT("const GetSourceManager must still observe the added section")));

		ASSERT_THAT(AreEqual((int)asAST_VERIFY_POST_SEAL_MUTATION, Context.SetBody(Fn, Body),
			TEXT("SetBody must reject post-seal mutation")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_POST_SEAL_MUTATION, Context.AddDeclChild(Tu, Fn),
			TEXT("AddDeclChild must reject post-seal mutation")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_POST_SEAL_MUTATION, Context.SetTarget(Body, Body),
			TEXT("SetTarget must reject post-seal mutation")));
		ASSERT_THAT(IsTrue(Context.AddSourceSection("ctor2.as", asAST_SOURCE_AUTHORED, Section, 21, 0) == 0,
			TEXT("AddSourceSection must reject post-seal mutation")));
	}

	TEST_METHOD(CallReceiverUsesExplicitExpressionEdge)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("CallReceiver");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTExprId Receiver = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL,
			IntType,
			asAST_VALUE_PRVALUE,
			asCSourceRange());
		const asASTExprId Call = Context.CreateExpr(
			asAST_EXPR_CALL,
			IntType,
			asAST_VALUE_PRVALUE,
			asCSourceRange());

		ASSERT_THAT(AreEqual(0, Context.SetExprReceiver(Call, Receiver),
			TEXT("call receiver must be stored through a dedicated AST edge")));
		const asCExpr* const StoredCall = Context.GetExpr(Call);
		ASSERT_THAT(IsNotNull(StoredCall, TEXT("call expression remains observable through const traversal")));
		ASSERT_THAT(IsTrue(StoredCall->receiver == Receiver,
			TEXT("call receiver must retain the receiver expression ID independently")));
		ASSERT_THAT(AreEqual((asQWORD)0, StoredCall->literalBits,
			TEXT("receiver identity must not consume literalBits")));
		asSAstVerifyResult VerifyResult;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerify(Context, VerifyResult),
			TEXT("a call receiver edge must be structurally verifiable before publication")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("a verified call receiver edge must survive canonical AST sealing")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_POST_SEAL_MUTATION, Context.SetExprReceiver(Call, Receiver),
			TEXT("call receiver mutation must be rejected after seal")));
	}
};

#endif
