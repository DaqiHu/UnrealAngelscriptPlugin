#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "../Preprocessor/NativePreprocessorTestSupport.h"

#include "as_builder.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_ast_context.h"
#include "frontend/Basic/as_identifier_table.h"
#include "frontend/Basic/as_source_edit.h"
#include "frontend/Lexer/as_preprocessor.h"
#include "frontend/Lexer/as_tokenizer.h"
#include "frontend/Parser/as_parser.h"
#include "frontend/Sema/as_body_fragment.h"
#include "frontend/Sema/as_sema.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DiagnosticsSyntax,
	"Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	static const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* FindGroup(
		const AS_NAMESPACE_QUALIFIER asCDiagnosticResult& Result,
		asUINT DiagnosticID)
	{
		for (const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup& Group : Result.GetGroups())
		{
			if (Group.Primary.DiagnosticID == DiagnosticID)
			{
				return &Group;
			}
		}
		return nullptr;
	}

	void LexExpect(FStringView Path, FStringView Text, asUINT DiagnosticID)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID LexFile;
		const AS_NAMESPACE_QUALIFIER asCSourceManager LexManager =
			AngelscriptNativeEngineTest::MakeSourceManager(Path, Text, &LexFile);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine LexEngine(LexManager);
		AS_NAMESPACE_QUALIFIER asCIdentifierTable LexIdentifiers;
		TConstArrayView<uint8> Bytes;
		ASSERT_THAT(IsTrue(LexManager.TryGetBytes(LexFile, Bytes)));
		AS_NAMESPACE_QUALIFIER asCTokenizer LexTokenizer(
			LexManager,
			LexManager.MakeRange(LexFile, 0, static_cast<asUINT>(Bytes.Num())),
			AS_NAMESPACE_QUALIFIER asSLexOptions(),
			LexIdentifiers,
			LexEngine);
		TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
		for (;;)
		{
			AS_NAMESPACE_QUALIFIER asCToken Token;
			if (!LexTokenizer.Lex(Token))
			{
				break;
			}
			Tokens.Add(Token);
			if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile)
			{
				break;
			}
		}
		LexTokenizer.FlushDiagnostics();
		ASSERT_THAT(IsNotNull(FindGroup(*LexEngine.CaptureResult(), DiagnosticID), FString(Path)));
	}

	void LexExpectBytes(FStringView Path, TConstArrayView<uint8> Bytes, asUINT DiagnosticID)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID LexFile;
		const AS_NAMESPACE_QUALIFIER asCSourceManager LexManager =
			AngelscriptNativeEngineTest::MakeSourceManagerFromBytes(Path, Bytes, &LexFile);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine LexEngine(LexManager);
		AS_NAMESPACE_QUALIFIER asCIdentifierTable LexIdentifiers;
		AS_NAMESPACE_QUALIFIER asCTokenizer LexTokenizer(
			LexManager,
			LexManager.MakeRange(LexFile, 0, static_cast<asUINT>(Bytes.Num())),
			AS_NAMESPACE_QUALIFIER asSLexOptions(),
			LexIdentifiers,
			LexEngine);
		for (;;)
		{
			AS_NAMESPACE_QUALIFIER asCToken Token;
			if (!LexTokenizer.Lex(Token) || Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile)
			{
				break;
			}
		}
		LexTokenizer.FlushDiagnostics();
		ASSERT_THAT(IsNotNull(FindGroup(*LexEngine.CaptureResult(), DiagnosticID), FString(Path)));
	}

	void PpExpect(FStringView Source, asUINT DiagnosticID)
	{
		const AngelscriptNativePreprocessorTest::FPreprocessorRun Run(Source, {});
		bool bFound = false;
		for (const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord& Record : Run.DiagnosticRecords)
		{
			if (Record.DiagnosticID == DiagnosticID)
			{
				bFound = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bFound, FString(Source)));
	}

	void SessionExpect(FStringView Source, asUINT DiagnosticID, bool bDeclarationsOnly)
	{
		FSessionRun Run({{TEXT("Bound.as"), FString(Source)}});
		Run.Session->CloseDeclarationBarrier();
		Run.Session->ResolveDeclarations(1);
		if (!bDeclarationsOnly)
		{
			Run.Session->AnalyzeBodies(1);
		}
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Result =
			Run.Diagnostics.CaptureResult();
		FString Seen = FString(Source) + TEXT(" -> ");
		for (const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup& Group : Result->GetGroups())
		{
			Seen += FString::FromInt(static_cast<int32>(Group.Primary.DiagnosticID));
			Seen += TEXT(",");
		}
		ASSERT_THAT(IsNotNull(FindGroup(*Result, DiagnosticID), Seen));
	}

	void ExpectBodySource(FStringView Source, asUINT DiagnosticID)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager =
			AngelscriptNativeEngineTest::MakeSourceManager(TEXT("Body.as"), Source, &FileID);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine LexEngine(Manager);
		AS_NAMESPACE_QUALIFIER asCIdentifierTable Identifiers;
		TConstArrayView<uint8> Bytes;
		ASSERT_THAT(IsTrue(Manager.TryGetBytes(FileID, Bytes), FString(Source)));
		AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(
			Manager,
			Manager.MakeRange(FileID, 0, static_cast<asUINT>(Bytes.Num())),
			AS_NAMESPACE_QUALIFIER asSLexOptions(),
			Identifiers,
			LexEngine);
		TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
		for (;;)
		{
			AS_NAMESPACE_QUALIFIER asCToken Token;
			if (!Tokenizer.Lex(Token))
			{
				break;
			}
			Tokens.Add(Token);
			if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile)
			{
				break;
			}
		}
		AS_NAMESPACE_QUALIFIER asSDirectiveProcessResult Input;
		Input.bValid = true;
		Input.Sources = Manager;
		Input.ActiveTokens = Tokens;
		ExpectParseBody(Input, DiagnosticID, Manager);
	}

	void ExpectParseBody(
		const AS_NAMESPACE_QUALIFIER asSDirectiveProcessResult& Input,
		asUINT DiagnosticID,
		const AS_NAMESPACE_QUALIFIER asCSourceManager& Manager)
	{
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Engine(Manager);
		AS_NAMESPACE_QUALIFIER asCASTContext Context(Manager);
		AS_NAMESPACE_QUALIFIER asCSema Sema(Context, Engine, TEXT("syntax"));
		AS_NAMESPACE_QUALIFIER asCBodyFragment Fragment;
		Sema.BeginBody(Fragment, {}, {});
		AS_NAMESPACE_QUALIFIER asCParser Parser(Sema);
		ASSERT_THAT(IsFalse(Parser.ParseBody(Input, Fragment)));
		Sema.EndBody();
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Result =
			Engine.CaptureResult();
		FString Seen;
		for (const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup& Group : Result->GetGroups())
		{
			if (!Seen.IsEmpty())
			{
				Seen += TEXT(",");
			}
			Seen += FString::FromInt(static_cast<int32>(Group.Primary.DiagnosticID));
		}
		ASSERT_THAT(IsNotNull(FindGroup(*Result, DiagnosticID), Seen));
	}

	TEST_METHOD(RealDelimiterRecovery)
	{
		const FString Source = TEXT("int F(){ return 1 } int G(){ return 2; }");
		FSessionRun Run({{TEXT("Recover.as"), Source}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Result =
			Run.Diagnostics.CaptureResult();
		const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* Group = FindGroup(
			*Result,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ReturnMissingSemicolon));
		ASSERT_THAT(IsNotNull(Group));
		const int32 One = Source.Find(TEXT("1"));
		ASSERT_THAT(IsTrue(One != INDEX_NONE));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(One + 1), Group->Primary.PrimaryRange.End.ByteOffset));
		ASSERT_THAT(AreEqual(1, Group->Fixes.Num()));
		ASSERT_THAT(AreEqual(1, Group->Fixes[0].SourceEdits.Num()));
		ASSERT_THAT(AreEqual(1, Group->Fixes[0].SourceEdits[0].ReplacementUtf8.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>(';'), Group->Fixes[0].SourceEdits[0].ReplacementUtf8[0]));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(One + 1),
			Group->Fixes[0].SourceEdits[0].Range.Begin.ByteOffset));

		auto* G = Run.Function(TEXT("G"));
		ASSERT_THAT(IsNotNull(G));
		ASSERT_THAT(IsNotNull(G->GetBody()));
		auto* F = Run.Function(TEXT("F"));
		ASSERT_THAT(IsNotNull(F));
		auto* FBody = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(F->GetBody());
		ASSERT_THAT(IsNotNull(FBody));
		ASSERT_THAT(IsTrue(FBody->GetStatements().Num() >= 1));
		ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCRecoveryStmt>(
			FBody->GetStatements()[0])));
	}

	TEST_METHOD(SilentAnnotationAndMalformedBytes)
	{
		FSessionRun Annotation({{TEXT("Anno.as"), TEXT("UCLASS() struct Bad {};")}});
		ASSERT_THAT(IsTrue(Annotation.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Annotation.Session->ResolveDeclarations(1)));
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> AnnotationResult =
			Annotation.Diagnostics.CaptureResult();
		ASSERT_THAT(IsNotNull(FindGroup(
			*AnnotationResult,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::InvalidAttributeTarget))));

		const uint8 Malformed[] = {0xC0, 0xAF};
		LexExpectBytes(
			TEXT("Bad.as"),
			MakeArrayView(Malformed),
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::InvalidUtf8));
	}

	TEST_METHOD(AcceptedSyntaxBoundaries)
	{
		struct FCase
		{
			const TCHAR* Source;
			asUINT DiagnosticID;
			bool bDeclarationsOnly = false;
		};
		const FCase Cases[] = {
			{TEXT("int F(){ break; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::BreakOutsideControl), false},
			{TEXT("UFUNCTION() delegate void Bad();"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::InvalidAttributeTarget), true},
			{TEXT("asset Old of Payload { Values = { 1 }; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDeclarationDiagnostic::RemovedAsset), true},
			{TEXT("import Foo from \"Bar.as\";"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDeclarationDiagnostic::RemovedImport), true},
			{TEXT("int F(){ return 1 }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ReturnMissingSemicolon), false},
			{TEXT("int F(){ int X = 1 }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::LocalDeclarationMissingSemicolon), false},
			{TEXT("int F(){ 1 }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::StatementMissingSemicolon), false},
			{TEXT("UCLASS()"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::AnnotationRequiresDeclaration), true},
			{TEXT("UCLASS struct S {};"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::AnnotationRequiresParentheses), true},
			{TEXT("UCLASS("), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnterminatedAnnotation), true},
			{TEXT("UCLASS(,Bad) class S {};"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedAnnotationPayload), true},
			{TEXT("class C { UCLASS() access Default = private; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::AccessPolicyDoesNotAcceptAnnotations), true},
			{TEXT("class C { default }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedClassDefaultStatement), true},
			{TEXT("typedef"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedTypeAlias), true},
			{TEXT("typedef int X"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::TypeAliasMissingSemicolon), true},
			{TEXT("delegate void"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDeclarationDiagnostic::InvalidCallable), true},
			{TEXT("enum E : int { A }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::EnumUnderlyingTypeNotSupported), true},
			{TEXT("class C : { }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedBaseSpecifier), true},
			{TEXT("enum E { = 1 }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::EnumConstantNameExpected), true},
			{TEXT("enum E { A = }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::EnumInitializerExpressionExpected), true},
			{TEXT("enum E { A B }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::EnumConstantSeparatorExpected), true},
			{TEXT("class"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedRecord), true},
			{TEXT("namespace"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedNamespace), true},
			{TEXT("int ("), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedDeclaration), true},
			{TEXT("access Default;"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedAccessPolicy), true},
			{TEXT("int F(){ return 1 ? 2; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ConditionalMissingColon), false},
			{TEXT("int F(){ return 1 ? 2 : ; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ConditionalMissingFalseExpression), false},
			{TEXT("int F(){ return 1 + ; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MissingBinaryOperand), false},
			{TEXT("int F(){ return - ; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MissingUnaryOperand), false},
			{TEXT("int F(){ return X. ; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MemberNameRequired), false},
			{TEXT("int F(){ return X.Name(; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedMemberCall), false},
			{TEXT("int F(){ return X[; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedPostfixArguments), false},
			{TEXT("int F(){ return X[Name:1]; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::NamedSubscriptNotSupported), false},
			{TEXT("int F(){ return G(1,); }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::CallTrailingComma), false},
			{TEXT("int F(){ return {1 2}; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ListInitializerMissingComma), false},
			{TEXT("int F(){ return {=}; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedListInitializer), false},
			{TEXT("int F(){ return Cast; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::CastTargetRequired), false},
			{TEXT("int F(){ return Cast<; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedCastTarget), false},
			{TEXT("int F(){ return Cast<int; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::CastMissingAngleClose), false},
			{TEXT("int F(){ return Cast<int>; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::CastMissingOperand), false},
			{TEXT("int F(){ return Cast<int>(1; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::CastMissingParenthesis), false},
			{TEXT("int F(){ return (1; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ParenthesizedExpressionMissingClose), false},
			{TEXT("int F(){ return int(; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedConstructionArguments), false},
			{TEXT("int F(){ return Foo::; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::QualifiedNameMissingComponent), false},
			{TEXT("int F(){ return G(; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedCallArgument), false},
			{TEXT("int F(){ int X,; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::LocalNameRequired), false},
			{TEXT("int F(){ int X = ; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MissingLocalInitializer), false},
			{TEXT("int F(){ int X(; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedDirectInitializer), false},
			{TEXT("int F(){ for X; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ForMissingOpeningParenthesis), false},
			{TEXT("int F(){ for (int X = 1 1; ; ) {} }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ForMissingInitializerSemicolon), false},
			{TEXT("int F(){ for (; true ) {} }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ForMissingConditionSemicolon), false},
			{TEXT("int F(){ for (;; I++ }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ForMissingClosingParenthesis), false},
			{TEXT("int F(){ for (;; I++,) {} }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ForTrailingIncrementComma), false},
			{TEXT("int F(){ switch 1 {} }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedSwitchSelector), false},
			{TEXT("int F(){ switch (1) }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::SwitchMissingOpeningBrace), false},
			{TEXT("int F(){ switch (1) { 1; } }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::SwitchStatementRequiresClause), false},
			{TEXT("int F(){ switch (1) { case } }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedCaseLabel), false},
			{TEXT("int F(){ switch (1) { default } }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::DefaultMissingColon), false},
			{TEXT("int F(){ switch (1) { case 1: fallthrough case 2: break; } }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::FallthroughMissingSemicolon), false},
			{TEXT("int F(){ switch (1) { case 1: int X = 1; } }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::SwitchClauseDeclarationRequiresScope), false},
			{TEXT("int F(){ foreach X; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ForeachMissingOpeningParenthesis), false},
			{TEXT("int F(){ foreach (); }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ForeachInvalidValueVariable), false},
			{TEXT("int F(){ foreach (int X, : Y) {} }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ForeachInvalidKeyVariable), false},
			{TEXT("int F(){ foreach (int X Y) {} }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ForeachMissingColon), false},
			{TEXT("int F(){ foreach (int X :) {} }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ForeachInvalidRange), false},
			{TEXT("int F(){ if () {} }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedIfCondition), false},
			{TEXT("int F(){ while () {} }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedWhileCondition), false},
			{TEXT("int F(){ do {} }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::DoMissingWhile), false},
			{TEXT("int F(){ do {} while; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedDoCondition), false},
			{TEXT("int F(){ for (;;) { break } }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::BreakMissingSemicolon), false},
			{TEXT("int F(){ for (;;) { continue } }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ContinueMissingSemicolon), false},
			{TEXT("int F(){ return }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedReturnExpression), false},
			{TEXT("int F(){ case 1: }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::SwitchLabelOutsideSwitch), false},
			{TEXT("int F(){ fallthrough; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::FallthroughOutsideSwitch), false},
			{TEXT("int F(){ ) ; }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedStatement), false},
			{TEXT("int F(){ for (=; ;) {} }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::InvalidForInitializer), false},
			{TEXT("int F(){ for (;; =) {} }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::InvalidForIncrement), false},
		};
		for (const FCase& Case : Cases)
		{
			SessionExpect(Case.Source, Case.DiagnosticID, Case.bDeclarationsOnly);
		}

		ExpectBodySource(TEXT("{ return {"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ListInitializerUnclosed));
		ExpectBodySource(TEXT("{ for (;;)"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MissingForBody));
		ExpectBodySource(TEXT("{ switch (1) {"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnterminatedSwitch));
		ExpectBodySource(TEXT("{ foreach (int X : Y)"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MissingForeachBody));
		ExpectBodySource(TEXT("{ if (true)"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MissingIfStatement));
		ExpectBodySource(TEXT("{ if (true) {} else"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MissingElseStatement));
		ExpectBodySource(TEXT("{ while (true)"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MissingWhileBody));
		ExpectBodySource(TEXT("{{"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnterminatedCompound));

		FString Deep = TEXT("int F(){ return ");
		for (int32 Index = 0; Index < 130; ++Index)
		{
			Deep += TEXT("(");
		}
		Deep += TEXT("1");
		for (int32 Index = 0; Index < 130; ++Index)
		{
			Deep += TEXT(")");
		}
		Deep += TEXT("; }");
		SessionExpect(Deep, static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ExpressionDepthLimit), false);

		LexExpect(TEXT("Comment.as"), TEXT("int F(){ /* x"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnterminatedBlockComment));
		LexExpect(TEXT("String.as"), TEXT("int F(){ return \"abc }"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnterminatedString));
		LexExpect(TEXT("Unicode.as"), TEXT("\u03c0Value"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnicodeIdentifierDisallowed));
		const uint8 EmbeddedNul[] = {'i', 'n', 't', 0};
		LexExpectBytes(TEXT("Nul.as"), MakeArrayView(EmbeddedNul), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::EmbeddedNul));
		const uint8 Unexpected[] = {'$'};
		LexExpectBytes(TEXT("Char.as"), MakeArrayView(Unexpected), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnexpectedCharacter));

		PpExpect(TEXT("#if UNKNOWN\nint X;\n#endif\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnknownFlag));
		PpExpect(TEXT("#if defined(\n#endif\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedCondition));
		PpExpect(TEXT("#if 1 && (\nint X;\n#endif\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedCondition));
		PpExpect(TEXT("#if defined()\nint X;\n#endif\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedCondition));
		PpExpect(TEXT("#if 2\nint X;\n#endif\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedCondition));
		PpExpect(TEXT("#if 1 +\nint X;\n#endif\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedCondition));
		PpExpect(TEXT("#ifdef\nint X;\n#endif\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedCondition));
		PpExpect(TEXT("#elif X\nint X;\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnexpectedElif));
		PpExpect(TEXT("#else\nint X;\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnexpectedElse));
		PpExpect(TEXT("#endif\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnexpectedEndif));
		PpExpect(TEXT("#if 1\n#else\n#elif 1\n#endif\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ElifAfterElse));
		PpExpect(TEXT("#if 1\n#else\n#else\n#endif\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::DuplicateElse));
		PpExpect(TEXT("#if 1\nint X;\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MissingEndif));
		PpExpect(TEXT("#pragma once\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnsupportedDirective));
		PpExpect(TEXT("#restrict access allow Gameplay\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::InvalidRestrictionOperation));
		PpExpect(TEXT("#restrict usage Gameplay\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::InvalidRestrictionPolicy));
		PpExpect(TEXT("#restrict usage allow\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MissingRestrictionPattern));
		PpExpect(TEXT("#restrict usage allow Gameplay Extra\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ExtraRestrictionArgument));
		PpExpect(TEXT("#include \"Other.as\"\n"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnsupportedInclude));

		const AngelscriptNativePreprocessorTest::FPreprocessorRun ShortCircuit(
			TEXT("#if 0 && UNKNOWN\nint Wrong;\n#endif\n#if 1 || UNKNOWN\nint Selected;\n#endif\n"),
			{});
		bool bUnknown = false;
		for (const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord& Record : ShortCircuit.DiagnosticRecords)
		{
			if (Record.DiagnosticID == static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnknownFlag))
			{
				bUnknown = true;
			}
		}
		ASSERT_THAT(IsFalse(bUnknown));

		AS_NAMESPACE_QUALIFIER asCSourceFileID BodyFile;
		const AS_NAMESPACE_QUALIFIER asCSourceManager BodyManager =
			AngelscriptNativeEngineTest::MakeSourceManager(TEXT("NoBrace.as"), TEXT("return 1;"), &BodyFile);
		AS_NAMESPACE_QUALIFIER asSDirectiveProcessResult InvalidInput;
		InvalidInput.Sources = BodyManager;
		ExpectParseBody(
			InvalidInput,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::BodyInputInvalid),
			BodyManager);

		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine BodyEngine(BodyManager);
		AS_NAMESPACE_QUALIFIER asCIdentifierTable BodyIds;
		TConstArrayView<uint8> BodyBytes;
		ASSERT_THAT(IsTrue(BodyManager.TryGetBytes(BodyFile, BodyBytes)));
		AS_NAMESPACE_QUALIFIER asCTokenizer BodyTokenizer(
			BodyManager,
			BodyManager.MakeRange(BodyFile, 0, static_cast<asUINT>(BodyBytes.Num())),
			AS_NAMESPACE_QUALIFIER asSLexOptions(),
			BodyIds,
			BodyEngine);
		TArray<AS_NAMESPACE_QUALIFIER asCToken> BodyTokens;
		for (;;)
		{
			AS_NAMESPACE_QUALIFIER asCToken Token;
			if (!BodyTokenizer.Lex(Token))
			{
				break;
			}
			BodyTokens.Add(Token);
			if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile)
			{
				break;
			}
		}
		AS_NAMESPACE_QUALIFIER asSDirectiveProcessResult NoBrace;
		NoBrace.bValid = true;
		NoBrace.Sources = BodyManager;
		NoBrace.ActiveTokens = BodyTokens;
		ExpectParseBody(
			NoBrace,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::BodyMissingOpeningBrace),
			BodyManager);
	}
};

#endif
