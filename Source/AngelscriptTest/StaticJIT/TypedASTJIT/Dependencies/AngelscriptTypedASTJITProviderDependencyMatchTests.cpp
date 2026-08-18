#include "StaticJIT/TypedASTJIT/Dependencies/AngelscriptTypedASTJITProviderDependencyTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptTypedASTJITProviderDependencyTestSupport;

namespace
{
	static const TCHAR* SharedScript = TEXT(R"AS(
enum EFoldedKind
{
	Seven = 7,
}

int Helper()
{
	return 1;
}

int RootA()
{
	return Helper() + 10;
}

int RootB()
{
	return Helper() + 20;
}

int Unrelated()
{
	return 30;
}

int DynamicRouted()
{
	return Helper() + 40;
}

int HelperC()
{
	return 2;
}

int HelperB()
{
	return HelperC() + 3;
}

int TransitiveRoot()
{
	return HelperB() + 4;
}

int Factorial(int Value)
{
	return Value <= 1 ? 1 : Value * Factorial(Value - 1);
}

int UseFolded()
{
	return 7;
}
)AS");
}

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITProviderDependencyMatchTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.Dependencies.ProviderMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(FunctionContentMatchesCurrentHelperExecution)
	{
		FScopedProjectRoot Project(TEXT("TypedASTJITDependencyMatch"));
		ASSERT_THAT(IsTrue(Project.WriteSource(SharedScript)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		if (!Routes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const Root =
			FindRoute(*Routes, TEXT("int RootA()"));
		const FAngelscriptFunctionRoute* const Helper =
			FindRoute(*Routes, TEXT("int Helper()"));
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Helper));
		if (Root == nullptr || Helper == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(Root->Identity.FunctionKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(Root->bHasVerifiedArtifactIdentity));
		FDependencyProvider Provider(*Routes, TEXT("match-exact"));
		ASSERT_THAT(IsFalse(Provider.Entries.IsEmpty()));
		Provider.SetEntryDependencies(
			Root->Identity.FunctionKey,
			{MakeFunctionContent(*Helper)});
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};

		const FAngelscriptJITProviderRouteRefreshResult Refresh =
			FAngelscriptJITProviderRouter::Refresh(*Engine);
		ASSERT_THAT(IsTrue(Refresh.IsApplied()));
		const FAngelscriptJITProviderRouteRecord* const Record =
			FindRecord(Refresh, Root->Identity.FunctionKey);
		ASSERT_THAT(IsNotNull(Record));
		if (Record == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			Record->MatchResult == EAngelscriptArtifactMatchResult::Exact));
		ASSERT_THAT(IsTrue(Record->bBindingPublished));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Published = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Published.IsValid()));
		if (!Published.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const PublishedRoot =
			FindRoute(*Published, TEXT("int RootA()"));
		ASSERT_THAT(IsNotNull(PublishedRoot));
		if (PublishedRoot == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(IsNativeProviderBinding(*PublishedRoot)));
	}

	TEST_METHOD(ChangedFunctionContentIsSemanticDependencyMismatch)
	{
		FScopedProjectRoot Project(TEXT("TypedASTJITDependencyMatch"));
		ASSERT_THAT(IsTrue(Project.WriteSource(SharedScript)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		if (!Routes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const Root =
			FindRoute(*Routes, TEXT("int RootA()"));
		const FAngelscriptFunctionRoute* const Helper =
			FindRoute(*Routes, TEXT("int Helper()"));
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Helper));
		if (Root == nullptr || Helper == nullptr)
		{
			return;
		}

		FAngelscriptArtifactSemanticDependency Stale = MakeFunctionContent(*Helper);
		Stale.ExpectedContentOrValue = MakeHash(TEXT("stale-helper-execution"));
		FDependencyProvider Provider(*Routes, TEXT("match-changed"));
		Provider.SetEntryDependencies(Root->Identity.FunctionKey, {Stale});
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};

		const FAngelscriptJITProviderRouteRefreshResult Refresh =
			FAngelscriptJITProviderRouter::Refresh(*Engine);
		ASSERT_THAT(IsTrue(Refresh.IsApplied()));
		const FAngelscriptJITProviderRouteRecord* const Record =
			FindRecord(Refresh, Root->Identity.FunctionKey);
		ASSERT_THAT(IsNotNull(Record));
		if (Record == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Record->MatchResult
			== EAngelscriptArtifactMatchResult::SemanticDependencyMismatch));
		ASSERT_THAT(IsFalse(Record->bBindingPublished));
	}

	TEST_METHOD(MissingWrongKindWrongAbiAndAmbiguousRowsAreSemanticDependencyMismatch)
	{
		FScopedProjectRoot Project(TEXT("TypedASTJITDependencyMatch"));
		ASSERT_THAT(IsTrue(Project.WriteSource(SharedScript)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		if (!Routes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const Root =
			FindRoute(*Routes, TEXT("int RootA()"));
		const FAngelscriptFunctionRoute* const Helper =
			FindRoute(*Routes, TEXT("int Helper()"));
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Helper));
		if (Root == nullptr || Helper == nullptr)
		{
			return;
		}

		auto ExpectMismatch = [this, &Engine, Root, Routes](
			const TCHAR* Seed,
			TArray<FAngelscriptArtifactSemanticDependency> Rows,
			const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
				ESPMode::ThreadSafe>& CurrentRoutes,
			const bool bExpectRefreshMismatch)
		{
			FDependencyProvider Provider(*Routes, Seed);
			Provider.SetEntryDependencies(Root->Identity.FunctionKey, MoveTemp(Rows));
			FAngelscriptJITProviderRegistry& Registry =
				FAngelscriptJITProviderRegistry::Get();
			ASSERT_THAT(IsTrue(
				Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
			ON_SCOPE_EXIT
			{
				Registry.UnregisterOwner(&Provider);
			};

			FExactResolver Resolver;
			FAngelscriptJITFunctionMatchInput Input = MakeMatchInput(*Root);
			Input.CurrentRoutes = CurrentRoutes;
			const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
				ESPMode::ThreadSafe> Snapshot = Registry.GetSnapshot();
			ASSERT_THAT(IsTrue(Snapshot.IsValid()));
			if (!Snapshot.IsValid())
			{
				return;
			}
			const FAngelscriptJITFunctionMatchResult Match =
				FAngelscriptJITProviderMatcher::Match(
					*Snapshot, Input, Resolver);
			ASSERT_THAT(IsTrue(Match.Result
				== EAngelscriptArtifactMatchResult::SemanticDependencyMismatch,
				FString::Printf(TEXT("%s should mismatch"), Seed)));

			if (!bExpectRefreshMismatch)
			{
				return;
			}
			const FAngelscriptJITProviderRouteRefreshResult Refresh =
				FAngelscriptJITProviderRouter::Refresh(*Engine);
			ASSERT_THAT(IsTrue(Refresh.IsApplied()));
			const FAngelscriptJITProviderRouteRecord* const Record =
				FindRecord(Refresh, Root->Identity.FunctionKey);
			ASSERT_THAT(IsNotNull(Record));
			if (Record != nullptr)
			{
				ASSERT_THAT(IsTrue(Record->MatchResult
					== EAngelscriptArtifactMatchResult::
						SemanticDependencyMismatch));
			}
		};

		ExpectMismatch(
			TEXT("match-missing"),
			{MakeDependency(
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
				EAngelscriptArtifactReferenceKind::ScriptFunction,
				MakeHash(TEXT("missing-helper")),
				MakeEntryAbi(*Helper),
				MakeHash(TEXT("missing-exec")))},
			Routes,
			true);

		FAngelscriptArtifactSemanticDependency WrongKind =
			MakeFunctionContent(*Helper);
		WrongKind.Target.Kind = EAngelscriptArtifactReferenceKind::ScriptGlobal;
		ExpectMismatch(TEXT("match-wrong-kind"), {WrongKind}, Routes, true);

		FAngelscriptArtifactSemanticDependency WrongAbi =
			MakeFunctionContent(*Helper);
		WrongAbi.Target.ExpectedAbi = MakeHash(TEXT("wrong-helper-abi"));
		ExpectMismatch(TEXT("match-wrong-abi"), {WrongAbi}, Routes, true);

		TSharedRef<FAngelscriptFunctionRouteSnapshot, ESPMode::ThreadSafe>
			AmbiguousRoutes = MakeShared<FAngelscriptFunctionRouteSnapshot,
				ESPMode::ThreadSafe>(*Routes);
		AmbiguousRoutes->FunctionRoutes.Add(*Helper);
		ExpectMismatch(
			TEXT("match-ambiguous"),
			{MakeFunctionContent(*Helper)},
			AmbiguousRoutes,
			false);
	}

	TEST_METHOD(HardValueLayoutAndStorageAuthorityAreValidated)
	{
		FScopedProjectRoot Project(TEXT("TypedASTJITDependencyMatch"));
		ASSERT_THAT(IsTrue(Project.WriteSource(SharedScript)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		if (!Routes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const UseFolded =
			FindRoute(*Routes, TEXT("int UseFolded()"));
		ASSERT_THAT(IsNotNull(UseFolded));
		if (UseFolded == nullptr)
		{
			return;
		}

		auto RefreshKind = [this, &Engine, Routes, UseFolded](
			const TCHAR* Seed,
			const EAngelscriptArtifactSemanticDependencyKind Kind,
			const bool bUseCurrentValue) -> EAngelscriptArtifactMatchResult
		{
			const FAngelscriptArtifactSemanticDependency* Current =
				FindSemanticDependency(*Routes, Kind);
			FAngelscriptArtifactSemanticDependency Row;
			if (Current != nullptr)
			{
				Row = *Current;
				if (!bUseCurrentValue)
				{
					Row.ExpectedContentOrValue =
						MakeHash(TEXT("stale-semantic-value"));
				}
			}
			else
			{
				Row = MakeDependency(
					Kind,
					EAngelscriptArtifactReferenceKind::ScriptGlobal,
					MakeHash(TEXT("missing-global")),
					MakeHash(TEXT("global-abi")),
					MakeHash(TEXT("global-value")));
			}
			FDependencyProvider Provider(*Routes, Seed);
			Provider.SetEntryDependencies(UseFolded->Identity.FunctionKey, {Row});
			FAngelscriptJITProviderRegistry& Registry =
				FAngelscriptJITProviderRegistry::Get();
			if (!Registry.RegisterProvider(&Provider, Provider).IsAccepted())
			{
				return EAngelscriptArtifactMatchResult::InvalidProvider;
			}
			ON_SCOPE_EXIT
			{
				Registry.UnregisterOwner(&Provider);
			};
			const FAngelscriptJITProviderRouteRefreshResult Refresh =
				FAngelscriptJITProviderRouter::Refresh(*Engine);
			if (!Refresh.IsApplied())
			{
				return EAngelscriptArtifactMatchResult::InvalidProvider;
			}
			const FAngelscriptJITProviderRouteRecord* const Record =
				FindRecord(Refresh, UseFolded->Identity.FunctionKey);
			return Record != nullptr
				? Record->MatchResult
				: EAngelscriptArtifactMatchResult::MissingProviderEntry;
		};

		const EAngelscriptArtifactSemanticDependencyKind Kinds[] = {
			EAngelscriptArtifactSemanticDependencyKind::HardValue,
			EAngelscriptArtifactSemanticDependencyKind::ValueLayout,
			EAngelscriptArtifactSemanticDependencyKind::PropertyLayout,
			EAngelscriptArtifactSemanticDependencyKind::GlobalStorage,
		};
		for (const EAngelscriptArtifactSemanticDependencyKind Kind : Kinds)
		{
			const bool bHasCurrent =
				FindSemanticDependency(*Routes, Kind) != nullptr;
			if (bHasCurrent)
			{
				ASSERT_THAT(IsTrue(
					RefreshKind(
						*FString::Printf(TEXT("authority-match-%u"),
							static_cast<uint32>(Kind)),
						Kind,
						true)
					== EAngelscriptArtifactMatchResult::Exact));
			}
			ASSERT_THAT(IsTrue(
				RefreshKind(
					*FString::Printf(TEXT("authority-mismatch-%u"),
						static_cast<uint32>(Kind)),
					Kind,
					false)
				== EAngelscriptArtifactMatchResult::SemanticDependencyMismatch));
		}
	}

	TEST_METHOD(TwoRootsShareOneHelperWhileUnrelatedAndDynamicStayIndependent)
	{
		FScopedProjectRoot Project(TEXT("TypedASTJITDependencyMatch"));
		ASSERT_THAT(IsTrue(Project.WriteSource(SharedScript)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		if (!Routes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const RootA =
			FindRoute(*Routes, TEXT("int RootA()"));
		const FAngelscriptFunctionRoute* const RootB =
			FindRoute(*Routes, TEXT("int RootB()"));
		const FAngelscriptFunctionRoute* const Unrelated =
			FindRoute(*Routes, TEXT("int Unrelated()"));
		const FAngelscriptFunctionRoute* const Dynamic =
			FindRoute(*Routes, TEXT("int DynamicRouted()"));
		const FAngelscriptFunctionRoute* const Helper =
			FindRoute(*Routes, TEXT("int Helper()"));
		ASSERT_THAT(IsNotNull(RootA));
		ASSERT_THAT(IsNotNull(RootB));
		ASSERT_THAT(IsNotNull(Unrelated));
		ASSERT_THAT(IsNotNull(Dynamic));
		ASSERT_THAT(IsNotNull(Helper));
		if (RootA == nullptr || RootB == nullptr || Unrelated == nullptr
			|| Dynamic == nullptr || Helper == nullptr)
		{
			return;
		}

		FDependencyProvider Provider(*Routes, TEXT("match-shared"));
		Provider.SetEntryDependencies(
			RootA->Identity.FunctionKey, {MakeFunctionContent(*Helper)});
		Provider.SetEntryDependencies(
			RootB->Identity.FunctionKey, {MakeFunctionContent(*Helper)});
		Provider.SetEntryDependencies(
			Dynamic->Identity.FunctionKey, {MakeSignature(*Helper)});
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};

		const FAngelscriptJITProviderRouteRefreshResult Refresh =
			FAngelscriptJITProviderRouter::Refresh(*Engine);
		ASSERT_THAT(IsTrue(Refresh.IsApplied()));
		auto ExpectExact = [this, &Refresh](
			const FAngelscriptStableFunctionKey& FunctionKey)
		{
			const FAngelscriptJITProviderRouteRecord* const Record =
				FindRecord(Refresh, FunctionKey);
			ASSERT_THAT(IsNotNull(Record));
			if (Record != nullptr)
			{
				ASSERT_THAT(IsTrue(Record->MatchResult
					== EAngelscriptArtifactMatchResult::Exact));
				ASSERT_THAT(IsTrue(Record->bBindingPublished));
			}
		};
		ExpectExact(RootA->Identity.FunctionKey);
		ExpectExact(RootB->Identity.FunctionKey);
		ExpectExact(Unrelated->Identity.FunctionKey);
		ExpectExact(Dynamic->Identity.FunctionKey);

		TArray<FAngelscriptStableFunctionKey> Dependents;
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderRouter::CollectSemanticDependents(
				*Engine,
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
				Helper->Identity.FunctionKey.Hash,
				Dependents)));
		ASSERT_THAT(AreEqual(2, Dependents.Num()));
		ASSERT_THAT(IsTrue(Dependents.Contains(RootA->Identity.FunctionKey)));
		ASSERT_THAT(IsTrue(Dependents.Contains(RootB->Identity.FunctionKey)));
		ASSERT_THAT(IsFalse(Dependents.Contains(Unrelated->Identity.FunctionKey)));
		ASSERT_THAT(IsFalse(Dependents.Contains(Dynamic->Identity.FunctionKey)));
	}

	TEST_METHOD(SelfRecursionDoesNotRequireSelfFunctionContent)
	{
		FScopedProjectRoot Project(TEXT("TypedASTJITDependencyMatch"));
		ASSERT_THAT(IsTrue(Project.WriteSource(SharedScript)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		if (!Routes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const Factorial =
			FindRouteByName(*Routes, "Factorial");
		ASSERT_THAT(IsNotNull(Factorial));
		if (Factorial == nullptr)
		{
			return;
		}

		FDependencyProvider Provider(*Routes, TEXT("match-self"));
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};
		const FAngelscriptJITProviderRouteRefreshResult Refresh =
			FAngelscriptJITProviderRouter::Refresh(*Engine);
		ASSERT_THAT(IsTrue(Refresh.IsApplied()));
		const FAngelscriptJITProviderRouteRecord* const Record =
			FindRecord(Refresh, Factorial->Identity.FunctionKey);
		ASSERT_THAT(IsNotNull(Record));
		if (Record == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			Record->MatchResult == EAngelscriptArtifactMatchResult::Exact));
		ASSERT_THAT(AreEqual(0,
			Provider.Entries[Provider.FindEntryIndex(
				Factorial->Identity.FunctionKey)].SemanticDependencyCount));
	}

	TEST_METHOD(TransitiveHelperContentIsValidated)
	{
		FScopedProjectRoot Project(TEXT("TypedASTJITDependencyMatch"));
		ASSERT_THAT(IsTrue(Project.WriteSource(SharedScript)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		if (!Routes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const Root =
			FindRoute(*Routes, TEXT("int TransitiveRoot()"));
		const FAngelscriptFunctionRoute* const HelperB =
			FindRoute(*Routes, TEXT("int HelperB()"));
		const FAngelscriptFunctionRoute* const HelperC =
			FindRoute(*Routes, TEXT("int HelperC()"));
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(HelperB));
		ASSERT_THAT(IsNotNull(HelperC));
		if (Root == nullptr || HelperB == nullptr || HelperC == nullptr)
		{
			return;
		}

		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		{
			FDependencyProvider Matching(*Routes, TEXT("match-transitive"));
			Matching.SetEntryDependencies(
				Root->Identity.FunctionKey,
				{MakeFunctionContent(*HelperB), MakeFunctionContent(*HelperC)});
			ASSERT_THAT(IsTrue(
				Registry.RegisterProvider(&Matching, Matching).IsAccepted()));
			ON_SCOPE_EXIT
			{
				Registry.UnregisterOwner(&Matching);
			};
			const FAngelscriptJITProviderRouteRefreshResult ExactRefresh =
				FAngelscriptJITProviderRouter::Refresh(*Engine);
			ASSERT_THAT(IsTrue(ExactRefresh.IsApplied()));
			const FAngelscriptJITProviderRouteRecord* const ExactRecord =
				FindRecord(ExactRefresh, Root->Identity.FunctionKey);
			ASSERT_THAT(IsNotNull(ExactRecord));
			if (ExactRecord != nullptr)
			{
				ASSERT_THAT(IsTrue(ExactRecord->MatchResult
					== EAngelscriptArtifactMatchResult::Exact));
			}
		}

		FAngelscriptArtifactSemanticDependency StaleC = MakeFunctionContent(*HelperC);
		StaleC.ExpectedContentOrValue = MakeHash(TEXT("stale-helper-c"));
		FDependencyProvider Stale(*Routes, TEXT("match-transitive-stale"));
		Stale.SetEntryDependencies(
			Root->Identity.FunctionKey,
			{MakeFunctionContent(*HelperB), StaleC});
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Stale, Stale).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Stale);
		};
		const FAngelscriptJITProviderRouteRefreshResult StaleRefresh =
			FAngelscriptJITProviderRouter::Refresh(*Engine);
		ASSERT_THAT(IsTrue(StaleRefresh.IsApplied()));
		const FAngelscriptJITProviderRouteRecord* const StaleRecord =
			FindRecord(StaleRefresh, Root->Identity.FunctionKey);
		ASSERT_THAT(IsNotNull(StaleRecord));
		if (StaleRecord != nullptr)
		{
			ASSERT_THAT(IsTrue(StaleRecord->MatchResult
				== EAngelscriptArtifactMatchResult::SemanticDependencyMismatch));
		}
	}

	TEST_METHOD(MatcherEvaluatesCopiedProviderSlicesAgainstCurrentRoutes)
	{
		FScopedProjectRoot Project(TEXT("TypedASTJITDependencyMatch"));
		ASSERT_THAT(IsTrue(Project.WriteSource(SharedScript)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		if (!Routes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const Root =
			FindRoute(*Routes, TEXT("int RootA()"));
		const FAngelscriptFunctionRoute* const Helper =
			FindRoute(*Routes, TEXT("int Helper()"));
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Helper));
		if (Root == nullptr || Helper == nullptr)
		{
			return;
		}

		FDependencyProvider Provider(*Routes, TEXT("match-copied-slices"));
		Provider.SetEntryDependencies(
			Root->Identity.FunctionKey, {MakeFunctionContent(*Helper)});
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};

		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Snapshot = Registry.GetSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		if (!Snapshot.IsValid())
		{
			return;
		}
		FExactResolver Resolver;
		FAngelscriptJITFunctionMatchInput Input = MakeMatchInput(*Root);
		Input.CurrentRoutes = Routes;
		const FAngelscriptJITFunctionMatchResult Exact =
			FAngelscriptJITProviderMatcher::Match(*Snapshot, Input, Resolver);
		ASSERT_THAT(IsTrue(
			Exact.Result == EAngelscriptArtifactMatchResult::Exact));

		TSharedRef<FAngelscriptFunctionRouteSnapshot, ESPMode::ThreadSafe>
			StaleRoutes = MakeShared<FAngelscriptFunctionRouteSnapshot,
				ESPMode::ThreadSafe>(*Routes);
		FAngelscriptFunctionRoute* const StaleHelper =
			StaleRoutes->FunctionRoutes.FindByPredicate(
				[Helper](const FAngelscriptFunctionRoute& Route)
				{
					return Route.Identity.FunctionKey
						== Helper->Identity.FunctionKey;
				});
		ASSERT_THAT(IsNotNull(StaleHelper));
		if (StaleHelper == nullptr)
		{
			return;
		}
		StaleHelper->Identity.Content.Execution =
			MakeHash(TEXT("reloaded-helper-execution"));
		Input.CurrentRoutes = StaleRoutes;
		const FAngelscriptJITFunctionMatchResult Stale =
			FAngelscriptJITProviderMatcher::Match(*Snapshot, Input, Resolver);
		ASSERT_THAT(IsTrue(Stale.Result
			== EAngelscriptArtifactMatchResult::SemanticDependencyMismatch));
	}
};

#endif
