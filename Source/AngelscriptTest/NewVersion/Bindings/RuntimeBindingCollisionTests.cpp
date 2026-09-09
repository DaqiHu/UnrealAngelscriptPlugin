#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "CollisionShape.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingCollisionFixture
{
static const FName CollisionProviderNames[] = {
	TEXT("CollisionProfile"), TEXT("FBodyInstance"),
	TEXT("FCollisionQueryParams.TypeDeclarations"), TEXT("FCollisionQueryParams.TypeInfrastructure"),
	TEXT("FCollisionQueryParams.ExplicitBindings"), TEXT("FCollisionShape.Types"), TEXT("FCollisionShape.Functions"),
	TEXT("FHitResult.Functions"), TEXT("FOverlapResult"), TEXT("UCollisionProfile"),
	TEXT("WorldCollision.TypeDeclarations"), TEXT("WorldCollision.TypeInfrastructure"), TEXT("WorldCollision.ExplicitBindings")
};

static const FName CollisionSurfaceProviderNames[] = {
	TEXT("CollisionProfile"), TEXT("FBodyInstance"),
	TEXT("FCollisionQueryParams.TypeInfrastructure"), TEXT("FCollisionQueryParams.ExplicitBindings"),
	TEXT("FCollisionShape.Functions"), TEXT("FHitResult.Functions"),
	TEXT("FOverlapResult"), TEXT("UCollisionProfile"), TEXT("WorldCollision.TypeInfrastructure"),
	TEXT("WorldCollision.ExplicitBindings")
};

struct FCollisionFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Create(bool bOwner = true)
	{
		auto Fail = [this](const TCHAR* Stage) { Diagnostic = FString(Stage) + TEXT(": ") + Diagnostic; UE_LOG(LogTemp, Error, TEXT("Collision fixture %s"), *Diagnostic); return false; };
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail(TEXT("Providers"));
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return Fail(TEXT("Declarations"));
		if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, CollisionSurfaceProviderNames, *Store, Diagnostic)) return Fail(TEXT("Surfaces"));
		if (!Store->Seal(Diagnostic)) return Fail(TEXT("Seal"));
		if (!bOwner) return true;
		FAngelscriptBindingDiagnostic BindingDiagnostic; Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; return Fail(TEXT("Create")); }
		return true;
	}

	asCScriptFunction* Function(const TCHAR* OwnerName, const TCHAR* Declaration) const
	{
		for (const auto& Installed : Owner->GetBindingInstallation()->GetMembers())
		{
			const auto* Record = Store->GetMember(Installed.Record); const auto* Type = Store->GetType(Installed.Record.Type);
			if (Installed.Function && Record && Type && Type->Declaration.Name == OwnerName && Record->Declaration == Declaration) return Installed.Function;
		}
		return nullptr;
	}

	const FAngelscriptTypeBindInfoMember* Property(const TCHAR* OwnerName, const TCHAR* Declaration) const
	{
		const auto* Type = Store->GetType(Store->FindType(OwnerName));
		return Type ? Type->Members.FindByPredicate([&](const auto& Member) { return Member.Declaration == Declaration; }) : nullptr;
	}
};

struct FTransientCollisionWorld
{
	UWorld* World = nullptr;
	FTransientCollisionWorld()
	{
		const FName WorldName(*FString::Printf(TEXT("RuntimeBindingCollisionWorld_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	}
	~FTransientCollisionWorld() { if (World) World->DestroyWorld(false); }
};
}

using namespace RuntimeBindingCollisionFixture;
TEST_CLASS_WITH_FLAGS(Collision, "Angelscript.UnitTest.RuntimeBindings.Runtime.Collision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SphereShapeRadiusTwoRoundTrips)
	{
		FCollisionFixture F; ASSERT_THAT(IsTrue(F.Create())); FCollisionShape Shape; asIScriptContext* C = F.Owner->Engine->CreateContext();
		auto* Set = F.Function(TEXT("FCollisionShape"), TEXT("void SetSphere(const float32 Radius)")); auto* Get = F.Function(TEXT("FCollisionShape"), TEXT("float32 GetSphereRadius() const"));
		ASSERT_THAT(IsNotNull(Set)); ASSERT_THAT(IsNotNull(Get)); ASSERT_THAT(AreEqual(0, C->Prepare(Set))); ASSERT_THAT(AreEqual(0, C->SetObject(&Shape))); ASSERT_THAT(AreEqual(0, C->SetArgFloat(0, 2.0f)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(0, C->Prepare(Get))); ASSERT_THAT(AreEqual(0, C->SetObject(&Shape)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(C->GetReturnFloat() == 2.0f)); C->Release();
	}

	TEST_METHOD(QueryIgnoredActorCollectionCopiesIndependently)
	{
		FCollisionFixture F; ASSERT_THAT(IsTrue(F.Create(false))); auto* A = NewObject<AActor>(); auto* B = NewObject<AActor>();
		FCollisionQueryParams Original; Original.AddIgnoredActor(A); FCollisionQueryParams Copy = Original; Original.AddIgnoredActor(B);
		ASSERT_THAT(AreEqual(2, Original.GetIgnoredSourceObjects().Num())); ASSERT_THAT(AreEqual(1, Copy.GetIgnoredSourceObjects().Num())); ASSERT_THAT(AreEqual(Original.GetIgnoredSourceObjects()[0], Copy.GetIgnoredSourceObjects()[0]));
	}

	TEST_METHOD(HitAndOverlapPropertiesMatchFixtureValues)
	{
		FCollisionFixture F; ASSERT_THAT(IsTrue(F.Create(false))); FHitResult Hit; Hit.Distance = 12.5f; Hit.Item = 17; FOverlapResult Overlap; Overlap.ItemIndex = 23;
		const auto* Distance = F.Property(TEXT("FHitResult"), TEXT("float32 Distance")); const auto* Item = F.Property(TEXT("FHitResult"), TEXT("int Item")); const auto* OverlapItem = F.Property(TEXT("FOverlapResult"), TEXT("int ItemIndex"));
		ASSERT_THAT(IsNotNull(Distance)); ASSERT_THAT(IsNotNull(Item)); ASSERT_THAT(IsNotNull(OverlapItem));
		ASSERT_THAT(IsTrue(*reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(&Hit) + Distance->PropertyOffset) == 12.5f));
		ASSERT_THAT(AreEqual(17, *reinterpret_cast<const int32*>(reinterpret_cast<const uint8*>(&Hit) + Item->PropertyOffset)));
		ASSERT_THAT(AreEqual(23, *reinterpret_cast<const int32*>(reinterpret_cast<const uint8*>(&Overlap) + OverlapItem->PropertyOffset)));
	}

	TEST_METHOD(EmptyTransientWorldQueryReturnsNoHit)
	{
		FCollisionFixture F; ASSERT_THAT(IsTrue(F.Create())); FTransientCollisionWorld W; ASSERT_THAT(IsNotNull(W.World)); bool Hit = true;
		ASSERT_THAT(IsTrue(F.Owner->GetBindingInstallation()->InvokeCollisionLineTraceTestByChannel(W.World, FVector::ZeroVector, FVector(100.0, 0.0, 0.0), ECC_Visibility,
			FCollisionQueryParams::DefaultQueryParam, FCollisionResponseParams::DefaultResponseParam, Hit, F.Diagnostic), *F.Diagnostic)); ASSERT_THAT(IsFalse(Hit));
	}

	TEST_METHOD(NullWorldContextProducesExplicitDiagnostic)
	{
		FCollisionFixture F; ASSERT_THAT(IsTrue(F.Create())); bool Hit = true;
		ASSERT_THAT(IsFalse(F.Owner->GetBindingInstallation()->InvokeCollisionLineTraceTestByChannel(nullptr, FVector::ZeroVector, FVector::OneVector, ECC_Visibility,
			FCollisionQueryParams::DefaultQueryParam, FCollisionResponseParams::DefaultResponseParam, Hit, F.Diagnostic)));
		ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("world"), ESearchCase::IgnoreCase))); ASSERT_THAT(IsFalse(Hit));
	}

	TEST_METHOD(EveryCollisionDeclarationAndNativeRecipeIsAccounted)
	{
		FCollisionFixture F; ASSERT_THAT(IsTrue(F.Create(false)));
		for (const TCHAR* Name : {TEXT("FCollisionShape"), TEXT("FCollisionQueryParams"), TEXT("FComponentQueryParams"), TEXT("FCollisionResponseParams"), TEXT("FCollisionObjectQueryParams"), TEXT("FTraceHandle"), TEXT("FTraceDatum"), TEXT("FOverlapDatum")})
			ASSERT_THAT(IsNotNull(F.Store->GetType(F.Store->FindType(Name))));
		int32 Contributions = 0, CallableRecipes = 0;
		for (const auto& Type : F.Store->GetTypes()) for (const auto& Member : Type.Members) for (FName Provider : CollisionProviderNames)
			if (Member.Source.Provider == Provider) { ++Contributions; if (!Member.NativeRecipes.IsEmpty()) ++CallableRecipes; break; }
		ASSERT_THAT(IsTrue(Contributions >= 100)); ASSERT_THAT(IsTrue(CallableRecipes >= 50));
	}
};
#endif
