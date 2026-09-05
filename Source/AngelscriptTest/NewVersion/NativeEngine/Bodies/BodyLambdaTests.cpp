#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "frontend/as_ast_cast.h"
#include "frontend/as_ast_visitor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace BodyLambdaTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;
	TEST_CLASS_WITH_FLAGS(BodiesLambdas, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(ImmediateInvocationInfersActualParameterAndReturnTypes)
		{
			FSessionRun Run({{TEXT("Lambda.as"), TEXT("int F() { return function(int Value) { return Value + 1; }(41); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* Call = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0])->GetValue());
			ASSERT_THAT(IsNotNull(Call));
			auto* Lambda = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCLambdaExpr>(Call->GetCallee());
			ASSERT_THAT(IsNotNull(Lambda));
			ASSERT_THAT(IsTrue(Lambda->GetFunction()->GetEnclosingFunction() == Run.Function(TEXT("F"))));
			ASSERT_THAT(IsFalse(Lambda->GetFunction()->GetStableIdentity().IsZero()));
			ASSERT_THAT(IsTrue(Lambda->GetFunction()->GetResolvedReturnType() == Run.Session->GetASTContext().GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32)));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(ContextualDelegateProvidesUntypedParameterSignature)
		{
			FSessionRun Run({{TEXT("Contextual.as"), TEXT("delegate int Transform(int Value); int F() { Transform Op = function(Value) { return Value + 1; }; return Op(41); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* Init = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(Body->GetStatements()[0])->GetInitializer();
			auto* Lambda = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCLambdaExpr>(Init);
			ASSERT_THAT(IsNotNull(Lambda));
			ASSERT_THAT(AreEqual(1, Lambda->GetFunction()->GetParameters().Num()));
			ASSERT_THAT(IsTrue(Lambda->GetFunction()->GetParameters()[0]->GetResolvedType() == Run.Session->GetASTContext().GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32)));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(ImmediateInvocationRetainsExplicitOuterCapture)
		{
			FSessionRun Run({{TEXT("Capture.as"), TEXT("int F() { int Base = 40; return function(int Value) { return Base + Value; }(2); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* Base = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(Body->GetStatements()[0])->GetDeclaration();
			auto* Call = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[1])->GetValue());
			auto* Lambda = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCLambdaExpr>(Call->GetCallee());
			ASSERT_THAT(AreEqual(1, Lambda->GetFunction()->GetCaptures().Num()));
			ASSERT_THAT(IsTrue(Lambda->GetFunction()->GetCaptures()[0] == Base));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(CapturedLambdaCannotEscapeIntoStoredDelegate)
		{
			FSessionRun Run({{TEXT("Escaping.as"), TEXT("delegate int Read(); int F() { int Value = 42; Read Op = function() { return Value; }; return Op(); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
		}

		TEST_METHOD(InconsistentInferredReturnsAreRejected)
		{
			FSessionRun Run({{TEXT("ReturnMismatch.as"), TEXT("int F() { return function(bool Pick) { if (Pick) return 1; return true; }(true); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(SeparateLambdaOriginsDoNotMergeIdenticalSignatures)
		{
			FSessionRun Run({{TEXT("Origins.as"), TEXT("int F() { int A = function() { return 1; }(); int B = function() { return 2; }(); return A + B; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(2)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* A = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCLambdaExpr>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(Body->GetStatements()[0])->GetInitializer())->GetCallee())->GetFunction();
			auto* B = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCLambdaExpr>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(Body->GetStatements()[1])->GetInitializer())->GetCallee())->GetFunction();
			ASSERT_THAT(IsTrue(A->GetStableIdentity() != B->GetStableIdentity()));
			ASSERT_THAT(IsTrue(A->GetResolvedSignature() == B->GetResolvedSignature()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(DefaultAndBodyOriginsRemainDistinctFromNestedOrigins)
		{
			const FString Script = TEXT("delegate int Read(); int F(Read First = function() { return 1; }, Read Second = function() { return 2; }) { return function() { return function() { return 3; }(); }(); }");
			FSessionRun Run({{TEXT("NestedOrigins.as"), Script}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(2)));
			TArray<AS_NAMESPACE_QUALIFIER asCFunctionDecl*> Lambdas;
			AS_NAMESPACE_QUALIFIER asCASTVisitor::Walk(Run.Session->GetTranslationUnit(), [&](const AS_NAMESPACE_QUALIFIER asCASTNode& Node)
			{
				if (auto* Lambda = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCLambdaExpr>(&Node)) Lambdas.Add(Lambda->GetFunction());
			});
			ASSERT_THAT(AreEqual(4, Lambdas.Num()));
			TArray<uint32> DirectOrdinals;
			int32 NestedCount = 0;
			for (auto* Lambda : Lambdas)
			{
				ASSERT_THAT(IsFalse(Lambda->GetStableIdentity().IsZero()));
				if (Lambda->GetEnclosingFunction() == Run.Function(TEXT("F"))) DirectOrdinals.Add(Lambda->GetLambdaOrdinal());
				else { ++NestedCount; ASSERT_THAT(AreEqual(uint32(0), Lambda->GetLambdaOrdinal())); }
			}
			DirectOrdinals.Sort();
			ASSERT_THAT(AreEqual(3, DirectOrdinals.Num()));
			ASSERT_THAT(AreEqual(uint32(0), DirectOrdinals[0]));
			ASSERT_THAT(AreEqual(uint32(1), DirectOrdinals[1]));
			ASSERT_THAT(AreEqual(uint32(2), DirectOrdinals[2]));
			ASSERT_THAT(AreEqual(1, NestedCount));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(TypedLambdaCannotMasqueradeAsDifferentContextualSignature)
		{
			FSessionRun Run({{TEXT("SignatureMismatch.as"), TEXT("delegate int Read(int Value); void F() { Read Op = function(bool Value) { return 1; }; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(NamespaceAndMethodLambdaRetainLexicalIdentityAndThisCapture)
		{
			FSessionRun Run({{TEXT("ScopedLambda.as"), TEXT("namespace Example { class Item { int Value; int Read() { return function() { return this.Value; }(); } } }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			AS_NAMESPACE_QUALIFIER asCLambdaExpr* Found = nullptr;
			AS_NAMESPACE_QUALIFIER asCASTVisitor::Walk(Run.Session->GetTranslationUnit(), [&](const AS_NAMESPACE_QUALIFIER asCASTNode& Node)
			{
				if (auto* Lambda = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCLambdaExpr>(&Node)) Found = const_cast<AS_NAMESPACE_QUALIFIER asCLambdaExpr*>(Lambda);
			});
			ASSERT_THAT(IsNotNull(Found));
			ASSERT_THAT(IsNotNull(Found->GetFunction()->GetCapturedThisRecord()));
			ASSERT_THAT(IsTrue(Found->GetFunction()->GetDeclContext() == Found->GetFunction()->GetEnclosingFunction()));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Identity;
			ASSERT_THAT(IsTrue(Run.Session->GetASTContext().GetTypeContext()->GetIdentityRegistry().ResolveFunction(Found->GetFunction()->GetStableIdentity(), Identity) == AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded));
			ASSERT_THAT(AreEqual(1, Identity.NamespacePath.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("Example")), Identity.NamespacePath[0]));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}
	};
}
#endif
