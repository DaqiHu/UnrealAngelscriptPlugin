#include "CQTest.h"
#include "RuntimeBindingFunctionTestTypes.h"
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptTypeBindInfoInspection.h"
#include "Core/AngelscriptTypeBindInfoReflection.h"
#include "Containers/ScriptArray.h"
#include "Dump/AngelscriptBindingDump.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProperties.h"
#include "Misc/Paths.h"
#include "as_objecttype.h"
#include "as_context.h"
#include "as_scriptengine.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingFullRuntimeFixture
{
static TUniquePtr<FAngelscriptEngine> Create(FString& OutDiagnostic)
{
	FAngelscriptBindingDiagnostic Diagnostic;
	auto Owner = FAngelscriptEngine::CreateForBindings(Diagnostic);
	OutDiagnostic = Diagnostic.Message;
	return Owner;
}

static FString ProviderIdentity(const FAngelscriptTypeBindSource& Source)
{
	return FString::Printf(TEXT("%s|%s|%u"), *Source.OwnerModule.ToString(),
		*Source.Provider.ToString(), static_cast<uint32>(Source.Phase));
}

static TSet<FString> ContributingProviders(const FAngelscriptTypeBindInfoStore& Store)
{
	TSet<FString> Result;
	for (const auto& Type : Store.GetTypes())
	{
		for (const auto& Source : Type.Sources) Result.Add(ProviderIdentity(Source));
		for (const auto& Member : Type.Members) Result.Add(ProviderIdentity(Member.Source));
	}
	for (const auto& Effect : Store.GetEffects()) Result.Add(ProviderIdentity(Effect.Source));
	for (const auto& Primitive : Store.GetPrimitiveDefinitions()) Result.Add(ProviderIdentity(Primitive.Source));
	for (const auto& NativeMap : Store.GetNativeFunctionMaps()) Result.Add(ProviderIdentity(NativeMap.Source));
	return Result;
}

static bool ReconcileExact(const TSet<FString>& Expected, const TSet<FString>& Actual, FString& OutDiagnostic)
{
	TArray<FString> Missing;
	TArray<FString> Unknown;
	for (const FString& Identity : Expected) if (!Actual.Contains(Identity)) Missing.Add(Identity);
	for (const FString& Identity : Actual) if (!Expected.Contains(Identity)) Unknown.Add(Identity);
	Missing.Sort();
	Unknown.Sort();
	OutDiagnostic = FString::Printf(TEXT("missing=[%s]; unknown=[%s]"),
		*FString::Join(Missing, TEXT(", ")), *FString::Join(Unknown, TEXT(", ")));
	return Missing.IsEmpty() && Unknown.IsEmpty();
}

static bool IsAcceptedFullRuntime(const FAngelscriptBindingInspection& Inspection)
{
	return Inspection.Scope == EAngelscriptBindingInspectionScope::FullRuntime
		&& !Inspection.bCollectionOnly && Inspection.bSealed
		&& Inspection.bValidationAttempted && Inspection.bValid;
}

static asCScriptFunction* InstalledFunction(const FAngelscriptEngine& Owner,
	const TCHAR* TypeName, const TCHAR* Declaration)
{
	const auto* Installation = Owner.GetBindingInstallation();
	if (!Installation) return nullptr;
	for (const auto& Installed : Installation->GetMembers())
	{
		const auto* Member = Installation->GetStore().GetMember(Installed.Record);
		const auto* Type = Installation->GetStore().GetType(Installed.Record.Type);
		if (Installed.Function && Member && Type && Type->Declaration.Name == TypeName
			&& Member->Declaration == Declaration) return Installed.Function;
	}
	return nullptr;
}
}

using namespace RuntimeBindingFullRuntimeFixture;
TEST_CLASS_WITH_FLAGS(FullRuntime, "Angelscript.UnitTest.RuntimeBindings.Coverage.FullRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FactoryPublishesTwoIndependentCompleteOwners)
	{
		FString Diagnostic;
		auto First = Create(Diagnostic);
		ASSERT_THAT(IsTrue(First.IsValid(), *Diagnostic));
		auto Second = Create(Diagnostic);
		ASSERT_THAT(IsTrue(Second.IsValid(), *Diagnostic));
		ASSERT_THAT(IsTrue(First->Engine != Second->Engine));
		ASSERT_THAT(IsTrue(First->GetBindingInstallation() != Second->GetBindingInstallation()));
		ASSERT_THAT(IsTrue(&First->GetBindingInstallation()->GetStore()
			!= &Second->GetBindingInstallation()->GetStore()));
		ASSERT_THAT(IsTrue(First->Engine->GetTypeInfoByName("FVector") != nullptr));
		ASSERT_THAT(IsTrue(First->Engine->GetTypeInfoByName("FString") != nullptr));
		ASSERT_THAT(AreEqual(1, First->GetBindState()->DirectCallbackExecutionCountForTesting));
		asCObjectType* StringArray = nullptr;
		ASSERT_THAT(IsTrue(First->GetBindingInstallation()->MaterializeTemplateInstance(
			TEXT("TArray<FString>"), *static_cast<asCScriptEngine*>(First->Engine), StringArray, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsNotNull(StringArray));
		First.Reset();
		ASSERT_THAT(IsTrue(Second->Engine->GetTypeInfoByName("FVector") != nullptr));
	}

	TEST_METHOD(RuntimeCollectionAndInstalledSnapshotHaveExactProviderAccounting)
	{
		FString Diagnostic;
		auto Owner = Create(Diagnostic);
		ASSERT_THAT(IsTrue(Owner.IsValid(), *Diagnostic));
		const auto& Store = Owner->GetBindingInstallation()->GetStore();
		const TArray<FAngelscriptBindMetadata> Collection = FAngelscriptBind::GetRegisteredBindMetadata();
		ASSERT_THAT(AreEqual(Collection.Num(), Store.GetProviders().Num()));
		TSet<FString> Recorded;
		for (const auto& Provider : Store.GetProviders())
		{
			const FString Identity = ProviderIdentity(Provider.Source);
			ASSERT_THAT(IsFalse(Recorded.Contains(Identity), *Identity));
			Recorded.Add(Identity);
		}
		for (const auto& Provider : Collection)
		{
			FAngelscriptTypeBindSource Source{Provider.OwnerModule, Provider.BindName,
				Provider.SourceFile, Provider.SourceLine, Provider.Phase};
			ASSERT_THAT(IsTrue(Recorded.Contains(ProviderIdentity(Source)), *ProviderIdentity(Source)));
		}
		const TSet<FString> Contributions = ContributingProviders(Store);
		TSet<FString> ExpectedNoOutput = {
			TEXT("AngelscriptRuntime|AssetManagerScriptMixins.GeneratedOverrides|2"),
			TEXT("AngelscriptRuntime|BlueprintType.StaticClasses|2"),
			TEXT("AngelscriptRuntime|InputComponentScriptMixins.GeneratedOverrides|2"),
			TEXT("AngelscriptRuntime|UObject.TypeInfrastructure|1"),
			TEXT("AngelscriptRuntime|UProjectileMovementComponent|2"),
			TEXT("AngelscriptRuntime|UStruct.ReflectionBindings|4"),
			TEXT("AngelscriptRuntime|UStruct.TypeInfrastructure|1"),
		};
		for (const TCHAR* Module : {TEXT("AIModule"), TEXT("AngelscriptRuntime"), TEXT("AssetRegistry"),
			TEXT("EngineSettings"), TEXT("Engine"), TEXT("EnhancedInput"), TEXT("Landscape"),
			TEXT("NavigationSystem"), TEXT("UMGEditor"), TEXT("UMG"), TEXT("UnrealEd")})
		{
			ExpectedNoOutput.Add(FString::Printf(TEXT("AngelscriptRuntime|UHT.FunctionBinding.%s|3"), Module));
		}
		TSet<FString> ObservedNoOutput;
		for (const auto& Provider : Store.GetProviders())
			if (Provider.bEligible && !Contributions.Contains(ProviderIdentity(Provider.Source)))
				ObservedNoOutput.Add(ProviderIdentity(Provider.Source));
		FString Reconciliation;
		ASSERT_THAT(IsTrue(ReconcileExact(ExpectedNoOutput, ObservedNoOutput, Reconciliation), *Reconciliation));
	}

	TEST_METHOD(ExactReconciliationRejectsBalancedMissingAndUnknownIdentities)
	{
		const TSet<FString> Expected = {TEXT("Provider.A"), TEXT("Provider.B")};
		const TSet<FString> BalancedButWrong = {TEXT("Provider.A"), TEXT("Provider.C")};
		FString Diagnostic;
		ASSERT_THAT(AreEqual(Expected.Num(), BalancedButWrong.Num()));
		ASSERT_THAT(IsFalse(ReconcileExact(Expected, BalancedButWrong, Diagnostic)));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Provider.B")) && Diagnostic.Contains(TEXT("Provider.C")), *Diagnostic));
	}

	TEST_METHOD(FullOwnerExecutesVectorStringContainerAndReflectedCalls)
	{
		FString Diagnostic;
		auto Owner = Create(Diagnostic);
		ASSERT_THAT(IsTrue(Owner.IsValid(), *Diagnostic));
		asIScriptContext* Context = Owner->Engine->CreateContext();
		FVector Vector(3.0, 4.0, 0.0);
		auto* Size = InstalledFunction(*Owner, TEXT("FVector"), TEXT("float64 Size() const"));
		ASSERT_THAT(IsNotNull(Size)); ASSERT_THAT(AreEqual(0, Context->Prepare(Size)));
		ASSERT_THAT(AreEqual(0, Context->SetObject(&Vector))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute()));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(5.0, Context->GetReturnDouble())));
		FString String = TEXT("four"); auto* Len = InstalledFunction(*Owner, TEXT("FString"), TEXT("int Len() const"));
		ASSERT_THAT(IsNotNull(Len)); ASSERT_THAT(AreEqual(0, Context->Prepare(Len)));
		ASSERT_THAT(AreEqual(0, Context->SetObject(&String))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute()));
		ASSERT_THAT(AreEqual(asDWORD(4), Context->GetReturnDWord())); Context->Release();

		asCObjectType* ArrayType = nullptr;
		ASSERT_THAT(IsTrue(Owner->GetBindingInstallation()->MaterializeTemplateInstance(
			TEXT("TArray<FString>"), *static_cast<asCScriptEngine*>(Owner->Engine), ArrayType, Diagnostic), *Diagnostic));
		const auto* Operations = Owner->GetBindingInstallation()->FindTemplateOperations(ArrayType);
		ASSERT_THAT(IsNotNull(Operations));
		alignas(FScriptArray) uint8 ArrayStorage[sizeof(FScriptArray)];
		FScriptArray& Array = *reinterpret_cast<FScriptArray*>(ArrayStorage);
		ASSERT_THAT(IsTrue(Operations->ConstructValue(&Array, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Operations->ArrayAddCopy(Array, &String, Diagnostic), *Diagnostic)); ASSERT_THAT(AreEqual(1, Array.Num()));
		const void* Element = Operations->ArrayElement(Array, 0, Diagnostic); ASSERT_THAT(IsNotNull(Element));
		ASSERT_THAT(AreEqual(String, *static_cast<const FString*>(Element)));
		ASSERT_THAT(IsTrue(Operations->DestroyValue(&Array, Diagnostic), *Diagnostic));

		const auto Snapshot = Owner->GetBindingInstallation()->GetStore().GetReflectedSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		const auto* Reflected = Snapshot->Find(URuntimeBindingFunctionFixtureChild::StaticClass());
		ASSERT_THAT(IsNotNull(Reflected));
		auto* ReflectedType = Owner->GetBindingInstallation()->FindType(
			Reflected->Declaration.Name, Reflected->Declaration.Namespace);
		ASSERT_THAT(IsNotNull(ReflectedType)); auto* Object = NewObject<URuntimeBindingFunctionFixtureChild>();
		int32 A = 2, B = 3, Result = 0; void* Arguments[] = {&A, &B};
		const EAngelscriptReflectedParameterDirection Directions[] = {
			EAngelscriptReflectedParameterDirection::In, EAngelscriptReflectedParameterDirection::In};
		ASSERT_THAT(IsTrue(Owner->GetBindingInstallation()->InvokeReflectedFunction(ReflectedType,
			URuntimeBindingFunctionFixtureChild::StaticClass(), Object, TEXT("Add"), Arguments, Directions,
			&Result, nullptr, Diagnostic), *Diagnostic));
		ASSERT_THAT(AreEqual(5, Result));
	}

	TEST_METHOD(FinalizationAndNativeFailuresPublishNoOwnerWithSourceDiagnostic)
	{
		{
			auto Store = MakeShared<FAngelscriptTypeBindInfoStore>(); FString SealDiagnostic;
			FAngelscriptBindEffect Failure; Failure.Kind = EAngelscriptBindEffectKind::DirectBindArchitectureProbe;
			Failure.SecondaryName = TEXT("inject-failure");
			Failure.Source = {TEXT("AngelscriptTest"), TEXT("FullRuntime.FinalizationFailure"),
				TEXT("RuntimeBindingFullRuntimeTests.cpp"), __LINE__, EAngelscriptBindPhase::Finalization};
			ASSERT_THAT(IsTrue(Store->RecordEffect(MoveTemp(Failure), SealDiagnostic), *SealDiagnostic));
			ASSERT_THAT(IsTrue(Store->Seal(SealDiagnostic), *SealDiagnostic));
			FAngelscriptBindingDiagnostic Diagnostic; auto Owner = FAngelscriptEngine::CreateForBindings(Store, Diagnostic);
			ASSERT_THAT(IsNull(Owner.Get())); ASSERT_THAT(IsTrue(Diagnostic.Message.Contains(TEXT("FullRuntime.FinalizationFailure")), *Diagnostic.Message));
		}
		{
			auto Store = MakeShared<FAngelscriptTypeBindInfoStore>(); FString SealDiagnostic;
			Store->SetSource({TEXT("AngelscriptTest"), TEXT("FullRuntime.NativeFailure"),
				TEXT("RuntimeBindingFullRuntimeTests.cpp"), __LINE__, EAngelscriptBindPhase::ExplicitBindings});
			FAngelscriptBinds Binds(*Store);
			Binds.BindGlobalFunctionDirectForTarget("void MissingNative()", asSFuncPtr(), asCALL_CDECL,
				ASAutoCaller::FunctionCaller::Make());
			ASSERT_THAT(IsTrue(Store->Seal(SealDiagnostic), *SealDiagnostic));
			FAngelscriptBindingDiagnostic Diagnostic; auto Owner = FAngelscriptEngine::CreateForBindings(Store, Diagnostic);
			ASSERT_THAT(IsNull(Owner.Get())); ASSERT_THAT(IsTrue(Diagnostic.Message.Contains(TEXT("FullRuntime.NativeFailure")), *Diagnostic.Message));
		}
	}

	TEST_METHOD(FullRuntimeManifestIsValidatedAndWrittenFromInstalledSnapshot)
	{
		FString Diagnostic;
		auto Owner = Create(Diagnostic);
		ASSERT_THAT(IsTrue(Owner.IsValid(), *Diagnostic));
		const auto Store = Owner->GetBindingInstallation()->GetStoreShared();
		FAngelscriptBindingInspection Inspection;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(
			Store, EAngelscriptBindingInspectionScope::FullRuntime, Inspection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Inspection.bSealed && Inspection.bValidationAttempted && Inspection.bValid));
		ASSERT_THAT(IsFalse(Inspection.bCollectionOnly));
		ASSERT_THAT(IsTrue(IsAcceptedFullRuntime(Inspection)));
		ASSERT_THAT(IsTrue(Inspection.Policy.bEditor == !!WITH_EDITOR));
		ASSERT_THAT(IsTrue(Inspection.Policy.bShipping == !!UE_BUILD_SHIPPING));
		ASSERT_THAT(IsNotNull(Inspection.Types.FindByPredicate([](const auto& Type) { return Type.Identity == TEXT("FVector"); })));
		ASSERT_THAT(IsTrue(!Inspection.ReflectedTypes.IsEmpty()));
		ASSERT_THAT(IsTrue(Inspection.ReflectedTypes.ContainsByPredicate([&Inspection](const auto& Reflected)
		{
			return !Reflected.IntendedIdentity.IsEmpty()
				&& Inspection.Types.ContainsByPredicate([&Reflected](const auto& Type) { return Type.Identity == Reflected.IntendedIdentity; });
		})));
		FAngelscriptBindingDumpResult Dump;
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Harness/RuntimeBindings/FullRuntime/Current");
		IFileManager::Get().DeleteDirectory(*Directory, false, true);
		ASSERT_THAT(IsTrue(FAngelscriptBindingDump::ExportToDirectory(Inspection, Directory, Dump, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Dump.Files.Num() >= 2));
		ASSERT_THAT(IsTrue(!Dump.SemanticJson.IsEmpty()));
		FString SecondJson; TMap<FString, FString> SecondTables;
		ASSERT_THAT(IsTrue(FAngelscriptBindingDump::Serialize(Inspection, SecondJson, SecondTables, Diagnostic), *Diagnostic));
		ASSERT_THAT(AreEqual(Dump.SemanticJson, SecondJson));
		FAngelscriptBindingInspection CollectionOnly;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectCollection(
			FAngelscriptBind::GetRegisteredCollection(), CollectionOnly, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsFalse(IsAcceptedFullRuntime(CollectionOnly)));
	}
};
#endif
