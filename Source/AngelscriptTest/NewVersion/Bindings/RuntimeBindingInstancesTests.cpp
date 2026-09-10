#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Containers/ScriptArray.h"
#include "UObject/UObjectGlobals.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingInstancesFixture
{
struct FCounted
{
	static int32 Copies;
	static int32 Destructions;
	int32 Value = 0;
	FCounted() = default;
	explicit FCounted(int32 InValue) : Value(InValue) {}
	FCounted(const FCounted& Other) : Value(Other.Value) { ++Copies; }
	~FCounted() { ++Destructions; }
	bool operator==(const FCounted& Other) const { return Value == Other.Value; }
};
int32 FCounted::Copies = 0;
int32 FCounted::Destructions = 0;
inline uint32 GetTypeHash(const FCounted& Value) { return ::GetTypeHash(Value.Value); }

struct FNoCopy
{
	FNoCopy() = default;
	FNoCopy(const FNoCopy&) = delete;
	~FNoCopy() {}
};

struct FNoHash
{
	int32 Value = 0;
	bool operator==(const FNoHash& Other) const { return Value == Other.Value; }
};

struct FArrayHolder
{
	FScriptArray Value;
};

struct FInstanceFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Record(bool bCounted = false, bool bNoCopy = false, bool bNoHash = false,
		const TCHAR* InitialUse = nullptr)
	{
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return false;
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(
			FAngelscriptBind::GetRegisteredCollection(), *Store, Diagnostic)) return false;
		Store->SetSource({TEXT("FixtureModule"), TEXT("TemplateFixture"), TEXT("Bindings/RuntimeBindingInstancesTests.cpp"), 40});
		FAngelscriptBinds Binds(*Store);
		if (bCounted) Binds.ValueClassForTarget<FCounted>("FCounted", FBindFlags());
		if (bNoCopy) Binds.ValueClassForTarget<FNoCopy>("FNoCopy", FBindFlags());
		if (bNoHash) Binds.ValueClassForTarget<FNoHash>("FNoHash", FBindFlags());
		if (InitialUse)
			Binds.ValueClassForTarget<FArrayHolder>("FArrayHolder", FBindFlags())
				.Property(FString::Printf(TEXT("%s Value"), InitialUse), &FArrayHolder::Value);
		if (!Store->Seal(Diagnostic)) return false;
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) Diagnostic = BindingDiagnostic.Message;
		return Owner.IsValid();
	}

	bool Instance(const TCHAR* Declaration, asCObjectType*& Out)
	{
		return Owner && Owner->GetBindingInstallation()->MaterializeTemplateInstance(
			Declaration, *static_cast<asCScriptEngine*>(Owner->Engine), Out, Diagnostic);
	}
};
}

using namespace RuntimeBindingInstancesFixture;
TEST_CLASS_WITH_FLAGS(Instances, "Angelscript.UnitTest.RuntimeBindings.Containers.Instances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ArrayPrimitiveStringAndNestedArgumentsAreCanonical)
	{
		FInstanceFixture F; ASSERT_THAT(IsTrue(F.Record()));
		asCObjectType *Ints = nullptr, *Strings = nullptr, *Nested = nullptr;
		ASSERT_THAT(IsTrue(F.Instance(TEXT("TArray<int>"), Ints)));
		ASSERT_THAT(IsTrue(F.Instance(TEXT("TArray<FString>"), Strings)));
		ASSERT_THAT(IsTrue(F.Instance(TEXT("TArray<TArray<int>>"), Nested)));
		ASSERT_THAT(IsTrue(Ints != Strings && Strings != Nested));
		ASSERT_THAT(AreEqual(asUINT(1), Ints->GetSubTypeCount()));
		ASSERT_THAT(IsTrue(Ints->GetSubTypeId(0) == asTYPEID_INT32));
		ASSERT_THAT(IsTrue(Strings->GetSubType(0) != nullptr && FString(Strings->GetSubType(0)->GetName()) == TEXT("FString")));
		ASSERT_THAT(IsTrue(Nested->GetSubType(0) != nullptr
			&& Nested->GetSubType(0)->GetSubTypeId(0) == asTYPEID_INT32));
	}

	TEST_METHOD(RepeatedUseSharesOnlyWithinOneOwner)
	{
		FInstanceFixture F; ASSERT_THAT(IsTrue(F.Record())); asCObjectType *First = nullptr, *Second = nullptr;
		ASSERT_THAT(IsTrue(F.Instance(TEXT("TArray<int>"), First)));
		ASSERT_THAT(IsTrue(F.Instance(TEXT("TArray<int>"), Second))); ASSERT_THAT(IsTrue(First == Second));
		FInstanceFixture Other; Other.Store = F.Store; FAngelscriptBindingDiagnostic D;
		Other.Owner = FAngelscriptEngine::CreateForBindings(Other.Store, D); ASSERT_THAT(IsNotNull(Other.Owner.Get()));
		asCObjectType* OtherType = nullptr; ASSERT_THAT(IsTrue(Other.Instance(TEXT("TArray<int>"), OtherType)));
		ASSERT_THAT(IsTrue(OtherType != First));
		ASSERT_THAT(IsTrue(Other.Owner->GetBindingInstallation()->FindTemplateOperations(OtherType)
			!= F.Owner->GetBindingInstallation()->FindTemplateOperations(First)));
	}

	TEST_METHOD(MissingCopyRequirementNamesElementAndProvider)
	{
		FInstanceFixture F; ASSERT_THAT(IsTrue(F.Record(false, true))); asCObjectType* Type = nullptr;
		ASSERT_THAT(IsFalse(F.Instance(TEXT("TArray<FNoCopy>"), Type)));
		ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("FNoCopy")) && F.Diagnostic.Contains(TEXT("TemplateFixture"))
			&& F.Diagnostic.Contains(TEXT("copy"), ESearchCase::IgnoreCase)));
	}

	TEST_METHOD(MissingHashRequirementNamesMapKeyAndProvider)
	{
		FInstanceFixture F; ASSERT_THAT(IsTrue(F.Record(false, false, true))); asCObjectType* Type = nullptr;
		ASSERT_THAT(IsFalse(F.Instance(TEXT("TSet<FNoHash>"), Type)));
		ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("FNoHash")) && F.Diagnostic.Contains(TEXT("TemplateFixture"))
			&& F.Diagnostic.Contains(TEXT("hash"), ESearchCase::IgnoreCase)));
	}

	TEST_METHOD(InitialUseIsMaterializedBeforePrimaryImageFreezes)
	{
		FInstanceFixture F; ASSERT_THAT(IsTrue(F.Record(false, false, false, TEXT("TArray<int>"))));
		auto* Engine = static_cast<asCScriptEngine*>(F.Owner->Engine);
		asCObjectType* Type = nullptr; ASSERT_THAT(IsTrue(F.Instance(TEXT("TArray<int>"), Type)));
		ASSERT_THAT(IsTrue(Type->GetMetadataImage() == F.Owner->GetBindingInstallation()->GetImage()));
		ASSERT_THAT(IsTrue(Engine->FindMetadataType(Type->GetStableKey()) == Type));
	}

	TEST_METHOD(LateInstanceAddsDependentImageWithoutChangingPrimaryImage)
	{
		FInstanceFixture F; ASSERT_THAT(IsTrue(F.Record())); auto* Installation = F.Owner->GetBindingInstallation();
		auto Primary = Installation->GetImage(); const auto Before = Primary->GetState();
		auto* Engine = static_cast<asCScriptEngine*>(F.Owner->Engine); const int32 ImageCount = Engine->GetRegisteredMetadataImages().Num();
		asCObjectType* Type = nullptr; ASSERT_THAT(IsTrue(F.Instance(TEXT("TArray<FString>"), Type)));
		ASSERT_THAT(IsTrue(Primary->GetState() == Before && Primary->GetState() == asEMetadataImageState::Attached));
		ASSERT_THAT(AreEqual(ImageCount + 1, Engine->GetRegisteredMetadataImages().Num()));
		ASSERT_THAT(IsTrue(Type->GetMetadataImage() != Primary));
	}

	TEST_METHOD(CountedArrayCopyAndDestructionUseElementRecipesExactlyOnce)
	{
		FCounted::Copies = 0; FCounted::Destructions = 0;
		FInstanceFixture F; ASSERT_THAT(IsTrue(F.Record(true))); asCObjectType* Type = nullptr;
		ASSERT_THAT(IsTrue(F.Instance(TEXT("TArray<FCounted>"), Type)));
		const auto* Ops = F.Owner->GetBindingInstallation()->FindTemplateOperations(Type); ASSERT_THAT(IsNotNull(Ops));
		alignas(FScriptArray) uint8 AStorage[sizeof(FScriptArray)], BStorage[sizeof(FScriptArray)];
		auto* A = new(AStorage) FScriptArray(); A->Add(2, sizeof(FCounted), alignof(FCounted));
		new(A->GetData()) FCounted(2); new(static_cast<uint8*>(A->GetData()) + sizeof(FCounted)) FCounted(5);
		FString D; ASSERT_THAT(IsTrue(Ops->CopyConstructValue(BStorage, A, D))); ASSERT_THAT(AreEqual(2, FCounted::Copies));
		ASSERT_THAT(IsTrue(Ops->DestroyValue(A, D))); ASSERT_THAT(IsTrue(Ops->DestroyValue(BStorage, D)));
		ASSERT_THAT(AreEqual(4, FCounted::Destructions));
	}

	TEST_METHOD(ObjectHandleArrayEnumeratesEachLiveReference)
	{
		FInstanceFixture F; ASSERT_THAT(IsTrue(F.Record())); asCObjectType* Type = nullptr;
		ASSERT_THAT(IsTrue(F.Instance(TEXT("TArray<UObject>"), Type)));
		const auto* Ops = F.Owner->GetBindingInstallation()->FindTemplateOperations(Type); ASSERT_THAT(IsNotNull(Ops));
		FScriptArray Array; Array.Add(2, sizeof(UObject*), alignof(UObject*));
		UObject* First = GetTransientPackage(); UObject* Second = UObject::StaticClass();
		auto** Values = static_cast<UObject**>(Array.GetData()); Values[0] = First; Values[1] = Second;
		TArray<UObject*> Seen; Ops->EnumerateReferences(&Array, [&](UObject* Object) { Seen.Add(Object); });
		ASSERT_THAT(AreEqual(2, Seen.Num())); ASSERT_THAT(IsTrue(Seen.Contains(First) && Seen.Contains(Second)));
		Array.Empty(0, sizeof(UObject*), alignof(UObject*));
	}
};
#endif
