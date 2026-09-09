#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingActorsFixture
{
static const FName ActorProviderNames[] = {
	TEXT("AActor.Manual"), TEXT("AActor.PostReflection"), TEXT("AController.Functions"),
	TEXT("APlayerController.Functions"), TEXT("APawn.Functions"), TEXT("AVolume"),
	TEXT("FActorSpawnParameters.TypeInfrastructure"), TEXT("FActorSpawnParameters.ExplicitBindings"),
	TEXT("UActorComponent.Manual"), TEXT("UActorComponent.PostReflection"), TEXT("UFXSystemComponent"),
	TEXT("UPrimitiveComponent"), TEXT("USceneComponent.Functions"), TEXT("UPoseableMeshComponent"),
	TEXT("FScopedMovementUpdate.TypeInfrastructure"),
	TEXT("UProjectileMovementComponent"), TEXT("USkeletalMeshComponent"), TEXT("USkinnedMeshComponent"),
	TEXT("UWorld.Functions"), TEXT("ALandscapeProxy.GetHeightAtLocation")
};

struct FActorsFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;
	bool Create(bool bInstall = true)
	{
		auto Fail = [this](const TCHAR* Stage) { Diagnostic = FString(Stage) + TEXT(": ") + Diagnostic; UE_LOG(LogTemp, Error, TEXT("Actors fixture %s"), *Diagnostic); return false; };
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail(TEXT("Providers"));
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return Fail(TEXT("Declarations"));
		TArray<FName> SurfaceProviders;
		for (FName Name : ActorProviderNames)
		{
			const bool bAlreadyRecorded = Store->GetProviders().ContainsByPredicate(
				[Name](const FAngelscriptRecordedProvider& Provider) { return Provider.Source.Provider == Name; });
			if (!bAlreadyRecorded) SurfaceProviders.Add(Name);
		}
		if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, SurfaceProviders, *Store, Diagnostic)) return Fail(TEXT("Surfaces"));
		if (!Store->Seal(Diagnostic)) return Fail(TEXT("Seal"));
		if (!bInstall) return true;
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
};

struct FTransientActorWorld
{
	UWorld* World = nullptr;
	FTransientActorWorld()
	{
		const FName Name(*FString::Printf(TEXT("RuntimeBindingActorsWorld_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		World = UWorld::CreateWorld(EWorldType::Game, false, Name, GetTransientPackage());
		if (World && GEngine)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
	}
	~FTransientActorWorld()
	{
		if (World)
		{
			World->DestroyWorld(false);
			if (GEngine) GEngine->DestroyWorldContext(World);
		}
	}
};
}

using namespace RuntimeBindingActorsFixture;
TEST_CLASS_WITH_FLAGS(Actors, "Angelscript.UnitTest.RuntimeBindings.Runtime.Actors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SpawnParameterFieldsAndFlagsRoundTrip)
	{
		FActorsFixture F; ASSERT_THAT(IsTrue(F.Create(false), *F.Diagnostic)); FActorSpawnParameters Params;
		Params.Name = TEXT("FixtureActor"); Params.bNoFail = true; Params.bDeferConstruction = true;
		const auto* Type = F.Store->GetType(F.Store->FindType(TEXT("FActorSpawnParameters"))); ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsNotNull(Type->Members.FindByPredicate([](const auto& M) { return M.Declaration == TEXT("FName Name"); })));
		ASSERT_THAT(AreEqual(FName(TEXT("FixtureActor")), Params.Name)); ASSERT_THAT(IsTrue(Params.bNoFail)); ASSERT_THAT(IsTrue(Params.bDeferConstruction));
	}

	TEST_METHOD(TransientActorAndSceneComponentPreserveTranslation)
	{
		FActorsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); FTransientActorWorld W; ASSERT_THAT(IsNotNull(W.World));
		AActor* Actor = W.World->SpawnActor<AActor>(); ASSERT_THAT(IsNotNull(Actor));
		USceneComponent* Component = NewObject<USceneComponent>(Actor); Actor->SetRootComponent(Component); Component->RegisterComponent();
		asIScriptContext* C = F.Owner->Engine->CreateContext(); auto* Set = F.Function(TEXT("USceneComponent"), TEXT("void SetRelativeLocation(const FVector& NewLocation)"));
		ASSERT_THAT(IsNotNull(Set)); FVector Translation(11.0, 22.0, 33.0); ASSERT_THAT(AreEqual(0, C->Prepare(Set))); ASSERT_THAT(AreEqual(0, C->SetObject(Component))); ASSERT_THAT(AreEqual(0, C->SetArgObject(0, &Translation)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(Component->GetComponentLocation() == Translation)); C->Release(); Actor->Destroy();
	}

	TEST_METHOD(CurrentWorldUsesExplicitAmbientFixture)
	{
		FActorsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); FTransientActorWorld W; FAngelscriptEngineScope Scope(*F.Owner, W.World);
		ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentWorldContextObject() == W.World));
		ASSERT_THAT(IsTrue(GEngine->GetWorldFromContextObject(W.World, EGetWorldErrorMode::ReturnNull) == W.World));
		auto* Get = F.Function(TEXT(""), TEXT("UWorld GetCurrentWorld()")); ASSERT_THAT(IsNotNull(Get)); asIScriptContext* C = F.Owner->Engine->CreateContext();
		ASSERT_THAT(AreEqual(0, C->Prepare(Get))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		ASSERT_THAT(IsTrue(C->GetReturnObject() == W.World)); C->Release();
	}

	TEST_METHOD(UnregisteredComponentNoneTagUsesExistingFalseContract)
	{
		FActorsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); UActorComponent* Component = NewObject<USceneComponent>();
		auto* HasTag = F.Function(TEXT("UActorComponent"), TEXT("bool ComponentHasTag(const FName& Tag) const")); ASSERT_THAT(IsNotNull(HasTag)); asIScriptContext* C = F.Owner->Engine->CreateContext(); FName None;
		ASSERT_THAT(AreEqual(0, C->Prepare(HasTag))); ASSERT_THAT(AreEqual(0, C->SetObject(Component))); ASSERT_THAT(AreEqual(0, C->SetArgObject(0, &None))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsFalse(C->GetReturnByte() != 0)); C->Release();
	}

	TEST_METHOD(LocalDestructionDrainsActorAndComponent)
	{
		FActorsFixture F; ASSERT_THAT(IsTrue(F.Create(false), *F.Diagnostic)); FTransientActorWorld W; AActor* Actor = W.World->SpawnActor<AActor>(); UActorComponent* Component = NewObject<USceneComponent>(Actor); Actor->AddInstanceComponent(Component); Component->RegisterComponent();
		TWeakObjectPtr<AActor> WeakActor = Actor; TWeakObjectPtr<UActorComponent> WeakComponent = Component; Component->DestroyComponent(); Actor->Destroy(); CollectGarbage(RF_NoFlags);
		ASSERT_THAT(IsFalse(WeakActor.IsValid())); ASSERT_THAT(IsFalse(WeakComponent.IsValid()));
	}

	TEST_METHOD(EveryActorComponentWorldAndMeshProviderIsAccounted)
	{
		FActorsFixture F; ASSERT_THAT(IsTrue(F.Create(false), *F.Diagnostic));
		for (const TCHAR* Name : {TEXT("AActor"), TEXT("UActorComponent"), TEXT("USceneComponent"), TEXT("UPrimitiveComponent"), TEXT("USkeletalMeshComponent"), TEXT("USkinnedMeshComponent"), TEXT("UPoseableMeshComponent"), TEXT("UWorld"), TEXT("AVolume"), TEXT("ALandscapeProxy")}) ASSERT_THAT(IsNotNull(F.Store->GetType(F.Store->FindType(Name))));
		int32 Contributions = 0, Recipes = 0; for (const auto& Type : F.Store->GetTypes()) for (const auto& Member : Type.Members) for (FName Provider : ActorProviderNames) if (Member.Source.Provider == Provider) { ++Contributions; if (!Member.NativeRecipes.IsEmpty()) ++Recipes; break; }
		ASSERT_THAT(IsTrue(Contributions >= 100)); ASSERT_THAT(IsTrue(Recipes >= 75));
	}
};
#endif
