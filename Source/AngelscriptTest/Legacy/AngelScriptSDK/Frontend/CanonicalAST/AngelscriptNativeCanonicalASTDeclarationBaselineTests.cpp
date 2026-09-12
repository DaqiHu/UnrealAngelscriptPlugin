#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/AST/as_ast_context.h"
#include "frontend/Parser/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptnode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalASTDeclarationBaselineTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.Declarations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ContainsNodeType(const asCScriptNode* Node, eScriptNode ExpectedType)
	{
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			if (Current->nodeType == ExpectedType)
			{
				return true;
			}
			if (Current->firstChild != nullptr && ContainsNodeType(Current->firstChild, ExpectedType))
			{
				return true;
			}
		}
		return false;
	}

public:
	TEST_METHOD(ParserFamiliesHaveCanonicalDeclKinds)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		auto ParseOne = [&](const char* Name, const char* Source, eScriptNode Expected) -> bool
		{
			asCModule* LocalModule = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, Name);
			if (LocalModule == nullptr)
			{
				return false;
			}
			asCBuilder Builder(ScriptEngine, LocalModule);
			Builder.silent = true;
			asCScriptCode Code;
			Code.SetCode(Name, Source, true);
			asCParser Parser(&Builder);
			if (Parser.ParseScript(&Code) != 0)
			{
				return false;
			}
			asCScriptNode* Root = Parser.GetScriptNode();
			return Root != nullptr && ContainsNodeType(Root, Expected);
		};

		ASSERT_THAT(IsTrue(ParseOne("ns", "namespace Gameplay { int Value = 1; }", snNamespace), TEXT("namespace family")));
		ASSERT_THAT(IsTrue(ParseOne("cls", "class FBase { }", snClass), TEXT("class family")));
		ASSERT_THAT(IsTrue(ParseOne("en", "enum EMode { Idle, Run }", snEnum), TEXT("enum family")));
		ASSERT_THAT(IsTrue(ParseOne("imp", "import int SharedValue() from \"OtherModule\";", snImport), TEXT("import family")));
		// This fork currently rejects script-level interface/funcdef at parse
		// (ParserDeclInterfaceImpl / ParserDeclFuncdef). Canonical AST still has those Decl kinds.

		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("DeclBaseline");
		ASSERT_THAT(IsTrue(Context.CreateDecl(asAST_DECL_NAMESPACE, Tu, asCSourceRange(), "N").IsValid(), TEXT("NamespaceDecl")));
		ASSERT_THAT(IsTrue(Context.CreateDecl(asAST_DECL_CLASS, Tu, asCSourceRange(), "C").IsValid(), TEXT("ClassDecl")));
		ASSERT_THAT(IsTrue(Context.CreateDecl(asAST_DECL_INTERFACE, Tu, asCSourceRange(), "I").IsValid(), TEXT("InterfaceDecl")));
		ASSERT_THAT(IsTrue(Context.CreateDecl(asAST_DECL_ENUM, Tu, asCSourceRange(), "E").IsValid(), TEXT("EnumDecl")));
		ASSERT_THAT(IsTrue(Context.CreateDecl(asAST_DECL_FUNCDEF, Tu, asCSourceRange(), "Cb").IsValid(), TEXT("FuncDefDecl")));
		ASSERT_THAT(IsTrue(Context.CreateDecl(asAST_DECL_IMPORT, Tu, asCSourceRange(), "Imported").IsValid(), TEXT("ImportDecl")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("canonical declaration kinds should verify")));
	}
};

#endif
