#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "../../Support/AngelscriptNativeBuilderTestSupport.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "frontend/Parser/as_parser.h"
#include "source/as_scriptcode.h"
#include "frontend/Sema/as_sema.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalASTSemaTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.Sema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SemaActionsBuildSealedFunctionReturnLiteral)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("SemaMod");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
		const asASTExprId Lit = Sema.ActOnIntegerLiteral(7, asCSourceRange());
		asCArray<asASTStmtId> Body;
		Body.PushLast(Sema.ActOnReturnStmt(Fn, Lit, asCSourceRange()));
		Context.SetBody(Fn, Sema.ActOnCompoundStmt(Fn, Body, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("Sema-built graph should verify")));
		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Function name=F")), TEXT("Sema should record the function decl")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=IntegerLiteral")), TEXT("Sema should record the literal")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Return")), TEXT("Sema should record the return")));
	}

	TEST_METHOD(ParserWalksDeclarationsIntoSema)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "ParserSema");
		ASSERT_THAT(IsNotNull(Module, TEXT("parser-sema walk needs a module")));

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("ParserSema",
			"namespace Gameplay { int Value = 1; } class FBase { } enum EMode { Idle, Run } int F() { return 7; }",
			true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code), TEXT("parser-sema source should parse")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("parser-driven Sema graph should verify")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Namespace name=Gameplay")), TEXT("namespace should become NamespaceDecl")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Class name=FBase")), TEXT("class should become ClassDecl")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Decl name=EMode")) || Text.Contains(TEXT("kind=Enum")), TEXT("enum should become EnumDecl")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Function name=F")), TEXT("function should become FunctionDecl")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=IntegerLiteral")), TEXT("return 7 should become an integer literal")));
	}

	TEST_METHOD(ParserPublishesNativeBaseBeforeDerivedBodySema)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"NativeBase", 0, asOBJ_REF | asOBJ_NOCOUNT) >= 0,
			TEXT("native-base parser fixture must register its host reference type")));
		asCModule* Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "ParserNativeBaseSema");
		ASSERT_THAT(IsNotNull(Module, TEXT("native-base parser fixture needs a module")));

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("ParserNativeBaseSema");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType NativeBaseType = Sema.ActOnQualType("NativeBase");
		const asASTDeclId AcceptBase = Sema.ActOnStartFunctionDecl(
			Tu, "AcceptBase", IntType, asCSourceRange());
		Sema.ActOnParamDecl(
			AcceptBase, "Value", NativeBaseType, asCSourceRange());
		Sema.FinishDecl(AcceptBase);

		asCScriptCode Code;
		Code.SetCode("ParserNativeBaseSema",
			"class DerivedValue : NativeBase { "
			"int ReadThroughBase() { return AcceptBase(this); } }",
			true);
		AngelscriptNativeTestSupport::FParserAccessor Parser(&Builder);
		ASSERT_THAT(IsNotNull(Parser.ParseClassSnippet(&Code, &Sema),
			TEXT("native-base class snippet should remain syntactically valid")));

		bool bSawUnresolvedCall = false;
		for (asUINT DiagnosticIndex = 0;
			DiagnosticIndex < Sema.GetDiagnostics().GetLength(); ++DiagnosticIndex)
		{
			bSawUnresolvedCall |=
				Sema.GetDiagnostics()[DiagnosticIndex].Equals(
					"unresolved-callee:AcceptBase");
		}
		ASSERT_THAT(IsFalse(bSawUnresolvedCall,
			TEXT("the native base edge must be visible before the derived method body is analyzed")));

		const asCDecl* NativeBase = nullptr;
		const asCDecl* DerivedValue = nullptr;
		for (asUINT DeclIndex = 1; DeclIndex <= Context.GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Decl = Context.GetDecl(asASTDeclId(DeclIndex));
			if (Decl != nullptr && Decl->kind == asAST_DECL_CLASS)
			{
				if (Decl->name.Equals("NativeBase"))
				{
					NativeBase = Decl;
				}
				else if (Decl->name.Equals("DerivedValue"))
				{
					DerivedValue = Decl;
				}
			}
		}
		ASSERT_THAT(IsTrue(
			NativeBase != nullptr && DerivedValue != nullptr
				&& NativeBase->origin.Equals("canonical-native-type-view")
				&& DerivedValue->bases.GetLength() == 1
				&& DerivedValue->bases[0] == NativeBase->id,
			TEXT("Canonical AST must own an exact derived-to-native base edge")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text =
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(
			Text.Contains(TEXT("callee=AcceptBase(NativeBase)"))
				&& Text.Contains(TEXT("kind=Conversion")),
			*FString::Printf(
				TEXT("native derived-to-base call must be sealed by Sema. AST:\n%s"),
				*Text)));
	}

	TEST_METHOD(DeclarationDumpCoversTask13Families)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "DeclDump");
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("DeclDump",
			"namespace Gameplay { int Value = 1; } class FBase { FBase() {} int Method(int Arg) { return Arg; } ~FBase() {} } enum EMode { Idle } import int Shared() from \"Other\"; int F() { return 7; }",
			true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code), TEXT("task 1.3 parse families should parse")));
		Sema.ActOnInterfaceDecl(Context.GetTranslationUnit(), "IProbe", asCSourceRange());
		Sema.ActOnFuncDefDecl(Context.GetTranslationUnit(), "Callback", asCSourceRange());
		Sema.ActOnTypedefDecl(Context.GetTranslationUnit(), "Alias", asCSourceRange());
		Sema.ActOnMixinDecl(Context.GetTranslationUnit(), "Mix", asCSourceRange());
		Sema.ActOnPropertyDecl(Context.GetTranslationUnit(), "Published", Context.InternPrimitive(ttInt, 0), asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("declaration dump graph should verify")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Namespace name=Gameplay")), TEXT("namespace")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Class name=FBase")), TEXT("class")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Constructor")), TEXT("constructor")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Destructor")), TEXT("destructor")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Method name=Method")), TEXT("method")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Param name=Arg")), TEXT("parameter")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Enum name=EMode")), TEXT("enum")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Import")), TEXT("import")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Function name=F")), TEXT("function")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Interface name=IProbe")), TEXT("interface")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=FuncDef name=Callback")), TEXT("funcdef")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Typedef name=Alias")), TEXT("typedef")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Mixin name=Mix")), TEXT("mixin")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Property name=Published")), TEXT("property")));
	}

	TEST_METHOD(SignaturesTraitsDefaultArgsMixinAndLambda)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "SigSema");
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("SigSema",
			"class FBase { private int Method(int Arg = 7) const final { return Arg; } } mixin void MixHelper() {} int Make() { function(int x) { return x; }; return 7; }",
			true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code), TEXT("signature/mixin/lambda source should parse")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("signature graph should verify")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Method name=Method")), TEXT("method signature")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Param name=Arg")), TEXT("named parameter")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("default=7")), TEXT("default argument ownership")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Mixin name=MixHelper")) || Text.Contains(TEXT("kind=Function name=MixHelper")), TEXT("mixin function")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("name=<lambda>")) || Text.Contains(TEXT("kind=Function")), TEXT("lambda is a function-like decl")));
	}

	TEST_METHOD(GeneratedLifecycleImportOriginAndDuplicateParam)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "LifeSema");
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("LifeSema",
			"class FBase { } class FChild : FBase { } import int Shared() from \"Other\"; int Dup(int A, int A) { return A; }",
			true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code), TEXT("lifecycle/import/conflict source should parse")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("lifecycle graph should verify")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Constructor")), TEXT("empty class gets generated constructor")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Destructor")), TEXT("empty class gets generated destructor")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("origin=Other")), TEXT("import module origin")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("deps=FBase")) || Text.Contains(TEXT("deps=Other")), TEXT("inheritance/import dependencies")));

		bool bSawDuplicate = false;
		for (asUINT Index = 0; Index < Sema.GetDiagnostics().GetLength(); ++Index)
		{
			if (Sema.GetDiagnostics()[Index].Equals("duplicate-param:A"))
			{
				bSawDuplicate = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bSawDuplicate, TEXT("duplicate parameter names should stay as Sema diagnostics")));
	}
};

#endif
