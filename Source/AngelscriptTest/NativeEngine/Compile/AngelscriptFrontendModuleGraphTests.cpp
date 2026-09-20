#include "CQTest.h"

#include "frontend/Compile/as_dependency_graph.h"
#include "frontend/Sema/as_type_identity.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptFrontendModuleGraphTests
{
	AS_NAMESPACE_QUALIFIER asCSourceRange Range(uint32 Begin, uint32 End)
	{
		return {
			AS_NAMESPACE_QUALIFIER asCSourceLocation(1, AS_NAMESPACE_QUALIFIER asCSourceFileID(1), Begin),
			AS_NAMESPACE_QUALIFIER asCSourceLocation(1, AS_NAMESPACE_QUALIFIER asCSourceFileID(1), End)};
	}

	FAngelscriptResolvedSourceAnchor Anchor(const TCHAR* Source, uint32 Begin, uint32 End)
	{
		return {Source, 1, Begin, End};
	}

	AS_NAMESPACE_QUALIFIER FAngelscriptDependencyEdge Edge(
		const TCHAR* Source,
		const TCHAR* Target,
		AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason Reason,
		uint32 Begin,
		uint32 End,
		AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement Completeness)
	{
		AS_NAMESPACE_QUALIFIER FAngelscriptDependencyEdge Result;
		Result.SourceModuleKey = Source;
		Result.TargetModuleKey = Target;
		Result.Reason = Reason;
		Result.SourceRange = Range(Begin, End);
		Result.StableSource = Anchor(TEXT("Graph.as"), Begin, End);
		Result.Completeness = Completeness;
		return Result;
	}

	TEST_CLASS_WITH_FLAGS(ModuleGraph, "Angelscript.UnitTest.NativeEngine.Compile", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(TypedEdgesRetainReasonRangeAndCompleteness)
		{
			AS_NAMESPACE_QUALIFIER FAngelscriptModuleDependencyGraph Graph;
			Graph.AddModule(TEXT("Game.Consumer"));
			Graph.AddModule(TEXT("Game.Provider"));
			Graph.AddEdge(Edge(TEXT("Game.Consumer"), TEXT("Game.Provider"), AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason::PropertyType, 10, 18, AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement::CompleteDefinition));
			ASSERT_THAT(IsTrue(Graph.Finalize()));
			ASSERT_THAT(AreEqual(1, Graph.GetEdges().Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason::PropertyType, Graph.GetEdges()[0].Reason));
			ASSERT_THAT(AreEqual(10u, Graph.GetEdges()[0].SourceRange.Begin.ByteOffset));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement::CompleteDefinition, Graph.GetEdges()[0].Completeness));
		}

		TEST_METHOD(CyclesProduceDeterministicStrongComponentsAndCondensation)
		{
			AS_NAMESPACE_QUALIFIER FAngelscriptModuleDependencyGraph Graph;
			for (const TCHAR* Module : {TEXT("Game.C"), TEXT("Game.B"), TEXT("Game.A")}) Graph.AddModule(Module);
			Graph.AddEdge(Edge(TEXT("Game.A"), TEXT("Game.B"), AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason::Base, 1, 2, AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement::CompleteDefinition));
			Graph.AddEdge(Edge(TEXT("Game.B"), TEXT("Game.A"), AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason::ReturnType, 3, 4, AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement::DeclarationOnly));
			Graph.AddEdge(Edge(TEXT("Game.C"), TEXT("Game.A"), AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason::ParameterType, 5, 6, AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement::DeclarationOnly));
			ASSERT_THAT(IsTrue(Graph.Finalize()));
			ASSERT_THAT(AreEqual(2, Graph.GetComponents().Num()));
			ASSERT_THAT(AreEqual(2, Graph.GetComponents()[1].Members.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("Game.A")), Graph.GetComponents()[1].Members[0]));
			ASSERT_THAT(AreEqual(FString(TEXT("Game.B")), Graph.GetComponents()[1].Members[1]));
			ASSERT_THAT(AreEqual(1, Graph.GetCondensationEdges().Num()));
		}

		TEST_METHOD(DistinctUseSitesRemainButExactTuplesAreDeduplicated)
		{
			AS_NAMESPACE_QUALIFIER FAngelscriptModuleDependencyGraph Graph;
			Graph.AddModule(TEXT("A")); Graph.AddModule(TEXT("B"));
			const auto First = Edge(TEXT("A"), TEXT("B"), AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason::PropertyType, 10, 12, AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement::CompleteDefinition);
			const auto Second = Edge(TEXT("A"), TEXT("B"), AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason::PropertyType, 20, 22, AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement::CompleteDefinition);
			Graph.AddEdge(First); Graph.AddEdge(First); Graph.AddEdge(Second);
			ASSERT_THAT(IsTrue(Graph.Finalize()));
			ASSERT_THAT(AreEqual(2, Graph.GetEdges().Num()));
		}

		TEST_METHOD(SameModuleReferencesDoNotCreateCrossModuleEdges)
		{
			AS_NAMESPACE_QUALIFIER FAngelscriptModuleDependencyGraph Graph;
			Graph.AddModule(TEXT("A"));
			ASSERT_THAT(IsFalse(Graph.AddEdge(Edge(TEXT("A"), TEXT("A"), AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason::ReturnType, 1, 2, AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement::DeclarationOnly))));
			ASSERT_THAT(IsTrue(Graph.Finalize()));
			ASSERT_THAT(AreEqual(0, Graph.GetEdges().Num()));
		}

		TEST_METHOD(BodyReferencesCannotChangeDeclarationComponents)
		{
			AS_NAMESPACE_QUALIFIER FAngelscriptModuleDependencyGraph Declarations;
			Declarations.AddModule(TEXT("A")); Declarations.AddModule(TEXT("B"));
			Declarations.Finalize();
			const FString Before = Declarations.SerializeStable();

			AS_NAMESPACE_QUALIFIER FAngelscriptBodyInvalidationGraph Bodies;
			AS_NAMESPACE_QUALIFIER FAngelscriptBodyInvalidationFact Fact;
			Fact.SourceModuleKey = TEXT("A"); Fact.TargetModuleKey = TEXT("B");
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor ReturnType;
			ReturnType.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
			ReturnType.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Void;
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Function;
			Function.StableScope = TEXT("game"); Function.Name = TEXT("Tick"); Function.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.InternTypeUse(ReturnType, Function.ReturnType)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.InternFunction(Function, Fact.StableFunctionKey)));
			Fact.Reason = AS_NAMESPACE_QUALIFIER EAngelscriptBodyReferenceReason::Call;
			Fact.SourceRange = Range(30, 34); Fact.StableSource = Anchor(TEXT("A.as"), 30, 34);
			ASSERT_THAT(IsTrue(Bodies.AddFact(MoveTemp(Fact))));
			ASSERT_THAT(IsTrue(Bodies.Finalize()));
			ASSERT_THAT(AreEqual(1, Bodies.GetFacts().Num()));
			ASSERT_THAT(AreEqual(Before, Declarations.SerializeStable()));
		}

		TEST_METHOD(PermutedInputsSerializeIdentically)
		{
			AS_NAMESPACE_QUALIFIER FAngelscriptModuleDependencyGraph First;
			AS_NAMESPACE_QUALIFIER FAngelscriptModuleDependencyGraph Second;
			First.AddModule(TEXT("B")); First.AddModule(TEXT("A"));
			Second.AddModule(TEXT("A")); Second.AddModule(TEXT("B"));
			const auto One = Edge(TEXT("A"), TEXT("B"), AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason::ReturnType, 2, 3, AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement::DeclarationOnly);
			const auto Two = Edge(TEXT("A"), TEXT("B"), AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason::PropertyType, 8, 9, AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement::CompleteDefinition);
			First.AddEdge(Two); First.AddEdge(One);
			Second.AddEdge(One); Second.AddEdge(Two);
			ASSERT_THAT(IsTrue(First.Finalize() && Second.Finalize()));
			ASSERT_THAT(AreEqual(First.SerializeStable(), Second.SerializeStable()));
		}
	};
}

#endif
