#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"
#include "as_builder.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "frontend/AST/as_ast_context.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/Parser/as_parser.h"
#include "frontend/Lexer/as_preprocessor.h"
#include "frontend/Sema/as_sema.h"
#include "frontend/Basic/as_stable_key.h"
#include "frontend/Lexer/as_tokenizer.h"
#include "frontend/AST/as_type_context.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(CanonicalNamespace, "Angelscript.UnitTest.NativeEngine.Compile", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(QualifiedCanonicalTypesCompileWithoutNestedFrontend)
	{
		const uint8 Bytes[] = {'i','n','t',' ','F','(',')','{','r','e','t','u','r','n',' ','1',';','}'};
		AS_NAMESPACE_QUALIFIER asCSourceManager Sources =
			AngelscriptNativeEngineTest::MakeSourceManagerFromBytes(TEXT("Canonical.as"), MakeArrayView(Bytes));
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics{Sources};
		AS_NAMESPACE_QUALIFIER asCASTContext Context{Sources};
		AS_NAMESPACE_QUALIFIER asCSema Sema{Context, Diagnostics, TEXT("canonical")};
		AS_NAMESPACE_QUALIFIER asCParser Parser{Sema};
		AS_NAMESPACE_QUALIFIER asCIdentifierTable Identifiers;
		AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(
			Sources,
			Sources.MakeRange(AS_NAMESPACE_QUALIFIER asCSourceFileID(1), 0, UE_ARRAY_COUNT(Bytes)),
			AS_NAMESPACE_QUALIFIER asSLexOptions(
				AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
				AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia),
			Identifiers,
			Diagnostics);
		AS_NAMESPACE_QUALIFIER asCPreprocessorConfiguration Configuration;
		AS_NAMESPACE_QUALIFIER asCPreprocessor Preprocessor{Sources, Configuration, Diagnostics};
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		AS_NAMESPACE_QUALIFIER asCTypeContext TypeContext;
		AS_NAMESPACE_QUALIFIER asCCompilationSession Session{Sources, Diagnostics, TEXT("canonical")};
		ASSERT_THAT(IsNotNull(&Parser));
		ASSERT_THAT(IsNotNull(&Tokenizer));
		ASSERT_THAT(IsNotNull(&Preprocessor));
		ASSERT_THAT(IsTrue(Key.IsZero()));
		ASSERT_THAT(IsNotNull(&TypeContext));
		ASSERT_THAT(IsNotNull(&Session.GetASTContext()));
	}

	TEST_METHOD(IndependentBuilderCompilesAuthoredDemoNamespaceWithoutEngine)
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources =
			AngelscriptNativeEngineTest::MakeSourceList(TEXT("Demo.as"), TEXT("namespace Demo { class Item {} }"));
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		Options.StableScope = TEXT("canonical.namespace");
		asCBuilder Builder(Sources, Options);
		ASSERT_THAT(IsNull(Builder.engine));
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		const asCObjectType* Item = nullptr;
		for (asCTypeInfo* Type : Builder.GetDefinitions()->GetTypes())
			if (Type->name == "Item") Item = static_cast<asCObjectType*>(Type);
		ASSERT_THAT(IsNotNull(Item));
		ASSERT_THAT(AreEqual(FString(TEXT("Demo")), FString(UTF8_TO_TCHAR(Item->GetNamespace()))));
		ASSERT_THAT(IsTrue(Builder.GetDefinitions()->GetFunctions().Num() >= 0));
		const asCScriptFunction* Any = Builder.GetDefinitions()->GetFunctions().Num()
			? Builder.GetDefinitions()->GetFunctions()[0] : nullptr;
		if (Any)
		{
			const AS_NAMESPACE_QUALIFIER asSStableKey& FunctionKey = Any->GetStableKey();
			ASSERT_THAT(IsFalse(FunctionKey.IsZero()));
		}
	}
};
#endif
