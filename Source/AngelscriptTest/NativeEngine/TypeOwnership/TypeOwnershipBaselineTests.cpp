#include "CQTest.h"
#include "TypeOwnershipTestSupport.h"
#include "as_datatype.h"
#include "as_scriptfunction.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#include <atomic>
#include <thread>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace TypeOwnershipBaselinePrivate
{
	using namespace TypeOwnershipTest;

	asSDefinitionParameter MakeIntParamLocal(const char* Name, asCDefinitions& Set)
	{
		return MakeIntParam(Name, Set);
	}

	asEByteCodeImageStatus EmitAddTwentyTwentyTwo(asCByteCodeImageBuilder& Builder, asCScriptFunction* Function)
	{
		asUINT Slot = 0;
		asEByteCodeImageStatus Status = Builder.AddFunctionRequirement(*Function, Slot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 16, ByteArgumentDwords(*Function));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand SetLeft[2] = {ByteLocal(1), ByteImm32(PairX)};
		Status = Builder.Emit(asBC_SetV4, MakeArrayView(SetLeft, 2));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand SetRight[2] = {ByteLocal(2), ByteImm32(PairY)};
		Status = Builder.Emit(asBC_SetV4, MakeArrayView(SetRight, 2));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Add[3] = {ByteLocal(3), ByteLocal(1), ByteLocal(2)};
		Status = Builder.Emit(asBC_ADDi, MakeArrayView(Add, 3));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Copy = ByteLocal(3);
		Status = Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Copy, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Function));
		Status = Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		return Builder.EndFunction();
	}

	asEByteCodeImageStatus EmitNativeAddCall(asCByteCodeImageBuilder& Builder, asCScriptFunction* Run, asCScriptFunction* Add)
	{
		asUINT AddSlot = 0;
		asUINT RunSlot = 0;
		asEByteCodeImageStatus Status = Builder.AddFunctionRequirement(*Add, AddSlot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.AddFunctionRequirement(*Run, RunSlot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Right = ByteImm32(PairY);
		Status = Builder.Emit(asBC_PshC4, MakeArrayView(&Right, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Left = ByteImm32(PairX);
		Status = Builder.Emit(asBC_PshC4, MakeArrayView(&Left, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, AddSlot);
		Status = Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		Status = Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		return Builder.EndFunction();
	}

	bool LinkAndPrepare(
		asCScriptEngine* Engine,
		asCByteCodeImageBuilder& Builder,
		asCScriptFunction* Function,
		TUniquePtr<asCByteCodeImage>& Image,
		asCExecutableSnapshot*& Snapshot,
		asIScriptContext*& Context)
	{
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return false;
		if (asLinkByteCodeImage(*Engine, *Image, Snapshot) != asEByteCodeLinkStatus::Succeeded) return false;
		Context = Engine->CreateContext();
		return Context != nullptr && Context->Prepare(Function) == 0;
	}

	int32 ExecutePrepared(asIScriptContext* Context)
	{
		if (Context->Execute() != asEXECUTION_FINISHED) return MIN_int32;
		return static_cast<int32>(Context->GetReturnDWord());
	}

	int32 ExecuteThiscallLocal(
		asCScriptEngine* Engine,
		asCScriptFunction* Run,
		asCScriptFunction* Method,
		FTypeOwnershipPair& Pair)
	{
		asCByteCodeImageBuilder Builder;
		if (EmitThiscallInt(Builder, Run, Method) != asEByteCodeImageStatus::Succeeded) return MIN_int32;
		TUniquePtr<asCByteCodeImage> Image;
		asCExecutableSnapshot* Snapshot = nullptr;
		asIScriptContext* Context = nullptr;
		if (!LinkAndPrepare(Engine, Builder, Run, Image, Snapshot, Context))
		{
			if (Context) Context->Release();
			if (Snapshot) Snapshot->Release();
			return MIN_int32;
		}
		if (Context->SetArgAddress(0, &Pair) != 0)
		{
			Context->Release();
			Snapshot->Release();
			return MIN_int32;
		}
		const int32 Result = ExecutePrepared(Context);
		Context->Release();
		Snapshot->Release();
		return Result;
	}

	FString ConsumerLedgerPath()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()
			/ TEXT("openspec/archive/changes/angelscript/2026-09-11-feature-types-explicit-ownership/attachments/data/consumer-migration.csv"));
	}

	bool CsvContains(const FString& Csv, const TCHAR* Needle)
	{
		return Csv.Contains(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart);
	}

	struct FVmSample
	{
		FString Name;
		int32 Result = 0;
		int32 Operations = 0;
		double Nanoseconds = 0;
		bool bSupported = true;
	};
}

TEST_CLASS_WITH_FLAGS(Baseline, "Angelscript.UnitTest.NativeEngine.TypeOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(Pair)
	{
		using namespace TypeOwnershipBaselinePrivate;
		FDetachedDefinitionFixture Fixture;
		Fixture.Scope = TEXT("type-ownership-pair");
		FPairMembers Pair;
		ASSERT_THAT(IsTrue(AddPairType(Fixture, TEXT("Pair"), Pair, true)));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		ASSERT_THAT(AreEqual(-1, Pair.Type->GetTypeId()));
		ASSERT_THAT(IsNull(Pair.Type->GetEngine()));
		ASSERT_THAT(IsNull(Fixture.Set->GetBoundEngine()));
		ASSERT_THAT(AreEqual(-1, Pair.Sum->GetId()));
		ASSERT_THAT(AreEqual(-1, Pair.GetX->GetId()));
		const char* NameX = nullptr;
		int TypeIdX = 0;
		int OffsetX = -1;
		ASSERT_THAT(AreEqual(0, Pair.Type->GetProperty(0, &NameX, &TypeIdX, nullptr, nullptr, &OffsetX, nullptr, nullptr, nullptr, nullptr)));
		ASSERT_THAT(AreEqual(FString(TEXT("X")), FString(UTF8_TO_TCHAR(NameX))));
		ASSERT_THAT(AreEqual(static_cast<int>(offsetof(FTypeOwnershipPair, X)), OffsetX));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, TypeIdX));
		const char* NameY = nullptr;
		int TypeIdY = 0;
		int OffsetY = -1;
		ASSERT_THAT(AreEqual(0, Pair.Type->GetProperty(1, &NameY, &TypeIdY, nullptr, nullptr, &OffsetY, nullptr, nullptr, nullptr, nullptr)));
		ASSERT_THAT(AreEqual(FString(TEXT("Y")), FString(UTF8_TO_TCHAR(NameY))));
		ASSERT_THAT(AreEqual(static_cast<int>(offsetof(FTypeOwnershipPair, Y)), OffsetY));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, TypeIdY));
		ASSERT_THAT(IsTrue(Pair.Type->GetMethodByName("Sum") == Pair.Sum));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asNOT_SUPPORTED, Engine.Ptr->GetTypeIdByDecl("Pair")));
		ASSERT_THAT(IsNull(Engine.Ptr->GetTypeInfoByDecl("Pair")));
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		const int PairTypeId = Pair.Type->GetTypeId();
		ASSERT_THAT(IsTrue(PairTypeId > asTYPEID_LAST_PRIMITIVE));
		ASSERT_THAT(IsTrue((PairTypeId & asTYPEID_APPOBJECT) != 0));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetTypeInfoById(PairTypeId) == Pair.Type));
		ASSERT_THAT(IsTrue(Pair.Type->GetEngine() == Engine.Ptr));
		ASSERT_THAT(IsNull(Fixture.Set.Get()));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetFunctionById(Pair.Sum->GetId()) == Pair.Sum));
		ASSERT_THAT(AreEqual(0, Pair.Type->GetProperty(0, nullptr, &TypeIdX, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, TypeIdX));
		ASSERT_THAT(AreEqual(0, Pair.Type->GetProperty(1, nullptr, &TypeIdY, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, TypeIdY));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, BindPairMethods(*Engine.Ptr, Pair)));
		FTypeOwnershipPair Host;
		Host.X = PairX;
		Host.Y = PairY;
		ASSERT_THAT(AreEqual(PairSum, ExecuteThiscall(Engine.Ptr, Pair.RunSum, Pair.Sum, Host)));
		ASSERT_THAT(AreEqual(PairX, ExecuteThiscall(Engine.Ptr, Pair.RunGetX, Pair.GetX, Host)));
		ASSERT_THAT(AreEqual(PairY, ExecuteThiscall(Engine.Ptr, Pair.RunGetY, Pair.GetY, Host)));
		ASSERT_THAT(AreEqual(PairOne, ExecuteThiscall(Engine.Ptr, Pair.RunOne, Pair.One, Host)));
		ASSERT_THAT(AreEqual(SemanticFixtureHash(), HashUtf8(SemanticFixtureText())));
	}

	TEST_METHOD(PrivateOwners)
	{
		using namespace TypeOwnershipBaselinePrivate;
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> ContextA =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> ContextB =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		FSourceExecutionInput InputA(TEXT("class ScriptThing { int Value; }"), ContextA, nullptr);
		FSourceExecutionInput InputB(TEXT("class ScriptThing { int Value; }"), ContextB, nullptr);
		TUniquePtr<asCBuilder> BuilderA;
		TUniquePtr<asCBuilder> BuilderB;
		asCObjectType* TypeA = nullptr;
		asCObjectType* TypeB = nullptr;
		ASSERT_THAT(IsTrue(CompileScriptThing(TEXT("type-ownership-a"), InputA, BuilderA, TypeA)));
		ASSERT_THAT(IsTrue(CompileScriptThing(TEXT("type-ownership-b"), InputB, BuilderB, TypeB)));
		ASSERT_THAT(IsTrue(TypeA != TypeB));
		ASSERT_THAT(IsFalse(TypeA->GetStableKey() == TypeB->GetStableKey()));
		ASSERT_THAT(AreEqual(-1, TypeA->GetTypeId()));
		ASSERT_THAT(AreEqual(-1, TypeB->GetTypeId()));
		FNativeEngineOwner EngineA;
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*EngineA.Ptr, *BuilderA)));
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*EngineB.Ptr, *BuilderB)));
		ASSERT_THAT(IsTrue(TypeA->GetEngine() == EngineA.Ptr));
		ASSERT_THAT(IsTrue(TypeB->GetEngine() == EngineB.Ptr));
		ASSERT_THAT(IsTrue(TypeA->GetTypeId() > asTYPEID_LAST_PRIMITIVE));
		ASSERT_THAT(IsTrue(TypeB->GetTypeId() > asTYPEID_LAST_PRIMITIVE));
		ASSERT_THAT(AreEqual(
			asERegistrationResult::InvalidArgument,
			RegisterCompiledDefinitions(*EngineB.Ptr, *BuilderA)));
		ASSERT_THAT(IsTrue(EngineB.Ptr->FindType("", "ScriptThing") == TypeB));
		ASSERT_THAT(IsTrue(EngineA.Ptr->FindType("", "ScriptThing") == TypeA));
		BuilderA.Reset();
		EngineA.Reset();
		ASSERT_THAT(IsTrue(TypeB->GetEngine() == EngineB.Ptr));
		ASSERT_THAT(IsTrue(EngineB.Ptr->FindType("", "ScriptThing") == TypeB));
	}

	TEST_METHOD(ConsumerLedger)
	{
		using namespace TypeOwnershipBaselinePrivate;
		FString Csv;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(Csv, *ConsumerLedgerPath())));
		const TCHAR* Required[] = {
			TEXT("as_typeinfo.cpp"),
			TEXT("as_scriptfunction.cpp"),
			TEXT("as_bytecode_linker.cpp"),
			TEXT("as_context.cpp"),
			TEXT("as_vm_object.cpp"),
			TEXT("as_scriptengine_metadata.cpp"),
			TEXT("AngelscriptTypeBindInfoApply.cpp"),
			TEXT("plainUserData"),
			TEXT("GetBoundEngine"),
			TEXT("GetEngine"),
			TEXT("GetUserData()==0"),
			TEXT("nextMetadataTypeId"),
			TEXT("typeIdSeqNbr"),
			TEXT("EngineRegistrationTests.cpp"),
			TEXT("VMObjectLifetimeTests.cpp"),
			TEXT("RuntimeBindingIsolationTests.cpp"),
			TEXT("Bind_TArray.h"),
			TEXT("4.1"),
			TEXT("2.2"),
			TEXT("3.1"),
			TEXT("active"),
			TEXT("dormant"),
		};
		for (const TCHAR* Needle : Required)
		{
			ASSERT_THAT(IsTrue(CsvContains(Csv, Needle), FString::Printf(TEXT("Ledger missing %s"), Needle)));
		}
		ASSERT_THAT(IsTrue(CsvContains(Csv, TEXT("owning_task"))));
		ASSERT_THAT(IsTrue(CsvContains(Csv, TEXT("disposition"))));
	}

	TEST_METHOD(VMExecution)
	{
		using namespace TypeOwnershipBaselinePrivate;
		TArray<FVmSample> Samples;

		{
			FDetachedDefinitionFixture Fixture;
			asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Add"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			FNativeEngineOwner Engine;
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
			asCByteCodeImageBuilder Builder;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitAddTwentyTwentyTwo(Builder, Function)));
			TUniquePtr<asCByteCodeImage> Image;
			asCExecutableSnapshot* Snapshot = nullptr;
			asIScriptContext* Context = nullptr;
			ASSERT_THAT(IsTrue(LinkAndPrepare(Engine.Ptr, Builder, Function, Image, Snapshot, Context)));
			for (int32 Index = 0; Index < VmWarmupCount; ++Index)
			{
				ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
				ASSERT_THAT(AreEqual(PairSum, ExecutePrepared(Context)));
			}
			FNsTimer Timer;
			int32 Last = 0;
			for (int32 Index = 0; Index < VmTimedCount; ++Index)
			{
				ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
				Last = ExecutePrepared(Context);
			}
			Samples.Add({TEXT("arithmetic"), Last, VmTimedCount, Timer.ElapsedNs(), true});
			ASSERT_THAT(AreEqual(PairSum, Last));
			Context->Release();
			Snapshot->Release();
		}

		{
			FDetachedDefinitionFixture Fixture;
			asSDefinitionParameter Params[2] = {MakeIntParamLocal("a", *Fixture.Set), MakeIntParamLocal("b", *Fixture.Set)};
			asCScriptFunction* Add = Fixture.DefineFunction(TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(Params, 2));
			asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			FNativeEngineOwner Engine;
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
				Engine.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(GenericNativeAdd), asCALL_GENERIC)));
			asCByteCodeImageBuilder Builder;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitNativeAddCall(Builder, Run, Add)));
			TUniquePtr<asCByteCodeImage> Image;
			asCExecutableSnapshot* Snapshot = nullptr;
			asIScriptContext* Context = nullptr;
			ASSERT_THAT(IsTrue(LinkAndPrepare(Engine.Ptr, Builder, Run, Image, Snapshot, Context)));
			FNsTimer Timer;
			int32 Last = 0;
			for (int32 Index = 0; Index < VmTimedCount; ++Index)
			{
				ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
				Last = ExecutePrepared(Context);
			}
			Samples.Add({TEXT("native_call"), Last, VmTimedCount, Timer.ElapsedNs(), true});
			ASSERT_THAT(AreEqual(PairSum, Last));
			Context->Release();
			Snapshot->Release();
		}

		{
			FDetachedDefinitionFixture Fixture;
			asCObjectType* Type = Fixture.DefineType(TEXT("Node"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
			asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			FNativeEngineOwner Engine;
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
			asCByteCodeImageBuilder Builder;
			asUINT TypeSlot = 0;
			asUINT RunSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
				Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
			asSByteCodeOperand Result = ByteLocal(6);
			asSByteCodeOperand One = ByteImm32(PairOne);
			asSByteCodeOperand SetResult[2] = {Result, One};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetResult, 2))));
			asSByteCodeOperand Obj = ByteLocal(2);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
			asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
			asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Result, 1))));
			asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			asCExecutableSnapshot* Snapshot = nullptr;
			asIScriptContext* Context = nullptr;
			ASSERT_THAT(IsTrue(LinkAndPrepare(Engine.Ptr, Builder, Run, Image, Snapshot, Context)));
			FNsTimer Timer;
			int32 Last = 0;
			for (int32 Index = 0; Index < VmTimedCount; ++Index)
			{
				ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
				Last = ExecutePrepared(Context);
			}
			Samples.Add({TEXT("alloc_release"), Last, VmTimedCount, Timer.ElapsedNs(), true});
			ASSERT_THAT(AreEqual(PairOne, Last));
			Context->Release();
			Snapshot->Release();
		}

		{
			FDetachedDefinitionFixture Fixture;
			asCObjectType* Type = Fixture.DefineType(TEXT("HandleNode"), TEXT(""), asOBJ_REF | asOBJ_NOCOUNT);
			asCScriptFunction* Run = Type
				? Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {})
				: nullptr;
			bool bHandleCopySupported = Type && Run && Fixture.FinalizeAndFreeze();
			FNativeEngineOwner Engine;
			asCByteCodeImageBuilder Builder;
			asUINT TypeSlot = 0;
			asUINT RunSlot = 0;
			bHandleCopySupported = bHandleCopySupported
				&& RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set)) == asERegistrationResult::Succeeded
				&& Builder.AddTypeRequirement(*Type, TypeSlot) == asEByteCodeImageStatus::Succeeded
				&& Builder.AddFunctionRequirement(*Run, RunSlot) == asEByteCodeImageStatus::Succeeded
				&& Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run)) == asEByteCodeImageStatus::Succeeded;
			if (bHandleCopySupported)
			{
				asSByteCodeOperand Result = ByteLocal(6);
				asSByteCodeOperand One = ByteImm32(PairOne);
				asSByteCodeOperand SetResult[2] = {Result, One};
				asSByteCodeOperand Obj = ByteLocal(2);
				asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
				asSByteCodeOperand RefType = ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot);
				asSByteCodeOperand Dest = ByteLocal(4);
				asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
				asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
				bHandleCopySupported = Builder.Emit(asBC_SetV4, MakeArrayView(SetResult, 2)) == asEByteCodeImageStatus::Succeeded
					&& Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1)) == asEByteCodeImageStatus::Succeeded
					&& Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2)) == asEByteCodeImageStatus::Succeeded
					&& Builder.Emit(asBC_PshVPtr, MakeArrayView(&Obj, 1)) == asEByteCodeImageStatus::Succeeded
					&& Builder.Emit(asBC_PSF, MakeArrayView(&Dest, 1)) == asEByteCodeImageStatus::Succeeded
					&& Builder.Emit(asBC_REFCPY, MakeArrayView(&RefType, 1)) == asEByteCodeImageStatus::Succeeded
					&& Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2)) == asEByteCodeImageStatus::Succeeded
					&& Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Result, 1)) == asEByteCodeImageStatus::Succeeded
					&& Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) == asEByteCodeImageStatus::Succeeded
					&& Builder.EndFunction() == asEByteCodeImageStatus::Succeeded;
			}
			TUniquePtr<asCByteCodeImage> Image;
			asCExecutableSnapshot* Snapshot = nullptr;
			asIScriptContext* Context = nullptr;
			if (bHandleCopySupported && LinkAndPrepare(Engine.Ptr, Builder, Run, Image, Snapshot, Context))
			{
				FNsTimer Timer;
				int32 Last = 0;
				bool bOk = true;
				for (int32 Index = 0; Index < VmTimedCount && bOk; ++Index)
				{
					bOk = Context->Prepare(Run) == 0;
					Last = ExecutePrepared(Context);
					bOk = bOk && Last == PairOne;
				}
				Samples.Add({TEXT("handle_copy"), Last, VmTimedCount, Timer.ElapsedNs(), bOk});
			}
			else
			{
				Samples.Add({TEXT("handle_copy"), 0, 0, 0, false});
			}
			if (Context) Context->Release();
			if (Snapshot) Snapshot->Release();
		}

		{
			FDetachedDefinitionFixture Fixture;
			asCObjectType* Base = Fixture.DefineType(TEXT("Base"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
			asCObjectType* Derived = Fixture.DefineType(TEXT("Derived"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
			ASSERT_THAT(AreEqual(asEDefinitionResult::Succeeded, Fixture.Set->SetBaseType(*Derived, *Base)));
			asCScriptFunction* Run = Fixture.DefineFunction(TEXT("RunCast"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			FNativeEngineOwner Engine;
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
			asCByteCodeImageBuilder Builder;
			asUINT BaseSlot = 0;
			asUINT DerivedSlot = 0;
			asUINT RunSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Base, BaseSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Derived, DerivedSlot)));
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
			asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, DerivedSlot)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
			asSByteCodeOperand Copy = ByteLocal(6);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Copy, 1))));
			asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
			TUniquePtr<asCByteCodeImage> Image;
			asCExecutableSnapshot* Snapshot = nullptr;
			asIScriptContext* Context = nullptr;
			ASSERT_THAT(IsTrue(LinkAndPrepare(Engine.Ptr, Builder, Run, Image, Snapshot, Context)));
			FNsTimer Timer;
			int32 Last = 0;
			for (int32 Index = 0; Index < VmTimedCount; ++Index)
			{
				ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
				Last = ExecutePrepared(Context);
			}
			Samples.Add({TEXT("cast"), Last, VmTimedCount, Timer.ElapsedNs(), true});
			ASSERT_THAT(IsTrue(Last != 0));
			Context->Release();
			Snapshot->Release();
		}

		Samples.Add({TEXT("template_operations"), 0, 0, 0, false});

		struct FEngineProgram
		{
			FDetachedDefinitionFixture Fixture;
			FNativeEngineOwner Engine;
			asCScriptFunction* Function = nullptr;
			TUniquePtr<asCByteCodeImage> Image;
			asCExecutableSnapshot* Snapshot = nullptr;
			asIScriptContext* Context = nullptr;
		};
		const int32 EngineCounts[] = {1, 2, 8};
		for (int32 EngineCount : EngineCounts)
		{
			TArray<TUniquePtr<FEngineProgram>> Programs;
			for (int32 Index = 0; Index < EngineCount; ++Index)
			{
				TUniquePtr<FEngineProgram> Program = MakeUnique<FEngineProgram>();
				Program->Function = Program->Fixture.DefineFunction(TEXT("Add"), TEXT(""), asFUNC_SCRIPT, Program->Fixture.Set->Primitive(ttInt), {});
				ASSERT_THAT(IsTrue(Program->Fixture.FinalizeAndFreeze()));
				ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, Program->Engine.Ptr->RegisterExternalDefinitions(Program->Fixture.TakeSet())));
				asCByteCodeImageBuilder Builder;
				ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, EmitAddTwentyTwentyTwo(Builder, Program->Function)));
				ASSERT_THAT(IsTrue(LinkAndPrepare(Program->Engine.Ptr, Builder, Program->Function, Program->Image, Program->Snapshot, Program->Context)));
				Programs.Add(MoveTemp(Program));
			}
			TArray<int32> Results;
			Results.SetNum(EngineCount);
			FTestBarrier Barrier(EngineCount);
			TArray<TUniquePtr<std::thread>> Workers;
			ON_SCOPE_EXIT
			{
				for (TUniquePtr<std::thread>& Worker : Workers)
				{
					if (Worker && Worker->joinable()) Worker->join();
				}
				for (TUniquePtr<FEngineProgram>& Program : Programs)
				{
					if (Program->Context) Program->Context->Release();
					if (Program->Snapshot) Program->Snapshot->Release();
					Program->Context = nullptr;
					Program->Snapshot = nullptr;
				}
			};
			std::atomic<int32> Failures{0};
			for (int32 Index = 0; Index < EngineCount; ++Index)
			{
				Workers.Add(MakeUnique<std::thread>([&, Index]()
				{
					Barrier.ArriveAndWait();
					int32 Last = MIN_int32;
					for (int32 Step = 0; Step < VmTimedCount; ++Step)
					{
						if (Programs[Index]->Context->Prepare(Programs[Index]->Function) != 0)
						{
							Failures.fetch_add(1);
							Results[Index] = MIN_int32;
							return;
						}
						Last = ExecutePrepared(Programs[Index]->Context);
					}
					Results[Index] = Last;
				}));
			}
			for (TUniquePtr<std::thread>& Worker : Workers)
			{
				Worker->join();
			}
			Workers.Reset();
			ASSERT_THAT(AreEqual(0, Failures.load()));
			for (int32 Index = 0; Index < EngineCount; ++Index)
			{
				ASSERT_THAT(AreEqual(PairSum, Results[Index]));
			}
			Samples.Add({FString::Printf(TEXT("concurrent_arithmetic_%d"), EngineCount), PairSum, VmTimedCount * EngineCount, 0, true});
		}

		int32 Supported = 0;
		for (const FVmSample& Sample : Samples)
		{
			if (Sample.bSupported) ++Supported;
		}
		ASSERT_THAT(IsTrue(Supported >= 4));
	}

	TEST_METHOD(Cost)
	{
		using namespace TypeOwnershipBaselinePrivate;
		FDetachedDefinitionFixture Fixture;
		Fixture.Scope = TEXT("type-ownership-cost");
		TArray<FPairMembers> Types;
		Types.SetNum(PairTypeCount);
		FNsTimer PrepareTimer;
		for (int32 Index = 0; Index < PairTypeCount; ++Index)
		{
			const FString Name = FString::Printf(TEXT("Pair%03d"), Index);
			ASSERT_THAT(IsTrue(AddPairType(Fixture, *Name, Types[Index], Index == 0)));
		}
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		const double PrepareNs = PrepareTimer.ElapsedNs();
		ASSERT_THAT(AreEqual(PairTypeCount, Fixture.Set->GetTypes().Num()));
		int32 MethodCount = 0;
		for (const FPairMembers& Type : Types)
		{
			ASSERT_THAT(IsNotNull(Type.Sum));
			ASSERT_THAT(IsNotNull(Type.GetX));
			ASSERT_THAT(IsNotNull(Type.GetY));
			ASSERT_THAT(IsNotNull(Type.One));
			++MethodCount;
			++MethodCount;
			++MethodCount;
			++MethodCount;
		}
		ASSERT_THAT(AreEqual(PairTypeCount * MethodsPerType, MethodCount));
		FNativeEngineOwner Engine;
		FNsTimer AttachTimer;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		const double AttachNs = AttachTimer.ElapsedNs();
		FNsTimer QueryTimer;
		int32 QueriedTypes = 0;
		int32 QueriedFunctions = 0;
		for (const FPairMembers& Type : Types)
		{
			ASSERT_THAT(IsTrue(Engine.Ptr->GetTypeInfoById(Type.Type->GetTypeId()) == Type.Type));
			ASSERT_THAT(IsTrue(Engine.Ptr->GetFunctionById(Type.Sum->GetId()) == Type.Sum));
			ASSERT_THAT(IsTrue(Engine.Ptr->GetFunctionById(Type.GetX->GetId()) == Type.GetX));
			ASSERT_THAT(IsTrue(Engine.Ptr->GetFunctionById(Type.GetY->GetId()) == Type.GetY));
			ASSERT_THAT(IsTrue(Engine.Ptr->GetFunctionById(Type.One->GetId()) == Type.One));
			++QueriedTypes;
			QueriedFunctions += MethodsPerType;
		}
		const double QueryNs = QueryTimer.ElapsedNs();
		ASSERT_THAT(AreEqual(PairTypeCount, QueriedTypes));
		ASSERT_THAT(AreEqual(PairTypeCount * MethodsPerType, QueriedFunctions));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, BindPairMethods(*Engine.Ptr, Types[0])));
		FTypeOwnershipPair Host;
		ASSERT_THAT(AreEqual(PairSum, ExecuteThiscall(Engine.Ptr, Types[0].RunSum, Types[0].Sum, Host)));
		ASSERT_THAT(AreEqual(PairX, ExecuteThiscall(Engine.Ptr, Types[0].RunGetX, Types[0].GetX, Host)));
		ASSERT_THAT(AreEqual(PairY, ExecuteThiscall(Engine.Ptr, Types[0].RunGetY, Types[0].GetY, Host)));
		ASSERT_THAT(AreEqual(PairOne, ExecuteThiscall(Engine.Ptr, Types[0].RunOne, Types[0].One, Host)));
		FNsTimer ReleaseTimer;
		Engine.Reset();
		Fixture.Reset();
		const double ReleaseNs = ReleaseTimer.ElapsedNs();
		ASSERT_THAT(IsTrue(PrepareNs >= 0.0));
		ASSERT_THAT(IsTrue(AttachNs >= 0.0));
		ASSERT_THAT(IsTrue(QueryNs >= 0.0));
		ASSERT_THAT(IsTrue(ReleaseNs >= 0.0));
		UE_LOG(LogTemp, Display, TEXT("TypeOwnership.Baseline.Cost fixture=%s prepare_ns=%.0f attach_ns=%.0f query_ns=%.0f release_ns=%.0f"),
			*SemanticFixtureHash(), PrepareNs, AttachNs, QueryNs, ReleaseNs);
	}
};

#endif
