#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "as_builder.h"
#include "as_module.h"
#include "as_objecttype.h"
#include "frontend/Parser/as_parser.h"
#include "source/as_scriptcode.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "frontend/Sema/as_sema.h"
#include "as_typeinfo.h"
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

	TEST_METHOD(BuilderCanonicalDeclarationIdentityMismatchFailsClosedWithoutMerging)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("builder identity fixture must keep LEGACY publication")));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F(int a)
			{
				return a;
			}
			)AS");

		asCModule* const LegacyModule = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "BuilderIdentityLegacy");
		ASSERT_THAT(IsNotNull(LegacyModule, TEXT("legacy identity module")));
		ASSERT_THAT(IsTrue(AngelscriptBuilderTestSupport::AddBuilderSection(
			*LegacyModule, "BuilderCanonicalIdentity.as", ScriptSource.c_str()),
			TEXT("legacy identity section")));
		ASSERT_THAT(AreEqual(0, LegacyModule->Build(),
			TEXT("LEGACY Build must publish Builder function facts for F(int)")));

		asCScriptFunction* BuilderF = nullptr;
		for (asUINT Index = 0; Index < LegacyModule->scriptFunctions.GetLength(); ++Index)
		{
			asCScriptFunction* const Function = LegacyModule->scriptFunctions[Index];
			if (Function != nullptr
				&& Function->funcType == asFUNC_SCRIPT
				&& Function->name.Equals("F")
				&& Function->objectType == nullptr)
			{
				BuilderF = Function;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(BuilderF, TEXT("legacy module must own script F")));

		auto ParseCanonical = [&](asCASTContext& Context, const char* ModuleName, bool bSeal) -> asASTDeclId
		{
			asCModule* const CanonicalModule = AngelscriptBuilderTestSupport::CreateBuilderModule(
				ScriptEngine, ModuleName);
			if (CanonicalModule == nullptr)
			{
				return asASTDeclId();
			}
			asCBuilder CanonicalBuilder(ScriptEngine, CanonicalModule);
			CanonicalBuilder.silent = true;
			asCScriptCode Code;
			Code.SetCode("BuilderCanonicalIdentity.as", ScriptSource.c_str(), true);
			asCSema Sema(ScriptEngine, Context);
			asCParser Parser(&CanonicalBuilder);
			Parser.SetSema(&Sema);
			if (Parser.ParseScript(&Code) != 0)
			{
				return asASTDeclId();
			}
			if (bSeal && Context.Seal() != 0)
			{
				return asASTDeclId();
			}
			for (asUINT Index = 1; Index <= Context.GetDeclCount(); ++Index)
			{
				const asCDecl* const Decl = Context.GetDecl(asASTDeclId(Index));
				if (Decl != nullptr
					&& Decl->kind == asAST_DECL_FUNCTION
					&& Decl->name.Equals("F"))
				{
					return Decl->id;
				}
			}
			return asASTDeclId();
		};

		asCASTContext CanonicalSealed;
		const asASTDeclId SealedF = ParseCanonical(CanonicalSealed, "BuilderIdentityCanonicalSealed", true);
		ASSERT_THAT(IsTrue(SealedF.IsValid(), TEXT("sealed canonical F decl")));

		asCString Match;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalSealed, SealedF, *BuilderF, Match);
		ASSERT_THAT(IsTrue(Match.Equals("match"),
			*FString::Printf(TEXT("Builder and Canonical F(int) identity must match before perturbation. diff=%s"),
				*AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Match))));

		asCASTContext CanonicalMutable;
		const asASTDeclId MutableF = ParseCanonical(CanonicalMutable, "BuilderIdentityCanonicalMutable", false);
		ASSERT_THAT(IsTrue(MutableF.IsValid(), TEXT("mutable canonical F decl")));
		const asCDecl* const Perturbed = CanonicalMutable.GetDecl(MutableF);
		ASSERT_THAT(IsNotNull(Perturbed, TEXT("canonical F")));
		const asASTDeclId OriginalParent = Perturbed->parent;
		const asCQualType OriginalType = Perturbed->type;
		const asDWORD OriginalTraits = Perturbed->traits;
		const asCSourceRange OriginalRange = Perturbed->range;

		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclParent(MutableF, asASTDeclId()), TEXT("clear parent")));
		asCString OwnerDiff;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalMutable, MutableF, *BuilderF, OwnerDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(OwnerDiff).StartsWith(TEXT("mismatch")),
			TEXT("owner perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(OwnerDiff).Contains(TEXT(".parent")),
			TEXT("owner perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclParent(MutableF, OriginalParent), TEXT("restore parent")));

		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclType(MutableF, CanonicalMutable.InternPrimitive(ttInt, asAST_QUAL_CONST)),
			TEXT("const type")));
		asCString TypeDiff;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalMutable, MutableF, *BuilderF, TypeDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TypeDiff).StartsWith(TEXT("mismatch")),
			TEXT("type perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TypeDiff).Contains(TEXT(".type")),
			TEXT("type perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclType(MutableF, OriginalType), TEXT("restore type")));

		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclTraits(MutableF, asAST_TRAIT_FINAL), TEXT("final trait")));
		asCString TraitDiff;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalMutable, MutableF, *BuilderF, TraitDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TraitDiff).StartsWith(TEXT("mismatch")),
			TEXT("trait perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TraitDiff).Contains(TEXT(".traits")),
			TEXT("trait perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclTraits(MutableF, OriginalTraits), TEXT("restore traits")));

		asCSourceRange PerturbedRange = OriginalRange;
		PerturbedRange.begin.offset = OriginalRange.begin.offset + 42;
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclRange(MutableF, PerturbedRange), TEXT("range offset")));
		asCString SourceDiff;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalMutable, MutableF, *BuilderF, SourceDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(SourceDiff).StartsWith(TEXT("mismatch")),
			TEXT("source perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(SourceDiff).Contains(TEXT(".range")),
			TEXT("source perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclRange(MutableF, OriginalRange), TEXT("restore range")));

		ASSERT_THAT(AreEqual(0, CanonicalMutable.ClearDeclDependencies(MutableF), TEXT("clear deps")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.AddDependency(MutableF, "Changed"), TEXT("changed dep")));
		asCString DepDiff;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalMutable, MutableF, *BuilderF, DepDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(DepDiff).StartsWith(TEXT("mismatch")),
			TEXT("dependency perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(DepDiff).Contains(TEXT(".dependencies")),
			TEXT("dependency perturbation path")));
		ASSERT_THAT(IsTrue(BuilderF->name.Equals("F")
			&& BuilderF->returnType.GetTokenType() == ttInt,
			TEXT("Builder facts must stay unmerged after Canonical perturbation")));
		asCString LeftDump;
		asCASTDump(CanonicalSealed, LeftDump);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(LeftDump).Contains(TEXT("key=F(int)")),
			TEXT("canonical dump must keep F(int) and not absorb Builder facts")));
	}

	TEST_METHOD(BuilderCanonicalClassMethodIdentityMismatchFailsClosedWithoutMerging)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("class/method identity fixture must keep LEGACY publication")));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int M(int a)
				{
					return a;
				}
			}
			)AS");

		asCModule* const LegacyModule = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "BuilderClassIdentityLegacy");
		ASSERT_THAT(IsNotNull(LegacyModule, TEXT("legacy class identity module")));
		ASSERT_THAT(IsTrue(AngelscriptBuilderTestSupport::AddBuilderSection(
			*LegacyModule, "BuilderCanonicalClassIdentity.as", ScriptSource.c_str()),
			TEXT("legacy class identity section")));
		ASSERT_THAT(AreEqual(0, LegacyModule->Build(),
			TEXT("LEGACY Build must publish Builder class T and method M")));

		asCObjectType* BuilderClass = nullptr;
		for (asUINT Index = 0; Index < LegacyModule->classTypes.GetLength(); ++Index)
		{
			asCObjectType* const Type = LegacyModule->classTypes[Index];
			if (Type != nullptr && Type->name.Equals("T") && Type->module == LegacyModule)
			{
				BuilderClass = Type;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(BuilderClass, TEXT("legacy module must own script class T")));

		asCScriptFunction* BuilderM = nullptr;
		for (asUINT Index = 0; Index < LegacyModule->scriptFunctions.GetLength(); ++Index)
		{
			asCScriptFunction* const Function = LegacyModule->scriptFunctions[Index];
			if (Function != nullptr
				&& Function->funcType == asFUNC_SCRIPT
				&& Function->name.Equals("M")
				&& Function->objectType == BuilderClass)
			{
				BuilderM = Function;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(BuilderM, TEXT("legacy class T must own script method M")));

		auto ParseCanonical = [&](asCASTContext& Context, const char* ModuleName, bool bSeal,
			asASTDeclId& OutClass, asASTDeclId& OutMethod) -> bool
		{
			OutClass = asASTDeclId();
			OutMethod = asASTDeclId();
			asCModule* const CanonicalModule = AngelscriptBuilderTestSupport::CreateBuilderModule(
				ScriptEngine, ModuleName);
			if (CanonicalModule == nullptr)
			{
				return false;
			}
			asCBuilder CanonicalBuilder(ScriptEngine, CanonicalModule);
			CanonicalBuilder.silent = true;
			asCScriptCode Code;
			Code.SetCode("BuilderCanonicalClassIdentity.as", ScriptSource.c_str(), true);
			asCSema Sema(ScriptEngine, Context);
			asCParser Parser(&CanonicalBuilder);
			Parser.SetSema(&Sema);
			if (Parser.ParseScript(&Code) != 0)
			{
				return false;
			}
			if (bSeal && Context.Seal() != 0)
			{
				return false;
			}
			for (asUINT Index = 1; Index <= Context.GetDeclCount(); ++Index)
			{
				const asCDecl* const Decl = Context.GetDecl(asASTDeclId(Index));
				if (Decl == nullptr)
				{
					continue;
				}
				if (Decl->kind == asAST_DECL_CLASS && Decl->name.Equals("T"))
				{
					OutClass = Decl->id;
				}
				if ((Decl->kind == asAST_DECL_METHOD || Decl->kind == asAST_DECL_FUNCTION)
					&& Decl->name.Equals("M"))
				{
					OutMethod = Decl->id;
				}
			}
			return OutClass.IsValid() && OutMethod.IsValid();
		};

		asCASTContext CanonicalSealed;
		asASTDeclId SealedClass;
		asASTDeclId SealedMethod;
		ASSERT_THAT(IsTrue(ParseCanonical(CanonicalSealed, "BuilderClassIdentityCanonicalSealed", true,
			SealedClass, SealedMethod),
			TEXT("sealed canonical class T and method M")));

		asCString ClassMatch;
		asCASTShadowDiffBuilderTypeIdentity(CanonicalSealed, SealedClass, *BuilderClass, ClassMatch);
		ASSERT_THAT(IsTrue(ClassMatch.Equals("match"),
			*FString::Printf(TEXT("Builder and Canonical class T identity must match before perturbation. diff=%s"),
				*AngelscriptNativeTestSupport::CanonicalAstDumpToFString(ClassMatch))));

		asCString MethodMatch;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalSealed, SealedMethod, *BuilderM, MethodMatch);
		ASSERT_THAT(IsTrue(MethodMatch.Equals("match"),
			*FString::Printf(TEXT("Builder and Canonical method T::M identity must match before perturbation. diff=%s"),
				*AngelscriptNativeTestSupport::CanonicalAstDumpToFString(MethodMatch))));

		asCASTContext CanonicalMutable;
		asASTDeclId MutableClass;
		asASTDeclId MutableMethod;
		ASSERT_THAT(IsTrue(ParseCanonical(CanonicalMutable, "BuilderClassIdentityCanonicalMutable", false,
			MutableClass, MutableMethod),
			TEXT("mutable canonical class T and method M")));

		const asCDecl* const ClassDecl = CanonicalMutable.GetDecl(MutableClass);
		ASSERT_THAT(IsNotNull(ClassDecl, TEXT("canonical class T")));
		const asASTDeclId OriginalClassParent = ClassDecl->parent;
		const asCQualType OriginalClassType = ClassDecl->type;
		const asDWORD OriginalClassTraits = ClassDecl->traits;
		const asCSourceRange OriginalClassRange = ClassDecl->range;

		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclParent(MutableClass, asASTDeclId()), TEXT("clear class parent")));
		asCString ClassOwnerDiff;
		asCASTShadowDiffBuilderTypeIdentity(CanonicalMutable, MutableClass, *BuilderClass, ClassOwnerDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(ClassOwnerDiff).StartsWith(TEXT("mismatch")),
			TEXT("class owner perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(ClassOwnerDiff).Contains(TEXT(".parent")),
			TEXT("class owner perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclParent(MutableClass, OriginalClassParent), TEXT("restore class parent")));

		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclType(MutableClass,
			CanonicalMutable.InternNamedType(asAST_TYPE_VALUE_OBJECT, "T", 0)),
			TEXT("class value type")));
		asCString ClassTypeDiff;
		asCASTShadowDiffBuilderTypeIdentity(CanonicalMutable, MutableClass, *BuilderClass, ClassTypeDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(ClassTypeDiff).StartsWith(TEXT("mismatch")),
			TEXT("class type perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(ClassTypeDiff).Contains(TEXT(".type")),
			TEXT("class type perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclType(MutableClass, OriginalClassType), TEXT("restore class type")));

		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclTraits(MutableClass, OriginalClassTraits | asAST_TRAIT_FINAL),
			TEXT("class final trait")));
		asCString ClassTraitDiff;
		asCASTShadowDiffBuilderTypeIdentity(CanonicalMutable, MutableClass, *BuilderClass, ClassTraitDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(ClassTraitDiff).StartsWith(TEXT("mismatch")),
			TEXT("class trait perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(ClassTraitDiff).Contains(TEXT(".traits")),
			TEXT("class trait perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclTraits(MutableClass, OriginalClassTraits), TEXT("restore class traits")));

		asCSourceRange PerturbedClassRange = OriginalClassRange;
		PerturbedClassRange.begin.offset = OriginalClassRange.begin.offset + 42;
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclRange(MutableClass, PerturbedClassRange), TEXT("class range")));
		asCString ClassSourceDiff;
		asCASTShadowDiffBuilderTypeIdentity(CanonicalMutable, MutableClass, *BuilderClass, ClassSourceDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(ClassSourceDiff).StartsWith(TEXT("mismatch")),
			TEXT("class source perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(ClassSourceDiff).Contains(TEXT(".range")),
			TEXT("class source perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclRange(MutableClass, OriginalClassRange), TEXT("restore class range")));

		ASSERT_THAT(AreEqual(0, CanonicalMutable.ClearDeclDependencies(MutableClass), TEXT("clear class deps")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.AddDependency(MutableClass, "Changed"), TEXT("changed class dep")));
		asCString ClassDepDiff;
		asCASTShadowDiffBuilderTypeIdentity(CanonicalMutable, MutableClass, *BuilderClass, ClassDepDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(ClassDepDiff).StartsWith(TEXT("mismatch")),
			TEXT("class dependency perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(ClassDepDiff).Contains(TEXT(".dependencies")),
			TEXT("class dependency perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.ClearDeclDependencies(MutableClass), TEXT("restore empty class deps")));

		const asCDecl* const MethodDecl = CanonicalMutable.GetDecl(MutableMethod);
		ASSERT_THAT(IsNotNull(MethodDecl, TEXT("canonical method M")));
		const asASTDeclId OriginalMethodParent = MethodDecl->parent;
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclParent(MutableMethod, asASTDeclId()), TEXT("clear method parent")));
		asCString MethodOwnerDiff;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalMutable, MutableMethod, *BuilderM, MethodOwnerDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(MethodOwnerDiff).StartsWith(TEXT("mismatch")),
			TEXT("method owner perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(MethodOwnerDiff).Contains(TEXT(".parent")),
			TEXT("method owner perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclParent(MutableMethod, OriginalMethodParent), TEXT("restore method parent")));

		ASSERT_THAT(IsTrue(BuilderClass->name.Equals("T")
			&& BuilderM->name.Equals("M")
			&& BuilderM->objectType == BuilderClass,
			TEXT("Builder class/method facts must stay unmerged after Canonical perturbation")));
		asCString LeftDump;
		asCASTDump(CanonicalSealed, LeftDump);
		const FString LeftText = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(LeftDump);
		ASSERT_THAT(IsTrue(LeftText.Contains(TEXT("kind=Class name=T"))
			&& (LeftText.Contains(TEXT("name=M"))),
			TEXT("canonical dump must keep class T and method M and not absorb Builder facts")));
	}

	TEST_METHOD(BuilderCanonicalEnumIdentityMismatchFailsClosedWithoutMerging)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("enum identity fixture must keep LEGACY publication")));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			enum E
			{
				A
			}
			)AS");

		asCModule* const LegacyModule = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "BuilderEnumIdentityLegacy");
		ASSERT_THAT(IsNotNull(LegacyModule, TEXT("legacy enum identity module")));
		ASSERT_THAT(IsTrue(AngelscriptBuilderTestSupport::AddBuilderSection(
			*LegacyModule, "BuilderCanonicalEnumIdentity.as", ScriptSource.c_str()),
			TEXT("legacy enum identity section")));
		ASSERT_THAT(AreEqual(0, LegacyModule->Build(),
			TEXT("LEGACY Build must publish Builder enum E")));

		asCEnumType* BuilderEnum = nullptr;
		for (asUINT Index = 0; Index < LegacyModule->enumTypes.GetLength(); ++Index)
		{
			asCEnumType* const Type = LegacyModule->enumTypes[Index];
			if (Type != nullptr && Type->name.Equals("E") && Type->module == LegacyModule)
			{
				BuilderEnum = Type;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(BuilderEnum, TEXT("legacy module must own script enum E")));

		auto ParseCanonical = [&](asCASTContext& Context, const char* ModuleName, bool bSeal) -> asASTDeclId
		{
			asCModule* const CanonicalModule = AngelscriptBuilderTestSupport::CreateBuilderModule(
				ScriptEngine, ModuleName);
			if (CanonicalModule == nullptr)
			{
				return asASTDeclId();
			}
			asCBuilder CanonicalBuilder(ScriptEngine, CanonicalModule);
			CanonicalBuilder.silent = true;
			asCScriptCode Code;
			Code.SetCode("BuilderCanonicalEnumIdentity.as", ScriptSource.c_str(), true);
			asCSema Sema(ScriptEngine, Context);
			asCParser Parser(&CanonicalBuilder);
			Parser.SetSema(&Sema);
			if (Parser.ParseScript(&Code) != 0)
			{
				return asASTDeclId();
			}
			if (bSeal && Context.Seal() != 0)
			{
				return asASTDeclId();
			}
			for (asUINT Index = 1; Index <= Context.GetDeclCount(); ++Index)
			{
				const asCDecl* const Decl = Context.GetDecl(asASTDeclId(Index));
				if (Decl != nullptr
					&& Decl->kind == asAST_DECL_ENUM
					&& Decl->name.Equals("E"))
				{
					return Decl->id;
				}
			}
			return asASTDeclId();
		};

		asCASTContext CanonicalSealed;
		const asASTDeclId SealedEnum = ParseCanonical(CanonicalSealed, "BuilderEnumIdentityCanonicalSealed", true);
		ASSERT_THAT(IsTrue(SealedEnum.IsValid(), TEXT("sealed canonical enum E")));

		asCString Match;
		asCASTShadowDiffBuilderTypeIdentity(CanonicalSealed, SealedEnum, *BuilderEnum, Match);
		ASSERT_THAT(IsTrue(Match.Equals("match"),
			*FString::Printf(TEXT("Builder and Canonical enum E identity must match before perturbation. diff=%s"),
				*AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Match))));

		asCASTContext CanonicalMutable;
		const asASTDeclId MutableEnum = ParseCanonical(CanonicalMutable, "BuilderEnumIdentityCanonicalMutable", false);
		ASSERT_THAT(IsTrue(MutableEnum.IsValid(), TEXT("mutable canonical enum E")));
		const asCDecl* const Perturbed = CanonicalMutable.GetDecl(MutableEnum);
		ASSERT_THAT(IsNotNull(Perturbed, TEXT("canonical enum E")));
		const asASTDeclId OriginalParent = Perturbed->parent;
		const asCQualType OriginalType = Perturbed->type;
		const asDWORD OriginalTraits = Perturbed->traits;
		const asCSourceRange OriginalRange = Perturbed->range;

		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclParent(MutableEnum, asASTDeclId()), TEXT("clear enum parent")));
		asCString OwnerDiff;
		asCASTShadowDiffBuilderTypeIdentity(CanonicalMutable, MutableEnum, *BuilderEnum, OwnerDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(OwnerDiff).StartsWith(TEXT("mismatch")),
			TEXT("enum owner perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(OwnerDiff).Contains(TEXT(".parent")),
			TEXT("enum owner perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclParent(MutableEnum, OriginalParent), TEXT("restore enum parent")));

		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclType(MutableEnum,
			CanonicalMutable.InternNamedType(asAST_TYPE_VALUE_OBJECT, "E", 0)),
			TEXT("enum value type")));
		asCString TypeDiff;
		asCASTShadowDiffBuilderTypeIdentity(CanonicalMutable, MutableEnum, *BuilderEnum, TypeDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TypeDiff).StartsWith(TEXT("mismatch")),
			TEXT("enum type perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TypeDiff).Contains(TEXT(".type")),
			TEXT("enum type perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclType(MutableEnum, OriginalType), TEXT("restore enum type")));

		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclTraits(MutableEnum, OriginalTraits | asAST_TRAIT_SHARED),
			TEXT("enum shared trait")));
		asCString TraitDiff;
		asCASTShadowDiffBuilderTypeIdentity(CanonicalMutable, MutableEnum, *BuilderEnum, TraitDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TraitDiff).StartsWith(TEXT("mismatch")),
			TEXT("enum trait perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TraitDiff).Contains(TEXT(".traits")),
			TEXT("enum trait perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclTraits(MutableEnum, OriginalTraits), TEXT("restore enum traits")));

		asCSourceRange PerturbedRange = OriginalRange;
		PerturbedRange.begin.offset = OriginalRange.begin.offset + 42;
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclRange(MutableEnum, PerturbedRange), TEXT("enum range")));
		asCString SourceDiff;
		asCASTShadowDiffBuilderTypeIdentity(CanonicalMutable, MutableEnum, *BuilderEnum, SourceDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(SourceDiff).StartsWith(TEXT("mismatch")),
			TEXT("enum source perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(SourceDiff).Contains(TEXT(".range")),
			TEXT("enum source perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclRange(MutableEnum, OriginalRange), TEXT("restore enum range")));

		ASSERT_THAT(AreEqual(0, CanonicalMutable.ClearDeclDependencies(MutableEnum), TEXT("clear enum deps")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.AddDependency(MutableEnum, "Changed"), TEXT("changed enum dep")));
		asCString DepDiff;
		asCASTShadowDiffBuilderTypeIdentity(CanonicalMutable, MutableEnum, *BuilderEnum, DepDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(DepDiff).StartsWith(TEXT("mismatch")),
			TEXT("enum dependency perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(DepDiff).Contains(TEXT(".dependencies")),
			TEXT("enum dependency perturbation path")));
		ASSERT_THAT(IsTrue(BuilderEnum->name.Equals("E")
			&& (BuilderEnum->flags & asOBJ_ENUM) != 0,
			TEXT("Builder enum facts must stay unmerged after Canonical perturbation")));
		asCString LeftDump;
		asCASTDump(CanonicalSealed, LeftDump);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(LeftDump).Contains(TEXT("kind=Enum name=E")),
			TEXT("canonical dump must keep enum E and not absorb Builder facts")));
	}

	TEST_METHOD(BuilderCanonicalImportIdentityMismatchFailsClosedWithoutMerging)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("import identity fixture must keep LEGACY publication")));
		ASSERT_THAT(AreEqual(0, ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, 0),
			TEXT("explicit import shells require automaticImports off")));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			import int F(int a) from "Other";
			)AS");

		asCModule* const LegacyModule = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "BuilderImportIdentityLegacy");
		ASSERT_THAT(IsNotNull(LegacyModule, TEXT("legacy import identity module")));
		ASSERT_THAT(IsTrue(AngelscriptBuilderTestSupport::AddBuilderSection(
			*LegacyModule, "BuilderCanonicalImportIdentity.as", ScriptSource.c_str()),
			TEXT("legacy import identity section")));
		ASSERT_THAT(AreEqual(0, LegacyModule->Build(),
			TEXT("LEGACY Build must publish Builder import F from Other")));

		asCScriptFunction* BuilderImport = nullptr;
		for (asUINT Index = 0; Index < LegacyModule->bindInformations.GetLength(); ++Index)
		{
			sBindInfo* const Info = LegacyModule->bindInformations[Index];
			if (Info != nullptr
				&& Info->importedFunctionSignature != nullptr
				&& Info->importedFunctionSignature->name.Equals("F")
				&& Info->importFromModule.Equals("Other"))
			{
				BuilderImport = Info->importedFunctionSignature;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(BuilderImport, TEXT("legacy module must own imported F from Other")));

		auto ParseCanonical = [&](asCASTContext& Context, const char* ModuleName, bool bSeal) -> asASTDeclId
		{
			asCModule* const CanonicalModule = AngelscriptBuilderTestSupport::CreateBuilderModule(
				ScriptEngine, ModuleName);
			if (CanonicalModule == nullptr)
			{
				return asASTDeclId();
			}
			asCBuilder CanonicalBuilder(ScriptEngine, CanonicalModule);
			CanonicalBuilder.silent = true;
			asCScriptCode Code;
			Code.SetCode("BuilderCanonicalImportIdentity.as", ScriptSource.c_str(), true);
			asCSema Sema(ScriptEngine, Context);
			asCParser Parser(&CanonicalBuilder);
			Parser.SetSema(&Sema);
			if (Parser.ParseScript(&Code) != 0)
			{
				return asASTDeclId();
			}
			if (bSeal && Context.Seal() != 0)
			{
				return asASTDeclId();
			}
			for (asUINT Index = 1; Index <= Context.GetDeclCount(); ++Index)
			{
				const asCDecl* const Decl = Context.GetDecl(asASTDeclId(Index));
				if (Decl != nullptr
					&& Decl->kind == asAST_DECL_IMPORT
					&& Decl->name.Equals("F"))
				{
					return Decl->id;
				}
			}
			return asASTDeclId();
		};

		asCASTContext CanonicalSealed;
		const asASTDeclId SealedImport = ParseCanonical(CanonicalSealed, "BuilderImportIdentityCanonicalSealed", true);
		ASSERT_THAT(IsTrue(SealedImport.IsValid(), TEXT("sealed canonical import F")));

		asCString Match;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalSealed, SealedImport, *BuilderImport, Match);
		ASSERT_THAT(IsTrue(Match.Equals("match"),
			*FString::Printf(TEXT("Builder and Canonical import F identity must match before perturbation. diff=%s"),
				*AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Match))));

		asCASTContext CanonicalMutable;
		const asASTDeclId MutableImport = ParseCanonical(CanonicalMutable, "BuilderImportIdentityCanonicalMutable", false);
		ASSERT_THAT(IsTrue(MutableImport.IsValid(), TEXT("mutable canonical import F")));
		const asCDecl* const Perturbed = CanonicalMutable.GetDecl(MutableImport);
		ASSERT_THAT(IsNotNull(Perturbed, TEXT("canonical import F")));
		const asASTDeclId OriginalParent = Perturbed->parent;
		const asCQualType OriginalType = Perturbed->type;
		const asDWORD OriginalTraits = Perturbed->traits;
		const asCSourceRange OriginalRange = Perturbed->range;

		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclParent(MutableImport, asASTDeclId()), TEXT("clear import parent")));
		asCString OwnerDiff;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalMutable, MutableImport, *BuilderImport, OwnerDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(OwnerDiff).StartsWith(TEXT("mismatch")),
			TEXT("import owner perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(OwnerDiff).Contains(TEXT(".parent")),
			TEXT("import owner perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclParent(MutableImport, OriginalParent), TEXT("restore import parent")));

		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclType(MutableImport,
			CanonicalMutable.InternPrimitive(ttInt, asAST_QUAL_CONST)),
			TEXT("const import type")));
		asCString TypeDiff;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalMutable, MutableImport, *BuilderImport, TypeDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TypeDiff).StartsWith(TEXT("mismatch")),
			TEXT("import type perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TypeDiff).Contains(TEXT(".type")),
			TEXT("import type perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclType(MutableImport, OriginalType), TEXT("restore import type")));

		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclTraits(MutableImport, asAST_TRAIT_FINAL), TEXT("import final trait")));
		asCString TraitDiff;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalMutable, MutableImport, *BuilderImport, TraitDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TraitDiff).StartsWith(TEXT("mismatch")),
			TEXT("import trait perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TraitDiff).Contains(TEXT(".traits")),
			TEXT("import trait perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclTraits(MutableImport, OriginalTraits), TEXT("restore import traits")));

		asCSourceRange PerturbedRange = OriginalRange;
		PerturbedRange.begin.offset = OriginalRange.begin.offset + 42;
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclRange(MutableImport, PerturbedRange), TEXT("import range")));
		asCString SourceDiff;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalMutable, MutableImport, *BuilderImport, SourceDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(SourceDiff).StartsWith(TEXT("mismatch")),
			TEXT("import source perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(SourceDiff).Contains(TEXT(".range")),
			TEXT("import source perturbation path")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.SetDeclRange(MutableImport, OriginalRange), TEXT("restore import range")));

		ASSERT_THAT(AreEqual(0, CanonicalMutable.ClearDeclDependencies(MutableImport), TEXT("clear import deps")));
		ASSERT_THAT(AreEqual(0, CanonicalMutable.AddDependency(MutableImport, "Changed"), TEXT("changed import dep")));
		asCString DepDiff;
		asCASTShadowDiffBuilderFunctionIdentity(CanonicalMutable, MutableImport, *BuilderImport, DepDiff);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(DepDiff).StartsWith(TEXT("mismatch")),
			TEXT("import dependency perturbation must fail closed")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(DepDiff).Contains(TEXT(".dependencies")),
			TEXT("import dependency perturbation path")));
		ASSERT_THAT(IsTrue(BuilderImport->name.Equals("F")
			&& BuilderImport->returnType.GetTokenType() == ttInt,
			TEXT("Builder import facts must stay unmerged after Canonical perturbation")));
		asCString LeftDump;
		asCASTDump(CanonicalSealed, LeftDump);
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(LeftDump).Contains(TEXT("kind=Import name=F")),
			TEXT("canonical dump must keep import F and not absorb Builder facts")));
	}
};

#endif
