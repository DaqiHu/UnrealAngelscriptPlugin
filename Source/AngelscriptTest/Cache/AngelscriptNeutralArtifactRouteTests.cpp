#include "Artifacts/AngelscriptArtifactReference.h"
#include "Artifacts/AngelscriptFunctionRoute.h"

#include "CQTest.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptNeutralArtifactRouteTests,
	"Angelscript.TestModule.Cache.NeutralArtifacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptCompatibilityDescriptor Descriptor;
		Descriptor.CanonicalInputs = {Value};
		return FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
			Descriptor).Hash;
	}

	static_assert(sizeof(EAngelscriptArtifactReferenceKind) == sizeof(uint8));
	static_assert(sizeof(EAngelscriptFunctionExecutionRoute) == sizeof(uint8));
	static_assert(sizeof(EAngelscriptArtifactMatchResult) == sizeof(uint8));
	static_assert(sizeof(FAngelscriptFunctionRouteGeneration) == sizeof(uint64));
	static_assert(std::is_standard_layout_v<FAngelscriptArtifactReference>);
	static_assert(std::is_same_v<
		decltype(FAngelscriptFunctionRouteSnapshot::Generation),
		FAngelscriptFunctionRouteGeneration>);

public:
	TEST_METHOD(StableReferenceValueHasDeterministicEqualityAndOrdering)
	{
		FAngelscriptArtifactReference FunctionReference;
		FunctionReference.Kind = EAngelscriptArtifactReferenceKind::ScriptFunction;
		FunctionReference.StableKey = MakeHash(TEXT("Function"));
		FunctionReference.ExpectedAbi = MakeHash(TEXT("Abi"));

		FAngelscriptArtifactReference SameReference = FunctionReference;
		ASSERT_THAT(IsTrue(FunctionReference == SameReference));
		ASSERT_THAT(IsFalse(FunctionReference != SameReference));

		FAngelscriptArtifactReference TypeReference = FunctionReference;
		TypeReference.Kind = EAngelscriptArtifactReferenceKind::ScriptType;
		ASSERT_THAT(IsTrue(TypeReference < FunctionReference,
			TEXT("Reference ordering should compare the frozen kind before hashes")));

		FAngelscriptArtifactReference OtherFunction = FunctionReference;
		OtherFunction.StableKey = MakeHash(TEXT("OtherFunction"));
		ASSERT_THAT(IsTrue(
			FunctionReference < OtherFunction || OtherFunction < FunctionReference,
			TEXT("Distinct full stable keys should have a deterministic order")));
	}

	TEST_METHOD(RouteGenerationAndTypedMismatchRemainValueState)
	{
		const FAngelscriptFunctionRouteGeneration First{41};
		const FAngelscriptFunctionRouteGeneration Second{42};
		ASSERT_THAT(IsTrue(First.IsValid()));
		ASSERT_THAT(IsTrue(First < Second));
		ASSERT_THAT(IsTrue(First != Second));

		FAngelscriptFunctionRoute VmRoute;
		VmRoute.Generation = Second;
		VmRoute.SelectedExecutionRoute = EAngelscriptFunctionExecutionRoute::Vm;
		VmRoute.MatchResult = EAngelscriptArtifactMatchResult::ContentMismatch;
		ASSERT_THAT(IsFalse(VmRoute.IsNative()));

		FAngelscriptFunctionRoute NativeRoute = VmRoute;
		NativeRoute.SelectedExecutionRoute = EAngelscriptFunctionExecutionRoute::Native;
		NativeRoute.MatchResult = EAngelscriptArtifactMatchResult::Exact;
		ASSERT_THAT(IsTrue(NativeRoute.IsNative()));

		FAngelscriptFunctionRouteSnapshot Snapshot;
		Snapshot.Generation = Second;
		Snapshot.FunctionRoutes.Add(NativeRoute);
		ASSERT_THAT(IsTrue(Snapshot.Generation == Second));
		ASSERT_THAT(AreEqual(1, Snapshot.FunctionRoutes.Num()));
		ASSERT_THAT(IsTrue(Snapshot.FunctionRoutes[0].Generation == Second));
	}

	TEST_METHOD(MismatchReasonsCoverSharedCacheAndProviderDecisions)
	{
		ASSERT_THAT(IsTrue(
			EAngelscriptArtifactMatchResult::MissingArtifactIdentity
				!= EAngelscriptArtifactMatchResult::ContentMismatch));
		ASSERT_THAT(IsTrue(
			EAngelscriptArtifactMatchResult::ProfileMismatch
				!= EAngelscriptArtifactMatchResult::EnvironmentMismatch));
		ASSERT_THAT(IsTrue(
			EAngelscriptArtifactMatchResult::ReferenceAbiMismatch
				!= EAngelscriptArtifactMatchResult::AmbiguousExactProvider));
		ASSERT_THAT(IsTrue(
			EAngelscriptArtifactMatchResult::ArtifactSetMismatch
				!= EAngelscriptArtifactMatchResult::StaleProviderGeneration));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
