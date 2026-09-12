#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_bytecode_verifier.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptfunction.h"
#include "frontend/as_type_identity.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GMixedGcDtorCount = 0;
	int32 GMixedTargetDtorCount = 0;

	struct alignas(8) FMixedNativeRecord
	{
		uint8 Tag;
		int32 Value;
		double Weight;
	};

	void GenericMixedGcDestruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GMixedGcDtorCount;
	}

	void GenericMixedTargetDestruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GMixedTargetDtorCount;
	}

	void GenericMixedSuspend(asIScriptGeneric* Generic)
	{
		(void)Generic;
		if (asIScriptContext* Context = asGetActiveContext())
		{
			Context->Suspend();
		}
	}

	asCDataType MakeMixedObjectRef(asCObjectType* Type)
	{
		asCDataType DataType = asCDataType::CreateType(Type, false);
		DataType.MakeReference(true);
		return DataType;
	}

	asSMetadataParameter MakeMixedRefParam(const char* Name, const asCDataType& Type)
	{
		asSMetadataParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Type;
		Parameter.Flags = asTM_INOUTREF;
		return Parameter;
	}

	asEByteCodeImageStatus EmitMixedConstantReturn(asCByteCodeImageBuilder& Builder, asCScriptFunction* Function, int32 Value)
	{
		asUINT Slot = 0;
		asEByteCodeImageStatus Status = Builder.AddFunctionRequirement(*Function, Slot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, ByteArgumentDwords(*Function));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Push = ByteImm32(Value);
		Status = Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Function));
		Status = Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		return Builder.EndFunction();
	}

	asEByteCodeImageStatus EmitMixedAllocHandleReturn(asCByteCodeImageBuilder& Builder, asCScriptFunction* Function, asUINT TypeSlot)
	{
		asUINT Slot = 0;
		asEByteCodeImageStatus Status = Builder.AddFunctionRequirement(*Function, Slot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 16, ByteArgumentDwords(*Function));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Obj = ByteLocal(2);
		Status = Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		Status = Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.Emit(asBC_PshVPtr, MakeArrayView(&Obj, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>());
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Function));
		Status = Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		return Builder.EndFunction();
	}

	struct FIntegrationThiscallHost
	{
		int32 Value = 0;
	};

	void GenericIntegrationThiscall(asIScriptGeneric* Generic)
	{
		FIntegrationThiscallHost* Self = static_cast<FIntegrationThiscallHost*>(Generic->GetObject());
		const int32 Addend = static_cast<int32>(Generic->GetArgDWord(0));
		Self->Value += Addend;
		Generic->SetReturnDWord(static_cast<asDWORD>(Self->Value));
	}

	asSByteCodeOperand ByteLabel(asUINT Index)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::Label;
		Operand.Label = Index;
		Operand.Immediate = static_cast<int64>(Index);
		return Operand;
	}

	asEByteCodeImageStatus EmitSetLocal(asCByteCodeImageBuilder& Builder, int32 Local, int32 Value)
	{
		asSByteCodeOperand Ops[2] = {ByteLocal(Local), ByteImm32(Value)};
		return Builder.Emit(asBC_SetV4, MakeArrayView(Ops, 2));
	}

	asEByteCodeImageStatus EmitTernary(asCByteCodeImageBuilder& Builder, asEBCInstr Opcode, int32 Dest, int32 Left, int32 Right)
	{
		asSByteCodeOperand Ops[3] = {ByteLocal(Dest), ByteLocal(Left), ByteLocal(Right)};
		return Builder.Emit(Opcode, MakeArrayView(Ops, 3));
	}

	asCGlobalProperty* DefineIntegrationGlobal(FDetachedDefinitionFixture& Fixture, const TCHAR* Name)
	{
		Fixture.Scope = TEXT("vm-integration-global");
		if (!Fixture.BindContext()) return nullptr;
		AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Namespace;
		Namespace.StableScope = Fixture.Scope;
		Namespace.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::Namespace;
		Namespace.Name = TEXT("G");
		AS_NAMESPACE_QUALIFIER asSStableKey Owner;
		if (Fixture.Context->GetIdentityRegistry().InternDeclaration(Namespace, Owner)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Declaration;
		Declaration.StableScope = Fixture.Scope;
		Declaration.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::GlobalVariable;
		Declaration.Name = Name;
		Declaration.Owner = Owner;
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		if (Fixture.Context->GetIdentityRegistry().InternDeclaration(Declaration, Key)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor TypeUse;
		TypeUse.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
		TypeUse.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32;
		TypeUse.Qualifiers = AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const;
		AS_NAMESPACE_QUALIFIER asSStableKey TypeKey;
		if (Fixture.Context->GetIdentityRegistry().InternTypeUse(TypeUse, TypeKey)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		asCGlobalProperty* Global = nullptr;
		if (Fixture.Set->CreateGlobalProperty(
			Key, TypeKey, TCHAR_TO_UTF8(Name), "G", Fixture.Set->Primitive(ttInt, true), Global)
			!= asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		return Global;
	}

	asEByteCodeImageStatus AddGlobalRequirement(asCByteCodeImageBuilder& Builder, asCGlobalProperty& Global, asUINT& OutSlot)
	{
		asSByteCodeRequirement Requirement;
		Requirement.Role = asEByteCodeRequirementRole::Global;
		Requirement.Key = Global.GetStableKey();
		Requirement.Name = UTF8_TO_TCHAR(Global.name.AddressOf());
		return Builder.AddRequirement(Requirement, OutSlot);
	}

	asEByteCodeImageStatus EmitReturnLocal(asCByteCodeImageBuilder& Builder, int32 Local)
	{
		asSByteCodeOperand Copy = ByteLocal(Local);
		asEByteCodeImageStatus Status = Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Copy, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = ByteImm16(0);
		return Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
	}

	asEByteCodeImageStatus EmitMixedThrow(asCByteCodeImageBuilder& Builder, asCScriptFunction* Function)
	{
		asUINT Slot = 0;
		asEByteCodeImageStatus Status = Builder.AddFunctionRequirement(*Function, Slot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, ByteArgumentDwords(*Function));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Kind = ByteImm16(1);
		Status = Builder.Emit(asBC_ThrowException, MakeArrayView(&Kind, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Function));
		Status = Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		return Builder.EndFunction();
	}
}

TEST_CLASS_WITH_FLAGS(VMIntegration, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(MixedImageStillReturnsFortyTwo)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		Builder.AddFunctionRequirement(*Function, Slot);
		Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, ByteArgumentDwords(*Function));
		asSByteCodeOperand Push = ByteImm32(42);
		Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Function));
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
		TUniquePtr<asCByteCodeImage> Image;
		Builder.Finish(Image);
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(MixedNativeDispatchDelegateSuspendCycleAndException)
	{
		GMixedGcDtorCount = 0;
		GMixedTargetDtorCount = 0;

		FDetachedDefinitionFixture Fixture;
		asCObjectType* NativeType = Fixture.DefineType(TEXT("NativeRecord"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		ASSERT_THAT(IsNotNull(NativeType));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Set->SetNativeLayout(*NativeType, sizeof(FMixedNativeRecord), alignof(FMixedNativeRecord))));
		asCObjectProperty* NativeValue = nullptr;
		asSNativePropertyAddress ValueAddress;
		ValueAddress.Mode = asENativeAddressMode::Direct;
		ValueAddress.Offset = static_cast<int>(offsetof(FMixedNativeRecord, Value));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Set->DefineNativeProperty(*NativeType, "Value", Fixture.Set->Primitive(ttInt), ValueAddress, NativeValue)));

		asCObjectType* Base = Fixture.DefineType(TEXT("Base"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCObjectType* Derived = Fixture.DefineType(TEXT("Derived"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Set->SetBaseType(*Derived, *Base)));
		asCScriptFunction* BaseValue = Fixture.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {}, Base);
		asCScriptFunction* DerivedValue = Fixture.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {}, Derived);
		BaseValue->vfTableIdx = 0;
		DerivedValue->vfTableIdx = 0;
		Base->virtualFunctionTable.PushLast(BaseValue);
		Derived->virtualFunctionTable.PushLast(DerivedValue);

		asCObjectType* GcType = Fixture.DefineType(TEXT("GcNode"), TEXT(""), asOBJ_REF | asOBJ_GC | asOBJ_SCRIPT_OBJECT);
		asCObjectProperty* Next = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Set->AddProperty(*GcType, "Next", asCDataType::CreateObjectHandle(GcType, false), Next)));
		asCScriptFunction* GcDtor = Fixture.DefineFunction(TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {}, GcType);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Set->AddBehaviour(*GcType, asBEHAVE_DESTRUCT, *GcDtor)));

		asCObjectType* Target = Fixture.DefineType(TEXT("Target"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* TargetDtor = Fixture.DefineFunction(TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {}, Target);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Set->AddBehaviour(*Target, asBEHAVE_DESTRUCT, *TargetDtor)));
		asCScriptFunction* TargetValue = Fixture.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {}, Target);

		asCScriptFunction* Yield = Fixture.DefineFunction(TEXT("Yield"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {});
		asSMetadataParameter Record = MakeMixedRefParam("record", MakeMixedObjectRef(NativeType));
		asCScriptFunction* NativeWrite = Fixture.DefineFunction(
			TEXT("NativeWrite"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Record, 1));
		asCScriptFunction* DispatchRun = Fixture.DefineFunction(TEXT("DispatchRun"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* CycleRun = Fixture.DefineFunction(TEXT("CycleRun"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* SuspendRun = Fixture.DefineFunction(TEXT("SuspendRun"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* AllocTargetFn = Fixture.DefineFunction(
			TEXT("AllocTarget"), TEXT(""), asFUNC_SCRIPT, MakeMixedObjectRef(Target), {});
		asCScriptFunction* ThrowFn = Fixture.DefineFunction(TEXT("Throw"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));

		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		Engine.Ptr->SetEngineProperty(asEP_AUTO_GARBAGE_COLLECT, 0);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(GcDtor->GetStableKey(), asFUNCTION(GenericMixedGcDestruct), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(TargetDtor->GetStableKey(), asFUNCTION(GenericMixedTargetDestruct), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Yield->GetStableKey(), asFUNCTION(GenericMixedSuspend), asCALL_GENERIC)));

		asCByteCodeImageBuilder Builder;
		asUINT NativeTypeSlot = 0;
		asUINT DerivedTypeSlot = 0;
		asUINT GcTypeSlot = 0;
		asUINT TargetTypeSlot = 0;
		asUINT BaseValueSlot = 0;
		asUINT YieldSlot = 0;
		asUINT NativeWriteSlot = 0;
		asUINT DispatchSlot = 0;
		asUINT CycleSlot = 0;
		asUINT SuspendSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*NativeType, NativeTypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Base, DerivedTypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Derived, DerivedTypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*GcType, GcTypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Target, TargetTypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitMixedConstantReturn(Builder, BaseValue, 1)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitMixedConstantReturn(Builder, DerivedValue, 2)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitMixedConstantReturn(Builder, TargetValue, 42)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitMixedAllocHandleReturn(Builder, AllocTargetFn, TargetTypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitMixedThrow(Builder, ThrowFn)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*BaseValue, BaseValueSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Yield, YieldSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*NativeWrite, NativeWriteSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*DispatchRun, DispatchSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*CycleRun, CycleSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*SuspendRun, SuspendSlot)));

		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(NativeWrite->GetStableKey(), NativeWrite->GetStableKey(), 8, ByteArgumentDwords(*NativeWrite))));
		asSByteCodeOperand Acc = ByteLocal(1);
		asSByteCodeOperand Seven = ByteImm32(7);
		asSByteCodeOperand SetSeven[2] = {Acc, Seven};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetSeven, 2))));
		asSByteCodeOperand Self = ByteLocal(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1))));
		asSByteCodeOperand ValueOff = ByteImm16(static_cast<int16>(NativeValue->byteOffset));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDSi, MakeArrayView(&ValueOff, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_WRTV4, MakeArrayView(&Acc, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDSi, MakeArrayView(&ValueOff, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RDR4, MakeArrayView(&Acc, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Acc, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*NativeWrite));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));

		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(DispatchRun->GetStableKey(), DispatchRun->GetStableKey(), 16, ByteArgumentDwords(*DispatchRun))));
		asSByteCodeOperand DerivedLocal = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&DerivedLocal, 1))));
		asSByteCodeOperand AllocDerived[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, DerivedTypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocDerived, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&DerivedLocal, 1))));
		asSByteCodeOperand CallIntf = ByteSlot(asEByteCodeOperandKind::FunctionSlot, BaseValueSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLINTF, MakeArrayView(&CallIntf, 1))));
		asSByteCodeOperand DispatchTemp = ByteLocal(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV4, MakeArrayView(&DispatchTemp, 1))));
		asSByteCodeOperand FreeDerived[2] = {DerivedLocal, ByteSlot(asEByteCodeOperandKind::TypeSlot, DerivedTypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeDerived, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&DispatchTemp, 1))));
		Ret = ByteImm16(ByteArgumentDwords(*DispatchRun));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));

		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(CycleRun->GetStableKey(), CycleRun->GetStableKey(), 16, ByteArgumentDwords(*CycleRun))));
		asSByteCodeOperand Cycle = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Cycle, 1))));
		asSByteCodeOperand AllocCycle[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, GcTypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocCycle, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Cycle, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Cycle, 1))));
		asSByteCodeOperand NextOff = ByteImm16(static_cast<int16>(Next->byteOffset));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDSi, MakeArrayView(&NextOff, 1))));
		asSByteCodeOperand RefType = ByteSlot(asEByteCodeOperandKind::TypeSlot, GcTypeSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_REFCPY, MakeArrayView(&RefType, 1))));
		asSByteCodeOperand FreeCycle[2] = {Cycle, ByteSlot(asEByteCodeOperandKind::TypeSlot, GcTypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeCycle, 2))));
		asSByteCodeOperand Zero = ByteImm32(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
		Ret = ByteImm16(ByteArgumentDwords(*CycleRun));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		const int32 CycleEnd = static_cast<int32>(Builder.EndFunction());
		ASSERT_THAT(AreEqual(0, CycleEnd, TEXT("CycleEndFunction")));

		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(SuspendRun->GetStableKey(), SuspendRun->GetStableKey(), 8, ByteArgumentDwords(*SuspendRun))));
		asSByteCodeOperand SuspendAcc = ByteLocal(1);
		asSByteCodeOperand One = ByteImm32(1);
		asSByteCodeOperand SetOne[2] = {SuspendAcc, One};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetOne, 2))));
		asSByteCodeOperand YieldCall = ByteSlot(asEByteCodeOperandKind::FunctionSlot, YieldSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&YieldCall, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SUSPEND, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Three = ByteImm32(3);
		asSByteCodeOperand SetThree[2] = {SuspendAcc, Three};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetThree, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&SuspendAcc, 1))));
		Ret = ByteImm16(ByteArgumentDwords(*SuspendRun));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));

		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));

		FMixedNativeRecord RecordValue;
		FMemory::Memset(&RecordValue, 0xAA, sizeof(RecordValue));
		RecordValue.Tag = 3;
		RecordValue.Value = 1;
		RecordValue.Weight = 1.5;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(NativeWrite)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &RecordValue)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(7, RecordValue.Value));
		ASSERT_THAT(AreEqual(static_cast<uint8>(3), RecordValue.Tag));
		ASSERT_THAT(IsTrue(RecordValue.Weight == 1.5));

		ASSERT_THAT(AreEqual(0, Context->Prepare(DispatchRun)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Context->GetReturnDWord())));

		ASSERT_THAT(AreEqual(0, Context->Prepare(CycleRun)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(0, GMixedGcDtorCount));
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GMixedGcDtorCount, TEXT("GcCycleDestructor")));

		ASSERT_THAT(AreEqual(0, Context->Prepare(SuspendRun)));
		ASSERT_THAT(AreEqual(asEXECUTION_SUSPENDED, Context->Execute(), TEXT("SuspendRunSuspended")));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(Context->GetReturnDWord())));

		ASSERT_THAT(AreEqual(0, Context->Prepare(AllocTargetFn)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		void* Receiver = Context->GetReturnAddress();
		ASSERT_THAT(IsNotNull(Receiver));
		asIScriptFunction* Delegate = Engine.Ptr->CreateDelegate(TargetValue, Receiver);
		ASSERT_THAT(IsNotNull(Delegate));
		ASSERT_THAT(IsTrue(Delegate->GetDelegateObject() == Receiver));
		asILockableSharedBool* Weak = Engine.Ptr->GetWeakRefFlagOfScriptObject(Receiver, Target);
		ASSERT_THAT(IsNotNull(Weak));
		ASSERT_THAT(IsFalse(Weak->Get()));
		ASSERT_THAT(AreEqual(0, GMixedTargetDtorCount));
		ASSERT_THAT(AreEqual(0, Context->Unprepare()));
		Engine.Ptr->ReleaseScriptObject(Receiver, Target);
		ASSERT_THAT(AreEqual(0, GMixedTargetDtorCount, TEXT("AfterReleaseScriptObject")));
		Delegate->Release();
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GMixedTargetDtorCount, TEXT("DelegateTargetDestructor")));
		ASSERT_THAT(IsTrue(Weak->Get(), TEXT("WeakInvalidAfterDelegateRelease")));
		Weak->Release();

		ASSERT_THAT(AreEqual(0, Context->Prepare(ThrowFn)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(IsNotNull(Context->GetExceptionString()));
		ASSERT_THAT(AreEqual(0, Context->Prepare(ThrowFn)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));

		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RejectsRetiredReservedAndPseudoOpcodes)
	{
		auto VerifyUnknown = [this](asEBCInstr Opcode)
		{
			asCByteCodeImageBuilder Builder;
			const asSStableKey Function = []()
			{
				uint8 Bytes[32] = {};
				Bytes[0] = 7;
				asSStableKey Key;
				asSStableKey::TryCreate(MakeArrayView(Bytes, 32), Key);
				return Key;
			}();
			Builder.BeginFunction(Function, Function, 4, 0);
			asSByteCodeOperand Push = ByteImm32(1);
			Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1));
			asSByteCodeOperand Ret = ByteImm16(0);
			Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
			Builder.EndFunction();
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
			Image->Functions[0].Instructions[0].Opcode = Opcode;
			ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::UnknownOpcode, asCByteCodeVerifier::Verify(*Image)));
		};
		VerifyUnknown(asBC_STR);
		for (int32 Value = 213; Value <= 250; ++Value)
		{
			VerifyUnknown(static_cast<asEBCInstr>(Value));
		}
		VerifyUnknown(asBC_VarDecl);
		VerifyUnknown(asBC_Block);
		VerifyUnknown(asBC_ObjInfo);
		VerifyUnknown(asBC_LINE);
		VerifyUnknown(asBC_LABEL);
	}

	TEST_METHOD(IntegerWidthShiftCompareAndJumpFamilies)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Function, Slot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 32, ByteArgumentDwords(*Function))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 1, 21)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 2, 1)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_BSLL, 3, 1, 2)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 4, 48)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 5, 6)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_SUBi, 6, 4, 5)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_BAND, 7, 3, 6)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 8, 2)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_DIVi, 9, 3, 8)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_MODi, 10, 3, 8)));
		asSByteCodeOperand Neg = ByteLocal(10);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_NEGi, MakeArrayView(&Neg, 1))));
		asSByteCodeOperand Inc = ByteLocal(7);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_IncVi, MakeArrayView(&Inc, 1))));
		asSByteCodeOperand Cmp[2] = {ByteLocal(7), ByteLocal(3)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CMPi, MakeArrayView(Cmp, 2))));
		const asUINT Skip = Builder.NewLabel();
		const asUINT Done = Builder.NewLabel();
		asSByteCodeOperand Js = ByteLabel(Skip);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JS, MakeArrayView(&Js, 1))));
		asSByteCodeOperand ToDone = ByteLabel(Done);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JMP, MakeArrayView(&ToDone, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(Skip)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 3, 0)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(Done)));
		asSByteCodeOperand Zero = ByteLocal(10);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Zero, 1))));
		const asUINT AfterLow = Builder.NewLabel();
		asSByteCodeOperand JLow = ByteLabel(AfterLow);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JLowNZ, MakeArrayView(&JLow, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(AfterLow)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReturnLocal(Builder, 3)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(FloatDoubleConversionAndPowerFamilies)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		float Left = 1.5f;
		float Right = 2.0f;
		int32 LeftBits = 0;
		int32 RightBits = 0;
		FMemory::Memcpy(&LeftBits, &Left, sizeof(LeftBits));
		FMemory::Memcpy(&RightBits, &Right, sizeof(RightBits));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Function, Slot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 32, ByteArgumentDwords(*Function))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 1, LeftBits)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 2, RightBits)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_ADDf, 3, 1, 2)));
		asSByteCodeOperand Conv[2] = {ByteLocal(4), ByteLocal(3)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_fTOi, MakeArrayView(Conv, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 5, 2)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 6, 3)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_POWi, 7, 5, 6)));
		asSByteCodeOperand Sum[3] = {ByteLocal(8), ByteLocal(4), ByteLocal(7)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDi, MakeArrayView(Sum, 3))));
		asSByteCodeOperand Set8[2] = {ByteLocal(10), ByteImm64(5)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV8, MakeArrayView(Set8, 2))));
		asSByteCodeOperand Narrow[2] = {ByteLocal(11), ByteLocal(10)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_i64TOi, MakeArrayView(Narrow, 2))));
		asSByteCodeOperand Final[3] = {ByteLocal(12), ByteLocal(8), ByteLocal(11)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDi, MakeArrayView(Final, 3))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReturnLocal(Builder, 12)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(16, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(CopyScriptNullThrowsThenListInitReturnsFortyTwo)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* CopyFn = Fixture.DefineFunction(TEXT("CopyNull"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* ListFn = Fixture.DefineFunction(TEXT("ListRun"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* ConstructFn = Fixture.DefineFunction(TEXT("ConstructRun"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCObjectType* Type = Fixture.DefineType(TEXT("CopyHost"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT CopySlot = 0;
		asUINT ListSlot = 0;
		asUINT ConstructSlot = 0;
		asUINT TypeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*CopyFn, CopySlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*ListFn, ListSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*ConstructFn, ConstructSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(CopyFn->GetStableKey(), CopyFn->GetStableKey(), 8, ByteArgumentDwords(*CopyFn))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshNull, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshNull, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand TypeOp = ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CopyScript, MakeArrayView(&TypeOp, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*CopyFn));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(ListFn->GetStableKey(), ListFn->GetStableKey(), 32, ByteArgumentDwords(*ListFn))));
		asSByteCodeOperand AllocMem[2] = {ByteLocal(8), ByteImm32(16)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_AllocMem, MakeArrayView(AllocMem, 2))));
		asSByteCodeOperand ListSize[3] = {ByteLocal(8), ByteImm32(0), ByteImm32(1)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetListSize, MakeArrayView(ListSize, 3))));
		asSByteCodeOperand ListTypeOps[3] = {ByteLocal(8), ByteImm32(4), ByteImm32(1)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetListType, MakeArrayView(ListTypeOps, 3))));
		asSByteCodeOperand Element[2] = {ByteLocal(8), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshListElmnt, MakeArrayView(Element, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 4, 42)));
		asSByteCodeOperand Write = ByteLocal(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_WRTV4, MakeArrayView(&Write, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshListElmnt, MakeArrayView(Element, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Read = ByteLocal(5);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RDR4, MakeArrayView(&Read, 1))));
		asSByteCodeOperand Track = ByteLocal(8);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_TrackRef, MakeArrayView(&Track, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_UntrackRef, MakeArrayView(&Track, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ValidateRef, MakeArrayView(&Track, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ResolveObjectPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand NullLocal = ByteLocal(12);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ClrVPtr, MakeArrayView(&NullLocal, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FreeNullV8, MakeArrayView(&NullLocal, 1))));
		asSByteCodeOperand CmpNull = ByteLocal(12);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CmpPtrNull, MakeArrayView(&CmpNull, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReturnLocal(Builder, 5)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(ConstructFn->GetStableKey(), ConstructFn->GetStableKey(), 16, ByteArgumentDwords(*ConstructFn))));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Obj, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FinConstruct, MakeArrayView(&TypeOp, 1))));
		asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 4, 1)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReturnLocal(Builder, 4)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(CopyFn)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(AreEqual(0, Context->Prepare(ListFn)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(ConstructFn)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(Thiscall1AddsSevenToReceiver)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("ThisHost"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Set->SetNativeLayout(*Type, sizeof(FIntegrationThiscallHost), alignof(FIntegrationThiscallHost))));
		asSMetadataParameter Addend;
		Addend.Name = "addend";
		Addend.Type = Fixture.Set->Primitive(ttInt);
		asCScriptFunction* Method = Fixture.DefineFunction(
			TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(&Addend, 1), Type);
		asCDataType ReceiverType = asCDataType::CreateType(Type, false);
		ReceiverType.MakeReference(true);
		asSMetadataParameter Receiver;
		Receiver.Name = "receiver";
		Receiver.Type = ReceiverType;
		Receiver.Flags = asTM_INOUTREF;
		asCScriptFunction* Run = Fixture.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Receiver, 1));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Method->GetStableKey(), asFUNCTION(GenericIntegrationThiscall), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT MethodSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Method, MethodSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Seven = ByteImm32(7);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Seven, 1))));
		asSByteCodeOperand Self = ByteLocal(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1))));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, MethodSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_Thiscall1, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		FIntegrationThiscallHost Host;
		Host.Value = 5;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &Host)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(12, Host.Value));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(MemoryWidthIncAndSwapFamilies)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Function, Slot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 16, ByteArgumentDwords(*Function))));
		asSByteCodeOperand Set1[2] = {ByteLocal(1), ByteImm32(7)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV1, MakeArrayView(Set1, 2))));
		asSByteCodeOperand Ld = ByteLocal(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LDV, MakeArrayView(&Ld, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_INCi8, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Set2[2] = {ByteLocal(2), ByteImm32(1)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV2, MakeArrayView(Set2, 2))));
		asSByteCodeOperand Copy[2] = {ByteLocal(3), ByteLocal(1)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoV4, MakeArrayView(Copy, 2))));
		asSByteCodeOperand ToR1 = ByteLocal(3);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR1, MakeArrayView(&ToR1, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ClrHi, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand FromR = ByteLocal(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV4, MakeArrayView(&FromR, 1))));
		asSByteCodeOperand Add[3] = {ByteLocal(5), ByteLocal(4), ByteLocal(2)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDi, MakeArrayView(Add, 3))));
		asSByteCodeOperand Psh = ByteLocal(5);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Psh, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshRPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SwapPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReturnLocal(Builder, 5)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(9, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(GlobalStoragePgaPshG4Roundtrip)
	{
		FDetachedDefinitionFixture Fixture;
		asCGlobalProperty* Global = DefineIntegrationGlobal(Fixture, TEXT("Counter"));
		ASSERT_THAT(IsNotNull(Global));
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		int32 Storage = 7;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindGlobalStorage(Global->GetStableKey(), &Storage)));
		asCByteCodeImageBuilder Builder;
		asUINT RunSlot = 0;
		asUINT GlobalSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Function, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, AddGlobalRequirement(Builder, *Global, GlobalSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 16, ByteArgumentDwords(*Function))));
		asSByteCodeOperand GlobalOp = ByteSlot(asEByteCodeOperandKind::GlobalSlot, GlobalSlot);
		asSByteCodeOperand CopyIn[2] = {ByteLocal(1), GlobalOp};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyGtoV4, MakeArrayView(CopyIn, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 2, 35)));
		asSByteCodeOperand CopyOut[2] = {ByteLocal(2), GlobalOp};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoG4, MakeArrayView(CopyOut, 2))));
		asSByteCodeOperand SetG[2] = {GlobalOp, ByteImm32(42)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetG4, MakeArrayView(SetG, 2))));
		asSByteCodeOperand Load[2] = {ByteLocal(3), GlobalOp};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LdGRdR4, MakeArrayView(Load, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshG4, MakeArrayView(&GlobalOp, 1))));
		asSByteCodeOperand Zero = ByteImm32(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PGA, MakeArrayView(&GlobalOp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LDG, MakeArrayView(&GlobalOp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReturnLocal(Builder, 3)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(42, Storage));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(GetObjMovesHandleThenGetRefReadsFortyTwo)
	{
		FDetachedDefinitionFixture Fixture;
		asCGlobalProperty* Global = DefineIntegrationGlobal(Fixture, TEXT("Slot"));
		ASSERT_THAT(IsNotNull(Global));
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		int32 Storage = 42;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindGlobalStorage(Global->GetStableKey(), &Storage)));
		asCByteCodeImageBuilder Builder;
		asUINT RunSlot = 0;
		asUINT GlobalSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Function, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, AddGlobalRequirement(Builder, *Global, GlobalSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 32, ByteArgumentDwords(*Function))));
		asSByteCodeOperand GlobalOp = ByteSlot(asEByteCodeOperandKind::GlobalSlot, GlobalSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PGA, MakeArrayView(&GlobalOp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Store = ByteLocal(8);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV8, MakeArrayView(&Store, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshNull, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand GetObj[2] = {ByteImm16(0), ByteLocal(8)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_GETOBJ, MakeArrayView(GetObj, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Read = ByteLocal(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RDR4, MakeArrayView(&Read, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 5, 0)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshNull, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand GetRef[2] = {ByteImm16(0), ByteLocal(4)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_GETREF, MakeArrayView(GetRef, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand ReadRef = ByteLocal(6);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RDR4, MakeArrayView(&ReadRef, 1))));
		asSByteCodeOperand Sum[3] = {ByteLocal(7), ByteLocal(4), ByteLocal(6)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDi, MakeArrayView(Sum, 3))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReturnLocal(Builder, 7)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(84, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(JmppIndexOneSkipsNextStore)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Function, Slot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 16, ByteArgumentDwords(*Function))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 2, 42)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 1, 1)));
		const asUINT StoreZero = Builder.NewLabel();
		const asUINT Done = Builder.NewLabel();
		asSByteCodeOperand Jmpp[2] = {ByteLocal(1), ByteImm32(1)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JMPP, MakeArrayView(Jmpp, 2))));
		asSByteCodeOperand ToStore;
		ToStore.Kind = asEByteCodeOperandKind::Label; ToStore.Label = StoreZero; ToStore.Immediate = StoreZero;
		asSByteCodeOperand ToDone;
		ToDone.Kind = asEByteCodeOperandKind::Label; ToDone.Label = Done; ToDone.Immediate = Done;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JMP, MakeArrayView(&ToStore, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JMP, MakeArrayView(&ToDone, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(StoreZero)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 2, 0)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(Done)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReturnLocal(Builder, 2)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RemainingWidthShiftFloatJumpAliases)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Function, Slot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 48, ByteArgumentDwords(*Function))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 1, 40)));
		asSByteCodeOperand AddIi[3] = {ByteLocal(2), ByteLocal(1), ByteImm32(2)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDIi, MakeArrayView(AddIi, 3))));
		asSByteCodeOperand Bnot = ByteLocal(3);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 3, 1)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_BNOT, MakeArrayView(&Bnot, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_BNOT, MakeArrayView(&Bnot, 1))));
		asSByteCodeOperand NotOp = ByteLocal(3);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_NOT, MakeArrayView(&NotOp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_BOR, 4, 3, 3)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_BXOR, 5, 4, 3)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 6, 1)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_BSRL, 7, 2, 6)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_BSRA, 8, 2, 6)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 9, 84)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 10, 2)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_DIVu, 11, 9, 10)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitTernary(Builder, asBC_MODu, 12, 9, 10)));
		float Two = 2.0f;
		int32 TwoBits = 0;
		FMemory::Memcpy(&TwoBits, &Two, sizeof(TwoBits));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 13, TwoBits)));
		asSByteCodeOperand NegF = ByteLocal(13);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_NEGf, MakeArrayView(&NegF, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_NEGf, MakeArrayView(&NegF, 1))));
		asSByteCodeOperand Set8[2] = {ByteLocal(16), ByteImm64(3)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV8, MakeArrayView(Set8, 2))));
		asSByteCodeOperand NegD = ByteLocal(16);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_NEGd, MakeArrayView(&NegD, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_NEGd, MakeArrayView(&NegD, 1))));
		asSByteCodeOperand Cmp[2] = {ByteLocal(2), ByteLocal(12)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CMPi, MakeArrayView(Cmp, 2))));
		const asUINT SkipZero = Builder.NewLabel();
		const asUINT Done = Builder.NewLabel();
		asSByteCodeOperand Jp = ByteLabel(SkipZero);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JP, MakeArrayView(&Jp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 2, 0)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(SkipZero)));
		asSByteCodeOperand Jns = ByteLabel(Done);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JNS, MakeArrayView(&Jns, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitSetLocal(Builder, 2, 0)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(Done)));
		asSByteCodeOperand Jnp = ByteLabel(SkipZero);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JNP, MakeArrayView(&Jnp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitReturnLocal(Builder, 2)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}
};

#endif
