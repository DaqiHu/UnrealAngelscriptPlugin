#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_ast_visitor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace BodyLambdaTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;
	TEST_CLASS_WITH_FLAGS(BodiesLambdas, "Angelscript.UnitTest.NativeEngine.Sema", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(ImmediateAnonymousInvocationIsRejected)
		{
			FSessionRun Run({{TEXT("Lambda.as"), TEXT("int F() { return function(int Value) { return Value + 1; }(41); }")}});
			Run.ResolveDeclarations();
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(2)));
			ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			asCCollectingDiagnosticConsumer Consumer;
			Run.Diagnostics.EmitTo(Consumer);
			bool Found = false;
			for (const auto& Record : Consumer.GetRecords())
				Found |= Record.DiagnosticID == 3008 && Record.Arguments.Num() == 1
					&& Record.Arguments[0].TextValue == TEXT("AnonymousFunction");
			ASSERT_THAT(IsTrue(Found));
		}

		TEST_METHOD(ContextualDelegateDoesNotAdmitAnonymousFunction)
		{
			FSessionRun Run({{TEXT("Contextual.as"), TEXT("delegate int Transform(int Value); int F() { Transform Op = function(Value) { return Value + 1; }; return Op(41); }")}});
			Run.ResolveDeclarations();
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(2)));
			ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			asCCollectingDiagnosticConsumer Consumer;
			Run.Diagnostics.EmitTo(Consumer);
			bool Found = false;
			for (const auto& Record : Consumer.GetRecords())
				Found |= Record.DiagnosticID == 3008 && Record.Arguments.Num() == 1
					&& Record.Arguments[0].TextValue == TEXT("AnonymousFunction");
			ASSERT_THAT(IsTrue(Found));
		}

		TEST_METHOD(ImmediateOuterCaptureIsRejected)
		{
			FSessionRun Run({{TEXT("Capture.as"), TEXT("int F() { int Base = 40; return function(int Value) { return Base + Value; }(2); }")}});
			Run.ResolveDeclarations();
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(2)));
			ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			asCCollectingDiagnosticConsumer Consumer;
			Run.Diagnostics.EmitTo(Consumer);
			bool Found = false;
			for (const auto& Record : Consumer.GetRecords())
				Found |= Record.DiagnosticID == 3008 && Record.Arguments.Num() == 1
					&& Record.Arguments[0].TextValue == TEXT("AnonymousFunction");
			ASSERT_THAT(IsTrue(Found));
		}

		TEST_METHOD(StoredCapturedAnonymousFunctionIsRejected)
		{
			FSessionRun Run({{TEXT("Escaping.as"), TEXT("delegate int Read(); int F() { int Value = 42; Read Op = function() { return Value; }; return Op(); }")}});
			Run.ResolveDeclarations();
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(2)));
			ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			asCCollectingDiagnosticConsumer Consumer;
			Run.Diagnostics.EmitTo(Consumer);
			bool Found = false;
			for (const auto& Record : Consumer.GetRecords())
				Found |= Record.DiagnosticID == 3008 && Record.Arguments.Num() == 1
					&& Record.Arguments[0].TextValue == TEXT("AnonymousFunction");
			ASSERT_THAT(IsTrue(Found));
		}

		TEST_METHOD(AnonymousReturnInferenceIsRemoved)
		{
			FSessionRun Run({{TEXT("ReturnMismatch.as"), TEXT("int F() { return function(bool Pick) { if (Pick) return 1; return true; }(true); }")}});
			Run.ResolveDeclarations();
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(2)));
			ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			asCCollectingDiagnosticConsumer Consumer;
			Run.Diagnostics.EmitTo(Consumer);
			bool Found = false;
			for (const auto& Record : Consumer.GetRecords())
				Found |= Record.DiagnosticID == 3008 && Record.Arguments.Num() == 1
					&& Record.Arguments[0].TextValue == TEXT("AnonymousFunction");
			ASSERT_THAT(IsTrue(Found));
		}

		TEST_METHOD(MultipleAnonymousOriginsAreRejected)
		{
			FSessionRun Run({{TEXT("Origins.as"), TEXT("int F() { int A = function() { return 1; }(); int B = function() { return 2; }(); return A + B; }")}});
			Run.ResolveDeclarations();
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(2)));
			ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			asCCollectingDiagnosticConsumer Consumer;
			Run.Diagnostics.EmitTo(Consumer);
			bool Found = false;
			for (const auto& Record : Consumer.GetRecords())
				Found |= Record.DiagnosticID == 3008 && Record.Arguments.Num() == 1
					&& Record.Arguments[0].TextValue == TEXT("AnonymousFunction");
			ASSERT_THAT(IsTrue(Found));
		}

		TEST_METHOD(DefaultAndNestedAnonymousOriginsAreRejected)
		{
			const FString Script = TEXT("delegate int Read(); int F(Read First = function() { return 1; }, Read Second = function() { return 2; }) { return function() { return function() { return 3; }(); }(); }");
			FSessionRun Run({{TEXT("NestedOrigins.as"), Script}});
			Run.ResolveDeclarations();
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(2)));
			ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			asCCollectingDiagnosticConsumer Consumer;
			Run.Diagnostics.EmitTo(Consumer);
			bool Found = false;
			for (const auto& Record : Consumer.GetRecords())
				Found |= Record.DiagnosticID == 3008 && Record.Arguments.Num() == 1
					&& Record.Arguments[0].TextValue == TEXT("AnonymousFunction");
			ASSERT_THAT(IsTrue(Found));
		}

		TEST_METHOD(TypedAnonymousFunctionIsRejected)
		{
			FSessionRun Run({{TEXT("SignatureMismatch.as"), TEXT("delegate int Read(int Value); void F() { Read Op = function(bool Value) { return 1; }; }")}});
			Run.ResolveDeclarations();
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(2)));
			ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			asCCollectingDiagnosticConsumer Consumer;
			Run.Diagnostics.EmitTo(Consumer);
			bool Found = false;
			for (const auto& Record : Consumer.GetRecords())
				Found |= Record.DiagnosticID == 3008 && Record.Arguments.Num() == 1
					&& Record.Arguments[0].TextValue == TEXT("AnonymousFunction");
			ASSERT_THAT(IsTrue(Found));
		}

		TEST_METHOD(MethodAnonymousThisCaptureIsRejected)
		{
			FSessionRun Run({{TEXT("ScopedLambda.as"), TEXT("namespace Example { class Item { int Value; int Read() { return function() { return this.Value; }(); } } }")}});
			Run.ResolveDeclarations();
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(2)));
			ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			asCCollectingDiagnosticConsumer Consumer;
			Run.Diagnostics.EmitTo(Consumer);
			bool Found = false;
			for (const auto& Record : Consumer.GetRecords())
				Found |= Record.DiagnosticID == 3008 && Record.Arguments.Num() == 1
					&& Record.Arguments[0].TextValue == TEXT("AnonymousFunction");
			ASSERT_THAT(IsTrue(Found));
		}

	};
}
#endif
