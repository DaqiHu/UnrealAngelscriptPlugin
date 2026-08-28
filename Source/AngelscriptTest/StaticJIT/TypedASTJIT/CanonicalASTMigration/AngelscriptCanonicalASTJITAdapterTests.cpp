#include "AngelscriptTestMacros.h"
#include "Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCanonical.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEligibility.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITModel.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_lifetime.h"
#include "source/as_datatype.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_sema.h"
#include "source/as_source_manager.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FCanonicalASTJITAdapterTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.CanonicalASTMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TypedASTJITAdmissionRequiresFrozenPublishableContext)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("JitLifecycleAdmission");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(
			Tu, "Run", IntType, asCSourceRange());
		asCArray<asASTStmtId> Body;
		Body.PushLast(Sema.ActOnReturnStmt(
			Fn, Sema.ActOnIntegerLiteral(1, asCSourceRange()),
			asCSourceRange()));
		Context.SetBody(Fn, Sema.ActOnCompoundStmt(
			Fn, Body, asCSourceRange()));
		Sema.FinishDecl(Fn);

		FAngelscriptTypedASTJITEligibilityInput Input;
		Input.bIsUFunctionRoot = true;
		Input.SealedAST = &Context;
		Input.CanonicalFunctionDecl = Fn;
		auto RequireRejected = [&]()
		{
			const FAngelscriptTypedASTJITEligibility Eligibility =
				EvaluateAngelscriptTypedASTJITEligibility(Input);
			ASSERT_THAT(AreEqual(
				static_cast<int32>(EAngelscriptTypedASTJITFallbackReason::MissingCanonicalAST),
				static_cast<int32>(Eligibility.Reason),
				TEXT("TypedASTJIT must fail closed before Frozen/Publishable")));
		};

		RequireRejected();
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_BUILDING, asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("fixture must reach SemaFinalized")));
		RequireRejected();
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_SEMA_FINALIZED, asAST_CONTEXT_LIFETIME_PLANNED),
			TEXT("fixture must reach LifetimePlanned")));
		RequireRejected();
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_LIFETIME_PLANNED, asAST_CONTEXT_FROZEN_PUBLISHABLE),
			TEXT("fixture must reach Frozen/Publishable")));
		const FAngelscriptTypedASTJITEligibility Ready =
			EvaluateAngelscriptTypedASTJITEligibility(Input);
		ASSERT_THAT(AreNotEqual(
			static_cast<int32>(EAngelscriptTypedASTJITFallbackReason::MissingCanonicalAST),
			static_cast<int32>(Ready.Reason),
			TEXT("Frozen/Publishable must cross the shared admission predicate")));
	}

	TEST_METHOD(SealedAstAnalysisDoesNotRequireBytecode)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("JitAst");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(
			Tu, "Run", IntType, asCSourceRange());
		asCArray<asASTStmtId> Body;
		Body.PushLast(Sema.ActOnReturnStmt(
			Fn, Sema.ActOnIntegerLiteral(1, asCSourceRange()),
			asCSourceRange()));
		Context.SetBody(Fn, Sema.ActOnCompoundStmt(
			Fn, Body, asCSourceRange()));
		Sema.FinishDecl(Fn);
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("JIT adapter needs a sealed AST")));

		asCString DumpBefore;
		asCASTDump(Context, DumpBefore);

		FAngelscriptTypedASTJITEligibilityInput Input;
		Input.bIsUFunctionRoot = true;
		Input.SealedAST = &Context;
		Input.CanonicalFunctionDecl = Fn;
		const FAngelscriptTypedASTJITEligibility Eligibility =
			EvaluateAngelscriptTypedASTJITEligibility(Input);
		ASSERT_THAT(AreNotEqual(
			static_cast<int32>(EAngelscriptTypedASTJITFallbackReason::MissingCanonicalAST),
			static_cast<int32>(Eligibility.Reason),
			TEXT("sealed AST is the authoritative body")));
		ASSERT_THAT(IsTrue(Eligibility.bEligible, TEXT("simple sealed UFunction AST stays eligible without widening")));

		asCString DumpAfter;
		asCASTDump(Context, DumpAfter);
		ASSERT_THAT(IsTrue(DumpBefore.Equals(DumpAfter), TEXT("analysis must leave AST dumps unchanged")));
		ASSERT_THAT(IsTrue(DumpAfter.FindLast("0x") < 0, TEXT("dump remains pointer-free")));
	}

	TEST_METHOD(ProductionEligibilityInputUsesCanonicalRuntimeShape)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("JitProductionAuthority");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(
			Tu, "Run", IntType, asCSourceRange());
		Sema.FinishDecl(Fn);
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("production authority fixture must seal")));

		asCScriptEngine Engine;
		asCScriptFunction RuntimeFunction(&Engine, nullptr, asFUNC_SCRIPT);
		RuntimeFunction.name = "Run";
		RuntimeFunction.returnType = asCDataType::CreatePrimitive(ttInt, false);

		FAngelscriptStaticJITCompiledFunctionView Function;
		Function.EngineLocalFunction = &RuntimeFunction;
		Function.SealedAST = &Context;
		Function.CanonicalFunctionDeclValue = Fn.value;

		const FAngelscriptTypedASTJITEligibilityInput Input =
			BuildAngelscriptTypedASTJITEligibilityInput(nullptr, Function);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(EAngelscriptTypedASTJITInvocationKind::Global),
			static_cast<int32>(Input.Shape.InvocationKind),
			TEXT("exact global Decl and Runtime signature select the global shape")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(EAngelscriptTypedASTJITReceiverKind::None),
			static_cast<int32>(Input.Shape.ReceiverKind),
			TEXT("global canonical function has no effective receiver")));
		ASSERT_THAT(AreEqual(INDEX_NONE, Input.Shape.ReceiverParameterIndex,
			TEXT("global canonical function has no receiver parameter")));
	}

	TEST_METHOD(CanonicalLifetimeFactsClassifyReleaseAndDestructorPlans)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asCSourceRange Range;
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("JitLifetimeFacts");
		const asCQualType VoidType = Context.InternPrimitive(ttVoid, 0);

		const asCQualType ReferenceType = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT, "FTrackedReference");
		const asASTDeclId ReleaseFunction = Sema.ActOnFunctionDecl(
			Tu, "ReleaseLocal", VoidType, Range);
		const asASTDeclId ReferenceLocal = Sema.ActOnVarDecl(
			ReleaseFunction, "Object", ReferenceType, Range);
		const asASTStmtId ReferenceDeclStmt = Context.CreateStmt(
			asAST_STMT_DECL, ReleaseFunction, Range);
		Context.SetStmtDecl(ReferenceDeclStmt, ReferenceLocal);
		const asASTExprId ReferenceInitTarget = Sema.ActOnDeclRef(
			ReferenceLocal, ReferenceType, Range);
		const asASTExprId ReferenceInitValue = Context.CreateExpr(
			asAST_EXPR_NULL_LITERAL, ReferenceType,
			asAST_VALUE_PRVALUE, Range);
		const asASTExprId ReferenceAssignment = Context.CreateExpr(
			asAST_EXPR_ASSIGN, ReferenceType,
			asAST_VALUE_LVALUE, Range);
		Context.SetLiteral(ReferenceAssignment, "=");
		Context.AddExprChild(ReferenceAssignment, ReferenceInitTarget);
		Context.AddExprChild(ReferenceAssignment, ReferenceInitValue);
		Context.AddDeclInit(ReferenceLocal, ReferenceAssignment);
		const asASTExprId ReleaseTarget = Sema.ActOnDeclRef(
			ReferenceLocal, ReferenceType, Range);
		const asASTExprId Release = Context.CreateExpr(
			asAST_EXPR_CLEANUP, ReferenceType, asAST_VALUE_PRVALUE, Range);
		Context.SetLiteral(Release, "scope-release");
		Context.AddExprChild(Release, ReleaseTarget);
		asCArray<asASTStmtId> ReleaseBody;
		ReleaseBody.PushLast(ReferenceDeclStmt);
		ReleaseBody.PushLast(Sema.ActOnExprStmt(
			ReleaseFunction, ReferenceAssignment, Range));
		ReleaseBody.PushLast(Sema.ActOnExprStmt(
			ReleaseFunction, Release, Range));
		const asASTStmtId ReleaseRegion = Sema.ActOnCompoundStmt(
			ReleaseFunction, ReleaseBody, Range);
		Context.SetBody(ReleaseFunction, ReleaseRegion);
		asSASTLifetimeRecord ReleaseRecord;
		ReleaseRecord.subjectKind = asAST_LIFETIME_SUBJECT_LOCAL;
		ReleaseRecord.subject = asSASTLifetimeNodeId::FromDecl(ReferenceLocal);
		ReleaseRecord.actionKind = asAST_LIFETIME_ACTION_RELEASE_REFERENCE;
		ReleaseRecord.actionTarget = asSASTLifetimeNodeId::FromDecl(ReferenceLocal);
		ReleaseRecord.activationPoint =
			asSASTLifetimeNodeId::FromExpr(ReferenceAssignment);
		ReleaseRecord.semanticRegion =
			asSASTLifetimeNodeId::FromStmt(ReleaseRegion);
		ReleaseRecord.phase = asAST_LIFETIME_PHASE_LEXICAL_SCOPE;
		ReleaseRecord.supportedExitMask = asAST_LIFETIME_EXIT_ALL;
		ReleaseRecord.order = 0;
		Context.AddLifetimeRecord(ReleaseRecord);
		Sema.FinishDecl(ReleaseFunction);

		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FTrackedValue");
		const asASTDeclId ValueClass = Sema.ActOnClassDecl(
			Tu, "FTrackedValue", Range);
		Context.AddDeclTrait(ValueClass, asAST_TRAIT_VALUE);
		const asASTDeclId Destructor = Sema.ActOnDestructorDecl(
			ValueClass, "FTrackedValue", Range);
		const asASTDeclId DestructorFunction = Sema.ActOnFunctionDecl(
			Tu, "DestroyLocal", VoidType, Range);
		const asASTDeclId ValueLocal = Sema.ActOnVarDecl(
			DestructorFunction, "Value", ValueType, Range);
		const asASTStmtId ValueDeclStmt = Context.CreateStmt(
			asAST_STMT_DECL, DestructorFunction, Range);
		Context.SetStmtDecl(ValueDeclStmt, ValueLocal);
		const asASTExprId ValueInitTarget = Sema.ActOnDeclRef(
			ValueLocal, ValueType, Range);
		const asASTExprId ValueInitValue = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, ValueType,
			asAST_VALUE_PRVALUE, Range);
		Context.SetLiteral(ValueInitValue, "0");
		const asASTExprId ValueAssignment = Context.CreateExpr(
			asAST_EXPR_ASSIGN, ValueType,
			asAST_VALUE_LVALUE, Range);
		Context.SetLiteral(ValueAssignment, "=");
		Context.AddExprChild(ValueAssignment, ValueInitTarget);
		Context.AddExprChild(ValueAssignment, ValueInitValue);
		Context.AddDeclInit(ValueLocal, ValueAssignment);
		const asASTExprId DestructorTarget = Sema.ActOnDeclRef(
			ValueLocal, ValueType, Range);
		const asASTExprId ScopeExit = Sema.ActOnCleanup(
			DestructorTarget, Range);
		Context.SetLiteral(ScopeExit, "scope-exit");
		Context.SetResolvedDecl(ScopeExit, Destructor);
		asCArray<asASTStmtId> DestructorBody;
		DestructorBody.PushLast(ValueDeclStmt);
		DestructorBody.PushLast(Sema.ActOnExprStmt(
			DestructorFunction, ValueAssignment, Range));
		DestructorBody.PushLast(Sema.ActOnExprStmt(
			DestructorFunction, ScopeExit, Range));
		const asASTStmtId DestructorRegion = Sema.ActOnCompoundStmt(
			DestructorFunction, DestructorBody, Range);
		Context.SetBody(DestructorFunction, DestructorRegion);
		asSASTLifetimeRecord DestructorRecord;
		DestructorRecord.subjectKind = asAST_LIFETIME_SUBJECT_LOCAL;
		DestructorRecord.subject = asSASTLifetimeNodeId::FromDecl(ValueLocal);
		DestructorRecord.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
		DestructorRecord.actionTarget = asSASTLifetimeNodeId::FromDecl(Destructor);
		DestructorRecord.activationPoint =
			asSASTLifetimeNodeId::FromExpr(ValueAssignment);
		DestructorRecord.semanticRegion =
			asSASTLifetimeNodeId::FromStmt(DestructorRegion);
		DestructorRecord.phase = asAST_LIFETIME_PHASE_LEXICAL_SCOPE;
		DestructorRecord.supportedExitMask = asAST_LIFETIME_EXIT_ALL;
		DestructorRecord.order = 1;
		Context.AddLifetimeRecord(DestructorRecord);
		Sema.FinishDecl(DestructorFunction);

		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("non-empty Canonical lifetime facts fixture must seal")));

		const FAngelscriptTypedASTJITCanonicalLifetimeFacts ReleaseFacts =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				Context, ReleaseFunction);
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCleanupPlanState::NonEmpty,
			ReleaseFacts.CleanupPlanState,
			TEXT("an exact scope-release action must publish a non-empty cleanup plan")));
		ASSERT_THAT(IsTrue(ReleaseFacts.bCleanupPlanCoversAllTransfers,
			TEXT("the authenticated release protocol proves every exit of this no-transfer body")));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(asAST_LIFETIME_PROTOCOL_REVISION),
			ReleaseFacts.LifetimeSummary.ProtocolRevision));
		ASSERT_THAT(IsFalse(ReleaseFacts.LifetimeSummary.StableKey.IsZero(),
			TEXT("the detached lifetime summary needs a stable semantic key")));
		ASSERT_THAT(IsFalse(ReleaseFacts.LifetimeSummary.ABIKey.IsZero(),
			TEXT("the detached lifetime summary needs a required ABI key")));
		ASSERT_THAT(AreEqual(1u, ReleaseFacts.LifetimeSummary.RecordCount));
		ASSERT_THAT(AreEqual(1u, ReleaseFacts.LifetimeSummary.ExitPlanCount));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(
				EAngelscriptTypedASTJITLifetimeActionFlags::ReleaseReference),
			static_cast<uint32>(ReleaseFacts.LifetimeSummary.ActionFlags)));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(
				EAngelscriptTypedASTJITLifetimeExitPlanFlags::Normal),
			static_cast<uint32>(ReleaseFacts.LifetimeSummary.ExitPlanFlags)));
		ASSERT_THAT(IsFalse(
			ReleaseFacts.LifetimeSummary.bNativeObjectFrameABIAvailable,
			TEXT("a reference release cannot publish partial native cleanup ABI")));

		const FAngelscriptTypedASTJITCanonicalLifetimeFacts DestructorFacts =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				Context, DestructorFunction);
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCleanupPlanState::ScriptDestructor,
			DestructorFacts.CleanupPlanState,
			TEXT("an exact scope-exit destructor action must retain its stronger classification")));
		ASSERT_THAT(IsTrue(DestructorFacts.bCleanupPlanCoversAllTransfers,
			TEXT("the authenticated destructor protocol proves every exit of this no-transfer body")));
		ASSERT_THAT(AreEqual(1u,
			DestructorFacts.LifetimeSummary.RecordCount));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(
				EAngelscriptTypedASTJITLifetimeActionFlags::DestroyValue),
			static_cast<uint32>(
				DestructorFacts.LifetimeSummary.ActionFlags)));
		ASSERT_THAT(IsFalse(
			DestructorFacts.LifetimeSummary.bNativeObjectFrameABIAvailable,
			TEXT("a script destructor cannot publish partial native cleanup ABI")));
	}

	TEST_METHOD(CanonicalLifetimeFactsPublishAuthenticatedEmptyNativeABI)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asCSourceRange Range;
		const asASTDeclId Tu = Sema.ActOnTranslationUnit(
			"JitAuthenticatedEmptyLifetime");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Function = Sema.ActOnFunctionDecl(
			Tu, "Run", IntType, Range);
		asCArray<asASTStmtId> Body;
		Body.PushLast(Sema.ActOnReturnStmt(
			Function, Sema.ActOnIntegerLiteral(7, Range), Range));
		Context.SetBody(Function, Sema.ActOnCompoundStmt(
			Function, Body, Range));
		Sema.FinishDecl(Function);
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("empty lifetime summary fixture must seal")));

		const FAngelscriptTypedASTJITCanonicalLifetimeFacts Facts =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				Context, Function);
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCleanupPlanState::VerifiedEmpty,
			Facts.CleanupPlanState));
		ASSERT_THAT(IsTrue(Facts.bCleanupPlanCoversAllTransfers));
		ASSERT_THAT(IsFalse(Facts.LifetimeSummary.StableKey.IsZero()));
		ASSERT_THAT(IsFalse(Facts.LifetimeSummary.ABIKey.IsZero()));
		ASSERT_THAT(AreEqual(0u, Facts.LifetimeSummary.RecordCount));
		ASSERT_THAT(AreEqual(0u, Facts.LifetimeSummary.ExitPlanCount));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(
				EAngelscriptTypedASTJITLifetimeActionFlags::None),
			static_cast<uint32>(Facts.LifetimeSummary.ActionFlags)));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(
				EAngelscriptTypedASTJITLifetimeExitPlanFlags::None),
			static_cast<uint32>(Facts.LifetimeSummary.ExitPlanFlags)));
		ASSERT_THAT(IsTrue(
			Facts.LifetimeSummary.bNativeObjectFrameABIAvailable,
			TEXT("an authenticated empty plan has the only currently implemented native lifetime ABI")));
	}

	TEST_METHOD(CanonicalTypedASTJITLifetimeAnalysisConsumesOnlyProtocolView)
	{
		const FString SourcePath = FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCanonical.cpp"));
		FString Source;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(Source, *SourcePath),
			TEXT("read the maintained TypedASTJIT canonical adapter source")));

		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("LifetimeView.GetRecord(")),
			TEXT("AOT lifetime facts must consume authenticated view records")));
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("LifetimeView.GetExitPlan(")),
			TEXT("AOT lifetime facts must consume authenticated exit plans")));
		ASSERT_THAT(IsFalse(Source.Contains(
			TEXT("TryGetCanonicalLexicalCleanupRequirement")),
			TEXT("AOT must not reclassify lifetime from DeclStmt/type shape")));
		ASSERT_THAT(IsFalse(Source.Contains(
			TEXT("TryReadCanonicalLexicalCleanupAction")),
			TEXT("AOT must not decode compatibility cleanup statements")));
		ASSERT_THAT(IsFalse(Source.Contains(
			TEXT("FindCanonicalLifetimeDestructor")),
			TEXT("AOT must not scan declarations by destructor spelling")));
		ASSERT_THAT(IsFalse(Source.Contains(TEXT("LifetimeView.FindCleanupBinding("))));
		ASSERT_THAT(IsFalse(Source.Contains(TEXT("LifetimeView.GetCleanupBinding("))));
	}

	TEST_METHOD(CanonicalBytecodeFallbackKeepsLegacyProviderDestructorBoundary)
	{
		const FString SourcePath = FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/BytecodeJIT/AngelscriptBytecodeJIT.cpp"));
		FString Source;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(Source, *SourcePath),
			TEXT("read the maintained BytecodeJIT provider adapter source")));

		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("IsLegacyProviderAdoptionCompatible")),
			TEXT("the canonical fallback chain must name its temporary LEGACY adoption boundary")));
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("ScriptFunction->traits.GetTrait(asTRAIT_DESTRUCTOR)")),
			TEXT("provider compatibility must use the authoritative function trait")));
		ASSERT_THAT(IsFalse(Source.Contains(
			TEXT("CanonicalDeclaration.StartsWith(TEXT(\"~\")")),
			TEXT("provider compatibility must not infer destructors from declaration spelling")));
	}

	TEST_METHOD(CanonicalLifetimeFactsProveReverseLiveOnlyTransferCoverage)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asCSourceRange Range;
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("JitLifetimeCoverage");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FTrackedLifetime");
		const asASTDeclId ValueClass = Sema.ActOnClassDecl(
			Tu, "FTrackedLifetime", Range);
		Context.AddDeclTrait(ValueClass, asAST_TRAIT_VALUE);
		const asASTDeclId Destructor = Sema.ActOnDestructorDecl(
			ValueClass, "FTrackedLifetime", Range);

		auto MakeDeclStmt = [&](const asASTDeclId Owner, const asASTDeclId Local)
		{
			const asASTStmtId Statement = Context.CreateStmt(
				asAST_STMT_DECL, Owner, Range);
			Context.SetStmtDecl(Statement, Local);
			return Statement;
		};
		auto MakeScopeExit = [&](const asASTDeclId Owner, const asASTDeclId Local)
		{
			const asASTExprId Target = Sema.ActOnDeclRef(Local, ValueType, Range);
			const asASTExprId Cleanup = Sema.ActOnCleanup(Target, Range);
			Context.SetLiteral(Cleanup, "scope-exit");
			Context.SetResolvedDecl(Cleanup, Destructor);
			return Sema.ActOnExprStmt(Owner, Cleanup, Range);
		};

		const asASTDeclId ProvenFunction = Sema.ActOnFunctionDecl(
			Tu, "ProvenLifetime", IntType, Range);
		const asASTDeclId ProvenLocal = Sema.ActOnVarDecl(
			ProvenFunction, "Later", ValueType, Range);
		const asASTExprId ProvenInitTarget = Sema.ActOnDeclRef(
			ProvenLocal, ValueType, Range);
		const asASTExprId ProvenInitValue = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, ValueType,
			asAST_VALUE_PRVALUE, Range);
		Context.SetLiteral(ProvenInitValue, "0");
		const asASTExprId ProvenAssignment = Context.CreateExpr(
			asAST_EXPR_ASSIGN, ValueType,
			asAST_VALUE_LVALUE, Range);
		Context.SetLiteral(ProvenAssignment, "=");
		Context.AddExprChild(ProvenAssignment, ProvenInitTarget);
		Context.AddExprChild(ProvenAssignment, ProvenInitValue);
		Context.AddDeclInit(ProvenLocal, ProvenAssignment);
		const asASTStmtId EarlyReturn = Sema.ActOnReturnStmt(
			ProvenFunction, Sema.ActOnIntegerLiteral(1, Range), Range);
		const asASTStmtId EarlyIf = Sema.ActOnIfStmt(
			ProvenFunction, Sema.ActOnBoolLiteral(true, Range),
			EarlyReturn, asASTStmtId(), Range);
		const asASTStmtId LateReturn = Sema.ActOnReturnStmt(
			ProvenFunction, Sema.ActOnIntegerLiteral(2, Range), Range);
		Context.AddStmtChild(
			LateReturn, MakeScopeExit(ProvenFunction, ProvenLocal));
		asCArray<asASTStmtId> ProvenBody;
		ProvenBody.PushLast(EarlyIf);
		ProvenBody.PushLast(MakeDeclStmt(ProvenFunction, ProvenLocal));
		ProvenBody.PushLast(Sema.ActOnExprStmt(
			ProvenFunction, ProvenAssignment, Range));
		ProvenBody.PushLast(LateReturn);
		ProvenBody.PushLast(MakeScopeExit(ProvenFunction, ProvenLocal));
		const asASTStmtId ProvenRegion = Sema.ActOnCompoundStmt(
			ProvenFunction, ProvenBody, Range);
		Context.SetBody(ProvenFunction, ProvenRegion);
		asSASTLifetimeRecord ProvenRecord;
		ProvenRecord.subjectKind = asAST_LIFETIME_SUBJECT_LOCAL;
		ProvenRecord.subject = asSASTLifetimeNodeId::FromDecl(ProvenLocal);
		ProvenRecord.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
		ProvenRecord.actionTarget = asSASTLifetimeNodeId::FromDecl(Destructor);
		ProvenRecord.activationPoint =
			asSASTLifetimeNodeId::FromExpr(ProvenAssignment);
		ProvenRecord.semanticRegion =
			asSASTLifetimeNodeId::FromStmt(ProvenRegion);
		ProvenRecord.phase = asAST_LIFETIME_PHASE_LEXICAL_SCOPE;
		ProvenRecord.supportedExitMask = asAST_LIFETIME_EXIT_ALL;
		ProvenRecord.order = 0;
		Context.AddLifetimeRecord(ProvenRecord);
		Sema.FinishDecl(ProvenFunction);

		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("reverse live-only lifetime coverage fixture must seal")));

		const FAngelscriptTypedASTJITCanonicalLifetimeFacts ProvenFacts =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				Context, ProvenFunction);
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCleanupPlanState::ScriptDestructor,
			ProvenFacts.CleanupPlanState,
			TEXT("the exact scope-exit plan must retain destructor classification")));
		ASSERT_THAT(IsTrue(ProvenFacts.bCleanupPlanCoversAllTransfers,
			TEXT("early-before-decl, late-after-decl, and normal exit must prove exact live-only coverage")));
	}

	TEST_METHOD(SourceCanonicalLifetimeFactsConsumeSemaLiveOnlyPlans)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("source lifetime facts fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"JitCanonicalLifetimeFacts", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("source lifetime facts module must exist")));
		ASSERT_THAT(AreEqual(0,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			struct FJitScopeValue
			{
				int Value = 2;
			}

			int ProveSourceLifetime(bool Early)
			{
				if (Early)
					return 1;
				FJitScopeValue Later;
				return 2;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"JitCanonicalLifetimeFacts.as", Source.c_str(),
			static_cast<unsigned int>(Source.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			*FString::Printf(TEXT(
				"source lifetime facts fixture must compile and seal: %s"),
				*CollectMessages(Engine.GetMessages()))));

		const asCModule* const Concrete = static_cast<asCModule*>(Module);
		const asCASTContext* const Context = Concrete != nullptr
			? Concrete->GetCanonicalASTContext() : nullptr;
		ASSERT_THAT(IsNotNull(Context,
			TEXT("source lifetime facts fixture must retain sealed AST")));
		if (Context == nullptr)
		{
			return;
		}
		const asASTDeclId FunctionDecl =
			FindAngelscriptTypedASTJITCanonicalFunctionDecl(
				*Context, "ProveSourceLifetime");
		ASSERT_THAT(IsTrue(FunctionDecl.IsValid(),
			TEXT("source lifetime facts fixture needs one exact function")));

		const FAngelscriptTypedASTJITCanonicalLifetimeFacts Facts =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				*Context, FunctionDecl);
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCleanupPlanState::ScriptDestructor,
			Facts.CleanupPlanState,
			TEXT("source Sema scope-exit actions must retain destructor classification")));
		ASSERT_THAT(IsTrue(Facts.bCleanupPlanCoversAllTransfers,
			TEXT("AOT facts must prove the exact source-produced live-only plan")));
		ASSERT_THAT(IsFalse(Facts.LifetimeSummary.StableKey.IsZero()));
		ASSERT_THAT(IsFalse(Facts.LifetimeSummary.ABIKey.IsZero()));
		ASSERT_THAT(IsFalse(
			Facts.LifetimeSummary.bNativeObjectFrameABIAvailable));

		FAngelscriptTypedASTJITEligibilityInput EligibilityInput;
		EligibilityInput.bIsUFunctionRoot = true;
		EligibilityInput.Purpose =
			EAngelscriptTypedASTJITEligibilityPurpose::RootCallClosure;
		EligibilityInput.SealedAST = Context;
		EligibilityInput.CanonicalFunctionDecl = FunctionDecl;
		EligibilityInput.Shape.SymbolName = TEXT("ASJIT_ProveSourceLifetime");
		EligibilityInput.Shape.ReturnType =
			asCDataType::CreatePrimitive(ttInt, false);
		const FAngelscriptTypedASTJITEligibility Eligibility =
			EvaluateAngelscriptTypedASTJITEligibilityFromCanonical(
				EligibilityInput);
		ASSERT_THAT(IsFalse(Eligibility.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime,
			Eligibility.Reason,
			TEXT("one authenticated non-empty plan must fall back only this function")));
		ASSERT_THAT(IsTrue(Eligibility.Detail.Contains(
			TEXT("AuthenticatedLifetimeRequiresNativeObjectFrameABI")),
			TEXT("fallback must name the missing native lifetime ABI, not an inferred destructor")));

		FAngelscriptTypedASTJITEmitOptions EmitOptions;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(
				*Context, FunctionDecl, EligibilityInput.Shape, EmitOptions);
		ASSERT_THAT(IsFalse(Emission.bSuccess));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime,
			Emission.FailureReason,
			TEXT("AOT must not publish a partial native cleanup plan")));
	}

	TEST_METHOD(SourceConstructorPartialConstructionPublishesAuthenticatedTypedFallback)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("constructor lifetime fallback fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"JitConstructorPartialConstruction", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			struct FJitCommittedPart
			{
				int Value = 1;
			}

			class FJitCommittedBase
			{
				FJitCommittedPart BasePart;
			}

			class FJitCommittedDerived : FJitCommittedBase
			{
				FJitCommittedPart First;
				FJitCommittedPart Second;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"JitConstructorPartialConstruction.as", Source.c_str(),
			static_cast<unsigned int>(Source.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			*FString::Printf(TEXT(
				"constructor lifetime fallback fixture must compile and seal: %s"),
				*CollectMessages(Engine.GetMessages()))));

		const asCASTContext* const Context =
			static_cast<asCModule*>(Module)->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("constructor lifetime fallback fixture must retain its sealed snapshot")));
		if (Context == nullptr)
		{
			return;
		}

		const asCDecl* DerivedClass = nullptr;
		const asCDecl* DerivedConstructor = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Decl = Context->GetDecl(asASTDeclId(DeclIndex));
			if (Decl != nullptr && Decl->kind == asAST_DECL_CLASS
				&& Decl->name.Equals("FJitCommittedDerived"))
			{
				DerivedClass = Decl;
			}
		}
		for (asUINT DeclIndex = 1;
			DerivedClass != nullptr && DeclIndex <= Context->GetDeclCount();
			++DeclIndex)
		{
			const asCDecl* const Decl = Context->GetDecl(asASTDeclId(DeclIndex));
			if (Decl != nullptr && Decl->kind == asAST_DECL_CONSTRUCTOR
				&& Decl->parent == DerivedClass->id
				&& (Decl->traits & asAST_TRAIT_GENERATED) != 0)
			{
				DerivedConstructor = Decl;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(DerivedConstructor,
			TEXT("fixture must expose the exact generated derived constructor")));
		if (DerivedConstructor == nullptr)
		{
			return;
		}

		const FAngelscriptTypedASTJITCanonicalLifetimeFacts Facts =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				*Context, DerivedConstructor->id);
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCleanupPlanState::PartialConstruction,
			Facts.CleanupPlanState,
			TEXT("base/member/complete commits must retain the partial-construction classification")));
		ASSERT_THAT(IsTrue(Facts.LifetimeSummary.IsAuthenticated()));
		ASSERT_THAT(IsFalse(Facts.LifetimeSummary.StableKey.IsZero()));
		ASSERT_THAT(IsFalse(Facts.LifetimeSummary.ABIKey.IsZero()));
		ASSERT_THAT(AreEqual(4u, Facts.LifetimeSummary.RecordCount));
		ASSERT_THAT(AreEqual(0u, Facts.LifetimeSummary.ExitPlanCount,
			TEXT("constructor commits are not lexical transfer cleanup plans")));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(
				EAngelscriptTypedASTJITLifetimeActionFlags::DestroyValue),
			static_cast<uint32>(Facts.LifetimeSummary.ActionFlags)));
		ASSERT_THAT(IsTrue(Facts.bHasExceptionCleanup));
		ASSERT_THAT(IsTrue(Facts.bCleanupPlanCoversAllTransfers,
			TEXT("the shared verifier-authenticated construction view is complete even though native emission lacks its ABI")));
		ASSERT_THAT(IsFalse(
			Facts.LifetimeSummary.bNativeObjectFrameABIAvailable,
			TEXT("the current TypedASTJIT profile must not claim a native committed-prefix frame ABI")));

		FAngelscriptTypedASTJITEligibilityInput EligibilityInput;
		EligibilityInput.bIsUFunctionRoot = true;
		EligibilityInput.Purpose =
			EAngelscriptTypedASTJITEligibilityPurpose::RootCallClosure;
		EligibilityInput.SealedAST = Context;
		EligibilityInput.CanonicalFunctionDecl = DerivedConstructor->id;
		EligibilityInput.Shape.SymbolName =
			TEXT("ASJIT_FJitCommittedDerived_Constructor");
		EligibilityInput.Shape.ReturnType =
			asCDataType::CreatePrimitive(ttVoid, false);
		const FAngelscriptTypedASTJITEligibility Eligibility =
			EvaluateAngelscriptTypedASTJITEligibilityFromCanonical(
				EligibilityInput);
		ASSERT_THAT(IsFalse(Eligibility.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime,
			Eligibility.Reason,
			TEXT("an authenticated constructor plan must fall back for its missing native object-frame ABI")));
		ASSERT_THAT(IsTrue(Eligibility.Detail.Contains(
			TEXT("AuthenticatedConstructorPartialConstructionRequiresNativeObjectFrameABI")),
			TEXT("the fallback must distinguish an unsupported authenticated construction plan from an invalid plan")));

		FAngelscriptTypedASTJITEmitOptions EmitOptions;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(
				*Context, DerivedConstructor->id,
				EligibilityInput.Shape, EmitOptions);
		ASSERT_THAT(IsFalse(Emission.bSuccess));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime,
			Emission.FailureReason,
			TEXT("AOT must reject only this constructor instead of publishing a partial native cleanup plan")));
	}

	TEST_METHOD(AggregatePartialConstructionPublishesStableTypedFallback)
	{
		asCASTContext Context;
		const asASTDeclId TranslationUnit =
			Context.CreateTranslationUnit("JitAggregatePartialConstruction");
		const asCQualType VoidType = Context.InternPrimitive(ttVoid);
		const asCQualType ElementType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FJitAggregateElement", 0);
		const asCQualType AggregateType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FJitAggregateBox", 0);

		const asASTDeclId ElementClass = Context.CreateDecl(
			asAST_DECL_CLASS, TranslationUnit,
			asCSourceRange(), "FJitAggregateElement");
		Context.SetStableKey(ElementClass, "FJitAggregateElement");
		Context.SetDeclType(ElementClass, ElementType);
		Context.SetDeclLayout(ElementClass, 4, 4);
		const asASTDeclId ElementDestructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, ElementClass,
			asCSourceRange(), "FJitAggregateElement");
		Context.SetStableKey(
			ElementDestructor, "FJitAggregateElement::~FJitAggregateElement()");
		Context.SetDeclType(ElementDestructor, VoidType);

		const asASTDeclId AggregateClass = Context.CreateDecl(
			asAST_DECL_CLASS, TranslationUnit,
			asCSourceRange(), "FJitAggregateBox");
		Context.SetStableKey(AggregateClass, "FJitAggregateBox");
		Context.SetDeclType(AggregateClass, AggregateType);
		Context.SetDeclLayout(AggregateClass, 8, 8);
		const asASTDeclId ListFactory = Context.CreateDecl(
			asAST_DECL_CONSTRUCTOR, AggregateClass,
			asCSourceRange(), "FJitAggregateBox");
		Context.SetStableKey(ListFactory, "FJitAggregateBox::list_factory");
		Context.SetDeclType(ListFactory, VoidType);
		Context.AddDeclTrait(ListFactory, asAST_TRAIT_LIST_FACTORY);

		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, TranslationUnit,
			asCSourceRange(), "BuildAggregate");
		Context.SetStableKey(Function, "BuildAggregate()");
		Context.SetDeclType(Function, VoidType);
		const asASTStmtId FunctionBody = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		Context.SetBody(Function, FunctionBody);

		const asASTExprId Aggregate = Context.CreateExpr(
			asAST_EXPR_CONSTRUCT, AggregateType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		Context.SetLiteral(Aggregate, "list-pattern");
		Context.SetResolvedDecl(Aggregate, ListFactory);
		for (asUINT ElementIndex = 0; ElementIndex < 3; ++ElementIndex)
		{
			const asASTExprId Element = Context.CreateExpr(
				asAST_EXPR_CONSTRUCT, ElementType,
				asAST_VALUE_PRVALUE, asCSourceRange());
			Context.SetLiteral(Element, "FJitAggregateElement()");
			Context.AddExprChild(Aggregate, Element);
		}
		const asASTStmtId AggregateStatement = Context.CreateStmt(
			asAST_STMT_EXPR, Function, asCSourceRange());
		Context.SetStmtExpr(AggregateStatement, Aggregate);
		Context.AddStmtChild(FunctionBody, AggregateStatement);

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
		Record.constructionStep = 0;
		Record.order = 0;
		Record.completeObjectCommit = false;
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Record)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("aggregate committed-prefix protocol must seal before AOT analysis")));

		const FAngelscriptTypedASTJITCanonicalLifetimeFacts FirstFacts =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				Context, Function);
		const FAngelscriptTypedASTJITCanonicalLifetimeFacts SecondFacts =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				Context, Function);
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCleanupPlanState::PartialConstruction,
			FirstFacts.CleanupPlanState,
			TEXT("aggregate progress must retain the authenticated partial-construction classification")));
		ASSERT_THAT(IsTrue(FirstFacts.LifetimeSummary.IsAuthenticated()));
		ASSERT_THAT(AreEqual(1u, FirstFacts.LifetimeSummary.RecordCount));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(
				EAngelscriptTypedASTJITLifetimeActionFlags::DestroyElement),
			static_cast<uint32>(FirstFacts.LifetimeSummary.ActionFlags)));
		ASSERT_THAT(IsTrue(FirstFacts.bHasExceptionCleanup));
		ASSERT_THAT(IsTrue(FirstFacts.bCleanupPlanCoversAllTransfers));
		ASSERT_THAT(IsFalse(
			FirstFacts.LifetimeSummary.bNativeObjectFrameABIAvailable,
			TEXT("TypedASTJIT has no reviewed native aggregate element-frame ABI")));
		ASSERT_THAT(IsTrue(
			FirstFacts.LifetimeSummary.StableKey
				== SecondFacts.LifetimeSummary.StableKey
			&& FirstFacts.LifetimeSummary.ABIKey
				== SecondFacts.LifetimeSummary.ABIKey,
			TEXT("aggregate lifetime summaries must be deterministic and pointer-free")));

		FAngelscriptTypedASTJITEligibilityInput EligibilityInput;
		EligibilityInput.bIsUFunctionRoot = true;
		EligibilityInput.Purpose =
			EAngelscriptTypedASTJITEligibilityPurpose::RootCallClosure;
		EligibilityInput.SealedAST = &Context;
		EligibilityInput.CanonicalFunctionDecl = Function;
		EligibilityInput.Shape.SymbolName = TEXT("ASJIT_BuildAggregate");
		EligibilityInput.Shape.ReturnType =
			asCDataType::CreatePrimitive(ttVoid, false);
		const FAngelscriptTypedASTJITEligibility Eligibility =
			EvaluateAngelscriptTypedASTJITEligibilityFromCanonical(
				EligibilityInput);
		ASSERT_THAT(IsFalse(Eligibility.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime,
			Eligibility.Reason,
			TEXT("unsupported native aggregate frames must fall back only this function")));
		ASSERT_THAT(IsTrue(Eligibility.Detail.Contains(
			TEXT("AuthenticatedAggregatePartialConstructionRequiresNativeElementFrameABI")),
			TEXT("fallback must distinguish an authenticated aggregate cursor from an invalid plan")));

		FAngelscriptTypedASTJITEmitOptions EmitOptions;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(
				Context, Function, EligibilityInput.Shape, EmitOptions);
		ASSERT_THAT(IsFalse(Emission.bSuccess));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime,
			Emission.FailureReason,
			TEXT("AOT must not publish a partial native aggregate cleanup plan")));
	}

	TEST_METHOD(AggregateCommittedCursorShapeParticipatesInTypedIdentity)
	{
		bool bFixturesSealed = true;
		auto AnalyzeAggregate = [&](const asUINT ElementCount)
			-> FAngelscriptTypedASTJITCanonicalLifetimeFacts
		{
			asCASTContext Context;
			const asASTDeclId TranslationUnit =
				Context.CreateTranslationUnit("JitAggregateCursorIdentity");
			const asCQualType VoidType = Context.InternPrimitive(ttVoid);
			const asCQualType ElementType = Context.InternNamedType(
				asAST_TYPE_VALUE_OBJECT, "FJitCursorElement", 0);
			const asCQualType AggregateType = Context.InternNamedType(
				asAST_TYPE_VALUE_OBJECT, "FJitCursorBox", 0);

			const asASTDeclId ElementClass = Context.CreateDecl(
				asAST_DECL_CLASS, TranslationUnit,
				asCSourceRange(), "FJitCursorElement");
			Context.SetStableKey(ElementClass, "FJitCursorElement");
			Context.SetDeclType(ElementClass, ElementType);
			Context.SetDeclLayout(ElementClass, 4, 4);
			const asASTDeclId ElementDestructor = Context.CreateDecl(
				asAST_DECL_DESTRUCTOR, ElementClass,
				asCSourceRange(), "FJitCursorElement");
			Context.SetStableKey(
				ElementDestructor, "FJitCursorElement::~FJitCursorElement()");
			Context.SetDeclType(ElementDestructor, VoidType);

			const asASTDeclId AggregateClass = Context.CreateDecl(
				asAST_DECL_CLASS, TranslationUnit,
				asCSourceRange(), "FJitCursorBox");
			Context.SetStableKey(AggregateClass, "FJitCursorBox");
			Context.SetDeclType(AggregateClass, AggregateType);
			Context.SetDeclLayout(AggregateClass, 8, 8);
			const asASTDeclId ListFactory = Context.CreateDecl(
				asAST_DECL_CONSTRUCTOR, AggregateClass,
				asCSourceRange(), "FJitCursorBox");
			Context.SetStableKey(ListFactory, "FJitCursorBox::list_factory");
			Context.SetDeclType(ListFactory, VoidType);
			Context.AddDeclTrait(ListFactory, asAST_TRAIT_LIST_FACTORY);

			const asASTDeclId Function = Context.CreateDecl(
				asAST_DECL_FUNCTION, TranslationUnit,
				asCSourceRange(), "BuildCursorAggregate");
			Context.SetStableKey(Function, "BuildCursorAggregate()");
			Context.SetDeclType(Function, VoidType);
			const asASTStmtId FunctionBody = Context.CreateStmt(
				asAST_STMT_BLOCK, Function, asCSourceRange());
			Context.SetBody(Function, FunctionBody);

			const asASTExprId Aggregate = Context.CreateExpr(
				asAST_EXPR_CONSTRUCT, AggregateType,
				asAST_VALUE_PRVALUE, asCSourceRange());
			Context.SetLiteral(Aggregate, "list-pattern");
			Context.SetResolvedDecl(Aggregate, ListFactory);
			for (asUINT ElementIndex = 0;
				ElementIndex < ElementCount; ++ElementIndex)
			{
				const asASTExprId Element = Context.CreateExpr(
					asAST_EXPR_CONSTRUCT, ElementType,
					asAST_VALUE_PRVALUE, asCSourceRange());
				Context.SetLiteral(Element, "FJitCursorElement()");
				Context.AddExprChild(Aggregate, Element);
			}
			const asASTStmtId AggregateStatement = Context.CreateStmt(
				asAST_STMT_EXPR, Function, asCSourceRange());
			Context.SetStmtExpr(AggregateStatement, Aggregate);
			Context.AddStmtChild(FunctionBody, AggregateStatement);

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
			Record.order = 0;
			bFixturesSealed &= Context.AddLifetimeRecord(Record) == 0;
			bFixturesSealed &= Context.Seal() == 0;
			if (!bFixturesSealed)
			{
				return FAngelscriptTypedASTJITCanonicalLifetimeFacts{};
			}
			return AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				Context, Function);
		};

		const FAngelscriptTypedASTJITCanonicalLifetimeFacts TwoElements =
			AnalyzeAggregate(2);
		const FAngelscriptTypedASTJITCanonicalLifetimeFacts ThreeElements =
			AnalyzeAggregate(3);
		ASSERT_THAT(IsTrue(bFixturesSealed,
			TEXT("both aggregate cursor identity fixtures must seal")));
		ASSERT_THAT(IsTrue(TwoElements.LifetimeSummary.IsAuthenticated()));
		ASSERT_THAT(IsTrue(ThreeElements.LifetimeSummary.IsAuthenticated()));
		ASSERT_THAT(IsTrue(
			TwoElements.LifetimeSummary.StableKey
				!= ThreeElements.LifetimeSummary.StableKey,
			TEXT("stable lifetime identity must include the aggregate commit-point count")));
		ASSERT_THAT(IsTrue(
			TwoElements.LifetimeSummary.ABIKey
				!= ThreeElements.LifetimeSummary.ABIKey,
			TEXT("native aggregate frame ABI identity must include the committed-cursor upper bound")));
	}

	TEST_METHOD(NestedAggregateParentOrdinalParticipatesInTypedIdentityAndFallback)
	{
		bool bFixturesSealed = true;
		auto BuildNestedAggregate = [&](asCASTContext& Context,
			const bool bNestedFirst) -> asASTDeclId
		{
			const asASTDeclId TranslationUnit =
				Context.CreateTranslationUnit("JitNestedAggregateIdentity");
			const asCQualType VoidType = Context.InternPrimitive(ttVoid);
			const asCQualType ElementType = Context.InternNamedType(
				asAST_TYPE_VALUE_OBJECT, "FJitNestedElement", 0);
			const asCQualType AggregateType = Context.InternNamedType(
				asAST_TYPE_VALUE_OBJECT, "FJitNestedBox", 0);

			const asASTDeclId ElementClass = Context.CreateDecl(
				asAST_DECL_CLASS, TranslationUnit,
				asCSourceRange(), "FJitNestedElement");
			Context.SetStableKey(ElementClass, "FJitNestedElement");
			Context.SetDeclType(ElementClass, ElementType);
			Context.SetDeclLayout(ElementClass, 4, 4);
			const asASTDeclId ElementConstructor = Context.CreateDecl(
				asAST_DECL_CONSTRUCTOR, ElementClass,
				asCSourceRange(), "FJitNestedElement");
			Context.SetStableKey(
				ElementConstructor, "FJitNestedElement::FJitNestedElement()");
			Context.SetDeclType(ElementConstructor, VoidType);
			const asASTDeclId ElementListFactory = Context.CreateDecl(
				asAST_DECL_CONSTRUCTOR, ElementClass,
				asCSourceRange(), "FJitNestedElement");
			Context.SetStableKey(
				ElementListFactory, "FJitNestedElement::list_factory");
			Context.SetDeclType(ElementListFactory, VoidType);
			Context.AddDeclTrait(
				ElementListFactory, asAST_TRAIT_LIST_FACTORY);
			const asASTDeclId ElementDestructor = Context.CreateDecl(
				asAST_DECL_DESTRUCTOR, ElementClass,
				asCSourceRange(), "FJitNestedElement");
			Context.SetStableKey(
				ElementDestructor,
				"FJitNestedElement::~FJitNestedElement()");
			Context.SetDeclType(ElementDestructor, VoidType);

			const asASTDeclId AggregateClass = Context.CreateDecl(
				asAST_DECL_CLASS, TranslationUnit,
				asCSourceRange(), "FJitNestedBox");
			Context.SetStableKey(AggregateClass, "FJitNestedBox");
			Context.SetDeclType(AggregateClass, AggregateType);
			Context.SetDeclLayout(AggregateClass, 8, 8);
			const asASTDeclId AggregateListFactory = Context.CreateDecl(
				asAST_DECL_CONSTRUCTOR, AggregateClass,
				asCSourceRange(), "FJitNestedBox");
			Context.SetStableKey(
				AggregateListFactory, "FJitNestedBox::list_factory");
			Context.SetDeclType(AggregateListFactory, VoidType);
			Context.AddDeclTrait(
				AggregateListFactory, asAST_TRAIT_LIST_FACTORY);

			const asASTDeclId Function = Context.CreateDecl(
				asAST_DECL_FUNCTION, TranslationUnit,
				asCSourceRange(), "BuildNestedAggregate");
			Context.SetStableKey(Function, "BuildNestedAggregate()");
			Context.SetDeclType(Function, VoidType);
			const asASTStmtId FunctionBody = Context.CreateStmt(
				asAST_STMT_BLOCK, Function, asCSourceRange());
			Context.SetBody(Function, FunctionBody);

			const asASTExprId InnerAggregate = Context.CreateExpr(
				asAST_EXPR_CONSTRUCT, ElementType,
				asAST_VALUE_PRVALUE, asCSourceRange());
			Context.SetLiteral(InnerAggregate, "inner-list-pattern");
			Context.SetResolvedDecl(InnerAggregate, ElementListFactory);
			for (asUINT ElementIndex = 0; ElementIndex < 2; ++ElementIndex)
			{
				const asASTExprId InnerElement = Context.CreateExpr(
					asAST_EXPR_CONSTRUCT, ElementType,
					asAST_VALUE_PRVALUE, asCSourceRange());
				Context.SetLiteral(InnerElement, "FJitNestedElement()");
				Context.SetResolvedDecl(InnerElement, ElementConstructor);
				Context.AddExprChild(InnerAggregate, InnerElement);
			}

			const asASTExprId SiblingElement = Context.CreateExpr(
				asAST_EXPR_CONSTRUCT, ElementType,
				asAST_VALUE_PRVALUE, asCSourceRange());
			Context.SetLiteral(SiblingElement, "FJitNestedElement()");
			Context.SetResolvedDecl(SiblingElement, ElementConstructor);

			const asASTExprId OuterAggregate = Context.CreateExpr(
				asAST_EXPR_CONSTRUCT, AggregateType,
				asAST_VALUE_PRVALUE, asCSourceRange());
			Context.SetLiteral(OuterAggregate, "outer-list-pattern");
			Context.SetResolvedDecl(OuterAggregate, AggregateListFactory);
			Context.AddExprChild(OuterAggregate,
				bNestedFirst ? InnerAggregate : SiblingElement);
			Context.AddExprChild(OuterAggregate,
				bNestedFirst ? SiblingElement : InnerAggregate);

			const asASTStmtId AggregateStatement = Context.CreateStmt(
				asAST_STMT_EXPR, Function, asCSourceRange());
			Context.SetStmtExpr(AggregateStatement, OuterAggregate);
			Context.AddStmtChild(FunctionBody, AggregateStatement);

			auto AddAggregateRecord = [&](const asASTExprId Aggregate,
				const asUINT Order)
			{
				asSASTLifetimeRecord Record;
				Record.subjectKind =
					asAST_LIFETIME_SUBJECT_AGGREGATE_ELEMENT;
				Record.subject = asSASTLifetimeNodeId::FromExpr(Aggregate);
				Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_ELEMENT;
				Record.actionTarget =
					asSASTLifetimeNodeId::FromDecl(ElementDestructor);
				Record.activationPoint =
					asSASTLifetimeNodeId::FromExpr(Aggregate);
				Record.semanticRegion =
					asSASTLifetimeNodeId::FromDecl(Function);
				Record.phase = asAST_LIFETIME_PHASE_AGGREGATE_ELEMENT;
				Record.supportedExitMask = asAST_LIFETIME_EXIT_EXCEPTION;
				Record.constructionStep = 0;
				Record.order = Order;
				bFixturesSealed &=
					Context.AddLifetimeRecord(Record) == 0;
			};
			AddAggregateRecord(OuterAggregate, 0);
			AddAggregateRecord(InnerAggregate, 1);
			bFixturesSealed &= Context.Seal() == 0;
			return Function;
		};

		asCASTContext NestedFirstContext;
		const asASTDeclId NestedFirstFunction = BuildNestedAggregate(
			NestedFirstContext, true);
		asCASTContext NestedSecondContext;
		const asASTDeclId NestedSecondFunction = BuildNestedAggregate(
			NestedSecondContext, false);
		ASSERT_THAT(IsTrue(bFixturesSealed,
			TEXT("both nested aggregate fixtures must seal")));

		const FAngelscriptTypedASTJITCanonicalLifetimeFacts NestedFirst =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				NestedFirstContext, NestedFirstFunction);
		const FAngelscriptTypedASTJITCanonicalLifetimeFacts NestedSecond =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				NestedSecondContext, NestedSecondFunction);
		ASSERT_THAT(IsTrue(NestedFirst.LifetimeSummary.IsAuthenticated()));
		ASSERT_THAT(IsTrue(NestedSecond.LifetimeSummary.IsAuthenticated()));
		ASSERT_THAT(AreEqual(2u, NestedFirst.LifetimeSummary.RecordCount));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCleanupPlanState::PartialConstruction,
			NestedFirst.CleanupPlanState));
		ASSERT_THAT(IsTrue(
			NestedFirst.LifetimeSummary.StableKey
				!= NestedSecond.LifetimeSummary.StableKey,
			TEXT("nested aggregate parent/child ordinal must participate in stable identity")));
		ASSERT_THAT(IsTrue(
			NestedFirst.LifetimeSummary.ABIKey
				!= NestedSecond.LifetimeSummary.ABIKey,
			TEXT("nested aggregate parent/child ordinal must participate in native frame ABI identity")));

		FAngelscriptTypedASTJITEligibilityInput EligibilityInput;
		EligibilityInput.bIsUFunctionRoot = true;
		EligibilityInput.Purpose =
			EAngelscriptTypedASTJITEligibilityPurpose::RootCallClosure;
		EligibilityInput.SealedAST = &NestedFirstContext;
		EligibilityInput.CanonicalFunctionDecl = NestedFirstFunction;
		EligibilityInput.Shape.SymbolName = TEXT("ASJIT_BuildNestedAggregate");
		EligibilityInput.Shape.ReturnType =
			asCDataType::CreatePrimitive(ttVoid, false);
		const FAngelscriptTypedASTJITEligibility Eligibility =
			EvaluateAngelscriptTypedASTJITEligibilityFromCanonical(
				EligibilityInput);
		ASSERT_THAT(IsFalse(Eligibility.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime,
			Eligibility.Reason));
		ASSERT_THAT(IsTrue(Eligibility.Detail.Contains(
			TEXT("AuthenticatedAggregatePartialConstructionRequiresNativeElementFrameABI")),
			TEXT("nested aggregate must use the same precise per-function VM fallback")));

		FAngelscriptTypedASTJITEmitOptions EmitOptions;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(
				NestedFirstContext, NestedFirstFunction,
				EligibilityInput.Shape, EmitOptions);
		ASSERT_THAT(IsFalse(Emission.bSuccess));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime,
			Emission.FailureReason,
			TEXT("nested aggregate must not publish a partial native cleanup plan")));
	}

	TEST_METHOD(DelegatingConstructorPublishesCompleteCommitAndTypedFallback)
	{
		asCASTContext Context;
		const asASTDeclId TranslationUnit =
			Context.CreateTranslationUnit("JitDelegatingConstructorLifetime");
		const asCQualType VoidType = Context.InternPrimitive(ttVoid);
		const asCQualType DerivedType = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT, "FJitDelegatingDerived",
			asAST_QUAL_HANDLE | asAST_QUAL_AUTO_HANDLE);
		const asASTDeclId DerivedClass = Context.CreateDecl(
			asAST_DECL_CLASS, TranslationUnit,
			asCSourceRange(), "FJitDelegatingDerived");
		Context.SetStableKey(DerivedClass, "FJitDelegatingDerived");
		Context.SetDeclType(DerivedClass, DerivedType);
		const asASTDeclId TargetConstructor = Context.CreateDecl(
			asAST_DECL_CONSTRUCTOR, DerivedClass,
			asCSourceRange(), "FJitDelegatingDerived");
		Context.SetDeclType(TargetConstructor, VoidType);
		Context.SetBody(TargetConstructor, Context.CreateStmt(
			asAST_STMT_BLOCK, TargetConstructor, asCSourceRange()));
		const asASTDeclId DerivedDestructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, DerivedClass,
			asCSourceRange(), "FJitDelegatingDerived");
		Context.SetDeclType(DerivedDestructor, VoidType);
		Context.SetBody(DerivedDestructor, Context.CreateStmt(
			asAST_STMT_BLOCK, DerivedDestructor, asCSourceRange()));

		const asASTDeclId DelegatingConstructor = Context.CreateDecl(
			asAST_DECL_CONSTRUCTOR, DerivedClass,
			asCSourceRange(), "FJitDelegatingDerived");
		Context.SetDeclType(DelegatingConstructor, VoidType);
		Context.SetBody(DelegatingConstructor, Context.CreateStmt(
			asAST_STMT_BLOCK, DelegatingConstructor, asCSourceRange()));
		const asASTExprId ThisExpr = Context.CreateExpr(
			asAST_EXPR_THIS, DerivedType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		Context.SetLiteral(ThisExpr, "this");
		const asASTExprId DelegatingCall = Context.CreateExpr(
			asAST_EXPR_CALL, VoidType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		Context.SetResolvedDecl(DelegatingCall, TargetConstructor);
		Context.SetExprReceiver(DelegatingCall, ThisExpr);
		Context.AddExprChild(DelegatingCall, ThisExpr);
		Context.AddDeclInit(DelegatingConstructor, DelegatingCall);

		asSASTLifetimeRecord Record;
		Record.subjectKind =
			asAST_LIFETIME_SUBJECT_CONSTRUCTION_SUBOBJECT;
		Record.subject = asSASTLifetimeNodeId::FromDecl(DerivedClass);
		Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
		Record.actionTarget =
			asSASTLifetimeNodeId::FromDecl(DerivedDestructor);
		Record.activationPoint =
			asSASTLifetimeNodeId::FromExpr(DelegatingCall);
		Record.semanticRegion =
			asSASTLifetimeNodeId::FromDecl(DelegatingConstructor);
		Record.phase = asAST_LIFETIME_PHASE_CONSTRUCTOR_DELEGATING;
		Record.supportedExitMask = asAST_LIFETIME_EXIT_EXCEPTION;
		Record.constructionStep = 0;
		Record.order = 0;
		Record.completeObjectCommit = true;
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Record)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("delegating target success must authenticate as the sole complete-object commit")));

		const FAngelscriptTypedASTJITCanonicalLifetimeFacts FirstFacts =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				Context, DelegatingConstructor);
		const FAngelscriptTypedASTJITCanonicalLifetimeFacts SecondFacts =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				Context, DelegatingConstructor);
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCleanupPlanState::PartialConstruction,
			FirstFacts.CleanupPlanState));
		ASSERT_THAT(IsTrue(FirstFacts.LifetimeSummary.IsAuthenticated()));
		ASSERT_THAT(AreEqual(1u, FirstFacts.LifetimeSummary.RecordCount));
		ASSERT_THAT(AreEqual(0u, FirstFacts.LifetimeSummary.ExitPlanCount));
		ASSERT_THAT(IsTrue(FirstFacts.bHasExceptionCleanup));
		ASSERT_THAT(IsTrue(FirstFacts.bCleanupPlanCoversAllTransfers));
		ASSERT_THAT(IsFalse(
			FirstFacts.LifetimeSummary.bNativeObjectFrameABIAvailable));
		ASSERT_THAT(IsTrue(
			FirstFacts.LifetimeSummary.StableKey
				== SecondFacts.LifetimeSummary.StableKey
			&& FirstFacts.LifetimeSummary.ABIKey
				== SecondFacts.LifetimeSummary.ABIKey,
			TEXT("delegating construction summaries must be deterministic and pointer-free")));

		FAngelscriptTypedASTJITEligibilityInput EligibilityInput;
		EligibilityInput.bIsUFunctionRoot = true;
		EligibilityInput.Purpose =
			EAngelscriptTypedASTJITEligibilityPurpose::RootCallClosure;
		EligibilityInput.SealedAST = &Context;
		EligibilityInput.CanonicalFunctionDecl = DelegatingConstructor;
		EligibilityInput.Shape.SymbolName =
			TEXT("ASJIT_FJitDelegatingDerived_Constructor");
		EligibilityInput.Shape.ReturnType =
			asCDataType::CreatePrimitive(ttVoid, false);
		const FAngelscriptTypedASTJITEligibility Eligibility =
			EvaluateAngelscriptTypedASTJITEligibilityFromCanonical(
				EligibilityInput);
		ASSERT_THAT(IsFalse(Eligibility.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime,
			Eligibility.Reason));
		ASSERT_THAT(IsTrue(Eligibility.Detail.Contains(
			TEXT("AuthenticatedConstructorPartialConstructionRequiresNativeObjectFrameABI"))));

		FAngelscriptTypedASTJITEmitOptions EmitOptions;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(
				Context, DelegatingConstructor,
				EligibilityInput.Shape, EmitOptions);
		ASSERT_THAT(IsFalse(Emission.bSuccess));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime,
			Emission.FailureReason,
			TEXT("the missing native delegating-constructor frame ABI must fall back only this function")));
	}

	TEST_METHOD(SourceObjectIteratorForeachPhaseProvesReturnBreakAndContinueCoverage)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("object-iterator lifetime fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"JitCanonicalForeachLifetimeFacts", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("object-iterator lifetime module must exist")));
		ASSERT_THAT(AreEqual(0,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			struct FJitForeachIterator
			{
				int Index;
			}

			struct FJitObjectForeachRange
			{
				FJitForeachIterator opForBegin()
				{
					FJitForeachIterator Iterator;
					Iterator.Index = 0;
					return Iterator;
				}

				bool opForEnd(const FJitForeachIterator& Iterator)
				{
					return Iterator.Index >= 2;
				}

				void opForNext(FJitForeachIterator& inout Iterator)
				{
					Iterator.Index = Iterator.Index + 1;
				}

				int opForValue(const FJitForeachIterator& Iterator)
				{
					return 20 + Iterator.Index;
				}
			}

			int ProveObjectForeachLifetime(
				bool ReturnEarly,
				bool ContinueEarly)
			{
				FJitObjectForeachRange Range;
				foreach (int Value : Range)
				{
					if (ReturnEarly)
						return Value;
					if (ContinueEarly)
						continue;
					break;
				}
				return 7;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"JitCanonicalForeachLifetimeFacts.as", Source.c_str(),
			static_cast<unsigned int>(Source.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			*FString::Printf(TEXT(
				"object-iterator lifetime fixture must compile and seal: %s"),
				*CollectMessages(Engine.GetMessages()))));

		const asCModule* const Concrete = static_cast<asCModule*>(Module);
		const asCASTContext* const Context = Concrete != nullptr
			? Concrete->GetCanonicalASTContext() : nullptr;
		ASSERT_THAT(IsNotNull(Context,
			TEXT("object-iterator lifetime fixture must retain sealed AST")));
		if (Context == nullptr)
		{
			return;
		}
		const asASTDeclId FunctionDecl =
			FindAngelscriptTypedASTJITCanonicalFunctionDecl(
				*Context, "ProveObjectForeachLifetime");
		ASSERT_THAT(IsTrue(FunctionDecl.IsValid(),
			TEXT("object-iterator lifetime fixture needs one exact function")));

		asCASTLifetimeView LifetimeView;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(*Context, LifetimeView),
			TEXT("the AOT fixture must consume the same authenticated lifetime view as Bytecode")));
		const asSASTForEachPhases* ForEachPhases = nullptr;
		for( asUINT StatementIndex = 1;
			StatementIndex <= Context->GetStmtCount(); ++StatementIndex )
		{
			const asASTStmtId StatementId(StatementIndex);
			const asCStmt* const Statement = Context->GetStmt(StatementId);
			if( Statement != nullptr && Statement->kind == asAST_STMT_FOREACH )
			{
				ASSERT_THAT(IsNull(ForEachPhases,
					TEXT("the fixture has one unambiguous foreach statement")));
				ForEachPhases = LifetimeView.FindForEachPhases(StatementId);
			}
			if( Statement != nullptr && Statement->kind == asAST_STMT_CONTINUE )
			{
				for( asUINT ChildIndex = 0;
					ChildIndex < Statement->children.GetLength(); ++ChildIndex )
				{
					ASSERT_THAT(IsNull(LifetimeView.FindCleanupBinding(
						Statement->children[ChildIndex]),
						TEXT("continue remains inside the foreach iterator lifetime")));
				}
			}
		}
		ASSERT_THAT(IsNotNull(ForEachPhases,
			TEXT("foreach positional children must resolve through one named phase view")));
		if (ForEachPhases != nullptr)
		{
			ASSERT_THAT(IsTrue(ForEachPhases->initializer.IsValid()));
			ASSERT_THAT(IsTrue(ForEachPhases->condition.IsValid()));
			ASSERT_THAT(IsTrue(ForEachPhases->body.IsValid()));
			ASSERT_THAT(IsTrue(ForEachPhases->increment.IsValid()));
			ASSERT_THAT(IsTrue(ForEachPhases->cleanup.IsValid()));
			ASSERT_THAT(AreNotEqual(
				static_cast<asUINT>(-1),
				ForEachPhases->lifetimeRecordIndex));
			const asSASTLifetimeCleanupBinding* const CleanupBinding =
				LifetimeView.FindCleanupBinding(ForEachPhases->cleanup);
			ASSERT_THAT(IsNotNull(CleanupBinding,
				TEXT("the named foreach cleanup phase must bind one authenticated record")));
			if (CleanupBinding != nullptr)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asAST_LIFETIME_COMPAT_FOREACH_EXIT),
					static_cast<int32>(CleanupBinding->role)));
				ASSERT_THAT(AreEqual(
					ForEachPhases->lifetimeRecordIndex,
					CleanupBinding->recordIndex));
			}
		}

		const FAngelscriptTypedASTJITCanonicalLifetimeFacts Facts =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				*Context, FunctionDecl);
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCleanupPlanState::ScriptDestructor,
			Facts.CleanupPlanState,
			TEXT("the foreach cleanup phase must retain exact destructor classification")));
		ASSERT_THAT(IsTrue(Facts.bCleanupPlanCoversAllTransfers,
			TEXT("the sealed foreach phase must cover return, break, normal exit, and keep continue inside the iterator lifetime")));
	}

	TEST_METHOD(ProductionCanonicalBodyRequiresSealedAstAndExactDecl)
	{
		FAngelscriptStaticJITCompiledFunctionView Function;
		ASSERT_THAT(IsFalse(FunctionHasTypedASTJITCanonicalBody(Function),
			TEXT("an empty production input must not act as a canonical body")));
	}

	TEST_METHOD(AmbiguousSameNameOverloadsRejectAstBind)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("JitOverload");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType FloatType = Context.InternPrimitive(ttFloat, 0);
		const asASTDeclId FInt = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
		Sema.ActOnParamDecl(FInt, "a", IntType, asCSourceRange());
		Sema.FinishDecl(FInt);
		const asASTDeclId FFloat = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
		Sema.ActOnParamDecl(FFloat, "a", FloatType, asCSourceRange());
		Sema.FinishDecl(FFloat);
		const asASTDeclId Unique = Sema.ActOnFunctionDecl(Tu, "G", IntType, asCSourceRange());
		Sema.FinishDecl(Unique);
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("overload identity fixture must seal")));

		const asASTDeclId FoundF =
			FindAngelscriptTypedASTJITCanonicalFunctionDecl(Context, "F");
		ASSERT_THAT(IsTrue(!FoundF.IsValid(),
			TEXT("ambiguous same-name overloads must reject, not bind the first F")));
		const asASTDeclId FoundG =
			FindAngelscriptTypedASTJITCanonicalFunctionDecl(Context, "G");
		ASSERT_THAT(AreEqual(Unique.value, FoundG.value,
			TEXT("unique name+signature must still bind")));
	}

	TEST_METHOD(FallbackCategoriesStayFrozenWithSealedAst)
	{
		const EAngelscriptTypedASTJITFallbackReason Reasons[] = {
			EAngelscriptTypedASTJITFallbackReason::None,
			EAngelscriptTypedASTJITFallbackReason::NotUFunctionRoot,
			EAngelscriptTypedASTJITFallbackReason::MissingCanonicalAST,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionKind,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionTrait,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedUFunctionFlags,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedStatement,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedGlobalStorage,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedGlobalInitializer,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedImportedRoute,
			EAngelscriptTypedASTJITFallbackReason::SemanticDependencyMismatch,
			EAngelscriptTypedASTJITFallbackReason::NonPortableNumericConversion,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionObservability,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionControl,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExceptionRegion,
			EAngelscriptTypedASTJITFallbackReason::ExceptionPayloadUnavailable,
			EAngelscriptTypedASTJITFallbackReason::BridgeExceptionAdoptionUnavailable,
			EAngelscriptTypedASTJITFallbackReason::SuspendOrExceptionState,
			EAngelscriptTypedASTJITFallbackReason::InvalidEffectiveReceiver,
			EAngelscriptTypedASTJITFallbackReason::InvalidCallEvaluationSequence,
			EAngelscriptTypedASTJITFallbackReason::InvalidControlTarget,
			EAngelscriptTypedASTJITFallbackReason::InvalidCleanupPlan,
			EAngelscriptTypedASTJITFallbackReason::InvalidCanonicalAST,
			EAngelscriptTypedASTJITFallbackReason::EmitterFailure,
			EAngelscriptTypedASTJITFallbackReason::BackendUnavailable,
		};
		ASSERT_THAT(AreEqual(31, static_cast<int32>(UE_ARRAY_COUNT(Reasons)),
			TEXT("typed fallback categories must not widen")));

		FAngelscriptTypedASTJITEligibilityInput Missing;
		Missing.bIsUFunctionRoot = true;
		const FAngelscriptTypedASTJITEligibility MissingResult =
			EvaluateAngelscriptTypedASTJITEligibility(Missing);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(EAngelscriptTypedASTJITFallbackReason::MissingCanonicalAST),
			static_cast<int32>(MissingResult.Reason),
			TEXT("no sealed AST remains MissingCanonicalAST")));
	}

	TEST_METHOD(VisitorsPreserveFallbackCategoriesForUnsupportedAstNodes)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("JitAstUnsupported");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "Run", IntType, asCSourceRange());
		asCArray<asASTExprId> Args;
		const asASTExprId Construct = Sema.ActOnConstruct(IntType, Args, asCSourceRange());
		asCArray<asASTStmtId> UnsupportedBody;
		UnsupportedBody.PushLast(Sema.ActOnReturnStmt(Fn, Construct, asCSourceRange()));
		Context.SetBody(Fn, Sema.ActOnCompoundStmt(Fn, UnsupportedBody, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("unsupported fixture must seal")));

		FAngelscriptTypedASTJITEligibilityInput Input;
		Input.bIsUFunctionRoot = true;
		Input.SealedAST = &Context;
		Input.CanonicalFunctionDecl = Fn;
		Input.Shape.SymbolName = TEXT("ASJIT_Run");
		Input.Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		const FAngelscriptTypedASTJITEligibility Eligibility =
			EvaluateAngelscriptTypedASTJITEligibility(Input);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime),
			static_cast<int32>(Eligibility.Reason),
			TEXT("construct nodes keep the existing lifetime fallback")));
		ASSERT_THAT(IsFalse(Eligibility.bEligible, TEXT("construct is outside the scalar native surface")));
	}

	TEST_METHOD(AnalyzeAndEmitScalarReturnFromSealedAst)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("JitAstScalar");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "Run", IntType, asCSourceRange());
		const asASTExprId Literal = Sema.ActOnIntegerLiteral(7, asCSourceRange());
		asCArray<asASTStmtId> ScalarBody;
		ScalarBody.PushLast(Sema.ActOnReturnStmt(Fn, Literal, asCSourceRange()));
		Context.SetBody(Fn, Sema.ActOnCompoundStmt(Fn, ScalarBody, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("scalar fixture must seal")));

		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("ASJIT_Run");
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		const FAngelscriptTypedASTJITAnalysis Analysis =
			AnalyzeTypedASTJITFunction(Context, Fn, Shape);
		ASSERT_THAT(IsTrue(Analysis.bEligible, TEXT("scalar return is in the current native surface")));

		asCString DumpBefore;
		asCASTDump(Context, DumpBefore);
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(Context, Fn, Shape, {});
		asCString DumpAfter;
		asCASTDump(Context, DumpAfter);
		ASSERT_THAT(IsTrue(Emission.bSuccess, TEXT("scalar AST must emit C++")));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(TEXT("return")), TEXT("emission lowers return")));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(TEXT("7")), TEXT("emission keeps the integer literal")));
		ASSERT_THAT(IsTrue(DumpBefore.Equals(DumpAfter), TEXT("emission must leave AST dumps unchanged")));
		ASSERT_THAT(AreEqual(Emission.Definition, EmitTypedASTJITFunction(Context, Fn, Shape, {}).Definition,
			TEXT("repeated canonical emission is deterministic")));
	}

	TEST_METHOD(CanonicalCallWithoutPlanStaysUnsupportedCall)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("JitAstCall");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Callee = Sema.ActOnFunctionDecl(Tu, "Helper", IntType, asCSourceRange());
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "Run", IntType, asCSourceRange());
		asCArray<asASTExprId> Args;
		const asASTExprId Call = Sema.ActOnCall(Callee, Args, IntType, asCSourceRange());
		asCArray<asASTStmtId> CallBody;
		CallBody.PushLast(Sema.ActOnReturnStmt(Fn, Call, asCSourceRange()));
		Context.SetBody(Fn, Sema.ActOnCompoundStmt(Fn, CallBody, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("call fixture must seal")));

		FAngelscriptTypedASTJITEligibilityInput Input;
		Input.bIsUFunctionRoot = true;
		Input.Purpose = EAngelscriptTypedASTJITEligibilityPurpose::StandaloneRoot;
		Input.SealedAST = &Context;
		Input.CanonicalFunctionDecl = Fn;
		Input.Shape.SymbolName = TEXT("ASJIT_Run");
		Input.Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		const FAngelscriptTypedASTJITEligibility Eligibility =
			EvaluateAngelscriptTypedASTJITEligibility(Input);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(EAngelscriptTypedASTJITFallbackReason::UnsupportedCall),
			static_cast<int32>(Eligibility.Reason),
			TEXT("standalone roots still require closure planning for calls")));
	}

	TEST_METHOD(CanonicalCallEmissionRestoresFormalArgumentOrder)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("JitAstCallOrder");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Callee =
			Sema.ActOnFunctionDecl(Tu, "Helper", IntType, asCSourceRange());
		Sema.ActOnParamDecl(Callee, "First", IntType, asCSourceRange());
		Sema.ActOnParamDecl(Callee, "Second", IntType, asCSourceRange());
		Sema.FinishDecl(Callee);

		const asASTDeclId Fn =
			Sema.ActOnFunctionDecl(Tu, "Run", IntType, asCSourceRange());
		asCArray<asASTExprId> Args;
		const asASTExprId FirstArg =
			Sema.ActOnIntegerLiteral(1, asCSourceRange());
		const asASTExprId SecondArg =
			Sema.ActOnIntegerLiteral(2, asCSourceRange());
		Args.PushLast(FirstArg);
		Args.PushLast(SecondArg);
		const asASTExprId Call =
			Sema.ActOnCall(Callee, Args, IntType, asCSourceRange());
		asCArray<asASTStmtId> Body;
		Body.PushLast(Sema.ActOnReturnStmt(Fn, Call, asCSourceRange()));
		Context.SetBody(Fn,
			Sema.ActOnCompoundStmt(Fn, Body, asCSourceRange()));
		Sema.FinishDecl(Fn);
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("formal-order fixture must seal")));

		const asCExpr* const SealedCall = Context.GetExpr(Call);
		ASSERT_THAT(IsTrue(SealedCall != nullptr
			&& SealedCall->children.GetLength() == 2,
			TEXT("sealed call owns both reverse-formal operands")));
		if (SealedCall == nullptr || SealedCall->children.GetLength() != 2)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			SecondArg.value,
			SealedCall->children[0].value,
			TEXT("fixture documents that canonical call children are reverse-formal")));

		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("ASJIT_Run");
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		TArray<FAngelscriptTypedASTJITScriptCallEmissionPlan> ScriptCalls;
		FAngelscriptTypedASTJITScriptCallEmissionPlan& Plan =
			ScriptCalls.Emplace_GetRef();
		Plan.Expression = Call;
		Plan.Symbol = TEXT("ASJIT_Helper");
		FAngelscriptTypedASTJITEmitOptions Options;
		Options.ScriptCalls = ScriptCalls;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(Context, Fn, Shape, Options);
		ASSERT_THAT(IsTrue(Emission.bSuccess,
			*FString::Printf(TEXT("planned canonical call must emit: %s"),
				*Emission.FailureDetail)));
		const int32 SecondEvaluationAt = Emission.Definition.Find(
			TEXT("const int32 as_ast_e3_arg1 = int32(2);"));
		const int32 FirstEvaluationAt = Emission.Definition.Find(
			TEXT("const int32 as_ast_e3_arg0 = int32(1);"));
		const int32 CallAt = Emission.Definition.Find(
			TEXT("ASJIT_Helper(Execution, as_ast_e3_arg0, as_ast_e3_arg1)"));
		ASSERT_THAT(IsTrue(SecondEvaluationAt != INDEX_NONE
			&& FirstEvaluationAt > SecondEvaluationAt
			&& CallAt > FirstEvaluationAt,
			*FString::Printf(TEXT(
				"C++ emission must evaluate reverse-formal AST children in stored order, then restore formal order at the call. definition:\n%s"),
				*Emission.Definition)));
	}

	TEST_METHOD(SourceEnumLocalEmitsScalarAbiStorageWithoutCppEnumDependency)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"JitCanonicalEnumLocal", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("enum-local module must exist")));
		ASSERT_THAT(AreEqual(0,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			enum EMode
			{
				Zero,
				One,
			}

			int ConvertMode(int RawMode)
			{
				EMode Mode = EMode(RawMode);
				return RawMode;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"JitCanonicalEnumLocal.as", Source.c_str())));
		Module->Build();

		const asCModule* const Concrete = static_cast<asCModule*>(Module);
		const asCASTContext* const Context =
			Concrete != nullptr ? Concrete->GetCanonicalASTContext() : nullptr;
		ASSERT_THAT(IsNotNull(Context,
			*FString::Printf(TEXT("enum-local source must seal canonical AST: %s"),
				*CollectMessages(Engine.GetMessages()))));
		if (Context == nullptr)
		{
			return;
		}

		const asASTDeclId FunctionDecl =
			FindAngelscriptTypedASTJITCanonicalFunctionDecl(
				*Context, "ConvertMode");
		ASSERT_THAT(IsTrue(FunctionDecl.IsValid(),
			TEXT("ConvertMode must have one exact canonical declaration")));

		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("ASJIT_ConvertMode");
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(*Context, FunctionDecl, Shape, {});
		ASSERT_THAT(IsTrue(Emission.bSuccess,
			*FString::Printf(TEXT(
				"a source enum local must emit through its scalar ABI storage: %s"),
				*Emission.FailureDetail)));
		ASSERT_THAT(IsTrue(
			Emission.Definition.Contains(TEXT("int8 as_ast_d"))
				&& Emission.Definition.Contains(
					TEXT("AngelscriptTypedASTJIT::WrapNarrow<int8>")),
			*FString::Printf(TEXT(
				"the enum local and conversion must preserve the current signed-byte ABI and wrapping-narrow semantics. definition:\n%s"),
				*Emission.Definition)));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(TEXT("EMode")),
			TEXT("generated C++ must not depend on a script-only enum declaration")));
	}

	TEST_METHOD(SourceControlFlowAndOperatorsConsumeCanonicalSemanticProtocol)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"JitCanonicalControlFlow", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("canonical control-flow module must exist")));
		ASSERT_THAT(AreEqual(0,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			enum EReviewMode
			{
				First,
				Second,
				Third,
			}

			int EmitReviewedControlFlow(int Raw)
			{
				EReviewMode Mode = EReviewMode(Raw);
				int Result = 0;
				switch (Mode)
				{
				case EReviewMode::First:
					Result = 10;
					break;
				case EReviewMode::Second:
					Result = 20;
					break;
				case EReviewMode::Third:
					Result = 30;
					break;
				}
				for (int Index = 0; Index < Raw; ++Index)
				{
					Result += Index;
					if (Index == 2)
						continue;
				}
				int Prefix = ++Result;
				int Postfix = Result++;
				return Prefix + Postfix + Result;
			}

			int EmitReviewedExhaustive(EReviewMode Mode)
			{
				switch (Mode)
				{
				case EReviewMode::First:
					return 1;
				case EReviewMode::Second:
					return 2;
				case EReviewMode::Third:
					return 3;
				}
			}

			double EmitReviewedPower(double Base, double Exponent)
			{
				return Base ** Exponent;
			}

			int EmitReviewedLogicalShift(int Value, int Count)
			{
				return Value >> Count;
			}

			int EmitReviewedArithmeticShift(int Value, int Count)
			{
				return Value >>> Count;
			}

			int EmitReviewedCheckedDivide(int Numerator, int Denominator)
			{
				return Numerator / Denominator;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"JitCanonicalControlFlow.as", Source.c_str())));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			*CollectMessages(Engine.GetMessages())));

		const asCModule* const Concrete = static_cast<asCModule*>(Module);
		const asCASTContext* const Context =
			Concrete != nullptr ? Concrete->GetCanonicalASTContext() : nullptr;
		ASSERT_THAT(IsNotNull(Context,
			*FString::Printf(TEXT(
				"control-flow source must seal canonical AST: %s"),
				*CollectMessages(Engine.GetMessages()))));
		if (Context == nullptr)
		{
			return;
		}

		const asASTDeclId FunctionDecl =
			FindAngelscriptTypedASTJITCanonicalFunctionDecl(
				*Context, "EmitReviewedControlFlow");
		const asCDecl* const Function = Context->GetDecl(FunctionDecl);
		ASSERT_THAT(IsTrue(Function != nullptr && Function->body.IsValid(),
			TEXT("reviewed control-flow function must retain its body")));
		if (Function == nullptr || !Function->body.IsValid())
		{
			return;
		}

		const asCStmt* CanonicalFor = nullptr;
		TArray<asASTStmtId> PendingStatements;
		PendingStatements.Add(Function->body);
		while (!PendingStatements.IsEmpty() && CanonicalFor == nullptr)
		{
			const asCStmt* const Statement =
				Context->GetStmt(PendingStatements.Pop(EAllowShrinking::No));
			if (Statement == nullptr)
			{
				continue;
			}
			if (Statement->kind == asAST_STMT_FOR)
			{
				CanonicalFor = Statement;
				break;
			}
			for (asUINT ChildIndex = 0;
				ChildIndex < Statement->children.GetLength(); ++ChildIndex)
			{
				PendingStatements.Add(Statement->children[ChildIndex]);
			}
		}
		ASSERT_THAT(IsTrue(CanonicalFor != nullptr
			&& CanonicalFor->children.GetLength() == 3,
			TEXT("ForStmt must retain [initializer, body, increment] phases")));
		if (CanonicalFor == nullptr || CanonicalFor->children.GetLength() != 3)
		{
			return;
		}

		asASTDeclId IndexDecl;
		PendingStatements.Reset();
		PendingStatements.Add(CanonicalFor->children[0]);
		while (!PendingStatements.IsEmpty() && !IndexDecl.IsValid())
		{
			const asCStmt* const Statement =
				Context->GetStmt(PendingStatements.Pop(EAllowShrinking::No));
			if (Statement == nullptr)
			{
				continue;
			}
			if (Statement->kind == asAST_STMT_DECL)
			{
				IndexDecl = Statement->decl;
				break;
			}
			for (asUINT ChildIndex = 0;
				ChildIndex < Statement->children.GetLength(); ++ChildIndex)
			{
				PendingStatements.Add(Statement->children[ChildIndex]);
			}
		}
		ASSERT_THAT(IsTrue(IndexDecl.IsValid(),
			TEXT("for initializer must retain its local declaration")));

		asASTExprId CompoundAssign;
		for (asUINT ExprIndex = 1;
			ExprIndex <= Context->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = Context->GetExpr(asASTExprId(ExprIndex));
			if (Expr != nullptr
				&& Expr->kind == asAST_EXPR_ASSIGN
				&& Expr->literal.Equals("+="))
			{
				CompoundAssign = asASTExprId(ExprIndex);
				break;
			}
		}
		ASSERT_THAT(IsTrue(CompoundAssign.IsValid(),
			TEXT("source += must remain an explicit Canonical AST compound-assignment fact")));

		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("ASJIT_EmitReviewedControlFlow");
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(*Context, FunctionDecl, Shape, {});
		ASSERT_THAT(IsTrue(Emission.bSuccess,
			*FString::Printf(TEXT(
				"reviewed canonical control flow must emit: %s"),
				*Emission.FailureDetail)));

		const int32 ForAt = Emission.Definition.Find(TEXT("for ("));
		const FString IndexDeclaration = FString::Printf(
			TEXT("int32 as_ast_d%u;"), IndexDecl.value);
		const int32 IndexDeclarationAt =
			Emission.Definition.Find(IndexDeclaration);
		const FString IndexIncrement = FString::Printf(TEXT(
			"AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d%u)"),
			IndexDecl.value);
		const int32 IndexIncrementAt = Emission.Definition.Find(
			IndexIncrement, ESearchCase::CaseSensitive,
			ESearchDir::FromStart, ForAt);
		const int32 ContinueAt = Emission.Definition.Find(TEXT("continue;"),
			ESearchCase::CaseSensitive, ESearchDir::FromStart, ForAt);

		ASSERT_THAT(IsTrue(IndexDeclarationAt != INDEX_NONE
			&& ForAt > IndexDeclarationAt,
			*FString::Printf(TEXT(
				"for initializer declaration must be emitted in the loop scope before the loop. definition:\n%s"),
				*Emission.Definition)));
		const auto BraceDepthAt = [&Emission](const int32 Offset)
		{
			int32 Depth = 0;
			for (int32 CharacterIndex = 0;
				CharacterIndex < Offset; ++CharacterIndex)
			{
				if (Emission.Definition[CharacterIndex] == TEXT('{'))
					++Depth;
				else if (Emission.Definition[CharacterIndex] == TEXT('}'))
					--Depth;
			}
			return Depth;
		};
		ASSERT_THAT(IsTrue(BraceDepthAt(IndexDeclarationAt)
			== BraceDepthAt(ForAt),
			*FString::Printf(TEXT(
				"for initializer declaration must remain in the same C++ scope as the loop condition and increment. definition:\n%s"),
				*Emission.Definition)));
		ASSERT_THAT(IsTrue(IndexIncrementAt > ForAt
			&& ContinueAt > IndexIncrementAt,
			*FString::Printf(TEXT(
				"for increment must be emitted before the body continue so C++ keeps it in the increment phase. definition:\n%s"),
				*Emission.Definition)));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(TEXT("case as_ast_d")),
			TEXT("enum case labels must lower to compile-time constants")));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(TEXT("case int8(")),
			*FString::Printf(TEXT(
				"script enum cases must use the signed-byte ABI carrier. definition:\n%s"),
				*Emission.Definition)));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(TEXT("pre++")),
			TEXT("pre++ is a Canonical AST semantic tag, not a C++ token")));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(TEXT("post++")),
			TEXT("post++ is a Canonical AST semantic tag, not a C++ token")));

		FAngelscriptTypedASTJITFunctionShape ExhaustiveShape;
		ExhaustiveShape.SymbolName = TEXT("ASJIT_EmitReviewedExhaustive");
		ExhaustiveShape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt8, false));
		ExhaustiveShape.ReturnType =
			asCDataType::CreatePrimitive(ttInt, false);
		const asASTDeclId ExhaustiveFunctionDecl =
			FindAngelscriptTypedASTJITCanonicalFunctionDecl(
				*Context, "EmitReviewedExhaustive");
		const asCStmt* ExhaustiveSwitch = nullptr;
		for (asUINT StatementIndex = 1;
			StatementIndex <= Context->GetStmtCount(); ++StatementIndex)
		{
			const asCStmt* const Statement =
				Context->GetStmt(asASTStmtId(StatementIndex));
			if (Statement != nullptr
				&& Statement->owner == ExhaustiveFunctionDecl
				&& Statement->kind == asAST_STMT_SWITCH)
			{
				ExhaustiveSwitch = Statement;
				break;
			}
		}
		ASSERT_THAT(IsTrue(ExhaustiveSwitch != nullptr
			&& ExhaustiveSwitch->safePointRole
				== asAST_SAFEPOINT_SWITCH_INVALID_VALUE,
			TEXT("Sema must retain the exhaustive-enum invalid-value edge on the Canonical SwitchStmt")));
		const FAngelscriptTypedASTJITEmission ExhaustiveEmission =
			EmitTypedASTJITFunction(*Context,
				ExhaustiveFunctionDecl,
				ExhaustiveShape, {});
		ASSERT_THAT(IsTrue(ExhaustiveEmission.bSuccess,
			*ExhaustiveEmission.FailureDetail));
		ASSERT_THAT(IsTrue(ExhaustiveEmission.bRequiresExecutionState
			&& ExhaustiveEmission.Definition.Contains(
				TEXT("FStaticJITFunction::SetSwitchValueInvalidException(Execution);")),
			*FString::Printf(TEXT(
				"the canonical invalid-enum edge must raise through the shared execution-state contract. definition:\n%s"),
				*ExhaustiveEmission.Definition)));
		ASSERT_THAT(IsTrue(ExhaustiveEmission.Definition.EndsWith(
			TEXT("\treturn int32{};\n}\n")),
			*FString::Printf(TEXT(
				"C++ needs a structural fallback after a Sema-proved exhaustive enum switch. definition:\n%s"),
				*ExhaustiveEmission.Definition)));

		FAngelscriptTypedASTJITFunctionShape PowerShape;
		PowerShape.SymbolName = TEXT("ASJIT_EmitReviewedPower");
		PowerShape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttFloat64, false));
		PowerShape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttFloat64, false));
		PowerShape.ReturnType = asCDataType::CreatePrimitive(ttFloat64, false);
		const FAngelscriptTypedASTJITEmission PowerEmission =
			EmitTypedASTJITFunction(*Context,
				FindAngelscriptTypedASTJITCanonicalFunctionDecl(
					*Context, "EmitReviewedPower"),
				PowerShape, {});
		ASSERT_THAT(IsTrue(PowerEmission.bSuccess,
			*PowerEmission.FailureDetail));
		ASSERT_THAT(IsTrue(PowerEmission.bRequiresExecutionState,
			TEXT("checked power must publish its hidden execution-state ABI before the function signature is emitted")));
		ASSERT_THAT(IsTrue(PowerEmission.Definition.Contains(
			TEXT("AngelscriptTypedASTJIT::CheckedPower<double>")),
			*FString::Printf(TEXT(
				"canonical ** must lower through the reviewed exception-aware helper. definition:\n%s"),
				*PowerEmission.Definition)));
		ASSERT_THAT(IsFalse(PowerEmission.Definition.Contains(TEXT(" ** ")),
			TEXT("AngelScript power spelling is not a C++ operator")));

		const asASTDeclId DivideFunctionDecl =
			FindAngelscriptTypedASTJITCanonicalFunctionDecl(
				*Context, "EmitReviewedCheckedDivide");
		const asCStmt* DivideReturn = nullptr;
		for (asUINT StatementIndex = 1;
			StatementIndex <= Context->GetStmtCount(); ++StatementIndex)
		{
			const asCStmt* const Statement =
				Context->GetStmt(asASTStmtId(StatementIndex));
			if (Statement != nullptr
				&& Statement->owner == DivideFunctionDecl
				&& Statement->kind == asAST_STMT_RETURN)
			{
				DivideReturn = Statement;
				break;
			}
		}
		asSSourceLineColumn DividePosition;
		ASSERT_THAT(IsTrue(DivideReturn != nullptr
			&& Context->GetSourceManager().GetLineColumn(
				DivideReturn->range.begin, DividePosition),
			TEXT("checked division must retain a resolvable Canonical AST source position")));
		FAngelscriptTypedASTJITGenerationCapabilityProfile PositionProfile;
		FString PositionProfileError;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
			EAngelscriptTypedASTJITDirectCapability::FramePosition
				| EAngelscriptTypedASTJITDirectCapability::RecursionBudget,
			PositionProfile,
			&PositionProfileError), *PositionProfileError));
		FAngelscriptTypedASTJITEmitOptions DivideOptions;
		DivideOptions.InstrumentationProfile = &PositionProfile;
		FAngelscriptTypedASTJITFunctionShape DivideShape;
		DivideShape.SymbolName = TEXT("ASJIT_EmitReviewedCheckedDivide");
		DivideShape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		DivideShape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		DivideShape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		const FAngelscriptTypedASTJITEmission DivideEmission =
			EmitTypedASTJITFunction(*Context,
				DivideFunctionDecl,
				DivideShape, DivideOptions);
		ASSERT_THAT(IsTrue(DivideEmission.bSuccess,
			*DivideEmission.FailureDetail));
		const FString DividePositionHook = FString::Printf(
			TEXT("TypedFrame.SetSourcePosition(%d, %d);"),
			DividePosition.row, DividePosition.column);
		const int32 DividePositionAt =
			DivideEmission.Definition.Find(DividePositionHook);
		const int32 CheckedDivideAt = DivideEmission.Definition.Find(
			TEXT("AngelscriptTypedASTJIT::CheckedDivide<int32>"));
		ASSERT_THAT(IsTrue(DividePositionAt != INDEX_NONE
			&& CheckedDivideAt > DividePositionAt,
			*FString::Printf(TEXT(
				"a checked Canonical AST operation must publish its owning statement position before it can raise. expected=%s definition:\n%s"),
				*DividePositionHook, *DivideEmission.Definition)));

		auto EmitShift = [&](const char* FunctionName,
			const TCHAR* SymbolName)
		{
			FAngelscriptTypedASTJITFunctionShape ShiftShape;
			ShiftShape.SymbolName = SymbolName;
			ShiftShape.ParameterTypes.Add(
				asCDataType::CreatePrimitive(ttInt, false));
			ShiftShape.ParameterTypes.Add(
				asCDataType::CreatePrimitive(ttInt, false));
			ShiftShape.ReturnType =
				asCDataType::CreatePrimitive(ttInt, false);
			return EmitTypedASTJITFunction(*Context,
				FindAngelscriptTypedASTJITCanonicalFunctionDecl(
					*Context, FunctionName),
				ShiftShape, {});
		};
		const FAngelscriptTypedASTJITEmission LogicalShiftEmission =
			EmitShift("EmitReviewedLogicalShift",
				TEXT("ASJIT_EmitReviewedLogicalShift"));
		ASSERT_THAT(IsTrue(LogicalShiftEmission.bSuccess,
			*LogicalShiftEmission.FailureDetail));
		ASSERT_THAT(IsTrue(LogicalShiftEmission.Definition.Contains(
			TEXT("AngelscriptTypedASTJIT::LogicalShiftRight<int32>")),
			*LogicalShiftEmission.Definition));
		const FAngelscriptTypedASTJITEmission ArithmeticShiftEmission =
			EmitShift("EmitReviewedArithmeticShift",
				TEXT("ASJIT_EmitReviewedArithmeticShift"));
		ASSERT_THAT(IsTrue(ArithmeticShiftEmission.bSuccess,
			*ArithmeticShiftEmission.FailureDetail));
		ASSERT_THAT(IsTrue(ArithmeticShiftEmission.Definition.Contains(
			TEXT("AngelscriptTypedASTJIT::ArithmeticShiftRight<int32>")),
			*ArithmeticShiftEmission.Definition));
		ASSERT_THAT(IsFalse(ArithmeticShiftEmission.Definition.Contains(
			TEXT(" >>> ")),
			TEXT("AngelScript arithmetic-shift spelling is not a C++ operator")));
	}
};

#endif
