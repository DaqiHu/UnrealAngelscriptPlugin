#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_ast_codec.h"
#include "frontend/AST/as_ast_projection.h"
#include "frontend/AST/as_ast_visitor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace BodyConversionTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;
	TEST_CLASS_WITH_FLAGS(BodiesConversions, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
	private:
		static TArray<const AS_NAMESPACE_QUALIFIER asCUserConversionExpr*> Conversions(const FSessionRun& Run)
		{
			TArray<const AS_NAMESPACE_QUALIFIER asCUserConversionExpr*> Result;
			AS_NAMESPACE_QUALIFIER asCASTVisitor::Walk(Run.Session->GetTranslationUnit(), [&](const AS_NAMESPACE_QUALIFIER asCASTNode& Node)
			{
				if (const auto* Conversion = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCUserConversionExpr>(&Node)) Result.Add(Conversion);
			});
			return Result;
		}
	public:
		TEST_METHOD(ImplicitReturnSelectsTheActualDestinationOverload)
		{
			FSessionRun Run({{TEXT("Destinations.as"), TEXT("class Source { int opImplConv() const; float64 opImplConv() const; } int F(Source Value) { return Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const auto Uses = Conversions(Run);
			ASSERT_THAT(AreEqual(1, Uses.Num()));
			ASSERT_THAT(IsTrue(Uses[0]->GetMode() == AS_NAMESPACE_QUALIFIER asEUserConversionMode::Implicit));
			ASSERT_THAT(IsTrue(Uses[0]->GetConversionFunction()->GetFunctionKind() == AS_NAMESPACE_QUALIFIER asEFunctionKind::Conversion));
			ASSERT_THAT(IsTrue(Uses[0]->GetExpressionQualType() == Uses[0]->GetConversionFunction()->GetResolvedSignature()->GetReturnType()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Uses[0]->GetExpressionType())->GetPrimitive()));
			auto* Source = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->Lookup(TEXT("Source"))[0]);
			const auto Overloads = Source->Lookup(TEXT("opImplConv"));
			ASSERT_THAT(AreEqual(2, Overloads.Num()));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Overloads[0])->GetStableIdentity() != AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Overloads[1])->GetStableIdentity()));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(ExplicitPrimitiveConstructionPrefersExplicitConversion)
		{
			FSessionRun Run({{TEXT("ExplicitConversion.as"), TEXT("class Source { int opConv() const; int opImplConv() const; } int F(Source Value) { return int(Value); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const auto Uses = Conversions(Run);
			ASSERT_THAT(AreEqual(1, Uses.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("opConv")), Uses[0]->GetConversionFunction()->GetName()));
			ASSERT_THAT(IsTrue(Uses[0]->GetMode() == AS_NAMESPACE_QUALIFIER asEUserConversionMode::Explicit));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(ExplicitOnlyConversionCannotLeakIntoImplicitReturn)
		{
			FSessionRun Run({{TEXT("ExplicitOnly.as"), TEXT("class Source { int opConv() const; } int F(Source Value) { return Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(AreEqual(0, Conversions(Run).Num()));
		}

		TEST_METHOD(ConstReceiverFiltersMutableConversionAndMutableReceiverPrefersIt)
		{
			for (const bool ConstReceiver : {false, true})
			{
				FSessionRun Run({{TEXT("ConstConversion.as"), FString::Printf(TEXT("class Source { int opImplConv(); int opImplConv() const; } int F(%sSource Value) { return Value; }"), ConstReceiver ? TEXT("const ") : TEXT(""))}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
				const auto Uses = Conversions(Run);
				ASSERT_THAT(AreEqual(1, Uses.Num()));
				ASSERT_THAT(AreEqual(ConstReceiver, Uses[0]->GetConversionFunction()->IsConstMethod()));
				const bool Verified = Run.Session->VerifyAST();
				ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
			}
		}

		TEST_METHOD(ConstReceiverCannotCallOnlyMutableConversion)
		{
			FSessionRun Run({{TEXT("MutableOnly.as"), TEXT("class Source { int opImplConv(); } int F(const Source Value) { return Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(InheritedConversionRetainsItsRealBaseDeclaration)
		{
			FSessionRun Run({{TEXT("InheritedConversion.as"), TEXT("class Base { int opImplConv() const; } class Source : Base {} int F(Source Value) { return Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const auto Uses = Conversions(Run);
			ASSERT_THAT(AreEqual(1, Uses.Num()));
			ASSERT_THAT(IsTrue(Uses[0]->GetConversionFunction()->GetDeclContext()->GetOwnerDecl() == Run.Session->Lookup(TEXT("Base"))[0]));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(EquallyViableUserConvertedCallOverloadsAreAmbiguous)
		{
			FSessionRun Run({{TEXT("AmbiguousConversions.as"), TEXT("class Source { int opImplConv() const; bool opImplConv() const; } void Use(int Value); void Use(bool Value); void F(Source Value) { Use(Value); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("unresolved-call:Use"))));
		}

		TEST_METHOD(ExactCallArgumentWinsOverUserConversion)
		{
			FSessionRun Run({{TEXT("ExactBeforeConversion.as"), TEXT("class Source { int opImplConv() const; } int Use(Source Value); bool Use(int Value); int F(Source Value) { return Use(Value); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(AreEqual(0, Conversions(Run).Num()));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(UserConversionPrecedesASeparateBuiltinNumericConversion)
		{
			FSessionRun Run({{TEXT("NumericAfterConversion.as"), TEXT("class Source { int opImplConv() const; } float64 F(Source Value) { return Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const auto Uses = Conversions(Run);
			ASSERT_THAT(AreEqual(1, Uses.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Uses[0]->GetExpressionType())->GetPrimitive()));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* Numeric = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCImplicitCastExpr>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0])->GetValue());
			ASSERT_THAT(IsNotNull(Numeric));
			ASSERT_THAT(IsTrue(Numeric->GetOperand() == Uses[0]));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float64, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Numeric->GetExpressionType())->GetPrimitive()));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(
				Run.Session->GetASTContext(), Run.Session->GetTranslationUnit(), Projection, Error), Error));
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsTrue(Decoded.Equals(Projection)));
			FString IdentityError;
			ASSERT_THAT(IsTrue(Decoded.ValidateIdentities(
				Run.Session->GetASTContext().GetTypeContext()->GetIdentityRegistry(), IdentityError), IdentityError));
		}

		TEST_METHOD(ImplicitHandleCastKeepsActualTargetAndCannotRemoveConst)
		{
			FSessionRun Run({{TEXT("HandleConversion.as"), TEXT("class Target {} class Source { Target@ opImplCast() const; } Target@ F(Source@ Value) { return Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const auto Uses = Conversions(Run);
			ASSERT_THAT(AreEqual(1, Uses.Num()));
			ASSERT_THAT(IsTrue(Uses[0]->GetExpressionQualType().IsHandle()));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
			FSessionRun Denied({{TEXT("ConstHandleConversion.as"), TEXT("class Target {} class Source { Target@ opImplCast() const; } Target@ F(const Source@ Value) { return Value; }")}});
			ASSERT_THAT(IsTrue(Denied.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Denied.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(ExplicitHandleCastSelectsOpCastInsteadOfOpImplCast)
		{
			FSessionRun Run({{TEXT("ExplicitHandleConversion.as"), TEXT("class Target {} class Source { Target@ opCast() const; Target@ opImplCast() const; } Target@ F(Source@ Value) { return Cast<Target@>(Value); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const auto Uses = Conversions(Run);
			ASSERT_THAT(AreEqual(1, Uses.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("opCast")), Uses[0]->GetConversionFunction()->GetName()));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(BoolConversionCreatesTypedConditionWithoutNumericTruthiness)
		{
			FSessionRun Run({{TEXT("BoolConversion.as"), TEXT("class Source { bool opImplConv() const; } int F(Source Value) { if (Value) return 1; while (Value) break; for (;Value;) break; do {} while(Value); return Value ? 2 : 3; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(AreEqual(5, Conversions(Run).Num()));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(
				Run.Session->GetASTContext(), Run.Session->GetTranslationUnit(), Projection, Error), Error));
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsTrue(Decoded.Equals(Projection)));
			FString IdentityError;
			ASSERT_THAT(IsTrue(Decoded.ValidateIdentities(
				Run.Session->GetASTContext().GetTypeContext()->GetIdentityRegistry(), IdentityError), IdentityError));
		}

		TEST_METHOD(PrivateConversionCannotBypassAccessPolicy)
		{
			FSessionRun Run({{TEXT("PrivateConversion.as"), TEXT("class Source { private int opImplConv() const; } int F(Source Value) { return Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(LogicalOperatorsKeepActualBoolConversionOperands)
		{
			FSessionRun Run({{TEXT("LogicalConversions.as"), TEXT("class Source { bool opImplConv() const; } bool F(Source A, Source B) { return !A || (A && B); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(AreEqual(3, Conversions(Run).Num()));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(UniqueUserConversionMaterializesForACallArgumentAndAssignment)
		{
			FSessionRun Run({{TEXT("ConversionConsumers.as"), TEXT("class Source { int opImplConv() const; } int Use(int Value); int F(Source Value) { int Result = Value; Result = Value; return Use(Value); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(AreEqual(3, Conversions(Run).Num()));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}
	};
}
#endif
