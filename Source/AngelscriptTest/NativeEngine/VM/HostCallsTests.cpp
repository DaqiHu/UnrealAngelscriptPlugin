#include "CQTest.h"
#include "as_callfunc.h"
#include "as_context.h"
#include "as_definitions.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

#include <atomic>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	std::atomic<int32> GHostCallsAddInvocations{0};

	void HostCallsGenericAdd(asIScriptGeneric* Generic)
	{
		GHostCallsAddInvocations.fetch_add(1);
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		Generic->SetReturnDWord(static_cast<asDWORD>(A + B));
	}

	struct FHostCallAuxiliary
	{
		int32 Value = 0;
		asIScriptContext* NestedContext = nullptr;
		asIScriptFunction* NestedFunction = nullptr;
		asCScriptEngine* RebindEngine = nullptr;
		asSStableKey RebindKey;
		FHostCallAuxiliary* Replacement = nullptr;
		void* AfterNested = nullptr;
		int32 NestedResult = -1;
		bool Rebound = false;
		asCScriptEngine* RetireEngine = nullptr;
		asIScriptFunction* DelegateFunction = nullptr;
		void* DelegateObject = nullptr;
		asCScriptFunction* CreatedDelegate = nullptr;
		bool LookedUpEngine = false;
	};

	void HostCallsGenericAuxiliary(asIScriptGeneric* Generic)
	{
		auto* State = static_cast<FHostCallAuxiliary*>(Generic->GetAuxiliary());
		if (!State)
		{
			Generic->SetReturnDWord(static_cast<asDWORD>(-1));
			return;
		}
		if (State->NestedContext && State->NestedFunction
			&& State->NestedContext->Prepare(State->NestedFunction) == 0
			&& State->NestedContext->Execute() == asEXECUTION_FINISHED)
		{
			State->NestedResult = static_cast<int32>(State->NestedContext->GetReturnDWord());
		}
		if (State->RebindEngine)
		{
			State->Rebound = State->RebindEngine->BindNativeFunction(
				State->RebindKey, asFUNCTION(HostCallsGenericAuxiliary), asCALL_GENERIC, nullptr, State->Replacement)
				== asEByteCodeLinkStatus::Succeeded;
		}
		if (State->DelegateFunction && State->DelegateObject)
		{
			State->CreatedDelegate = CreateDelegate(
				static_cast<asCScriptFunction*>(State->DelegateFunction), State->DelegateObject);
		}
		if (State->RetireEngine)
		{
			State->RetireEngine->RetireExternalDefinitions();
		}
		if (asIScriptFunction* Active = asGetActiveFunction())
		{
			(void)Active->GetEngine();
			State->LookedUpEngine = true;
		}
		State->AfterNested = Generic->GetAuxiliary();
		Generic->SetReturnDWord(static_cast<asDWORD>(State->Value));
	}

	struct FHostCallsPair
	{
		int32 X = 20;
		int32 Y = 22;
		int32 Sum() const { return X + Y; }
	};

	void HostCallsGenericPairSum(asIScriptGeneric* Generic)
	{
		const FHostCallsPair* Self = static_cast<const FHostCallsPair*>(Generic->GetObject());
		Generic->SetReturnDWord(static_cast<asDWORD>(Self->X + Self->Y));
	}
}

TEST_CLASS_WITH_FLAGS(HostCalls, "Angelscript.UnitTest.NativeEngine.VM",
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

	static int32 ExecuteAdd(asCScriptEngine& Engine, asCScriptFunction& Function, int32 A, int32 B)
	{
		asIScriptContext* Context = Engine.CreateContext();
		if (!Context) return MIN_int32;
		const int Prepared = Context->Prepare(&Function);
		if (Prepared != 0)
		{
			Context->Release();
			return Prepared == asINVALID_ARG ? asINVALID_ARG : MIN_int32;
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

	bool BuildAdd(asCDefinitions& Graph, asCScriptFunction*& Add, uint32 FunctionKey)
	{
		Add = nullptr;
		asSDefinitionParameter Left;
		Left.Name = "A";
		Left.Type = Graph.Primitive(ttInt);
		asSDefinitionParameter Right;
		Right.Name = "B";
		Right.Type = Graph.Primitive(ttInt);
		asSDefinitionParameter Params[2] = {Left, Right};
		return Success(Graph.CreateFunction(Key(FunctionKey), "Add", "", asFUNC_SYSTEM, Add))
			&& Success(Graph.DefineFunction(*Add, Graph.Primitive(ttInt), MakeArrayView(Params)))
			&& Success(Graph.BindNativeFunction(*Add, asFUNCTION(HostCallsGenericAdd), asCALL_GENERIC))
			&& Success(Graph.FinalizeLayouts())
			&& Success(Graph.Freeze());
	}

	bool BuildPairSum(asCDefinitions& Graph, asCObjectType*& Type, asCScriptFunction*& Sum)
	{
		Type = nullptr;
		Sum = nullptr;
		asCObjectProperty* X = nullptr;
		asCObjectProperty* Y = nullptr;
		asSNativePropertyAddress XAddress;
		XAddress.Offset = static_cast<int>(offsetof(FHostCallsPair, X));
		asSNativePropertyAddress YAddress;
		YAddress.Offset = static_cast<int>(offsetof(FHostCallsPair, Y));
		return Success(Graph.CreateObjectType(Key(30), "Pair", "", asOBJ_VALUE | asOBJ_APP_CLASS, Type))
			&& Success(Graph.SetNativeLayout(*Type, sizeof(FHostCallsPair), alignof(FHostCallsPair)))
			&& Success(Graph.DefineNativeProperty(*Type, "X", Graph.Primitive(ttInt), XAddress, X))
			&& Success(Graph.DefineNativeProperty(*Type, "Y", Graph.Primitive(ttInt), YAddress, Y))
			&& Success(Graph.CreateFunction(Key(31), "Sum", "", asFUNC_SYSTEM, Sum))
			&& Success(Graph.DefineFunction(*Sum, Graph.Primitive(ttInt), {}, Type))
			&& Success(Graph.BindNativeFunction(*Sum, asFUNCTION(HostCallsGenericPairSum), asCALL_GENERIC))
			&& Success(Graph.FinalizeLayouts())
			&& Success(Graph.Freeze());
	}

public:
	TEST_METHOD(AdmittedExecution)
	{
		FEngineOwner A;
		FEngineOwner B;
		FEngineOwner C;
		TUniquePtr<asCDefinitions> Producer = asCDefinitions::Create(HostOptions());
		asCScriptFunction* Add = nullptr;
		ASSERT_THAT(IsTrue(BuildAdd(*Producer, Add, 1)));
		asCDefinitionGraphLease Graph = asCDefinitions::AdoptGraph(MoveTemp(Producer));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		GHostCallsAddInvocations.store(0);
		ASSERT_THAT(AreEqual(42, ExecuteAdd(*A.Ptr, *Add, 20, 22)));
		ASSERT_THAT(AreEqual(42, ExecuteAdd(*B.Ptr, *Add, 20, 22)));
		const int32 BeforeC = GHostCallsAddInvocations.load();
		ASSERT_THAT(AreEqual(int32(asINVALID_ARG), ExecuteAdd(*C.Ptr, *Add, 20, 22)));
		ASSERT_THAT(AreEqual(BeforeC, GHostCallsAddInvocations.load()));
		ASSERT_THAT(IsNull(Add->GetEngine()));
	}

	TEST_METHOD(LocalOverrideAndReentry)
	{
		FEngineOwner A;
		FEngineOwner B;
		TUniquePtr<asCDefinitions> Producer = asCDefinitions::Create(HostOptions());
		asCScriptFunction* Add = nullptr;
		ASSERT_THAT(IsTrue(BuildAdd(*Producer, Add, 2)));
		asCDefinitionGraphLease Graph = asCDefinitions::AdoptGraph(MoveTemp(Producer));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		FHostCallAuxiliary AuxA;
		FHostCallAuxiliary AuxB;
		FHostCallAuxiliary Replacement;
		AuxA.Value = 42;
		AuxB.Value = 99;
		Replacement.Value = 7;
		AuxA.NestedContext = B.Ptr->CreateContext();
		AuxA.NestedFunction = Add;
		AuxA.RebindEngine = A.Ptr;
		AuxA.RebindKey = Add->GetStableKey();
		AuxA.Replacement = &Replacement;
		ASSERT_THAT(IsTrue(A.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(HostCallsGenericAuxiliary), asCALL_GENERIC, nullptr, &AuxA)
			== asEByteCodeLinkStatus::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(HostCallsGenericAuxiliary), asCALL_GENERIC, nullptr, &AuxB)
			== asEByteCodeLinkStatus::Succeeded));
		ASSERT_THAT(AreEqual(42, ExecuteAdd(*A.Ptr, *Add, 0, 0)));
		ASSERT_THAT(AreEqual(99, AuxA.NestedResult));
		ASSERT_THAT(AreEqual(99, ExecuteAdd(*B.Ptr, *Add, 0, 0)));
		ASSERT_THAT(IsTrue(AuxA.Rebound));
		ASSERT_THAT(IsTrue(AuxA.AfterNested == &AuxA));
		ASSERT_THAT(AreEqual(7, ExecuteAdd(*A.Ptr, *Add, 0, 0)));
		if (AuxA.NestedContext) AuxA.NestedContext->Release();
	}

	TEST_METHOD(RetireAndReachableDelegates)
	{
		FEngineOwner A;
		FEngineOwner B;
		TUniquePtr<asCDefinitions> Producer = asCDefinitions::Create(HostOptions());
		asCObjectType* Type = nullptr;
		asCScriptFunction* Sum = nullptr;
		ASSERT_THAT(IsTrue(BuildPairSum(*Producer, Type, Sum)));
		asCDefinitionGraphLease Graph = asCDefinitions::AdoptGraph(MoveTemp(Producer));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		asCScriptFunction* Add = nullptr;
		TUniquePtr<asCDefinitions> AddProducer = asCDefinitions::Create(HostOptions());
		ASSERT_THAT(IsTrue(BuildAdd(*AddProducer, Add, 40)));
		asCDefinitionGraphLease AddGraph = asCDefinitions::AdoptGraph(MoveTemp(AddProducer));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*AddGraph) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*AddGraph) == asERegistrationResult::Succeeded));
		FHostCallsPair Pair;
		FHostCallAuxiliary Aux;
		Aux.Value = 42;
		Aux.RetireEngine = A.Ptr;
		Aux.DelegateFunction = Sum;
		Aux.DelegateObject = &Pair;
		ASSERT_THAT(IsTrue(A.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(HostCallsGenericAuxiliary), asCALL_GENERIC, nullptr, &Aux)
			== asEByteCodeLinkStatus::Succeeded));
		ASSERT_THAT(AreEqual(42, ExecuteAdd(*A.Ptr, *Add, 0, 0)));
		ASSERT_THAT(IsTrue(Aux.LookedUpEngine));
		ASSERT_THAT(IsNull(Add->GetEngine()));
		ASSERT_THAT(IsNull(Sum->GetEngine()));
		ASSERT_THAT(IsTrue(Aux.CreatedDelegate != nullptr));
		ASSERT_THAT(IsTrue(Aux.CreatedDelegate->GetDelegateFunction() == Sum));
		Aux.CreatedDelegate->Release();
		ASSERT_THAT(AreEqual(42, ExecuteAdd(*B.Ptr, *Add, 20, 22)));
		TUniquePtr<asCDefinitions> Script = asCDefinitions::Create();
		asCScriptFunction* Foreign = nullptr;
		ASSERT_THAT(IsTrue(Success(Script->CreateFunction(Key(50), "Foreign", "", asFUNC_SCRIPT, Foreign))));
		ASSERT_THAT(IsTrue(Success(Script->DefineFunction(*Foreign, Script->Primitive(ttInt), {}))));
		ASSERT_THAT(IsTrue(Success(Script->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Script->Freeze())));
		asIScriptContext* Context = B.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(int(asINVALID_ARG), Context->Prepare(Foreign)));
		Context->Release();
	}
};

#endif
