#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_vm_object.h"

#include <atomic>
#include <thread>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	asCScriptEngine* GDrainEngine = nullptr;
	int32 GDrainCalls = 0;
	int32 GDrainDtors = 0;
	std::atomic<bool> GDrainEntered{false};
	std::atomic<bool> GDrainRelease{false};
	std::atomic<int32> GDrainPrepareStatus{-999};

	void GenericRequestShutdown(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GDrainCalls;
		if (GDrainEngine)
		{
			GDrainEngine->ShutDownAndRelease();
		}
	}

	void GenericDrainDestruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GDrainDtors;
	}

	void GenericBlockUntilSignaled(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GDrainCalls;
		GDrainEntered.store(true);
		while (!GDrainRelease.load())
		{
			FPlatformProcess::Sleep(0.001f);
		}
	}

	void GenericSuspend(asIScriptGeneric* Generic)
	{
		(void)Generic;
		if (asIScriptContext* Context = asGetActiveContext())
		{
			Context->Suspend();
		}
	}

	asSByteCodeOperand DrainImm32(int32 Value)
	{
		return ByteImm32(Value);
	}

	asSByteCodeOperand DrainLocal(int32 Offset)
	{
		return ByteLocal(Offset);
	}

	bool EmitReturnFortyTwo(asCByteCodeImageBuilder& Builder, asCScriptFunction* Run)
	{
		asUINT RunSlot = 0;
		if (Builder.AddFunctionRequirement(*Run, RunSlot) != asEByteCodeImageStatus::Succeeded) return false;
		if (Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 4, 0) != asEByteCodeImageStatus::Succeeded) return false;
		asSByteCodeOperand Acc = DrainLocal(1);
		asSByteCodeOperand SetOps[2] = {Acc, DrainImm32(42)};
		if (Builder.Emit(asBC_SetV4, MakeArrayView(SetOps, 2)) != asEByteCodeImageStatus::Succeeded) return false;
		if (Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Acc, 1)) != asEByteCodeImageStatus::Succeeded) return false;
		asSByteCodeOperand Ret = ByteImm16(0);
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded) return false;
		return Builder.EndFunction() == asEByteCodeImageStatus::Succeeded;
	}
}

TEST_CLASS_WITH_FLAGS(VMShutdownDrain, "Angelscript.UnitTest.NativeEngine.VM",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(NativeCallbackShutdownReturnsWithoutDeadlock)
	{
		GDrainEngine = nullptr;
		GDrainCalls = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Halt = Fixture.DefineFunction(TEXT("Halt"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		Engine.Ptr->AddRef();
		GDrainEngine = Engine.Ptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Halt->GetStableKey(), asFUNCTION(GenericRequestShutdown), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT HaltSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Halt, HaltSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 4, 0)));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, HaltSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand FortyTwo = ByteImm32(42);
		asSByteCodeOperand Acc = ByteLocal(1);
		asSByteCodeOperand SetOps[2] = {Acc, FortyTwo};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Acc, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, GDrainCalls));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(IsTrue(Context->Prepare(Run) != 0));
		ASSERT_THAT(IsNotNull(Run->GetName()));
		Engine.Ptr->ShutDownAndRelease();
		GDrainEngine = nullptr;
		Context->Release();
		Snapshot->Release();
		Engine.Ptr = nullptr;
	}

	TEST_METHOD(RepeatedShutdownIsIdempotentAndMetadataStaysReadable)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		Engine.Ptr->AddRef();
		Engine.Ptr->ShutDownAndRelease();
		Engine.Ptr->ShutDownAndRelease();
		ASSERT_THAT(IsNotNull(Function->GetName()));
		Engine.Ptr = nullptr;
	}

	TEST_METHOD(ShutdownRejectsNewLinkAndPrepare)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		ASSERT_THAT(IsTrue(EmitReturnFortyTwo(Builder, Run)));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		Engine.Ptr->AddRef();
		Engine.Ptr->ShutDownAndRelease();
		asCExecutableSnapshot* RejectedSnapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Retired, asLinkByteCodeImage(*Engine.Ptr, *Image, RejectedSnapshot)));
		ASSERT_THAT(IsNull(RejectedSnapshot));
		ASSERT_THAT(AreNotEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(IsNotNull(Run->GetName()));
		Engine.Ptr->Release();
		Engine.Ptr = nullptr;
		Context->Release();
		if (Snapshot) Snapshot->Release();
	}

	TEST_METHOD(ShutdownAbortsSuspendedAndReleasesRoot)
	{
		GDrainDtors = 0;
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Hold"), TEXT(""), asOBJ_REF | asOBJ_GC | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* Dtor = Fixture.DefineFunction(TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {}, Type);
		ASSERT_THAT(AreEqual(asEDefinitionResult::Succeeded, Fixture.Set->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *Dtor)));
		asCScriptFunction* Yield = Fixture.DefineFunction(TEXT("Yield"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericDrainDestruct), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Yield->GetStableKey(), asFUNCTION(GenericSuspend), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT YieldSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Yield, YieldSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, 0)));
		asSByteCodeOperand Obj = DrainLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), DrainImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		asSByteCodeFrameDescriptor Frame;
		Frame.LocalDwords = 16;
		Frame.InitializedObjectSlots.Add(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.SetFrameDescriptor(Frame)));
		asSByteCodeCleanupAction Cleanup;
		Cleanup.Kind = asEByteCodeCleanupKind::DestroyObject;
		Cleanup.Path = asEByteCodeCleanupPath::Exception;
		Cleanup.InstructionIndex = 1;
		Cleanup.FrameOffset = 2;
		Cleanup.TypeSlot = TypeSlot;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddCleanupAction(Cleanup)));
		Cleanup.Path = asEByteCodeCleanupPath::Normal;
		Cleanup.InstructionIndex = 3;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddCleanupAction(Cleanup)));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, YieldSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		asSByteCodeOperand Acc = DrainLocal(1);
		asSByteCodeOperand SetOps[2] = {Acc, DrainImm32(1)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Acc, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_SUSPENDED, Context->Execute()));
		ASSERT_THAT(AreEqual(0, GDrainDtors));
		Engine.Ptr->AddRef();
		Engine.Ptr->ShutDownAndRelease();
		ASSERT_THAT(AreEqual(0, Context->Abort()));
		ASSERT_THAT(IsTrue(Context->GetState() == asEXECUTION_ABORTED || Context->GetState() == asEXECUTION_EXCEPTION));
		Engine.Ptr->GarbageCollect();
		ASSERT_THAT(AreEqual(1, GDrainDtors));
		Context->Release();
		if (Snapshot) Snapshot->Release();
		Engine.Ptr->Release();
		Engine.Ptr = nullptr;
	}

	TEST_METHOD(RetainedObjectDelaysDestroyUntilFinalRelease)
	{
		GDrainDtors = 0;
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Hold"), TEXT(""), asOBJ_REF | asOBJ_GC | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* Dtor = Fixture.DefineFunction(TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {}, Type);
		ASSERT_THAT(AreEqual(asEDefinitionResult::Succeeded, Fixture.Set->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *Dtor)));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericDrainDestruct), asCALL_GENERIC)));
		void* Object = asVmAllocateObject(Type);
		ASSERT_THAT(IsNotNull(Object));
		Engine.Ptr->AddRef();
		Engine.Ptr->ShutDownAndRelease();
		ASSERT_THAT(AreEqual(0, GDrainDtors));
		asVmRelease(Object);
		Engine.Ptr->GarbageCollect();
		ASSERT_THAT(AreEqual(1, GDrainDtors));
		Engine.Ptr->Release();
		Engine.Ptr = nullptr;
	}

	TEST_METHOD(ConcurrentPrepareVersusShutdownIsCoherent)
	{
		GDrainPrepareStatus.store(-999);
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		ASSERT_THAT(IsTrue(EmitReturnFortyTwo(Builder, Run)));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asCScriptEngine* Raw = Engine.Ptr;
		Raw->AddRef();
		std::thread Worker([&]()
		{
			asIScriptContext* Context = Raw->CreateContext();
			GDrainPrepareStatus.store(Context->Prepare(Run));
			Context->Release();
		});
		Raw->ShutDownAndRelease();
		Worker.join();
		asIScriptContext* After = Raw->CreateContext();
		ASSERT_THAT(AreNotEqual(0, After->Prepare(Run)));
		After->Release();
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Retired, asLinkByteCodeImage(*Raw, *Image, Failed)));
		if (Snapshot) Snapshot->Release();
		Engine.Ptr->Release();
		Engine.Ptr = nullptr;
	}

	TEST_METHOD(BlockedNativeReleasedBySignalWithoutEarlyComplete)
	{
		GDrainCalls = 0;
		GDrainEntered.store(false);
		GDrainRelease.store(false);
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Block = Fixture.DefineFunction(TEXT("Block"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Block->GetStableKey(), asFUNCTION(GenericBlockUntilSignaled), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT BlockSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Block, BlockSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 4, 0)));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, BlockSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Acc = DrainLocal(1);
		asSByteCodeOperand SetOps[2] = {Acc, DrainImm32(42)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Acc, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asCScriptEngine* Raw = Engine.Ptr;
		Raw->AddRef();
		std::atomic<int> ExecResult{-1};
		std::thread Worker([&]()
		{
			asIScriptContext* Context = Raw->CreateContext();
			Context->Prepare(Run);
			ExecResult.store(Context->Execute());
			Context->Release();
		});
		const double Start = FPlatformTime::Seconds();
		while (!GDrainEntered.load() && FPlatformTime::Seconds() - Start < 5.0)
		{
			FPlatformProcess::Sleep(0.001f);
		}
		ASSERT_THAT(IsTrue(GDrainEntered.load()));
		Raw->ShutDownAndRelease();
		ASSERT_THAT(AreEqual(-1, ExecResult.load()));
		GDrainRelease.store(true);
		Worker.join();
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ExecResult.load()));
		ASSERT_THAT(AreEqual(1, GDrainCalls));
		if (Snapshot) Snapshot->Release();
		Engine.Ptr->Release();
		Engine.Ptr = nullptr;
	}
};

#endif
