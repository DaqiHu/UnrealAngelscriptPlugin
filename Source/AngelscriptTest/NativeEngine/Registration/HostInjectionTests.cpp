#include "CQTest.h"
#include "as_callfunc.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

struct FHostInjectionPair
{
	int32 X = 0;
	int32 Y = 0;
};

int32 HostInjectionPairSum(FHostInjectionPair* Self)
{
	return Self->X + Self->Y;
}

TEST_CLASS_WITH_FLAGS(HostInjection, "Angelscript.UnitTest.NativeEngine.Registration",
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

	static bool Success(asEDefinitionResult Result)
	{
		return Result == asEDefinitionResult::Succeeded;
	}

	static bool Injected(asERegistrationResult Result)
	{
		return Result == asERegistrationResult::Succeeded;
	}

	static int32 CallSum(const asCScriptFunction& Function, FHostInjectionPair& Pair)
	{
		const std::shared_ptr<asSSystemFunctionInterface> Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->func)
		{
			return MIN_int32;
		}
		using FSum = int32(*)(FHostInjectionPair*);
		return reinterpret_cast<FSum>(Interface->func)(&Pair);
	}

	static int32 CountRegistered(const asCScriptEngine& Engine, const asCDefinitions* Graph)
	{
		int32 Count = 0;
		for (const asCDefinitions* Item : Engine.GetRegisteredDefinitions())
		{
			if (Item == Graph)
			{
				++Count;
			}
		}
		return Count;
	}

	bool BuildHostPair(
		asCDefinitions& Graph,
		asCObjectType*& Type,
		asCScriptFunction*& Sum,
		uint32 TypeKey,
		uint32 FunctionKey)
	{
		Type = nullptr;
		Sum = nullptr;
		asCObjectProperty* X = nullptr;
		asCObjectProperty* Y = nullptr;
		asSNativePropertyAddress XAddress;
		XAddress.Offset = static_cast<int>(offsetof(FHostInjectionPair, X));
		asSNativePropertyAddress YAddress;
		YAddress.Offset = static_cast<int>(offsetof(FHostInjectionPair, Y));
		return Success(Graph.CreateObjectType(Key(TypeKey), "Pair", "", asOBJ_VALUE | asOBJ_APP_CLASS, Type))
			&& Success(Graph.SetNativeLayout(*Type, sizeof(FHostInjectionPair), alignof(FHostInjectionPair)))
			&& Success(Graph.DefineNativeProperty(*Type, "X", Graph.Primitive(ttInt), XAddress, X))
			&& Success(Graph.DefineNativeProperty(*Type, "Y", Graph.Primitive(ttInt), YAddress, Y))
			&& Success(Graph.CreateFunction(Key(FunctionKey), "Sum", "", asFUNC_SYSTEM, Sum))
			&& Success(Graph.DefineFunction(*Sum, Graph.Primitive(ttInt), {}, Type))
			&& Success(Graph.BindNativeFunction(*Sum, asFUNCTION(HostInjectionPairSum), asCALL_CDECL_OBJFIRST))
			&& Success(Graph.FinalizeLayouts())
			&& Success(Graph.Freeze());
	}

public:
	TEST_METHOD(TwoReceivingDirectories)
	{
		FEngineOwner A;
		FEngineOwner B;
		TUniquePtr<asCDefinitions> Producer = asCDefinitions::Create(HostOptions());
		asCObjectType* Type = nullptr;
		asCScriptFunction* Sum = nullptr;
		ASSERT_THAT(IsTrue(BuildHostPair(*Producer, Type, Sum, 1, 2)));
		const int TypeId = Type->GetTypeId();
		const int FunctionId = Sum->GetId();
		asCDefinitionGraphLease Graph = asCDefinitions::AdoptGraph(MoveTemp(Producer));
		ASSERT_THAT(IsTrue(Injected(A.Ptr->InjectDefinitions(*Graph))));
		ASSERT_THAT(IsTrue(Injected(B.Ptr->InjectDefinitions(*Graph))));
		ASSERT_THAT(IsTrue(A.Ptr->FindType("", "Pair") == Type));
		ASSERT_THAT(IsTrue(B.Ptr->FindType("", "Pair") == Type));
		ASSERT_THAT(IsTrue(A.Ptr->FindType(Key(1)) == Type));
		ASSERT_THAT(IsTrue(B.Ptr->FindType(Key(1)) == Type));
		ASSERT_THAT(IsTrue(A.Ptr->FindFunction(Key(2)) == Sum));
		ASSERT_THAT(IsTrue(B.Ptr->FindFunction(Key(2)) == Sum));
		ASSERT_THAT(IsTrue(A.Ptr->GetTypeInfoById(TypeId) == Type));
		ASSERT_THAT(IsTrue(B.Ptr->GetTypeInfoById(TypeId) == Type));
		ASSERT_THAT(IsTrue(A.Ptr->GetFunctionById(FunctionId) == Sum));
		ASSERT_THAT(IsTrue(B.Ptr->GetFunctionById(FunctionId) == Sum));
		ASSERT_THAT(IsNull(Type->GetEngine()));
		ASSERT_THAT(IsNull(Sum->GetEngine()));
		ASSERT_THAT(AreEqual(TypeId, Type->GetTypeId()));
		ASSERT_THAT(AreEqual(FunctionId, Sum->GetId()));
		FHostInjectionPair Pair{20, 22};
		ASSERT_THAT(AreEqual(42, CallSum(*Sum, Pair)));
	}

	TEST_METHOD(ConflictLeavesPriorState)
	{
		FEngineOwner A;
		FEngineOwner B;
		TUniquePtr<asCDefinitions> Producer = asCDefinitions::Create(HostOptions());
		asCObjectType* Type = nullptr;
		asCScriptFunction* Sum = nullptr;
		ASSERT_THAT(IsTrue(BuildHostPair(*Producer, Type, Sum, 10, 11)));
		asCDefinitionGraphLease Graph = asCDefinitions::AdoptGraph(MoveTemp(Producer));
		ASSERT_THAT(IsTrue(Injected(A.Ptr->InjectDefinitions(*Graph))));
		ASSERT_THAT(IsTrue(Injected(B.Ptr->InjectDefinitions(*Graph))));
		const auto ABefore = A.Ptr->GetRegistrationCounts();
		const auto BBefore = B.Ptr->GetRegistrationCounts();
		const int TypeId = Type->GetTypeId();
		const int FunctionId = Sum->GetId();

		TUniquePtr<asCDefinitions> NameClash = asCDefinitions::Create(HostOptions());
		asCObjectType* OtherType = nullptr;
		asCScriptFunction* OtherSum = nullptr;
		ASSERT_THAT(IsTrue(BuildHostPair(*NameClash, OtherType, OtherSum, 12, 13)));
		asCDefinitionGraphLease NameClashLease = asCDefinitions::AdoptGraph(MoveTemp(NameClash));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*NameClashLease) == asERegistrationResult::NameConflict));

		TUniquePtr<asCDefinitions> Unfrozen = asCDefinitions::Create(HostOptions());
		asCObjectType* Late = nullptr;
		ASSERT_THAT(IsTrue(Success(Unfrozen->CreateObjectType(Key(14), "Late", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Late))));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Unfrozen) == asERegistrationResult::InvalidState));

		TUniquePtr<asCDefinitions> Script = asCDefinitions::Create();
		asCObjectType* ScriptType = nullptr;
		ASSERT_THAT(IsTrue(Success(Script->CreateObjectType(Key(15), "Box", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, ScriptType))));
		ASSERT_THAT(IsTrue(Success(Script->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Script->Freeze())));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Script) != asERegistrationResult::Succeeded));

		ASSERT_THAT(IsTrue(A.Ptr->GetRegistrationCounts() == ABefore));
		ASSERT_THAT(IsTrue(B.Ptr->GetRegistrationCounts() == BBefore));
		ASSERT_THAT(IsTrue(A.Ptr->FindType("", "Pair") == Type));
		ASSERT_THAT(IsTrue(B.Ptr->FindType("", "Pair") == Type));
		ASSERT_THAT(IsTrue(A.Ptr->GetTypeInfoById(TypeId) == Type));
		ASSERT_THAT(IsTrue(A.Ptr->GetFunctionById(FunctionId) == Sum));
		ASSERT_THAT(AreEqual(TypeId, Type->GetTypeId()));
		ASSERT_THAT(IsNull(Type->GetEngine()));
		FHostInjectionPair Pair{20, 22};
		ASSERT_THAT(AreEqual(42, CallSum(*Sum, Pair)));
	}

	TEST_METHOD(DetachAndRetry)
	{
		FEngineOwner A;
		FEngineOwner B;
		TUniquePtr<asCDefinitions> Producer = asCDefinitions::Create(HostOptions());
		asCObjectType* Type = nullptr;
		asCScriptFunction* Sum = nullptr;
		ASSERT_THAT(IsTrue(BuildHostPair(*Producer, Type, Sum, 20, 21)));
		asSDefinitionDestroyCounters Counters;
		Producer->SetDestroyCounters(&Counters);
		asCDefinitionGraphLease Graph = asCDefinitions::AdoptGraph(MoveTemp(Producer));
		ASSERT_THAT(IsTrue(Injected(A.Ptr->InjectDefinitions(*Graph))));
		ASSERT_THAT(IsTrue(Injected(B.Ptr->InjectDefinitions(*Graph))));
		ASSERT_THAT(AreEqual(1, CountRegistered(*A.Ptr, Graph.Get())));
		ASSERT_THAT(AreEqual(1, CountRegistered(*B.Ptr, Graph.Get())));
		const auto BBefore = B.Ptr->GetRegistrationCounts();
		Graph = asCDefinitionGraphLease();
		A.Reset();
		ASSERT_THAT(IsTrue(B.Ptr->FindType("", "Pair") == Type));
		ASSERT_THAT(IsTrue(B.Ptr->FindFunction(Key(21)) == Sum));
		ASSERT_THAT(IsNull(Type->GetEngine()));
		FHostInjectionPair Pair{20, 22};
		ASSERT_THAT(AreEqual(42, CallSum(*Sum, Pair)));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Type->GetDefinitions()) == asERegistrationResult::AlreadyRegistered));
		ASSERT_THAT(IsTrue(B.Ptr->GetRegistrationCounts() == BBefore));
		ASSERT_THAT(AreEqual(1, CountRegistered(*B.Ptr, Type->GetDefinitions())));
		B.Reset();
		ASSERT_THAT(AreEqual(1, Counters.Types.load()));
		ASSERT_THAT(AreEqual(1, Counters.Functions.load()));
		ASSERT_THAT(AreEqual(1, Counters.Namespaces.load()));
	}
};

#endif
