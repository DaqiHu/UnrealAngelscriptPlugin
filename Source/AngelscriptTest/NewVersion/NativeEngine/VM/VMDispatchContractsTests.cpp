#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "frontend/as_type_identity.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GContractNativeCalls = 0;
	int32 GContractDtorCount = 0;

	void ResetContract()
	{
		GContractNativeCalls = 0;
		GContractDtorCount = 0;
	}

	void GenericContractFortyTwo(asIScriptGeneric* Generic)
	{
		++GContractNativeCalls;
		Generic->SetReturnDWord(42);
	}

	void GenericContractDestruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GContractDtorCount;
	}

	asCDataType MakeContractObjectRef(asCObjectType* Type)
	{
		asCDataType DataType = asCDataType::CreateType(Type, false);
		DataType.MakeReference(true);
		return DataType;
	}

	asCObjectType* DefineContractInterface(FDetachedDefinitionFixture& Fixture, const TCHAR* Name)
	{
		if (!Fixture.BindContext()) return nullptr;
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = Fixture.Scope;
		Descriptor.Name = Name;
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Interface;
		asSStableKey Key;
		if (Fixture.Context->GetIdentityRegistry().InternNominalType(Descriptor, Key)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		asCObjectType* Type = nullptr;
		const auto Utf8Name = StringCast<ANSICHAR>(Name);
		if (Fixture.Image->CreateObjectType(Key, Utf8Name.Get(), "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Type)
			!= asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		if (Fixture.Image->SetInterfaceDeclaration(*Type) != asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		return Type;
	}

	void EmitContractReturn(asCByteCodeImageBuilder& Builder, asCScriptFunction* Function, int32 Value)
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
}

TEST_CLASS_WITH_FLAGS(VMDispatchContracts, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(InterfaceFactoryRequiresOwnerAndLeavesCountUnchanged)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* ClassType = Fixture.DefineType(TEXT("Plain"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCObjectType* InterfaceType = DefineContractInterface(Fixture, TEXT("IValue"));
		ASSERT_THAT(IsNotNull(InterfaceType));
		const int32 Before = Fixture.Image->GetFunctions().Num();
		asCScriptFunction* Invalid = Fixture.DefineFunction(
			TEXT("Value"), TEXT(""), asFUNC_INTERFACE, Fixture.Image->Primitive(ttInt), {}, ClassType);
		ASSERT_THAT(IsNull(Invalid));
		ASSERT_THAT(AreEqual(Before, Fixture.Image->GetFunctions().Num()));
		asCScriptFunction* Valid = Fixture.DefineFunction(
			TEXT("Value"), TEXT(""), asFUNC_INTERFACE, Fixture.Image->Primitive(ttInt), {}, InterfaceType);
		ASSERT_THAT(IsNotNull(Valid));
		ASSERT_THAT(AreEqual(static_cast<int32>(asFUNC_INTERFACE), static_cast<int32>(Valid->funcType)));
		ASSERT_THAT(IsTrue(Valid->objectType == InterfaceType));
		ASSERT_THAT(IsNull(Valid->scriptData));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
	}

	TEST_METHOD(InterfaceCallSelectsDerivedWhileBaseCallStaysOne)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* InterfaceType = DefineContractInterface(Fixture, TEXT("IValue"));
		ASSERT_THAT(IsNotNull(InterfaceType));
		asCObjectType* Derived = Fixture.DefineType(TEXT("Derived"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddInterface(*Derived, *InterfaceType)));
		asCScriptFunction* InterfaceValue = Fixture.DefineFunction(
			TEXT("Value"), TEXT(""), asFUNC_INTERFACE, Fixture.Image->Primitive(ttInt), {}, InterfaceType);
		asCScriptFunction* DerivedValue = Fixture.DefineFunction(
			TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {}, Derived);
		asCScriptFunction* RunIntf = Fixture.DefineFunction(TEXT("RunIntf"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* RunBase = Fixture.DefineFunction(TEXT("RunBase"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsNotNull(InterfaceValue));
		InterfaceValue->vfTableIdx = 0;
		DerivedValue->vfTableIdx = 0;
		InterfaceType->virtualFunctionTable.PushLast(InterfaceValue);
		Derived->virtualFunctionTable.PushLast(DerivedValue);
		Derived->interfaceVFTOffsets.PushLast(0);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Fixture.Image))));
		asCByteCodeImageBuilder Builder;
		asUINT DerivedSlot = 0;
		asUINT InterfaceValueSlot = 0;
		asUINT DerivedValueSlot = 0;
		asUINT RunIntfSlot = 0;
		asUINT RunBaseSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Derived, DerivedSlot)));
		EmitContractReturn(Builder, DerivedValue, 2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*InterfaceValue, InterfaceValueSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*DerivedValue, DerivedValueSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunIntf, RunIntfSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunBase, RunBaseSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(RunIntf->GetStableKey(), RunIntf->GetStableKey(), 16, ByteArgumentDwords(*RunIntf))));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, DerivedSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand CallIntf = ByteSlot(asEByteCodeOperandKind::FunctionSlot, InterfaceValueSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLINTF, MakeArrayView(&CallIntf, 1))));
		asSByteCodeOperand Temp = ByteLocal(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV4, MakeArrayView(&Temp, 1))));
		asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, DerivedSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Temp, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*RunIntf));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(RunBase->GetStableKey(), RunBase->GetStableKey(), 16, ByteArgumentDwords(*RunBase))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand CallBase = ByteSlot(asEByteCodeOperandKind::FunctionSlot, DerivedValueSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALL, MakeArrayView(&CallBase, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV4, MakeArrayView(&Temp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Temp, 1))));
		Ret = ByteImm16(ByteArgumentDwords(*RunBase));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(RunIntf)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(RunBase)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(static_cast<int32>(asFUNC_INTERFACE), static_cast<int32>(InterfaceValue->funcType)));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(FuncdefSignatureAcceptsMatchAndRejectsReturnMismatch)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Signature = Fixture.DefineFunction(
			TEXT("Callback"), TEXT(""), asFUNC_CALLABLE_SIGNATURE, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* Match = Fixture.DefineFunction(
			TEXT("Match"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* Mismatch = Fixture.DefineFunction(
			TEXT("Mismatch"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttFloat64), {});
		ASSERT_THAT(IsNotNull(Signature));
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor TypeDescriptor;
		TypeDescriptor.StableScope = Fixture.Scope;
		TypeDescriptor.Name = TEXT("CallbackType");
		TypeDescriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::CallableType;
		asSStableKey TypeKey;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
			Fixture.Context->GetIdentityRegistry().InternNominalType(TypeDescriptor, TypeKey)));
		asCCallableType* CallableType = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->CreateCallableType(TypeKey, "CallbackType", "", *Signature, CallableType)));
		ASSERT_THAT(IsNotNull(CallableType));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		ASSERT_THAT(AreEqual(static_cast<int32>(asFUNC_CALLABLE_SIGNATURE), static_cast<int32>(Signature->funcType)));
		ASSERT_THAT(IsTrue(Match->IsSignatureExceptNameEqual(Signature)));
		ASSERT_THAT(IsFalse(Mismatch->IsSignatureExceptNameEqual(Signature)));
		ASSERT_THAT(IsNull(Signature->scriptData));
	}

	TEST_METHOD(CallBndUnboundAndWrongSignatureRejectBeforeExecute)
	{
		ResetContract();
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Native = Fixture.DefineFunction(
			TEXT("NativeFortyTwo"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Fixture.Image))));
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
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingBinding, Engine.Ptr->LinkByteCodeImage(*Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(AreEqual(0, GContractNativeCalls));
		FDetachedDefinitionFixture IntReturn;
		IntReturn.Scope = TEXT("vm-dispatch-sig");
		asCScriptFunction* Left = IntReturn.DefineFunction(TEXT("Read"), TEXT(""), asFUNC_SCRIPT, IntReturn.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(IntReturn.FinalizeAndFreeze()));
		asCByteCodeImageBuilder SigBuilder;
		asUINT LeftSlot = 0;
		asUINT SigRunSlot = 0;
		asCScriptFunction* SigRun = Left;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SigBuilder.AddFunctionRequirement(*Left, LeftSlot)));
		EmitContractReturn(SigBuilder, Left, 1);
		TUniquePtr<asCByteCodeImage> IntImage;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SigBuilder.Finish(IntImage)));
		FDetachedDefinitionFixture DoubleReturn;
		DoubleReturn.Scope = TEXT("vm-dispatch-sig");
		asCScriptFunction* Right = DoubleReturn.DefineFunction(TEXT("Read"), TEXT(""), asFUNC_SCRIPT, DoubleReturn.Image->Primitive(ttFloat64), {});
		ASSERT_THAT(IsTrue(DoubleReturn.FinalizeAndFreeze()));
		FNativeEngineOwner Dest;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Dest.Ptr->RegisterMetadataImage(MoveTemp(DoubleReturn.Image))));
		asCExecutableSnapshot* Mismatch = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, Dest.Ptr->LinkByteCodeImage(*IntImage, Mismatch)));
		ASSERT_THAT(IsNull(Mismatch));
	}

	TEST_METHOD(CallBndScriptAndNativeIndependentOracles)
	{
		ResetContract();
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Script = Fixture.DefineFunction(TEXT("ScriptFortyOne"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* Native = Fixture.DefineFunction(TEXT("NativeFortyTwo"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* RunScript = Fixture.DefineFunction(TEXT("RunScript"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* RunNative = Fixture.DefineFunction(TEXT("RunNative"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Fixture.Image))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Native->GetStableKey(), asFUNCTION(GenericContractFortyTwo), asCALL_GENERIC)));
		{
			asCByteCodeImageBuilder Builder;
			asUINT ScriptSlot = 0;
			asUINT RunSlot = 0;
			EmitContractReturn(Builder, Script, 41);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Script, ScriptSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunScript, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
				Builder.BeginFunction(RunScript->GetStableKey(), RunScript->GetStableKey(), 4, ByteArgumentDwords(*RunScript))));
			asSByteCodeOperand Bound = ByteSlot(asEByteCodeOperandKind::FunctionSlot, ScriptSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLBND, MakeArrayView(&Bound, 1))));
			asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*RunScript));
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
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
				Builder.BeginFunction(RunNative->GetStableKey(), RunNative->GetStableKey(), 4, ByteArgumentDwords(*RunNative))));
			asSByteCodeOperand Bound = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NativeSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLBND, MakeArrayView(&Bound, 1))));
			asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*RunNative));
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
			ASSERT_THAT(AreEqual(1, GContractNativeCalls));
			Context->Release();
			Snapshot->Release();
		}
	}

	TEST_METHOD(TwoDelegatesReleaseEachReceiverOnce)
	{
		ResetContract();
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Target"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* Dtor = Fixture.DefineFunction(
			TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), {}, Type);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *Dtor)));
		asCScriptFunction* Value = Fixture.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {}, Type);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, MakeContractObjectRef(Type), {});
		asSMetadataParameter Fn;
		Fn.Name = "fn";
		Fn.Type = Fixture.Image->Primitive(ttInt64);
		asCScriptFunction* Invoke = Fixture.DefineFunction(
			TEXT("Invoke"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), MakeArrayView(&Fn, 1));
		asCScriptFunction* Shape = Fixture.DefineFunction(TEXT("DelegateShape"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Fixture.Image))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericContractDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		asUINT InvokeSlot = 0;
		asUINT ShapeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Shape, ShapeSlot)));
		EmitContractReturn(Builder, Value, 42);
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
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		void* First = Context->GetReturnAddress();
		ASSERT_THAT(IsNotNull(First));
		asIScriptFunction* FirstDelegate = Engine.Ptr->CreateDelegate(Value, First);
		ASSERT_THAT(IsNotNull(FirstDelegate));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		void* Second = Context->GetReturnAddress();
		asIScriptFunction* SecondDelegate = Engine.Ptr->CreateDelegate(Value, Second);
		ASSERT_THAT(IsTrue(First != Second));
		ASSERT_THAT(IsTrue(FirstDelegate->GetDelegateObject() == First));
		ASSERT_THAT(IsTrue(SecondDelegate->GetDelegateObject() == Second));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Invoke)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, FirstDelegate)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Engine.Ptr->ReleaseScriptObject(First, Type);
		FirstDelegate->Release();
		ASSERT_THAT(IsTrue(SecondDelegate->GetDelegateObject() == Second));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Invoke)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, SecondDelegate)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Engine.Ptr->ReleaseScriptObject(Second, Type);
		SecondDelegate->Release();
		Context->Release();
		Snapshot->Release();
	}
};

#endif
