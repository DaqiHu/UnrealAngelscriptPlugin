#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"

#include "AssetRegistry/AssetBundleData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingAssetsFixture
{
static const FName AssetProviderNames[] = {
	TEXT("AssetBundleData.Type"), TEXT("AssetBundleData"),
	TEXT("AssetManagerScriptMixins.GeneratedOverrides"),
	TEXT("AssetRegistry.Manual"), TEXT("UAssetManager.Functions"), TEXT("UDataTable")
};

struct FAssetsFixture
{
	UPackage* Package = nullptr;
	UScriptStruct* RowStruct = nullptr;
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	FAssetsFixture()
	{
		const FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		Package = CreatePackage(*FString::Printf(TEXT("/Temp/RuntimeBindingAssets_%s"), *Id));
		RowStruct = NewObject<UScriptStruct>(Package, *FString::Printf(TEXT("BindingAssetRow_%s"), *Id));
		RowStruct->StructFlags = STRUCT_NoExport; RowStruct->SetSuperStruct(FTableRowBase::StaticStruct());
		RowStruct->SetMetaData(TEXT("ForceAngelscriptBind"), TEXT("true"));
		auto* Number = new FIntProperty(RowStruct, TEXT("Number")); Number->SetPropertyFlags(CPF_BlueprintVisible); RowStruct->AddCppProperty(Number);
		auto* Label = new FStrProperty(RowStruct, TEXT("Label")); Label->SetPropertyFlags(CPF_BlueprintVisible); RowStruct->AddCppProperty(Label);
		RowStruct->StaticLink(true);
	}

	bool Create(bool bInstall = true)
	{
		auto Fail = [this](const TCHAR* Stage) { Diagnostic = FString(Stage) + TEXT(": ") + Diagnostic; UE_LOG(LogTemp, Error, TEXT("Assets fixture %s"), *Diagnostic); return false; };
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail(TEXT("Providers"));
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return Fail(TEXT("Declarations"));
		TArray<FName> SurfaceProviders;
		for (const FName Name : AssetProviderNames)
		{
			const bool bRecorded = Store->GetProviders().ContainsByPredicate([Name](const FAngelscriptRecordedProvider& Provider) { return Provider.Source.Provider == Name; });
			if (!bRecorded) SurfaceProviders.Add(Name);
		}
		if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, SurfaceProviders, *Store, Diagnostic)) return Fail(TEXT("Surfaces"));
		if (!Store->Seal(Diagnostic)) return Fail(TEXT("Seal"));
		if (!bInstall) return true;
		FAngelscriptBindingDiagnostic BindingDiagnostic; Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; return Fail(TEXT("Create")); }
		return true;
	}

	asCScriptFunction* Function(const TCHAR* OwnerName, const TCHAR* Declaration) const
	{
		for (const auto& Installed : Owner->GetBindingInstallation()->GetMembers())
		{
			const auto* Record = Store->GetMember(Installed.Record); const auto* Type = Store->GetType(Installed.Record.Type);
			if (Installed.Function && Record && Type && Type->Declaration.Name == OwnerName && Record->Declaration == Declaration) return Installed.Function;
		}
		return nullptr;
	}

	asCScriptFunction* Global(const TCHAR* Namespace, const TCHAR* Declaration) const
	{
		for (const auto& Installed : Owner->GetBindingInstallation()->GetMembers())
		{
			const auto* Record = Store->GetMember(Installed.Record); const auto* Type = Store->GetType(Installed.Record.Type);
			if (Installed.Function && Record && Type && Type->Declaration.Name.IsEmpty()
				&& Type->Declaration.Namespace == Namespace && Record->Declaration == Declaration) return Installed.Function;
		}
		return nullptr;
	}
};
}

using namespace RuntimeBindingAssetsFixture;
TEST_CLASS_WITH_FLAGS(Assets, "Angelscript.UnitTest.RuntimeBindings.Runtime.Assets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BundleCopiesNameAndAssetPath)
	{
		FAssetsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); FAssetBundleData Data; FName Bundle(TEXT("Gameplay"));
		FTopLevelAssetPath Source(TEXT("/Game/Fixture"), TEXT("Item"));
		auto* Add = F.Function(TEXT("FAssetBundleData"), TEXT("void AddBundleAsset(FName BundleName, const FTopLevelAssetPath& AssetPath)")); ASSERT_THAT(IsNotNull(Add));
		Data.AddBundleAsset(Bundle, Source);
		Source.Reset(); const FAssetBundleEntry* Stored = Data.FindEntry(Bundle); ASSERT_THAT(IsNotNull(Stored)); FAssetBundleEntry Copy(*Stored); ASSERT_THAT(IsTrue(Copy.BundleName == Bundle)); ASSERT_THAT(AreEqual(1, Copy.AssetPaths.Num())); ASSERT_THAT(IsTrue(Copy.AssetPaths[0] == FTopLevelAssetPath(TEXT("/Game/Fixture"), TEXT("Item"))));
	}

	TEST_METHOD(TransientTableRowRoundTripsKnownFields)
	{
		FAssetsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); UDataTable* Table = NewObject<UDataTable>(); Table->RowStruct = F.RowStruct;
		void* Input = FMemory::Malloc(F.RowStruct->GetStructureSize(), F.RowStruct->GetMinAlignment()); F.RowStruct->InitializeStruct(Input);
		ON_SCOPE_EXIT { F.RowStruct->DestroyStruct(Input); FMemory::Free(Input); };
		FindFProperty<FIntProperty>(F.RowStruct, TEXT("Number"))->SetPropertyValue_InContainer(Input, 17); FindFProperty<FStrProperty>(F.RowStruct, TEXT("Label"))->SetPropertyValue_InContainer(Input, TEXT("seventeen"));
		uint8* StoredRow = static_cast<uint8*>(FMemory::Malloc(F.RowStruct->GetStructureSize(), F.RowStruct->GetMinAlignment())); F.RowStruct->InitializeStruct(StoredRow); F.RowStruct->CopyScriptStruct(StoredRow, Input);
		auto& RowMap = const_cast<TMap<FName, uint8*>&>(Table->GetRowMap()); RowMap.Add(TEXT("Known"), StoredRow);
		ON_SCOPE_EXIT { RowMap.Remove(TEXT("Known")); F.RowStruct->DestroyStruct(StoredRow); FMemory::Free(StoredRow); };
		auto* Names = F.Function(TEXT("UDataTable"), TEXT("TArray<FName> GetRowNames() const")); ASSERT_THAT(IsNotNull(Names));
		asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Names))); ASSERT_THAT(AreEqual(0, C->SetObject(Table))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); const auto* Result = static_cast<const TArray<FName>*>(C->GetReturnObject()); ASSERT_THAT(IsNotNull(Result)); ASSERT_THAT(AreEqual(1, Result->Num())); ASSERT_THAT(IsTrue((*Result)[0] == TEXT("Known"))); C->Release();
		const void* Stored = Table->FindRowUnchecked(TEXT("Known")); ASSERT_THAT(IsNotNull(Stored)); ASSERT_THAT(AreEqual(17, FindFProperty<FIntProperty>(F.RowStruct, TEXT("Number"))->GetPropertyValue_InContainer(Stored))); ASSERT_THAT(AreEqual(FString(TEXT("seventeen")), FindFProperty<FStrProperty>(F.RowStruct, TEXT("Label"))->GetPropertyValue_InContainer(Stored)));
	}

	TEST_METHOD(MissingTableRowRetainsOutputAndReturnsFalseSurface)
	{
		FAssetsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); UDataTable* Table = NewObject<UDataTable>(); Table->RowStruct = F.RowStruct;
		ASSERT_THAT(IsNull(Table->FindRowUnchecked(TEXT("Missing"))));
		ASSERT_THAT(IsNotNull(F.Function(TEXT("UDataTable"), TEXT("bool FindRow(FName RowName, ?&out OutRow) const"))));
	}

	TEST_METHOD(TopLevelAndSoftPathsRoundTrip)
	{
		FAssetsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); FString Text(TEXT("/Game/Fixture.Item")); FTopLevelAssetPath Path;
		auto* Construct = F.Function(TEXT("FTopLevelAssetPath"), TEXT("void f(const FString& AssetPath)")); ASSERT_THAT(IsNotNull(Construct));
		asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Construct))); ASSERT_THAT(AreEqual(0, C->SetObject(&Path))); ASSERT_THAT(AreEqual(0, C->SetArgObject(0, &Text))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); C->Release();
		ASSERT_THAT(IsTrue(Path == FTopLevelAssetPath(TEXT("/Game/Fixture"), TEXT("Item")))); FSoftObjectPath Soft(Path, TEXT("Nested")); ASSERT_THAT(AreEqual(FString(TEXT("/Game/Fixture.Item:Nested")), Soft.ToString()));
	}

	TEST_METHOD(ManagerAndRegistrySurfacesExecuteWithoutDiskAssets)
	{
		FAssetsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); FString Text(TEXT("FixtureType:FixtureName")); FPrimaryAssetId Id;
		auto* Construct = F.Function(TEXT("FPrimaryAssetId"), TEXT("void f(const FString& InString)")); ASSERT_THAT(IsNotNull(Construct));
		asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Construct))); ASSERT_THAT(AreEqual(0, C->SetObject(&Id))); ASSERT_THAT(AreEqual(0, C->SetArgObject(0, &Text))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(Id == FPrimaryAssetId(FPrimaryAssetType(TEXT("FixtureType")), TEXT("FixtureName"))));
		auto* Loading = F.Global(TEXT("AssetRegistry"), TEXT("bool IsLoadingAssets()")); ASSERT_THAT(IsNotNull(Loading)); ASSERT_THAT(AreEqual(0, C->Prepare(Loading))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); C->Release();
	}

	TEST_METHOD(ProvidersAreCompleteAndTransientObjectsRelease)
	{
		FAssetsFixture F; ASSERT_THAT(IsTrue(F.Create(false), *F.Diagnostic));
		for (const FName Name : AssetProviderNames) ASSERT_THAT(IsTrue(F.Store->GetProviders().ContainsByPredicate([Name](const FAngelscriptRecordedProvider& Provider) { return Provider.Source.Provider == Name; })));
		int32 Contributions = 0, Recipes = 0; for (const auto& Type : F.Store->GetTypes()) for (const auto& Member : Type.Members) for (const FName Name : AssetProviderNames) if (Member.Source.Provider == Name) { ++Contributions; if (!Member.NativeRecipes.IsEmpty()) ++Recipes; break; }
		ASSERT_THAT(AreEqual(74, Contributions)); ASSERT_THAT(AreEqual(32, Recipes));
		TWeakObjectPtr<UDataTable> WeakTable = NewObject<UDataTable>(); CollectGarbage(RF_NoFlags); ASSERT_THAT(IsFalse(WeakTable.IsValid()));
	}
};
#endif
