#include "CQTest.h"
#include "Core/AngelscriptTypeBindInfoReflection.h"
#include "Core/AngelscriptTypeBindInfoDraft.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Components/StaticMeshComponent.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "UObject/GarbageCollection.h"
#include "Async/Async.h"
#include "as_objecttype.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(Definitions, "Angelscript.UnitTest.RuntimeBindings.Reflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static UScriptStruct* StructFixture(bool bString = false)
	{
		auto* Struct = NewObject<UScriptStruct>(GetTransientPackage());
		Struct->StructFlags = STRUCT_NoExport; Struct->SetMetaData(TEXT("ForceAngelscriptBind"), TEXT("true"));
		if (bString)
		{
			auto* Text = new FStrProperty(Struct, TEXT("Text")); Text->SetPropertyFlags(CPF_BlueprintVisible); Struct->AddCppProperty(Text);
		}
		else
		{
			auto* X = new FIntProperty(Struct, TEXT("X")); X->SetPropertyFlags(CPF_BlueprintVisible); Struct->AddCppProperty(X);
			auto* Object = new FObjectProperty(Struct, TEXT("Object")); Object->PropertyClass = UObject::StaticClass();
			Object->SetPropertyFlags(CPF_BlueprintVisible); Struct->AddCppProperty(Object);
		}
		Struct->StaticLink(true); return Struct;
	}
	static UEnum* EnumFixture()
	{
		auto* Enum = NewObject<UEnum>(GetTransientPackage());
		TArray<TPair<FName, int64>> Values = {{TEXT("Fixture::First"), -3}, {TEXT("Fixture::Large"), 1234567890123LL}};
		Enum->SetEnums(Values, UEnum::ECppForm::EnumClass, UEnum::EUnderlyingType::int64, EEnumFlags::None, UEnum::EAddMaxKeyIfMissing::No);
		return Enum;
	}
	static const FAngelscriptReflectedProperty* Property(const FAngelscriptReflectedTypeRecord& Record, const TCHAR* Name)
	{
		return Record.Properties.FindByPredicate([Name](const auto& P) { return P.Name == Name; });
	}
public:
	TEST_METHOD(DerivedBeforeBaseRecordsRealNominalParentWithoutEngine)
	{
		UClass* Derived = UStaticMeshComponent::StaticClass(); UClass* Base = Derived->GetSuperClass();
		TArray<UObject*> Roots = {Derived, Base}; TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture(Roots, {}, Snapshot, Diagnostic)));
		const auto* Record = Snapshot->Find(Derived); ASSERT_THAT(IsNotNull(Record)); ASSERT_THAT(IsTrue(Record->bEligible));
		ASSERT_THAT(IsTrue(Record->Parent == Base)); ASSERT_THAT(IsNotNull(Snapshot->Find(Base)));
		ASSERT_THAT(AreEqual(Derived->GetPathName(), Record->NativePath));
		auto Store = MakeShared<FAngelscriptTypeBindInfoStore>();
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::RecordDeclarations(*Store, Snapshot.ToSharedRef(), Diagnostic)));
		ASSERT_THAT(IsTrue(Store->Seal(Diagnostic))); TUniquePtr<FAngelscriptTypeBindInfoDraft> Draft;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoDraft::Create(Store, Draft, Diagnostic)));
		auto* Type = static_cast<AS_NAMESPACE_QUALIFIER asCObjectType*>(Draft->FindType(Record->Declaration.Name)); ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Type->derivedFrom == Draft->FindType(Snapshot->Find(Base)->Declaration.Name)));
		ASSERT_THAT(IsNull(Draft->GetImage()->GetBoundEngine())); ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
	}
	TEST_METHOD(StructLayoutAndPropertyUsesMatchNativeReflection)
	{
		auto* Struct = StructFixture(); TArray<UObject*> Roots = {Struct}; TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture(Roots, {}, Snapshot, Diagnostic)));
		const auto* Record = Snapshot->Find(Struct); ASSERT_THAT(IsNotNull(Record)); ASSERT_THAT(IsTrue(Record->bEligible));
		ASSERT_THAT(IsTrue(Record->Kind == EAngelscriptReflectedKind::Struct));
		ASSERT_THAT(AreEqual(uint32(Struct->GetStructureSize()), Record->Declaration.NativeSize));
		ASSERT_THAT(AreEqual(uint32(Struct->GetMinAlignment()), Record->Declaration.NativeAlignment));
		const auto* X = Property(*Record, TEXT("X")); ASSERT_THAT(IsNotNull(X));
		auto* NativeX = FindFProperty<FIntProperty>(Struct, TEXT("X")); ASSERT_THAT(IsTrue(X->NativeProperty == NativeX));
		ASSERT_THAT(AreEqual(NativeX->GetOffset_ForInternal(), X->Offset)); ASSERT_THAT(AreEqual(FString(TEXT("IntProperty")), X->Type.Kind));
		const auto* Object = Property(*Record, TEXT("Object")); ASSERT_THAT(IsNotNull(Object));
		ASSERT_THAT(IsTrue(Object->Type.ReferencedType == UObject::StaticClass())); ASSERT_THAT(IsTrue(Record->Lifetime.bHasGCReferences));
		ASSERT_THAT(IsTrue(Record->Declaration.Source.Provider == FName(TEXT("UStruct.TypeDeclarations"))));
	}
	TEST_METHOD(EnumNamesAndInt64ValuesAreDetachedFromLaterReflectionMutation)
	{
		auto* Enum = EnumFixture(); TArray<UObject*> Roots = {Enum}; TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture(Roots, {}, Snapshot, Diagnostic)));
		const auto* Record = Snapshot->Find(Enum); ASSERT_THAT(IsNotNull(Record)); ASSERT_THAT(AreEqual(2, Record->EnumValues.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("First")), Record->EnumValues[0].Key)); ASSERT_THAT(AreEqual(int64(-3), Record->EnumValues[0].Value));
		ASSERT_THAT(AreEqual(int64(1234567890123LL), Record->EnumValues[1].Value));
		TArray<TPair<FName, int64>> Replaced = {{TEXT("Fixture::Changed"), 7}};
		Enum->SetEnums(Replaced, UEnum::ECppForm::EnumClass, UEnum::EUnderlyingType::int64, EEnumFlags::None, UEnum::EAddMaxKeyIfMissing::No);
		ASSERT_THAT(AreEqual(2, Record->EnumValues.Num())); ASSERT_THAT(AreEqual(FString(TEXT("Large")), Record->EnumValues[1].Key));
		ASSERT_THAT(AreEqual(int64(1234567890123LL), Record->EnumValues[1].Value));
	}
	TEST_METHOD(DelegateSignatureKeepsReturnAndReferenceFlagsWithoutInvocation)
	{
		auto* Signature = NewObject<UDelegateFunction>(GetTransientPackage()); Signature->FunctionFlags = FUNC_Delegate;
		auto* Result = new FIntProperty(Signature, TEXT("ReturnValue")); Result->SetPropertyFlags(CPF_Parm | CPF_ReturnParm); Signature->AddCppProperty(Result);
		auto* Input = new FIntProperty(Signature, TEXT("Input")); Input->SetPropertyFlags(CPF_Parm | CPF_ReferenceParm | CPF_ConstParm); Signature->AddCppProperty(Input);
		Signature->StaticLink(true); TArray<UObject*> Roots = {Signature}; TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture(Roots, {}, Snapshot, Diagnostic)));
		const auto* Record = Snapshot->Find(Signature); ASSERT_THAT(IsNotNull(Record)); ASSERT_THAT(IsTrue(Record->Kind == EAngelscriptReflectedKind::Delegate));
		ASSERT_THAT(AreEqual(Signature->GetPathName(), Record->NativePath)); ASSERT_THAT(AreEqual(2, Record->Properties.Num()));
		const auto* Return = Property(*Record, TEXT("ReturnValue")); const auto* Param = Property(*Record, TEXT("Input"));
		ASSERT_THAT(IsNotNull(Return)); ASSERT_THAT(IsNotNull(Param)); ASSERT_THAT(IsTrue((Return->Flags & CPF_ReturnParm) != 0));
		ASSERT_THAT(IsTrue((Param->Flags & (CPF_ReferenceParm | CPF_ConstParm)) == (CPF_ReferenceParm | CPF_ConstParm)));
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
	}
	TEST_METHOD(EditorOnlyTypeHasExplicitCookedPolicyExclusion)
	{
		auto* Package = CreatePackage(*FString::Printf(TEXT("/Temp/BindingDefinitions_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		Package->SetPackageFlags(PKG_EditorOnly); auto* Class = NewObject<UClass>(Package);
		Class->ClassFlags |= CLASS_Native; Class->SetSuperStruct(UObject::StaticClass()); Class->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		FAngelscriptBindRecordingPolicy Policy; Policy.bEditor = false; Policy.bSimulateCooked = true;
		TArray<UObject*> Roots = {Class}; TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture(Roots, Policy, Snapshot, Diagnostic)));
		const auto* Record = Snapshot->Find(Class); ASSERT_THAT(IsNotNull(Record)); ASSERT_THAT(IsFalse(Record->bEligible));
		ASSERT_THAT(IsTrue(Record->ExclusionReason.Contains(TEXT("editor"), ESearchCase::IgnoreCase)));
		ASSERT_THAT(IsTrue(Snapshot->GetPolicy().bSimulateCooked));
	}
	TEST_METHOD(SnapshotAndRecordedStoreRetainTemporaryReflectionUntilRelease)
	{
		TWeakObjectPtr<UScriptStruct> Weak; TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		auto Store = MakeShared<FAngelscriptTypeBindInfoStore>();
		{
			auto* Struct = StructFixture(); Weak = Struct; TArray<UObject*> Roots = {Struct};
			ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture(Roots, {}, Snapshot, Diagnostic)));
			ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::RecordDeclarations(*Store, Snapshot.ToSharedRef(), Diagnostic)));
			ASSERT_THAT(IsTrue(Store->Seal(Diagnostic)));
		}
		Snapshot.Reset(); CollectGarbage(RF_NoFlags); ASSERT_THAT(IsTrue(Weak.IsValid()));
		ASSERT_THAT(IsNotNull(Store->GetReflectedSnapshot()->Find(Weak.Get())));
		// Releasing the only store owner must also release its reflection pins.
		TSharedPtr<FAngelscriptTypeBindInfoStore> StoreOwner = Store; Store = MakeShared<FAngelscriptTypeBindInfoStore>(); StoreOwner.Reset();
		CollectGarbage(RF_NoFlags); ASSERT_THAT(IsFalse(Weak.IsValid()));
	}
	TEST_METHOD(LoadedCaptureCanBeReadRepeatedlyWithNoCurrentEngine)
	{
		TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::CaptureLoaded({}, Snapshot, Diagnostic)));
		const auto* First = Snapshot->Find(UObject::StaticClass()); ASSERT_THAT(IsNotNull(First));
		ASSERT_THAT(IsTrue(First->bEligible)); ASSERT_THAT(AreEqual(FString(TEXT("UObject")), First->Declaration.Name));
		for (int32 Iteration = 0; Iteration < 3; ++Iteration)
		{
			ASSERT_THAT(IsTrue(First == Snapshot->Find(UObject::StaticClass())));
			ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
		}
	}
	TEST_METHOD(StructLifetimeRecipeInitializesCopiesAndDestroysNativeStringStorage)
	{
		auto* Struct = StructFixture(true); TArray<UObject*> Roots = {Struct}; TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture(Roots, {}, Snapshot, Diagnostic)));
		const auto* Record = Snapshot->Find(Struct); ASSERT_THAT(IsNotNull(Record)); const auto& Recipe = Record->Lifetime;
		ASSERT_THAT(IsTrue(Recipe.Initialize != nullptr && Recipe.Copy != nullptr && Recipe.Destroy != nullptr)); ASSERT_THAT(IsTrue(Recipe.bNeedsDestruction));
		void* A = FMemory::Malloc(Struct->GetStructureSize(), Struct->GetMinAlignment()); void* B = FMemory::Malloc(Struct->GetStructureSize(), Struct->GetMinAlignment());
		Recipe.Initialize(Struct, A); Recipe.Initialize(Struct, B); auto* Field = FindFProperty<FStrProperty>(Struct, TEXT("Text"));
		Field->SetPropertyValue_InContainer(A, TEXT("initial")); Recipe.Copy(Struct, B, A); Field->SetPropertyValue_InContainer(A, TEXT("changed"));
		const FString Copied = Field->GetPropertyValue_InContainer(B); Recipe.Destroy(Struct, A); Recipe.Destroy(Struct, B); FMemory::Free(A); FMemory::Free(B);
		ASSERT_THAT(AreEqual(FString(TEXT("initial")), Copied));
	}
	TEST_METHOD(WorkerThreadCaptureFailsBeforeTouchingReflection)
	{
		const auto Result = Async(EAsyncExecution::ThreadPool, []
		{
			TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
			const bool bCaptured = FAngelscriptTypeBindInfoReflection::CaptureLoaded({}, Snapshot, Diagnostic);
			return !bCaptured && !Snapshot.IsValid() && Diagnostic.Contains(TEXT("GameThread"));
		}).Get();
		ASSERT_THAT(IsTrue(Result));
	}
	TEST_METHOD(NativeInterfaceIdentityAndImplementationRelationshipAreCaptured)
	{
		TArray<UObject*> Roots = {UActorComponent::StaticClass()}; TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture(Roots, {}, Snapshot, Diagnostic)));
		const auto* Component = Snapshot->Find(UActorComponent::StaticClass()); ASSERT_THAT(IsNotNull(Component));
		ASSERT_THAT(IsTrue(Component->Interfaces.Contains(UInterface_AssetUserData::StaticClass())));
		const auto* Interface = Snapshot->Find(UInterface_AssetUserData::StaticClass()); ASSERT_THAT(IsNotNull(Interface));
		ASSERT_THAT(IsTrue(Interface->Kind == EAngelscriptReflectedKind::Interface)); ASSERT_THAT(IsTrue(Interface->bEligible));
		ASSERT_THAT(IsTrue(Interface->NativeType == UInterface_AssetUserData::StaticClass()));
	}
	TEST_METHOD(ContainerPropertyRetainsNestedNominalDependencyShape)
	{
		auto* Struct = StructFixture(); auto* Array = new FArrayProperty(Struct, TEXT("Objects"));
		auto* Inner = new FObjectProperty(Array, TEXT("Element")); Inner->PropertyClass = UObject::StaticClass(); Array->Inner = Inner;
		Array->SetPropertyFlags(CPF_BlueprintVisible); Struct->AddCppProperty(Array); Struct->StaticLink(true);
		TArray<UObject*> Roots = {Struct}; TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture(Roots, {}, Snapshot, Diagnostic)));
		const auto* Record = Snapshot->Find(Struct); ASSERT_THAT(IsNotNull(Record)); const auto* Objects = Property(*Record, TEXT("Objects"));
		ASSERT_THAT(IsNotNull(Objects)); ASSERT_THAT(AreEqual(FString(TEXT("ArrayProperty")), Objects->Type.Kind));
		ASSERT_THAT(AreEqual(1, Objects->Type.Arguments.Num())); ASSERT_THAT(AreEqual(FString(TEXT("ObjectProperty")), Objects->Type.Arguments[0].Kind));
		ASSERT_THAT(IsTrue(Objects->Type.Arguments[0].ReferencedType == UObject::StaticClass()));
	}
	TEST_METHOD(OriginalDeclarationProvidersReuseTheDetachedSnapshot)
	{
		auto* Struct = StructFixture(); auto* Enum = EnumFixture(); TArray<UObject*> Roots = {UStaticMeshComponent::StaticClass(), Struct, Enum};
		TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture(Roots, {}, Snapshot, Diagnostic)));
		auto Store = MakeShared<FAngelscriptTypeBindInfoStore>(); ASSERT_THAT(IsTrue(Store->SetReflectedSnapshot(Snapshot.ToSharedRef(), Diagnostic)));
		FAngelscriptBinds Binds(*Store); int32 Providers = 0;
		for (const auto& Provider : FAngelscriptBind::GetRegisteredCollection().GetRecords())
		{
			if (Provider.BindName != TEXT("BlueprintType.ReferenceClasses") && Provider.BindName != TEXT("UStruct.TypeDeclarations") && Provider.BindName != TEXT("Enums")) continue;
			Provider.Callback(Binds); ++Providers; ASSERT_THAT(IsFalse(Store->HasFailed()));
		}
		ASSERT_THAT(AreEqual(3, Providers)); ASSERT_THAT(IsTrue(Store->Seal(Diagnostic)));
		for (UObject* Native : Roots) ASSERT_THAT(IsNotNull(Store->GetType(Store->FindType(Snapshot->Find(Native)->Declaration.Name))));
		ASSERT_THAT(IsTrue(Store->GetReflectedSnapshot().Get() == Snapshot.Get())); ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
	}
	TEST_METHOD(RecordingRejectsSnapshotWithDifferentEffectivePolicy)
	{
		FAngelscriptBindRecordingPolicy Policy; Policy.bSimulateCooked = true;
		TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture({}, Policy, Snapshot, Diagnostic)));
		auto Store = MakeShared<FAngelscriptTypeBindInfoStore>();
		ASSERT_THAT(IsFalse(FAngelscriptTypeBindInfoReflection::RecordDeclarations(*Store, Snapshot.ToSharedRef(), Diagnostic)));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("policy"), ESearchCase::IgnoreCase)));
		ASSERT_THAT(IsFalse(Store->GetReflectedSnapshot().IsValid())); ASSERT_THAT(AreEqual(0, Store->GetTypes().Num()));
	}
};
#endif
