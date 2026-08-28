#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "../../Support/AngelscriptNativeBuilderTestSupport.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptnode.h"
#include "source/as_sema.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

class FCanonicalASTTypeParserAccessor : public asCParser
{
public:
	explicit FCanonicalASTTypeParserAccessor(asCBuilder* Builder)
		: asCParser(Builder)
	{
	}

	using asCParser::BuildQualTypeSyntaxAction;
};

TEST_CLASS_WITH_FLAGS(FCanonicalASTTypeSemaTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.TypeSema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ParserTypesProduceInternedQualTypes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "TypeSema");
		ASSERT_THAT(IsNotNull(Module, TEXT("type Sema needs a module")));
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;

		auto ParseQual = [&](const char* Source, bool bReturnType) -> asCQualType
		{
			asCScriptCode Code;
			Code.SetCode("TypeSema", Source, true);
			asCASTContext Context;
			asCSema Sema(ScriptEngine, Context);
			FCanonicalASTTypeParserAccessor Parser(&Builder);
			Parser.SetSema(&Sema);
			if (Parser.ParseDataType(&Code, bReturnType, false) != 0)
			{
				return asCQualType();
			}
			asCScriptNode* Root = Parser.GetScriptNode();
			asCScriptNode* TypeNode = Root ? Root->firstChild : nullptr;
			asCScriptNode* TypeMod = TypeNode ? TypeNode->next : nullptr;
			return Sema.ActOnQualTypeAction(
				Parser.BuildQualTypeSyntaxAction(TypeNode, TypeMod));
		};

		const asCQualType ConstInt = ParseQual("const int", false);
		ASSERT_THAT(IsTrue(ConstInt.IsValid() && ConstInt.IsConst(), TEXT("const int should intern a const QualType")));

		const asCQualType RefInt = ParseQual("int&", true);
		ASSERT_THAT(IsTrue(RefInt.IsValid() && RefInt.IsReference(), TEXT("int& should intern a reference QualType")));

		asCScriptCode ParamCode;
		ParamCode.SetCode("TypeSemaParam", "void F(int& inout Arg) { }", true);
		asCASTContext ParamContext;
		asCSema ParamSema(ScriptEngine, ParamContext);
		asCParser ParamParser(&Builder);
		ParamParser.SetSema(&ParamSema);
		ASSERT_THAT(AreEqual(0, ParamParser.ParseScript(&ParamCode), TEXT("inout param source should parse")));
		ASSERT_THAT(AreEqual(0, ParamContext.Seal(), TEXT("inout param graph should verify")));
		asCString Dump;
		asCASTDump(ParamContext, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Param name=Arg")), TEXT("inout parameter should be a Param decl")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("type=int")), TEXT("parameter type key should be interned int")));
	}
};

#endif
