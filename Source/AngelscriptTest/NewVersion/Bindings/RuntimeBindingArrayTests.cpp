#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Containers/ScriptArray.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingArrayFixture
{
struct FArrayCounted
{
	static int32 Copies;
	static int32 Destructions;
	int32 Value = 0;
	FArrayCounted() = default;
	explicit FArrayCounted(int32 InValue) : Value(InValue) {}
	FArrayCounted(const FArrayCounted& Other) : Value(Other.Value) { ++Copies; }
	~FArrayCounted() { ++Destructions; }
	bool operator==(const FArrayCounted& Other) const { return Value == Other.Value; }
};
int32 FArrayCounted::Copies = 0;
int32 FArrayCounted::Destructions = 0;
inline uint32 GetTypeHash(const FArrayCounted& Value) { return ::GetTypeHash(Value.Value); }

struct FArrayFixture
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
			const FName Provider[] = {TEXT("TArray.MethodSurface")};
			if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, Provider, *Store, Diagnostic)) return false;
		}
		Store->SetSource({TEXT("FixtureModule"), TEXT("ArrayFixture"), TEXT("Bindings/RuntimeBindingArrayTests.cpp"), 35});
		if (bCounted) FAngelscriptBinds(*Store).ValueClassForTarget<FArrayCounted>("FArrayCounted", FBindFlags());
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

template<typename T> void AddRaw(FScriptArray& Array, const T& Value)
{
	Array.Add(1, sizeof(T), alignof(T));
	new(static_cast<uint8*>(Array.GetData()) + static_cast<SIZE_T>(Array.Num() - 1) * sizeof(T)) T(Value);
}
}

using namespace RuntimeBindingArrayFixture;
TEST_CLASS_WITH_FLAGS(Array, "Angelscript.UnitTest.RuntimeBindings.Containers.Array",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompleteTArrayProviderSurfaceIsRecorded)
	{
		FArrayFixture F;
		const bool bRecorded = F.Record(false, true);
		const FString RecordDiagnostic = F.Diagnostic;
		ASSERT_THAT(IsTrue(bRecorded, *RecordDiagnostic));
		auto CountProviderMembers = [&](const TCHAR* TypeName)
		{
			const auto* Type = F.Store->GetType(F.Store->FindType(TypeName));
			if (!Type) return int32(INDEX_NONE);
			int32 Count = 0;
			for (const auto& Member : Type->Members)
				if (Member.Source.Provider == TEXT("TArray.MethodSurface")) ++Count;
			return Count;
		};
		ASSERT_THAT(AreEqual(48, CountProviderMembers(TEXT("TArray"))));
		ASSERT_THAT(AreEqual(5, CountProviderMembers(TEXT("TArrayIterator"))));
		ASSERT_THAT(AreEqual(5, CountProviderMembers(TEXT("TArrayConstIterator"))));
		int32 CallableCount = 0;
		int32 PropertyCount = 0;
		int32 NativeRecipeCount = 0;
		for (const TCHAR* TypeName : {TEXT("TArray"), TEXT("TArrayIterator"), TEXT("TArrayConstIterator")})
		{
			const auto* Type = F.Store->GetType(F.Store->FindType(TypeName));
			ASSERT_THAT(IsNotNull(Type));
			for (const auto& Member : Type->Members)
			{
				if (Member.Source.Provider != TEXT("TArray.MethodSurface")) continue;
				NativeRecipeCount += Member.NativeRecipes.Num();
				if (Member.Kind == EAngelscriptTypeBindMemberKind::Property) ++PropertyCount;
				else { ++CallableCount; ASSERT_THAT(IsTrue(Member.Native.Function.flag != 0)); }
			}
		}
		ASSERT_THAT(AreEqual(56, CallableCount));
		ASSERT_THAT(AreEqual(2, PropertyCount));
		ASSERT_THAT(AreEqual(41, NativeRecipeCount));
	}

	TEST_METHOD(CopyThenClearOriginalRetainsTwoAndFive)
	{
		FArrayFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TArray<int>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptArray Source; AddRaw(Source, int32(2)); AddRaw(Source, int32(5)); alignas(FScriptArray) uint8 CopyStorage[sizeof(FScriptArray)];
		ASSERT_THAT(IsTrue(Ops->CopyConstructValue(CopyStorage, &Source, F.Diagnostic))); auto& Copy = *reinterpret_cast<FScriptArray*>(CopyStorage);
		const int32 Five = 5;
		ASSERT_THAT(IsTrue(Ops->ArrayEquals(Source, Copy, F.Diagnostic)));
		ASSERT_THAT(AreEqual(1, Ops->ArrayFindIndex(Copy, &Five, F.Diagnostic)));
		ASSERT_THAT(IsTrue(Ops->ArrayEmpty(Source, F.Diagnostic))); ASSERT_THAT(AreEqual(0, Source.Num())); ASSERT_THAT(AreEqual(2, Copy.Num()));
		ASSERT_THAT(AreEqual(5, *static_cast<const int32*>(Ops->ArrayElement(Copy, 1, F.Diagnostic))));
		ASSERT_THAT(IsTrue(Ops->DestroyValue(&Copy, F.Diagnostic)));
	}

	TEST_METHOD(AppendRemoveAndIterationYieldTwoThenFive)
	{
		FArrayFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TArray<int>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptArray A, B; int32 Two = 2, Five = 5; ASSERT_THAT(IsTrue(Ops->ArrayAddCopy(A, &Two, F.Diagnostic)));
		ASSERT_THAT(IsTrue(Ops->ArrayAddCopy(B, &Five, F.Diagnostic))); ASSERT_THAT(IsTrue(Ops->ArrayAppendCopy(A, B, F.Diagnostic)));
		ASSERT_THAT(AreEqual(2, *static_cast<const int32*>(Ops->ArrayElement(A, 0, F.Diagnostic))));
		ASSERT_THAT(AreEqual(5, *static_cast<const int32*>(Ops->ArrayElement(A, 1, F.Diagnostic))));
		ASSERT_THAT(IsTrue(Ops->ArrayRemoveAt(A, 0, F.Diagnostic))); ASSERT_THAT(AreEqual(5, *static_cast<const int32*>(Ops->ArrayElement(A, 0, F.Diagnostic))));
		ASSERT_THAT(IsTrue(Ops->ArrayEmpty(A, F.Diagnostic))); ASSERT_THAT(IsTrue(Ops->ArrayEmpty(B, F.Diagnostic)));
	}

	TEST_METHOD(InvalidIndexReturnsDiagnosticWithoutMutation)
	{
		FArrayFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TArray<int>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptArray A; int32 Two = 2; ASSERT_THAT(IsTrue(Ops->ArrayAddCopy(A, &Two, F.Diagnostic)));
		ASSERT_THAT(IsNull(Ops->ArrayElement(A, 3, F.Diagnostic))); ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("index"), ESearchCase::IgnoreCase)));
		ASSERT_THAT(AreEqual(1, A.Num())); ASSERT_THAT(IsTrue(Ops->ArrayEmpty(A, F.Diagnostic)));
	}

	TEST_METHOD(StringCopyIsIndependent)
	{
		FArrayFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TArray<FString>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptArray Source; FString Alpha(TEXT("alpha")); ASSERT_THAT(IsTrue(Ops->ArrayAddCopy(Source, &Alpha, F.Diagnostic)));
		alignas(FScriptArray) uint8 CopyStorage[sizeof(FScriptArray)]; ASSERT_THAT(IsTrue(Ops->CopyConstructValue(CopyStorage, &Source, F.Diagnostic)));
		auto& Copy = *reinterpret_cast<FScriptArray*>(CopyStorage); ASSERT_THAT(IsTrue(Ops->ArrayEmpty(Source, F.Diagnostic)));
		ASSERT_THAT(IsTrue(*static_cast<const FString*>(Ops->ArrayElement(Copy, 0, F.Diagnostic)) == TEXT("alpha")));
		ASSERT_THAT(IsTrue(Ops->DestroyValue(&Copy, F.Diagnostic)));
	}

	TEST_METHOD(CountedElementsCopyAndDestroyExactlyOncePerInstance)
	{
		FArrayCounted::Copies = 0; FArrayCounted::Destructions = 0; FArrayFixture F; ASSERT_THAT(IsTrue(F.Record(true)));
		const auto* Ops = F.Operations(TEXT("TArray<FArrayCounted>")); ASSERT_THAT(IsNotNull(Ops)); FScriptArray A; FArrayCounted Value(7);
		ASSERT_THAT(IsTrue(Ops->ArrayAddCopy(A, &Value, F.Diagnostic))); ASSERT_THAT(AreEqual(1, FArrayCounted::Copies));
		ASSERT_THAT(IsTrue(Ops->ArrayRemoveAt(A, 0, F.Diagnostic))); ASSERT_THAT(AreEqual(1, FArrayCounted::Destructions));
		ASSERT_THAT(IsTrue(Ops->ArrayEmpty(A, F.Diagnostic)));
	}

	TEST_METHOD(NestedAndHandleElementsRetainOwnedValues)
	{
		FArrayFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* InnerOps = F.Operations(TEXT("TArray<int>"));
		const auto* NestedOps = F.Operations(TEXT("TArray<TArray<int>>")); const auto* HandleOps = F.Operations(TEXT("TArray<UObject>"));
		ASSERT_THAT(IsNotNull(InnerOps)); ASSERT_THAT(IsNotNull(NestedOps)); ASSERT_THAT(IsNotNull(HandleOps));
		FScriptArray Inner, Nested, Handles; int32 Five = 5; ASSERT_THAT(IsTrue(InnerOps->ArrayAddCopy(Inner, &Five, F.Diagnostic)));
		ASSERT_THAT(IsTrue(NestedOps->ArrayAddCopy(Nested, &Inner, F.Diagnostic))); ASSERT_THAT(IsTrue(InnerOps->ArrayEmpty(Inner, F.Diagnostic)));
		const auto* NestedInner = static_cast<const FScriptArray*>(NestedOps->ArrayElement(Nested, 0, F.Diagnostic));
		ASSERT_THAT(IsNotNull(NestedInner)); ASSERT_THAT(AreEqual(5, *static_cast<const int32*>(InnerOps->ArrayElement(*NestedInner, 0, F.Diagnostic))));
		UObject* Object = GetTransientPackage(); ASSERT_THAT(IsTrue(HandleOps->ArrayAddCopy(Handles, &Object, F.Diagnostic)));
		TArray<UObject*> Seen; HandleOps->EnumerateReferences(&Handles, [&](UObject* Item) { Seen.Add(Item); });
		ASSERT_THAT(AreEqual(1, Seen.Num())); ASSERT_THAT(IsTrue(Seen[0] == Object));
		ASSERT_THAT(IsTrue(NestedOps->ArrayEmpty(Nested, F.Diagnostic))); ASSERT_THAT(IsTrue(HandleOps->ArrayEmpty(Handles, F.Diagnostic)));
	}
};
#endif
