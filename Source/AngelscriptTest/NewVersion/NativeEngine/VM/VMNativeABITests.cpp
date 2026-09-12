#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_objecttype.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GAbiCalls = 0;
	int32* GAbiForwarded = nullptr;

	struct alignas(8) FAbiReceiver
	{
		int32 Value = 0;
		uint8 Pad[4] = {};
		int32 Sentinel = 0;
	};

	struct FAbiBaseA
	{
		int32 Pad = 0;
	};

	struct FAbiBaseB
	{
		int32 Value = 0;
		int32 Sentinel = 0;
	};

	struct FAbiDerived : FAbiBaseA, FAbiBaseB
	{
	};

	struct FAbiValue
	{
		int32 Value = 0;
		int32 Guard = 0;
	};

	FAbiValue GAbiHandleTarget{42, 0x5A5A5A5A};

	void ResetAbi()
	{
		GAbiCalls = 0;
		GAbiForwarded = nullptr;
	}

	asSMetadataParameter AbiParam(const char* Name, const asCDataType& Type, asDWORD Flags = asTM_NONE)
	{
		asSMetadataParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Type;
		Parameter.Flags = Flags;
		return Parameter;
	}

	asCDataType AbiObjectRef(asCObjectType* Type)
	{
		asCDataType DataType = asCDataType::CreateType(Type, false);
		DataType.MakeReference(true);
		return DataType;
	}

	asSByteCodeOperand AbiBits32(uint32 Bits)
	{
		return ByteImm32(static_cast<int32>(Bits));
	}

	uint32 AbiFloatBits(float Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	void GenericAbiAddInt(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		Generic->SetReturnDWord(static_cast<asDWORD>(A + B));
	}

	void GenericAbiAddInt64(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		Generic->SetReturnQWord(static_cast<asQWORD>(
			static_cast<int64>(Generic->GetArgQWord(0)) + static_cast<int64>(Generic->GetArgQWord(1))));
	}

	void GenericAbiAddInt8(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		Generic->SetReturnByte(static_cast<asBYTE>(Generic->GetArgByte(0) + Generic->GetArgByte(1)));
	}

	void GenericAbiAddInt16(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		Generic->SetReturnWord(static_cast<asWORD>(Generic->GetArgWord(0) + Generic->GetArgWord(1)));
	}

	void GenericAbiAndBool(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		Generic->SetReturnByte(static_cast<asBYTE>(Generic->GetArgByte(0) && Generic->GetArgByte(1)));
	}

	void GenericAbiAddFloat(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		Generic->SetReturnFloat(Generic->GetArgFloat(0) + Generic->GetArgFloat(1));
	}

	void GenericAbiAddDouble(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		Generic->SetReturnDouble(Generic->GetArgDouble(0) + Generic->GetArgDouble(1));
	}

	void GenericAbiAddToReceiver(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		FAbiReceiver* Self = static_cast<FAbiReceiver*>(Generic->GetObject());
		Generic->SetReturnDWord(static_cast<asDWORD>(Self->Value + static_cast<int32>(Generic->GetArgDWord(0))));
	}

	void GenericAbiAddToBaseB(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		FAbiBaseB* Self = static_cast<FAbiBaseB*>(Generic->GetObject());
		Generic->SetReturnDWord(static_cast<asDWORD>(Self->Value + static_cast<int32>(Generic->GetArgDWord(0))));
	}

	void GenericAbiOut(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		*static_cast<int32*>(Generic->GetArgAddress(0)) = 41;
	}

	void GenericAbiInOut(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		int32* Value = static_cast<int32*>(Generic->GetArgAddress(0));
		*Value = *Value + 1;
	}

	void GenericAbiIn(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		(void)*static_cast<int32*>(Generic->GetArgAddress(0));
	}

	void GenericAbiForward(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		GAbiForwarded = static_cast<int32*>(Generic->GetArgAddress(0));
	}

	void GenericAbiWriteValue(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		FAbiValue* Value = static_cast<FAbiValue*>(Generic->GetArgAddress(0));
		Value->Value = 42;
	}

	void GenericAbiReturnHandle(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		Generic->SetReturnObject(&GAbiHandleTarget);
	}

	void GenericAbiThrow(asIScriptGeneric* Generic)
	{
		++GAbiCalls;
		(void)Generic;
		asGetActiveContext()->SetException("native failed");
	}

	int AbiTypedAddInt(int A, int B)
	{
		++GAbiCalls;
		return A + B;
	}

	int8 AbiTypedAddInt8(int8 A, int8 B)
	{
		++GAbiCalls;
		return static_cast<int8>(A + B);
	}

	int16 AbiTypedAddInt16(int16 A, int16 B)
	{
		++GAbiCalls;
		return static_cast<int16>(A + B);
	}

	int64 AbiTypedAddInt64(int64 A, int64 B)
	{
		++GAbiCalls;
		return A + B;
	}

	bool AbiTypedAndBool(bool A, bool B)
	{
		++GAbiCalls;
		return A && B;
	}

	float AbiTypedAddFloat(float A, float B)
	{
		++GAbiCalls;
		return A + B;
	}

	double AbiTypedAddDouble(double A, double B)
	{
		++GAbiCalls;
		return A + B;
	}

	int AbiObjFirstAdd(FAbiReceiver* Self, int Addend)
	{
		++GAbiCalls;
		return Self->Value + Addend;
	}

	int AbiObjLastAdd(int Addend, FAbiReceiver* Self)
	{
		++GAbiCalls;
		return Self->Value + Addend;
	}

	void CallAbiAddInt(asFUNCTION_t Method, void** Parameters, void* ReturnValue)
	{
		using Fn = int (*)(int, int);
		*static_cast<int*>(ReturnValue) = reinterpret_cast<Fn>(Method)(
			*static_cast<int*>(Parameters[0]), *static_cast<int*>(Parameters[1]));
	}

	void CallAbiAddInt8(asFUNCTION_t Method, void** Parameters, void* ReturnValue)
	{
		using Fn = int8 (*)(int8, int8);
		*static_cast<int8*>(ReturnValue) = reinterpret_cast<Fn>(Method)(
			*static_cast<int8*>(Parameters[0]), *static_cast<int8*>(Parameters[1]));
	}

	void CallAbiAddInt16(asFUNCTION_t Method, void** Parameters, void* ReturnValue)
	{
		using Fn = int16 (*)(int16, int16);
		*static_cast<int16*>(ReturnValue) = reinterpret_cast<Fn>(Method)(
			*static_cast<int16*>(Parameters[0]), *static_cast<int16*>(Parameters[1]));
	}

	void CallAbiAddInt64(asFUNCTION_t Method, void** Parameters, void* ReturnValue)
	{
		using Fn = int64 (*)(int64, int64);
		*static_cast<int64*>(ReturnValue) = reinterpret_cast<Fn>(Method)(
			*static_cast<int64*>(Parameters[0]), *static_cast<int64*>(Parameters[1]));
	}

	void CallAbiAndBool(asFUNCTION_t Method, void** Parameters, void* ReturnValue)
	{
		using Fn = bool (*)(bool, bool);
		*static_cast<bool*>(ReturnValue) = reinterpret_cast<Fn>(Method)(
			*static_cast<bool*>(Parameters[0]), *static_cast<bool*>(Parameters[1]));
	}

	void CallAbiAddFloat(asFUNCTION_t Method, void** Parameters, void* ReturnValue)
	{
		using Fn = float (*)(float, float);
		*static_cast<float*>(ReturnValue) = reinterpret_cast<Fn>(Method)(
			*static_cast<float*>(Parameters[0]), *static_cast<float*>(Parameters[1]));
	}

	void CallAbiAddDouble(asFUNCTION_t Method, void** Parameters, void* ReturnValue)
	{
		using Fn = double (*)(double, double);
		*static_cast<double*>(ReturnValue) = reinterpret_cast<Fn>(Method)(
			*static_cast<double*>(Parameters[0]), *static_cast<double*>(Parameters[1]));
	}

	void CallAbiObjFirst(asFUNCTION_t Method, void** Parameters, void* ReturnValue)
	{
		using Fn = int (*)(FAbiReceiver*, int);
		*static_cast<int*>(ReturnValue) = reinterpret_cast<Fn>(Method)(
			static_cast<FAbiReceiver*>(Parameters[0]), *static_cast<int*>(Parameters[1]));
	}

	void CallAbiObjLast(asFUNCTION_t Method, void** Parameters, void* ReturnValue)
	{
		using Fn = int (*)(int, FAbiReceiver*);
		*static_cast<int*>(ReturnValue) = reinterpret_cast<Fn>(Method)(
			*static_cast<int*>(Parameters[0]), static_cast<FAbiReceiver*>(Parameters[1]));
	}

	asEByteCodeImageStatus EmitTwoImm32Call(
		asCByteCodeImageBuilder& Builder,
		asCScriptFunction* Run,
		asCScriptFunction* Native,
		int32 Left,
		int32 Right)
	{
		asUINT NativeSlot = 0;
		asUINT RunSlot = 0;
		asEByteCodeImageStatus Status = Builder.AddFunctionRequirement(*Native, NativeSlot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.AddFunctionRequirement(*Run, RunSlot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand RightOp = ByteImm32(Right);
		Status = Builder.Emit(asBC_PshC4, MakeArrayView(&RightOp, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand LeftOp = ByteImm32(Left);
		Status = Builder.Emit(asBC_PshC4, MakeArrayView(&LeftOp, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NativeSlot);
		Status = Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		Status = Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		return Builder.EndFunction();
	}

	asEByteCodeImageStatus EmitReceiverCall(
		asCByteCodeImageBuilder& Builder,
		asCScriptFunction* Run,
		asCScriptFunction* Method,
		asCObjectType* Type,
		int32 Addend)
	{
		asUINT TypeSlot = 0;
		asUINT MethodSlot = 0;
		asUINT RunSlot = 0;
		asEByteCodeImageStatus Status = Builder.AddTypeRequirement(*Type, TypeSlot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.AddFunctionRequirement(*Method, MethodSlot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.AddFunctionRequirement(*Run, RunSlot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Five = ByteImm32(Addend);
		Status = Builder.Emit(asBC_PshC4, MakeArrayView(&Five, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Self = ByteLocal(0);
		Status = Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, MethodSlot);
		Status = Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		Status = Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		return Builder.EndFunction();
	}
}

TEST_CLASS_WITH_FLAGS(VMNativeABI, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(PaddedReceiverSevenPlusFiveReturnsTwelve)
	{
		ResetAbi();
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("AbiReceiver"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Set->SetNativeLayout(*Type, sizeof(FAbiReceiver), alignof(FAbiReceiver))));
		asSMetadataParameter Addend = AbiParam("addend", Fixture.Set->Primitive(ttInt));
		asCScriptFunction* Method = Fixture.DefineFunction(
			TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(&Addend, 1), Type);
		asSMetadataParameter Receiver = AbiParam("receiver", AbiObjectRef(Type), asTM_INOUTREF);
		asCScriptFunction* Run = Fixture.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Receiver, 1));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Method->GetStableKey(), asFUNCTION(GenericAbiAddToReceiver), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReceiverCall(Builder, Run, Method, Type, 5)));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		FAbiReceiver ReceiverValue;
		FMemory::Memset(&ReceiverValue, 0xAA, sizeof(ReceiverValue));
		ReceiverValue.Value = 7;
		ReceiverValue.Sentinel = 0x44556677;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &ReceiverValue)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(1, GAbiCalls));
		ASSERT_THAT(AreEqual(7, ReceiverValue.Value));
		ASSERT_THAT(AreEqual(0x44556677, ReceiverValue.Sentinel));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0xAA), ReceiverValue.Pad[0]));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(ObjectFirstAndLastReachReceiver)
	{
		ResetAbi();
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("AbiReceiver"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Set->SetNativeLayout(*Type, sizeof(FAbiReceiver), alignof(FAbiReceiver))));
		asSMetadataParameter Addend = AbiParam("addend", Fixture.Set->Primitive(ttInt));
		asCScriptFunction* First = Fixture.DefineFunction(
			TEXT("AddFirst"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(&Addend, 1), Type);
		asCScriptFunction* Last = Fixture.DefineFunction(
			TEXT("AddLast"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(&Addend, 1), Type);
		asSMetadataParameter Receiver = AbiParam("receiver", AbiObjectRef(Type), asTM_INOUTREF);
		asCScriptFunction* RunFirst = Fixture.DefineFunction(
			TEXT("RunFirst"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Receiver, 1));
		asCScriptFunction* RunLast = Fixture.DefineFunction(
			TEXT("RunLast"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Receiver, 1));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(First->GetStableKey(), asFUNCTION(AbiObjFirstAdd), asCALL_CDECL_OBJFIRST, asFunctionCaller(&CallAbiObjFirst))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Last->GetStableKey(), asFUNCTION(AbiObjLastAdd), asCALL_CDECL_OBJLAST, asFunctionCaller(&CallAbiObjLast))));
		FAbiReceiver ReceiverValue;
		FMemory::Memset(&ReceiverValue, 0xAA, sizeof(ReceiverValue));
		ReceiverValue.Value = 7;
		ReceiverValue.Sentinel = 0x44556677;
		{
			asCByteCodeImageBuilder Builder;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReceiverCall(Builder, RunFirst, First, Type, 5)));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(RunFirst)));
			ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &ReceiverValue)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(12, static_cast<int32>(Context->GetReturnDWord())));
			Context->Release();
			Snapshot->Release();
		}
		{
			asCByteCodeImageBuilder Builder;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReceiverCall(Builder, RunLast, Last, Type, 5)));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(RunLast)));
			ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &ReceiverValue)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(12, static_cast<int32>(Context->GetReturnDWord())));
			Context->Release();
			Snapshot->Release();
		}
		ASSERT_THAT(AreEqual(2, GAbiCalls));
		ASSERT_THAT(AreEqual(7, ReceiverValue.Value));
		ASSERT_THAT(AreEqual(0x44556677, ReceiverValue.Sentinel));
	}

	TEST_METHOD(MultipleInheritanceAppliesBaseOffset)
	{
		ResetAbi();
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("AbiDerived"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Set->SetNativeLayout(*Type, sizeof(FAbiDerived), alignof(FAbiDerived))));
		asSMetadataParameter Addend = AbiParam("addend", Fixture.Set->Primitive(ttInt));
		asCScriptFunction* Method = Fixture.DefineFunction(
			TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(&Addend, 1), Type);
		asSMetadataParameter Receiver = AbiParam("receiver", AbiObjectRef(Type), asTM_INOUTREF);
		asCScriptFunction* Run = Fixture.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Receiver, 1));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		const int BaseOffset = static_cast<int>(offsetof(FAbiDerived, Value));
		ASSERT_THAT(IsTrue(BaseOffset != 0));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Method->GetStableKey(), asFUNCTION(GenericAbiAddToBaseB), asCALL_GENERIC, nullptr, nullptr, BaseOffset)));
		asCByteCodeImageBuilder Builder;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReceiverCall(Builder, Run, Method, Type, 5)));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		FAbiDerived Derived;
		Derived.Pad = 0x11111111;
		Derived.Value = 7;
		Derived.Sentinel = 0x44556677;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &Derived)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(1, GAbiCalls));
		ASSERT_THAT(AreEqual(0x11111111, Derived.Pad));
		ASSERT_THAT(AreEqual(7, Derived.Value));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(PrimitiveWidthsGenericAndTyped)
	{
		ResetAbi();
		FDetachedDefinitionFixture Fixture;
		auto Two = [&](eTokenType Token, const TCHAR* Name)
		{
			asSMetadataParameter Params[2] = { AbiParam("a", Fixture.Set->Primitive(Token)), AbiParam("b", Fixture.Set->Primitive(Token)) };
			return Fixture.DefineFunction(Name, TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(Token), MakeArrayView(Params, 2));
		};
		asCScriptFunction* AddI8 = Two(ttInt8, TEXT("AddI8"));
		asCScriptFunction* AddI16 = Two(ttInt16, TEXT("AddI16"));
		asCScriptFunction* AddI32 = Two(ttInt, TEXT("AddI32"));
		asCScriptFunction* AddI64 = Two(ttInt64, TEXT("AddI64"));
		asCScriptFunction* AndB = Two(ttBool, TEXT("AndB"));
		asCScriptFunction* AddF = Two(ttFloat32, TEXT("AddF"));
		asCScriptFunction* AddD = Two(ttFloat64, TEXT("AddD"));
		asCScriptFunction* RunI8 = Fixture.DefineFunction(TEXT("RunI8"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt8), {});
		asCScriptFunction* RunI16 = Fixture.DefineFunction(TEXT("RunI16"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt16), {});
		asCScriptFunction* RunI32 = Fixture.DefineFunction(TEXT("RunI32"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* RunI64 = Fixture.DefineFunction(TEXT("RunI64"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt64), {});
		asCScriptFunction* RunB = Fixture.DefineFunction(TEXT("RunB"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttBool), {});
		asCScriptFunction* RunF = Fixture.DefineFunction(TEXT("RunF"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttFloat32), {});
		asCScriptFunction* RunD = Fixture.DefineFunction(TEXT("RunD"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttFloat64), {});
		asCScriptFunction* TypedI32 = Two(ttInt, TEXT("TypedI32"));
		asCScriptFunction* TypedI8 = Two(ttInt8, TEXT("TypedI8"));
		asCScriptFunction* TypedI16 = Two(ttInt16, TEXT("TypedI16"));
		asCScriptFunction* TypedI64 = Two(ttInt64, TEXT("TypedI64"));
		asCScriptFunction* TypedB = Two(ttBool, TEXT("TypedB"));
		asCScriptFunction* TypedF = Two(ttFloat32, TEXT("TypedF"));
		asCScriptFunction* TypedD = Two(ttFloat64, TEXT("TypedD"));
		asCScriptFunction* RunTI32 = Fixture.DefineFunction(TEXT("RunTI32"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* RunTI8 = Fixture.DefineFunction(TEXT("RunTI8"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt8), {});
		asCScriptFunction* RunTI16 = Fixture.DefineFunction(TEXT("RunTI16"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt16), {});
		asCScriptFunction* RunTI64 = Fixture.DefineFunction(TEXT("RunTI64"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt64), {});
		asCScriptFunction* RunTB = Fixture.DefineFunction(TEXT("RunTB"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttBool), {});
		asCScriptFunction* RunTF = Fixture.DefineFunction(TEXT("RunTF"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttFloat32), {});
		asCScriptFunction* RunTD = Fixture.DefineFunction(TEXT("RunTD"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttFloat64), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(AddI8->GetStableKey(), asFUNCTION(GenericAbiAddInt8), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(AddI16->GetStableKey(), asFUNCTION(GenericAbiAddInt16), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(AddI32->GetStableKey(), asFUNCTION(GenericAbiAddInt), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(AddI64->GetStableKey(), asFUNCTION(GenericAbiAddInt64), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(AndB->GetStableKey(), asFUNCTION(GenericAbiAndBool), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(AddF->GetStableKey(), asFUNCTION(GenericAbiAddFloat), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(AddD->GetStableKey(), asFUNCTION(GenericAbiAddDouble), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(TypedI32->GetStableKey(), asFUNCTION(AbiTypedAddInt), asCALL_CDECL, asFunctionCaller(&CallAbiAddInt))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(TypedI8->GetStableKey(), asFUNCTION(AbiTypedAddInt8), asCALL_CDECL, asFunctionCaller(&CallAbiAddInt8))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(TypedI16->GetStableKey(), asFUNCTION(AbiTypedAddInt16), asCALL_CDECL, asFunctionCaller(&CallAbiAddInt16))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(TypedI64->GetStableKey(), asFUNCTION(AbiTypedAddInt64), asCALL_CDECL, asFunctionCaller(&CallAbiAddInt64))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(TypedB->GetStableKey(), asFUNCTION(AbiTypedAndBool), asCALL_CDECL, asFunctionCaller(&CallAbiAndBool))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(TypedF->GetStableKey(), asFUNCTION(AbiTypedAddFloat), asCALL_CDECL, asFunctionCaller(&CallAbiAddFloat))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(TypedD->GetStableKey(), asFUNCTION(AbiTypedAddDouble), asCALL_CDECL, asFunctionCaller(&CallAbiAddDouble))));

		auto LinkExecute = [&](asCScriptFunction* Run, asCScriptFunction* Native, auto EmitBody, auto Observe)
		{
			asCByteCodeImageBuilder Builder;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitBody(Builder, Run, Native)));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			Observe(Context);
			Context->Release();
			Snapshot->Release();
		};

		auto EmitI32 = [](asCByteCodeImageBuilder& Builder, asCScriptFunction* Run, asCScriptFunction* Native)
		{
			return EmitTwoImm32Call(Builder, Run, Native, 20, 22);
		};
		LinkExecute(RunI8, AddI8, EmitI32, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnByte())));
		});
		LinkExecute(RunI16, AddI16, EmitI32, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnWord())));
		});
		LinkExecute(RunI32, AddI32, EmitI32, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		});
		LinkExecute(RunB, AndB, [](asCByteCodeImageBuilder& Builder, asCScriptFunction* Run, asCScriptFunction* Native)
		{
			return EmitTwoImm32Call(Builder, Run, Native, 1, 1);
		}, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnByte())));
		});
		LinkExecute(RunTI32, TypedI32, EmitI32, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		});
		LinkExecute(RunTI8, TypedI8, EmitI32, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnByte())));
		});
		LinkExecute(RunTI16, TypedI16, EmitI32, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnWord())));
		});
		LinkExecute(RunTB, TypedB, [](asCByteCodeImageBuilder& Builder, asCScriptFunction* Run, asCScriptFunction* Native)
		{
			return EmitTwoImm32Call(Builder, Run, Native, 1, 1);
		}, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnByte())));
		});

		auto EmitI64 = [](asCByteCodeImageBuilder& Builder, asCScriptFunction* Run, asCScriptFunction* Native)
		{
			asUINT NativeSlot = 0;
			asUINT RunSlot = 0;
			if (Builder.AddFunctionRequirement(*Native, NativeSlot) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			if (Builder.AddFunctionRequirement(*Run, RunSlot) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			if (Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			asSByteCodeOperand Right = ByteImm64(22);
			if (Builder.Emit(asBC_PshC8, MakeArrayView(&Right, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			asSByteCodeOperand Left = ByteImm64(20);
			if (Builder.Emit(asBC_PshC8, MakeArrayView(&Left, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NativeSlot);
			if (Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
			if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			return Builder.EndFunction();
		};
		LinkExecute(RunI64, AddI64, EmitI64, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(IsTrue(Context->GetReturnQWord() == 42));
		});
		LinkExecute(RunTI64, TypedI64, EmitI64, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(IsTrue(Context->GetReturnQWord() == 42));
		});

		auto EmitF32 = [](asCByteCodeImageBuilder& Builder, asCScriptFunction* Run, asCScriptFunction* Native)
		{
			asUINT NativeSlot = 0;
			asUINT RunSlot = 0;
			if (Builder.AddFunctionRequirement(*Native, NativeSlot) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			if (Builder.AddFunctionRequirement(*Run, RunSlot) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			if (Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			asSByteCodeOperand Right = AbiBits32(AbiFloatBits(0.25f));
			if (Builder.Emit(asBC_PshC4, MakeArrayView(&Right, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			asSByteCodeOperand Left = AbiBits32(AbiFloatBits(0.5f));
			if (Builder.Emit(asBC_PshC4, MakeArrayView(&Left, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NativeSlot);
			if (Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
			if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			return Builder.EndFunction();
		};
		LinkExecute(RunF, AddF, EmitF32, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(IsTrue(Context->GetReturnFloat() == 0.75f));
		});
		LinkExecute(RunTF, TypedF, EmitF32, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(IsTrue(Context->GetReturnFloat() == 0.75f));
		});

		auto EmitF64 = [](asCByteCodeImageBuilder& Builder, asCScriptFunction* Run, asCScriptFunction* Native)
		{
			asUINT NativeSlot = 0;
			asUINT RunSlot = 0;
			if (Builder.AddFunctionRequirement(*Native, NativeSlot) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			if (Builder.AddFunctionRequirement(*Run, RunSlot) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			if (Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			int64 RightBits = 0;
			const double RightValue = 0.25;
			FMemory::Memcpy(&RightBits, &RightValue, sizeof(RightBits));
			asSByteCodeOperand Right = ByteImm64(RightBits);
			if (Builder.Emit(asBC_PshC8, MakeArrayView(&Right, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			int64 LeftBits = 0;
			const double LeftValue = 0.5;
			FMemory::Memcpy(&LeftBits, &LeftValue, sizeof(LeftBits));
			asSByteCodeOperand Left = ByteImm64(LeftBits);
			if (Builder.Emit(asBC_PshC8, MakeArrayView(&Left, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NativeSlot);
			if (Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
			if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			return Builder.EndFunction();
		};
		LinkExecute(RunD, AddD, EmitF64, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(IsTrue(Context->GetReturnDouble() == 0.75));
		});
		LinkExecute(RunTD, TypedD, EmitF64, [&](asIScriptContext* Context)
		{
			ASSERT_THAT(IsTrue(Context->GetReturnDouble() == 0.75));
		});
	}

	TEST_METHOD(InOutInoutAndForwardedReference)
	{
		ResetAbi();
		FDetachedDefinitionFixture Fixture;
		asCDataType RefInt = Fixture.Set->Primitive(ttInt);
		RefInt.MakeReference(true);
		asSMetadataParameter OutP = AbiParam("outValue", RefInt, asTM_OUTREF);
		asSMetadataParameter InOutP = AbiParam("inoutValue", RefInt, asTM_INOUTREF);
		asSMetadataParameter InP = AbiParam("inValue", RefInt, asTM_INREF);
		asCScriptFunction* OutFn = Fixture.DefineFunction(TEXT("FillOut"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), MakeArrayView(&OutP, 1));
		asCScriptFunction* InOutFn = Fixture.DefineFunction(TEXT("Bump"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), MakeArrayView(&InOutP, 1));
		asCScriptFunction* InFn = Fixture.DefineFunction(TEXT("Peek"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), MakeArrayView(&InP, 1));
		asCScriptFunction* FwdFn = Fixture.DefineFunction(TEXT("Forward"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), MakeArrayView(&InOutP, 1));
		asSMetadataParameter RunRef = AbiParam("value", RefInt, asTM_INOUTREF);
		asCScriptFunction* RunOut = Fixture.DefineFunction(TEXT("RunOut"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&RunRef, 1));
		asCScriptFunction* RunInOut = Fixture.DefineFunction(TEXT("RunInOut"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&RunRef, 1));
		asCScriptFunction* RunIn = Fixture.DefineFunction(TEXT("RunIn"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&RunRef, 1));
		asCScriptFunction* RunFwd = Fixture.DefineFunction(TEXT("RunFwd"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&RunRef, 1));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(OutFn->GetStableKey(), asFUNCTION(GenericAbiOut), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(InOutFn->GetStableKey(), asFUNCTION(GenericAbiInOut), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(InFn->GetStableKey(), asFUNCTION(GenericAbiIn), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(FwdFn->GetStableKey(), asFUNCTION(GenericAbiForward), asCALL_GENERIC)));

		auto EmitRefCallReturnLocal = [](asCByteCodeImageBuilder& Builder, asCScriptFunction* Run, asCScriptFunction* Native)
		{
			asUINT NativeSlot = 0;
			asUINT RunSlot = 0;
			if (Builder.AddFunctionRequirement(*Native, NativeSlot) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			if (Builder.AddFunctionRequirement(*Run, RunSlot) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			if (Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			asSByteCodeOperand Self = ByteLocal(0);
			if (Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NativeSlot);
			if (Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			if (Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Self, 1)) != asEByteCodeImageStatus::Succeeded)
			{
				asSByteCodeOperand Read = ByteLocal(0);
				(void)Read;
			}
			asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
			if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded) return asEByteCodeImageStatus::InvalidArgument;
			return Builder.EndFunction();
		};

		auto RunRefCase = [&](asCScriptFunction* Run, asCScriptFunction* Native, int32 Initial, int32 Expected)
		{
			asCByteCodeImageBuilder Builder;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitRefCallReturnLocal(Builder, Run, Native)));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
			int32 Storage = Initial;
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
			ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &Storage)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(Expected, Storage));
			Context->Release();
			Snapshot->Release();
		};

		RunRefCase(RunOut, OutFn, 0, 41);
		RunRefCase(RunInOut, InOutFn, 41, 42);
		RunRefCase(RunIn, InFn, 42, 42);

		{
			asCByteCodeImageBuilder Builder;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitRefCallReturnLocal(Builder, RunFwd, FwdFn)));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
			int32 Storage = 7;
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(RunFwd)));
			ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &Storage)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(IsTrue(GAbiForwarded == &Storage));
			Context->Release();
			Snapshot->Release();
		}
	}

	TEST_METHOD(ValueObjectArgumentAndHandleReturn)
	{
		ResetAbi();
		FDetachedDefinitionFixture Fixture;
		asCObjectType* ValueType = Fixture.DefineType(TEXT("AbiValue"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Set->SetNativeLayout(*ValueType, sizeof(FAbiValue), alignof(FAbiValue))));
		asCObjectType* HandleType = Fixture.DefineType(TEXT("AbiHandle"), TEXT(""), asOBJ_REF | asOBJ_NOCOUNT | asOBJ_APP_CLASS);
		ASSERT_THAT(IsNotNull(HandleType));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Set->SetNativeLayout(*HandleType, sizeof(void*), alignof(void*))));
		asSMetadataParameter ValueParam = AbiParam("value", AbiObjectRef(ValueType), asTM_INOUTREF);
		asCScriptFunction* Write = Fixture.DefineFunction(
			TEXT("Write"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), MakeArrayView(&ValueParam, 1));
		asCDataType HandleReturn = asCDataType::CreateType(HandleType, false);
		ASSERT_THAT(IsTrue(HandleReturn.MakeHandle(true, true) >= 0));
		asCScriptFunction* MakeHandle = Fixture.DefineFunction(
			TEXT("Make"), TEXT(""), asFUNC_SYSTEM, HandleReturn, {});
		ASSERT_THAT(IsNotNull(Write));
		ASSERT_THAT(IsNotNull(MakeHandle));
		asCScriptFunction* RunWrite = Fixture.DefineFunction(
			TEXT("RunWrite"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&ValueParam, 1));
		asCScriptFunction* RunHandle = Fixture.DefineFunction(
			TEXT("RunHandle"), TEXT(""), asFUNC_SCRIPT, HandleReturn, {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(Write->GetStableKey(), asFUNCTION(GenericAbiWriteValue), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(MakeHandle->GetStableKey(), asFUNCTION(GenericAbiReturnHandle), asCALL_GENERIC)));
		{
			asCByteCodeImageBuilder Builder;
			asUINT NativeSlot = 0;
			asUINT RunSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*ValueType, NativeSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Write, NativeSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunWrite, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(RunWrite->GetStableKey(), RunWrite->GetStableKey(), 8, ByteArgumentDwords(*RunWrite))));
			asSByteCodeOperand Self = ByteLocal(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1))));
			asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NativeSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
			asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*RunWrite));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
			FAbiValue Storage;
			FMemory::Memset(&Storage, 0xAA, sizeof(Storage));
			Storage.Value = 1;
			Storage.Guard = 0x5A5A5A5A;
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(RunWrite)));
			ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &Storage)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(42, Storage.Value));
			ASSERT_THAT(AreEqual(0x5A5A5A5A, Storage.Guard));
			Context->Release();
			Snapshot->Release();
		}
		{
			asCByteCodeImageBuilder Builder;
			asUINT NativeSlot = 0;
			asUINT RunSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*HandleType, NativeSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*MakeHandle, NativeSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunHandle, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(RunHandle->GetStableKey(), RunHandle->GetStableKey(), 4, ByteArgumentDwords(*RunHandle))));
			asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NativeSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
			asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*RunHandle));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(RunHandle)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(IsTrue(Context->GetReturnObject() == &GAbiHandleTarget));
			Context->Release();
			Snapshot->Release();
		}
	}

	TEST_METHOD(RejectsMissingCallerUnsupportedNullReceiverAndThrowsOnce)
	{
		ResetAbi();
		FDetachedDefinitionFixture Fixture;
		asSMetadataParameter Params[2] = { AbiParam("a", Fixture.Set->Primitive(ttInt)), AbiParam("b", Fixture.Set->Primitive(ttInt)) };
		asCScriptFunction* Add = Fixture.DefineFunction(TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(Params, 2));
		asCObjectType* Type = Fixture.DefineType(TEXT("AbiReceiver"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Set->SetNativeLayout(*Type, sizeof(FAbiReceiver), alignof(FAbiReceiver))));
		asSMetadataParameter Addend = AbiParam("addend", Fixture.Set->Primitive(ttInt));
		asCScriptFunction* Method = Fixture.DefineFunction(
			TEXT("AddM"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(&Addend, 1), Type);
		asSMetadataParameter Receiver = AbiParam("receiver", AbiObjectRef(Type), asTM_INOUTREF);
		asCScriptFunction* RunMethod = Fixture.DefineFunction(
			TEXT("RunM"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Receiver, 1));
		asCScriptFunction* Fail = Fixture.DefineFunction(TEXT("Fail"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {});
		asCScriptFunction* RunFail = Fixture.DefineFunction(TEXT("RunFail"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::InvalidArgument,
			Engine.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(AbiTypedAddInt), asCALL_CDECL)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::InvalidArgument,
			Engine.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(AbiTypedAddInt), asCALL_THISCALL)));
		ASSERT_THAT(AreEqual(0, GAbiCalls));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Method->GetStableKey(), asFUNCTION(GenericAbiAddToReceiver), asCALL_GENERIC)));
		{
			asCByteCodeImageBuilder Builder;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReceiverCall(Builder, RunMethod, Method, Type, 5)));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(RunMethod)));
			ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, nullptr)));
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
			ASSERT_THAT(AreEqual(0, GAbiCalls));
			Context->Release();
			Snapshot->Release();
		}
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Fail->GetStableKey(), asFUNCTION(GenericAbiThrow), asCALL_GENERIC)));
		{
			asCByteCodeImageBuilder Builder;
			asUINT FailSlot = 0;
			asUINT RunSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Fail, FailSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunFail, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(RunFail->GetStableKey(), RunFail->GetStableKey(), 4, ByteArgumentDwords(*RunFail))));
			asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, FailSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
			asSByteCodeOperand Push = ByteImm32(1);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
			asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*RunFail));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(RunFail)));
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
			ASSERT_THAT(AreEqual(1, GAbiCalls));
			ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Context->GetExceptionString(), "native failed") != nullptr));
			Context->Release();
			Snapshot->Release();
		}
	}
};

#endif
