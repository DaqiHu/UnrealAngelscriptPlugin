#include "CQTest.h"

#include "frontend/AST/as_ast_context.h"
#include "frontend/AST/as_ast_projection.h"
#include "frontend/AST/as_ast_verifier.h"
#include "frontend/AST/as_ast_visitor.h"
#include "frontend/AST/as_attr.h"

#include <limits>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace TypedASTTraversalTests
{
	static TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot()
	{
		auto Result = MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		const uint8 Bytes[] = {'n','a','m','e','s','p','a','c','e',' ','N',' ','{','}',';'};
		Result->AddFile(TEXT("Tree.as"), MakeArrayView(Bytes));
		Result->Freeze();
		return Result;
	}

	static AS_NAMESPACE_QUALIFIER asCSourceRange Range(const AS_NAMESPACE_QUALIFIER asCSourceSnapshot& Source, asUINT Begin, asUINT End)
	{
		return {{Source.GetSnapshotID(), AS_NAMESPACE_QUALIFIER asCSourceFileID(1), Begin}, {Source.GetSnapshotID(), AS_NAMESPACE_QUALIFIER asCSourceFileID(1), End}};
	}

	TEST_CLASS_WITH_FLAGS(
		ASTTraversal,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(OwningTraversalIsDeterministicAndOnceOnly)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Root = Context.Create<AS_NAMESPACE_QUALIFIER asCTranslationUnitDecl>(Range(*Source, 0, 15));
			auto* Namespace = Context.Create<AS_NAMESPACE_QUALIFIER asCNamespaceDecl>(TEXT("N"), Range(*Source, 0, 14));
			auto* Record = Context.Create<AS_NAMESPACE_QUALIFIER asCRecordDecl>(TEXT("R"), Range(*Source, 2, 10));
			auto* Field = Context.Create<AS_NAMESPACE_QUALIFIER asCVarDecl>(AS_NAMESPACE_QUALIFIER asEDeclKind::Var, TEXT("V"), Range(*Source, 4, 5));
			Root->AddDecl(Namespace);
			Namespace->AddDecl(Record);
			Record->AddDecl(Field);

			TArray<uint16> First;
			TArray<uint16> Second;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVisitStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTVisitor::Walk(Root, [&First](const AS_NAMESPACE_QUALIFIER asCASTNode& Node) { First.Add(Node.GetRawKind()); })));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVisitStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTVisitor::Walk(Root, [&Second](const AS_NAMESPACE_QUALIFIER asCASTNode& Node) { Second.Add(Node.GetRawKind()); })));
			ASSERT_THAT(IsTrue(First == Second));
			ASSERT_THAT(AreEqual(4, First.Num()));
		}

		TEST_METHOD(NonOwningDeclReferencesDoNotCreateCycles)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Variable = Context.Create<AS_NAMESPACE_QUALIFIER asCVarDecl>(AS_NAMESPACE_QUALIFIER asEDeclKind::Var, TEXT("V"), Range(*Source, 0, 1));
			auto* Reference = Context.Create<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Variable, Range(*Source, 2, 3));
			int32 Count = 0;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVisitStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTVisitor::Walk(Reference, [&Count](const AS_NAMESPACE_QUALIFIER asCASTNode&) { ++Count; })));
			ASSERT_THAT(AreEqual(1, Count));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::GetCrossReference(*Reference) == Variable));
		}

		TEST_METHOD(SharedCanonicalTypesAreSemanticReferencesNotOwningChildren)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* SharedType = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			auto* Compound = Context.Create<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Range(*Source, 0, 15));
			auto* First = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(1, Range(*Source, 1, 2));
			auto* Second = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(2, Range(*Source, 3, 4));
			ASSERT_THAT(IsTrue(First->SetExpressionSemantics(SharedType, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsTrue(Second->SetExpressionSemantics(SharedType, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsTrue(Compound->AddStatement(First)));
			ASSERT_THAT(IsTrue(Compound->AddStatement(Second)));

			int32 OwningVisitCount = 0;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVisitStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTVisitor::Walk(Compound,
					[&OwningVisitCount](const AS_NAMESPACE_QUALIFIER asCASTNode&) { ++OwningVisitCount; })));
			ASSERT_THAT(AreEqual(3, OwningVisitCount));
			TArray<AS_NAMESPACE_QUALIFIER asSASTSemanticTypeReference> FirstReferences;
			TArray<AS_NAMESPACE_QUALIFIER asSASTSemanticTypeReference> SecondReferences;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectSemanticTypeReferences(*First, FirstReferences)));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectSemanticTypeReferences(*Second, SecondReferences)));
			ASSERT_THAT(AreEqual(1, FirstReferences.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTSemanticTypeReferenceKind::ExpressionType,
				FirstReferences[0].Kind));
			ASSERT_THAT(IsTrue(FirstReferences[0].Type.GetType() == SharedType));
			ASSERT_THAT(IsTrue(SecondReferences[0].Type.GetType() == SharedType));
		}

		TEST_METHOD(VerifierRejectsDuplicateOwningEdges)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Compound = Context.Create<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Range(*Source, 0, 5));
			auto* Literal = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(1, Range(*Source, 1, 2));
			ASSERT_THAT(IsTrue(Literal->SetExpressionSemantics(
				Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32),
				AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			Compound->AddStatement(Literal);
			Compound->AddStatement(Literal);
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER asCASTVerifier::Verify(Context, Compound, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::DuplicateOwningEdge, Result.Status));
		}

		TEST_METHOD(VerifierRejectsOwningCycles)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Compound = Context.Create<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Range(*Source, 0, 5));
			Compound->AddStatement(Compound);
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER asCASTVerifier::Verify(Context, Compound, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::OwningCycle, Result.Status));
		}

		TEST_METHOD(VerifierRejectsForeignChildrenAndRanges)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			AS_NAMESPACE_QUALIFIER asCASTContext Foreign(Source);
			auto* Compound = Context.Create<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Range(*Source, 0, 5));
			auto* ForeignLiteral = Foreign.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(1, Range(*Source, 1, 2));
			Compound->AddStatement(ForeignLiteral);
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER asCASTVerifier::Verify(Context, Compound, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::ForeignNode, Result.Status));

			AS_NAMESPACE_QUALIFIER asCASTContext OtherContext(Source);
			AS_NAMESPACE_QUALIFIER asCSourceRange Wrong = Range(*Source, 0, 1);
			Wrong.Begin.SnapshotID += 1;
			Wrong.End.SnapshotID += 1;
			auto* WrongRange = OtherContext.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(1, Wrong);
			ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER asCASTVerifier::Verify(OtherContext, WrongRange, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidSourceRange, Result.Status));
		}

		TEST_METHOD(RecoveryNodesCannotSealAPublishableRoot)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Compound = Context.Create<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Range(*Source, 0, 5));
			Compound->AddStatement(Context.Create<AS_NAMESPACE_QUALIFIER asCRecoveryExpr>(Range(*Source, 1, 2)));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Compound, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::RecoveryReachable, Result.Status));
			ASSERT_THAT(IsFalse(Context.IsSealed()));
		}

		TEST_METHOD(VerifierRejectsMissingExpressionSemantics)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Literal = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(1, Range(*Source, 1, 2));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Literal, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::MissingSemanticReference, Result.Status));
			ASSERT_THAT(IsFalse(Context.IsSealed()));
		}

		TEST_METHOD(VerifierRejectsInvalidConcretePayloadAndCrossReferenceTarget)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext InvalidPayloadContext(Source);
			auto* Type = InvalidPayloadContext.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			auto* Operand = InvalidPayloadContext.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(1, Range(*Source, 1, 2));
			Operand->SetExpressionSemantics(Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue);
			auto* InvalidOperator = InvalidPayloadContext.Create<AS_NAMESPACE_QUALIFIER asCUnaryOperator>(
				static_cast<AS_NAMESPACE_QUALIFIER asEUnaryOperator>(255), Operand,
				Range(*Source, 0, 1), Range(*Source, 0, 2));
			InvalidOperator->SetExpressionSemantics(Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue);
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(InvalidPayloadContext.VerifyAndSeal(InvalidOperator, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidPayload, Result.Status));

			AS_NAMESPACE_QUALIFIER asCASTContext InvalidReferenceContext(Source);
			auto* NonLoop = InvalidReferenceContext.Create<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Range(*Source, 0, 4));
			auto* Break = InvalidReferenceContext.Create<AS_NAMESPACE_QUALIFIER asCBreakStmt>(NonLoop, Range(*Source, 1, 2));
			ASSERT_THAT(IsFalse(InvalidReferenceContext.VerifyAndSeal(Break, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidCrossReference, Result.Status));
		}

		TEST_METHOD(VerifierAcceptsAuthoredEmptyValuesAndUnnamedParameters)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext ParameterContext(Source);
			auto* Parameter = ParameterContext.Create<AS_NAMESPACE_QUALIFIER asCVarDecl>(
				AS_NAMESPACE_QUALIFIER asEDeclKind::ParmVar, FString{}, Range(*Source, 0, 4));
			AS_NAMESPACE_QUALIFIER asSTypeSyntax ParameterSyntax;
			ParameterSyntax.NameParts.Add(TEXT("int"));
			ParameterSyntax.ComponentRanges.Add(Range(*Source, 0, 3));
			ParameterSyntax.Range = Range(*Source, 0, 3);
			ASSERT_THAT(IsTrue(Parameter->SetTypeSyntax(MoveTemp(ParameterSyntax))));
			ASSERT_THAT(IsTrue(Parameter->SetResolvedType(
				ParameterContext.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32))));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsTrue(ParameterContext.VerifyAndSeal(Parameter, Result)));

			AS_NAMESPACE_QUALIFIER asSAnnotationArgument EmptyString;
			EmptyString.Name = TEXT("DisplayName");
			EmptyString.Kind = AS_NAMESPACE_QUALIFIER asEAnnotationValueKind::String;
			EmptyString.Range = Range(*Source, 0, 4);
			EmptyString.ValueRange = Range(*Source, 3, 4);
			AS_NAMESPACE_QUALIFIER asSAnnotationArgument EmptyGroup;
			EmptyGroup.Name = TEXT("Meta");
			EmptyGroup.Kind = AS_NAMESPACE_QUALIFIER asEAnnotationValueKind::Group;
			EmptyGroup.Range = Range(*Source, 5, 10);
			EmptyGroup.ValueRange = Range(*Source, 9, 10);
			AS_NAMESPACE_QUALIFIER asCASTContext AttributeContext(Source);
			auto* Attribute = AttributeContext.Create<AS_NAMESPACE_QUALIFIER asCAnnotationAttr>(
				AS_NAMESPACE_QUALIFIER asEAnnotationKind::UProperty,
				TArray<AS_NAMESPACE_QUALIFIER asSAnnotationArgument>{MoveTemp(EmptyString), MoveTemp(EmptyGroup)},
				Range(*Source, 0, 11), AS_NAMESPACE_QUALIFIER asEAttrTarget::Variable);
			ASSERT_THAT(IsTrue(AttributeContext.VerifyAndSeal(Attribute, Result)));
		}

		TEST_METHOD(VerifierRejectsLiteralPayloadsThatDisagreeWithSemanticTypes)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;

			AS_NAMESPACE_QUALIFIER asCASTContext IntegerContext(Source);
			auto* Integer = IntegerContext.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(256, Range(*Source, 0, 3));
			ASSERT_THAT(IsTrue(Integer->SetExpressionSemantics(
				IntegerContext.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt8),
				AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsFalse(IntegerContext.VerifyAndSeal(Integer, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidPayload, Result.Status));

			AS_NAMESPACE_QUALIFIER asCASTContext BooleanContext(Source);
			auto* Boolean = BooleanContext.Create<AS_NAMESPACE_QUALIFIER asCBooleanLiteral>(true, Range(*Source, 0, 4));
			ASSERT_THAT(IsTrue(Boolean->SetExpressionSemantics(
				BooleanContext.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32),
				AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsFalse(BooleanContext.VerifyAndSeal(Boolean, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidPayload, Result.Status));

			AS_NAMESPACE_QUALIFIER asCASTContext FloatingContext(Source);
			auto* Floating = FloatingContext.Create<AS_NAMESPACE_QUALIFIER asCFloatingLiteral>(
				std::numeric_limits<double>::quiet_NaN(), Range(*Source, 0, 4));
			ASSERT_THAT(IsTrue(Floating->SetExpressionSemantics(
				FloatingContext.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float64),
				AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsFalse(FloatingContext.VerifyAndSeal(Floating, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidPayload, Result.Status));
		}

		TEST_METHOD(QualifiedExpressionTypeIsACompleteSemanticReference)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Type = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asCQualType Qualified;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(
				Type,
				AS_NAMESPACE_QUALIFIER asETypeQualifiers::Const
					| AS_NAMESPACE_QUALIFIER asETypeQualifiers::Reference
					| AS_NAMESPACE_QUALIFIER asETypeQualifiers::Handle
					| AS_NAMESPACE_QUALIFIER asETypeQualifiers::Nullable,
				Qualified)));
			auto* Expression = Context.Create<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(
				Context.Create<AS_NAMESPACE_QUALIFIER asCVarDecl>(
					AS_NAMESPACE_QUALIFIER asEDeclKind::Var, TEXT("Value"), Range(*Source, 0, 5)),
				Range(*Source, 0, 5));
			ASSERT_THAT(IsTrue(Expression->SetExpressionQualType(
				Qualified, AS_NAMESPACE_QUALIFIER asEValueCategory::LValue)));

			TArray<AS_NAMESPACE_QUALIFIER asSASTSemanticTypeReference> References;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectSemanticTypeReferences(
				*Expression, References)));
			ASSERT_THAT(AreEqual(1, References.Num()));
			ASSERT_THAT(IsTrue(References[0].Type == Qualified));
			ASSERT_THAT(IsTrue(References[0].Type.IsConst()));
			ASSERT_THAT(IsTrue(References[0].Type.IsReference()));
			ASSERT_THAT(IsTrue(References[0].Type.IsHandle()));
			ASSERT_THAT(IsTrue(References[0].Type.IsNullable()));
		}

		TEST_METHOD(MemberAndSubscriptReferencesDoNotOwnTheirDeclarations)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* BaseDecl = Context.Create<AS_NAMESPACE_QUALIFIER asCVarDecl>(
				AS_NAMESPACE_QUALIFIER asEDeclKind::Var, TEXT("Object"), Range(*Source, 0, 6));
			auto* MemberDecl = Context.Create<AS_NAMESPACE_QUALIFIER asCVarDecl>(
				AS_NAMESPACE_QUALIFIER asEDeclKind::Var, TEXT("Field"), Range(*Source, 7, 12));
			auto* Operator = Context.Create<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(
				TEXT("opIndex"), Range(*Source, 0, 7));
			auto* Base = Context.Create<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(
				BaseDecl, Range(*Source, 0, 6));
			auto* Index = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(
				0, Range(*Source, 13, 14));
			auto* Member = Context.Create<AS_NAMESPACE_QUALIFIER asCMemberExpr>(
				Base, MemberDecl, Range(*Source, 7, 12), Range(*Source, 0, 12));
			auto* Subscript = Context.Create<AS_NAMESPACE_QUALIFIER asCArraySubscriptExpr>(
				Base, TArray<AS_NAMESPACE_QUALIFIER asCExpr*>{Index}, Operator, Range(*Source, 0, 15));

			TArray<const AS_NAMESPACE_QUALIFIER asCASTNode*> Children;
			TArray<AS_NAMESPACE_QUALIFIER asSASTCrossReference> References;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectOwningChildren(*Member, Children)));
			ASSERT_THAT(AreEqual(1, Children.Num()));
			ASSERT_THAT(IsTrue(Children[0] == Base));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectCrossReferences(*Member, References)));
			ASSERT_THAT(AreEqual(1, References.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCrossReferenceKind::MemberDeclaration, References[0].Kind));
			ASSERT_THAT(IsTrue(References[0].Target == MemberDecl));

			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectOwningChildren(*Subscript, Children)));
			ASSERT_THAT(AreEqual(2, Children.Num()));
			ASSERT_THAT(IsTrue(Children[0] == Base));
			ASSERT_THAT(IsTrue(Children[1] == Index));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectCrossReferences(*Subscript, References)));
			ASSERT_THAT(AreEqual(1, References.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCrossReferenceKind::OperatorFunction, References[0].Kind));
			ASSERT_THAT(IsTrue(References[0].Target == Operator));
		}

		TEST_METHOD(ParameterOwnsDefaultInitializerWhileDefaultUseOnlyReferencesParameter)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Type = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			auto* Initializer = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(
				7, Range(*Source, 8, 9));
			ASSERT_THAT(IsTrue(Initializer->SetExpressionSemantics(
				Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			auto* Parameter = Context.Create<AS_NAMESPACE_QUALIFIER asCVarDecl>(
				AS_NAMESPACE_QUALIFIER asEDeclKind::ParmVar, TEXT("Count"), Range(*Source, 0, 9));
			ASSERT_THAT(IsTrue(Parameter->SetResolvedType(Type)));
			ASSERT_THAT(IsTrue(Parameter->SetInitializer(Initializer)));
			auto* Use = Context.Create<AS_NAMESPACE_QUALIFIER asCDefaultArgumentExpr>(
				Parameter, Range(*Source, 10, 10));
			ASSERT_THAT(IsTrue(Use->SetExpressionSemantics(
				Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));

			TArray<const AS_NAMESPACE_QUALIFIER asCASTNode*> Children;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectOwningChildren(*Parameter, Children)));
			ASSERT_THAT(AreEqual(1, Children.Num()));
			ASSERT_THAT(IsTrue(Children[0] == Initializer));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectOwningChildren(*Use, Children)));
			ASSERT_THAT(AreEqual(0, Children.Num()));
			TArray<AS_NAMESPACE_QUALIFIER asSASTCrossReference> References;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectCrossReferences(*Use, References)));
			ASSERT_THAT(AreEqual(1, References.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCrossReferenceKind::DefaultParameter, References[0].Kind));
			ASSERT_THAT(IsTrue(References[0].Target == Parameter));
		}

		TEST_METHOD(DefaultArgumentUsesReferenceParametersValueForm)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Type = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asSTypeSyntax Syntax;
			Syntax.NameParts.Add(TEXT("int"));
			Syntax.ComponentRanges.Add(Range(*Source, 0, 3));
			Syntax.Range = Range(*Source, 0, 3);
			Syntax.bConst = true;
			Syntax.Reference = AS_NAMESPACE_QUALIFIER asETypeReferenceDirection::In;
			auto* Initializer = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(7, Range(*Source, 4, 5));
			AS_NAMESPACE_QUALIFIER asCQualType ValueType;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(
				Type, AS_NAMESPACE_QUALIFIER asETypeQualifiers::Const, ValueType)));
			ASSERT_THAT(IsTrue(Initializer->SetExpressionQualType(
				ValueType, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			auto* Parameter = Context.Create<AS_NAMESPACE_QUALIFIER asCVarDecl>(
				AS_NAMESPACE_QUALIFIER asEDeclKind::ParmVar, TEXT("Value"), Range(*Source, 0, 5));
			ASSERT_THAT(IsTrue(Parameter->SetTypeSyntax(MoveTemp(Syntax))));
			ASSERT_THAT(IsTrue(Parameter->SetResolvedType(Type)));
			ASSERT_THAT(IsTrue(Parameter->SetInitializer(Initializer)));
			auto* Use = Context.Create<AS_NAMESPACE_QUALIFIER asCDefaultArgumentExpr>(
				Parameter, Range(*Source, 6, 7));
			ASSERT_THAT(IsTrue(Use->SetExpressionQualType(
				ValueType, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));

			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Use, Result)));
			ASSERT_THAT(IsTrue(Parameter->GetResolvedQualType().IsReference()));
			ASSERT_THAT(IsFalse(Use->GetExpressionQualType().IsReference()));
			ASSERT_THAT(IsTrue(Use->GetExpressionQualType().IsConst()));
		}

		TEST_METHOD(DeferredInitializerMustBeMaterializedBeforeSealing)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Variable = Context.Create<AS_NAMESPACE_QUALIFIER asCVarDecl>(
				AS_NAMESPACE_QUALIFIER asEDeclKind::Var, TEXT("Value"), Range(*Source, 0, 9));
			ASSERT_THAT(IsTrue(Variable->SetResolvedType(
				Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32))));
			ASSERT_THAT(IsTrue(Variable->SetDeferredInitializerRange(Range(*Source, 6, 9))));

			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Variable, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidPayload, Result.Status));
		}

		TEST_METHOD(NullLiteralRequiresAContextualHandleType)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;

			AS_NAMESPACE_QUALIFIER asCASTContext InvalidContext(Source);
			auto* Invalid = InvalidContext.Create<AS_NAMESPACE_QUALIFIER asCNullPtrLiteral>(
				Range(*Source, 0, 4));
			ASSERT_THAT(IsTrue(Invalid->SetExpressionSemantics(
				InvalidContext.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32),
				AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsFalse(InvalidContext.VerifyAndSeal(Invalid, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidPayload, Result.Status));

			AS_NAMESPACE_QUALIFIER asCASTContext ValidContext(Source);
			auto& Registry = ValidContext.GetTypeContext()->GetIdentityRegistry();
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Declaration;
			Declaration.StableScope = TEXT("ast-null");
			Declaration.Name = TEXT("Object");
			Declaration.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
			AS_NAMESPACE_QUALIFIER asSStableKey DeclarationKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(Declaration, DeclarationKey)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor UseDescriptor;
			UseDescriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
			UseDescriptor.Declaration = DeclarationKey;
			AS_NAMESPACE_QUALIFIER asSStableKey UseKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(UseDescriptor, UseKey)));
			auto* ObjectType = ValidContext.GetNominalType(DeclarationKey, UseKey);
			AS_NAMESPACE_QUALIFIER asCQualType HandleType;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(
				ObjectType, AS_NAMESPACE_QUALIFIER asETypeQualifiers::Handle, HandleType)));
			auto* Valid = ValidContext.Create<AS_NAMESPACE_QUALIFIER asCNullPtrLiteral>(
				Range(*Source, 0, 4));
			ASSERT_THAT(IsTrue(Valid->SetExpressionQualType(
				HandleType, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsTrue(ValidContext.VerifyAndSeal(Valid, Result)));
		}

		TEST_METHOD(VerifierRejectsMalformedRecursiveTypeSyntax)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			AS_NAMESPACE_QUALIFIER asCASTContext CastContext(Source);
			auto* Type = CastContext.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			auto* Operand = CastContext.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(
				1, Range(*Source, 0, 1));
			Operand->SetExpressionSemantics(Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue);
			AS_NAMESPACE_QUALIFIER asSTypeSyntax ArgumentSyntax;
			ArgumentSyntax.NameParts = {TEXT("int")};
			ArgumentSyntax.ComponentRanges = {Range(*Source, 11, 14)};
			ArgumentSyntax.Range = Range(*Source, 11, 14);
			AS_NAMESPACE_QUALIFIER asSTypeSyntax CastSyntax;
			CastSyntax.NameParts = {TEXT("Box")};
			CastSyntax.ComponentRanges = {Range(*Source, 0, 3)};
			CastSyntax.GenericArguments = {MoveTemp(ArgumentSyntax)};
			CastSyntax.Range = Range(*Source, 0, 10);
			auto* Cast = CastContext.Create<AS_NAMESPACE_QUALIFIER asCExplicitCastExpr>(
				Operand, MoveTemp(CastSyntax), Range(*Source, 0, 14));
			Cast->SetExpressionSemantics(Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue);
			ASSERT_THAT(IsFalse(CastContext.VerifyAndSeal(Cast, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidPayload, Result.Status));
		}

		TEST_METHOD(ReferenceBindingCannotMasqueradeAsAnArbitraryImplicitCast)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Int = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			auto* Operand = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(
				1, Range(*Source, 0, 1));
			ASSERT_THAT(IsTrue(Operand->SetExpressionSemantics(
				Int, AS_NAMESPACE_QUALIFIER asEValueCategory::LValue)));
			AS_NAMESPACE_QUALIFIER asCQualType ReferenceType;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(
				Int, AS_NAMESPACE_QUALIFIER asETypeQualifiers::Reference, ReferenceType)));
			auto* Cast = Context.Create<AS_NAMESPACE_QUALIFIER asCImplicitCastExpr>(
				Operand, AS_NAMESPACE_QUALIFIER asEImplicitConversionKind::ReferenceBinding,
				Range(*Source, 0, 1));
			ASSERT_THAT(IsTrue(Cast->SetExpressionQualType(
				ReferenceType, AS_NAMESPACE_QUALIFIER asEValueCategory::LValue)));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Cast, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidPayload, Result.Status));
		}

		TEST_METHOD(VerifierRejectsDuplicateCallArgumentSourceOrdinals)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			AS_NAMESPACE_QUALIFIER asCASTContext CallContext(Source);
			auto* Int = CallContext.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asCQualType IntType;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(
				Int, AS_NAMESPACE_QUALIFIER asETypeQualifiers::None, IntType)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Signature;
			Signature.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Callable;
			Signature.ReturnType = Int->GetStableIdentity();
			Signature.Parameters = {
				{Int->GetStableIdentity(), AS_NAMESPACE_QUALIFIER asEParameterPassingMode::Value},
				{Int->GetStableIdentity(), AS_NAMESPACE_QUALIFIER asEParameterPassingMode::Value}};
			AS_NAMESPACE_QUALIFIER asSStableKey SignatureKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				CallContext.GetTypeContext()->GetIdentityRegistry().InternTypeUse(
					Signature, SignatureKey)));
			auto* FunctionType = CallContext.GetFunctionType(
				IntType, TArray<AS_NAMESPACE_QUALIFIER asCQualType>{IntType, IntType}, SignatureKey);
			auto* Function = CallContext.Create<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(
				TEXT("F"), Range(*Source, 0, 1));
			auto* Callee = CallContext.Create<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(
				Function, Range(*Source, 0, 1));
			Callee->SetExpressionSemantics(FunctionType, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue);
			auto* Call = CallContext.Create<AS_NAMESPACE_QUALIFIER asCCallExpr>(
				Callee, Range(*Source, 0, 15));
			for (int32 Offset : {2, 4})
			{
				auto* Argument = CallContext.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(
					Offset, Range(*Source, Offset, Offset + 1));
				Argument->SetExpressionSemantics(Int, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue);
				Call->AddArgument(Argument);
			}
			ASSERT_THAT(IsTrue(Call->SetArgumentSourceOrdinals({0, 0})));
			Call->SetExpressionSemantics(Int, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue);
			ASSERT_THAT(IsFalse(CallContext.VerifyAndSeal(Call, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidPayload, Result.Status));
		}

		TEST_METHOD(VerifierRejectsMissingCallArgumentSourceMapping)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Type = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asCQualType IntType;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(
				Type, AS_NAMESPACE_QUALIFIER asETypeQualifiers::None, IntType)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Signature;
			Signature.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Callable;
			Signature.ReturnType = Type->GetStableIdentity();
			Signature.Parameters = {{Type->GetStableIdentity(),
				AS_NAMESPACE_QUALIFIER asEParameterPassingMode::Value}};
			AS_NAMESPACE_QUALIFIER asSStableKey SignatureKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.GetTypeContext()->GetIdentityRegistry().InternTypeUse(
					Signature, SignatureKey)));
			auto* FunctionType = Context.GetFunctionType(
				IntType, TArray<AS_NAMESPACE_QUALIFIER asCQualType>{IntType}, SignatureKey);
			auto* Function = Context.Create<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(
				TEXT("F"), Range(*Source, 0, 1));
			auto* Callee = Context.Create<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(
				Function, Range(*Source, 0, 1));
			auto* Argument = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(
				1, Range(*Source, 2, 3));
			auto* Call = Context.Create<AS_NAMESPACE_QUALIFIER asCCallExpr>(
				Callee, Range(*Source, 0, 4));
			ASSERT_THAT(IsTrue(Callee->SetExpressionSemantics(
				FunctionType, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsTrue(Argument->SetExpressionSemantics(
				Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsTrue(Call->AddArgument(Argument)));
			ASSERT_THAT(IsTrue(Call->SetExpressionSemantics(
				Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Call, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidPayload, Result.Status));
		}

		TEST_METHOD(TypeAliasProjectsAuthoredSyntaxAndSemanticTargetWithoutOwningTheType)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Int = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asSTypeSyntax Syntax;
			Syntax.NameParts = {TEXT("int")};
			Syntax.ComponentRanges = {Range(*Source, 0, 3)};
			Syntax.Range = Range(*Source, 0, 3);
			auto* Alias = Context.Create<AS_NAMESPACE_QUALIFIER asCTypeAliasDecl>(
				TEXT("Count"), MoveTemp(Syntax), Range(*Source, 0, 10));
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = TEXT("ast-alias");
			Descriptor.Name = TEXT("Count");
			Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Typedef;
			AS_NAMESPACE_QUALIFIER asSStableKey AliasKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.GetTypeContext()->GetIdentityRegistry().InternNominalType(
					Descriptor, AliasKey)));
			ASSERT_THAT(IsTrue(Alias->SetStableIdentity(AliasKey)));
			ASSERT_THAT(IsTrue(Alias->SetResolvedAliasedType(Int)));
			ASSERT_THAT(IsTrue(Alias->SetAccess(AS_NAMESPACE_QUALIFIER asEAccessSpecifier::Private)));

			TArray<const AS_NAMESPACE_QUALIFIER asCASTNode*> Children;
			TArray<AS_NAMESPACE_QUALIFIER asSASTSemanticTypeReference> Semantic;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectOwningChildren(*Alias, Children)));
			ASSERT_THAT(AreEqual(0, Children.Num()));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectSemanticTypeReferences(*Alias, Semantic)));
			ASSERT_THAT(AreEqual(1, Semantic.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTSemanticTypeReferenceKind::AliasedType, Semantic[0].Kind));
			ASSERT_THAT(IsTrue(Semantic[0].Type.GetType() == Int));

			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verification;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Alias, Verification)));
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Alias, Projection, Error)));
			ASSERT_THAT(AreEqual(1, Projection.GetRecords().Num()));
			const auto& Record = Projection.GetRecords()[0];
			ASSERT_THAT(IsTrue(Record.Declaration.bPresent));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAccessSpecifier::Private, Record.Declaration.Access));
			const auto* Payload = Record.TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTTypeAliasDeclPayload>();
			ASSERT_THAT(IsNotNull(Payload));
			ASSERT_THAT(AreEqual(FString(TEXT("Count")), Payload->Name));
			ASSERT_THAT(AreEqual(FString(TEXT("int")), Payload->TypeSyntax.Spelling));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTSemanticTypeReferenceKind::AliasedType,
				Record.SemanticReferences[0].Kind));
			ASSERT_THAT(IsTrue(Record.SemanticReferences[0].TypeIdentity == Int->GetStableIdentity()));
		}

		TEST_METHOD(VerifierRejectsUnknownRecordModifierBits)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Record = Context.Create<AS_NAMESPACE_QUALIFIER asCRecordDecl>(
				TEXT("R"), Range(*Source, 0, 10));
			ASSERT_THAT(IsTrue(Record->SetModifiers(
				static_cast<AS_NAMESPACE_QUALIFIER asERecordModifiers>(0x80))));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Record, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidPayload, Result.Status));
		}

		TEST_METHOD(VerifierRejectsQualifiedTypeAliasTargets)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			AS_NAMESPACE_QUALIFIER asSTypeSyntax Syntax;
			Syntax.NameParts = {TEXT("int")};
			Syntax.ComponentRanges = {Range(*Source, 0, 3)};
			Syntax.Range = Range(*Source, 0, 3);
			Syntax.bConst = true;
			auto* Alias = Context.Create<AS_NAMESPACE_QUALIFIER asCTypeAliasDecl>(
				TEXT("Count"), MoveTemp(Syntax), Range(*Source, 0, 10));
			ASSERT_THAT(IsTrue(Alias->SetResolvedAliasedType(
				Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32))));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Alias, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidPayload, Result.Status));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
