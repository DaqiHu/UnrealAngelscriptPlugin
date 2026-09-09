#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Containers/ScriptArray.h"
#include "Containers/Map.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingMapFixture
{
struct FMapCounted
{
	static int32 Copies;
	static int32 Destructions;
	int32 Value = 0;
	FMapCounted() = default;
	explicit FMapCounted(int32 InValue) : Value(InValue) {}
	FMapCounted(const FMapCounted& Other) : Value(Other.Value) { ++Copies; }
	~FMapCounted() { ++Destructions; }
};
int32 FMapCounted::Copies = 0;
int32 FMapCounted::Destructions = 0;

struct FMapFixture
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
			const FName Provider[] = {TEXT("TMap.MethodSurface")};
			if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, Provider, *Store, Diagnostic)) return false;
		}
		Store->SetSource({TEXT("FixtureModule"), TEXT("MapFixture"), TEXT("Bindings/RuntimeBindingMapTests.cpp"), 42});
		if (bCounted) FAngelscriptBinds(*Store).ValueClassForTarget<FMapCounted>("FMapCounted", FBindFlags());
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

using namespace RuntimeBindingMapFixture;
TEST_CLASS_WITH_FLAGS(Map, "Angelscript.UnitTest.RuntimeBindings.Containers.Map",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompleteTMapProviderSurfaceIsRecorded)
	{
		FMapFixture F; const bool bRecorded = F.Record(false, true);
		ASSERT_THAT(IsTrue(bRecorded, *F.Diagnostic));
		auto CountProviderMembers = [&](const TCHAR* TypeName)
		{
			const auto* Type = F.Store->GetType(F.Store->FindType(TypeName)); if (!Type) return int32(INDEX_NONE);
			int32 Count = 0;
			for (const auto& Member : Type->Members) if (Member.Source.Provider == TEXT("TMap.MethodSurface")) ++Count;
			return Count;
		};
		ASSERT_THAT(AreEqual(30, CountProviderMembers(TEXT("TMap"))));
		ASSERT_THAT(AreEqual(9, CountProviderMembers(TEXT("TMapIterator"))));
		ASSERT_THAT(AreEqual(7, CountProviderMembers(TEXT("TMapConstIterator"))));
		int32 Accounted = 0;
		for (const TCHAR* TypeName : {TEXT("TMap"), TEXT("TMapIterator"), TEXT("TMapConstIterator")})
		{
			const auto* Type = F.Store->GetType(F.Store->FindType(TypeName)); ASSERT_THAT(IsNotNull(Type));
			for (const auto& Member : Type->Members) if (Member.Source.Provider == TEXT("TMap.MethodSurface"))
			{
				++Accounted;
				if (Member.Kind != EAngelscriptTypeBindMemberKind::Property)
					ASSERT_THAT(IsTrue(Member.Native.Function.flag != 0));
			}
		}
		ASSERT_THAT(AreEqual(46, Accounted));
	}

	TEST_METHOD(SetExistingKeyReplacesItsValue)
	{
		FMapFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TMap<FString,int>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptMap Values; FString Key(TEXT("a")); int32 Two = 2, Five = 5, Found = 0;
		ASSERT_THAT(IsTrue(Ops->MapSetCopy(Values, &Key, &Two, F.Diagnostic)));
		ASSERT_THAT(IsTrue(Ops->MapSetCopy(Values, &Key, &Five, F.Diagnostic))); ASSERT_THAT(AreEqual(1, Values.Num()));
		ASSERT_THAT(IsTrue(Ops->MapFindCopy(Values, &Key, &Found, F.Diagnostic))); ASSERT_THAT(AreEqual(5, Found));
		ASSERT_THAT(IsTrue(Ops->MapEmpty(Values, F.Diagnostic)));
	}

	TEST_METHOD(CopySurvivesOriginalRemoval)
	{
		FMapFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TMap<FString,int>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptMap Source; FString Key(TEXT("a")); int32 Five = 5; ASSERT_THAT(IsTrue(Ops->MapSetCopy(Source, &Key, &Five, F.Diagnostic)));
		alignas(FScriptMap) uint8 Storage[sizeof(FScriptMap)]; ASSERT_THAT(IsTrue(Ops->CopyConstructValue(Storage, &Source, F.Diagnostic)));
		auto& Copy = *reinterpret_cast<FScriptMap*>(Storage); ASSERT_THAT(IsTrue(Ops->MapRemove(Source, &Key, F.Diagnostic)));
		int32 Found = 0; ASSERT_THAT(IsTrue(Ops->MapFindCopy(Copy, &Key, &Found, F.Diagnostic))); ASSERT_THAT(AreEqual(5, Found));
		ASSERT_THAT(IsTrue(Ops->MapEmpty(Source, F.Diagnostic))); ASSERT_THAT(IsTrue(Ops->DestroyValue(&Copy, F.Diagnostic)));
	}

	TEST_METHOD(MissingKeyLeavesOutputUnchanged)
	{
		FMapFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TMap<FString,int>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptMap Values; FString Missing(TEXT("missing")); int32 Output = 17;
		ASSERT_THAT(IsFalse(Ops->MapFindCopy(Values, &Missing, &Output, F.Diagnostic))); ASSERT_THAT(AreEqual(17, Output));
		ASSERT_THAT(IsTrue(F.Diagnostic.IsEmpty())); ASSERT_THAT(IsTrue(Ops->MapEmpty(Values, F.Diagnostic)));
	}

	TEST_METHOD(IteratorYieldsExpectedKeyAndValueAndRejectsPastEnd)
	{
		FMapFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TMap<FString,int>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptMap Values; FString Key(TEXT("a")); int32 Five = 5; ASSERT_THAT(IsTrue(Ops->MapSetCopy(Values, &Key, &Five, F.Diagnostic)));
		const int32 Index = Ops->MapFirstIndex(Values, F.Diagnostic); ASSERT_THAT(IsTrue(Index != INDEX_NONE));
		ASSERT_THAT(IsTrue(*static_cast<const FString*>(Ops->MapKey(Values, Index, F.Diagnostic)) == TEXT("a")));
		ASSERT_THAT(AreEqual(5, *static_cast<const int32*>(Ops->MapValue(Values, Index, F.Diagnostic))));
		ASSERT_THAT(AreEqual(INDEX_NONE, Ops->MapNextIndex(Values, Index, F.Diagnostic)));
		ASSERT_THAT(IsNull(Ops->MapValue(Values, INDEX_NONE, F.Diagnostic)));
		ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("valid entry")))); ASSERT_THAT(IsTrue(Ops->MapEmpty(Values, F.Diagnostic)));
	}

	TEST_METHOD(CountedValuesReleaseOnReplaceAndRemove)
	{
		FMapCounted::Copies = 0; FMapCounted::Destructions = 0; FMapFixture F; ASSERT_THAT(IsTrue(F.Record(true)));
		const auto* Ops = F.Operations(TEXT("TMap<FString,FMapCounted>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptMap Values; FString Key(TEXT("a")); FMapCounted Two(2), Five(5);
		ASSERT_THAT(IsTrue(Ops->MapSetCopy(Values, &Key, &Two, F.Diagnostic))); ASSERT_THAT(AreEqual(1, FMapCounted::Copies));
		ASSERT_THAT(IsTrue(Ops->MapSetCopy(Values, &Key, &Five, F.Diagnostic))); ASSERT_THAT(AreEqual(2, FMapCounted::Copies));
		ASSERT_THAT(AreEqual(1, FMapCounted::Destructions)); ASSERT_THAT(IsTrue(Ops->MapRemove(Values, &Key, F.Diagnostic)));
		ASSERT_THAT(AreEqual(2, FMapCounted::Destructions)); ASSERT_THAT(IsTrue(Ops->MapEmpty(Values, F.Diagnostic)));
	}

	TEST_METHOD(NestedArrayValueCopyIsIndependent)
	{
		FMapFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* ArrayOps = F.Operations(TEXT("TArray<int>"));
		const auto* MapOps = F.Operations(TEXT("TMap<FString,TArray<int>>")); ASSERT_THAT(IsNotNull(ArrayOps)); ASSERT_THAT(IsNotNull(MapOps));
		FScriptArray Array; int32 Five = 5; ASSERT_THAT(IsTrue(ArrayOps->ArrayAddCopy(Array, &Five, F.Diagnostic)));
		FScriptMap Values; FString Key(TEXT("a")); ASSERT_THAT(IsTrue(MapOps->MapSetCopy(Values, &Key, &Array, F.Diagnostic)));
		ASSERT_THAT(IsTrue(ArrayOps->ArrayEmpty(Array, F.Diagnostic))); const int32 Index = MapOps->MapFirstIndex(Values, F.Diagnostic);
		const auto* Stored = static_cast<const FScriptArray*>(MapOps->MapValue(Values, Index, F.Diagnostic)); ASSERT_THAT(IsNotNull(Stored));
		ASSERT_THAT(AreEqual(5, *static_cast<const int32*>(ArrayOps->ArrayElement(*Stored, 0, F.Diagnostic))));
		ASSERT_THAT(IsTrue(MapOps->MapEmpty(Values, F.Diagnostic)));
	}
};
#endif
