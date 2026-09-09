#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Binds/Helper_ToString.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingFinalizationFixture
{
struct FFinalizationFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Record(bool bSeal = true, bool bCreateOwner = true)
	{
		auto Fail = [this](const TCHAR* Stage) { Diagnostic = FString(Stage) + TEXT(": ") + Diagnostic; UE_LOG(LogTemp, Error, TEXT("Finalization fixture %s"), *Diagnostic); return false; };
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail(TEXT("Providers"));
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return Fail(TEXT("Declarations"));
		TArray<FName> Selected;
		const TSet<FName> InfrastructureToStrings = {
			TEXT("FLinearColor.Infrastructure"), TEXT("FName.Infrastructure"), TEXT("FQuat.Infrastructure"),
			TEXT("FQuat4f.Infrastructure"), TEXT("FRotator.Infrastructure"), TEXT("FRotator3f.Infrastructure"),
			TEXT("FText.Infrastructure"), TEXT("FTransform.Infrastructure"), TEXT("FTransform3f.Infrastructure"),
			TEXT("FVector2D.Infrastructure"), TEXT("FVector2f.Infrastructure"), TEXT("FVector3f.TypeInfrastructure"),
			TEXT("FVector4.Infrastructure"), TEXT("FVector4f.Infrastructure")};
		for (const FAngelscriptBindRecord& Record : Collection.GetRecords())
		{
			const FString Name = Record.BindName.ToString();
			if (Name.Contains(TEXT("ToStringContribution")) || InfrastructureToStrings.Contains(Record.BindName)
				|| Record.BindName == TEXT("FString.ExplicitBindings")
				|| Record.BindName == TEXT("SkipBinds.Defaults") || Record.BindName == TEXT("DirectBindArchitectureProbe"))
			{
				const bool bAlreadyRecorded = Store->GetProviders().ContainsByPredicate([&](const auto& Provider) { return Provider.Source.Provider == Record.BindName; });
				if (!bAlreadyRecorded) Selected.Add(Record.BindName);
			}
		}
		if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, Selected, *Store, Diagnostic)) return Fail(TEXT("Effects"));
		if (!bSeal) return true;
		if (!Store->Seal(Diagnostic)) return Fail(TEXT("Seal"));
		if (!bCreateOwner) return true;
		FAngelscriptBindingDiagnostic BindingDiagnostic; Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; return Fail(TEXT("Create")); }
		return true;
	}

	int32 CountEffects(EAngelscriptBindEffectKind Kind) const
	{
		int32 Count = 0; for (const auto& Effect : Store->GetEffects()) Count += Effect.Kind == Kind ? 1 : 0; return Count;
	}
};
}

using namespace RuntimeBindingFinalizationFixture;
TEST_CLASS_WITH_FLAGS(Finalization, "Angelscript.UnitTest.RuntimeBindings.Runtime.Finalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EveryToStringContributionIsRecordedAndOwnerLocal)
	{
		FFinalizationFixture F; ASSERT_THAT(IsTrue(F.Record(), *F.Diagnostic));
		const int32 EffectCount = F.CountEffects(EAngelscriptBindEffectKind::ToString);
		ASSERT_THAT(AreEqual(45, EffectCount));
		ASSERT_THAT(AreEqual(EffectCount, F.Owner->GetToStringList()->Num()));
		const auto* IntEffect = F.Store->GetEffects().FindByPredicate([](const auto& Effect) { return Effect.Kind == EAngelscriptBindEffectKind::ToString && Effect.Name == TEXT("int32"); });
		const auto* BoolEffect = F.Store->GetEffects().FindByPredicate([](const auto& Effect) { return Effect.Kind == EAngelscriptBindEffectKind::ToString && Effect.Name == TEXT("bool"); });
		ASSERT_THAT(IsNotNull(IntEffect)); ASSERT_THAT(IsNotNull(BoolEffect));
		int32 Number = 42; bool Flag = true; FString NumberText, BoolText; IntEffect->ToString(&Number, NumberText); BoolEffect->ToString(&Flag, BoolText);
		ASSERT_THAT(AreEqual(FString(TEXT("42")), NumberText)); ASSERT_THAT(AreEqual(FString(TEXT("true")), BoolText));
		FAngelscriptBindingDiagnostic D; TUniquePtr<FAngelscriptEngine> Other = FAngelscriptEngine::CreateForBindings(F.Store, D); ASSERT_THAT(IsNotNull(Other.Get()));
		ASSERT_THAT(AreNotEqual(F.Owner->GetToStringList(), Other->GetToStringList())); ASSERT_THAT(AreEqual(EffectCount, Other->GetToStringList()->Num()));
	}

	TEST_METHOD(SkipRulesInstallWithManualGeneratedReflectionPrecedence)
	{
		FFinalizationFixture F; ASSERT_THAT(IsTrue(F.Record(), *F.Diagnostic));
		ASSERT_THAT(AreEqual(5, F.CountEffects(EAngelscriptBindEffectKind::SkipFunction)));
		ASSERT_THAT(AreEqual(4, F.CountEffects(EAngelscriptBindEffectKind::SkipClass)));
		const auto* State = F.Owner->GetBindState(); ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(IsTrue(State->SkipBindNames.Contains(MakeTuple(FName(TEXT("StaticMesh")), FName(TEXT("GetMinLODForQualityLevels"))))));
		ASSERT_THAT(IsTrue(State->SkipBindNames.Contains(MakeTuple(FName(TEXT("SourceEffectEQPreset")), FName(TEXT("SetSettings"))))));
		ASSERT_THAT(IsTrue(State->SkipBindClasses.Contains(TEXT("NiagaraPreviewGrid"))));
	}

	TEST_METHOD(ConfigEnumsAndDeprecationsFollowCapturedEditorPolicy)
	{
		FFinalizationFixture F; ASSERT_THAT(IsTrue(F.Record(false, false), *F.Diagnostic));
		ASSERT_THAT(IsTrue(F.Store->GetPolicy().bEditor));
		for (const TCHAR* Name : {TEXT("ETraceTypeQuery"), TEXT("ECollisionChannel"), TEXT("EObjectTypeQuery")})
		{
			const auto* Type = F.Store->GetType(F.Store->FindType(Name)); ASSERT_THAT(IsNotNull(Type));
			ASSERT_THAT(IsTrue(Type->Members.ContainsByPredicate([](const auto& Member) { return Member.Kind == EAngelscriptTypeBindMemberKind::EnumValue; })));
		}
		ASSERT_THAT(AreEqual(12, F.CountEffects(EAngelscriptBindEffectKind::DeprecateFunction)));
	}

	TEST_METHOD(AllEligibleFinalizersHaveTypedEffects)
	{
		FFinalizationFixture F; ASSERT_THAT(IsTrue(F.Record(), *F.Diagnostic));
		ASSERT_THAT(AreEqual(1, F.CountEffects(EAngelscriptBindEffectKind::DirectBindArchitectureProbe)));
		ASSERT_THAT(AreEqual(1, F.Owner->GetBindState()->DirectCallbackExecutionCountForTesting));
		const auto& Records = FAngelscriptBind::GetRegisteredCollection().GetRecords();
		for (const auto& Record : Records) if (Record.Phase == EAngelscriptBindPhase::Finalization)
			ASSERT_THAT(IsTrue(F.Store->GetEffects().ContainsByPredicate([&](const auto& Effect) { return Effect.Source.Provider == Record.BindName; }), *Record.BindName.ToString()));
	}

	TEST_METHOD(FinalizationFailureNamesProviderAndPublishesNoOwner)
	{
		FFinalizationFixture F; ASSERT_THAT(IsTrue(F.Record(false, false), *F.Diagnostic));
		FAngelscriptBindEffect Failure; Failure.Kind = EAngelscriptBindEffectKind::DirectBindArchitectureProbe;
		Failure.SecondaryName = TEXT("inject-failure"); Failure.Source.OwnerModule = TEXT("AngelscriptTest");
		Failure.Source.Provider = TEXT("FinalizationFailureFixture"); Failure.Source.File = TEXT("RuntimeBindingFinalizationTests.cpp");
		Failure.Source.Line = __LINE__; Failure.Source.Phase = EAngelscriptBindPhase::Finalization;
		ASSERT_THAT(IsTrue(F.Store->RecordEffect(MoveTemp(Failure), F.Diagnostic), *F.Diagnostic)); ASSERT_THAT(IsTrue(F.Store->Seal(F.Diagnostic), *F.Diagnostic));
		FAngelscriptBindingDiagnostic D; TUniquePtr<FAngelscriptEngine> Owner = FAngelscriptEngine::CreateForBindings(F.Store, D);
		ASSERT_THAT(IsNull(Owner.Get())); ASSERT_THAT(IsTrue(D.Message.Contains(TEXT("FinalizationFailureFixture")), *D.Message));
	}
};
#endif
