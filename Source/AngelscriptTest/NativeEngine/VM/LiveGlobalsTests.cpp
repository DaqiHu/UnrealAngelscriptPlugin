#include "CQTest.h"
#include "as_callfunc.h"
#include "as_definitions.h"
#include "as_generic.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	void LiveGlobalsGenericAdd(asIScriptGeneric* Generic)
	{
		if (void* Auxiliary = Generic->GetAuxiliary())
		{
			Generic->SetReturnDWord(*static_cast<asDWORD*>(Auxiliary));
			return;
		}
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		Generic->SetReturnDWord(static_cast<asDWORD>(A + B));
	}

	int32 LiveGlobalsTypedAdd(int32 A, int32 B)
	{
		return A + B;
	}

	void LiveGlobalsCallTypedAdd(asFUNCTION_t Method, void** Parameters, void* ReturnValue)
	{
		using FAdd = int32(*)(int32, int32);
		*static_cast<int32*>(ReturnValue) = reinterpret_cast<FAdd>(Method)(
			*static_cast<int32*>(Parameters[0]),
			*static_cast<int32*>(Parameters[1]));
	}
}

TEST_CLASS_WITH_FLAGS(LiveGlobals, "Angelscript.UnitTest.NativeEngine.VM",
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

	static int32 ExecuteAdd(asCScriptEngine& Engine, asIScriptFunction* Function, int32 A, int32 B)
	{
		asIScriptContext* Context = Engine.CreateContext();
		if (!Context || !Function) return MIN_int32;
		if (Context->Prepare(Function) != 0)
		{
			Context->Release();
			return MIN_int32;
		}
		if (Context->SetArgDWord(0, static_cast<asDWORD>(A)) != 0
			|| Context->SetArgDWord(1, static_cast<asDWORD>(B)) != 0)
		{
			Context->Release();
			return MIN_int32;
		}
		const int32 Result = Context->Execute() == asEXECUTION_FINISHED
			? static_cast<int32>(Context->GetReturnDWord())
			: MIN_int32;
		Context->Release();
		return Result;
	}

	static asIScriptFunction* FunctionById(asCScriptEngine& Engine, int Id)
	{
		return Id >= 0 ? Engine.GetFunctionById(Id) : nullptr;
	}

public:
	TEST_METHOD(GlobalCallAndStorage)
	{
		FEngineOwner A;
		int32 Counter = 7;
		const int AddId = A.Ptr->RegisterGlobalFunction("int Add(int, int)",
			asFUNCTION(LiveGlobalsGenericAdd), asCALL_GENERIC);
		ASSERT_THAT(IsTrue(AddId >= 0));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterGlobalProperty("int Counter", &Counter) >= 0));
		asIScriptFunction* Add = FunctionById(*A.Ptr, AddId);
		ASSERT_THAT(IsNotNull(Add));
		ASSERT_THAT(AreEqual(42, ExecuteAdd(*A.Ptr, Add, 20, 22)));
		void* Bound = nullptr;
		ASSERT_THAT(AreEqual(0, A.Ptr->GetGlobalPropertyByIndex(0, nullptr, nullptr, nullptr, nullptr, nullptr, &Bound, nullptr)));
		ASSERT_THAT(IsTrue(Bound == &Counter));
		*static_cast<int32*>(Bound) = 9;
		ASSERT_THAT(AreEqual(9, Counter));
	}

	TEST_METHOD(PerOwnerGlobals)
	{
		FEngineOwner A;
		FEngineOwner B;
		asDWORD AuxA = 42;
		asDWORD AuxB = 99;
		int32 CounterA = 7;
		int32 CounterB = 3;
		const int AddA = A.Ptr->RegisterGlobalFunction("int Add(int, int)",
			asFUNCTION(LiveGlobalsGenericAdd), asCALL_GENERIC, nullptr, &AuxA);
		const int AddB = B.Ptr->RegisterGlobalFunction("int Add(int, int)",
			asFUNCTION(LiveGlobalsGenericAdd), asCALL_GENERIC, nullptr, &AuxB);
		ASSERT_THAT(IsTrue(AddA >= 0 && AddB >= 0));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterGlobalProperty("int Counter", &CounterA) >= 0));
		ASSERT_THAT(IsTrue(B.Ptr->RegisterGlobalProperty("int Counter", &CounterB) >= 0));
		ASSERT_THAT(AreEqual(42, ExecuteAdd(*A.Ptr, FunctionById(*A.Ptr, AddA), 0, 0)));
		ASSERT_THAT(AreEqual(99, ExecuteAdd(*B.Ptr, FunctionById(*B.Ptr, AddB), 0, 0)));
		void* BoundB = nullptr;
		ASSERT_THAT(AreEqual(0, B.Ptr->GetGlobalPropertyByIndex(0, nullptr, nullptr, nullptr, nullptr, nullptr, &BoundB, nullptr)));
		A.Reset();
		ASSERT_THAT(IsTrue(BoundB == &CounterB));
		*static_cast<int32*>(BoundB) = 11;
		ASSERT_THAT(AreEqual(11, CounterB));
		ASSERT_THAT(AreEqual(99, ExecuteAdd(*B.Ptr, FunctionById(*B.Ptr, AddB), 1, 2)));
	}

	TEST_METHOD(InvalidAndConflictingGlobals)
	{
		FEngineOwner A;
		FEngineOwner B;
		TUniquePtr<asCDefinitions> Producer = asCDefinitions::Create([]
		{
			asSDefinitionOptions Options;
			Options.Kind = asETypeInfoKind::HostProcess;
			return Options;
		}());
		asCGlobalProperty* HostCounter = nullptr;
		ASSERT_THAT(IsTrue(Producer->CreateNativeGlobalProperty(Key(80), Key(80), "Counter", "",
			Producer->Primitive(ttInt), HostCounter) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Producer->FinalizeLayouts() == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Producer->Freeze() == asEDefinitionResult::Succeeded));
		asCDefinitionGraphLease Graph = asCDefinitions::AdoptGraph(MoveTemp(Producer));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		const asUINT Functions = A.Ptr->GetGlobalFunctionCount();
		const asUINT Properties = A.Ptr->GetGlobalPropertyCount();
		int32 DummyCounter = 0;
		ASSERT_THAT(IsTrue(A.Ptr->RegisterGlobalFunction("int Add(Missing, int)",
			asFUNCTION(LiveGlobalsGenericAdd), asCALL_GENERIC) < 0));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterGlobalFunction("int Add(int, int)",
			asFUNCTION(LiveGlobalsGenericAdd), asCALL_CDECL) < 0));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterGlobalProperty("int Counter", &DummyCounter) < 0));
		ASSERT_THAT(AreEqual(Functions, A.Ptr->GetGlobalFunctionCount()));
		ASSERT_THAT(AreEqual(Properties, A.Ptr->GetGlobalPropertyCount()));
		const int GenericId = B.Ptr->RegisterGlobalFunction("int Add(int, int)",
			asFUNCTION(LiveGlobalsGenericAdd), asCALL_GENERIC);
		const int TypedId = B.Ptr->RegisterGlobalFunction("int TypedAdd(int, int)",
			asFUNCTION(LiveGlobalsTypedAdd), asCALL_CDECL, asFunctionCaller(LiveGlobalsCallTypedAdd));
		ASSERT_THAT(IsTrue(GenericId >= 0 && TypedId >= 0));
		ASSERT_THAT(AreEqual(42, ExecuteAdd(*B.Ptr, FunctionById(*B.Ptr, GenericId), 20, 22)));
		ASSERT_THAT(AreEqual(42, ExecuteAdd(*B.Ptr, FunctionById(*B.Ptr, TypedId), 20, 22)));
	}
};

#endif
