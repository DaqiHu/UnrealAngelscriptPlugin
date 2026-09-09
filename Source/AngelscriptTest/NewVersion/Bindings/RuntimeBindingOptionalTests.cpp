#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingOptionalFixture
{
struct FOptionalCounted
{
	static int32 Copies;
	static int32 Destructions;
	int32 Value = 0;
	FOptionalCounted() = default;
	explicit FOptionalCounted(int32 InValue) : Value(InValue) {}
	FOptionalCounted(const FOptionalCounted& Other) : Value(Other.Value) { ++Copies; }
	~FOptionalCounted() { ++Destructions; }
};
int32 FOptionalCounted::Copies = 0;
int32 FOptionalCounted::Destructions = 0;

struct FOptionalFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Record(bool bCounted = false, bool bMethodSurface = false)
	{
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return false;
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return false;
		if (bMethodSurface)
		{
			const FName Provider[] = {TEXT("TOptional.MethodSurface")};
			if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, Provider, *Store, Diagnostic)) return false;
		}
		Store->SetSource({TEXT("FixtureModule"), TEXT("OptionalFixture"), TEXT("Bindings/RuntimeBindingOptionalTests.cpp"), 41});
		if (bCounted) FAngelscriptBinds(*Store).ValueClassForTarget<FOptionalCounted>("FOptionalCounted", FBindFlags());
		if (!Store->Seal(Diagnostic)) return false;
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) Diagnostic = BindingDiagnostic.Message;
		return Owner.IsValid();
	}

	const FAngelscriptTemplateInstanceOperations* Operations(const TCHAR* Declaration)
	{
		asCObjectType* Type = nullptr;
		if (!Owner || !Owner->GetBindingInstallation()->MaterializeTemplateInstance(
			Declaration, *static_cast<asCScriptEngine*>(Owner->Engine), Type, Diagnostic)) return nullptr;
		return Owner->GetBindingInstallation()->FindTemplateOperations(Type);
	}
};
}

using namespace RuntimeBindingOptionalFixture;
TEST_CLASS_WITH_FLAGS(Optional, "Angelscript.UnitTest.RuntimeBindings.Containers.Optional",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompleteTOptionalProviderSurfaceIsRecorded)
	{
		FOptionalFixture F; const bool bRecorded = F.Record(false, true); const FString RecordDiagnostic = F.Diagnostic;
		ASSERT_THAT(IsTrue(bRecorded, *RecordDiagnostic)); const auto* Type = F.Store->GetType(F.Store->FindType(TEXT("TOptional"))); ASSERT_THAT(IsNotNull(Type));
		int32 Accounted = 0;
		for (const auto& Member : Type->Members) if (Member.Source.Provider == TEXT("TOptional.MethodSurface"))
		{
			++Accounted; ASSERT_THAT(IsTrue(Member.Native.Function.flag != 0));
		}
		ASSERT_THAT(AreEqual(14, Accounted));
	}

	TEST_METHOD(DefaultAssignAndReadTrackEngagement)
	{
		FOptionalFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TOptional<int>")); ASSERT_THAT(IsNotNull(Ops));
		TArray<uint8> Storage; Storage.SetNumUninitialized(Ops->ValueSize); ASSERT_THAT(IsTrue(Ops->ConstructValue(Storage.GetData(), F.Diagnostic)));
		ASSERT_THAT(IsFalse(Ops->OptionalIsSet(Storage.GetData(), F.Diagnostic))); int32 Seven = 7; ASSERT_THAT(IsTrue(Ops->OptionalSetCopy(Storage.GetData(), &Seven, F.Diagnostic)));
		ASSERT_THAT(IsTrue(Ops->OptionalIsSet(Storage.GetData(), F.Diagnostic))); ASSERT_THAT(AreEqual(7, *static_cast<const int32*>(Ops->OptionalValue(Storage.GetData(), F.Diagnostic))));
		ASSERT_THAT(IsTrue(Ops->DestroyValue(Storage.GetData(), F.Diagnostic)));
	}

	TEST_METHOD(CopyRetainsValueAfterOriginalReset)
	{
		FOptionalFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TOptional<int>")); ASSERT_THAT(IsNotNull(Ops));
		TArray<uint8> A, B; A.SetNumUninitialized(Ops->ValueSize); B.SetNumUninitialized(Ops->ValueSize); ASSERT_THAT(IsTrue(Ops->ConstructValue(A.GetData(), F.Diagnostic)));
		int32 Seven = 7; ASSERT_THAT(IsTrue(Ops->OptionalSetCopy(A.GetData(), &Seven, F.Diagnostic))); ASSERT_THAT(IsTrue(Ops->CopyConstructValue(B.GetData(), A.GetData(), F.Diagnostic)));
		ASSERT_THAT(IsTrue(Ops->OptionalReset(A.GetData(), F.Diagnostic))); ASSERT_THAT(AreEqual(7, *static_cast<const int32*>(Ops->OptionalValue(B.GetData(), F.Diagnostic))));
		ASSERT_THAT(IsTrue(Ops->DestroyValue(A.GetData(), F.Diagnostic))); ASSERT_THAT(IsTrue(Ops->DestroyValue(B.GetData(), F.Diagnostic)));
	}

	TEST_METHOD(ResetDestroysOneCountedElement)
	{
		FOptionalCounted::Copies = 0; FOptionalCounted::Destructions = 0; FOptionalFixture F; ASSERT_THAT(IsTrue(F.Record(true)));
		const auto* Ops = F.Operations(TEXT("TOptional<FOptionalCounted>")); ASSERT_THAT(IsNotNull(Ops)); TArray<uint8> Storage; Storage.SetNumUninitialized(Ops->ValueSize);
		ASSERT_THAT(IsTrue(Ops->ConstructValue(Storage.GetData(), F.Diagnostic))); FOptionalCounted Seven(7); ASSERT_THAT(IsTrue(Ops->OptionalSetCopy(Storage.GetData(), &Seven, F.Diagnostic)));
		ASSERT_THAT(AreEqual(1, FOptionalCounted::Copies)); ASSERT_THAT(IsTrue(Ops->OptionalReset(Storage.GetData(), F.Diagnostic))); ASSERT_THAT(AreEqual(1, FOptionalCounted::Destructions));
		ASSERT_THAT(IsTrue(Ops->DestroyValue(Storage.GetData(), F.Diagnostic)));
	}

	TEST_METHOD(UnsetValueReportsDiagnostic)
	{
		FOptionalFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TOptional<int>")); ASSERT_THAT(IsNotNull(Ops));
		TArray<uint8> Storage; Storage.SetNumUninitialized(Ops->ValueSize); ASSERT_THAT(IsTrue(Ops->ConstructValue(Storage.GetData(), F.Diagnostic)));
		ASSERT_THAT(IsNull(Ops->OptionalValue(Storage.GetData(), F.Diagnostic))); ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("unset"), ESearchCase::IgnoreCase)));
		ASSERT_THAT(IsTrue(Ops->DestroyValue(Storage.GetData(), F.Diagnostic)));
	}

	TEST_METHOD(NestedOptionalCopiesIndependentValue)
	{
		FOptionalFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* InnerOps = F.Operations(TEXT("TOptional<int>")); const auto* OuterOps = F.Operations(TEXT("TOptional<TOptional<int>>"));
		ASSERT_THAT(IsNotNull(InnerOps)); ASSERT_THAT(IsNotNull(OuterOps)); TArray<uint8> Inner, Outer; Inner.SetNumUninitialized(InnerOps->ValueSize); Outer.SetNumUninitialized(OuterOps->ValueSize);
		ASSERT_THAT(IsTrue(InnerOps->ConstructValue(Inner.GetData(), F.Diagnostic))); ASSERT_THAT(IsTrue(OuterOps->ConstructValue(Outer.GetData(), F.Diagnostic))); int32 Seven = 7;
		ASSERT_THAT(IsTrue(InnerOps->OptionalSetCopy(Inner.GetData(), &Seven, F.Diagnostic))); ASSERT_THAT(IsTrue(OuterOps->OptionalSetCopy(Outer.GetData(), Inner.GetData(), F.Diagnostic)));
		ASSERT_THAT(IsTrue(InnerOps->OptionalReset(Inner.GetData(), F.Diagnostic))); const void* StoredInner = OuterOps->OptionalValue(Outer.GetData(), F.Diagnostic); ASSERT_THAT(IsNotNull(StoredInner));
		ASSERT_THAT(AreEqual(7, *static_cast<const int32*>(InnerOps->OptionalValue(StoredInner, F.Diagnostic)))); ASSERT_THAT(IsTrue(InnerOps->DestroyValue(Inner.GetData(), F.Diagnostic))); ASSERT_THAT(IsTrue(OuterOps->DestroyValue(Outer.GetData(), F.Diagnostic)));
	}

	TEST_METHOD(HandleReferenceEnumerationFollowsEngagement)
	{
		FOptionalFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TOptional<UObject>")); ASSERT_THAT(IsNotNull(Ops));
		TArray<uint8> Storage; Storage.SetNumUninitialized(Ops->ValueSize); ASSERT_THAT(IsTrue(Ops->ConstructValue(Storage.GetData(), F.Diagnostic))); UObject* Object = GetTransientPackage();
		ASSERT_THAT(IsTrue(Ops->OptionalSetCopy(Storage.GetData(), &Object, F.Diagnostic))); TArray<UObject*> Seen; Ops->EnumerateReferences(Storage.GetData(), [&](UObject* Item) { Seen.Add(Item); });
		ASSERT_THAT(AreEqual(1, Seen.Num())); ASSERT_THAT(IsTrue(Seen[0] == Object)); ASSERT_THAT(IsTrue(Ops->OptionalReset(Storage.GetData(), F.Diagnostic))); Seen.Reset();
		Ops->EnumerateReferences(Storage.GetData(), [&](UObject* Item) { Seen.Add(Item); }); ASSERT_THAT(AreEqual(0, Seen.Num())); ASSERT_THAT(IsTrue(Ops->DestroyValue(Storage.GetData(), F.Diagnostic)));
	}
};
#endif
