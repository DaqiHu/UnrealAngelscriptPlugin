#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoInspection.h"
#include "Core/AngelscriptTypeBindInfoReflection.h"
#include "Dump/AngelscriptBindingDump.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
using namespace RuntimeBindingTestSupport;

TEST_CLASS_WITH_FLAGS(Manifest, "Angelscript.UnitTest.RuntimeBindings.Recording",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static TSharedRef<FAngelscriptTypeBindInfoStore> Surface(bool bReverse = false, int32 Constant = 7)
	{
		auto Store = MakeShared<FAngelscriptTypeBindInfoStore>(); FString Diagnostic;
		FAngelscriptRecordedProvider Provider;
		Provider.Source = {TEXT("AngelscriptTest"), TEXT("Manifest.Provider"), TEXT("Fixture,\"Unicode_绑定\".cpp"), 19, EAngelscriptBindPhase::ExplicitBindings};
		check(Store->RecordProvider(Provider, Diagnostic));
		Store->SetSource(Provider.Source); FAngelscriptBinds Binds(*Store); FBindFlags Flags; Flags.bPOD = true;
		FAngelscriptTypeBindDeclaration Base;
		Base.Name = TEXT("Base"); Base.Kind = EAngelscriptTypeBindKind::ObjectHandle; Base.Source = Provider.Source;
		Base.NativeSize = sizeof(void*); Base.NativeAlignment = alignof(void*); Base.ObjectFlags = asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE;
		FAngelscriptTypeBindHandle BaseHandle;
		if (bReverse) check(Store->RecordType(Base, BaseHandle, Diagnostic));
		auto Pair = Binds.ValueClassForTarget<FPair>("Pair", Flags);
		Pair.Property("int X", &FPair::X); Pair.Method("int Sum() const", &FPair::Sum);
		if (!bReverse) check(Store->RecordType(Base, BaseHandle, Diagnostic));
		FAngelscriptTypeBindDeclaration Interface;
		Interface.Name = TEXT("ITest"); Interface.Kind = EAngelscriptTypeBindKind::Interface; Interface.Source = Provider.Source;
		Interface.NativeSize = sizeof(void*); Interface.NativeAlignment = alignof(void*); Interface.ObjectFlags = asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE;
		FAngelscriptTypeBindHandle InterfaceHandle; check(Store->RecordType(Interface, InterfaceHandle, Diagnostic));
		FAngelscriptTypeBindDeclaration Derived;
		Derived.Name = TEXT("Derived"); Derived.Kind = EAngelscriptTypeBindKind::ObjectHandle;
		Derived.NativeSize = sizeof(void*); Derived.NativeAlignment = alignof(void*); Derived.ObjectFlags = asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE;
		Derived.BaseType = TEXT("Base"); Derived.Interfaces = {TEXT("ITest")}; Derived.Source = Provider.Source;
		FAngelscriptTypeBindHandle DerivedHandle; check(Store->RecordType(Derived, DerivedHandle, Diagnostic));
		{
			FAngelscriptBinds::FNamespace Namespace(Binds, "Test");
			Binds.BindGlobalFunctionForTarget("int Add(int A, int B = 1)", &Add);
			int32 Answer = Constant;
			Binds.BindGlobalVariableForTarget("const int Answer", &Answer).PureConstant(Constant);
		}
		check(Store->Seal(Diagnostic)); return Store;
	}
	static const FAngelscriptBindingInspectionType* Type(const FAngelscriptBindingInspection& Value, const TCHAR* Identity)
	{
		return Value.Types.FindByPredicate([Identity](const auto& Item) { return Item.Identity == Identity; });
	}
	static FString TempPath(const TCHAR* Leaf)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("BindingManifest"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits), Leaf);
	}
public:
	TEST_METHOD(IntendedSurfaceContainsQualifiedDeclarationsSourcesAndTypedReferences)
	{
		FAngelscriptBindingInspection Value; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(Surface(), EAngelscriptBindingInspectionScope::SelectedFamily, Value, Diagnostic)));
		ASSERT_THAT(IsTrue(Value.bSealed)); ASSERT_THAT(AreEqual(FString(), Value.Diagnostic)); ASSERT_THAT(IsTrue(Value.bValid)); ASSERT_THAT(IsFalse(Value.bCollectionOnly));
		const auto* Pair = Type(Value, TEXT("Pair")); ASSERT_THAT(IsNotNull(Pair)); ASSERT_THAT(AreEqual(FString(TEXT("value")), Pair->Kind));
		ASSERT_THAT(AreEqual(uint64(sizeof(FPair)), Pair->NativeSize)); ASSERT_THAT(AreEqual(2, Pair->Members.Num()));
		ASSERT_THAT(IsTrue(Pair->LifecycleRecipes.Contains(TEXT("copy-construct")))); ASSERT_THAT(IsTrue(Pair->LifecycleRecipes.Contains(TEXT("destroy"))));
		const auto* Property = Pair->Members.FindByPredicate([](const auto& M) { return M.Declaration == TEXT("int X"); });
		ASSERT_THAT(IsNotNull(Property)); ASSERT_THAT(AreEqual(FString(TEXT("int")), Property->TypeReference));
		ASSERT_THAT(AreEqual(FString(TEXT("builtin")), Property->TypeCategory));
		const auto* Scope = Type(Value, TEXT("namespace:Test")); ASSERT_THAT(IsNotNull(Scope));
		ASSERT_THAT(IsNotNull(Scope->Members.FindByPredicate([](const auto& M) { return M.Declaration.StartsWith(TEXT("int Add")); })));
		ASSERT_THAT(AreEqual(FString(TEXT("Fixture,\"Unicode_绑定\".cpp")), Pair->Sources[0].File));
	}

	TEST_METHOD(SemanticOutputIgnoresRegistrationOrderAndNativeAddresses)
	{
		FAngelscriptBindingInspection A, B; FString Diagnostic, JsonA, JsonB; TMap<FString, FString> TablesA, TablesB;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(Surface(false), EAngelscriptBindingInspectionScope::SelectedFamily, A, Diagnostic)));
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(Surface(true), EAngelscriptBindingInspectionScope::SelectedFamily, B, Diagnostic)));
		ASSERT_THAT(IsTrue(FAngelscriptBindingDump::Serialize(A, JsonA, TablesA, Diagnostic)));
		ASSERT_THAT(IsTrue(FAngelscriptBindingDump::Serialize(B, JsonB, TablesB, Diagnostic)));
		ASSERT_THAT(AreEqual(JsonA, JsonB)); ASSERT_THAT(AreEqual(TablesA.FindChecked(TEXT("members.csv")), TablesB.FindChecked(TEXT("members.csv"))));
	}

	TEST_METHOD(SemanticMutationsChangeOnlyRelevantManifestFacts)
	{
		FAngelscriptBindingInspection A, B; FString Diagnostic, JsonA, JsonB; TMap<FString, FString> Tables;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(Surface(false, 7), EAngelscriptBindingInspectionScope::SelectedFamily, A, Diagnostic)));
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(Surface(false, 9), EAngelscriptBindingInspectionScope::SelectedFamily, B, Diagnostic)));
		ASSERT_THAT(IsTrue(FAngelscriptBindingDump::Serialize(A, JsonA, Tables, Diagnostic)));
		ASSERT_THAT(IsTrue(FAngelscriptBindingDump::Serialize(B, JsonB, Tables, Diagnostic)));
		ASSERT_THAT(IsFalse(JsonA == JsonB));
		const auto* ScopeA = Type(A, TEXT("namespace:Test")); const auto* ScopeB = Type(B, TEXT("namespace:Test"));
		ASSERT_THAT(IsNotNull(ScopeA)); ASSERT_THAT(IsNotNull(ScopeB));
		const auto* AnswerA = ScopeA->Members.FindByPredicate([](const auto& M) { return M.Declaration == TEXT("const int Answer"); });
		const auto* AnswerB = ScopeB->Members.FindByPredicate([](const auto& M) { return M.Declaration == TEXT("const int Answer"); });
		ASSERT_THAT(IsNotNull(AnswerA)); ASSERT_THAT(IsNotNull(AnswerB));
		ASSERT_THAT(AreEqual(FString(TEXT("7")), AnswerA->ConstantValue)); ASSERT_THAT(AreEqual(FString(TEXT("9")), AnswerB->ConstantValue));
	}

	TEST_METHOD(ReflectionFactsKeepNestedShapeInterfacesExclusionsAndInt64Values)
	{
		auto* Struct = NewObject<UScriptStruct>(GetTransientPackage()); Struct->StructFlags = STRUCT_NoExport;
		Struct->SetMetaData(TEXT("ForceAngelscriptBind"), TEXT("true")); auto* Array = new FArrayProperty(Struct, TEXT("Objects"));
		auto* Inner = new FObjectProperty(Array, TEXT("Element")); Inner->PropertyClass = UObject::StaticClass(); Array->Inner = Inner;
		Array->SetPropertyFlags(CPF_BlueprintVisible); Struct->AddCppProperty(Array); Struct->StaticLink(true);
		auto* Enum = NewObject<UEnum>(GetTransientPackage()); TArray<TPair<FName, int64>> Values = {{TEXT("E::Small"), -3}, {TEXT("E::Large"), 1234567890123LL}};
		Enum->SetEnums(Values, UEnum::ECppForm::EnumClass, UEnum::EUnderlyingType::int64, EEnumFlags::None, UEnum::EAddMaxKeyIfMissing::No);
		auto* Excluded = NewObject<UClass>(GetTransientPackage()); Excluded->ClassFlags |= CLASS_Native; Excluded->SetSuperStruct(UObject::StaticClass()); Excluded->SetMetaData(TEXT("NotInAngelscript"), TEXT("true"));
		TArray<UObject*> Roots = {Struct, Enum, Excluded}; TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture(Roots, {}, Snapshot, Diagnostic)));
		auto Store = MakeShared<FAngelscriptTypeBindInfoStore>(); ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::RecordDeclarations(*Store, Snapshot.ToSharedRef(), Diagnostic)));
		ASSERT_THAT(IsTrue(Store->Seal(Diagnostic))); FAngelscriptBindingInspection Inspection;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(Store, EAngelscriptBindingInspectionScope::SelectedFamily, Inspection, Diagnostic)));
		const auto* EnumRow = Inspection.ReflectedTypes.FindByPredicate([Enum](const auto& R) { return R.NativePath == Enum->GetPathName(); });
		ASSERT_THAT(IsNotNull(EnumRow)); ASSERT_THAT(AreEqual(FString(TEXT("-3")), EnumRow->EnumValues[0].Value));
		ASSERT_THAT(AreEqual(FString(TEXT("1234567890123")), EnumRow->EnumValues[1].Value));
		const auto* StructRow = Inspection.ReflectedTypes.FindByPredicate([Struct](const auto& R) { return R.NativePath == Struct->GetPathName(); });
		ASSERT_THAT(IsNotNull(StructRow)); ASSERT_THAT(IsTrue(StructRow->Properties[0].Contains(TEXT("ArrayProperty"))));
		const auto* ExcludedRow = Inspection.ReflectedTypes.FindByPredicate([Excluded](const auto& R) { return R.NativePath == Excluded->GetPathName(); });
		ASSERT_THAT(IsNotNull(ExcludedRow)); ASSERT_THAT(IsFalse(ExcludedRow->bEligible)); ASSERT_THAT(IsFalse(ExcludedRow->ExclusionReason.IsEmpty()));
	}

	TEST_METHOD(CollectionInspectionAccountsForDescriptorsWithoutExecutingCallbacks)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection; FString Diagnostic; static int32 Calls = 0; Calls = 0;
		FAngelscriptBindRecord Record; Record.OwnerModule = TEXT("Test"); Record.BindName = TEXT("Manifest.Collection");
		Record.SourceFile = __FILE__; Record.SourceLine = __LINE__; Record.Callback = [](FAngelscriptBinds&) { ++Calls; };
		ASSERT_THAT(IsTrue(Collection.Append(Record, Diagnostic))); ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic)));
		FAngelscriptBindingInspection Inspection; ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectCollection(Collection, Inspection, Diagnostic)));
		ASSERT_THAT(AreEqual(0, Calls)); ASSERT_THAT(AreEqual(1, Inspection.Providers.Num())); ASSERT_THAT(IsTrue(Inspection.bCollectionOnly));
		ASSERT_THAT(AreEqual(FString(TEXT("collection-only")), Inspection.Providers[0].Disposition == TEXT("unconditional") ? TEXT("collection-only") : TEXT("wrong")));
	}

	TEST_METHOD(CompiledRuntimeCollectionIsInspectableWithoutProviderExecution)
	{
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		FAngelscriptBindingInspection Inspection; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectCollection(Collection, Inspection, Diagnostic)));
		ASSERT_THAT(AreEqual(Collection.GetRecords().Num(), Inspection.Providers.Num()));
		ASSERT_THAT(IsTrue(Inspection.bCollectionOnly)); ASSERT_THAT(IsTrue(Inspection.Types.IsEmpty()));
	}

	TEST_METHOD(JsonAndCsvRoundTripQuotedUnicodeFromOneProjection)
	{
		FAngelscriptBindingInspection Inspection; FString Diagnostic, Json; TMap<FString, FString> Tables;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(Surface(), EAngelscriptBindingInspectionScope::SelectedFamily, Inspection, Diagnostic)));
		ASSERT_THAT(IsTrue(FAngelscriptBindingDump::Serialize(Inspection, Json, Tables, Diagnostic)));
		TSharedPtr<FJsonObject> Root; const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		ASSERT_THAT(IsTrue(FJsonSerializer::Deserialize(Reader, Root))); ASSERT_THAT(AreEqual(1, Root->GetIntegerField(TEXT("schemaVersion"))));
		ASSERT_THAT(IsTrue(Tables.FindChecked(TEXT("providers.csv")).Contains(TEXT("\"Fixture,\"\"Unicode_绑定\"\".cpp\""))));
		ASSERT_THAT(IsTrue(Tables.Contains(TEXT("types.csv")) && Tables.Contains(TEXT("classes.csv")) && Tables.Contains(TEXT("members.csv"))
			&& Tables.Contains(TEXT("globals.csv")) && Tables.Contains(TEXT("reflection.csv")) && Tables.Contains(TEXT("exclusions.csv"))));
	}

	TEST_METHOD(SealedSemanticFailureRemainsInspectableButIsNotReady)
	{
		auto Store = MakeShared<FAngelscriptTypeBindInfoStore>(); FAngelscriptTypeBindDeclaration Type;
		Type.Name = TEXT("Broken"); Type.BaseType = TEXT("Missing"); Type.Source = {TEXT("Test"), TEXT("Broken.Provider"), TEXT("Broken.cpp"), 7};
		FAngelscriptTypeBindHandle Handle; FString Diagnostic; ASSERT_THAT(IsTrue(Store->RecordType(Type, Handle, Diagnostic))); ASSERT_THAT(IsTrue(Store->Seal(Diagnostic)));
		FAngelscriptBindingInspection Inspection; ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(Store, EAngelscriptBindingInspectionScope::SelectedFamily, Inspection, Diagnostic)));
		ASSERT_THAT(IsTrue(Inspection.bValidationAttempted)); ASSERT_THAT(IsFalse(Inspection.bValid)); ASSERT_THAT(IsFalse(Inspection.Diagnostic.IsEmpty()));
		FString Json; TMap<FString, FString> Tables; ASSERT_THAT(IsTrue(FAngelscriptBindingDump::Serialize(Inspection, Json, Tables, Diagnostic)));
		ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"valid\": false"))));
	}

	TEST_METHOD(ExportRejectsUnsealedAndOccupiedDestinationWithoutSuccessMetadata)
	{
		auto Unsealed = Surface(); // Build a distinct unsealed control because Surface deliberately seals.
		auto Mutable = MakeShared<FAngelscriptTypeBindInfoStore>(); FAngelscriptBindingDumpResult Result; FString Diagnostic;
		ASSERT_THAT(IsFalse(FAngelscriptBindingDump::ExportStoreToDirectory(Mutable, EAngelscriptBindingInspectionScope::SelectedFamily, TempPath(TEXT("unsealed")), Result, Diagnostic)));
		ASSERT_THAT(IsTrue(Result.Files.IsEmpty() && Result.SemanticJson.IsEmpty()));
		FAngelscriptBindingInspection Inspection; ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(Unsealed, EAngelscriptBindingInspectionScope::SelectedFamily, Inspection, Diagnostic)));
		const FString Occupied = TempPath(TEXT("occupied")); ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(*Occupied, true)));
		ASSERT_THAT(IsFalse(FAngelscriptBindingDump::ExportToDirectory(Inspection, Occupied, Result, Diagnostic)));
		ASSERT_THAT(IsTrue(Result.Files.IsEmpty() && Result.SemanticJson.IsEmpty()));
		IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Occupied), false, true);
	}

	TEST_METHOD(RepeatedExportAndRejectedMutationPreserveSemanticOutput)
	{
		auto Store = Surface(); FAngelscriptBindingInspection Before, After; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(Store, EAngelscriptBindingInspectionScope::SelectedFamily, Before, Diagnostic)));
		FAngelscriptTypeBindDeclaration Late; Late.Name = TEXT("Late"); Late.Source = {TEXT("Test"), TEXT("Late"), TEXT("Late.cpp"), 1};
		FAngelscriptTypeBindHandle Handle; ASSERT_THAT(IsFalse(Store->RecordType(Late, Handle, Diagnostic)));
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(Store, EAngelscriptBindingInspectionScope::SelectedFamily, After, Diagnostic)));
		FString BeforeJson, AfterJson; TMap<FString, FString> BeforeTables, AfterTables;
		ASSERT_THAT(IsTrue(FAngelscriptBindingDump::Serialize(Before, BeforeJson, BeforeTables, Diagnostic)));
		ASSERT_THAT(IsTrue(FAngelscriptBindingDump::Serialize(After, AfterJson, AfterTables, Diagnostic)));
		ASSERT_THAT(AreEqual(BeforeJson, AfterJson));
		const FString Directory = TempPath(TEXT("dump")); FAngelscriptBindingDumpResult Result;
		ASSERT_THAT(IsTrue(FAngelscriptBindingDump::ExportToDirectory(Before, Directory, Result, Diagnostic)));
		ASSERT_THAT(IsTrue(Result.Files.Contains(TEXT("manifest.json")))); ASSERT_THAT(IsTrue(FPaths::FileExists(FPaths::Combine(Directory, TEXT("globals.csv")))));
		FString DiskJson; ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(DiskJson, *FPaths::Combine(Directory, TEXT("manifest.json")))));
		ASSERT_THAT(AreEqual(BeforeJson, DiskJson));
		IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Directory), false, true);
	}
};
#endif
