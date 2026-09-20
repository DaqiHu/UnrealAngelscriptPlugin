#include "CQTest.h"
#include "as_datatype.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_typeinfo.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FLiveServicesConstant
	{
		TArray<char> Bytes;
	};

	class FLiveServicesFactory final : public asIStringFactory
	{
	public:
		mutable int32 Acquires = 0;
		mutable int32 Releases = 0;
		TArray<TUniquePtr<FLiveServicesConstant>> Owned;

		const void* GetStringConstant(const char* Data, asUINT Length) override
		{
			++Acquires;
			auto Constant = MakeUnique<FLiveServicesConstant>();
			if (Length && Data) Constant->Bytes.Append(Data, static_cast<int32>(Length));
			const void* Pointer = Constant.Get();
			Owned.Add(MoveTemp(Constant));
			return Pointer;
		}

		int ReleaseStringConstant(const void* Str) override
		{
			++Releases;
			for (int32 Index = 0; Index < Owned.Num(); ++Index)
			{
				if (Owned[Index].Get() == Str)
				{
					Owned.RemoveAt(Index);
					return 0;
				}
			}
			return -1;
		}

		int GetRawStringData(const void* Str, char* Data, asUINT* Length) const override
		{
			const auto* Constant = static_cast<const FLiveServicesConstant*>(Str);
			if (!Constant) return -1;
			if (Length) *Length = static_cast<asUINT>(Constant->Bytes.Num());
			if (Data && Constant->Bytes.Num()) FMemory::Memcpy(Data, Constant->Bytes.GetData(), Constant->Bytes.Num());
			return 0;
		}
	};
}

TEST_CLASS_WITH_FLAGS(LiveServices, "Angelscript.UnitTest.NativeEngine.Registration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
		void Reset() { if (Ptr) Ptr->ShutDownAndRelease(); Ptr = nullptr; }
	};

	static AS_NAMESPACE_QUALIFIER asSStableKey Key(uint32 Value)
	{
		uint8 Bytes[32] = {};
		for (int Index = 0; Index != 4; ++Index)
		{
			Bytes[Index] = static_cast<uint8>(Value >> (Index * 8));
		}
		AS_NAMESPACE_QUALIFIER asSStableKey Result;
		check(AS_NAMESPACE_QUALIFIER asSStableKey::TryCreate(MakeArrayView(Bytes), Result));
		return Result;
	}

	static asSDefinitionOptions HostOptions()
	{
		asSDefinitionOptions Options;
		Options.Kind = asETypeInfoKind::HostProcess;
		return Options;
	}

	static bool FreezeHost(asCDefinitions& Graph)
	{
		return Graph.FinalizeLayouts() == asEDefinitionResult::Succeeded
			&& Graph.Freeze() == asEDefinitionResult::Succeeded;
	}

	static bool BuildHostString(asCDefinitions& Graph, asCObjectType*& Type)
	{
		Type = nullptr;
		return Graph.CreateObjectType(Key(40), "String", "", asOBJ_VALUE | asOBJ_APP_CLASS, Type) == asEDefinitionResult::Succeeded
			&& Graph.SetNativeLayout(*Type, 8, 8) == asEDefinitionResult::Succeeded
			&& FreezeHost(Graph);
	}

	static bool BuildHostArray(asCDefinitions& Graph, asCObjectType*& Type, uint32 TypeKey)
	{
		Type = nullptr;
		return Graph.CreateObjectType(Key(TypeKey), "Array", "", asOBJ_TEMPLATE | asOBJ_REF | asOBJ_NOCOUNT, Type) == asEDefinitionResult::Succeeded
			&& Graph.SetTypeDeclarationFacts(*Type, asETypeDeclKind::Class, 1) == asEDefinitionResult::Succeeded
			&& Graph.SetFixedTemplateLayout(*Type, static_cast<asUINT>(sizeof(void*)), static_cast<asUINT>(sizeof(void*))) == asEDefinitionResult::Succeeded
			&& FreezeHost(Graph);
	}

	static asCObjectType* ResolveIntArray(asCScriptEngine& Engine)
	{
		asCDataType Element = asCDataType::CreatePrimitive(ttInt, false);
		if (Element.MakeArray(&Engine, nullptr) != 0) return nullptr;
		return Element.GetTypeInfo() ? static_cast<asCObjectType*>(Element.GetTypeInfo()) : nullptr;
	}

public:
	TEST_METHOD(StringFactoryRoundTrip)
	{
		FEngineOwner A;
		TUniquePtr<asCDefinitions> Producer = asCDefinitions::Create(HostOptions());
		asCObjectType* StringType = nullptr;
		ASSERT_THAT(IsTrue(BuildHostString(*Producer, StringType)));
		asCDefinitionGraphLease Graph = asCDefinitions::AdoptGraph(MoveTemp(Producer));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		FLiveServicesFactory Factory;
		ASSERT_THAT(AreEqual(0, A.Ptr->RegisterStringFactory("String", &Factory)));
		asDWORD Flags = 0xFFFFFFFFu;
		const int TypeId = A.Ptr->GetStringFactoryReturnTypeId(&Flags);
		ASSERT_THAT(AreEqual(StringType->GetTypeId(), TypeId));
		const void* Literal = A.Ptr->stringFactory->GetStringConstant("abc", 3);
		ASSERT_THAT(IsNotNull(Literal));
		asUINT Length = 0;
		ASSERT_THAT(AreEqual(0, Factory.GetRawStringData(Literal, nullptr, &Length)));
		ASSERT_THAT(AreEqual(3u, Length));
		char Bytes[4] = {};
		ASSERT_THAT(AreEqual(0, Factory.GetRawStringData(Literal, Bytes, &Length)));
		ASSERT_THAT(AreEqual(0, FCStringAnsi::Strncmp(Bytes, "abc", 3)));
		ASSERT_THAT(AreEqual(0, A.Ptr->stringFactory->ReleaseStringConstant(Literal)));
		ASSERT_THAT(AreEqual(1, Factory.Acquires));
		ASSERT_THAT(AreEqual(1, Factory.Releases));
	}

	TEST_METHOD(DefaultArraySelection)
	{
		FEngineOwner A;
		FEngineOwner B;
		TUniquePtr<asCDefinitions> ProducerA = asCDefinitions::Create(HostOptions());
		TUniquePtr<asCDefinitions> ProducerB = asCDefinitions::Create(HostOptions());
		asCObjectType* ArrayA = nullptr;
		asCObjectType* ArrayB = nullptr;
		ASSERT_THAT(IsTrue(BuildHostArray(*ProducerA, ArrayA, 50)));
		ASSERT_THAT(IsTrue(BuildHostArray(*ProducerB, ArrayB, 51)));
		asCDefinitionGraphLease GraphA = asCDefinitions::AdoptGraph(MoveTemp(ProducerA));
		asCDefinitionGraphLease GraphB = asCDefinitions::AdoptGraph(MoveTemp(ProducerB));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*GraphA) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*GraphB) == asERegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(0, A.Ptr->RegisterDefaultArrayType("Array")));
		const int Chosen = A.Ptr->GetDefaultArrayTypeId();
		ASSERT_THAT(AreEqual(ArrayA->GetTypeId(), Chosen));
		ASSERT_THAT(IsTrue(B.Ptr->GetDefaultArrayTypeId() < 0));
		asCObjectType* Resolved = ResolveIntArray(*A.Ptr);
		ASSERT_THAT(IsNotNull(Resolved));
		ASSERT_THAT(IsTrue(Resolved->templateBaseType == ArrayA));
		ASSERT_THAT(IsNull(ResolveIntArray(*B.Ptr)));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterDefaultArrayType("Missing") < 0));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterDefaultArrayType("String") < 0));
		ASSERT_THAT(AreEqual(Chosen, A.Ptr->GetDefaultArrayTypeId()));
	}

	TEST_METHOD(SharedDefinitionRemainsFrozen)
	{
		FEngineOwner A;
		FEngineOwner B;
		TUniquePtr<asCDefinitions> Producer = asCDefinitions::Create(HostOptions());
		asCObjectType* StringType = nullptr;
		asCObjectType* ArrayType = nullptr;
		ASSERT_THAT(IsTrue(Producer->CreateObjectType(Key(40), "String", "", asOBJ_VALUE | asOBJ_APP_CLASS, StringType) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Producer->SetNativeLayout(*StringType, 8, 8) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Producer->CreateObjectType(Key(50), "Array", "", asOBJ_TEMPLATE | asOBJ_REF | asOBJ_NOCOUNT, ArrayType) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Producer->SetTypeDeclarationFacts(*ArrayType, asETypeDeclKind::Class, 1) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Producer->SetFixedTemplateLayout(*ArrayType, static_cast<asUINT>(sizeof(void*)), static_cast<asUINT>(sizeof(void*))) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(FreezeHost(*Producer)));
		asCDefinitionGraphLease Graph = asCDefinitions::AdoptGraph(MoveTemp(Producer));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		const asQWORD StringFlags = StringType->GetFlags();
		FLiveServicesFactory Factory;
		ASSERT_THAT(AreEqual(0, A.Ptr->RegisterStringFactory("String", &Factory)));
		ASSERT_THAT(AreEqual(0, A.Ptr->RegisterDefaultArrayType("Array")));
		ASSERT_THAT(IsTrue(A.Ptr->stringFactory == &Factory));
		ASSERT_THAT(IsNull(B.Ptr->stringFactory));
		ASSERT_THAT(IsTrue(B.Ptr->GetDefaultArrayTypeId() < 0));
		ASSERT_THAT(IsNull(StringType->GetEngine()));
		ASSERT_THAT(AreEqual(StringFlags, StringType->GetFlags()));
		ASSERT_THAT(IsTrue(B.Ptr->GetTypeInfoByName("String") == StringType));
		A.Ptr->RetireExternalDefinitions();
		ASSERT_THAT(IsTrue(A.Ptr->RegisterStringFactory("String", &Factory) < 0));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterDefaultArrayType("Array") < 0));
		ASSERT_THAT(IsTrue(B.Ptr->GetTypeInfoByName("String") == StringType));
		ASSERT_THAT(IsNull(StringType->GetEngine()));
	}
};

#endif
