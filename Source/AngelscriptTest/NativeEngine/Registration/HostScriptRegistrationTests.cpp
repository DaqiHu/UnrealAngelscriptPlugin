#include "CQTest.h"
#include "as_callfunc.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"
#include "frontend/AST/as_type_context.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

struct FHostScriptPair
{
	int32 X = 0;
	int32 Y = 0;
};

int32 HostScriptPairSum(FHostScriptPair* Self)
{
	return Self->X + Self->Y;
}

TEST_CLASS_WITH_FLAGS(HostScriptRegistration, "Angelscript.UnitTest.NativeEngine.Registration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
		void Reset() { if (Ptr) Ptr->ShutDownAndRelease(); Ptr = nullptr; }
	};

	static AS_NAMESPACE_QUALIFIER asSStableKey RawKey(uint32 Value)
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

	static int32 CallSum(const asCScriptFunction& Function, FHostScriptPair& Pair)
	{
		const std::shared_ptr<asSSystemFunctionInterface> Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->func)
		{
			return MIN_int32;
		}
		using FSum = int32(*)(FHostScriptPair*);
		return reinterpret_cast<FSum>(Interface->func)(&Pair);
	}

	bool BuildHostPair(asCDefinitions& Graph, asCObjectType*& Type, asCScriptFunction*& Sum, uint32 TypeKey, uint32 FunctionKey)
	{
		Type = nullptr;
		Sum = nullptr;
		asCObjectProperty* X = nullptr;
		asCObjectProperty* Y = nullptr;
		asSNativePropertyAddress XAddress;
		XAddress.Offset = static_cast<int>(offsetof(FHostScriptPair, X));
		asSNativePropertyAddress YAddress;
		YAddress.Offset = static_cast<int>(offsetof(FHostScriptPair, Y));
		return Success(Graph.CreateObjectType(RawKey(TypeKey), "Pair", "", asOBJ_VALUE | asOBJ_APP_CLASS, Type))
			&& Success(Graph.SetNativeLayout(*Type, sizeof(FHostScriptPair), alignof(FHostScriptPair)))
			&& Success(Graph.DefineNativeProperty(*Type, "X", Graph.Primitive(ttInt), XAddress, X))
			&& Success(Graph.DefineNativeProperty(*Type, "Y", Graph.Primitive(ttInt), YAddress, Y))
			&& Success(Graph.CreateFunction(RawKey(FunctionKey), "Sum", "", asFUNC_SYSTEM, Sum))
			&& Success(Graph.DefineFunction(*Sum, Graph.Primitive(ttInt), {}, Type))
			&& Success(Graph.BindNativeFunction(*Sum, asFUNCTION(HostScriptPairSum), asCALL_CDECL_OBJFIRST))
			&& Success(Graph.FinalizeLayouts())
			&& Success(Graph.Freeze());
	}

	bool BuildBox(
		asCDefinitions& Graph,
		asCDefinitions& Host,
		asCObjectType& Pair,
		asCObjectType*& Box,
		const TCHAR* Scope)
	{
		Box = nullptr;
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		if (!Success(Graph.SetIdentityContext(Context))) return false;
		if (!Success(Graph.AddDependency(&Host))) return false;
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = Scope;
		Descriptor.Name = TEXT("Box");
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct;
		AS_NAMESPACE_QUALIFIER asSStableKey BoxKey;
		if (Context->GetIdentityRegistry().InternNominalType(Descriptor, BoxKey) != AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return false;
		}
		asCObjectProperty* Value = nullptr;
		return Success(Graph.CreateObjectType(BoxKey, "Box", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Box))
			&& Success(Graph.AddProperty(*Box, "Value", asCDataType::CreateType(&Pair, false), Value))
			&& Success(Graph.FinalizeLayouts())
			&& Success(Graph.Freeze());
	}

public:
	TEST_METHOD(MixedClosure)
	{
		FEngineOwner A;
		FEngineOwner B;
		TUniquePtr<asCDefinitions> HostProducer = asCDefinitions::Create(HostOptions());
		asCObjectType* Pair = nullptr;
		asCScriptFunction* Sum = nullptr;
		ASSERT_THAT(IsTrue(BuildHostPair(*HostProducer, Pair, Sum, 1, 2)));
		asCDefinitionGraphLease Host = asCDefinitions::AdoptGraph(MoveTemp(HostProducer));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Host) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Host) == asERegistrationResult::Succeeded));
		TUniquePtr<asCDefinitions> BoxSet = asCDefinitions::Create();
		asCObjectType* Box = nullptr;
		ASSERT_THAT(IsTrue(BuildBox(*BoxSet, *Host, *Pair, Box, TEXT("HostScript.Mixed"))));
		const asCObjectType* OriginalPair = Pair;
		ASSERT_THAT(IsTrue(A.Ptr->RegisterExternalDefinitions(MoveTemp(BoxSet)) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(A.Ptr->FindType("", "Box") == Box));
		ASSERT_THAT(IsTrue(Box->GetEngine() == A.Ptr));
		ASSERT_THAT(IsTrue(Box->GetTypeInfoKind() == asETypeInfoKind::ScriptEngine));
		ASSERT_THAT(IsTrue(A.Ptr->FindType("", "Pair") == OriginalPair));
		ASSERT_THAT(IsTrue(B.Ptr->FindType("", "Pair") == OriginalPair));
		ASSERT_THAT(IsNull(OriginalPair->GetEngine()));
		ASSERT_THAT(IsNull(Sum->GetEngine()));
		ASSERT_THAT(IsTrue(Box->properties[0]->type.GetTypeInfo() == OriginalPair));
		ASSERT_THAT(IsNull(B.Ptr->FindType("", "Box")));
		FHostScriptPair Native{20, 22};
		ASSERT_THAT(AreEqual(42, CallSum(*Sum, Native)));
	}

	TEST_METHOD(MissingDependency)
	{
		FEngineOwner C;
		TUniquePtr<asCDefinitions> HostProducer = asCDefinitions::Create(HostOptions());
		asCObjectType* Pair = nullptr;
		asCScriptFunction* Sum = nullptr;
		ASSERT_THAT(IsTrue(BuildHostPair(*HostProducer, Pair, Sum, 10, 11)));
		asCDefinitionGraphLease Host = asCDefinitions::AdoptGraph(MoveTemp(HostProducer));
		TUniquePtr<asCDefinitions> FirstBox = asCDefinitions::Create();
		asCObjectType* Box = nullptr;
		ASSERT_THAT(IsTrue(BuildBox(*FirstBox, *Host, *Pair, Box, TEXT("HostScript.MissingA"))));
		const auto Before = C.Ptr->GetRegistrationCounts();
		ASSERT_THAT(IsTrue(C.Ptr->RegisterExternalDefinitions(MoveTemp(FirstBox)) != asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(C.Ptr->GetRegistrationCounts() == Before));
		ASSERT_THAT(IsNull(C.Ptr->FindType("", "Box")));
		ASSERT_THAT(IsNull(C.Ptr->FindType("", "Pair")));
		ASSERT_THAT(IsNull(C.Ptr->FindFunction(RawKey(11))));
		ASSERT_THAT(IsTrue(C.Ptr->InjectDefinitions(*Host) == asERegistrationResult::Succeeded));
		TUniquePtr<asCDefinitions> SecondBox = asCDefinitions::Create();
		asCObjectType* LateBox = nullptr;
		ASSERT_THAT(IsTrue(BuildBox(*SecondBox, *Host, *Pair, LateBox, TEXT("HostScript.MissingB"))));
		ASSERT_THAT(IsTrue(C.Ptr->RegisterExternalDefinitions(MoveTemp(SecondBox)) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(C.Ptr->FindType("", "Box") == LateBox));
		ASSERT_THAT(IsTrue(LateBox->GetEngine() == C.Ptr));
		ASSERT_THAT(IsTrue(C.Ptr->FindType("", "Pair") == Pair));
		ASSERT_THAT(IsNull(Pair->GetEngine()));
		FHostScriptPair Native{20, 22};
		ASSERT_THAT(AreEqual(42, CallSum(*Sum, Native)));
	}

	TEST_METHOD(PrivateOwnerAndRetirement)
	{
		FEngineOwner A;
		FEngineOwner B;
		TUniquePtr<asCDefinitions> HostProducer = asCDefinitions::Create(HostOptions());
		asCObjectType* Pair = nullptr;
		asCScriptFunction* Sum = nullptr;
		ASSERT_THAT(IsTrue(BuildHostPair(*HostProducer, Pair, Sum, 20, 21)));
		asCDefinitionGraphLease Host = asCDefinitions::AdoptGraph(MoveTemp(HostProducer));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Host) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Host) == asERegistrationResult::Succeeded));
		TUniquePtr<asCDefinitions> BoxSet = asCDefinitions::Create();
		asCObjectType* Box = nullptr;
		ASSERT_THAT(IsTrue(BuildBox(*BoxSet, *Host, *Pair, Box, TEXT("HostScript.Private"))));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterExternalDefinitions(MoveTemp(BoxSet)) == asERegistrationResult::Succeeded));
		asCDefinitions* PrivateGraph = Box->GetDefinitions();
		ASSERT_THAT(IsNotNull(PrivateGraph));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*PrivateGraph) == asERegistrationResult::InvalidArgument));
		TUniquePtr<asCDefinitions> Empty;
		ASSERT_THAT(IsTrue(B.Ptr->RegisterExternalDefinitions(MoveTemp(Empty)) == asERegistrationResult::InvalidArgument));
		ASSERT_THAT(IsNull(B.Ptr->FindType("", "Box")));
		ASSERT_THAT(IsTrue(A.Ptr->FindType("", "Box") == Box));
		A.Ptr->RetireExternalDefinitions();
		ASSERT_THAT(IsNull(A.Ptr->FindType("", "Box")));
		ASSERT_THAT(IsTrue(B.Ptr->FindType("", "Pair") == Pair));
		ASSERT_THAT(IsTrue(Pair->GetTypeInfoKind() == asETypeInfoKind::HostProcess));
		ASSERT_THAT(IsTrue(Pair->GetDefinitions()->GetState() == asEDefinitionState::Frozen));
		ASSERT_THAT(IsNull(Pair->GetEngine()));
		FHostScriptPair Native{20, 22};
		ASSERT_THAT(AreEqual(42, CallSum(*Sum, Native)));
	}
};

#endif
