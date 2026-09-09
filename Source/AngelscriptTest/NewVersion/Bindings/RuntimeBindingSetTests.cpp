#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Containers/Set.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingSetFixture
{
struct FUnhashableSetElement
{
	int32 Value = 0;
	bool operator==(const FUnhashableSetElement& Other) const { return Value == Other.Value; }
};

struct FSetFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Record(bool bUnhashable = false, bool bMethodSurface = false)
	{
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return false;
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return false;
		if (bMethodSurface)
		{
			const FName Provider[] = {TEXT("TSet.MethodSurface")};
			if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, Provider, *Store, Diagnostic)) return false;
		}
		Store->SetSource({TEXT("FixtureModule"), TEXT("SetFixture"), TEXT("Bindings/RuntimeBindingSetTests.cpp"), 36});
		if (bUnhashable) FAngelscriptBinds(*Store).ValueClassForTarget<FUnhashableSetElement>("FUnhashableSetElement", FBindFlags());
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

using namespace RuntimeBindingSetFixture;
TEST_CLASS_WITH_FLAGS(Set, "Angelscript.UnitTest.RuntimeBindings.Containers.Set",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompleteTSetProviderSurfaceIsRecorded)
	{
		FSetFixture F; const bool bRecorded = F.Record(false, true); const FString RecordDiagnostic = F.Diagnostic;
		ASSERT_THAT(IsTrue(bRecorded, *RecordDiagnostic));
		int32 Accounted = 0;
		for (const TCHAR* TypeName : {TEXT("TSet"), TEXT("TSetIterator"), TEXT("TSetConstIterator")})
		{
			const auto* Type = F.Store->GetType(F.Store->FindType(TypeName)); ASSERT_THAT(IsNotNull(Type));
			for (const auto& Member : Type->Members) if (Member.Source.Provider == TEXT("TSet.MethodSurface"))
			{
				++Accounted;
				if (Member.Kind != EAngelscriptTypeBindMemberKind::Property)
					ASSERT_THAT(IsTrue(Member.Native.Function.flag != 0, *FString::Printf(TEXT("missing native target: %s"), *Member.Declaration)));
			}
		}
		ASSERT_THAT(AreEqual(33, Accounted));
	}

	TEST_METHOD(DuplicateInsertRemoveAndContainsUseHashEquality)
	{
		FSetFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TSet<int>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptSet Values; int32 Two = 2, Five = 5;
		ASSERT_THAT(IsTrue(Ops->SetAddCopy(Values, &Two, F.Diagnostic))); ASSERT_THAT(IsTrue(Ops->SetAddCopy(Values, &Two, F.Diagnostic)));
		ASSERT_THAT(AreEqual(1, Values.Num())); ASSERT_THAT(IsTrue(Ops->SetAddCopy(Values, &Five, F.Diagnostic))); ASSERT_THAT(AreEqual(2, Values.Num()));
		ASSERT_THAT(IsTrue(Ops->SetContains(Values, &Two, F.Diagnostic))); ASSERT_THAT(IsTrue(Ops->SetRemove(Values, &Two, F.Diagnostic)));
		ASSERT_THAT(IsFalse(Ops->SetContains(Values, &Two, F.Diagnostic))); ASSERT_THAT(IsTrue(Ops->SetContains(Values, &Five, F.Diagnostic)));
		ASSERT_THAT(IsTrue(Ops->SetEmpty(Values, F.Diagnostic)));
	}

	TEST_METHOD(IterationVisitsTwoAndFiveExactlyOnce)
	{
		FSetFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TSet<int>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptSet Values; int32 Two = 2, Five = 5; ASSERT_THAT(IsTrue(Ops->SetAddCopy(Values, &Two, F.Diagnostic))); ASSERT_THAT(IsTrue(Ops->SetAddCopy(Values, &Five, F.Diagnostic)));
		TSet<int32> Seen;
		for (int32 Index = Ops->SetFirstIndex(Values, F.Diagnostic); Index != INDEX_NONE; Index = Ops->SetNextIndex(Values, Index, F.Diagnostic))
			Seen.Add(*static_cast<const int32*>(Ops->SetElement(Values, Index, F.Diagnostic)));
		ASSERT_THAT(AreEqual(2, Seen.Num())); ASSERT_THAT(IsTrue(Seen.Contains(2))); ASSERT_THAT(IsTrue(Seen.Contains(5)));
		ASSERT_THAT(IsTrue(Ops->SetEmpty(Values, F.Diagnostic)));
	}

	TEST_METHOD(StringCopySurvivesSourceClear)
	{
		FSetFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TSet<FString>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptSet Source; FString Alpha(TEXT("alpha")); ASSERT_THAT(IsTrue(Ops->SetAddCopy(Source, &Alpha, F.Diagnostic)));
		alignas(FScriptSet) uint8 Storage[sizeof(FScriptSet)]; ASSERT_THAT(IsTrue(Ops->CopyConstructValue(Storage, &Source, F.Diagnostic)));
		auto& Copy = *reinterpret_cast<FScriptSet*>(Storage); ASSERT_THAT(IsTrue(Ops->SetEmpty(Source, F.Diagnostic)));
		ASSERT_THAT(IsTrue(Ops->SetContains(Copy, &Alpha, F.Diagnostic))); ASSERT_THAT(IsTrue(Ops->DestroyValue(&Copy, F.Diagnostic)));
	}

	TEST_METHOD(UnhashableElementIsRejectedWithSource)
	{
		FSetFixture F; ASSERT_THAT(IsTrue(F.Record(true))); ASSERT_THAT(IsNull(F.Operations(TEXT("TSet<FUnhashableSetElement>"))));
		ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("FUnhashableSetElement")))); ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("hash"), ESearchCase::IgnoreCase)));
		ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("SetFixture"))));
	}

	TEST_METHOD(HandleReferencesAreEnumerated)
	{
		FSetFixture F; ASSERT_THAT(IsTrue(F.Record())); const auto* Ops = F.Operations(TEXT("TSet<UObject>")); ASSERT_THAT(IsNotNull(Ops));
		FScriptSet Values; UObject* Object = GetTransientPackage(); ASSERT_THAT(IsTrue(Ops->SetAddCopy(Values, &Object, F.Diagnostic)));
		TArray<UObject*> Seen; Ops->EnumerateReferences(&Values, [&](UObject* Item) { Seen.Add(Item); });
		ASSERT_THAT(AreEqual(1, Seen.Num())); ASSERT_THAT(IsTrue(Seen[0] == Object)); ASSERT_THAT(IsTrue(Ops->SetEmpty(Values, F.Diagnostic)));
	}
};
#endif
