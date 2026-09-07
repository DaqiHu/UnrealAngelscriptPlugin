#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace VMResourceOpcodeMatrixPrivate
{
	int32 GNativeCalls = 0;
	int32 GDtorCount = 0;

	struct FThisHost
	{
		int32 Value = 0;
	};

	void ResetTraces()
	{
		GNativeCalls = 0;
		GDtorCount = 0;
	}

	void GenericFortyTwo(asIScriptGeneric* Generic)
	{
		++GNativeCalls;
		Generic->SetReturnDWord(42);
	}

	void GenericThiscallAdd(asIScriptGeneric* Generic)
	{
		FThisHost* Self = static_cast<FThisHost*>(Generic->GetObject());
		const int32 Addend = static_cast<int32>(Generic->GetArgDWord(0));
		Self->Value += Addend;
		Generic->SetReturnDWord(static_cast<asDWORD>(Self->Value));
	}

	void GenericDestruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GDtorCount;
	}

	void BoundLineCallback(asCContext* Context)
	{
		(void)Context;
		++GNativeCalls;
	}

	asSByteCodeOperand Loc(int32 Offset)
	{
		return ByteLocal(Offset);
	}

	asSByteCodeOperand Imm32(int32 Value)
	{
		return ByteImm32(Value);
	}

	asSByteCodeOperand Imm16(int16 Value)
	{
		return ByteImm16(Value);
	}

	asEByteCodeImageStatus Set4(asCByteCodeImageBuilder& Builder, int32 Offset, int32 Value)
	{
		asSByteCodeOperand Ops[2] = {Loc(Offset), Imm32(Value)};
		return Builder.Emit(asBC_SetV4, MakeArrayView(Ops, 2));
	}

	asEByteCodeImageStatus Ret4(asCByteCodeImageBuilder& Builder, int32 Offset)
	{
		asSByteCodeOperand Copy = Loc(Offset);
		const asEByteCodeImageStatus Status = Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Copy, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = Imm16(0);
		return Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
	}

	void EmitConstantReturn(asCByteCodeImageBuilder& Builder, asCScriptFunction* Function, int32 Value)
	{
		asUINT Slot = 0;
		Builder.AddFunctionRequirement(*Function, Slot);
		Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, 0);
		asSByteCodeOperand Acc = Loc(1);
		asSByteCodeOperand Imm = Imm32(Value);
		asSByteCodeOperand SetOps[2] = {Acc, Imm};
		Builder.Emit(asBC_SetV4, MakeArrayView(SetOps, 2));
		Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Acc, 1));
		asSByteCodeOperand Ret = Imm16(0);
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
	}

	asCDataType ObjectRef(asCObjectType* Type)
	{
		asCDataType DataType = asCDataType::CreateType(Type, false);
		DataType.MakeReference(true);
		return DataType;
	}

	asSMetadataParameter IntParam(const char* Name)
	{
		asSMetadataParameter Param;
		Param.Name = Name;
		return Param;
	}
}

TEST_CLASS_WITH_FLAGS(VMResourceOpcodeMatrix, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(RetiredAndReservedReject)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		{
			asCByteCodeImageBuilder Builder;
			asSStableKey Key;
			uint8 Bytes[32] = {};
			Bytes[0] = 0x93;
			asSStableKey::TryCreate(MakeArrayView(Bytes, 32), Key);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Key, Key, 4, 0)));
			asSByteCodeOperand Str = Imm16(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::UnknownOpcode, Builder.Emit(asBC_STR, MakeArrayView(&Str, 1))));
		}
		asSByteCodeOpcodeInfo Info;
		ASSERT_THAT(IsFalse(asByteCodeOpcodeIsExecutable(asBC_STR)));
		ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(asBC_STR, Info)));
		for (int32 Value = 213; Value <= 250; ++Value)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(Value);
			ASSERT_THAT(IsFalse(asByteCodeOpcodeIsExecutable(Opcode)));
			ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(Opcode, Info)));
		}
		ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(asBC_VarDecl, Info)));
		ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(asBC_Block, Info)));
		ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(asBC_ObjInfo, Info)));
		ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(asBC_LINE, Info)));
		ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(asBC_LABEL, Info)));
	}

	TEST_METHOD(NestedCallAndReturn)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Inner = Fixture.DefineFunction(TEXT("Inner"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* Outer = Fixture.DefineFunction(TEXT("Outer"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asCByteCodeImageBuilder Builder;
		asUINT InnerSlot = 0;
		asUINT OuterSlot = 0;
		EmitConstantReturn(Builder, Inner, 41);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Inner, InnerSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Outer, OuterSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Outer->GetStableKey(), Outer->GetStableKey(), 4, 0)));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, InnerSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALL, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Outer)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(41, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(CallSysReturnsFortyTwo)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		ResetTraces();
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Native = Fixture.DefineFunction(TEXT("Native"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Native->GetStableKey(), asFUNCTION(GenericFortyTwo), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT NativeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Native, NativeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 4, 0)));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NativeSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(1, GNativeCalls));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(CallBndScriptAndNative)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		ResetTraces();
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Script = Fixture.DefineFunction(TEXT("Script"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* Native = Fixture.DefineFunction(TEXT("Native"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* RunScript = Fixture.DefineFunction(TEXT("RunScript"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* RunNative = Fixture.DefineFunction(TEXT("RunNative"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Native->GetStableKey(), asFUNCTION(GenericFortyTwo), asCALL_GENERIC)));
		{
			asCByteCodeImageBuilder Builder;
			asUINT ScriptSlot = 0;
			asUINT RunSlot = 0;
			EmitConstantReturn(Builder, Script, 41);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Script, ScriptSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunScript, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(RunScript->GetStableKey(), RunScript->GetStableKey(), 4, 0)));
			asSByteCodeOperand Bound = ByteSlot(asEByteCodeOperandKind::FunctionSlot, ScriptSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLBND, MakeArrayView(&Bound, 1))));
			asSByteCodeOperand Ret = Imm16(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(RunScript)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(41, static_cast<int32>(Context->GetReturnDWord())));
			Context->Release();
			Snapshot->Release();
		}
		{
			asCByteCodeImageBuilder Builder;
			asUINT NativeSlot = 0;
			asUINT RunSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Native, NativeSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunNative, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(RunNative->GetStableKey(), RunNative->GetStableKey(), 4, 0)));
			asSByteCodeOperand Bound = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NativeSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLBND, MakeArrayView(&Bound, 1))));
			asSByteCodeOperand Ret = Imm16(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(RunNative)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
			Context->Release();
			Snapshot->Release();
		}
	}

	TEST_METHOD(FuncPtrCallPtrAndNull)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Target = Fixture.DefineFunction(TEXT("Target"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asCByteCodeImageBuilder Builder;
		asUINT TargetSlot = 0;
		asUINT RunSlot = 0;
		EmitConstantReturn(Builder, Target, 42);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Target, TargetSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, 0)));
		asSByteCodeOperand Func = ByteSlot(asEByteCodeOperandKind::FunctionSlot, TargetSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FuncPtr, MakeArrayView(&Func, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Store = Loc(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV8, MakeArrayView(&Store, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CallPtr, MakeArrayView(&Store, 1))));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(ListCopyTypeIdAndNullChecks)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCObjectType* Type = Fixture.DefineType(TEXT("Box"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asCByteCodeImageBuilder Builder;
		asUINT RunSlot = 0;
		asUINT TypeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 32, 0)));
		asSByteCodeOperand AllocMem[2] = {Loc(8), Imm32(16)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_AllocMem, MakeArrayView(AllocMem, 2))));
		asSByteCodeOperand ListSize[3] = {Loc(8), Imm32(0), Imm32(1)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetListSize, MakeArrayView(ListSize, 3))));
		asSByteCodeOperand ListTypeOps[3] = {Loc(8), Imm32(4), Imm32(1)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetListType, MakeArrayView(ListTypeOps, 3))));
		asSByteCodeOperand Element[2] = {Loc(8), Imm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshListElmnt, MakeArrayView(Element, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Builder, 4, 42)));
		asSByteCodeOperand Write = Loc(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_WRTV4, MakeArrayView(&Write, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshListElmnt, MakeArrayView(Element, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Read = Loc(5);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RDR4, MakeArrayView(&Read, 1))));
		asSByteCodeOperand Track = Loc(8);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_TrackRef, MakeArrayView(&Track, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_UntrackRef, MakeArrayView(&Track, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ValidateRef, MakeArrayView(&Track, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ResolveObjectPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand NullLocal = Loc(12);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ClrVPtr, MakeArrayView(&NullLocal, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FreeNullV8, MakeArrayView(&NullLocal, 1))));
		asSByteCodeOperand CmpNull = Loc(12);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CmpPtrNull, MakeArrayView(&CmpNull, 1))));
		asSByteCodeOperand TypeOp = ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_OBJTYPE, MakeArrayView(&TypeOp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_TYPEID, MakeArrayView(&TypeOp, 1))));
		asSByteCodeOperand Zero = Imm32(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Builder, 5)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(ObjectAllocCopyFreeAndGetRef)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		ResetTraces();
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCObjectProperty* X = Fixture.DefineProperty(*Type, "x", Fixture.Image->Primitive(ttInt));
		asCScriptFunction* Dtor = Fixture.DefineFunction(TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), {}, Type);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *Dtor)));
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 32, 0)));
		asSByteCodeOperand Src = Loc(2);
		asSByteCodeOperand Dst = Loc(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Src, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), Imm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		asSByteCodeOperand TypeOp = ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Src, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FinConstruct, MakeArrayView(&TypeOp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Dst, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Builder, 6, 17)));
		asSByteCodeOperand LoadSrc[2] = {Src, Imm16(static_cast<int16>(X->byteOffset))};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LoadRObjR, MakeArrayView(LoadSrc, 2))));
		asSByteCodeOperand Temp = Loc(6);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_WRTV4, MakeArrayView(&Temp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Src, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Dst, 1))));
		asSByteCodeOperand Size = Imm16(static_cast<int16>(sizeof(int32)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_COPY, MakeArrayView(&Size, 1))));
		asSByteCodeOperand LoadDst[2] = {Dst, Imm16(static_cast<int16>(X->byteOffset))};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LoadRObjR, MakeArrayView(LoadDst, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RDR4, MakeArrayView(&Temp, 1))));
		asSByteCodeOperand FreeSrc[2] = {Src, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeSrc, 2))));
		asSByteCodeOperand FreeDst[2] = {Dst, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_DestructScript, MakeArrayView(FreeDst, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Builder, 6)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(17, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
		{
			FDetachedDefinitionFixture ValueFixture;
			asCScriptFunction* ValueRun = ValueFixture.DefineFunction(TEXT("LoadV"), TEXT(""), asFUNC_SCRIPT, ValueFixture.Image->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(ValueFixture.FinalizeAndFreeze()));
			FNativeEngineOwner ValueEngine;
			ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, ValueEngine.Ptr->RegisterMetadataImage(ValueFixture.Image)));
			asCByteCodeImageBuilder ValueBuilder;
			asUINT ValueSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, ValueBuilder.AddFunctionRequirement(*ValueRun, ValueSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, ValueBuilder.BeginFunction(ValueRun->GetStableKey(), ValueRun->GetStableKey(), 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(ValueBuilder, 2, 17)));
			asSByteCodeOperand LoadV[2] = {Loc(2), Imm16(0)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, ValueBuilder.Emit(asBC_LoadVObjR, MakeArrayView(LoadV, 2))));
			asSByteCodeOperand Out = Loc(4);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, ValueBuilder.Emit(asBC_RDR4, MakeArrayView(&Out, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(ValueBuilder, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, ValueBuilder.EndFunction()));
			TUniquePtr<asCByteCodeImage> ValueImage;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, ValueBuilder.Finish(ValueImage)));
			asCExecutableSnapshot* ValueSnapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, ValueEngine.Ptr->LinkByteCodeImage(*ValueImage, ValueSnapshot)));
			asIScriptContext* ValueContext = ValueEngine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, ValueContext->Prepare(ValueRun)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ValueContext->Execute()));
			ASSERT_THAT(AreEqual(17, static_cast<int32>(ValueContext->GetReturnDWord())));
			ValueContext->Release();
			ValueSnapshot->Release();
		}
	}

	TEST_METHOD(GetObjGetRefChkAndAddSi)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		FDetachedDefinitionFixture Fixture;
		asCGlobalProperty* Global = nullptr;
		{
			if (!Fixture.BindContext()) { ASSERT_THAT(IsTrue(false)); return; }
			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Namespace;
			Namespace.StableScope = Fixture.Scope;
			Namespace.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::Namespace;
			Namespace.Name = TEXT("G");
			AS_NAMESPACE_QUALIFIER asSStableKey Owner;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Fixture.Context->GetIdentityRegistry().InternDeclaration(Namespace, Owner)));
			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Declaration;
			Declaration.StableScope = Fixture.Scope;
			Declaration.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::GlobalVariable;
			Declaration.Name = TEXT("Slot");
			Declaration.Owner = Owner;
			asSStableKey Key;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Fixture.Context->GetIdentityRegistry().InternDeclaration(Declaration, Key)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor TypeUse;
			TypeUse.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
			TypeUse.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32;
			TypeUse.Qualifiers = AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const;
			asSStableKey TypeKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Fixture.Context->GetIdentityRegistry().InternTypeUse(TypeUse, TypeKey)));
			ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
				Fixture.Image->CreateGlobalProperty(Key, TypeKey, "Slot", "G", Fixture.Image->Primitive(ttInt, true), Global)));
		}
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		int32 Storage = 42;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindGlobalStorage(Global->GetStableKey(), &Storage)));
		asCByteCodeImageBuilder Builder;
		asUINT RunSlot = 0;
		asUINT GlobalSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Function, RunSlot)));
		asSByteCodeRequirement Requirement;
		Requirement.Role = asEByteCodeRequirementRole::Global;
		Requirement.Key = Global->GetStableKey();
		Requirement.Name = TEXT("Slot");
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(Requirement, GlobalSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 32, 0)));
		asSByteCodeOperand GlobalOp = ByteSlot(asEByteCodeOperandKind::GlobalSlot, GlobalSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PGA, MakeArrayView(&GlobalOp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Store = Loc(8);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV8, MakeArrayView(&Store, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshNull, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand GetObj[2] = {Imm16(0), Loc(8)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_GETOBJ, MakeArrayView(GetObj, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Read = Loc(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RDR4, MakeArrayView(&Read, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshNull, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand GetRef[2] = {Imm16(0), Loc(4)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_GETREF, MakeArrayView(GetRef, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CHKREF, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Add = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDSi, MakeArrayView(&Add, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand ReadRef = Loc(6);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RDR4, MakeArrayView(&ReadRef, 1))));
		asSByteCodeOperand Sum[3] = {Loc(7), Loc(4), Loc(6)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDi, MakeArrayView(Sum, 3))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Builder, 7)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(84, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(NullChecksThrow)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		auto ExpectNull = [&](const TCHAR* Name, auto EmitBody)
		{
			FDetachedDefinitionFixture Fixture;
			asCScriptFunction* Run = Fixture.DefineFunction(Name, TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			FNativeEngineOwner Engine;
			ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
			asCByteCodeImageBuilder Builder;
			asUINT Slot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, Slot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitBody(Builder)));
			asSByteCodeOperand Ret = Imm16(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
			ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Context->GetExceptionString(), "Null pointer") != nullptr));
			Context->Release();
			Snapshot->Release();
		};
		ExpectNull(TEXT("ChkNullV"), [](asCByteCodeImageBuilder& Builder)
		{
			asSByteCodeOperand Local = Loc(2);
			asEByteCodeImageStatus Status = Builder.Emit(asBC_ClrVPtr, MakeArrayView(&Local, 1));
			if (Status != asEByteCodeImageStatus::Succeeded) return Status;
			return Builder.Emit(asBC_ChkNullV, MakeArrayView(&Local, 1));
		});
		ExpectNull(TEXT("ChkNullS"), [](asCByteCodeImageBuilder& Builder)
		{
			asEByteCodeImageStatus Status = Builder.Emit(asBC_PshNull, TConstArrayView<asSByteCodeOperand>());
			if (Status != asEByteCodeImageStatus::Succeeded) return Status;
			asSByteCodeOperand Off = Imm16(0);
			return Builder.Emit(asBC_ChkNullS, MakeArrayView(&Off, 1));
		});
		ExpectNull(TEXT("ChkRefS"), [](asCByteCodeImageBuilder& Builder)
		{
			asEByteCodeImageStatus Status = Builder.Emit(asBC_PshNull, TConstArrayView<asSByteCodeOperand>());
			if (Status != asEByteCodeImageStatus::Succeeded) return Status;
			return Builder.Emit(asBC_ChkRefS, TConstArrayView<asSByteCodeOperand>());
		});
		ExpectNull(TEXT("CHKREF"), [](asCByteCodeImageBuilder& Builder)
		{
			asEByteCodeImageStatus Status = Builder.Emit(asBC_PshNull, TConstArrayView<asSByteCodeOperand>());
			if (Status != asEByteCodeImageStatus::Succeeded) return Status;
			return Builder.Emit(asBC_CHKREF, TConstArrayView<asSByteCodeOperand>());
		});
	}

	TEST_METHOD(ObserversMarkersSuspendAndThrow)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		ResetTraces();
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* ThrowRun = Fixture.DefineFunction(TEXT("ThrowRun"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		{
			asCByteCodeImageBuilder Builder;
			asUINT Slot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, Slot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JitEntry, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SaveReturnValue, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SUSPEND, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Builder, 1, 42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Builder, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			asCContext* Context = static_cast<asCContext*>(Engine.Ptr->CreateContext());
			ASSERT_THAT(AreEqual(0, Context->SetLineCallback(&BoundLineCallback)));
			ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
			ASSERT_THAT(IsTrue(GNativeCalls > 0));
			Context->Release();
			Snapshot->Release();
		}
		{
			asCByteCodeImageBuilder Builder;
			asUINT Slot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*ThrowRun, Slot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(ThrowRun->GetStableKey(), ThrowRun->GetStableKey(), 4, 0)));
			asSByteCodeOperand Kind = Imm16(1);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ThrowException, MakeArrayView(&Kind, 1))));
			asSByteCodeOperand Ret = Imm16(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			asIScriptContext* Context = Engine.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(ThrowRun)));
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
			ASSERT_THAT(IsNotNull(Context->GetExceptionString()));
			Context->Release();
			Snapshot->Release();
		}
	}

	TEST_METHOD(CastStoreLoadAndCallIntf)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Base = Fixture.DefineType(TEXT("Base"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCObjectType* Derived = Fixture.DefineType(TEXT("Derived"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCObjectType* Other = Fixture.DefineType(TEXT("Other"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->SetBaseType(*Derived, *Base)));
		asCScriptFunction* DerivedValue = Fixture.DefineFunction(
			TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {}, Derived);
		asCScriptFunction* RunCast = Fixture.DefineFunction(TEXT("RunCast"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* RunIntf = Fixture.DefineFunction(TEXT("RunIntf"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		DerivedValue->vfTableIdx = 0;
		Derived->virtualFunctionTable.PushLast(DerivedValue);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asCByteCodeImageBuilder Builder;
		asUINT BaseSlot = 0;
		asUINT DerivedSlot = 0;
		asUINT OtherSlot = 0;
		asUINT DerivedValueSlot = 0;
		asUINT RunCastSlot = 0;
		asUINT RunIntfSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Base, BaseSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Derived, DerivedSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Other, OtherSlot)));
		EmitConstantReturn(Builder, DerivedValue, 2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*DerivedValue, DerivedValueSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunCast, RunCastSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunIntf, RunIntfSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(RunCast->GetStableKey(), RunCast->GetStableKey(), 16, 0)));
		asSByteCodeOperand Obj = Loc(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, DerivedSlot), Imm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand CastBase = ByteSlot(asEByteCodeOperandKind::TypeSlot, BaseSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_Cast, MakeArrayView(&CastBase, 1))));
		asSByteCodeOperand Casted = Loc(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_STOREOBJ, MakeArrayView(&Casted, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LOADOBJ, MakeArrayView(&Casted, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_STOREOBJ, MakeArrayView(&Casted, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Casted, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_TNZ, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Ok = Loc(6);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV4, MakeArrayView(&Ok, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand CastOther = ByteSlot(asEByteCodeOperandKind::TypeSlot, OtherSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_Cast, MakeArrayView(&CastOther, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_STOREOBJ, MakeArrayView(&Casted, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Casted, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_TZ, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Failed = Loc(7);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV4, MakeArrayView(&Failed, 1))));
		asSByteCodeOperand AndOps[3] = {Loc(8), Ok, Failed};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_BAND, MakeArrayView(AndOps, 3))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Builder, 8)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(RunIntf->GetStableKey(), RunIntf->GetStableKey(), 16, 0)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand CallIntf = ByteSlot(asEByteCodeOperandKind::FunctionSlot, DerivedValueSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLINTF, MakeArrayView(&CallIntf, 1))));
		asSByteCodeOperand Temp = Loc(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV4, MakeArrayView(&Temp, 1))));
		asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, DerivedSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Builder, 4)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(RunCast)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(RunIntf)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(Thiscall1AddsSevenToReceiver)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("ThisHost"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->SetNativeLayout(*Type, sizeof(FThisHost), alignof(FThisHost))));
		asSMetadataParameter Addend = IntParam("addend");
		Addend.Type = Fixture.Image->Primitive(ttInt);
		asCScriptFunction* Method = Fixture.DefineFunction(
			TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(&Addend, 1), Type);
		asSMetadataParameter Receiver;
		Receiver.Name = "receiver";
		Receiver.Type = ObjectRef(Type);
		Receiver.Flags = asTM_INOUTREF;
		asCScriptFunction* Run = Fixture.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), MakeArrayView(&Receiver, 1));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Method->GetStableKey(), asFUNCTION(GenericThiscallAdd), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT MethodSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Method, MethodSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 2)));
		asSByteCodeOperand Seven = Imm32(7);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Seven, 1))));
		asSByteCodeOperand Self = Loc(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1))));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, MethodSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_Thiscall1, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		FThisHost Host;
		Host.Value = 5;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &Host)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(12, Host.Value));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(GetObjRefReplacesStackWithHandleThenReadsSeventeen)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		ResetTraces();
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCObjectProperty* X = Fixture.DefineProperty(*Type, "x", Fixture.Image->Primitive(ttInt));
		asCScriptFunction* Dtor = Fixture.DefineFunction(TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), {}, Type);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *Dtor)));
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 32, 0)));
		asSByteCodeOperand Handle = Loc(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Handle, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), Imm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Builder, 6, 17)));
		asSByteCodeOperand Load[2] = {Handle, Imm16(static_cast<int16>(X->byteOffset))};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LoadRObjR, MakeArrayView(Load, 2))));
		asSByteCodeOperand Temp = Loc(6);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_WRTV4, MakeArrayView(&Temp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshNull, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand GetObjRef[2] = {Imm16(0), Handle};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_GETOBJREF, MakeArrayView(GetObjRef, 2))));
		asSByteCodeOperand Field = Imm16(static_cast<int16>(X->byteOffset));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDSi, MakeArrayView(&Field, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Out = Loc(8);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RDR4, MakeArrayView(&Out, 1))));
		asSByteCodeOperand FreeOps[2] = {Handle, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Builder, 8)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(17, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(LoadThisRReadsReceiverFieldSeventeenAndNullThrows)
	{
		using namespace VMResourceOpcodeMatrixPrivate;
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("ThisHost"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->SetNativeLayout(*Type, sizeof(FThisHost), alignof(FThisHost))));
		asCScriptFunction* Read = Fixture.DefineFunction(
			TEXT("Read"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {}, Type);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT ReadSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Read, ReadSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Read->GetStableKey(), Read->GetStableKey(), 8, static_cast<asUINT>(AS_PTR_SIZE))));
		asSByteCodeOperand LoadThis[2] = {Imm16(static_cast<int16>(offsetof(FThisHost, Value))), Imm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LoadThisR, MakeArrayView(LoadThis, 2))));
		asSByteCodeOperand Out = Loc(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RDR4, MakeArrayView(&Out, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Builder, 2)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		FThisHost Host;
		Host.Value = 17;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Read)));
		ASSERT_THAT(AreEqual(0, Context->SetObject(&Host)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(17, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Read)));
		ASSERT_THAT(AreEqual(0, Context->SetObject(nullptr)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		Context->Release();
		Snapshot->Release();
	}
};

#endif
