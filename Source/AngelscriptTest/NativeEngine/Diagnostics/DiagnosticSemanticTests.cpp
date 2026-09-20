#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"

#include "as_definitions.h"
#include "as_objecttype.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/Basic/as_source_edit.h"
#include "frontend/Sema/as_call_assessment.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DiagnosticsSemantic,
	"Angelscript.UnitTest.NativeEngine.Diagnostics",
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

	TEST_METHOD(ViableTypoFix)
	{
		const FString Source = TEXT("int F(){ int Count=1; return Cout; }");
		FSessionRun Run({{TEXT("Typo.as"), Source}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Result =
			Run.Diagnostics.CaptureResult();
		const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* Group = FindGroup(
			*Result,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnresolvedReference));
		ASSERT_THAT(IsNotNull(Group));
		ASSERT_THAT(AreEqual(1, Group->Primary.Arguments.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("Cout")), Group->Primary.Arguments[0].TextValue));
		const int32 Cout = Source.Find(TEXT("Cout"));
		ASSERT_THAT(IsTrue(Cout != INDEX_NONE));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(Cout), Group->Primary.PrimaryRange.Begin.ByteOffset));
		ASSERT_THAT(AreEqual(1, Group->Notes.Num()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::DidYouMean),
			Group->Notes[0].DiagnosticID));
		ASSERT_THAT(AreEqual(FString(TEXT("Count")), Group->Notes[0].Arguments[0].TextValue));
		ASSERT_THAT(AreEqual(1, Group->Fixes.Num()));
		ASSERT_THAT(AreEqual(1, Group->Fixes[0].SourceEdits.Num()));
		const TArray<uint8>& Replacement = Group->Fixes[0].SourceEdits[0].ReplacementUtf8;
		ASSERT_THAT(AreEqual(5, Replacement.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>('C'), Replacement[0]));
		ASSERT_THAT(AreEqual(static_cast<uint8>('t'), Replacement[4]));

		FSessionRun Ambiguous({{TEXT("Both.as"), TEXT("int F(){ int Count=1; int Court=1; return Cout; }")}});
		ASSERT_THAT(IsTrue(Ambiguous.ResolveDeclarations()));
		ASSERT_THAT(IsFalse(Ambiguous.Session->AnalyzeBodies(1)));
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> AmbiguousResult =
			Ambiguous.Diagnostics.CaptureResult();
		const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* AmbiguousGroup = FindGroup(
			*AmbiguousResult,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnresolvedReference));
		ASSERT_THAT(IsNotNull(AmbiguousGroup));
		ASSERT_THAT(AreEqual(2, AmbiguousGroup->Notes.Num()));
		ASSERT_THAT(AreEqual(0, AmbiguousGroup->Fixes.Num()));
	}

	TEST_METHOD(DistinctSemanticCauses)
	{
		FSessionRun Missing({{TEXT("Missing.as"), TEXT("Missing F(){ return 1; }")}});
		ASSERT_THAT(IsTrue(Missing.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Missing.Session->ResolveDeclarations(1)));
		ASSERT_THAT(IsNotNull(FindGroup(
			*Missing.Diagnostics.CaptureResult(),
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnknownType))));

		FSessionRun Divide({{TEXT("Enum.as"), TEXT("enum A { Value = 1/0 }")}});
		ASSERT_THAT(IsTrue(Divide.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Divide.Session->ResolveDeclarations(1)));
		ASSERT_THAT(IsNotNull(FindGroup(
			*Divide.Diagnostics.CaptureResult(),
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::EnumConstantDivideByZero))));

		FSessionRun Shift({{TEXT("Shift.as"), TEXT("enum Shifted { Value = 1 << 99 }")}});
		ASSERT_THAT(IsTrue(Shift.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Shift.Session->ResolveDeclarations(1)));
		ASSERT_THAT(IsNotNull(FindGroup(
			*Shift.Diagnostics.CaptureResult(),
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::EnumConstantInvalidShift))));

		FSessionRun Duplicates({
			{TEXT("A.as"), TEXT("int Shared;")},
			{TEXT("B.as"), TEXT("int Shared;")},
		});
		ASSERT_THAT(IsTrue(Duplicates.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Duplicates.Session->ResolveDeclarations(1)));
		const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* Duplicate = FindGroup(
			*Duplicates.Diagnostics.CaptureResult(),
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::DuplicateDeclaration));
		ASSERT_THAT(IsNotNull(Duplicate));
		ASSERT_THAT(AreEqual(1, Duplicate->Primary.RelatedRanges.Num()));
	}

	TEST_METHOD(AcceptedSemanticBoundaries)
	{
		FSessionRun Qualifier({{TEXT("Qual.as"), TEXT("int@ F(){ return nullptr; }")}});
		ASSERT_THAT(IsTrue(Qualifier.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Qualifier.Session->ResolveDeclarations(1)));
		ASSERT_THAT(IsNotNull(FindGroup(
			*Qualifier.Diagnostics.CaptureResult(),
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::InvalidQualifier))));

		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Types =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCDefinitions> Set = MakeUnique<asCDefinitions>();
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = TEXT("semantic-host");
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct;
		Descriptor.Name = TEXT("TArray");
		Descriptor.GenericArity = 1;
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		asCObjectType* Type = nullptr;
		ASSERT_THAT(IsTrue(Set->SetIdentityContext(Types) == asEDefinitionResult::Succeeded
			&& Types->GetIdentityRegistry().InternNominalType(Descriptor, Key)
				== AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded
			&& Set->CreateObjectType(Key, "TArray", "", asOBJ_VALUE | asOBJ_TEMPLATE, Type) == asEDefinitionResult::Succeeded
			&& Set->SetTypeDeclarationFacts(*Type, AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct, 1) == asEDefinitionResult::Succeeded
			&& Set->SetFixedTemplateLayout(*Type, 16, 8) == asEDefinitionResult::Succeeded
			&& Set->FinalizeLayouts() == asEDefinitionResult::Succeeded
			&& Set->Freeze() == asEDefinitionResult::Succeeded));
		FSessionRun Arity({{TEXT("Arity.as"), TEXT("TArray<int, bool> X;")}}, {}, Types);
		ASSERT_THAT(IsTrue(Arity.Session->AddExternalDefinitions(Set.Get())));
		ASSERT_THAT(IsTrue(Arity.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Arity.Session->ResolveDeclarations(1)));
		ASSERT_THAT(IsNotNull(FindGroup(
			*Arity.Diagnostics.CaptureResult(),
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::GenericArity))));

		FSessionRun Conversion({{TEXT("Conv.as"), TEXT("int F(){ return true; }")}});
		ASSERT_THAT(IsTrue(Conversion.ResolveDeclarations()));
		ASSERT_THAT(IsFalse(Conversion.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsNotNull(FindGroup(
			*Conversion.Diagnostics.CaptureResult(),
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::IncompatibleReturnValue))));

		FSessionRun Pick({{TEXT("Pick.as"), TEXT(
			"int Pick(int First,int Second=2);\n"
			"int F(){ return Pick(Second:4,First:3); }\n")}});
		ASSERT_THAT(IsTrue(Pick.ResolveDeclarations()));
		auto* Function = Pick.Function(TEXT("Pick"));
		ASSERT_THAT(IsNotNull(Function));
		const AS_NAMESPACE_QUALIFIER asCQualType IntType = Function->GetParameters()[0]->GetResolvedQualType();
		AS_NAMESPACE_QUALIFIER asSCallAssessmentRequest Request;
		Request.ScriptCandidates.Add(Function);
		Request.Mode = AS_NAMESPACE_QUALIFIER asECallAssessmentMode::Complete;
		AS_NAMESPACE_QUALIFIER asSCallArgumentFact Second;
		Second.Name = TEXT("Second");
		Second.QualType = IntType;
		AS_NAMESPACE_QUALIFIER asSCallArgumentFact First;
		First.Name = TEXT("First");
		First.QualType = IntType;
		Request.Arguments.Add(Second);
		Request.Arguments.Add(First);
		const int32 Nodes = Pick.Session->GetASTContext().GetNodeCount();
		const AS_NAMESPACE_QUALIFIER asSCallAssessment FirstAssessment = Pick.Session->GetSema().AssessCall(Request);
		const AS_NAMESPACE_QUALIFIER asSCallAssessment SecondAssessment = Pick.Session->GetSema().AssessCall(Request);
		ASSERT_THAT(AreEqual(1, FirstAssessment.Candidates.Num()));
		ASSERT_THAT(AreEqual(FirstAssessment.Candidates[0].bViable, SecondAssessment.Candidates[0].bViable));
		ASSERT_THAT(AreEqual(Nodes, Pick.Session->GetASTContext().GetNodeCount()));
		ASSERT_THAT(IsTrue(Pick.Session->AnalyzeBodies(1)));

		FSessionRun Fallthrough({{TEXT("Fall.as"), TEXT(
			"int F(int X){ switch(X){ case 1: X=1; case 2: return X; } return 0; }")}});
		ASSERT_THAT(IsTrue(Fallthrough.ResolveDeclarations()));
		ASSERT_THAT(IsTrue(Fallthrough.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsNotNull(FindGroup(
			*Fallthrough.Diagnostics.CaptureResult(),
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::ImplicitCaseFallthrough))));
	}
};

#endif
