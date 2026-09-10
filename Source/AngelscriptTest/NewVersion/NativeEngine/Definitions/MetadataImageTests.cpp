#include "CQTest.h"
#include "as_metadata_image.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

#include <atomic>
#include <thread>
#include <type_traits>
#include <vector>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

static_assert(!std::is_constructible_v<asCObjectType, asCMetadataImage&, const AS_NAMESPACE_QUALIFIER asSStableKey&>);
static_assert(!std::is_constructible_v<asCScriptFunction, asCMetadataImage&, const AS_NAMESPACE_QUALIFIER asSStableKey&, asEFuncType>);
static_assert(!std::is_constructible_v<asCEnumType, asCMetadataImage&, const AS_NAMESPACE_QUALIFIER asSStableKey&>);
static_assert(!std::is_constructible_v<asCTypedefType, asCMetadataImage&, const AS_NAMESPACE_QUALIFIER asSStableKey&>);

TEST_CLASS_WITH_FLAGS(MetadataImage, "Angelscript.UnitTest.NativeEngine",
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

	static bool Success(asEMetadataResult Result) { return Result == asEMetadataResult::Succeeded; }

public:
	TEST_METHOD(ActualTypesAndMethodsNeedNoEngineOrNumericIds)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Type = nullptr;
		asCScriptFunction* Method = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(1), "Counter", "Gameplay", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Image->CreateFunction(Key(2), "Read", "Gameplay", asFUNC_SCRIPT, Method))));
		ASSERT_THAT(IsTrue(Success(Image->DefineFunction(*Method, Image->Primitive(ttInt), {}, Type))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
		ASSERT_THAT(IsTrue(Type->GetMethodByIndex(0) == Method));
		ASSERT_THAT(IsTrue(Type->GetMethodByName("Read") == Method));
		ASSERT_THAT(IsTrue(Method->GetObjectType() == Type));
		ASSERT_THAT(IsTrue(Image->FindType(Key(1)) == Type));
		ASSERT_THAT(IsTrue(Image->FindFunction(Key(2)) == Method));
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
		auto Image = asCMetadataImage::Create();
		asCObjectType* Type = nullptr;
		asCScriptFunction* Method = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(10), "Node", "Graph", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Image->CreateFunction(Key(11), "Next", "Graph", asFUNC_SCRIPT, Method))));
		ASSERT_THAT(IsTrue(Success(Image->DefineFunction(*Method, asCDataType::CreateObjectHandle(Type, false), {}, Type))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
		ASSERT_THAT(IsTrue(Method->GetObjectType() == Type));
		ASSERT_THAT(IsTrue(Method->GetReturnDataType().GetTypeInfo() == Type));
		ASSERT_THAT(AreEqual(FString(TEXT("Graph")), FString(ANSI_TO_TCHAR(Type->GetNamespace()))));
		ASSERT_THAT(IsTrue(FString(ANSI_TO_TCHAR(Method->GetDeclaration())).Contains(TEXT("Node::Next"))));
		ASSERT_THAT(IsNotNull(Image.Get()));
	}

	TEST_METHOD(TypeLeaseRetainsRecursiveMembersUntilFinalRelease)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Left = nullptr;
		asCObjectType* Right = nullptr;
		asCObjectProperty* Property = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(20), "Left", "Pair", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Left))));
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(21), "Right", "Pair", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Right))));
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Left, "Other", asCDataType::CreateObjectHandle(Right, false), Property))));
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Right, "Other", asCDataType::CreateObjectHandle(Left, false), Property))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
		ASSERT_THAT(IsTrue(Left->properties[0]->type.GetTypeInfo() == Right));
		ASSERT_THAT(IsTrue(Right->properties[0]->type.GetTypeInfo() == Left));
		ASSERT_THAT(IsNotNull(Image.Get()));
	}

	TEST_METHOD(ByValueRecursionFailsWithoutPartialLayouts)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Left = nullptr;
		asCObjectType* Right = nullptr;
		asCObjectProperty* First = nullptr;
		asCObjectProperty* Second = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(30), "Left", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Left))));
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(31), "Right", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Right))));
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Left, "Value", asCDataType::CreateType(Right, false), First))));
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Right, "Value", asCDataType::CreateType(Left, false), Second))));
		ASSERT_THAT(IsTrue(Image->FinalizeLayouts() == asEMetadataResult::RecursiveValueType));
		ASSERT_THAT(AreEqual(-1, First->byteOffset));
		ASSERT_THAT(AreEqual(-1, Second->byteOffset));
		ASSERT_THAT(IsTrue(Image->Freeze() == asEMetadataResult::IncompleteDefinition));
		ASSERT_THAT(IsFalse(Image->IsFrozen()));
	}

	TEST_METHOD(FreezeRequiresCompleteSignaturesAndFinalizedLayouts)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Type = nullptr;
		asCScriptFunction* Function = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(40), "Value", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Image->CreateFunction(Key(41), "Run", "", asFUNC_SCRIPT, Function))));
		ASSERT_THAT(IsTrue(Image->Freeze() == asEMetadataResult::IncompleteDefinition));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Image->Freeze() == asEMetadataResult::IncompleteDefinition));
		ASSERT_THAT(IsTrue(Success(Image->DefineFunction(*Function, Image->Primitive(ttVoid), {}))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
	}

	TEST_METHOD(FrozenImageRejectsNewObjectsAndSemanticEdges)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Type = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(50), "Frozen", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
		asCObjectProperty* Property = nullptr;
		asCObjectType* NewType = nullptr;
		ASSERT_THAT(IsTrue(Image->AddProperty(*Type, "Late", Image->Primitive(ttInt), Property) == asEMetadataResult::InvalidState));
		ASSERT_THAT(IsTrue(Image->CreateObjectType(Key(51), "Late", "", asOBJ_VALUE, NewType) == asEMetadataResult::InvalidState));
		ASSERT_THAT(IsNull(Property));
		ASSERT_THAT(IsNull(NewType));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Type->GetPropertyCount())));
		ASSERT_THAT(AreEqual(1, Image->GetTypes().Num()));
	}

	TEST_METHOD(LayoutUsesTargetPointerSizeAndAlignment)
	{
		asSMetadataOptions Options;
		Options.PointerByteSize = 4;
		auto Image = asCMetadataImage::Create(Options);
		asCObjectType* Type = nullptr;
		asCObjectProperty* Tag = nullptr;
		asCObjectProperty* Number = nullptr;
		asCObjectProperty* Next = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(60), "Layout", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Type, "Tag", Image->Primitive(ttUInt8), Tag))));
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Type, "Number", Image->Primitive(ttInt), Number))));
		asCDataType Reference = asCDataType::CreateType(Type, false);
		Reference.MakeReference(true);
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Type, "Next", Reference, Next))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(AreEqual(0, Tag->byteOffset));
		ASSERT_THAT(AreEqual(4, Number->byteOffset));
		ASSERT_THAT(AreEqual(8, Next->byteOffset));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Type->GetSize())));
		ASSERT_THAT(AreEqual(4, Type->alignment));
	}

	TEST_METHOD(EnumAliasAndFuncdefAreActualOwnedTypes)
	{
		auto Image = asCMetadataImage::Create();
		asCEnumType* Enum = nullptr;
		asCTypedefType* Alias = nullptr;
		asCCallableType* CallableType = nullptr;
		asCScriptFunction* Signature = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateEnumType(Key(70), "Mode", "Api", Enum))));
		ASSERT_THAT(IsTrue(Success(Image->AddEnumValue(*Enum, "Enabled", 17))));
		ASSERT_THAT(IsTrue(Success(Image->CreateTypedefType(Key(71), "ModeAlias", "Api", Alias))));
		ASSERT_THAT(IsTrue(Success(Image->SetAlias(*Alias, asCDataType::CreateType(Enum, false)))));
		ASSERT_THAT(IsTrue(Success(Image->CreateFunction(Key(72), "Callback", "Api", asFUNC_CALLABLE_SIGNATURE, Signature))));
		asSMetadataParameter Parameter;
		Parameter.Name = "Mode";
		Parameter.Type = asCDataType::CreateType(Alias, false);
		ASSERT_THAT(IsTrue(Success(Image->DefineFunction(*Signature, Image->Primitive(ttVoid), MakeArrayView(&Parameter, 1)))));
		ASSERT_THAT(IsTrue(Success(Image->CreateCallableType(Key(73), "Callback", "Api", *Signature, CallableType))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
		int Value = 0;
		ASSERT_THAT(AreEqual(FString(TEXT("Enabled")), FString(ANSI_TO_TCHAR(Enum->GetEnumValueByIndex(0, &Value)))));
		ASSERT_THAT(AreEqual(17, Value));
		ASSERT_THAT(IsTrue(CallableType->GetCallableSignature() == Signature));
		ASSERT_THAT(IsTrue(Signature->GetParameterDataType(0)->GetTypeInfo() == Alias));
		ASSERT_THAT(IsTrue(Alias->aliasForType.GetTypeInfo() == Enum));
		ASSERT_THAT(IsNotNull(Image.Get()));
	}

	TEST_METHOD(FrozenDependencyLifetimeAndForeignEdgesAreExplicit)
	{
		auto Dependency = asCMetadataImage::Create();
		asCObjectType* External = nullptr;
		ASSERT_THAT(IsTrue(Success(Dependency->CreateObjectType(Key(80), "External", "Shared", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, External))));
		auto Image = asCMetadataImage::Create();
		ASSERT_THAT(IsTrue(Image->AddDependency(Dependency.Get()) == asEMetadataResult::UnfrozenDependency));
		ASSERT_THAT(IsTrue(Success(Dependency->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Dependency->Freeze())));
		asCObjectType* Local = nullptr;
		asCObjectProperty* Property = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(81), "Local", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Local))));
		ASSERT_THAT(IsTrue(Image->AddProperty(*Local, "External", asCDataType::CreateType(External, false), Property) == asEMetadataResult::ForeignOwner));
		ASSERT_THAT(IsTrue(Success(Image->AddDependency(Dependency.Get()))));
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Local, "External", asCDataType::CreateType(External, false), Property))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
		ASSERT_THAT(IsTrue(Dependency->AddDependency(Image.Get()) == asEMetadataResult::InvalidState));
		ASSERT_THAT(AreEqual(FString(TEXT("Shared")), FString(ANSI_TO_TCHAR(Property->type.GetTypeInfo()->GetNamespace()))));
		ASSERT_THAT(IsNotNull(Dependency.Get()));
		ASSERT_THAT(IsNotNull(Image.Get()));
	}

	TEST_METHOD(ImmutableOptionsDoNotFollowCallerOrOtherImageChanges)
	{
		asSMetadataOptions Options;
		Options.FloatIsFloat64 = false;
		auto Float32Image = asCMetadataImage::Create(Options);
		Options.FloatIsFloat64 = true;
		auto Float64Image = asCMetadataImage::Create(Options);
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
		auto Image = asCMetadataImage::Create();
		asCScriptFunction* Function = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateFunction(Key(90), "Read", "Api", asFUNC_SCRIPT, Function))));
		asSMetadataParameter Parameter;
		Parameter.Name = "Count";
		Parameter.Type = Image->Primitive(ttInt);
		Parameter.Flags = asTM_NONE;
		Parameter.DefaultArgument = "7";
		ASSERT_THAT(IsTrue(Success(Image->DefineFunction(*Function, Image->Primitive(ttInt), MakeArrayView(&Parameter, 1)))));
		Parameter.Name = "ChangedCallerStorage";
		int TypeId = 999;
		const char* Name = nullptr;
		const char* Default = nullptr;
		asDWORD Flags = 999;
		ASSERT_THAT(AreEqual(asSUCCESS, Function->GetParam(0, &TypeId, &Flags, &Name, &Default)));
		ASSERT_THAT(IsTrue(TypeId < 0));
		ASSERT_THAT(AreEqual(FString(TEXT("Count")), FString(ANSI_TO_TCHAR(Name))));
		ASSERT_THAT(AreEqual(FString(TEXT("7")), FString(ANSI_TO_TCHAR(Default))));
		ASSERT_THAT(AreEqual(asDWORD(asTM_NONE), Flags));
		ASSERT_THAT(IsTrue(Function->GetParameterDataType(0)->GetTokenType() == ttInt));
		ASSERT_THAT(IsNull(Function->GetParameterDataType(1)));
		ASSERT_THAT(IsTrue(Function->GetReturnTypeId() < 0));
	}

	TEST_METHOD(DuplicateIdentityAndNameFailuresDoNotPublishObjects)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Type = nullptr;
		asCObjectType* Other = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(100), "Name", "Scope", asOBJ_VALUE, Type))));
		ASSERT_THAT(IsTrue(Image->CreateObjectType(Key(100), "Other", "Scope", asOBJ_VALUE, Other) == asEMetadataResult::DuplicateIdentity));
		ASSERT_THAT(IsTrue(Image->CreateObjectType(Key(101), "Name", "Scope", asOBJ_VALUE, Other) == asEMetadataResult::DuplicateName));
		ASSERT_THAT(IsNull(Other));
		ASSERT_THAT(AreEqual(1, Image->GetTypes().Num()));
		asCScriptFunction* DelegateInstance = nullptr;
		ASSERT_THAT(IsTrue(Image->CreateFunction(Key(102), "LiveDelegate", "", asFUNC_DELEGATE, DelegateInstance) == asEMetadataResult::InvalidArgument));
		ASSERT_THAT(IsNull(DelegateInstance));
	}

	TEST_METHOD(FactoryAndBehaviourQueriesUseObjectsBeforeRegistration)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Type = nullptr;
		asCScriptFunction* Factory = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(110), "Thing", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Image->CreateFunction(Key(111), "MakeThing", "", asFUNC_SCRIPT, Factory))));
		ASSERT_THAT(IsTrue(Success(Image->DefineFunction(*Factory, asCDataType::CreateObjectHandle(Type, false), {}))));
		ASSERT_THAT(IsTrue(Success(Image->AddBehaviour(*Type, asBEHAVE_FACTORY, *Factory))));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Type->GetFactoryCount())));
		ASSERT_THAT(IsTrue(Type->GetFactoryByIndex(0) == Factory));
		asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
		ASSERT_THAT(IsTrue(Type->GetBehaviourByIndex(0, &Behaviour) == Factory));
		ASSERT_THAT(IsTrue(Behaviour == asBEHAVE_FACTORY));
		ASSERT_THAT(IsTrue(Factory->GetId() <= 0));
	}

	TEST_METHOD(InheritanceAndInterfacesRemainLocalDefinitionRelations)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Base = nullptr;
		asCObjectType* Derived = nullptr;
		asCObjectType* Interface = nullptr;
		asCObjectProperty* Property = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(120), "Base", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Base))));
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(121), "Derived", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Derived))));
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(122), "Interface", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Interface))));
		ASSERT_THAT(IsTrue(Success(Image->SetInterfaceDeclaration(*Interface))));
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Base, "First", Image->Primitive(ttInt), Property))));
		ASSERT_THAT(IsTrue(Success(Image->SetBaseType(*Derived, *Base))));
		ASSERT_THAT(IsTrue(Success(Image->AddInterface(*Derived, *Interface))));
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Derived, "Second", Image->Primitive(ttInt), Property))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Derived->GetBaseType() == Base));
		ASSERT_THAT(IsTrue(Derived->DerivesFrom(Base)));
		ASSERT_THAT(IsTrue(Derived->Implements(Interface)));
		ASSERT_THAT(AreEqual(4, Property->byteOffset));
		ASSERT_THAT(AreEqual(8, static_cast<int32>(Derived->GetSize())));
	}

	TEST_METHOD(AliasCycleIsRejectedWithoutRecursingForever)
	{
		auto Image = asCMetadataImage::Create();
		asCTypedefType* Left = nullptr;
		asCTypedefType* Right = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateTypedefType(Key(130), "Left", "", Left))));
		ASSERT_THAT(IsTrue(Success(Image->CreateTypedefType(Key(131), "Right", "", Right))));
		ASSERT_THAT(IsTrue(Success(Image->SetAlias(*Left, asCDataType::CreateType(Right, false)))));
		ASSERT_THAT(IsTrue(Success(Image->SetAlias(*Right, asCDataType::CreateType(Left, false)))));
		ASSERT_THAT(IsTrue(Image->FinalizeLayouts() == asEMetadataResult::RecursiveValueType));
		ASSERT_THAT(IsFalse(Image->IsFrozen()));
	}

	TEST_METHOD(InvalidNativeLayoutDoesNotBecomeAValidFrozenType)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Native = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(140), "Native", "", asOBJ_VALUE | asOBJ_POD, Native))));
		ASSERT_THAT(IsTrue(Image->SetNativeLayout(*Native, 12, 3) == asEMetadataResult::InvalidLayout));
		ASSERT_THAT(IsTrue(Image->FinalizeLayouts() == asEMetadataResult::IncompleteDefinition));
		ASSERT_THAT(IsTrue(Success(Image->SetNativeLayout(*Native, 16, 8))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
		ASSERT_THAT(AreEqual(16, static_cast<int32>(Native->GetSize())));
		ASSERT_THAT(AreEqual(8, Native->alignment));
	}

	TEST_METHOD(ConcurrentFactoriesSerializeOwnershipWithoutAnEngine)
	{
		auto Image = asCMetadataImage::Create();
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
				if (!Success(Image->CreateObjectType(Key(200 + Index), Name.AddressOf(), "Parallel", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))) Failed.store(true);
			});
		}
		while (Ready.load() != 8) std::this_thread::yield();
		Start.store(true, std::memory_order_release);
		for (std::thread& Worker : Workers) Worker.join();
		ASSERT_THAT(IsFalse(Failed.load()));
		ASSERT_THAT(AreEqual(8, Image->GetTypes().Num()));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
	}

	TEST_METHOD(RejectedSignatureDoesNotPublishOwnerOrParameters)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Owner = nullptr;
		asCScriptFunction* Function = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(300), "Owner", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Owner))));
		ASSERT_THAT(IsTrue(Success(Image->CreateFunction(Key(301), "Method", "", asFUNC_SCRIPT, Function))));
		asSMetadataParameter Parameters[2];
		Parameters[0].Type = Image->Primitive(ttInt);
		Parameters[0].DefaultArgument = "1";
		Parameters[1].Type = Image->Primitive(ttInt);
		ASSERT_THAT(IsTrue(Image->DefineFunction(*Function, Image->Primitive(ttInt), MakeArrayView(Parameters), Owner) == asEMetadataResult::InvalidSignature));
		ASSERT_THAT(IsNull(Function->GetObjectType()));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Function->GetParamCount())));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Owner->GetMethodCount())));
		Parameters[1].DefaultArgument = "2";
		ASSERT_THAT(IsTrue(Success(Image->DefineFunction(*Function, Image->Primitive(ttInt), MakeArrayView(Parameters), Owner))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
	}

	TEST_METHOD(InheritanceCycleIsRejectedBeforeDraftQueries)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Left = nullptr;
		asCObjectType* Right = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(310), "Left", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Left))));
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(311), "Right", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Right))));
		ASSERT_THAT(IsTrue(Success(Image->SetBaseType(*Left, *Right))));
		ASSERT_THAT(IsTrue(Image->SetBaseType(*Right, *Left) == asEMetadataResult::RecursiveValueType));
		ASSERT_THAT(IsNull(Right->GetBaseType()));
		ASSERT_THAT(IsTrue(Left->DerivesFrom(Right)));
		ASSERT_THAT(IsFalse(Right->DerivesFrom(Left)));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
	}

	TEST_METHOD(ImportedLayoutAndTargetCompatibilityAreExplicit)
	{
		auto Dependency = asCMetadataImage::Create();
		asCObjectType* External = nullptr;
		ASSERT_THAT(IsTrue(Success(Dependency->CreateObjectType(Key(320), "Native", "", asOBJ_VALUE | asOBJ_POD, External))));
		ASSERT_THAT(IsTrue(Success(Dependency->SetNativeLayout(*External, 16, 8))));
		ASSERT_THAT(IsTrue(Success(Dependency->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Dependency->Freeze())));
		asSMetadataOptions OtherOptions;
		OtherOptions.PointerByteSize = sizeof(void*) == 8 ? 4 : 8;
		auto Incompatible = asCMetadataImage::Create(OtherOptions);
		ASSERT_THAT(IsTrue(Incompatible->AddDependency(Dependency.Get()) == asEMetadataResult::InvalidLayout));
		ASSERT_THAT(AreEqual(0, Incompatible->GetDependencies().Num()));
		auto Image = asCMetadataImage::Create();
		ASSERT_THAT(IsTrue(Success(Image->AddDependency(Dependency.Get()))));
		asCObjectType* Local = nullptr;
		asCObjectProperty* Property = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(321), "Local", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Local))));
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Local, "Tag", Image->Primitive(ttUInt8), Property))));
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Local, "Value", asCDataType::CreateType(External, false), Property))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(AreEqual(8, Property->byteOffset));
		ASSERT_THAT(AreEqual(24, static_cast<int32>(Local->GetSize())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
	}

	TEST_METHOD(AdmittedFloatAliasesAndTargetReferencesMatchLayoutQueries)
	{
		asSMetadataOptions Options;
		Options.FloatIsFloat64 = true;
		Options.PointerByteSize = 4;
		auto Image = asCMetadataImage::Create(Options);
		asCObjectType* Type = nullptr;
		asCObjectProperty* Float = nullptr;
		asCObjectProperty* Reference = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(330), "Values", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Type, "Float", asCDataType::CreatePrimitive(ttFloat, false), Float))));
		asCDataType IntReference = Image->Primitive(ttInt);
		IntReference.MakeReference(true);
		ASSERT_THAT(IsTrue(Success(Image->AddProperty(*Type, "Reference", IntReference, Reference))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Float->type.IsFloat64Type()));
		ASSERT_THAT(AreEqual(8, Float->type.GetSizeInMemoryBytes()));
		ASSERT_THAT(AreEqual(8, Reference->byteOffset));
		ASSERT_THAT(AreEqual(4, Reference->type.GetAlignment()));
		ASSERT_THAT(AreEqual(4, Reference->type.GetSizeOfVariableBytes()));
		ASSERT_THAT(AreEqual(1, Reference->type.GetSizeOnStackDWords()));
		ASSERT_THAT(AreEqual(16, static_cast<int32>(Type->GetSize())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
	}

	TEST_METHOD(DefaultArrayAndChildFuncdefRetainSemanticEdges)
	{
		asSMetadataOptions Options;
		Options.ExpandDefaultArrayToTemplate = false;
		auto Image = asCMetadataImage::Create(Options);
		asCObjectType* Array = nullptr;
		asCScriptFunction* Signature = nullptr;
		asCCallableType* Child = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(340), "Array", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Array))));
		ASSERT_THAT(IsTrue(Success(Image->AddTemplateArgument(*Array, Image->Primitive(ttInt)))));
		ASSERT_THAT(IsTrue(Success(Image->SetDefaultArrayType(*Array))));
		ASSERT_THAT(IsTrue(Success(Image->CreateFunction(Key(341), "Visitor", "", asFUNC_CALLABLE_SIGNATURE, Signature))));
		ASSERT_THAT(IsTrue(Success(Image->DefineFunction(*Signature, Image->Primitive(ttVoid), {}))));
		ASSERT_THAT(IsTrue(Success(Image->CreateCallableType(Key(342), "Visitor", "", *Signature, Child))));
		ASSERT_THAT(IsTrue(Success(Image->AddChildCallableType(*Array, *Child))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
		ASSERT_THAT(IsTrue(Array->GetChildCallableType(0) == Child));
		ASSERT_THAT(IsTrue(Child->GetParentType() == Array));
		const asCDataType ArrayType = asCDataType::CreateType(Array, false);
		ASSERT_THAT(IsTrue(ArrayType.IsArrayType()));
		ASSERT_THAT(AreEqual(FString(TEXT("int[]")), FString(ANSI_TO_TCHAR(ArrayType.Format(nullptr).AddressOf()))));
	}

	TEST_METHOD(ConcurrentExternalReferencesKeepTheFrozenGraphAlive)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Type = nullptr;
		asCScriptFunction* Function = nullptr;
		ASSERT_THAT(IsTrue(Success(Image->CreateObjectType(Key(220), "Shared", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Image->CreateFunction(Key(221), "Read", "", asFUNC_SCRIPT, Function))));
		ASSERT_THAT(IsTrue(Success(Image->DefineFunction(*Function, Image->Primitive(ttInt), {}, Type))));
		ASSERT_THAT(IsTrue(Success(Image->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Image->Freeze())));
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
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(IsTrue(Type->GetMethodByIndex(0) == Function));
	}
};

#endif
