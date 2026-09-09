#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Framework/Commands/InputChord.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingInputUIFixture
{
static const FName InputUIProviderNames[] = {
	TEXT("FInputActionKeyMapping"), TEXT("FInputActionValue"), TEXT("FInputBindingHandle.Types"),
	TEXT("FInputBindingHandle.Functions"), TEXT("InputComponentScriptMixins.GeneratedOverrides"),
	TEXT("InputEvents.Manual"), TEXT("UEnhancedInputComponent"), TEXT("UInputMappingContext.InputAction"),
	TEXT("UInputMappingContext.EnhancedActionKeyMapping"), TEXT("UInputMappingContext.Functions"),
	TEXT("UInputSettings"), TEXT("UUserWidget.Manual")
};

struct FInputUIFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Create(bool bInstall = true)
	{
		auto Fail = [this](const TCHAR* Stage)
		{
			Diagnostic = FString(Stage) + TEXT(": ") + Diagnostic;
			UE_LOG(LogTemp, Error, TEXT("Input/UI fixture %s"), *Diagnostic);
			return false;
		};
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail(TEXT("Providers"));
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return Fail(TEXT("Declarations"));
		TArray<FName> SurfaceProviders;
		for (const FName Name : InputUIProviderNames)
		{
			const bool bAlreadyRecorded = Store->GetProviders().ContainsByPredicate(
				[Name](const FAngelscriptRecordedProvider& Provider) { return Provider.Source.Provider == Name; });
			if (!bAlreadyRecorded) SurfaceProviders.Add(Name);
		}
		if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, SurfaceProviders, *Store, Diagnostic)) return Fail(TEXT("Surfaces"));
		if (!Store->Seal(Diagnostic)) return Fail(TEXT("Seal"));
		if (!bInstall) return true;
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; return Fail(TEXT("Create")); }
		return true;
	}

	asCScriptFunction* Function(const TCHAR* OwnerName, const TCHAR* Declaration) const
	{
		for (const auto& Installed : Owner->GetBindingInstallation()->GetMembers())
		{
			const auto* Record = Store->GetMember(Installed.Record);
			const auto* Type = Store->GetType(Installed.Record.Type);
			if (Installed.Function && Record && Type
				&& Type->Declaration.Name == OwnerName && Record->Declaration == Declaration)
				return Installed.Function;
		}
		return nullptr;
	}
};
}

using namespace RuntimeBindingInputUIFixture;
TEST_CLASS_WITH_FLAGS(InputUI, "Angelscript.UnitTest.RuntimeBindings.Runtime.InputUI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AxisTwoValuePreservesComponentsAndType)
	{
		FInputUIFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		FInputActionValue Value(FVector2D(1.0, 2.0));
		auto* Get = F.Function(TEXT("FInputActionValue"), TEXT("FVector2D GetAxis2D() const")); ASSERT_THAT(IsNotNull(Get));
		asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Get))); ASSERT_THAT(AreEqual(0, C->SetObject(&Value)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); const auto* Result = static_cast<const FVector2D*>(C->GetReturnObject()); ASSERT_THAT(IsNotNull(Result));
		ASSERT_THAT(IsTrue(*Result == FVector2D(1.0, 2.0))); ASSERT_THAT(IsTrue(Value.GetValueType() == EInputActionValueType::Axis2D)); C->Release();
	}

	TEST_METHOD(ActionMappingCopyPreservesKeyAndModifiers)
	{
		FInputUIFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		FInputActionKeyMapping Left(TEXT("Jump"), EKeys::SpaceBar, true, false, true, false); FInputActionKeyMapping Right(Left);
		auto* Equals = F.Function(TEXT("FInputActionKeyMapping"), TEXT("bool opEquals(const FInputActionKeyMapping& Other) const")); ASSERT_THAT(IsNotNull(Equals));
		asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Equals))); ASSERT_THAT(AreEqual(0, C->SetObject(&Left))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Right)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(C->GetReturnByte() != 0));
		ASSERT_THAT(IsTrue(Right.Key == EKeys::SpaceBar && Right.bShift && !Right.bCtrl && Right.bAlt && !Right.bCmd)); C->Release();
	}

	TEST_METHOD(RemoveBindingByHandleAffectsExactlyOneBinding)
	{
		FInputUIFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		UEnhancedInputComponent* Component = NewObject<UEnhancedInputComponent>(); UInputAction* A = NewObject<UInputAction>(); UInputAction* B = NewObject<UInputAction>();
		const uint32 RemovedHandle = Component->BindActionValue(A).GetHandle(); Component->BindActionValue(B); ASSERT_THAT(AreEqual(2, Component->GetActionValueBindings().Num()));
		auto* Remove = F.Function(TEXT("UEnhancedInputComponent"), TEXT("bool RemoveBindingByHandle(const uint32 BindingIndex)")); ASSERT_THAT(IsNotNull(Remove));
		asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Remove))); ASSERT_THAT(AreEqual(0, C->SetObject(Component))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(0, RemovedHandle)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(C->GetReturnByte() != 0)); ASSERT_THAT(AreEqual(1, Component->GetActionValueBindings().Num()));
		ASSERT_THAT(IsTrue(Component->GetActionValueBindings()[0].GetAction() == B)); C->Release();
	}

	TEST_METHOD(InputChordConstructorPreservesModifierFlags)
	{
		FInputUIFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); FInputChord Chord; FKey Key = EKeys::K;
		auto* Construct = F.Function(TEXT("FInputChord"), TEXT("void f(const FKey& Key, bool bShift, bool bCtrl, bool bAlt, bool bCmd)")); ASSERT_THAT(IsNotNull(Construct));
		asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Construct))); ASSERT_THAT(AreEqual(0, C->SetObject(&Chord))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Key)));
		ASSERT_THAT(AreEqual(0, C->SetArgByte(1, 1))); ASSERT_THAT(AreEqual(0, C->SetArgByte(2, 0))); ASSERT_THAT(AreEqual(0, C->SetArgByte(3, 1))); ASSERT_THAT(AreEqual(0, C->SetArgByte(4, 0)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(Chord.Key == EKeys::K && Chord.bShift && !Chord.bCtrl && Chord.bAlt && !Chord.bCmd)); C->Release();
	}

	TEST_METHOD(TransientWidgetRootLookupPreservesVisibility)
	{
		FInputUIFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		UUserWidget* Widget = GetMutableDefault<UUserWidget>(); UWidgetTree* PreviousTree = Widget->WidgetTree;
		ON_SCOPE_EXIT { Widget->WidgetTree = PreviousTree; };
		Widget->WidgetTree = NewObject<UWidgetTree>(Widget);
		UBorder* Root = Widget->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FixtureRoot")); Root->SetVisibility(ESlateVisibility::Collapsed);
		auto* SetRoot = F.Function(TEXT("UUserWidget"), TEXT("void SetRootWidget(UWidget NewRootWidget)")); auto* GetRoot = F.Function(TEXT("UUserWidget"), TEXT("UWidget GetRootWidget() const")); ASSERT_THAT(IsNotNull(SetRoot)); ASSERT_THAT(IsNotNull(GetRoot));
		asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(SetRoot))); ASSERT_THAT(AreEqual(0, C->SetObject(Widget))); ASSERT_THAT(AreEqual(0, C->SetArgObject(0, Root))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		ASSERT_THAT(AreEqual(0, C->Prepare(GetRoot))); ASSERT_THAT(AreEqual(0, C->SetObject(Widget))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(C->GetReturnObject() == Root));
		ASSERT_THAT(IsTrue(Root->GetVisibility() == ESlateVisibility::Collapsed)); C->Release();
	}

	TEST_METHOD(BindingResourcesReleaseWithOwnerAndProvidersAreAccounted)
	{
		FInputUIFixture F; ASSERT_THAT(IsTrue(F.Create(false), *F.Diagnostic));
		TWeakObjectPtr<UEnhancedInputComponent> WeakComponent; TWeakObjectPtr<UInputAction> WeakAction;
		{
			UEnhancedInputComponent* Component = NewObject<UEnhancedInputComponent>(); UInputAction* Action = NewObject<UInputAction>(); Component->BindActionValue(Action);
			WeakComponent = Component; WeakAction = Action;
		}
		CollectGarbage(RF_NoFlags); ASSERT_THAT(IsFalse(WeakComponent.IsValid())); ASSERT_THAT(IsFalse(WeakAction.IsValid()));
		for (const FName ProviderName : InputUIProviderNames)
		{
			ASSERT_THAT(IsTrue(F.Store->GetProviders().ContainsByPredicate(
				[ProviderName](const FAngelscriptRecordedProvider& Provider) { return Provider.Source.Provider == ProviderName; })));
		}
		int32 Contributions = 0, Recipes = 0;
		for (const auto& Type : F.Store->GetTypes()) for (const auto& Member : Type.Members) for (const FName Provider : InputUIProviderNames)
			if (Member.Source.Provider == Provider) { ++Contributions; if (!Member.NativeRecipes.IsEmpty()) ++Recipes; break; }
		ASSERT_THAT(AreEqual(427, Contributions)); ASSERT_THAT(AreEqual(63, Recipes));
	}
};
#endif
