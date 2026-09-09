#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Binds/Bind_FMemoryReader.h"
#include "Binds/Bind_Json.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"

#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryReader.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingSerializationFixture
{
static const FName SerializationProviderNames[] = {
	TEXT("Json.TypeDeclarations"), TEXT("Json.Manual"), TEXT("JsonObjectConverter.Functions"),
	TEXT("FInstancedStruct.Functions"), TEXT("FMemoryReader.Type"), TEXT("FMemoryReader.Functions")
};

struct FSerializationFixture
{
	UPackage* Package = nullptr;
	UScriptStruct* Struct = nullptr;
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	FSerializationFixture()
	{
		const FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		Package = CreatePackage(*FString::Printf(TEXT("/Temp/RuntimeBindingSerialization_%s"), *Id));
		Struct = NewObject<UScriptStruct>(Package, *FString::Printf(TEXT("BindingSerialization_%s"), *Id));
		Struct->StructFlags = STRUCT_NoExport;
		Struct->SetMetaData(TEXT("ForceAngelscriptBind"), TEXT("true"));
		auto* Count = new FIntProperty(Struct, TEXT("Count")); Count->SetPropertyFlags(CPF_BlueprintVisible); Struct->AddCppProperty(Count);
		auto* Name = new FStrProperty(Struct, TEXT("Name")); Name->SetPropertyFlags(CPF_BlueprintVisible); Struct->AddCppProperty(Name);
		Struct->StaticLink(true);
	}

	bool Create(bool bInstall = true)
	{
		auto Fail = [this](const TCHAR* Stage) { Diagnostic = FString(Stage) + TEXT(": ") + Diagnostic; UE_LOG(LogTemp, Error, TEXT("Serialization fixture %s"), *Diagnostic); return false; };
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail(TEXT("Providers"));
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return Fail(TEXT("Declarations"));
		TArray<FName> SurfaceProviders;
		for (const FName Name : SerializationProviderNames)
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

	asCScriptFunction* Function(const TCHAR* OwnerName, const TCHAR* Declaration, const TCHAR* Namespace = TEXT("")) const
	{
		for (const auto& Installed : Owner->GetBindingInstallation()->GetMembers())
		{
			const auto* Record = Store->GetMember(Installed.Record); const auto* Type = Store->GetType(Installed.Record.Type);
			if (Installed.Function && Record && Type && Type->Declaration.Name == OwnerName
				&& Type->Declaration.Namespace == Namespace && Record->Declaration == Declaration) return Installed.Function;
		}
		return nullptr;
	}

	void* AllocateStruct() const
	{
		void* Data = FMemory::Malloc(Struct->GetStructureSize(), Struct->GetMinAlignment()); Struct->InitializeStruct(Data); return Data;
	}
};
}

using namespace RuntimeBindingSerializationFixture;
TEST_CLASS_WITH_FLAGS(Serialization, "Angelscript.UnitTest.RuntimeBindings.Runtime.Serialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(JsonLiteralParsesAndExposesFields)
	{
		FSerializationFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		auto* Parse = F.Function(TEXT(""), TEXT("FJsonObject ParseString(const FString& JsonStr)"), TEXT("Json")); ASSERT_THAT(IsNotNull(Parse));
		FString Source(TEXT("{\"Count\":7,\"Name\":\"a\"}")); asIScriptContext* C = F.Owner->Engine->CreateContext();
		ASSERT_THAT(AreEqual(0, C->Prepare(Parse))); ASSERT_THAT(AreEqual(0, C->SetArgObject(0, &Source))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		const auto* Parsed = static_cast<const FJsonObjectContainer*>(C->GetReturnObject()); ASSERT_THAT(IsNotNull(Parsed)); FJsonObjectContainer Object(*Parsed);
		auto* GetNumber = F.Function(TEXT("FJsonObject"), TEXT("float64 GetNumberField(const FString& FieldName) const")); auto* GetString = F.Function(TEXT("FJsonObject"), TEXT("FString GetStringField(const FString& FieldName) const"));
		ASSERT_THAT(IsNotNull(GetNumber)); ASSERT_THAT(IsNotNull(GetString)); FString Count(TEXT("Count")); FString Name(TEXT("Name"));
		ASSERT_THAT(AreEqual(0, C->Prepare(GetNumber))); ASSERT_THAT(AreEqual(0, C->SetObject(&Object))); ASSERT_THAT(AreEqual(0, C->SetArgObject(0, &Count))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(C->GetReturnDouble() == 7.0));
		ASSERT_THAT(AreEqual(0, C->Prepare(GetString))); ASSERT_THAT(AreEqual(0, C->SetObject(&Object))); ASSERT_THAT(AreEqual(0, C->SetArgObject(0, &Name))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(FString(TEXT("a")), *static_cast<const FString*>(C->GetReturnObject()))); C->Release();
	}

	TEST_METHOD(MalformedJsonReportsFailure)
	{
		FSerializationFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		auto* Parse = F.Function(TEXT(""), TEXT("FJsonObject ParseString(const FString& JsonStr)"), TEXT("Json")); ASSERT_THAT(IsNotNull(Parse));
		FString Source(TEXT("{\"Count\":")); asIScriptContext* C = F.Owner->Engine->CreateContext();
		ASSERT_THAT(AreEqual(0, C->Prepare(Parse))); ASSERT_THAT(AreEqual(0, C->SetArgObject(0, &Source))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		const auto* Parsed = static_cast<const FJsonObjectContainer*>(C->GetReturnObject()); ASSERT_THAT(IsNotNull(Parsed)); ASSERT_THAT(IsFalse(Parsed->JsonObject.IsValid())); C->Release();
	}

	TEST_METHOD(ReflectedStructRoundTripPreservesFields)
	{
		FSerializationFixture F; void* Input = F.AllocateStruct(); void* Output = F.AllocateStruct();
		auto* Count = FindFProperty<FIntProperty>(F.Struct, TEXT("Count")); auto* Name = FindFProperty<FStrProperty>(F.Struct, TEXT("Name"));
		Count->SetPropertyValue_InContainer(Input, 7); Name->SetPropertyValue_InContainer(Input, TEXT("a")); FString Json;
		ASSERT_THAT(IsTrue(FJsonObjectConverter::UStructToJsonObjectString(F.Struct, Input, Json, 0, 0, 0, nullptr, false)));
		TSharedPtr<FJsonObject> Parsed; const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		ASSERT_THAT(IsTrue(FJsonSerializer::Deserialize(Reader, Parsed))); ASSERT_THAT(IsTrue(Parsed.IsValid()));
		ASSERT_THAT(IsTrue(FJsonObjectConverter::JsonObjectToUStruct(Parsed.ToSharedRef(), F.Struct, Output, 0, 0)));
		ASSERT_THAT(AreEqual(7, Count->GetPropertyValue_InContainer(Output))); ASSERT_THAT(AreEqual(FString(TEXT("a")), Name->GetPropertyValue_InContainer(Output)));
		F.Struct->DestroyStruct(Input); F.Struct->DestroyStruct(Output); FMemory::Free(Input); FMemory::Free(Output);
	}

	TEST_METHOD(InstancedStructCopyOwnsIndependentValue)
	{
		FSerializationFixture F; void* Input = F.AllocateStruct(); auto* Count = FindFProperty<FIntProperty>(F.Struct, TEXT("Count")); auto* Name = FindFProperty<FStrProperty>(F.Struct, TEXT("Name"));
		Count->SetPropertyValue_InContainer(Input, 7); Name->SetPropertyValue_InContainer(Input, TEXT("a")); FInstancedStruct Original; Original.InitializeAs(F.Struct, static_cast<const uint8*>(Input)); FInstancedStruct Copy(Original);
		Count->SetPropertyValue_InContainer(Original.GetMutableMemory(), 9); Name->SetPropertyValue_InContainer(Original.GetMutableMemory(), TEXT("changed"));
		ASSERT_THAT(AreEqual(7, Count->GetPropertyValue_InContainer(Copy.GetMemory()))); ASSERT_THAT(AreEqual(FString(TEXT("a")), Name->GetPropertyValue_InContainer(Copy.GetMemory())));
		Original.Reset(); ASSERT_THAT(IsFalse(Original.IsValid())); ASSERT_THAT(IsTrue(Copy.IsValid())); Copy.Reset(); F.Struct->DestroyStruct(Input); FMemory::Free(Input);
	}

	TEST_METHOD(MemoryReaderReadsKnownSequenceAndRejectsOverrun)
	{
		FSerializationFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); TArray<uint8> Bytes = {0x2a, 0x78, 0x56, 0x34, 0x12}; FMemoryReader Reader(Bytes);
		auto* ReadByte = F.Function(TEXT("FMemoryReader"), TEXT("uint8 ReadUInt8()")); auto* ReadInt = F.Function(TEXT("FMemoryReader"), TEXT("int32 ReadInt32()")); auto* Skip = F.Function(TEXT("FMemoryReader"), TEXT("void Skip(int Count)"));
		ASSERT_THAT(IsNotNull(ReadByte)); ASSERT_THAT(IsNotNull(ReadInt)); ASSERT_THAT(IsNotNull(Skip)); asIScriptContext* C = F.Owner->Engine->CreateContext();
		ASSERT_THAT(AreEqual(0, C->Prepare(ReadByte))); ASSERT_THAT(AreEqual(0, C->SetObject(&Reader))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(asBYTE(0x2a), C->GetReturnByte()));
		ASSERT_THAT(AreEqual(0, C->Prepare(ReadInt))); ASSERT_THAT(AreEqual(0, C->SetObject(&Reader))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(asDWORD(0x12345678), C->GetReturnDWord()));
		ASSERT_THAT(AreEqual(0, C->Prepare(Skip))); ASSERT_THAT(AreEqual(0, C->SetObject(&Reader))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(0, 1))); ASSERT_THAT(AreEqual(int(asEXECUTION_EXCEPTION), C->Execute())); ASSERT_THAT(AreEqual(int64(5), Reader.Tell())); C->Release();
	}

	TEST_METHOD(EverySerializationProviderIsAccounted)
	{
		FSerializationFixture F; ASSERT_THAT(IsTrue(F.Create(false), *F.Diagnostic));
		for (const FName Name : SerializationProviderNames) ASSERT_THAT(IsTrue(F.Store->GetProviders().ContainsByPredicate([Name](const FAngelscriptRecordedProvider& Provider) { return Provider.Source.Provider == Name; })));
		int32 Contributions = 0, Recipes = 0; for (const auto& Type : F.Store->GetTypes()) for (const auto& Member : Type.Members) for (const FName Name : SerializationProviderNames) if (Member.Source.Provider == Name) { ++Contributions; if (!Member.NativeRecipes.IsEmpty()) ++Recipes; break; }
		ASSERT_THAT(AreEqual(84, Contributions)); ASSERT_THAT(AreEqual(12, Recipes));
	}
};
#endif
