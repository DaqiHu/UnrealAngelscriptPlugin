#include "CQTest.h"
#include "RuntimeBindingMixinTestTypes.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "UObject/GarbageCollection.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingPointersFixture
{
struct FPointersFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Create(bool bSurfaces = true)
	{
		auto Fail = [this](const TCHAR* Stage) { Diagnostic = FString(Stage) + TEXT(": ") + Diagnostic; UE_LOG(LogTemp, Error, TEXT("Pointer fixture %s"), *Diagnostic); return false; };
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail(TEXT("Providers"));
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return Fail(TEXT("Declarations"));
		if (bSurfaces)
		{
			const FName Providers[] = {TEXT("TObjectPtr.MethodSurface"), TEXT("TSubclassOf.MethodSurface"),
				TEXT("TWeakObjectPtr.MethodSurface"), TEXT("SoftReferences.TypeInfrastructure"),
				TEXT("SoftReferences.Functions"), TEXT("SoftObjectPath.Functions")};
			if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, Providers, *Store, Diagnostic)) return Fail(TEXT("Surfaces"));
		}
		if (!Store->Seal(Diagnostic)) return Fail(TEXT("Seal"));
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; return Fail(TEXT("Create")); }
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

using namespace RuntimeBindingPointersFixture;
TEST_CLASS_WITH_FLAGS(Pointers, "Angelscript.UnitTest.RuntimeBindings.Reflection.Pointers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CorrectSubclassAssignmentSucceedsAndUnrelatedClassIsRejected)
	{
		FPointersFixture F; ASSERT_THAT(IsTrue(F.Create())); const auto* Ops = F.Operations(TEXT("TSubclassOf<URuntimeBindingMixinTarget>")); ASSERT_THAT(IsNotNull(Ops));
		TSubclassOf<UObject> Value; ASSERT_THAT(IsTrue(Ops->AssignSubclass(Value, URuntimeBindingMixinTarget::StaticClass(), F.Diagnostic), *F.Diagnostic));
		ASSERT_THAT(IsTrue(Value.Get() == URuntimeBindingMixinTarget::StaticClass()));
		ASSERT_THAT(IsFalse(Ops->AssignSubclass(Value, URuntimeBindingStaticLibrary::StaticClass(), F.Diagnostic)));
		ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("subclass")))); ASSERT_THAT(IsTrue(Value.Get() == URuntimeBindingMixinTarget::StaticClass()));
	}

	TEST_METHOD(StrongReferenceRetainsFixtureObject)
	{
		FPointersFixture F; ASSERT_THAT(IsTrue(F.Create())); const auto* Ops = F.Operations(TEXT("TObjectPtr<URuntimeBindingMixinTarget>")); ASSERT_THAT(IsNotNull(Ops));
		UObject* Object = NewObject<URuntimeBindingMixinTarget>(); TWeakObjectPtr<UObject> Witness = Object; TObjectPtr<UObject> Strong;
		ASSERT_THAT(IsTrue(F.Owner->GetBindingInstallation()->AssignStrongObject(*Ops, Strong, Object, F.Diagnostic), *F.Diagnostic));
		Object = nullptr; CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS); ASSERT_THAT(IsTrue(Witness.IsValid())); ASSERT_THAT(IsTrue(Strong.Get() == Witness.Get()));
		F.Owner->GetBindingInstallation()->ReleaseReflectedObjects(); Strong = nullptr; CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS); ASSERT_THAT(IsFalse(Witness.IsValid()));
	}

	TEST_METHOD(WeakPointerBecomesInvalidAfterCollection)
	{
		FPointersFixture F; ASSERT_THAT(IsTrue(F.Create())); const auto* Ops = F.Operations(TEXT("TWeakObjectPtr<URuntimeBindingMixinTarget>")); ASSERT_THAT(IsNotNull(Ops));
		UObject* Object = NewObject<URuntimeBindingMixinTarget>(); TWeakObjectPtr<UObject> Weak;
		ASSERT_THAT(IsTrue(Ops->AssignWeakObject(Weak, Object, F.Diagnostic), *F.Diagnostic)); Object = nullptr;
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS); ASSERT_THAT(IsFalse(Weak.IsValid())); ASSERT_THAT(IsTrue(Weak.IsStale()));
	}

	TEST_METHOD(SoftPathRoundTripsWithoutLoadingAsset)
	{
		FPointersFixture F; ASSERT_THAT(IsTrue(F.Create())); const auto* Ops = F.Operations(TEXT("TSoftObjectPtr<URuntimeBindingMixinTarget>")); ASSERT_THAT(IsNotNull(Ops));
		const FSoftObjectPath Path(TEXT("/Game/RuntimeBinding/MissingAsset.MissingAsset")); FSoftObjectPtr Soft;
		ASSERT_THAT(IsTrue(Ops->AssignSoftObjectPath(Soft, Path, F.Diagnostic), *F.Diagnostic));
		ASSERT_THAT(IsTrue(Soft.ToSoftObjectPath() == Path)); ASSERT_THAT(IsNull(Soft.Get())); ASSERT_THAT(IsTrue(Soft.IsPending()));
	}

	TEST_METHOD(NullPointerOperationsFollowExistingContract)
	{
		FPointersFixture F; ASSERT_THAT(IsTrue(F.Create())); const auto* StrongOps = F.Operations(TEXT("TObjectPtr<URuntimeBindingMixinTarget>"));
		const auto* WeakOps = F.Operations(TEXT("TWeakObjectPtr<URuntimeBindingMixinTarget>")); const auto* SubclassOps = F.Operations(TEXT("TSubclassOf<URuntimeBindingMixinTarget>"));
		const auto* SoftOps = F.Operations(TEXT("TSoftObjectPtr<URuntimeBindingMixinTarget>")); ASSERT_THAT(IsNotNull(StrongOps)); ASSERT_THAT(IsNotNull(WeakOps)); ASSERT_THAT(IsNotNull(SubclassOps)); ASSERT_THAT(IsNotNull(SoftOps));
		TObjectPtr<UObject> Strong = NewObject<URuntimeBindingMixinTarget>(); TWeakObjectPtr<UObject> Weak(Strong.Get()); TSubclassOf<UObject> Subclass(URuntimeBindingMixinTarget::StaticClass()); FSoftObjectPtr Soft(FSoftObjectPath(TEXT("/Game/A.A")));
		ASSERT_THAT(IsTrue(F.Owner->GetBindingInstallation()->AssignStrongObject(*StrongOps, Strong, nullptr, F.Diagnostic), *F.Diagnostic));
		ASSERT_THAT(IsTrue(WeakOps->AssignWeakObject(Weak, nullptr, F.Diagnostic), *F.Diagnostic)); ASSERT_THAT(IsTrue(SubclassOps->AssignSubclass(Subclass, nullptr, F.Diagnostic), *F.Diagnostic));
		ASSERT_THAT(IsTrue(SoftOps->AssignSoftObjectPath(Soft, FSoftObjectPath(), F.Diagnostic), *F.Diagnostic));
		ASSERT_THAT(IsNull(Strong.Get())); ASSERT_THAT(IsTrue(Weak.IsExplicitlyNull())); ASSERT_THAT(IsNull(Subclass.Get())); ASSERT_THAT(IsTrue(Soft.IsNull()));
	}

	TEST_METHOD(TwoOwnersShareNoMutableTemplateOperations)
	{
		FPointersFixture A, B; ASSERT_THAT(IsTrue(A.Create(false))); ASSERT_THAT(IsTrue(B.Create(false)));
		const auto* AO = A.Operations(TEXT("TObjectPtr<URuntimeBindingMixinTarget>")); const auto* BO = B.Operations(TEXT("TObjectPtr<URuntimeBindingMixinTarget>"));
		ASSERT_THAT(IsNotNull(AO)); ASSERT_THAT(IsNotNull(BO)); ASSERT_THAT(IsTrue(AO != BO)); ASSERT_THAT(AreEqual(AO->ValueSize, BO->ValueSize));
	}

	TEST_METHOD(FullPointerFamilyDeclarationsAreAccounted)
	{
		FPointersFixture F; ASSERT_THAT(IsTrue(F.Create()));
		const TCHAR* Templates[] = {TEXT("TObjectPtr"), TEXT("TSubclassOf"), TEXT("TWeakObjectPtr"), TEXT("TSoftObjectPtr"), TEXT("TSoftClassPtr")};
		for (const TCHAR* Name : Templates) { const auto* Type = F.Store->GetType(F.Store->FindType(Name)); ASSERT_THAT(IsNotNull(Type)); ASSERT_THAT(IsTrue(Type->Members.Num() > 0)); }
		ASSERT_THAT(IsNotNull(F.Store->GetType(F.Store->FindType(TEXT("FSoftObjectPath")))));
		ASSERT_THAT(IsNotNull(F.Store->GetType(F.Store->FindType(TEXT("FSoftClassPath")))));
	}
};
#endif
