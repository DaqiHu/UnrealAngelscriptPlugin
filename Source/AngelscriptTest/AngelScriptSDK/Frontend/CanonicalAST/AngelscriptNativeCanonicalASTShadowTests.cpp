#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_builder.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_sema.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalASTShadowTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.Shadow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ShadowMismatchReportsOwnerTypeTraitSourceAndDependencyWithoutMerging)
	{
		auto Build = [](asCASTContext& Context)
		{
			asCSema Sema(nullptr, Context);
			const asASTDeclId Tu = Sema.ActOnTranslationUnit("Shadow");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
			Sema.ActOnParamDecl(Fn, "A", IntType, asCSourceRange());
			Sema.RecordDependency(Fn, "Other");
			return Fn;
		};

		asCASTContext Left;
		asCASTContext Right;
		Build(Left);
		const asASTDeclId RightFn = Build(Right);

		asCString Match;
		asCASTShadowDiff(Left, Right, Match);
		ASSERT_THAT(IsTrue(Match.Equals("match"), TEXT("identical Sema graphs should match")));

		const asCDecl* Perturbed = Right.GetDecl(RightFn);
		ASSERT_THAT(IsNotNull(Perturbed, TEXT("right function decl")));
		const asASTDeclId OriginalParent = Perturbed->parent;
		const asCQualType OriginalType = Perturbed->type;
		const asDWORD OriginalTraits = Perturbed->traits;
		const asUINT OriginalOffset = Perturbed->range.begin.offset;
		const asUINT OriginalDepCount = Perturbed->dependencies.GetLength();
		asCSourceRange OriginalRange = Perturbed->range;

		ASSERT_THAT(AreEqual(0, Right.SetDeclParent(RightFn, asASTDeclId()), TEXT("clear parent")));
		asCString OwnerDiff;
		asCASTShadowDiff(Left, Right, OwnerDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(OwnerDiff).Contains(TEXT(".parent ")), TEXT("parent perturbation path")));
		ASSERT_THAT(AreEqual(0, Right.SetDeclParent(RightFn, OriginalParent), TEXT("restore parent")));

		ASSERT_THAT(AreEqual(0, Right.SetDeclType(RightFn, Right.InternPrimitive(ttInt, asAST_QUAL_CONST)), TEXT("const type")));
		asCString TypeDiff;
		asCASTShadowDiff(Left, Right, TypeDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TypeDiff).Contains(TEXT(".type ")), TEXT("type perturbation path")));
		ASSERT_THAT(AreEqual(0, Right.SetDeclType(RightFn, OriginalType), TEXT("restore type")));

		ASSERT_THAT(AreEqual(0, Right.SetDeclTraits(RightFn, asAST_TRAIT_FINAL), TEXT("final trait")));
		asCString TraitDiff;
		asCASTShadowDiff(Left, Right, TraitDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TraitDiff).Contains(TEXT(".traits ")), TEXT("trait perturbation path")));
		ASSERT_THAT(AreEqual(0, Right.SetDeclTraits(RightFn, OriginalTraits), TEXT("restore traits")));

		asCSourceRange PerturbedRange = OriginalRange;
		PerturbedRange.begin.offset = 42;
		ASSERT_THAT(AreEqual(0, Right.SetDeclRange(RightFn, PerturbedRange), TEXT("range offset")));
		asCString SourceDiff;
		asCASTShadowDiff(Left, Right, SourceDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(SourceDiff).Contains(TEXT(".range ")), TEXT("source range perturbation path")));
		ASSERT_THAT(AreEqual(0, Right.SetDeclRange(RightFn, OriginalRange), TEXT("restore range")));

		ASSERT_THAT(AreEqual(0, Right.ClearDeclDependencies(RightFn), TEXT("clear deps")));
		ASSERT_THAT(AreEqual(0, Right.AddDependency(RightFn, "Changed"), TEXT("changed dep")));
		asCString DepDiff;
		asCASTShadowDiff(Left, Right, DepDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(DepDiff).Contains(TEXT(".dependencies[0] ")), TEXT("dependency perturbation path")));
		ASSERT_THAT(AreEqual(OriginalDepCount, Left.GetDecl(RightFn)->dependencies.GetLength(),
			TEXT("left facts must stay unmerged after right-side perturbation")));
		asCString LeftDump;
		asCASTDump(Left, LeftDump);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(LeftDump).Contains(TEXT("deps=Other")),
			TEXT("canonical and perturbed facts stay in separate contexts")));
	}

	TEST_METHOD(CompileSealShadowMismatchFailsClosedOnOwnerTypeTrait)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F(int a)
			{
				return a;
			}
			)AS");

		auto ParseSema = [&](asCASTContext& Context, const char* ModuleName, bool bSeal)
		{
			asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, ModuleName);
			ASSERT_THAT(IsNotNull(Module, TEXT("compile-seal shadow module")));
			asCBuilder Builder(ScriptEngine, Module);
			Builder.silent = true;
			asCScriptCode Code;
			Code.SetCode("SemaShadow.as", ScriptSource.c_str(), true);
			asCSema Sema(ScriptEngine, Context);
			asCParser Parser(&Builder);
			Parser.SetSema(&Sema);
			ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code), TEXT("identical F(int) sources must parse")));
			if (bSeal)
			{
				ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("left compile-seal graph should verify")));
			}
		};

		asCASTContext Left;
		asCASTContext Right;
		ParseSema(Left, "SemaShadowLeft", true);
		ParseSema(Right, "SemaShadowRight", false);

		asCString Match;
		asCASTShadowDiff(Left, Right, Match);
		ASSERT_THAT(IsTrue(Match.Equals("match"),
			TEXT("two compile-seal graphs of the same script must match before perturbation")));

		asASTDeclId RightFn = asASTDeclId();
		for (asUINT Index = 1; Index <= Right.GetDeclCount(); ++Index)
		{
			const asCDecl* Decl = Right.GetDecl(asASTDeclId(Index));
			if (Decl != nullptr && Decl->kind == asAST_DECL_FUNCTION && Decl->name.Equals("F"))
			{
				RightFn = Decl->id;
				break;
			}
		}
		ASSERT_THAT(IsTrue(RightFn.IsValid(), TEXT("right F decl")));
		const asCDecl* Perturbed = Right.GetDecl(RightFn);
		ASSERT_THAT(IsNotNull(Perturbed, TEXT("right function decl")));
		if (Perturbed == nullptr)
		{
			return;
		}

		const asASTDeclId OriginalParent = Perturbed->parent;
		const asCQualType OriginalType = Perturbed->type;
		const asDWORD OriginalTraits = Perturbed->traits;

		ASSERT_THAT(AreEqual(0, Right.SetDeclParent(RightFn, asASTDeclId()), TEXT("clear parent")));
		asCString OwnerDiff;
		asCASTShadowDiff(Left, Right, OwnerDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(OwnerDiff).StartsWith(TEXT("mismatch")),
			TEXT("owner perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(OwnerDiff).Contains(TEXT(".parent ")),
			TEXT("parent perturbation path")));
		ASSERT_THAT(AreEqual(0, Right.SetDeclParent(RightFn, OriginalParent), TEXT("restore parent")));

		ASSERT_THAT(AreEqual(0, Right.SetDeclType(RightFn, Right.InternPrimitive(ttInt, asAST_QUAL_CONST)), TEXT("const type")));
		asCString TypeDiff;
		asCASTShadowDiff(Left, Right, TypeDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TypeDiff).StartsWith(TEXT("mismatch")),
			TEXT("type perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TypeDiff).Contains(TEXT(".type ")),
			TEXT("type perturbation path")));
		ASSERT_THAT(AreEqual(0, Right.SetDeclType(RightFn, OriginalType), TEXT("restore type")));

		ASSERT_THAT(AreEqual(0, Right.SetDeclTraits(RightFn, asAST_TRAIT_FINAL), TEXT("final trait")));
		asCString TraitDiff;
		asCASTShadowDiff(Left, Right, TraitDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TraitDiff).StartsWith(TEXT("mismatch")),
			TEXT("trait perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TraitDiff).Contains(TEXT(".traits ")),
			TEXT("trait perturbation path")));
		ASSERT_THAT(AreEqual(0, Right.SetDeclTraits(RightFn, OriginalTraits), TEXT("restore traits")));

		asCString LeftDump;
		asCASTDump(Left, LeftDump);
		const FString LeftText = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(LeftDump);
		ASSERT_THAT(IsTrue(LeftText.Contains(TEXT("key=F(int)")),
			TEXT("left compile-seal dump must keep F(int) and not merge right perturbations")));
	}
};

#endif
