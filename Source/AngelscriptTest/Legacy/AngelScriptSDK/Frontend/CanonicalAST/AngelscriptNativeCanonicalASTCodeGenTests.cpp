#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"
#include "../../Support/AngelscriptNativeCompilerBytecodeTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_bytecode_codegen.h"
#include "as_module.h"
#include "frontend/Parser/as_parser.h"
#include "source/as_scriptcode.h"
#include "as_scriptfunction.h"
#include "frontend/Sema/as_sema.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	asCSourceRange AddAuthoredCodeGenSource(
		asCASTContext& Context,
		const char* LogicalKey)
	{
		static const char Source[] = "synthetic canonical AST CodeGen fixture";
		const asASTFileID File = Context.AddSourceSection(
			LogicalKey,
			asAST_SOURCE_AUTHORED,
			Source,
			sizeof(Source) - 1,
			0);
		return Context.GetSourceManager().MakeRange(
			File, 0, sizeof(Source) - 1);
	}

	void AttachReturnAsBody(asCSema& Sema, asCASTContext& Ast, asASTDeclId Fn, asASTExprId Value)
	{
		const asCSourceRange Range;
		asCArray<asASTStmtId> Stmts;
		Stmts.PushLast(Sema.ActOnReturnStmt(Fn, Value, Range));
		Ast.SetBody(Fn, Sema.ActOnCompoundStmt(Fn, Stmts, Range));
	}

	int GenerateCanonicalFromSource(
		asCScriptEngine* ScriptEngine,
		asCModule* Module,
		const char* Section,
		const char* Source,
		FAutomationTestBase* Test = nullptr)
	{
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode(Section, Source, true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		const int ParseResult = Parser.ParseScript(&Code);
		if (ParseResult < 0)
		{
			if (Test != nullptr)
			{
				Test->AddError(FString::Printf(TEXT("canonical parse failed for %s: %d"),
					ANSI_TO_TCHAR(Section), ParseResult));
			}
			return ParseResult;
		}
		Sema.LayoutScriptClassFields();
		const int SealResult = Context.Seal();
		asCString AstDump;
		asCASTDump(Context, AstDump);
		if (SealResult != 0)
		{
			if (Test != nullptr)
			{
				Test->AddError(FString::Printf(
					TEXT("canonical seal failed for %s: %d ast=%s"),
					ANSI_TO_TCHAR(Section),
					SealResult,
					UTF8_TO_TCHAR(AstDump.AddressOf() ? AstDump.AddressOf() : "")));
			}
			return SealResult;
		}
		asCBytecodeCodeGen CodeGen;
		const int GenerateResult = CodeGen.Generate(Context, Module);
		if (GenerateResult != 0 && Test != nullptr)
		{
			Test->AddError(FString::Printf(
				TEXT("canonical Generate failed for %s: error=%d line=%d ast=%s"),
				ANSI_TO_TCHAR(Section),
				CodeGen.GetError(),
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(AstDump.AddressOf() ? AstDump.AddressOf() : "")));
		}
		return GenerateResult;
	}

	void CanonicalHandleAddRef(asIScriptGeneric*)
	{
	}

	void CanonicalHandleRelease(asIScriptGeneric*)
	{
	}

	bool GenerateExecuteDiscardInt(
		FAutomationTestBase& Test,
		asCScriptEngine* ScriptEngine,
		const char* ModuleName,
		const char* Source,
		const char* EntryDecl,
		int32 Expected)
	{
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, ModuleName);
		if (Module == nullptr)
		{
			return false;
		}
		const int GenerateResult = GenerateCanonicalFromSource(ScriptEngine, Module, ModuleName, Source, &Test);
		if (GenerateResult != 0)
		{
			return false;
		}
		int32 Value = 0;
		if (!AngelscriptNativeTestSupport::CanonicalExecuteInt(Test, ScriptEngine, Module, EntryDecl, Value)
			|| Value != Expected)
		{
			Test.AddError(FString::Printf(TEXT("canonical execute %s: got %d expected %d"),
				ANSI_TO_TCHAR(EntryDecl), Value, Expected));
			Module->Discard();
			return false;
		}
		Module->Discard();
		return true;
	}

	bool BytecodeContainsOpcode(asIScriptFunction* Function, const asEBCInstr Opcode)
	{
		asCScriptFunction* const Internal = static_cast<asCScriptFunction*>(Function);
		if (Internal == nullptr || Internal->scriptData == nullptr)
		{
			return false;
		}
		const asDWORD* const Bytes = Internal->scriptData->byteCode.AddressOf();
		const asUINT Length = Internal->scriptData->byteCode.GetLength();
		asUINT Index = 0;
		while (Index < Length)
		{
			const asEBCInstr Instruction = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(Bytes + Index));
			if (Instruction == Opcode)
			{
				return true;
			}
			const asUINT Size = static_cast<asUINT>(asBCTypeSize[asBCInfo[Instruction].type]);
			Index += Size > 0 ? Size : 1;
		}
		return false;
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTCodeGenTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.CodeGen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CodeGenRejectsUnsealedAstAndAcceptsSealed)
	{
		asCASTContext Open;
		asCSema Sema(nullptr, Open);
		Sema.ActOnTranslationUnit("CodeGenLifecycle");
		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION, CodeGen.Generate(Open, nullptr),
			TEXT("CodeGen must reject an unsealed context")));
		ASSERT_THAT(IsTrue(CodeGen.Failed(), TEXT("failure must be sticky and must not publish functions")));

		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Open.AdvanceLifecycle(
			asAST_CONTEXT_BUILDING, asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("fixture must reach SemaFinalized")));
		asCBytecodeCodeGen SemaFinalized;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION,
			SemaFinalized.Generate(Open, nullptr),
			TEXT("CodeGen must reject SemaFinalized without a lifetime plan")));

		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Open.AdvanceLifecycle(
			asAST_CONTEXT_SEMA_FINALIZED, asAST_CONTEXT_LIFETIME_PLANNED),
			TEXT("fixture must reach LifetimePlanned")));
		asCBytecodeCodeGen LifetimePlanned;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION,
			LifetimePlanned.Generate(Open, nullptr),
			TEXT("CodeGen must reject a lifetime-planned but unfrozen Context")));

		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Open.AdvanceLifecycle(
			asAST_CONTEXT_LIFETIME_PLANNED, asAST_CONTEXT_FROZEN_PUBLISHABLE),
			TEXT("fixture must reach Frozen/Publishable")));
		asCBytecodeCodeGen Ready;
		ASSERT_THAT(AreEqual(0, Ready.Generate(Open, nullptr), TEXT("Frozen/Publishable AST is accepted")));
		ASSERT_THAT(IsFalse(Ready.Failed(), TEXT("success leaves no partial failure flag")));
	}

	TEST_METHOD(CodeGenLeavesAstDumpUnchanged)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("DumpEq");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
		AttachReturnAsBody(Sema, Context, Fn, Sema.ActOnIntegerLiteral(7, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("dump-equal graph should seal")));

		const FString Before = AngelscriptNativeTestSupport::CanonicalNormalizeAstDump(Context);
		asCBytecodeCodeGen First;
		(void)First.Generate(Context, nullptr);
		asCBytecodeCodeGen Second;
		(void)Second.Generate(Context, nullptr);
		const FString After = AngelscriptNativeTestSupport::CanonicalNormalizeAstDump(Context);
		ASSERT_THAT(AreEqual(Before, After, TEXT("CodeGen must not mutate sealed AST nodes")));
	}

	TEST_METHOD(CodeGenFailureLeavesNoPartialModuleState)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "Partial");
		ASSERT_THAT(IsNotNull(Module, TEXT("partial-state test needs a module")));

		asCASTContext Open;
		asCSema Sema(nullptr, Open);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("Partial");
		const asCQualType IntType = Open.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
		AttachReturnAsBody(Sema, Open, Fn, Sema.ActOnIntegerLiteral(7, asCSourceRange()));

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION, CodeGen.Generate(Open, Module),
			TEXT("unsealed Generate must fail before publishing")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("failure must leave no module functions")));
	}

	TEST_METHOD(CodeGenRequiresModuleToPublishFunctions)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("NeedMod");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
		AttachReturnAsBody(Sema, Context, Fn, Sema.ActOnIntegerLiteral(7, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("function AST should seal")));

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(asNO_MODULE, CodeGen.Generate(Context, nullptr),
			TEXT("function bodies cannot be published without a module")));
		ASSERT_THAT(IsTrue(CodeGen.Failed(), TEXT("missing module is a sticky failure")));
	}

	TEST_METHOD(CodeGenDoesNotPublishPartialFunctionsOnUnsupportedBody)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "NoPartial");
		ASSERT_THAT(IsNotNull(Module, TEXT("unsupported-body test needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("NoPartial");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId OkFn = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
		AttachReturnAsBody(Sema, Context, OkFn, Sema.ActOnIntegerLiteral(7, asCSourceRange()));

		const asCQualType Obj = Context.InternNamedType(asAST_TYPE_VALUE_OBJECT, "FValue", 0);
		const asASTDeclId BadFn = Sema.ActOnFunctionDecl(Tu, "Make", Obj, asCSourceRange());
		asCArray<asASTExprId> Args;
		AttachReturnAsBody(Sema, Context, BadFn, Sema.ActOnConstruct(Obj, Args, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("mixed supported/unsupported graph should still verify")));

		asCBytecodeCodeGen CodeGen;
		const int GenerateResult = CodeGen.Generate(Context, Module);
		ASSERT_THAT(IsTrue(GenerateResult != 0,
			TEXT("an unresolved construct/lifetime must fail closed before installing functions")));
		ASSERT_THAT(IsTrue(CodeGen.Failed(),
			TEXT("a verifier-level publication rejection must mark CodeGen as failed")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("failure must not leave the earlier function published")));
	}

	TEST_METHOD(CodeGenEmitsIntegerLiteralReturn)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "LitRet");
		ASSERT_THAT(IsNotNull(Module, TEXT("literal-return test needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("LitRet");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCSourceRange FunctionRange = AddAuthoredCodeGenSource(Context, "LitRet/F.as");
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "F", IntType, FunctionRange);
		AttachReturnAsBody(Sema, Context, Fn, Sema.ActOnIntegerLiteral(7, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("literal-return graph should seal")));

		const FString Before = AngelscriptNativeTestSupport::CanonicalNormalizeAstDump(Context);
		asCBytecodeCodeGen CodeGen;
		const int GenerateResult = CodeGen.Generate(Context, Module);
		ASSERT_THAT(AreEqual(0, GenerateResult,
			*FString::Printf(
				TEXT("sealed integer-return function should lower; result=%d detail=%s"),
				GenerateResult,
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		ASSERT_THAT(IsFalse(CodeGen.Failed(), TEXT("successful emit must clear the failure flag")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher(),
			TEXT("isolated Generate must record canonical CodeGen as publisher")));
		ASSERT_THAT(AreEqual(Before, AngelscriptNativeTestSupport::CanonicalNormalizeAstDump(Context),
			TEXT("successful emit must not mutate AST")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value),
			TEXT("canonical CodeGen function should be executable")));
		ASSERT_THAT(AreEqual(7, Value, TEXT("int F() { return 7; } must return 7")));
	}

	TEST_METHOD(CodeGenEmitsParameterReadsAndScalarOps)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "ScalarOps");
		ASSERT_THAT(IsNotNull(Module, TEXT("scalar-ops test needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("ScalarOps");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCSourceRange FunctionRange = AddAuthoredCodeGenSource(Context, "ScalarOps/Add.as");
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "Add", IntType, FunctionRange);
		const asASTDeclId A = Sema.ActOnParamDecl(Fn, "A", IntType, asCSourceRange());
		const asASTDeclId B = Sema.ActOnParamDecl(Fn, "B", IntType, asCSourceRange());
		const asASTExprId Sum = Sema.ActOnBinary(
			"+",
			Sema.ActOnDeclRef(A, IntType, asCSourceRange()),
			Sema.ActOnDeclRef(B, IntType, asCSourceRange()),
			IntType,
			asCSourceRange());
		AttachReturnAsBody(Sema, Context, Fn, Sum);
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("parameter-arithmetic graph should seal")));

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(0, CodeGen.Generate(Context, Module),
			TEXT("parameter reads and integer add should lower")));

		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, Engine.Get(), Module, "int Add(int, int)");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Add(int,int) should resolve after CodeGen")));
		Invoker.AddArg(3).AddArg(4);
		ASSERT_THAT(AreEqual(7, Invoker.CallAndReturn<int32>(), TEXT("Add(3,4) must return 7")));
	}

	TEST_METHOD(CodeGenEmitsLocalAssignmentAndUnaryMinus)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "Locals");
		ASSERT_THAT(IsNotNull(Module, TEXT("local-assign test needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("Locals");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCSourceRange FunctionRange = AddAuthoredCodeGenSource(Context, "Locals/Negate.as");
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "Negate", IntType, FunctionRange);
		const asASTDeclId X = Sema.ActOnParamDecl(Fn, "X", IntType, asCSourceRange());
		const asASTDeclId Y = Sema.ActOnVarDecl(Fn, "Y", IntType, asCSourceRange());

		asCArray<asASTStmtId> Body;
		const asASTStmtId DeclY = Context.CreateStmt(asAST_STMT_DECL, Fn, asCSourceRange());
		Context.SetStmtDecl(DeclY, Y);
		Body.PushLast(DeclY);
		Body.PushLast(Sema.ActOnExprStmt(
			Fn,
			Sema.ActOnAssign(
				Sema.ActOnDeclRef(Y, IntType, asCSourceRange()),
				Sema.ActOnUnary("-", Sema.ActOnDeclRef(X, IntType, asCSourceRange()), IntType, asCSourceRange()),
				IntType,
				asCSourceRange()),
			asCSourceRange()));
		Body.PushLast(Sema.ActOnReturnStmt(Fn, Sema.ActOnDeclRef(Y, IntType, asCSourceRange()), asCSourceRange()));
		const asASTStmtId Block = Sema.ActOnBlock(Fn, Body, asCSourceRange());
		Context.SetBody(Fn, Block);
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("local assignment graph should seal")));

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(0, CodeGen.Generate(Context, Module),
			TEXT("locals, assignment, and unary minus should lower")));

		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, Engine.Get(), Module, "int Negate(int)");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Negate(int) should resolve after CodeGen")));
		Invoker.AddArg(5);
		ASSERT_THAT(AreEqual(-5, Invoker.CallAndReturn<int32>(), TEXT("Negate(5) must return -5")));
	}

	TEST_METHOD(CodeGenEmitsIfAndComparison)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "IfCmp");
		ASSERT_THAT(IsNotNull(Module, TEXT("if-comparison test needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("IfCmp");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);
		const asCSourceRange FunctionRange = AddAuthoredCodeGenSource(Context, "IfCmp/Abs.as");
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "Abs", IntType, FunctionRange);
		const asASTDeclId X = Sema.ActOnParamDecl(Fn, "X", IntType, asCSourceRange());
		const asASTExprId Cond = Sema.ActOnBinary(
			">",
			Sema.ActOnDeclRef(X, IntType, asCSourceRange()),
			Sema.ActOnIntegerLiteral(0, asCSourceRange()),
			BoolType,
			asCSourceRange());
		const asASTStmtId ThenStmt = Sema.ActOnReturnStmt(
			Fn, Sema.ActOnDeclRef(X, IntType, asCSourceRange()), asCSourceRange());
		const asASTStmtId ElseStmt = Sema.ActOnReturnStmt(
			Fn,
			Sema.ActOnUnary("-", Sema.ActOnDeclRef(X, IntType, asCSourceRange()), IntType, asCSourceRange()),
			asCSourceRange());
		const asASTStmtId IfStmt = Sema.ActOnIf(Fn, Cond, ThenStmt, ElseStmt, asCSourceRange());
		Context.SetBody(Fn, IfStmt);
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("if/compare graph should seal")));

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(0, CodeGen.Generate(Context, Module), TEXT("if and comparison should lower")));

		AngelscriptSDKTestSupport::FSdkFunctionInvoker Pos(*TestRunner, Engine.Get(), Module, "int Abs(int)");
		ASSERT_THAT(IsTrue(Pos.IsValid(), TEXT("Abs(int) should resolve")));
		Pos.AddArg(4);
		ASSERT_THAT(AreEqual(4, Pos.CallAndReturn<int32>(), TEXT("Abs(4) must return 4")));

		AngelscriptSDKTestSupport::FSdkFunctionInvoker Neg(*TestRunner, Engine.Get(), Module, "int Abs(int)");
		ASSERT_THAT(IsTrue(Neg.IsValid(), TEXT("Abs(int) should resolve for the negative path")));
		Neg.AddArg(-3);
		ASSERT_THAT(AreEqual(3, Neg.CallAndReturn<int32>(), TEXT("Abs(-3) must return 3")));
	}

	TEST_METHOD(CodeGenEmitsWhileLoop)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "WhileLoop");
		ASSERT_THAT(IsNotNull(Module, TEXT("while-loop test needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("WhileLoop");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);
		const asCSourceRange FunctionRange = AddAuthoredCodeGenSource(Context, "WhileLoop/Count.as");
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "Count", IntType, FunctionRange);
		const asASTDeclId N = Sema.ActOnParamDecl(Fn, "N", IntType, asCSourceRange());
		const asASTDeclId Y = Sema.ActOnVarDecl(Fn, "Y", IntType, asCSourceRange());

		asCArray<asASTStmtId> Body;
		const asASTStmtId DeclY = Context.CreateStmt(asAST_STMT_DECL, Fn, asCSourceRange());
		Context.SetStmtDecl(DeclY, Y);
		Body.PushLast(DeclY);
		Body.PushLast(Sema.ActOnExprStmt(
			Fn,
			Sema.ActOnAssign(
				Sema.ActOnDeclRef(Y, IntType, asCSourceRange()),
				Sema.ActOnIntegerLiteral(0, asCSourceRange()),
				IntType,
				asCSourceRange()),
			asCSourceRange()));
		const asASTExprId Cond = Sema.ActOnBinary(
			"<",
			Sema.ActOnDeclRef(Y, IntType, asCSourceRange()),
			Sema.ActOnDeclRef(N, IntType, asCSourceRange()),
			BoolType,
			asCSourceRange());
		const asASTStmtId Incr = Sema.ActOnExprStmt(
			Fn,
			Sema.ActOnAssign(
				Sema.ActOnDeclRef(Y, IntType, asCSourceRange()),
				Sema.ActOnBinary(
					"+",
					Sema.ActOnDeclRef(Y, IntType, asCSourceRange()),
					Sema.ActOnIntegerLiteral(1, asCSourceRange()),
					IntType,
					asCSourceRange()),
				IntType,
				asCSourceRange()),
			asCSourceRange());
		Body.PushLast(Sema.ActOnWhile(Fn, Cond, Incr, asCSourceRange()));
		Body.PushLast(Sema.ActOnReturnStmt(Fn, Sema.ActOnDeclRef(Y, IntType, asCSourceRange()), asCSourceRange()));
		const asASTStmtId Block = Sema.ActOnBlock(Fn, Body, asCSourceRange());
		Context.SetBody(Fn, Block);
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("while-loop graph should seal")));

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(0, CodeGen.Generate(Context, Module), TEXT("while loop should lower")));

		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, Engine.Get(), Module, "int Count(int)");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Count(int) should resolve")));
		Invoker.AddArg(4);
		ASSERT_THAT(AreEqual(4, Invoker.CallAndReturn<int32>(), TEXT("Count(4) must return 4")));
	}

	TEST_METHOD(CodeGenEmitsCallInReverseFormalOrder)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "CallRev");
		ASSERT_THAT(IsNotNull(Module, TEXT("call-order test needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("CallRev");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCSourceRange FunctionRange = AddAuthoredCodeGenSource(Context, "CallRev/Functions.as");
		const asASTDeclId Sub = Sema.ActOnFunctionDecl(Tu, "Sub", IntType, FunctionRange);
		const asASTDeclId A = Sema.ActOnParamDecl(Sub, "A", IntType, asCSourceRange());
		const asASTDeclId B = Sema.ActOnParamDecl(Sub, "B", IntType, asCSourceRange());
		AttachReturnAsBody(
			Sema,
			Context,
			Sub,
			Sema.ActOnBinary(
				"-",
				Sema.ActOnDeclRef(A, IntType, asCSourceRange()),
				Sema.ActOnDeclRef(B, IntType, asCSourceRange()),
				IntType,
				asCSourceRange()));

		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "F", IntType, FunctionRange);
		asCArray<asASTExprId> Args;
		Args.PushLast(Sema.ActOnIntegerLiteral(3, asCSourceRange()));
		Args.PushLast(Sema.ActOnIntegerLiteral(4, asCSourceRange()));
		AttachReturnAsBody(Sema, Context, Fn, Sema.ActOnCall(Sub, Args, IntType, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("call graph should seal")));

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(0, CodeGen.Generate(Context, Module), TEXT("script-to-script call should lower")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value),
			TEXT("F() should execute through the lowered call")));
		ASSERT_THAT(AreEqual(-1, Value, TEXT("F() must return Sub(3,4) == -1, proving reverse-formal pushes")));
	}

	TEST_METHOD(CodeGenParserSourceEmitsDebugLinesAndMatchesLegacy)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const CanonicalModule = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "CanonSrc");
		ASSERT_THAT(IsNotNull(CanonicalModule, TEXT("parser CodeGen needs a module")));

		const char* const Source = "int F() { return 7; }";
		asCBuilder Builder(ScriptEngine, CanonicalModule);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("CanonSrc", Source, true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code), TEXT("parser should feed Sema")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("parser-built graph should seal")));

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(0, CodeGen.Generate(Context, CanonicalModule),
			TEXT("parser-built sealed AST should lower")));

		int32 CanonicalValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), CanonicalModule, "int F()", CanonicalValue),
			TEXT("canonical parser+CodeGen F() should execute")));
		ASSERT_THAT(AreEqual(7, CanonicalValue, TEXT("canonical F() must return 7")));

		asIScriptFunction* const Function = CanonicalModule->GetFunctionByDecl("int F()");
		ASSERT_THAT(IsNotNull(Function, TEXT("debug metadata needs the published function")));
		asCScriptFunction* const CanonicalFunction = static_cast<asCScriptFunction*>(Function);
		asCString CanonicalDump;
		asCBytecodeCodeGenDumpFunction(CanonicalFunction, CanonicalDump);
		const int CanonicalNextFromZero = Function->FindNextLineWithCode(0);
		ASSERT_THAT(AreEqual(-1, CanonicalNextFromZero,
			TEXT("FindNextLineWithCode must reject a query before the function declaration")));
		const int CanonicalDeclaredLine = CanonicalFunction->scriptData->declaredAt & 0xFFFFF;
		const int CanonicalNextLine = Function->FindNextLineWithCode(CanonicalDeclaredLine);
		ASSERT_THAT(IsTrue(CanonicalNextLine > 0,
			FString::Printf(
				TEXT("CodeGen should publish debug line numbers as backend output; declaredLine=%d nextLine=%d dump=%s"),
				CanonicalDeclaredLine,
				CanonicalNextLine,
				UTF8_TO_TCHAR(CanonicalDump.AddressOf() ? CanonicalDump.AddressOf() : ""))));

		asIScriptModule* LegacyModule = nullptr;
		const asECompilerPipeline PreviousPipeline = ScriptEngine->GetCompilerPipeline();
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("the differential half must explicitly select LEGACY")));
		const int LegacyCompileResult = AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(), "LegacySrc", Source, LegacyModule);
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(PreviousPipeline),
			TEXT("the test must restore the caller's compiler selection")));
		ASSERT_THAT(AreEqual(0, LegacyCompileResult,
			TEXT("legacy pipeline should still compile the same source")));
		asCModule* const LegacyConcreteModule = static_cast<asCModule*>(LegacyModule);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asBYTECODE_PUBLISHER_COMPILER),
			static_cast<int32>(LegacyConcreteModule->GetLastBytecodePublisher()),
			TEXT("the differential module must be emitted by the legacy compiler")));
		ASSERT_THAT(IsTrue(LegacyConcreteModule->GetLastLegacyCompilerInvocationCount() > 0,
			TEXT("the differential module must observe a legacy compiler invocation")));
		asIScriptFunction* const LegacyFunctionView = LegacyModule->GetFunctionByDecl("int F()");
		ASSERT_THAT(IsNotNull(LegacyFunctionView, TEXT("legacy debug metadata needs the published function")));
		asCScriptFunction* const LegacyFunction = static_cast<asCScriptFunction*>(LegacyFunctionView);
		asCString LegacyDump;
		asCBytecodeCodeGenDumpFunction(LegacyFunction, LegacyDump);
		const int LegacyDeclaredLine = LegacyFunction->scriptData->declaredAt & 0xFFFFF;
		const int LegacyNextLine = LegacyFunctionView->FindNextLineWithCode(LegacyDeclaredLine);
		ASSERT_THAT(AreEqual(CanonicalDeclaredLine, LegacyDeclaredLine,
			FString::Printf(TEXT("canonical and legacy declaration rows must match; canonical=%s legacy=%s"),
				UTF8_TO_TCHAR(CanonicalDump.AddressOf() ? CanonicalDump.AddressOf() : ""),
				UTF8_TO_TCHAR(LegacyDump.AddressOf() ? LegacyDump.AddressOf() : ""))));
		ASSERT_THAT(AreEqual(CanonicalNextLine, LegacyNextLine,
			FString::Printf(TEXT("canonical and legacy first executable rows must match; canonical=%s legacy=%s"),
				UTF8_TO_TCHAR(CanonicalDump.AddressOf() ? CanonicalDump.AddressOf() : ""),
				UTF8_TO_TCHAR(LegacyDump.AddressOf() ? LegacyDump.AddressOf() : ""))));
		int32 LegacyValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), LegacyModule, "int F()", LegacyValue),
			TEXT("legacy F() should execute")));
		ASSERT_THAT(AreEqual(LegacyValue, CanonicalValue,
			TEXT("isolated canonical vs legacy execution must match")));
	}

	TEST_METHOD(CodeGenSaveLoadByteCodeRoundTripHasNoAstBytes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "SaveSrc");
		ASSERT_THAT(IsNotNull(Module, TEXT("save/load test needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("SaveSrc");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCSourceRange FunctionRange = AddAuthoredCodeGenSource(Context, "SaveSrc/F.as");
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "F", IntType, FunctionRange);
		AttachReturnAsBody(Sema, Context, Fn, Sema.ActOnIntegerLiteral(7, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("save/load graph should seal")));
		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(0, CodeGen.Generate(Context, Module), TEXT("save/load source should lower")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->SaveByteCode(&Stream, false),
			TEXT("canonical Bytecode should save")));
		ASSERT_THAT(IsTrue(Stream.Num() > 0, TEXT("VM archive must be non-empty")));

		const TArray<asBYTE>& Bytes = Stream.GetBytes();
		auto ContainsAscii = [&Bytes](const char* Needle) -> bool
		{
			const int32 NeedleLen = static_cast<int32>(FCStringAnsi::Strlen(Needle));
			if (NeedleLen <= 0 || Bytes.Num() < NeedleLen)
			{
				return false;
			}
			for (int32 Index = 0; Index + NeedleLen <= Bytes.Num(); ++Index)
			{
				if (FMemory::Memcmp(Bytes.GetData() + Index, Needle, NeedleLen) == 0)
				{
					return true;
				}
			}
			return false;
		};
		ASSERT_THAT(IsFalse(ContainsAscii("TypedSemantic"),
			TEXT("VM archive must not contain HIR sidecar bytes")));
		ASSERT_THAT(IsFalse(ContainsAscii("ASTBodySidecar"),
			TEXT("VM archive must not contain AST sidecar bytes")));

		Stream.ResetReadPosition();
		asIScriptModule* const Restored = ScriptEngine->GetModule("SaveDst", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Restored, TEXT("restored module should exist")));
		bool bStripped = true;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Restored->LoadByteCode(&Stream, &bStripped),
			TEXT("canonical Bytecode should load")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Restored, "int F()", Value),
			TEXT("loaded canonical Bytecode should execute")));
		ASSERT_THAT(AreEqual(7, Value, TEXT("loaded F() must return 7")));
	}

	TEST_METHOD(CodeGenLoadByteCodeRejectsUnsupportedVersion)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "VerSrc");
		ASSERT_THAT(IsNotNull(Module, TEXT("version-reject test needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("VerSrc");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCSourceRange FunctionRange = AddAuthoredCodeGenSource(Context, "VerSrc/F.as");
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "F", IntType, FunctionRange);
		AttachReturnAsBody(Sema, Context, Fn, Sema.ActOnIntegerLiteral(7, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("version-reject graph should seal")));
		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(0, CodeGen.Generate(Context, Module), TEXT("version-reject source should lower")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->SaveByteCode(&Stream, false),
			TEXT("canonical Bytecode should save before version mutation")));
		TArray<asBYTE>& Bytes = const_cast<TArray<asBYTE>&>(Stream.GetBytes());
		ASSERT_THAT(IsTrue(Bytes.Num() >= 2, TEXT("VM archive has a magic+version prefix")));
		Bytes[1] = static_cast<asBYTE>(99);

		Stream.ResetReadPosition();
		asIScriptModule* const Restored = ScriptEngine->GetModule("VerDst", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Restored, TEXT("unsupported-version destination should exist")));
		bool bStripped = true;
		ASSERT_THAT(IsTrue(Restored->LoadByteCode(&Stream, &bStripped) < 0,
			TEXT("unsupported bytecode version must fail closed")));
	}

	TEST_METHOD(CodeGenEmitsGlobalIntReadWrite)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "Glob");
		ASSERT_THAT(IsNotNull(Module, TEXT("global test needs a module")));
		ASSERT_THAT(AreEqual(0, GenerateCanonicalFromSource(
			ScriptEngine,
			Module,
			"Glob",
			"int G; int SetAndGet(int X) { G = X; return G; }"),
			TEXT("global int read/write should lower")));

		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, Engine.Get(), Module, "int SetAndGet(int)");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("SetAndGet(int) should resolve")));
		Invoker.AddArg(9);
		ASSERT_THAT(AreEqual(9, Invoker.CallAndReturn<int32>(), TEXT("SetAndGet(9) must store and return the global")));
	}

	TEST_METHOD(CodeGenGlobalInt8WriteUsesWrtv1)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "GlobInt8");
		ASSERT_THAT(IsNotNull(Module, TEXT("narrow global test needs a module")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, GenerateCanonicalFromSource(
			ScriptEngine,
			Module,
			"GlobInt8",
			"int8 G; int8 SetAndGet(int8 X) { G = X; return G; }",
			TestRunner),
			TEXT("global int8 read/write should lower")));

		asIScriptFunction* const Published = Module->GetFunctionByName("SetAndGet");
		ASSERT_THAT(IsNotNull(Published, TEXT("int8 global writer must install")));
		if (Published == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Published, asBC_WRTV1),
			TEXT("int8 global assignment must use WRTV1 from the sealed global type")));
		ASSERT_THAT(IsFalse(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Published, asBC_WRTV4),
			TEXT("int8 global assignment must not use hardcoded WRTV4")));
	}

	TEST_METHOD(CodeGenEmitsValueObjectConstructMemberAndCleanup)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		AngelscriptNativeTestSupport::FNativeLifecycleRecorder Recorder;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::RegisterNativeCaseValue(*ScriptEngine, Recorder),
			TEXT("value-object test should register FNativeCaseValue")));

		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "ValObj");
		ASSERT_THAT(IsNotNull(Module, TEXT("value-object test needs a module")));
		ASSERT_THAT(AreEqual(0, GenerateCanonicalFromSource(
			ScriptEngine,
			Module,
			"ValObj",
			"int F() { FNativeCaseValue V(7); return V.Value; }"),
			TEXT("value-object construct/member/cleanup should lower")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value),
			TEXT("value-object function should execute")));
		ASSERT_THAT(AreEqual(7, Value, TEXT("F() must return the constructed member")));
		ASSERT_THAT(IsTrue(Recorder.Num(AngelscriptNativeTestSupport::ENativeLifecycleEvent::ValueConstruct) >= 1,
			TEXT("constructor must run")));
		ASSERT_THAT(IsTrue(Recorder.Num(AngelscriptNativeTestSupport::ENativeLifecycleEvent::Destruct) >= 1,
			TEXT("canonical cleanup must run the destructor")));
		ASSERT_THAT(AreEqual(0, Recorder.GetLiveObjectCount(), TEXT("no value-object leak after return")));
	}

	TEST_METHOD(CodeGenEmitsHandleParameterNullCheck)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType("CObj", 0, asOBJ_REF) >= 0
			&& ScriptEngine->RegisterObjectBehaviour("CObj", asBEHAVE_ADDREF, "void f()", asFUNCTION(CanonicalHandleAddRef), asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectBehaviour("CObj", asBEHAVE_RELEASE, "void f()", asFUNCTION(CanonicalHandleRelease), asCALL_GENERIC) >= 0,
			TEXT("handle test should register a ref type")));

		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "Hdl");
		ASSERT_THAT(IsNotNull(Module, TEXT("handle test needs a module")));
		ASSERT_THAT(AreEqual(0, GenerateCanonicalFromSource(
			ScriptEngine,
			Module,
			"Hdl",
			"bool IsNull(CObj Obj) { return Obj == nullptr; }"),
			TEXT("handle null check should lower")));

		asIScriptFunction* Alt = Module->GetFunctionByDecl("bool IsNull(CObj@+)");
		if (Alt == nullptr)
		{
			Alt = Module->GetFunctionByDecl("bool IsNull(CObj@)");
		}
		if (Alt == nullptr)
		{
			Alt = Module->GetFunctionByDecl("bool IsNull(CObj)");
		}
		if (Alt == nullptr && Module->GetFunctionCount() > 0)
		{
			Alt = Module->GetFunctionByIndex(0);
		}
		ASSERT_THAT(IsNotNull(Alt, TEXT("IsNull should resolve")));
		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("handle test needs a context")));
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->Prepare(Alt), TEXT("Prepare IsNull")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->SetArgAddress(0, nullptr), TEXT("pass a null handle")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("IsNull should execute")));
		ASSERT_THAT(IsTrue(Context->GetReturnByte() != 0, TEXT("null handle must compare true")));
	}

	TEST_METHOD(CodeGenEmitsFuncdefCallAndLambda)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterFuncdef("int Callback(int)") >= 0,
			TEXT("funcdef/lambda test should register Callback")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "FnPtr");
		ASSERT_THAT(IsNotNull(Module, TEXT("funcdef/lambda test needs a module")));
		ASSERT_THAT(AreEqual(0, GenerateCanonicalFromSource(
			ScriptEngine,
			Module,
			"FnPtr",
			"int Double(int X) { return X + X; } int Invoke(Callback Cb, int X) { return Cb(X); } int F() { Callback L = function(int X) { return X + 1; }; return Invoke(Double, 3) + L(4); }"),
			TEXT("funcdef, call-ptr, and lambda should lower")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value),
			TEXT("funcdef/lambda function should execute")));
		ASSERT_THAT(AreEqual(11, Value, TEXT("Invoke(Double,3)+L(4) must be 6+5")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenDumpsFuncdefHandleConversionAndOpcodes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterFuncdef("int Callback(int)") >= 0,
			TEXT("conversion dump needs Callback")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "FnPtrDump");
		ASSERT_THAT(IsNotNull(Module, TEXT("conversion dump needs a module")));
		ASSERT_THAT(AreEqual(0, GenerateCanonicalFromSource(
			ScriptEngine,
			Module,
			"FnPtrDump",
			"int Double(int X) { return X + X; } int Invoke(Callback Cb, int X) { return Cb(X); } int F() { return Invoke(Double, 3); }",
			TestRunner),
			TEXT("funcdef Conversion to Callback should lower")));

		asCString Dump;
		asCBytecodeCodeGenDumpModule(Module, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("FuncPtr")), TEXT("dump must show FuncPtr of Double")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("REFCPY")), TEXT("dump must show REFCPY of $func")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("$func")) || Text.Contains(TEXT("Callback")),
			TEXT("dump must name $func or Callback on handle ownership opcodes")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenFuncdefHandleTeardownSurvivesEngineDestroy)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterFuncdef("int Callback(int)") >= 0,
			TEXT("funcdef-handle teardown needs Callback")));
		ASSERT_THAT(IsTrue(GenerateExecuteDiscardInt(
			*TestRunner,
			ScriptEngine,
			"FnPtrHandle",
			"int Double(int X) { return X + X; } int Invoke(Callback Cb, int X) { return Cb(X); } int F() { return Invoke(Double, 3); }",
			"int F()",
			6),
			TEXT("funcdef handle + indirect call must execute and discard without teardown AV")));
	}

	TEST_METHOD(CodeGenLambdaTeardownSurvivesEngineDestroy)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterFuncdef("int Callback(int)") >= 0,
			TEXT("lambda teardown needs Callback")));
		ASSERT_THAT(IsTrue(GenerateExecuteDiscardInt(
			*TestRunner,
			ScriptEngine,
			"FnPtrLambda",
			"int F() { Callback L = function(int X) { return X + 1; }; return L(4); }",
			"int F()",
			5),
			TEXT("lambda stored in a funcdef handle must execute and discard without teardown AV")));
	}

	TEST_METHOD(CodeGenFuncPtrStoreTeardownSurvivesEngineDestroy)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterFuncdef("int Callback(int)") >= 0,
			TEXT("funcptr-store teardown needs Callback")));
		ASSERT_THAT(IsTrue(GenerateExecuteDiscardInt(
			*TestRunner,
			ScriptEngine,
			"FnPtrStore",
			"int Double(int X) { return X + X; } int F() { Callback L = Double; return L(3); }",
			"int F()",
			6),
			TEXT("storing a function in a funcdef local must execute and discard without teardown AV")));
	}

	TEST_METHOD(CodeGenIndirectCallTeardownSurvivesEngineDestroy)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterFuncdef("int Callback(int)") >= 0,
			TEXT("indirect-call teardown needs Callback")));
		ASSERT_THAT(IsTrue(GenerateExecuteDiscardInt(
			*TestRunner,
			ScriptEngine,
			"FnPtrCall",
			"int Double(int X) { return X + X; } int F(Callback Cb) { return Cb(3); } int Entry() { return F(Double); }",
			"int Entry()",
			6),
			TEXT("CallPtr through a funcdef parameter must execute and discard without teardown AV")));
	}

	TEST_METHOD(CodeGenEmitsLoopSuspendAndIndex)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "Susp");
		ASSERT_THAT(IsNotNull(Module, TEXT("suspend/index test needs a module")));
		ASSERT_THAT(AreEqual(0, GenerateCanonicalFromSource(
			ScriptEngine,
			Module,
			"Susp",
			"int Count(int N) { int Y = 0; while (Y < N) { Y = Y + 1; } return Y; }"),
			TEXT("loop with suspend-safe points should lower")));

		asIScriptFunction* Function = Module->GetFunctionByDecl("int Count(int)");
		if (Function == nullptr && Module->GetFunctionCount() > 0)
		{
			Function = Module->GetFunctionByIndex(0);
		}
		ASSERT_THAT(IsNotNull(Function, TEXT("Count should resolve")));
		ASSERT_THAT(IsTrue(BytecodeContainsOpcode(Function, asBC_SUSPEND),
			TEXT("loop lowering must emit suspend/safe-point instructions")));

		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, Engine.Get(), Module, "int Count(int)");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Count(int) should invoke")));
		Invoker.AddArg(3);
		ASSERT_THAT(AreEqual(3, Invoker.CallAndReturn<int32>(), TEXT("Count(3) must return 3")));
	}
};

#endif
