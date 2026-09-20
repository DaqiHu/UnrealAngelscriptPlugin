#include "CQTest.h"
#include "as_callfunc.h"
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
	std::atomic<int32> GLiveObjectsDestructs{0};

	struct FLiveObjectsPair
	{
		int32 X = 0;
		int32 Y = 0;
	};

	void LiveObjectsGenericConstruct(asIScriptGeneric* Generic)
	{
		new (Generic->GetObject()) FLiveObjectsPair();
	}

	void LiveObjectsGenericDestruct(asIScriptGeneric* Generic)
	{
		GLiveObjectsDestructs.fetch_add(1);
		static_cast<FLiveObjectsPair*>(Generic->GetObject())->~FLiveObjectsPair();
	}

	void LiveObjectsGenericSum(asIScriptGeneric* Generic)
	{
		const FLiveObjectsPair* Self = static_cast<const FLiveObjectsPair*>(Generic->GetObject());
		Generic->SetReturnDWord(static_cast<asDWORD>(Self->X + Self->Y));
	}

	int32 HostLivePairSum(FLiveObjectsPair* Self)
	{
		return Self->X + Self->Y;
	}
}

TEST_CLASS_WITH_FLAGS(LiveObjects, "Angelscript.UnitTest.NativeEngine.Registration",
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

	static int32 ExecuteMethod(asCScriptEngine& Engine, asIScriptFunction* Function, void* Object)
	{
		asIScriptContext* Context = Engine.CreateContext();
		if (!Context || !Function) return MIN_int32;
		const int Prepared = Context->Prepare(Function);
		if (Prepared != 0)
		{
			Context->Release();
			return Prepared == asINVALID_ARG ? asINVALID_ARG : MIN_int32;
		}
		if (Object && Context->SetObject(Object) != 0)
		{
			Context->Release();
			return MIN_int32;
		}
		const int32 Status = Context->Execute();
		const int32 Result = Status == asEXECUTION_FINISHED
			? static_cast<int32>(Context->GetReturnDWord())
			: MIN_int32;
		Context->Release();
		return Result;
	}

	static int32 ExecuteVoidMethod(asCScriptEngine& Engine, asIScriptFunction* Function, void* Object)
	{
		asIScriptContext* Context = Engine.CreateContext();
		if (!Context || !Function) return MIN_int32;
		const int Prepared = Context->Prepare(Function);
		if (Prepared != 0)
		{
			Context->Release();
			return Prepared;
		}
		if (Object && Context->SetObject(Object) != 0)
		{
			Context->Release();
			return MIN_int32;
		}
		const int32 Status = Context->Execute();
		Context->Release();
		return Status;
	}

	static bool RegisterPairSurface(asCScriptEngine& Engine)
	{
		if (Engine.RegisterObjectType("Pair", sizeof(FLiveObjectsPair), asOBJ_VALUE | asOBJ_APP_CLASS) < 0)
		{
			return false;
		}
		return Engine.RegisterObjectProperty("Pair", "int X", static_cast<int>(offsetof(FLiveObjectsPair, X))) >= 0
			&& Engine.RegisterObjectProperty("Pair", "int Y", static_cast<int>(offsetof(FLiveObjectsPair, Y))) >= 0
			&& Engine.RegisterObjectBehaviour("Pair", asBEHAVE_CONSTRUCT, "void f()",
				asFUNCTION(LiveObjectsGenericConstruct), asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectBehaviour("Pair", asBEHAVE_DESTRUCT, "void f()",
				asFUNCTION(LiveObjectsGenericDestruct), asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod("Pair", "int Sum() const",
				asFUNCTION(LiveObjectsGenericSum), asCALL_GENERIC) >= 0;
	}

	bool BuildHostPair(asCDefinitions& Graph, asCObjectType*& Type, asCScriptFunction*& Sum)
	{
		Type = nullptr;
		Sum = nullptr;
		asCObjectProperty* X = nullptr;
		asCObjectProperty* Y = nullptr;
		asSNativePropertyAddress XAddress;
		XAddress.Offset = static_cast<int>(offsetof(FLiveObjectsPair, X));
		asSNativePropertyAddress YAddress;
		YAddress.Offset = static_cast<int>(offsetof(FLiveObjectsPair, Y));
		return Success(Graph.CreateObjectType(Key(30), "Pair", "", asOBJ_VALUE | asOBJ_APP_CLASS, Type))
			&& Success(Graph.SetNativeLayout(*Type, sizeof(FLiveObjectsPair), alignof(FLiveObjectsPair)))
			&& Success(Graph.DefineNativeProperty(*Type, "X", Graph.Primitive(ttInt), XAddress, X))
			&& Success(Graph.DefineNativeProperty(*Type, "Y", Graph.Primitive(ttInt), YAddress, Y))
			&& Success(Graph.CreateFunction(Key(31), "Sum", "", asFUNC_SYSTEM, Sum))
			&& Success(Graph.DefineFunction(*Sum, Graph.Primitive(ttInt), {}, Type))
			&& Success(Graph.BindNativeFunction(*Sum, asFUNCTION(HostLivePairSum), asCALL_CDECL_OBJFIRST))
			&& Success(Graph.FinalizeLayouts())
			&& Success(Graph.Freeze());
	}

public:
	TEST_METHOD(NativePairRegistration)
	{
		FEngineOwner A;
		GLiveObjectsDestructs.store(0);
		ASSERT_THAT(IsTrue(RegisterPairSurface(*A.Ptr)));
		asCTypeInfo* Type = static_cast<asCTypeInfo*>(A.Ptr->GetTypeInfoByName("Pair"));
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Type->GetTypeInfoKind() == asETypeInfoKind::LiveRegister));
		ASSERT_THAT(IsTrue(Type->GetEngine() == A.Ptr));
		ASSERT_THAT(AreEqual(static_cast<int>(sizeof(FLiveObjectsPair)), Type->GetSize()));
		ASSERT_THAT(AreEqual(2u, Type->GetPropertyCount()));
		int XOffset = -1;
		int YOffset = -1;
		const char* XName = nullptr;
		const char* YName = nullptr;
		ASSERT_THAT(AreEqual(0, Type->GetProperty(0, &XName, nullptr, nullptr, nullptr, &XOffset, nullptr, nullptr, nullptr, nullptr)));
		ASSERT_THAT(AreEqual(0, Type->GetProperty(1, &YName, nullptr, nullptr, nullptr, &YOffset, nullptr, nullptr, nullptr, nullptr)));
		ASSERT_THAT(AreEqual(static_cast<int>(offsetof(FLiveObjectsPair, X)), XOffset));
		ASSERT_THAT(AreEqual(static_cast<int>(offsetof(FLiveObjectsPair, Y)), YOffset));
		asIScriptFunction* Sum = Type->GetMethodByName("Sum");
		asEBehaviours Behaviour = asBEHAVE_MAX;
		asIScriptFunction* Construct = nullptr;
		asIScriptFunction* Destruct = nullptr;
		for (asUINT Index = 0; Index < Type->GetBehaviourCount(); ++Index)
		{
			asEBehaviours Kind = asBEHAVE_MAX;
			asIScriptFunction* Function = Type->GetBehaviourByIndex(Index, &Kind);
			if (Kind == asBEHAVE_CONSTRUCT) Construct = Function;
			if (Kind == asBEHAVE_DESTRUCT) Destruct = Function;
			Behaviour = Kind;
		}
		(void)Behaviour;
		ASSERT_THAT(IsNotNull(Sum));
		ASSERT_THAT(IsNotNull(Construct));
		ASSERT_THAT(IsNotNull(Destruct));
		ASSERT_THAT(IsTrue(static_cast<asCScriptFunction*>(Sum)->GetTypeInfoKind() == asETypeInfoKind::LiveRegister));
		alignas(FLiveObjectsPair) uint8 Storage[sizeof(FLiveObjectsPair)] = {};
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), ExecuteVoidMethod(*A.Ptr, Construct, Storage)));
		auto* Pair = reinterpret_cast<FLiveObjectsPair*>(Storage);
		Pair->X = 20;
		Pair->Y = 22;
		ASSERT_THAT(AreEqual(42, ExecuteMethod(*A.Ptr, Sum, Pair)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), ExecuteVoidMethod(*A.Ptr, Destruct, Pair)));
		ASSERT_THAT(AreEqual(1, GLiveObjectsDestructs.load()));
	}

	TEST_METHOD(HostProtectionAndReverseConflict)
	{
		FEngineOwner A;
		FEngineOwner B;
		TUniquePtr<asCDefinitions> Producer = asCDefinitions::Create(HostOptions());
		asCObjectType* HostType = nullptr;
		asCScriptFunction* HostSum = nullptr;
		ASSERT_THAT(IsTrue(BuildHostPair(*Producer, HostType, HostSum)));
		asCDefinitionGraphLease Graph = asCDefinitions::AdoptGraph(MoveTemp(Producer));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Graph) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterObjectType("Pair", sizeof(FLiveObjectsPair), asOBJ_VALUE | asOBJ_APP_CLASS) < 0));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterObjectProperty("Pair", "int X", 0) < 0));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterObjectMethod("Pair", "int Sum() const",
			asFUNCTION(LiveObjectsGenericSum), asCALL_GENERIC) < 0));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterObjectBehaviour("Pair", asBEHAVE_CONSTRUCT, "void f()",
			asFUNCTION(LiveObjectsGenericConstruct), asCALL_GENERIC) < 0));
		ASSERT_THAT(IsTrue(B.Ptr->GetTypeInfoByName("Pair") == HostType));
		ASSERT_THAT(IsNull(HostType->GetEngine()));
		FLiveObjectsPair Pair{20, 22};
		const std::shared_ptr<asSSystemFunctionInterface> HostInterface = HostSum->GetProcessInterface();
		ASSERT_THAT(IsTrue(HostInterface && HostInterface->func));
		using FHostSum = int32(*)(FLiveObjectsPair*);
		ASSERT_THAT(AreEqual(42, reinterpret_cast<FHostSum>(HostInterface->func)(&Pair)));
		FEngineOwner LiveFirst;
		ASSERT_THAT(IsTrue(RegisterPairSurface(*LiveFirst.Ptr)));
		asCTypeInfo* LiveType = static_cast<asCTypeInfo*>(LiveFirst.Ptr->GetTypeInfoByName("Pair"));
		ASSERT_THAT(IsNotNull(LiveType));
		ASSERT_THAT(IsTrue(LiveType->GetTypeInfoKind() == asETypeInfoKind::LiveRegister));
		ASSERT_THAT(IsTrue(LiveFirst.Ptr->InjectDefinitions(*Graph) != asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(LiveFirst.Ptr->GetTypeInfoByName("Pair") == LiveType));
		alignas(FLiveObjectsPair) uint8 Storage[sizeof(FLiveObjectsPair)] = {};
		asIScriptFunction* Construct = nullptr;
		asIScriptFunction* Sum = LiveType->GetMethodByName("Sum");
		for (asUINT Index = 0; Index < LiveType->GetBehaviourCount(); ++Index)
		{
			asEBehaviours Kind = asBEHAVE_MAX;
			asIScriptFunction* Function = LiveType->GetBehaviourByIndex(Index, &Kind);
			if (Kind == asBEHAVE_CONSTRUCT) Construct = Function;
		}
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), ExecuteVoidMethod(*LiveFirst.Ptr, Construct, Storage)));
		auto* LivePair = reinterpret_cast<FLiveObjectsPair*>(Storage);
		LivePair->X = 20;
		LivePair->Y = 22;
		ASSERT_THAT(AreEqual(42, ExecuteMethod(*LiveFirst.Ptr, Sum, LivePair)));
	}

	TEST_METHOD(TwoPrivateOwnersAndInvalidLayout)
	{
		FEngineOwner A;
		FEngineOwner B;
		ASSERT_THAT(IsTrue(RegisterPairSurface(*A.Ptr)));
		ASSERT_THAT(IsTrue(RegisterPairSurface(*B.Ptr)));
		asCTypeInfo* TypeA = static_cast<asCTypeInfo*>(A.Ptr->GetTypeInfoByName("Pair"));
		asCTypeInfo* TypeB = static_cast<asCTypeInfo*>(B.Ptr->GetTypeInfoByName("Pair"));
		ASSERT_THAT(IsNotNull(TypeA));
		ASSERT_THAT(IsNotNull(TypeB));
		ASSERT_THAT(IsTrue(TypeA != TypeB));
		ASSERT_THAT(IsTrue(TypeA->GetTypeId() != TypeB->GetTypeId()));
		ASSERT_THAT(IsTrue(TypeA->GetEngine() == A.Ptr));
		ASSERT_THAT(IsTrue(TypeB->GetEngine() == B.Ptr));
		asIScriptFunction* SumA = TypeA->GetMethodByName("Sum");
		asIScriptFunction* SumB = TypeB->GetMethodByName("Sum");
		ASSERT_THAT(IsTrue(SumA != SumB));
		const asUINT PropertyCount = TypeA->GetPropertyCount();
		const asUINT MethodCount = TypeA->GetMethodCount();
		ASSERT_THAT(IsTrue(A.Ptr->RegisterObjectProperty("Pair", "int Z", 10000) < 0));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterObjectMethod("Pair", "int Nope(Unknown a)",
			asFUNCTION(LiveObjectsGenericSum), asCALL_GENERIC) < 0));
		ASSERT_THAT(AreEqual(PropertyCount, TypeA->GetPropertyCount()));
		ASSERT_THAT(AreEqual(MethodCount, TypeA->GetMethodCount()));
		FLiveObjectsPair Pair{20, 22};
		ASSERT_THAT(AreEqual(int32(asINVALID_ARG), ExecuteMethod(*A.Ptr, SumB, &Pair)));
		ASSERT_THAT(AreEqual(42, ExecuteMethod(*B.Ptr, SumB, &Pair)));
	}
};

#endif
