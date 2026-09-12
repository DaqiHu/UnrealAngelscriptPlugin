#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheASTBodySidecar.h"

#include "CQTest.h"
#include <cstring>

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_lifetime.h"
#include "source/as_ast_sidecar.h"
#include "frontend/Sema/as_sema.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheASTBodySidecarTests,
	"Angelscript.TestModule.Cache.ASTBodySidecar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SidecarAdmissionRequiresBuildingInputAndFrozenPublishableOutput)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		Sema.ActOnTranslationUnit("SidecarLifecycle");
		asSASTSidecarHeader Header;
		Header.functionKey = "SidecarLifecycle";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;

		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("Sidecar encode must reject Building")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_BUILDING, asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("fixture must reach SemaFinalized")));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("Sidecar encode must reject SemaFinalized")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_SEMA_FINALIZED, asAST_CONTEXT_LIFETIME_PLANNED),
			TEXT("fixture must reach LifetimePlanned")));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("Sidecar encode must reject LifetimePlanned")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_LIFETIME_PLANNED, asAST_CONTEXT_FROZEN_PUBLISHABLE),
			TEXT("fixture must reach Frozen/Publishable")));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("Sidecar encode must accept only Frozen/Publishable")));

		asCASTContext StaleDecodeTarget;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK,
			StaleDecodeTarget.AdvanceLifecycle(
				asAST_CONTEXT_BUILDING, asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("decode rejection fixture must leave Building")));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTDecodeSidecar(
				StaleDecodeTarget, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("Sidecar decode must reject every non-Building destination")));
		ASSERT_THAT(IsFalse(StaleDecodeTarget.IsPublishable(),
			TEXT("rejected decode must not publish an intermediate Context")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("Sidecar decode must accept a fresh Building destination")));
		ASSERT_THAT(IsTrue(Restored.IsPublishable(),
			TEXT("successful decode must complete the same Frozen/Publishable lifecycle")));
	}

	TEST_METHOD(RecordKindVersionRoundTripAndAbsenceRules)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		Sema.ActOnTranslationUnit("Mod");
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("sample AST should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "Mod.F";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode sealed AST sidecar")));
		ASSERT_THAT(IsTrue(Bytes.GetLength() > 0, TEXT("sidecar payload is not absence")));

		asCASTContext Restored;
		asSASTSidecarHeader Expected = Header;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Expected, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("retain restore performs zero re-Sema")));
		ASSERT_THAT(IsTrue(Restored.IsPublishable(), TEXT("retain restore publishes a Frozen/Publishable AST")));

		asSASTSidecarHeader WrongKey = Header;
		WrongKey.functionKey = "Other.F";
		asCASTContext Mismatch;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_FUNCTION_KEY_MISMATCH,
			asCASTDecodeSidecar(Mismatch, WrongKey, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("FunctionKey mismatch fails closed")));

		asSASTSidecarHeader WrongProfile = Header;
		WrongProfile.profile = "Android";
		asCASTContext ProfileMismatch;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_PROFILE_MISMATCH,
			asCASTDecodeSidecar(ProfileMismatch, WrongProfile, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("profile mismatch fails closed")));

		asCASTContext Empty;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_EMPTY_NOT_ABSENCE,
			asCASTDecodeSidecar(Empty, Header, Bytes.AddressOf(), 0),
			TEXT("empty payload is not canonical absence")));

		const asBYTE Unknown[] = { 'C', 'A', 'S', 'T', 99, 0, 0, 0 };
		asCASTContext UnknownKind;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNKNOWN_KIND,
			asCASTDecodeSidecar(UnknownKind, Header, Unknown, 8),
			TEXT("unknown node/type/schema kind fails closed")));
	}

	TEST_METHOD(SidecarV13RoundTripPreservesLoopBodyEdgeSafePoint)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit(
			"SidecarLoopBodySafePoint");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Function, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(
			Function, "SidecarLoopBodySafePoint.F()")));
		const asASTStmtId FunctionBody = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		const asASTStmtId Loop = Context.CreateStmt(
			asAST_STMT_WHILE, Function, asCSourceRange());
		const asASTStmtId LoopBody = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		const asASTExprId Condition = Context.CreateExpr(
			asAST_EXPR_BOOL_LITERAL, BoolType, asAST_VALUE_PRVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Function, FunctionBody)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(FunctionBody, Loop)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, LoopBody)));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Loop, Condition)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Condition, 1)));
		ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
			FunctionBody, asAST_SAFEPOINT_FUNCTION_ENTRY)));
		ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
			Loop, asAST_SAFEPOINT_LOOP_BACKEDGE)));
		ASSERT_THAT(AreEqual(0, Context.SetLoopBodySafePointRole(
			Loop, asAST_SAFEPOINT_LOOP_ENTRY)));
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asSASTSidecarHeader Header;
		Header.functionKey = "SidecarLoopBodySafePoint.F()";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes)));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(
				Restored, Header, Bytes.AddressOf(), Bytes.GetLength())));
		const asCStmt* const RestoredLoop = Restored.GetStmt(Loop);
		const asCStmt* const RestoredBody = Restored.GetStmt(LoopBody);
		ASSERT_THAT(IsNotNull(RestoredLoop));
		ASSERT_THAT(IsNotNull(RestoredBody));
		ASSERT_THAT(IsTrue(RestoredLoop != nullptr
			&& RestoredLoop->safePointRole == asAST_SAFEPOINT_LOOP_BACKEDGE
			&& RestoredLoop->loopBodySafePointRole
				== asAST_SAFEPOINT_LOOP_ENTRY));
		ASSERT_THAT(IsTrue(RestoredBody != nullptr
			&& RestoredBody->safePointRole == asAST_SAFEPOINT_NONE
			&& RestoredBody->loopBodySafePointRole == asAST_SAFEPOINT_NONE));

		asCArray<asBYTE> Reencoded;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Restored, Header, Reencoded)));
		ASSERT_THAT(AreEqual(Bytes.GetLength(), Reencoded.GetLength()));
		ASSERT_THAT(IsTrue(std::memcmp(
			Bytes.AddressOf(), Reencoded.AddressOf(), Bytes.GetLength()) == 0,
			TEXT("V13 loop-edge safe-point round trip remains byte exact")));
	}

	TEST_METHOD(SidecarV13RoundTripPreservesCompileOutCallRewriteAuthority)
	{
		// CTA-S174 schema gate: CALL_REWRITE is append-only semantic authority.
		// Persistence must retain its exact disposition declaration, final value
		// edge and source-to-formal/receiver provenance without Runtime pointers.
		const char Source[] = "Erase(9); Keep(7, 9); Probe.Chain(5);";
		asCASTContext Context;
		const asASTFileID File = Context.AddSourceSection(
			"CallRewriteSidecar.as",
			asAST_SOURCE_AUTHORED,
			Source,
			sizeof(Source) - 1,
			0);
		ASSERT_THAT(IsTrue(File != 0));
		const asCSourceRange Range = Context.GetSourceManager().MakeRange(
			File, 0, sizeof(Source) - 1);
		const asASTDeclId Tu =
			Context.CreateTranslationUnit("CallRewriteSidecar");
		const asCQualType VoidType = Context.InternPrimitive(ttVoid, 0);
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType ProbeType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FCallRewriteSidecarProbe", 0);

		const asASTDeclId Erase = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, Range, "Erase");
		const asASTDeclId EraseValue = Context.CreateDecl(
			asAST_DECL_PARAM, Erase, Range, "Value");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Erase, VoidType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(EraseValue, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclTraits(
			Erase, asAST_TRAIT_COMPILE_OUT_ENTIRELY)));

		const asASTDeclId Keep = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, Range, "Keep");
		const asASTDeclId First = Context.CreateDecl(
			asAST_DECL_PARAM, Keep, Range, "First");
		const asASTDeclId Ignored = Context.CreateDecl(
			asAST_DECL_PARAM, Keep, Range, "Ignored");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Keep, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(First, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Ignored, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclTraits(
			Keep, asAST_TRAIT_REPLACE_WITH_FIRST_PARAM)));

		const asASTDeclId ProbeClass = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, Range, "FCallRewriteSidecarProbe");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(
			ProbeClass, "FCallRewriteSidecarProbe")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(ProbeClass, ProbeType)));
		const asASTDeclId Chain = Context.CreateDecl(
			asAST_DECL_METHOD, ProbeClass, Range, "Chain");
		const asASTDeclId ChainIgnored = Context.CreateDecl(
			asAST_DECL_PARAM, Chain, Range, "Ignored");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Chain, ProbeType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(ChainIgnored, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclTraits(
			Chain, asAST_TRAIT_COMPILE_OUT_AS_METHOD_CHAIN)));

		const asASTDeclId Entry = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, Range, "Entry");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Entry, VoidType)));
		const asASTDeclId Probe = Context.CreateDecl(
			asAST_DECL_VAR, Entry, Range, "Probe");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Probe, ProbeType)));
		const asASTStmtId Body = Context.CreateStmt(
			asAST_STMT_BLOCK, Entry, Range);
		ASSERT_THAT(AreEqual(0, Context.SetBody(Entry, Body)));

		const asASTExprId Entirely = Context.CreateExpr(
			asAST_EXPR_CALL_REWRITE, VoidType, asAST_VALUE_PRVALUE, Range);
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Entirely, Erase)));
		ASSERT_THAT(AreEqual(0,
			Context.SetLiteral(Entirely, "compile-out-entirely")));

		const asASTExprId FirstValue = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, Range);
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(FirstValue, 7)));
		const asASTExprId FirstRewrite = Context.CreateExpr(
			asAST_EXPR_CALL_REWRITE, IntType, asAST_VALUE_PRVALUE, Range);
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(FirstRewrite, Keep)));
		ASSERT_THAT(AreEqual(0,
			Context.SetLiteral(FirstRewrite, "compile-out-first-param")));
		ASSERT_THAT(AreEqual(0,
			Context.AddExprChild(FirstRewrite, FirstValue)));
		asSASTCallArgument FirstRecord;
		FirstRecord.expression = FirstValue;
		FirstRecord.formal = First;
		FirstRecord.formalType = IntType;
		FirstRecord.range = Range;
		FirstRecord.formalIndex = 0;
		FirstRecord.sourceOrdinal = 0;
		FirstRecord.origin = asAST_CALL_ARGUMENT_POSITIONAL;
		ASSERT_THAT(AreEqual(0,
			Context.AddCallArgument(FirstRewrite, FirstRecord)));

		const asASTExprId Receiver = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ProbeType, asAST_VALUE_LVALUE, Range);
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Receiver, Probe)));
		const asASTExprId MethodRewrite = Context.CreateExpr(
			asAST_EXPR_CALL_REWRITE, ProbeType, asAST_VALUE_LVALUE, Range);
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(MethodRewrite, Chain)));
		ASSERT_THAT(AreEqual(0,
			Context.SetLiteral(MethodRewrite, "compile-out-method-chain")));
		ASSERT_THAT(AreEqual(0, Context.SetExprReceiver(
			MethodRewrite, Receiver)));
		ASSERT_THAT(AreEqual(0,
			Context.AddExprChild(MethodRewrite, Receiver)));
		asSASTCallArgument ReceiverRecord;
		ReceiverRecord.expression = Receiver;
		ReceiverRecord.formalType = ProbeType;
		ReceiverRecord.range = Range;
		ReceiverRecord.origin = asAST_CALL_ARGUMENT_IMPLICIT_RECEIVER;
		ASSERT_THAT(AreEqual(0,
			Context.AddCallArgument(MethodRewrite, ReceiverRecord)));

		const asASTExprId Rewrites[] = {
			Entirely, FirstRewrite, MethodRewrite };
		for (const asASTExprId Rewrite : Rewrites)
		{
			const asASTStmtId Statement = Context.CreateStmt(
				asAST_STMT_EXPR, Entry, Range);
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Statement, Rewrite)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Body, Statement)));
		}
		const int SealStatus = Context.Seal();
		ASSERT_THAT(AreEqual(0, SealStatus,
			TEXT("all three CALL_REWRITE dispositions must publish")));
		if (SealStatus != 0)
		{
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<asUINT>(13),
			asAST_SIDECAR_SCHEMA_VERSION,
			TEXT("CALL_REWRITE remains admitted by Sidecar schema V13")));
		asSASTSidecarHeader Header;
		Header.functionKey = "CallRewriteSidecar.Entry";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes)));
		asCASTContext Restored;
		const int DecodeStatus = asCASTDecodeSidecar(
			Restored, Header, Bytes.AddressOf(), Bytes.GetLength());
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, DecodeStatus));
		if (DecodeStatus != asAST_SIDECAR_OK)
		{
			return;
		}

		const asCExpr* const RestoredEntirely = Restored.GetExpr(Entirely);
		const asCExpr* const RestoredFirst = Restored.GetExpr(FirstRewrite);
		const asCExpr* const RestoredMethod = Restored.GetExpr(MethodRewrite);
		const asCDecl* const RestoredErase = Restored.GetDecl(Erase);
		const asCDecl* const RestoredKeep = Restored.GetDecl(Keep);
		const asCDecl* const RestoredChain = Restored.GetDecl(Chain);
		ASSERT_THAT(IsTrue(
			RestoredEntirely != nullptr
				&& RestoredEntirely->kind == asAST_EXPR_CALL_REWRITE
				&& RestoredEntirely->resolvedDecl == Erase
				&& RestoredEntirely->literal.Equals("compile-out-entirely")
				&& RestoredErase != nullptr
				&& (RestoredErase->traits
					& asAST_TRAIT_COMPILE_OUT_ENTIRELY) != 0));
		ASSERT_THAT(IsTrue(
			RestoredFirst != nullptr
				&& RestoredFirst->kind == asAST_EXPR_CALL_REWRITE
				&& RestoredFirst->resolvedDecl == Keep
				&& RestoredFirst->children.GetLength() == 1
				&& RestoredFirst->callArguments.GetLength() == 1
				&& RestoredFirst->callArguments[0].expression == FirstValue
				&& RestoredFirst->callArguments[0].formal == First
				&& RestoredFirst->callArguments[0].formalIndex == 0
				&& RestoredFirst->callArguments[0].sourceOrdinal == 0
				&& RestoredKeep != nullptr
				&& (RestoredKeep->traits
					& asAST_TRAIT_REPLACE_WITH_FIRST_PARAM) != 0));
		ASSERT_THAT(IsTrue(
			RestoredMethod != nullptr
				&& RestoredMethod->kind == asAST_EXPR_CALL_REWRITE
				&& RestoredMethod->resolvedDecl == Chain
				&& RestoredMethod->receiver == Receiver
				&& RestoredMethod->children.GetLength() == 1
				&& RestoredMethod->callArguments.GetLength() == 1
				&& RestoredMethod->callArguments[0].expression == Receiver
				&& RestoredMethod->callArguments[0].origin
					== asAST_CALL_ARGUMENT_IMPLICIT_RECEIVER
				&& RestoredChain != nullptr
				&& (RestoredChain->traits
					& asAST_TRAIT_COMPILE_OUT_AS_METHOD_CHAIN) != 0));

		asCArray<asBYTE> RestoredBytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Restored, Header, RestoredBytes)));
		ASSERT_THAT(IsTrue(Bytes.GetLength() == RestoredBytes.GetLength()
				&& (Bytes.GetLength() == 0 || std::memcmp(
					Bytes.AddressOf(), RestoredBytes.AddressOf(),
					Bytes.GetLength()) == 0),
			TEXT("CALL_REWRITE V13 decode/encode must be byte exact")));
	}

	TEST_METHOD(SidecarCallArgumentsRoundTripDeterministicallyAndRejectCorruption)
	{
		// Production mutation caught: V8-style child-only persistence can restore
		// values and callee shape while losing source-to-formal binding authority.
		// No decoder may reconstruct origin/type/range facts from literals or child
		// position. Keep the four origins in one fixture so this remains a schema
		// gate rather than four near-identical CQTest processes.
		const char Source[] = "Pack(1, B:7, C:3, Hidden:4)";
		asCASTContext Context;
		const asASTFileID File = Context.AddSourceSection(
			"CallArgumentSidecar.as",
			asAST_SOURCE_AUTHORED,
			Source,
			sizeof(Source) - 1,
			0);
		ASSERT_THAT(IsTrue(File != 0,
			TEXT("call-argument fixture must own its authored source")));
		const asCSourceManager& Sources = Context.GetSourceManager();
		const asCSourceRange CallRange =
			Sources.MakeRange(File, 0, sizeof(Source) - 1);
		const asCSourceRange PositionalRange = Sources.MakeRange(File, 5, 6);
		const asCSourceRange NamedRange = Sources.MakeRange(File, 10, 11);
		const asCSourceRange DefaultRange = Sources.MakeRange(File, 15, 16);
		const asCSourceRange HiddenRange = Sources.MakeRange(File, 25, 26);
		const asASTDeclId Tu = Context.CreateTranslationUnit("CallArgumentSidecar");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Callee = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, CallRange, "Pack");
		const asASTDeclId A = Context.CreateDecl(
			asAST_DECL_PARAM, Callee, PositionalRange, "A");
		const asASTDeclId B = Context.CreateDecl(
			asAST_DECL_PARAM, Callee, NamedRange, "B");
		const asASTDeclId C = Context.CreateDecl(
			asAST_DECL_PARAM, Callee, DefaultRange, "C");
		const asASTDeclId Hidden = Context.CreateDecl(
			asAST_DECL_PARAM, Callee, HiddenRange, "Hidden");
		const asASTDeclId Entry = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, CallRange, "Entry");
		const asASTStmtId Return = Context.CreateStmt(
			asAST_STMT_RETURN, Entry, CallRange);
		const asASTExprId PositionalA = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
			PositionalRange);
		const asASTExprId NamedB = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
			NamedRange);
		const asASTExprId DefaultC = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
			DefaultRange);
		const asASTExprId HiddenValue = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
			HiddenRange);
		const asASTExprId Call = Context.CreateExpr(
			asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, CallRange);
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Callee, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(A, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(B, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(C, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Hidden, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Entry, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDefaultArg(C, "3")));
		ASSERT_THAT(AreEqual(0, Context.SetOrigin(Hidden, "hidden")));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(PositionalA, 1)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(NamedB, 7)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(DefaultC, 3)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(HiddenValue, 4)));
		// CALL children and records use the Runtime reverse-formal order.
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Call, HiddenValue)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Call, DefaultC)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Call, NamedB)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Call, PositionalA)));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Call, Callee)));
		ASSERT_THAT(AreEqual(0, Context.SetExprCallDispatch(
			Call, asAST_CALL_DISPATCH_DIRECT)));

		asSASTCallArgument HiddenRecord;
		HiddenRecord.expression = HiddenValue;
		HiddenRecord.formal = Hidden;
		HiddenRecord.formalType = IntType;
		HiddenRecord.range = HiddenRange;
		HiddenRecord.formalIndex = 3;
		HiddenRecord.origin = asAST_CALL_ARGUMENT_HIDDEN;
		HiddenRecord.sourceOrdinal = asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL;
		ASSERT_THAT(AreEqual(0, Context.AddCallArgument(Call, HiddenRecord)));
		asSASTCallArgument CRecord;
		CRecord.expression = DefaultC;
		CRecord.formal = C;
		CRecord.formalType = IntType;
		CRecord.range = DefaultRange;
		CRecord.formalIndex = 2;
		CRecord.origin = asAST_CALL_ARGUMENT_DEFAULT;
		CRecord.sourceOrdinal = asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL;
		ASSERT_THAT(AreEqual(0, Context.AddCallArgument(Call, CRecord)));
		asSASTCallArgument BRecord;
		BRecord.expression = NamedB;
		BRecord.formal = B;
		BRecord.formalType = IntType;
		BRecord.range = NamedRange;
		BRecord.formalIndex = 1;
		BRecord.origin = asAST_CALL_ARGUMENT_NAMED;
		BRecord.authoredName = "B";
		BRecord.sourceOrdinal = 1;
		ASSERT_THAT(AreEqual(0, Context.AddCallArgument(Call, BRecord)));
		asSASTCallArgument ARecord;
		ARecord.expression = PositionalA;
		ARecord.formal = A;
		ARecord.formalType = IntType;
		ARecord.range = PositionalRange;
		ARecord.formalIndex = 0;
		ARecord.origin = asAST_CALL_ARGUMENT_POSITIONAL;
		ARecord.sourceOrdinal = 0;
		ASSERT_THAT(AreEqual(0, Context.AddCallArgument(Call, ARecord)));
		ASSERT_THAT(AreEqual(0, Context.SetBody(Entry, Return)));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Return, Call)));

		// An indirect funcdef invocation resolves through a VAR/PARAM declaration,
		// which does not own the invocation ParamDecl list. V9 must therefore keep
		// its pointer-free formal ordinal/type authority with an invalid formal ID.
		const asCQualType CallbackType = Context.InternNamedType(
			asAST_TYPE_FUNCDEF, "FCallback(int)", 0);
		const asASTDeclId IndirectEntry = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, CallRange, "IndirectEntry");
		const asASTDeclId Callback = Context.CreateDecl(
			asAST_DECL_PARAM, IndirectEntry, CallRange, "Callback");
		const asASTStmtId IndirectReturn = Context.CreateStmt(
			asAST_STMT_RETURN, IndirectEntry, CallRange);
		const asASTExprId IndirectArg = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
			PositionalRange);
		const asASTExprId IndirectCall = Context.CreateExpr(
			asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, CallRange);
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(IndirectEntry, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Callback, CallbackType)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(IndirectArg, 9)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(IndirectCall, IndirectArg)));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(IndirectCall, Callback)));
		ASSERT_THAT(AreEqual(0, Context.SetExprCallDispatch(
			IndirectCall, asAST_CALL_DISPATCH_INDIRECT)));
		asSASTCallArgument IndirectRecord;
		IndirectRecord.expression = IndirectArg;
		IndirectRecord.formalType = IntType;
		IndirectRecord.range = PositionalRange;
		IndirectRecord.formalIndex = 0;
		IndirectRecord.sourceOrdinal = 0;
		IndirectRecord.origin = asAST_CALL_ARGUMENT_POSITIONAL;
		ASSERT_THAT(AreEqual(0,
			Context.AddCallArgument(IndirectCall, IndirectRecord)));
		ASSERT_THAT(AreEqual(0,
			Context.SetBody(IndirectEntry, IndirectReturn)));
		ASSERT_THAT(AreEqual(0,
			Context.SetStmtExpr(IndirectReturn, IndirectCall)));
		const int SealStatus = Context.Seal();
		ASSERT_THAT(AreEqual(0, SealStatus,
			TEXT("call-argument sidecar fixture must publish")));
		if (SealStatus != 0)
		{
			return;
		}
		asSASTSidecarHeader Header;
		Header.functionKey = "CallArgumentSidecar.Entry";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes)));
		asCArray<asBYTE> RepeatedBytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, RepeatedBytes)));
		ASSERT_THAT(IsTrue(Bytes.GetLength() == RepeatedBytes.GetLength()
				&& (Bytes.GetLength() == 0 || std::memcmp(
					Bytes.AddressOf(), RepeatedBytes.AddressOf(),
					Bytes.GetLength()) == 0),
			TEXT("repeated V13 encode must be byte deterministic")));

		asCASTContext Restored;
		const int DecodeStatus =
			asCASTDecodeSidecar(
				Restored, Header, Bytes.AddressOf(), Bytes.GetLength());
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, DecodeStatus,
			TEXT("V13 decode must restore formal and CallArgument authority before Seal")));
		if (DecodeStatus != asAST_SIDECAR_OK)
		{
			return;
		}
		const asCDecl* const RestoredADecl = Restored.GetDecl(A);
		const asCDecl* const RestoredBDecl = Restored.GetDecl(B);
		const asCDecl* const RestoredCDecl = Restored.GetDecl(C);
		const asCDecl* const RestoredHiddenDecl = Restored.GetDecl(Hidden);
		ASSERT_THAT(IsTrue(RestoredADecl != nullptr
			&& RestoredBDecl != nullptr
			&& RestoredCDecl != nullptr
			&& RestoredHiddenDecl != nullptr
			&& RestoredADecl->formalIndex == 0
			&& RestoredBDecl->formalIndex == 1
			&& RestoredCDecl->formalIndex == 2
			&& RestoredHiddenDecl->formalIndex == 3,
			TEXT("V13 must round-trip sealed ParamDecl formal ordinals independently of child position")));
		const asCExpr* const RestoredCall = Restored.GetExpr(Call);
		ASSERT_THAT(IsNotNull(RestoredCall,
			TEXT("Sidecar restore must preserve the call expression identity")));
		if (RestoredCall != nullptr)
		{
			ASSERT_THAT(AreEqual(static_cast<asUINT>(4),
				RestoredCall->callArguments.GetLength(),
				TEXT("Sidecar must preserve every formal binding record")));
			if (RestoredCall->callArguments.GetLength() == 4)
			{
				const asSASTCallArgument& RestoredHidden =
					RestoredCall->callArguments[0];
				const asSASTCallArgument& RestoredC =
					RestoredCall->callArguments[1];
				const asSASTCallArgument& RestoredB =
					RestoredCall->callArguments[2];
				const asSASTCallArgument& RestoredA =
					RestoredCall->callArguments[3];
				ASSERT_THAT(IsTrue(
					RestoredHidden.expression == HiddenValue
						&& RestoredHidden.formal == Hidden
						&& RestoredHidden.formalType == IntType
						&& RestoredHidden.formalIndex == 3
						&& RestoredHidden.origin == asAST_CALL_ARGUMENT_HIDDEN
						&& RestoredHidden.sourceOrdinal
							== asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL
						&& RestoredHidden.range.begin.offset
							== HiddenRange.begin.offset
						&& RestoredHidden.range.end.offset
							== HiddenRange.end.offset,
					TEXT("hidden argument authority must round-trip exactly")));
				ASSERT_THAT(IsTrue(
					RestoredC.expression == DefaultC
						&& RestoredC.formal == C
						&& RestoredC.formalType == IntType
						&& RestoredC.formalIndex == 2
						&& RestoredC.origin == asAST_CALL_ARGUMENT_DEFAULT
						&& RestoredC.sourceOrdinal
							== asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL
						&& RestoredC.range.begin.offset
							== DefaultRange.begin.offset
						&& RestoredC.range.end.offset
							== DefaultRange.end.offset,
					TEXT("default argument authority must round-trip exactly")));
				ASSERT_THAT(IsTrue(
					RestoredB.expression == NamedB
						&& RestoredB.formal == B
						&& RestoredB.formalType == IntType
						&& RestoredB.formalIndex == 1
						&& RestoredB.origin == asAST_CALL_ARGUMENT_NAMED
						&& RestoredB.authoredName.Equals("B")
						&& RestoredB.sourceOrdinal == 1
						&& RestoredB.range.begin.offset == NamedRange.begin.offset
						&& RestoredB.range.end.offset == NamedRange.end.offset,
					TEXT("named B provenance must round-trip exactly")));
				ASSERT_THAT(IsTrue(
					RestoredA.expression == PositionalA
						&& RestoredA.formal == A
						&& RestoredA.formalType == IntType
						&& RestoredA.formalIndex == 0
						&& RestoredA.origin == asAST_CALL_ARGUMENT_POSITIONAL
						&& RestoredA.authoredName.GetLength() == 0
						&& RestoredA.sourceOrdinal == 0
						&& RestoredA.range.begin.offset
							== PositionalRange.begin.offset
						&& RestoredA.range.end.offset
							== PositionalRange.end.offset,
						TEXT("positional A provenance must round-trip exactly")));
				}
			}
			const asCExpr* const RestoredIndirectCall =
				Restored.GetExpr(IndirectCall);
			ASSERT_THAT(IsNotNull(RestoredIndirectCall,
				TEXT("V9 restore must preserve the indirect funcdef call")));
			if (RestoredIndirectCall != nullptr)
			{
				ASSERT_THAT(AreEqual(
					static_cast<asUINT>(asAST_CALL_DISPATCH_INDIRECT),
					static_cast<asUINT>(RestoredIndirectCall->callDispatch),
					TEXT("indirect funcdef dispatch must round-trip")));
				ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
					RestoredIndirectCall->callArguments.GetLength(),
					TEXT("indirect funcdef call must retain one formal record")));
				if (RestoredIndirectCall->callArguments.GetLength() == 1)
				{
					const asSASTCallArgument& RestoredIndirect =
						RestoredIndirectCall->callArguments[0];
					ASSERT_THAT(IsTrue(
						RestoredIndirect.expression == IndirectArg
							&& !RestoredIndirect.formal.IsValid()
							&& RestoredIndirect.formalType == IntType
							&& RestoredIndirect.formalIndex == 0
							&& RestoredIndirect.origin
								== asAST_CALL_ARGUMENT_POSITIONAL
							&& RestoredIndirect.sourceOrdinal == 0
							&& RestoredIndirect.range.begin.offset
								== PositionalRange.begin.offset
							&& RestoredIndirect.range.end.offset
								== PositionalRange.end.offset,
						TEXT("indirect funcdef formal ordinal/type provenance must round-trip without a Runtime owner")));
				}
			}
			ASSERT_THAT(AreEqual(static_cast<asUINT>(13),
				asAST_SIDECAR_SCHEMA_VERSION,
				TEXT("V13 retains V12 CALL_REWRITE and V11 CallArgument/formal authority")));

		asCArray<asBYTE> RestoredBytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Restored, Header, RestoredBytes)));
		ASSERT_THAT(IsTrue(Bytes.GetLength() == RestoredBytes.GetLength()
				&& (Bytes.GetLength() == 0 || std::memcmp(
					Bytes.AddressOf(), RestoredBytes.AddressOf(),
					Bytes.GetLength()) == 0),
			TEXT("decode then encode must preserve exact V13 bytes")));

			// The V9 trailer ends with the indirect funcdef's positional record. Its
			// fixed-size payload is 48 bytes because authoredName and formal are empty.
			// These mutations pin the indirect admission path without duplicating the
			// whole sidecar decoder here.
		ASSERT_THAT(IsTrue(Bytes.GetLength() > 48,
			TEXT("V9 fixture must contain one complete trailing record")));
		if (Bytes.GetLength() <= 48)
		{
			return;
		}
		const asUINT LastRecordOffset = Bytes.GetLength() - 48;
			auto DecodeMustFailClosed = [&](const asCArray<asBYTE>& Corrupt,
			const TCHAR* Message)
		{
			asCASTContext Candidate;
			ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
				asCASTDecodeSidecar(
					Candidate, Header, Corrupt.AddressOf(), Corrupt.GetLength()),
				Message));
			ASSERT_THAT(IsFalse(Candidate.IsPublishable(),
				TEXT("corrupt CallArgument bytes must not publish a partial graph")));
			ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
				Candidate.GetExprCount(),
				TEXT("failed V13 decode must discard the complete candidate graph")));
		};

		asCArray<asBYTE> BadExpression = Bytes;
		BadExpression[LastRecordOffset] = 0;
		BadExpression[LastRecordOffset + 1] = 0;
		BadExpression[LastRecordOffset + 2] = 0;
		BadExpression[LastRecordOffset + 3] = 0;
		DecodeMustFailClosed(BadExpression,
			TEXT("dangling CallArgument expression must fail closed"));

		asCArray<asBYTE> BadFormalType = Bytes;
		BadFormalType[LastRecordOffset + 8] = 0xff;
		BadFormalType[LastRecordOffset + 9] = 0xff;
		BadFormalType[LastRecordOffset + 10] = 0xff;
		BadFormalType[LastRecordOffset + 11] = 0xff;
		DecodeMustFailClosed(BadFormalType,
			TEXT("unknown CallArgument formal type must fail closed"));

		asCArray<asBYTE> BadRange = Bytes;
		BadRange[LastRecordOffset + 16] = 0xff;
		BadRange[LastRecordOffset + 17] = 0xff;
		BadRange[LastRecordOffset + 18] = 0xff;
		BadRange[LastRecordOffset + 19] = 0xff;
		DecodeMustFailClosed(BadRange,
			TEXT("foreign CallArgument source range must fail closed"));

		asCArray<asBYTE> BadOrigin = Bytes;
		BadOrigin[BadOrigin.GetLength() - 4] = 0xff;
		BadOrigin[BadOrigin.GetLength() - 3] = 0xff;
		BadOrigin[BadOrigin.GetLength() - 2] = 0xff;
		BadOrigin[BadOrigin.GetLength() - 1] = 0xff;
		DecodeMustFailClosed(BadOrigin,
			TEXT("unknown CallArgument origin must fail closed"));

		asCArray<asBYTE> Truncated = Bytes;
		Truncated.SetLength(Truncated.GetLength() - 1);
		DecodeMustFailClosed(Truncated,
			TEXT("truncated CallArgument record must fail closed"));

		asCArray<asSASTSidecarRecordId> BaselineRecords;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTCollectFunctionRecords(Restored, "Win64", BaselineRecords),
			TEXT("collect baseline V9 function identity")));
		asCExpr* const MutatedCall =
			const_cast<asCExpr*>(Restored.GetExpr(Call));
		ASSERT_THAT(IsNotNull(MutatedCall,
			TEXT("identity mutation must find the restored call")));
		if (MutatedCall == nullptr || MutatedCall->callArguments.GetLength() != 4)
		{
			return;
		}
		MutatedCall->callArguments[2].origin = asAST_CALL_ARGUMENT_POSITIONAL;
		MutatedCall->callArguments[2].authoredName = "";
		asCArray<asSASTSidecarRecordId> MutatedRecords;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTCollectFunctionRecords(Restored, "Win64", MutatedRecords),
			TEXT("collect call-provenance mutation identity")));
		const asSASTSidecarRecordId* BaselineEntry = nullptr;
		const asSASTSidecarRecordId* MutatedEntry = nullptr;
		for (asUINT Index = 0; Index < BaselineRecords.GetLength(); ++Index)
		{
			if (BaselineRecords[Index].functionKey.Equals("Entry"))
			{
				BaselineEntry = &BaselineRecords[Index];
				break;
			}
		}
		for (asUINT Index = 0; Index < MutatedRecords.GetLength(); ++Index)
		{
			if (MutatedRecords[Index].functionKey.Equals("Entry"))
			{
				MutatedEntry = &MutatedRecords[Index];
				break;
			}
		}
			ASSERT_THAT(IsTrue(BaselineEntry != nullptr && MutatedEntry != nullptr
					&& !BaselineEntry->contentHash.Equals(
						MutatedEntry->contentHash),
				TEXT("CallArgument provenance must participate in function identity")));

			// Restore the direct record, then prove that the pointer-free indirect
			// record independently participates in its owning function identity.
			MutatedCall->callArguments[2].origin = asAST_CALL_ARGUMENT_NAMED;
			MutatedCall->callArguments[2].authoredName = "B";
			asCExpr* const MutatedIndirectCall =
				const_cast<asCExpr*>(Restored.GetExpr(IndirectCall));
			ASSERT_THAT(IsNotNull(MutatedIndirectCall,
				TEXT("indirect identity mutation must find the restored call")));
			if (MutatedIndirectCall == nullptr
				|| MutatedIndirectCall->callArguments.GetLength() != 1)
			{
				return;
			}
			MutatedIndirectCall->callArguments[0].range.begin.offset -= 1;
			asCArray<asSASTSidecarRecordId> IndirectMutatedRecords;
			ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
				asCASTCollectFunctionRecords(
					Restored, "Win64", IndirectMutatedRecords),
				TEXT("collect indirect funcdef provenance mutation identity")));
			const asSASTSidecarRecordId* BaselineIndirect = nullptr;
			const asSASTSidecarRecordId* MutatedIndirect = nullptr;
			for (asUINT Index = 0; Index < BaselineRecords.GetLength(); ++Index)
			{
				if (BaselineRecords[Index].functionKey.Equals("IndirectEntry"))
				{
					BaselineIndirect = &BaselineRecords[Index];
					break;
				}
			}
			for (asUINT Index = 0; Index < IndirectMutatedRecords.GetLength(); ++Index)
			{
				if (IndirectMutatedRecords[Index].functionKey.Equals("IndirectEntry"))
				{
					MutatedIndirect = &IndirectMutatedRecords[Index];
					break;
				}
			}
			ASSERT_THAT(IsTrue(
				BaselineIndirect != nullptr && MutatedIndirect != nullptr
					&& !BaselineIndirect->contentHash.Equals(
						MutatedIndirect->contentHash),
				TEXT("indirect funcdef CallArgument provenance must participate in function identity")));
		}

	TEST_METHOD(SidecarConstructCallArgumentsRoundTripExactFormalRelation)
	{
		// CTA-S92 persistence gate: an ordinary CONSTRUCT owns the same sealed
		// source-to-formal relation as CALL. Cache/Hot Reload must persist that
		// authority instead of accepting only the child expression sequence.
		const char Source[] = "FConstructPlan(B:7)";
		asCASTContext Context;
		const asASTFileID File = Context.AddSourceSection(
			"ConstructArgumentSidecar.as",
			asAST_SOURCE_AUTHORED,
			Source,
			sizeof(Source) - 1,
			0);
		ASSERT_THAT(IsTrue(File != 0,
			TEXT("construct sidecar fixture must own its authored source")));
		const asCSourceManager& Sources = Context.GetSourceManager();
		const asCSourceRange ConstructRange =
			Sources.MakeRange(File, 0, sizeof(Source) - 1);
		const asCSourceRange DefaultRange = Sources.MakeRange(File, 0, 0);
		const asCSourceRange NamedRange = Sources.MakeRange(File, 17, 18);

		const asASTDeclId Tu =
			Context.CreateTranslationUnit("ConstructArgumentSidecar");
		const asCQualType VoidType = Context.InternPrimitive(ttVoid, 0);
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType ConstructType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FConstructPlan", 0);
		const asASTDeclId Class = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, ConstructRange, "FConstructPlan");
		ASSERT_THAT(AreEqual(0,
			Context.SetStableKey(Class, "FConstructPlan")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Class, ConstructType)));
		const asASTDeclId Constructor = Context.CreateDecl(
			asAST_DECL_CONSTRUCTOR, Class, ConstructRange, "FConstructPlan");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Constructor, VoidType)));
		const asASTDeclId A = Context.CreateDecl(
			asAST_DECL_PARAM, Constructor, DefaultRange, "A");
		const asASTDeclId B = Context.CreateDecl(
			asAST_DECL_PARAM, Constructor, NamedRange, "B");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(A, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(B, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDefaultArg(A, "42")));

		const asASTDeclId Entry = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, ConstructRange, "Entry");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Entry, VoidType)));
		const asASTStmtId Body = Context.CreateStmt(
			asAST_STMT_BLOCK, Entry, ConstructRange);
		const asASTStmtId Statement = Context.CreateStmt(
			asAST_STMT_EXPR, Entry, ConstructRange);
		ASSERT_THAT(AreEqual(0, Context.SetBody(Entry, Body)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Body, Statement)));

		const asASTExprId DefaultA = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
			DefaultRange);
		const asASTExprId NamedB = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
			NamedRange);
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(DefaultA, 42)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(NamedB, 7)));
		const asASTExprId Construct = Context.CreateExpr(
			asAST_EXPR_CONSTRUCT, ConstructType, asAST_VALUE_PRVALUE,
			ConstructRange);
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Construct, Constructor)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Construct, DefaultA)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Construct, NamedB)));

		asSASTCallArgument DefaultRecord;
		DefaultRecord.expression = DefaultA;
		DefaultRecord.formal = A;
		DefaultRecord.formalType = IntType;
		DefaultRecord.range = DefaultRange;
		DefaultRecord.formalIndex = 0;
		DefaultRecord.origin = asAST_CALL_ARGUMENT_DEFAULT;
		DefaultRecord.sourceOrdinal = asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL;
		ASSERT_THAT(AreEqual(0,
			Context.AddCallArgument(Construct, DefaultRecord)));

		asSASTCallArgument NamedRecord;
		NamedRecord.expression = NamedB;
		NamedRecord.formal = B;
		NamedRecord.formalType = IntType;
		NamedRecord.range = NamedRange;
		NamedRecord.formalIndex = 1;
		NamedRecord.origin = asAST_CALL_ARGUMENT_NAMED;
		NamedRecord.sourceOrdinal = 0;
		NamedRecord.authoredName = "B";
		ASSERT_THAT(AreEqual(0,
			Context.AddCallArgument(Construct, NamedRecord)));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Statement, Construct)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("ordinary construct relation must be publishable before persistence")));

		asSASTSidecarHeader Header;
		Header.functionKey = "ConstructArgumentSidecar.Entry";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		const int EncodeStatus = asCASTEncodeSidecar(Context, Header, Bytes);
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, EncodeStatus,
			TEXT("ordinary CONSTRUCT must persist its sealed CallArgument relation")));
		if (EncodeStatus != asAST_SIDECAR_OK)
		{
			return;
		}

		asCASTContext Restored;
		const int DecodeStatus = asCASTDecodeSidecar(
			Restored, Header, Bytes.AddressOf(), Bytes.GetLength());
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, DecodeStatus,
			TEXT("ordinary CONSTRUCT relation must restore before publication")));
		if (DecodeStatus != asAST_SIDECAR_OK)
		{
			return;
		}
		const asCExpr* const RestoredConstruct = Restored.GetExpr(Construct);
		ASSERT_THAT(IsNotNull(RestoredConstruct));
		if (RestoredConstruct == nullptr
			|| RestoredConstruct->callArguments.GetLength() != 2)
		{
			return;
		}
		const asSASTCallArgument& RestoredDefault =
			RestoredConstruct->callArguments[0];
		const asSASTCallArgument& RestoredNamed =
			RestoredConstruct->callArguments[1];
		ASSERT_THAT(IsTrue(
			RestoredConstruct->kind == asAST_EXPR_CONSTRUCT
				&& RestoredConstruct->resolvedDecl == Constructor
				&& RestoredDefault.expression == DefaultA
				&& RestoredDefault.formal == A
				&& RestoredDefault.formalType == IntType
				&& RestoredDefault.formalIndex == 0
				&& RestoredDefault.origin == asAST_CALL_ARGUMENT_DEFAULT
				&& RestoredDefault.sourceOrdinal
					== asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL
				&& RestoredDefault.authoredName.GetLength() == 0
				&& RestoredNamed.expression == NamedB
				&& RestoredNamed.formal == B
				&& RestoredNamed.formalType == IntType
				&& RestoredNamed.formalIndex == 1
				&& RestoredNamed.origin == asAST_CALL_ARGUMENT_NAMED
				&& RestoredNamed.sourceOrdinal == 0
				&& RestoredNamed.authoredName.Equals("B")
				&& RestoredNamed.range.begin.offset == NamedRange.begin.offset
				&& RestoredNamed.range.end.offset == NamedRange.end.offset,
			TEXT("V13 must restore the exact default/named constructor formal relation")));

		asCArray<asBYTE> RestoredBytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Restored, Header, RestoredBytes)));
		ASSERT_THAT(IsTrue(Bytes.GetLength() == RestoredBytes.GetLength()
				&& (Bytes.GetLength() == 0 || std::memcmp(
					Bytes.AddressOf(), RestoredBytes.AddressOf(),
					Bytes.GetLength()) == 0),
			TEXT("CONSTRUCT relation decode then encode must remain byte exact")));
	}

	TEST_METHOD(SidecarRoundTripPreservesCanonicalLifetimeProtocol)
	{
		asCASTContext Context;
		const asASTDeclId TranslationUnit =
			Context.CreateTranslationUnit("LifetimeSidecar");
		const asCQualType VoidType = Context.InternPrimitive(ttVoid);
		const asCQualType ElementType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FElement", 0);
		const asCQualType ArrayType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FElementArray", 0);

		const asASTDeclId ElementClass = Context.CreateDecl(
			asAST_DECL_CLASS, TranslationUnit, asCSourceRange(), "FElement");
		ASSERT_THAT(AreEqual(0,
			Context.SetStableKey(ElementClass, "FElement")));
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(ElementClass, ElementType)));
		const asASTDeclId ElementDestructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, ElementClass,
			asCSourceRange(), "FElement");
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(ElementDestructor, VoidType)));
		const asASTDeclId ElementConstructor = Context.CreateDecl(
			asAST_DECL_CONSTRUCTOR, ElementClass,
			asCSourceRange(), "FElement");
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(ElementConstructor, VoidType)));

		const asASTDeclId ArrayClass = Context.CreateDecl(
			asAST_DECL_CLASS, TranslationUnit,
			asCSourceRange(), "FElementArray");
		ASSERT_THAT(AreEqual(0,
			Context.SetStableKey(ArrayClass, "FElementArray")));
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(ArrayClass, ArrayType)));
		const asASTDeclId ListFactory = Context.CreateDecl(
			asAST_DECL_CONSTRUCTOR, ArrayClass,
			asCSourceRange(), "FElementArray");
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(ListFactory, VoidType)));
		ASSERT_THAT(AreEqual(0,
			Context.AddDeclTrait(ListFactory, asAST_TRAIT_LIST_FACTORY)));

		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, TranslationUnit,
			asCSourceRange(), "BuildArray");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Function, VoidType)));
		const asASTStmtId Body = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Function, Body)));

		const asASTExprId Aggregate = Context.CreateExpr(
			asAST_EXPR_CONSTRUCT, ArrayType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetLiteral(Aggregate, "list-pattern")));
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(Aggregate, ListFactory)));
		const asASTExprId Element = Context.CreateExpr(
			asAST_EXPR_CONSTRUCT, ElementType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetLiteral(Element, "FElement::FElement()")));
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(Element, ElementConstructor)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Aggregate, Element)));
		const asASTStmtId AggregateStatement = Context.CreateStmt(
			asAST_STMT_EXPR, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetStmtExpr(AggregateStatement, Aggregate)));
		ASSERT_THAT(AreEqual(0,
			Context.AddStmtChild(Body, AggregateStatement)));

		asSASTLifetimeRecord Record;
		Record.subjectKind = asAST_LIFETIME_SUBJECT_AGGREGATE_ELEMENT;
		Record.subject = asSASTLifetimeNodeId::FromExpr(Aggregate);
		Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_ELEMENT;
		Record.actionTarget =
			asSASTLifetimeNodeId::FromDecl(ElementDestructor);
		Record.activationPoint = asSASTLifetimeNodeId::FromExpr(Aggregate);
		Record.semanticRegion = asSASTLifetimeNodeId::FromDecl(Function);
		Record.phase = asAST_LIFETIME_PHASE_AGGREGATE_ELEMENT;
		Record.supportedExitMask = asAST_LIFETIME_EXIT_EXCEPTION;
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Record)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("the Sema-authored lifetime fixture must publish")));

		asSASTSidecarHeader Header;
		Header.functionKey = "LifetimeSidecar.BuildArray";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes)));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(
				Restored, Header, Bytes.AddressOf(), Bytes.GetLength())));
		ASSERT_THAT(AreEqual(
			Context.GetLifetimeProtocolRevision(),
			Restored.GetLifetimeProtocolRevision(),
			TEXT("the durable lifetime protocol revision must round-trip")));
		ASSERT_THAT(AreEqual(
			Context.GetLifetimeRecordCount(),
			Restored.GetLifetimeRecordCount(),
			TEXT("Sidecar restore must not erase Sema-authored lifetime facts")));
		ASSERT_THAT(AreEqual(
			Context.GetLifetimeProtocolHash(),
			Restored.GetLifetimeProtocolHash(),
			TEXT("the typed lifetime protocol identity must round-trip exactly")));
		ASSERT_THAT(IsNotNull(Restored.GetLifetimeRecord(0)));
		if (Restored.GetLifetimeRecord(0) != nullptr)
		{
			ASSERT_THAT(IsTrue(
				*Restored.GetLifetimeRecord(0) == Record,
				TEXT("every pointer-free lifetime field must round-trip")));
		}

		asCArray<asBYTE> CorruptBytes = Bytes;
		// V13 ends with the per-expression CallArgument counts. The final
		// zero-formal CONSTRUCT has no record bytes, so claiming one is malformed.
		CorruptBytes[CorruptBytes.GetLength() - 4] = 1;
		asCASTContext CorruptRestore;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTDecodeSidecar(
				CorruptRestore,
				Header,
				CorruptBytes.AddressOf(),
				CorruptBytes.GetLength()),
			TEXT("a forged lifetime commit flag must fail closed")));
		ASSERT_THAT(IsFalse(CorruptRestore.IsPublishable(),
			TEXT("malformed lifetime bytes must not publish a partial snapshot")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			CorruptRestore.GetLifetimeRecordCount(),
			TEXT("failed decode must abandon all candidate lifetime records")));
	}

	TEST_METHOD(SidecarRoundTripRestoresSourceFunctionBodyAndExpression)
	{
		const char Source[] = "int F() { return 7; }";
		asCASTContext Context;
		const asASTFileID File = Context.AddSourceSection(
			"CacheBody.as",
			asAST_SOURCE_AUTHORED,
			Source,
			sizeof(Source) - 1,
			3);
		ASSERT_THAT(IsTrue(File != 0, TEXT("source section should be owned by the sidecar snapshot")));
		const asCSourceManager& Sources = Context.GetSourceManager();
		const asCSourceRange FunctionRange = Sources.MakeRange(File, 0, sizeof(Source) - 1);
		const asCSourceRange ReturnRange = Sources.MakeRange(File, 10, 19);
		const asCSourceRange LiteralRange = Sources.MakeRange(File, 17, 18);

		const asASTDeclId Tu = Context.CreateTranslationUnit("CacheBody");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Function = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, FunctionRange, "F");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(Function, "F()"), TEXT("function identity should include its signature")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Function, IntType), TEXT("function return type")));
		const asASTStmtId Return = Context.CreateStmt(asAST_STMT_RETURN, Function, ReturnRange);
		ASSERT_THAT(AreEqual(0, Context.SetBody(Function, Return), TEXT("function body")));
		const asASTExprId Literal = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, LiteralRange);
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Literal, "7"), TEXT("return literal")));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Return, Literal), TEXT("return expression")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("complete sidecar input should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "F()";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode complete body sidecar")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode must rebuild a sealed AST without re-running Sema")));
		ASSERT_THAT(AreEqual((asUINT)1, Restored.GetSourceManager().GetSectionCount(),
			TEXT("source model survives sidecar round-trip")));
		ASSERT_THAT(IsTrue(asCString(Restored.GetSourceManager().GetLogicalKey(1)).Equals("CacheBody.as"),
			TEXT("restored source keeps its logical identity")));
		ASSERT_THAT(AreEqual(3, Restored.GetSourceManager().GetLineOffset(1),
			TEXT("restored source keeps its line offset")));
		ASSERT_THAT(AreEqual((asUINT)2, Restored.GetDeclCount(), TEXT("translation unit and function restore")));
		ASSERT_THAT(AreEqual((asUINT)1, Restored.GetStmtCount(), TEXT("function body statement restores")));
		ASSERT_THAT(AreEqual((asUINT)1, Restored.GetExprCount(), TEXT("function body expression restores")));

		const asCDecl* RestoredFunction = Restored.GetDecl(asASTDeclId(2));
		ASSERT_THAT(IsNotNull(RestoredFunction, TEXT("function declaration restores")));
		ASSERT_THAT(IsTrue(RestoredFunction->stableKey.Equals("F()"), TEXT("function stable key restores")));
		ASSERT_THAT(IsTrue(RestoredFunction->body.IsValid(), TEXT("function retains its body link")));
		ASSERT_THAT(IsTrue(Restored.GetSourceManager().IsValidRange(RestoredFunction->range),
			TEXT("function source range remaps into the restored source model")));

		const asCStmt* RestoredReturn = Restored.GetStmt(RestoredFunction->body);
		ASSERT_THAT(IsNotNull(RestoredReturn, TEXT("return statement restores")));
		ASSERT_THAT(AreEqual((int)asAST_STMT_RETURN, (int)RestoredReturn->kind, TEXT("return statement kind restores")));
		const asCExpr* RestoredLiteral = Restored.GetExpr(RestoredReturn->expr);
		ASSERT_THAT(IsNotNull(RestoredLiteral, TEXT("return literal restores")));
		ASSERT_THAT(AreEqual((int)asAST_EXPR_INTEGER_LITERAL, (int)RestoredLiteral->kind, TEXT("literal expression kind restores")));
		ASSERT_THAT(IsTrue(RestoredLiteral->literal.Equals("7"), TEXT("literal payload restores")));
	}

	TEST_METHOD(SidecarRoundTripPreservesCompleteSourceModel)
	{
		struct FSourceFixture
		{
			const char* LogicalKey;
			asEASTSourceOrigin Origin;
			const char* Bytes;
			int LineOffset;
		};

		const FSourceFixture Fixtures[] = {
			{ "/Angelscript/Game/Script/Authored.as", asAST_SOURCE_AUTHORED, "int Authored;\n", 2 },
			{ "/Angelscript/Game/Script/Processed.as", asAST_SOURCE_PROCESSED, "int Processed;\n", 5 },
			{ "<generated:default:SourceModel>", asAST_SOURCE_GENERATED, "void Generated() {}\n", 0 },
		};

		asCASTContext Context;
		for (const FSourceFixture& Fixture : Fixtures)
		{
			ASSERT_THAT(IsTrue(Context.AddSourceSection(
				Fixture.LogicalKey,
				Fixture.Origin,
				Fixture.Bytes,
				std::strlen(Fixture.Bytes),
				Fixture.LineOffset) != 0,
				TEXT("every source provenance class must be owned by the sidecar input")));
		}
		Context.CreateTranslationUnit("SourceModel");
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("source-model sidecar input should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "SourceModel";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode complete source model")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode complete source model without re-running Sema")));
		const asCSourceManager& RestoredSources = Restored.GetSourceManager();
		ASSERT_THAT(AreEqual((asUINT)UE_ARRAY_COUNT(Fixtures), RestoredSources.GetSectionCount(),
			TEXT("all source records must survive sidecar round-trip")));

		for (asUINT Index = 0; Index < UE_ARRAY_COUNT(Fixtures); ++Index)
		{
			const FSourceFixture& Fixture = Fixtures[Index];
			const asASTFileID File = RestoredSources.GetFileIDByIndex(Index);
			ASSERT_THAT(IsTrue(FCStringAnsi::Strcmp(
				RestoredSources.GetLogicalKey(File), Fixture.LogicalKey) == 0,
				TEXT("restored source must preserve its logical key")));
			ASSERT_THAT(AreEqual((int)Fixture.Origin, (int)RestoredSources.GetOrigin(File),
				TEXT("restored source must preserve its provenance class")));
			ASSERT_THAT(AreEqual(Fixture.LineOffset, RestoredSources.GetLineOffset(File),
				TEXT("restored source must preserve its line offset")));
			ASSERT_THAT(AreEqual((asUINT)std::strlen(Fixture.Bytes), RestoredSources.GetBufferLength(File),
				TEXT("restored source must preserve its byte length")));
			ASSERT_THAT(IsTrue(std::memcmp(
				RestoredSources.GetBuffer(File),
				Fixture.Bytes,
				std::strlen(Fixture.Bytes)) == 0,
				TEXT("restored source must preserve its exact bytes")));
		}
	}

	TEST_METHOD(SidecarRoundTripPreservesNeutralSourceProvenance)
	{
		const char* const Processed = "int __GeneratedAsset = 1;";
		asSSourceProvenanceRange Provenance;
		Provenance.processedOffset = 4;
		Provenance.processedLength = 16;
		Provenance.hasAuthoredOrigin = true;
		Provenance.authoredOrigin.section =
			"/Angelscript/Game/AuthoredAsset.as";
		Provenance.authoredOrigin.sourceOffset = 31;
		Provenance.authoredOrigin.sourceLength = 12;
		Provenance.authoredOrigin.row = 7;
		Provenance.authoredOrigin.column = 4;
		Provenance.hasGeneratedOrigin = true;
		Provenance.generatedOrigin.kind = asEGeneratedSourceKind::LiteralAsset;
		Provenance.generatedOrigin.generatorName = "LiteralAsset:GeneratedAsset";
		Provenance.generatedOrigin.hasAuthoredAnchor = true;
		Provenance.generatedOrigin.authoredAnchor = Provenance.authoredOrigin;

		asCASTContext Context;
		const asASTFileID File = Context.AddSourceSection(
			"/Angelscript/Game/ProcessedAsset.as",
			asAST_SOURCE_PROCESSED,
			Processed,
			std::strlen(Processed),
			0,
			&Provenance,
			1);
		ASSERT_THAT(IsTrue(File != 0));
		Context.CreateTranslationUnit("SourceProvenanceSidecar");
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asSASTSidecarHeader Header;
		Header.functionKey = "SourceProvenanceSidecar";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode snapshot-owned source provenance")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(
				Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode source provenance without re-running preprocessing or Sema")));
		asSSourceSpan RestoredSpan;
		ASSERT_THAT(IsTrue(Restored.GetSourceManager().ResolveSourceSpan(
			Restored.GetSourceManager().MakeRange(1, 4, 20),
			RestoredSpan)));
		ASSERT_THAT(IsTrue(RestoredSpan.hasAuthoredOrigin,
			TEXT("Cache restore must preserve authored diagnostic provenance")));
		ASSERT_THAT(IsTrue(RestoredSpan.authoredOrigin.section
			== "/Angelscript/Game/AuthoredAsset.as"));
		ASSERT_THAT(IsTrue(RestoredSpan.hasGeneratedOrigin,
			TEXT("Cache restore must preserve generated diagnostic provenance")));
		ASSERT_THAT(IsTrue(RestoredSpan.generatedOrigin.kind
			== asEGeneratedSourceKind::LiteralAsset));
		ASSERT_THAT(IsTrue(RestoredSpan.generatedOrigin.generatorName
			== "LiteralAsset:GeneratedAsset"));
		ASSERT_THAT(IsTrue(RestoredSpan.generatedOrigin.hasAuthoredAnchor));
	}

	TEST_METHOD(SourceProvenanceIsDiagnosticOnlyForFunctionRecordIdentity)
	{
		auto BuildContext = [](
			asCASTContext& Context,
			const char* AuthoredSection) -> int
		{
			const char* const Source = "int F() { return 7; }";
			asSSourceProvenanceRange Provenance;
			Provenance.processedOffset = 0;
			Provenance.processedLength = (asUINT)std::strlen(Source);
			Provenance.hasAuthoredOrigin = true;
			Provenance.authoredOrigin.section = AuthoredSection;
			Provenance.authoredOrigin.sourceOffset = 11;
			Provenance.authoredOrigin.sourceLength = 8;
			Provenance.authoredOrigin.row = 3;
			Provenance.authoredOrigin.column = 2;

			const asASTFileID File = Context.AddSourceSection(
				"/Angelscript/Game/ProcessedIdentity.as",
				asAST_SOURCE_PROCESSED,
				Source,
				std::strlen(Source),
				0,
				&Provenance,
				1);
			if (File == 0)
			{
				return -1;
			}
			const asCSourceRange Range = Context.GetSourceManager().MakeRange(
				File, 0, (asUINT)std::strlen(Source));
			const asASTDeclId Tu = Context.CreateTranslationUnit("IdentityModule");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTDeclId Function = Context.CreateDecl(
				asAST_DECL_FUNCTION, Tu, Range, "F");
			Context.SetStableKey(Function, "F()");
			Context.SetDeclType(Function, IntType);
			const asASTStmtId Return = Context.CreateStmt(
				asAST_STMT_RETURN, Function, Range);
			Context.SetBody(Function, Return);
			const asASTExprId Value = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL,
				IntType,
				asAST_VALUE_PRVALUE,
				Range);
			Context.SetLiteral(Value, "7");
			Context.SetStmtExpr(Return, Value);
			return Context.Seal();
		};

		asCASTContext First;
		asCASTContext Second;
		ASSERT_THAT(AreEqual(0,
			BuildContext(First, "/Angelscript/Game/FirstAuthored.as")));
		ASSERT_THAT(AreEqual(0,
			BuildContext(Second, "/Angelscript/Game/SecondAuthored.as")));

		asCArray<asSASTSidecarRecordId> FirstRecords;
		asCArray<asSASTSidecarRecordId> SecondRecords;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTCollectFunctionRecords(First, "Win64", FirstRecords)));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTCollectFunctionRecords(Second, "Win64", SecondRecords)));
		ASSERT_THAT(AreEqual((asUINT)1, FirstRecords.GetLength()));
		ASSERT_THAT(AreEqual((asUINT)1, SecondRecords.GetLength()));
		ASSERT_THAT(IsTrue(FirstRecords[0].contentHash.Equals(
			SecondRecords[0].contentHash),
			TEXT("authored/generated diagnostic provenance must not alter semantic function identity")));
	}

	TEST_METHOD(MalformedSourceProvenanceFailsClosed)
	{
		const char* const Source = "int Generated = 1;";
		const char* const LogicalKey = "/Angelscript/Game/MalformedProvenance.as";
		asSSourceProvenanceRange Provenance;
		Provenance.processedOffset = 0;
		Provenance.processedLength = (asUINT)std::strlen(Source);
		Provenance.hasAuthoredOrigin = true;
		Provenance.authoredOrigin.section = "/Angelscript/Game/Authored.as";

		asCASTContext Context;
		ASSERT_THAT(IsTrue(Context.AddSourceSection(
			LogicalKey,
			asAST_SOURCE_PROCESSED,
			Source,
			std::strlen(Source),
			0,
			&Provenance,
			1) != 0));
		Context.CreateTranslationUnit("MalformedProvenance");
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asSASTSidecarHeader Header;
		Header.functionKey = "MalformedProvenance";
		Header.profile = "Win64";
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes)));

		const asUINT ProvenanceOffset = 8
			+ 4 + Header.functionKey.GetLength()
			+ 4 + Header.profile.GetLength()
			+ 4
			+ 4 + (asUINT)std::strlen(LogicalKey)
			+ 4
			+ 4
			+ 4 + (asUINT)std::strlen(Source)
			+ 4;
		const asUINT HasAuthoredOriginOffset = ProvenanceOffset + 8;
		ASSERT_THAT(IsTrue(HasAuthoredOriginOffset < Bytes.GetLength()));
		Bytes[HasAuthoredOriginOffset] = 2;

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTDecodeSidecar(
				Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("invalid provenance presence flag must fail closed")));
		ASSERT_THAT(IsFalse(Restored.IsPublishable()));
		ASSERT_THAT(AreEqual((asUINT)0,
			Restored.GetSourceManager().GetSectionCount(),
			TEXT("failed provenance decode must not publish a partial source model")));
	}

	TEST_METHOD(SidecarRoundTripPreservesSealedCallDispatch)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("CallDispatchCache");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType OwnerType = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT, "DispatchOwner", 0);
		const asASTDeclId Owner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "DispatchOwner");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(Owner, "DispatchOwner")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Owner, OwnerType)));

		const asASTDeclId VirtualTarget = Context.CreateDecl(
			asAST_DECL_METHOD, Owner, asCSourceRange(), "VirtualTarget");
		const asASTDeclId DirectTarget = Context.CreateDecl(
			asAST_DECL_METHOD, Owner, asCSourceRange(), "DirectTarget");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(
			VirtualTarget, "DispatchOwner::VirtualTarget()")));
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(
			DirectTarget, "DispatchOwner::DirectTarget()")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(VirtualTarget, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(DirectTarget, IntType)));

		const asASTDeclId Caller = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "Caller");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(Caller, "Caller()")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Caller, IntType)));
		const asASTStmtId Body = Context.CreateStmt(
			asAST_STMT_BLOCK, Caller, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Caller, Body)));

		const asASTExprId VirtualCall = Context.CreateExpr(
			asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(VirtualCall, VirtualTarget)));
		ASSERT_THAT(AreEqual(0, Context.SetExprSafePointRole(
			VirtualCall, asAST_SAFEPOINT_CALL)));
		ASSERT_THAT(AreEqual(0, Context.SetExprCallDispatch(
			VirtualCall, asAST_CALL_DISPATCH_VIRTUAL)));
		const asASTStmtId VirtualStatement = Context.CreateStmt(
			asAST_STMT_EXPR, Caller, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(VirtualStatement, VirtualCall)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Body, VirtualStatement)));

		const asASTExprId DirectCall = Context.CreateExpr(
			asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(DirectCall, DirectTarget)));
		ASSERT_THAT(AreEqual(0, Context.SetExprSafePointRole(
			DirectCall, asAST_SAFEPOINT_CALL)));
		ASSERT_THAT(AreEqual(0, Context.SetExprCallDispatch(
			DirectCall, asAST_CALL_DISPATCH_DIRECT)));
		const asASTStmtId DirectStatement = Context.CreateStmt(
			asAST_STMT_EXPR, Caller, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(DirectStatement, DirectCall)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Body, DirectStatement)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("call-dispatch sidecar input must be a sealed semantic graph")));

		asSASTSidecarHeader Header;
		Header.functionKey = "Caller()";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode sealed call-dispatch facts")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(
				Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode call-dispatch facts without re-running Sema")));
		const asCExpr* const RestoredVirtual = Restored.GetExpr(VirtualCall);
		const asCExpr* const RestoredDirect = Restored.GetExpr(DirectCall);
		ASSERT_THAT(IsTrue(
			RestoredVirtual != nullptr
				&& RestoredVirtual->callDispatch == asAST_CALL_DISPATCH_VIRTUAL,
			TEXT("sidecar must preserve ordinary virtual-eligible member dispatch")));
		ASSERT_THAT(IsTrue(
			RestoredDirect != nullptr
				&& RestoredDirect->callDispatch == asAST_CALL_DISPATCH_DIRECT,
			TEXT("sidecar must preserve qualified base direct dispatch")));
	}

	TEST_METHOD(SidecarRoundTripPreservesStringLiteralBytesAfterEmbeddedNull)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("StringLiteralCache");
		const asCQualType VoidType = Context.InternPrimitive(ttVoid, 0);
		const asCQualType StringType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FString", asAST_QUAL_CONST);
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "Entry");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(Function, "Entry()"),
			TEXT("string sidecar function identity")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Function, VoidType),
			TEXT("string sidecar function return type")));
		const asASTStmtId Statement = Context.CreateStmt(
			asAST_STMT_EXPR, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Function, Statement),
			TEXT("string sidecar function body")));

		static const char ExpectedBytes[] = {
			'l', 'i', 'n', 'e', '\n', 'm', 'i', 'd', '\0', 't', 'a', 'i', 'l'
		};
		const asASTExprId Literal = Context.CreateExpr(
			asAST_EXPR_STRING_LITERAL, StringType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBytes(
			Literal, ExpectedBytes, static_cast<asUINT>(sizeof(ExpectedBytes))),
			TEXT("string sidecar semantic byte payload")));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(
			Literal, static_cast<asQWORD>(sizeof(ExpectedBytes))),
			TEXT("string sidecar exact byte length")));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Statement, Literal),
			TEXT("string sidecar expression statement")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("string sidecar input must seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "Entry()";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode exact string byte payload")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode exact string byte payload")));
		const asCExpr* const RestoredLiteral = Restored.GetExpr(asASTExprId(1));
		ASSERT_THAT(IsNotNull(RestoredLiteral, TEXT("restored string literal")));
		if (RestoredLiteral == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<size_t>(sizeof(ExpectedBytes)),
			RestoredLiteral->literal.GetLength(),
			TEXT("sidecar must not truncate the semantic payload at embedded NUL")));
		ASSERT_THAT(AreEqual(static_cast<asQWORD>(sizeof(ExpectedBytes)),
			RestoredLiteral->literalBits,
			TEXT("sidecar must preserve the exact StringLiteral byte length")));
		bool BytesMatch = RestoredLiteral->literal.GetLength() == sizeof(ExpectedBytes);
		for (size_t ByteIndex = 0; BytesMatch && ByteIndex < sizeof(ExpectedBytes); ++ByteIndex)
		{
			BytesMatch = RestoredLiteral->literal[ByteIndex] == ExpectedBytes[ByteIndex];
		}
		ASSERT_THAT(IsTrue(BytesMatch,
			TEXT("sidecar must preserve bytes after the embedded NUL")));
	}

	TEST_METHOD(SidecarRoundTripPreservesResolvedFloatWidthsAndConstantBits)
	{
		// A Cache V2 sidecar carries the already-resolved canonical AST.  It must
		// preserve both width tokens and raw constant storage, never recover them
		// later from the source spelling `float`.
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("FloatCache");
		const asCQualType Float32 = Context.InternPrimitive(ttFloat32, asAST_QUAL_CONST);
		const asCQualType Float64 = Context.InternPrimitive(ttFloat64, asAST_QUAL_CONST);
		const asASTDeclId Narrow = Context.CreateDecl(asAST_DECL_VAR, Tu, asCSourceRange(), "Narrow");
		const asASTDeclId Wide = Context.CreateDecl(asAST_DECL_VAR, Tu, asCSourceRange(), "Wide");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Narrow, Float32), TEXT("float32 declaration type")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Wide, Float64), TEXT("float64 declaration type")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclConstantValue(Narrow, 0x40500000ull),
			TEXT("float32 constant bits")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclConstantValue(Wide, 0x4012000000000000ull),
			TEXT("float64 constant bits")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("resolved float-width sidecar input should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "FloatCache.F";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode float-width Cache V2 sidecar")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode float-width Cache V2 sidecar without re-Sema")));
		const asCDecl* const RestoredNarrow = Restored.GetDecl(Narrow);
		const asCDecl* const RestoredWide = Restored.GetDecl(Wide);
		ASSERT_THAT(IsNotNull(RestoredNarrow, TEXT("float32 declaration restores")));
		ASSERT_THAT(IsNotNull(RestoredWide, TEXT("float64 declaration restores")));
		if (RestoredNarrow != nullptr)
		{
			const asCType* const NarrowType = Restored.GetType(RestoredNarrow->type.type);
			ASSERT_THAT(IsTrue(NarrowType != nullptr && NarrowType->primitiveToken == ttFloat32,
				TEXT("Cache V2 must preserve resolved float32 rather than a source spelling")));
			ASSERT_THAT(IsTrue(RestoredNarrow->hasConstantValue && RestoredNarrow->constantValue == 0x40500000ull,
				TEXT("Cache V2 must preserve float32 raw storage bits")));
		}
		if (RestoredWide != nullptr)
		{
			const asCType* const WideType = Restored.GetType(RestoredWide->type.type);
			ASSERT_THAT(IsTrue(WideType != nullptr && WideType->primitiveToken == ttFloat64,
				TEXT("Cache V2 must preserve resolved float64 rather than a source spelling")));
			ASSERT_THAT(IsTrue(RestoredWide->hasConstantValue && RestoredWide->constantValue == 0x4012000000000000ull,
				TEXT("Cache V2 must preserve float64 raw storage bits")));
		}
	}

	TEST_METHOD(SidecarRoundTripPreservesMixedWidthComparisonNormalization)
	{
		// Cache V2 persists the *sealed* semantic graph.  This graph is the
		// post-Sema form of `float64Value > 0.5f`: the literal remains float32
		// beneath a widening conversion, both comparison operands are float64,
		// and the operation itself is bool.
		asCASTContext Context;
		Context.CreateTranslationUnit("FloatComparisonCache");
		const asCQualType Float32 = Context.InternPrimitive(ttFloat32, 0);
		const asCQualType Float64 = Context.InternPrimitive(ttFloat64, 0);
		const asCQualType Bool = Context.InternPrimitive(ttBool, 0);
		const asASTExprId Left = Context.CreateExpr(asAST_EXPR_FLOAT_LITERAL, Float64, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId OriginalRight = Context.CreateExpr(asAST_EXPR_FLOAT_LITERAL, Float32, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId WidenedRight = Context.CreateExpr(asAST_EXPR_CONVERSION, Float64, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId Comparison = Context.CreateExpr(asAST_EXPR_BINARY, Bool, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Left, "2.0"), TEXT("float64 comparison left literal")));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(OriginalRight, "0.5f"), TEXT("float32 comparison source literal")));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(WidenedRight, "conv"), TEXT("widening conversion marker")));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Comparison, ">"), TEXT("comparison operator")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(WidenedRight, OriginalRight), TEXT("conversion owns float32 literal")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Comparison, Left), TEXT("comparison owns float64 left operand")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Comparison, WidenedRight), TEXT("comparison owns widened right operand")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("mixed-width comparison sidecar input should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "FloatComparisonCache.F";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode normalized mixed-width comparison sidecar")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode normalized comparison without recomputing Sema")));
		const asCExpr* const RestoredComparison = Restored.GetExpr(Comparison);
		ASSERT_THAT(IsNotNull(RestoredComparison, TEXT("normalized comparison restores")));
		if (RestoredComparison == nullptr || RestoredComparison->children.GetLength() != 2)
		{
			return;
		}
		const asCExpr* const RestoredLeft = Restored.GetExpr(RestoredComparison->children[0]);
		const asCExpr* const RestoredWidenedRight = Restored.GetExpr(RestoredComparison->children[1]);
		const asCType* const RestoredResultType = Restored.GetType(RestoredComparison->type.type);
		const asCType* const RestoredLeftType = RestoredLeft != nullptr ? Restored.GetType(RestoredLeft->type.type) : nullptr;
		const asCType* const RestoredRightType = RestoredWidenedRight != nullptr ? Restored.GetType(RestoredWidenedRight->type.type) : nullptr;
		ASSERT_THAT(IsTrue(RestoredResultType != nullptr && RestoredResultType->primitiveToken == ttBool,
			TEXT("Cache V2 preserves bool comparison result")));
		ASSERT_THAT(IsTrue(RestoredLeftType != nullptr && RestoredLeftType->primitiveToken == ttFloat64,
			TEXT("Cache V2 preserves normalized float64 left operand")));
		ASSERT_THAT(IsTrue(RestoredWidenedRight != nullptr && RestoredWidenedRight->kind == asAST_EXPR_CONVERSION,
			TEXT("Cache V2 preserves explicit widening conversion")));
		ASSERT_THAT(IsTrue(RestoredRightType != nullptr && RestoredRightType->primitiveToken == ttFloat64,
			TEXT("Cache V2 preserves normalized float64 right operand")));
		if (RestoredWidenedRight != nullptr && RestoredWidenedRight->children.GetLength() == 1)
		{
			const asCExpr* const RestoredOriginalRight = Restored.GetExpr(RestoredWidenedRight->children[0]);
			const asCType* const RestoredOriginalRightType = RestoredOriginalRight != nullptr
				? Restored.GetType(RestoredOriginalRight->type.type)
				: nullptr;
			ASSERT_THAT(IsTrue(RestoredOriginalRight != nullptr && RestoredOriginalRight->kind == asAST_EXPR_FLOAT_LITERAL,
				TEXT("Cache V2 keeps the original literal beneath conversion")));
			ASSERT_THAT(IsTrue(RestoredOriginalRightType != nullptr && RestoredOriginalRightType->primitiveToken == ttFloat32,
				TEXT("Cache V2 keeps the float32 source-literal type")));
		}
	}

	TEST_METHOD(UnsupportedSidecarVersionIsRejectedWithoutHirBytes)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		Sema.ActOnTranslationUnit("Ver");
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("version sample should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "Ver.F";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("current sidecar version should encode")));
		ASSERT_THAT(IsTrue(Bytes.GetLength() >= 8, TEXT("sidecar has magic+version")));
		const asUINT PreviousSchema = asAST_SIDECAR_SCHEMA_VERSION - 1;
		Bytes[4] = static_cast<asBYTE>(PreviousSchema & 0xff);
		Bytes[5] = static_cast<asBYTE>((PreviousSchema >> 8) & 0xff);
		Bytes[6] = static_cast<asBYTE>((PreviousSchema >> 16) & 0xff);
		Bytes[7] = static_cast<asBYTE>((PreviousSchema >> 24) & 0xff);

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNKNOWN_KIND,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("the previous V5 sidecar schema must fail closed after the V6 bump")));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNKNOWN_KIND,
			asCASTRejectNonSidecarInput(Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("reject helper must not reinterpret the previous schema as current AST input")));

		const asBYTE HirDump[] = { '.', 'h', 'i', 'r', ' ', 'x' };
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNSUPPORTED_INPUT,
			asCASTRejectNonSidecarInput(HirDump, 6),
			TEXT("HIR dump bytes are not a Cache V2 VM/AST payload")));
	}

	TEST_METHOD(SidecarRoundTripPreservesAccessSpecifierPermissionAndMemberEdge)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("AccessSidecar");
		const asASTDeclId Record = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "Vault");
		const asASTDeclId Specifier = Context.CreateDecl(
			asAST_DECL_ACCESS_SPECIFIER, Record, asCSourceRange(), "Internal");
		const asASTDeclId Permission = Context.CreateDecl(
			asAST_DECL_ACCESS_PERMISSION, Specifier, asCSourceRange(), "*");
		const asASTDeclId Field = Context.CreateDecl(
			asAST_DECL_VAR, Record, asCSourceRange(), "Value");
		ASSERT_THAT(AreEqual(0, Context.SetDeclTraits(
			Specifier, asAST_TRAIT_PRIVATE), TEXT("access base trait")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclTraits(
			Permission,
			asAST_TRAIT_ACCESS_WILDCARD
				| asAST_TRAIT_ACCESS_READONLY
				| asAST_TRAIT_ACCESS_EDITDEFAULTS),
			TEXT("access permission traits")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclAccessSpecifier(Field, Specifier),
			TEXT("member-to-access edge")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("access metadata sidecar graph should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "AccessSidecar.Vault";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode access metadata sidecar")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode access metadata without re-Sema")));
		const asCDecl* const RestoredSpecifier = Restored.GetDecl(Specifier);
		const asCDecl* const RestoredPermission = Restored.GetDecl(Permission);
		const asCDecl* const RestoredField = Restored.GetDecl(Field);
		ASSERT_THAT(IsTrue(RestoredSpecifier != nullptr
			&& RestoredSpecifier->kind == asAST_DECL_ACCESS_SPECIFIER
			&& (RestoredSpecifier->traits & asAST_TRAIT_PRIVATE) != 0,
			TEXT("sidecar preserves access specifier declaration and base")));
		ASSERT_THAT(IsTrue(RestoredPermission != nullptr
			&& RestoredPermission->kind == asAST_DECL_ACCESS_PERMISSION
			&& (RestoredPermission->traits & asAST_TRAIT_ACCESS_WILDCARD) != 0
			&& (RestoredPermission->traits & asAST_TRAIT_ACCESS_READONLY) != 0
			&& (RestoredPermission->traits & asAST_TRAIT_ACCESS_EDITDEFAULTS) != 0,
			TEXT("sidecar preserves permission flags")));
		ASSERT_THAT(IsTrue(RestoredField != nullptr
			&& RestoredField->accessSpecifier.value == Specifier.value,
			TEXT("sidecar preserves the exact member-to-access DeclId edge")));
	}

	TEST_METHOD(SidecarHeaderInspectionExposesOwnerAndProfileWithoutWildcardRestore)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		Sema.ActOnTranslationUnit("Inspect");
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("inspection sample should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "module:abc123";
		Header.profile = "profile:def456";
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode inspectable V2 sidecar")));

		asSASTSidecarHeader Inspected;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTReadSidecarHeader(Bytes.AddressOf(), Bytes.GetLength(), Inspected),
			TEXT("inspect V2 header without reconstructing a graph")));
		ASSERT_THAT(IsTrue(Inspected.functionKey.Equals("module:abc123"),
			TEXT("inspection exposes the durable owner key")));
		ASSERT_THAT(IsTrue(Inspected.profile.Equals("profile:def456"),
			TEXT("inspection exposes the exact target profile")));
		ASSERT_THAT(AreEqual(asAST_SIDECAR_SCHEMA_VERSION, Inspected.version,
			TEXT("inspection reports the accepted schema version")));

		const asBYTE Truncated[] = {
			'C', 'A', 'S', 'T',
			static_cast<asBYTE>(asAST_SIDECAR_SCHEMA_VERSION & 0xff),
			static_cast<asBYTE>((asAST_SIDECAR_SCHEMA_VERSION >> 8) & 0xff),
			static_cast<asBYTE>((asAST_SIDECAR_SCHEMA_VERSION >> 16) & 0xff),
			static_cast<asBYTE>((asAST_SIDECAR_SCHEMA_VERSION >> 24) & 0xff),
		};
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTReadSidecarHeader(Truncated, 8, Inspected),
			TEXT("truncated owner/profile data is a safe miss")));
	}

	TEST_METHOD(EnvelopeUsesASTBodySidecarKindWithoutFunctionBodyBytes)
	{
		TArray<uint8> Payload = {1, 2, 3, 4};
		TArray<uint8> Envelope;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
				EAngelscriptCacheRecordKind::ASTBodySidecar,
				Payload,
				Envelope);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), TEXT("ASTBodySidecar envelope should serialize")));
		ASSERT_THAT(IsTrue(Envelope.Num() > Payload.Num(), TEXT("envelope wraps payload")));
		ASSERT_THAT(AreEqual(static_cast<uint8>(EAngelscriptCacheRecordKind::ASTBodySidecar), Envelope[12],
			TEXT("record kind byte is ASTBodySidecar")));
	}

	TEST_METHOD(RuntimeWrapperRejectsInvalidPayloadAndPreservesVerifiedCurrentDto)
	{
		FAngelscriptCacheASTBodySidecar Sidecar;
		Sidecar.FunctionKey = TEXT("Mod.F");
		TArray<uint8> Bytes;
		const FAngelscriptCacheASTBodySidecarResult Empty =
			AngelscriptEncodeASTBodySidecar(Sidecar, Bytes);
		ASSERT_THAT(IsTrue(Empty.Error == EAngelscriptCacheASTBodySidecarError::EmptyPayloadNotAbsence,
			TEXT("discard/empty sidecar cannot be treated as a body")));

		Sidecar.CanonicalAstBytes = {1};
		const FAngelscriptCacheASTBodySidecarResult Invalid =
			AngelscriptEncodeASTBodySidecar(Sidecar, Bytes);
		ASSERT_THAT(IsTrue(Invalid.Error == EAngelscriptCacheASTBodySidecarError::Malformed,
			TEXT("the runtime wrapper must reject an arbitrary non-empty placeholder payload")));
		ASSERT_THAT(AreEqual(0, Bytes.Num(), TEXT("failed validation must not publish replacement bytes")));

		asCASTContext PayloadContext;
		asCSema PayloadSema(nullptr, PayloadContext);
		PayloadSema.ActOnTranslationUnit("Mod");
		ASSERT_THAT(AreEqual(0, PayloadContext.Seal(), TEXT("wrapper test payload must seal")));
		asSASTSidecarHeader Header;
		Header.functionKey = "Mod.F";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> ForkBytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(PayloadContext, Header, ForkBytes),
			TEXT("construct an independently verified current-schema sidecar")));
		Sidecar.CanonicalAstBytes.Reset();
		for (asUINT Index = 0; Index < ForkBytes.GetLength(); ++Index)
		{
			Sidecar.CanonicalAstBytes.Add(ForkBytes[Index]);
		}

		const FAngelscriptCacheASTBodySidecarResult EmptyProfileEncode =
			AngelscriptEncodeASTBodySidecar(Sidecar, Bytes);
		ASSERT_THAT(IsTrue(EmptyProfileEncode.Error == EAngelscriptCacheASTBodySidecarError::ProfileMismatch,
			TEXT("runtime encode must not treat an empty Sidecar.Profile as a wildcard")));
		ASSERT_THAT(AreEqual(0, Bytes.Num(), TEXT("empty-profile encode must not publish replacement bytes")));

		Sidecar.Profile = TEXT("Win64");
		const FAngelscriptCacheASTBodySidecarResult Encoded =
			AngelscriptEncodeASTBodySidecar(Sidecar, Bytes);
		ASSERT_THAT(IsTrue(Encoded.IsSuccess(), TEXT("verified current-schema sidecar encodes")));
		ASSERT_THAT(IsTrue(Bytes == Sidecar.CanonicalAstBytes,
			TEXT("the runtime wrapper must preserve, rather than replace, the verified DTO")));

		FAngelscriptCacheASTBodySidecar WildcardDecoded;
		const FAngelscriptCacheASTBodySidecarResult WildcardRoundTrip =
			AngelscriptDecodeASTBodySidecar(Bytes, TEXT("Mod.F"), TEXT(""), WildcardDecoded);
		ASSERT_THAT(IsTrue(WildcardRoundTrip.Error == EAngelscriptCacheASTBodySidecarError::ProfileMismatch,
			TEXT("runtime decode must not treat an empty expected profile as a wildcard")));

		FAngelscriptCacheASTBodySidecar Decoded;
		const FAngelscriptCacheASTBodySidecarResult RoundTrip =
			AngelscriptDecodeASTBodySidecar(Bytes, TEXT("Mod.F"), TEXT("Win64"), Decoded);
		ASSERT_THAT(IsTrue(RoundTrip.IsSuccess(), TEXT("round-trip decode")));
		ASSERT_THAT(IsTrue(Decoded.FunctionKey == TEXT("Mod.F"), TEXT("function key remaps without Engine pointers")));
		ASSERT_THAT(IsTrue(Decoded.Profile == TEXT("Win64"), TEXT("exact profile remaps without Engine pointers")));

		TArray<uint8> Truncated = Bytes;
		Truncated.Pop(EAllowShrinking::No);
		FAngelscriptCacheASTBodySidecar Inspected;
		const FAngelscriptCacheASTBodySidecarResult HeaderOnly =
			AngelscriptInspectASTBodySidecar(Truncated, Inspected);
		ASSERT_THAT(IsTrue(HeaderOnly.IsSuccess(),
			TEXT("discard-policy admission should inspect the bounded envelope/header without rebuilding the optional AST graph")));
		ASSERT_THAT(IsTrue(Inspected.FunctionKey == TEXT("Mod.F"),
			TEXT("header-only admission keeps the durable owner for graph validation")));
		ASSERT_THAT(IsTrue(Inspected.CanonicalAstBytes.IsEmpty(),
			TEXT("header-only admission must not retain a duplicate AST payload")));
		FAngelscriptCacheASTBodySidecar RejectedRetain;
		const FAngelscriptCacheASTBodySidecarResult FullDecode =
			AngelscriptDecodeASTBodySidecar(
				Truncated, TEXT("Mod.F"), TEXT("Win64"), RejectedRetain);
		ASSERT_THAT(IsFalse(FullDecode.IsSuccess(),
			TEXT("a retained snapshot must still strictly reject a body-truncated sidecar")));
	}

	TEST_METHOD(IncrementalRebuildsChangedFunctionReusesUnchangedAndStaysAtomic)
	{
		asCASTContext Previous;
		asCSema PrevSema(nullptr, Previous);
		const asASTDeclId PrevTu = PrevSema.ActOnTranslationUnit("IncMod");
		const asCQualType IntType = Previous.InternPrimitive(ttInt, 0);
		const asASTDeclId PrevF = PrevSema.ActOnFunctionDecl(PrevTu, "F", IntType, asCSourceRange());
		const asASTDeclId PrevG = PrevSema.ActOnFunctionDecl(PrevTu, "G", IntType, asCSourceRange());
		PrevSema.RecordDependency(PrevF, "Other");
		ASSERT_THAT(AreEqual(0, Previous.Seal(), TEXT("previous graph should seal")));

		asCArray<asSASTSidecarRecordId> PrevRecords;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTCollectFunctionRecords(Previous, "Win64", PrevRecords),
			TEXT("collect previous function records")));

		asCASTContext Current;
		asCSema CurrSema(nullptr, Current);
		const asASTDeclId CurrTu = CurrSema.ActOnTranslationUnit("IncMod");
		const asCQualType CurrInt = Current.InternPrimitive(ttInt, 0);
		const asASTDeclId CurrF = CurrSema.ActOnFunctionDecl(CurrTu, "F", CurrInt, asCSourceRange());
		const asASTDeclId CurrG = CurrSema.ActOnFunctionDecl(CurrTu, "G", Current.InternPrimitive(ttInt, asAST_QUAL_CONST), asCSourceRange());
		CurrSema.RecordDependency(CurrF, "Other");
		ASSERT_THAT(AreEqual(0, Current.Seal(), TEXT("current graph should seal")));

		asCArray<asSASTSidecarRecordId> CurrRecords;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTCollectFunctionRecords(Current, "Win64", CurrRecords),
			TEXT("collect current function records")));

		asCArray<asCString> Reused;
		asCArray<asCString> Rebuilt;
		asCArray<asCString> Retired;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTPlanIncrementalSidecars(PrevRecords, CurrRecords, Reused, Rebuilt, Retired),
			TEXT("incremental plan")));
		bool bReusedF = false;
		bool bRebuiltG = false;
		for (asUINT i = 0; i < Reused.GetLength(); ++i)
		{
			if (Reused[i].Equals("F()"))
			{
				bReusedF = true;
			}
		}
		for (asUINT i = 0; i < Rebuilt.GetLength(); ++i)
		{
			if (Rebuilt[i].Equals("G()"))
			{
				bRebuiltG = true;
			}
		}
		ASSERT_THAT(IsTrue(bReusedF, TEXT("unchanged F must reuse its record id")));
		ASSERT_THAT(IsTrue(bRebuiltG, TEXT("changed G must rebuild its FunctionBody/ASTBodySidecar")));
		ASSERT_THAT(AreEqual((asUINT)0, Retired.GetLength(), TEXT("no retirement on in-place body edit")));

		asCArray<asSASTSidecarRecordId> Live;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTCommitSidecarSet(Live, PrevRecords),
			TEXT("first activation")));
		asCArray<asSASTSidecarRecordId> Invalid = CurrRecords;
		Invalid[0].contentHash = "";
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED, asCASTCommitSidecarSet(Live, Invalid),
			TEXT("failed activation must not mutate the live set")));
		ASSERT_THAT(AreEqual(PrevRecords.GetLength(), Live.GetLength(),
			TEXT("atomic activation keeps the previous sidecar set")));
		(void)PrevG;
		(void)CurrG;
	}

	TEST_METHOD(DependencyChangeInvalidatesOnlyDependentClosure)
	{
		asCASTContext Previous;
		asCSema PrevSema(nullptr, Previous);
		const asASTDeclId PrevTu = PrevSema.ActOnTranslationUnit("DepMod");
		const asCQualType IntType = Previous.InternPrimitive(ttInt, 0);
		const asASTDeclId PrevF = PrevSema.ActOnFunctionDecl(PrevTu, "F", IntType, asCSourceRange());
		PrevSema.ActOnFunctionDecl(PrevTu, "H", IntType, asCSourceRange());
		PrevSema.RecordDependency(PrevF, "Other");
		ASSERT_THAT(AreEqual(0, Previous.Seal(), TEXT("previous dep graph should seal")));

		asCASTContext Current;
		asCSema CurrSema(nullptr, Current);
		const asASTDeclId CurrTu = CurrSema.ActOnTranslationUnit("DepMod");
		const asCQualType CurrInt = Current.InternPrimitive(ttInt, 0);
		const asASTDeclId CurrF = CurrSema.ActOnFunctionDecl(CurrTu, "F", CurrInt, asCSourceRange());
		CurrSema.ActOnFunctionDecl(CurrTu, "H", CurrInt, asCSourceRange());
		CurrSema.RecordDependency(CurrF, "OtherChanged");
		ASSERT_THAT(AreEqual(0, Current.Seal(), TEXT("current dep graph should seal")));

		asCArray<asSASTSidecarRecordId> PrevRecords;
		asCArray<asSASTSidecarRecordId> CurrRecords;
		asCASTCollectFunctionRecords(Previous, "Win64", PrevRecords);
		asCASTCollectFunctionRecords(Current, "Win64", CurrRecords);
		asCArray<asCString> Reused;
		asCArray<asCString> Rebuilt;
		asCArray<asCString> Retired;
		asCASTPlanIncrementalSidecars(PrevRecords, CurrRecords, Reused, Rebuilt, Retired);
		bool bReusedH = false;
		bool bRebuiltF = false;
		for (asUINT i = 0; i < Reused.GetLength(); ++i)
		{
			if (Reused[i].Equals("H()"))
			{
				bReusedH = true;
			}
		}
		for (asUINT i = 0; i < Rebuilt.GetLength(); ++i)
		{
			if (Rebuilt[i].Equals("F()"))
			{
				bRebuiltF = true;
			}
		}
		ASSERT_THAT(IsTrue(bReusedH, TEXT("H has no import/type dependency change")));
		ASSERT_THAT(IsTrue(bRebuiltF, TEXT("F's import/type dependency change invalidates its sidecar")));
	}

	TEST_METHOD(FunctionBodyAndProfileChangesInvalidateFunctionSidecarRecord)
	{
		auto BuildReturnLiteral = [](asCASTContext& Context, const char* Literal) -> int
		{
			const asASTDeclId Tu = Context.CreateTranslationUnit("BodyIdentity");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTDeclId Function = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
			Context.SetDeclType(Function, IntType);
			const asASTStmtId Return = Context.CreateStmt(asAST_STMT_RETURN, Function, asCSourceRange());
			Context.SetBody(Function, Return);
			const asASTExprId Value = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
			Context.SetLiteral(Value, Literal);
			Context.SetStmtExpr(Return, Value);
			return Context.Seal();
		};

		asCASTContext Previous;
		asCASTContext BodyChanged;
		ASSERT_THAT(AreEqual(0, BuildReturnLiteral(Previous, "7"), TEXT("previous body should seal")));
		ASSERT_THAT(AreEqual(0, BuildReturnLiteral(BodyChanged, "8"), TEXT("changed body should seal")));

		asCArray<asSASTSidecarRecordId> PreviousRecords;
		asCArray<asSASTSidecarRecordId> BodyChangedRecords;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTCollectFunctionRecords(Previous, "Win64", PreviousRecords),
			TEXT("collect previous body record")));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTCollectFunctionRecords(BodyChanged, "Win64", BodyChangedRecords),
			TEXT("collect changed body record")));
		ASSERT_THAT(AreEqual((asUINT)1, PreviousRecords.GetLength(), TEXT("one previous function record")));
		ASSERT_THAT(AreEqual((asUINT)1, BodyChangedRecords.GetLength(), TEXT("one changed function record")));
		ASSERT_THAT(IsFalse(PreviousRecords[0].contentHash.Equals(BodyChangedRecords[0].contentHash),
			TEXT("a return-expression change must invalidate the same function's sidecar identity")));

		asCArray<asCString> Reused;
		asCArray<asCString> Rebuilt;
		asCArray<asCString> Retired;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTPlanIncrementalSidecars(PreviousRecords, BodyChangedRecords, Reused, Rebuilt, Retired),
			TEXT("plan body-change increment")));
		ASSERT_THAT(AreEqual((asUINT)0, Reused.GetLength(), TEXT("changed function body must never be reused")));
		ASSERT_THAT(AreEqual((asUINT)1, Rebuilt.GetLength(), TEXT("changed function body must rebuild exactly once")));

		asCArray<asSASTSidecarRecordId> OtherProfileRecords;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTCollectFunctionRecords(Previous, "Android", OtherProfileRecords),
			TEXT("collect profile-specific record")));
		ASSERT_THAT(AreEqual((asUINT)1, OtherProfileRecords.GetLength(), TEXT("one profile-specific function record")));
		ASSERT_THAT(IsFalse(PreviousRecords[0].contentHash.Equals(OtherProfileRecords[0].contentHash),
			TEXT("the target profile must participate in FunctionBody/ASTBodySidecar identity")));
	}

	TEST_METHOD(OldSchemaDumpAndHirInputsAreSafeMisses)
	{
		const asBYTE Dump[] = { 'A', 'S', 'T', '\n', 'T', 'U' };
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNSUPPORTED_INPUT,
			asCASTRejectNonSidecarInput(Dump, 6),
			TEXT("AST dump is not a Cache input path")));
		const asBYTE Hir[] = { 'x', '.', 'h', 'i', 'r', '.', 't', 'x', 't' };
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNSUPPORTED_INPUT,
			asCASTRejectNonSidecarInput(Hir, 9),
			TEXT(".hir.txt/.hir.json are not Cache inputs")));
		const asBYTE OldSchema[] = { 'C', 'A', 'S', 'T', 0, 0, 0, 0 };
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNKNOWN_KIND,
			asCASTRejectNonSidecarInput(OldSchema, 8),
			TEXT("old sidecar schema is a safe miss")));
	}

	TEST_METHOD(AdversarialSidecarLengthFieldsFailClosedWithoutHugeAlloc)
	{
		const asBYTE Schema = static_cast<asBYTE>(asAST_SIDECAR_SCHEMA_VERSION & 0xff);
		const asBYTE UintMaxOwnerLen[] = {
			'C', 'A', 'S', 'T',
			Schema, 0, 0, 0,
			0xff, 0xff, 0xff, 0xff,
		};
		asSASTSidecarHeader Inspected;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTReadSidecarHeader(UintMaxOwnerLen, 12, Inspected),
			TEXT("UINT_MAX owner length must not wrap offset+n")));
		asCASTContext MaxDecode;
		asSASTSidecarHeader Expected;
		Expected.functionKey = "Mod.F";
		Expected.profile = "Win64";
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTDecodeSidecar(MaxDecode, Expected, UintMaxOwnerLen, 12),
			TEXT("UINT_MAX owner length must fail closed on full decode")));
		ASSERT_THAT(IsFalse(MaxDecode.IsPublishable(),
			TEXT("malicious length must not publish a graph")));

		const asBYTE UintMaxMinusOne[] = {
			'C', 'A', 'S', 'T',
			Schema, 0, 0, 0,
			0xfe, 0xff, 0xff, 0xff,
		};
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTReadSidecarHeader(UintMaxMinusOne, 12, Inspected),
			TEXT("UINT_MAX-1 owner length must fail closed")));

		const asBYTE ExactRemaining[] = {
			'C', 'A', 'S', 'T',
			Schema, 0, 0, 0,
			4, 0, 0, 0,
			'M', 'o', 'd', '.',
		};
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTReadSidecarHeader(ExactRemaining, 16, Inspected),
			TEXT("exact remaining owner bytes without a profile are still truncated")));

		const asBYTE TruncatedCount[] = {
			'C', 'A', 'S', 'T',
			Schema, 0, 0, 0,
			2, 0, 0, 0,
			'A',
		};
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTReadSidecarHeader(TruncatedCount, 13, Inspected),
			TEXT("truncated owner payload is a safe miss")));

		const asBYTE NestedUintMaxCount[] = {
			'C', 'A', 'S', 'T',
			Schema, 0, 0, 0,
			5, 0, 0, 0, 'M', 'o', 'd', '.', 'F',
			5, 0, 0, 0, 'W', 'i', 'n', '6', '4',
			0xff, 0xff, 0xff, 0xff,
		};
		asCASTContext NestedCount;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_BUDGET,
			asCASTDecodeSidecar(NestedCount, Expected, NestedUintMaxCount, sizeof(NestedUintMaxCount)),
			TEXT("UINT_MAX nested source count must fail closed before allocation")));
		ASSERT_THAT(IsFalse(NestedCount.IsPublishable(),
			TEXT("nested UINT_MAX count must not publish a graph")));

		const asBYTE NestedUintMaxString[] = {
			'C', 'A', 'S', 'T',
			Schema, 0, 0, 0,
			5, 0, 0, 0, 'M', 'o', 'd', '.', 'F',
			5, 0, 0, 0, 'W', 'i', 'n', '6', '4',
			1, 0, 0, 0,
			0xff, 0xff, 0xff, 0xff,
		};
		asCASTContext NestedString;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTDecodeSidecar(NestedString, Expected, NestedUintMaxString, sizeof(NestedUintMaxString)),
			TEXT("UINT_MAX nested string length must fail closed without wrapping offset+n")));
		ASSERT_THAT(IsFalse(NestedString.IsPublishable(),
			TEXT("nested UINT_MAX string must not publish a graph")));
	}

	TEST_METHOD(FullDecodeRejectsEmptyExpectedProfileWhenPayloadProfileIsPresent)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		Sema.ActOnTranslationUnit("Mod");
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("profile-exact sample should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "Mod.F";
		Header.profile = "Win64";
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode a sidecar with a non-empty profile")));

		asSASTSidecarHeader EmptyExpected = Header;
		EmptyExpected.profile = "";
		asCASTContext Wildcard;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_PROFILE_MISMATCH,
			asCASTDecodeSidecar(Wildcard, EmptyExpected, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("full decode must not treat empty expected profile as wildcard")));
		ASSERT_THAT(IsFalse(Wildcard.IsPublishable(),
			TEXT("wildcard restore must not publish a graph")));

		asSASTSidecarHeader Inspected;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTReadSidecarHeader(Bytes.AddressOf(), Bytes.GetLength(), Inspected),
			TEXT("header inspection remains available without an expected profile")));
		ASSERT_THAT(IsTrue(Inspected.profile.Equals("Win64"),
			TEXT("inspection still reports the encoded profile")));
	}
};

#endif
