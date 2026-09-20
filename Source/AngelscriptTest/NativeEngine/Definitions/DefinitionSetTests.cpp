#include "CQTest.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

#include <atomic>
#include <thread>
#include <type_traits>
#include <vector>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

static_assert(!std::is_constructible_v<asCObjectType, asCDefinitions&, const AS_NAMESPACE_QUALIFIER asSStableKey&>);
static_assert(!std::is_constructible_v<asCScriptFunction, asCDefinitions&, const AS_NAMESPACE_QUALIFIER asSStableKey&, asEFuncType>);
static_assert(!std::is_constructible_v<asCEnumType, asCDefinitions&, const AS_NAMESPACE_QUALIFIER asSStableKey&>);
static_assert(!std::is_constructible_v<asCTypedefType, asCDefinitions&, const AS_NAMESPACE_QUALIFIER asSStableKey&>);

TEST_CLASS_WITH_FLAGS(DefinitionSet, "Angelscript.UnitTest.NativeEngine.Definitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static AS_NAMESPACE_QUALIFIER asSStableKey Key(uint32 Value)
	{
		uint8 Bytes[32] = {};
		for (int Index = 0; Index != 4; ++Index) Bytes[Index] = static_cast<uint8>(Value >> (Index * 8));
		AS_NAMESPACE_QUALIFIER asSStableKey Result;
		check(AS_NAMESPACE_QUALIFIER asSStableKey::TryCreate(MakeArrayView(Bytes), Result));
		return Result;
	}

	static bool Success(asEDefinitionResult Result) { return Result == asEDefinitionResult::Succeeded; }

public:
	TEST_METHOD(ActualTypesAndMethodsNeedNoEngineOrNumericIds)
	{
		TUniquePtr<asCDefinitions> Set = MakeUnique<asCDefinitions>();
		asCObjectType* Type = nullptr;
		asCScriptFunction* Method = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(1), "Counter", "Gameplay", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Set->CreateFunction(Key(2), "Read", "Gameplay", asFUNC_SCRIPT, Method))));
		ASSERT_THAT(IsTrue(Success(Set->DefineFunction(*Method, Set->Primitive(ttInt), {}, Type))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
		ASSERT_THAT(IsTrue(Type->GetDefinitions() == Set.Get()));
		ASSERT_THAT(IsTrue(Method->GetDefinitions() == Set.Get()));
		ASSERT_THAT(IsTrue(Type->GetMethodByIndex(0) == Method));
		ASSERT_THAT(IsTrue(Type->GetMethodByName("Read") == Method));
		ASSERT_THAT(IsTrue(Method->GetObjectType() == Type));
		ASSERT_THAT(IsTrue(Set->FindType(Key(1)) == Type));
		ASSERT_THAT(IsTrue(Set->FindFunction(Key(2)) == Method));
		ASSERT_THAT(IsTrue(Type->GetStableKey() == Key(1)));
		ASSERT_THAT(IsTrue(Method->GetStableKey() == Key(2)));
		ASSERT_THAT(IsNull(Type->GetEngine()));
		ASSERT_THAT(IsNull(Method->GetEngine()));
		ASSERT_THAT(IsTrue(Type->GetTypeId() < 0));
		ASSERT_THAT(IsTrue(Method->GetId() <= 0));
		ASSERT_THAT(IsTrue(Method->GetTypeId() < 0));
		ASSERT_THAT(IsNull(Type->GetConfigGroup()));
		ASSERT_THAT(IsNull(Method->GetConfigGroup()));
	}

	TEST_METHOD(MethodLeaseRetainsOwnerSignatureAndNamespace)
	{
		auto Set = asCDefinitions::Create();
		asCObjectType* Type = nullptr;
		asCScriptFunction* Method = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(10), "Node", "Graph", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Set->CreateFunction(Key(11), "Next", "Graph", asFUNC_SCRIPT, Method))));
		ASSERT_THAT(IsTrue(Success(Set->DefineFunction(*Method, asCDataType::CreateObjectHandle(Type, false), {}, Type))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
		ASSERT_THAT(IsTrue(Method->GetObjectType() == Type));
		ASSERT_THAT(IsTrue(Method->GetReturnDataType().GetTypeInfo() == Type));
		ASSERT_THAT(AreEqual(FString(TEXT("Graph")), FString(ANSI_TO_TCHAR(Type->GetNamespace()))));
		ASSERT_THAT(IsTrue(FString(ANSI_TO_TCHAR(Method->GetDeclaration())).Contains(TEXT("Node::Next"))));
		ASSERT_THAT(IsNotNull(Set.Get()));
	}

	TEST_METHOD(TypeLeaseRetainsRecursiveMembersUntilFinalRelease)
	{
		auto Set = asCDefinitions::Create();
		asCObjectType* Left = nullptr;
		asCObjectType* Right = nullptr;
		asCObjectProperty* Property = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(20), "Left", "Pair", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Left))));
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(21), "Right", "Pair", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Right))));
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Left, "Other", asCDataType::CreateObjectHandle(Right, false), Property))));
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Right, "Other", asCDataType::CreateObjectHandle(Left, false), Property))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
		ASSERT_THAT(IsTrue(Left->properties[0]->type.GetTypeInfo() == Right));
		ASSERT_THAT(IsTrue(Right->properties[0]->type.GetTypeInfo() == Left));
		ASSERT_THAT(IsNotNull(Set.Get()));
	}

	TEST_METHOD(ByValueRecursionFailsWithoutPartialLayouts)
	{
		auto Set = asCDefinitions::Create();
		asCObjectType* Left = nullptr;
		asCObjectType* Right = nullptr;
		asCObjectProperty* First = nullptr;
		asCObjectProperty* Second = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(30), "Left", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Left))));
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(31), "Right", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Right))));
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Left, "Value", asCDataType::CreateType(Right, false), First))));
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Right, "Value", asCDataType::CreateType(Left, false), Second))));
		ASSERT_THAT(IsTrue(Set->FinalizeLayouts() == asEDefinitionResult::RecursiveValueType));
		ASSERT_THAT(AreEqual(-1, First->byteOffset));
		ASSERT_THAT(AreEqual(-1, Second->byteOffset));
		ASSERT_THAT(IsTrue(Set->Freeze() == asEDefinitionResult::IncompleteDefinition));
		ASSERT_THAT(IsFalse(Set->IsFrozen()));
	}

	TEST_METHOD(FreezeRequiresCompleteSignaturesAndFinalizedLayouts)
	{
		auto Set = asCDefinitions::Create();
		asCObjectType* Type = nullptr;
		asCScriptFunction* Function = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(40), "Value", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Set->CreateFunction(Key(41), "Run", "", asFUNC_SCRIPT, Function))));
		ASSERT_THAT(IsTrue(Set->Freeze() == asEDefinitionResult::IncompleteDefinition));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Set->Freeze() == asEDefinitionResult::IncompleteDefinition));
		ASSERT_THAT(IsTrue(Success(Set->DefineFunction(*Function, Set->Primitive(ttVoid), {}))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
	}

	TEST_METHOD(FrozenSetRejectsNewObjectsAndSemanticEdges)
	{
		auto Set = asCDefinitions::Create();
		asCObjectType* Type = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(50), "Frozen", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
		asCObjectProperty* Property = nullptr;
		asCObjectType* NewType = nullptr;
		ASSERT_THAT(IsTrue(Set->AddProperty(*Type, "Late", Set->Primitive(ttInt), Property) == asEDefinitionResult::InvalidState));
		ASSERT_THAT(IsTrue(Set->CreateObjectType(Key(51), "Late", "", asOBJ_VALUE, NewType) == asEDefinitionResult::InvalidState));
		ASSERT_THAT(IsNull(Property));
		ASSERT_THAT(IsNull(NewType));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Type->GetPropertyCount())));
		ASSERT_THAT(AreEqual(1, Set->GetTypes().Num()));
	}

	TEST_METHOD(LayoutUsesTargetPointerSizeAndAlignment)
	{
		asSDefinitionOptions Options;
		Options.PointerByteSize = 4;
		auto Set = asCDefinitions::Create(Options);
		asCObjectType* Type = nullptr;
		asCObjectProperty* Tag = nullptr;
		asCObjectProperty* Number = nullptr;
		asCObjectProperty* Next = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(60), "Layout", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Type, "Tag", Set->Primitive(ttUInt8), Tag))));
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Type, "Number", Set->Primitive(ttInt), Number))));
		asCDataType Reference = asCDataType::CreateType(Type, false);
		Reference.MakeReference(true);
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Type, "Next", Reference, Next))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(AreEqual(0, Tag->byteOffset));
		ASSERT_THAT(AreEqual(4, Number->byteOffset));
		ASSERT_THAT(AreEqual(8, Next->byteOffset));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Type->GetSize())));
		ASSERT_THAT(AreEqual(4, Type->alignment));
	}

	TEST_METHOD(EnumAliasAndFuncdefAreActualOwnedTypes)
	{
		auto Set = asCDefinitions::Create();
		asCEnumType* Enum = nullptr;
		asCTypedefType* Alias = nullptr;
		asCCallableType* CallableType = nullptr;
		asCScriptFunction* Signature = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateEnumType(Key(70), "Mode", "Api", Enum))));
		ASSERT_THAT(IsTrue(Success(Set->AddEnumValue(*Enum, "Enabled", 17))));
		ASSERT_THAT(IsTrue(Success(Set->CreateTypedefType(Key(71), "ModeAlias", "Api", Alias))));
		ASSERT_THAT(IsTrue(Success(Set->SetAlias(*Alias, asCDataType::CreateType(Enum, false)))));
		ASSERT_THAT(IsTrue(Success(Set->CreateFunction(Key(72), "Callback", "Api", asFUNC_CALLABLE_SIGNATURE, Signature))));
		asSDefinitionParameter Parameter;
		Parameter.Name = "Mode";
		Parameter.Type = asCDataType::CreateType(Alias, false);
		ASSERT_THAT(IsTrue(Success(Set->DefineFunction(*Signature, Set->Primitive(ttVoid), MakeArrayView(&Parameter, 1)))));
		ASSERT_THAT(IsTrue(Success(Set->CreateCallableType(Key(73), "Callback", "Api", *Signature, CallableType))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
		int Value = 0;
		ASSERT_THAT(AreEqual(FString(TEXT("Enabled")), FString(ANSI_TO_TCHAR(Enum->GetEnumValueByIndex(0, &Value)))));
		ASSERT_THAT(AreEqual(17, Value));
		ASSERT_THAT(IsTrue(CallableType->GetCallableSignature() == Signature));
		ASSERT_THAT(IsTrue(Signature->GetParameterDataType(0)->GetTypeInfo() == Alias));
		ASSERT_THAT(IsTrue(Alias->aliasForType.GetTypeInfo() == Enum));
		ASSERT_THAT(IsNotNull(Set.Get()));
	}

	TEST_METHOD(FrozenDependencyLifetimeAndForeignEdgesAreExplicit)
	{
		auto Dependency = asCDefinitions::Create();
		asCObjectType* External = nullptr;
		ASSERT_THAT(IsTrue(Success(Dependency->CreateObjectType(Key(80), "External", "Shared", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, External))));
		auto Set = asCDefinitions::Create();
		ASSERT_THAT(IsTrue(Set->AddDependency(Dependency.Get()) == asEDefinitionResult::UnfrozenDependency));
		ASSERT_THAT(IsTrue(Success(Dependency->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Dependency->Freeze())));
		asCObjectType* Local = nullptr;
		asCObjectProperty* Property = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(81), "Local", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Local))));
		ASSERT_THAT(IsTrue(Set->AddProperty(*Local, "External", asCDataType::CreateType(External, false), Property) == asEDefinitionResult::ForeignOwner));
		ASSERT_THAT(IsTrue(Success(Set->AddDependency(Dependency.Get()))));
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Local, "External", asCDataType::CreateType(External, false), Property))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
		ASSERT_THAT(IsTrue(Dependency->AddDependency(Set.Get()) == asEDefinitionResult::InvalidState));
		ASSERT_THAT(AreEqual(FString(TEXT("Shared")), FString(ANSI_TO_TCHAR(Property->type.GetTypeInfo()->GetNamespace()))));
		ASSERT_THAT(IsNotNull(Dependency.Get()));
		ASSERT_THAT(IsNotNull(Set.Get()));
	}

	TEST_METHOD(ImmutableOptionsDoNotFollowCallerOrOtherSetChanges)
	{
		asSDefinitionOptions Options;
		Options.FloatIsFloat64 = false;
		auto Float32Image = asCDefinitions::Create(Options);
		Options.FloatIsFloat64 = true;
		auto Float64Image = asCDefinitions::Create(Options);
		const asCDataType Float32 = Float32Image->Primitive(ttFloat32);
		const asCDataType Float64 = Float64Image->Primitive(ttFloat64);
		ASSERT_THAT(AreEqual(FString(TEXT("float")), FString(ANSI_TO_TCHAR(Float32.Format(nullptr).AddressOf()))));
		ASSERT_THAT(AreEqual(FString(TEXT("float")), FString(ANSI_TO_TCHAR(Float64.Format(nullptr).AddressOf()))));
		ASSERT_THAT(AreEqual(FString(TEXT("float32")), FString(ANSI_TO_TCHAR(Float64Image->Primitive(ttFloat32).Format(nullptr).AddressOf()))));
		ASSERT_THAT(IsFalse(Float32Image->GetOptions().FloatIsFloat64));
		ASSERT_THAT(AreEqual(FString(TEXT("float")), FString(ANSI_TO_TCHAR(Float32.Format(nullptr).AddressOf()))));
		ASSERT_THAT(IsTrue(Float32Image->Primitive(ttFloat).GetTokenType() == ttFloat32));
		ASSERT_THAT(IsTrue(Float64Image->Primitive(ttFloat).GetTokenType() == ttFloat64));
		ASSERT_THAT(AreEqual(4, Float32Image->Primitive(ttFloat).GetSizeInMemoryBytes()));
		ASSERT_THAT(AreEqual(8, Float64Image->Primitive(ttDouble).GetSizeInMemoryBytes()));
	}

	TEST_METHOD(SignatureQueriesRetainTypesNamesFlagsAndDefaults)
	{
		auto Set = asCDefinitions::Create();
		asCScriptFunction* Function = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateFunction(Key(90), "Read", "Api", asFUNC_SCRIPT, Function))));
		asSDefinitionParameter Parameter;
		Parameter.Name = "Count";
		Parameter.Type = Set->Primitive(ttInt);
		Parameter.Flags = asTM_NONE;
		Parameter.DefaultArgument = "7";
		ASSERT_THAT(IsTrue(Success(Set->DefineFunction(*Function, Set->Primitive(ttInt), MakeArrayView(&Parameter, 1)))));
		Parameter.Name = "ChangedCallerStorage";
		int TypeId = 999;
		const char* Name = nullptr;
		const char* Default = nullptr;
		asDWORD Flags = 999;
		ASSERT_THAT(AreEqual(asSUCCESS, Function->GetParam(0, &TypeId, &Flags, &Name, &Default)));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, TypeId));
		ASSERT_THAT(AreEqual(FString(TEXT("Count")), FString(ANSI_TO_TCHAR(Name))));
		ASSERT_THAT(AreEqual(FString(TEXT("7")), FString(ANSI_TO_TCHAR(Default))));
		ASSERT_THAT(AreEqual(asDWORD(asTM_NONE), Flags));
		ASSERT_THAT(IsTrue(Function->GetParameterDataType(0)->GetTokenType() == ttInt));
		ASSERT_THAT(IsNull(Function->GetParameterDataType(1)));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, Function->GetReturnTypeId()));
	}

	TEST_METHOD(DuplicateIdentityAndNameFailuresDoNotPublishObjects)
	{
		auto Set = asCDefinitions::Create();
		asCObjectType* Type = nullptr;
		asCObjectType* Other = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(100), "Name", "Scope", asOBJ_VALUE, Type))));
		ASSERT_THAT(IsTrue(Set->CreateObjectType(Key(100), "Other", "Scope", asOBJ_VALUE, Other) == asEDefinitionResult::DuplicateIdentity));
		ASSERT_THAT(IsTrue(Set->CreateObjectType(Key(101), "Name", "Scope", asOBJ_VALUE, Other) == asEDefinitionResult::DuplicateName));
		ASSERT_THAT(IsNull(Other));
		ASSERT_THAT(AreEqual(1, Set->GetTypes().Num()));
		asCScriptFunction* DelegateInstance = nullptr;
		ASSERT_THAT(IsTrue(Set->CreateFunction(Key(102), "LiveDelegate", "", asFUNC_DELEGATE, DelegateInstance) == asEDefinitionResult::InvalidArgument));
		ASSERT_THAT(IsNull(DelegateInstance));
	}

	TEST_METHOD(FactoryAndBehaviourQueriesUseObjectsBeforeRegistration)
	{
		auto Set = asCDefinitions::Create();
		asCObjectType* Type = nullptr;
		asCScriptFunction* Factory = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(110), "Thing", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Set->CreateFunction(Key(111), "MakeThing", "", asFUNC_SCRIPT, Factory))));
		ASSERT_THAT(IsTrue(Success(Set->DefineFunction(*Factory, asCDataType::CreateObjectHandle(Type, false), {}))));
		ASSERT_THAT(IsTrue(Success(Set->AddBehaviour(*Type, asBEHAVE_FACTORY, *Factory))));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Type->GetFactoryCount())));
		ASSERT_THAT(IsTrue(Type->GetFactoryByIndex(0) == Factory));
		asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
		ASSERT_THAT(IsTrue(Type->GetBehaviourByIndex(0, &Behaviour) == Factory));
		ASSERT_THAT(IsTrue(Behaviour == asBEHAVE_FACTORY));
		ASSERT_THAT(IsTrue(Factory->GetId() <= 0));
	}

	TEST_METHOD(InheritanceAndInterfacesRemainLocalDefinitionRelations)
	{
		auto Set = asCDefinitions::Create();
		asCObjectType* Base = nullptr;
		asCObjectType* Derived = nullptr;
		asCObjectType* Interface = nullptr;
		asCObjectProperty* Property = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(120), "Base", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Base))));
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(121), "Derived", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Derived))));
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(122), "Interface", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Interface))));
		ASSERT_THAT(IsTrue(Success(Set->SetInterfaceDeclaration(*Interface))));
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Base, "First", Set->Primitive(ttInt), Property))));
		ASSERT_THAT(IsTrue(Success(Set->SetBaseType(*Derived, *Base))));
		ASSERT_THAT(IsTrue(Success(Set->AddInterface(*Derived, *Interface))));
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Derived, "Second", Set->Primitive(ttInt), Property))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Derived->GetBaseType() == Base));
		ASSERT_THAT(IsTrue(Derived->DerivesFrom(Base)));
		ASSERT_THAT(IsTrue(Derived->Implements(Interface)));
		ASSERT_THAT(AreEqual(4, Property->byteOffset));
		ASSERT_THAT(AreEqual(8, static_cast<int32>(Derived->GetSize())));
	}

	TEST_METHOD(AliasCycleIsRejectedWithoutRecursingForever)
	{
		auto Set = asCDefinitions::Create();
		asCTypedefType* Left = nullptr;
		asCTypedefType* Right = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateTypedefType(Key(130), "Left", "", Left))));
		ASSERT_THAT(IsTrue(Success(Set->CreateTypedefType(Key(131), "Right", "", Right))));
		ASSERT_THAT(IsTrue(Success(Set->SetAlias(*Left, asCDataType::CreateType(Right, false)))));
		ASSERT_THAT(IsTrue(Success(Set->SetAlias(*Right, asCDataType::CreateType(Left, false)))));
		ASSERT_THAT(IsTrue(Set->FinalizeLayouts() == asEDefinitionResult::RecursiveValueType));
		ASSERT_THAT(IsFalse(Set->IsFrozen()));
	}

	TEST_METHOD(InvalidNativeLayoutDoesNotBecomeAValidFrozenType)
	{
		auto Set = asCDefinitions::Create();
		asCObjectType* Native = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(140), "Native", "", asOBJ_VALUE | asOBJ_POD, Native))));
		ASSERT_THAT(IsTrue(Set->SetNativeLayout(*Native, 12, 3) == asEDefinitionResult::InvalidLayout));
		ASSERT_THAT(IsTrue(Set->FinalizeLayouts() == asEDefinitionResult::IncompleteDefinition));
		ASSERT_THAT(IsTrue(Success(Set->SetNativeLayout(*Native, 16, 8))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
		ASSERT_THAT(AreEqual(16, static_cast<int32>(Native->GetSize())));
		ASSERT_THAT(AreEqual(8, Native->alignment));
	}

	TEST_METHOD(ConcurrentFactoriesSerializeOwnershipWithoutAnEngine)
	{
		auto Set = asCDefinitions::Create();
		std::atomic<int> Ready{0};
		std::atomic<bool> Start{false};
		std::atomic<bool> Failed{false};
		std::vector<std::thread> Workers;
		for (uint32 Index = 0; Index < 8; ++Index)
		{
			Workers.emplace_back([&, Index]
			{
				Ready.fetch_add(1);
				while (!Start.load(std::memory_order_acquire)) std::this_thread::yield();
				asCObjectType* Type = nullptr;
				asCString Name;
				Name.Format("Worker%u", Index);
				if (!Success(Set->CreateObjectType(Key(200 + Index), Name.AddressOf(), "Parallel", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))) Failed.store(true);
			});
		}
		while (Ready.load() != 8) std::this_thread::yield();
		Start.store(true, std::memory_order_release);
		for (std::thread& Worker : Workers) Worker.join();
		ASSERT_THAT(IsFalse(Failed.load()));
		ASSERT_THAT(AreEqual(8, Set->GetTypes().Num()));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
	}

	TEST_METHOD(RejectedSignatureDoesNotPublishOwnerOrParameters)
	{
		auto Set = asCDefinitions::Create();
		asCObjectType* Owner = nullptr;
		asCScriptFunction* Function = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(300), "Owner", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Owner))));
		ASSERT_THAT(IsTrue(Success(Set->CreateFunction(Key(301), "Method", "", asFUNC_SCRIPT, Function))));
		asSDefinitionParameter Parameters[2];
		Parameters[0].Type = Set->Primitive(ttInt);
		Parameters[0].DefaultArgument = "1";
		Parameters[1].Type = Set->Primitive(ttInt);
		ASSERT_THAT(IsTrue(Set->DefineFunction(*Function, Set->Primitive(ttInt), MakeArrayView(Parameters), Owner) == asEDefinitionResult::InvalidSignature));
		ASSERT_THAT(IsNull(Function->GetObjectType()));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Function->GetParamCount())));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Owner->GetMethodCount())));
		Parameters[1].DefaultArgument = "2";
		ASSERT_THAT(IsTrue(Success(Set->DefineFunction(*Function, Set->Primitive(ttInt), MakeArrayView(Parameters), Owner))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
	}

	TEST_METHOD(InheritanceCycleIsRejectedBeforeDraftQueries)
	{
		auto Set = asCDefinitions::Create();
		asCObjectType* Left = nullptr;
		asCObjectType* Right = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(310), "Left", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Left))));
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(311), "Right", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Right))));
		ASSERT_THAT(IsTrue(Success(Set->SetBaseType(*Left, *Right))));
		ASSERT_THAT(IsTrue(Set->SetBaseType(*Right, *Left) == asEDefinitionResult::RecursiveValueType));
		ASSERT_THAT(IsNull(Right->GetBaseType()));
		ASSERT_THAT(IsTrue(Left->DerivesFrom(Right)));
		ASSERT_THAT(IsFalse(Right->DerivesFrom(Left)));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
	}

	TEST_METHOD(ImportedLayoutAndTargetCompatibilityAreExplicit)
	{
		auto Dependency = asCDefinitions::Create();
		asCObjectType* External = nullptr;
		ASSERT_THAT(IsTrue(Success(Dependency->CreateObjectType(Key(320), "Native", "", asOBJ_VALUE | asOBJ_POD, External))));
		ASSERT_THAT(IsTrue(Success(Dependency->SetNativeLayout(*External, 16, 8))));
		ASSERT_THAT(IsTrue(Success(Dependency->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Dependency->Freeze())));
		asSDefinitionOptions OtherOptions;
		OtherOptions.PointerByteSize = sizeof(void*) == 8 ? 4 : 8;
		auto Incompatible = asCDefinitions::Create(OtherOptions);
		ASSERT_THAT(IsTrue(Incompatible->AddDependency(Dependency.Get()) == asEDefinitionResult::InvalidLayout));
		ASSERT_THAT(AreEqual(0, Incompatible->GetDependencies().Num()));
		auto Set = asCDefinitions::Create();
		ASSERT_THAT(IsTrue(Success(Set->AddDependency(Dependency.Get()))));
		asCObjectType* Local = nullptr;
		asCObjectProperty* Property = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(321), "Local", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Local))));
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Local, "Tag", Set->Primitive(ttUInt8), Property))));
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Local, "Value", asCDataType::CreateType(External, false), Property))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(AreEqual(8, Property->byteOffset));
		ASSERT_THAT(AreEqual(24, static_cast<int32>(Local->GetSize())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
	}

	TEST_METHOD(AdmittedFloatAliasesAndTargetReferencesMatchLayoutQueries)
	{
		asSDefinitionOptions Options;
		Options.FloatIsFloat64 = true;
		Options.PointerByteSize = 4;
		auto Set = asCDefinitions::Create(Options);
		asCObjectType* Type = nullptr;
		asCObjectProperty* Float = nullptr;
		asCObjectProperty* Reference = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(330), "Values", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Type, "Float", asCDataType::CreatePrimitive(ttFloat, false), Float))));
		asCDataType IntReference = Set->Primitive(ttInt);
		IntReference.MakeReference(true);
		ASSERT_THAT(IsTrue(Success(Set->AddProperty(*Type, "Reference", IntReference, Reference))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Float->type.IsFloat64Type()));
		ASSERT_THAT(AreEqual(8, Float->type.GetSizeInMemoryBytes()));
		ASSERT_THAT(AreEqual(8, Reference->byteOffset));
		ASSERT_THAT(AreEqual(4, Reference->type.GetAlignment()));
		ASSERT_THAT(AreEqual(4, Reference->type.GetSizeOfVariableBytes()));
		ASSERT_THAT(AreEqual(1, Reference->type.GetSizeOnStackDWords()));
		ASSERT_THAT(AreEqual(16, static_cast<int32>(Type->GetSize())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
	}

	TEST_METHOD(DefaultArrayAndChildFuncdefRetainSemanticEdges)
	{
		asSDefinitionOptions Options;
		Options.ExpandDefaultArrayToTemplate = false;
		auto Set = asCDefinitions::Create(Options);
		asCObjectType* Array = nullptr;
		asCScriptFunction* Signature = nullptr;
		asCCallableType* Child = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(340), "Array", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Array))));
		ASSERT_THAT(IsTrue(Success(Set->AddTemplateArgument(*Array, Set->Primitive(ttInt)))));
		ASSERT_THAT(IsTrue(Success(Set->SetDefaultArrayType(*Array))));
		ASSERT_THAT(IsTrue(Success(Set->CreateFunction(Key(341), "Visitor", "", asFUNC_CALLABLE_SIGNATURE, Signature))));
		ASSERT_THAT(IsTrue(Success(Set->DefineFunction(*Signature, Set->Primitive(ttVoid), {}))));
		ASSERT_THAT(IsTrue(Success(Set->CreateCallableType(Key(342), "Visitor", "", *Signature, Child))));
		ASSERT_THAT(IsTrue(Success(Set->AddChildCallableType(*Array, *Child))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
		ASSERT_THAT(IsTrue(Array->GetChildCallableType(0) == Child));
		ASSERT_THAT(IsTrue(Child->GetParentType() == Array));
		const asCDataType ArrayType = asCDataType::CreateType(Array, false);
		ASSERT_THAT(IsTrue(ArrayType.IsArrayType()));
		ASSERT_THAT(AreEqual(FString(TEXT("int[]")), FString(ANSI_TO_TCHAR(ArrayType.Format(nullptr).AddressOf()))));
	}

	TEST_METHOD(ConcurrentExternalReferencesKeepTheFrozenGraphAlive)
	{
		auto Set = asCDefinitions::Create();
		asCObjectType* Type = nullptr;
		asCScriptFunction* Function = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateObjectType(Key(220), "Shared", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Set->CreateFunction(Key(221), "Read", "", asFUNC_SCRIPT, Function))));
		ASSERT_THAT(IsTrue(Success(Set->DefineFunction(*Function, Set->Primitive(ttInt), {}, Type))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
		std::atomic<bool> Failed{false};
		std::vector<std::thread> Workers;
		for (int Index = 0; Index < 8; ++Index)
		{
			Workers.emplace_back([&]
			{
				for (int Iteration = 0; Iteration < 1000; ++Iteration)
				{
					Type->AddRef();
					Function->AddRef();
					if (Type->GetMethodByIndex(0) != Function || Function->GetObjectType() != Type) Failed.store(true);
					Function->Release();
					Type->Release();
				}
			});
		}
		for (std::thread& Worker : Workers) Worker.join();
		ASSERT_THAT(IsFalse(Failed.load()));
		ASSERT_THAT(IsNotNull(Set.Get()));
		ASSERT_THAT(IsTrue(Type->GetMethodByIndex(0) == Function));
	}
};

#endif
