#include "CQTest.h"
#include "../NativeEngineTestSupport.h"

#include "as_module_definition_set.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/Sema/as_call_assessment.h"
#include "frontend/Sema/as_sema.h"
#include "frontend/AST/as_type_context.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	CallAssessment,
	"Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	static AS_NAMESPACE_QUALIFIER asSCallArgumentFact IntFact(const FString& Name, AS_NAMESPACE_QUALIFIER asCQualType Type)
	{
		AS_NAMESPACE_QUALIFIER asSCallArgumentFact Fact;
		Fact.Name = Name;
		Fact.QualType = Type;
		return Fact;
	}

	TEST_METHOD(CompleteAndIncompleteMapping)
	{
		FSessionRun Run({{TEXT("Pick.as"), TEXT(
			"int Pick(int First,int Second=2);\n"
			"int F() { return Pick(Second:4,First:3); }\n")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		auto* Pick = Run.Function(TEXT("Pick"));
		ASSERT_THAT(IsNotNull(Pick));
		ASSERT_THAT(AreEqual(2, Pick->GetParameters().Num()));
		const AS_NAMESPACE_QUALIFIER asCQualType IntType = Pick->GetParameters()[0]->GetResolvedQualType();
		ASSERT_THAT(IsTrue(IntType.IsValid()));

		AS_NAMESPACE_QUALIFIER asSCallAssessmentRequest Complete;
		Complete.ScriptCandidates.Add(Pick);
		Complete.Mode = AS_NAMESPACE_QUALIFIER asECallAssessmentMode::Complete;
		Complete.Arguments.Add(IntFact(TEXT("Second"), IntType));
		Complete.Arguments.Add(IntFact(TEXT("First"), IntType));
		const AS_NAMESPACE_QUALIFIER asSCallAssessment CompleteResult = Run.Session->GetSema().AssessCall(Complete);
		ASSERT_THAT(AreEqual(1, CompleteResult.Candidates.Num()));
		ASSERT_THAT(IsTrue(CompleteResult.Candidates[0].bViable));
		ASSERT_THAT(AreEqual(2, CompleteResult.Candidates[0].AuthoredToFormal.Num()));
		ASSERT_THAT(AreEqual(1, CompleteResult.Candidates[0].AuthoredToFormal[0]));
		ASSERT_THAT(AreEqual(0, CompleteResult.Candidates[0].AuthoredToFormal[1]));

		AS_NAMESPACE_QUALIFIER asSCallAssessmentRequest Incomplete;
		Incomplete.ScriptCandidates.Add(Pick);
		Incomplete.Mode = AS_NAMESPACE_QUALIFIER asECallAssessmentMode::Incomplete;
		Incomplete.Arguments.Add(IntFact(FString(), IntType));
		const AS_NAMESPACE_QUALIFIER asSCallAssessment IncompleteResult = Run.Session->GetSema().AssessCall(Incomplete);
		ASSERT_THAT(AreEqual(1, IncompleteResult.Candidates.Num()));
		ASSERT_THAT(IsTrue(IncompleteResult.Candidates[0].bViable));
		ASSERT_THAT(AreNotEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asECandidateRejection::MissingRequiredArgument),
			static_cast<int32>(IncompleteResult.Candidates[0].Rejection)));
		ASSERT_THAT(AreEqual(2, IncompleteResult.Candidates[0].FormalSlots.Num()));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEArgumentSlotKind::Supplied),
			static_cast<int32>(IncompleteResult.Candidates[0].FormalSlots[0])));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEArgumentSlotKind::UnknownFuture),
			static_cast<int32>(IncompleteResult.Candidates[0].FormalSlots[1])));

		AS_NAMESPACE_QUALIFIER asSCallAssessmentRequest Duplicate;
		Duplicate.ScriptCandidates.Add(Pick);
		Duplicate.Mode = AS_NAMESPACE_QUALIFIER asECallAssessmentMode::Incomplete;
		Duplicate.Arguments.Add(IntFact(TEXT("First"), IntType));
		Duplicate.Arguments.Add(IntFact(TEXT("First"), IntType));
		const AS_NAMESPACE_QUALIFIER asSCallAssessment DuplicateResult = Run.Session->GetSema().AssessCall(Duplicate);
		ASSERT_THAT(AreEqual(1, DuplicateResult.Candidates.Num()));
		ASSERT_THAT(IsFalse(DuplicateResult.Candidates[0].bViable));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asECandidateRejection::DuplicateNamedArgument),
			static_cast<int32>(DuplicateResult.Candidates[0].Rejection)));
	}

	TEST_METHOD(CandidateLocalFailureAndImmutability)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCModuleDefinitionSet> Set = MakeUnique<asCModuleDefinitionSet>();
		ASSERT_THAT(IsTrue(Set->SetIdentityContext(TypeContext) == asEMetadataResult::Succeeded));
		AS_NAMESPACE_QUALIFIER asSStableKey IntType;
		ASSERT_THAT(IsTrue(
			TypeContext->InternPrimitiveTypeUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, IntType)
			== AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded));

		asSFunctionIdentityDescriptor WideIdentity;
		WideIdentity.StableScope = TEXT("frozen-host");
		WideIdentity.NamespacePath = {TEXT("Host")};
		WideIdentity.Name = TEXT("Pick");
		WideIdentity.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global;
		WideIdentity.ReturnType = IntType;
		WideIdentity.Parameters.Add({IntType});
		WideIdentity.Parameters.Add({IntType});
		AS_NAMESPACE_QUALIFIER asSStableKey WideKey;
		asCScriptFunction* Wide = nullptr;
		asSMetadataParameter First;
		First.Name = "First";
		First.Type = Set->Primitive(ttInt);
		asSMetadataParameter Second;
		Second.Name = "Second";
		Second.Type = Set->Primitive(ttInt);
		Second.DefaultArgument = "2";
		ASSERT_THAT(IsTrue(
			TypeContext->GetIdentityRegistry().InternFunction(WideIdentity, WideKey)
			== AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded
			&& Set->CreateFunction(WideKey, "Pick", "Host", asFUNC_SCRIPT, Wide) == asEMetadataResult::Succeeded
			&& Set->DefineFunction(*Wide, Set->Primitive(ttInt), {First, Second}) == asEMetadataResult::Succeeded
			&& Set->SetFunctionDeclarationFacts(*Wide, AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global)
				== asEMetadataResult::Succeeded));

		asSFunctionIdentityDescriptor NarrowIdentity = WideIdentity;
		NarrowIdentity.Parameters.Reset();
		NarrowIdentity.Parameters.Add({IntType});
		AS_NAMESPACE_QUALIFIER asSStableKey NarrowKey;
		asCScriptFunction* Narrow = nullptr;
		asSMetadataParameter Value;
		Value.Name = "Value";
		Value.Type = Set->Primitive(ttInt);
		ASSERT_THAT(IsTrue(
			TypeContext->GetIdentityRegistry().InternFunction(NarrowIdentity, NarrowKey)
			== AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded
			&& Set->CreateFunction(NarrowKey, "Pick", "Host", asFUNC_SCRIPT, Narrow) == asEMetadataResult::Succeeded
			&& Set->DefineFunction(*Narrow, Set->Primitive(ttInt), {Value}) == asEMetadataResult::Succeeded
			&& Set->SetFunctionDeclarationFacts(*Narrow, AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global)
				== asEMetadataResult::Succeeded
			&& Set->FinalizeLayouts() == asEMetadataResult::Succeeded
			&& Set->Freeze() == asEMetadataResult::Succeeded
			&& Set->ValidateFrozenDefinitions()));

		FSessionRun Run(
			{{TEXT("Main.as"), TEXT("int F() { return Host::Pick(41); }")}},
			{},
			TypeContext);
		ASSERT_THAT(IsTrue(Run.Session->AddExternalDefinitions(Set.Get())));
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		AS_NAMESPACE_QUALIFIER asSExternalCallable WideCallable;
		WideCallable.Key = WideKey;
		WideCallable.Function = Wide;
		WideCallable.Set = Set.Get();
		WideCallable.QualifiedName = TEXT("Host::Pick");
		AS_NAMESPACE_QUALIFIER asSExternalCallable NarrowCallable;
		NarrowCallable.Key = NarrowKey;
		NarrowCallable.Function = Narrow;
		NarrowCallable.Set = Set.Get();
		NarrowCallable.QualifiedName = TEXT("Host::Pick");

		AS_NAMESPACE_QUALIFIER asCQualType ScriptInt;
		AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(
			Run.Session->GetASTContext().GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32),
			AS_NAMESPACE_QUALIFIER asETypeQualifiers::None,
			ScriptInt);
		ASSERT_THAT(IsTrue(ScriptInt.IsValid()));

		AS_NAMESPACE_QUALIFIER asSCallAssessmentRequest Request;
		Request.HostCandidates.Add(WideCallable);
		Request.HostCandidates.Add(NarrowCallable);
		Request.Mode = AS_NAMESPACE_QUALIFIER asECallAssessmentMode::Complete;
		Request.Arguments.Add(IntFact(FString(), ScriptInt));

		const int32 Nodes = Run.Session->GetASTContext().GetNodeCount();
		const FString Projection = Run.Session->SerializeStableProjection();
		const FString BodyProjection = Run.Session->SerializeStableBodyProjection();
		const asUINT Pending = Run.Diagnostics.GetPendingCount();
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Before =
			Run.Diagnostics.CaptureResult();

		const AS_NAMESPACE_QUALIFIER asSCallAssessment FirstAssessment = Run.Session->GetSema().AssessCall(Request);
		const AS_NAMESPACE_QUALIFIER asSCallAssessment SecondAssessment = Run.Session->GetSema().AssessCall(Request);
		ASSERT_THAT(AreEqual(2, FirstAssessment.Candidates.Num()));
		int32 Unsupported = 0;
		int32 Viable = 0;
		const AS_NAMESPACE_QUALIFIER asSExternalCallable* Chosen = nullptr;
		for (const AS_NAMESPACE_QUALIFIER asSCandidateAssessment& Candidate : FirstAssessment.Candidates)
		{
			if (Candidate.Rejection == AS_NAMESPACE_QUALIFIER asECandidateRejection::UnsupportedDefault)
			{
				++Unsupported;
				ASSERT_THAT(IsFalse(Candidate.bViable));
			}
			if (Candidate.bViable)
			{
				++Viable;
				Chosen = Candidate.HostCallable.Function ? &Candidate.HostCallable : nullptr;
			}
		}
		ASSERT_THAT(AreEqual(1, Unsupported));
		ASSERT_THAT(AreEqual(1, Viable));
		ASSERT_THAT(IsNotNull(Chosen));
		ASSERT_THAT(IsTrue(Chosen->Function == Narrow));
		ASSERT_THAT(AreEqual(FirstAssessment.SelectedIndex, SecondAssessment.SelectedIndex));
		ASSERT_THAT(AreEqual(Nodes, Run.Session->GetASTContext().GetNodeCount()));
		ASSERT_THAT(AreEqual(Projection, Run.Session->SerializeStableProjection()));
		ASSERT_THAT(AreEqual(BodyProjection, Run.Session->SerializeStableBodyProjection()));
		ASSERT_THAT(AreEqual(Pending, Run.Diagnostics.GetPendingCount()));
		ASSERT_THAT(AreEqual(Before->GetGroups().Num(), Run.Diagnostics.CaptureResult()->GetGroups().Num()));

		ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		auto* Function = Run.Function(TEXT("F"));
		ASSERT_THAT(IsNotNull(Function));
		auto* Body = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Function->GetBody());
		ASSERT_THAT(IsNotNull(Body));
		ASSERT_THAT(AreEqual(1, Body->GetStatements().Num()));
		auto* Return = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0]);
		ASSERT_THAT(IsNotNull(Return));
		auto* Call = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Return->GetValue());
		ASSERT_THAT(IsNotNull(Call));
		auto* Callee = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Call->GetCallee());
		ASSERT_THAT(IsNotNull(Callee));
		ASSERT_THAT(IsTrue(Callee->GetExternalFunction() == Narrow));
	}
};

#endif
