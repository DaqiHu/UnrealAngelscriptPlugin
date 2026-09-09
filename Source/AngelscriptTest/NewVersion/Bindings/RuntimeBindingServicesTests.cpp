#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Testing/AngelscriptTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingServicesFixture
{
static const FName ServiceProviderNames[] = {
	TEXT("Subsystems.PostReflection"), TEXT("SystemTimers"), TEXT("UGameInstance"), TEXT("ULocalPlayer"),
	TEXT("AngelscriptTest.ExplicitBindings"), TEXT("AngelscriptScriptTestSuite.ExplicitBindings")
};

struct FServicesFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner; FString Diagnostic;
	bool Create(bool bInstall = true)
	{
		auto Fail = [this](const TCHAR* Stage) { Diagnostic = FString(Stage) + TEXT(": ") + Diagnostic; UE_LOG(LogTemp, Error, TEXT("Services fixture %s"), *Diagnostic); return false; };
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail(TEXT("Providers")); const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return Fail(TEXT("Declarations"));
		TArray<FName> Surfaces; for (const FName Name : ServiceProviderNames) if (!Store->GetProviders().ContainsByPredicate([Name](const auto& P) { return P.Source.Provider == Name; })) Surfaces.Add(Name);
		if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, Surfaces, *Store, Diagnostic)) return Fail(TEXT("Surfaces"));
		if (!Store->Seal(Diagnostic)) return Fail(TEXT("Seal")); if (!bInstall) return true;
		FAngelscriptBindingDiagnostic BindingDiagnostic; Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic); if (!Owner) { Diagnostic = BindingDiagnostic.Message; return Fail(TEXT("Create")); } return true;
	}
	asCScriptFunction* Function(const TCHAR* OwnerName, const TCHAR* Namespace, const TCHAR* Declaration) const
	{
		for (const auto& Installed : Owner->GetBindingInstallation()->GetMembers()) { const auto* R = Store->GetMember(Installed.Record); const auto* T = Store->GetType(Installed.Record.Type);
			if (Installed.Function && R && T && T->Declaration.Name == OwnerName && T->Declaration.Namespace == Namespace && R->Declaration == Declaration) return Installed.Function; }
		return nullptr;
	}
};

struct FTransientServiceWorld
{
	UWorld* World = nullptr;
	FTransientServiceWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, FName(*FString::Printf(TEXT("RuntimeBindingServices_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))), GetTransientPackage());
		if (World && GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	}
	~FTransientServiceWorld() { if (World) { World->GetTimerManager().ClearAllTimersForObject(World); World->DestroyWorld(false); if (GEngine) GEngine->DestroyWorldContext(World); } }
};
}

using namespace RuntimeBindingServicesFixture;
TEST_CLASS_WITH_FLAGS(Services, "Angelscript.UnitTest.RuntimeBindings.Runtime.Services",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FixtureTimerFiresOnceThenCancellationPreventsFurtherCalls)
	{
		FServicesFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); FTransientServiceWorld W; ASSERT_THAT(IsNotNull(W.World)); FAngelscriptEngineScope Scope(*F.Owner, W.World);
		int32 Calls = 0; FTimerHandle Handle; W.World->GetTimerManager().SetTimer(Handle, [&Calls]() { ++Calls; }, 0.01f, true);
		W.World->GetTimerManager().Tick(0.0f); ++GFrameCounter;
		W.World->GetTimerManager().Tick(0.011f); ASSERT_THAT(AreEqual(1, Calls));
		auto* Clear = F.Function(TEXT(""), TEXT("System"), TEXT("void ClearAndInvalidateTimerHandle(FTimerHandle& Handle)")); ASSERT_THAT(IsNotNull(Clear)); ASSERT_THAT(IsNotNull(F.Function(TEXT(""), TEXT("System"), TEXT("FTimerHandle SetTimer(const UObject Object, const FName& FunctionName, float32 Time, bool bLooping = false)"))));
		asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Clear))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Handle))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); C->Release();
		ASSERT_THAT(IsFalse(Handle.IsValid())); ++GFrameCounter; W.World->GetTimerManager().Tick(0.05f); ASSERT_THAT(AreEqual(1, Calls));
	}

	TEST_METHOD(SubsystemLookupUsesFixtureWorldAndRejectsWrongOrNullContext)
	{
		FServicesFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* GetWorld = F.Function(TEXT(""), TEXT("USubsystemLibrary"), TEXT("UObject GetWorldSubsystem(UClass Class)")); ASSERT_THAT(IsNotNull(GetWorld));
		UClass* WrongClass = UGameInstance::StaticClass(); asIScriptContext* C = F.Owner->Engine->CreateContext();
		{ FTransientServiceWorld W; FAngelscriptEngineScope Scope(*F.Owner, W.World); ASSERT_THAT(AreEqual(0, C->Prepare(GetWorld))); ASSERT_THAT(AreEqual(0, C->SetArgObject(0, WrongClass))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsNull(C->GetReturnObject())); }
		ASSERT_THAT(AreEqual(0, C->Prepare(GetWorld))); ASSERT_THAT(AreEqual(0, C->SetArgObject(0, UWorldSubsystem::StaticClass()))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsNull(C->GetReturnObject())); C->Release();
	}

	TEST_METHOD(GameInstanceAndLocalPlayerRecordsAreComplete)
	{
		FServicesFixture F; ASSERT_THAT(IsTrue(F.Create(false), *F.Diagnostic)); const auto* Game = F.Store->GetType(F.Store->FindType(TEXT("UGameInstance"))); const auto* Local = F.Store->GetType(F.Store->FindType(TEXT("ULocalPlayer")));
		ASSERT_THAT(IsNotNull(Game)); ASSERT_THAT(IsNotNull(Local)); int32 GameCount = 0, LocalCount = 0;
		for (const auto& M : Game->Members) GameCount += M.Source.Provider == TEXT("UGameInstance") ? 1 : 0; for (const auto& M : Local->Members) LocalCount += M.Source.Provider == TEXT("ULocalPlayer") ? 1 : 0;
		ASSERT_THAT(AreEqual(11, GameCount)); ASSERT_THAT(AreEqual(2, LocalCount));
	}

	TEST_METHOD(RuntimeTestHelperWithoutActiveContextReturnsExplicitDiagnostic)
	{
		FServicesFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* GetWorld = F.Function(TEXT(""), TEXT("FAngelscriptTest"), TEXT("UWorld GetTestWorld()")); ASSERT_THAT(IsNotNull(GetWorld));
		TestRunner->AddExpectedError(TEXT("FAngelscriptTest::GetTestWorld requires an active method leaf"), EAutomationExpectedErrorFlags::Contains, 1);
		asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(GetWorld))); ASSERT_THAT(AreEqual(int(asEXECUTION_EXCEPTION), C->Execute())); ASSERT_THAT(IsNull(C->GetReturnObject())); C->Release();
	}

	TEST_METHOD(AllTimerContextResourcesReleaseAndProvidersAreAccounted)
	{
		FServicesFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); FTransientServiceWorld W; FTimerHandle Handle; W.World->GetTimerManager().SetTimer(Handle, []() {}, 1.0f, true); ASSERT_THAT(IsTrue(Handle.IsValid()));
		W.World->GetTimerManager().ClearTimer(Handle); Handle.Invalidate(); ASSERT_THAT(IsFalse(Handle.IsValid())); for (const FName Name : ServiceProviderNames) ASSERT_THAT(IsTrue(F.Store->GetProviders().ContainsByPredicate([Name](const auto& P) { return P.Source.Provider == Name; })));
		int32 Contributions = 0, Recipes = 0; for (const auto& T : F.Store->GetTypes()) for (const auto& M : T.Members) for (const FName Name : ServiceProviderNames) if (M.Source.Provider == Name) { ++Contributions; if (!M.NativeRecipes.IsEmpty()) ++Recipes; break; }
		ASSERT_THAT(AreEqual(226, Contributions)); ASSERT_THAT(AreEqual(12, Recipes)); F.Owner.Reset(); ASSERT_THAT(IsNull(F.Owner.Get()));
	}
};
#endif
