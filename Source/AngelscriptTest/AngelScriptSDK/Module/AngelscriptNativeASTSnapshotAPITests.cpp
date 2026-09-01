#include "AngelscriptTestMacros.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "HAL/ThreadSafeBool.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalASTSnapshotAPITests,
	"Angelscript.TestModule.AngelScriptSDK.Module.CanonicalAST.Snapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DiscardPolicyHasNoPublicSnapshot)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("discard lifecycle coverage requires explicit Canonical selection")));

		asIScriptModule* Module = nullptr;
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(), "DiscardSnap", "int F() { return 1; }", Module),
			TEXT("Canonical discard source should compile")));
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual((int)asAST_DISCARD_AFTER_CODEGEN, (int)Module->GetASTRetentionPolicy(),
			TEXT("default policy is discard")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("discard must happen only after Canonical CodeGen publishes bytecode")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("Canonical discard must not invoke the legacy compiler")));
		asQWORD CanonicalDigest = 0;
		ASSERT_THAT(IsTrue(InternalModule->GetLastCanonicalCodeGenAstDigest(
			CanonicalDigest) && CanonicalDigest != 0,
			TEXT("discarded AST must be the same sealed graph consumed by Canonical CodeGen")));
		ASSERT_THAT(IsTrue(Module->AcquireASTSnapshot(asAST_API_VERSION_1) == nullptr,
			TEXT("discard policy must not publish a public snapshot")));
		ASSERT_THAT(IsNull(InternalModule->GetCanonicalASTContext(),
			TEXT("discard policy must release its pending Canonical AST after CodeGen")));
	}

	TEST_METHOD(RetainPolicyPublishesImmutableV1Snapshot)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptModule* Module = Engine.Get()->GetModule("RetainSnap", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("retain test needs a module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT),
			TEXT("policy can be selected before Build")));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection("RetainSnap", "int F() { return 1; }"),
			TEXT("section should add")));
		ASSERT_THAT(AreEqual(0, Module->Build(), TEXT("retain module should build")));
		asIASTSnapshot* Snapshot = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Snapshot, TEXT("retain policy should publish V1 snapshot")));
		ASSERT_THAT(AreEqual(asAST_API_VERSION_1, Snapshot->GetAPIVersion(), TEXT("API version is V1")));
		ASSERT_THAT(IsTrue(Snapshot->IsCurrentGeneration(), TEXT("fresh snapshot is current")));
		ASSERT_THAT(IsTrue(Snapshot->GetTranslationUnitDecl().IsValid(), TEXT("TU id is valid")));
		asSASTDeclView View = {};
		View.structSize = sizeof(View);
		ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(Snapshot->GetTranslationUnitDecl(), &View),
			TEXT("TU view should fill")));
		ASSERT_THAT(AreEqual(asAST_API_VERSION_1, View.apiVersion, TEXT("view carries apiVersion")));

		asSASTDeclView TooSmall = {};
		TooSmall.structSize = static_cast<asUINT>(offsetof(asSASTDeclView, kind));
		TooSmall.apiVersion = 0xA57u;
		TooSmall.kind = 0x5Au;
		ASSERT_THAT(AreEqual(asINVALID_ARG, Snapshot->GetDecl(Snapshot->GetTranslationUnitDecl(), &TooSmall),
			TEXT("V1 must reject an output view smaller than its full published layout")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(offsetof(asSASTDeclView, kind)), TooSmall.structSize,
			TEXT("rejected short V1 view must preserve caller structSize")));
		ASSERT_THAT(AreEqual((asDWORD)0xA57u, TooSmall.apiVersion,
			TEXT("rejected short V1 view must not overwrite caller apiVersion")));
		ASSERT_THAT(AreEqual((asBYTE)0x5Au, TooSmall.kind,
			TEXT("rejected short V1 view must not overwrite payload bytes")));

		asSASTDeclView IncompatibleVersion = {};
		IncompatibleVersion.structSize = sizeof(IncompatibleVersion);
		IncompatibleVersion.apiVersion = asAST_API_VERSION_1 + 1;
		IncompatibleVersion.kind = 0x5Au;
		ASSERT_THAT(AreEqual(asNOT_SUPPORTED,
			Snapshot->GetDecl(Snapshot->GetTranslationUnitDecl(), &IncompatibleVersion),
			TEXT("a non-zero incompatible view apiVersion must fail closed")));
		ASSERT_THAT(AreEqual(asAST_API_VERSION_1 + 1, IncompatibleVersion.apiVersion,
			TEXT("incompatible-version rejection must preserve the caller request")));
		ASSERT_THAT(AreEqual((asBYTE)0x5Au, IncompatibleVersion.kind,
			TEXT("incompatible-version rejection must not overwrite payload bytes")));
		ASSERT_THAT(IsTrue(Module->AcquireASTSnapshot(99) == nullptr, TEXT("unsupported version fails closed")));
		ASSERT_THAT(AreEqual(asNOT_SUPPORTED, Module->SetASTRetentionPolicy(asAST_DISCARD_AFTER_CODEGEN),
			TEXT("built module rejects in-place policy change")));
		Snapshot->Release();
	}

	TEST_METHOD(RetainedV1SnapshotTraversesSourceAndResolvedSemanticEdges)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptModule* Module = Engine.Get()->GetModule("SnapshotTraversal", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("traversal test needs a module")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"SnapshotTraversal.as", "int F(int Value) { return Value; }", 0, 7),
			TEXT("source section should add with a non-zero line offset")));
		ASSERT_THAT(AreEqual(0, Module->Build(), TEXT("retained source should build")));

		asIASTSnapshot* Snapshot = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Snapshot, TEXT("retained module should publish a V1 snapshot")));
		if (Snapshot == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Snapshot->Release(); };

		const asASTDeclId TranslationUnit = Snapshot->GetTranslationUnitDecl();
		asSASTDeclView TranslationUnitView = {};
		TranslationUnitView.structSize = sizeof(TranslationUnitView);
		ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(TranslationUnit, &TranslationUnitView)));
		ASSERT_THAT(IsTrue(TranslationUnitView.childCount > 0,
			TEXT("translation unit must publish its declaration traversal count")));

		asASTDeclId FunctionId;
		asSASTDeclView FunctionView = {};
		for (asUINT Index = 0; Index < TranslationUnitView.childCount; ++Index)
		{
			asASTDeclId Candidate;
			ASSERT_THAT(AreEqual(0, Snapshot->GetDeclChild(TranslationUnit, Index, &Candidate),
				TEXT("each declared child must be addressable through the owning snapshot")));
			asSASTDeclView CandidateView = {};
			CandidateView.structSize = sizeof(CandidateView);
			ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(Candidate, &CandidateView)));
			if (CandidateView.kind == asAST_DECL_FUNCTION
				&& CandidateView.name != nullptr
				&& FCStringAnsi::Strcmp(CandidateView.name, "F") == 0)
			{
				FunctionId = Candidate;
				FunctionView = CandidateView;
				break;
			}
		}
		ASSERT_THAT(IsTrue(FunctionId.IsValid(), TEXT("the function declaration must be reachable from the TU")));
		if (!FunctionId.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(FunctionView.body.IsValid(), TEXT("function body is a snapshot-local statement edge")));
		ASSERT_THAT(IsTrue(FunctionView.sourceRange.begin.file.IsValid(),
			TEXT("function source range carries a snapshot-owned source-file ID")));
		ASSERT_THAT(AreEqual(FunctionView.sourceRange.begin.file, FunctionView.sourceRange.end.file,
			TEXT("a V1 source range stays within one source file")));
		ASSERT_THAT(IsTrue(FunctionView.sourceRange.begin.offset <= FunctionView.sourceRange.end.offset,
			TEXT("a V1 source range is ordered")));

		asSASTSourceFileView SourceFile = {};
		SourceFile.structSize = sizeof(SourceFile);
		ASSERT_THAT(AreEqual(0, Snapshot->GetSourceFile(FunctionView.sourceRange.begin.file, &SourceFile),
			TEXT("the source range must resolve only through its owning snapshot")));
		ASSERT_THAT(AreEqual(asAST_SOURCE_AUTHORED, SourceFile.origin,
			TEXT("the user section is retained as authored source")));
		ASSERT_THAT(IsTrue(SourceFile.logicalKey != nullptr
			&& FCStringAnsi::Strcmp(SourceFile.logicalKey, "SnapshotTraversal.as") == 0,
			TEXT("the source view exposes a stable logical key rather than an internal buffer pointer")));
		asUINT SourceLine = 0;
		asUINT SourceColumn = 0;
		ASSERT_THAT(AreEqual(0, Snapshot->GetSourceLineColumn(
			FunctionView.sourceRange.begin, &SourceLine, &SourceColumn),
			TEXT("source offsets map to snapshot-owned line and column coordinates")));
		ASSERT_THAT(AreEqual((asUINT)8, SourceLine,
			TEXT("the public mapping preserves AddScriptSection lineOffset")));
		ASSERT_THAT(IsTrue(SourceColumn > 0, TEXT("source columns are one-based")));

		asSASTStmtView BodyView = {};
		BodyView.structSize = sizeof(BodyView);
		ASSERT_THAT(AreEqual(0, Snapshot->GetStmt(FunctionView.body, &BodyView)));
		ASSERT_THAT(AreEqual(asAST_STMT_BLOCK, BodyView.kind));
		ASSERT_THAT(IsTrue(BodyView.childCount > 0, TEXT("body block must enumerate its statement children")));
		asASTStmtId ReturnId;
		ASSERT_THAT(AreEqual(0, Snapshot->GetStmtChild(FunctionView.body, 0, &ReturnId)));
		asSASTStmtView ReturnView = {};
		ReturnView.structSize = sizeof(ReturnView);
		ASSERT_THAT(AreEqual(0, Snapshot->GetStmt(ReturnId, &ReturnView)));
		ASSERT_THAT(AreEqual(asAST_STMT_RETURN, ReturnView.kind));
		ASSERT_THAT(IsTrue(ReturnView.owner == FunctionId,
			TEXT("statement ownership is exposed as a snapshot-local declaration edge")));
		ASSERT_THAT(IsTrue(ReturnView.expr.IsValid(), TEXT("return expression is an explicit semantic edge")));

		asSASTExprView ValueView = {};
		ValueView.structSize = sizeof(ValueView);
		ASSERT_THAT(AreEqual(0, Snapshot->GetExpr(ReturnView.expr, &ValueView)));
		ASSERT_THAT(AreEqual(asAST_EXPR_DECL_REF, ValueView.kind));
		ASSERT_THAT(AreEqual(asAST_VALUE_LVALUE, ValueView.valueCategory,
			TEXT("the resolved parameter reference preserves its value category")));
		ASSERT_THAT(IsTrue(ValueView.resolvedDecl.IsValid(),
			TEXT("the declaration reference preserves its resolved declaration edge")));
		asSASTDeclView ParameterView = {};
		ParameterView.structSize = sizeof(ParameterView);
		ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(ValueView.resolvedDecl, &ParameterView)));
		ASSERT_THAT(AreEqual(asAST_DECL_PARAM, ParameterView.kind));
		ASSERT_THAT(IsTrue(ParameterView.name != nullptr
			&& FCStringAnsi::Strcmp(ParameterView.name, "Value") == 0,
			TEXT("the resolved target is the original parameter declaration")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ParameterView.formalIndex,
			TEXT("the public ParamDecl view exposes its sealed formal slot")));
	}

	TEST_METHOD(RetainedV1StmtViewAppendsLoopBodyEdgeSafePointCompatibly)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* const Module = Engine.Get()->GetModule(
			"SnapshotLoopBodySafePoint", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"SnapshotLoopBodySafePoint.as",
			"int F(bool Run) { while (Run) break; return 0; }")));
		ASSERT_THAT(AreEqual(0, Module->Build()));

		asIASTSnapshot* const Snapshot =
			Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Snapshot));
		if (Snapshot == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Snapshot->Release(); };

		asSASTDeclView TranslationUnit = {};
		TranslationUnit.structSize = sizeof(TranslationUnit);
		const asASTDeclId TranslationUnitId =
			Snapshot->GetTranslationUnitDecl();
		ASSERT_THAT(AreEqual(0,
			Snapshot->GetDecl(TranslationUnitId, &TranslationUnit)));
		asASTDeclId FunctionId;
		asSASTDeclView Function = {};
		for (asUINT Index = 0; Index < TranslationUnit.childCount; ++Index)
		{
			asASTDeclId Candidate;
			ASSERT_THAT(AreEqual(0, Snapshot->GetDeclChild(
				TranslationUnitId, Index, &Candidate)));
			asSASTDeclView CandidateView = {};
			CandidateView.structSize = sizeof(CandidateView);
			ASSERT_THAT(AreEqual(0,
				Snapshot->GetDecl(Candidate, &CandidateView)));
			if (CandidateView.kind == asAST_DECL_FUNCTION
				&& CandidateView.name != nullptr
				&& FCStringAnsi::Strcmp(CandidateView.name, "F") == 0)
			{
				FunctionId = Candidate;
				Function = CandidateView;
				break;
			}
		}
		ASSERT_THAT(IsTrue(FunctionId.IsValid()));
		ASSERT_THAT(IsTrue(Function.body.IsValid()));
		if (!Function.body.IsValid())
		{
			return;
		}

		asSASTStmtView FunctionBody = {};
		FunctionBody.structSize = sizeof(FunctionBody);
		ASSERT_THAT(AreEqual(0,
			Snapshot->GetStmt(Function.body, &FunctionBody)));
		asASTStmtId LoopId;
		for (asUINT Index = 0; Index < FunctionBody.childCount; ++Index)
		{
			asASTStmtId Candidate;
			ASSERT_THAT(AreEqual(0, Snapshot->GetStmtChild(
				Function.body, Index, &Candidate)));
			asSASTStmtView CandidateView = {};
			CandidateView.structSize = sizeof(CandidateView);
			ASSERT_THAT(AreEqual(0,
				Snapshot->GetStmt(Candidate, &CandidateView)));
			if (CandidateView.kind == asAST_STMT_WHILE)
			{
				LoopId = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsTrue(LoopId.IsValid()));
		if (!LoopId.IsValid())
		{
			return;
		}

		asSASTStmtView Loop = {};
		Loop.structSize = sizeof(Loop);
		ASSERT_THAT(AreEqual(0, Snapshot->GetStmt(LoopId, &Loop)));
		ASSERT_THAT(AreEqual((int)asAST_SAFEPOINT_LOOP_BACKEDGE,
			(int)Loop.safePointRole));
		ASSERT_THAT(AreEqual((int)asAST_SAFEPOINT_LOOP_ENTRY,
			(int)Loop.loopBodySafePointRole));
		ASSERT_THAT(AreEqual((asUINT)1, Loop.childCount));

		asASTStmtId BreakId;
		ASSERT_THAT(AreEqual(0,
			Snapshot->GetStmtChild(LoopId, 0, &BreakId)));
		asSASTStmtView Break = {};
		Break.structSize = sizeof(Break);
		ASSERT_THAT(AreEqual(0, Snapshot->GetStmt(BreakId, &Break)));
		ASSERT_THAT(AreEqual(asAST_STMT_BREAK, Break.kind));
		ASSERT_THAT(AreEqual((int)asAST_SAFEPOINT_TRANSFER,
			(int)Break.safePointRole,
			TEXT("unbraced Break retains its intrinsic role")));
		ASSERT_THAT(AreEqual((int)asAST_SAFEPOINT_NONE,
			(int)Break.loopBodySafePointRole));

		asSASTStmtView LegacyLoop = {};
		LegacyLoop.structSize = static_cast<asUINT>(
			offsetof(asSASTStmtView, loopBodySafePointRole));
		LegacyLoop.loopBodySafePointRole =
			asAST_SAFEPOINT_SWITCH_INVALID_VALUE;
		ASSERT_THAT(AreEqual(0, Snapshot->GetStmt(LoopId, &LegacyLoop),
			TEXT("pre-extension V1 StmtView capacity remains supported")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(sizeof(asSASTStmtView)),
			LegacyLoop.structSize));
		ASSERT_THAT(AreEqual((int)asAST_SAFEPOINT_SWITCH_INVALID_VALUE,
			(int)LegacyLoop.loopBodySafePointRole,
			TEXT("legacy capacity must not be overwritten past its extent")));
	}

	TEST_METHOD(RetainedV1SnapshotPublishesAccessSpecifierAndCapacityAwareMemberEdge)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());

		asIScriptModule* const LegacyModule =
			Engine.Get()->GetModule("AccessSnapshotLegacy", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(LegacyModule,
			TEXT("the access snapshot test needs a Legacy shadow module")));
		if (LegacyModule == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, LegacyModule->AddScriptSection(
			"AccessSnapshotLegacy.as",
			"class LegacyVault { "
			"access Internal = private, *(readonly); "
			"access:Internal int Value; "
			"}")));
		ASSERT_THAT(AreEqual(0, LegacyModule->Build(),
			TEXT("the default Legacy pipeline must preserve Builder access registration")));
		asCObjectType* const RuntimeVault = static_cast<asCObjectType*>(
			LegacyModule->GetTypeInfoByDecl("LegacyVault"));
		ASSERT_THAT(IsNotNull(RuntimeVault,
			TEXT("the transitional Builder path must still publish the Runtime type")));
		asSAccessSpecifier* const RuntimeAccess = RuntimeVault != nullptr
			? RuntimeVault->GetAccessSpecifier("Internal") : nullptr;
		ASSERT_THAT(IsNotNull(RuntimeAccess,
			TEXT("Builder Runtime access registration remains the shadow comparison path")));
		ASSERT_THAT(IsTrue(RuntimeAccess != nullptr
			&& RuntimeAccess->bIsPrivate
			&& RuntimeAccess->bAnyReadOnly,
			TEXT("Runtime shadow facts must agree with the authored access declaration")));

		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("access metadata must be produced by the explicit Canonical pipeline")));

		asIScriptModule* const Module =
			Engine.Get()->GetModule("AccessSnapshot", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("access snapshot test needs a module")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"AccessSnapshot.as",
			"class Vault { "
			"access Internal = private, *(readonly); "
			"access:Internal int Value; "
			"access:Internal int Read() { return Value; } "
			"} int Entry() { return 1; }")));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("Canonical access source must build before publication")));

		asIASTSnapshot* const Snapshot =
			Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Snapshot,
			TEXT("retained Canonical generation must publish its access metadata")));
		if (Snapshot == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Snapshot->Release(); };

		asSASTDeclView TranslationUnit = {};
		TranslationUnit.structSize = sizeof(TranslationUnit);
		ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(
			Snapshot->GetTranslationUnitDecl(), &TranslationUnit)));
		asASTDeclId Vault;
		for (asUINT Index = 0; Index < TranslationUnit.childCount; ++Index)
		{
			asASTDeclId Candidate;
			ASSERT_THAT(AreEqual(0, Snapshot->GetDeclChild(
				Snapshot->GetTranslationUnitDecl(), Index, &Candidate)));
			asSASTDeclView CandidateView = {};
			CandidateView.structSize = sizeof(CandidateView);
			ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(Candidate, &CandidateView)));
			if (CandidateView.kind == asAST_DECL_CLASS
				&& CandidateView.name != nullptr
				&& FCStringAnsi::Strcmp(CandidateView.name, "Vault") == 0)
			{
				Vault = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsTrue(Vault.IsValid(),
			TEXT("the retained record must be reachable from the translation unit")));
		if (!Vault.IsValid())
		{
			return;
		}

		asSASTDeclView VaultView = {};
		VaultView.structSize = sizeof(VaultView);
		ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(Vault, &VaultView)));
		asASTDeclId Access;
		asASTDeclId Field;
		asASTDeclId Method;
		for (asUINT Index = 0; Index < VaultView.childCount; ++Index)
		{
			asASTDeclId Candidate;
			ASSERT_THAT(AreEqual(0, Snapshot->GetDeclChild(Vault, Index, &Candidate)));
			asSASTDeclView CandidateView = {};
			CandidateView.structSize = sizeof(CandidateView);
			ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(Candidate, &CandidateView)));
			if (CandidateView.kind == asAST_DECL_ACCESS_SPECIFIER
				&& CandidateView.name != nullptr
				&& FCStringAnsi::Strcmp(CandidateView.name, "Internal") == 0)
			{
				Access = Candidate;
			}
			else if (CandidateView.kind == asAST_DECL_VAR
				&& CandidateView.name != nullptr
				&& FCStringAnsi::Strcmp(CandidateView.name, "Value") == 0)
			{
				Field = Candidate;
			}
			else if (CandidateView.kind == asAST_DECL_METHOD
				&& CandidateView.name != nullptr
				&& FCStringAnsi::Strcmp(CandidateView.name, "Read") == 0)
			{
				Method = Candidate;
			}
		}
		ASSERT_THAT(IsTrue(Access.IsValid() && Field.IsValid() && Method.IsValid(),
			TEXT("access definition and both referenced members must be public declarations")));
		if (!Access.IsValid() || !Field.IsValid() || !Method.IsValid())
		{
			return;
		}

		asSASTDeclView AccessView = {};
		AccessView.structSize = sizeof(AccessView);
		ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(Access, &AccessView)));
		ASSERT_THAT(AreEqual(asAST_DECL_ACCESS_SPECIFIER, AccessView.kind));
		ASSERT_THAT(IsTrue((AccessView.traits & asAST_TRAIT_PRIVATE) != 0));
		ASSERT_THAT(AreEqual((asUINT)1, AccessView.childCount,
			TEXT("the wildcard permission is an explicit access child")));
		asASTDeclId Permission;
		ASSERT_THAT(AreEqual(0, Snapshot->GetDeclChild(Access, 0, &Permission)));
		asSASTDeclView PermissionView = {};
		PermissionView.structSize = sizeof(PermissionView);
		ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(Permission, &PermissionView)));
		ASSERT_THAT(AreEqual(asAST_DECL_ACCESS_PERMISSION, PermissionView.kind));
		ASSERT_THAT(IsTrue((PermissionView.traits & asAST_TRAIT_ACCESS_WILDCARD) != 0
			&& (PermissionView.traits & asAST_TRAIT_ACCESS_READONLY) != 0,
			TEXT("permission traits preserve wildcard and readonly facts")));

		asSASTDeclView FieldView = {};
		FieldView.structSize = sizeof(FieldView);
		ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(Field, &FieldView)));
		asSASTDeclView MethodView = {};
		MethodView.structSize = sizeof(MethodView);
		ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(Method, &MethodView)));
		ASSERT_THAT(IsTrue(FieldView.accessSpecifier == Access
			&& MethodView.accessSpecifier == Access,
			TEXT("public member views retain the exact snapshot-local access DeclId edge")));

		asSASTDeclView LegacyCapacity = {};
		LegacyCapacity.structSize = static_cast<asUINT>(
			offsetof(asSASTDeclView, accessSpecifier));
		ASSERT_THAT(AreEqual(0, Snapshot->GetDecl(Field, &LegacyCapacity),
			TEXT("an older V1 caller ending before the appended edge remains compatible")));
		ASSERT_THAT(AreEqual(asAST_DECL_VAR, LegacyCapacity.kind));
		ASSERT_THAT(IsTrue(LegacyCapacity.name != nullptr
			&& FCStringAnsi::Strcmp(LegacyCapacity.name, "Value") == 0,
			TEXT("capacity-aware publication still fills every pre-extension field")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(sizeof(asSASTDeclView)),
			LegacyCapacity.structSize,
			TEXT("the view reports the current implementation size without writing the appended field")));

		asSASTDeclView AccessSpecifierCapacity = {};
		AccessSpecifierCapacity.structSize = static_cast<asUINT>(
			offsetof(asSASTDeclView, formalIndex));
		ASSERT_THAT(AreEqual(0,
			Snapshot->GetDecl(Field, &AccessSpecifierCapacity),
			TEXT("the previous access-specifier-capable V1 layout remains compatible")));
		ASSERT_THAT(IsTrue(AccessSpecifierCapacity.accessSpecifier == Access,
			TEXT("the prior appended access edge is written without requiring room for formalIndex")));
	}

	TEST_METHOD(RetainRebuildKeepsOldSnapshotLease)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("candidate-first snapshot replacement requires explicit Canonical selection")));

		asIScriptModule* Module = Engine.Get()->GetModule("LeaseSnap", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("lease module should exist")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection("LeaseSnap", "int F() { return 1; }")));
		ASSERT_THAT(AreEqual(0, Module->Build(), TEXT("generation A should build")));

		asIASTSnapshot* GenerationA = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(GenerationA, TEXT("generation A lease")));
		const FString KeyA = UTF8_TO_TCHAR(GenerationA->GetGenerationKey());
		ASSERT_THAT(IsTrue(GenerationA->IsCurrentGeneration(), TEXT("A is current before rebuild")));
		asSASTDeclView ViewA = {};
		ViewA.structSize = sizeof(ViewA);
		ASSERT_THAT(AreEqual(0, GenerationA->GetDecl(GenerationA->GetTranslationUnitDecl(), &ViewA),
			TEXT("A is traversable")));

		ASSERT_THAT(AreEqual(0, Module->AddScriptSection("LeaseSnap", "int F() { return 2; }")));
		ASSERT_THAT(AreEqual(0, Module->Build(), TEXT("generation B should build")));
		asIASTSnapshot* GenerationB = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(GenerationB, TEXT("generation B snapshot")));
		ASSERT_THAT(IsFalse(GenerationA->IsCurrentGeneration(), TEXT("A is not current after B publishes")));
		ASSERT_THAT(IsTrue(GenerationB->IsCurrentGeneration(), TEXT("B is current")));
		ASSERT_THAT(IsTrue(KeyA != FString(UTF8_TO_TCHAR(GenerationB->GetGenerationKey())),
			TEXT("generation keys must change")));
		asSASTDeclView ViewAAfter = {};
		ViewAAfter.structSize = sizeof(ViewAAfter);
		ASSERT_THAT(AreEqual(0, GenerationA->GetDecl(GenerationA->GetTranslationUnitDecl(), &ViewAAfter),
			TEXT("old lease remains traversable")));

		ASSERT_THAT(AreEqual(0, Module->AddScriptSection("LeaseSnap", "int F() { unknown_symbol; }")));
		ASSERT_THAT(IsTrue(Module->Build() < 0, TEXT("failed replacement must fail closed")));
		asIASTSnapshot* AfterFailure = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(AfterFailure, TEXT("failed replacement keeps a snapshot")));
		ASSERT_THAT(IsTrue(AfterFailure->IsCurrentGeneration(),
			TEXT("failed replacement keeps the last successful generation current")));
		AfterFailure->Release();
		GenerationB->Release();
		GenerationA->Release();
	}

	TEST_METHOD(MissingPendingContextCannotReplaceLastGoodSnapshot)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("last-good snapshot publication requires explicit Canonical selection")));

		asCModule* const Module = static_cast<asCModule*>(Engine.Get()->GetModule(
			"MissingPendingSnapshot", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module,
			TEXT("missing-pending snapshot test needs a module")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(
			asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"MissingPendingSnapshot.as", "int F() { return 1; }", 0, 0)));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("generation A must publish a retained snapshot")));

		asIASTSnapshot* const GenerationA =
			Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(GenerationA,
			TEXT("generation A needs a last-good snapshot lease")));
		if (GenerationA == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { GenerationA->Release(); };
		const FString GenerationKeyA =
			UTF8_TO_TCHAR(GenerationA->GetGenerationKey());
		ASSERT_THAT(IsTrue(GenerationA->IsCurrentGeneration(),
			TEXT("generation A must start current")));

		Module->PublishCanonicalASTSnapshot("missing-pending-context-test");

		ASSERT_THAT(IsTrue(GenerationA->IsCurrentGeneration(),
			TEXT("missing pending AST must not retire the last-good generation")));
		asIASTSnapshot* const Current =
			Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Current,
			TEXT("missing pending AST must preserve the public last-good snapshot")));
		if (Current == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Current->Release(); };
		ASSERT_THAT(IsTrue(Current == GenerationA,
			TEXT("no replacement snapshot may be fabricated without a real pending graph")));
		ASSERT_THAT(IsTrue(GenerationKeyA == FString(
			UTF8_TO_TCHAR(Current->GetGenerationKey())),
			TEXT("missing pending AST must not advance the snapshot generation key")));
	}

	TEST_METHOD(SnapshotPreparationFailureKeepsLastGoodExecutableAndSnapshot)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("candidate-first publication requires explicit Canonical selection")));

		asCModule* const Module = static_cast<asCModule*>(
			Engine.Get()->GetModule(
				"SnapshotPreparationFailure", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(
			asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"SnapshotPreparationFailure.as", "int F() { return 1; }", 0, 0)));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("generation A must publish executable and snapshot authority")));

		asIASTSnapshot* const GenerationA =
			Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(GenerationA));
		if (GenerationA == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { GenerationA->Release(); };
		const FString GenerationKeyA =
			UTF8_TO_TCHAR(GenerationA->GetGenerationKey());
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptSDKTestSupport::ExecuteScriptFunction<int32>(
				*TestRunner, Engine.Get(), Module, "int F()", Value)
			&& Value == 1,
			TEXT("generation A executable must be current before replacement")));

		Module->SetCanonicalSnapshotPreparationFailureForTesting(
			asOUT_OF_MEMORY);
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"SnapshotPreparationFailure.as", "int F() { return 2; }", 0, 0)));
		ASSERT_THAT(AreEqual(asOUT_OF_MEMORY, Module->Build(),
			TEXT("a valid sealed candidate that cannot prepare its snapshot must fail before publication")));

		asIASTSnapshot* const Current =
			Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Current));
		if (Current == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Current->Release(); };
		ASSERT_THAT(IsTrue(Current == GenerationA
			&& Current->IsCurrentGeneration()
			&& GenerationKeyA == FString(
				UTF8_TO_TCHAR(Current->GetGenerationKey())),
			TEXT("snapshot preparation failure must not expose or retire a generation")));
		Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptSDKTestSupport::ExecuteScriptFunction<int32>(
				*TestRunner, Engine.Get(), Module, "int F()", Value)
			&& Value == 1,
			TEXT("snapshot preparation failure must preserve the last-good executable with its snapshot")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Module->GetLastLegacyCompilerInvocationCount()));
	}

	TEST_METHOD(FailedRebuildKeepsCurrentSnapshotStableForConcurrentAcquires)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("failed candidate publication requires explicit Canonical selection")));

		asIScriptModule* Module = Engine.Get()->GetModule("FailedPublishRace", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("concurrency test needs a module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"FailedPublishRace", "int F() { return 1; }")));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("the initial retained generation must build")));

		asIASTSnapshot* const GenerationA = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(GenerationA, TEXT("the initial generation needs a lease")));
		if (GenerationA == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { GenerationA->Release(); };
		const FString GenerationKeyA = UTF8_TO_TCHAR(GenerationA->GetGenerationKey());

		FThreadSafeBool bStop = false;
		FThreadSafeBool bReaderAcquired = false;
		FThreadSafeBool bReaderObservedInvalidSnapshot = false;
		TFuture<void> Reader = Async(EAsyncExecution::Thread,
			[Module, &bStop, &bReaderAcquired, &bReaderObservedInvalidSnapshot]()
			{
				while (!bStop)
				{
					asIASTSnapshot* const Snapshot =
						Module->AcquireASTSnapshot(asAST_API_VERSION_1);
					if (Snapshot == nullptr)
					{
						bReaderObservedInvalidSnapshot = true;
						break;
					}

					bReaderAcquired = true;
					asSASTDeclView View = {};
					View.structSize = sizeof(View);
					const bool bValid = Snapshot->IsCurrentGeneration()
						&& Snapshot->GetDecl(Snapshot->GetTranslationUnitDecl(), &View) == 0
						&& View.apiVersion == asAST_API_VERSION_1
						&& View.structSize == sizeof(View);
					Snapshot->Release();
					if (!bValid)
					{
						bReaderObservedInvalidSnapshot = true;
						break;
					}
				}
			});

		for (int32 Attempt = 0; Attempt != 100 && !bReaderAcquired; ++Attempt)
		{
			FPlatformProcess::SleepNoStats(0.001f);
		}
		ASSERT_THAT(IsTrue((bool)bReaderAcquired,
			TEXT("the reader must hold an immutable snapshot lease before replacement fails")));

		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"FailedPublishRace", "int F() { unknown_symbol; }")));
		ASSERT_THAT(IsTrue(Module->Build() < 0,
			TEXT("the replacement must fail before snapshot publication")));

		bStop = true;
		Reader.Wait();
		ASSERT_THAT(IsFalse((bool)bReaderObservedInvalidSnapshot,
			TEXT("a failed replacement must leave concurrent snapshot readers on the complete last-good generation")));

		asIASTSnapshot* const Current = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Current, TEXT("failed replacement must retain the last-good snapshot")));
		if (Current == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Current->Release(); };
		ASSERT_THAT(IsTrue(Current->IsCurrentGeneration(),
			TEXT("the failed replacement must not retire the last successful generation")));
		ASSERT_THAT(IsTrue(GenerationKeyA == FString(UTF8_TO_TCHAR(Current->GetGenerationKey())),
			TEXT("a failed replacement must not publish a partial or replacement generation")));
	}

	TEST_METHOD(SnapshotRejectsForeignOpaqueIds)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptModule* FirstModule = Engine.Get()->GetModule("ForeignSnapshotFirst", asGM_ALWAYS_CREATE);
		asIScriptModule* SecondModule = Engine.Get()->GetModule("ForeignSnapshotSecond", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(FirstModule, TEXT("first retained module should exist")));
		ASSERT_THAT(IsNotNull(SecondModule, TEXT("second retained module should exist")));
		if (FirstModule == nullptr || SecondModule == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, FirstModule->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, SecondModule->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, FirstModule->AddScriptSection(
			"ForeignSnapshotFirst", "int First() { return 1; }")));
		ASSERT_THAT(AreEqual(0, SecondModule->AddScriptSection(
			"ForeignSnapshotSecond", "int Second() { return 2; }")));
		ASSERT_THAT(AreEqual(0, FirstModule->Build(), TEXT("first retained module should build")));
		ASSERT_THAT(AreEqual(0, SecondModule->Build(), TEXT("second retained module should build")));

		asIASTSnapshot* FirstSnapshot = FirstModule->AcquireASTSnapshot(asAST_API_VERSION_1);
		asIASTSnapshot* SecondSnapshot = SecondModule->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(FirstSnapshot, TEXT("first retained module should publish a snapshot")));
		ASSERT_THAT(IsNotNull(SecondSnapshot, TEXT("second retained module should publish a snapshot")));
		if (FirstSnapshot == nullptr || SecondSnapshot == nullptr)
		{
			if (SecondSnapshot != nullptr)
			{
				SecondSnapshot->Release();
			}
			if (FirstSnapshot != nullptr)
			{
				FirstSnapshot->Release();
			}
			return;
		}
		ON_SCOPE_EXIT
		{
			SecondSnapshot->Release();
			FirstSnapshot->Release();
		};

		const asASTDeclId FirstTranslationUnit = FirstSnapshot->GetTranslationUnitDecl();
		asSASTDeclView FirstView = {};
		FirstView.structSize = sizeof(FirstView);
		ASSERT_THAT(AreEqual(0, FirstSnapshot->GetDecl(FirstTranslationUnit, &FirstView),
			TEXT("the owning snapshot must accept its translation-unit opaque ID")));

		asSASTDeclView ForeignView = {};
		ForeignView.structSize = sizeof(ForeignView);
		ForeignView.apiVersion = 0xA57u;
		ForeignView.kind = 0x5Au;
		ASSERT_THAT(AreEqual(asINVALID_ARG, SecondSnapshot->GetDecl(FirstTranslationUnit, &ForeignView),
			TEXT("a snapshot must reject a valid opaque ID issued by another snapshot")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(sizeof(ForeignView)), ForeignView.structSize,
			TEXT("foreign-ID rejection must preserve the caller view capacity")));
		ASSERT_THAT(AreEqual((asDWORD)0xA57u, ForeignView.apiVersion,
			TEXT("foreign-ID rejection must not populate a view from the wrong snapshot")));
		ASSERT_THAT(AreEqual((asBYTE)0x5Au, ForeignView.kind,
			TEXT("foreign-ID rejection must not overwrite view payload bytes")));
	}

	TEST_METHOD(SnapshotLeaseOutlivesModuleAfterLastRelease)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptModule* Module = Engine.Get()->GetModule("LeaseDestroy", asGM_ALWAYS_CREATE);
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection("LeaseDestroy", "int F() { return 1; }")));
		ASSERT_THAT(AreEqual(0, Module->Build()));
		asIASTSnapshot* Lease = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Lease));
		Engine.Get()->DiscardModule("LeaseDestroy");
		asSASTDeclView View = {};
		View.structSize = sizeof(View);
		ASSERT_THAT(AreEqual(0, Lease->GetDecl(Lease->GetTranslationUnitDecl(), &View),
			TEXT("lease must survive module discard until released")));
		Lease->Release();
	}

	TEST_METHOD(CompileFunctionAddToModuleInvalidatesRetainedSnapshot)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptModule* Module = Engine.Get()->GetModule(
			"CompileFunctionSnapshot", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("CompileFunction snapshot test needs a module")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT),
			TEXT("retained module must select its policy before Build")));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"CompileFunctionSnapshot.as", "int F() { return 1; }"),
			TEXT("initial source must add")));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("initial retained module must build")));

		asIASTSnapshot* const GenerationA =
			Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(GenerationA,
			TEXT("initial Build must publish a current retained snapshot")));
		if (GenerationA == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { GenerationA->Release(); };
		ASSERT_THAT(IsTrue(GenerationA->IsCurrentGeneration(),
			TEXT("initial lease must start current")));

		asIScriptFunction* AddedFunction = nullptr;
		ASSERT_THAT(AreEqual(0, Module->CompileFunction(
			"CompileFunctionSnapshot_Add.as",
			"int Added() { return 2; }",
			0,
			asCOMP_ADD_TO_MODULE,
			&AddedFunction),
			TEXT("CompileFunction ADD_TO_MODULE must add the executable function")));
		ASSERT_THAT(IsNotNull(AddedFunction,
			TEXT("CompileFunction ADD_TO_MODULE must return the added function")));
		ON_SCOPE_EXIT
		{
			if (AddedFunction != nullptr)
			{
				AddedFunction->Release();
			}
		};
		ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int Added()"),
			TEXT("the new executable declaration must be present on the module")));
		ASSERT_THAT(IsFalse(GenerationA->IsCurrentGeneration(),
			TEXT("ADD_TO_MODULE must retire an incomplete retained AST generation")));
		ASSERT_THAT(IsNull(Module->AcquireASTSnapshot(asAST_API_VERSION_1),
			TEXT("ADD_TO_MODULE must not publish an old AST that omits the new function")));
	}

	TEST_METHOD(AcquireDuringRebuildOnlyReturnsStableLeases)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("atomic snapshot replacement requires explicit Canonical selection")));

		asIScriptModule* Module = Engine.Get()->GetModule("AcquirePublishRace", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("concurrency test needs a module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection("AcquirePublishRace", "int F() { return 0; }")));
		ASSERT_THAT(AreEqual(0, Module->Build(), TEXT("initial generation should build")));

		FThreadSafeBool bStop = false;
		FThreadSafeBool bAcquired = false;
		FThreadSafeBool bObservedInvalidLease = false;
		FThreadSafeBool bBuildFailed = false;
		TFuture<void> Reader = Async(EAsyncExecution::Thread, [Module, &bStop, &bAcquired, &bObservedInvalidLease]()
		{
			while (!bStop)
			{
				asIASTSnapshot* Snapshot = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
				if (Snapshot == nullptr)
				{
					bObservedInvalidLease = true;
					break;
				}

				bAcquired = true;
				asSASTDeclView View = {};
				View.structSize = sizeof(View);
				if (Snapshot->GetDecl(Snapshot->GetTranslationUnitDecl(), &View) != 0
					|| View.apiVersion != asAST_API_VERSION_1
					|| View.structSize != sizeof(View))
				{
					bObservedInvalidLease = true;
				}
				Snapshot->Release();
				if (bObservedInvalidLease)
				{
					break;
				}
			}
		});

		for (int32 Generation = 1; Generation <= 64; ++Generation)
		{
			const char* Source = (Generation & 1) == 0
				? "int F() { return 2; }"
				: "int F() { return 1; }";
			if (Module->AddScriptSection("AcquirePublishRace", Source) != 0 || Module->Build() != 0)
			{
				bBuildFailed = true;
				break;
			}
		}

		bStop = true;
		Reader.Wait();
		ASSERT_THAT(IsTrue((bool)bAcquired, TEXT("reader must acquire at least one module-managed lease")));
		ASSERT_THAT(IsFalse((bool)bBuildFailed, TEXT("each replacement generation should build")));
		ASSERT_THAT(IsFalse((bool)bObservedInvalidLease,
			TEXT("AcquireASTSnapshot must not return null, retired, or partially published leases during replacement")));

		asIASTSnapshot* Current = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Current, TEXT("the final generation remains available")));
		ASSERT_THAT(IsTrue(Current->IsCurrentGeneration(), TEXT("the final generation is current")));
		Current->Release();
	}
};

#endif
