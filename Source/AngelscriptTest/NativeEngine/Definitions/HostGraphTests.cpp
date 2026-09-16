#include "CQTest.h"
#include "as_callfunc.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptfunction.h"
#include "as_type_id_registry.h"
#include "as_typeinfo.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

struct FHostPair
{
	int32 X = 0;
	int32 Y = 0;
};

int32 HostPairSum(FHostPair* Self)
{
	return Self->X + Self->Y;
}

TEST_CLASS_WITH_FLAGS(HostGraph, "Angelscript.UnitTest.NativeEngine.Definitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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

	static asSDefinitionOptions HostOptions(asCTypeIdAllocator* Allocator)
	{
		asSDefinitionOptions Options;
		Options.Kind = asETypeInfoKind::HostProcess;
		Options.TypeIdAllocator = Allocator;
		return Options;
	}

	static bool Success(asEDefinitionResult Result)
	{
		return Result == asEDefinitionResult::Succeeded;
	}

	static int32 CallHostPairSum(const asCScriptFunction& Function, FHostPair& Pair)
	{
		const std::shared_ptr<asSSystemFunctionInterface> Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->func)
		{
			return MIN_int32;
		}
		using FSum = int32(*)(FHostPair*);
		const FSum Target = reinterpret_cast<FSum>(Interface->func);
		return Target(&Pair);
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
		XAddress.Mode = asENativeAddressMode::Direct;
		XAddress.Offset = static_cast<int>(offsetof(FHostPair, X));
		asSNativePropertyAddress YAddress;
		YAddress.Mode = asENativeAddressMode::Direct;
		YAddress.Offset = static_cast<int>(offsetof(FHostPair, Y));
		return Success(Graph.CreateObjectType(Key(TypeKey), "Pair", "", asOBJ_VALUE | asOBJ_APP_CLASS, Type))
			&& Success(Graph.SetNativeLayout(*Type, sizeof(FHostPair), alignof(FHostPair)))
			&& Success(Graph.DefineNativeProperty(*Type, "X", Graph.Primitive(ttInt), XAddress, X))
			&& Success(Graph.DefineNativeProperty(*Type, "Y", Graph.Primitive(ttInt), YAddress, Y))
			&& Success(Graph.CreateFunction(Key(FunctionKey), "Sum", "", asFUNC_SYSTEM, Sum))
			&& Success(Graph.DefineFunction(*Sum, Graph.Primitive(ttInt), {}, Type))
			&& Success(Graph.BindNativeFunction(*Sum, asFUNCTION(HostPairSum), asCALL_CDECL_OBJFIRST))
			&& Success(Graph.FinalizeLayouts())
			&& Success(Graph.Freeze());
	}

public:
	TEST_METHOD(FrozenSharedIdentity)
	{
		asCTypeIdAllocator Allocator;
		TUniquePtr<asCDefinitions> Graph = asCDefinitions::Create(HostOptions(&Allocator));
		asCObjectType* Type = nullptr;
		asCScriptFunction* Sum = nullptr;
		ASSERT_THAT(IsTrue(BuildHostPair(*Graph, Type, Sum, 1, 2)));
		ASSERT_THAT(IsTrue(Type->GetTypeInfoKind() == asETypeInfoKind::HostProcess));
		ASSERT_THAT(IsTrue(Sum->GetTypeInfoKind() == asETypeInfoKind::HostProcess));
		ASSERT_THAT(IsNull(Type->GetEngine()));
		ASSERT_THAT(IsNull(Sum->GetEngine()));
		ASSERT_THAT(IsTrue(Type->GetStableKey() == Key(1)));
		ASSERT_THAT(IsTrue(Sum->GetStableKey() == Key(2)));
		const int TypeId = Type->GetTypeId();
		const int FunctionId = Sum->GetId();
		ASSERT_THAT(IsTrue(TypeId > 0));
		ASSERT_THAT(IsTrue(FunctionId > 0));
		FHostPair Pair{20, 22};
		ASSERT_THAT(AreEqual(42, CallHostPairSum(*Sum, Pair)));
		ASSERT_THAT(IsTrue(Graph->BindNativeFunction(*Sum, asFUNCTION(HostPairSum), asCALL_CDECL_OBJFIRST)
			== asEDefinitionResult::InvalidState));
		ASSERT_THAT(AreEqual(TypeId, Type->GetTypeId()));
		ASSERT_THAT(AreEqual(FunctionId, Sum->GetId()));
		ASSERT_THAT(AreEqual(42, CallHostPairSum(*Sum, Pair)));
	}

	TEST_METHOD(RetainedRecursiveGraph)
	{
		asCTypeIdAllocator Allocator;
		TUniquePtr<asCDefinitions> Producer = asCDefinitions::Create(HostOptions(&Allocator));
		asCObjectType* Left = nullptr;
		asCObjectType* Right = nullptr;
		asCScriptFunction* Next = nullptr;
		asCObjectProperty* LeftOther = nullptr;
		asCObjectProperty* RightOther = nullptr;
		ASSERT_THAT(IsTrue(Success(Producer->CreateObjectType(Key(20), "Left", "Pair", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Left))));
		ASSERT_THAT(IsTrue(Success(Producer->CreateObjectType(Key(21), "Right", "Pair", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Right))));
		ASSERT_THAT(IsTrue(Success(Producer->AddProperty(*Left, "Other", asCDataType::CreateObjectHandle(Right, false), LeftOther))));
		ASSERT_THAT(IsTrue(Success(Producer->AddProperty(*Right, "Other", asCDataType::CreateObjectHandle(Left, false), RightOther))));
		ASSERT_THAT(IsTrue(Success(Producer->CreateFunction(Key(22), "Next", "Pair", asFUNC_SCRIPT, Next))));
		ASSERT_THAT(IsTrue(Success(Producer->DefineFunction(*Next, asCDataType::CreateObjectHandle(Right, false), {}, Left))));
		ASSERT_THAT(IsTrue(Success(Producer->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Producer->Freeze())));
		asSDefinitionDestroyCounters Counters;
		Producer->SetDestroyCounters(&Counters);
		asCDefinitionGraphLease First = asCDefinitions::AdoptGraph(MoveTemp(Producer));
		ASSERT_THAT(IsTrue(!Producer.IsValid()));
		ASSERT_THAT(IsTrue(static_cast<bool>(First)));
		asCDefinitionGraphLease Second = First->RetainGraph();
		First = asCDefinitionGraphLease();
		ASSERT_THAT(IsTrue(Left->GetTypeInfoKind() == asETypeInfoKind::HostProcess));
		ASSERT_THAT(IsTrue(Next->GetObjectType() == Left));
		ASSERT_THAT(IsTrue(Next->GetReturnDataType().GetTypeInfo() == Right));
		ASSERT_THAT(IsTrue(Left->GetMethodByName("Next") == Next));
		ASSERT_THAT(IsTrue(Left->properties[0]->type.GetTypeInfo() == Right));
		ASSERT_THAT(IsTrue(Right->properties[0]->type.GetTypeInfo() == Left));
		ASSERT_THAT(IsTrue(Left->GetTypeId() > 0));
		ASSERT_THAT(IsTrue(Next->GetId() > 0));
		Second = asCDefinitionGraphLease();
		ASSERT_THAT(AreEqual(2, Counters.Types.load()));
		ASSERT_THAT(AreEqual(1, Counters.Functions.load()));
		ASSERT_THAT(AreEqual(1, Counters.Namespaces.load()));
	}

	TEST_METHOD(InvalidFreezeDoesNotPublish)
	{
		asCTypeIdAllocator Isolated(asTYPEID_LAST_PRIMITIVE + 1, 1, 1);
		const int FirstTypeSequence = Isolated.PeekNextTypeSequence();
		const int FirstFunctionId = Isolated.PeekNextFunctionId();

		{
			TUniquePtr<asCDefinitions> Cycle = asCDefinitions::Create(HostOptions(&Isolated));
			asCObjectType* Left = nullptr;
			asCObjectType* Right = nullptr;
			asCObjectProperty* First = nullptr;
			asCObjectProperty* Second = nullptr;
			ASSERT_THAT(IsTrue(Success(Cycle->CreateObjectType(Key(30), "Left", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Left))));
			ASSERT_THAT(IsTrue(Success(Cycle->CreateObjectType(Key(31), "Right", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Right))));
			ASSERT_THAT(IsTrue(Success(Cycle->AddProperty(*Left, "Value", asCDataType::CreateType(Right, false), First))));
			ASSERT_THAT(IsTrue(Success(Cycle->AddProperty(*Right, "Value", asCDataType::CreateType(Left, false), Second))));
			ASSERT_THAT(IsTrue(Cycle->FinalizeLayouts() == asEDefinitionResult::RecursiveValueType));
			ASSERT_THAT(IsTrue(Cycle->Freeze() != asEDefinitionResult::Succeeded));
			ASSERT_THAT(IsFalse(Cycle->IsFrozen()));
			ASSERT_THAT(IsTrue(Left->GetTypeId() < 0));
			ASSERT_THAT(IsTrue(Right->GetTypeId() < 0));
		}
		ASSERT_THAT(AreEqual(FirstTypeSequence, Isolated.PeekNextTypeSequence()));
		ASSERT_THAT(AreEqual(FirstFunctionId, Isolated.PeekNextFunctionId()));

		{
			TUniquePtr<asCDefinitions> Incomplete = asCDefinitions::Create(HostOptions(&Isolated));
			asCObjectType* Type = nullptr;
			ASSERT_THAT(IsTrue(Success(Incomplete->CreateObjectType(Key(40), "Pair", "", asOBJ_VALUE | asOBJ_APP_CLASS, Type))));
			ASSERT_THAT(IsTrue(Incomplete->FinalizeLayouts() == asEDefinitionResult::IncompleteDefinition));
			ASSERT_THAT(IsTrue(Incomplete->Freeze() != asEDefinitionResult::Succeeded));
			ASSERT_THAT(IsFalse(Incomplete->IsFrozen()));
			ASSERT_THAT(IsTrue(Type->GetTypeId() < 0));
		}
		ASSERT_THAT(AreEqual(FirstTypeSequence, Isolated.PeekNextTypeSequence()));
		ASSERT_THAT(AreEqual(FirstFunctionId, Isolated.PeekNextFunctionId()));

		asCObjectType* LiveType = nullptr;
		asCScriptFunction* LiveSum = nullptr;
		TUniquePtr<asCDefinitions> Live = asCDefinitions::Create(HostOptions(&Isolated));
		ASSERT_THAT(IsTrue(BuildHostPair(*Live, LiveType, LiveSum, 50, 51)));
		const int LiveTypeId = LiveType->GetTypeId();
		const int LiveFunctionId = LiveSum->GetId();
		ASSERT_THAT(IsTrue(LiveTypeId > 0));
		ASSERT_THAT(IsTrue(LiveFunctionId > 0));
		FHostPair Pair{20, 22};
		ASSERT_THAT(AreEqual(42, CallHostPairSum(*LiveSum, Pair)));

		TUniquePtr<asCDefinitions> Exhausted = asCDefinitions::Create(HostOptions(&Isolated));
		asCObjectType* LateType = nullptr;
		asCScriptFunction* LateSum = nullptr;
		ASSERT_THAT(IsTrue(Success(Exhausted->CreateObjectType(Key(60), "Pair", "", asOBJ_VALUE | asOBJ_APP_CLASS, LateType))));
		ASSERT_THAT(IsTrue(Success(Exhausted->SetNativeLayout(*LateType, sizeof(FHostPair), alignof(FHostPair)))));
		asCObjectProperty* X = nullptr;
		asSNativePropertyAddress Address;
		Address.Mode = asENativeAddressMode::Direct;
		Address.Offset = static_cast<int>(offsetof(FHostPair, X));
		ASSERT_THAT(IsTrue(Success(Exhausted->DefineNativeProperty(*LateType, "X", Exhausted->Primitive(ttInt), Address, X))));
		ASSERT_THAT(IsTrue(Success(Exhausted->CreateFunction(Key(61), "Sum", "", asFUNC_SYSTEM, LateSum))));
		ASSERT_THAT(IsTrue(Success(Exhausted->DefineFunction(*LateSum, Exhausted->Primitive(ttInt), {}, LateType))));
		ASSERT_THAT(IsTrue(Success(Exhausted->BindNativeFunction(*LateSum, asFUNCTION(HostPairSum), asCALL_CDECL_OBJFIRST))));
		ASSERT_THAT(IsTrue(Success(Exhausted->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Exhausted->Freeze() != asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsFalse(Exhausted->IsFrozen()));
		ASSERT_THAT(IsTrue(LateType->GetTypeId() < 0));
		ASSERT_THAT(IsTrue(LateSum->GetId() <= 0));
		ASSERT_THAT(AreEqual(LiveTypeId, LiveType->GetTypeId()));
		ASSERT_THAT(AreEqual(LiveFunctionId, LiveSum->GetId()));
		ASSERT_THAT(AreEqual(42, CallHostPairSum(*LiveSum, Pair)));
	}
};

#endif
