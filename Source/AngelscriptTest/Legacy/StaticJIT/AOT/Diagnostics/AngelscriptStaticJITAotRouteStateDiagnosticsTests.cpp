#include "CQTest.h"

#include "Core/Artifacts/AngelscriptArtifactIdentity.h"
#include "StaticJIT/StaticJITDiagnostics.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS

namespace AngelscriptStaticJITAotRouteStateDiagnosticsTests_Private
{
	FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("static-jit-route-state-diagnostics-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	FStaticJITDiagnostics::FReferenceRecord& AddReference(
		FStaticJITDiagnostics::FProviderEntryRecord& Entry,
		const uint32 SlotIndex,
		const EAngelscriptJITReferenceResolutionCode Resolution,
		const FStaticJITDiagnostics::EReferenceAdoptionState Adoption)
	{
		FStaticJITDiagnostics::FReferenceRecord& Reference =
			Entry.References.AddDefaulted_GetRef();
		Reference.SlotIndex = SlotIndex;
		Reference.Reference.Kind =
			EAngelscriptArtifactReferenceKind::EnvironmentSymbol;
		Reference.Reference.StableKey = MakeHash(*FString::Printf(
			TEXT("Target-%u"), SlotIndex));
		Reference.Reference.ExpectedAbi = MakeHash(*FString::Printf(
			TEXT("Abi-%u"), SlotIndex));
		Reference.bResolved = Resolution
			== EAngelscriptJITReferenceResolutionCode::Exact;
		Reference.CurrentResolution = Resolution;
		Reference.AdoptionState = Adoption;
		Reference.CurrentTargetDeclaration = FString::Printf(
			TEXT("void Target%u()"), SlotIndex);
		Reference.CurrentTargetKind = TEXT("SystemFunction");
		return Reference;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotRouteStateDiagnosticTests,
	"Angelscript.TestModule.StaticJIT.AOT.Diagnostics.RouteState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ReferenceResolutionAndAdoptionVocabularyIsStable)
	{
		using namespace
			AngelscriptStaticJITAotRouteStateDiagnosticsTests_Private;

		FStaticJITDiagnostics::FSnapshot Snapshot;
		FStaticJITDiagnostics::FProviderRecord& Provider =
			Snapshot.Providers.AddDefaulted_GetRef();
		Provider.ProviderId.Hash = MakeHash(TEXT("Provider"));
		FStaticJITDiagnostics::FProviderEntryRecord& Entry =
			Provider.Entries.AddDefaulted_GetRef();
		Entry.FunctionKey.Hash = MakeHash(TEXT("Function"));

		AddReference(Entry, 0,
			EAngelscriptJITReferenceResolutionCode::Exact,
			FStaticJITDiagnostics::EReferenceAdoptionState::Bound);
		AddReference(Entry, 1,
			EAngelscriptJITReferenceResolutionCode::Missing,
			FStaticJITDiagnostics::EReferenceAdoptionState::Unbound);
		AddReference(Entry, 2,
			EAngelscriptJITReferenceResolutionCode::Exact,
			FStaticJITDiagnostics::EReferenceAdoptionState::Stale);
		AddReference(Entry, 3,
			EAngelscriptJITReferenceResolutionCode::AbiMismatch,
			FStaticJITDiagnostics::EReferenceAdoptionState::Rejected);

		const FStaticJITDiagnostics::FJsonResult Result =
			FStaticJITDiagnostics::Serialize(Snapshot);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), Result.Detail));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"currentResolution\":\"Exact\",\"adoptionState\":\"Bound\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"currentResolution\":\"Missing\",\"adoptionState\":\"Unbound\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"currentResolution\":\"Exact\",\"adoptionState\":\"Stale\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"currentResolution\":\"AbiMismatch\",\"adoptionState\":\"Rejected\""))));
		ASSERT_THAT(IsFalse(Result.Json.Contains(TEXT("0x"),
			ESearchCase::IgnoreCase)));
	}

	TEST_METHOD(CurrentExecutionRouteAndProviderMatchStaySeparate)
	{
		using namespace
			AngelscriptStaticJITAotRouteStateDiagnosticsTests_Private;

		FStaticJITDiagnostics::FSnapshot Snapshot;
		FStaticJITDiagnostics::FRouteRecord& NativeRoute =
			Snapshot.Routes.AddDefaulted_GetRef();
		NativeRoute.ModuleKey.Hash = MakeHash(TEXT("Module"));
		NativeRoute.Identity.FunctionKey.Hash = MakeHash(TEXT("NativeFunction"));
		NativeRoute.SelectedExecutionRoute =
			EAngelscriptFunctionExecutionRoute::Native;
		NativeRoute.MatchResult = EAngelscriptArtifactMatchResult::Exact;
		NativeRoute.ProviderId.Hash = MakeHash(TEXT("Provider"));
		NativeRoute.ProviderEntryIndex = 4;
		NativeRoute.VMExecutionCount = 2;
		NativeRoute.RawExecutionCount = 5;
		NativeRoute.ParmsExecutionCount = 7;

		FStaticJITDiagnostics::FRouteRecord& VmRoute =
			Snapshot.Routes.AddDefaulted_GetRef();
		VmRoute.ModuleKey = NativeRoute.ModuleKey;
		VmRoute.Identity.FunctionKey.Hash = MakeHash(TEXT("VmFunction"));
		VmRoute.SelectedExecutionRoute = EAngelscriptFunctionExecutionRoute::Vm;
		VmRoute.MatchResult =
			EAngelscriptArtifactMatchResult::EntryAbiMismatch;

		const FStaticJITDiagnostics::FJsonResult Result =
			FStaticJITDiagnostics::Serialize(Snapshot);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), Result.Detail));
		ASSERT_THAT(IsTrue(Result.Json.Contains(TEXT("\"selectedRoute\":\"Native\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(TEXT("\"matchResult\":\"Exact\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(TEXT("\"vmExecutionCount\":\"2\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(TEXT("\"rawExecutionCount\":\"5\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(TEXT("\"parmsExecutionCount\":\"7\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(TEXT("\"selectedRoute\":\"VM\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"matchResult\":\"EntryAbiMismatch\""))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS
