#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptTypeBindInfoReflection.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "UObject/EnumProperty.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/GarbageCollection.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingObjectsFixture
{
struct FObjectsFixture
{
	UPackage* Package = nullptr;
	UClass* Parent = nullptr;
	UClass* Child = nullptr;
	UScriptStruct* Struct = nullptr;
	UEnum* Enum = nullptr;
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	FObjectsFixture()
	{
		const FString FixtureId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		Package = CreatePackage(*FString::Printf(TEXT("/Temp/RuntimeBindingObjects_%s"), *FixtureId));
		Parent = NewObject<UClass>(Package, *FString::Printf(TEXT("BindingParent_%s"), *FixtureId)); Parent->ClassFlags |= CLASS_Native; Parent->ClassFlags &= ~CLASS_Abstract; Parent->ClassWithin = UObject::StaticClass(); Parent->ClassConstructor = UObject::StaticClass()->ClassConstructor; Parent->SetSuperStruct(UObject::StaticClass()); Parent->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		auto* ParentValue = new FIntProperty(Parent, TEXT("ParentValue")); ParentValue->SetPropertyFlags(CPF_BlueprintVisible); Parent->AddCppProperty(ParentValue); Parent->StaticLink(true);
		Child = NewObject<UClass>(Package, *FString::Printf(TEXT("BindingChild_%s"), *FixtureId)); Child->ClassFlags |= CLASS_Native; Child->ClassFlags &= ~CLASS_Abstract; Child->ClassWithin = UObject::StaticClass(); Child->ClassConstructor = UObject::StaticClass()->ClassConstructor; Child->SetSuperStruct(Parent); Child->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		auto* ChildValue = new FIntProperty(Child, TEXT("ChildValue")); ChildValue->SetPropertyFlags(CPF_BlueprintVisible); Child->AddCppProperty(ChildValue);
		auto* ReadOnly = new FIntProperty(Child, TEXT("ReadOnlyValue")); ReadOnly->SetPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly); Child->AddCppProperty(ReadOnly); Child->StaticLink(true);

		Enum = NewObject<UEnum>(Package, *FString::Printf(TEXT("BindingMode_%s"), *FixtureId));
		TArray<TPair<FName, int64>> EnumValues = {{TEXT("BindingMode::Off"), 0}, {TEXT("BindingMode::On"), 1}};
		Enum->SetEnums(EnumValues, UEnum::ECppForm::EnumClass,
			UEnum::EUnderlyingType::uint8, EEnumFlags::None, UEnum::EAddMaxKeyIfMissing::No);
		Struct = NewObject<UScriptStruct>(Package, *FString::Printf(TEXT("BindingPayload_%s"), *FixtureId)); Struct->StructFlags = STRUCT_NoExport; Struct->SetMetaData(TEXT("ForceAngelscriptBind"), TEXT("true"));
		auto* Text = new FStrProperty(Struct, TEXT("Text")); Text->SetPropertyFlags(CPF_BlueprintVisible); Struct->AddCppProperty(Text);
		auto* Mode = new FByteProperty(Struct, TEXT("Mode")); Mode->Enum = Enum; Mode->SetPropertyFlags(CPF_BlueprintVisible); Struct->AddCppProperty(Mode); Struct->StaticLink(true);
	}

	bool Create()
	{
		TArray<UObject*> Roots = {Child, Parent, Struct, Enum, UActorComponent::StaticClass()};
		TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot;
		if (!FAngelscriptTypeBindInfoReflection::Capture(Roots, {}, Snapshot, Diagnostic)) return false;
		if (!FAngelscriptTypeBindInfoReflection::RecordDeclarations(*Store, Snapshot.ToSharedRef(), Diagnostic)) return false;
		if (!Store->Seal(Diagnostic)) return false;
		FAngelscriptBindingDiagnostic BindingDiagnostic; Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) Diagnostic = BindingDiagnostic.Message; return Owner.IsValid();
	}

	const FAngelscriptReflectedTypeRecord* Record(UObject* Native) const { return Store->GetReflectedSnapshot()->Find(Native); }
	asCTypeInfo* Type(UObject* Native) const
	{
		const auto* R = Record(Native); return R ? Owner->GetBindingInstallation()->FindType(R->Declaration.Name, R->Declaration.Namespace) : nullptr;
	}
	const FAngelscriptTypeBindInfoInstallation& Install() const { return *Owner->GetBindingInstallation(); }
};
}

using namespace RuntimeBindingObjectsFixture;
TEST_CLASS_WITH_FLAGS(Objects, "Angelscript.UnitTest.RuntimeBindings.Reflection.Objects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ParentAndChildPropertiesAccessOnlyTheirObject)
	{
		FObjectsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); UObject* Object = NewObject<UObject>(GetTransientPackage(), F.Child); ASSERT_THAT(IsNotNull(Object));
		int32 Eleven = 11, Thirteen = 13; const bool bParentWritten = F.Install().WriteReflectedProperty(F.Type(F.Parent), F.Parent, Object, TEXT("ParentValue"), &Eleven, F.Diagnostic); const FString ParentDiagnostic = F.Diagnostic;
		ASSERT_THAT(IsTrue(bParentWritten, *ParentDiagnostic));
		ASSERT_THAT(IsTrue(F.Install().WriteReflectedProperty(F.Type(F.Child), F.Child, Object, TEXT("ChildValue"), &Thirteen, F.Diagnostic)));
		int32 ParentRead = 0, ChildRead = 0; ASSERT_THAT(IsTrue(F.Install().ReadReflectedProperty(F.Type(F.Parent), F.Parent, Object, TEXT("ParentValue"), &ParentRead, F.Diagnostic)));
		ASSERT_THAT(IsTrue(F.Install().ReadReflectedProperty(F.Type(F.Child), F.Child, Object, TEXT("ChildValue"), &ChildRead, F.Diagnostic)));
		ASSERT_THAT(AreEqual(11, ParentRead)); ASSERT_THAT(AreEqual(13, ChildRead));
	}

	TEST_METHOD(InterfaceLookupIdentifiesConcreteImplementation)
	{
		FObjectsFixture F; const bool bCreated = F.Create(); const FString CreateDiagnostic = F.Diagnostic; ASSERT_THAT(IsTrue(bCreated, *CreateDiagnostic));
		ASSERT_THAT(IsTrue(F.Install().ReflectedTypeImplements(F.Type(UActorComponent::StaticClass()), UActorComponent::StaticClass(), UInterface_AssetUserData::StaticClass(), F.Diagnostic)));
		asCTypeInfo* ComponentType = F.Type(UActorComponent::StaticClass()); ASSERT_THAT(IsTrue(ComponentType->GetInterfaceCount() > 0));
		bool bFoundInterface = false; for (asUINT Index = 0; Index < ComponentType->GetInterfaceCount(); ++Index)
			bFoundInterface |= FString(UTF8_TO_TCHAR(ComponentType->GetInterface(Index)->GetName())) == F.Record(UInterface_AssetUserData::StaticClass())->Declaration.Name;
		ASSERT_THAT(IsTrue(bFoundInterface));
	}

	TEST_METHOD(NativeStructCopyOwnsIndependentStringStorage)
	{
		FObjectsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); void* A = FMemory::Malloc(F.Struct->GetStructureSize(), F.Struct->GetMinAlignment()); void* B = FMemory::Malloc(F.Struct->GetStructureSize(), F.Struct->GetMinAlignment());
		ASSERT_THAT(IsTrue(F.Install().InitializeReflectedStruct(F.Type(F.Struct), F.Struct, A, F.Diagnostic))); ASSERT_THAT(IsTrue(F.Install().InitializeReflectedStruct(F.Type(F.Struct), F.Struct, B, F.Diagnostic)));
		auto* Text = FindFProperty<FStrProperty>(F.Struct, TEXT("Text")); Text->SetPropertyValue_InContainer(A, TEXT("initial")); ASSERT_THAT(IsTrue(F.Install().CopyReflectedStruct(F.Type(F.Struct), F.Struct, B, A, F.Diagnostic)));
		Text->SetPropertyValue_InContainer(A, TEXT("changed")); ASSERT_THAT(AreEqual(FString(TEXT("initial")), Text->GetPropertyValue_InContainer(B)));
		ASSERT_THAT(IsTrue(F.Install().DestroyReflectedStruct(F.Type(F.Struct), F.Struct, A, F.Diagnostic))); ASSERT_THAT(IsTrue(F.Install().DestroyReflectedStruct(F.Type(F.Struct), F.Struct, B, F.Diagnostic))); FMemory::Free(A); FMemory::Free(B);
	}

	TEST_METHOD(EnumPropertyRoundTripsUnderlyingValue)
	{
		FObjectsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); void* Value = FMemory::Malloc(F.Struct->GetStructureSize(), F.Struct->GetMinAlignment()); ASSERT_THAT(IsTrue(F.Install().InitializeReflectedStruct(F.Type(F.Struct), F.Struct, Value, F.Diagnostic)));
		uint8 On = 1, Read = 0; ASSERT_THAT(IsTrue(F.Install().WriteReflectedProperty(F.Type(F.Struct), F.Struct, Value, TEXT("Mode"), &On, F.Diagnostic))); ASSERT_THAT(IsTrue(F.Install().ReadReflectedProperty(F.Type(F.Struct), F.Struct, Value, TEXT("Mode"), &Read, F.Diagnostic)));
		ASSERT_THAT(AreEqual(uint8(1), Read)); ASSERT_THAT(IsTrue(F.Install().DestroyReflectedStruct(F.Type(F.Struct), F.Struct, Value, F.Diagnostic))); FMemory::Free(Value);
	}

	TEST_METHOD(ReadOnlyPropertyRejectsMutation)
	{
		FObjectsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); UObject* Object = NewObject<UObject>(GetTransientPackage(), F.Child); int32 Value = 7;
		ASSERT_THAT(IsFalse(F.Install().WriteReflectedProperty(F.Type(F.Child), F.Child, Object, TEXT("ReadOnlyValue"), &Value, F.Diagnostic))); ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("read-only"))));
	}

	TEST_METHOD(TypeFromDifferentOwnerIsRejected)
	{
		FObjectsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); FAngelscriptBindingDiagnostic BindingDiagnostic; TUniquePtr<FAngelscriptEngine> Other = FAngelscriptEngine::CreateForBindings(F.Store, BindingDiagnostic); ASSERT_THAT(IsTrue(Other.IsValid(), *BindingDiagnostic.Message));
		const auto* R = F.Record(F.Child); asCTypeInfo* Foreign = Other->GetBindingInstallation()->FindType(R->Declaration.Name, R->Declaration.Namespace); UObject* Object = NewObject<UObject>(GetTransientPackage(), F.Child); int32 Read = 0;
		ASSERT_THAT(IsFalse(F.Install().ReadReflectedProperty(Foreign, F.Child, Object, TEXT("ChildValue"), &Read, F.Diagnostic))); ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("different binding owner"))));
	}

	TEST_METHOD(EngineOwnedReferenceRetainsAndReleasesUObject)
	{
		FObjectsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); TWeakObjectPtr<UObject> Weak;
		{
			UObject* Object = NewObject<UStaticMeshComponent>(); Weak = Object; ASSERT_THAT(IsTrue(F.Install().RetainReflectedObject(Object, F.Diagnostic))); Object = nullptr;
		}
		CollectGarbage(RF_NoFlags); ASSERT_THAT(IsTrue(Weak.IsValid())); F.Owner.Reset(); CollectGarbage(RF_NoFlags); ASSERT_THAT(IsFalse(Weak.IsValid()));
	}
};
#endif
