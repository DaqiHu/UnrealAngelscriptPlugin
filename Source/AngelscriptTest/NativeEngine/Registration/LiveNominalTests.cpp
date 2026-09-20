#include "CQTest.h"
#include "as_definitions.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(LiveNominal, "Angelscript.UnitTest.NativeEngine.Registration",
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

	static bool RegisterColor(asCScriptEngine& Engine)
	{
		return Engine.RegisterEnum("Color") >= 0
			&& Engine.RegisterEnumValue("Color", "Red", 3) >= 0;
	}

	static bool RegisterIValue(asCScriptEngine& Engine)
	{
		return Engine.RegisterInterface("IValue") >= 0
			&& Engine.RegisterInterfaceMethod("IValue", "int Value()") >= 0;
	}

	static int32 EnumRed(asITypeInfo* Type)
	{
		if (!Type) return MIN_int32;
		for (asUINT Index = 0; Index < Type->GetEnumValueCount(); ++Index)
		{
			int Value = MIN_int32;
			const char* Name = Type->GetEnumValueByIndex(Index, &Value);
			if (Name && FCStringAnsi::Strcmp(Name, "Red") == 0) return Value;
		}
		return MIN_int32;
	}

public:
	TEST_METHOD(NominalRegistrationQueries)
	{
		FEngineOwner A;
		ASSERT_THAT(IsTrue(RegisterIValue(*A.Ptr)));
		ASSERT_THAT(IsTrue(RegisterColor(*A.Ptr)));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterTypedef("Count", "int") >= 0));
		asITypeInfo* Interface = A.Ptr->GetTypeInfoByName("IValue");
		asITypeInfo* Enum = A.Ptr->GetTypeInfoByName("Color");
		asITypeInfo* Alias = A.Ptr->GetTypeInfoByName("Count");
		ASSERT_THAT(IsNotNull(Interface));
		ASSERT_THAT(IsNotNull(Enum));
		ASSERT_THAT(IsNotNull(Alias));
		ASSERT_THAT(IsTrue(static_cast<asCTypeInfo*>(Interface)->GetTypeInfoKind() == asETypeInfoKind::LiveRegister));
		asIScriptFunction* Value = Interface->GetMethodByName("Value");
		ASSERT_THAT(IsNotNull(Value));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, Value->GetReturnTypeId()));
		ASSERT_THAT(AreEqual(3, EnumRed(Enum)));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, Alias->GetTypedefTypeId()));
	}

	TEST_METHOD(NamespacesAndConflicts)
	{
		FEngineOwner A;
		ASSERT_THAT(IsTrue(A.Ptr->RegisterEnum("Palette") >= 0));
		ASSERT_THAT(IsTrue(A.Ptr->SetDefaultNamespace("N") >= 0));
		ASSERT_THAT(IsTrue(RegisterColor(*A.Ptr)));
		asITypeInfo* Nested = A.Ptr->GetTypeInfoByName("Color");
		ASSERT_THAT(IsNotNull(Nested));
		ASSERT_THAT(AreEqual(3, EnumRed(Nested)));
		ASSERT_THAT(IsTrue(A.Ptr->SetDefaultNamespace("") >= 0));
		asITypeInfo* Global = A.Ptr->GetTypeInfoByName("Palette");
		ASSERT_THAT(IsNotNull(Global));
		ASSERT_THAT(IsTrue(Global != Nested));
		ASSERT_THAT(IsNull(A.Ptr->GetTypeInfoByName("Color")));
		TUniquePtr<asCDefinitions> Producer = asCDefinitions::Create([]
		{
			asSDefinitionOptions Options;
			Options.Kind = asETypeInfoKind::HostProcess;
			return Options;
		}());
		asCEnumType* HostColor = nullptr;
		ASSERT_THAT(IsTrue(Producer->CreateEnumType(Key(90), "Color", "", HostColor) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Producer->AddEnumValue(*HostColor, "Red", 3) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Producer->FinalizeLayouts() == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Producer->Freeze() == asEDefinitionResult::Succeeded));
		asCDefinitionGraphLease Graph = asCDefinitions::AdoptGraph(MoveTemp(Producer));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		const asUINT Enums = A.Ptr->GetEnumCount();
		ASSERT_THAT(IsTrue(A.Ptr->RegisterEnum("Color") < 0));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterEnumValue("Color", "Blue", 4) < 0));
		ASSERT_THAT(AreEqual(Enums, A.Ptr->GetEnumCount()));
		ASSERT_THAT(IsTrue(A.Ptr->GetTypeInfoByName("Color") == HostColor));
		ASSERT_THAT(AreEqual(3, EnumRed(HostColor)));
	}

	TEST_METHOD(PrivateNominalLifetimes)
	{
		FEngineOwner A;
		FEngineOwner B;
		ASSERT_THAT(IsTrue(RegisterColor(*A.Ptr) && RegisterIValue(*A.Ptr)));
		ASSERT_THAT(IsTrue(RegisterColor(*B.Ptr) && RegisterIValue(*B.Ptr)));
		asITypeInfo* ColorA = A.Ptr->GetTypeInfoByName("Color");
		asITypeInfo* ColorB = B.Ptr->GetTypeInfoByName("Color");
		asITypeInfo* InterfaceB = B.Ptr->GetTypeInfoByName("IValue");
		ASSERT_THAT(IsNotNull(ColorA));
		ASSERT_THAT(IsNotNull(ColorB));
		ASSERT_THAT(IsTrue(ColorA != ColorB));
		ASSERT_THAT(IsTrue(ColorA->GetEngine() == A.Ptr));
		ASSERT_THAT(IsTrue(ColorB->GetEngine() == B.Ptr));
		const asUINT MethodCount = InterfaceB->GetMethodCount();
		ASSERT_THAT(IsTrue(B.Ptr->RegisterTypedef("Count", "Missing") < 0));
		ASSERT_THAT(IsTrue(B.Ptr->RegisterInterfaceMethod("IValue", "Missing Value()") < 0));
		ASSERT_THAT(AreEqual(MethodCount, InterfaceB->GetMethodCount()));
		A.Reset();
		ASSERT_THAT(AreEqual(3, EnumRed(B.Ptr->GetTypeInfoByName("Color"))));
		ASSERT_THAT(IsNotNull(B.Ptr->GetTypeInfoByName("IValue")->GetMethodByName("Value")));
	}
};

#endif
