#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GDispatchDtorCount = 0;

	void GenericFortyTwo(asIScriptGeneric* Generic)
	{
		Generic->SetReturnDWord(42);
	}

	void GenericDispatchDestruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GDispatchDtorCount;
	}

	void EmitConstantReturn(asCByteCodeImageBuilder& Builder, asCScriptFunction* Function, int32 Value)
	{
		asUINT Slot = 0;
		Builder.AddFunctionRequirement(*Function, Slot);
		Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, ByteArgumentDwords(*Function));
		asSByteCodeOperand Push = ByteImm32(Value);
		Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Function));
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
	}

	asCDataType MakeDispatchObjectRef(asCObjectType* Type)
	{
		asCDataType DataType = asCDataType::CreateType(Type, false);
		DataType.MakeReference(true);
		return DataType;
	}
}

TEST_CLASS_WITH_FLAGS(VMDispatch, "Angelscript.UnitTest.NativeEngine.VM",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(MethodFunctionExecutesThroughSnapshot)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		Builder.AddFunctionRequirement(*Function, Slot);
		Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, ByteArgumentDwords(*Function));
		asSByteCodeOperand Push = ByteImm32(2);
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
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(DerivedDispatchThroughBaseChoosesTwo)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Base = Fixture.DefineType(TEXT("Base"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCObjectType* Derived = Fixture.DefineType(TEXT("Derived"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(AreEqual(asEDefinitionResult::Succeeded, Fixture.Set->SetBaseType(*Derived, *Base)));
		asCScriptFunction* BaseValue = Fixture.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {}, Base);
		asCScriptFunction* DerivedValue = Fixture.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {}, Derived);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		BaseValue->vfTableIdx = 0;
		DerivedValue->vfTableIdx = 0;
		Base->virtualFunctionTable.PushLast(BaseValue);
		Derived->virtualFunctionTable.PushLast(DerivedValue);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		const asSStableKey BaseKey = BaseValue->GetStableKey();
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT BaseTypeSlot = 0;
		asUINT DerivedTypeSlot = 0;
		asUINT BaseValueSlot = 0;
		asUINT DerivedValueSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Base, BaseTypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Derived, DerivedTypeSlot)));
		EmitConstantReturn(Builder, BaseValue, 1);
		EmitConstantReturn(Builder, DerivedValue, 2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*BaseValue, BaseValueSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*DerivedValue, DerivedValueSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, DerivedTypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, BaseValueSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLINTF, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Temp = ByteLocal(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV4, MakeArrayView(&Temp, 1))));
		asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, DerivedTypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Temp, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(IsTrue(BaseValue->GetStableKey() == BaseKey));
		ASSERT_THAT(IsTrue(Base->virtualFunctionTable[0] == BaseValue));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(ExplicitBaseCallRemainsOne)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Base = Fixture.DefineType(TEXT("Base"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCObjectType* Derived = Fixture.DefineType(TEXT("Derived"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(AreEqual(asEDefinitionResult::Succeeded, Fixture.Set->SetBaseType(*Derived, *Base)));
		asCScriptFunction* BaseValue = Fixture.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {}, Base);
		asCScriptFunction* DerivedValue = Fixture.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {}, Derived);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		BaseValue->vfTableIdx = 0;
		DerivedValue->vfTableIdx = 0;
		Base->virtualFunctionTable.PushLast(BaseValue);
		Derived->virtualFunctionTable.PushLast(DerivedValue);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT DerivedTypeSlot = 0;
		asUINT BaseValueSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Base, DerivedTypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Derived, DerivedTypeSlot)));
		EmitConstantReturn(Builder, BaseValue, 1);
		EmitConstantReturn(Builder, DerivedValue, 2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*BaseValue, BaseValueSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, DerivedTypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, BaseValueSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALL, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Temp = ByteLocal(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV4, MakeArrayView(&Temp, 1))));
		asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, DerivedTypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Temp, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(FuncPtrCallPtrReturnsFortyTwo)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* FortyTwo = Fixture.DefineFunction(TEXT("FortyTwo"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT FortyTwoSlot = 0;
		asUINT RunSlot = 0;
		EmitConstantReturn(Builder, FortyTwo, 42);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*FortyTwo, FortyTwoSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Func = ByteSlot(asEByteCodeOperandKind::FunctionSlot, FortyTwoSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FuncPtr, MakeArrayView(&Func, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Local = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV8, MakeArrayView(&Local, 1))));
		asSByteCodeOperand Indirect[2] = {Local, Func};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CallPtr, MakeArrayView(Indirect, 2))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(NullCallPtrThrows)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Local = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ClrVPtr, MakeArrayView(&Local, 1))));
		asSByteCodeOperand Indirect[2] = {Local, ByteSlot(asEByteCodeOperandKind::FunctionSlot, RunSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CallPtr, MakeArrayView(Indirect, 2))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(CastDerivedToBaseSucceedsUnrelatedFails)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Base = Fixture.DefineType(TEXT("Base"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCObjectType* Derived = Fixture.DefineType(TEXT("Derived"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCObjectType* Other = Fixture.DefineType(TEXT("Other"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(AreEqual(asEDefinitionResult::Succeeded, Fixture.Set->SetBaseType(*Derived, *Base)));
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT BaseSlot = 0;
		asUINT DerivedSlot = 0;
		asUINT OtherSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Base, BaseSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Derived, DerivedSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Other, OtherSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, DerivedSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand CastBase = ByteSlot(asEByteCodeOperandKind::TypeSlot, BaseSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_Cast, MakeArrayView(&CastBase, 1))));
		asSByteCodeOperand Casted = ByteLocal(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_STOREOBJ, MakeArrayView(&Casted, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Casted, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_TNZ, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Ok = ByteLocal(6);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV4, MakeArrayView(&Ok, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand CastOther = ByteSlot(asEByteCodeOperandKind::TypeSlot, OtherSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_Cast, MakeArrayView(&CastOther, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_STOREOBJ, MakeArrayView(&Casted, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Casted, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_TZ, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Failed = ByteLocal(7);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV4, MakeArrayView(&Failed, 1))));
		asSByteCodeOperand AndOps[3] = {ByteLocal(8), Ok, Failed};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_BAND, MakeArrayView(AndOps, 3))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&AndOps[0], 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(BoundCallAndNativeCallReturnFortyTwo)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Script = Fixture.DefineFunction(TEXT("ScriptFortyTwo"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* Native = Fixture.DefineFunction(TEXT("NativeFortyTwo"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Native->GetStableKey(), asFUNCTION(GenericFortyTwo), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT ScriptSlot = 0;
		asUINT NativeSlot = 0;
		asUINT RunSlot = 0;
		EmitConstantReturn(Builder, Script, 42);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Script, ScriptSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Native, NativeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Bound = ByteSlot(asEByteCodeOperandKind::FunctionSlot, ScriptSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLBND, MakeArrayView(&Bound, 1))));
		asSByteCodeOperand NativeCall = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NativeSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLBND, MakeArrayView(&NativeCall, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(NullCastLeavesNull)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Base = Fixture.DefineType(TEXT("Base"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT BaseSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Base, BaseSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Handle = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ClrVPtr, MakeArrayView(&Handle, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Handle, 1))));
		asSByteCodeOperand CastBase = ByteSlot(asEByteCodeOperandKind::TypeSlot, BaseSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_Cast, MakeArrayView(&CastBase, 1))));
		asSByteCodeOperand Casted = ByteLocal(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_STOREOBJ, MakeArrayView(&Casted, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Casted, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_TZ, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(BoundCallUnboundNativeThrows)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Native = Fixture.DefineFunction(TEXT("MissingNative"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT NativeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Native, NativeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 4, ByteArgumentDwords(*Run))));
		asSByteCodeOperand NativeCall = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NativeSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLBND, MakeArrayView(&NativeCall, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingBinding, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		ASSERT_THAT(IsNull(Snapshot));
	}

	TEST_METHOD(DelegateCaptureReleasesReceiver)
	{
		GDispatchDtorCount = 0;
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Target"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* Dtor = Fixture.DefineFunction(
			TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {}, Type);
		ASSERT_THAT(AreEqual(asEDefinitionResult::Succeeded, Fixture.Set->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *Dtor)));
		asCScriptFunction* Value = Fixture.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {}, Type);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, MakeDispatchObjectRef(Type), {});
		asSDefinitionParameter Fn;
		Fn.Name = "fn";
		Fn.Type = Fixture.Set->Primitive(ttInt64);
		asCScriptFunction* Invoke = Fixture.DefineFunction(
			TEXT("Invoke"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Fn, 1));
		asCScriptFunction* Shape = Fixture.DefineFunction(TEXT("DelegateShape"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericDispatchDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		asUINT InvokeSlot = 0;
		asUINT ShapeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Shape, ShapeSlot)));
		EmitConstantReturn(Builder, Value, 42);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Invoke, InvokeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Obj, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Invoke->GetStableKey(), Invoke->GetStableKey(), 8, ByteArgumentDwords(*Invoke))));
		asSByteCodeOperand FnLocal = ByteLocal(0);
		asSByteCodeOperand Indirect[2] = {FnLocal, ByteSlot(asEByteCodeOperandKind::FunctionSlot, ShapeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CallPtr, MakeArrayView(Indirect, 2))));
		Ret = ByteImm16(ByteArgumentDwords(*Invoke));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		void* First = Context->GetReturnAddress();
		ASSERT_THAT(IsNotNull(First));
		asIScriptFunction* FirstDelegate = Engine.Ptr->CreateDelegate(Value, First);
		ASSERT_THAT(IsNotNull(FirstDelegate));
		ASSERT_THAT(IsTrue(FirstDelegate->GetDelegateObject() == First));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		void* Second = Context->GetReturnAddress();
		ASSERT_THAT(IsNotNull(Second));
		ASSERT_THAT(IsTrue(First != Second));
		asIScriptFunction* SecondDelegate = Engine.Ptr->CreateDelegate(Value, Second);
		ASSERT_THAT(IsNotNull(FirstDelegate));
		ASSERT_THAT(IsNotNull(SecondDelegate));
		ASSERT_THAT(IsTrue(FirstDelegate->GetDelegateObject() == First));
		ASSERT_THAT(IsTrue(SecondDelegate->GetDelegateObject() == Second));
		ASSERT_THAT(IsTrue(FirstDelegate->GetDelegateFunction() == Value));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Invoke)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, FirstDelegate)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, GDispatchDtorCount));
		Engine.Ptr->ReleaseScriptObject(First, Type);
		ASSERT_THAT(AreEqual(0, GDispatchDtorCount));
		FirstDelegate->Release();
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GDispatchDtorCount));
		ASSERT_THAT(IsTrue(SecondDelegate->GetDelegateObject() == Second));
		Engine.Ptr->ReleaseScriptObject(Second, Type);
		SecondDelegate->Release();
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(2, GDispatchDtorCount));
		Context->Release();
		Snapshot->Release();
	}
};

#endif
