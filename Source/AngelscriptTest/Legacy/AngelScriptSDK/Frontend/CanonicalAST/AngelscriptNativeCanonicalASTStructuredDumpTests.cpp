#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_context.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_traversal.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
struct FStructuredDumpFixture
{
	asASTDeclId Tu;
	asASTDeclId Helper;
	asASTDeclId Function;
	asASTStmtId Body;
	asASTStmtId Return;
	asASTExprId Call;
};

FStructuredDumpFixture BuildStructuredDumpFixture(asCASTContext& Context)
{
	FStructuredDumpFixture Fixture;
	Fixture.Tu = Context.CreateTranslationUnit("Dump\"Module\\Name");
	const char* HelperSource = "int Helper() { return 1; }\n";
	const char* MainSource = "int F() { return Helper(); }\n";
	const asASTFileID HelperFile = Context.AddSourceSection(
		"Scripts/Helper.as", asAST_SOURCE_AUTHORED, HelperSource, FCStringAnsi::Strlen(HelperSource), 0);
	const asASTFileID MainFile = Context.AddSourceSection(
		"Scripts/Main\"Quoted.as", asAST_SOURCE_AUTHORED, MainSource, FCStringAnsi::Strlen(MainSource), 8);
	const asCQualType IntType = Context.InternPrimitive(ttInt, 0);

	Fixture.Helper = Context.CreateDecl(
		asAST_DECL_FUNCTION,
		Fixture.Tu,
		Context.GetSourceManager().MakeRange(HelperFile, 0, 12),
		"Helper");
	Context.SetStableKey(Fixture.Helper, "Dump::Helper\\Stable");
	Context.SetDeclType(Fixture.Helper, IntType);

	Fixture.Function = Context.CreateDecl(
		asAST_DECL_FUNCTION,
		Fixture.Tu,
		Context.GetSourceManager().MakeRange(MainFile, 0, FCStringAnsi::Strlen(MainSource)),
		"F\"Quoted");
	Context.SetStableKey(Fixture.Function, "Dump::F\"Stable");
	Context.SetDeclType(Fixture.Function, IntType);
	Context.AddDeclTrait(Fixture.Function, asAST_TRAIT_GENERATED);
	Fixture.Body = Context.CreateStmt(
		asAST_STMT_BLOCK,
		Fixture.Function,
		Context.GetSourceManager().MakeRange(MainFile, 8, 28));
	Context.SetBody(Fixture.Function, Fixture.Body);
	Fixture.Return = Context.CreateStmt(
		asAST_STMT_RETURN,
		Fixture.Function,
		Context.GetSourceManager().MakeRange(MainFile, 10, 26));
	Context.AddStmtChild(Fixture.Body, Fixture.Return);
	Fixture.Call = Context.CreateExpr(
		asAST_EXPR_CALL,
		IntType,
		asAST_VALUE_PRVALUE,
		Context.GetSourceManager().MakeRange(MainFile, 17, 25));
	Context.SetResolvedDecl(Fixture.Call, Fixture.Helper);
	Context.SetStmtExpr(Fixture.Return, Fixture.Call);
	Context.Seal();
	return Fixture;
}

FString StructuredDump(
	const asCASTContext& Context,
	const asSASTStructuredDumpOptions& Options)
{
	asCString Dump;
	const int Status = asCASTStructuredDump(Context, Options, Dump);
	if (Status != asAST_TRAVERSAL_OK)
	{
		return FString::Printf(TEXT("<status:%d>"), Status);
	}
	return AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTStructuredDumpTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.StructuredDump",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TreeAndJsonAreDeterministicAddressFreeAndCarrySemanticFacts)
	{
		asCASTContext First;
		asCASTContext Second;
		BuildStructuredDumpFixture(First);
		BuildStructuredDumpFixture(Second);
		ASSERT_THAT(IsTrue(First.IsPublishable() && Second.IsPublishable(), TEXT("structured dump fixtures must be publishable")));

		asSASTStructuredDumpOptions TreeOptions;
		TreeOptions.format = asAST_STRUCTURED_DUMP_TREE;
		TreeOptions.includeSourceSnippets = true;
		const FString FirstTree = StructuredDump(First, TreeOptions);
		const FString SecondTree = StructuredDump(Second, TreeOptions);
		ASSERT_THAT(AreEqual(FirstTree, SecondTree, TEXT("identical ASTs need byte-identical tree dumps")));
		ASSERT_THAT(IsTrue(FirstTree.StartsWith(TEXT("AST_TREE diagnosticOnly=true cacheInput=false")),
			TEXT("tree output must declare its diagnostic-only non-Cache contract")));
		ASSERT_THAT(IsTrue(FirstTree.Contains(TEXT("role=expr-resolved-decl")), TEXT("tree should name semantic reference edges")));
		ASSERT_THAT(IsTrue(FirstTree.Contains(TEXT("targetKey=\"Dump::Helper\\\\Stable\"")), TEXT("resolved target needs stable identity")));
		ASSERT_THAT(IsTrue(FirstTree.Contains(TEXT("type=\"int\" typeKind=Primitive quals=0")), TEXT("tree should carry exact type facts")));
		ASSERT_THAT(IsTrue(FirstTree.Contains(TEXT("traits=")), TEXT("decl traits must be explicit")));
		ASSERT_THAT(IsTrue(FirstTree.Contains(TEXT("source=\"Scripts/Main\\\"Quoted.as\" line=9 column=1")),
			TEXT("logical source and line/column should be printed and escaped")));
		ASSERT_THAT(IsTrue(FirstTree.Contains(TEXT("snippet=\"Helper()\"")), TEXT("requested source snippets should use the node range")));
		ASSERT_THAT(IsTrue(!FirstTree.Contains(TEXT("0x")) && !FirstTree.Contains(TEXT("EngineLocal")),
			TEXT("diagnostic tree must contain neither pointer spelling nor Engine-local IDs")));

		asSASTStructuredDumpOptions JsonOptions;
		JsonOptions.format = asAST_STRUCTURED_DUMP_JSON;
		JsonOptions.includeSourceSnippets = true;
		const FString FirstJson = StructuredDump(First, JsonOptions);
		const FString SecondJson = StructuredDump(Second, JsonOptions);
		ASSERT_THAT(AreEqual(FirstJson, SecondJson, TEXT("identical ASTs need byte-identical JSON dumps")));
		TSharedPtr<FJsonObject> Parsed;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FirstJson);
		ASSERT_THAT(IsTrue(FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid(), TEXT("structured JSON must parse")));
		ASSERT_THAT(IsTrue(Parsed->GetBoolField(TEXT("diagnosticOnly")), TEXT("JSON must declare diagnosticOnly=true")));
		ASSERT_THAT(IsTrue(!Parsed->GetBoolField(TEXT("cacheInput")), TEXT("JSON must declare cacheInput=false")));
		ASSERT_THAT(IsTrue(FirstJson.Contains(TEXT("Dump\\\"Module\\\\Name")), TEXT("JSON must escape quotes and backslashes")));
		ASSERT_THAT(IsTrue(!FirstJson.Contains(TEXT("0x")) && !FirstJson.Contains(TEXT("EngineLocal")),
			TEXT("diagnostic JSON must contain neither pointer spelling nor Engine-local IDs")));
	}

	TEST_METHOD(StableKeyNodeKindIdModuleAndLogicalSourceFiltersKeepStructuralClosure)
	{
		asCASTContext Context;
		const FStructuredDumpFixture Fixture = BuildStructuredDumpFixture(Context);

		asSASTStructuredDumpOptions Options;
		Options.format = asAST_STRUCTURED_DUMP_TREE;
		Options.filter.stableDeclKey = "Dump::F\"Stable";
		FString Text = StructuredDump(Context, Options);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("node=D1")) && Text.Contains(TEXT("node=D3")),
			TEXT("stable-key filter should retain the TU ancestor and matching function")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("node=S1")) && Text.Contains(TEXT("node=S2")) && Text.Contains(TEXT("node=E1")),
			TEXT("stable-key filter should retain the matching declaration's structural subtree")));
		ASSERT_THAT(IsTrue(!Text.Contains(TEXT("node=D2 class=Decl")), TEXT("unmatched sibling declaration should not become a dumped node")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("target=D2")) && Text.Contains(TEXT("targetKey=\"Dump::Helper\\\\Stable\"")),
			TEXT("reference target identity should remain visible without pulling its declaration subtree into the filter")));

		Options.filter = asSASTStructuredDumpFilter();
		Options.filter.nodeClass = asAST_NODE_EXPR;
		Options.filter.nodeId = Fixture.Call.value;
		Text = StructuredDump(Context, Options);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("node=D1")) && Text.Contains(TEXT("node=E1")), TEXT("node-ID filter should include root-to-node closure")));
		ASSERT_THAT(IsTrue(!Text.Contains(TEXT("node=D2 class=Decl")), TEXT("node-ID closure should exclude unrelated sibling nodes")));

		Options.filter = asSASTStructuredDumpFilter();
		Options.filter.nodeKind = "Call";
		Text = StructuredDump(Context, Options);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("node=E1 class=Expr kind=Call")), TEXT("kind filter should select canonical kind names")));

		Options.filter = asSASTStructuredDumpFilter();
		Options.filter.logicalSource = "Scripts/Main\"Quoted.as";
		Text = StructuredDump(Context, Options);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("node=D3")) && Text.Contains(TEXT("node=E1")), TEXT("logical-source filter should retain matching nodes")));
		ASSERT_THAT(IsTrue(!Text.Contains(TEXT("node=D2 class=Decl")), TEXT("logical-source filter should exclude declarations from another source")));

		Options.filter = asSASTStructuredDumpFilter();
		Options.filter.moduleName = "NoSuchModule";
		Text = StructuredDump(Context, Options);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("nodes=0")) && !Text.Contains(TEXT("node=D1")), TEXT("module mismatch should return a valid empty diagnostic dump")));
	}

	TEST_METHOD(SourceSnippetIsOptionalAndBounded)
	{
		asCASTContext Context;
		BuildStructuredDumpFixture(Context);
		asSASTStructuredDumpOptions Options;
		Options.format = asAST_STRUCTURED_DUMP_JSON;
		Options.includeSourceSnippets = false;
		FString Text = StructuredDump(Context, Options);
		ASSERT_THAT(IsTrue(!Text.Contains(TEXT("\"snippet\"")), TEXT("snippet field should be absent unless requested")));

		Options.includeSourceSnippets = true;
		Options.maxSnippetBytes = 4;
		Options.filter.nodeKind = "Call";
		Text = StructuredDump(Context, Options);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("\"snippet\":\"Help\"")), TEXT("snippet bytes should obey the explicit deterministic bound")));
	}
};

#endif
