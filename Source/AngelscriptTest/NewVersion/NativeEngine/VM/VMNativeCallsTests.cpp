#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_property.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GNativeAddCalls = 0;
	int32 GNativeLastA = 0;
	int32 GNativeLastB = 0;

	void ResetNativeAddTrace()
	{
		GNativeAddCalls = 0;
		GNativeLastA = 0;
		GNativeLastB = 0;
	}

	void GenericAdd(asIScriptGeneric* Generic)
	{
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		++GNativeAddCalls;
		GNativeLastA = A;
		GNativeLastB = B;
		Generic->SetReturnDWord(static_cast<asDWORD>(A + B));
	}

	int NativeAdd(int A, int B)
	{
		++GNativeAddCalls;
		GNativeLastA = A;
		GNativeLastB = B;
		return A + B;
	}

	void CallNativeAdd(asFUNCTION_t Method, void** Parameters, void* ReturnValue)
	{
		using FNativeAdd = int (*)(int, int);
		*static_cast<int*>(ReturnValue) = reinterpret_cast<FNativeAdd>(Method)(
			*static_cast<int*>(Parameters[0]),
			*static_cast<int*>(Parameters[1]));
	}

	struct alignas(8) FVmCallNativeRecord
	{
		uint8 Tag;
		int32 Value;
		double Weight;
	};

	struct FVmCallNativeInner
	{
		int32 Value;
	};

	struct FVmCallNativeOuterIndirect
	{
		FVmCallNativeInner* Inner;
	};

	struct FVmCallNativeReceiver
	{
		int32 Value;
		uint8 Pad[4];
		int32 Sentinel;
	};

	void GenericAddToValue(asIScriptGeneric* Generic)
	{
		FVmCallNativeReceiver* Self = static_cast<FVmCallNativeReceiver*>(Generic->GetObject());
		const int32 Addend = static_cast<int32>(Generic->GetArgDWord(0));
		++GNativeAddCalls;
		Generic->SetReturnDWord(static_cast<asDWORD>(Self->Value + Addend));
	}

	void GenericThrow(asIScriptGeneric* Generic)
	{
		(void)Generic;
		asIScriptContext* Context = asGetActiveContext();
		Context->SetException("native failed");
	}

	void GenericAddInt64(asIScriptGeneric* Generic)
	{
		const int64 A = static_cast<int64>(Generic->GetArgQWord(0));
		const int64 B = static_cast<int64>(Generic->GetArgQWord(1));
		Generic->SetReturnQWord(static_cast<asQWORD>(A + B));
	}

	void GenericAndBool(asIScriptGeneric* Generic)
	{
		const bool A = Generic->GetArgByte(0) != 0;
		const bool B = Generic->GetArgByte(1) != 0;
		Generic->SetReturnByte(static_cast<asBYTE>(A && B));
	}

	void GenericFillOut(asIScriptGeneric* Generic)
	{
		int32* Out = static_cast<int32*>(Generic->GetArgAddress(0));
		*Out = 41;
	}

	asCDataType MakeObjectRef(asCObjectType* Type)
	{
		asCDataType DataType = asCDataType::CreateType(Type, false);
		DataType.MakeReference(true);
		return DataType;
	}

	asSMetadataParameter MakeRefParam(const char* Name, const asCDataType& Type)
	{
		asSMetadataParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Type;
		Parameter.Flags = asTM_INOUTREF;
		return Parameter;
	}

	asSMetadataParameter MakeIntParam(const char* Name)
	{
		asSMetadataParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = asCDataType::CreatePrimitive(ttInt, false);
		return Parameter;
	}
}

TEST_CLASS_WITH_FLAGS(VMNativeCalls, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	asCScriptFunction* DefineIntAdd(FDetachedDefinitionFixture& Fixture)
	{
		asSMetadataParameter Params[2] = {MakeIntParam("a"), MakeIntParam("b")};
		Params[0].Type = Fixture.Image->Primitive(ttInt);
		Params[1].Type = Fixture.Image->Primitive(ttInt);
		return Fixture.DefineFunction(
			TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(Params, 2));
	}

	asCScriptFunction* DefineIntRun(FDetachedDefinitionFixture& Fixture)
	{
		return Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
	}

	void EmitCallNativeAdd(asCByteCodeImageBuilder& Builder, asCScriptFunction* Run, asCScriptFunction* Add, int32 Left, int32 Right)
	{
		asUINT AddSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Add, AddSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 0)));
		asSByteCodeOperand RightOp = ByteImm32(Right);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&RightOp, 1))));
		asSByteCodeOperand LeftOp = ByteImm32(Left);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&LeftOp, 1))));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, AddSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
	}

public:
	TEST_METHOD(BindNativeFunctionRequiresRegisteredDeclaration)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		int Sentinel = 99;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingDeclaration, Engine.Ptr->BindNativeFunction(Function->GetStableKey(), &Sentinel)));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(Function->GetStableKey(), &Sentinel)));
		ASSERT_THAT(AreEqual(99, Sentinel));
	}

	TEST_METHOD(GenericAddTwentyAndTwentyTwoReturnsFortyTwo)
	{
		ResetNativeAddTrace();
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Add = DefineIntAdd(Fixture);
		asCScriptFunction* Run = DefineIntRun(Fixture);
		ASSERT_THAT(IsNotNull(Add));
		ASSERT_THAT(IsNotNull(Run));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(GenericAdd), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		EmitCallNativeAdd(Builder, Run, Add, 20, 22);
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
		ASSERT_THAT(AreEqual(1, GNativeAddCalls));
		ASSERT_THAT(AreEqual(20, GNativeLastA));
		ASSERT_THAT(AreEqual(22, GNativeLastB));
	}

	TEST_METHOD(TypedCallerAddTwentyAndTwentyTwoReturnsFortyTwo)
	{
		ResetNativeAddTrace();
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Add = DefineIntAdd(Fixture);
		asCScriptFunction* Run = DefineIntRun(Fixture);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asFunctionCaller Caller(&CallNativeAdd);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(NativeAdd), asCALL_CDECL, Caller)));
		asCByteCodeImageBuilder Builder;
		EmitCallNativeAdd(Builder, Run, Add, 20, 22);
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
		ASSERT_THAT(AreEqual(1, GNativeAddCalls));
		ASSERT_THAT(AreEqual(20, GNativeLastA));
		ASSERT_THAT(AreEqual(22, GNativeLastB));
	}

	TEST_METHOD(MethodReceiverValueSevenPlusFiveReturnsTwelveWithSentinels)
	{
		ResetNativeAddTrace();
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("NativeReceiver"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->SetNativeLayout(*Type, sizeof(FVmCallNativeReceiver), alignof(FVmCallNativeReceiver))));
		asSMetadataParameter Addend = MakeIntParam("addend");
		Addend.Type = Fixture.Image->Primitive(ttInt);
		asCScriptFunction* Method = Fixture.DefineFunction(
			TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(&Addend, 1), Type);
		asSMetadataParameter Receiver = MakeRefParam("receiver", MakeObjectRef(Type));
		asCScriptFunction* Run = Fixture.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), MakeArrayView(&Receiver, 1));
		ASSERT_THAT(IsNotNull(Method));
		ASSERT_THAT(IsNotNull(Run));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Method->GetStableKey(), asFUNCTION(GenericAddToValue), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT MethodSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Method, MethodSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 2)));
		asSByteCodeOperand Five = ByteImm32(5);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Five, 1))));
		asSByteCodeOperand Self = ByteLocal(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1))));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, MethodSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		FVmCallNativeReceiver ReceiverValue;
		FMemory::Memset(&ReceiverValue, 0xAA, sizeof(ReceiverValue));
		ReceiverValue.Value = 7;
		ReceiverValue.Sentinel = 0x44556677;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &ReceiverValue)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(1, GNativeAddCalls));
		ASSERT_THAT(AreEqual(7, ReceiverValue.Value));
		ASSERT_THAT(AreEqual(0x44556677, ReceiverValue.Sentinel));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0xAA), ReceiverValue.Pad[0]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0xAA), ReceiverValue.Pad[3]));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(BytecodeWritesNativeRecordValueAndPreservesPadding)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("NativeRecord"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->SetNativeLayout(*Type, sizeof(FVmCallNativeRecord), alignof(FVmCallNativeRecord))));
		asCObjectProperty* Tag = nullptr;
		asCObjectProperty* Value = nullptr;
		asCObjectProperty* Weight = nullptr;
		asSNativePropertyAddress TagAddress;
		TagAddress.Mode = asENativeAddressMode::Direct;
		TagAddress.Offset = static_cast<int>(offsetof(FVmCallNativeRecord, Tag));
		asSNativePropertyAddress ValueAddress;
		ValueAddress.Mode = asENativeAddressMode::Direct;
		ValueAddress.Offset = static_cast<int>(offsetof(FVmCallNativeRecord, Value));
		asSNativePropertyAddress WeightAddress;
		WeightAddress.Mode = asENativeAddressMode::Direct;
		WeightAddress.Offset = static_cast<int>(offsetof(FVmCallNativeRecord, Weight));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->DefineNativeProperty(*Type, "Tag", Fixture.Image->Primitive(ttUInt8), TagAddress, Tag)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->DefineNativeProperty(*Type, "Value", Fixture.Image->Primitive(ttInt), ValueAddress, Value)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->DefineNativeProperty(*Type, "Weight", Fixture.Image->Primitive(ttFloat64), WeightAddress, Weight)));
		asSMetadataParameter Record = MakeRefParam("record", MakeObjectRef(Type));
		asCScriptFunction* Run = Fixture.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), MakeArrayView(&Record, 1));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		ASSERT_THAT(AreEqual(static_cast<int>(offsetof(FVmCallNativeRecord, Value)), Value->byteOffset));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asCByteCodeImageBuilder Builder;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 2)));
		asSByteCodeOperand FortyTwo = ByteLocal(1);
		asSByteCodeOperand Immediate = ByteImm32(42);
		asSByteCodeOperand SetOps[2] = {FortyTwo, Immediate};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetOps, 2))));
		asSByteCodeOperand Self = ByteLocal(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1))));
		asSByteCodeOperand Offset = ByteImm16(static_cast<int16>(Value->byteOffset));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDSi, MakeArrayView(&Offset, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_WRTV4, MakeArrayView(&FortyTwo, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDSi, MakeArrayView(&Offset, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RDR4, MakeArrayView(&FortyTwo, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&FortyTwo, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		FVmCallNativeRecord RecordValue;
		FMemory::Memset(&RecordValue, 0xAA, sizeof(RecordValue));
		RecordValue.Tag = 7;
		RecordValue.Value = 1;
		RecordValue.Weight = 1.5;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &RecordValue)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(static_cast<uint8>(7), RecordValue.Tag));
		ASSERT_THAT(AreEqual(42, RecordValue.Value));
		ASSERT_THAT(IsTrue(RecordValue.Weight == 1.5));
		uint8* Bytes = reinterpret_cast<uint8*>(&RecordValue);
		for (int32 Index = 1; Index < static_cast<int32>(offsetof(FVmCallNativeRecord, Value)); ++Index)
		{
			ASSERT_THAT(AreEqual(static_cast<uint8>(0xAA), Bytes[Index]));
		}
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(NullIndirectCompositeThrowsBeforeWrite)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* InnerType = Fixture.DefineType(TEXT("Inner"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		asCObjectType* OuterType = Fixture.DefineType(TEXT("OuterIndirect"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->SetNativeLayout(*InnerType, sizeof(FVmCallNativeInner), alignof(FVmCallNativeInner))));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->SetNativeLayout(*OuterType, sizeof(FVmCallNativeOuterIndirect), alignof(FVmCallNativeOuterIndirect))));
		asCObjectProperty* InnerValue = nullptr;
		asSNativePropertyAddress InnerAddress;
		InnerAddress.Mode = asENativeAddressMode::Direct;
		InnerAddress.Offset = static_cast<int>(offsetof(FVmCallNativeInner, Value));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->DefineNativeProperty(*InnerType, "Value", Fixture.Image->Primitive(ttInt), InnerAddress, InnerValue)));
		asCObjectProperty* OuterValue = nullptr;
		asSNativePropertyAddress OuterAddress;
		OuterAddress.Mode = asENativeAddressMode::CompositeIndirect;
		OuterAddress.Offset = static_cast<int>(offsetof(FVmCallNativeOuterIndirect, Inner));
		OuterAddress.MemberOffset = static_cast<int>(offsetof(FVmCallNativeInner, Value));
		OuterAddress.CompositeStorageType = InnerType;
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->DefineNativeProperty(*OuterType, "Value", Fixture.Image->Primitive(ttInt), OuterAddress, OuterValue)));
		asSMetadataParameter Outer = MakeRefParam("outer", MakeObjectRef(OuterType));
		asCScriptFunction* Run = Fixture.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), MakeArrayView(&Outer, 1));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asCByteCodeImageBuilder Builder;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 2)));
		asSByteCodeOperand Self = ByteLocal(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1))));
		asSByteCodeOperand Composite = ByteImm16(static_cast<int16>(OuterValue->compositeOffset));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDSi, MakeArrayView(&Composite, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RDSPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Member = ByteImm16(static_cast<int16>(OuterValue->byteOffset));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDSi, MakeArrayView(&Member, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		FVmCallNativeOuterIndirect OuterValueStorage;
		OuterValueStorage.Inner = nullptr;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &OuterValueStorage)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(UnsupportedThiscallOnFreeFunctionFailsBeforeCallback)
	{
		ResetNativeAddTrace();
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Add = DefineIntAdd(Fixture);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::InvalidArgument,
			Engine.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(NativeAdd), asCALL_THISCALL)));
		ASSERT_THAT(AreEqual(0, GNativeAddCalls));
	}

	TEST_METHOD(UnboundSystemCallRejectsAtLink)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Add = DefineIntAdd(Fixture);
		asCScriptFunction* Run = DefineIntRun(Fixture);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asCByteCodeImageBuilder Builder;
		EmitCallNativeAdd(Builder, Run, Add, 20, 22);
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingBinding, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		ASSERT_THAT(IsNull(Snapshot));
	}

	TEST_METHOD(GenericInt64BoolAndOutParameter)
	{
		FDetachedDefinitionFixture Fixture;
		asSMetadataParameter Int64Params[2];
		Int64Params[0].Name = "a";
		Int64Params[0].Type = Fixture.Image->Primitive(ttInt64);
		Int64Params[1].Name = "b";
		Int64Params[1].Type = Fixture.Image->Primitive(ttInt64);
		asCScriptFunction* Add64 = Fixture.DefineFunction(
			TEXT("Add64"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt64), MakeArrayView(Int64Params, 2));
		asSMetadataParameter BoolParams[2];
		BoolParams[0].Name = "a";
		BoolParams[0].Type = Fixture.Image->Primitive(ttBool);
		BoolParams[1].Name = "b";
		BoolParams[1].Type = Fixture.Image->Primitive(ttBool);
		asCScriptFunction* AndBool = Fixture.DefineFunction(
			TEXT("AndBool"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttBool), MakeArrayView(BoolParams, 2));
		asCDataType OutInt = Fixture.Image->Primitive(ttInt);
		OutInt.MakeReference(true);
		asSMetadataParameter OutParam = MakeRefParam("outValue", OutInt);
		OutParam.Flags = asTM_OUTREF;
		asCScriptFunction* Fill = Fixture.DefineFunction(
			TEXT("Fill"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), MakeArrayView(&OutParam, 1));
		asCScriptFunction* RunAdd = Fixture.DefineFunction(TEXT("RunAdd"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt64), {});
		asCScriptFunction* RunAnd = Fixture.DefineFunction(TEXT("RunAnd"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttBool), {});
		asCScriptFunction* RunFill = Fixture.DefineFunction(TEXT("RunFill"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Add64->GetStableKey(), asFUNCTION(GenericAddInt64), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(AndBool->GetStableKey(), asFUNCTION(GenericAndBool), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Fill->GetStableKey(), asFUNCTION(GenericFillOut), asCALL_GENERIC)));

		{
			asCByteCodeImageBuilder Builder;
			asUINT AddSlot = 0;
			asUINT RunSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Add64, AddSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunAdd, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
				Builder.BeginFunction(RunAdd->GetStableKey(), RunAdd->GetStableKey(), 8, 0)));
			asSByteCodeOperand Right = ByteImm64(22);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC8, MakeArrayView(&Right, 1))));
			asSByteCodeOperand Left = ByteImm64(20);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC8, MakeArrayView(&Left, 1))));
			asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, AddSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
			asSByteCodeOperand Ret = ByteImm16(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(RunAdd)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(IsTrue(Context->GetReturnQWord() == 42));
			Context->Release();
			Snapshot->Release();
		}

		{
			asCByteCodeImageBuilder Builder;
			asUINT AndSlot = 0;
			asUINT RunSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*AndBool, AndSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunAnd, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
				Builder.BeginFunction(RunAnd->GetStableKey(), RunAnd->GetStableKey(), 8, 0)));
			asSByteCodeOperand Right = ByteImm32(1);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Right, 1))));
			asSByteCodeOperand Left = ByteImm32(1);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Left, 1))));
			asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, AndSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
			asSByteCodeOperand Ret = ByteImm16(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(RunAnd)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnByte())));
			Context->Release();
			Snapshot->Release();
		}

		{
			asCByteCodeImageBuilder Builder;
			asUINT FillSlot = 0;
			asUINT RunSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Fill, FillSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunFill, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
				Builder.BeginFunction(RunFill->GetStableKey(), RunFill->GetStableKey(), 8, 0)));
			asSByteCodeOperand Local = ByteLocal(1);
			asSByteCodeOperand Zero = ByteImm32(0);
			asSByteCodeOperand SetOps[2] = {Local, Zero};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetOps, 2))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Local, 1))));
			asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, FillSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Local, 1))));
			asSByteCodeOperand Ret = ByteImm16(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(RunFill)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(41, static_cast<int32>(Context->GetReturnDWord())));
			Context->Release();
			Snapshot->Release();
		}
	}

	TEST_METHOD(NativeSetExceptionReportsVmException)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Fail = Fixture.DefineFunction(
			TEXT("Fail"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), {});
		asCScriptFunction* Run = DefineIntRun(Fixture);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Fail->GetStableKey(), asFUNCTION(GenericThrow), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT FailSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Fail, FailSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 4, 0)));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, FailSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Push = ByteImm32(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Context->GetExceptionString(), "native failed") != nullptr));
		Context->Release();
		Snapshot->Release();
	}
};

#endif
