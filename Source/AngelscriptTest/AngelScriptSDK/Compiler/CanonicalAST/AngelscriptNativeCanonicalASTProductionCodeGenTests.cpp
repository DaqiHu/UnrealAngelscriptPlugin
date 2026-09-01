#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"
#include "../../Support/AngelscriptNativeCompilerBytecodeTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_traversal.h"
#include "source/as_bytecode_codegen.h"
#include "source/as_decl.h"
#include "source/as_module.h"
#include "source/as_objecttype.h"
#include "source/as_property.h"
#include "source/as_restore.h"
#include "source/as_runtime_type_binding.h"
#include "source/as_runtime_type_bridge.h"
#include "source/as_scriptfunction.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <cstring>
#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

static void ProdCanonicalHandleAddRef(asIScriptGeneric*)
{
}

static void ProdCanonicalHandleRelease(asIScriptGeneric*)
{
}

struct FProdWorldContextHost
{
	int32 Unused = 0;
};

static FProdWorldContextHost GProdWorldContextHost;
static int32 GProdWorldContextInjectCount = 0;

static void ProdInjectWorldContextGeneric(asIScriptGeneric* Generic)
{
	++GProdWorldContextInjectCount;
	if (Generic != nullptr)
	{
		Generic->SetReturnAddress(&GProdWorldContextHost);
	}
}

static void ProdWithWorldGeneric(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}
	void* const Host = Generic->GetArgAddress(0);
	const asDWORD Visible = Generic->GetArgDWord(1);
	if (Host == &GProdWorldContextHost)
	{
		Generic->SetReturnDWord(Visible + 40);
		return;
	}
	Generic->SetReturnDWord(0);
}

struct FProdDefaultGlobalValue
{
	int32 Stored = 0;
};

struct FProdAutomaticImportValue
{
	int32 Stored = 0;
};

static int32 GProdDefaultGlobalConstructed = 0;
static int32 GProdDefaultGlobalDestructed = 0;

static void ProdDefaultGlobalConstruct(asIScriptGeneric* Generic)
{
	if (Generic != nullptr && Generic->GetObject() != nullptr)
	{
		FProdDefaultGlobalValue* const Value =
			new (Generic->GetObject()) FProdDefaultGlobalValue();
		Value->Stored = 42;
		++GProdDefaultGlobalConstructed;
	}
}

static void ProdDefaultGlobalDestruct(asIScriptGeneric* Generic)
{
	if (Generic != nullptr && Generic->GetObject() != nullptr)
	{
		static_cast<FProdDefaultGlobalValue*>(Generic->GetObject())
			->~FProdDefaultGlobalValue();
		++GProdDefaultGlobalDestructed;
	}
}

struct FProdNativeLifecycleMember
{
	int32 Stored = 0;
	int32 Padding[7] = {};
};

static int32 GProdNativeLifecycleMemberConstructed = 0;
static int32 GProdNativeLifecycleMemberDestructed = 0;
static int32 GProdNativeLifecycleMemberCopyConstructed = 0;
static int32 GProdNativeLifecycleMemberCopiedValue = 0;
static int32 GProdNativeLifecycleMemberAssigned = 0;
static int32 GProdNativeLifecycleMemberAssignedValue = 0;
static int32 GProdNativeLifecycleMemberRead = 0;
static int32 GProdNativeLifecycleMemberReferenceObserved = 0;
static int32 GProdSuccessSensitiveInitializerAttempts = 0;
static TArray<int32> GProdSuccessSensitiveDestructionOrder;

struct FProdConstructorCommittedPart
{
	int32 Id = 0;
	int32 Padding[3] = {};
};

static int32 GProdConstructorCommittedAttempts = 0;
static int32 GProdConstructorCommittedFailAt = 0;
static bool GProdConstructorBodyShouldFail = false;
static int32 GProdCompleteObjectDestructorCalls = 0;
static TArray<int32> GProdConstructorCommittedDestructionOrder;

static void ProdConstructorCommittedPartConstruct(asIScriptGeneric* Generic)
{
	if (Generic == nullptr || Generic->GetObject() == nullptr)
	{
		return;
	}
	FProdConstructorCommittedPart* const Value =
		new (Generic->GetObject()) FProdConstructorCommittedPart();
	Value->Id = ++GProdConstructorCommittedAttempts;
	if (GProdConstructorCommittedFailAt == Value->Id)
	{
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			Context->SetException("constructor committed-prefix member failure");
		}
	}
}

static void ProdConstructorCommittedPartDestruct(asIScriptGeneric* Generic)
{
	if (Generic == nullptr || Generic->GetObject() == nullptr)
	{
		return;
	}
	FProdConstructorCommittedPart* const Value =
		static_cast<FProdConstructorCommittedPart*>(Generic->GetObject());
	GProdConstructorCommittedDestructionOrder.Add(Value->Id);
	Value->~FProdConstructorCommittedPart();
}

static void ProdMaybeFailConstructorBody(asIScriptGeneric*)
{
	if (GProdConstructorBodyShouldFail)
	{
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			Context->SetException("constructor committed-prefix body failure");
		}
	}
}

static void ProdRecordCompleteObjectDestructor(asIScriptGeneric*)
{
	++GProdCompleteObjectDestructorCalls;
}

static void ProdSuccessSensitiveLifetimeInitializer(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}
	++GProdSuccessSensitiveInitializerAttempts;
	if (Generic->GetArgByte(0) != 0)
	{
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			Context->SetException("success-sensitive lifetime initializer");
		}
	}
}

static void ProdRecordSuccessSensitiveDestructor(asIScriptGeneric* Generic)
{
	if (Generic != nullptr)
	{
		GProdSuccessSensitiveDestructionOrder.Add(
			static_cast<int32>(Generic->GetArgDWord(0)));
	}
}

static void ProdResetSuccessSensitiveDestructorTrace(asIScriptGeneric*)
{
	GProdSuccessSensitiveDestructionOrder.Reset();
}

static void ProdNativeLifecycleMemberConstruct(asIScriptGeneric* Generic)
{
	if (Generic != nullptr && Generic->GetObject() != nullptr)
	{
		FProdNativeLifecycleMember* const Value =
			new (Generic->GetObject()) FProdNativeLifecycleMember();
		Value->Stored = GProdNativeLifecycleMemberConstructed == 0 ? 41 : 0;
		++GProdNativeLifecycleMemberConstructed;
	}
}

static void ProdNativeLifecycleMemberDestruct(asIScriptGeneric* Generic)
{
	if (Generic != nullptr && Generic->GetObject() != nullptr)
	{
		static_cast<FProdNativeLifecycleMember*>(Generic->GetObject())
			->~FProdNativeLifecycleMember();
		++GProdNativeLifecycleMemberDestructed;
	}
}

static void ProdNativeLifecycleMemberCopyConstruct(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}
	FProdNativeLifecycleMember* const Destination =
		static_cast<FProdNativeLifecycleMember*>(Generic->GetObject());
	const FProdNativeLifecycleMember* const Source =
		static_cast<const FProdNativeLifecycleMember*>(
			Generic->GetArgAddress(0));
	if (Destination != nullptr && Source != nullptr)
	{
		new (Destination) FProdNativeLifecycleMember(*Source);
		++GProdNativeLifecycleMemberCopyConstructed;
		GProdNativeLifecycleMemberCopiedValue = Destination->Stored;
	}
}

static void ProdNativeLifecycleMemberAssign(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}
	FProdNativeLifecycleMember* const Self =
		static_cast<FProdNativeLifecycleMember*>(Generic->GetObject());
	const FProdNativeLifecycleMember* const Other =
		static_cast<const FProdNativeLifecycleMember*>(
			Generic->GetArgAddress(0));
	if (Self != nullptr && Other != nullptr)
	{
		Self->Stored = Other->Stored;
		++GProdNativeLifecycleMemberAssigned;
		GProdNativeLifecycleMemberAssignedValue = Self->Stored;
	}
	Generic->SetReturnAddress(Self);
}

static void ProdNativeLifecycleMemberReadStored(asIScriptGeneric* Generic)
{
	const FProdNativeLifecycleMember* const Self = Generic != nullptr
		? static_cast<const FProdNativeLifecycleMember*>(Generic->GetObject())
		: nullptr;
	++GProdNativeLifecycleMemberRead;
	if (Generic != nullptr)
	{
		Generic->SetReturnDWord(static_cast<asDWORD>(
			Self != nullptr ? Self->Stored : 0));
	}
}

static void ProdNativeLifecycleMemberGetObject(asIScriptGeneric* Generic)
{
	++GProdNativeLifecycleMemberRead;
	if (Generic != nullptr)
	{
		Generic->SetReturnAddress(nullptr);
	}
}

static void ProdNativeLifecycleMemberObserveReference(asIScriptGeneric* Generic)
{
	const FProdNativeLifecycleMember* const Value = Generic != nullptr
		? static_cast<const FProdNativeLifecycleMember*>(
			Generic->GetArgAddress(0))
		: nullptr;
	++GProdNativeLifecycleMemberReferenceObserved;
	if (Generic != nullptr)
	{
		Generic->SetReturnDWord(static_cast<asDWORD>(
			Value != nullptr ? Value->Stored : 0));
	}
}

static void ProdNativeLifecycleMemberAppendAndReturnReference(
	asIScriptGeneric* Generic)
{
	FProdNativeLifecycleMember* const Self = Generic != nullptr
		? static_cast<FProdNativeLifecycleMember*>(Generic->GetObject())
		: nullptr;
	if (Self != nullptr && Generic != nullptr)
	{
		Self->Stored += static_cast<int32>(Generic->GetArgDWord(0));
	}
	if (Generic != nullptr)
	{
		Generic->SetReturnAddress(Self);
	}
}

struct FProdCanonicalStringValue
{
	asDWORD Storage = 0;
};

class FProdCanonicalStringFactory final : public asIStringFactory
{
private:
	struct FEntry
	{
		explicit FEntry(std::string InBytes)
			: Bytes(MoveTemp(InBytes))
		{
		}

		std::string Bytes;
		int32 ReferenceCount = 1;
	};

public:
	const void* GetStringConstant(const char* Data, asUINT Length) override
	{
		const std::string Bytes = Data != nullptr
			? std::string(Data, static_cast<size_t>(Length))
			: std::string();
		for (const TUniquePtr<FEntry>& Entry : Entries)
		{
			if (Entry->Bytes == Bytes)
			{
				++Entry->ReferenceCount;
				++AcquireCallCount;
				return Entry.Get();
			}
		}

		TUniquePtr<FEntry> Entry = MakeUnique<FEntry>(Bytes);
		const FEntry* const Result = Entry.Get();
		Entries.Add(MoveTemp(Entry));
		++AcquireCallCount;
		return Result;
	}

	int ReleaseStringConstant(const void* String) override
	{
		for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
		{
			FEntry* const Entry = Entries[EntryIndex].Get();
			if (Entry != String)
			{
				continue;
			}

			++ReleaseCallCount;
			--Entry->ReferenceCount;
			if (Entry->ReferenceCount == 0)
			{
				Entries.RemoveAt(EntryIndex);
			}
			return asSUCCESS;
		}
		return asINVALID_ARG;
	}

	int GetRawStringData(
		const void* String,
		char* Data,
		asUINT* Length) const override
	{
		if (Length == nullptr)
		{
			return asINVALID_ARG;
		}
		const FEntry* const Entry = FindEntry(String);
		if (Entry == nullptr)
		{
			return asINVALID_ARG;
		}

		*Length = static_cast<asUINT>(Entry->Bytes.size());
		if (Data == nullptr)
		{
			++RawLengthQueryCount;
			return asSUCCESS;
		}
		if (!Entry->Bytes.empty())
		{
			std::memcpy(Data, Entry->Bytes.data(), Entry->Bytes.size());
		}
		++RawCopyCallCount;
		return asSUCCESS;
	}

	int32 ObserveExact(const void* String, const char* Expected, asUINT ExpectedLength)
	{
		++ObserverCallCount;
		asUINT Length = 0;
		if (GetRawStringData(String, nullptr, &Length) != asSUCCESS
			|| Length != ExpectedLength)
		{
			return -1;
		}
		std::string Bytes(static_cast<size_t>(Length), '\0');
		if (GetRawStringData(String, Bytes.data(), &Length) != asSUCCESS
			|| Length != ExpectedLength
			|| (Length != 0 && std::memcmp(Bytes.data(), Expected, Length) != 0))
		{
			return -1;
		}
		return 42;
	}

	int32 GetEntryCount() const { return Entries.Num(); }
	int32 GetAcquireCallCount() const { return AcquireCallCount; }
	int32 GetReleaseCallCount() const { return ReleaseCallCount; }
	int32 GetObserverCallCount() const { return ObserverCallCount; }
	int32 GetTotalReferenceCount() const
	{
		int32 Result = 0;
		for (const TUniquePtr<FEntry>& Entry : Entries)
		{
			Result += Entry->ReferenceCount;
		}
		return Result;
	}

private:
	const FEntry* FindEntry(const void* String) const
	{
		for (const TUniquePtr<FEntry>& Entry : Entries)
		{
			if (Entry.Get() == String)
			{
				return Entry.Get();
			}
		}
		return nullptr;
	}

	TArray<TUniquePtr<FEntry>> Entries;
	int32 AcquireCallCount = 0;
	int32 ReleaseCallCount = 0;
	int32 ObserverCallCount = 0;
	mutable int32 RawLengthQueryCount = 0;
	mutable int32 RawCopyCallCount = 0;
};

static FProdCanonicalStringFactory* GProdCanonicalStringFactory = nullptr;

static void ProdObserveCanonicalString(asIScriptGeneric* Generic)
{
	static const char ExpectedBytes[] = {
		'l', 'i', 'n', 'e', '\n', 'm', 'i', 'd', '\0', 't', 'a', 'i', 'l'
	};
	const int32 Result = Generic != nullptr && GProdCanonicalStringFactory != nullptr
		? GProdCanonicalStringFactory->ObserveExact(
			Generic->GetArgAddress(0),
			ExpectedBytes,
			static_cast<asUINT>(sizeof(ExpectedBytes)))
		: -1;
	if (Generic != nullptr)
	{
		Generic->SetReturnDWord(static_cast<asDWORD>(Result));
	}
}

struct FProdTrackedHandleState;

struct FProdTrackedHandle
{
	explicit FProdTrackedHandle(FProdTrackedHandleState& InState);
	~FProdTrackedHandle();

	void AddRef();
	void Release();

	FProdTrackedHandleState* State = nullptr;
	int32 ReferenceCount = 1;
	int32 Value = 42;
};

struct FProdTrackedHandleState
{
	int32 Created = 0;
	int32 Destroyed = 0;
	int32 AddRefCalls = 0;
	int32 ReleaseCalls = 0;
	int32 LiveObjects = 0;
	FProdTrackedHandle* LastObject = nullptr;
};

static FProdTrackedHandleState* GProdTrackedHandleState = nullptr;

FProdTrackedHandle::FProdTrackedHandle(FProdTrackedHandleState& InState)
	: State(&InState)
{
	++State->Created;
	++State->LiveObjects;
	State->LastObject = this;
}

FProdTrackedHandle::~FProdTrackedHandle()
{
	++State->Destroyed;
	--State->LiveObjects;
	State->LastObject = nullptr;
}

void FProdTrackedHandle::AddRef()
{
	++ReferenceCount;
	++State->AddRefCalls;
}

void FProdTrackedHandle::Release()
{
	++State->ReleaseCalls;
	--ReferenceCount;
	if (ReferenceCount == 0)
	{
		delete this;
	}
}

static FProdTrackedHandle* ProdGetTrackedHandle(asIScriptGeneric* Generic)
{
	return Generic != nullptr
		? static_cast<FProdTrackedHandle*>(Generic->GetObject())
		: nullptr;
}

static void ProdTrackedHandleAddRef(asIScriptGeneric* Generic)
{
	if (FProdTrackedHandle* const Object = ProdGetTrackedHandle(Generic))
	{
		Object->AddRef();
	}
}

static void ProdTrackedHandleRelease(asIScriptGeneric* Generic)
{
	if (FProdTrackedHandle* const Object = ProdGetTrackedHandle(Generic))
	{
		Object->Release();
	}
}

static void ProdTrackedHandleFactory(asIScriptGeneric* Generic)
{
	if (Generic != nullptr && GProdTrackedHandleState != nullptr)
	{
		Generic->SetReturnAddress(new FProdTrackedHandle(*GProdTrackedHandleState));
	}
}

static void ProdTrackedHandleStep(asIScriptGeneric* Generic)
{
	FProdTrackedHandle* const Object = ProdGetTrackedHandle(Generic);
	if (Object != nullptr)
	{
		Object->AddRef();
	}
	if (Generic != nullptr)
	{
		Generic->SetReturnAddress(Object);
	}
}

static void ProdTrackedHandleThrow(asIScriptGeneric* Generic)
{
	if (asIScriptContext* const Context = asGetActiveContext())
	{
		Context->SetException("tracked implicit-handle exception");
	}
	if (Generic != nullptr)
	{
		Generic->SetReturnAddress(nullptr);
	}
}

struct FProdCanonicalIntArray
{
	TArray<int32> Items;
};

static FProdCanonicalIntArray* GProdCanonicalArray = nullptr;

static void ProdCanonicalArrayFactory(asIScriptGeneric* Generic)
{
	FProdCanonicalIntArray* const Array = new FProdCanonicalIntArray();
	GProdCanonicalArray = Array;
	*reinterpret_cast<FProdCanonicalIntArray**>(Generic->GetAddressOfReturnLocation()) = Array;
	Generic->SetReturnAddress(Array);
}

static bool IsProdCanonicalArray(void* Ptr)
{
	return Ptr != nullptr && Ptr == GProdCanonicalArray;
}

static void ProdCanonicalArrayInsertLast(asIScriptGeneric* Generic)
{
	FProdCanonicalIntArray* const Array = static_cast<FProdCanonicalIntArray*>(Generic->GetObject());
	const int32* const Value = static_cast<const int32*>(Generic->GetArgAddress(0));
	if (IsProdCanonicalArray(Array) && Value != nullptr)
	{
		Array->Items.Add(*Value);
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("Canonical array insertLast ABI mismatch: receiver=%p expected=%p argument=%p"),
			Array,
			GProdCanonicalArray,
			Value);
	}
}

static void ProdCanonicalArrayInsertLastBoolPoison(asIScriptGeneric* Generic)
{
	FProdCanonicalIntArray* const Array = static_cast<FProdCanonicalIntArray*>(Generic->GetObject());
	if (IsProdCanonicalArray(Array))
	{
		Array->Items.Add(0);
	}
}

struct FProdBindBox
{
	int32 Stored = 0;
};

struct FProdFields
{
	int32 Poison = 0;
	int32 Stored = 0;
};

struct FProdTriple
{
	int32 First = 0;
	int32 Second = 0;
	int32 Third = 0;
};

struct FProdSmallPOD
{
	int32 Value = 0;
};

static void ProdFieldsConstruct(asIScriptGeneric* Generic)
{
	new (Generic->GetObject()) FProdFields();
}

static void ProdTripleConstruct(asIScriptGeneric* Generic)
{
	new (Generic->GetObject()) FProdTriple();
}

static void ProdTripleSum(asIScriptGeneric* Generic)
{
	FProdTriple* const Input = Generic != nullptr
		? static_cast<FProdTriple*>(Generic->GetAddressOfArg(0))
		: nullptr;
	if (Input == nullptr)
	{
		return;
	}

	const int32 Sum = Input->First * 100 + Input->Second * 10 + Input->Third;
	Generic->SetReturnDWord(static_cast<asDWORD>(Sum));
}

static void ProdSmallPODConstruct(asIScriptGeneric* Generic)
{
	new (Generic->GetObject()) FProdSmallPOD();
}

static void ProdSmallPODEcho(asIScriptGeneric* Generic)
{
	if (Generic != nullptr)
	{
		Generic->SetReturnObject(Generic->GetArgObject(0));
	}
}

static void ProdBindBoxConstruct(asIScriptGeneric* Generic)
{
	new (Generic->GetObject()) FProdBindBox();
}

static void ProdBindBoxConstructFloatPoison(asIScriptGeneric* Generic)
{
	FProdBindBox* const Box = new (Generic->GetObject()) FProdBindBox();
	Box->Stored = 0;
	(void)Generic->GetArgFloat(0);
}

static void ProdBindBoxConstructInt(asIScriptGeneric* Generic)
{
	FProdBindBox* const Box = new (Generic->GetObject()) FProdBindBox();
	Box->Stored = 42;
	(void)Generic->GetArgDWord(0);
}

static void ProdBindBoxGetBoolPoison(asIScriptGeneric* Generic)
{
	Generic->SetReturnDWord(0);
}

static void ProdBindBoxGetInt(asIScriptGeneric* Generic)
{
	Generic->SetReturnDWord(42);
}

static void ProdHostPickBoolPoison(asIScriptGeneric* Generic)
{
	(void)Generic->GetArgByte(0);
	Generic->SetReturnDWord(1);
}

static void ProdHostPickInt(asIScriptGeneric* Generic)
{
	Generic->SetReturnDWord(Generic->GetArgDWord(0) + 1);
}

static void ProdCanonicalArrayOpIndex(asIScriptGeneric* Generic)
{
	FProdCanonicalIntArray* const Array = static_cast<FProdCanonicalIntArray*>(Generic->GetObject());
	const asUINT Index = Generic->GetArgDWord(0);
	static int32 Missing = 0;
	if (!IsProdCanonicalArray(Array) || !Array->Items.IsValidIndex(static_cast<int32>(Index)))
	{
		Generic->SetReturnAddress(&Missing);
		return;
	}
	Generic->SetReturnAddress(&Array->Items[static_cast<int32>(Index)]);
}

static int32 GProdOpIndexBoolPoison = 0;

static void ProdCanonicalArrayOpIndexBoolPoison(asIScriptGeneric* Generic)
{
	GProdOpIndexBoolPoison = 0;
	Generic->SetReturnAddress(&GProdOpIndexBoolPoison);
	(void)Generic->GetArgDWord(0);
}

struct FProdBindHost
{
	int32 Stored = 0;
};

static void ProdBindHostFactoryBoolPoison(asIScriptGeneric* Generic)
{
	FProdBindHost* const Host = new FProdBindHost();
	Host->Stored = 0;
	(void)Generic->GetArgDWord(0);
	Generic->SetReturnAddress(Host);
}

static void ProdBindHostFactoryInt(asIScriptGeneric* Generic)
{
	FProdBindHost* const Host = new FProdBindHost();
	Host->Stored = 42;
	(void)Generic->GetArgDWord(0);
	Generic->SetReturnAddress(Host);
}

static bool ProdBytecodeCallsFunctionWithOpcode(
	asIScriptFunction* Function,
	int FunctionId,
	asEBCInstr ExpectedOpcode)
{
	if (Function == nullptr || FunctionId <= 0)
	{
		return false;
	}

	asUINT BytecodeLength = 0;
	const asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
	if (Bytecode == nullptr || BytecodeLength == 0)
	{
		return false;
	}

	asUINT DwordIndex = 0;
	while (DwordIndex < BytecodeLength)
	{
		const asDWORD* const Instr = Bytecode + DwordIndex;
		const asEBCInstr Opcode = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(Instr));
		if (Opcode == ExpectedOpcode
			&& (Opcode == asBC_CALL || Opcode == asBC_CALLINTF)
			&& static_cast<int>(asBC_DWORDARG(Instr)) == FunctionId)
		{
			return true;
		}
		if (ExpectedOpcode == asBC_CALLSYS && Opcode == asBC_CALLSYS)
		{
			asCScriptFunction* const Callee = reinterpret_cast<asCScriptFunction*>(asBC_PTRARG(Instr));
			if (Callee != nullptr && Callee->GetId() == FunctionId)
			{
				return true;
			}
		}
		if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
		{
			break;
		}
		const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
		if (InstructionSize <= 0)
		{
			break;
		}
		DwordIndex += static_cast<asUINT>(InstructionSize);
	}
	return false;
}

static bool ProdBytecodeCallsFunction(asIScriptFunction* Function, int FunctionId)
{
	return ProdBytecodeCallsFunctionWithOpcode(Function, FunctionId, asBC_CALL)
		|| ProdBytecodeCallsFunctionWithOpcode(Function, FunctionId, asBC_CALLINTF)
		|| ProdBytecodeCallsFunctionWithOpcode(Function, FunctionId, asBC_CALLSYS);
}

static int32 ProdCountBytecodeCallsFunction(
	asIScriptFunction* Function,
	int FunctionId)
{
	if (Function == nullptr || FunctionId <= 0)
	{
		return 0;
	}

	asUINT BytecodeLength = 0;
	const asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
	if (Bytecode == nullptr || BytecodeLength == 0)
	{
		return 0;
	}

	int32 Count = 0;
	asUINT DwordIndex = 0;
	while (DwordIndex < BytecodeLength)
	{
		const asDWORD* const Instr = Bytecode + DwordIndex;
		const asEBCInstr Opcode =
			static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(Instr));
		if ((Opcode == asBC_CALL || Opcode == asBC_CALLINTF)
			&& static_cast<int>(asBC_DWORDARG(Instr)) == FunctionId)
		{
			++Count;
		}
		else if (Opcode == asBC_CALLSYS)
		{
			asCScriptFunction* const Callee =
				reinterpret_cast<asCScriptFunction*>(asBC_PTRARG(Instr));
			if (Callee != nullptr && Callee->GetId() == FunctionId)
			{
				++Count;
			}
		}
		if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
		{
			break;
		}
		const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
		if (InstructionSize <= 0)
		{
			break;
		}
		DwordIndex += static_cast<asUINT>(InstructionSize);
	}
	return Count;
}

static bool ProdBytecodeCallsSystemDeclaration(
	asIScriptFunction* Function,
	const char* ExpectedDeclaration)
{
	if (Function == nullptr || ExpectedDeclaration == nullptr)
	{
		return false;
	}

	asUINT BytecodeLength = 0;
	const asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
	if (Bytecode == nullptr || BytecodeLength == 0)
	{
		return false;
	}

	for (asUINT DwordIndex = 0; DwordIndex < BytecodeLength;)
	{
		const asDWORD* const Instr = Bytecode + DwordIndex;
		const asEBCInstr Opcode = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(Instr));
		if (Opcode == asBC_CALLSYS)
		{
			asCScriptFunction* const Callee = reinterpret_cast<asCScriptFunction*>(asBC_PTRARG(Instr));
			if (Callee != nullptr && Callee->GetDeclaration() != nullptr
				&& FCStringAnsi::Strcmp(Callee->GetDeclaration(), ExpectedDeclaration) == 0)
			{
				return true;
			}
		}
		if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
		{
			break;
		}
		const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
		if (InstructionSize <= 0)
		{
			break;
		}
		DwordIndex += static_cast<asUINT>(InstructionSize);
	}
	return false;
}

static bool ProdBytecodeAllocatesWithConstructor(
	asIScriptFunction* Function,
	asITypeInfo* ObjectType,
	int ConstructorId)
{
	if (Function == nullptr || ObjectType == nullptr || ConstructorId <= 0)
	{
		return false;
	}

	asUINT BytecodeLength = 0;
	const asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
	if (Bytecode == nullptr || BytecodeLength == 0)
	{
		return false;
	}

	for (asUINT DwordIndex = 0; DwordIndex < BytecodeLength;)
	{
		const asDWORD* const Instr = Bytecode + DwordIndex;
		const asEBCInstr Opcode = static_cast<asEBCInstr>(
			*reinterpret_cast<const asBYTE*>(Instr));
		if (Opcode == asBC_ALLOC
			&& reinterpret_cast<asITypeInfo*>(asBC_PTRARG(Instr)) == ObjectType
			&& asBC_INTARG(Instr + AS_PTR_SIZE) == ConstructorId)
		{
			return true;
		}
		if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
		{
			break;
		}
		const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
		if (InstructionSize <= 0)
		{
			break;
		}
		DwordIndex += static_cast<asUINT>(InstructionSize);
	}
	return false;
}

static int32 ProdBytecodeFirstCallOffset(asIScriptFunction* Function, int FunctionId)
{
	if (Function == nullptr || FunctionId <= 0)
	{
		return INDEX_NONE;
	}

	asUINT BytecodeLength = 0;
	const asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
	if (Bytecode == nullptr || BytecodeLength == 0)
	{
		return INDEX_NONE;
	}

	asUINT DwordIndex = 0;
	while (DwordIndex < BytecodeLength)
	{
		const asDWORD* const Instr = Bytecode + DwordIndex;
		const asEBCInstr Opcode = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(Instr));
		if ((Opcode == asBC_CALL || Opcode == asBC_CALLINTF)
			&& static_cast<int>(asBC_DWORDARG(Instr)) == FunctionId)
		{
			return static_cast<int32>(DwordIndex);
		}
		if (Opcode == asBC_CALLSYS)
		{
			asCScriptFunction* const Callee = reinterpret_cast<asCScriptFunction*>(asBC_PTRARG(Instr));
			if (Callee != nullptr && Callee->GetId() == FunctionId)
			{
				return static_cast<int32>(DwordIndex);
			}
		}
		if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
		{
			break;
		}
		const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
		if (InstructionSize <= 0)
		{
			break;
		}
		DwordIndex += static_cast<asUINT>(InstructionSize);
	}
	return INDEX_NONE;
}

static TArray<int32> ProdBytecodeCallOffsets(asIScriptFunction* Function, int FunctionId)
{
	TArray<int32> Offsets;
	if (Function == nullptr || FunctionId <= 0)
	{
		return Offsets;
	}

	asUINT BytecodeLength = 0;
	const asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
	if (Bytecode == nullptr)
	{
		return Offsets;
	}

	for (asUINT DwordIndex = 0; DwordIndex < BytecodeLength;)
	{
		const asDWORD* const Instr = Bytecode + DwordIndex;
		const asEBCInstr Opcode =
			static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(Instr));
		if ((Opcode == asBC_CALL || Opcode == asBC_CALLINTF)
			&& static_cast<int>(asBC_DWORDARG(Instr)) == FunctionId)
		{
			Offsets.Add(static_cast<int32>(DwordIndex));
		}
		else if (Opcode == asBC_CALLSYS)
		{
			asCScriptFunction* const Callee =
				reinterpret_cast<asCScriptFunction*>(asBC_PTRARG(Instr));
			if (Callee != nullptr && Callee->GetId() == FunctionId)
			{
				Offsets.Add(static_cast<int32>(DwordIndex));
			}
		}

		const int InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
		if (InstructionSize <= 0)
		{
			break;
		}
		DwordIndex += static_cast<asUINT>(InstructionSize);
	}
	return Offsets;
}

static FString ProdBytecodeCallTargets(asIScriptFunction* Function)
{
	FString Out;
	if (Function == nullptr)
	{
		return TEXT("<null>");
	}

	asUINT BytecodeLength = 0;
	const asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
	if (Bytecode == nullptr || BytecodeLength == 0)
	{
		return TEXT("<no bytecode>");
	}

	asUINT DwordIndex = 0;
	while (DwordIndex < BytecodeLength)
	{
		const asDWORD* const Instr = Bytecode + DwordIndex;
		const asEBCInstr Opcode = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(Instr));
		if (Opcode == asBC_CALL || Opcode == asBC_CALLINTF)
		{
			Out += FString::Printf(TEXT(" %s:%d"),
				Opcode == asBC_CALL ? TEXT("CALL") : TEXT("CALLINTF"),
				static_cast<int>(asBC_DWORDARG(Instr)));
		}
		else if (Opcode == asBC_CALLSYS)
		{
			asCScriptFunction* const Callee = reinterpret_cast<asCScriptFunction*>(asBC_PTRARG(Instr));
			Out += FString::Printf(
				TEXT(" CALLSYS:%d[%s]"),
				Callee != nullptr ? Callee->GetId() : 0,
				Callee != nullptr && Callee->GetDeclaration() != nullptr
					? UTF8_TO_TCHAR(Callee->GetDeclaration())
					: TEXT("<null>"));
		}
		else if (Opcode == asBC_ALLOC)
		{
			Out += FString::Printf(TEXT(" ALLOC:%d"), static_cast<int>(asBC_DWORDARG(Instr)));
		}
		if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
		{
			break;
		}
		const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
		if (InstructionSize <= 0)
		{
			break;
		}
		DwordIndex += static_cast<asUINT>(InstructionSize);
	}
	return Out.IsEmpty() ? FString(TEXT("<no CALL/CALLSYS/ALLOC>")) : Out;
}

static FString ProdRuntimeObjectDirectory(asIScriptFunction* Function)
{
	const asCScriptFunction* const Internal =
		static_cast<const asCScriptFunction*>(Function);
	if (Internal == nullptr || Internal->scriptData == nullptr)
	{
		return TEXT("<no script data>");
	}

	FString Out = FString::Printf(
		TEXT("heap=%u objects=%u"),
		Internal->scriptData->objVariablesOnHeap,
		Internal->scriptData->objVariablePos.GetLength());
	for (asUINT Index = 0;
		Index < Internal->scriptData->objVariablePos.GetLength(); ++Index)
	{
		const asCTypeInfo* const Type =
			Index < Internal->scriptData->objVariableTypes.GetLength()
				? Internal->scriptData->objVariableTypes[Index] : nullptr;
		Out += FString::Printf(
			TEXT(" [%u:%d:%s]"),
			Index,
			Internal->scriptData->objVariablePos[Index],
			Type != nullptr && Type->GetName() != nullptr
				? UTF8_TO_TCHAR(Type->GetName()) : TEXT("<null>"));
	}
	return Out;
}

static FString ProdBytecodeImmediates(asIScriptFunction* Function)
{
	FString Out;
	if (Function == nullptr)
	{
		return TEXT("<null>");
	}

	asUINT BytecodeLength = 0;
	const asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
	if (Bytecode == nullptr || BytecodeLength == 0)
	{
		return TEXT("<no bytecode>");
	}

	asUINT DwordIndex = 0;
	while (DwordIndex < BytecodeLength)
	{
		const asDWORD* const Instr = Bytecode + DwordIndex;
		const asEBCInstr Opcode = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(Instr));
		if (Opcode == asBC_SetV4)
		{
			Out += FString::Printf(
				TEXT(" SetV4 v%d=%d"),
				static_cast<int>(asBC_SWORDARG0(Instr)),
				static_cast<int>(asBC_DWORDARG(Instr)));
		}
		else if (Opcode == asBC_ADDi)
		{
			Out += FString::Printf(
				TEXT(" ADDi v%d=v%d+v%d"),
				static_cast<int>(asBC_SWORDARG0(Instr)),
				static_cast<int>(asBC_SWORDARG1(Instr)),
				static_cast<int>(asBC_SWORDARG2(Instr)));
		}
		else if (Opcode == asBC_WRTV4)
		{
			Out += FString::Printf(TEXT(" WRTV4 v%d"), static_cast<int>(asBC_SWORDARG0(Instr)));
		}
		else if (Opcode == asBC_RDR4)
		{
			Out += FString::Printf(TEXT(" RDR4 v%d"), static_cast<int>(asBC_SWORDARG0(Instr)));
		}
		else if (Opcode == asBC_PSF)
		{
			Out += FString::Printf(TEXT(" PSF %d"), static_cast<int>(asBC_SWORDARG0(Instr)));
		}
		else if (Opcode == asBC_PshV4)
		{
			Out += FString::Printf(TEXT(" PshV4 %d"), static_cast<int>(asBC_SWORDARG0(Instr)));
		}
		else if (Opcode == asBC_PshV8)
		{
			Out += FString::Printf(TEXT(" PshV8 %d"), static_cast<int>(asBC_SWORDARG0(Instr)));
		}
		if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
		{
			break;
		}
		const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
		if (InstructionSize <= 0)
		{
			break;
		}
		DwordIndex += static_cast<asUINT>(InstructionSize);
	}
	return Out.IsEmpty() ? FString(TEXT("<no immediates>")) : Out;
}

static bool ProdContainsSetV4(
	asIScriptFunction* Function,
	int32 VariableOffset,
	int32 Immediate)
{
	if (Function == nullptr)
	{
		return false;
	}
	asUINT BytecodeLength = 0;
	const asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
	for (asUINT DwordIndex = 0; Bytecode != nullptr && DwordIndex < BytecodeLength;)
	{
		const asDWORD* const Instr = Bytecode + DwordIndex;
		const asEBCInstr Opcode = static_cast<asEBCInstr>(
			*reinterpret_cast<const asBYTE*>(Instr));
		if (Opcode == asBC_SetV4
			&& static_cast<int32>(asBC_SWORDARG0(Instr)) == VariableOffset
			&& static_cast<int32>(asBC_DWORDARG(Instr)) == Immediate)
		{
			return true;
		}
		if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
		{
			break;
		}
		const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
		if (InstructionSize <= 0)
		{
			break;
		}
		DwordIndex += static_cast<asUINT>(InstructionSize);
	}
	return false;
}

static bool ProdDumpHasConstructCallee(const FString& Dump)
{
	TArray<FString> Lines;
	Dump.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		if (!Line.Contains(TEXT("kind=Construct")))
		{
			continue;
		}
		const int32 At = Line.Find(TEXT("callee="));
		if (At < 0)
		{
			continue;
		}
		const FString Rest = Line.Mid(At + 7);
		if (Rest.Len() > 0 && Rest[0] != TEXT(' ') && Rest[0] != TEXT('\t'))
		{
			return true;
		}
	}
	return false;
}

static void ProdBindHostFactoryZero(asIScriptGeneric* Generic)
{
	FProdBindHost* const Host = new FProdBindHost();
	Host->Stored = 41;
	Generic->SetReturnAddress(Host);
}

static void ProdBindHostFactoryIntPoison(asIScriptGeneric* Generic)
{
	FProdBindHost* const Host = new FProdBindHost();
	Host->Stored = 0;
	(void)Generic->GetArgDWord(0);
	Generic->SetReturnAddress(Host);
}

static void ProdBindBoxConstructZero(asIScriptGeneric* Generic)
{
	FProdBindBox* const Box = new (Generic->GetObject()) FProdBindBox();
	Box->Stored = 41;
}

static void ProdBindBoxConstructIntPoison(asIScriptGeneric* Generic)
{
	FProdBindBox* const Box = new (Generic->GetObject()) FProdBindBox();
	Box->Stored = 0;
	(void)Generic->GetArgDWord(0);
}

struct FProdCanonicalListBox
{
	int32 Marker = 0;
};

static void ProdCanonicalListFactory(asIScriptGeneric* Generic)
{
	FProdCanonicalListBox* const Box = new FProdCanonicalListBox();
	void* const List = Generic != nullptr ? Generic->GetArgAddress(0) : nullptr;
	if (List != nullptr)
	{
		const asUINT Count = *reinterpret_cast<asUINT*>(List);
		if (Count > 0)
		{
			Box->Marker = *reinterpret_cast<int*>(reinterpret_cast<asUINT*>(List) + 1);
		}
	}
	if (Generic != nullptr)
	{
		Generic->SetReturnAddress(Box);
	}
}

static void ProdCanonicalDoubleListFactory(asIScriptGeneric* Generic)
{
	FProdCanonicalListBox* const Box = new FProdCanonicalListBox();
	void* const List = Generic != nullptr ? Generic->GetArgAddress(0) : nullptr;
	if (List != nullptr)
	{
		const asUINT Count = *reinterpret_cast<asUINT*>(List);
		if (Count > 0)
		{
			const double Stored = *reinterpret_cast<double*>(reinterpret_cast<asUINT*>(List) + 1);
			Box->Marker = static_cast<int32>(Stored);
		}
	}
	if (Generic != nullptr)
	{
		Generic->SetReturnAddress(Box);
	}
}

struct FProdAggregateElement
{
	int32 Id = 0;
};

struct FProdAggregateListBox
{
	int32 Marker = 0;
};

static FProdAggregateListBox GProdAggregateListBox;
static bool GProdAggregateFactoryShouldFail = false;
static int32 GProdAggregateFactoryCalls = 0;
static TArray<const void*> GProdAggregateReturnStorage;
static TArray<int32> GProdAggregateBufferDestructionOrder;
static TArray<int32> GProdAggregateTemporaryDestructionOrder;

static void ProdAggregateElementConstruct(asIScriptGeneric* Generic)
{
	if (Generic != nullptr && Generic->GetObject() != nullptr)
	{
		new (Generic->GetObject()) FProdAggregateElement();
	}
}

static void ProdAggregateElementDestruct(asIScriptGeneric* Generic)
{
	if (Generic == nullptr || Generic->GetObject() == nullptr)
	{
		return;
	}
	FProdAggregateElement* const Element =
		static_cast<FProdAggregateElement*>(Generic->GetObject());
	if (GProdAggregateReturnStorage.Contains(Element))
	{
		GProdAggregateTemporaryDestructionOrder.Add(Element->Id);
	}
	else
	{
		GProdAggregateBufferDestructionOrder.Add(Element->Id);
	}
	Element->~FProdAggregateElement();
}

static void ProdMakeAggregateElement(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}
	const int32 Id = static_cast<int32>(Generic->GetArgDWord(0));
	if (Generic->GetArgByte(1) != 0)
	{
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			Context->SetException("aggregate middle-element failure");
		}
		return;
	}

	FProdAggregateElement Value;
	Value.Id = Id;
	GProdAggregateReturnStorage.Add(Generic->GetAddressOfReturnLocation());
	Generic->SetReturnObject(&Value);
}

static void ProdSetAggregateFactoryFailure(asIScriptGeneric* Generic)
{
	GProdAggregateFactoryShouldFail = Generic != nullptr
		&& Generic->GetArgByte(0) != 0;
}

static void ProdAggregateListFactory(asIScriptGeneric* Generic)
{
	++GProdAggregateFactoryCalls;
	if (Generic == nullptr)
	{
		return;
	}
	if (GProdAggregateFactoryShouldFail)
	{
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			Context->SetException("aggregate list-factory failure");
		}
		return;
	}

	const asBYTE* const Buffer =
		static_cast<const asBYTE*>(Generic->GetArgAddress(0));
	const asUINT Count = Buffer != nullptr
		? *reinterpret_cast<const asUINT*>(Buffer) : 0;
	GProdAggregateListBox.Marker = 0;
	for (asUINT Index = 0; Index < Count; ++Index)
	{
		const FProdAggregateElement* const Element =
			reinterpret_cast<const FProdAggregateElement*>(
				Buffer + sizeof(asUINT)
					+ Index * sizeof(FProdAggregateElement));
		GProdAggregateListBox.Marker =
			GProdAggregateListBox.Marker * 10 + Element->Id;
	}
	Generic->SetReturnAddress(&GProdAggregateListBox);
}

static bool ProdFunctionIsOnGlobalList(asCModule* Module, asIScriptFunction* Function)
{
	if (Module == nullptr || Function == nullptr)
	{
		return false;
	}
	for (asUINT Index = 0; Index < Module->globalFunctionList.GetLength(); ++Index)
	{
		if (Module->globalFunctionList[Index] == Function)
		{
			return true;
		}
	}
	return false;
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTProductionCodeGenTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FPreparedRestoreRollbackContext
	{
		AngelscriptNativeTestSupport::FMemoryBinaryStream* FirstArtifact = nullptr;
		int32 FirstRestoreCount = 0;
		asUINT PendingFunctionCount = 0;
		bool bSawPendingSecond = false;
	};

	static asEBuildArtifactRestoreResult RestorePreparedFirstForRollback(
		const asSBuildArtifactInvocation* Invocation,
		asCScriptFunction* Function,
		const asSBuildArtifactRestoreContext* RestoreContext,
		void* UserData)
	{
		if (Invocation == nullptr || Function == nullptr || UserData == nullptr)
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}
		if (Function->name != "First")
		{
			return asBUILD_ARTIFACT_RESTORE_MISS;
		}

		FPreparedRestoreRollbackContext& Context =
			*static_cast<FPreparedRestoreRollbackContext*>(UserData);
		if (Context.FirstArtifact == nullptr || RestoreContext == nullptr
			|| !RestoreContext->IsValid())
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}

		++Context.FirstRestoreCount;
		Context.PendingFunctionCount = RestoreContext->transientFunctionCount;
		for (asUINT Index = 0;
			Index < RestoreContext->transientFunctionCount; ++Index)
		{
			const asCScriptFunction* const Pending =
				RestoreContext->transientFunctions[Index];
			if (Pending != nullptr && Pending->name == "Second")
			{
				Context.bSawPendingSecond = true;
				break;
			}
		}

		Context.FirstArtifact->ResetReadPosition();
		asCReader Reader(
			Function->module,
			Context.FirstArtifact,
			Function->engine,
			RestoreContext);
		asCScriptFunction* Artifact = nullptr;
		if (Reader.RestoreFunctionArtifactDetached(
				static_cast<asUINT>(Context.FirstArtifact->Num()),
				&Artifact) < 0
			|| Artifact == nullptr)
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}
		if (Reader.CommitFunctionArtifactToExisting(Artifact, Function) < 0)
		{
			Artifact->DestroyHalfCreated();
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}
		return asBUILD_ARTIFACT_RESTORE_RESTORED;
	}

public:
	TEST_METHOD(PreparedBuilderShellKeepsIdentityAndPublishesCanonicalBody)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalShell");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalShell.as",
			"int PreparedEntry() { return 42; }")));
		ASSERT_THAT(IsNotNull(Module->builder,
			TEXT("AddScriptSection must create the staged Builder")));
		if (Module->builder == nullptr)
		{
			return;
		}
		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("the staged Builder must prepare and lay out the Runtime shell")));
		ASSERT_THAT(AreEqual(0, Builder.SealCanonicalAST(),
			TEXT("prepared-shell CodeGen accepts only the sealed verified graph")));

		asCScriptFunction* const PreparedFunction =
			static_cast<asCScriptFunction*>(Module->GetFunctionByDecl(
				"int PreparedEntry()"));
		ASSERT_THAT(IsNotNull(PreparedFunction));
		if (PreparedFunction == nullptr)
		{
			return;
		}
		const int PreparedFunctionId = PreparedFunction->GetId();
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			PreparedFunction->scriptData->byteCode.GetLength(),
			TEXT("the gate must observe the shell before any legacy body compiler")));

		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK,
			CodeGen.GeneratePreparedModule(*Context, Builder),
			*FString::Printf(TEXT("prepared-shell CodeGen must succeed: %s"),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));

		asCScriptFunction* const PublishedFunction =
			static_cast<asCScriptFunction*>(Module->GetFunctionByDecl(
				"int PreparedEntry()"));
		ASSERT_THAT(IsTrue(PublishedFunction == PreparedFunction,
			TEXT("CodeGen must fill the exact Stage 2 Runtime shell")));
		ASSERT_THAT(AreEqual(PreparedFunctionId, PublishedFunction->GetId(),
			TEXT("prepared CodeGen must preserve the authoritative FunctionId")));
		ASSERT_THAT(IsTrue(PublishedFunction->scriptData != nullptr
			&& PublishedFunction->scriptData->byteCode.GetLength() > 0,
			TEXT("the existing shell must receive executable Canonical Bytecode")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Module->GetLastLegacyCompilerInvocationCount()));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int PreparedEntry()", Value)
			&& Value == 42,
			TEXT("the prepared Runtime shell must execute its canonical body")));
	}

	TEST_METHOD(PreparedBytecodeParametersUseSealedFormalOrdinalRelation)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalExactFormalRelation");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int EncodeFormals(int First, int Second)
			{
				return First * 10 + Second;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalExactFormalRelation.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("the exact-formal fixture must reach the prepared Runtime shell")));
		asCASTContext* const BuildingContext = Builder.canonicalAST;
		ASSERT_THAT(IsNotNull(BuildingContext));
		if (BuildingContext == nullptr)
		{
			return;
		}

		asCDecl* FunctionDecl = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= BuildingContext->GetDeclCount(); ++DeclIndex)
		{
			asCDecl* const Candidate = const_cast<asCDecl*>(
				BuildingContext->GetDecl(asASTDeclId(DeclIndex)));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_FUNCTION
				&& Candidate->name == "EncodeFormals")
			{
				ASSERT_THAT(IsNull(FunctionDecl,
					TEXT("the exact-formal fixture must own one function declaration")));
				FunctionDecl = Candidate;
			}
		}
		ASSERT_THAT(IsNotNull(FunctionDecl));
		if (FunctionDecl == nullptr)
		{
			return;
		}

		const asCDecl* const FirstFormal =
			BuildingContext->GetFormalDecl(FunctionDecl->id, 0);
		const asCDecl* const SecondFormal =
			BuildingContext->GetFormalDecl(FunctionDecl->id, 1);
		ASSERT_THAT(IsTrue(FirstFormal != nullptr && SecondFormal != nullptr
			&& FirstFormal->name == "First" && SecondFormal->name == "Second",
			TEXT("Sema must publish the authored formal relation before reordering")));
		if (FirstFormal == nullptr || SecondFormal == nullptr)
		{
			return;
		}

		asUINT FirstChildIndex = asUINT(-1);
		asUINT SecondChildIndex = asUINT(-1);
		for (asUINT ChildIndex = 0;
			ChildIndex < FunctionDecl->children.GetLength(); ++ChildIndex)
		{
			if (FunctionDecl->children[ChildIndex] == FirstFormal->id)
			{
				FirstChildIndex = ChildIndex;
			}
			else if (FunctionDecl->children[ChildIndex] == SecondFormal->id)
			{
				SecondChildIndex = ChildIndex;
			}
		}
		ASSERT_THAT(IsTrue(FirstChildIndex != asUINT(-1)
			&& SecondChildIndex != asUINT(-1)));
		if (FirstChildIndex == asUINT(-1) || SecondChildIndex == asUINT(-1))
		{
			return;
		}

		// Children are a traversal container. Preserve each sealed formalIndex
		// while adversarially changing only structural order; both parameters are
		// int so positional binding would silently change 42 into 24.
		FunctionDecl->children[FirstChildIndex] = SecondFormal->id;
		FunctionDecl->children[SecondChildIndex] = FirstFormal->id;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST(),
			TEXT("the reordered same-typed formal graph must remain publishable")));

		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK,
			CodeGen.GeneratePreparedModule(*Context, Builder),
			*FString::Printf(TEXT("exact-formal CodeGen must succeed: %s"),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));

		asIScriptFunction* const Function = Module->GetFunctionByDecl(
			"int EncodeFormals(int, int)");
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}
		asIScriptContext* const RuntimeContext = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(RuntimeContext));
		if (RuntimeContext == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { RuntimeContext->Release(); };
		ASSERT_THAT(AreEqual(asSUCCESS, RuntimeContext->Prepare(Function)));
		ASSERT_THAT(AreEqual(asSUCCESS, RuntimeContext->SetArgDWord(0, 4)));
		ASSERT_THAT(AreEqual(asSUCCESS, RuntimeContext->SetArgDWord(1, 2)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, RuntimeContext->Execute()));
		ASSERT_THAT(AreEqual(static_cast<asDWORD>(42),
			RuntimeContext->GetReturnDWord(),
			TEXT("Runtime slots must bind through sealed formalIndex, not PARAM child order")));
	}

	TEST_METHOD(PreparedConstructArgumentsUseSealedFormalOrdinalRelation)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalConstructFormalRelation");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			struct FConstructPlan
			{
				int A;
				int B;

				FConstructPlan(int InA = 40 + 2, int InB = 1)
				{
					A = InA;
					B = InB;
				}
			}

			int Entry()
			{
				FConstructPlan Value(InB: 7);
				return Value.A * 10 + Value.B;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalConstructFormalRelation.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("construct exact-formal fixture must reach prepared Runtime shells")));
		asCASTContext* const BuildingContext = Builder.canonicalAST;
		ASSERT_THAT(IsNotNull(BuildingContext));
		if (BuildingContext == nullptr)
		{
			return;
		}

		asCExpr* Construct = nullptr;
		for (asUINT ExprIndex = 1;
			ExprIndex <= BuildingContext->GetExprCount(); ++ExprIndex)
		{
			asCExpr* const Candidate = const_cast<asCExpr*>(
				BuildingContext->GetExpr(asASTExprId(ExprIndex)));
			if (Candidate == nullptr
				|| Candidate->kind != asAST_EXPR_CONSTRUCT
				|| Candidate->children.GetLength() != 2
				|| Candidate->callArguments.GetLength() != 2)
			{
				continue;
			}
			const asCDecl* const Callee = BuildingContext->GetDecl(
				Candidate->resolvedDecl);
			if (Callee != nullptr
				&& Callee->kind == asAST_DECL_CONSTRUCTOR
				&& BuildingContext->GetFormalDeclCount(Callee->id) == 2)
			{
				ASSERT_THAT(IsNull(Construct,
					TEXT("fixture must own one two-formal ordinary CONSTRUCT")));
				Construct = Candidate;
			}
		}
		ASSERT_THAT(IsNotNull(Construct,
			TEXT("Sema must publish the named/default CONSTRUCT relation")));
		if (Construct == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			Construct->callArguments[0].formalIndex == 0
			&& Construct->callArguments[0].origin == asAST_CALL_ARGUMENT_DEFAULT
			&& Construct->callArguments[1].formalIndex == 1
			&& Construct->callArguments[1].origin == asAST_CALL_ARGUMENT_NAMED,
			TEXT("source Sema must first seal default InA and named InB exactly")));

		// Children and records are an evaluation-order sequence, not the ABI map.
		// Reorder each expression/record pair while preserving its sealed formalIndex.
		// Both formals are int, so a positional consumer can silently execute the
		// wrong value without any type mismatch.
		const asASTExprId FirstChild = Construct->children[0];
		Construct->children[0] = Construct->children[1];
		Construct->children[1] = FirstChild;
		const asSASTCallArgument FirstRecord = Construct->callArguments[0];
		Construct->callArguments[0] = Construct->callArguments[1];
		Construct->callArguments[1] = FirstRecord;
		ASSERT_THAT(IsTrue(
			Construct->callArguments[0].formalIndex == 1
			&& Construct->callArguments[1].formalIndex == 0,
			TEXT("adversarial graph must differ from formal ABI order")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST(),
			TEXT("reordered expression/record pairs must remain publishable")));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK,
			CodeGen.GeneratePreparedModule(*Context, Builder),
			*FString::Printf(
				TEXT("exact construct-formal CodeGen must succeed: %s"),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 427,
			*FString::Printf(TEXT(
				"constructor ABI slots must use sealed formalIndex, not reordered record position; status=%d got=%d exception=%s calls={%s} immediates={%s} objects={%s}"),
				ExecuteStatus, Value, *Exception,
				*ProdBytecodeCallTargets(Module->GetFunctionByDecl("int Entry()")),
				*ProdBytecodeImmediates(Module->GetFunctionByDecl("int Entry()")),
				*ProdRuntimeObjectDirectory(Module->GetFunctionByDecl("int Entry()")))));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher(),
			TEXT("adversarial construct relation must publish through CodeGen")));
	}

	TEST_METHOD(PreparedAccessSpecifierDefinitionsAndMemberEdgesUseExactCanonicalDeclIds)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalAccessSpecifier");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FAccessProjectionProbe
			{
				access Internal = private, Friend(inherited);
				access External = protected, *(readonly, editdefaults);
				access:Internal int Value;
				access:External int Read() const { return Value; }
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalAccessSpecifier.as", Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		const size_t InternalDefinition = Source.find("Internal");
		const size_t InternalUse = Source.find("Internal", InternalDefinition + 1);
		const size_t ExternalDefinition = Source.find("External");
		const size_t ExternalUse = Source.find("External", ExternalDefinition + 1);
		ASSERT_THAT(IsTrue(
			InternalDefinition != std::string::npos
				&& InternalUse != std::string::npos
				&& ExternalDefinition != std::string::npos
				&& ExternalUse != std::string::npos,
			TEXT("fixture must expose two same-width definitions and member references")));
		if (InternalDefinition == std::string::npos
			|| InternalUse == std::string::npos
			|| ExternalDefinition == std::string::npos
			|| ExternalUse == std::string::npos)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts(),
			TEXT("typed actions and exact identities must be complete before Stage 2")));
		ASSERT_THAT(IsTrue(Builder.scripts.GetLength() == 1
			&& Builder.scripts[0] != nullptr
			&& Builder.scripts[0]->code != nullptr,
			TEXT("fixture must own one mutable retained source buffer")));
		if (Builder.scripts.GetLength() != 1
			|| Builder.scripts[0] == nullptr
			|| Builder.scripts[0]->code == nullptr)
		{
			return;
		}

		// Preserve every parse-identity coordinate while making the retained
		// LEGACY/reference spelling disagree with the already-published typed
		// facts. A Canonical Runtime projection must be insensitive to this
		// test-only poison; the LEGACY Builder decoder intentionally is not.
		char* const RetainedSource = Builder.scripts[0]->code;
		std::memcpy(RetainedSource + InternalDefinition, "Poisoned", 8);
		std::memcpy(RetainedSource + InternalUse, "External", 8);
		std::memcpy(RetainedSource + ExternalUse, "Internal", 8);

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		ASSERT_THAT(IsTrue(Builder.classDeclarations.GetLength() == 1
			&& Builder.classDeclarations[0] != nullptr
			&& Builder.classDeclarations[0]->node != nullptr));
		if (Builder.classDeclarations.GetLength() != 1
			|| Builder.classDeclarations[0] == nullptr
			|| Builder.classDeclarations[0]->node == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			Builder.classDeclarations[0]->canonicalASTStableDeclKey.GetLength() > 0,
			TEXT("Stage 1 must carry the exact Parser/Sema class identity before member projection")));
		++Builder.classDeclarations[0]->node->tokenPos;
		asCScriptNode* NativeAccessNodes[2] = { nullptr, nullptr };
		asUINT NativeAccessNodeCount = 0;
		for (asCScriptNode* Node =
			Builder.classDeclarations[0]->node->firstChild;
			Node != nullptr; Node = Node->next)
		{
			if (Node->nodeType == snAccessDeclaration
				&& NativeAccessNodeCount < 2)
			{
				NativeAccessNodes[NativeAccessNodeCount++] = Node;
			}
		}
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2),
			NativeAccessNodeCount,
			TEXT("fixture must retain two native syntax nodes after Canonical actions are complete")));
		if (NativeAccessNodeCount != 2)
		{
			return;
		}
		const size_t FirstTokenPos = NativeAccessNodes[0]->tokenPos;
		const size_t FirstTokenLength = NativeAccessNodes[0]->tokenLength;
		NativeAccessNodes[0]->tokenPos = NativeAccessNodes[1]->tokenPos;
		NativeAccessNodes[0]->tokenLength = NativeAccessNodes[1]->tokenLength;
		NativeAccessNodes[1]->tokenPos = FirstTokenPos;
		NativeAccessNodes[1]->tokenLength = FirstTokenLength;

		const int GenerateFunctions = Builder.BuildGenerateFunctions();
		ASSERT_THAT(AreEqual(asSUCCESS, GenerateFunctions,
			TEXT("Canonical Stage 2 must project access metadata from exact declarations, not poisoned source spelling or native declaration inventory")));
		if (GenerateFunctions < 0)
		{
			ReportBuilderFailureDiagnostics(*TestRunner, Engine);
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildLayoutClasses()));
		Builder.BuildAllocateGlobalVariables();
		ASSERT_THAT(AreEqual(asSUCCESS,
			Builder.BuildLayoutFunctionsWithoutGlobalInitializers()));

		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FAccessProjectionProbe"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}
		asSAccessSpecifier* const Internal =
			ObjectType->GetAccessSpecifier("Internal");
		asSAccessSpecifier* const External =
			ObjectType->GetAccessSpecifier("External");
		ASSERT_THAT(IsTrue(Internal != nullptr
			&& Internal->bIsPrivate
			&& !Internal->bIsProtected
			&& Internal->permissions.GetLength() == 1
			&& Internal->permissions[0].accessName == "Friend"
			&& Internal->permissions[0].bInherited,
			TEXT("Internal Runtime DTO must preserve the exact Canonical definition")));
		ASSERT_THAT(IsTrue(External != nullptr
			&& !External->bIsPrivate
			&& External->bIsProtected
			&& External->bAnyReadOnly
			&& External->bAnyEditDefaults
			&& External->permissions.GetLength() == 0,
			TEXT("External Runtime DTO must preserve the exact Canonical wildcard traits")));
		ASSERT_THAT(IsNull(ObjectType->GetAccessSpecifier("Poisoned"),
			TEXT("poisoned retained spelling must never become Runtime metadata")));
		if (Internal == nullptr || External == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ObjectType->accessSpecifiers.GetLength() == 2
			&& ObjectType->accessSpecifiers[0] == Internal
			&& ObjectType->accessSpecifiers[1] == External,
			TEXT("Runtime definition order must follow Canonical owner children, not mutated native declaration coordinates")));

		asCObjectProperty* ValueProperty = nullptr;
		for (asUINT Index = 0; Index < ObjectType->properties.GetLength(); ++Index)
		{
			asCObjectProperty* const Property = ObjectType->properties[Index];
			if (Property != nullptr && Property->name == "Value")
			{
				ValueProperty = Property;
				break;
			}
		}
		asCScriptFunction* const ReadMethod = static_cast<asCScriptFunction*>(
			ObjectType->GetMethodByDecl("int Read() const"));
		ASSERT_THAT(IsTrue(ValueProperty != nullptr
			&& ValueProperty->accessSpecifier == Internal,
			TEXT("field Runtime metadata must follow its exact VarDecl access edge")));
		ASSERT_THAT(IsTrue(ReadMethod != nullptr
			&& ReadMethod->accessSpecifier == External,
			TEXT("method Runtime metadata must follow its exact MethodDecl access edge")));
	}

	TEST_METHOD(PreparedAccessWildcardTraitsAccumulateLikeLegacyRuntimeMetadata)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalAccessWildcardAccumulation");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FAccessWildcardAccumulationProbe
			{
				access Combined = private, *(readonly), *(editdefaults);
				access:Combined int Value;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalAccessWildcardAccumulation.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		const int GenerateFunctions = Builder.BuildGenerateFunctions();
		ASSERT_THAT(AreEqual(asSUCCESS, GenerateFunctions,
			TEXT("multiple wildcard permission entries are a legal maintained-fork access declaration")));
		if (GenerateFunctions < 0)
		{
			ReportBuilderFailureDiagnostics(*TestRunner, Engine);
			return;
		}

		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FAccessWildcardAccumulationProbe"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}
		asSAccessSpecifier* const Combined =
			ObjectType->GetAccessSpecifier("Combined");
		ASSERT_THAT(IsTrue(Combined != nullptr
			&& Combined->bAnyReadOnly
			&& Combined->bAnyEditDefaults,
			TEXT("Canonical wildcard traits must use the LEGACY-compatible union of every wildcard permission")));
	}

	TEST_METHOD(PreparedAccessSpecifierProjectionRejectsRuntimeMetadataAbsentFromCanonicalRecord)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalAccessSpecifierContamination");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FAccessProjectionContaminationProbe
			{
				int Value;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalAccessSpecifierContamination.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FAccessProjectionContaminationProbe"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}

		asSAccessSpecifier* const ForeignRuntimeMetadata =
			asNEW(asSAccessSpecifier);
		ASSERT_THAT(IsNotNull(ForeignRuntimeMetadata));
		if (ForeignRuntimeMetadata == nullptr)
		{
			return;
		}
		ForeignRuntimeMetadata->name = "Foreign";
		const asUINT PreviousCount = ObjectType->accessSpecifiers.GetLength();
		ObjectType->accessSpecifiers.PushLast(ForeignRuntimeMetadata);
		ASSERT_THAT(AreEqual(PreviousCount + 1,
			ObjectType->accessSpecifiers.GetLength(),
			TEXT("the fixture must install one Runtime-only access DTO")));
		if (ObjectType->accessSpecifiers.GetLength() != PreviousCount + 1)
		{
			asDELETE(ForeignRuntimeMetadata, asSAccessSpecifier);
			return;
		}

		ASSERT_THAT(IsTrue(Builder.BuildGenerateFunctions() < 0,
			TEXT("Canonical Stage 2 must reject Runtime access metadata that has no exact declaration in the Canonical record")));
		ASSERT_THAT(IsTrue(ObjectType->accessSpecifiers.GetLength() == 1
			&& ObjectType->accessSpecifiers[0] == ForeignRuntimeMetadata,
			TEXT("rejection must leave the pre-existing Runtime owner unchanged")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->properties.GetLength(),
			TEXT("a failed Canonical class projection must stop before publishing member shells")));
	}

	TEST_METHOD(PreparedForeignFieldAccessEdgePublishesNoPropertyShell)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalForeignFieldAccessEdge");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FAccessOwnerA
			{
				access Internal = private;
				access:Internal int Value;
			}

			class FAccessOwnerB
			{
				access External = protected;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalForeignFieldAccessEdge.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		asCASTContext* const Context = Builder.canonicalAST;
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}

		asASTDeclId FieldId;
		asASTDeclId ForeignSpecifierId;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			const asCDecl* const Owner = Candidate != nullptr
				? Context->GetDecl(Candidate->parent) : nullptr;
			if (Candidate != nullptr && Owner != nullptr
				&& Candidate->kind == asAST_DECL_VAR
				&& Candidate->name == "Value"
				&& Owner->name == "FAccessOwnerA")
			{
				FieldId = Candidate->id;
			}
			if (Candidate != nullptr && Owner != nullptr
				&& Candidate->kind == asAST_DECL_ACCESS_SPECIFIER
				&& Candidate->name == "External"
				&& Owner->name == "FAccessOwnerB")
			{
				ForeignSpecifierId = Candidate->id;
			}
		}
		ASSERT_THAT(IsTrue(FieldId.IsValid()
			&& ForeignSpecifierId.IsValid(),
			TEXT("fixture must expose an exact field and a foreign access declaration")));
		if (!FieldId.IsValid() || !ForeignSpecifierId.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK,
			Context->SetDeclAccessSpecifier(FieldId, ForeignSpecifierId),
			TEXT("the unsealed fixture must admit an invalid edge so Stage 2 can prove fail-closed publication")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FAccessOwnerA"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->properties.GetLength()));

		ASSERT_THAT(IsTrue(Builder.BuildGenerateFunctions() < 0,
			TEXT("a foreign exact field edge must be rejected during Canonical Stage 2")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->properties.GetLength(),
			TEXT("field access resolution must complete before the Runtime property shell is published")));
	}

	TEST_METHOD(PreparedDanglingAccessOwnerChildFailsBeforeZeroDefinitionSuccess)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalDanglingAccessOwnerChild");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FAccessDanglingOwnerChildProbe
			{
				int Value;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalDanglingAccessOwnerChild.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		asCASTContext* const Context = Builder.canonicalAST;
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}

		asASTDeclId OwnerId;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_CLASS
				&& Candidate->name == "FAccessDanglingOwnerChildProbe")
			{
				OwnerId = Candidate->id;
				break;
			}
		}
		ASSERT_THAT(IsTrue(OwnerId.IsValid(),
			TEXT("fixture must expose the exact Canonical class owner")));
		if (!OwnerId.IsValid())
		{
			return;
		}
		const asASTDeclId DanglingChild(Context->GetDeclCount() + 100);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK,
			Context->AddDeclChild(OwnerId, DanglingChild),
			TEXT("the unsealed fixture must admit a dangling child for Stage 2 validation")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FAccessDanglingOwnerChildProbe"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Builder.BuildGenerateFunctions() < 0,
			TEXT("a dangling Canonical owner child must not be mistaken for an empty access-definition inventory")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->accessSpecifiers.GetLength(),
			TEXT("dangling-child rejection must publish no Runtime access aggregate")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->properties.GetLength(),
			TEXT("dangling-child rejection must stop before member shell publication")));
	}

	TEST_METHOD(PreparedDuplicateAccessDefinitionNamesPublishNoRuntimeAggregate)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalDuplicateAccessDefinitionNames");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FAccessDuplicateDefinitionProbe
			{
				access Internal = private;
				access External = protected;
				int Value;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalDuplicateAccessDefinitionNames.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		asCASTContext* const Context = Builder.canonicalAST;
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}

		asASTDeclId ExternalSpecifierId;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			const asCDecl* const Owner = Candidate != nullptr
				? Context->GetDecl(Candidate->parent) : nullptr;
			if (Candidate != nullptr && Owner != nullptr
				&& Candidate->kind == asAST_DECL_ACCESS_SPECIFIER
				&& Candidate->name == "External"
				&& Owner->name == "FAccessDuplicateDefinitionProbe")
			{
				ExternalSpecifierId = Candidate->id;
				break;
			}
		}
		ASSERT_THAT(IsTrue(ExternalSpecifierId.IsValid(),
			TEXT("fixture must expose the second exact access definition")));
		if (!ExternalSpecifierId.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK,
			Context->SetDeclName(ExternalSpecifierId, "Internal"),
			TEXT("the unsealed fixture must admit duplicate names for Stage 2 validation")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FAccessDuplicateDefinitionProbe"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Builder.BuildGenerateFunctions() < 0,
			TEXT("different Canonical access declarations with the same name must fail closed like Parser/Sema and LEGACY registration")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->accessSpecifiers.GetLength(),
			TEXT("duplicate-name rejection must publish no Runtime access aggregate")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->properties.GetLength(),
			TEXT("duplicate-name rejection must stop before member shell publication")));
	}

	TEST_METHOD(PreparedDuplicateAccessPermissionDeclIdPublishesNoRuntimeAggregate)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalDuplicateAccessPermission");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FAccessDuplicatePermissionProbe
			{
				access Internal = private, Friend(readonly);
				int Value;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalDuplicateAccessPermission.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		asCASTContext* const Context = Builder.canonicalAST;
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}

		asASTDeclId SpecifierId;
		asASTDeclId PermissionId;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			const asCDecl* const Owner = Candidate != nullptr
				? Context->GetDecl(Candidate->parent) : nullptr;
			if (Candidate != nullptr && Owner != nullptr
				&& Candidate->kind == asAST_DECL_ACCESS_SPECIFIER
				&& Candidate->name == "Internal"
				&& Owner->name == "FAccessDuplicatePermissionProbe")
			{
				SpecifierId = Candidate->id;
			}
			if (Candidate != nullptr && Owner != nullptr
				&& Candidate->kind == asAST_DECL_ACCESS_PERMISSION
				&& Candidate->name == "Friend")
			{
				PermissionId = Candidate->id;
			}
		}
		ASSERT_THAT(IsTrue(SpecifierId.IsValid() && PermissionId.IsValid(),
			TEXT("fixture must expose one exact access definition and permission")));
		if (!SpecifierId.IsValid() || !PermissionId.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK,
			Context->AddDeclChild(SpecifierId, PermissionId),
			TEXT("the unsealed fixture must admit a duplicate child identity for Stage 2 validation")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FAccessDuplicatePermissionProbe"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Builder.BuildGenerateFunctions() < 0,
			TEXT("one Canonical permission declaration listed twice must fail closed instead of publishing two Runtime entries")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->accessSpecifiers.GetLength(),
			TEXT("duplicate-permission rejection must publish no Runtime access aggregate")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->properties.GetLength(),
			TEXT("duplicate-permission rejection must stop before member shell publication")));
	}

	TEST_METHOD(PreparedAccessSpecifierSelfIdMismatchPublishesNoRuntimeAggregate)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalAccessSpecifierSelfIdMismatch");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FAccessSpecifierSelfIdProbe
			{
				access Internal = private;
				int Value;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalAccessSpecifierSelfIdMismatch.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		asCASTContext* const Context = Builder.canonicalAST;
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}

		asASTDeclId SpecifierId;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			const asCDecl* const Owner = Candidate != nullptr
				? Context->GetDecl(Candidate->parent) : nullptr;
			if (Candidate != nullptr && Owner != nullptr
				&& Candidate->kind == asAST_DECL_ACCESS_SPECIFIER
				&& Candidate->name == "Internal"
				&& Owner->name == "FAccessSpecifierSelfIdProbe")
			{
				SpecifierId = asASTDeclId(DeclIndex);
				break;
			}
		}
		ASSERT_THAT(IsTrue(SpecifierId.IsValid(),
			TEXT("fixture must expose the indexed Canonical access definition")));
		asCDecl* const MutableSpecifier = SpecifierId.IsValid()
			? const_cast<asCDecl*>(Context->GetDecl(SpecifierId)) : nullptr;
		ASSERT_THAT(IsNotNull(MutableSpecifier));
		if (MutableSpecifier == nullptr)
		{
			return;
		}
		MutableSpecifier->id = asASTDeclId(Context->GetDeclCount() + 100);

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FAccessSpecifierSelfIdProbe"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Builder.BuildGenerateFunctions() < 0,
			TEXT("an access declaration whose embedded identity disagrees with its arena index must fail closed")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->accessSpecifiers.GetLength(),
			TEXT("access self-id rejection must publish no Runtime access aggregate")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->properties.GetLength(),
			TEXT("access self-id rejection must stop before member shell publication")));
	}

	TEST_METHOD(PreparedAccessPermissionSelfIdMismatchPublishesNoRuntimeAggregate)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalAccessPermissionSelfIdMismatch");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FAccessPermissionSelfIdProbe
			{
				access Internal = private, Friend(readonly);
				int Value;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalAccessPermissionSelfIdMismatch.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		asCASTContext* const Context = Builder.canonicalAST;
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}

		asASTDeclId PermissionId;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			const asCDecl* const Owner = Candidate != nullptr
				? Context->GetDecl(Candidate->parent) : nullptr;
			const asCDecl* const ClassOwner = Owner != nullptr
				? Context->GetDecl(Owner->parent) : nullptr;
			if (Candidate != nullptr && Owner != nullptr
				&& ClassOwner != nullptr
				&& Candidate->kind == asAST_DECL_ACCESS_PERMISSION
				&& Candidate->name == "Friend"
				&& Owner->kind == asAST_DECL_ACCESS_SPECIFIER
				&& Owner->name == "Internal"
				&& ClassOwner->name == "FAccessPermissionSelfIdProbe")
			{
				PermissionId = asASTDeclId(DeclIndex);
				break;
			}
		}
		ASSERT_THAT(IsTrue(PermissionId.IsValid(),
			TEXT("fixture must expose the indexed Canonical access permission")));
		asCDecl* const MutablePermission = PermissionId.IsValid()
			? const_cast<asCDecl*>(Context->GetDecl(PermissionId)) : nullptr;
		ASSERT_THAT(IsNotNull(MutablePermission));
		if (MutablePermission == nullptr)
		{
			return;
		}
		MutablePermission->id = asASTDeclId(Context->GetDeclCount() + 100);

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FAccessPermissionSelfIdProbe"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Builder.BuildGenerateFunctions() < 0,
			TEXT("a permission whose embedded identity disagrees with its arena index must fail closed")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->accessSpecifiers.GetLength(),
			TEXT("permission self-id rejection must publish no Runtime access aggregate")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->properties.GetLength(),
			TEXT("permission self-id rejection must stop before member shell publication")));
	}

	TEST_METHOD(PreparedAccessOwnerSelfIdMismatchPublishesNoRuntimeAggregate)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalAccessOwnerSelfIdMismatch");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FAccessOwnerSelfIdProbe
			{
				access Internal = private;
				access:Internal int Value;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalAccessOwnerSelfIdMismatch.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		asCASTContext* const Context = Builder.canonicalAST;
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}

		asASTDeclId OwnerId;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_CLASS
				&& Candidate->name == "FAccessOwnerSelfIdProbe")
			{
				OwnerId = asASTDeclId(DeclIndex);
				break;
			}
		}
		ASSERT_THAT(IsTrue(OwnerId.IsValid(),
			TEXT("fixture must expose the indexed Canonical class owner")));
		asCDecl* const MutableOwner = OwnerId.IsValid()
			? const_cast<asCDecl*>(Context->GetDecl(OwnerId)) : nullptr;
		ASSERT_THAT(IsNotNull(MutableOwner));
		if (MutableOwner == nullptr)
		{
			return;
		}
		MutableOwner->id = asASTDeclId(Context->GetDeclCount() + 100);

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FAccessOwnerSelfIdProbe"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Builder.BuildGenerateFunctions() < 0,
			TEXT("a class owner whose embedded identity disagrees with its arena index must fail closed")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->accessSpecifiers.GetLength(),
			TEXT("owner self-id rejection must publish no Runtime access aggregate")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->properties.GetLength(),
			TEXT("owner self-id rejection must stop before member shell publication")));
	}

	TEST_METHOD(PreparedAccessFieldSelfIdMismatchPublishesNoPropertyShell)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalAccessFieldSelfIdMismatch");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FAccessFieldSelfIdProbe
			{
				access Internal = private;
				access:Internal int Value;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalAccessFieldSelfIdMismatch.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		asCASTContext* const Context = Builder.canonicalAST;
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}

		asASTDeclId FieldId;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			const asCDecl* const Owner = Candidate != nullptr
				? Context->GetDecl(Candidate->parent) : nullptr;
			if (Candidate != nullptr && Owner != nullptr
				&& Candidate->kind == asAST_DECL_VAR
				&& Candidate->name == "Value"
				&& Owner->name == "FAccessFieldSelfIdProbe")
			{
				FieldId = asASTDeclId(DeclIndex);
				break;
			}
		}
		ASSERT_THAT(IsTrue(FieldId.IsValid(),
			TEXT("fixture must expose the indexed Canonical field")));
		asCDecl* const MutableField = FieldId.IsValid()
			? const_cast<asCDecl*>(Context->GetDecl(FieldId)) : nullptr;
		ASSERT_THAT(IsNotNull(MutableField));
		if (MutableField == nullptr)
		{
			return;
		}
		MutableField->id = asASTDeclId(Context->GetDeclCount() + 100);

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FAccessFieldSelfIdProbe"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->properties.GetLength()));

		ASSERT_THAT(IsTrue(Builder.BuildGenerateFunctions() < 0,
			TEXT("a field whose embedded identity disagrees with its arena index must fail closed")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ObjectType->properties.GetLength(),
			TEXT("field self-id authentication must complete before Runtime property publication")));
	}

	TEST_METHOD(PreparedAccessMethodSelfIdMismatchPublishesNoMethodShell)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalAccessMethodSelfIdMismatch");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FAccessMethodSelfIdProbe
			{
				access Internal = private;
				access:Internal int Read() { return 42; }
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalAccessMethodSelfIdMismatch.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		asCASTContext* const Context = Builder.canonicalAST;
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}

		asASTDeclId MethodId;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			const asCDecl* const Owner = Candidate != nullptr
				? Context->GetDecl(Candidate->parent) : nullptr;
			if (Candidate != nullptr && Owner != nullptr
				&& Candidate->kind == asAST_DECL_METHOD
				&& Candidate->name == "Read"
				&& Owner->name == "FAccessMethodSelfIdProbe")
			{
				MethodId = asASTDeclId(DeclIndex);
				break;
			}
		}
		ASSERT_THAT(IsTrue(MethodId.IsValid(),
			TEXT("fixture must expose the indexed Canonical method")));
		asCDecl* const MutableMethod = MethodId.IsValid()
			? const_cast<asCDecl*>(Context->GetDecl(MethodId)) : nullptr;
		ASSERT_THAT(IsNotNull(MutableMethod));
		if (MutableMethod == nullptr)
		{
			return;
		}
		MutableMethod->id = asASTDeclId(Context->GetDeclCount() + 100);

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FAccessMethodSelfIdProbe"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNull(ObjectType->GetMethodByDecl("int Read()")));

		ASSERT_THAT(IsTrue(Builder.BuildGenerateFunctions() < 0,
			TEXT("a method whose embedded identity disagrees with its arena index must fail closed")));
		ASSERT_THAT(IsNull(ObjectType->GetMethodByDecl("int Read()"),
			TEXT("method self-id authentication must complete before Runtime method publication")));
	}

	TEST_METHOD(PreparedForeignMethodAccessEdgePublishesNoMethodShell)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalForeignMethodAccessEdge");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FMethodAccessOwnerA
			{
				access Internal = private;
				access:Internal int Read() { return 42; }
			}

			class FMethodAccessOwnerB
			{
				access External = protected;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "PreparedCanonicalForeignMethodAccessEdge.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		asCASTContext* const Context = Builder.canonicalAST;
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}

		asASTDeclId MethodId;
		asASTDeclId ForeignSpecifierId;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			const asCDecl* const Owner = Candidate != nullptr
				? Context->GetDecl(Candidate->parent) : nullptr;
			if (Candidate != nullptr && Owner != nullptr
				&& Candidate->kind == asAST_DECL_METHOD
				&& Candidate->name == "Read"
				&& Owner->name == "FMethodAccessOwnerA")
			{
				MethodId = Candidate->id;
			}
			if (Candidate != nullptr && Owner != nullptr
				&& Candidate->kind == asAST_DECL_ACCESS_SPECIFIER
				&& Candidate->name == "External"
				&& Owner->name == "FMethodAccessOwnerB")
			{
				ForeignSpecifierId = Candidate->id;
			}
		}
		ASSERT_THAT(IsTrue(MethodId.IsValid()
			&& ForeignSpecifierId.IsValid(),
			TEXT("fixture must expose an exact method and a foreign access declaration")));
		if (!MethodId.IsValid() || !ForeignSpecifierId.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK,
			Context->SetDeclAccessSpecifier(MethodId, ForeignSpecifierId),
			TEXT("the unsealed fixture must admit an invalid edge so Stage 2 can prove fail-closed publication")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FMethodAccessOwnerA"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNull(ObjectType->GetMethodByDecl("int Read()")));

		ASSERT_THAT(IsTrue(Builder.BuildGenerateFunctions() < 0,
			TEXT("a foreign exact method edge must be rejected during Canonical Stage 2")));
		ASSERT_THAT(IsNull(ObjectType->GetMethodByDecl("int Read()"),
			TEXT("method access resolution must complete before the Runtime method shell is published")));
	}

	TEST_METHOD(PreparedInterfaceShellsAreAuthenticatedBySealedRelations)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalInterfaceRelations");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			interface IPreparedBaseProbe
			{
				int ProbeBase(int Value);
			}

			interface IPreparedDerivedProbe : IPreparedBaseProbe
			{
				int ProbeDerived(bool Value);
			}

			class PreparedInterfaceProbe : IPreparedDerivedProbe
			{
				int ProbeBase(int Value)
				{
					return Value + 1;
				}

				int ProbeDerived(bool Value)
				{
					return Value ? 40 : 0;
				}
			}

			int RunPreparedInterfaceDispatch()
			{
				PreparedInterfaceProbe Object;
				IPreparedDerivedProbe View = Object;
				return View.ProbeBase(1) + View.ProbeDerived(true);
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalInterfaceRelations.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("Stage 2 must create the complete prepared interface graph")));

		asCObjectType* const BaseInterface = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IPreparedBaseProbe"));
		asCObjectType* const DerivedInterface = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IPreparedDerivedProbe"));
		asCObjectType* const PreparedType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("PreparedInterfaceProbe"));
		asCScriptFunction* const Entry = static_cast<asCScriptFunction*>(
			Module->GetFunctionByDecl("int RunPreparedInterfaceDispatch()"));
		ASSERT_THAT(IsNotNull(BaseInterface));
		ASSERT_THAT(IsNotNull(DerivedInterface));
		ASSERT_THAT(IsNotNull(PreparedType));
		ASSERT_THAT(IsNotNull(Entry));
		if (BaseInterface == nullptr || DerivedInterface == nullptr
			|| PreparedType == nullptr || Entry == nullptr)
		{
			return;
		}

		asCScriptFunction* const BaseRequirement =
			static_cast<asCScriptFunction*>(BaseInterface->GetMethodByDecl(
				"int ProbeBase(int)"));
		asCScriptFunction* const DerivedRequirement =
			static_cast<asCScriptFunction*>(DerivedInterface->GetMethodByDecl(
				"int ProbeDerived(bool)"));
		asCScriptFunction* const BaseImplementation =
			static_cast<asCScriptFunction*>(PreparedType->GetMethodByDecl(
				"int ProbeBase(int)"));
		asCScriptFunction* const DerivedImplementation =
			static_cast<asCScriptFunction*>(PreparedType->GetMethodByDecl(
				"int ProbeDerived(bool)"));
		ASSERT_THAT(IsNotNull(BaseRequirement));
		ASSERT_THAT(IsNotNull(DerivedRequirement));
		ASSERT_THAT(IsNotNull(BaseImplementation));
		ASSERT_THAT(IsNotNull(DerivedImplementation));
		if (BaseRequirement == nullptr || DerivedRequirement == nullptr
			|| BaseImplementation == nullptr || DerivedImplementation == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			BaseRequirement->funcType == asFUNC_INTERFACE
				&& DerivedRequirement->funcType == asFUNC_INTERFACE
				&& BaseRequirement->scriptData == nullptr
				&& DerivedRequirement->scriptData == nullptr,
			TEXT("Stage 2 interface requirements must remain declaration-only shells")));
		ASSERT_THAT(IsTrue(
			BaseRequirement->objectType == BaseInterface
				&& DerivedRequirement->objectType == DerivedInterface,
			TEXT("each declaration-only shell must belong to its exact interface")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2),
			PreparedType->interfaces.GetLength(),
			TEXT("the prepared class must publish direct plus transitive interfaces")));
		ASSERT_THAT(AreEqual(PreparedType->interfaces.GetLength(),
			PreparedType->interfaceVFTOffsets.GetLength()));
		ASSERT_THAT(IsTrue(
			PreparedType->interfaces.GetLength() == 2
				&& PreparedType->interfaces[0] == DerivedInterface
				&& PreparedType->interfaces[1] == BaseInterface,
			TEXT("Stage 2 must use the legacy-compatible exact DFS closure")));
		ASSERT_THAT(IsTrue(
			Entry->scriptData != nullptr
				&& Entry->scriptData->byteCode.GetLength() == 0,
			TEXT("the prepared entry body must still be unpublished")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* BaseRequirementDecl = nullptr;
		const asCDecl* DerivedRequirementDecl = nullptr;
		for (asUINT DeclIndex = 1; DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate == nullptr || Candidate->kind != asAST_DECL_METHOD)
			{
				continue;
			}
			const asCDecl* const Owner = Context->GetDecl(Candidate->parent);
			if (Owner != nullptr && Owner->kind == asAST_DECL_INTERFACE
				&& Owner->name.Equals("IPreparedBaseProbe")
				&& Candidate->name.Equals("ProbeBase"))
			{
				BaseRequirementDecl = Candidate;
			}
			if (Owner != nullptr && Owner->kind == asAST_DECL_INTERFACE
				&& Owner->name.Equals("IPreparedDerivedProbe")
				&& Candidate->name.Equals("ProbeDerived"))
			{
				DerivedRequirementDecl = Candidate;
			}
		}
		ASSERT_THAT(IsNotNull(BaseRequirementDecl));
		ASSERT_THAT(IsNotNull(DerivedRequirementDecl));
		if (BaseRequirementDecl == nullptr || DerivedRequirementDecl == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			BaseRequirement->canonicalASTStableDeclKey
				== BaseRequirementDecl->stableKey
				&& DerivedRequirement->canonicalASTStableDeclKey
					== DerivedRequirementDecl->stableKey,
			TEXT("bodyless interface shells must carry their exact sealed identity")));

		// Source coordinates belong to the detached body publication transaction,
		// not to read-only authentication of the Builder-owned Stage 2 shell. Use
		// an explicit unpublished sentinel so the negative graph gate proves that
		// it cannot leak otherwise-valid metadata before rejecting the graph.
		Entry->scriptData->scriptSectionIdx = -1;
		Entry->scriptData->declaredAt = 0;

		// Poison only the non-owning interface ordering. A prepared graph gate must
		// reject this before detaching a body or publishing module provenance, and
		// it must not attempt to repair or reinstall the Builder-owned graph.
		asCObjectType* const OriginalFirstInterface = PreparedType->interfaces[0];
		asCObjectType* const OriginalSecondInterface = PreparedType->interfaces[1];
		PreparedType->interfaces[0] = OriginalSecondInterface;
		PreparedType->interfaces[1] = OriginalFirstInterface;
		asCBytecodeCodeGen RejectedCodeGen;
		const int RejectedResult =
			RejectedCodeGen.GeneratePreparedModule(*Context, Builder);
		PreparedType->interfaces[0] = OriginalFirstInterface;
		PreparedType->interfaces[1] = OriginalSecondInterface;
		ASSERT_THAT(AreEqual(asINVALID_DECLARATION, RejectedResult,
			TEXT("a Stage 2 graph that disagrees with sealed relations must fail closed")));
		if (RejectedResult != asINVALID_DECLARATION)
		{
			return;
		}
		const FString RejectedDetail = UTF8_TO_TCHAR(
			RejectedCodeGen.GetFailureDetail().AddressOf()
				? RejectedCodeGen.GetFailureDetail().AddressOf() : "");
		ASSERT_THAT(IsTrue(
			RejectedDetail.Contains(TEXT("prepared interface dispatch graph"))
				&& RejectedDetail.Contains(TEXT("interface-order mismatch")),
			*FString::Printf(
				TEXT("the rejection must diagnose the exact interface-order mismatch; detail=%s"),
				*RejectedDetail)));
		ASSERT_THAT(IsTrue(
			Entry->scriptData != nullptr
				&& Entry->scriptData->byteCode.GetLength() == 0
				&& Entry->scriptData->scriptSectionIdx == -1
				&& Entry->scriptData->declaredAt == 0
				&& Module->GetLastBytecodePublisher()
					== asBYTECODE_PUBLISHER_NONE,
			TEXT("graph authentication failure must leave body, source metadata, and provenance unpublished")));

		asCBytecodeCodeGen CodeGen;
		const int CodeGenResult = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, CodeGenResult,
			*FString::Printf(
				TEXT("the exact prepared graph must authenticate: result=%d detail=%s"),
				CodeGenResult,
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (CodeGenResult != asAST_VERIFY_OK)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			BaseRequirement->funcType == asFUNC_INTERFACE
				&& DerivedRequirement->funcType == asFUNC_INTERFACE
				&& BaseRequirement->scriptData == nullptr
				&& DerivedRequirement->scriptData == nullptr,
			TEXT("authentication must not turn interface requirements into bodies")));
		ASSERT_THAT(IsTrue(
			PreparedType->interfaces.GetLength() == 2
				&& PreparedType->interfaces[0] == DerivedInterface
				&& PreparedType->interfaces[1] == BaseInterface,
			TEXT("successful authentication must preserve the exact Stage 2 graph")));
		ASSERT_THAT(IsTrue(
			DerivedRequirement->vfTableIdx >= 0
				&& BaseRequirement->vfTableIdx >= 0
				&& PreparedType->interfaceVFTOffsets[0]
					+ static_cast<asUINT>(DerivedRequirement->vfTableIdx)
						< PreparedType->virtualFunctionTable.GetLength()
				&& PreparedType->interfaceVFTOffsets[1]
					+ static_cast<asUINT>(BaseRequirement->vfTableIdx)
						< PreparedType->virtualFunctionTable.GetLength(),
			TEXT("each exact requirement must address its prepared dispatch chunk")));
		if (DerivedRequirement->vfTableIdx < 0 || BaseRequirement->vfTableIdx < 0)
		{
			return;
		}
		const asUINT DerivedDispatchIndex = PreparedType->interfaceVFTOffsets[0]
			+ static_cast<asUINT>(DerivedRequirement->vfTableIdx);
		const asUINT BaseDispatchIndex = PreparedType->interfaceVFTOffsets[1]
			+ static_cast<asUINT>(BaseRequirement->vfTableIdx);
		ASSERT_THAT(IsTrue(
			DerivedDispatchIndex < PreparedType->virtualFunctionTable.GetLength()
				&& PreparedType->virtualFunctionTable[DerivedDispatchIndex]
					== DerivedImplementation
				&& BaseDispatchIndex < PreparedType->virtualFunctionTable.GetLength()
				&& PreparedType->virtualFunctionTable[BaseDispatchIndex]
					== BaseImplementation,
			TEXT("the prepared chunks must retain their exact sealed implementations")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(
				Entry, asBC_CALLINTF),
			TEXT("the prepared interface-typed call must lower to CALLINTF")));
		ASSERT_THAT(IsTrue(
			Entry->scriptData != nullptr
				&& Entry->scriptData->scriptSectionIdx >= 0
				&& Entry->scriptData->declaredAt != 0,
			TEXT("successful detached publication must restore exact sealed source metadata")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Module->GetLastLegacyCompilerInvocationCount()));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner,
			Engine.Get(),
			Module,
			"int RunPreparedInterfaceDispatch()",
			Value) && Value == 42,
			*FString::Printf(
				TEXT("authenticated prepared dispatch must execute 42; got=%d"),
				Value)));
	}

	TEST_METHOD(PreparedInitDefaultsAuthenticatesAfterAuthoredMethodOrder)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalInitDefaultsOrder");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class PreparedDefaultsOrderProbe
			{
				int Value;
				default Value = 7;

				int Read()
				{
					return Value;
				}

				int Twice()
				{
					return Read() * 2;
				}
			}

			int RunPreparedDefaultsOrder()
			{
				PreparedDefaultsOrderProbe Object;
				return Object.Read();
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalInitDefaultsOrder.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module)));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("PreparedDefaultsOrderProbe"));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<asUINT>(3),
			ObjectType->methods.GetLength(),
			TEXT("Stage 2 must own two authored methods followed by one defaults helper")));
		if (ObjectType->methods.GetLength() != 3)
		{
			return;
		}
		auto ResolveMethod = [&](asUINT Index) -> asCScriptFunction*
		{
			const int FunctionId = ObjectType->methods[Index];
			return FunctionId >= 0
				&& static_cast<asUINT>(FunctionId)
					< ScriptEngine->scriptFunctions.GetLength()
					? ScriptEngine->scriptFunctions[
						static_cast<asUINT>(FunctionId)] : nullptr;
		};
		asCScriptFunction* const Read = ResolveMethod(0);
		asCScriptFunction* const Twice = ResolveMethod(1);
		asCScriptFunction* const InitDefaults = ResolveMethod(2);
		ASSERT_THAT(IsTrue(
			Read != nullptr && Read->name.Equals("Read")
				&& Twice != nullptr && Twice->name.Equals("Twice")
				&& InitDefaults != nullptr
				&& InitDefaults->name.Equals("__InitDefaults"),
			TEXT("the maintained Builder appends __InitDefaults after authored methods")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* DefaultsDecl = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_METHOD
				&& Candidate->name.Equals("__InitDefaults"))
			{
				DefaultsDecl = Candidate;
			}
		}
		ASSERT_THAT(IsTrue(
			DefaultsDecl != nullptr
				&& DefaultsDecl->origin.Equals("canonical-init-defaults")
				&& DefaultsDecl->body.IsValid(),
			TEXT("Sema must seal one exact defaults declaration and body")));

		asCBytecodeCodeGen CodeGen;
		const int CodeGenResult =
			CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, CodeGenResult,
			*FString::Printf(
				TEXT("the sealed defaults helper must authenticate in maintained Runtime order; result=%d detail=%s"),
				CodeGenResult,
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
	}

	TEST_METHOD(PreparedNamespaceSameNameTypeShellSwapFailsExactAuthentication)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalNamespaceTypeSwap");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			namespace PreparedLeft
			{
				class Same
				{
				}
			}

			namespace PreparedRight
			{
				class Same
				{
				}
			}

			int PreparedNamespaceTypeSwapEntry()
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalNamespaceTypeSwap.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("Stage 2 must create both namespace-distinct Runtime shells")));

		asCObjectType* const LeftType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByDecl("PreparedLeft::Same"));
		asCObjectType* const RightType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByDecl("PreparedRight::Same"));
		asCScriptFunction* const Entry = static_cast<asCScriptFunction*>(
			Module->GetFunctionByDecl("int PreparedNamespaceTypeSwapEntry()"));
		ASSERT_THAT(IsNotNull(LeftType));
		ASSERT_THAT(IsNotNull(RightType));
		ASSERT_THAT(IsNotNull(Entry));
		ASSERT_THAT(IsTrue(LeftType != RightType,
			TEXT("namespace-distinct same-name declarations must own different Runtime shells")));
		if (LeftType == nullptr || RightType == nullptr || Entry == nullptr)
		{
			return;
		}

		sClassDeclaration* LeftProducer = nullptr;
		sClassDeclaration* RightProducer = nullptr;
		for (asUINT Index = 0; Index < Builder.classDeclarations.GetLength(); ++Index)
		{
			sClassDeclaration* const Candidate = Builder.classDeclarations[Index];
			if (Candidate != nullptr && Candidate->typeInfo == LeftType)
			{
				LeftProducer = Candidate;
			}
			if (Candidate != nullptr && Candidate->typeInfo == RightType)
			{
				RightProducer = Candidate;
			}
		}
		ASSERT_THAT(IsNotNull(LeftProducer));
		ASSERT_THAT(IsNotNull(RightProducer));
		if (LeftProducer == nullptr || RightProducer == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			LeftProducer->canonicalASTStableDeclKey.GetLength() > 0
				&& RightProducer->canonicalASTStableDeclKey.GetLength() > 0
				&& LeftProducer->canonicalASTStableDeclKey
					!= RightProducer->canonicalASTStableDeclKey,
			TEXT("each namespace-distinct producer must carry its exact sealed identity")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		Entry->scriptData->scriptSectionIdx = -1;
		Entry->scriptData->declaredAt = 0;

		// Keep each exact producer key in place while swapping only its Stage 2
		// Runtime shell. Short-name/kind validation cannot distinguish this graph;
		// the sealed type identity must authenticate the exact Runtime pointer.
		LeftProducer->typeInfo = RightType;
		RightProducer->typeInfo = LeftType;
		asCBytecodeCodeGen RejectedCodeGen;
		const int RejectedResult =
			RejectedCodeGen.GeneratePreparedModule(*Context, Builder);
		LeftProducer->typeInfo = LeftType;
		RightProducer->typeInfo = RightType;

		ASSERT_THAT(AreEqual(asINVALID_DECLARATION, RejectedResult,
			TEXT("a namespace-swapped Stage 2 type shell must fail exact sealed authentication")));
		if (RejectedResult != asINVALID_DECLARATION)
		{
			return;
		}
		const FString RejectedDetail = UTF8_TO_TCHAR(
			RejectedCodeGen.GetFailureDetail().AddressOf()
				? RejectedCodeGen.GetFailureDetail().AddressOf() : "");
		ASSERT_THAT(IsTrue(
			RejectedDetail.Contains(TEXT("sealed type"))
				&& RejectedDetail.Contains(TEXT("runtimeTypeMatch=0")),
			*FString::Printf(
				TEXT("the rejection must identify the exact sealed type mismatch; detail=%s"),
				*RejectedDetail)));
		ASSERT_THAT(IsTrue(
			Entry->scriptData != nullptr
				&& Entry->scriptData->byteCode.GetLength() == 0
				&& Entry->scriptData->scriptSectionIdx == -1
				&& Entry->scriptData->declaredAt == 0
				&& Module->GetLastBytecodePublisher()
					== asBYTECODE_PUBLISHER_NONE,
			TEXT("exact type authentication failure must publish no body, source metadata, or provenance")));

		asCBytecodeCodeGen CodeGen;
		const int CodeGenResult = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, CodeGenResult,
			*FString::Printf(
				TEXT("restoring exact Stage 2 type shells must allow retry: result=%d detail=%s"),
				CodeGenResult,
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (CodeGenResult != asAST_VERIFY_OK)
		{
			return;
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner,
			Engine.Get(),
			Module,
			"int PreparedNamespaceTypeSwapEntry()",
			Value) && Value == 42,
			*FString::Printf(
				TEXT("restored exact prepared shells must execute 42; got=%d"),
				Value)));
	}

	TEST_METHOD(PreparedImportedDependencyGenerationShadowsCoexistingPublishedType)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		FScopedNativeModule CurrentProvider(
			*TestRunner,
			Engine,
			"PreparedCanonicalDependencyProviderCurrent",
			ASTEST_AS_ANSI(R"AS(
				namespace PreparedDependency
				{
					class Payload
					{
						int Padding;
						int Value;
					}
				}
				)AS"));
		ASSERT_THAT(IsTrue(CurrentProvider.IsValid(),
			TEXT("the current dependency generation must publish before an old shell coexists")));
		asCModule* const CurrentProviderModule =
			static_cast<asCModule*>(CurrentProvider.Get());
		if (!CurrentProvider.IsValid() || CurrentProviderModule == nullptr)
		{
			return;
		}

		asCModule* const OldProviderModule = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalDependencyProviderOld");
		ASSERT_THAT(IsNotNull(OldProviderModule));
		if (OldProviderModule == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			ScriptEngine->DiscardModule("PreparedCanonicalDependencyProviderOld");
		};
		const std::string OldProviderSource = ASTEST_AS_ANSI(R"AS(
			namespace PreparedDependency
			{
				class Payload
				{
					int Padding;
				}
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*OldProviderModule,
			"PreparedCanonicalDependencyProviderOld.as",
			OldProviderSource.c_str())));
		ASSERT_THAT(IsNotNull(OldProviderModule->builder));
		if (OldProviderModule->builder == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner,
			*OldProviderModule->builder,
			OldProviderModule),
			TEXT("the old staged shell must coexist without republishing over the current provider")));
		asCTypeInfo* const OldPayload = static_cast<asCTypeInfo*>(
			OldProviderModule->GetTypeInfoByDecl("PreparedDependency::Payload"));
		asCTypeInfo* const CurrentPayload = static_cast<asCTypeInfo*>(
			CurrentProviderModule->GetTypeInfoByDecl("PreparedDependency::Payload"));
		ASSERT_THAT(IsNotNull(OldPayload));
		ASSERT_THAT(IsNotNull(CurrentPayload));
		ASSERT_THAT(IsTrue(
			OldPayload != nullptr && CurrentPayload != nullptr
				&& OldPayload != CurrentPayload,
			TEXT("old/current providers must expose distinct same-nominal Runtime revisions")));
		if (OldPayload == nullptr || CurrentPayload == nullptr)
		{
			return;
		}

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalDependencyConsumer");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		Module->ImportModule(CurrentProviderModule);
		ASSERT_THAT(IsTrue(
			Module->importedModules.GetLength() == 1
				&& Module->importedModules[0] == CurrentProviderModule
				&& !Module->importedModules.Exists(OldProviderModule),
			TEXT("the prepared consumer must name only the current dependency generation")));

		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
			int PreparedImportedDependencyProbe(
				PreparedDependency::Payload Value)
			{
				return 42;
			}

			int PreparedImportedDependencyEntry()
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalDependencyConsumer.as",
			ConsumerSource.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("Stage 2 must resolve the consumer through its exact imported provider")));
		asCScriptFunction* const Probe = static_cast<asCScriptFunction*>(
			Module->GetFunctionByName("PreparedImportedDependencyProbe"));
		asCScriptFunction* const Entry = static_cast<asCScriptFunction*>(
			Module->GetFunctionByDecl("int PreparedImportedDependencyEntry()"));
		ASSERT_THAT(IsNotNull(Probe));
		ASSERT_THAT(IsNotNull(Entry));
		if (Probe == nullptr || Entry == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			Probe->parameterTypes.GetLength() == 1
				&& Probe->parameterTypes[0].GetTypeInfo() == CurrentPayload
				&& Probe->parameterTypes[0].GetTypeInfo() != OldPayload,
			TEXT("Stage 2 must bind the imported handle ABI to the current dependency generation")));
		ASSERT_THAT(IsTrue(
			Entry->scriptData != nullptr
				&& Entry->scriptData->byteCode.GetLength() == 0,
			TEXT("Stage 2 must leave the consumer body unpublished")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* ProbeDecl = nullptr;
		const asCDecl* ProbeParam = nullptr;
		for (asUINT DeclIndex = 1; DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate = Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_FUNCTION
				&& Candidate->name.Equals("PreparedImportedDependencyProbe"))
			{
				ProbeDecl = Candidate;
				for (asUINT ChildIndex = 0;
					ChildIndex < Candidate->children.GetLength(); ++ChildIndex)
				{
					const asCDecl* const Child =
						Context->GetDecl(Candidate->children[ChildIndex]);
					if (Child != nullptr && Child->kind == asAST_DECL_PARAM)
					{
						ProbeParam = Child;
						break;
					}
				}
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ProbeDecl));
		ASSERT_THAT(IsNotNull(ProbeParam));
		ASSERT_THAT(IsTrue(
			Module->importedModules.GetLength() == 1
				&& Module->importedModules[0] == CurrentProviderModule,
			TEXT("the flattened current dependency authority must survive Stage 2 and sealing")));
		if (ProbeParam == nullptr)
		{
			return;
		}
		asCArray<asCTypeInfo*> CurrentDependencyTypes;
		CurrentDependencyTypes.PushLast(CurrentPayload);
		asCDataType ResolvedProbeParam;
		asCTypeInfo* ResolvedProbeType = nullptr;
		asCString ResolveDetail;
		const int ResolveResult = asCRuntimeTypeBindingTable::ResolveCanonicalType(
			ScriptEngine,
			*Context,
			ProbeParam->type,
			ResolvedProbeParam,
			ResolvedProbeType,
			&CurrentDependencyTypes,
			&ResolveDetail);
		const asCType* const CanonicalProbeParamType =
			Context->GetType(ProbeParam->type.type);
		ASSERT_THAT(IsTrue(
			CanonicalProbeParamType != nullptr
				&& CanonicalProbeParamType->kind == asAST_TYPE_REFERENCE_OBJECT,
			*FString::Printf(
				TEXT("Sema must copy the imported script class as a reference-object fact; kind=%d stable-key=%s"),
				CanonicalProbeParamType != nullptr
					? static_cast<int>(CanonicalProbeParamType->kind) : -1,
				CanonicalProbeParamType != nullptr
					? UTF8_TO_TCHAR(CanonicalProbeParamType->stableKey.AddressOf())
					: TEXT("<missing>"))));
		if (CanonicalProbeParamType == nullptr
			|| CanonicalProbeParamType->kind != asAST_TYPE_REFERENCE_OBJECT)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ResolveResult == asRUNTIME_TYPE_BIND_OK
				&& ResolvedProbeParam.IsValid()
				&& ResolvedProbeType == CurrentPayload,
			*FString::Printf(
				TEXT("the sealed parameter key must resolve to the exact current provider shell; result=%d detail=%s stable-key=%s runtime-flags=0x%llX"),
				ResolveResult,
				UTF8_TO_TCHAR(ResolveDetail.AddressOf()
					? ResolveDetail.AddressOf() : ""),
				CanonicalProbeParamType != nullptr
						? UTF8_TO_TCHAR(CanonicalProbeParamType->stableKey.AddressOf())
						: TEXT("<missing>"),
					static_cast<unsigned long long>(CurrentPayload->GetFlags()))));

		asCBytecodeCodeGen RejectedTypeAuthorityCodeGen;
		RejectedTypeAuthorityCodeGen.SetTestFailureBeforePreparedRuntimeTypeAppend(
			0, asOUT_OF_MEMORY);
		const int RejectedTypeAuthorityResult =
			RejectedTypeAuthorityCodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asOUT_OF_MEMORY, RejectedTypeAuthorityResult,
			TEXT("a failed transient Runtime type append must surface as OOM")));
		ASSERT_THAT(IsTrue(
			RejectedTypeAuthorityCodeGen
				.WasTestPreparedRuntimeTypeAppendFailureInjected(),
			TEXT("the focused transaction seam must reach the first imported type append")));
		ASSERT_THAT(IsTrue(
			Entry->scriptData != nullptr
				&& Entry->scriptData->byteCode.GetLength() == 0
				&& Module->GetLastBytecodePublisher()
					== asBYTECODE_PUBLISHER_NONE
				&& Module->importedModules.GetLength() == 1
				&& Module->importedModules[0] == CurrentProviderModule
				&& Probe->parameterTypes.GetLength() == 1
				&& Probe->parameterTypes[0].GetTypeInfo() == CurrentPayload,
			TEXT("transient authority OOM must not mutate the prepared module or its dependency generation")));

		asCBytecodeCodeGen CodeGen;
		const int CodeGenResult = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, CodeGenResult,
			*FString::Printf(
				TEXT("the imported current generation must shadow the coexisting old type: result=%d detail=%s"),
				CodeGenResult,
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (CodeGenResult != asAST_VERIFY_OK)
		{
			return;
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner,
			Engine.Get(),
			Module,
			"int PreparedImportedDependencyEntry()",
			Value) && Value == 42,
			*FString::Printf(
				TEXT("the consumer must publish alongside the current dependency ABI; got=%d"),
				Value)));
	}

	TEST_METHOD(PreparedNativeShadowBaseIsAuthenticatedAsPreClassRelation)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"CPreparedNativeShadowBase",
			0,
			asOBJ_REF | asOBJ_NOCOUNT) >= 0,
			TEXT("the PreClass fixture must register its exact native shadow root")));
		asCObjectType* const NativeShadowBase = static_cast<asCObjectType*>(
			ScriptEngine->GetTypeInfoByDecl("CPreparedNativeShadowBase"));
		ASSERT_THAT(IsNotNull(NativeShadowBase));
		if (NativeShadowBase == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalNativeShadowBase");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		asPreClassData PreClassData;
		PreClassData.PropertyOffset = 16;
		PreClassData.ShadowType = NativeShadowBase;
		Module->AddPreClassData("FPreparedNativeShadowRef", PreClassData);
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class FPreparedNativeShadowRef
			{
				int Value;

				FPreparedNativeShadowRef(int InValue)
				{
					Value = InValue;
				}

				int Read() const
				{
					return Value;
				}
			}

			int RunPreparedNativeShadowRef()
			{
				FPreparedNativeShadowRef Value(42);
				return Value.Read();
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalNativeShadowBase.as",
			Source.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("Stage 2 must retain the PreClass shadow root separately from script inheritance")));
		asCObjectType* const PreparedType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("FPreparedNativeShadowRef"));
		asCScriptFunction* const Entry = static_cast<asCScriptFunction*>(
			Module->GetFunctionByDecl("int RunPreparedNativeShadowRef()"));
		ASSERT_THAT(IsNotNull(PreparedType));
		ASSERT_THAT(IsNotNull(Entry));
		if (PreparedType == nullptr || Entry == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			PreparedType->shadowType == NativeShadowBase
				&& PreparedType->derivedFrom == nullptr,
			TEXT("Stage 2 must represent a native PreClass root as shadowType, not derivedFrom")));
		ASSERT_THAT(IsTrue(Entry->scriptData != nullptr
			&& Entry->scriptData->byteCode.GetLength() == 0,
			TEXT("the prepared body must remain unpublished before Canonical CodeGen")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* ScriptRecord = nullptr;
		const asCDecl* ProjectedNativeBase = nullptr;
		for (asUINT DeclIndex = 1; DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_CLASS
				&& Candidate->name.Equals("FPreparedNativeShadowRef"))
			{
				ScriptRecord = Candidate;
			}
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_CLASS
				&& Candidate->stableKey.Equals("CPreparedNativeShadowBase")
				&& Candidate->origin.Equals("canonical-native-type-view"))
			{
				ProjectedNativeBase = Candidate;
			}
		}
		ASSERT_THAT(IsTrue(
			ScriptRecord != nullptr
				&& ProjectedNativeBase != nullptr
				&& ScriptRecord->bases.GetLength() == 1
				&& ScriptRecord->bases[0] == ProjectedNativeBase->id,
			TEXT("sealed Sema must own the exact pointer-free PreClass base relation")));

		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Result,
			*FString::Printf(
				TEXT("the exact PreClass shadow relation must authenticate: result=%d detail=%s"),
				Result,
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (Result != asAST_VERIFY_OK)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			PreparedType->shadowType == NativeShadowBase
				&& PreparedType->derivedFrom == nullptr,
			TEXT("authentication must preserve the Stage 2 PreClass representation")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Module->GetLastLegacyCompilerInvocationCount()));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner,
			Engine.Get(),
			Module,
			"int RunPreparedNativeShadowRef()",
			Value) && Value == 42,
			*FString::Printf(
				TEXT("the authenticated PreClass fixture must execute 42; got=%d"),
				Value)));
	}

	TEST_METHOD(PreparedImportBindsExactShellAndExecutes)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		FScopedNativeModule Provider(
			*TestRunner,
			Engine,
			"PreparedCanonicalImportProvider",
			ASTEST_AS_ANSI(R"AS(
				int PreparedSharedValue(int A = 1, int B = 2)
				{
					return A * 10 + B;
				}
				)AS"));
		ASSERT_THAT(IsTrue(Provider.IsValid(),
			TEXT("prepared-import provider must compile canonically")));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalImportConsumer");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
			import int PreparedSharedValue(int A = 1, int B = 2)
				from "PreparedCanonicalImportProvider";

			int PreparedImportEntry()
			{
				return PreparedSharedValue(B: 7);
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalImportConsumer.as",
			ConsumerSource.c_str())));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("Stage2 must create function and import Runtime shells")));
		ASSERT_THAT(AreEqual(1u, Module->GetImportedFunctionCount(),
			TEXT("Stage2 must own exactly one prepared import shell")));
		asCScriptFunction* const PreparedImport = Module->GetImportedFunction(0);
		ASSERT_THAT(IsNotNull(PreparedImport));
		if (PreparedImport == nullptr)
		{
			return;
		}
		const int PreparedImportId = PreparedImport->GetId();
		asCScriptFunction* const PreparedEntry =
			static_cast<asCScriptFunction*>(Module->GetFunctionByDecl(
				"int PreparedImportEntry()"));
		ASSERT_THAT(IsNotNull(PreparedEntry));
		if (PreparedEntry == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0u,
			PreparedEntry->scriptData->byteCode.GetLength(),
			TEXT("Stage2 caller shell must be empty before Canonical CodeGen")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* ImportDecl = nullptr;
		for (asUINT DeclIndex = 1; DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_IMPORT
				&& Candidate->name.Equals("PreparedSharedValue"))
			{
				ASSERT_THAT(IsNull(ImportDecl,
					TEXT("sealed graph must contain one exact import declaration")));
				ImportDecl = Candidate;
			}
		}
		ASSERT_THAT(IsNotNull(ImportDecl));
		if (ImportDecl == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ImportDecl->origin.Equals("PreparedCanonicalImportProvider"),
			TEXT("sealed import must own its exact source module")));
		bool bResolvedImportCall = false;
		for (asUINT ExprIndex = 1; ExprIndex <= Context->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr =
				Context->GetExpr(asASTExprId(ExprIndex));
			bResolvedImportCall |= Expr != nullptr
				&& Expr->kind == asAST_EXPR_CALL
				&& Expr->resolvedDecl == ImportDecl->id;
		}
		ASSERT_THAT(IsTrue(bResolvedImportCall,
			TEXT("call expression must consume the exact sealed import DeclId")));
		ASSERT_THAT(IsTrue(
			PreparedImport->canonicalASTStableDeclKey == ImportDecl->stableKey,
			TEXT("Stage2 must publish the producer-carried import declaration key")));
		ASSERT_THAT(IsTrue(
			PreparedImport->parameterTypes.GetLength() == 2
				&& PreparedImport->parameterTypes[0].IsPrimitive()
				&& PreparedImport->parameterTypes[0].IsReadOnly()
				&& !PreparedImport->parameterTypes[0].IsReference()
				&& PreparedImport->parameterTypes[1].IsPrimitive()
				&& PreparedImport->parameterTypes[1].IsReadOnly()
				&& !PreparedImport->parameterTypes[1].IsReference()
				&& PreparedImport->inOutFlags.GetLength() == 2
				&& PreparedImport->inOutFlags[0] == asTM_NONE
				&& PreparedImport->inOutFlags[1] == asTM_NONE,
			TEXT("prepared import matching must compare Builder's normalized primitive ABI")));
		ASSERT_THAT(IsTrue(
			PreparedImport->defaultArgs.GetLength() == 2
				&& PreparedImport->defaultArgs[0] != nullptr
				&& PreparedImport->defaultArgs[0]->Equals("1")
				&& PreparedImport->defaultArgs[1] != nullptr
				&& PreparedImport->defaultArgs[1]->Equals("2"),
			TEXT("Stage2 prepared import shell must retain both owning authored defaults")));

		const asCString OriginalImportKey =
			PreparedImport->canonicalASTStableDeclKey;
		PreparedImport->canonicalASTStableDeclKey =
			"poisoned-prepared-import-key";
		asCBytecodeCodeGen RejectedCodeGen;
		const int RejectedResult =
			RejectedCodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asINVALID_DECLARATION, RejectedResult,
			TEXT("a mismatched Stage2 import identity must fail closed")));
		const FString RejectedDetail = UTF8_TO_TCHAR(
			RejectedCodeGen.GetFailureDetail().AddressOf()
				? RejectedCodeGen.GetFailureDetail().AddressOf() : "");
		ASSERT_THAT(IsTrue(RejectedDetail.Contains(
			TEXT("prepared import must bind exactly one Stage2 shell")),
			*FString::Printf(
				TEXT("identity rejection must diagnose the exact import boundary; detail=%s"),
				*RejectedDetail)));
		ASSERT_THAT(IsTrue(
			RejectedDetail.Contains(TEXT("runtime-shells=1"))
				&& RejectedDetail.Contains(
					TEXT("key=poisoned-prepared-import-key"))
				&& RejectedDetail.Contains(
					TEXT("mismatches=stable-key")),
			*FString::Printf(
				TEXT("identity rejection must inventory the Stage2 shell and name the exact mismatched field; detail=%s"),
				*RejectedDetail)));
		ASSERT_THAT(IsTrue(
			Module->GetImportedFunctionCount() == 1
				&& Module->GetImportedFunction(0) == PreparedImport
				&& PreparedImport->GetId() == PreparedImportId
				&& PreparedEntry->scriptData->byteCode.GetLength() == 0,
			TEXT("failed prepared import binding must publish no body or import mutation")));
		PreparedImport->canonicalASTStableDeclKey = OriginalImportKey;

		asCBytecodeCodeGen CodeGen;
		const int CodeGenResult =
			CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, CodeGenResult,
			*FString::Printf(
				TEXT("prepared import CodeGen must consume the Stage2 shell: result=%d line=%d detail=%s"),
				CodeGenResult,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (CodeGenResult != asAST_VERIFY_OK)
		{
			return;
		}

		ASSERT_THAT(AreEqual(1u, Module->GetImportedFunctionCount(),
			TEXT("CodeGen must not duplicate or replace the prepared import slot")));
		ASSERT_THAT(IsTrue(Module->GetImportedFunction(0) == PreparedImport,
			TEXT("CodeGen must preserve the exact Stage2 import shell")));
		ASSERT_THAT(AreEqual(PreparedImportId, PreparedImport->GetId(),
			TEXT("CodeGen must preserve the authoritative imported FunctionId")));
		ASSERT_THAT(IsTrue(
			PreparedImport->canonicalASTStableDeclKey == ImportDecl->stableKey,
			TEXT("prepared import shell must retain the producer-carried sealed key")));

		asIScriptFunction* const ProviderFunction =
			Provider->GetFunctionByDecl(
				"int PreparedSharedValue(const int, const int)");
		ASSERT_THAT(IsNotNull(ProviderFunction));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Module->BindImportedFunction(0, ProviderFunction),
			TEXT("prepared import must retain the public binding contract")));

		asIScriptFunction* const Published =
			GetNativeFunctionByDecl(Module, "int PreparedImportEntry()");
		ASSERT_THAT(IsNotNull(Published));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(
				Published, asBC_CALLBND),
			TEXT("prepared caller must emit CALLBND for the imported FunctionId")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner,
			Engine.Get(),
			Module,
			"int PreparedImportEntry()",
			Value) && Value == 17,
			TEXT("bound prepared import must execute named B=7 with default A=1")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(0u,
			Module->GetLastLegacyCompilerInvocationCount()));
	}

	TEST_METHOD(PreparedAutomaticImportBindsSameBatchProviderWithoutImportSlot)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(
			asEP_AUTOMATIC_IMPORTS, 1)));

		asCModule* const Provider = CreateBuilderModule(
			ScriptEngine, "PreparedAutomaticImportProvider");
		asCModule* const Consumer = CreateBuilderModule(
			ScriptEngine, "PreparedAutomaticImportConsumer");
		ASSERT_THAT(IsNotNull(Provider));
		ASSERT_THAT(IsNotNull(Consumer));
		if (Provider == nullptr || Consumer == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Provider,
			"PreparedAutomaticImportProvider.as",
			"int PreparedAutomaticValue(int Value) { return Value; }")));
		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
			import int PreparedAutomaticValue(int Value) from "PreparedAutomaticImportProvider";

			int PreparedAutomaticImportEntry()
			{
				return PreparedAutomaticValue(41) + 1;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Consumer,
			"PreparedAutomaticImportConsumer.as",
			ConsumerSource.c_str())));
		ASSERT_THAT(IsNotNull(Provider->builder));
		ASSERT_THAT(IsNotNull(Consumer->builder));
		if (Provider->builder == nullptr || Consumer->builder == nullptr)
		{
			return;
		}

		asCBuilder& ProviderBuilder = *Provider->builder;
		asCBuilder& ConsumerBuilder = *Consumer->builder;
		// Match FAngelscriptEngine's same-batch ordering: every module is parsed
		// before any Stage2 function shell is created. The consumer therefore
		// seals an ImportDecl even though automatic-import mode deliberately owns
		// no mutable CALLBND slot.
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, ProviderBuilder,
			TEXT("Provider.Parse"), &asCBuilder::BuildParallelParseScripts,
			Provider)));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, ConsumerBuilder,
			TEXT("Consumer.Parse"), &asCBuilder::BuildParallelParseScripts,
			Consumer)));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, ProviderBuilder,
			TEXT("Provider.Types"), &asCBuilder::BuildGenerateTypes, Provider)));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, ConsumerBuilder,
			TEXT("Consumer.Types"), &asCBuilder::BuildGenerateTypes, Consumer)));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, ProviderBuilder,
			TEXT("Provider.Functions"), &asCBuilder::BuildGenerateFunctions,
			Provider)));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, ConsumerBuilder,
			TEXT("Consumer.Functions"), &asCBuilder::BuildGenerateFunctions,
			Consumer)));
		ASSERT_THAT(AreEqual(0u, Consumer->GetImportedFunctionCount(),
			TEXT("automatic imports must not allocate a mutable Runtime import slot")));

		auto FinishLayout = [this](asCBuilder& Builder, asCModule* Module,
			const TCHAR* Prefix)
		{
			if (!RunBuilderStage(*TestRunner, Builder,
				FString::Printf(TEXT("%s.Classes"), Prefix),
				&asCBuilder::BuildLayoutClasses, Module))
			{
				return false;
			}
			Builder.BuildAllocateGlobalVariables();
			return RunBuilderStage(*TestRunner, Builder,
				FString::Printf(TEXT("%s.FunctionLayout"), Prefix),
				&asCBuilder::BuildLayoutFunctions, Module);
		};
		ASSERT_THAT(IsTrue(FinishLayout(
			ProviderBuilder, Provider, TEXT("Provider"))));
		ASSERT_THAT(IsTrue(FinishLayout(
			ConsumerBuilder, Consumer, TEXT("Consumer"))));

		ASSERT_THAT(AreEqual(asSUCCESS, ProviderBuilder.SealCanonicalAST()));
		ASSERT_THAT(AreEqual(asSUCCESS, ConsumerBuilder.SealCanonicalAST()));
		asCASTContext* const ProviderContext =
			ProviderBuilder.TakeCanonicalAST();
		asCASTContext* const ConsumerContext =
			ConsumerBuilder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(ProviderContext));
		ASSERT_THAT(IsNotNull(ConsumerContext));
		if (ProviderContext == nullptr || ConsumerContext == nullptr)
		{
			if (ProviderContext != nullptr)
				asDELETE(ProviderContext, asCASTContext);
			if (ConsumerContext != nullptr)
				asDELETE(ConsumerContext, asCASTContext);
			return;
		}
		ON_SCOPE_EXIT
		{
			asDELETE(ProviderContext, asCASTContext);
			asDELETE(ConsumerContext, asCASTContext);
		};

		const asCDecl* ImportDecl = nullptr;
		for (asUINT Index = 1; Index <= ConsumerContext->GetDeclCount(); ++Index)
		{
			const asCDecl* const Candidate =
				ConsumerContext->GetDecl(asASTDeclId(Index));
			if (Candidate != nullptr && Candidate->kind == asAST_DECL_IMPORT
				&& Candidate->name.Equals("PreparedAutomaticValue"))
			{
				ImportDecl = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ImportDecl,
			TEXT("the source import must remain an explicit sealed semantic fact")));

		asCBytecodeCodeGen ProviderCodeGen;
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK,
			ProviderCodeGen.GeneratePreparedModule(
				*ProviderContext, ProviderBuilder),
			*FString::Printf(TEXT("provider CodeGen failed: %s"),
				UTF8_TO_TCHAR(ProviderCodeGen.GetFailureDetail().AddressOf()
					? ProviderCodeGen.GetFailureDetail().AddressOf() : ""))));

		asCBytecodeCodeGen ConsumerCodeGen;
		const int ConsumerResult = ConsumerCodeGen.GeneratePreparedModule(
			*ConsumerContext, ConsumerBuilder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, ConsumerResult,
			*FString::Printf(
				TEXT("same-batch automatic import must bind the exact provider shell without CALLBND: result=%d detail=%s"),
				ConsumerResult,
				UTF8_TO_TCHAR(ConsumerCodeGen.GetFailureDetail().AddressOf()
					? ConsumerCodeGen.GetFailureDetail().AddressOf() : ""))));
		if (ConsumerResult != asAST_VERIFY_OK)
		{
			return;
		}

		asIScriptFunction* const Published = GetNativeFunctionByDecl(
			Consumer, "int PreparedAutomaticImportEntry()");
		ASSERT_THAT(IsNotNull(Published));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(
				Published, asBC_CALL)
			&& !AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(
				Published, asBC_CALLBND),
			TEXT("automatic import must call the provider shell directly and allocate no CALLBND slot")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Consumer,
			"int PreparedAutomaticImportEntry()", Value) && Value == 42,
			TEXT("the same-batch automatic import must execute the exact provider body")));
	}

	TEST_METHOD(PreparedGeneratedAccessorClosurePublishesCallableMethods)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalGeneratedAccessor");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalGeneratedAccessor.as",
			R"AS(
				struct FPreparedAccessorPayload
				{
					int Value;
				}

				int RunPreparedGeneratedAccessor()
				{
					FPreparedAccessorPayload Payload;
					Payload.Value = 41;
					return Payload.Value + 1;
				}
			)AS")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("the staged Builder must prepare the authored Runtime shells")));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));

		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* PayloadClass = nullptr;
		const asCDecl* PayloadField = nullptr;
		const asCDecl* GetterDecl = nullptr;
		const asCDecl* SetterDecl = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_CLASS
				&& Candidate->name.Equals("FPreparedAccessorPayload"))
			{
				PayloadClass = Candidate;
				break;
			}
		}
		if (PayloadClass != nullptr)
		{
			for (asUINT ChildIndex = 0;
				ChildIndex < PayloadClass->children.GetLength(); ++ChildIndex)
			{
				const asCDecl* const Child =
					Context->GetDecl(PayloadClass->children[ChildIndex]);
				if (Child == nullptr)
				{
					continue;
				}
				if (Child->kind == asAST_DECL_VAR
					&& Child->name.Equals("Value"))
				{
					PayloadField = Child;
				}
				else if (Child->accessorKind == asAST_ACCESSOR_GET)
				{
					GetterDecl = Child;
				}
				else if (Child->accessorKind == asAST_ACCESSOR_SET)
				{
					SetterDecl = Child;
				}
			}
		}
		ASSERT_THAT(IsNotNull(PayloadClass));
		ASSERT_THAT(IsNotNull(PayloadField));
		ASSERT_THAT(IsNotNull(GetterDecl));
		ASSERT_THAT(IsNotNull(SetterDecl));
		if (PayloadField == nullptr || GetterDecl == nullptr || SetterDecl == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			GetterDecl->accessorField == PayloadField->id
				&& SetterDecl->accessorField == PayloadField->id,
			TEXT("sealed Sema accessors must name the exact backing-field declaration")));

		bool bResolvedGetterCall = false;
		bool bResolvedSetterCall = false;
		for (asUINT ExprIndex = 1;
			ExprIndex <= Context->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr =
				Context->GetExpr(asASTExprId(ExprIndex));
			if (Expr == nullptr || Expr->kind != asAST_EXPR_CALL)
			{
				continue;
			}
			bResolvedGetterCall |= Expr->resolvedDecl == GetterDecl->id;
			bResolvedSetterCall |= Expr->resolvedDecl == SetterDecl->id;
		}
		ASSERT_THAT(IsTrue(bResolvedGetterCall,
			TEXT("sealed property read must resolve to the generated getter declaration")));
		ASSERT_THAT(IsTrue(bResolvedSetterCall,
			TEXT("sealed property write must resolve to the generated setter declaration")));

		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Result,
			*FString::Printf(
				TEXT("prepared generated-accessor closure must publish: result=%d line=%d detail=%s"),
				Result,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (Result != asAST_VERIFY_OK)
		{
			return;
		}

		asITypeInfo* const TypeInfo =
			Module->GetTypeInfoByName("FPreparedAccessorPayload");
		ASSERT_THAT(IsNotNull(TypeInfo));
		if (TypeInfo == nullptr)
		{
			return;
		}
		asIScriptFunction* const Getter = TypeInfo->GetMethodByName("GetValue");
		asIScriptFunction* const Setter = TypeInfo->GetMethodByName("SetValue");
		asIScriptFunction* const Entry = Module->GetFunctionByDecl(
			"int RunPreparedGeneratedAccessor()");
		asIScriptFunction* GeneratedDestructor = nullptr;
		for (asUINT BehaviourIndex = 0;
			BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Candidate =
				TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
			if (Candidate != nullptr && Behaviour == asBEHAVE_DESTRUCT)
			{
				GeneratedDestructor = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(Getter,
			TEXT("prepared CodeGen must publish generated GetValue as an object method")));
		ASSERT_THAT(IsNotNull(Setter,
			TEXT("prepared CodeGen must publish generated SetValue as an object method")));
		ASSERT_THAT(IsNotNull(GeneratedDestructor,
			TEXT("prepared CodeGen must publish the Sema-generated value destructor atomically")));
		ASSERT_THAT(IsNotNull(Entry));
		if (Getter == nullptr || Setter == nullptr
			|| GeneratedDestructor == nullptr || Entry == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Getter, asBC_RDR4),
			TEXT("generated getter must own its sealed backing-field read body")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Setter, asBC_WRTV4),
			TEXT("generated setter must own its sealed backing-field write body")));
		ASSERT_THAT(IsTrue(ProdBytecodeCallsFunction(Entry, Getter->GetId()),
			TEXT("prepared caller must relocate its property read to GetValue")));
		ASSERT_THAT(IsTrue(ProdBytecodeCallsFunction(Entry, Setter->GetId()),
			TEXT("prepared caller must relocate its property write to SetValue")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Entry, GeneratedDestructor->GetId()),
			TEXT("prepared caller must consume the sealed scope-exit plan through the published destructor")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int RunPreparedGeneratedAccessor()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("prepared generated accessor closure must execute to 42; got=%d"),
				Value)));
	}

	TEST_METHOD(PreparedNativeNonPodMemberDefaultConstructsInExactFieldStorage)
	{
		using namespace AngelscriptBuilderTestSupport;

		GProdNativeLifecycleMemberConstructed = 0;
		GProdNativeLifecycleMemberDestructed = 0;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdNativeLifecycleMember",
				sizeof(FProdNativeLifecycleMember),
				asOBJ_VALUE | asOBJ_APP_CLASS_CD
					| asOBJ_APP_CLASS_ALLINTS) >= 0,
			TEXT("the gate requires a 32-byte non-POD native VALUE type")));
		const int NativeCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberConstruct),
			asCALL_GENERIC);
		const int NativeDtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_DESTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberDestruct),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(NativeCtorId > 0 && NativeDtorId > 0));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FProdNativeLifecycleMember",
				"int Stored",
				asOFFSET(FProdNativeLifecycleMember, Stored)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalNativeLifecycleMember");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalNativeLifecycleMember.as",
			R"AS(
				struct FPreparedNativeLifecycleOwner
				{
					FProdNativeLifecycleMember Inner;

					// Keep this gate scoped to member construction/destruction. Canonical
					// generated non-POD by-value accessors are a separate copy/assignment
					// slice and are deliberately suppressed by these unused names.
					void GetInner() {}
					void SetInner() {}
				}

				int RunPreparedNativeLifecycleMember()
				{
					FPreparedNativeLifecycleOwner Owner;
					return 42;
				}
			)AS")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("the staged Builder must prepare the owner and entry shells")));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* OwnerClass = nullptr;
		const asCDecl* InnerField = nullptr;
		const asCDecl* OwnerCtor = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_CLASS
				&& Candidate->name.Equals("FPreparedNativeLifecycleOwner"))
			{
				OwnerClass = Candidate;
				break;
			}
		}
		if (OwnerClass != nullptr)
		{
			for (asUINT ChildIndex = 0;
				ChildIndex < OwnerClass->children.GetLength(); ++ChildIndex)
			{
				const asCDecl* const Child =
					Context->GetDecl(OwnerClass->children[ChildIndex]);
				if (Child != nullptr && Child->kind == asAST_DECL_VAR
					&& Child->name.Equals("Inner"))
				{
					InnerField = Child;
				}
				else if (Child != nullptr
					&& Child->kind == asAST_DECL_CONSTRUCTOR)
				{
					OwnerCtor = Child;
				}
			}
		}
		ASSERT_THAT(IsNotNull(OwnerClass));
		ASSERT_THAT(IsNotNull(InnerField));
		ASSERT_THAT(IsNotNull(OwnerCtor));
		if (InnerField == nullptr || OwnerCtor == nullptr)
		{
			return;
		}

		const asCExpr* InitAssign = nullptr;
		const asCExpr* NativeConstruct = nullptr;
		for (asUINT InitIndex = 0;
			InitIndex < OwnerCtor->inits.GetLength(); ++InitIndex)
		{
			const asCExpr* const Candidate =
				Context->GetExpr(OwnerCtor->inits[InitIndex]);
			if (Candidate == nullptr || Candidate->kind != asAST_EXPR_ASSIGN
				|| Candidate->children.GetLength() != 2)
			{
				continue;
			}
			const asCExpr* const Lhs =
				Context->GetExpr(Candidate->children[0]);
			if (Lhs == nullptr || Lhs->kind != asAST_EXPR_DECL_REF
				|| Lhs->resolvedDecl != InnerField->id)
			{
				continue;
			}
			InitAssign = Candidate;
			NativeConstruct = Context->GetExpr(Candidate->children[1]);
			while (NativeConstruct != nullptr
				&& (NativeConstruct->kind == asAST_EXPR_CLEANUP
					|| NativeConstruct->kind == asAST_EXPR_MATERIALIZE_TEMPORARY)
				&& NativeConstruct->children.GetLength() == 1)
			{
				NativeConstruct = Context->GetExpr(
					NativeConstruct->children[0]);
			}
			break;
		}
		const asCDecl* const NativeCtor = NativeConstruct != nullptr
			? Context->GetDecl(NativeConstruct->resolvedDecl) : nullptr;
		const asCDecl* const NativeOwner = NativeCtor != nullptr
			? Context->GetDecl(NativeCtor->parent) : nullptr;
		ASSERT_THAT(IsTrue(
			InitAssign != nullptr
				&& NativeConstruct != nullptr
				&& NativeConstruct->kind == asAST_EXPR_CONSTRUCT
				&& NativeConstruct->type == InnerField->type
				&& NativeCtor != nullptr
				&& NativeCtor->kind == asAST_DECL_CONSTRUCTOR
				&& NativeOwner != nullptr
				&& NativeOwner->kind == asAST_DECL_CLASS
				&& NativeOwner->name.Equals("FProdNativeLifecycleMember"),
			TEXT("sealed Sema must bind the exact field-init construct to the native zero-arg constructor")));

		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Result,
			*FString::Printf(
				TEXT("prepared non-POD native member CodeGen must succeed: result=%d line=%d detail=%s"),
				Result,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (Result != asAST_VERIFY_OK)
		{
			return;
		}

		asITypeInfo* const OwnerType =
			Module->GetTypeInfoByName("FPreparedNativeLifecycleOwner");
		asIScriptFunction* RuntimeOwnerCtor = nullptr;
		asIScriptFunction* RuntimeOwnerDtor = nullptr;
		if (OwnerType != nullptr)
		{
			for (asUINT BehaviourIndex = 0;
				BehaviourIndex < OwnerType->GetBehaviourCount(); ++BehaviourIndex)
			{
				asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
				asIScriptFunction* const Candidate =
					OwnerType->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
				if (Candidate != nullptr && Behaviour == asBEHAVE_CONSTRUCT)
				{
					RuntimeOwnerCtor = Candidate;
				}
				else if (Candidate != nullptr && Behaviour == asBEHAVE_DESTRUCT)
				{
					RuntimeOwnerDtor = Candidate;
				}
			}
		}
		ASSERT_THAT(IsNotNull(RuntimeOwnerCtor));
		ASSERT_THAT(IsNotNull(RuntimeOwnerDtor));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(RuntimeOwnerCtor, NativeCtorId),
			TEXT("the owner constructor must CALLSYS the exact native constructor in field storage")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(RuntimeOwnerDtor, NativeDtorId),
			TEXT("the owner destructor must CALLSYS the exact native field destructor")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int RunPreparedNativeLifecycleMember()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("the owner scope containing the native member must execute; got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberConstructed,
			TEXT("the member must be constructed exactly once")));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberDestructed,
			TEXT("the member must be destroyed exactly once after owner scope exit")));
	}

	TEST_METHOD(PreparedNativeNonPodGeneratedGetterSealsAndInvokesExactCopyConstructor)
	{
		using namespace AngelscriptBuilderTestSupport;

		GProdNativeLifecycleMemberConstructed = 0;
		GProdNativeLifecycleMemberDestructed = 0;
		GProdNativeLifecycleMemberCopyConstructed = 0;
		GProdNativeLifecycleMemberCopiedValue = 0;
		GProdNativeLifecycleMemberRead = 0;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdNativeLifecycleMember",
				sizeof(FProdNativeLifecycleMember),
				asOBJ_VALUE | asOBJ_APP_CLASS_CD
					| asOBJ_APP_CLASS_ALLINTS) >= 0,
			TEXT("the accessor gate requires a 32-byte non-POD native VALUE type")));
		const int NativeCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberConstruct),
			asCALL_GENERIC);
		const int NativeCopyCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_CONSTRUCT,
			"void f(const FProdNativeLifecycleMember&in Other)",
			asFUNCTION(ProdNativeLifecycleMemberCopyConstruct),
			asCALL_GENERIC);
		const int NativeDtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_DESTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberDestruct),
			asCALL_GENERIC);
		const int NativeReadId = ScriptEngine->RegisterObjectMethod(
			"FProdNativeLifecycleMember",
			"int ReadStored() const",
			asFUNCTION(ProdNativeLifecycleMemberReadStored),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(
			NativeCtorId > 0 && NativeCopyCtorId > 0
				&& NativeDtorId > 0 && NativeReadId > 0));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FProdNativeLifecycleMember",
				"int Stored",
				asOFFSET(FProdNativeLifecycleMember, Stored)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalNativeNonPodGeneratedGetter");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalNativeNonPodGeneratedGetter.as",
			R"AS(
				struct FPreparedNativeGetterOwner
				{
					FProdNativeLifecycleMember Inner;

					// Suppress only the generated setter so this gate isolates the
					// getter's by-value copy-construction contract.
					void SetInner() {}
				}

				int RunPreparedNativeNonPodGeneratedGetter()
				{
					FPreparedNativeGetterOwner Owner;
					return Owner.Inner.ReadStored() + 1;
				}
			)AS")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("the staged Builder must prepare the owner and entry shells")));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* OwnerClass = nullptr;
		const asCDecl* InnerField = nullptr;
		const asCDecl* GetterDecl = nullptr;
		const asCDecl* EntryDecl = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_CLASS
				&& Candidate->name.Equals("FPreparedNativeGetterOwner"))
			{
				OwnerClass = Candidate;
			}
			else if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_FUNCTION
				&& Candidate->name.Equals(
					"RunPreparedNativeNonPodGeneratedGetter"))
			{
				EntryDecl = Candidate;
			}
		}
		if (OwnerClass != nullptr)
		{
			for (asUINT ChildIndex = 0;
				ChildIndex < OwnerClass->children.GetLength(); ++ChildIndex)
			{
				const asCDecl* const Child =
					Context->GetDecl(OwnerClass->children[ChildIndex]);
				if (Child != nullptr && Child->kind == asAST_DECL_VAR
					&& Child->name.Equals("Inner"))
				{
					InnerField = Child;
				}
				else if (Child != nullptr
					&& Child->kind == asAST_DECL_METHOD
					&& Child->name.Equals("GetInner")
					&& Child->accessorKind == asAST_ACCESSOR_GET)
				{
					GetterDecl = Child;
				}
			}
		}

		asCString RawDump;
		asCASTDump(*Context, RawDump);
		const FString Dump =
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(RawDump);
		ASSERT_THAT(IsTrue(
			InnerField != nullptr && GetterDecl != nullptr
				&& GetterDecl->accessorField == InnerField->id
				&& GetterDecl->type == InnerField->type,
			*FString::Printf(
				TEXT("sealed getter must name the exact non-POD backing field and return QualType. AST:\n%s"),
				*Dump)));
		ASSERT_THAT(IsNotNull(EntryDecl,
			*FString::Printf(
				TEXT("sealed AST must retain the authored entry declaration. AST:\n%s"),
				*Dump)));
		if (InnerField == nullptr || GetterDecl == nullptr
			|| EntryDecl == nullptr)
		{
			return;
		}

		class FReachableAccessorCallVisitor final : public asIASTConstVisitor
		{
		public:
			explicit FReachableAccessorCallVisitor(asASTDeclId InGetterDecl)
				: GetterDecl(InGetterDecl)
			{
			}

			bool EnterNode(
				const asCASTContext& InContext,
				const asSASTNodeRef& Node,
				const asSASTTraversalEdge* IncomingEdge,
				asUINT) override
			{
				if (Node.nodeClass != asAST_NODE_EXPR)
				{
					return true;
				}
				const asCExpr* const Expr = InContext.GetExpr(
					asASTExprId(Node.value, Node.snapshotOwner));
				if (Expr == nullptr || Expr->kind != asAST_EXPR_CALL)
				{
					return true;
				}
				if (Expr->resolvedDecl == GetterDecl)
				{
					// Canonical method calls deliberately expose the same effective
					// receiver through a named receiver edge and as the trailing
					// reverse-formal child. Generic traversal visits both structural
					// paths, so single evaluation is a count of unique expression IDs,
					// not a count of visitor entries.
					if (GetterCall == nullptr || GetterCall->id != Expr->id)
					{
						++GetterCallCount;
						GetterCall = Expr;
					}
					GetterTrace += FString::Printf(
						TEXT("E%u<-%s%u/role=%u/index=%u "),
						Node.value,
						IncomingEdge != nullptr
							? (IncomingEdge->source.nodeClass == asAST_NODE_DECL ? TEXT("D")
								: IncomingEdge->source.nodeClass == asAST_NODE_STMT ? TEXT("S")
								: IncomingEdge->source.nodeClass == asAST_NODE_EXPR ? TEXT("E")
								: TEXT("T"))
							: TEXT("root"),
						IncomingEdge != nullptr ? IncomingEdge->source.value : 0,
						IncomingEdge != nullptr
							? static_cast<uint32>(IncomingEdge->role) : 0,
						IncomingEdge != nullptr ? IncomingEdge->index : 0);
				}
				const asCDecl* const Target =
					InContext.GetDecl(Expr->resolvedDecl);
				if (Target != nullptr && Target->name.Equals("ReadStored"))
				{
					NativeReadCall = Expr;
				}
				return true;
			}

			bool VisitEdge(
				const asCASTContext&,
				const asSASTTraversalEdge&,
				asUINT) override
			{
				return true;
			}

			bool LeaveNode(
				const asCASTContext&,
				const asSASTNodeRef&,
				const asSASTTraversalEdge*,
				asUINT) override
			{
				return true;
			}

			asASTDeclId GetterDecl;
			asUINT GetterCallCount = 0;
			const asCExpr* GetterCall = nullptr;
			const asCExpr* NativeReadCall = nullptr;
			FString GetterTrace;
		};

		FReachableAccessorCallVisitor CallVisitor(GetterDecl->id);
		asSASTTraversalResult TraversalResult;
		ASSERT_THAT(AreEqual(
			static_cast<int>(asAST_TRAVERSAL_OK),
			asCASTTraverse(
				*Context,
				asSASTNodeRef::FromDecl(EntryDecl->id),
				CallVisitor,
				asSASTTraversalOptions(),
				TraversalResult),
			TEXT("the single-evaluation assertion must inspect the entry's reachable structural graph")));
		ASSERT_THAT(IsTrue(
			CallVisitor.GetterCallCount == 1
				&& CallVisitor.GetterCall != nullptr
				&& CallVisitor.NativeReadCall != nullptr
				&& CallVisitor.NativeReadCall->receiver
					== CallVisitor.GetterCall->id,
			*FString::Printf(
				TEXT("one authored property read must make one GetInner call reachable from the entry and own it directly through ReadStored; reachableGetterCalls=%u trace=%s. AST:\n%s"),
				CallVisitor.GetterCallCount,
				*CallVisitor.GetterTrace,
				*Dump)));

		const asCExpr* CopyPlan = GetterDecl->inits.GetLength() == 1
			? Context->GetExpr(GetterDecl->inits[0]) : nullptr;
		while (CopyPlan != nullptr
			&& (CopyPlan->kind == asAST_EXPR_CLEANUP
				|| CopyPlan->kind == asAST_EXPR_MATERIALIZE_TEMPORARY)
			&& CopyPlan->children.GetLength() == 1)
		{
			CopyPlan = Context->GetExpr(CopyPlan->children[0]);
		}
		const asCDecl* const CopyCtor = CopyPlan != nullptr
			? Context->GetDecl(CopyPlan->resolvedDecl) : nullptr;
		const asCDecl* const NativeValueOwner = CopyCtor != nullptr
			? Context->GetDecl(CopyCtor->parent) : nullptr;
		const asCExpr* const CopySource = CopyPlan != nullptr
			&& CopyPlan->children.GetLength() == 1
			? Context->GetExpr(CopyPlan->children[0]) : nullptr;
		const asCDecl* CopyParam = nullptr;
		if (CopyCtor != nullptr)
		{
			for (asUINT ChildIndex = 0;
				ChildIndex < CopyCtor->children.GetLength(); ++ChildIndex)
			{
				const asCDecl* const Child =
					Context->GetDecl(CopyCtor->children[ChildIndex]);
				if (Child != nullptr && Child->kind == asAST_DECL_PARAM)
				{
					CopyParam = Child;
					break;
				}
			}
		}
		ASSERT_THAT(IsTrue(
			GetterDecl->inits.GetLength() == 1
				&& CopyPlan != nullptr
				&& CopyPlan->kind == asAST_EXPR_CONSTRUCT
				&& CopyPlan->type == InnerField->type
				&& CopySource != nullptr
				&& CopySource->kind == asAST_EXPR_DECL_REF
				&& CopySource->resolvedDecl == InnerField->id
				&& CopyCtor != nullptr
				&& CopyCtor->kind == asAST_DECL_CONSTRUCTOR
				&& NativeValueOwner != nullptr
				&& NativeValueOwner->name.Equals("FProdNativeLifecycleMember")
				&& CopyParam != nullptr
				&& CopyParam->type.type == InnerField->type.type
				&& CopyParam->type.IsConst()
				&& CopyParam->type.IsReference(),
			*FString::Printf(
				TEXT("sealed non-POD getter must carry one exact field-to-copy-constructor plan. AST:\n%s"),
				*Dump)));
		if (CopyCtor == nullptr)
		{
			return;
		}

		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Result,
			*FString::Printf(
				TEXT("prepared non-POD generated getter must publish: result=%d line=%d detail=%s"),
				Result,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (Result != asAST_VERIFY_OK)
		{
			return;
		}

		asITypeInfo* const OwnerType =
			Module->GetTypeInfoByName("FPreparedNativeGetterOwner");
		asIScriptFunction* const RuntimeGetter = OwnerType != nullptr
			? OwnerType->GetMethodByName("GetInner") : nullptr;
		ASSERT_THAT(IsNotNull(RuntimeGetter));
		const asCScriptFunction* const InternalGetter =
			static_cast<const asCScriptFunction*>(RuntimeGetter);
		ASSERT_THAT(IsTrue(
			InternalGetter != nullptr && InternalGetter->DoesReturnOnStack(),
			TEXT("the Runtime getter ABI must use its caller-owned hidden return object")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(RuntimeGetter, NativeCopyCtorId),
			TEXT("generated getter must call the exact sealed native copy constructor")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int RunPreparedNativeNonPodGeneratedGetter()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("generated non-POD getter must execute through its detached return object; got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberConstructed,
			TEXT("the backing field must be default-constructed once")));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberCopyConstructed,
			TEXT("the getter result must be copy-constructed exactly once")));
		ASSERT_THAT(AreEqual(41, GProdNativeLifecycleMemberCopiedValue,
			TEXT("the copy constructor must receive the live backing-field payload")));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberRead,
			TEXT("ReadStored must observe the detached getter result once")));
		ASSERT_THAT(AreEqual(2, GProdNativeLifecycleMemberDestructed,
			TEXT("the getter result and backing field must each be destroyed once")));
	}

	TEST_METHOD(PreparedNativeNonPodGeneratedSetterTransfersOwnedParameterAndInvokesExactAssignment)
	{
		using namespace AngelscriptBuilderTestSupport;

		GProdNativeLifecycleMemberConstructed = 0;
		GProdNativeLifecycleMemberDestructed = 0;
		GProdNativeLifecycleMemberCopyConstructed = 0;
		GProdNativeLifecycleMemberCopiedValue = 0;
		GProdNativeLifecycleMemberAssigned = 0;
		GProdNativeLifecycleMemberAssignedValue = 0;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdNativeLifecycleMember",
				sizeof(FProdNativeLifecycleMember),
				asOBJ_VALUE | asOBJ_APP_CLASS_CD
					| asOBJ_APP_CLASS_ALLINTS) >= 0,
			TEXT("the generated setter gate requires a 32-byte non-POD native VALUE type")));
		const int NativeCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberConstruct),
			asCALL_GENERIC);
		const int NativeCopyCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_CONSTRUCT,
			"void f(const FProdNativeLifecycleMember&in Other)",
			asFUNCTION(ProdNativeLifecycleMemberCopyConstruct),
			asCALL_GENERIC);
		const int NativeDtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_DESTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberDestruct),
			asCALL_GENERIC);
		const int NativeAssignId = ScriptEngine->RegisterObjectMethod(
			"FProdNativeLifecycleMember",
			"FProdNativeLifecycleMember& opAssign(const FProdNativeLifecycleMember&in Other)",
			asFUNCTION(ProdNativeLifecycleMemberAssign),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(
			NativeCtorId > 0 && NativeCopyCtorId > 0
				&& NativeDtorId > 0 && NativeAssignId > 0,
			TEXT("the generated setter gate requires exact construct/copy/destroy/assign declarations")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalNativeNonPodGeneratedSetter");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalNativeNonPodGeneratedSetter.as",
			R"AS(
				struct FPreparedNativeSetterOwner
				{
					FProdNativeLifecycleMember Inner;

					// Suppress only the generated getter. The property write below
					// must still dispatch through the generated by-value setter.
					void GetInner() {}
				}

				int RunPreparedNativeNonPodGeneratedSetter()
				{
					FPreparedNativeSetterOwner Owner;
					FProdNativeLifecycleMember Value;
					Owner.Inner = Value;
					return 42;
				}
			)AS")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("the staged Builder must prepare the generated setter and its real caller")));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* OwnerClass = nullptr;
		const asCDecl* InnerField = nullptr;
		const asCDecl* SetterDecl = nullptr;
		const asCDecl* SetterParam = nullptr;
		const asCDecl* EntryDecl = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_CLASS
				&& Candidate->name.Equals("FPreparedNativeSetterOwner"))
			{
				OwnerClass = Candidate;
			}
			else if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_FUNCTION
				&& Candidate->name.Equals(
					"RunPreparedNativeNonPodGeneratedSetter"))
			{
				EntryDecl = Candidate;
			}
		}
		if (OwnerClass != nullptr)
		{
			for (asUINT ChildIndex = 0;
				ChildIndex < OwnerClass->children.GetLength(); ++ChildIndex)
			{
				const asCDecl* const Child =
					Context->GetDecl(OwnerClass->children[ChildIndex]);
				if (Child != nullptr && Child->kind == asAST_DECL_VAR
					&& Child->name.Equals("Inner"))
				{
					InnerField = Child;
				}
				else if (Child != nullptr
					&& Child->kind == asAST_DECL_METHOD
					&& Child->name.Equals("SetInner")
					&& Child->accessorKind == asAST_ACCESSOR_SET)
				{
					SetterDecl = Child;
				}
			}
		}
		if (SetterDecl != nullptr)
		{
			for (asUINT ChildIndex = 0;
				ChildIndex < SetterDecl->children.GetLength(); ++ChildIndex)
			{
				const asCDecl* const Child =
					Context->GetDecl(SetterDecl->children[ChildIndex]);
				if (Child != nullptr && Child->kind == asAST_DECL_PARAM)
				{
					SetterParam = Child;
					break;
				}
			}
		}

		const asCExpr* const AssignPlan = SetterDecl != nullptr
			&& SetterDecl->inits.GetLength() == 1
			? Context->GetExpr(SetterDecl->inits[0]) : nullptr;
		const asCExpr* const AssignLhs = AssignPlan != nullptr
			&& AssignPlan->children.GetLength() == 2
			? Context->GetExpr(AssignPlan->children[0]) : nullptr;
		const asCExpr* const AssignRhs = AssignPlan != nullptr
			&& AssignPlan->children.GetLength() == 2
			? Context->GetExpr(AssignPlan->children[1]) : nullptr;
		const asCDecl* const AssignOperator = AssignPlan != nullptr
			? Context->GetDecl(AssignPlan->resolvedDecl) : nullptr;
		const asCDecl* SourceValueDecl = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_VAR
				&& Candidate->parent == (EntryDecl != nullptr
					? EntryDecl->id : asASTDeclId())
				&& Candidate->name.Equals("Value"))
			{
				SourceValueDecl = Candidate;
				break;
			}
		}
		const asCExpr* SetterCall = nullptr;
		for (asUINT ExprIndex = 1;
			ExprIndex <= Context->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Candidate =
				Context->GetExpr(asASTExprId(ExprIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_EXPR_CALL
				&& SetterDecl != nullptr
				&& Candidate->resolvedDecl == SetterDecl->id)
			{
				SetterCall = Candidate;
				break;
			}
		}
		const asCExpr* CopyArgument = SetterCall != nullptr
			&& SetterCall->children.GetLength() == 2
			? Context->GetExpr(SetterCall->children[0]) : nullptr;
		while (CopyArgument != nullptr
			&& (CopyArgument->kind == asAST_EXPR_CLEANUP
				|| CopyArgument->kind == asAST_EXPR_MATERIALIZE_TEMPORARY)
			&& CopyArgument->children.GetLength() == 1)
		{
			CopyArgument = Context->GetExpr(CopyArgument->children[0]);
		}
		const asCDecl* const CopyConstructor = CopyArgument != nullptr
			? Context->GetDecl(CopyArgument->resolvedDecl) : nullptr;
		const asCExpr* const CopySource = CopyArgument != nullptr
			&& CopyArgument->children.GetLength() == 1
			? Context->GetExpr(CopyArgument->children[0]) : nullptr;

		asCString RawDump;
		asCASTDump(*Context, RawDump);
		const FString Dump =
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(RawDump);
		ASSERT_THAT(IsTrue(
			InnerField != nullptr && SetterDecl != nullptr
				&& SetterParam != nullptr && EntryDecl != nullptr
				&& SetterDecl->accessorField == InnerField->id
				&& SetterParam->type == InnerField->type,
			*FString::Printf(
				TEXT("sealed setter must name its exact non-POD field, by-value parameter and caller. AST:\n%s"),
				*Dump)));
		ASSERT_THAT(IsTrue(
			AssignPlan != nullptr && AssignPlan->kind == asAST_EXPR_ASSIGN
				&& AssignLhs != nullptr
				&& AssignLhs->kind == asAST_EXPR_DECL_REF
				&& AssignLhs->resolvedDecl == InnerField->id
				&& AssignRhs != nullptr
				&& AssignRhs->kind == asAST_EXPR_DECL_REF
				&& AssignRhs->resolvedDecl == SetterParam->id
				&& AssignOperator != nullptr
				&& AssignOperator->name.Equals("opAssign"),
			*FString::Printf(
				TEXT("sealed setter must carry its exact field/parameter/opAssign plan. AST:\n%s"),
				*Dump)));
		ASSERT_THAT(IsTrue(SetterCall != nullptr,
			*FString::Printf(
				TEXT("the authored property write must resolve to the generated setter. AST:\n%s"),
				*Dump)));
		ASSERT_THAT(IsTrue(
			SourceValueDecl != nullptr
				&& SetterCall != nullptr
				&& SetterCall->receiver.IsValid()
				&& SetterCall->children.GetLength() == 2
				&& SetterCall->children[1] == SetterCall->receiver
				&& CopyArgument != nullptr
				&& CopyArgument->kind == asAST_EXPR_CONSTRUCT
				&& CopyArgument->type == SetterParam->type
				&& CopyConstructor != nullptr
				&& CopyConstructor->kind == asAST_DECL_CONSTRUCTOR
				&& CopySource != nullptr
				&& CopySource->kind == asAST_EXPR_DECL_REF
				&& CopySource->resolvedDecl == SourceValueDecl->id,
			*FString::Printf(
				TEXT("the by-value non-POD call edge must seal one exact copy-construction ownership-transfer plan. AST:\n%s"),
				*Dump)));

		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Result,
			*FString::Printf(
				TEXT("prepared non-POD generated setter must publish: result=%d line=%d detail=%s"),
				Result,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (Result != asAST_VERIFY_OK)
		{
			return;
		}

		asITypeInfo* const OwnerType =
			Module->GetTypeInfoByName("FPreparedNativeSetterOwner");
		asIScriptFunction* const RuntimeSetter = OwnerType != nullptr
			? OwnerType->GetMethodByName("SetInner") : nullptr;
		asIScriptFunction* const RuntimeEntry = Module->GetFunctionByDecl(
			"int RunPreparedNativeNonPodGeneratedSetter()");
		ASSERT_THAT(IsNotNull(RuntimeSetter));
		ASSERT_THAT(IsNotNull(RuntimeEntry));
		if (RuntimeSetter == nullptr || RuntimeEntry == nullptr)
		{
			return;
		}
		asCRuntimeTypeBridge RuntimeBridge(ScriptEngine);
		asASTDeclId BoundRuntimeSetter;
		asCString RuntimeSetterBindDetail;
		const int RuntimeSetterBindResult = RuntimeBridge.BindFunctionDeclaration(
			*Context,
			*static_cast<asCScriptFunction*>(RuntimeSetter),
			BoundRuntimeSetter,
			&RuntimeSetterBindDetail);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_DECL_BIND_OK),
			static_cast<int32>(RuntimeSetterBindResult),
			*FString::Printf(
				TEXT("a generated accessor Runtime ABI must bind its exact generated declaration without applying authored-Builder parameter normalization; detail=%s"),
				UTF8_TO_TCHAR(RuntimeSetterBindDetail.AddressOf()))));
		ASSERT_THAT(IsTrue(BoundRuntimeSetter == SetterDecl->id,
			TEXT("the generated setter must publish its exact generated DeclId")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(RuntimeSetter, NativeAssignId),
			TEXT("generated setter must call the exact sealed native opAssign")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(
				RuntimeSetter, asBC_FREE),
			TEXT("generated setter must release its callee-owned by-value object parameter")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeAllocatesWithConstructor(
				RuntimeEntry,
				ScriptEngine->GetTypeInfoByName("FProdNativeLifecycleMember"),
				NativeCopyCtorId),
			TEXT("the caller must ALLOC a unique heap copy with the exact sealed copy constructor before transfer")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(RuntimeEntry, RuntimeSetter->GetId()),
			TEXT("the authored property write must call the generated setter body")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int RunPreparedNativeNonPodGeneratedSetter()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("generated non-POD setter must execute through owned parameter transfer; got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(2, GProdNativeLifecycleMemberConstructed,
			TEXT("the backing field and source local must each default-construct once")));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberCopyConstructed,
			TEXT("the caller must copy-construct exactly one transferred argument")));
		ASSERT_THAT(AreEqual(0, GProdNativeLifecycleMemberCopiedValue,
			TEXT("the transferred copy must preserve the source local payload")));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberAssigned,
			TEXT("the generated setter must invoke opAssign exactly once")));
		ASSERT_THAT(AreEqual(0, GProdNativeLifecycleMemberAssignedValue,
			TEXT("opAssign must receive the transferred source payload")));
		ASSERT_THAT(AreEqual(3, GProdNativeLifecycleMemberDestructed,
			TEXT("callee-owned copy, source local and backing field must each be destroyed once")));
	}

	TEST_METHOD(PreparedNativeNonPodMemberAssignmentCallsExactSealedOperator)
	{
		using namespace AngelscriptBuilderTestSupport;

		GProdNativeLifecycleMemberConstructed = 0;
		GProdNativeLifecycleMemberDestructed = 0;
		GProdNativeLifecycleMemberAssigned = 0;
		GProdNativeLifecycleMemberAssignedValue = 0;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdNativeLifecycleMember",
				sizeof(FProdNativeLifecycleMember),
				asOBJ_VALUE | asOBJ_APP_CLASS_CD
					| asOBJ_APP_CLASS_ALLINTS) >= 0));
		const int NativeCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberConstruct),
			asCALL_GENERIC);
		const int NativeDtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_DESTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberDestruct),
			asCALL_GENERIC);
		const int NativeAssignId = ScriptEngine->RegisterObjectMethod(
			"FProdNativeLifecycleMember",
			"FProdNativeLifecycleMember& opAssign(const FProdNativeLifecycleMember&in Other)",
			asFUNCTION(ProdNativeLifecycleMemberAssign),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(
			NativeCtorId > 0 && NativeDtorId > 0 && NativeAssignId > 0));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FProdNativeLifecycleMember",
				"int Stored",
				asOFFSET(FProdNativeLifecycleMember, Stored)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalNativeLifecycleAssignment");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalNativeLifecycleAssignment.as",
			R"AS(
				struct FPreparedNativeLifecycleOwner
				{
					FProdNativeLifecycleMember Inner;

					// Class-body functions are attached before generated accessors are
					// synthesized, matching the preprocessor delegate-wrapper shape.
					void CopyFrom(const FPreparedNativeLifecycleOwner& Other)
					{
						Inner = Other.Inner;
					}
					// These later declarations only suppress unrelated generated by-value
					// accessors; CopyFrom already owns a direct field assignment AST.
					void GetInner() {}
					void SetInner() {}
				}

				int RunPreparedNativeLifecycleAssignment()
				{
					FPreparedNativeLifecycleOwner Left;
					FPreparedNativeLifecycleOwner Right;
					Right.CopyFrom(Left);
					return 42;
				}
			)AS")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("the staged Builder must prepare the native assignment fixture")));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCExpr* NativeAssign = nullptr;
		const asCDecl* InnerField = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_VAR
				&& Candidate->name.Equals("Inner"))
			{
				InnerField = Candidate;
				break;
			}
		}
		for (asUINT ExprIndex = 1;
			ExprIndex <= Context->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Candidate =
				Context->GetExpr(asASTExprId(ExprIndex));
			if (Candidate == nullptr
				|| Candidate->kind != asAST_EXPR_ASSIGN
				|| Candidate->children.GetLength() != 2)
			{
				continue;
			}
			const asCExpr* const Lhs =
				Context->GetExpr(Candidate->children[0]);
			const asCExpr* const Rhs =
				Context->GetExpr(Candidate->children[1]);
			const asCExpr* RhsTerminal = Rhs;
			while (RhsTerminal != nullptr
				&& RhsTerminal->kind == asAST_EXPR_SEQUENCE
				&& RhsTerminal->children.GetLength() > 0)
			{
				RhsTerminal = Context->GetExpr(
					RhsTerminal->children[
						RhsTerminal->children.GetLength() - 1]);
			}
			if (Lhs != nullptr && Rhs != nullptr
				&& (Lhs->kind == asAST_EXPR_DECL_REF
					|| Lhs->kind == asAST_EXPR_MEMBER_REF)
				&& RhsTerminal != nullptr
				&& RhsTerminal->kind == asAST_EXPR_MEMBER_REF
				&& InnerField != nullptr
				&& Lhs->resolvedDecl == InnerField->id
				&& RhsTerminal->resolvedDecl == InnerField->id)
			{
				NativeAssign = Candidate;
				break;
			}
		}
		asCString AssignmentAstDump;
		asCASTDump(*Context, AssignmentAstDump);
		ASSERT_THAT(IsNotNull(InnerField));
		ASSERT_THAT(IsNotNull(NativeAssign,
			*FString::Printf(
				TEXT("the source fixture must expose direct native member assignment; AST:\n%s"),
				UTF8_TO_TCHAR(AssignmentAstDump.AddressOf()
					? AssignmentAstDump.AddressOf() : ""))));
		if (InnerField == nullptr || NativeAssign == nullptr)
		{
			return;
		}

		const asCExpr* const AssignLhs =
			Context->GetExpr(NativeAssign->children[0]);
		const asCExpr* const AssignRhs =
			Context->GetExpr(NativeAssign->children[1]);
		const asCDecl* const AssignOperator =
			Context->GetDecl(NativeAssign->resolvedDecl);
		const asCDecl* const NativeOwner = AssignOperator != nullptr
			? Context->GetDecl(AssignOperator->parent) : nullptr;
		const asCDecl* AssignParam = nullptr;
		if (AssignOperator != nullptr)
		{
			for (asUINT ChildIndex = 0;
				ChildIndex < AssignOperator->children.GetLength(); ++ChildIndex)
			{
				const asCDecl* const Child = Context->GetDecl(
					AssignOperator->children[ChildIndex]);
				if (Child != nullptr && Child->kind == asAST_DECL_PARAM)
				{
					AssignParam = Child;
					break;
				}
			}
		}
		ASSERT_THAT(IsTrue(
			AssignLhs != nullptr
				&& AssignRhs != nullptr
				&& AssignLhs->type == InnerField->type
				&& AssignRhs->type == InnerField->type
				&& AssignOperator != nullptr
				&& AssignOperator->kind == asAST_DECL_METHOD
				&& AssignOperator->name.Equals("opAssign")
				&& NativeOwner != nullptr
				&& NativeOwner->kind == asAST_DECL_CLASS
				&& NativeOwner->name.Equals("FProdNativeLifecycleMember")
				&& AssignParam != nullptr
				&& AssignParam->type.type == InnerField->type.type
				&& (AssignParam->type.quals & asAST_QUAL_REFERENCE) != 0
				&& (AssignParam->type.quals & asAST_QUAL_IN) != 0,
			TEXT("sealed Sema must bind native non-POD assignment to the exact opAssign method and reference parameter")));

		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Result,
			*FString::Printf(
				TEXT("prepared native opAssign CodeGen must succeed: result=%d line=%d detail=%s"),
				Result,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (Result != asAST_VERIFY_OK)
		{
			return;
		}

		asIScriptFunction* const Entry = Module->GetFunctionByDecl(
			"int RunPreparedNativeLifecycleAssignment()");
		asITypeInfo* const OwnerType = Module->GetTypeInfoByName(
			"FPreparedNativeLifecycleOwner");
		asIScriptFunction* const CopyFrom = OwnerType != nullptr
			? OwnerType->GetMethodByName("CopyFrom") : nullptr;
		ASSERT_THAT(IsNotNull(Entry));
		ASSERT_THAT(IsNotNull(CopyFrom));
		if (Entry == nullptr || CopyFrom == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(CopyFrom, NativeAssignId),
			TEXT("the assignment expression must CALLSYS the exact sealed native opAssign")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Entry, CopyFrom->GetId()),
			TEXT("the entry must call the script method that owns the sealed native assignment")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int RunPreparedNativeLifecycleAssignment()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("native member assignment must copy the observed payload; got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(2, GProdNativeLifecycleMemberConstructed));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberAssigned));
		ASSERT_THAT(AreEqual(41, GProdNativeLifecycleMemberAssignedValue,
			TEXT("opAssign must receive Left's first-instance payload, not Right's zero payload")));
		ASSERT_THAT(AreEqual(2, GProdNativeLifecycleMemberDestructed));
	}

	TEST_METHOD(PreparedNativeNonPodMemberMethodCallUsesExactSealedReceiver)
	{
		using namespace AngelscriptBuilderTestSupport;

		GProdNativeLifecycleMemberConstructed = 0;
		GProdNativeLifecycleMemberDestructed = 0;
		GProdNativeLifecycleMemberRead = 0;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdNativeLifecycleMember",
				sizeof(FProdNativeLifecycleMember),
				asOBJ_VALUE | asOBJ_APP_CLASS_CD
					| asOBJ_APP_CLASS_ALLINTS) >= 0));
		const int NativeCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberConstruct),
			asCALL_GENERIC);
		const int NativeDtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_DESTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberDestruct),
			asCALL_GENERIC);
		const int NativeReadId = ScriptEngine->RegisterObjectMethod(
			"FProdNativeLifecycleMember",
			"int ReadStored() const",
			asFUNCTION(ProdNativeLifecycleMemberReadStored),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(
			NativeCtorId > 0 && NativeDtorId > 0 && NativeReadId > 0));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FProdNativeLifecycleMember",
				"int Stored",
				asOFFSET(FProdNativeLifecycleMember, Stored)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalNativeLifecycleReceiver");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalNativeLifecycleReceiver.as",
			R"AS(
				struct FPreparedNativeLifecycleOwner
				{
					FProdNativeLifecycleMember Inner;

					int ReadInner()
					{
						return Inner.ReadStored();
					}
					// Suppress unrelated generated by-value accessor bodies.
					void GetInner() {}
					void SetInner() {}
				}

				int RunPreparedNativeLifecycleReceiver()
				{
					FPreparedNativeLifecycleOwner Owner;
					return Owner.ReadInner();
				}
			)AS")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("the staged Builder must prepare the native receiver fixture")));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* InnerField = nullptr;
		const asCExpr* NativeCall = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_VAR
				&& Candidate->name.Equals("Inner"))
			{
				InnerField = Candidate;
				break;
			}
		}
		for (asUINT ExprIndex = 1;
			ExprIndex <= Context->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Candidate =
				Context->GetExpr(asASTExprId(ExprIndex));
			const asCDecl* const Target = Candidate != nullptr
				? Context->GetDecl(Candidate->resolvedDecl) : nullptr;
			if (Candidate != nullptr
				&& Candidate->kind == asAST_EXPR_CALL
				&& Target != nullptr
				&& Target->kind == asAST_DECL_METHOD
				&& Target->name.Equals("ReadStored"))
			{
				NativeCall = Candidate;
				break;
			}
		}
		const asCExpr* Receiver = NativeCall != nullptr
			? Context->GetExpr(NativeCall->receiver) : nullptr;
		while (Receiver != nullptr
			&& Receiver->kind == asAST_EXPR_SEQUENCE
			&& Receiver->children.GetLength() > 0)
		{
			Receiver = Context->GetExpr(
				Receiver->children[Receiver->children.GetLength() - 1]);
		}
		const asCDecl* const NativeMethod = NativeCall != nullptr
			? Context->GetDecl(NativeCall->resolvedDecl) : nullptr;
		const asCDecl* const NativeOwner = NativeMethod != nullptr
			? Context->GetDecl(NativeMethod->parent) : nullptr;
		bool NativeMethodHasParameter = false;
		if (NativeMethod != nullptr)
		{
			for (asUINT ChildIndex = 0;
				ChildIndex < NativeMethod->children.GetLength(); ++ChildIndex)
			{
				const asCDecl* const Child = Context->GetDecl(
					NativeMethod->children[ChildIndex]);
				if (Child != nullptr && Child->kind == asAST_DECL_PARAM)
				{
					NativeMethodHasParameter = true;
					break;
				}
			}
		}
		asCString ReceiverAstDump;
		asCASTDump(*Context, ReceiverAstDump);
		ASSERT_THAT(IsTrue(
			InnerField != nullptr
				&& NativeCall != nullptr
				&& NativeMethod != nullptr
				&& NativeMethod->kind == asAST_DECL_METHOD
				&& NativeMethod->name.Equals("ReadStored")
				&& NativeOwner != nullptr
				&& NativeOwner->kind == asAST_DECL_CLASS
				&& NativeOwner->name.Equals("FProdNativeLifecycleMember")
				&& !NativeMethodHasParameter
				// Canonical call storage keeps the effective receiver both in the
				// dedicated receiver field and as the one implicit reverse-formal
				// child.  It is not an authored or Runtime method parameter.
				&& NativeCall->children.GetLength() == 1
				&& NativeCall->children[0] == NativeCall->receiver
				&& Receiver != nullptr
				&& (Receiver->kind == asAST_EXPR_DECL_REF
					|| Receiver->kind == asAST_EXPR_MEMBER_REF)
				&& Receiver->resolvedDecl == InnerField->id
				&& Receiver->type == InnerField->type,
			*FString::Printf(
				TEXT("sealed Sema must bind the exact native method and Inner value receiver; AST:\n%s"),
				UTF8_TO_TCHAR(ReceiverAstDump.AddressOf()
					? ReceiverAstDump.AddressOf() : ""))));

		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Result,
			*FString::Printf(
				TEXT("prepared native value receiver CodeGen must succeed: result=%d line=%d detail=%s"),
				Result,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (Result != asAST_VERIFY_OK)
		{
			return;
		}

		asITypeInfo* const OwnerType = Module->GetTypeInfoByName(
			"FPreparedNativeLifecycleOwner");
		asIScriptFunction* const ReadInner = OwnerType != nullptr
			? OwnerType->GetMethodByName("ReadInner") : nullptr;
		ASSERT_THAT(IsNotNull(ReadInner));
		if (ReadInner == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(ReadInner, NativeReadId),
			TEXT("the script method must CALLSYS the exact sealed native method")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int RunPreparedNativeLifecycleReceiver()", Value)
				&& Value == 41,
			*FString::Printf(
				TEXT("the native method must observe the live field payload; got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberConstructed));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberRead));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberDestructed));
	}

	TEST_METHOD(PreparedGeneratedNativeNonPodMemberCallDoesNotSequenceReceiverTwice)
	{
		using namespace AngelscriptBuilderTestSupport;

		GProdNativeLifecycleMemberConstructed = 0;
		GProdNativeLifecycleMemberDestructed = 0;
		GProdNativeLifecycleMemberRead = 0;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdNativeLifecycleMember",
				sizeof(FProdNativeLifecycleMember),
				asOBJ_VALUE | asOBJ_APP_CLASS_CD
					| asOBJ_APP_CLASS_ALLINTS) >= 0));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"CProdReceiverObject", 0,
				asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0
				&& ScriptEngine->RegisterObjectBehaviour(
					"CProdReceiverObject", asBEHAVE_ADDREF, "void f()",
					asFUNCTION(ProdCanonicalHandleAddRef), asCALL_GENERIC) >= 0
				&& ScriptEngine->RegisterObjectBehaviour(
					"CProdReceiverObject", asBEHAVE_RELEASE, "void f()",
					asFUNCTION(ProdCanonicalHandleRelease), asCALL_GENERIC) >= 0));
		const int NativeCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberConstruct),
			asCALL_GENERIC);
		const int NativeDtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_DESTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberDestruct),
			asCALL_GENERIC);
		const int NativeReadId = ScriptEngine->RegisterObjectMethod(
			"FProdNativeLifecycleMember",
			"CProdReceiverObject GetObject() const",
			asFUNCTION(ProdNativeLifecycleMemberGetObject),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(
			NativeCtorId > 0 && NativeDtorId > 0 && NativeReadId > 0,
			*FString::Printf(
				TEXT("native generated receiver fixture registration failed: ctor=%d dtor=%d method=%d"),
				NativeCtorId, NativeDtorId, NativeReadId)));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FProdNativeLifecycleMember",
				"int Stored",
				asOFFSET(FProdNativeLifecycleMember, Stored)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalGeneratedNativeReceiver");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalGeneratedNativeReceiver.as",
			R"AS(
				struct FPreparedGeneratedNativeReceiverOwner
				{
					FProdNativeLifecycleMember Inner;

					CProdReceiverObject ReadInner() const __generated
					{
						return Inner.GetObject();
					}
					// Suppress unrelated generated by-value accessor bodies.
					void GetInner() {}
					void SetInner() {}
				}

				int RunPreparedGeneratedNativeReceiver()
				{
					FPreparedGeneratedNativeReceiverOwner Owner;
					Owner.ReadInner();
					return 41;
				}
			)AS")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		const bool Prepared = RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module);
		if (!Prepared)
		{
			ReportBuilderFailureDiagnostics(*TestRunner, Engine);
		}
		ASSERT_THAT(IsTrue(
			Prepared,
			TEXT("the staged Builder must prepare the generated receiver fixture")));
		if (!Prepared)
		{
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* InnerField = nullptr;
		const asCExpr* NativeCall = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_VAR
				&& Candidate->name.Equals("Inner"))
			{
				InnerField = Candidate;
				break;
			}
		}
		for (asUINT ExprIndex = 1;
			ExprIndex <= Context->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Candidate =
				Context->GetExpr(asASTExprId(ExprIndex));
			const asCDecl* const Target = Candidate != nullptr
				? Context->GetDecl(Candidate->resolvedDecl) : nullptr;
			if (Candidate != nullptr
				&& Candidate->kind == asAST_EXPR_CALL
				&& Target != nullptr
				&& Target->kind == asAST_DECL_METHOD
				&& Target->name.Equals("GetObject"))
			{
				NativeCall = Candidate;
				break;
			}
		}
		const asCExpr* const Receiver = NativeCall != nullptr
			? Context->GetExpr(NativeCall->receiver) : nullptr;
		const asCDecl* const NativeMethod = NativeCall != nullptr
			? Context->GetDecl(NativeCall->resolvedDecl) : nullptr;
		const asCDecl* const NativeOwner = NativeMethod != nullptr
			? Context->GetDecl(NativeMethod->parent) : nullptr;
		asUINT DirectChildParentCount = 0;
		const asCExpr* UnexpectedParent = nullptr;
		if (Receiver != nullptr)
		{
			for (asUINT ExprIndex = 1;
				ExprIndex <= Context->GetExprCount(); ++ExprIndex)
			{
				const asCExpr* const Candidate =
					Context->GetExpr(asASTExprId(ExprIndex));
				if (Candidate == nullptr)
				{
					continue;
				}
				for (asUINT ChildIndex = 0;
					ChildIndex < Candidate->children.GetLength(); ++ChildIndex)
				{
					if (Candidate->children[ChildIndex] != Receiver->id)
					{
						continue;
					}
					++DirectChildParentCount;
					if (Candidate != NativeCall)
					{
						UnexpectedParent = Candidate;
					}
				}
			}
		}
		asCString ReceiverAstDump;
		asCASTDump(*Context, ReceiverAstDump);
		const bool HasExactReceiverShape =
			InnerField != nullptr
			&& NativeCall != nullptr
			&& NativeMethod != nullptr
			&& NativeMethod->kind == asAST_DECL_METHOD
			&& NativeMethod->name.Equals("GetObject")
			&& NativeOwner != nullptr
			&& NativeOwner->kind == asAST_DECL_CLASS
			&& NativeOwner->name.Equals("FProdNativeLifecycleMember")
			&& NativeCall->children.GetLength() == 1
			&& NativeCall->children[0] == NativeCall->receiver
			&& Receiver != nullptr
			&& Receiver->kind == asAST_EXPR_DECL_REF
			&& Receiver->resolvedDecl == InnerField->id
			&& Receiver->type == InnerField->type
			&& DirectChildParentCount == 1
			&& UnexpectedParent == nullptr;
		ASSERT_THAT(IsTrue(
			HasExactReceiverShape,
			*FString::Printf(
				TEXT("generated member call must not keep its receiver as a second Sequence value; parents=%u unexpected=%u kind=%d AST:\n%s"),
				DirectChildParentCount,
				UnexpectedParent != nullptr ? UnexpectedParent->id.value : 0,
				UnexpectedParent != nullptr
					? static_cast<int32>(UnexpectedParent->kind) : -1,
				UTF8_TO_TCHAR(ReceiverAstDump.AddressOf()
					? ReceiverAstDump.AddressOf() : ""))));
		if (!HasExactReceiverShape)
		{
			return;
		}

		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Result,
			*FString::Printf(
				TEXT("generated native receiver CodeGen must succeed: result=%d line=%d detail=%s"),
				Result,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (Result != asAST_VERIFY_OK)
		{
			return;
		}

		asITypeInfo* const OwnerType = Module->GetTypeInfoByName(
			"FPreparedGeneratedNativeReceiverOwner");
		asIScriptFunction* const ReadInner = OwnerType != nullptr
			? OwnerType->GetMethodByName("ReadInner") : nullptr;
		ASSERT_THAT(IsNotNull(ReadInner));
		if (ReadInner == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(ReadInner, NativeReadId),
			TEXT("the generated script method must CALLSYS the exact sealed native method")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int RunPreparedGeneratedNativeReceiver()", Value)
				&& Value == 41,
			*FString::Printf(
				TEXT("the generated receiver must be evaluated once by address; got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberConstructed));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberRead));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberDestructed));
	}

	TEST_METHOD(PreparedNativeNonPodMemberReferenceArgumentUsesExactSealedAddress)
	{
		using namespace AngelscriptBuilderTestSupport;

		GProdNativeLifecycleMemberConstructed = 0;
		GProdNativeLifecycleMemberDestructed = 0;
		GProdNativeLifecycleMemberReferenceObserved = 0;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdNativeLifecycleMember",
				sizeof(FProdNativeLifecycleMember),
				asOBJ_VALUE | asOBJ_APP_CLASS_CD
					| asOBJ_APP_CLASS_ALLINTS) >= 0));
		const int NativeCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberConstruct),
			asCALL_GENERIC);
		const int NativeDtorId = ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_DESTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberDestruct),
			asCALL_GENERIC);
		const int NativeObserveId = ScriptEngine->RegisterGlobalFunction(
			"int ObserveNativeMember(const FProdNativeLifecycleMember&in Value)",
			asFUNCTION(ProdNativeLifecycleMemberObserveReference),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(
			NativeCtorId > 0 && NativeDtorId > 0 && NativeObserveId > 0));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FProdNativeLifecycleMember",
				"int Stored",
				asOFFSET(FProdNativeLifecycleMember, Stored)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalNativeLifecycleReferenceArgument");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalNativeLifecycleReferenceArgument.as",
			R"AS(
				struct FPreparedNativeLifecycleOwner
				{
					FProdNativeLifecycleMember Inner;

					int Execute() const
					{
						return ObserveNativeMember(Inner);
					}
					// Suppress unrelated generated by-value accessor bodies.
					void GetInner() {}
					void SetInner() {}
				}

				int RunPreparedNativeLifecycleReferenceArgument()
				{
					FPreparedNativeLifecycleOwner Owner;
					return Owner.Execute();
				}
			)AS")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("the staged Builder must prepare the reference-argument fixture")));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* InnerField = nullptr;
		const asCDecl* NativeFunction = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Decl =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Decl != nullptr && Decl->kind == asAST_DECL_VAR
				&& Decl->name.Equals("Inner"))
			{
				InnerField = Decl;
			}
			else if (Decl != nullptr && Decl->kind == asAST_DECL_FUNCTION
				&& Decl->name.Equals("ObserveNativeMember"))
			{
				NativeFunction = Decl;
			}
		}

		const asCDecl* FormalParameter = nullptr;
		if (NativeFunction != nullptr)
		{
			for (asUINT ChildIndex = 0;
				ChildIndex < NativeFunction->children.GetLength(); ++ChildIndex)
			{
				const asCDecl* const Child = Context->GetDecl(
					NativeFunction->children[ChildIndex]);
				if (Child != nullptr && Child->kind == asAST_DECL_PARAM)
				{
					FormalParameter = Child;
					break;
				}
			}
		}

		const asCExpr* NativeCall = nullptr;
		for (asUINT ExprIndex = 1;
			ExprIndex <= Context->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Candidate =
				Context->GetExpr(asASTExprId(ExprIndex));
			if (Candidate != nullptr && NativeFunction != nullptr
				&& Candidate->kind == asAST_EXPR_CALL
				&& Candidate->resolvedDecl == NativeFunction->id)
			{
				NativeCall = Candidate;
				break;
			}
		}
		const asCExpr* const Argument = NativeCall != nullptr
			&& NativeCall->children.GetLength() == 1
			? Context->GetExpr(NativeCall->children[0]) : nullptr;

		asCString RawDump;
		asCASTDump(*Context, RawDump);
		ASSERT_THAT(IsTrue(
			InnerField != nullptr
				&& NativeFunction != nullptr
				&& FormalParameter != nullptr
				&& FormalParameter->type.type == InnerField->type.type
				&& (FormalParameter->type.quals & asAST_QUAL_CONST) != 0
				&& (FormalParameter->type.quals & asAST_QUAL_REFERENCE) != 0
				&& (FormalParameter->type.quals & asAST_QUAL_IN) != 0
				&& NativeCall != nullptr
				&& !NativeCall->receiver.IsValid()
				&& Argument != nullptr
				&& (Argument->kind == asAST_EXPR_DECL_REF
					|| Argument->kind == asAST_EXPR_MEMBER_REF)
				&& Argument->resolvedDecl == InnerField->id
				&& Argument->type == InnerField->type,
			*FString::Printf(
				TEXT("sealed Sema must bind the exact wide member argument and const reference formal before CodeGen; AST:\n%s"),
				UTF8_TO_TCHAR(RawDump.AddressOf()
					? RawDump.AddressOf() : ""))));

		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Result,
			*FString::Printf(
				TEXT("prepared native wide reference-argument CodeGen must succeed: result=%d line=%d detail=%s"),
				Result,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (Result != asAST_VERIFY_OK)
		{
			return;
		}

		asITypeInfo* const OwnerType = Module->GetTypeInfoByName(
			"FPreparedNativeLifecycleOwner");
		asIScriptFunction* const Execute = OwnerType != nullptr
			? OwnerType->GetMethodByName("Execute") : nullptr;
		ASSERT_THAT(IsNotNull(Execute));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Execute, NativeObserveId),
			TEXT("the script method must CALLSYS the exact sealed global native function")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int RunPreparedNativeLifecycleReferenceArgument()", Value)
				&& Value == 41,
			*FString::Printf(
				TEXT("the global native function must observe the live field by address; got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberConstructed));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberReferenceObserved));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberDestructed));
	}

	TEST_METHOD(CanonicalReferenceReturningTemporaryReceiverPreservesExactValueAddress)
	{
		GProdNativeLifecycleMemberConstructed = 0;
		GProdNativeLifecycleMemberDestructed = 0;
		GProdNativeLifecycleMemberReferenceObserved = 0;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdNativeLifecycleMember",
				sizeof(FProdNativeLifecycleMember),
				asOBJ_VALUE | asOBJ_APP_CLASS_CD
					| asOBJ_APP_CLASS_ALLINTS) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberConstruct),
			asCALL_GENERIC) > 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
			"FProdNativeLifecycleMember",
			asBEHAVE_DESTRUCT,
			"void f()",
			asFUNCTION(ProdNativeLifecycleMemberDestruct),
			asCALL_GENERIC) > 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectMethod(
			"FProdNativeLifecycleMember",
			"FProdNativeLifecycleMember& Append(int Value) accept_temporary_this",
			asFUNCTION(ProdNativeLifecycleMemberAppendAndReturnReference),
			asCALL_GENERIC) > 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"int ObserveNativeMember(const FProdNativeLifecycleMember&in Value)",
			asFUNCTION(ProdNativeLifecycleMemberObserveReference),
			asCALL_GENERIC) > 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdReferenceReturningTemporaryReceiver", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return ObserveNativeMember(
					FProdNativeLifecycleMember().Append(1));
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdReferenceReturningTemporaryReceiver.as",
			ScriptSource.c_str(),
			static_cast<asUINT>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("a reference-returning call on a materialized value receiver must lower through Canonical CodeGen")));

		const asCModule* const Internal = static_cast<asCModule*>(Module);
		const asCASTContext* const Ast = Internal->GetCanonicalASTContext();
		bool bReferenceCallStayedUnmaterialized = false;
		for (asUINT ExprIndex = 1;
			Ast != nullptr && ExprIndex <= Ast->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = Ast->GetExpr(asASTExprId(ExprIndex));
			const asCDecl* const Callee = Expr != nullptr
				? Ast->GetDecl(Expr->resolvedDecl) : nullptr;
			if (Expr != nullptr && Expr->kind == asAST_EXPR_CALL
				&& Callee != nullptr && Callee->name.Equals("Append"))
			{
				bReferenceCallStayedUnmaterialized = Expr->type.IsReference();
				for (asUINT WrapperIndex = 1;
					WrapperIndex <= Ast->GetExprCount(); ++WrapperIndex)
				{
					const asCExpr* const Wrapper =
						Ast->GetExpr(asASTExprId(WrapperIndex));
					if (Wrapper != nullptr
						&& Wrapper->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
						&& Wrapper->children.GetLength() == 1
						&& Wrapper->children[0] == Expr->id)
					{
						bReferenceCallStayedUnmaterialized = false;
					}
				}
				break;
			}
		}
		ASSERT_THAT(IsTrue(bReferenceCallStayedUnmaterialized,
			TEXT("a reference result is an lvalue alias of the receiver, not a new materialized object")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 42,
			TEXT("the chained reference return must preserve the temporary object's exact address")));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberConstructed));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberReferenceObserved));
		ASSERT_THAT(AreEqual(1, GProdNativeLifecycleMemberDestructed));
	}

	TEST_METHOD(PreparedStringLiteralUsesFactoryAndExactReferenceABI)
	{
		using namespace AngelscriptBuilderTestSupport;

		FProdCanonicalStringFactory StringFactory;
		GProdCanonicalStringFactory = &StringFactory;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			GProdCanonicalStringFactory = nullptr;
			Engine.Destroy();
		};

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"FProdCanonicalString",
			sizeof(FProdCanonicalStringValue),
			asOBJ_VALUE | asOBJ_POD
				| asGetTypeTraits<FProdCanonicalStringValue>()) >= 0,
			TEXT("string CodeGen fixture type must register")));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->RegisterStringFactory(
			"FProdCanonicalString", &StringFactory),
			TEXT("string CodeGen fixture factory must become Engine authority")));
		const int ObserveStringId = ScriptEngine->RegisterGlobalFunction(
			"int ObserveString(const FProdCanonicalString&in Text)",
			asFUNCTION(ProdObserveCanonicalString),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(ObserveStringId > 0,
			TEXT("string CodeGen fixture observer must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalStringLiteral");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalStringLiteral.as",
			R"AS(
				int RunPreparedStringLiteral()
				{
					return ObserveString("line\nmid\0tail");
				}
			)AS")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module),
			TEXT("the staged Builder must prepare the exact string literal fixture")));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST(),
			TEXT("the exact string literal AST must verify and seal")));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCExpr* Literal = nullptr;
		const asCExpr* ObserveCall = nullptr;
		asUINT StringLiteralCount = 0;
		for (asUINT ExprIndex = 1; ExprIndex <= Context->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Candidate = Context->GetExpr(asASTExprId(ExprIndex));
			if (Candidate != nullptr && Candidate->kind == asAST_EXPR_STRING_LITERAL)
			{
				Literal = Candidate;
				++StringLiteralCount;
			}
			const asCDecl* const Target = Candidate != nullptr
				? Context->GetDecl(Candidate->resolvedDecl) : nullptr;
			if (Candidate != nullptr
				&& Candidate->kind == asAST_EXPR_CALL
				&& Target != nullptr
				&& Target->name.Equals("ObserveString"))
			{
				ObserveCall = Candidate;
			}
		}

		static const char ExpectedBytes[] = {
			'l', 'i', 'n', 'e', '\n', 'm', 'i', 'd', '\0', 't', 'a', 'i', 'l'
		};
		asCString AstDump;
		asCASTDump(*Context, AstDump);
		const FString AstFailure = FString::Printf(
			TEXT("sealed Sema must own the exact decoded string payload and callee; AST:\n%s"),
			UTF8_TO_TCHAR(AstDump.AddressOf() ? AstDump.AddressOf() : ""));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1), StringLiteralCount, *AstFailure));
		ASSERT_THAT(IsNotNull(Literal, *AstFailure));
		ASSERT_THAT(IsNotNull(ObserveCall, *AstFailure));
		if (Literal == nullptr || ObserveCall == nullptr)
		{
			return;
		}
		const asCType* const LiteralType = Context->GetType(Literal->type.type);
		ASSERT_THAT(IsTrue(LiteralType != nullptr
			&& LiteralType->stableKey.Equals("FProdCanonicalString"), *AstFailure));
		ASSERT_THAT(IsTrue(ObserveCall->children.GetLength() == 1
			&& ObserveCall->children[0] == Literal->id,
			TEXT("the exact literal must remain the resolved observer argument")));
		ASSERT_THAT(AreEqual(static_cast<size_t>(sizeof(ExpectedBytes)),
			Literal->literal.GetLength(), *AstFailure));
		ASSERT_THAT(AreEqual(static_cast<asQWORD>(sizeof(ExpectedBytes)),
			Literal->literalBits, *AstFailure));
		bool LiteralBytesMatch = Literal->literal.GetLength() == sizeof(ExpectedBytes);
		for (size_t ByteIndex = 0;
			LiteralBytesMatch && ByteIndex < sizeof(ExpectedBytes); ++ByteIndex)
		{
			LiteralBytesMatch = Literal->literal[ByteIndex] == ExpectedBytes[ByteIndex];
		}
		ASSERT_THAT(IsTrue(LiteralBytesMatch,
			TEXT("CodeGen input must be semantic bytes, not quote/escape spelling")));

		asCBytecodeCodeGen CodeGen;
		const int CodeGenResult = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, CodeGenResult,
			*FString::Printf(
				TEXT("prepared string literal CodeGen must use the Engine string factory: result=%d line=%d detail=%s"),
				CodeGenResult,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (CodeGenResult != asAST_VERIFY_OK)
		{
			return;
		}

		asIScriptFunction* const Entry = Module->GetFunctionByDecl(
			"int RunPreparedStringLiteral()");
		ASSERT_THAT(IsNotNull(Entry));
		ASSERT_THAT(IsTrue(ProdBytecodeCallsFunction(Entry, ObserveStringId),
			TEXT("string literal bytecode must CALLSYS the exact sealed observer")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Module->GetLastLegacyCompilerInvocationCount()));
		ASSERT_THAT(AreEqual(2, StringFactory.GetAcquireCallCount(),
			TEXT("CodeGen lease and function-owned bytecode reference must each acquire once")));
		ASSERT_THAT(AreEqual(1, StringFactory.GetReleaseCallCount(),
			TEXT("the temporary CodeGen lease must release after AddReferences")));
		ASSERT_THAT(AreEqual(1, StringFactory.GetEntryCount()));
		ASSERT_THAT(AreEqual(1, StringFactory.GetTotalReferenceCount(),
			TEXT("only the published function may retain the constant after CodeGen")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int RunPreparedStringLiteral()", Value)
			&& Value == 42,
			*FString::Printf(
				TEXT("native observer must receive all 13 bytes through reference ABI; got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(1, StringFactory.GetObserverCallCount()));

		Engine.Destroy();
		GProdCanonicalStringFactory = nullptr;
		ASSERT_THAT(AreEqual(2, StringFactory.GetAcquireCallCount()));
		ASSERT_THAT(AreEqual(2, StringFactory.GetReleaseCallCount(),
			TEXT("Engine/module retirement must release the function-owned constant")));
		ASSERT_THAT(AreEqual(0, StringFactory.GetEntryCount()));
		ASSERT_THAT(AreEqual(0, StringFactory.GetTotalReferenceCount()));
	}

	TEST_METHOD(PreparedValueParameterBindsSourceIdentityToNormalizedRuntimeSignature)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalValueParameter");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalValueParameter.as",
			R"AS(
				struct FPreparedValueParameter
				{
					int Value;
				}

				int ReadPreparedValueParameter(FPreparedValueParameter Payload)
				{
					return Payload.Value;
				}

				int ReadPreparedBareReference(const FPreparedValueParameter& Payload)
				{
					return Payload.Value;
				}
			)AS")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(
			*TestRunner, Builder, Module)));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));

		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* FunctionDecl = nullptr;
		const asCDecl* ParameterDecl = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_FUNCTION
				&& Candidate->name.Equals("ReadPreparedValueParameter"))
			{
				FunctionDecl = Candidate;
				for (asUINT ChildIndex = 0;
					ChildIndex < Candidate->children.GetLength(); ++ChildIndex)
				{
					const asCDecl* const Child =
						Context->GetDecl(Candidate->children[ChildIndex]);
					if (Child != nullptr && Child->kind == asAST_DECL_PARAM)
					{
						ParameterDecl = Child;
						break;
					}
				}
				break;
			}
		}
		ASSERT_THAT(IsNotNull(FunctionDecl));
		ASSERT_THAT(IsNotNull(ParameterDecl));
		if (FunctionDecl == nullptr || ParameterDecl == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(ParameterDecl->type.IsConst()));
		ASSERT_THAT(IsFalse(ParameterDecl->type.IsReference()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("ReadPreparedValueParameter(FPreparedValueParameter)")),
			FString(UTF8_TO_TCHAR(FunctionDecl->stableKey.AddressOf())),
			TEXT("stable identity must preserve the source-level by-value signature")));

		asCScriptFunction* RuntimeFunction = nullptr;
		for (asUINT FunctionIndex = 0;
			FunctionIndex < Module->scriptFunctions.GetLength(); ++FunctionIndex)
		{
			asCScriptFunction* const Candidate =
				Module->scriptFunctions[FunctionIndex];
			if (Candidate != nullptr
				&& Candidate->name.Equals("ReadPreparedValueParameter"))
			{
				RuntimeFunction = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(RuntimeFunction));
		if (RuntimeFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(RuntimeFunction->parameterTypes.GetLength() == 1
			&& RuntimeFunction->parameterTypes[0].IsReadOnly()
			&& RuntimeFunction->parameterTypes[0].IsReference()
			&& RuntimeFunction->inOutFlags.GetLength() == 1
			&& RuntimeFunction->inOutFlags[0] == asTM_INOUTREF,
			TEXT("Builder must expose the exact normalized VM signature used in production")));

		asCRuntimeTypeBridge Bridge(ScriptEngine);
		asASTDeclId Bound;
		asCString Detail;
		const int BindResult = Bridge.BindFunctionDeclaration(
			*Context, *RuntimeFunction, Bound, &Detail);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_DECL_BIND_OK),
			static_cast<int32>(BindResult),
			*FString::Printf(
				TEXT("source-value parameter must bind its exact normalized Runtime ABI; detail=%s"),
				UTF8_TO_TCHAR(Detail.AddressOf()))));
		ASSERT_THAT(IsTrue(Bound == FunctionDecl->id));

		const asCDecl* BareReferenceDecl = nullptr;
		const asCDecl* BareReferenceParam = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate == nullptr
				|| Candidate->kind != asAST_DECL_FUNCTION
				|| !Candidate->name.Equals("ReadPreparedBareReference"))
			{
				continue;
			}
			BareReferenceDecl = Candidate;
			for (asUINT ChildIndex = 0;
				ChildIndex < Candidate->children.GetLength(); ++ChildIndex)
			{
				const asCDecl* const Child =
					Context->GetDecl(Candidate->children[ChildIndex]);
				if (Child != nullptr && Child->kind == asAST_DECL_PARAM)
				{
					BareReferenceParam = Child;
					break;
				}
			}
			break;
		}
		ASSERT_THAT(IsNotNull(BareReferenceDecl));
		ASSERT_THAT(IsNotNull(BareReferenceParam));
		if (BareReferenceDecl == nullptr || BareReferenceParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(BareReferenceParam->type.IsConst()
			&& BareReferenceParam->type.IsReference()
			&& (BareReferenceParam->type.quals & asAST_QUAL_PARAM_DIR_MASK) == 0,
			TEXT("the sealed source type must distinguish bare '&' from an explicit direction")));

		asCScriptFunction* BareReferenceRuntime = nullptr;
		for (asUINT FunctionIndex = 0;
			FunctionIndex < Module->scriptFunctions.GetLength(); ++FunctionIndex)
		{
			asCScriptFunction* const Candidate =
				Module->scriptFunctions[FunctionIndex];
			if (Candidate != nullptr
				&& Candidate->name.Equals("ReadPreparedBareReference"))
			{
				BareReferenceRuntime = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(BareReferenceRuntime));
		if (BareReferenceRuntime == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(BareReferenceRuntime->inOutFlags.GetLength() == 1
			&& BareReferenceRuntime->inOutFlags[0] == asTM_INOUTREF,
			TEXT("Builder must freeze bare parameter '&' as its default INOUTREF Runtime passing")));

		asASTDeclId BareReferenceBound;
		asCString BareReferenceDetail;
		const int BareReferenceBindResult = Bridge.BindFunctionDeclaration(
			*Context, *BareReferenceRuntime,
			BareReferenceBound, &BareReferenceDetail);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_DECL_BIND_OK),
			static_cast<int32>(BareReferenceBindResult),
			*FString::Printf(
				TEXT("bare parameter '&' must bind to its exact default Runtime passing; detail=%s"),
				UTF8_TO_TCHAR(BareReferenceDetail.AddressOf()))));
		ASSERT_THAT(IsTrue(BareReferenceBound == BareReferenceDecl->id));
	}

	TEST_METHOD(CanonicalPrimitiveValueParameterUsesBuilderRuntimeABI)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"CanonicalPrimitiveParameterABI",
			ASTEST_AS_ANSI(R"AS(
				int Echo(int Value)
				{
					return Value;
				}

				int Entry()
				{
					return Echo(42);
				}
				)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(),
			TEXT("Canonical primitive-parameter fixture must compile")));
		if (!Module.IsValid())
		{
			return;
		}
		asCModule* const RuntimeModule =
			static_cast<asCModule*>(Module.Get());
		ASSERT_THAT(IsNotNull(RuntimeModule));
		if (RuntimeModule == nullptr)
		{
			return;
		}

		asCScriptFunction* const Echo = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(RuntimeModule, "int Echo(const int)"));
		ASSERT_THAT(IsNotNull(Echo,
			TEXT("Canonical CodeGen must preserve Builder's public const-value parameter ABI")));
		if (Echo == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			Echo->parameterTypes.GetLength() == 1
				&& Echo->parameterTypes[0].IsPrimitive()
				&& Echo->parameterTypes[0].IsReadOnly()
				&& !Echo->parameterTypes[0].IsReference()
				&& Echo->inOutFlags.GetLength() == 1
				&& Echo->inOutFlags[0] == asTM_NONE,
			TEXT("primitive source values must lower to const, non-reference Runtime parameters")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(
				*TestRunner, ScriptEngine, RuntimeModule,
				"int Entry()", Value)
			&& Value == 42,
			TEXT("the normalized Canonical primitive call must execute")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			RuntimeModule->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			RuntimeModule->GetLastLegacyCompilerInvocationCount()));
	}

	TEST_METHOD(PreparedEnumInitializerConsumesSealedConstantWithoutLegacyCompiler)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalEnumInitializer");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalEnumInitializer.as",
			"enum EPreparedMode { Value = 41 }")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateFunctions()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Module->GetLastLegacyCompilerInvocationCount(),
			TEXT("Stage 2 must read the Canonical enumerator value instead of constructing asCCompiler")));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildLayoutClasses()));
		Builder.BuildAllocateGlobalVariables();
		ASSERT_THAT(AreEqual(asSUCCESS,
			Builder.BuildLayoutFunctionsWithoutGlobalInitializers()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));

		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* EnumeratorDecl = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			const asCDecl* const Owner = Candidate != nullptr
				? Context->GetDecl(Candidate->parent) : nullptr;
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_VAR
				&& Candidate->name.Equals("Value")
				&& Owner != nullptr
				&& Owner->kind == asAST_DECL_ENUM
				&& Owner->name.Equals("EPreparedMode"))
			{
				EnumeratorDecl = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsTrue(EnumeratorDecl != nullptr
				&& EnumeratorDecl->hasConstantValue
				&& EnumeratorDecl->constantValue == 41,
			TEXT("Sema must seal EPreparedMode::Value as the exact constant 41 before CodeGen")));

		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Result,
			*FString::Printf(
				TEXT("prepared enum CodeGen must consume the sealed constant: result=%d detail=%s"),
				Result,
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (Result != asAST_VERIFY_OK)
		{
			return;
		}

		asITypeInfo* const TypeInfo = Module->GetTypeInfoByName("EPreparedMode");
		int EnumValue = 0;
		const char* const EnumName = TypeInfo != nullptr
			&& TypeInfo->GetEnumValueCount() == 1
			? TypeInfo->GetEnumValueByIndex(0, &EnumValue) : nullptr;
		ASSERT_THAT(IsTrue(EnumName != nullptr
				&& FCStringAnsi::Strcmp(EnumName, "Value") == 0
				&& EnumValue == 41,
			TEXT("prepared publication must retain the exact runtime enum value")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Module->GetLastLegacyCompilerInvocationCount()));
		asQWORD Digest = 0;
		ASSERT_THAT(IsTrue(Module->GetLastCanonicalCodeGenAstDigest(Digest)
			&& Digest != 0));
	}

	TEST_METHOD(PreparedObjectGlobalConsumesSealedDefaultConstructionPolicy)
	{
		using namespace AngelscriptBuilderTestSupport;

		GProdDefaultGlobalConstructed = 0;
		GProdDefaultGlobalDestructed = 0;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdDefaultGlobalValue",
				sizeof(FProdDefaultGlobalValue),
				asOBJ_VALUE | asOBJ_APP_CLASS_CD
					| asOBJ_APP_CLASS_ALLINTS) >= 0,
			TEXT("default-global VALUE type must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdDefaultGlobalValue",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdDefaultGlobalConstruct),
				asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdDefaultGlobalValue",
				asBEHAVE_DESTRUCT,
				"void f()",
				asFUNCTION(ProdDefaultGlobalDestruct),
				asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FProdDefaultGlobalValue",
				"int Stored",
				asOFFSET(FProdDefaultGlobalValue, Stored)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalDefaultObjectGlobal");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalDefaultObjectGlobal.as",
			"const FProdDefaultGlobalValue _PreparedObjectGlobal; "
			"int PreparedObjectGlobalEntry() { "
			"  return _PreparedObjectGlobal.Stored; "
			"}")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));

		// Split BuildGenerateFunctions at its existing Stage 2 boundary. Parser/Sema
		// has already authored the exact VarDecl, while ParseScripts materializes
		// the Runtime global description from the retained native syntax tree.
		// Poison that tree only after the Runtime name/type shell exists: a
		// Canonical build must carry the producer DeclId directly and must not
		// reconstruct semantic identity from these mutable coordinates later.
		Builder.ParseScripts();
		ASSERT_THAT(AreEqual(0, Builder.numErrors));
		Builder.CompileInterfaces();
		Builder.CompileClasses(0);
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			Builder.globVariableList.GetLength()));
		sGlobalVariableDescription* const Description =
			Builder.globVariableList.GetLength() == 1
				? Builder.globVariableList[0] : nullptr;
		ASSERT_THAT(IsTrue(Description != nullptr
			&& Description->declaredAtNode != nullptr
			&& Description->name == "_PreparedObjectGlobal",
			TEXT("the fixture must materialize one exact Runtime global shell before late registration")));
		if (Description == nullptr || Description->declaredAtNode == nullptr)
		{
			return;
		}
		++Description->declaredAtNode->tokenPos;
		Builder.RegisterGlobalVariables();
		ASSERT_THAT(AreEqual(0, Builder.numErrors));
		ASSERT_THAT(IsTrue(
			Description->canonicalASTStableDeclKey.GetLength() > 0,
			TEXT("the Runtime global shell must retain the Parser/Sema producer identity after its native node coordinates change")));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildLayoutClasses()));
		Builder.BuildAllocateGlobalVariables();
		ASSERT_THAT(AreEqual(asSUCCESS,
			Builder.BuildLayoutFunctionsWithoutGlobalInitializers()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		ASSERT_THAT(IsTrue(Description != nullptr
			&& Description->property != nullptr
			&& Description->datatype.IsNonPrimitiveValueType()));
		if (Description == nullptr || Description->property == nullptr)
		{
			return;
		}
		asCGlobalProperty* const Property = Description->property;
		ASSERT_THAT(IsFalse(Property->isDefaultInit,
			TEXT("Stage 2 must not run or reproduce the legacy global compiler")));

		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };
		const asCDecl* GlobalDecl = nullptr;
		for (asUINT Index = 1; Index <= Context->GetDeclCount(); ++Index)
		{
			const asCDecl* const Candidate = Context->GetDecl(asASTDeclId(Index));
			if (Candidate != nullptr && Candidate->kind == asAST_DECL_VAR
				&& Candidate->name.Equals("_PreparedObjectGlobal"))
			{
				GlobalDecl = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsTrue(GlobalDecl != nullptr
			&& (GlobalDecl->traits & asAST_TRAIT_DEFAULT_INITIALIZED) != 0,
			TEXT("the CodeGen fixture requires a Sema-owned lifecycle decision")));

		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Result,
			*FString::Printf(
				TEXT("prepared default-object global CodeGen must succeed: result=%d line=%d detail=%s"),
				Result,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (Result != asAST_VERIFY_OK)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Property->isDefaultInit,
			TEXT("CodeGen must atomically publish the sealed default-construction policy")));
		ASSERT_THAT(IsNull(Property->GetInitFunc(),
			TEXT("default construction is a module lifecycle route, not a synthetic scalar initializer")));
		ASSERT_THAT(AreEqual(0, GProdDefaultGlobalConstructed));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->ResetGlobalVars(nullptr)));
		ASSERT_THAT(AreEqual(1, GProdDefaultGlobalConstructed));
		ASSERT_THAT(AreEqual(0, GProdDefaultGlobalDestructed));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int PreparedObjectGlobalEntry()", Value)
			&& Value == 42,
			TEXT("ResetGlobalVars must allocate and default-construct the exact object global")));

		ASSERT_THAT(AreEqual(asSUCCESS, Module->ResetGlobalVars(nullptr)));
		ASSERT_THAT(AreEqual(2, GProdDefaultGlobalConstructed));
		ASSERT_THAT(AreEqual(1, GProdDefaultGlobalDestructed));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Module->GetLastLegacyCompilerInvocationCount()));
	}

	TEST_METHOD(PreparedBuilderFailureRestoresEveryOriginalShell)
	{
		using namespace AngelscriptBuilderTestSupport;
		const char* const Source =
			"int _RollbackGlobal = 9; "
			"int First() { return Second(); } "
			"int Second() { return _RollbackGlobal + 1; }";

		AngelscriptNativeTestSupport::FMemoryBinaryStream FirstArtifact;
		{
			AngelscriptNativeTestSupport::FNativeTestEngine Producer;
			Producer.Create(*TestRunner);
			ON_SCOPE_EXIT { Producer.Destroy(); };
			asCScriptEngine* const ProducerEngine =
				static_cast<asCScriptEngine*>(Producer.Get());
			ASSERT_THAT(IsTrue(ProducerEngine->SetCompilerPipeline(
				asCOMPILER_PIPELINE_LEGACY)));
			asIScriptModule* const ProducerModule = ProducerEngine->GetModule(
				"PreparedCanonicalRollback", asGM_ALWAYS_CREATE);
			ASSERT_THAT(IsNotNull(ProducerModule));
			if (ProducerModule == nullptr)
			{
				return;
			}
			ASSERT_THAT(AreEqual(asSUCCESS, ProducerModule->AddScriptSection(
				"PreparedCanonicalRollback.as",
				Source,
				static_cast<asUINT>(std::strlen(Source)))));
			ASSERT_THAT(AreEqual(asSUCCESS, ProducerModule->Build()));
			asCScriptFunction* const ProducerFirst =
				static_cast<asCScriptFunction*>(
					ProducerModule->GetFunctionByDecl("int First()"));
			ASSERT_THAT(IsNotNull(ProducerFirst));
			if (ProducerFirst == nullptr)
			{
				return;
			}
			asCWriter Writer(
				static_cast<asCModule*>(ProducerModule),
				&FirstArtifact,
				ProducerEngine,
				true);
			ASSERT_THAT(AreEqual(asSUCCESS,
				Writer.WriteFunctionArtifact(ProducerFirst)));
			ASSERT_THAT(IsTrue(FirstArtifact.Num() > 0));
		}

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalRollback");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalRollback.as",
			Source)));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateFunctions()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildLayoutClasses()));
		Builder.BuildAllocateGlobalVariables();
		ASSERT_THAT(AreEqual(asSUCCESS,
			Builder.BuildLayoutFunctionsWithoutGlobalInitializers()));
		ASSERT_THAT(AreEqual(0, Builder.SealCanonicalAST()));
		asCScriptFunction* const First = static_cast<asCScriptFunction*>(
			Module->GetFunctionByDecl("int First()"));
		asCScriptFunction* const Second = static_cast<asCScriptFunction*>(
			Module->GetFunctionByDecl("int Second()"));
		ASSERT_THAT(IsNotNull(First));
		ASSERT_THAT(IsNotNull(Second));
		if (First == nullptr || Second == nullptr)
		{
			return;
		}
		asCScriptFunction::ScriptFunctionData* const FirstOriginal =
			First->scriptData;
		asCScriptFunction::ScriptFunctionData* const SecondOriginal =
			Second->scriptData;
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			Builder.globVariableList.GetLength()));
		sGlobalVariableDescription* const PreparedGlobal =
			Builder.globVariableList.GetLength() == 1
				? Builder.globVariableList[0] : nullptr;
		ASSERT_THAT(IsNotNull(PreparedGlobal));
		if (PreparedGlobal == nullptr || PreparedGlobal->property == nullptr)
		{
			return;
		}
		asCGlobalProperty* const OriginalProperty = PreparedGlobal->property;
		const asUINT OriginalEngineFunctionSlots =
			ScriptEngine->scriptFunctions.GetLength();
		const asUINT OriginalFreeFunctionSlots =
			ScriptEngine->freeScriptFunctionIds.GetLength();
		const int OriginalSecondInternalReferences =
			Second->internalRefCount.get();

		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };
		FPreparedRestoreRollbackContext Restore;
		Restore.FirstArtifact = &FirstArtifact;
		Module->SetBuildArtifactRestoreCallback(
			&RestorePreparedFirstForRollback, &Restore);
		asCBytecodeCodeGen CodeGen;
		CodeGen.SetTestFailureBeforeFunctionEmission(1, asERROR);
		ASSERT_THAT(IsTrue(
			CodeGen.GeneratePreparedModule(*Context, Builder) < 0,
			TEXT("the deterministic second-body boundary must reject publication")));
		ASSERT_THAT(IsTrue(First->scriptData == FirstOriginal
			&& Second->scriptData == SecondOriginal,
			TEXT("failure must restore both exact pre-CodeGen ScriptFunctionData owners")));
		ASSERT_THAT(AreEqual(1, Restore.FirstRestoreCount,
			TEXT("the first prepared body must restore exactly once before the injected failure")));
		ASSERT_THAT(IsTrue(Restore.PendingFunctionCount >= 2
			&& Restore.bSawPendingSecond,
			TEXT("the restore callback must receive the complete pending function graph")));
		ASSERT_THAT(AreEqual(OriginalSecondInternalReferences,
			Second->internalRefCount.get(),
			TEXT("rollback must release the restored First-to-pending-Second call reference")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			First->scriptData->byteCode.GetLength()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Second->scriptData->byteCode.GetLength()));
		ASSERT_THAT(IsTrue(PreparedGlobal->property == OriginalProperty
			&& !PreparedGlobal->isCompiled
			&& OriginalProperty->GetInitFunc() == nullptr,
			TEXT("failure after a prepared global candidate must leave the exact property unpublished")));
		ASSERT_THAT(AreEqual(OriginalEngineFunctionSlots,
			ScriptEngine->scriptFunctions.GetLength(),
			TEXT("failed prepared global candidates must not consume an Engine function slot")));
		ASSERT_THAT(AreEqual(OriginalFreeFunctionSlots,
			ScriptEngine->freeScriptFunctionIds.GetLength(),
			TEXT("failed prepared global candidates must preserve the free-id stack")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_NONE,
			Module->GetLastBytecodePublisher(),
			TEXT("failed prepared CodeGen must not claim a publisher")));
		asQWORD Digest = 0;
		ASSERT_THAT(IsFalse(Module->GetLastCanonicalCodeGenAstDigest(Digest),
			TEXT("failed prepared CodeGen must not publish an AST digest")));
	}

	TEST_METHOD(PreparedPureConstantGlobalPublicationFlagsRollBackBeforeRetry)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalPureConstantRollback");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalPureConstantRollback.as",
			"const int _PreparedPureConstantRollback = 41; "
			"int PreparedPureConstantRollbackEntry() { "
			"  return _PreparedPureConstantRollback + 1; "
			"}")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateFunctions()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildLayoutClasses()));
		Builder.BuildAllocateGlobalVariables();
		ASSERT_THAT(AreEqual(asSUCCESS,
			Builder.BuildLayoutFunctionsWithoutGlobalInitializers()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			Builder.globVariableList.GetLength()));
		sGlobalVariableDescription* const PreparedGlobal =
			Builder.globVariableList.GetLength() == 1
				? Builder.globVariableList[0] : nullptr;
		ASSERT_THAT(IsNotNull(PreparedGlobal));
		if (PreparedGlobal == nullptr || PreparedGlobal->property == nullptr)
		{
			return;
		}
		asCGlobalProperty* const Property = PreparedGlobal->property;
		asCScriptFunction* const Entry = static_cast<asCScriptFunction*>(
			Module->GetFunctionByDecl(
				"int PreparedPureConstantRollbackEntry()"));
		ASSERT_THAT(IsNotNull(Entry));
		if (Entry == nullptr || Entry->scriptData == nullptr)
		{
			return;
		}

		// Stage 2 owns an unpublished property shell. Make its transaction
		// boundary explicit even if a future Builder optimization begins carrying
		// the semantic const fact earlier than it does today.
		PreparedGlobal->isPureConstant = false;
		Property->isPureConstant = false;
		const asDWORD OriginalStorage =
			*static_cast<asDWORD*>(Property->GetAddressOfValue());
		ASSERT_THAT(IsFalse(PreparedGlobal->isCompiled));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		asCBytecodeCodeGen RejectedGlobalStateCodeGen;
		RejectedGlobalStateCodeGen
			.SetTestFailureBeforePreparedGlobalStateRegistration(
				0, asOUT_OF_MEMORY);
		const int RejectedGlobalStateResult =
			RejectedGlobalStateCodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asOUT_OF_MEMORY, RejectedGlobalStateResult,
			TEXT("a failed prepared global-state registration must surface as OOM")));
		ASSERT_THAT(IsTrue(
			RejectedGlobalStateCodeGen
				.WasTestPreparedGlobalStateRegistrationFailureInjected(),
			TEXT("the focused transaction seam must reach the pure-constant state registration")));
		ASSERT_THAT(IsTrue(
			!PreparedGlobal->isPureConstant
				&& !Property->isPureConstant
				&& !PreparedGlobal->isCompiled
				&& Property->GetInitFunc() == nullptr
				&& *static_cast<asDWORD*>(Property->GetAddressOfValue())
					== OriginalStorage
				&& Entry->scriptData->byteCode.GetLength() == 0
				&& Module->GetLastBytecodePublisher()
					== asBYTECODE_PUBLISHER_NONE,
			TEXT("global-state OOM must occur before pure-constant flags or candidate state become visible")));

		asCBytecodeCodeGen RejectedCodeGen;
		RejectedCodeGen.SetTestFailureBeforeFunctionEmission(0, asERROR);
		const int RejectedResult =
			RejectedCodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(IsTrue(RejectedResult < 0,
			TEXT("the deterministic body-emission boundary must reject publication")));
		ASSERT_THAT(IsTrue(
			!PreparedGlobal->isPureConstant
				&& !Property->isPureConstant
				&& !PreparedGlobal->isCompiled
				&& Property->GetInitFunc() == nullptr
				&& *static_cast<asDWORD*>(Property->GetAddressOfValue())
					== OriginalStorage
				&& Entry->scriptData->byteCode.GetLength() == 0
				&& Module->GetLastBytecodePublisher()
					== asBYTECODE_PUBLISHER_NONE,
			TEXT("failure must restore pure-constant flags, storage, body, and provenance")));

		asCBytecodeCodeGen RetryCodeGen;
		const int RetryResult =
			RetryCodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, RetryResult,
			*FString::Printf(
				TEXT("the same prepared Builder must retry: result=%d detail=%s"),
				RetryResult,
				UTF8_TO_TCHAR(RetryCodeGen.GetFailureDetail().AddressOf()
					? RetryCodeGen.GetFailureDetail().AddressOf() : ""))));
		if (RetryResult != asAST_VERIFY_OK)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			PreparedGlobal->isPureConstant
				&& Property->isPureConstant
				&& PreparedGlobal->isCompiled
				&& *static_cast<asDWORD*>(Property->GetAddressOfValue()) == 41,
			TEXT("successful commit must publish the sealed pure constant exactly once")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner,
			Engine.Get(),
			Module,
			"int PreparedPureConstantRollbackEntry()",
			Value) && Value == 42,
			*FString::Printf(
				TEXT("the retried canonical body must execute 42; got=%d"), Value)));
	}

	TEST_METHOD(PreparedScalarGlobalKeepsPropertyIdentityAndCanonicalInitializer)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalGlobal");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalGlobal.as",
			"int _PreparedGlobal = 41; "
			"int PreparedGlobalEntry() { return _PreparedGlobal + 1; }")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateFunctions()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildLayoutClasses()));
		Builder.BuildAllocateGlobalVariables();
		ASSERT_THAT(AreEqual(asSUCCESS,
			Builder.BuildLayoutFunctionsWithoutGlobalInitializers(),
			TEXT("Canonical Stage 2 must lay out shells without invoking the legacy global compiler")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Module->GetLastLegacyCompilerInvocationCount()));

		const int GlobalIndex = Module->GetGlobalVarIndexByName("_PreparedGlobal");
		ASSERT_THAT(IsTrue(GlobalIndex >= 0));
		if (GlobalIndex < 0)
		{
			return;
		}
		asCGlobalProperty* const PreparedProperty =
			Module->scriptGlobalsList[static_cast<asUINT>(GlobalIndex)];
		ASSERT_THAT(IsNotNull(PreparedProperty));
		ASSERT_THAT(IsTrue(PreparedProperty != nullptr
			&& PreparedProperty->GetInitFunc() == nullptr,
			TEXT("Stage 2 must leave the prepared global initializer unpublished")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		const asCDecl* GlobalDecl = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_VAR
				&& Candidate->name.Equals("_PreparedGlobal"))
			{
				GlobalDecl = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsTrue(GlobalDecl != nullptr
			&& GlobalDecl->stableKey.GetLength() > 0
			&& GlobalDecl->hasConstantValue
			&& GlobalDecl->constantValue == 41,
			TEXT("the sealed AST must own the prepared global's exact identity and initializer value")));

		asCBytecodeCodeGen CodeGen;
		const int CodeGenResult =
			CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK,
			CodeGenResult,
			*FString::Printf(
				TEXT("prepared scalar-global CodeGen must succeed: result=%d line=%d section=%s detail=%s"),
				CodeGenResult,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureSection().AddressOf()
					? CodeGen.GetFailureSection().AddressOf() : ""),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		ASSERT_THAT(IsTrue(
			Module->scriptGlobalsList[static_cast<asUINT>(GlobalIndex)]
				== PreparedProperty,
			TEXT("CodeGen must initialize the exact Stage 2 global property")));
		ASSERT_THAT(IsNotNull(PreparedProperty->GetInitFunc()));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->ResetGlobalVars(nullptr)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int PreparedGlobalEntry()", Value)
			&& Value == 42,
			TEXT("the prepared body must read the Canonical initializer result")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Module->GetLastLegacyCompilerInvocationCount()));
	}

	TEST_METHOD(PreparedGlobalMissingAddressPublicationFailsClosedAndRollsBack)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalMissingGlobalAddress");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalMissingGlobalAddress.as",
			"int _PreparedMissingAddress = 12; "
			"int ReadPreparedMissingAddress() { return _PreparedMissingAddress; }")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateFunctions()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildLayoutClasses()));
		Builder.BuildAllocateGlobalVariables();
		ASSERT_THAT(AreEqual(asSUCCESS,
			Builder.BuildLayoutFunctionsWithoutGlobalInitializers()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));

		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			Builder.globVariableList.GetLength()));
		sGlobalVariableDescription* const Description =
			Builder.globVariableList.GetLength() == 1
				? Builder.globVariableList[0] : nullptr;
		ASSERT_THAT(IsTrue(Description != nullptr
			&& Description->property != nullptr));
		asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
			Module->GetFunctionByDecl("int ReadPreparedMissingAddress()"));
		ASSERT_THAT(IsNotNull(Function));
		if (Description == nullptr || Description->property == nullptr
			|| Function == nullptr)
		{
			return;
		}

		asCGlobalProperty* const Property = Description->property;
		void* const Address = Property->GetAddressOfValue();
		asCScriptFunction::ScriptFunctionData* const OriginalData =
			Function->scriptData;
		ASSERT_THAT(IsNotNull(ScriptEngine->varAddressMap.Find(Address)));
		ScriptEngine->varAddressMap.Remove(Address);
		ON_SCOPE_EXIT
		{
			if (ScriptEngine->varAddressMap.Find(Address) == nullptr)
			{
				ScriptEngine->varAddressMap.Add(Address, Property);
			}
		};

		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };
		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asINVALID_CONFIGURATION, Result));
		ASSERT_THAT(IsTrue(FString(UTF8_TO_TCHAR(
			CodeGen.GetFailureDetail().AddressOf())).Contains(
			TEXT("prepared global address is not published")),
			TEXT("the missing address must produce a stable fail-closed diagnostic")));
		ASSERT_THAT(IsTrue(Function->scriptData == OriginalData
			&& Function->scriptData->byteCode.GetLength() == 0,
			TEXT("the reference preflight failure must restore the exact prepared function shell")));
		ASSERT_THAT(IsTrue(!Description->isCompiled
			&& Property->GetInitFunc() == nullptr,
			TEXT("the reference preflight failure must leave the prepared global unpublished")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_NONE,
			Module->GetLastBytecodePublisher()));
	}

	TEST_METHOD(PreparedReferenceClassKeepsFactoryIdentityAndExecutesCanonicalConstructor)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalFactory");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalFactory.as",
			"class FPreparedRef { "
			"  int Value; "
			"  FPreparedRef(int InValue) { Value = InValue; } "
			"  int Read() { return Value; } "
			"} "
			"int PreparedFactoryEntry() { "
			"  FPreparedRef Value = FPreparedRef(42); "
			"  return Value.Read(); "
			"} "
			"int PreparedDefaultFactoryEntry() { "
			"  FPreparedRef Value; "
			"  return Value.Read(); "
			"}")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateFunctions()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildLayoutClasses()));
		Builder.BuildAllocateGlobalVariables();
		ASSERT_THAT(AreEqual(asSUCCESS,
			Builder.BuildLayoutFunctionsWithoutGlobalInitializers()));
		ASSERT_THAT(IsTrue(Builder.factories.GetLength() > 0,
			TEXT("Stage 2 must register reference-class factory shells")));

		asCObjectType* ObjectType = nullptr;
		for (asUINT Index = 0; Index < Module->classTypes.GetLength(); ++Index)
		{
			asCObjectType* const Candidate = Module->classTypes[Index];
			if (Candidate != nullptr && Candidate->name == "FPreparedRef")
			{
				ObjectType = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ObjectType));
		asCScriptFunction* PreparedFactory = nullptr;
		int PreparedFactoryId = -1;
		for (asUINT Index = 0; Index < Builder.factories.GetLength(); ++Index)
		{
			const sFactoryDescription& Description = Builder.factories[Index];
			asCScriptFunction* const Candidate = Description.funcId >= 0
				&& static_cast<asUINT>(Description.funcId)
					< ScriptEngine->scriptFunctions.GetLength()
				? ScriptEngine->scriptFunctions[Description.funcId] : nullptr;
			if (Description.objType == ObjectType && Candidate != nullptr
				&& Candidate->parameterTypes.GetLength() == 1)
			{
				PreparedFactory = Candidate;
				PreparedFactoryId = Description.funcId;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(PreparedFactory,
			TEXT("the authored one-argument constructor must own one prepared factory shell")));
		if (ObjectType == nullptr || PreparedFactory == nullptr)
		{
			return;
		}
		asCScriptFunction::ScriptFunctionData* const OriginalFactoryData =
			PreparedFactory->scriptData;
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			OriginalFactoryData->byteCode.GetLength()));
		ASSERT_THAT(IsTrue(ObjectType->beh.factories.IndexOf(PreparedFactoryId) >= 0,
			TEXT("the object behaviour table must already own the exact Stage 2 factory id")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };
		const asCDecl* ConstructorDecl = nullptr;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Candidate =
				Context->GetDecl(asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_CONSTRUCTOR
				&& Candidate->name.Equals("FPreparedRef")
				&& Candidate->body.IsValid())
			{
				ConstructorDecl = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsTrue(ConstructorDecl != nullptr
			&& ConstructorDecl->stableKey.GetLength() > 0,
			TEXT("the sealed AST must own the authored constructor body and exact stable identity")));

		asCBytecodeCodeGen CodeGen;
		const int Result = CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Result,
			*FString::Printf(
				TEXT("prepared class/factory CodeGen must succeed: result=%d line=%d detail=%s"),
				Result,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (Result != asAST_VERIFY_OK)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScriptEngine->scriptFunctions[PreparedFactoryId]
			== PreparedFactory,
			TEXT("CodeGen must populate, not replace, the Stage 2 factory shell")));
		ASSERT_THAT(IsTrue(PreparedFactory->scriptData != nullptr
			&& PreparedFactory->scriptData->byteCode.GetLength() > 0,
			TEXT("the exact Stage 2 factory function shell must atomically install executable detached ScriptFunctionData")));
		ASSERT_THAT(IsTrue(PreparedFactory->scriptData != OriginalFactoryData,
			TEXT("successful publication must displace the empty prepared body only after the detached body is complete")));
		ASSERT_THAT(IsTrue(ObjectType->beh.factories.IndexOf(PreparedFactoryId) >= 0));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asBUILD_ARTIFACT_INVOCATION_FACTORY),
			static_cast<int32>(PreparedFactory->artifactInvocationKind),
			TEXT("the prepared Runtime wrapper must remain explicitly classified as a factory")));
		ASSERT_THAT(IsTrue(PreparedFactory->artifactOwnerType == ObjectType,
			TEXT("the synthetic factory must retain its constructor owner independently of objectType")));
		asCRuntimeTypeBridge Bridge(ScriptEngine);
		asASTDeclId BoundFactoryDeclaration;
		asCString FactoryBindDetail;
		const int FactoryBindResult = Bridge.BindFunctionDeclaration(
			*Context,
			*PreparedFactory,
			BoundFactoryDeclaration,
			&FactoryBindDetail);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_DECL_BIND_OK),
			static_cast<int32>(FactoryBindResult),
			*FString::Printf(
				TEXT("a synthetic factory must bind the exact producer-carried constructor declaration through its derived Runtime ABI; detail=%s"),
				UTF8_TO_TCHAR(FactoryBindDetail.AddressOf()))));
		ASSERT_THAT(IsTrue(BoundFactoryDeclaration == ConstructorDecl->id,
			TEXT("the factory wrapper must publish the constructor DeclId, never invent a second source declaration")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int PreparedFactoryEntry()", Value)
			&& Value == 42,
			TEXT("the prepared factory must allocate an object and run the Canonical constructor")));
		Value = -1;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int PreparedDefaultFactoryEntry()", Value)
			&& Value == 0,
			TEXT("the generated default constructor and factory must execute their sealed field-initialization plan")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Module->GetLastLegacyCompilerInvocationCount()));
	}

	TEST_METHOD(PreparedGeneratedDefaultFactoryPublishesConstructorSourceMetadataAfterBodySwap)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedFactorySourceMetadata");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedFactorySourceMetadata.as",
			"class FFactorySourceMetadata { int Value; }")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateFunctions()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildLayoutClasses()));
		Builder.BuildAllocateGlobalVariables();
		ASSERT_THAT(AreEqual(asSUCCESS,
			Builder.BuildLayoutFunctionsWithoutGlobalInitializers()));

		asCObjectType* ObjectType = nullptr;
		for (asUINT TypeIndex = 0;
			TypeIndex < Module->classTypes.GetLength(); ++TypeIndex)
		{
			asCObjectType* const Candidate = Module->classTypes[TypeIndex];
			if (Candidate != nullptr
				&& Candidate->name == "FFactorySourceMetadata")
			{
				ObjectType = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}

		asCScriptFunction* Factory = nullptr;
		for (asUINT FactoryIndex = 0;
			FactoryIndex < Builder.factories.GetLength(); ++FactoryIndex)
		{
			const sFactoryDescription& Description =
				Builder.factories[FactoryIndex];
			asCScriptFunction* const Candidate = Description.funcId >= 0
				&& static_cast<asUINT>(Description.funcId)
					< ScriptEngine->scriptFunctions.GetLength()
				? ScriptEngine->scriptFunctions[Description.funcId] : nullptr;
			if (Description.objType == ObjectType && Candidate != nullptr
				&& Candidate->parameterTypes.GetLength() == 0)
			{
				Factory = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(Factory));
		if (Factory == nullptr)
		{
			return;
		}
		asCScriptFunction::ScriptFunctionData* const PreparedData =
			Factory->scriptData;
		ASSERT_THAT(IsTrue(PreparedData != nullptr
			&& PreparedData->byteCode.GetLength() == 0,
			TEXT("Stage 2 must expose an empty generated default factory shell")));

		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));
		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		asCBytecodeCodeGen CodeGen;
		const int GenerateResult =
			CodeGen.GeneratePreparedModule(*Context, Builder);
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, GenerateResult,
			*FString::Printf(
				TEXT("prepared factory CodeGen must succeed: result=%d line=%d detail=%s"),
				GenerateResult,
				CodeGen.GetLastFailLine(),
				UTF8_TO_TCHAR(CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : ""))));
		if (GenerateResult != asAST_VERIFY_OK)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asBUILD_ARTIFACT_INVOCATION_FACTORY),
			static_cast<int32>(Factory->artifactInvocationKind),
			TEXT("the inspected Runtime function must be the synthetic factory wrapper")));
		ASSERT_THAT(IsTrue(Factory->scriptData != nullptr
			&& Factory->scriptData->byteCode.GetLength() > 0,
			TEXT("the prepared factory must own its generated detached body")));
		ASSERT_THAT(IsTrue(Factory->scriptData != PreparedData,
			TEXT("successful publication must replace the empty Stage 2 body")));
		ASSERT_THAT(IsTrue(Factory->canonicalASTStableDeclKey.GetLength() > 0,
			TEXT("the factory must derive the exact sealed constructor identity")));
		ASSERT_THAT(IsTrue(Factory->GetScriptSectionName() != nullptr,
			TEXT("detached factory publication must preserve the constructor source section")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("PreparedFactorySourceMetadata.as")),
			FString(UTF8_TO_TCHAR(Factory->GetScriptSectionName()
				? Factory->GetScriptSectionName() : "")),
			TEXT("the factory debug section must come from its exact sealed constructor")));
		ASSERT_THAT(IsTrue(Factory->scriptData != nullptr
			&& Factory->scriptData->declaredAt != 0,
			TEXT("the factory declaration position must survive the detached body swap")));
	}

	TEST_METHOD(PreparedFactoryEmissionFailureRestoresEveryRuntimeShellAndBehaviour)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "PreparedCanonicalFactoryRollback");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"PreparedCanonicalFactoryRollback.as",
			"class FPreparedRollbackRef { "
			"  int Value; "
			"  FPreparedRollbackRef(int InValue) { Value = InValue; } "
			"  int Read() { return Value; } "
			"} "
			"int PreparedRollbackEntry() { "
			"  FPreparedRollbackRef Value = FPreparedRollbackRef(42); "
			"  return Value.Read(); "
			"}")));
		ASSERT_THAT(IsNotNull(Module->builder));
		if (Module->builder == nullptr)
		{
			return;
		}

		asCBuilder& Builder = *Module->builder;
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildParallelParseScripts()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateTypes()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildGenerateFunctions()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.BuildLayoutClasses()));
		Builder.BuildAllocateGlobalVariables();
		ASSERT_THAT(AreEqual(asSUCCESS,
			Builder.BuildLayoutFunctionsWithoutGlobalInitializers()));
		ASSERT_THAT(AreEqual(asSUCCESS, Builder.SealCanonicalAST()));

		asCASTContext* const Context = Builder.TakeCanonicalAST();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { asDELETE(Context, asCASTContext); };

		TArray<asCScriptFunction*> PreparedFunctions;
		TArray<asCScriptFunction::ScriptFunctionData*> OriginalFunctionData;
		for (asUINT FunctionIndex = 0;
			FunctionIndex < Builder.functions.GetLength(); ++FunctionIndex)
		{
			const sFunctionDescription* const Description =
				Builder.functions[FunctionIndex];
			asCScriptFunction* const Function = Description != nullptr
				&& Description->funcId >= 0
				&& static_cast<asUINT>(Description->funcId)
					< ScriptEngine->scriptFunctions.GetLength()
				? ScriptEngine->scriptFunctions[Description->funcId] : nullptr;
			if (Description == nullptr || Function == nullptr
				|| Description->isExistingShared)
			{
				continue;
			}
			PreparedFunctions.Add(Function);
			OriginalFunctionData.Add(Function->scriptData);
			const asCDecl* BoundDecl = nullptr;
			for (asUINT DeclIndex = 1;
				DeclIndex <= Context->GetDeclCount(); ++DeclIndex)
			{
				const asCDecl* const Candidate =
					Context->GetDecl(asASTDeclId(DeclIndex));
				if (Candidate != nullptr
					&& Candidate->stableKey
						== Function->canonicalASTStableDeclKey)
				{
					BoundDecl = Candidate;
					break;
				}
			}
			ASSERT_THAT(IsNotNull(BoundDecl,
				TEXT("every prepared function must carry a producer-bound sealed declaration")));
			if (BoundDecl != nullptr && (BoundDecl->body.IsValid()
				|| BoundDecl->kind == asAST_DECL_CONSTRUCTOR
				|| BoundDecl->kind == asAST_DECL_DESTRUCTOR))
			{
				ASSERT_THAT(IsTrue(BoundDecl->stableKey.GetLength() > 0,
					TEXT("every emitted prepared function must retain its sealed stable key")));
			}
		}

		TArray<asCScriptFunction*> PreparedFactories;
		TArray<asCScriptFunction::ScriptFunctionData*> OriginalFactoryData;
		TArray<asCString> OriginalFactoryKeys;
		TArray<int> OriginalFactoryIds;
		asUINT OneArgumentFactoryOrdinal = static_cast<asUINT>(-1);
		asCObjectType* FactoryOwner = nullptr;
		for (asUINT FactoryIndex = 0;
			FactoryIndex < Builder.factories.GetLength(); ++FactoryIndex)
		{
			const sFactoryDescription& Description = Builder.factories[FactoryIndex];
			asCScriptFunction* const Factory = Description.funcId >= 0
				&& static_cast<asUINT>(Description.funcId)
					< ScriptEngine->scriptFunctions.GetLength()
				? ScriptEngine->scriptFunctions[Description.funcId] : nullptr;
			ASSERT_THAT(IsNotNull(Factory));
			if (Factory == nullptr)
			{
				continue;
			}
			PreparedFactories.Add(Factory);
			OriginalFactoryData.Add(Factory->scriptData);
			OriginalFactoryKeys.Add(Factory->canonicalASTStableDeclKey);
			OriginalFactoryIds.Add(Description.funcId);
			if (Factory->parameterTypes.GetLength() == 1)
			{
				OneArgumentFactoryOrdinal = FactoryIndex;
				FactoryOwner = Description.objType;
			}
		}
		ASSERT_THAT(IsTrue(OneArgumentFactoryOrdinal != static_cast<asUINT>(-1)
			&& PreparedFactories.Num() == OriginalFactoryData.Num()
			&& PreparedFactories.Num() >= 2,
			TEXT("the fixture must stage default and one-argument factory shells")));
		if (OneArgumentFactoryOrdinal == static_cast<asUINT>(-1)
			|| FactoryOwner == nullptr)
		{
			return;
		}

		asCBytecodeCodeGen CodeGen;
		CodeGen.SetTestFailureBeforePreparedFactoryEmission(
			OneArgumentFactoryOrdinal, asERROR);
		ASSERT_THAT(AreEqual(asERROR,
			CodeGen.GeneratePreparedModule(*Context, Builder)));
		const FString FailureDetail = UTF8_TO_TCHAR(
			CodeGen.GetFailureDetail().AddressOf());
		ASSERT_THAT(IsTrue(FailureDetail.Contains(
			TEXT("injected-prepared-factory-emission")),
			*FString::Printf(
				TEXT("the failure must occur at the deterministic factory emission boundary; actual=%s"),
				*FailureDetail)));

		for (int32 Index = 0; Index < PreparedFunctions.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(PreparedFunctions[Index]->scriptData
				== OriginalFunctionData[Index]
				&& PreparedFunctions[Index]->scriptData->byteCode.GetLength() == 0,
				TEXT("factory failure must restore every exact prepared function body owner")));
		}
		for (int32 Index = 0; Index < PreparedFactories.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(PreparedFactories[Index]->scriptData
				== OriginalFactoryData[Index]
				&& PreparedFactories[Index]->scriptData->byteCode.GetLength() == 0
				&& PreparedFactories[Index]->canonicalASTStableDeclKey
					== OriginalFactoryKeys[Index],
				TEXT("factory failure must restore every exact factory body and preserve its producer identity")));
			ASSERT_THAT(IsTrue(FactoryOwner->beh.factories.IndexOf(
				OriginalFactoryIds[Index]) >= 0,
				TEXT("factory failure must not detach any Stage 2 behaviour-table entry")));
		}
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_NONE,
			Module->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Module->GetLastLegacyCompilerInvocationCount()));
	}

	TEST_METHOD(CanonicalModuleBuildPublishesCodeGenForIntegerReturn)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("CANONICAL selection must remain available")));
		ASSERT_THAT(IsTrue(ScriptEngine->IsCanonicalBytecodeCodeGenReady(),
			TEXT("Ready is true only because CANONICAL Build() calls Generate")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				return 7;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdInt",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL integer Build must succeed from CodeGen")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 7,
			TEXT("CANONICAL module Build must execute F()==7 from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL Build of int F() must not record asCCompiler")));
	}

	TEST_METHOD(LegacyModuleBuildStillPublishesCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("legacy publication fixture must explicitly select the rollback compiler")));
		ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_LEGACY, ScriptEngine->GetCompilerPipeline(),
			TEXT("explicit LEGACY selection must remain effective")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				return 7;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdLegacy",
			ScriptSource.c_str(),
			Module),
			TEXT("LEGACY integer Build must still succeed")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_COMPILER,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("LEGACY Build must keep asCCompiler")));
	}

	TEST_METHOD(CanonicalValueObjectBuildPublishesCodeGenAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("value-object fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FValue
			{
				int Value;
				FValue()
				{
					Value = 41;
				}
			}

			int F()
			{
				FValue Object;
				return Object.Value + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdValue",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL value-object Build must succeed from CodeGen, not fail closed")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL value-object F() must execute from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL value-object Build must not record asCCompiler")));
	}

	TEST_METHOD(CanonicalOverloadedAssignExecutesOpAssignCallWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 opAssign CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdOpAssignCallRewrite", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				T& opAssign(int Value)
				{
					Stored = Value;
					return this;
				}
			}

			int Entry()
			{
				T Object;
				Object = 7;
				return Object.Stored;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdOpAssignCallRewrite.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Object = 7 must publish Canonical CodeGen, not asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("overloaded assignment Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("overloaded assignment must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 7,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Object = 7 through T::opAssign(int); Entry()=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalOverloadedCompoundAssignExecutesOpAddAssignCallWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 opAddAssign CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdOpAddAssignCallRewrite", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				T& opAddAssign(int Value)
				{
					Stored = Stored + Value;
					return this;
				}
			}

			int Entry()
			{
				T Object;
				Object += 7;
				return Object.Stored;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdOpAddAssignCallRewrite.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Object += 7 must publish Canonical CodeGen, not asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("overloaded compound assignment Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("overloaded compound assignment must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 7,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Object += 7 through T::opAddAssign(int); Entry()=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalReverseOperatorExecutesOpAddRCallWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 opAdd_r CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdOpAddRCallRewrite", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 2;

				int opAdd_r(int Value) const
				{
					return Value + Stored;
				}
			}

			int Entry()
			{
				T Object;
				return 40 + Object;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdOpAddRCallRewrite.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("40 + Object must publish Canonical CodeGen, not asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("reverse operator Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("reverse operator must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute 40 + Object through T::opAdd_r(int); Entry()=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalInequalityExecutesNegatedOpEqualsCallWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 opEquals inequality CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdOpEqualsInequalityRewrite", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				bool opEquals(const T&in Other) const
				{
					return Stored == Other.Stored;
				}
			}

			int Entry()
			{
				T Left;
				T Right;
				Right.Stored = 1;
				return Left != Right ? 42 : 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdOpEqualsInequalityRewrite.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Left != Right must publish Canonical CodeGen, not asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("inequality Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("inequality must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Left != Right as !opEquals; Entry()=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalLessThanExecutesOpCmpComparedAgainstZeroWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 opCmp less-than CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdOpCmpLessThanRewrite", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				int opCmp(const T&in Other) const
				{
					return Stored - Other.Stored;
				}
			}

			int Entry()
			{
				T Left;
				T Right;
				Left.Stored = 3;
				Right.Stored = 1;
				return Left < Right ? 42 : 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdOpCmpLessThanRewrite.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Left < Right must publish Canonical CodeGen, not asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("less-than Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("less-than must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 0,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Left < Right as opCmp < 0; 3<1 is false so Entry()=0, not leftover truthy opCmp; Entry()=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalBitwiseOrExecutesOpOrCallWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 opOr CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdOpOrCallRewrite", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				int opOr(int Value) const
				{
					return Stored | Value;
				}
			}

			int Entry()
			{
				T Object;
				Object.Stored = 8;
				return Object | 2;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdOpOrCallRewrite.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Object | 2 must publish Canonical CodeGen, not asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("bitwise-or Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("bitwise-or must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 10,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Object | 2 as opOr; Entry()=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalSwappedLessThanExecutesOpCmpComparedAgainstZeroWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 swapped opCmp less-than CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdSwappedOpCmpLessThanRewrite", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				int opCmp(int Value) const
				{
					return Stored - Value;
				}
			}

			int Entry()
			{
				T Object;
				Object.Stored = 5;
				return 3 < Object ? 42 : 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdSwappedOpCmpLessThanRewrite.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("3 < Object must publish Canonical CodeGen, not asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("swapped less-than Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("swapped less-than must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute 3 < Object as swapped opCmp > 0; 5>3 so Entry()=42, not leftover Binary or unswapped < 0; Entry()=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalBitwiseNotExecutesOpComCallWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 opCom CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdOpComCallRewrite", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				int opCom() const
				{
					return Stored == 5 ? 42 : 0;
				}
			}

			int Entry()
			{
				T Object;
				Object.Stored = 5;
				return ~Object;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdOpComCallRewrite.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("~Object must publish Canonical CodeGen, not asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("bitwise-not Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("bitwise-not must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute ~Object as opCom; Entry()=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalPrefixPropertyIncrementExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 prefix property CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdPrefixPropertyInc", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int Stored = 5;

				int GetValue()
				{
					return Stored;
				}

				void SetValue(int Next)
				{
					Stored = Next;
				}
			}

			T Make()
			{
				T Object;
				return Object;
			}

			int Entry()
			{
				return ++Make().Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdPrefixPropertyInc.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("++Make().Value must publish Canonical CodeGen, not leftover Unary pre++ or asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("prefix property increment Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("prefix property increment must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 6,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute ++Make().Value as Get then Set yielding 6 from Stored=5; Entry()=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalPostfixPropertyIncrementExecutesOldValueAndWriteThrough)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE, 2),
			TEXT("postfix property CodeGen gate uses the maintained accessor profile")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.4 postfix property CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdPostfixPropertyInc", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int SequenceState = 0;

			class T
			{
				int Stored = 5;

				int GetValue()
				{
					SequenceState = SequenceState == 1 ? 2 : 99;
					return Stored;
				}

				void SetValue(int Next)
				{
					SequenceState = SequenceState == 2 ? 3 : 99;
					Stored = Next;
				}
			}

			T Make()
			{
				SequenceState = SequenceState == 0 ? 1 : 99;
				T Object = T();
				return Object;
			}

			int RunMakePostfix()
			{
				SequenceState = 0;
				int Original = Make().Value++;
				return Original * 100 + SequenceState;
			}

			int RunLocalPostfixAndRead()
			{
				T Object = T();
				SequenceState = 1;
				int Original = Object.Value++;
				return Original * 10000 + Object.Stored * 100 + SequenceState;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdPostfixPropertyInc.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("postfix property mutation must publish Canonical CodeGen; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("postfix property increment Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("postfix property increment must not construct asCCompiler")));
		asQWORD CanonicalDigest = 0;
		ASSERT_THAT(IsTrue(Internal != nullptr
			&& Internal->GetLastCanonicalCodeGenAstDigest(CanonicalDigest)
			&& CanonicalDigest != 0,
			TEXT("postfix property increment must publish a sealed AST digest")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int RunMakePostfix()", Value)
				&& Value == 503,
			*FString::Printf(
				TEXT("Make().Value++ must yield old 5 and observe Make->Get->Set state 3; value=%d"),
				Value)));
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int RunLocalPostfixAndRead()", Value)
				&& Value == 50603,
			*FString::Printf(
				TEXT("local Value++ must yield 5, write Stored=6, and execute Get->Set once; value=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalPrefixIndexIncrementExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 prefix index CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdPrefixIndexInc", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int Stored = 5;

				int &opIndex(int Index)
				{
					return Stored;
				}
			}

			T Make()
			{
				T Object;
				return Object;
			}

			int Entry()
			{
				return ++Make()[0];
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdPrefixIndexInc.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("++Make()[0] must publish Canonical CodeGen, not leftover Unary pre++ or asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("prefix index increment Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("prefix index increment must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 6,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute ++Make()[0] as opIndex then Assign yielding 6 from Stored=5; Entry()=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalPostfixIndexIncrementExecutesOldValueWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 postfix index CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdPostfixIndexInc", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int Stored = 5;

				int &opIndex(int Index)
				{
					return Stored;
				}
			}

			T Make()
			{
				T Object;
				return Object;
			}

			int Entry()
			{
				return Make()[0]++;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdPostfixIndexInc.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Make()[0]++ must publish Canonical CodeGen, not leftover Unary post++ or asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("postfix index increment Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("postfix index increment must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 5,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Make()[0]++ as postfix: yield 5 from Stored=5 then write 6; Entry()=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalRvalueCompoundAssignExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 rvalue opAddAssign CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdRvalueOpAddAssign", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				T& opAddAssign(int Value)
				{
					Stored = Stored + Value;
					return this;
				}
			}

			T Make()
			{
				T Object;
				return Object;
			}

			int Entry()
			{
				Make() += 7;
				return 42;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdRvalueOpAddAssign.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Make() += 7 must publish Canonical CodeGen, not leftover Assign += or asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("rvalue compound assignment Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("rvalue compound assignment must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Make() += 7 as opAddAssign on the temporary and return 42; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalMixinOmittedDefaultExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 mixin omitted-default CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdMixinOmittedDefault", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
			}

			mixin int MixHelper(T self, int a = 40 + 1)
			{
				return a;
			}

			int Entry()
			{
				T Object;
				return Object.MixHelper();
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdMixinOmittedDefault.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Object.MixHelper() must publish Canonical CodeGen, not asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("mixin omitted-default Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("mixin omitted-default must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 41,
			*FString::Printf(
				TEXT("Canonical CodeGen must fill MixHelper's omitted default 40+1; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalMemberNamedDefaultExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 member named+default CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdMemberNamedDefault", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int Pack(int A = 1, int B = 2)
				{
					return A * 10 + B;
				}
			}

			int Entry()
			{
				T Object;
				return Object.Pack(B: 7);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdMemberNamedDefault.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Object.Pack(B: 7) must publish Canonical CodeGen, not asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("member named+default Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("member named+default must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 17,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Object.Pack(B: 7) as A=1 B=7; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalUnaryNotOpImplConvExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 unary-not opImplConv CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdUnaryNotOpImplConv", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				bool opImplConv() const
				{
					return Stored != 0;
				}
			}

			int Entry()
			{
				T Object;
				return !Object ? 42 : 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdUnaryNotOpImplConv.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("!Object must publish Canonical CodeGen, not leftover Unary of T or asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("unary-not Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("unary-not must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute !Object as !opImplConv of Stored=0; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalConditionalOpImplConvExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 conditional opImplConv CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdConditionalOpImplConv", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				bool opImplConv() const
				{
					return Stored != 0;
				}
			}

			int Entry()
			{
				T Object;
				return Object ? 42 : 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdConditionalOpImplConv.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Object ? 42 : 0 must publish Canonical CodeGen, not leftover cond of T or asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("conditional opImplConv Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("conditional opImplConv must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 0,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Object as opImplConv of Stored=0, not leftover object-as-true; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalIfWhileOpImplConvExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 if/while opImplConv CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdIfWhileOpImplConv", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				bool opImplConv() const
				{
					return Stored != 0;
				}
			}

			int Entry()
			{
				T Object;
				int Value = 0;
				if (Object)
				{
					Value = 1;
				}
				while (Object)
				{
					Value = 2;
					break;
				}
				return Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdIfWhileOpImplConv.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("if (Object) / while (Object) must publish Canonical CodeGen, not leftover cond of T or asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("if/while opImplConv Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("if/while opImplConv must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 0,
			*FString::Printf(
				TEXT("Canonical CodeGen must skip if/while when opImplConv of Stored=0 is false, not leftover object-as-true; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalForCondOpImplConvExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 for-cond opImplConv CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdForCondOpImplConv", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				bool opImplConv() const
				{
					return Stored != 0;
				}
			}

			int Entry()
			{
				T Object;
				int Value = 0;
				for (; Object; )
				{
					Value = 1;
					break;
				}
				return Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdForCondOpImplConv.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("for (; Object; ) must publish Canonical CodeGen, not leftover cond of T or asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("for-cond opImplConv Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("for-cond opImplConv must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 0,
			*FString::Printf(
				TEXT("Canonical CodeGen must skip for when opImplConv of Stored=0 is false, not leftover object-as-true; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalUnaryNotOfBoolExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 unary-not bool CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdUnaryNotBool", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				bool Flag = false;
				return !Flag ? 42 : 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdUnaryNotBool.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("!Flag must publish Canonical CodeGen, not leftover Unary ! typed int or asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("unary-not bool Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("unary-not bool must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute !false as true and return 42; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalWorldContextHiddenCallExecutesWithoutLegacyCompiler)
	{
		GProdWorldContextInjectCount = 0;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 WorldContext execute fixture selects CANONICAL")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"WorldContextHost",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("WorldContext host type must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"WorldContextHost __WorldContext()",
				asFUNCTION(ProdInjectWorldContextGeneric),
				asCALL_GENERIC) >= 0,
			TEXT("__WorldContext injector must register")));
		const int WithWorldId = ScriptEngine->RegisterGlobalFunction(
			"int WithWorld(WorldContextHost WorldContext, int Visible)",
			asFUNCTION(ProdWithWorldGeneric),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(WithWorldId >= 0, TEXT("WithWorld must register")));
		asCScriptFunction* const WithWorldFn =
			static_cast<asCScriptFunction*>(ScriptEngine->GetFunctionById(WithWorldId));
		ASSERT_THAT(IsNotNull(WithWorldFn, TEXT("WithWorld script function")));
		if (WithWorldFn != nullptr)
		{
			WithWorldFn->hiddenArgumentIndex = 0;
			WithWorldFn->hiddenArgumentDefault = "__WorldContext()";
		}

		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdWorldContextHidden", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return WithWorld(3);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdWorldContextHidden.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("WithWorld(3) must publish Canonical CodeGen, not leftover hidden WorldContext or asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("WorldContext hidden Call Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("WorldContext hidden Call must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED
				&& Value == 43
				&& GProdWorldContextInjectCount == 1,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute injected __WorldContext() then WithWorld(3) as 43; status=%d Entry()=%d injects=%d exception=%s"),
				ExecuteStatus,
				Value,
				GProdWorldContextInjectCount,
				*Exception)));
	}

	TEST_METHOD(CanonicalLogicalAndOpImplConvExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 logical-and opImplConv CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdLogicalAndOpImplConv", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int Stored = 0;

				bool opImplConv() const
				{
					return Stored != 0;
				}
			}

			int Entry()
			{
				T Object;
				return Object && true ? 42 : 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdLogicalAndOpImplConv.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Object && true must publish Canonical CodeGen, not leftover Logical of T or asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("logical-and opImplConv Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("logical-and opImplConv must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 0,
			*FString::Printf(
				TEXT("Canonical CodeGen must skip Object && true when opImplConv of Stored=0 is false, not leftover object-as-true; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalConstGlobalAndClassFactoryExecuteWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 factory/global-init CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdCanonicalNoLegacyCompilerSites", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			const int G = 40 + 2;

			class T
			{
				int Stored = 0;
			}

			int Entry()
			{
				return G;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdCanonicalNoLegacyCompilerSites.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("const G plus class T must publish Canonical CodeGen without asCCompiler; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("factory/global-init Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("factory/global-init must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute sealed const G=42; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalObjectOpCallExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 object opCall CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdObjectOpCall", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int opCall(int n)
				{
					return n + 1;
				}
			}

			int Entry()
			{
				T Object;
				return Object(41);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdObjectOpCall.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Object(41) must publish Canonical CodeGen; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("object opCall Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("object opCall must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Object(41) as T::opCall; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalPostfixMakeOpCallExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 postfix Make()(41) CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdPostfixMakeOpCall", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int opCall(int n)
				{
					return n + 1;
				}
			}

			T Make()
			{
				T Object;
				return Object;
			}

			int Entry()
			{
				return Make()(41);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdPostfixMakeOpCall.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Make()(41) must publish Canonical CodeGen; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("postfix Make() opCall Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("postfix Make() opCall must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Make()(41) as T::opCall; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalCastIntOpConvExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 explicit opConv CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdObjectOpConv", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int opConv() const
				{
					return 40 + 2;
				}
			}

			int Entry()
			{
				T Object;
				return Cast<int>(Object);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdObjectOpConv.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Cast<int>(Object) must publish Canonical CodeGen; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("explicit opConv Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("explicit opConv must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Cast<int>(Object) as T::opConv; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalReturnObjectOpImplConvExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 implicit opImplConv CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdReturnOpImplConv", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int opImplConv() const
				{
					return 40 + 2;
				}
			}

			int Entry()
			{
				T Object;
				return Object;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdReturnOpImplConv.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("return Object must publish Canonical CodeGen; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("implicit opImplConv Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("implicit opImplConv must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute return Object as T::opImplConv; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalLocalInitObjectOpImplConvExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 implicit opImplConv local-init CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdLocalInitOpImplConv", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int opImplConv() const
				{
					return 40 + 2;
				}
			}

			int Entry()
			{
				T Object;
				int Value = Object;
				return Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdLocalInitOpImplConv.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("int Value = Object must publish Canonical CodeGen; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("implicit opImplConv local-init Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("implicit opImplConv local-init must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute int Value = Object as T::opImplConv; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalCallArgumentObjectOpImplConvExecutesWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("5.3 implicit opImplConv call-argument CodeGen fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdCallArgumentOpImplConv", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int opImplConv() const
				{
					return 40 + 2;
				}
			}

			int Consume(int Value)
			{
				return Value;
			}

			int Entry()
			{
				T Object;
				return Consume(Object);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdCallArgumentOpImplConv.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const int BuildResult = Module->Build();
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("Consume(Object) must publish Canonical CodeGen; result=%d publisher=%d legacy=%u messages=%s"),
				BuildResult,
				Internal != nullptr ? static_cast<int>(Internal->GetLastBytecodePublisher()) : -1,
				Internal != nullptr ? Internal->GetLastLegacyCompilerInvocationCount() : 0u,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("implicit opImplConv call-argument Bytecode must be CodeGen, not asCCompiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("implicit opImplConv call argument must not construct asCCompiler")));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int Entry()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 42,
			*FString::Printf(
				TEXT("Canonical CodeGen must execute Consume(Object) through T::opImplConv; status=%d Entry()=%d exception=%s"),
				ExecuteStatus,
				Value,
				*Exception)));
	}

	TEST_METHOD(CanonicalBytecodeLifetimeEmitterConsumesOnlyProtocolExitPlans)
	{
		const FString SourcePath = FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_bytecode_codegen.cpp"));
		FString Source;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(Source, *SourcePath),
			TEXT("read the maintained Canonical Bytecode emitter source")));
		const int32 EmitterBegin = Source.Find(
			TEXT("class asCCanonicalFunctionEmitter"));
		const int32 EmitterEnd = Source.Find(
			TEXT("// Global initialization"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			EmitterBegin);
		ASSERT_THAT(IsTrue(EmitterBegin != INDEX_NONE && EmitterEnd > EmitterBegin,
			TEXT("locate the complete Canonical function-emitter source slice")));
		const FString Emitter = Source.Mid(
			EmitterBegin, EmitterEnd - EmitterBegin);

		ASSERT_THAT(IsTrue(Emitter.Contains(TEXT("lifetimeView.FindExitPlan(")),
			TEXT("Bytecode cleanup routing must start from authenticated exit plans")));
		ASSERT_THAT(IsTrue(Emitter.Contains(TEXT("lifetimeView.GetRecord(")),
			TEXT("Bytecode actions must consume exact authenticated lifetime records")));
		ASSERT_THAT(IsFalse(Emitter.Contains(TEXT("FindCleanupBinding(")),
			TEXT("compatibility cleanup statement identity must not route Bytecode")));
		ASSERT_THAT(IsFalse(Emitter.Contains(TEXT("GetCleanupBinding(")),
			TEXT("compatibility cleanup tables are verifier-only evidence")));
		ASSERT_THAT(IsFalse(Emitter.Contains(TEXT("EmitSealedTransferCleanups")),
			TEXT("transfer cleanup must not traverse compatibility statement children")));
		ASSERT_THAT(IsFalse(Emitter.Contains(TEXT("asCASTDump(")),
			TEXT("diagnostic dump text must never select a Bytecode action")));
		ASSERT_THAT(IsFalse(Emitter.Contains(TEXT("\"scope-exit\""))));
		ASSERT_THAT(IsFalse(Emitter.Contains(TEXT("\"scope-release\""))));
		ASSERT_THAT(IsFalse(Emitter.Contains(TEXT("destructor->name")),
			TEXT("destructor spelling must never select a Bytecode action")));
	}

	TEST_METHOD(CanonicalLexicalValueObjectsExecuteSealedCleanupPlansExactlyOnce)
	{
		GProdNativeLifecycleMemberConstructed = 0;
		GProdNativeLifecycleMemberDestructed = 0;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdNativeLifecycleMember",
				sizeof(FProdNativeLifecycleMember),
				asOBJ_VALUE | asOBJ_APP_CLASS_CD | asOBJ_APP_CLASS_ALLINTS) >= 0,
			TEXT("lexical cleanup gate requires a non-POD native value type")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdNativeLifecycleMember",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdNativeLifecycleMemberConstruct),
				asCALL_GENERIC) >= 0,
			TEXT("lexical cleanup gate requires the exact native constructor")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdNativeLifecycleMember",
				asBEHAVE_DESTRUCT,
				"void f()",
				asFUNCTION(ProdNativeLifecycleMemberDestruct),
				asCALL_GENERIC) >= 0,
			TEXT("lexical cleanup gate requires the exact native destructor")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("lexical cleanup fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int RunLexicalCleanup(const bool Early)
			{
				FProdNativeLifecycleMember Outer;
				{
					FProdNativeLifecycleMember First;
					FProdNativeLifecycleMember Second;
					if (Early)
						return 42;
				}
				return 42;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdLexicalValueCleanup",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL lexical value-object cleanup Build must succeed")));
		asIScriptFunction* const Entry = Module != nullptr
			? Module->GetFunctionByDecl("int RunLexicalCleanup(const bool)") : nullptr;
		ASSERT_THAT(IsNotNull(Entry));
		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ON_SCOPE_EXIT
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
		};
		if (Entry == nullptr || Context == nullptr)
		{
			return;
		}

		auto ExecuteAndRequireExactCleanup = [&](const bool bEarly)
		{
			GProdNativeLifecycleMemberConstructed = 0;
			GProdNativeLifecycleMemberDestructed = 0;
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Entry)));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgByte(0, bEarly ? 1 : 0)));
			const int ExecuteStatus = Context->Execute();
			ASSERT_THAT(IsTrue(
				ExecuteStatus == asEXECUTION_FINISHED
					&& Context->GetReturnDWord() == 42,
				TEXT("both lexical exits must execute the published Canonical bytecode")));
			ASSERT_THAT(AreEqual(3, GProdNativeLifecycleMemberConstructed,
				TEXT("Outer, First, and Second must each construct once")));
			ASSERT_THAT(AreEqual(3, GProdNativeLifecycleMemberDestructed,
				TEXT("the sealed cleanup plans must destroy every live local exactly once")));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare()));
		};
		ExecuteAndRequireExactCleanup(true);
		ExecuteAndRequireExactCleanup(false);

		const asCModule* const Internal = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal->GetLastBytecodePublisher(),
			TEXT("lexical cleanup execution must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			0,
			Internal->GetLastLegacyCompilerInvocationCount(),
			TEXT("lexical cleanup execution must not invoke asCCompiler")));
	}

	TEST_METHOD(CanonicalLocalValueLifetimeIsSuccessSensitiveOnException)
	{
		GProdSuccessSensitiveInitializerAttempts = 0;
		GProdSuccessSensitiveDestructionOrder.Reset();
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"void LifetimeInitializer(bool)",
				asFUNCTION(ProdSuccessSensitiveLifetimeInitializer),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterGlobalFunction(
				"void RecordLifetimeDestructor(int)",
				asFUNCTION(ProdRecordSuccessSensitiveDestructor),
				asCALL_GENERIC) >= 0,
			TEXT("success-sensitive production fixture registers its native surface")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("success-sensitive production fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FSuccessSensitiveValue
			{
				int Id;
				FSuccessSensitiveValue(int InId, bool Fail)
				{
					Id = InId;
					LifetimeInitializer(Fail);
				}
				~FSuccessSensitiveValue()
				{
					RecordLifetimeDestructor(Id);
				}
			}

			int Run(const bool FailSecond)
			{
				FSuccessSensitiveValue First(1, false);
				FSuccessSensitiveValue Second(2, FailSecond);
				return 42;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdSuccessSensitiveLocalLifetime",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL success-sensitive local lifetime Build must succeed")));
		asIScriptFunction* const Entry = Module != nullptr
			? Module->GetFunctionByDecl("int Run(const bool)") : nullptr;
		ASSERT_THAT(IsNotNull(Entry));
		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ON_SCOPE_EXIT
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
		};
		if (Entry == nullptr || Context == nullptr)
		{
			return;
		}

		auto Execute = [&](const bool bFailSecond)
		{
			GProdSuccessSensitiveInitializerAttempts = 0;
			GProdSuccessSensitiveDestructionOrder.Reset();
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Entry)));
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->SetArgByte(0, bFailSecond ? 1 : 0)));
			const int ExecuteStatus = Context->Execute();
			if (bFailSecond)
			{
				ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, ExecuteStatus,
					TEXT("the second initializer must terminate Run with a VM exception")));
				ASSERT_THAT(IsTrue(FString(UTF8_TO_TCHAR(
					Context->GetExceptionString())).Contains(
					TEXT("success-sensitive lifetime initializer")),
					TEXT("the native initializer exception must stay observable")));
			}
			else
			{
				ASSERT_THAT(IsTrue(ExecuteStatus == asEXECUTION_FINISHED
					&& Context->GetReturnDWord() == 42,
					TEXT("the normal edge must finish through Canonical bytecode")));
			}
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
				TEXT("the context must finish all normal/exception cleanup before inspection")));
			ASSERT_THAT(AreEqual(2, GProdSuccessSensitiveInitializerAttempts,
				TEXT("both initializer bodies must be attempted")));
			if (bFailSecond)
			{
				const FString ActualOrder = FString::JoinBy(
					GProdSuccessSensitiveDestructionOrder,
					TEXT(","),
					[](const int32 Id) { return FString::FromInt(Id); });
				ASSERT_THAT(IsTrue(
					GProdSuccessSensitiveDestructionOrder.Num() == 1
						&& GProdSuccessSensitiveDestructionOrder[0] == 1,
					*FString::Printf(
						TEXT("the abort edge destroys First only; failed Second is never live; actual=[%s]"),
						*ActualOrder)));
			}
			else
			{
				const FString ActualOrder = FString::JoinBy(
					GProdSuccessSensitiveDestructionOrder,
					TEXT(","),
					[](const int32 Id) { return FString::FromInt(Id); });
				ASSERT_THAT(IsTrue(
					GProdSuccessSensitiveDestructionOrder.Num() == 2
						&& GProdSuccessSensitiveDestructionOrder[0] == 2
						&& GProdSuccessSensitiveDestructionOrder[1] == 1,
					*FString::Printf(
						TEXT("the normal edge destroys committed locals in strict reverse order; actual=[%s] calls={%s} objects={%s}"),
						*ActualOrder,
						*ProdBytecodeCallTargets(Entry),
						*ProdRuntimeObjectDirectory(Entry))));
			}
		};
		Execute(false);
		Execute(true);

		const asCModule* const Internal = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal->GetLastBytecodePublisher(),
			TEXT("success-sensitive lifetime execution must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			0,
			Internal->GetLastLegacyCompilerInvocationCount(),
			TEXT("success-sensitive lifetime execution must not invoke asCCompiler")));
	}

	TEST_METHOD(CanonicalConstructorFailureDestroysOnlyCommittedPrefix)
	{
		GProdConstructorCommittedAttempts = 0;
		GProdConstructorCommittedFailAt = 0;
		GProdConstructorBodyShouldFail = false;
		GProdCompleteObjectDestructorCalls = 0;
		GProdConstructorCommittedDestructionOrder.Reset();

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdConstructorCommittedPart",
				sizeof(FProdConstructorCommittedPart),
				asOBJ_VALUE | asOBJ_APP_CLASS_CD
					| asOBJ_APP_CLASS_ALLINTS) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"FProdConstructorCommittedPart",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdConstructorCommittedPartConstruct),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"FProdConstructorCommittedPart",
				asBEHAVE_DESTRUCT,
				"void f()",
				asFUNCTION(ProdConstructorCommittedPartDestruct),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterGlobalFunction(
				"void MaybeFailConstructorBody()",
				asFUNCTION(ProdMaybeFailConstructorBody),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterGlobalFunction(
				"void RecordCompleteObjectDestructor()",
				asFUNCTION(ProdRecordCompleteObjectDestructor),
				asCALL_GENERIC) >= 0,
			TEXT("constructor committed-prefix fixture registers its exact native lifecycle surface")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("constructor committed-prefix fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class FCommittedRuntimeBase
			{
				FProdConstructorCommittedPart BasePart;
				void GetBasePart() {}
				void SetBasePart() {}
			}

			class FCommittedRuntimeDerived : FCommittedRuntimeBase
			{
				FProdConstructorCommittedPart First;
				FProdConstructorCommittedPart Second;
				void GetFirst() {}
				void SetFirst() {}
				void GetSecond() {}
				void SetSecond() {}

				FCommittedRuntimeDerived()
				{
					MaybeFailConstructorBody();
				}

				~FCommittedRuntimeDerived()
				{
					RecordCompleteObjectDestructor();
				}
			}

			int RunCommittedRuntimeConstructor()
			{
				FCommittedRuntimeDerived Value;
				return 42;
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(),
				"ProdConstructorCommittedPrefix",
				ScriptSource.c_str(),
				Module),
			TEXT("CANONICAL constructor committed-prefix source must compile")));

		asITypeInfo* const DerivedType = Module != nullptr
			? Module->GetTypeInfoByDecl("FCommittedRuntimeDerived") : nullptr;
		asCObjectType* const InternalDerivedType =
			static_cast<asCObjectType*>(DerivedType);
		asCScriptFunction* const Constructor =
			InternalDerivedType != nullptr
			&& InternalDerivedType->beh.construct > 0
			&& static_cast<asUINT>(InternalDerivedType->beh.construct)
				< ScriptEngine->scriptFunctions.GetLength()
			? ScriptEngine->scriptFunctions[InternalDerivedType->beh.construct]
			: nullptr;
		asCScriptFunction* Factory = nullptr;
		if (InternalDerivedType != nullptr)
		{
			for (asUINT Index = 0;
				Index < InternalDerivedType->beh.factories.GetLength(); ++Index)
			{
				const int FactoryId = InternalDerivedType->beh.factories[Index];
				asCScriptFunction* const Candidate = FactoryId > 0
					&& static_cast<asUINT>(FactoryId)
						< ScriptEngine->scriptFunctions.GetLength()
					? ScriptEngine->scriptFunctions[FactoryId]
					: nullptr;
				if (Candidate != nullptr
					&& Candidate->artifactInvocationKind
						== asBUILD_ARTIFACT_INVOCATION_FACTORY
					&& Candidate->artifactOwnerType == InternalDerivedType)
				{
					Factory = Candidate;
					break;
				}
			}
		}
		asIScriptFunction* const Entry = Module != nullptr
			? Module->GetFunctionByDecl(
				"int RunCommittedRuntimeConstructor()") : nullptr;
		ASSERT_THAT(IsNotNull(Constructor,
			TEXT("the derived Runtime type must publish its exact Canonical constructor")));
		ASSERT_THAT(IsNotNull(Factory,
			TEXT("the derived Runtime type must publish its exact Canonical factory")));
		ASSERT_THAT(IsNotNull(Entry));
		if (Factory != nullptr && Factory->scriptData != nullptr)
		{
			const int32 PartialObjectOffset =
				Factory->scriptData->partialConstructionObjectOffset;
			ASSERT_THAT(IsTrue(
				PartialObjectOffset > 0
					&& Factory->scriptData->partialConstructionObjectType
						== InternalDerivedType,
				TEXT("the Canonical factory must publish its raw partial-construction allocation")));
			ASSERT_THAT(IsTrue(
				Factory->scriptData->pointerVariablePos.Exists(PartialObjectOffset),
				TEXT("the raw partial-construction slot must remain non-owning but VM/JIT-addressable")));

			const int32 SavedPartialObjectOffset =
				Factory->scriptData->partialConstructionObjectOffset;
			auto RequireForgedFactoryOffsetRejected =
				[&](int32 ForgedOffset, const TCHAR* FailureMessage)
			{
				// A detached artifact may reconstruct its non-owning pointer
				// directory only from the exact authenticated factory slot. Keep
				// executable bytecode/variableSpace unchanged while forging the
				// semantic tail, then require reader-side fail-closed validation.
				Factory->scriptData->partialConstructionObjectOffset = ForgedOffset;
				AngelscriptNativeTestSupport::FMemoryBinaryStream ForgedArtifact;
				asCWriter ForgedWriter(
					static_cast<asCModule*>(Module),
					&ForgedArtifact,
					ScriptEngine,
					true);
				const int ForgedWriteResult =
					ForgedWriter.WriteFunctionArtifact(Factory);
				Factory->scriptData->partialConstructionObjectOffset =
					SavedPartialObjectOffset;
				ASSERT_THAT(AreEqual(asSUCCESS, ForgedWriteResult,
					TEXT("the forged artifact fixture must reach reader-side frame validation")));
				ASSERT_THAT(IsTrue(ForgedArtifact.Num() > 0));

				ForgedArtifact.ResetReadPosition();
				asCReader ForgedReader(
					static_cast<asCModule*>(Module),
					&ForgedArtifact,
					ScriptEngine);
				asCScriptFunction* ForgedRestoredFactory = nullptr;
				asSFunctionArtifactValidationDiagnostics ForgedDiagnostics{};
				const int ForgedRestoreResult =
					ForgedReader.RestoreFunctionArtifactDetached(
						static_cast<asUINT>(ForgedArtifact.Num()),
						&ForgedRestoredFactory,
						&ForgedDiagnostics);
				const bool bRejected = ForgedRestoreResult < 0
					&& ForgedRestoredFactory == nullptr;
				if (ForgedRestoredFactory != nullptr)
				{
					ForgedRestoredFactory->DestroyHalfCreated();
				}
				ASSERT_THAT(IsTrue(bRejected, FailureMessage));
			};
			RequireForgedFactoryOffsetRejected(
				SavedPartialObjectOffset > 1
					? SavedPartialObjectOffset - 1
					: SavedPartialObjectOffset + 1,
				TEXT("detached restore must reject an in-frame partial-construction coordinate that does not match the exact factory bytecode slot"));
			RequireForgedFactoryOffsetRejected(
				static_cast<int32>(Factory->scriptData->variableSpace)
					+ AS_PTR_SIZE + 1,
				TEXT("detached restore must reject a partial-construction coordinate beyond variableSpace"));
		}
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(
				Constructor, asBC_FinConstruct),
			TEXT("the complete-object protocol must lower to FinConstruct only after the constructor body succeeds")));
		ASSERT_THAT(IsNotNull(Constructor->scriptData));
		if (Constructor->scriptData != nullptr)
		{
			const auto& Cleanup = Constructor->scriptData->constructorCleanupInfo;
			const int32 CommittedOffset =
				Constructor->scriptData->constructorCommittedCountOffset;
			ASSERT_THAT(IsTrue(CommittedOffset > 0,
				TEXT("Canonical constructors require a private committed-count frame slot")));
			bool bPublishedCommittedCountTemporary = false;
			for (asUINT Index = 0;
				Index < Constructor->scriptData->temporaryVariables.GetLength(); ++Index)
			{
				const auto& Temporary =
					Constructor->scriptData->temporaryVariables[Index];
				if (Temporary.Offset == CommittedOffset
					&& Temporary.Token == ttUInt)
				{
					bPublishedCommittedCountTemporary = true;
					break;
				}
			}
			ASSERT_THAT(IsTrue(
				bPublishedCommittedCountTemporary,
				TEXT("the committed-count slot must be published as ttUInt in the Runtime/JIT temporary-variable directory")));
			ASSERT_THAT(AreEqual(4, static_cast<int32>(Cleanup.GetLength()),
				TEXT("the physical plan must contain base, First, Second and complete-object records")));
			if (Cleanup.GetLength() == 4)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asAST_LIFETIME_PHASE_CONSTRUCTOR_BASE),
					static_cast<int32>(Cleanup[0].phase)));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER),
					static_cast<int32>(Cleanup[1].phase)));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER),
					static_cast<int32>(Cleanup[2].phase)));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asAST_LIFETIME_PHASE_COMPLETE_OBJECT),
					static_cast<int32>(Cleanup[3].phase)));
				ASSERT_THAT(IsFalse(Cleanup[0].completeObject));
				ASSERT_THAT(IsFalse(Cleanup[1].completeObject));
				ASSERT_THAT(IsFalse(Cleanup[2].completeObject));
				ASSERT_THAT(IsTrue(Cleanup[3].completeObject));
				ASSERT_THAT(IsTrue(Cleanup[0].objectType
					== InternalDerivedType->derivedFrom));
				ASSERT_THAT(IsTrue(Cleanup[1].objectOffset > 0));
				ASSERT_THAT(IsTrue(Cleanup[2].objectOffset
					> Cleanup[1].objectOffset));
				ASSERT_THAT(IsTrue(Cleanup[1].actionFunctionId > 0));
				ASSERT_THAT(AreEqual(
					Cleanup[1].actionFunctionId,
					Cleanup[2].actionFunctionId,
					TEXT("same native member type must resolve the same exact Runtime destructor")));
			}
			for (int32 Commit = 0; Commit <= 4; ++Commit)
			{
				ASSERT_THAT(IsTrue(ProdContainsSetV4(
					Constructor, CommittedOffset, Commit),
					*FString::Printf(
						TEXT("constructor bytecode must retain committed-count write %d; %s"),
						Commit, *ProdBytecodeImmediates(Constructor))));
			}

			AngelscriptNativeTestSupport::FMemoryBinaryStream ArtifactStream;
			asCWriter Writer(
				static_cast<asCModule*>(Module),
				&ArtifactStream,
				ScriptEngine,
				true);
			ASSERT_THAT(AreEqual(asSUCCESS,
				Writer.WriteFunctionArtifact(Constructor),
				TEXT("constructor cleanup protocol must be representable in the detached VM artifact")));
			ASSERT_THAT(IsTrue(ArtifactStream.Num() > 0));
			ArtifactStream.ResetReadPosition();
			asCReader Reader(
				static_cast<asCModule*>(Module),
				&ArtifactStream,
				ScriptEngine);
			asCScriptFunction* RestoredConstructor = nullptr;
			asSFunctionArtifactValidationDiagnostics RestoreDiagnostics{};
			const int RestoreResult = Reader.RestoreFunctionArtifactDetached(
				static_cast<asUINT>(ArtifactStream.Num()),
				&RestoredConstructor,
				&RestoreDiagnostics);
			ASSERT_THAT(AreEqual(asSUCCESS,
				RestoreResult,
				*FString::Printf(
					TEXT("constructor cleanup protocol must restore through semantic type/function tables; result=%d stage=%u bytes=%u/%d error=%d new=%d"),
					RestoreResult,
					RestoreDiagnostics.stage,
					RestoreDiagnostics.bytesRead,
					ArtifactStream.Num(),
					RestoreDiagnostics.hadError ? 1 : 0,
					RestoreDiagnostics.wasNewFunction ? 1 : 0)));
			ON_SCOPE_EXIT
			{
				if (RestoredConstructor != nullptr)
				{
					RestoredConstructor->DestroyHalfCreated();
				}
			};
			ASSERT_THAT(IsNotNull(RestoredConstructor));
			if (RestoredConstructor != nullptr
				&& RestoredConstructor->scriptData != nullptr)
			{
				const auto& RestoredCleanup =
					RestoredConstructor->scriptData->constructorCleanupInfo;
				bool bRestoredCommittedCountTemporary = false;
				for (asUINT Index = 0;
					Index < RestoredConstructor->scriptData->temporaryVariables.GetLength();
					++Index)
				{
					const auto& Temporary =
						RestoredConstructor->scriptData->temporaryVariables[Index];
					if (Temporary.Offset == CommittedOffset
						&& Temporary.Token == ttUInt)
					{
						bRestoredCommittedCountTemporary = true;
						break;
					}
				}
				ASSERT_THAT(IsTrue(
					bRestoredCommittedCountTemporary,
					TEXT("detached function restore must retain the committed-count Runtime/JIT temporary entry")));
				ASSERT_THAT(AreEqual(CommittedOffset,
					RestoredConstructor->scriptData->constructorCommittedCountOffset,
					TEXT("artifact restore must preserve the private committed-count frame slot")));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(Cleanup.GetLength()),
					static_cast<int32>(RestoredCleanup.GetLength()),
					TEXT("artifact restore must preserve the complete constructor cleanup directory")));
				for (asUINT Index = 0;
					Index < Cleanup.GetLength()
						&& Index < RestoredCleanup.GetLength(); ++Index)
				{
					ASSERT_THAT(IsTrue(
						RestoredCleanup[Index].phase == Cleanup[Index].phase
							&& RestoredCleanup[Index].action == Cleanup[Index].action
							&& RestoredCleanup[Index].completeObject
								== Cleanup[Index].completeObject
							&& RestoredCleanup[Index].objectType
								== Cleanup[Index].objectType
							&& RestoredCleanup[Index].objectOffset
								== Cleanup[Index].objectOffset
							&& RestoredCleanup[Index].actionFunctionId
								== Cleanup[Index].actionFunctionId,
						TEXT("artifact restore must resolve every cleanup step to the exact current-generation authority")));
				}
			}
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ON_SCOPE_EXIT
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
		};
		if (Entry == nullptr || Context == nullptr)
		{
			return;
		}

		auto ExecuteAndRequirePrefix = [&](int32 FailAt, bool bFailBody,
			int ExpectedStatus, int32 ExpectedAttempts,
			int32 ExpectedCompleteDestructorCalls,
			const TArray<int32>& ExpectedDestructionOrder)
		{
			GProdConstructorCommittedAttempts = 0;
			GProdConstructorCommittedFailAt = FailAt;
			GProdConstructorBodyShouldFail = bFailBody;
			GProdCompleteObjectDestructorCalls = 0;
			GProdConstructorCommittedDestructionOrder.Reset();
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Entry)));
			const int ExecuteStatus = Context->Execute();
			ASSERT_THAT(AreEqual(ExpectedStatus, ExecuteStatus));
			if (ExpectedStatus == asEXECUTION_FINISHED)
			{
				ASSERT_THAT(AreEqual(static_cast<asDWORD>(42),
					Context->GetReturnDWord()));
			}
			else
			{
				const FString Exception = FString(UTF8_TO_TCHAR(
					Context->GetExceptionString()));
				ASSERT_THAT(IsTrue(Exception.Contains(
					bFailBody ? TEXT("body failure") : TEXT("member failure")),
					TEXT("the original constructor exception must remain observable")));
			}
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
				TEXT("constructor failure must finish VM unwind before trace inspection")));
			ASSERT_THAT(AreEqual(ExpectedAttempts,
				GProdConstructorCommittedAttempts));
			ASSERT_THAT(AreEqual(ExpectedCompleteDestructorCalls,
				GProdCompleteObjectDestructorCalls,
				TEXT("the complete-object destructor is unreachable before its distinct commit")));
			FString ActualDestructionOrder;
			for (const int32 Value : GProdConstructorCommittedDestructionOrder)
			{
				if (!ActualDestructionOrder.IsEmpty())
				{
					ActualDestructionOrder += TEXT(",");
				}
				ActualDestructionOrder += FString::FromInt(Value);
			}
			const FString DestructionCountContext = FString::Printf(
				TEXT("FailAt=%d FailBody=%d actual destruction order=[%s] bytecode=%s"),
				FailAt, bFailBody ? 1 : 0, *ActualDestructionOrder,
				*ProdBytecodeImmediates(Constructor));
			ASSERT_THAT(AreEqual(ExpectedDestructionOrder.Num(),
				GProdConstructorCommittedDestructionOrder.Num(),
				*DestructionCountContext));
			for (int32 Index = 0;
				Index < ExpectedDestructionOrder.Num()
					&& Index < GProdConstructorCommittedDestructionOrder.Num();
				++Index)
			{
				ASSERT_THAT(AreEqual(
					ExpectedDestructionOrder[Index],
					GProdConstructorCommittedDestructionOrder[Index],
					TEXT("constructor abort cleanup must be the exact reverse committed prefix")));
			}
		};

		ExecuteAndRequirePrefix(
			0, false, asEXECUTION_FINISHED, 3, 1,
			TArray<int32>{3, 2, 1});
		ExecuteAndRequirePrefix(
			1, false, asEXECUTION_EXCEPTION, 1, 0,
			TArray<int32>{});
		ExecuteAndRequirePrefix(
			2, false, asEXECUTION_EXCEPTION, 2, 0,
			TArray<int32>{1});
		ExecuteAndRequirePrefix(
			3, false, asEXECUTION_EXCEPTION, 3, 0,
			TArray<int32>{2, 1});
		ExecuteAndRequirePrefix(
			0, true, asEXECUTION_EXCEPTION, 3, 0,
			TArray<int32>{3, 2, 1});

		const asCModule* const Internal = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(
			0,
			Internal->GetLastLegacyCompilerInvocationCount(),
			TEXT("constructor committed-prefix execution must never invoke asCCompiler")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream ModuleBytecode;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Module->SaveByteCode(&ModuleBytecode, false),
			TEXT("the complete module stream must persist constructor cleanup authority")));
		ASSERT_THAT(IsTrue(ModuleBytecode.Num() > 0));
		ModuleBytecode.ResetReadPosition();
		asIScriptModule* const RestoredModule = ScriptEngine->GetModule(
			"ProdConstructorCommittedPrefixRestored",
			asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(RestoredModule));
		bool bDebugWasStripped = true;
		const int LoadResult = RestoredModule != nullptr
			? RestoredModule->LoadByteCode(&ModuleBytecode, &bDebugWasStripped)
			: asNO_MODULE;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			LoadResult,
			TEXT("the complete module stream must restore constructor cleanup through stable type/function/property identities")));
		ASSERT_THAT(IsFalse(bDebugWasStripped));
		ASSERT_THAT(IsFalse(ModuleBytecode.HasReadError()));
		if (LoadResult != asSUCCESS || RestoredModule == nullptr)
		{
			return;
		}

		asCObjectType* const RestoredDerivedType = static_cast<asCObjectType*>(
			RestoredModule->GetTypeInfoByDecl("FCommittedRuntimeDerived"));
		asCScriptFunction* const RestoredModuleConstructor =
			RestoredDerivedType != nullptr
			&& RestoredDerivedType->beh.construct > 0
			&& static_cast<asUINT>(RestoredDerivedType->beh.construct)
				< ScriptEngine->scriptFunctions.GetLength()
			? ScriptEngine->scriptFunctions[RestoredDerivedType->beh.construct]
			: nullptr;
		asIScriptFunction* const RestoredEntry = RestoredModule->GetFunctionByDecl(
			"int RunCommittedRuntimeConstructor()");
		ASSERT_THAT(IsNotNull(RestoredModuleConstructor));
		ASSERT_THAT(IsNotNull(RestoredEntry));
		ASSERT_THAT(IsNotNull(
			RestoredModuleConstructor != nullptr
				? RestoredModuleConstructor->scriptData
				: nullptr));
		if (RestoredModuleConstructor != nullptr
			&& RestoredModuleConstructor->scriptData != nullptr)
		{
			const auto& RestoredCleanup =
				RestoredModuleConstructor->scriptData->constructorCleanupInfo;
			bool bRestoredModuleCommittedCountTemporary = false;
			for (asUINT Index = 0;
				Index < RestoredModuleConstructor->scriptData->temporaryVariables.GetLength();
				++Index)
			{
				const auto& Temporary =
					RestoredModuleConstructor->scriptData->temporaryVariables[Index];
				if (Temporary.Offset
						== RestoredModuleConstructor->scriptData->constructorCommittedCountOffset
					&& Temporary.Token == ttUInt)
				{
					bRestoredModuleCommittedCountTemporary = true;
					break;
				}
			}
			ASSERT_THAT(IsTrue(
				bRestoredModuleCommittedCountTemporary,
				TEXT("complete module restore must retain the committed-count Runtime/JIT temporary entry")));
			ASSERT_THAT(IsTrue(
				RestoredModuleConstructor->scriptData->constructorCommittedCountOffset > 0,
				TEXT("module restore must retain a valid backend-local committed-count slot")));
			ASSERT_THAT(AreEqual(
				4,
				static_cast<int32>(RestoredCleanup.GetLength()),
				TEXT("module restore must retain base, two members and complete-object cleanup records")));
			if (RestoredCleanup.GetLength() == 4)
			{
				ASSERT_THAT(IsTrue(
					RestoredCleanup[0].phase
						== asAST_LIFETIME_PHASE_CONSTRUCTOR_BASE
					&& RestoredCleanup[0].objectType
						== RestoredDerivedType->derivedFrom));
				ASSERT_THAT(IsTrue(
					RestoredCleanup[1].phase
						== asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER
					&& RestoredCleanup[1].objectOffset > 0));
				ASSERT_THAT(IsTrue(
					RestoredCleanup[2].phase
						== asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER
					&& RestoredCleanup[2].objectOffset
						> RestoredCleanup[1].objectOffset));
				ASSERT_THAT(IsTrue(
					RestoredCleanup[3].phase
						== asAST_LIFETIME_PHASE_COMPLETE_OBJECT
					&& RestoredCleanup[3].completeObject));
				ASSERT_THAT(IsTrue(
					RestoredCleanup[1].actionFunctionId > 0
					&& RestoredCleanup[1].actionFunctionId
						== RestoredCleanup[2].actionFunctionId,
					TEXT("module restore must relocate both native member cleanups to the exact current action")));
			}
		}

		asIScriptContext* const RestoredContext = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(RestoredContext));
		ON_SCOPE_EXIT
		{
			if (RestoredContext != nullptr)
			{
				RestoredContext->Release();
			}
		};
		if (RestoredEntry == nullptr || RestoredContext == nullptr)
		{
			return;
		}
		GProdConstructorCommittedAttempts = 0;
		GProdConstructorCommittedFailAt = 3;
		GProdConstructorBodyShouldFail = false;
		GProdCompleteObjectDestructorCalls = 0;
		GProdConstructorCommittedDestructionOrder.Reset();
		ASSERT_THAT(AreEqual(asSUCCESS, RestoredContext->Prepare(RestoredEntry)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, RestoredContext->Execute()));
		ASSERT_THAT(AreEqual(asSUCCESS, RestoredContext->Unprepare()));
		ASSERT_THAT(AreEqual(3, GProdConstructorCommittedAttempts));
		ASSERT_THAT(AreEqual(0, GProdCompleteObjectDestructorCalls));
		ASSERT_THAT(IsTrue(
			GProdConstructorCommittedDestructionOrder.Num() == 2
			&& GProdConstructorCommittedDestructionOrder[0] == 2
			&& GProdConstructorCommittedDestructionOrder[1] == 1,
			TEXT("restored module execution must destroy only the reverse committed prefix [2,1]")));
	}

	TEST_METHOD(CanonicalReturnInsideObjectForeachInterleavesLifetimeCleanupOrder)
	{
		GProdSuccessSensitiveDestructionOrder.Reset();
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"void RecordLifetimeDestructor(int)",
				asFUNCTION(ProdRecordSuccessSensitiveDestructor),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterGlobalFunction(
				"void ResetLifetimeDestructorTrace()",
				asFUNCTION(ProdResetSuccessSensitiveDestructorTrace),
				asCALL_GENERIC) >= 0,
			TEXT("nested foreach-return fixture registers its lifetime trace surface")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("nested foreach-return fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FReturnOrderValue
			{
				int Id;
				FReturnOrderValue(int InId) { Id = InId; }
				~FReturnOrderValue() { RecordLifetimeDestructor(Id); }
			}

			struct FReturnOrderIterator
			{
				int Index;
			}

			struct FReturnOrderRange
			{
				FReturnOrderIterator opForBegin()
				{
					FReturnOrderIterator Iterator;
					Iterator.Index = 0;
					return Iterator;
				}

				bool opForEnd(const FReturnOrderIterator& Iterator)
				{
					return Iterator.Index >= 1;
				}

				void opForNext(FReturnOrderIterator& inout Iterator)
				{
					Iterator.Index += 1;
				}

				int opForValue(const FReturnOrderIterator& Iterator)
				{
					return 42;
				}
			}

			int RunNestedReturnOrder()
			{
				FReturnOrderValue Outer(1);
				FReturnOrderRange Range;
				foreach (int Value : Range)
				{
					ResetLifetimeDestructorTrace();
					FReturnOrderValue Inner(3);
					return Value;
				}
				return 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdNestedForeachReturnCleanupOrder",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL nested foreach-return lifetime Build must succeed")));
		if (Module == nullptr)
		{
			return;
		}
		asIScriptFunction* const Published = Module->GetFunctionByDecl(
			"int RunNestedReturnOrder()");
		asITypeInfo* const ValueType = Module->GetTypeInfoByName("FReturnOrderValue");
		asITypeInfo* const IteratorType = Module->GetTypeInfoByName("FReturnOrderIterator");
		auto FindDestructor = [](asITypeInfo* Type) -> asIScriptFunction*
		{
			if (Type == nullptr)
			{
				return nullptr;
			}
			for (asUINT BehaviourIndex = 0;
				BehaviourIndex < Type->GetBehaviourCount(); ++BehaviourIndex)
			{
				asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
				asIScriptFunction* const Candidate = Type->GetBehaviourByIndex(
					BehaviourIndex, &Behaviour);
				if (Candidate != nullptr && Behaviour == asBEHAVE_DESTRUCT)
				{
					return Candidate;
				}
			}
			return nullptr;
		};
		asIScriptFunction* const ValueDestructor = FindDestructor(ValueType);
		asIScriptFunction* const IteratorDestructor = FindDestructor(IteratorType);
		ASSERT_THAT(IsNotNull(Published));
		ASSERT_THAT(IsNotNull(ValueDestructor));
		ASSERT_THAT(IsNotNull(IteratorDestructor));
		const TArray<int32> ValueDestructorOffsets = ProdBytecodeCallOffsets(
			Published, ValueDestructor != nullptr ? ValueDestructor->GetId() : 0);
		const int32 IteratorDestructorOffset = ProdBytecodeFirstCallOffset(
			Published, IteratorDestructor != nullptr ? IteratorDestructor->GetId() : 0);
		ASSERT_THAT(IsTrue(
			ValueDestructorOffsets.Num() >= 2
				&& IteratorDestructorOffset != INDEX_NONE
				&& ValueDestructorOffsets[0] < IteratorDestructorOffset
				&& IteratorDestructorOffset < ValueDestructorOffsets[1],
			*FString::Printf(
				TEXT("return bytecode must interleave the foreach iterator cleanup between Inner and Outer value cleanup; values=%s iterator=%d calls={%s}"),
				*FString::JoinBy(ValueDestructorOffsets, TEXT(","),
					[](const int32 Offset) { return FString::FromInt(Offset); }),
				IteratorDestructorOffset,
				*ProdBytecodeCallTargets(Published))));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = AngelscriptNativeTestSupport::CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int RunNestedReturnOrder()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 42,
			*FString::Printf(
				TEXT("return inside object foreach must execute through CANONICAL bytecode; status=%d got=%d exception=%s"),
				ExecuteStatus, Value, *Exception)));
		ASSERT_THAT(IsTrue(
			GProdSuccessSensitiveDestructionOrder.Num() == 2
				&& GProdSuccessSensitiveDestructionOrder[0] == 3
				&& GProdSuccessSensitiveDestructionOrder[1] == 1,
			TEXT("return execution must destroy the traced Inner and Outer values once in reverse order")));

		const asCModule* const Internal = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal->GetLastBytecodePublisher(),
			TEXT("nested foreach-return cleanup must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			0,
			Internal->GetLastLegacyCompilerInvocationCount(),
			TEXT("nested foreach-return cleanup must not invoke asCCompiler")));
	}

	TEST_METHOD(CanonicalLexicalOwningHandleExecutesSealedReleasePlansExactlyOnce)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		FProdTrackedHandleState State;
		GProdTrackedHandleState = &State;
		ON_SCOPE_EXIT
		{
			while (State.LastObject != nullptr && State.LastObject->ReferenceCount > 0)
			{
				State.LastObject->Release();
			}
			GProdTrackedHandleState = nullptr;
		};

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"CTrackedHandle", 0, asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"CTrackedHandle",
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(ProdTrackedHandleAddRef),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"CTrackedHandle",
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(ProdTrackedHandleRelease),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"CTrackedHandle", "int Value", asOFFSET(FProdTrackedHandle, Value)) >= 0
			&& ScriptEngine->RegisterGlobalFunction(
				"CTrackedHandle MakeTrackedHandle()",
				asFUNCTION(ProdTrackedHandleFactory),
				asCALL_GENERIC) >= 0,
			TEXT("lexical owning-handle fixture must register its native surface")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("lexical owning-handle fixture selects CANONICAL")));

		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdLexicalOwningHandleCleanup", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int RunOwnedHandleCleanup(const bool Early)
			{
				CTrackedHandle Object = MakeTrackedHandle();
				if (Early)
					return Object.Value;
				return Object.Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdLexicalOwningHandleCleanup.as",
			ScriptSource.c_str(),
			static_cast<asUINT>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("CANONICAL lexical owning-handle cleanup Build must succeed")));

		const asCASTContext* const CanonicalContext =
			static_cast<asCModule*>(Module)->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(CanonicalContext));
		if (CanonicalContext == nullptr)
		{
			return;
		}
		asCString AstDump;
		asCASTDump(*CanonicalContext, AstDump);
		int32 ReleasePlanCount = 0;
		for (asUINT ExprIndex = 1; ExprIndex <= CanonicalContext->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Cleanup = CanonicalContext->GetExpr(asASTExprId(ExprIndex));
			const asCExpr* const Target = Cleanup != nullptr
				&& Cleanup->kind == asAST_EXPR_CLEANUP
				&& Cleanup->literal.Equals("scope-release")
				&& !Cleanup->resolvedDecl.IsValid()
				&& Cleanup->children.GetLength() == 1
				? CanonicalContext->GetExpr(Cleanup->children[0]) : nullptr;
			const asCDecl* const TargetDecl = Target != nullptr
				&& Target->kind == asAST_EXPR_DECL_REF
				? CanonicalContext->GetDecl(Target->resolvedDecl) : nullptr;
			if (TargetDecl != nullptr && TargetDecl->kind == asAST_DECL_VAR
				&& TargetDecl->name.Equals("Object"))
			{
				++ReleasePlanCount;
			}
		}
		ASSERT_THAT(AreEqual(
			3,
			ReleasePlanCount,
			*FString::Printf(
				TEXT("Object must own one normal and two return scope-release plans. AST:\n%s"),
				UTF8_TO_TCHAR(AstDump.AddressOf() ? AstDump.AddressOf() : ""))));

		asIScriptFunction* const Entry = Module->GetFunctionByDecl(
			"int RunOwnedHandleCleanup(const bool)");
		ASSERT_THAT(IsNotNull(Entry));
		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ON_SCOPE_EXIT
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
		};
		if (Entry == nullptr || Context == nullptr)
		{
			return;
		}

		auto ExecuteAndRequireBalancedRelease = [&](const bool bEarly)
		{
			const int32 CreatedBefore = State.Created;
			const int32 DestroyedBefore = State.Destroyed;
			const int32 AddRefBefore = State.AddRefCalls;
			const int32 ReleaseBefore = State.ReleaseCalls;
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Entry)));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgByte(0, bEarly ? 1 : 0)));
			const int ExecuteStatus = Context->Execute();
			ASSERT_THAT(IsTrue(
				ExecuteStatus == asEXECUTION_FINISHED
					&& Context->GetReturnDWord() == 42,
				TEXT("both owning-handle exits must execute Canonical bytecode")));
			ASSERT_THAT(AreEqual(1, State.Created - CreatedBefore,
				TEXT("each execution must create one owned native handle")));
			ASSERT_THAT(AreEqual(1, State.Destroyed - DestroyedBefore,
				TEXT("each sealed release path must destroy the owned handle exactly once")));
			ASSERT_THAT(AreEqual(
				(State.AddRefCalls - AddRefBefore) + 1,
				State.ReleaseCalls - ReleaseBefore,
				TEXT("release count must exactly balance transient AddRef calls plus factory ownership")));
			ASSERT_THAT(AreEqual(0, State.LiveObjects,
				TEXT("sealed release must leave no native object live")));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare()));
		};
		ExecuteAndRequireBalancedRelease(true);
		ExecuteAndRequireBalancedRelease(false);

		const asCModule* const Internal = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal->GetLastBytecodePublisher(),
			TEXT("owning-handle cleanup execution must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			0,
			Internal->GetLastLegacyCompilerInvocationCount(),
			TEXT("owning-handle cleanup execution must not invoke asCCompiler")));
	}

	TEST_METHOD(CanonicalDefaultNullFuncdefMatchesLegacyAndExecutesSealedRelease)
	{
		using namespace AngelscriptNativeTestSupport;

		auto RegisterSurface = [&](asCScriptEngine* ScriptEngine)
		{
			if (ScriptEngine == nullptr
				|| ScriptEngine->RegisterFuncdef("int FDefaultNullCallback()") < 0
				|| ScriptEngine->RegisterObjectType(
					"FDefaultNullReference", 0,
					asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) < 0)
			{
				return false;
			}
			asCTypeInfo* const CallbackType = static_cast<asCTypeInfo*>(
				ScriptEngine->GetTypeInfoByName("FDefaultNullCallback"));
			if (CallbackType == nullptr)
			{
				return false;
			}
			CallbackType->flags |= asOBJ_IMPLICIT_HANDLE;
			return true;
		};

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int ObserveDefaultNullLocals()
			{
				FDefaultNullCallback Callback;
				FDefaultNullReference Reference;
				return 42;
			}
			)AS");

		{
			FNativeTestEngine LegacyEngine;
			LegacyEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { LegacyEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine =
				static_cast<asCScriptEngine*>(LegacyEngine.Get());
			ASSERT_THAT(IsTrue(RegisterSurface(ScriptEngine),
				TEXT("LEGACY default-null fixture must register its native surface")));
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY)));
			asIScriptModule* Module = nullptr;
			ASSERT_THAT(AreEqual(0, CompileNativeModule(
				LegacyEngine.Get(),
				"LegacyDefaultNullLocals",
				ScriptSource.c_str(),
				Module)));
			int32 Value = 0;
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner,
				LegacyEngine.Get(),
				Module,
				"int ObserveDefaultNullLocals()",
				Value) && Value == 42,
				TEXT("LEGACY defines both uninitialized owning slots as null")));
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_COMPILER,
				static_cast<asCModule*>(Module)->GetLastBytecodePublisher()));
		}

		{
			FNativeTestEngine CanonicalEngine;
			CanonicalEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { CanonicalEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine =
				static_cast<asCScriptEngine*>(CanonicalEngine.Get());
			ASSERT_THAT(IsTrue(RegisterSurface(ScriptEngine),
				TEXT("CANONICAL default-null fixture must register its native surface")));
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));
			asIScriptModule* const Module = ScriptEngine->GetModule(
				"CanonicalDefaultNullLocals", asGM_ALWAYS_CREATE);
			ASSERT_THAT(IsNotNull(Module));
			ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
			ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
				"CanonicalDefaultNullLocals.as",
				ScriptSource.c_str(),
				static_cast<asUINT>(ScriptSource.size()))));
			ASSERT_THAT(AreEqual(0, Module->Build(),
				TEXT("CANONICAL default-null fixture must build from sealed AST")));

			const asCASTContext* const Ast =
				static_cast<asCModule*>(Module)->GetCanonicalASTContext();
			ASSERT_THAT(IsNotNull(Ast));
			const asCDecl* Callback = nullptr;
			const asCDecl* Reference = nullptr;
			for (asUINT DeclIndex = 1; Ast != nullptr && DeclIndex <= Ast->GetDeclCount(); ++DeclIndex)
			{
				const asCDecl* const Decl = Ast->GetDecl(asASTDeclId(DeclIndex));
				if (Decl != nullptr && Decl->kind == asAST_DECL_VAR
					&& Decl->name.Equals("Callback"))
				{
					Callback = Decl;
				}
				else if (Decl != nullptr && Decl->kind == asAST_DECL_VAR
					&& Decl->name.Equals("Reference"))
				{
					Reference = Decl;
				}
			}
			const asCExpr* const Init = Callback != nullptr && Callback->inits.GetLength() == 1
				? Ast->GetExpr(Callback->inits[0]) : nullptr;
			const asCExpr* const ReferenceInit = Reference != nullptr
				&& Reference->inits.GetLength() == 1
				? Ast->GetExpr(Reference->inits[0]) : nullptr;
			int32 ReleasePlanCount = 0;
			for (asUINT ExprIndex = 1; Ast != nullptr && ExprIndex <= Ast->GetExprCount(); ++ExprIndex)
			{
				const asCExpr* const Cleanup = Ast->GetExpr(asASTExprId(ExprIndex));
				const asCExpr* const Target = Cleanup != nullptr
					&& Cleanup->kind == asAST_EXPR_CLEANUP
					&& Cleanup->literal.Equals("scope-release")
					&& Cleanup->children.GetLength() == 1
					? Ast->GetExpr(Cleanup->children[0]) : nullptr;
				if (Callback != nullptr && Target != nullptr
					&& Target->resolvedDecl == Callback->id)
				{
					++ReleasePlanCount;
				}
			}
			ASSERT_THAT(IsTrue(Init != nullptr && Init->kind == asAST_EXPR_NULL_LITERAL,
				TEXT("production funcdef local must seal an explicit default-null initializer")));
			ASSERT_THAT(IsTrue(
				ReferenceInit != nullptr
					&& ReferenceInit->kind == asAST_EXPR_NULL_LITERAL,
				TEXT("implicit-handle reference local must seal null instead of an unresolved inline Construct")));
			ASSERT_THAT(AreEqual(2, ReleasePlanCount,
				TEXT("production funcdef local must seal normal plus return releases")));
			int32 Value = 0;
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner,
				CanonicalEngine.Get(),
				Module,
				"int ObserveDefaultNullLocals()",
				Value) && Value == 42,
				TEXT("CANONICAL funcdef default lifetime must match LEGACY execution")));
			asIScriptFunction* const Entry = Module->GetFunctionByDecl(
				"int ObserveDefaultNullLocals()");
			ASSERT_THAT(IsTrue(
				AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_FREE),
				TEXT("sealed default-null funcdef cleanup must lower to FREE")));
			const asCModule* const Internal = static_cast<asCModule*>(Module);
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
				Internal->GetLastBytecodePublisher()));
			ASSERT_THAT(AreEqual(0, Internal->GetLastLegacyCompilerInvocationCount()));
		}
	}

	TEST_METHOD(CanonicalConstGlobalBuildPublishesCodeGenAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("const-global fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			const int G = 41;

			int F()
			{
				return G + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdConstGlobal",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL const-global Build must succeed from CodeGen")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL const-global F() must execute from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL const-global Build must not record asCCompiler")));
	}

	TEST_METHOD(CanonicalFloatNamespaceGlobalInitializesExecutesAndPublishesCodeGen)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("float namespace-global fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule("ProdFloatNamespaceGlobal", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("ProdFloatNamespaceGlobal module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			namespace Game
			{
				const float x = 2;

				float Read()
				{
					return x;
				}
			}

			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection("ProdFloatNamespaceGlobal.as", ScriptSource.c_str())));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("Sema-proven float namespace global must lower through Canonical CodeGen")));

		asCModule* const Internal = static_cast<asCModule*>(Module);
		ASSERT_THAT(IsTrue(Internal != nullptr && Internal->GetCanonicalASTContext() != nullptr,
			TEXT("float namespace-global execution must retain a sealed canonical AST")));
		if (Internal != nullptr && Internal->GetCanonicalASTContext() != nullptr)
		{
			asCString Dump;
			asCASTDump(*Internal->GetCanonicalASTContext(), Dump);
			const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
			ASSERT_THAT(IsTrue(
				Text.Contains(TEXT("kind=Var name=x"))
				&& Text.Contains(TEXT("type=double"))
				&& Text.Contains(TEXT("constant=")),
				*FString::Printf(
					TEXT("the sealed AST must carry a typed scalar value, not only source text. dump:\n%s"),
					*Text)));
		}

		using namespace AngelscriptSDKTestSupport;
		FSdkFunctionInvoker Reader(*TestRunner, Engine.Get(), Module, "float Game::Read()");
		ASSERT_THAT(IsTrue(Reader.IsValid(),
			TEXT("the namespaced float reader must resolve after Canonical CodeGen publication")));
		if (Reader.IsValid())
		{
			// This fork can expose AngelScript `float` through a double ABI slot;
			// assert the numerical result at the boundary rather than tying the
			// AST scalar-bit contract to the host return representation.
			const double Value = Reader.CallAndReturn<double>(0.0);
			ASSERT_THAT(IsNear(2.0, Value, 0.0001,
				TEXT("the typed float namespace global must preserve its value through CodeGen execution")));
		}
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("float namespace global must not fall back to asCCompiler")));
	}

	TEST_METHOD(CanonicalConstGlobalBinaryInitEvaluatesFortyPlusOne)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("binary-global fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule("ProdConstGlobalAdd", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("ProdConstGlobalAdd module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			const int G = 40 + 1;

			int F()
			{
				return G + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection("ProdConstGlobalAdd.as", ScriptSource.c_str())));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("CANONICAL const-global 40+1 Build must succeed from CodeGen")));
		asCModule* const Internal = static_cast<asCModule*>(Module);
		ASSERT_THAT(IsTrue(Internal != nullptr && Internal->GetCanonicalASTContext() != nullptr,
			TEXT("retain sealed AST for 40+1 init dump")));
		asCString Dump;
		asCASTDump(*Internal->GetCanonicalASTContext(), Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(
			Text.Contains(TEXT("kind=Var name=G"))
			&& Text.Contains(TEXT("default=41"))
			&& Text.Contains(TEXT("constant=41")),
			*FString::Printf(
				TEXT("Sema must store evaluated constant=41 for G=40+1; CodeGen must not reparse default text. dump:\n%s"),
				*Text)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("Sema must evaluate 40+1 to 41, not atoi the source prefix 40; F()=G+1 expected 42 got %d. dump:\n%s"),
				Value,
				*Text)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("binary-global Build must not record asCCompiler")));
	}

	TEST_METHOD(CanonicalConstGlobalHexInitEvaluates0x29)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("hex-global fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			const int G = 0x29;

			int F()
			{
				return G + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdConstGlobalHex",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL const-global 0x29 Build must succeed from CodeGen")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("Sema must evaluate 0x29 to 41, not atoi(\"0x29\")==0; F()=G+1 expected 42 got %d"),
				Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("hex-global Build must not record asCCompiler")));
	}

	TEST_METHOD(CanonicalLambdaBuildPublishesCodeGenAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("lambda fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				return function(int X)
				{
					return X + 1;
				}(41);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdLambda",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL lambda Build must succeed from CodeGen")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL lambda F() must execute from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL lambda Build must not record asCCompiler")));
	}

	TEST_METHOD(CanonicalTemporaryConstructPublishesCodeGenAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("temporary-construct fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FValue
			{
				int Value;
				FValue()
				{
					Value = 41;
				}
			}

			int F()
			{
				return FValue().Value + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdTemporary",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL temporary construct Build must succeed from CodeGen")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL temporary F() must execute from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL temporary Build must not record asCCompiler")));
	}

	TEST_METHOD(CanonicalTryCatchRejectedDoesNotPublishCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("try/catch fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				try
				{
					return 1;
				}
				catch
				{
					return 0;
				}
			}
			)AS");
		const int BuildResult = AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdTryCatch",
			ScriptSource.c_str(),
			Module);
		ASSERT_THAT(IsTrue(BuildResult < 0,
			TEXT("CANONICAL try/catch must stay rejected rather than execute via asCCompiler")));
		if (Module != nullptr)
		{
			ASSERT_THAT(IsTrue(
				static_cast<asCModule*>(Module)->GetLastBytecodePublisher() != asBYTECODE_PUBLISHER_COMPILER,
				TEXT("rejected CANONICAL try/catch must not claim asCCompiler publication")));
		}
	}

	TEST_METHOD(CanonicalFailedRebuildKeepsLastSuccessfulModuleExecutable)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("rebuild-preservation fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule("ProdFailedRebuild", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("rebuild-preservation fixture needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		const std::string GoodSource = ASTEST_AS_ANSI(R"AS(
			int Stable()
			{
				return 7;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdFailedRebuild_Good.as", GoodSource.c_str(), static_cast<unsigned int>(GoodSource.size())),
			TEXT("the initial Canonical module version must accept its source")));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("the initial Canonical module version must publish successfully")));
		int32 InitialValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Stable()", InitialValue)
				&& InitialValue == 7,
			TEXT("the initial Canonical module version must execute before the failed rebuild")));

		const std::string InvalidReplacementSource = ASTEST_AS_ANSI(R"AS(
			int Broken()
			{
				try
				{
					return 1;
				}
				catch
				{
					return 0;
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdFailedRebuild_Invalid.as", InvalidReplacementSource.c_str(),
			static_cast<unsigned int>(InvalidReplacementSource.size())),
			TEXT("a replacement attempt must stage its own source after the successful build")));
		ASSERT_THAT(IsTrue(Module->Build() < 0,
			TEXT("unsupported Canonical try/catch must reject the replacement build")));

		asIScriptFunction* const Retained = Module->GetFunctionByDecl("int Stable()");
		ASSERT_THAT(IsNotNull(Retained,
			TEXT("a rejected Canonical replacement must retain the last successfully published function")));
		int32 RetainedValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Stable()", RetainedValue)
				&& RetainedValue == 7,
			TEXT("a rejected Canonical replacement must keep the last successful module executable")));
	}

	TEST_METHOD(CanonicalExplicitScopeMemberMissFailsClosedAndPreservesGeneration)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("explicit-scope publication fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdExplicitScopeFailure", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("explicit-scope publication fixture needs a module")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));

		const std::string GoodSource = ASTEST_AS_ANSI(R"AS(
			int Stable()
			{
				return 7;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdExplicitScopeFailure.as", GoodSource.c_str(),
			static_cast<unsigned int>(GoodSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("generation A must publish before the invalid qualified replacement")));

		asCModule* const Internal = static_cast<asCModule*>(Module);
		asQWORD DigestBefore = 0;
		ASSERT_THAT(IsTrue(Internal->GetLastCanonicalCodeGenAstDigest(DigestBefore)
			&& DigestBefore != 0,
			TEXT("generation A must expose its sealed AST digest")));
		asIASTSnapshot* const SnapshotBefore = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(SnapshotBefore));
		if (SnapshotBefore == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { SnapshotBefore->Release(); };
		const FString GenerationKeyBefore = UTF8_TO_TCHAR(SnapshotBefore->GetGenerationKey());

		const std::string InvalidSource = ASTEST_AS_ANSI(R"AS(
			int Target()
			{
				return 42;
			}

			namespace Missing
			{
			}

			int Broken()
			{
				return Missing::Target();
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdExplicitScopeFailure.as", InvalidSource.c_str(),
			static_cast<unsigned int>(InvalidSource.size()))));
		ASSERT_THAT(IsTrue(Module->Build() < 0,
			TEXT("a missing member in a resolved explicit scope must fail before CodeGen publication")));

		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal->GetLastBytecodePublisher(),
			TEXT("failed replacement must retain generation A publisher")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Internal->GetLastLegacyCompilerInvocationCount(),
			TEXT("scope failure must never invoke the legacy body compiler")));
		asQWORD DigestAfter = 0;
		ASSERT_THAT(IsTrue(Internal->GetLastCanonicalCodeGenAstDigest(DigestAfter)
			&& DigestAfter == DigestBefore,
			TEXT("failed replacement must retain generation A sealed digest")));
		ASSERT_THAT(IsTrue(SnapshotBefore->IsCurrentGeneration(),
			TEXT("failed replacement must leave generation A snapshot current")));
		asIASTSnapshot* const SnapshotAfter = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(SnapshotAfter));
		if (SnapshotAfter != nullptr)
		{
			ASSERT_THAT(AreEqual(GenerationKeyBefore,
				FString(UTF8_TO_TCHAR(SnapshotAfter->GetGenerationKey())),
				TEXT("failed replacement must not advance the combined generation")));
			SnapshotAfter->Release();
		}
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Broken()"),
			TEXT("invalid replacement declarations must not become public")));
		int32 RetainedValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Stable()", RetainedValue)
				&& RetainedValue == 7,
			TEXT("failed explicit-scope replacement must keep generation A executable")));
	}

	TEST_METHOD(CanonicalQualifiedDeclRefUsesExactScopeAndFailedReplacementPreservesGeneration)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("qualified DeclRef publication fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdQualifiedDeclRef", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("qualified DeclRef publication fixture needs a module")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));

		const std::string GoodSource = ASTEST_AS_ANSI(R"AS(
			int Value = 1;

			int Entry()
			{
				return Later::Value;
			}

			namespace Later
			{
				int Value = 42;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdQualifiedDeclRef.as", GoodSource.c_str(),
			static_cast<unsigned int>(GoodSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("generation A must publish the exact later-qualified global")));

		asCModule* const Internal = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Internal->GetLastLegacyCompilerInvocationCount()));
		int32 InitialValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", InitialValue)
				&& InitialValue == 42,
			TEXT("Later::Value must execute the exact namespace global, not global Value")));

		asQWORD DigestBefore = 0;
		ASSERT_THAT(IsTrue(Internal->GetLastCanonicalCodeGenAstDigest(DigestBefore)
			&& DigestBefore != 0));
		asIASTSnapshot* const SnapshotBefore = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(SnapshotBefore));
		if (SnapshotBefore == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { SnapshotBefore->Release(); };
		const FString GenerationKeyBefore = UTF8_TO_TCHAR(SnapshotBefore->GetGenerationKey());

		const std::string InvalidSource = ASTEST_AS_ANSI(R"AS(
			int Value = 99;

			int Broken()
			{
				return Missing::Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdQualifiedDeclRef.as", InvalidSource.c_str(),
			static_cast<unsigned int>(InvalidSource.size()))));
		ASSERT_THAT(IsTrue(Module->Build() < 0,
			TEXT("an unresolved explicit DeclRef scope must fail before publication")));

		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal->GetLastBytecodePublisher(),
			TEXT("failed DeclRef replacement must retain generation A publisher")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Internal->GetLastLegacyCompilerInvocationCount(),
			TEXT("DeclRef scope failure must never invoke the legacy body compiler")));
		asQWORD DigestAfter = 0;
		ASSERT_THAT(IsTrue(Internal->GetLastCanonicalCodeGenAstDigest(DigestAfter)
			&& DigestAfter == DigestBefore,
			TEXT("failed DeclRef replacement must retain generation A digest")));
		ASSERT_THAT(IsTrue(SnapshotBefore->IsCurrentGeneration(),
			TEXT("failed DeclRef replacement must leave generation A current")));
		asIASTSnapshot* const SnapshotAfter = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(SnapshotAfter));
		if (SnapshotAfter != nullptr)
		{
			ASSERT_THAT(AreEqual(GenerationKeyBefore,
				FString(UTF8_TO_TCHAR(SnapshotAfter->GetGenerationKey()))));
			SnapshotAfter->Release();
		}
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Broken()")));
		int32 RetainedValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", RetainedValue)
				&& RetainedValue == 42,
			TEXT("failed DeclRef replacement must keep exact generation A executable")));
	}

	TEST_METHOD(CanonicalDeferredQualifiedNamesRecomputeParentAndExecute)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("deferred parent reconciliation fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdDeferredParentReconciliation", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("deferred parent reconciliation fixture needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			double EntryCall()
			{
				return Later::Target() + 1;
			}

			double EntryRef()
			{
				return Later::Value + 1;
			}

			namespace Later
			{
				double Target()
				{
					return 41.0;
				}

				double Value = 41.0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdDeferredParentReconciliation.as", Source.c_str(),
			static_cast<unsigned int>(Source.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("deferred call and DeclRef parents must reconcile before Canonical CodeGen")));

		asCModule* const Internal = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Internal->GetLastLegacyCompilerInvocationCount()));

		double CallValue = 0.0;
		ASSERT_THAT(IsTrue(
			AngelscriptSDKTestSupport::ExecuteScriptFunction(
				*TestRunner, Engine.Get(), Module, "double EntryCall()", CallValue)
				&& FMath::IsNearlyEqual(CallValue, 42.0),
			TEXT("deferred qualified call parent must execute as double 42.0")));
		double RefValue = 0.0;
		ASSERT_THAT(IsTrue(
			AngelscriptSDKTestSupport::ExecuteScriptFunction(
				*TestRunner, Engine.Get(), Module, "double EntryRef()", RefValue)
				&& FMath::IsNearlyEqual(RefValue, 42.0),
			TEXT("deferred qualified DeclRef parent must execute as double 42.0")));
	}

	TEST_METHOD(CanonicalFailedRebuildKeepsLastSuccessfulSnapshotCurrent)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("snapshot-preservation fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdFailedRebuildSnapshot", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("snapshot-preservation fixture needs a module")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT),
			TEXT("snapshot-preservation fixture must retain successful Canonical AST generations")));

		const std::string GoodSource = ASTEST_AS_ANSI(R"AS(
			int Stable()
			{
				return 7;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdFailedRebuildSnapshot_Good.as", GoodSource.c_str(),
			static_cast<unsigned int>(GoodSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("generation A must publish before the rejected replacement")));

		asIASTSnapshot* const Before = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Before,
			TEXT("the successful generation must publish a retained AST snapshot")));
		if (Before == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Before->Release(); };
		const FString GenerationKeyBefore = UTF8_TO_TCHAR(Before->GetGenerationKey());
		ASSERT_THAT(IsTrue(Before->IsCurrentGeneration(),
			TEXT("generation A snapshot must initially be current")));

		const std::string InvalidReplacementSource = ASTEST_AS_ANSI(R"AS(
			int Broken()
			{
				try
				{
					return 1;
				}
				catch
				{
					return 0;
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdFailedRebuildSnapshot_Invalid.as", InvalidReplacementSource.c_str(),
			static_cast<unsigned int>(InvalidReplacementSource.size()))));
		ASSERT_THAT(IsTrue(Module->Build() < 0,
			TEXT("the replacement must fail through the Canonical rejection path")));

		ASSERT_THAT(IsTrue(Before->IsCurrentGeneration(),
			TEXT("a failed replacement must leave the prior snapshot current")));
		asIASTSnapshot* const Fresh = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Fresh,
			TEXT("a failed replacement must not clear the last-good snapshot")));
		if (Fresh != nullptr)
		{
			ASSERT_THAT(IsTrue(Fresh->IsCurrentGeneration(),
				TEXT("the newly acquired last-good snapshot must still be current")));
			ASSERT_THAT(AreEqual(GenerationKeyBefore,
				FString(UTF8_TO_TCHAR(Fresh->GetGenerationKey())),
				TEXT("a failed replacement must not advance the retained AST generation")));
			Fresh->Release();
		}
	}

	TEST_METHOD(CanonicalSuccessfulRebuildReplacesExecutableGeneration)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("successful-rebuild fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule("ProdSuccessfulRebuild", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("successful-rebuild fixture needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		const std::string GenerationASource = ASTEST_AS_ANSI(R"AS(
			int Stable()
			{
				return 7;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdSuccessfulRebuild.as", GenerationASource.c_str(),
			static_cast<unsigned int>(GenerationASource.size())),
			TEXT("generation A source must be accepted")));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("generation A must publish successfully")));
		int32 InitialValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Stable()", InitialValue)
				&& InitialValue == 7,
			TEXT("generation A must execute before replacement")));

		const std::string GenerationBSource = ASTEST_AS_ANSI(R"AS(
			int Replacement()
			{
				return 9;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdSuccessfulRebuild.as", GenerationBSource.c_str(),
			static_cast<unsigned int>(GenerationBSource.size())),
			TEXT("generation B source must replace the pending source section")));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("generation B must publish successfully")));

		ASSERT_THAT(IsTrue(
			ScriptEngine->GetModule("ProdSuccessfulRebuild", asGM_ONLY_IF_EXISTS) == Module,
			TEXT("successful Canonical replacement must retain the public module identity")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Stable()"),
			TEXT("generation B must not retain generation A executable declarations")));
		ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int Replacement()"),
			TEXT("generation B must publish its executable declaration")));
		int32 ReplacementValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Replacement()", ReplacementValue)
				&& ReplacementValue == 9,
			TEXT("generation B executable declaration must execute from the promoted candidate")));
	}

	TEST_METHOD(CanonicalSuccessfulRebuildReplacesSameNamedTypeGeneration)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("type-rebuild fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule("ProdTypeRebuild", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("type-rebuild fixture needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		const std::string GenerationASource = ASTEST_AS_ANSI(R"AS(
			struct FRevisioned
			{
				int Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdTypeRebuild.as", GenerationASource.c_str(),
			static_cast<unsigned int>(GenerationASource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("type generation A must publish successfully")));
		asITypeInfo* const TypeA = Module->GetTypeInfoByDecl("FRevisioned");
		ASSERT_THAT(IsNotNull(TypeA, TEXT("type generation A must publish its declaration")));
		if (TypeA == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1), TypeA->GetPropertyCount(),
			TEXT("type generation A must expose its original layout")));

		const std::string GenerationBSource = ASTEST_AS_ANSI(R"AS(
			struct FRevisioned
			{
				int Value;
				int Revision;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdTypeRebuild.as", GenerationBSource.c_str(),
			static_cast<unsigned int>(GenerationBSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("type generation B must publish successfully")));

		asITypeInfo* const TypeB = Module->GetTypeInfoByDecl("FRevisioned");
		ASSERT_THAT(IsNotNull(TypeB, TEXT("type generation B must publish its replacement declaration")));
		if (TypeB == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(TypeB != TypeA,
			TEXT("same-named Canonical types must be distinct generations rather than a mutated old type")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2), TypeB->GetPropertyCount(),
			TEXT("type generation B must expose the replacement layout")));
		ASSERT_THAT(IsTrue(TypeB->GetModule() == Module,
			TEXT("a promoted replacement type must be owned by the public module")));
		asCTypeInfo* EngineVisibleType = nullptr;
		ScriptEngine->allScriptDeclaredTypes.FindAllUntil("FRevisioned", [&EngineVisibleType](asCTypeInfo* Candidate)
		{
			if (Candidate != nullptr && Candidate->nameSpace != nullptr
				&& Candidate->nameSpace->name.GetLength() == 0)
			{
				EngineVisibleType = Candidate;
				return true;
			}
			return false;
		});
		ASSERT_THAT(IsTrue(EngineVisibleType == TypeB,
			TEXT("the script-type availability table must resolve the promoted type rather than the retired generation")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("ProdTypeRebuild"),
			TEXT("type replacement fixture must discard its public module")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->GarbageCollect(),
			TEXT("discarded type generations must be released after their normal collection boundary")));
		ASSERT_THAT(IsFalse(ScriptEngine->allScriptDeclaredTypes.Contains("FRevisioned"),
			TEXT("collecting the discarded replacement module must remove its type from script-type availability")));
	}

	TEST_METHOD(CanonicalMutableIntegerGlobalInitializesStoresAndPublishesCodeGen)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("mutable-global fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int G = 1;

			int ReadInitial()
			{
				return G;
			}

			int StoreAndRead()
			{
				G = 42;
				return G;
			}

			int ReadAfterStore()
			{
				return G;
			}
			)AS");
		const int BuildResult = AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdMutableGlobal",
			ScriptSource.c_str(),
			Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("CANONICAL constant-initialized mutable global must Build through CodeGen")));
		int32 InitialValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int ReadInitial()", InitialValue)
				&& InitialValue == 1,
			*FString::Printf(
				TEXT("CANONICAL mutable global must reinitialize to its sealed constant value after Build; got=%d"),
				InitialValue)));
		int32 StoredValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int StoreAndRead()", StoredValue)
				&& StoredValue == 42,
			*FString::Printf(
				TEXT("CANONICAL mutable global must accept a direct function-body store; got=%d"),
				StoredValue)));
		int32 ReadValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int ReadAfterStore()", ReadValue)
				&& ReadValue == 42,
			*FString::Printf(
				TEXT("CANONICAL mutable global write must be visible to a later function invocation; got=%d"),
				ReadValue)));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->ResetGlobalVars(nullptr),
			TEXT("CANONICAL mutable global reset must execute the property-owned initializer")));
		int32 ResetValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int ReadAfterStore()", ResetValue)
				&& ResetValue == 1,
			*FString::Printf(
				TEXT("CANONICAL mutable global must restore its initializer after ResetGlobalVars; got=%d"),
				ResetValue)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL mutable global must not fall back to asCCompiler")));
	}

	TEST_METHOD(CanonicalMutableUInt64GlobalPreservesExactInitializerAcrossReset)
	{
		using namespace AngelscriptSDKTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("uint64 mutable-global fixture selects CANONICAL")));
		const uint64 ExpectedValue = static_cast<uint64>(-1);
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			uint64 G = 18446744073709551615;

			uint64 Read()
			{
				return G;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdMutableUInt64Global",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL uint64 mutable global must Build through CodeGen")));

		FSdkFunctionInvoker InitialReader(*TestRunner, Engine.Get(), Module, "uint64 Read()");
		ASSERT_THAT(IsTrue(InitialReader.IsValid(), TEXT("uint64 mutable global reader must resolve")));
		const uint64 InitialValue = InitialReader.CallAndReturn<uint64>();
		ASSERT_THAT(AreEqual(ExpectedValue, InitialValue,
			*FString::Printf(
				TEXT("CANONICAL uint64 global must retain all initializer bits through uint64 max; expected=%llu actual=%llu"),
				static_cast<unsigned long long>(ExpectedValue),
				static_cast<unsigned long long>(InitialValue))));
		asIScriptFunction* const ReaderFunction =
			AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "uint64 Read()");
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(ReaderFunction, asBC_RDR8),
			TEXT("uint64 global reader must use the sealed eight-byte RDR8 load")));
		ASSERT_THAT(IsFalse(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(ReaderFunction, asBC_CpyGtoV4),
			TEXT("uint64 global reader must not truncate through the four-byte CpyGtoV4 fast path")));
		const asCScriptFunction* const InternalReader =
			static_cast<const asCScriptFunction*>(ReaderFunction);
		bool bPublishedUInt64Temporary = false;
		if (InternalReader != nullptr && InternalReader->scriptData != nullptr)
		{
			for (asUINT Index = 0;
				Index < InternalReader->scriptData->temporaryVariables.GetLength();
				++Index)
			{
				if (InternalReader->scriptData->temporaryVariables[Index].Token
					== ttUInt64)
				{
					bPublishedUInt64Temporary = true;
					break;
				}
			}
		}
		ASSERT_THAT(IsTrue(
			bPublishedUInt64Temporary,
			TEXT("Canonical CodeGen must publish the uint64 RDR8 result in the Runtime temporary-variable type directory")));

		const asUINT GlobalCount = Module != nullptr ? Module->GetGlobalVarCount() : 0;
		ASSERT_THAT(AreEqual(asUINT(1), GlobalCount,
			TEXT("uint64 mutable-global fixture must publish its one writable property")));
		void* const GlobalAddress = GlobalCount == 1
			? Module->GetAddressOfGlobalVar(0)
			: nullptr;
		ASSERT_THAT(IsNotNull(GlobalAddress, TEXT("uint64 mutable global must expose writable storage")));
		if (GlobalAddress != nullptr)
		{
			*static_cast<uint64*>(GlobalAddress) = 7;
		}
		FSdkFunctionInvoker OverwrittenReader(*TestRunner, Engine.Get(), Module, "uint64 Read()");
		const uint64 OverwrittenValue = OverwrittenReader.CallAndReturn<uint64>();
		ASSERT_THAT(AreEqual(uint64(7), OverwrittenValue,
			TEXT("test fixture must prove ResetGlobalVars overwrites existing uint64 storage")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->ResetGlobalVars(nullptr),
			TEXT("CANONICAL uint64 global reset must execute its property-owned initializer")));
		FSdkFunctionInvoker ResetReader(*TestRunner, Engine.Get(), Module, "uint64 Read()");
		const uint64 ResetValue = ResetReader.CallAndReturn<uint64>();
		ASSERT_THAT(AreEqual(ExpectedValue, ResetValue,
			*FString::Printf(
				TEXT("CANONICAL uint64 global reset must restore every initializer bit; expected=%llu actual=%llu"),
				static_cast<unsigned long long>(ExpectedValue),
				static_cast<unsigned long long>(ResetValue))));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL uint64 mutable global must not fall back to asCCompiler")));
	}

	TEST_METHOD(CanonicalDoubleToFloatPublishesNormalizedTemporaryType)
	{
		using namespace AngelscriptSDKTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("numeric-conversion fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			float32 Narrow(float64 Value)
			{
				return float32(Value);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdNormalizedFloatTemporary",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL float64-to-float32 conversion must Build through CodeGen")));

		asIScriptFunction* const NarrowFunction =
			AngelscriptNativeTestSupport::GetNativeFunctionByDecl(
				Module, "float32 Narrow(float64 Value)");
		ASSERT_THAT(IsNotNull(NarrowFunction,
			TEXT("float64-to-float32 conversion function must resolve")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(NarrowFunction, asBC_dTOf),
			TEXT("float64-to-float32 conversion must retain the dTOf VM opcode")));
		const asCScriptFunction* const InternalFunction =
			static_cast<const asCScriptFunction*>(NarrowFunction);
		bool bPublishedFloat32Temporary = false;
		if (InternalFunction != nullptr && InternalFunction->scriptData != nullptr)
		{
			for (asUINT Index = 0;
				Index < InternalFunction->scriptData->temporaryVariables.GetLength();
				++Index)
			{
				if (InternalFunction->scriptData->temporaryVariables[Index].Token
					== ttFloat32)
				{
					bPublishedFloat32Temporary = true;
					break;
				}
			}
		}
		ASSERT_THAT(IsTrue(
			bPublishedFloat32Temporary,
			TEXT("Canonical CodeGen must publish dTOf output as normalized ttFloat32, not an integer-sized source token")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("numeric-conversion Build must not fall back to asCCompiler")));
	}

	TEST_METHOD(CanonicalIntegerNarrowingPublishesNormalizedTemporaryTypes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("integer-narrowing fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			uint8 NarrowByte(int Value)
			{
				return uint8(Value);
			}

			uint16 NarrowWord(int Value)
			{
				return uint16(Value);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdNormalizedNarrowingTemporaries",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL integer-narrowing fixture must Build through CodeGen")));

		auto HasNormalizedDestination = [](asIScriptFunction* Function,
			asEBCInstr ExpectedOpcode, eTokenType ExpectedToken)
		{
			const asCScriptFunction* const Internal =
				static_cast<const asCScriptFunction*>(Function);
			if (Internal == nullptr || Internal->scriptData == nullptr)
				return false;
			const asCArray<asDWORD>& Bytecode = Internal->scriptData->byteCode;
			for (asUINT ProgramCounter = 0; ProgramCounter < Bytecode.GetLength();)
			{
				const asEBCInstr Opcode = static_cast<asEBCInstr>(
					*reinterpret_cast<const asBYTE*>(&Bytecode[ProgramCounter]));
				if (Opcode == ExpectedOpcode)
				{
					const int Destination = asBC_SWORDARG0(&Bytecode[ProgramCounter]);
					for (asUINT Index = 0;
						Index < Internal->scriptData->temporaryVariables.GetLength(); ++Index)
					{
						const auto& Variable =
							Internal->scriptData->temporaryVariables[Index];
						if (Variable.Offset == Destination
							&& Variable.Token == ExpectedToken)
						{
							return true;
						}
					}
				}
				ProgramCounter += asBCTypeSize[asBCInfo[Opcode].type];
			}
			return false;
		};

		asIScriptFunction* const NarrowByte = Module != nullptr
			? Module->GetFunctionByDecl("uint8 NarrowByte(int Value)") : nullptr;
		asIScriptFunction* const NarrowWord = Module != nullptr
			? Module->GetFunctionByDecl("uint16 NarrowWord(int Value)") : nullptr;
		ASSERT_THAT(IsTrue(
			HasNormalizedDestination(NarrowByte, asBC_iTOb, ttUInt8),
			TEXT("iTOb destination must be published as a one-byte Runtime/JIT slot")));
		ASSERT_THAT(IsTrue(
			HasNormalizedDestination(NarrowWord, asBC_iTOw, ttUInt16),
			TEXT("iTOw destination must be published as a two-byte Runtime/JIT slot")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("integer-narrowing Build must not fall back to asCCompiler")));
	}

	TEST_METHOD(CanonicalInRefBuildPublishesCodeGenAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("&in fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int InF(int &in x)
			{
				return x;
			}

			int F()
			{
				return InF(41) + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdInRef",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL &in Build must succeed from CodeGen")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL &in F() must execute from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL &in Build must not record asCCompiler")));
		asIScriptFunction* const Caller = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		asIScriptFunction* const Callee = Module != nullptr ? Module->GetFunctionByName("InF") : nullptr;
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Caller, asBC_PSF),
			TEXT("CANONICAL &in call must PSF the address of the int, not PshV* of the value")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Callee, asBC_RDR4),
			TEXT("CANONICAL &in callee must load-through the referenced int")));
	}

	TEST_METHOD(CanonicalOutRefBuildPublishesCodeGenAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("&out fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int OutF(int &out x)
			{
				x = 41;
				return 1;
			}

			int F()
			{
				int v = 0;
				OutF(v);
				return v + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdOutRef",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL &out Build must succeed from CodeGen")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL &out F() must execute write-back from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL &out Build must not record asCCompiler")));
		asIScriptFunction* const Caller = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		asIScriptFunction* const Callee = Module != nullptr ? Module->GetFunctionByName("OutF") : nullptr;
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Caller, asBC_PSF),
			TEXT("CANONICAL &out call must PSF the address of the local")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Callee, asBC_WRTV4),
			TEXT("CANONICAL &out callee must store-through the referenced int")));
	}

	TEST_METHOD(CanonicalPropertyOutRefExecutesSealedDeferredWriteBackExactlyOnce)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("property &out fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdDeferredPropertyOut", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FDeferredOutBox
			{
				int Value;
			}

			int Fill(int& out Target)
			{
				Target = 41;
				return 1;
			}

			int Entry()
			{
				FDeferredOutBox Box;
				return Fill(Box.Value) + Box.Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdDeferredPropertyOut.as",
			ScriptSource.c_str(),
			static_cast<asUINT>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("sealed property &out plan must lower through Canonical CodeGen")));

		asCModule* const Internal = static_cast<asCModule*>(Module);
		const asCASTContext* const Ast = Internal->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(Ast,
			TEXT("property &out Build must retain its sealed semantic plan")));
		const asCExpr* DeferredOut = nullptr;
		for (asUINT ExprIndex = 1;
			Ast != nullptr && ExprIndex <= Ast->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Candidate =
				Ast->GetExpr(asASTExprId(ExprIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_EXPR_DEFERRED_OUT)
			{
				DeferredOut = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(DeferredOut,
			TEXT("CodeGen input must contain the Sema-owned deferred-out plan")));
		ASSERT_THAT(IsTrue(DeferredOut != nullptr
			&& DeferredOut->receiver.IsValid()
			&& DeferredOut->children.GetLength() == 1
			&& DeferredOut->children[0] == DeferredOut->receiver,
			TEXT("the plan must preserve one opaque receiver edge")));

		asITypeInfo* const BoxType =
			Module->GetTypeInfoByName("FDeferredOutBox");
		asIScriptFunction* const Getter =
			BoxType != nullptr ? BoxType->GetMethodByName("GetValue") : nullptr;
		asIScriptFunction* const Setter =
			BoxType != nullptr ? BoxType->GetMethodByName("SetValue") : nullptr;
		asIScriptFunction* const Fill = Module->GetFunctionByName("Fill");
		asIScriptFunction* const Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Getter));
		ASSERT_THAT(IsNotNull(Setter));
		ASSERT_THAT(IsNotNull(Fill));
		ASSERT_THAT(IsNotNull(Entry));
		ASSERT_THAT(IsTrue(DeferredOut != nullptr && Setter != nullptr
			&& Ast->GetDecl(DeferredOut->resolvedDecl) != nullptr
			&& Ast->GetDecl(DeferredOut->resolvedDecl)->stableKey.Equals(
				"FDeferredOutBox::SetValue(int)"),
			TEXT("the call edge must own the exact sealed SetValue declaration")));
		ASSERT_THAT(IsTrue(Fill != nullptr && Entry != nullptr
			&& ProdBytecodeCallsFunction(Entry, Fill->GetId()),
			TEXT("Entry must call the authored out function")));
		ASSERT_THAT(IsTrue(Setter != nullptr && Entry != nullptr
			&& ProdCountBytecodeCallsFunction(Entry, Setter->GetId()) == 1,
			TEXT("deferred write-back must call the exact setter once")));
		ASSERT_THAT(IsTrue(Getter != nullptr && Entry != nullptr
			&& ProdCountBytecodeCallsFunction(Entry, Getter->GetId()) == 1,
			TEXT("the out path must not pre-read the property through its getter")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 42,
			TEXT("Fill must write 41 through the sealed setter before Entry returns")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Internal->GetLastLegacyCompilerInvocationCount()));
	}

	TEST_METHOD(CanonicalInRefScalarBinaryUsesPointeeWidth)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("reference scalar-binary fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int DoubleRef(int &in Value, int Factor)
			{
				return Value * Factor;
			}

			int F()
			{
				int Value = 21;
				return DoubleRef(Value, 2);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdInRefScalarBinary",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL reference scalar-binary Build must succeed from CodeGen")));

		asIScriptFunction* const Callee = Module != nullptr
			? Module->GetFunctionByName("DoubleRef") : nullptr;
		ASSERT_THAT(IsNotNull(Callee));
		asCString CalleeDump;
		asCBytecodeCodeGenDumpFunction(
			static_cast<asCScriptFunction*>(Callee), CalleeDump);
		const FString CalleeDumpText = UTF8_TO_TCHAR(
			CalleeDump.AddressOf() != nullptr ? CalleeDump.AddressOf() : "");
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Callee, asBC_MULi)
				&& !AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Callee, asBC_MULi64),
			*FString::Printf(
				TEXT("an int reference read must select 32-bit pointee arithmetic, not the 64-bit reference storage width; bytecode={%s}"),
				*CalleeDumpText)));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL reference scalar-binary execution must preserve pointee value semantics")));
	}

	TEST_METHOD(CanonicalWhileSuspendBuildPublishesCodeGenAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("while-suspend fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				int Y = 0;
				while (Y < 42)
				{
					Y = Y + 1;
				}
				return Y;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdWhileSuspend",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL while Build must succeed from CodeGen")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL while F() must execute from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL while Build must not record asCCompiler")));
		asIScriptFunction* const Function = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Function, asBC_SUSPEND),
			TEXT("CANONICAL while must emit asBC_SUSPEND on the production publisher")));
	}

	TEST_METHOD(CanonicalSelfHandleReturnPublishesPointerTemporaryMetadata)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class SelfReturn
			{
				SelfReturn ReturnSelf()
				{
					return this;
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdSelfHandleReturn",
			ScriptSource.c_str(),
			Module),
			TEXT("self-handle return fixture must build through Canonical CodeGen")));

		asITypeInfo* const SelfType = Module != nullptr
			? Module->GetTypeInfoByName("SelfReturn") : nullptr;
		asIScriptFunction* const ReturnSelf = SelfType != nullptr
			? SelfType->GetMethodByName("ReturnSelf") : nullptr;
		ASSERT_THAT(IsNotNull(ReturnSelf));
		if (ReturnSelf == nullptr)
		{
			return;
		}
		asCScriptFunction* const InternalReturnSelf =
			static_cast<asCScriptFunction*>(ReturnSelf);

		int32 ReturnCopySlot = INDEX_NONE;
		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = ReturnSelf != nullptr
			? ReturnSelf->GetByteCode(&BytecodeLength) : nullptr;
		for (asUINT DwordIndex = 0;
			Bytecode != nullptr && DwordIndex < BytecodeLength;)
		{
			const asDWORD* const Instr = Bytecode + DwordIndex;
			const asEBCInstr Opcode = static_cast<asEBCInstr>(
				*reinterpret_cast<const asBYTE*>(Instr));
			if (Opcode == asBC_CpyVtoV8)
			{
				ReturnCopySlot = static_cast<int32>(asBC_SWORDARG0(Instr));
				break;
			}
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				break;
			}
			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0)
			{
				break;
			}
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		asCString ReturnDump;
		asCBytecodeCodeGenDumpFunction(InternalReturnSelf, ReturnDump);
		const FString ReturnDumpText = UTF8_TO_TCHAR(
			ReturnDump.AddressOf() != nullptr ? ReturnDump.AddressOf() : "");
		ASSERT_THAT(IsTrue(
			ReturnCopySlot > 0
				&& InternalReturnSelf != nullptr
				&& InternalReturnSelf->scriptData != nullptr
				&& InternalReturnSelf->scriptData->pointerVariablePos.Exists(ReturnCopySlot),
			*FString::Printf(
				TEXT("the pointer-sized self return slot must be published as pointer frame metadata; slot=%d bytecode={%s}"),
				ReturnCopySlot,
				*ReturnDumpText)));
	}

	TEST_METHOD(CanonicalForeachExecutesSealedProtocolWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("foreach protocol execution selects the production CANONICAL pipeline")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FCanonicalForeachRange
			{
				int opForBegin()
				{
					return 0;
				}

				bool opForEnd(const int Iterator)
				{
					return Iterator >= 2;
				}

				void opForNext(int& inout Iterator)
				{
					Iterator = Iterator + 1;
				}

				int opForValue(const int Iterator)
				{
					return 20 + Iterator;
				}
			}

			int CanonicalForeachSum()
			{
				FCanonicalForeachRange Range;
				int Sum = 0;
				foreach (int Value : Range)
				{
					Sum += Value;
				}
				return Sum;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdCanonicalForeach",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL foreach Build must consume the protocol already sealed by Sema")));
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner,
				Engine.Get(),
				Module,
				"int CanonicalForeachSum()",
				Value)
				&& Value == 41,
			TEXT("CANONICAL foreach must execute opForValue(0)+opForValue(1)==41")));
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("foreach must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("foreach must never call the retained LEGACY compiler")));
	}

	TEST_METHOD(CanonicalObjectIteratorForeachExecutesSealedScopeExitCleanup)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("object-iterator foreach selects the production CANONICAL pipeline")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdCanonicalObjectIteratorForeach", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FCanonicalForeachIterator
			{
				int Index;
			}

			struct FCanonicalObjectForeachRange
			{
				FCanonicalForeachIterator opForBegin()
				{
					FCanonicalForeachIterator Iterator;
					Iterator.Index = 0;
					return Iterator;
				}

				bool opForEnd(const FCanonicalForeachIterator& Iterator)
				{
					return Iterator.Index >= 2;
				}

				void opForNext(FCanonicalForeachIterator& inout Iterator)
				{
					Iterator.Index = Iterator.Index + 1;
				}

				int opForValue(const FCanonicalForeachIterator& Iterator)
				{
					return 20 + Iterator.Index;
				}
			}

			int AfterCanonicalForeach(int Value)
			{
				return Value + 1;
			}

			int CanonicalObjectForeachBreak()
			{
				FCanonicalObjectForeachRange Range;
				int Sum = 0;
				foreach (int Value : Range)
				{
					Sum += Value;
					break;
				}
				return AfterCanonicalForeach(Sum);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdCanonicalObjectIteratorForeach.as", ScriptSource.c_str())));
		const int BuildResult = Module->Build();
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Context =
			static_cast<asCModule*>(Module)->GetCanonicalASTContext())
		{
			asCString RawDump;
			asCASTDump(*Context, RawDump);
			AstDump = CanonicalAstDumpToFString(RawDump);
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("CANONICAL object-iterator foreach must consume the sealed lifetime plan. AST:\n%s"),
				*AstDump)));
		if (BuildResult != 0)
		{
			return;
		}

		asIScriptFunction* const Published = GetNativeFunctionByDecl(
			Module, "int CanonicalObjectForeachBreak()");
		asIScriptFunction* const After = GetNativeFunctionByDecl(
			Module, "int AfterCanonicalForeach(int)");
		asITypeInfo* const IteratorType = Module->GetTypeInfoByName(
			"FCanonicalForeachIterator");
		asIScriptFunction* Destructor = nullptr;
		if (IteratorType != nullptr)
		{
			for (asUINT BehaviourIndex = 0;
				BehaviourIndex < IteratorType->GetBehaviourCount(); ++BehaviourIndex)
			{
				asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
				asIScriptFunction* const Candidate = IteratorType->GetBehaviourByIndex(
					BehaviourIndex, &Behaviour);
				if (Candidate != nullptr && Behaviour == asBEHAVE_DESTRUCT)
				{
					Destructor = Candidate;
					break;
				}
			}
		}
		ASSERT_THAT(IsNotNull(Published));
		ASSERT_THAT(IsNotNull(After));
		ASSERT_THAT(IsNotNull(Destructor,
			TEXT("the sealed cleanup must name the generated iterator destructor")));
		const int32 DestructorOffset = ProdBytecodeFirstCallOffset(
			Published, Destructor != nullptr ? Destructor->GetId() : 0);
		const int32 AfterOffset = ProdBytecodeFirstCallOffset(
			Published, After != nullptr ? After->GetId() : 0);
		ASSERT_THAT(IsTrue(
			DestructorOffset != INDEX_NONE && AfterOffset != INDEX_NONE
				&& DestructorOffset < AfterOffset,
			*FString::Printf(
				TEXT("break must run the exact foreach cleanup before evaluating the following call; dtor=%d after=%d calls={%s}"),
				DestructorOffset,
				AfterOffset,
				*ProdBytecodeCallTargets(Published))));

		int32 Value = 0;
		FString Exception;
		const int ExecuteStatus = CanonicalExecuteIntStatus(
			Engine.Get(), Module, "int CanonicalObjectForeachBreak()", Value, Exception);
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Value == 21,
			*FString::Printf(
				TEXT("object-iterator foreach must execute the first value, break, clean up, then return 21; status=%d got=%d exception=%s"),
				ExecuteStatus, Value, *Exception)));
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("object-iterator foreach must never call the retained LEGACY compiler")));
	}

	TEST_METHOD(CanonicalKeyedForeachExecutesSealedKeyProtocolWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("keyed foreach execution selects the production CANONICAL pipeline")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FCanonicalKeyedForeachRange
			{
				int opForBegin()
				{
					return 0;
				}

				bool opForEnd(const int Iterator)
				{
					return Iterator >= 2;
				}

				void opForNext(int& inout Iterator)
				{
					Iterator = Iterator + 1;
				}

				int opForValue(const int Iterator)
				{
					return 2 + Iterator;
				}

				int opForKey(const int Iterator)
				{
					return 7 + Iterator;
				}
			}

			int CanonicalKeyedForeachSum()
			{
				FCanonicalKeyedForeachRange Range;
				int Sum = 0;
				foreach (int Value, int Key : Range)
				{
					Sum += Value * 10 + Key;
				}
				return Sum;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdCanonicalKeyedForeach",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL keyed foreach Build must consume the Sema-sealed key protocol")));
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner,
				Engine.Get(),
				Module,
				"int CanonicalKeyedForeachSum()",
				Value)
				&& Value == 65,
			TEXT("CANONICAL keyed foreach must execute (2*10+7)+(3*10+8)==65")));
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("keyed foreach must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("keyed foreach must never call the retained LEGACY compiler")));
	}

	TEST_METHOD(CanonicalHandleNullCheckBuildPublishesCodeGenAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType("CObj", 0, asOBJ_REF) >= 0
			&& ScriptEngine->RegisterObjectBehaviour("CObj", asBEHAVE_ADDREF, "void f()", asFUNCTION(ProdCanonicalHandleAddRef), asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectBehaviour("CObj", asBEHAVE_RELEASE, "void f()", asFUNCTION(ProdCanonicalHandleRelease), asCALL_GENERIC) >= 0,
			TEXT("handle fixture registers a host REF type")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("handle fixture selects CANONICAL")));
		asIScriptModule* Module = ScriptEngine->GetModule("ProdHandle", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("handle fixture needs a module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT),
			TEXT("retain sealed AST so the empty-publish dump can name intern vs owner")));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				CObj Obj = nullptr;
				if (Obj == nullptr)
				{
					return 42;
				}
				return 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdHandle",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size())),
			TEXT("handle fixture must add the section")));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("CANONICAL handle Build must succeed from CodeGen")));
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString InternalFunctions;
		for (asUINT FunctionIndex = 0; FunctionIndex < InternalModule->scriptFunctions.GetLength(); ++FunctionIndex)
		{
			asCScriptFunction* const InternalFunction = InternalModule->scriptFunctions[FunctionIndex];
			if (InternalFunction == nullptr)
			{
				continue;
			}
			if (!InternalFunctions.IsEmpty())
			{
				InternalFunctions += TEXT(", ");
			}
			asITypeInfo* const OwnerType = InternalFunction->GetObjectType();
			InternalFunctions += FString::Printf(
				TEXT("%s objectType=%s"),
				UTF8_TO_TCHAR(InternalFunction->GetDeclaration() ? InternalFunction->GetDeclaration() : "<anon>"),
				OwnerType && OwnerType->GetName() ? UTF8_TO_TCHAR(OwnerType->GetName()) : TEXT("<global>"));
		}
		if (InternalFunctions.IsEmpty())
		{
			InternalFunctions = TEXT("<no scriptFunctions>");
		}
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(
			Published,
			*FString::Printf(
				TEXT("CANONICAL handle module must publish int F(); have {%s}; scriptFunctions {%s}; ast {%s}"),
				*AngelscriptNativeTestSupport::CollectFunctionDeclarations(Module),
				*InternalFunctions,
				*AstDump)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL handle F() must execute from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL handle Build must not record asCCompiler")));
		asIScriptFunction* const Function = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Function, asBC_CmpPtrNull),
			TEXT("CANONICAL handle null check must emit CmpPtrNull")));
	}

	TEST_METHOD(CanonicalImplicitHandleCallResultsAreReleasedAfterExpressionUse)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		FProdTrackedHandleState State;
		GProdTrackedHandleState = &State;
		ON_SCOPE_EXIT
		{
			// Keep a RED run from leaking native test memory. This executes only
			// after the observable LiveObjects assertion has caught a missing VM
			// cleanup, and is not part of the production behavior under test.
			while (State.LastObject != nullptr && State.LastObject->ReferenceCount > 0)
			{
				State.LastObject->Release();
			}
			GProdTrackedHandleState = nullptr;
		};

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"CTrackedHandle", 0, asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"CTrackedHandle",
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(ProdTrackedHandleAddRef),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"CTrackedHandle",
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(ProdTrackedHandleRelease),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectMethod(
				"CTrackedHandle",
				"CTrackedHandle Step()",
				asFUNCTION(ProdTrackedHandleStep),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectMethod(
				"CTrackedHandle",
				"CTrackedHandle opIndex(int)",
				asFUNCTION(ProdTrackedHandleStep),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"CTrackedHandle", "int Value", asOFFSET(FProdTrackedHandle, Value)) >= 0
			&& ScriptEngine->RegisterGlobalFunction(
				"CTrackedHandle MakeTrackedHandle()",
				asFUNCTION(ProdTrackedHandleFactory),
				asCALL_GENERIC) >= 0,
			TEXT("tracked implicit-handle fixture must register its native surface")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("tracked implicit-handle fixture selects CANONICAL")));

		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdTrackedImplicitHandle", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("tracked implicit-handle module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT),
			TEXT("tracked implicit-handle test retains its sealed AST gate")));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return MakeTrackedHandle().Step()[1].Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdTrackedImplicitHandle.as",
			ScriptSource.c_str(),
			static_cast<asUINT>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("tracked implicit-handle expression must compile through Canonical CodeGen")));

		const asCASTContext* const Context =
			static_cast<asCModule*>(Module)->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("tracked implicit-handle AST must be retained")));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Context->IsPublishable(),
			TEXT("CodeGen may consume only the sealed tracked-handle AST")));
		const FCanonicalASTMatch NativeClass =
			CanonicalASTUniqueDecl(*Context, "CTrackedHandle");
		ASSERT_THAT(IsTrue(NativeClass.IsMatch(), *NativeClass.FailureMessage));
		const asCDecl* const NativeClassDecl = Context->GetDecl(
			asASTDeclId(NativeClass.Node.value, NativeClass.Node.snapshotOwner));
		ASSERT_THAT(IsTrue(
			NativeClassDecl != nullptr
				&& NativeClassDecl->origin.Equals("canonical-native-type-view"),
			TEXT("the sealed host class projection must retain external ABI layout ownership")));
		const FCanonicalASTMatch NativeValue =
			CanonicalASTUniqueDecl(*Context, "CTrackedHandle::Value");
		ASSERT_THAT(IsTrue(NativeValue.IsMatch(), *NativeValue.FailureMessage));
		const asCDecl* const NativeValueDecl = Context->GetDecl(
			asASTDeclId(NativeValue.Node.value, NativeValue.Node.snapshotOwner));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asOFFSET(FProdTrackedHandle, Value)),
			NativeValueDecl != nullptr ? NativeValueDecl->byteOffset : -1,
			TEXT("class layout sealing must preserve the registered host-property byte offset")));
		const FCanonicalASTMatch FactoryCall =
			CanonicalASTResolvedCallee(*Context, "MakeTrackedHandle()");
		ASSERT_THAT(IsTrue(FactoryCall.IsMatch(), *FactoryCall.FailureMessage));
		const FCanonicalASTMatch FactoryType = CanonicalASTExactType(
			*Context,
			FactoryCall.Node,
			"CTrackedHandle",
			asAST_QUAL_HANDLE);
		ASSERT_THAT(IsTrue(FactoryType.IsMatch(), *FactoryType.FailureMessage));
		const FCanonicalASTMatch StepCall =
			CanonicalASTResolvedCallee(*Context, "CTrackedHandle::Step()");
		ASSERT_THAT(IsTrue(StepCall.IsMatch(), *StepCall.FailureMessage));
		const FCanonicalASTMatch StepType = CanonicalASTExactType(
			*Context,
			StepCall.Node,
			"CTrackedHandle",
			asAST_QUAL_HANDLE);
		ASSERT_THAT(IsTrue(StepType.IsMatch(), *StepType.FailureMessage));
		const FCanonicalASTMatch Receiver = CanonicalASTReceiver(
			*Context,
			asASTExprId(StepCall.Node.value, StepCall.Node.snapshotOwner),
			asAST_EXPR_MATERIALIZE_TEMPORARY);
		ASSERT_THAT(IsTrue(Receiver.IsMatch(), *Receiver.FailureMessage));
		const FCanonicalASTMatch MaterializedFactory = CanonicalASTNamedEdge(
			*Context, Receiver.Related, asAST_EDGE_EXPR_CHILD, 0);
		ASSERT_THAT(IsTrue(MaterializedFactory.IsMatch(), *MaterializedFactory.FailureMessage));
		ASSERT_THAT(IsTrue(MaterializedFactory.Related == FactoryCall.Node,
			TEXT("Step receiver materialization must own the sealed factory-call result")));
		const FCanonicalASTMatch IndexCall =
			CanonicalASTResolvedCallee(*Context, "CTrackedHandle::opIndex(int)");
		ASSERT_THAT(IsTrue(IndexCall.IsMatch(), *IndexCall.FailureMessage));
		const FCanonicalASTMatch IndexType = CanonicalASTExactType(
			*Context,
			IndexCall.Node,
			"CTrackedHandle",
			asAST_QUAL_HANDLE);
		ASSERT_THAT(IsTrue(IndexType.IsMatch(), *IndexType.FailureMessage));
		asCString AstTree;
		asSASTStructuredDumpOptions AstDumpOptions;
		ASSERT_THAT(AreEqual(0, asCASTStructuredDump(*Context, AstDumpOptions, AstTree),
			TEXT("tracked implicit-handle AST tree must be available for lifecycle diagnostics")));

		asIScriptFunction* const Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Entry, TEXT("tracked implicit-handle Entry publication")));
		asCString EntryDump;
		asCBytecodeCodeGenDumpFunction(static_cast<asCScriptFunction*>(Entry), EntryDump);
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_STOREOBJ),
			*FString::Printf(
				TEXT("owning implicit-handle call returns must move objectRegister into a tracked slot; bytecode {%s}"),
				UTF8_TO_TCHAR(EntryDump.AddressOf() ? EntryDump.AddressOf() : ""))));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_FREE),
			*FString::Printf(
				TEXT("every owning implicit-handle call result must enter VM cleanup tracking; bytecode {%s}"),
				UTF8_TO_TCHAR(EntryDump.AddressOf() ? EntryDump.AddressOf() : ""))));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int Entry()", Value) && Value == 42,
			*FString::Printf(
				TEXT("tracked implicit-handle chain must execute through Canonical CodeGen; value=%d created=%d addref=%d release=%d destroyed=%d live=%d bytecode {%s} ast {%s}"),
				Value,
				State.Created,
				State.AddRefCalls,
				State.ReleaseCalls,
				State.Destroyed,
				State.LiveObjects,
				UTF8_TO_TCHAR(EntryDump.AddressOf() ? EntryDump.AddressOf() : ""),
				UTF8_TO_TCHAR(AstTree.AddressOf() ? AstTree.AddressOf() : ""))));
		ASSERT_THAT(IsTrue(
			State.Created == 1
			&& State.AddRefCalls == 2
			&& State.ReleaseCalls == 3
			&& State.Destroyed == 1
			&& State.LiveObjects == 0,
			*FString::Printf(
				TEXT("one factory + Step + opIndex owning results must balance exactly; created=%d addref=%d release=%d destroyed=%d live=%d bytecode {%s} ast {%s}"),
				State.Created,
				State.AddRefCalls,
				State.ReleaseCalls,
				State.Destroyed,
				State.LiveObjects,
				UTF8_TO_TCHAR(EntryDump.AddressOf() ? EntryDump.AddressOf() : ""),
				UTF8_TO_TCHAR(AstTree.AddressOf() ? AstTree.AddressOf() : ""))));
		ASSERT_THAT(AreEqual(1, State.Created,
			TEXT("factory must create exactly one tracked native object")));
		ASSERT_THAT(AreEqual(2, State.AddRefCalls,
			TEXT("Step and opIndex must each publish one additional owning handle")));
		ASSERT_THAT(AreEqual(3, State.ReleaseCalls,
			TEXT("factory, Step, and opIndex results must each release their owned handle")));
		ASSERT_THAT(AreEqual(1, State.Destroyed,
			TEXT("tracked native object must be destroyed after expression use")));
		ASSERT_THAT(AreEqual(0, State.LiveObjects,
			TEXT("implicit-handle expression cleanup must leave no native object live")));
	}

	TEST_METHOD(CanonicalImplicitHandleReceiverIsReleasedDuringNativeExceptionUnwind)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		FProdTrackedHandleState State;
		GProdTrackedHandleState = &State;
		ON_SCOPE_EXIT
		{
			// Preserve the failing counts while ensuring a RED run cannot leak the
			// native fixture beyond this test process.
			while (State.LastObject != nullptr && State.LastObject->ReferenceCount > 0)
			{
				State.LastObject->Release();
			}
			GProdTrackedHandleState = nullptr;
		};

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"CTrackedHandle", 0, asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"CTrackedHandle",
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(ProdTrackedHandleAddRef),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"CTrackedHandle",
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(ProdTrackedHandleRelease),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectMethod(
				"CTrackedHandle",
				"CTrackedHandle StepCall()",
				asFUNCTION(ProdTrackedHandleThrow),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectMethod(
				"CTrackedHandle",
				"CTrackedHandle GetMember()",
				asFUNCTION(ProdTrackedHandleStep),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"CTrackedHandle", "int Value", asOFFSET(FProdTrackedHandle, Value)) >= 0
			&& ScriptEngine->RegisterGlobalFunction(
				"CTrackedHandle MakeTrackedHandle()",
				asFUNCTION(ProdTrackedHandleFactory),
				asCALL_GENERIC) >= 0,
			TEXT("exceptional implicit-handle fixture must register its native surface")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("exceptional implicit-handle fixture selects CANONICAL")));

		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdTrackedImplicitHandleException", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("exceptional implicit-handle module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT),
			TEXT("exceptional implicit-handle test retains its sealed AST gate")));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				int Result = MakeTrackedHandle().StepCall().GetMember().Value;
				return Result;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdTrackedImplicitHandleException.as",
			ScriptSource.c_str(),
			static_cast<asUINT>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("exceptional implicit-handle expression must compile through Canonical CodeGen")));

		const asCASTContext* const AstContext =
			static_cast<asCModule*>(Module)->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(AstContext, TEXT("exceptional implicit-handle AST must be retained")));
		if (AstContext == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AstContext->IsPublishable(),
			TEXT("exception cleanup CodeGen may consume only the sealed AST")));
		const FCanonicalASTMatch ThrowCall =
			CanonicalASTResolvedCallee(*AstContext, "CTrackedHandle::StepCall()");
		ASSERT_THAT(IsTrue(ThrowCall.IsMatch(), *ThrowCall.FailureMessage));
		const FCanonicalASTMatch Receiver = CanonicalASTReceiver(
			*AstContext,
			asASTExprId(ThrowCall.Node.value, ThrowCall.Node.snapshotOwner),
			asAST_EXPR_MATERIALIZE_TEMPORARY);
		ASSERT_THAT(IsTrue(Receiver.IsMatch(), *Receiver.FailureMessage));

		asIScriptFunction* const Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Entry, TEXT("exceptional implicit-handle Entry publication")));
		asCString EntryDump;
		asCScriptFunction* const InternalEntry = static_cast<asCScriptFunction*>(Entry);
		asCBytecodeCodeGenDumpFunction(InternalEntry, EntryDump);
		if (InternalEntry != nullptr && InternalEntry->scriptData != nullptr)
		{
			EntryDump += "lifecycle=";
			for (asUINT InfoIndex = 0;
				InfoIndex < InternalEntry->scriptData->objVariableInfo.GetLength();
				++InfoIndex)
			{
				const asSObjectVariableInfo& Info =
					InternalEntry->scriptData->objVariableInfo[InfoIndex];
				asCString InfoText;
				InfoText.Format(" pos=%u offset=%d option=%d",
					Info.programPos,
					Info.variableOffset,
					static_cast<int>(Info.option));
				EntryDump += InfoText;
			}
			EntryDump += "\n";
		}
		asIScriptContext* const RuntimeContext = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(RuntimeContext, TEXT("exceptional implicit-handle runtime context")));
		if (RuntimeContext == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { RuntimeContext->Release(); };
		ASSERT_THAT(AreEqual(asSUCCESS, RuntimeContext->Prepare(Entry)));
		const int ExecuteStatus = RuntimeContext->Execute();
		const char* ExceptionSection = nullptr;
		int ExceptionColumn = 0;
		const int ExceptionLine = RuntimeContext->GetExceptionLineNumber(
			&ExceptionColumn, &ExceptionSection);
		const FString ExceptionSectionName = UTF8_TO_TCHAR(
			ExceptionSection != nullptr ? ExceptionSection : "");
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, ExecuteStatus,
			TEXT("native Throw must terminate Entry with a VM exception")));
		ASSERT_THAT(AreEqual(asSUCCESS, RuntimeContext->Unprepare(),
			TEXT("exceptional implicit-handle context must unwind and unprepare")));
		ASSERT_THAT(IsTrue(
			ExceptionLine > 0
			&& ExceptionColumn > 0
			&& ExceptionSectionName == TEXT("ProdTrackedImplicitHandleException.as"),
			*FString::Printf(
				TEXT("Canonical exception metadata must retain its exact source location; section=%s line=%d column=%d"),
				*ExceptionSectionName,
				ExceptionLine,
				ExceptionColumn)));
		ASSERT_THAT(IsTrue(
			State.Created == 1
			&& State.AddRefCalls == 0
			&& State.ReleaseCalls == 1
			&& State.Destroyed == 1
			&& State.LiveObjects == 0,
			*FString::Printf(
				TEXT("exception unwind must release the materialized receiver when the first handle-returning chain method throws; created=%d addref=%d release=%d destroyed=%d live=%d bytecode {%s}"),
				State.Created,
				State.AddRefCalls,
				State.ReleaseCalls,
				State.Destroyed,
				State.LiveObjects,
				UTF8_TO_TCHAR(EntryDump.AddressOf() ? EntryDump.AddressOf() : ""))));
	}

	TEST_METHOD(CanonicalGeneratedDefaultCtorBuildPublishesCodeGenAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("generated-default-ctor fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FValue
			{
				int Value = 41;
			}

			int F()
			{
				FValue Object;
				return Object.Value + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdGenDefaultCtor",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL generated default ctor Build must succeed from CodeGen")));
		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(
			Published,
			*FString::Printf(
				TEXT("CANONICAL generated-default-ctor module must publish int F(); have {%s}"),
				*AngelscriptNativeTestSupport::CollectFunctionDeclarations(Module))));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL generated default ctor F() must execute from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL generated default ctor Build must not record asCCompiler")));
		asITypeInfo* const TypeInfo = Module != nullptr ? Module->GetTypeInfoByName("FValue") : nullptr;
		asIScriptFunction* Ctor = nullptr;
		if (TypeInfo != nullptr)
		{
			for (asUINT BehaviourIndex = 0; BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
			{
				asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
				asIScriptFunction* const Candidate = TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
				if (Candidate != nullptr && Behaviour == asBEHAVE_CONSTRUCT)
				{
					Ctor = Candidate;
					break;
				}
			}
		}
		ASSERT_THAT(IsNotNull(Ctor, TEXT("generated default ctor must be installed on FValue, not only int F()")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Ctor, asBC_WRTV4),
			TEXT("generated default ctor must write Value = 41, not RET an uninitialized local")));
	}

	TEST_METHOD(CanonicalHostFuncdefBuildPublishesCodeGenAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterFuncdef("int Callback(int)") >= 0,
			TEXT("host funcdef fixture registers Callback")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("host funcdef fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Double(int X)
			{
				return X + X;
			}

			int Invoke(Callback Cb, int X)
			{
				return Cb(X);
			}

			int InvokeLocal()
			{
				Callback Cb = Double;
				return Cb(21);
			}

			int F()
			{
				return Invoke(Double, 21);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdHostFuncdef",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL host funcdef Build must succeed from CodeGen")));
		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(
			Published,
			*FString::Printf(
				TEXT("CANONICAL host funcdef module must publish int F(); have {%s}"),
				*AngelscriptNativeTestSupport::CollectFunctionDeclarations(Module))));
		asIScriptFunction* Invoke = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int Invoke(Callback, const int)");
		if (Invoke == nullptr)
		{
			Invoke = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int Invoke(Callback@, const int)");
		}
		if (Invoke == nullptr)
		{
			Invoke = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int Invoke(Callback@+, const int)");
		}
		ASSERT_THAT(IsNotNull(Invoke, TEXT("host funcdef Invoke must be published")));
		asIScriptFunction* const Double = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int Double(const int)");
		ASSERT_THAT(IsNotNull(Double, TEXT("host funcdef Double must be published")));
		asIScriptFunction* const InvokeLocal =
			AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int InvokeLocal()");
		ASSERT_THAT(IsNotNull(
			InvokeLocal,
			TEXT("host funcdef local-variable caller must be published")));
		asCString FDump;
		asCString InvokeDump;
		asCString InvokeLocalDump;
		asCString DoubleDump;
		asCBytecodeCodeGenDumpFunction(static_cast<asCScriptFunction*>(Published), FDump);
		asCBytecodeCodeGenDumpFunction(static_cast<asCScriptFunction*>(Invoke), InvokeDump);
		asCBytecodeCodeGenDumpFunction(
			static_cast<asCScriptFunction*>(InvokeLocal), InvokeLocalDump);
		asCBytecodeCodeGenDumpFunction(static_cast<asCScriptFunction*>(Double), DoubleDump);
		int32 Value = 0;
		const bool Executed = AngelscriptNativeTestSupport::CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int F()", Value);
		ASSERT_THAT(IsTrue(Executed && Value == 42,
			*FString::Printf(
				TEXT("CANONICAL host funcdef F() must execute from CodeGen; executed=%d value=%d\nF:\n%s\nInvoke:\n%s\nDouble:\n%s"),
				Executed ? 1 : 0,
				Value,
				UTF8_TO_TCHAR(FDump.AddressOf() ? FDump.AddressOf() : ""),
				UTF8_TO_TCHAR(InvokeDump.AddressOf() ? InvokeDump.AddressOf() : ""),
				UTF8_TO_TCHAR(DoubleDump.AddressOf() ? DoubleDump.AddressOf() : ""))));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL host funcdef Build must not record asCCompiler")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Invoke, asBC_CallPtr),
			TEXT("Invoke(Callback) must emit CallPtr, not a missing callee")));
		int32 LocalValue = 0;
		const bool LocalExecuted = AngelscriptNativeTestSupport::CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int InvokeLocal()", LocalValue);
		ASSERT_THAT(IsTrue(
			LocalExecuted && LocalValue == 42,
			*FString::Printf(
				TEXT("CANONICAL local funcdef variable must execute its sealed indirect Call; executed=%d value=%d\nInvokeLocal:\n%s"),
				LocalExecuted ? 1 : 0,
				LocalValue,
				UTF8_TO_TCHAR(InvokeLocalDump.AddressOf()
					? InvokeLocalDump.AddressOf() : ""))));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(
				InvokeLocal, asBC_CallPtr),
			TEXT("local Callback Cb = Double; Cb(21) must emit CallPtr")));
	}

	TEST_METHOD(CanonicalStoredNonCapturingLambdaBuildPublishesCodeGenAndExecutes)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterFuncdef("int StoredLambdaCallback()") >= 0,
			TEXT("stored-lambda fixture registers its host funcdef")));
		asCTypeInfo* const CallbackType = static_cast<asCTypeInfo*>(
			ScriptEngine->GetTypeInfoByName("StoredLambdaCallback"));
		ASSERT_THAT(IsNotNull(CallbackType,
			TEXT("stored-lambda fixture resolves its host funcdef type")));
		if (CallbackType == nullptr)
		{
			return;
		}
		CallbackType->flags |= asOBJ_IMPLICIT_HANDLE;
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("stored-lambda fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				StoredLambdaCallback Callback = function()
				{
					return 42;
				};
				return Callback();
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdStoredLambda",
			ScriptSource.c_str(),
			Module),
			*FString::Printf(
				TEXT("a non-capturing lambda must be storable in a funcdef handle; messages:\n%s"),
				*CollectMessages(Engine.GetMessages()))));
		asIScriptFunction* const Published = GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(Published,
			TEXT("stored non-capturing lambda must publish int F()")));
		int32 Value = 0;
		const bool bExecuted = CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int F()", Value);
		ASSERT_THAT(IsTrue(bExecuted && Value == 42,
			*FString::Printf(
				TEXT("stored non-capturing lambda must execute through its funcdef handle; executed=%d value=%d"),
				bExecuted ? 1 : 0,
				Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("stored non-capturing lambda must publish only Canonical CodeGen bytecode")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Published, asBC_CallPtr),
			TEXT("Callback() must emit CallPtr after storing the anonymous function")));
	}

	TEST_METHOD(CanonicalStoredCapturingLambdaRejectedBeforePublication)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterFuncdef("int StoredCaptureCallback()") >= 0,
			TEXT("stored-capture rejection fixture registers its host funcdef")));
		asCTypeInfo* const CallbackType = static_cast<asCTypeInfo*>(
			ScriptEngine->GetTypeInfoByName("StoredCaptureCallback"));
		ASSERT_THAT(IsNotNull(CallbackType,
			TEXT("stored-capture rejection fixture resolves its host funcdef type")));
		if (CallbackType == nullptr)
		{
			return;
		}
		CallbackType->flags |= asOBJ_IMPLICIT_HANDLE;
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("stored-capture rejection fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				int X = 42;
				StoredCaptureCallback Callback = function()
				{
					return X;
				};
				return Callback();
			}
			)AS");
		const int BuildResult = CompileNativeModule(
			Engine.Get(),
			"ProdStoredCaptureReject",
			ScriptSource.c_str(),
			Module);
		const FString Messages = CollectMessages(Engine.GetMessages());
		ASSERT_THAT(IsTrue(BuildResult < 0,
			*FString::Printf(
				TEXT("captured lambda escape must fail before CodeGen publication; result=%d messages:\n%s"),
				BuildResult,
				*Messages)));
		ASSERT_THAT(IsTrue(Messages.Contains(TEXT("capturing-lambda-cannot-escape")),
			*FString::Printf(
				TEXT("capture escape must report the stable Sema diagnostic, not a late signature accident; messages:\n%s"),
				*Messages)));
		if (Module != nullptr)
		{
			ASSERT_THAT(AreNotEqual(
				asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
				static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
				TEXT("rejected captured-lambda escape must publish no Canonical candidate")));
			ASSERT_THAT(AreNotEqual(
				asBYTECODE_PUBLISHER_COMPILER,
				static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
				TEXT("rejected captured-lambda escape must not fall back to asCCompiler")));
		}
	}

	TEST_METHOD(CanonicalScriptFuncdefRejectedDoesNotPublishCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("script funcdef reject selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			funcdef int Cb(int);
			int F()
			{
				return 1;
			}
			)AS");
		const int BuildResult = AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdScriptFuncdef",
			ScriptSource.c_str(),
			Module);
		ASSERT_THAT(IsTrue(BuildResult < 0, TEXT("script funcdef keyword must stay rejected")));
		if (Module != nullptr)
		{
			ASSERT_THAT(AreNotEqual(
				asBYTECODE_PUBLISHER_COMPILER,
				static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
				TEXT("rejected script funcdef must not launder asCCompiler")));
		}
	}

	TEST_METHOD(CanonicalArrayIntBuildPublishesCodeGenAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"array<class T>",
				0,
				asOBJ_REF | asOBJ_TEMPLATE | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("array fixture registers host array<T> template")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"array<T>",
				asBEHAVE_FACTORY,
				"array<T> f(int&in)",
				asFUNCTION(ProdCanonicalArrayFactory),
				asCALL_GENERIC) >= 0,
			TEXT("array fixture registers default factory without @ (token disabled)")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectMethod(
				"array<T>",
				"void insertLast(const T&in value)",
				asFUNCTION(ProdCanonicalArrayInsertLast),
				asCALL_GENERIC) >= 0,
			TEXT("array fixture registers insertLast")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectMethod(
				"array<T>",
				"T &opIndex(uint index)",
				asFUNCTION(ProdCanonicalArrayOpIndex),
				asCALL_GENERIC) >= 0,
			TEXT("array fixture registers opIndex")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("array fixture selects CANONICAL")));
		asIScriptModule* Module = ScriptEngine->GetModule("ProdArrayInt", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("array fixture needs a module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT),
			TEXT("retain sealed AST so a failed array Build can dump CALL callee")));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				array<int> Values;
				Values.insertLast(41);
				return Values[0] + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdArrayInt",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size())),
			TEXT("array fixture must add the section")));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("CANONICAL array<int> Build must succeed from CodeGen; publisher=%d ast {%s}"),
				static_cast<int32>(InternalModule->GetLastBytecodePublisher()),
				*AstDump)));
		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(
			Published,
			*FString::Printf(
				TEXT("CANONICAL array module must publish int F(); have {%s}"),
				*AngelscriptNativeTestSupport::CollectFunctionDeclarations(Module))));
		int32 Value = 0;
		const bool bExecuted = AngelscriptNativeTestSupport::CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int F()", Value);
		ASSERT_THAT(IsTrue(
			bExecuted && Value == 42,
			*FString::Printf(
				TEXT("CANONICAL array<int> F() must execute 42 from CodeGen; executed=%s value=%d"),
				bExecuted ? TEXT("true") : TEXT("false"),
				Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL array<int> Build must not record asCCompiler")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Published, asBC_CALLSYS)
			|| AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Published, asBC_ALLOC),
			TEXT("array factory/insertLast/opIndex must emit CALLSYS or ALLOC, not a missing callee")));
		ASSERT_THAT(IsTrue(
			ProdDumpHasConstructCallee(AstDump),
			*FString::Printf(
				TEXT("array<int> Values default-init must record Sema factory callee= on Construct; ast {%s}"),
				*AstDump)));
	}

	TEST_METHOD(CanonicalNativeSameArityMethodExecutesSelectedNotFirstRegistered)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"array<class T>",
				0,
				asOBJ_REF | asOBJ_TEMPLATE | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("same-arity insertLast registers host array<T>")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"array<T>",
				asBEHAVE_FACTORY,
				"array<T> f(int&in)",
				asFUNCTION(ProdCanonicalArrayFactory),
				asCALL_GENERIC) >= 0,
			TEXT("same-arity insertLast registers factory")));
		const int BoolInsertId = ScriptEngine->RegisterObjectMethod(
			"array<T>",
			"void insertLast(bool value)",
			asFUNCTION(ProdCanonicalArrayInsertLastBoolPoison),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(BoolInsertId >= 0, TEXT("poison insertLast(bool) must register first")));
		const int IntInsertId = ScriptEngine->RegisterObjectMethod(
			"array<T>",
			"void insertLast(const T&in value)",
			asFUNCTION(ProdCanonicalArrayInsertLast),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(IntInsertId >= 0, TEXT("selected insertLast(const T&in) must register second")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectMethod(
				"array<T>",
				"T &opIndex(uint index)",
				asFUNCTION(ProdCanonicalArrayOpIndex),
				asCALL_GENERIC) >= 0,
			TEXT("same-arity insertLast registers opIndex")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("same-arity insertLast selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdArrayExactBind", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("same-arity insertLast needs a module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				array<int> Values;
				Values.insertLast(41);
				return Values[0] + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdArrayExactBind",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(TEXT("exact insertLast Bind must Generate; ast {%s}"), *AstDump)));
		ASSERT_THAT(IsTrue(
			AstDump.Contains(TEXT("callee=array<int>::insertLast(const int&in)")),
			*AstDump));
		ASSERT_THAT(IsFalse(
			AstDump.Contains(TEXT("callee=array<int>::insertLast(bool)")),
			*AstDump));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("exact insertLast must publish CodeGen")));

		asIScriptFunction* const IntInsert = ScriptEngine->GetFunctionById(IntInsertId);
		asIScriptFunction* const BoolInsert = ScriptEngine->GetFunctionById(BoolInsertId);
		ASSERT_THAT(IsNotNull(IntInsert, TEXT("registered insertLast(const T&in) must remain")));
		ASSERT_THAT(IsNotNull(BoolInsert, TEXT("registered insertLast(bool) must remain")));

		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(Published, TEXT("exact insertLast module must publish int F()")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
			&& Value == 42,
			*FString::Printf(TEXT("Sema-selected insertLast(const int&in) must execute 42, not poison 1; got=%d"), Value)));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsSystemDeclaration(Published, "void array::insertLast(const int&in)"),
			*FString::Printf(
				TEXT("bytecode must CALLSYS the materialized int insertLast, not the template prototype or first same-arity bool; prototype=%d calls=%s"),
				IntInsert->GetId(),
				*ProdBytecodeCallTargets(Published))));
		ASSERT_THAT(IsFalse(
			ProdBytecodeCallsSystemDeclaration(Published, "void array::insertLast(bool)"),
			*FString::Printf(
				TEXT("bytecode must not CALLSYS materialized poison insertLast(bool); prototype=%d calls=%s"),
				BoolInsert->GetId(),
				*ProdBytecodeCallTargets(Published))));
	}

	TEST_METHOD(CanonicalNativeSameArityMemberExecutesSelectedNotFirstRegistered)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FBindBox",
				sizeof(FProdBindBox),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS) >= 0,
			TEXT("FBindBox VALUE POD")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FBindBox",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdBindBoxConstruct),
				asCALL_GENERIC) >= 0,
			TEXT("FBindBox default construct")));
		const int BoolGetId = ScriptEngine->RegisterObjectMethod(
			"FBindBox",
			"int Get(bool)",
			asFUNCTION(ProdBindBoxGetBoolPoison),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(BoolGetId >= 0, TEXT("poison Get(bool) first")));
		const int IntGetId = ScriptEngine->RegisterObjectMethod(
			"FBindBox",
			"int Get(int)",
			asFUNCTION(ProdBindBoxGetInt),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(IntGetId >= 0, TEXT("selected Get(int) second")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("FBindBox Get selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdBindGet", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				FBindBox b;
				return b.Get(3);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdBindGet",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(TEXT("exact Get(int) must Generate; ast {%s}"), *AstDump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher()));

		asIScriptFunction* const IntGet = ScriptEngine->GetFunctionById(IntGetId);
		asIScriptFunction* const BoolGet = ScriptEngine->GetFunctionById(BoolGetId);
		ASSERT_THAT(IsNotNull(IntGet));
		ASSERT_THAT(IsNotNull(BoolGet));

		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(Published));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
			&& Value == 42,
			*FString::Printf(TEXT("Sema-selected Get(int) must execute 42, not poison 0; got=%d"), Value)));
		ASSERT_THAT(IsTrue(ProdBytecodeCallsFunction(Published, IntGet->GetId()),
			TEXT("bytecode must CALLSYS Get(int)")));
		ASSERT_THAT(IsFalse(ProdBytecodeCallsFunction(Published, BoolGet->GetId()),
			TEXT("bytecode must not CALLSYS Get(bool)")));
	}

	TEST_METHOD(CanonicalNativeConstAndMutableMemberExecutesReceiverQualifiedSelection)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FBindBox",
				sizeof(FProdBindBox),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS) >= 0,
			TEXT("FBindBox VALUE POD")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FBindBox",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdBindBoxConstruct),
				asCALL_GENERIC) >= 0,
			TEXT("FBindBox default construct")));
		const int ConstReadId = ScriptEngine->RegisterObjectMethod(
			"FBindBox",
			"int Read() const",
			asFUNCTION(ProdBindBoxGetBoolPoison),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(ConstReadId >= 0, TEXT("poison const Read() first")));
		const int MutableReadId = ScriptEngine->RegisterObjectMethod(
			"FBindBox",
			"int Read()",
			asFUNCTION(ProdBindBoxGetInt),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(MutableReadId >= 0, TEXT("selected mutable Read() second")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("FBindBox receiver-qualified Read selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdBindReceiverQualifiedRead", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				FBindBox b;
				return b.Read();
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdBindReceiverQualifiedRead",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(TEXT("receiver-qualified mutable Read() must Generate; ast {%s}"), *AstDump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("receiver-qualified native method must publish CodeGen")));

		asIScriptFunction* const MutableRead = ScriptEngine->GetFunctionById(MutableReadId);
		asIScriptFunction* const ConstRead = ScriptEngine->GetFunctionById(ConstReadId);
		ASSERT_THAT(IsNotNull(MutableRead));
		ASSERT_THAT(IsNotNull(ConstRead));

		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(Published));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
			&& Value == 42,
			*FString::Printf(TEXT("mutable receiver must execute mutable Read() and return 42, not const poison 0; got=%d"), Value)));
		ASSERT_THAT(IsTrue(ProdBytecodeCallsFunction(Published, MutableRead->GetId()),
			TEXT("bytecode must CALLSYS mutable Read()")));
		ASSERT_THAT(IsFalse(ProdBytecodeCallsFunction(Published, ConstRead->GetId()),
			TEXT("bytecode must not CALLSYS const Read() for a mutable receiver")));
	}

	TEST_METHOD(CanonicalNativeSameArityConstructorExecutesSelectedNotFirstRegistered)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FBindCtor",
				sizeof(FProdBindBox),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_CONSTRUCTOR
					| asOBJ_APP_CLASS_MORE_CONSTRUCTORS) >= 0,
			TEXT("FBindCtor VALUE POD")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FBindCtor",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdBindBoxConstruct),
				asCALL_GENERIC) >= 0,
			TEXT("FBindCtor dummy 0-arg construct for STMT_DECL")));
		const int BoolCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FBindCtor",
			asBEHAVE_CONSTRUCT,
			"void f(bool value)",
			asFUNCTION(ProdBindBoxConstructFloatPoison),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(BoolCtorId >= 0, TEXT("poison construct(bool) first")));
		const int IntCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FBindCtor",
			asBEHAVE_CONSTRUCT,
			"void f(int value)",
			asFUNCTION(ProdBindBoxConstructInt),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(IntCtorId >= 0, TEXT("selected construct(int) second")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FBindCtor",
				"int Stored",
				asOFFSET(FProdBindBox, Stored)) >= 0,
			TEXT("FBindCtor Stored")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("FBindCtor selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdBindCtor", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				FBindCtor b(3);
				return b.Stored;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdBindCtor",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(TEXT("exact construct(int) must Generate; ast {%s}"), *AstDump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher()));

		asIScriptFunction* const IntCtor = ScriptEngine->GetFunctionById(IntCtorId);
		asIScriptFunction* const BoolCtor = ScriptEngine->GetFunctionById(BoolCtorId);
		ASSERT_THAT(IsNotNull(IntCtor));
		ASSERT_THAT(IsNotNull(BoolCtor));
		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(Published));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
			&& Value == 42,
			*FString::Printf(TEXT("Sema-selected construct(int) must execute 42, not poison 0; got=%d"), Value)));
		ASSERT_THAT(IsTrue(ProdBytecodeCallsFunction(Published, IntCtor->GetId()),
			TEXT("bytecode must CALLSYS construct(int)")));
		ASSERT_THAT(IsFalse(ProdBytecodeCallsFunction(Published, BoolCtor->GetId()),
			TEXT("bytecode must not CALLSYS construct(bool)")));
	}

	TEST_METHOD(CanonicalNativeSameArityFactoryExecutesSelectedNotFirstRegistered)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FBindHost",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("FBindHost REF no @")));
		const int BoolFactoryId = ScriptEngine->RegisterObjectBehaviour(
			"FBindHost",
			asBEHAVE_FACTORY,
			"FBindHost f(bool)",
			asFUNCTION(ProdBindHostFactoryBoolPoison),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(BoolFactoryId >= 0, TEXT("poison factory(bool) first")));
		const int IntFactoryId = ScriptEngine->RegisterObjectBehaviour(
			"FBindHost",
			asBEHAVE_FACTORY,
			"FBindHost f(int)",
			asFUNCTION(ProdBindHostFactoryInt),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(IntFactoryId >= 0, TEXT("selected factory(int) second")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FBindHost",
				"int Stored",
				asOFFSET(FProdBindHost, Stored)) >= 0,
			TEXT("FBindHost Stored")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("FBindHost selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdBindHost", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				FBindHost h(3);
				return h.Stored;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdBindHost",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(TEXT("exact factory(int) must Generate; ast {%s}"), *AstDump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher()));

		asIScriptFunction* const IntFactory = ScriptEngine->GetFunctionById(IntFactoryId);
		asIScriptFunction* const BoolFactory = ScriptEngine->GetFunctionById(BoolFactoryId);
		ASSERT_THAT(IsNotNull(IntFactory));
		ASSERT_THAT(IsNotNull(BoolFactory));
		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(Published));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
			&& Value == 42,
			*FString::Printf(TEXT("Sema-selected factory(int) must execute 42, not poison 0; got=%d"), Value)));
		ASSERT_THAT(IsTrue(ProdBytecodeCallsFunction(Published, IntFactory->GetId()),
			TEXT("bytecode must CALLSYS factory(int)")));
		ASSERT_THAT(IsFalse(ProdBytecodeCallsFunction(Published, BoolFactory->GetId()),
			TEXT("bytecode must not CALLSYS factory(bool)")));
	}

	TEST_METHOD(CanonicalNativeZeroArgDefaultFactoryExecutesInternedNotFirstRegistered)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FBindHost",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("FBindHost zero-arg REF")));
		const int PoisonFactoryId = ScriptEngine->RegisterObjectBehaviour(
			"FBindHost",
			asBEHAVE_FACTORY,
			"FBindHost f(int)",
			asFUNCTION(ProdBindHostFactoryIntPoison),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(PoisonFactoryId >= 0, TEXT("poison factory(int) first")));
		const int ZeroFactoryId = ScriptEngine->RegisterObjectBehaviour(
			"FBindHost",
			asBEHAVE_FACTORY,
			"FBindHost f()",
			asFUNCTION(ProdBindHostFactoryZero),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(ZeroFactoryId >= 0, TEXT("selected factory() second")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FBindHost",
				"int Stored",
				asOFFSET(FProdBindHost, Stored)) >= 0,
			TEXT("FBindHost Stored")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("zero-arg factory selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdBindHostZero", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				FBindHost h;
				return h.Stored + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdBindHostZero",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(TEXT("0-arg factory default-init must Generate; ast {%s}"), *AstDump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher()));
		ASSERT_THAT(IsTrue(
			ProdDumpHasConstructCallee(AstDump),
			*FString::Printf(TEXT("0-arg factory Construct must have callee=; ast {%s}"), *AstDump)));

		asIScriptFunction* const ZeroFactory = ScriptEngine->GetFunctionById(ZeroFactoryId);
		asIScriptFunction* const PoisonFactory = ScriptEngine->GetFunctionById(PoisonFactoryId);
		ASSERT_THAT(IsNotNull(ZeroFactory));
		ASSERT_THAT(IsNotNull(PoisonFactory));
		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(Published));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
			&& Value == 42,
			*FString::Printf(TEXT("interned factory() must execute 42, not poison 1-arg; got=%d"), Value)));
		ASSERT_THAT(IsTrue(ProdBytecodeCallsFunction(Published, ZeroFactory->GetId()),
			TEXT("bytecode must CALLSYS factory()")));
		ASSERT_THAT(IsFalse(ProdBytecodeCallsFunction(Published, PoisonFactory->GetId()),
			TEXT("bytecode must not CALLSYS factory(int)")));
	}

	TEST_METHOD(CanonicalNativeZeroArgDefaultConstructorExecutesInternedNotFirstRegistered)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FBindCtor",
				sizeof(FProdBindBox),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_CONSTRUCTOR
					| asOBJ_APP_CLASS_MORE_CONSTRUCTORS) >= 0,
			TEXT("FBindCtor zero-arg VALUE POD")));
		const int PoisonCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FBindCtor",
			asBEHAVE_CONSTRUCT,
			"void f(int value)",
			asFUNCTION(ProdBindBoxConstructIntPoison),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(PoisonCtorId >= 0, TEXT("poison construct(int) first")));
		const int ZeroCtorId = ScriptEngine->RegisterObjectBehaviour(
			"FBindCtor",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ProdBindBoxConstructZero),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(ZeroCtorId >= 0, TEXT("selected construct() second")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FBindCtor",
				"int Stored",
				asOFFSET(FProdBindBox, Stored)) >= 0,
			TEXT("FBindCtor Stored")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("zero-arg ctor selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdBindCtorZero", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				FBindCtor b;
				return b.Stored + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdBindCtorZero",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(TEXT("0-arg ctor default-init must Generate; ast {%s}"), *AstDump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher()));
		ASSERT_THAT(IsTrue(
			ProdDumpHasConstructCallee(AstDump),
			*FString::Printf(TEXT("0-arg ctor Construct must have callee=; ast {%s}"), *AstDump)));

		asIScriptFunction* const ZeroCtor = ScriptEngine->GetFunctionById(ZeroCtorId);
		asIScriptFunction* const PoisonCtor = ScriptEngine->GetFunctionById(PoisonCtorId);
		ASSERT_THAT(IsNotNull(ZeroCtor));
		ASSERT_THAT(IsNotNull(PoisonCtor));
		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(Published));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
			&& Value == 42,
			*FString::Printf(TEXT("interned construct() must execute 42, not poison 1-arg; got=%d"), Value)));
		ASSERT_THAT(IsTrue(ProdBytecodeCallsFunction(Published, ZeroCtor->GetId()),
			TEXT("bytecode must CALLSYS construct()")));
		ASSERT_THAT(IsFalse(ProdBytecodeCallsFunction(Published, PoisonCtor->GetId()),
			TEXT("bytecode must not CALLSYS construct(int)")));
	}

	TEST_METHOD(CanonicalNativeSameArityIndexExecutesSelectedNotFirstRegistered)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"array<class T>",
				0,
				asOBJ_REF | asOBJ_TEMPLATE | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("same-arity opIndex registers host array<T>")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"array<T>",
				asBEHAVE_FACTORY,
				"array<T> f(int&in)",
				asFUNCTION(ProdCanonicalArrayFactory),
				asCALL_GENERIC) >= 0,
			TEXT("same-arity opIndex registers factory")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectMethod(
				"array<T>",
				"void insertLast(const T&in value)",
				asFUNCTION(ProdCanonicalArrayInsertLast),
				asCALL_GENERIC) >= 0,
			TEXT("same-arity opIndex registers insertLast")));
		const int BoolIndexId = ScriptEngine->RegisterObjectMethod(
			"array<T>",
			"T &opIndex(bool)",
			asFUNCTION(ProdCanonicalArrayOpIndexBoolPoison),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(BoolIndexId >= 0, TEXT("poison opIndex(bool) first")));
		const int UintIndexId = ScriptEngine->RegisterObjectMethod(
			"array<T>",
			"T &opIndex(uint index)",
			asFUNCTION(ProdCanonicalArrayOpIndex),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(UintIndexId >= 0, TEXT("selected opIndex(uint) second")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("same-arity opIndex selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdArrayExactIndex", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				array<int> Values;
				Values.insertLast(41);
				return Values[0] + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdArrayExactIndex",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(TEXT("exact opIndex(uint) must Generate; ast {%s}"), *AstDump)));
		ASSERT_THAT(IsTrue(
			AstDump.Contains(TEXT("callee=array<int>::opIndex(uint)")),
			*FString::Printf(TEXT("Index must record Sema-selected opIndex(uint); ast {%s}"), *AstDump)));
		ASSERT_THAT(IsFalse(
			AstDump.Contains(TEXT("callee=array<int>::opIndex(bool)")),
			*FString::Printf(TEXT("Index must not bind poison opIndex(bool); ast {%s}"), *AstDump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher()));

		asIScriptFunction* const UintIndex = ScriptEngine->GetFunctionById(UintIndexId);
		asIScriptFunction* const BoolIndex = ScriptEngine->GetFunctionById(BoolIndexId);
		ASSERT_THAT(IsNotNull(UintIndex));
		ASSERT_THAT(IsNotNull(BoolIndex));
		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(Published));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
			&& Value == 42,
			*FString::Printf(TEXT("Sema-selected opIndex(uint) must execute 42, not poison 1; got=%d"), Value)));
		ASSERT_THAT(IsTrue(ProdBytecodeCallsSystemDeclaration(Published, "int& array::opIndex(uint)"),
			*FString::Printf(
				TEXT("bytecode must CALLSYS materialized opIndex(uint); prototype=%d calls=%s"),
				UintIndex->GetId(),
				*ProdBytecodeCallTargets(Published))));
		ASSERT_THAT(IsFalse(ProdBytecodeCallsSystemDeclaration(Published, "int& array::opIndex(bool)"),
			*FString::Printf(
				TEXT("bytecode must not CALLSYS materialized poison opIndex(bool); prototype=%d calls=%s"),
				BoolIndex->GetId(),
				*ProdBytecodeCallTargets(Published))));
	}

	TEST_METHOD(CanonicalImportCallBuildPublishesCodeGenAndExecutes)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("import fixture selects CANONICAL")));

		FScopedNativeModule Provider(
			*TestRunner,
			Engine,
			"ProdImportProvider",
			ASTEST_AS_ANSI(R"AS(
				int SharedValue()
				{
					return 41;
				}
			)AS"));
		ASSERT_THAT(IsTrue(Provider.IsValid(), TEXT("CANONICAL import provider must compile")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			import int SharedValue() from "ProdImportProvider";

			int F()
			{
				return SharedValue() + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdImportConsumer",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL import consumer Build must succeed from CodeGen")));
		asIScriptFunction* const Published = GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(
			Published,
			*FString::Printf(
				TEXT("CANONICAL import consumer must publish int F(); have {%s}"),
				*CollectFunctionDeclarations(Module))));
		ASSERT_THAT(AreEqual(1u, Module->GetImportedFunctionCount(),
			TEXT("consumer must install one asFUNC_IMPORTED slot, not skip DECL_IMPORT")));
		asIScriptFunction* const Shared = Provider->GetFunctionByDecl("int SharedValue()");
		ASSERT_THAT(IsNotNull(Shared, TEXT("provider must expose int SharedValue()")));
		ASSERT_THAT(AreEqual(0, Module->BindImportedFunction(0, Shared),
			TEXT("imported function must bind to the provider")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL import F() must execute SharedValue()+1 from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL import Build must not record asCCompiler")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Published, asBC_CALLBND),
			TEXT("F() must emit CALLBND, not CALL/CallPtr without an import slot")));
	}

	TEST_METHOD(CanonicalSourceQualifierMetadataDirectImportPublishesExactFormalIdentity)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"FProdAutomaticImportValue",
			sizeof(FProdAutomaticImportValue),
			asOBJ_VALUE | asOBJ_POD
				| asGetTypeTraits<FProdAutomaticImportValue>()) >= 0,
			TEXT("direct-import source-qualifier fixture must register its value type")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		FScopedNativeModule Provider(
			*TestRunner,
			Engine,
			"ProdSourceQualifierImportProvider",
			ASTEST_AS_ANSI(R"AS(
				int ReadExplicit(
					const FProdAutomaticImportValue& inout Value)
				{
					return 42;
				}
			)AS"));
		ASSERT_THAT(IsTrue(Provider.IsValid(),
			TEXT("source-qualifier import provider must compile")));
		if (!Provider.IsValid())
		{
			return;
		}

		asIScriptModule* Consumer = nullptr;
		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
			import int ReadExplicit(
				const FProdAutomaticImportValue& inout Value)
				from "ProdSourceQualifierImportProvider";

			int Entry(const FProdAutomaticImportValue& inout Value)
			{
				return ReadExplicit(Value);
			}
		)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdSourceQualifierImportConsumer",
			ConsumerSource.c_str(),
			Consumer),
			TEXT("direct Canonical import consumer must compile")));
		ASSERT_THAT(IsNotNull(Consumer));
		if (Consumer == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(1u, Consumer->GetImportedFunctionCount()));
		asCScriptFunction* const Imported =
			static_cast<asCModule*>(Consumer)->GetImportedFunction(0);
		const asDWORD ExpectedQualifiers = asAST_QUAL_CONST
			| asAST_QUAL_REFERENCE | asAST_QUAL_INOUT;
		ASSERT_THAT(IsTrue(Imported != nullptr
			&& Imported->canonicalASTStableDeclKey.GetLength() > 0,
			TEXT("direct pending import must publish its exact Canonical declaration identity")));
		ASSERT_THAT(IsTrue(Imported != nullptr
			&& Imported->canonicalASTSourceParameterQualifiers.GetLength() == 1
			&& Imported->canonicalASTSourceParameterQualifiers[0]
				== ExpectedQualifiers,
			TEXT("direct pending import must publish exact source qualifiers instead of relying on Runtime ABI inversion")));

		asIScriptFunction* const ProviderFunction =
			Provider->GetFunctionByName("ReadExplicit");
		ASSERT_THAT(IsNotNull(ProviderFunction));
		ASSERT_THAT(AreEqual(0,
			Consumer->BindImportedFunction(0, ProviderFunction),
			TEXT("the source-exact imported shell must retain the established Runtime binding ABI")));
	}

	TEST_METHOD(CanonicalDirectImportOwnsAuthoredDefaultsAndExecutesNamedCall)
	{
		// Production mutation caught: a null defaultArgs entry on the detached
		// import shell loses reflection/Cache identity even when the sealed AST
		// can still execute the same-source named/default call.
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		FScopedNativeModule Provider(
			*TestRunner,
			Engine,
			"ProdDirectDefaultImportProvider",
			ASTEST_AS_ANSI(R"AS(
				int ImportedPack(int A = 1, int B = 2)
				{
					return A * 10 + B;
				}
			)AS"));
		ASSERT_THAT(IsTrue(Provider.IsValid(),
			TEXT("direct default-import provider must compile")));
		if (!Provider.IsValid())
		{
			return;
		}

		asIScriptModule* Consumer = nullptr;
		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
			import int ImportedPack(int A = 1, int B = 2)
				from "ProdDirectDefaultImportProvider";

			int Entry()
			{
				return ImportedPack(B: 7);
			}
		)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdDirectDefaultImportConsumer",
			ConsumerSource.c_str(),
			Consumer),
			TEXT("direct named/default import consumer must compile")));
		ASSERT_THAT(IsNotNull(Consumer));
		if (Consumer == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(1u, Consumer->GetImportedFunctionCount(),
			TEXT("direct default-import consumer must publish one import shell")));
		asIScriptFunction* const Imported =
			static_cast<asCModule*>(Consumer)->GetImportedFunction(0);
		ASSERT_THAT(IsNotNull(Imported));
		if (Imported == nullptr)
		{
			return;
		}

		const char* DefaultA = nullptr;
		const char* DefaultB = nullptr;
		ASSERT_THAT(AreEqual(asSUCCESS,
			Imported->GetParam(0, nullptr, nullptr, nullptr, &DefaultA),
			TEXT("direct import formal A metadata must be queryable")));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Imported->GetParam(1, nullptr, nullptr, nullptr, &DefaultB),
			TEXT("direct import formal B metadata must be queryable")));
		ASSERT_THAT(AreEqual(FString(TEXT("1")),
			FString(UTF8_TO_TCHAR(DefaultA != nullptr ? DefaultA : "")),
			TEXT("direct import shell must own authored default A=1")));
		ASSERT_THAT(AreEqual(FString(TEXT("2")),
			FString(UTF8_TO_TCHAR(DefaultB != nullptr ? DefaultB : "")),
			TEXT("direct import shell must own authored default B=2")));

		asIScriptFunction* const ProviderFunction =
			Provider->GetFunctionByDecl("int ImportedPack(int, int)");
		ASSERT_THAT(IsNotNull(ProviderFunction));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Consumer->BindImportedFunction(0, ProviderFunction),
			TEXT("default metadata must not alter the Runtime import ABI")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Consumer, "int Entry()", Value)
			&& Value == 17,
			*FString::Printf(TEXT(
				"ImportedPack(B: 7) must execute A=1 B=7; got=%d"), Value)));
	}

	TEST_METHOD(CanonicalSourceQualifierMetadataConstructorFactoryCopiesExactFormalIdentity)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"FProdAutomaticImportValue",
			sizeof(FProdAutomaticImportValue),
			asOBJ_VALUE | asOBJ_POD
				| asGetTypeTraits<FProdAutomaticImportValue>()) >= 0,
			TEXT("constructor-factory source-qualifier fixture must register its value type")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* PublicModule = nullptr;
		const std::string FactorySource = ASTEST_AS_ANSI(R"AS(
			class FProdSourceQualifierFactory
			{
				FProdSourceQualifierFactory(
					const FProdAutomaticImportValue& inout Value)
				{
				}
			}
		)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdSourceQualifierFactory",
			FactorySource.c_str(),
			PublicModule),
			TEXT("direct Canonical constructor/factory module must compile")));
		asCModule* const Module = static_cast<asCModule*>(PublicModule);
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		asCObjectType* ObjectType = nullptr;
		for (asUINT Index = 0; Index < Module->classTypes.GetLength(); ++Index)
		{
			asCObjectType* const Candidate = Module->classTypes[Index];
			if (Candidate != nullptr
				&& Candidate->name.Equals("FProdSourceQualifierFactory"))
			{
				ObjectType = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ObjectType));
		if (ObjectType == nullptr)
		{
			return;
		}

		asCScriptFunction* Constructor = nullptr;
		for (asUINT Index = 0; Index < ObjectType->beh.constructors.GetLength(); ++Index)
		{
			const int FunctionId = ObjectType->beh.constructors[Index];
			asCScriptFunction* const Candidate = FunctionId > 0
				&& static_cast<asUINT>(FunctionId)
					< ScriptEngine->scriptFunctions.GetLength()
				? ScriptEngine->scriptFunctions[FunctionId] : nullptr;
			if (Candidate != nullptr
				&& Candidate->parameterTypes.GetLength() == 1)
			{
				Constructor = Candidate;
				break;
			}
		}
		asCScriptFunction* Factory = nullptr;
		for (asUINT Index = 0; Index < ObjectType->beh.factories.GetLength(); ++Index)
		{
			const int FunctionId = ObjectType->beh.factories[Index];
			asCScriptFunction* const Candidate = FunctionId > 0
				&& static_cast<asUINT>(FunctionId)
					< ScriptEngine->scriptFunctions.GetLength()
				? ScriptEngine->scriptFunctions[FunctionId] : nullptr;
			if (Candidate != nullptr
				&& Candidate->parameterTypes.GetLength() == 1)
			{
				Factory = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(Constructor));
		ASSERT_THAT(IsNotNull(Factory));
		const asDWORD ExpectedQualifiers = asAST_QUAL_CONST
			| asAST_QUAL_REFERENCE | asAST_QUAL_INOUT;
		ASSERT_THAT(IsTrue(Constructor != nullptr
			&& Constructor->canonicalASTSourceParameterQualifiers.GetLength() == 1
			&& Constructor->canonicalASTSourceParameterQualifiers[0]
				== ExpectedQualifiers,
			TEXT("direct constructor must retain its exact source formal identity")));
		ASSERT_THAT(IsTrue(Factory != nullptr && Constructor != nullptr
			&& Factory->canonicalASTStableDeclKey
				== Constructor->canonicalASTStableDeclKey,
			TEXT("synthetic factory must derive the exact constructor declaration identity")));
		ASSERT_THAT(IsTrue(Factory != nullptr
			&& Factory->canonicalASTSourceParameterQualifiers.GetLength() == 1
			&& Factory->canonicalASTSourceParameterQualifiers[0]
				== ExpectedQualifiers,
			TEXT("synthetic factory must copy exact constructor source qualifiers")));
	}

	TEST_METHOD(CanonicalAutomaticImportsPublishGlobalsAndResolveFunctions)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("automatic-import fixture selects CANONICAL")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS),
			ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, true),
			TEXT("automatic-import fixture enables the established cross-module lookup contract")));

		FScopedNativeModule Provider(
			*TestRunner,
			Engine,
			"ProdAutomaticImportProvider",
			ASTEST_AS_ANSI(R"AS(
				namespace AutoProvider
				{
					int SharedGlobal = 40;

					int SharedValue()
					{
						return SharedGlobal + 1;
					}
				}
				)AS"));
		ASSERT_THAT(IsTrue(Provider.IsValid(),
			TEXT("CANONICAL automatic-import provider must compile")));
		if (!Provider.IsValid())
		{
			return;
		}

		asIScriptFunction* const ProviderFunction =
			Provider->GetFunctionByDecl("int AutoProvider::SharedValue()");
		ASSERT_THAT(IsNotNull(ProviderFunction,
			TEXT("provider must publish its namespace-qualified global function")));
		asCGlobalProperty* ProviderGlobal = nullptr;
		asCModule* const ProviderModule = static_cast<asCModule*>(Provider.Get());
		for (asUINT Index = 0; ProviderModule != nullptr && Index < ProviderModule->scriptGlobalsList.GetLength(); ++Index)
		{
			asCGlobalProperty* const Candidate = ProviderModule->scriptGlobalsList[Index];
			if (Candidate != nullptr && Candidate->name == "SharedGlobal"
				&& Candidate->nameSpace != nullptr && Candidate->nameSpace->name == "AutoProvider")
			{
				ProviderGlobal = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ProviderGlobal,
			TEXT("provider must publish its namespace-qualified mutable global")));

		const bool bFunctionAvailable = ProviderFunction != nullptr
			&& ScriptEngine->allScriptGlobalFunctions.Contains(
				static_cast<asCScriptFunction*>(ProviderFunction));
		const bool bGlobalAvailable = ProviderGlobal != nullptr
			&& ScriptEngine->allScriptGlobalVariables.Contains(ProviderGlobal);
		ASSERT_THAT(IsTrue(bFunctionAvailable,
			TEXT("a committed CANONICAL global function must enter automatic-import availability")));
		ASSERT_THAT(IsTrue(bGlobalAvailable,
			TEXT("a committed CANONICAL namespaced global must enter automatic-import availability")));
		if (!bFunctionAvailable || !bGlobalAvailable)
		{
			return;
		}

		asIScriptModule* Consumer = nullptr;
		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return AutoProvider::SharedValue() + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdAutomaticImportConsumer",
			ConsumerSource.c_str(),
			Consumer),
			TEXT("CANONICAL consumer must resolve committed provider globals through automatic imports")));
		if (Consumer == nullptr)
		{
			return;
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), Consumer, "int Entry()", Value) && Value == 42,
			TEXT("CANONICAL automatic-import consumer must execute against the committed provider function generation")));

		// A direct cross-module mutable-global reference is valid only when the
		// consumer bytecode owns the provider property until the consumer itself
		// is retired. Force a GC sweep between retiring the provider and executing
		// the consumer; merely leaving the discarded provider module allocated
		// would not prove that the raw LDG address remains safe.
		FScopedNativeModule GlobalConsumer(
			*TestRunner,
			Engine,
			"ProdAutomaticImportGlobalConsumer",
			ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return AutoProvider::SharedGlobal + 2;
			}
			)AS"));
		ASSERT_THAT(IsTrue(GlobalConsumer.IsValid(),
			TEXT("CANONICAL must publish a direct automatic-import mutable-global consumer")));
		if (!GlobalConsumer.IsValid())
		{
			return;
		}

		int32 GlobalValue = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), GlobalConsumer.Get(), "int Entry()", GlobalValue)
				&& GlobalValue == 42,
			TEXT("CANONICAL mutable-global consumer must execute before provider retirement")));

		void* const ProviderGlobalAddress = ProviderGlobal->GetAddressOfValue();
		ASSERT_THAT(AreEqual(asSUCCESS, Provider.Discard(),
			TEXT("provider retirement must succeed while a consumer owns its mutable-global bytecode reference")));
		ASSERT_THAT(AreEqual(asSUCCESS, Engine.Get()->GarbageCollect(),
			TEXT("provider GC sweep must complete while the consumer retains the global property")));
		ASSERT_THAT(IsNotNull(ScriptEngine->varAddressMap.Find(ProviderGlobalAddress),
			TEXT("provider global address must remain mapped while a live consumer bytecode generation owns it")));

		GlobalValue = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), GlobalConsumer.Get(), "int Entry()", GlobalValue)
				&& GlobalValue == 42,
			TEXT("CANONICAL mutable-global consumer must remain executable after provider retirement and GC")));

		ASSERT_THAT(AreEqual(asSUCCESS, GlobalConsumer.Discard(),
			TEXT("consumer retirement must release its mutable-global property reference")));
		ASSERT_THAT(AreEqual(asSUCCESS, Engine.Get()->GarbageCollect(),
			TEXT("consumer GC sweep must complete after releasing the provider property reference")));
		ASSERT_THAT(IsNull(ScriptEngine->varAddressMap.Find(ProviderGlobalAddress),
			TEXT("provider global address must be reclaimed after its final consumer retires")));
	}

	TEST_METHOD(CanonicalAutomaticImportProjectsDefaultsForNamedCall)
	{
		// Production mutation caught: projecting only Runtime type/name data
		// makes the omitted defaulted A formal non-viable for Pack(B: 7).
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("automatic default-import fixture selects CANONICAL")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS),
			ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, true),
			TEXT("automatic default-import fixture enables cross-module lookup")));

		FScopedNativeModule Provider(
			*TestRunner,
			Engine,
			"ProdAutomaticDefaultImportProvider",
			ASTEST_AS_ANSI(R"AS(
				namespace AutoDefault
				{
					int Pack(int A = 1, int B = 2)
					{
						return A * 10 + B;
					}
				}
			)AS"));
		ASSERT_THAT(IsTrue(Provider.IsValid(),
			TEXT("automatic default-import provider must compile")));
		if (!Provider.IsValid())
		{
			return;
		}

		asIScriptModule* Consumer = nullptr;
		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return AutoDefault::Pack(B: 7);
			}
		)AS");
		const int BuildResult = CompileNativeModule(
			Engine.Get(),
			"ProdAutomaticDefaultImportConsumer",
			ConsumerSource.c_str(),
			Consumer);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("automatic-import named/default call must compile canonically")));
		if (BuildResult < 0 || Consumer == nullptr)
		{
			return;
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Consumer, "int Entry()", Value)
			&& Value == 17,
			*FString::Printf(TEXT(
				"AutoDefault::Pack(B: 7) must execute A=1 B=7; got=%d"), Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Consumer)->GetLastBytecodePublisher(),
			TEXT("automatic default-import call must use Canonical CodeGen")));
	}

	TEST_METHOD(CanonicalAutomaticImportBindsScriptValueParameterABI)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"FProdAutomaticImportValue",
			sizeof(FProdAutomaticImportValue),
			asOBJ_VALUE | asOBJ_POD
				| asGetTypeTraits<FProdAutomaticImportValue>()) >= 0,
			TEXT("automatic-import ABI fixture must register its value type")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS),
			ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, true)));

		FScopedNativeModule Provider(
			*TestRunner,
			Engine,
			"ProdAutomaticImportValueABIProvider",
			ASTEST_AS_ANSI(R"AS(
				namespace AutoValueProvider
				{
					int ReadValue(FProdAutomaticImportValue Value)
					{
						return 42;
					}
				}
			)AS"));
		ASSERT_THAT(IsTrue(Provider.IsValid(),
			TEXT("automatic-import value ABI provider must build")));
		if (!Provider.IsValid())
		{
			return;
		}

		FScopedNativeModule Consumer(
			*TestRunner,
			Engine,
			"ProdAutomaticImportValueABIConsumer",
			ASTEST_AS_ANSI(R"AS(
				int Entry(FProdAutomaticImportValue Value)
				{
					return AutoValueProvider::ReadValue(Value);
				}
			)AS"));
		ASSERT_THAT(IsTrue(Consumer.IsValid(),
			TEXT("automatic-import consumer must bind source by-value identity to the provider Runtime ABI")));
		if (!Consumer.IsValid())
		{
			return;
		}

		asIScriptFunction* const Entry = Consumer->GetFunctionByDecl(
			"int Entry(FProdAutomaticImportValue)");
		ASSERT_THAT(IsNotNull(Entry));
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Entry != nullptr && Context != nullptr)
		{
			FProdAutomaticImportValue Argument;
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Entry)));
			ASSERT_THAT(AreEqual(
				asSUCCESS, Context->SetArgObject(0, &Argument)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(
				42, static_cast<int32>(Context->GetReturnDWord()),
				TEXT("automatic-import value ABI consumer must execute the exact provider function")));
			Context->Release();
		}
	}

	TEST_METHOD(CanonicalGeneratedAccessorBuildPublishesCodeGenAndExecutes)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("generated-accessor fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FValue
			{
				int Value;
			}

			int F()
			{
				FValue Object;
				Object.Value = 41;
				return Object.Value + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdGenAccessor",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL generated accessor Build must succeed from CodeGen")));
		asIScriptFunction* const Published = GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(
			Published,
			*FString::Printf(
				TEXT("CANONICAL generated-accessor module must publish int F(); have {%s}"),
				*CollectFunctionDeclarations(Module))));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL generated accessor F() must execute Object.Value=41 then +1 from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL generated accessor Build must not record asCCompiler")));

		asITypeInfo* const TypeInfo = Module != nullptr ? Module->GetTypeInfoByName("FValue") : nullptr;
		ASSERT_THAT(IsNotNull(TypeInfo, TEXT("FValue must be installed as a module type")));
		asIScriptFunction* const Getter = TypeInfo->GetMethodByName("GetValue");
		asIScriptFunction* const Setter = TypeInfo->GetMethodByName("SetValue");
		ASSERT_THAT(IsNotNull(Getter, TEXT("generated GetValue must be installed on FValue with a body")));
		ASSERT_THAT(IsNotNull(Setter, TEXT("generated SetValue must be installed on FValue with a body")));
		ASSERT_THAT(IsTrue(Getter->GetObjectType() == TypeInfo,
			TEXT("GetValue objectType must be FValue, not a module global")));
		ASSERT_THAT(IsTrue(Setter->GetObjectType() == TypeInfo,
			TEXT("SetValue objectType must be FValue, not a module global")));
		asCModule* const ScriptModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(IsFalse(ProdFunctionIsOnGlobalList(ScriptModule, Getter),
			TEXT("GetValue must not appear in module globalFunctionList")));
		ASSERT_THAT(IsFalse(ProdFunctionIsOnGlobalList(ScriptModule, Setter),
			TEXT("SetValue must not appear in module globalFunctionList")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Getter, asBC_RDR4),
			TEXT("generated GetValue body must read the field (RDR4), not be an empty RET")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Setter, asBC_WRTV4),
			TEXT("generated SetValue body must write the field (WRTV4), not be an empty RET")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Published, Getter->GetId()),
			TEXT("F() must CALL GetValue; inline ADDSi/RDR4 on F() is a false green")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Published, Setter->GetId()),
			TEXT("F() must CALL SetValue; MEMBER_REF WRTV4 on F() is a false green")));
		ASSERT_THAT(IsFalse(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Published, asBC_ADDSi),
			TEXT("F() must not inline ADDSi on FindFunc-miss generated Get; that is GetFirstProperty false green")));
	}

	TEST_METHOD(CanonicalTwoValueScriptTypesMemberRefUsesSealedTypeNotOnlyValueGuess)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("two VALUE types fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FOther
			{
				int X;
			}

			struct FFields
			{
				int Poison;
				int Stored;
			}

			int F()
			{
				FFields Object;
				Object.Stored = 41;
				return Object.Stored + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdTwoValueTypes",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL MemberRef must Resolve sealed FFields, not guess the only VALUE type")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("ObjectTypeFromExpr must not pick FOther via only-VALUE guess; got=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalGeneratedAccessorSecondFieldExecutesNotFirstProperty)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("second-field accessor fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FFields
			{
				int Poison;
				int Stored;
			}

			int F()
			{
				FFields Object;
				Object.Poison = 7;
				Object.Stored = 41;
				return Object.Stored;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdGenAccessorSecond",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL two-field generated accessor Build must succeed")));
		asITypeInfo* const TypeInfo = Module != nullptr ? Module->GetTypeInfoByName("FFields") : nullptr;
		ASSERT_THAT(IsNotNull(TypeInfo, TEXT("FFields must be installed")));
		asIScriptFunction* const Getter = TypeInfo->GetMethodByName("GetStored");
		asIScriptFunction* const Setter = TypeInfo->GetMethodByName("SetStored");
		ASSERT_THAT(IsNotNull(Getter, TEXT("generated GetStored must exist")));
		ASSERT_THAT(IsNotNull(Setter, TEXT("generated SetStored must exist")));
		asIScriptFunction* const Published = GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Published, Getter->GetId()),
			TEXT("F() must CALL GetStored, not inline first property Poison")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Published, Setter->GetId()),
			TEXT("F() must CALL SetStored")));
		ASSERT_THAT(IsFalse(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Published, asBC_ADDSi),
			TEXT("caller F() must not ADDSi-inline generated GetStored")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 41,
			*FString::Printf(
				TEXT("SetStored/GetStored must hit Stored, not Poison; got=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalGeneratedInt64AccessorUsesRdr8NotRdr4)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("int64 accessor fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FValue
			{
				int64 Value;
			}

			int64 F()
			{
				FValue Object;
				Object.Value = 41;
				return Object.Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdGenAccessorI64",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL int64 accessor Build must succeed from CodeGen")));
		asITypeInfo* const TypeInfo = Module != nullptr ? Module->GetTypeInfoByName("FValue") : nullptr;
		ASSERT_THAT(IsNotNull(TypeInfo, TEXT("int64 FValue must be installed")));
		asIScriptFunction* const Getter = TypeInfo->GetMethodByName("GetValue");
		asIScriptFunction* const Setter = TypeInfo->GetMethodByName("SetValue");
		ASSERT_THAT(IsNotNull(Getter, TEXT("generated GetValue for int64")));
		ASSERT_THAT(IsNotNull(Setter, TEXT("generated SetValue for int64")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Getter, asBC_RDR8),
			TEXT("int64 GetValue must RDR8 from sealed 8-byte type size, not hardcoded RDR4")));
		ASSERT_THAT(IsFalse(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Getter, asBC_RDR4),
			TEXT("int64 GetValue must not emit 4-byte RDR4")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Setter, asBC_WRTV8),
			TEXT("int64 SetValue must WRTV8, not hardcoded WRTV4")));
		ASSERT_THAT(IsFalse(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Setter, asBC_WRTV4),
			TEXT("int64 SetValue must not emit 4-byte WRTV4")));
		asIScriptFunction* const Published = GetNativeFunctionByDecl(Module, "int64 F()");
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Published, Getter->GetId()),
			TEXT("int64 F() must CALL GetValue")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Published, Setter->GetId()),
			TEXT("int64 F() must CALL SetValue")));
		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, Engine.Get(), Module, "int64 F()");
		const int64 Got = Invoker.CallAndReturn<int64>(0);
		ASSERT_THAT(AreEqual(static_cast<int64>(41), Got,
			TEXT("int64 accessor F() must return the stored 41")));
	}

	TEST_METHOD(CanonicalGeneratedDoubleAccessorUsesRdr8NotRdr4)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("double accessor fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FValue
			{
				double Value;
			}

			double F()
			{
				FValue Object;
				Object.Value = 41.25;
				return Object.Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdGenAccessorDbl",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL double accessor Build must succeed from CodeGen")));
		asITypeInfo* const TypeInfo = Module != nullptr ? Module->GetTypeInfoByName("FValue") : nullptr;
		ASSERT_THAT(IsNotNull(TypeInfo, TEXT("double FValue must be installed")));
		asIScriptFunction* const Getter = TypeInfo->GetMethodByName("GetValue");
		asIScriptFunction* const Setter = TypeInfo->GetMethodByName("SetValue");
		ASSERT_THAT(IsNotNull(Getter, TEXT("generated GetValue for double")));
		ASSERT_THAT(IsNotNull(Setter, TEXT("generated SetValue for double")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Getter, asBC_RDR8),
			TEXT("double GetValue must RDR8 from sealed 8-byte type size")));
		ASSERT_THAT(IsFalse(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Getter, asBC_RDR4),
			TEXT("double GetValue must not emit 4-byte RDR4")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Setter, asBC_WRTV8),
			TEXT("double SetValue must WRTV8")));
		ASSERT_THAT(IsFalse(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Setter, asBC_WRTV4),
			TEXT("double SetValue must not emit 4-byte WRTV4")));
		double Result = 0.0;
		ASSERT_THAT(IsTrue(
			AngelscriptSDKTestSupport::ExecuteScriptFunction(*TestRunner, Engine.Get(), Module, "double F()", Result)
				&& Result == 41.25,
			*FString::Printf(TEXT("double accessor F() must round-trip 41.25, not a 4-byte slice; got %f"), Result)));
	}

	TEST_METHOD(CanonicalGeneratedHandleAccessorUsesPointerWidthNotRdr4)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType("CObj", 0, asOBJ_REF) >= 0
			&& ScriptEngine->RegisterObjectBehaviour("CObj", asBEHAVE_ADDREF, "void f()", asFUNCTION(ProdCanonicalHandleAddRef), asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectBehaviour("CObj", asBEHAVE_RELEASE, "void f()", asFUNCTION(ProdCanonicalHandleRelease), asCALL_GENERIC) >= 0,
			TEXT("handle accessor fixture registers a host REF type")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("handle accessor fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FHolder
			{
				CObj Value;
			}

			int F()
			{
				FHolder Object;
				Object.Value = nullptr;
				if (Object.Value == nullptr)
				{
					return 42;
				}
				return 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdGenAccessorHandle",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL handle accessor Build must succeed from CodeGen")));
		asITypeInfo* const TypeInfo = Module != nullptr ? Module->GetTypeInfoByName("FHolder") : nullptr;
		ASSERT_THAT(IsNotNull(TypeInfo, TEXT("handle FHolder must be installed")));
		asIScriptFunction* const Getter = TypeInfo->GetMethodByName("GetValue");
		asIScriptFunction* const Setter = TypeInfo->GetMethodByName("SetValue");
		ASSERT_THAT(IsNotNull(Getter, TEXT("generated GetValue for handle")));
		ASSERT_THAT(IsNotNull(Setter, TEXT("generated SetValue for handle")));
#if AS_PTR_SIZE >= 2
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Getter, asBC_RDR8),
			TEXT("handle GetValue must RDR8 on 64-bit, not hardcoded RDR4")));
		ASSERT_THAT(IsFalse(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Getter, asBC_RDR4),
			TEXT("handle GetValue must not emit 4-byte RDR4 on 64-bit")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Setter, asBC_WRTV8),
			TEXT("handle SetValue must WRTV8 on 64-bit")));
		ASSERT_THAT(IsFalse(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Setter, asBC_WRTV4),
			TEXT("handle SetValue must not emit 4-byte WRTV4 on 64-bit")));
#endif
		int32 Value = 0;
		const bool bExecuted = CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int F()", Value);
		ASSERT_THAT(IsTrue(
			bExecuted && Value == 42,
			*FString::Printf(
				TEXT("handle accessor F() must round-trip nullptr through Get/SetValue; executed=%s value=%d"),
				bExecuted ? TEXT("true") : TEXT("false"),
				Value)));
	}

	TEST_METHOD(CanonicalPackedInt8MemberStoreUsesWrtv1NotWrtv4)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("packed int8 fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FPackedBytes
			{
				int8 A;
				int8 B;
				int8 C;
				int8 D;
			}

			struct FPair16Words
			{
				int16 Lo;
				int16 Hi;
			}

			int RunPacked()
			{
				FPackedBytes p;
				p.A = 1;
				p.B = 2;
				p.C = 3;
				p.D = 4;
				p.B = 9;
				return p.A * 1000 + p.B * 100 + p.C * 10 + p.D;
			}

			int RunPair16()
			{
				FPair16Words p;
				p.Lo = 1;
				p.Hi = 2;
				p.Lo = 9;
				return p.Lo * 100 + p.Hi;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdPackedInt8Width",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL packed int8/int16 Build must succeed from CodeGen")));
		asITypeInfo* const PackedInfo = Module != nullptr ? Module->GetTypeInfoByName("FPackedBytes") : nullptr;
		ASSERT_THAT(IsNotNull(PackedInfo, TEXT("FPackedBytes must be installed")));
		const char* PackedName = nullptr;
		int PackedOffset = -1;
		ASSERT_THAT(AreEqual(0, PackedInfo->GetProperty(1, &PackedName, nullptr, nullptr, nullptr, &PackedOffset),
			TEXT("FPackedBytes.B property query")));
		ASSERT_THAT(AreEqual(1, PackedOffset, TEXT("FPackedBytes.B must stay packed at byte 1, not dword-aligned")));
		asITypeInfo* const PairInfo = Module != nullptr ? Module->GetTypeInfoByName("FPair16Words") : nullptr;
		ASSERT_THAT(IsNotNull(PairInfo, TEXT("FPair16Words must be installed")));
		int PairOffset = -1;
		ASSERT_THAT(AreEqual(0, PairInfo->GetProperty(1, nullptr, nullptr, nullptr, nullptr, &PairOffset),
			TEXT("FPair16Words.Hi property query")));
		ASSERT_THAT(AreEqual(2, PairOffset, TEXT("FPair16Words.Hi must stay packed at byte 2")));

		asIScriptFunction* const SetB = PackedInfo->GetMethodByName("SetB");
		asIScriptFunction* const SetLo = PairInfo->GetMethodByName("SetLo");
		if (SetB != nullptr)
		{
			ASSERT_THAT(IsTrue(
				AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(SetB, asBC_WRTV1),
				TEXT("generated SetB must WRTV1 from sealed 1-byte field, not WRTV4")));
			ASSERT_THAT(IsFalse(
				AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(SetB, asBC_WRTV4),
				TEXT("generated SetB must not smash neighbors with WRTV4")));
		}
		else
		{
			asIScriptFunction* const RunPacked = GetNativeFunctionByDecl(Module, "int RunPacked()");
			ASSERT_THAT(IsTrue(
				AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(RunPacked, asBC_WRTV1),
				TEXT("RunPacked MemberRef store must WRTV1")));
			ASSERT_THAT(IsFalse(
				AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(RunPacked, asBC_WRTV4),
				TEXT("RunPacked must not WRTV4 packed int8 neighbors")));
		}
		if (SetLo != nullptr)
		{
			ASSERT_THAT(IsTrue(
				AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(SetLo, asBC_WRTV2),
				TEXT("generated SetLo must WRTV2 from sealed 2-byte field")));
			ASSERT_THAT(IsFalse(
				AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(SetLo, asBC_WRTV4),
				TEXT("generated SetLo must not smash Hi with WRTV4")));
		}
		int32 PackedValue = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int RunPacked()", PackedValue)
				&& PackedValue == 1934,
			*FString::Printf(
				TEXT("packed int8 neighbor store must execute 1934 (WRTV1), not smash C/D; got=%d"),
				PackedValue)));
		int32 PairValue = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int RunPair16()", PairValue)
				&& PairValue == 902,
			*FString::Printf(
				TEXT("packed int16 neighbor store must execute 902 (WRTV2); got=%d"),
				PairValue)));
	}

	TEST_METHOD(CanonicalGeneratedDestructorBuildPublishesCodeGenAndExecutes)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("generated-destructor fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FValue
			{
				int Value;
				FValue()
				{
					Value = 41;
				}
			}

			int F()
			{
				FValue Object;
				return Object.Value + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdGenDtor",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL generated destructor Build must succeed from CodeGen")));
		asIScriptFunction* const Published = GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(
			Published,
			*FString::Printf(
				TEXT("CANONICAL generated-destructor module must publish int F(); have {%s}"),
				*CollectFunctionDeclarations(Module))));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("do not weaken FValue Object execute 42")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL generated destructor Build must not record asCCompiler")));

		asITypeInfo* const TypeInfo = Module != nullptr ? Module->GetTypeInfoByName("FValue") : nullptr;
		ASSERT_THAT(IsNotNull(TypeInfo, TEXT("FValue must be installed as a module type")));
		asIScriptFunction* Dtor = nullptr;
		if (TypeInfo != nullptr)
		{
			for (asUINT BehaviourIndex = 0; BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
			{
				asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
				asIScriptFunction* const Candidate = TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
				if (Candidate != nullptr && Behaviour == asBEHAVE_DESTRUCT)
				{
					Dtor = Candidate;
					break;
				}
			}
		}
		ASSERT_THAT(IsNotNull(Dtor, TEXT("generated ~FValue must be installed as asBEHAVE_DESTRUCT, not omitted as POD")));
		ASSERT_THAT(IsTrue(Dtor->GetObjectType() == TypeInfo,
			TEXT("generated destructor objectType must be FValue, not a module global")));
		asCModule* const ScriptModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(IsFalse(ProdFunctionIsOnGlobalList(ScriptModule, Dtor),
			TEXT("generated destructor must not appear in module globalFunctionList")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Published, Dtor->GetId()),
			TEXT("F() must CALL the generated destructor; silent DestroyLiveObjects no-op is a false green")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Published, asBC_CALL),
			TEXT("generated destructor call must be asBC_CALL, not CALLSYS of a host recorder")));
	}

	TEST_METHOD(CanonicalListFactoryBuildPublishesCodeGenAndExecutes)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FSemaListBox",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("list factory fixture registers FSemaListBox without @")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FSemaListBox",
				"int Marker",
				asOFFSET(FProdCanonicalListBox, Marker)) >= 0,
			TEXT("list factory fixture exposes Marker")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FSemaListBox",
				asBEHAVE_LIST_FACTORY,
				"FSemaListBox f(int &in) {repeat int}",
				asFUNCTION(ProdCanonicalListFactory),
				asCALL_GENERIC) >= 0,
			TEXT("list factory fixture registers {repeat int} without @")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("list factory fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				FSemaListBox Box = {41};
				return Box.Marker + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdListFactory",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL list factory Build must succeed from CodeGen")));
		asIScriptFunction* const Published = GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(
			Published,
			*FString::Printf(
				TEXT("CANONICAL list factory module must publish int F(); have {%s}"),
				*CollectFunctionDeclarations(Module))));
		int32 Value = 0;
		const bool bExecuted = CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value);
		ASSERT_THAT(IsTrue(
			bExecuted && Value == 42,
			*FString::Printf(
				TEXT("CANONICAL list factory F() must execute Box.Marker+1 from CodeGen; executed=%s value=%d"),
				bExecuted ? TEXT("true") : TEXT("false"),
				Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL list factory Build must not record asCCompiler")));

		asITypeInfo* const TypeInfo = ScriptEngine->GetTypeInfoByName("FSemaListBox");
		asIScriptFunction* ListFactory = nullptr;
		if (TypeInfo != nullptr)
		{
			for (asUINT BehaviourIndex = 0; BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
			{
				asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
				asIScriptFunction* const Candidate = TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
				if (Candidate != nullptr && Behaviour == asBEHAVE_LIST_FACTORY)
				{
					ListFactory = Candidate;
					break;
				}
			}
		}
		ASSERT_THAT(IsNotNull(ListFactory, TEXT("registered list factory must be findable on FSemaListBox")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Published, ListFactory->GetId()),
			TEXT("F() must CALLSYS the registered list factory id, not a 0-arg factory/ALLOC that never sees {41}")));
	}

	TEST_METHOD(CanonicalListFactoryDoubleUsesWrtv8NotWrtv4)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FSemaDoubleListBox",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("double list factory registers FSemaDoubleListBox")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FSemaDoubleListBox",
				"int Marker",
				asOFFSET(FProdCanonicalListBox, Marker)) >= 0,
			TEXT("double list factory exposes Marker")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FSemaDoubleListBox",
				asBEHAVE_LIST_FACTORY,
				"FSemaDoubleListBox f(int &in) {repeat double}",
				asFUNCTION(ProdCanonicalDoubleListFactory),
				asCALL_GENERIC) >= 0,
			TEXT("double list factory registers {repeat double}")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("double list factory fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				FSemaDoubleListBox Box = {41.25};
				return Box.Marker + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdListFactoryDbl",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL double list factory Build must succeed from CodeGen")));
		asIScriptFunction* const Published = GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(Published, TEXT("double list factory must publish int F()")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Published, asBC_WRTV8),
			TEXT("double list buffer store must WRTV8 from sealed element size, not hardcoded WRTV4")));
		ASSERT_THAT(IsFalse(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Published, asBC_WRTV4),
			TEXT("double list buffer store must not emit 4-byte WRTV4")));
		int32 Value = 0;
		const bool bExecuted = CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value);
		ASSERT_THAT(IsTrue(
			bExecuted && Value == 42,
			*FString::Printf(
				TEXT("double list factory must store 41.25 as 8 bytes so Marker==41; executed=%s value=%d"),
				bExecuted ? TEXT("true") : TEXT("false"),
				Value)));
	}

	TEST_METHOD(CanonicalAggregateListCommitsEachElementAndUnwindsOnlyCommittedPrefix)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"FProdAggregateElement",
			sizeof(FProdAggregateElement),
			asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CD
				| asOBJ_APP_CLASS_ALLINTS) >= 0,
			TEXT("aggregate element must declare trivial storage-copy ABI plus exact cleanup")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
			"FProdAggregateElement",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ProdAggregateElementConstruct),
			asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
			"FProdAggregateElement",
			asBEHAVE_DESTRUCT,
			"void f()",
			asFUNCTION(ProdAggregateElementDestruct),
			asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectProperty(
			"FProdAggregateElement",
			"int Id",
			asOFFSET(FProdAggregateElement, Id)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"FProdAggregateElement MakeAggregateElement(int, bool)",
			asFUNCTION(ProdMakeAggregateElement),
			asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"void SetAggregateFactoryFailure(bool)",
			asFUNCTION(ProdSetAggregateFactoryFailure),
			asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"FProdAggregateListBox",
			0,
			asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectProperty(
			"FProdAggregateListBox",
			"int Marker",
			asOFFSET(FProdAggregateListBox, Marker)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
			"FProdAggregateListBox",
			asBEHAVE_LIST_FACTORY,
			"FProdAggregateListBox f(int &in) {repeat FProdAggregateElement}",
			asFUNCTION(ProdAggregateListFactory),
			asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Run(const bool FailFirst, const bool FailSecond,
				const bool FailFactory)
			{
				SetAggregateFactoryFailure(FailFactory);
				FProdAggregateListBox Box = {
					MakeAggregateElement(1, FailFirst),
					MakeAggregateElement(2, FailSecond),
					MakeAggregateElement(3, false)
				};
				return Box.Marker;
			}
			)AS");
		ASSERT_THAT(AreEqual(asSUCCESS, CompileNativeModule(
			Engine.Get(),
			"ProdAggregateCommittedPrefix",
			ScriptSource.c_str(),
			Module),
			TEXT("aggregate committed-prefix fixture must compile through Canonical CodeGen")));
		asCScriptFunction* const Entry = static_cast<asCScriptFunction*>(
			Module != nullptr
				? Module->GetFunctionByDecl(
					"int Run(const bool, const bool, const bool)")
				: nullptr);
		ASSERT_THAT(IsNotNull(Entry));
		if (Entry == nullptr || Entry->scriptData == nullptr)
		{
			return;
		}

		TArray<asUINT> CommittedSizes;
		for (asUINT Offset = 0;
			Offset < Entry->scriptData->byteCode.GetLength(); )
		{
			asDWORD* const Instruction =
				Entry->scriptData->byteCode.AddressOf() + Offset;
			const asEBCInstr Opcode = static_cast<asEBCInstr>(
				*reinterpret_cast<const asBYTE*>(Instruction));
			if (Opcode == asBC_SetListSize)
			{
				CommittedSizes.Add(asBC_DWORDARG(Instruction + 1));
			}
			Offset += static_cast<asUINT>(
				asBCTypeSize[asBCInfo[Opcode].type]);
		}
		const bool bHasCommittedCursor = CommittedSizes.Num() == 7
			&& CommittedSizes[0] == 0
			&& CommittedSizes[1] == 1
			&& CommittedSizes[2] == 2
			&& CommittedSizes[3] == 3
			&& CommittedSizes[4] == 2
			&& CommittedSizes[5] == 1
			&& CommittedSizes[6] == 0;
		ASSERT_THAT(IsTrue(
			bHasCommittedCursor,
			*FString::Printf(
				TEXT("list buffer must publish commits 0,1,2,3 then retire 2,1,0; bytecode {%s}"),
				*ProdBytecodeImmediates(Entry))));
		if (!bHasCommittedCursor)
		{
			// Keep the RED run memory-safe. The old emitter publishes the full count
			// before element evaluation and has no tracked exception cleanup for the
			// list buffer, so executing its failure cases would deliberately leak.
			return;
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ON_SCOPE_EXIT
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
		};
		if (Context == nullptr)
		{
			return;
		}

		auto Execute = [&](asIScriptFunction* const Function,
			const bool bFailFirst, const bool bFailSecond,
			const bool bFailFactory)
		{
			GProdAggregateFactoryShouldFail = false;
			GProdAggregateFactoryCalls = 0;
			GProdAggregateReturnStorage.Reset();
			GProdAggregateBufferDestructionOrder.Reset();
			GProdAggregateTemporaryDestructionOrder.Reset();
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->SetArgByte(0, bFailFirst ? 1 : 0)));
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->SetArgByte(1, bFailSecond ? 1 : 0)));
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->SetArgByte(2, bFailFactory ? 1 : 0)));
			const int Status = Context->Execute();
			if (bFailFirst || bFailSecond || bFailFactory)
			{
				ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Status));
			}
			else
			{
				ASSERT_THAT(IsTrue(Status == asEXECUTION_FINISHED
					&& Context->GetReturnDWord() == 123,
					TEXT("the list factory must observe the complete committed aggregate")));
			}
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare()));
		};

		Execute(Entry, false, false, false);
		ASSERT_THAT(IsTrue(
			GProdAggregateFactoryCalls == 1
				&& GProdAggregateBufferDestructionOrder == TArray<int32>({3, 2, 1}),
			TEXT("normal list-buffer cleanup must destroy all committed elements in reverse order")));

		Execute(Entry, true, false, false);
		ASSERT_THAT(IsTrue(
			GProdAggregateFactoryCalls == 0
				&& GProdAggregateBufferDestructionOrder.IsEmpty(),
			TEXT("first-element failure must retain a zero committed cursor and destroy no buffer element")));

		Execute(Entry, false, true, false);
		ASSERT_THAT(IsTrue(
			GProdAggregateFactoryCalls == 0
				&& GProdAggregateBufferDestructionOrder == TArray<int32>({1}),
			TEXT("middle-element failure must destroy only the first committed buffer element")));

		Execute(Entry, false, false, true);
		ASSERT_THAT(IsTrue(
			GProdAggregateFactoryCalls == 1
				&& GProdAggregateBufferDestructionOrder == TArray<int32>({3, 2, 1}),
			TEXT("list-factory failure must unwind the complete buffer prefix in reverse order")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(
			0,
			static_cast<asCModule*>(Module)->GetLastLegacyCompilerInvocationCount()));

		FMemoryBinaryStream ModuleBytecode;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Module->SaveByteCode(&ModuleBytecode, false),
			TEXT("complete module persistence must encode aggregate cleanup through stable type/function identities")));
		ASSERT_THAT(IsTrue(ModuleBytecode.Num() > 0));
		ModuleBytecode.ResetReadPosition();
		asIScriptModule* const RestoredModule = ScriptEngine->GetModule(
			"ProdAggregateCommittedPrefixRestored",
			asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(RestoredModule));
		bool bDebugWasStripped = true;
		const int LoadResult = RestoredModule != nullptr
			? RestoredModule->LoadByteCode(&ModuleBytecode, &bDebugWasStripped)
			: asNO_MODULE;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			LoadResult,
			*FString::Printf(
				TEXT("complete module persistence must restore aggregate cleanup through stable type/function identities; result=%d messages:\n%s"),
				LoadResult,
				*Engine.GetMessagesText())));
		ASSERT_THAT(IsFalse(bDebugWasStripped));
		ASSERT_THAT(IsFalse(ModuleBytecode.HasReadError()));
		asCScriptFunction* const RestoredEntry = static_cast<asCScriptFunction*>(
			RestoredModule != nullptr
				? RestoredModule->GetFunctionByDecl(
					"int Run(const bool, const bool, const bool)")
				: nullptr);
		ASSERT_THAT(IsNotNull(RestoredEntry));
		const bool bRestoredAggregateAuthority = RestoredEntry != nullptr
			&& RestoredEntry->scriptData != nullptr
			&& RestoredEntry->scriptData->aggregateCleanupInfo.GetLength() == 1;
		ASSERT_THAT(IsTrue(
			bRestoredAggregateAuthority,
			TEXT("LoadByteCode must restore the exact aggregate cleanup directory before exceptional execution")));
		if (!bRestoredAggregateAuthority)
		{
			// Keep the RED memory-safe. Executing the failure path without the
			// authenticated aggregate directory would deliberately leak or apply the
			// legacy list-pattern heuristic to a partially initialized buffer.
			return;
		}

		const asSAggregateCleanupInfo& RestoredCleanup =
			RestoredEntry->scriptData->aggregateCleanupInfo[0];
		ASSERT_THAT(IsTrue(
			RestoredCleanup.bufferOffset > 0
				&& RestoredCleanup.elementType
					== ScriptEngine->GetTypeInfoByDecl("FProdAggregateElement")
				&& RestoredCleanup.actionFunctionId > 0
				&& RestoredCleanup.committedCountOffset == 0
				&& RestoredCleanup.firstElementOffset > 0
				&& RestoredCleanup.elementStride == sizeof(FProdAggregateElement)
				&& RestoredCleanup.maxElementCount == 3,
			TEXT("restored aggregate cleanup must retain its exact current-generation frame layout and action")));

		auto HasUniqueAggregateHeapEntry = [](const asCScriptFunction* const Function,
			const int BufferOffset)
		{
			if (Function == nullptr || Function->scriptData == nullptr
				|| Function->scriptData->objVariablePos.GetLength()
					!= Function->scriptData->objVariableTypes.GetLength()
				|| Function->scriptData->objVariablesOnHeap
					> Function->scriptData->objVariablePos.GetLength())
			{
				return false;
			}

			asUINT MatchingEntries = 0;
			for (asUINT Index = 0;
				Index < Function->scriptData->objVariablesOnHeap;
				++Index)
			{
				asCTypeInfo* const Type =
					Function->scriptData->objVariableTypes[Index];
				if (Function->scriptData->objVariablePos[Index] == BufferOffset
					&& Type != nullptr
					&& (Type->GetFlags() & asOBJ_LIST_PATTERN) != 0)
				{
					++MatchingEntries;
				}
			}
			return MatchingEntries == 1;
		};
		const bool bRestoredHeapDirectory = HasUniqueAggregateHeapEntry(
			RestoredEntry,
			RestoredCleanup.bufferOffset);
		ASSERT_THAT(IsTrue(
			bRestoredHeapDirectory,
			TEXT("LoadByteCode must rebuild one typed heap-cleanup entry for the aggregate buffer before VM liveness analysis")));
		if (!bRestoredHeapDirectory)
		{
			// Keep the structural RED memory-safe: objVariableInfo contains the
			// aggregate INIT edge, so DetermineLiveObjects cannot process it until
			// the restored heap directory names the same buffer exactly once.
			return;
		}

		Execute(RestoredEntry, false, true, false);
		ASSERT_THAT(IsTrue(
			GProdAggregateFactoryCalls == 0
				&& GProdAggregateBufferDestructionOrder == TArray<int32>({1}),
			TEXT("restored middle-element failure must destroy only the first committed buffer element")));
		Execute(RestoredEntry, false, false, true);
		ASSERT_THAT(IsTrue(
			GProdAggregateFactoryCalls == 1
				&& GProdAggregateBufferDestructionOrder == TArray<int32>({3, 2, 1}),
			TEXT("restored factory failure must unwind the complete aggregate prefix in reverse order")));

		FMemoryBinaryStream FunctionArtifact;
		asCWriter ArtifactWriter(
			static_cast<asCModule*>(Module),
			&FunctionArtifact,
			ScriptEngine,
			true);
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ArtifactWriter.WriteFunctionArtifact(Entry),
			TEXT("detached artifact V6 must encode aggregate cleanup through semantic tables")));
		ASSERT_THAT(IsTrue(FunctionArtifact.Num() > 0));
		FunctionArtifact.ResetReadPosition();
		asCModule* const ArtifactModule = static_cast<asCModule*>(
			ScriptEngine->GetModule(
				"ProdAggregateCommittedPrefixArtifact",
				asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(ArtifactModule));
		asCScriptFunction* ArtifactEntry = nullptr;
		asCReader ArtifactReader(
			ArtifactModule,
			&FunctionArtifact,
			ScriptEngine);
		const int ArtifactRestoreResult = ArtifactReader.RestoreGlobalFunctionArtifact(
			static_cast<asUINT>(FunctionArtifact.Num()),
			&ArtifactEntry);
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ArtifactRestoreResult,
			TEXT("detached artifact V6 must reconstruct aggregate cleanup before publication")));
		const bool bArtifactAggregateAuthority = ArtifactEntry != nullptr
			&& ArtifactEntry->scriptData != nullptr
			&& ArtifactEntry->scriptData->aggregateCleanupInfo.GetLength() == 1
			&& HasUniqueAggregateHeapEntry(
				ArtifactEntry,
				ArtifactEntry->scriptData->aggregateCleanupInfo[0].bufferOffset);
		ASSERT_THAT(IsTrue(
			bArtifactAggregateAuthority,
			TEXT("published detached artifact must own one authenticated aggregate cleanup and heap-directory entry")));
		if (bArtifactAggregateAuthority)
		{
			Execute(ArtifactEntry, false, true, false);
			ASSERT_THAT(IsTrue(
				GProdAggregateFactoryCalls == 0
					&& GProdAggregateBufferDestructionOrder == TArray<int32>({1}),
				TEXT("detached artifact middle-element failure must preserve committed-prefix cleanup")));
		}

		const asUINT SavedMaxElementCount =
			Entry->scriptData->aggregateCleanupInfo[0].maxElementCount;
		Entry->scriptData->aggregateCleanupInfo[0].maxElementCount = 2;
		FMemoryBinaryStream ForgedArtifact;
		asCWriter ForgedWriter(
			static_cast<asCModule*>(Module),
			&ForgedArtifact,
			ScriptEngine,
			true);
		const int ForgedWriteResult =
			ForgedWriter.WriteFunctionArtifact(Entry);
		Entry->scriptData->aggregateCleanupInfo[0].maxElementCount =
			SavedMaxElementCount;
		ASSERT_THAT(IsTrue(
			ForgedWriteResult < 0,
			TEXT("aggregate candidate with a forged structural count must fail closed before artifact publication")));
	}

	TEST_METHOD(CanonicalConstGlobalOnlyBuildInstallsOrFailsClosed)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("global-only F1 fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			const int G = 41;
			)AS");
		const int BuildResult = AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdConstGlobalOnly",
			ScriptSource.c_str(),
			Module);

		bool bGReadable = false;
		if (BuildResult == 0 && Module != nullptr)
		{
			for (asUINT Index = 0; Index < Module->GetGlobalVarCount(); ++Index)
			{
				const char* Name = nullptr;
				Module->GetGlobalVar(Index, &Name);
				if (Name == nullptr || FCStringAnsi::Strcmp(Name, "G") != 0)
				{
					continue;
				}
				int* const Address = static_cast<int*>(Module->GetAddressOfGlobalVar(Index));
				bGReadable = Address != nullptr && *Address == 41;
				break;
			}
			ASSERT_THAT(IsTrue(bGReadable,
				TEXT("CANONICAL global-only Build()==0 is forbidden unless G is installed and readable as 41")));
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
				static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
				TEXT("successful global-only install must be CodeGen, not asCCompiler")));
		}
		else
		{
			ASSERT_THAT(IsTrue(BuildResult < 0,
				TEXT("fail-closed is allowed when G cannot be installed")));
			if (Module != nullptr)
			{
				ASSERT_THAT(IsTrue(
					static_cast<asCModule*>(Module)->GetLastBytecodePublisher() != asBYTECODE_PUBLISHER_COMPILER,
					TEXT("rejected global-only module must not claim asCCompiler publication")));
			}
		}
	}

	TEST_METHOD(CanonicalNamespaceFunctionBuildsAndPublishesExactRuntimeNamespace)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("namespace function fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			namespace Tools::Utilities
			{
				const int Value = 41;

				int Entry()
				{
					return Value;
				}
			}
			)AS");
		const int BuildResult = CompileNativeModule(
			Engine.Get(),
			"ProdNamespaceFunction",
			ScriptSource.c_str(),
			Module);

		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("CANONICAL Build must support namespace declaration ownership")));
		ASSERT_THAT(IsNotNull(Module,
			TEXT("successful namespaced CANONICAL Build must return its module")));

		asIScriptFunction* const Namespaced = Module->GetFunctionByDecl("int Tools::Utilities::Entry()");
			asIScriptFunction* const Root = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Namespaced,
			TEXT("CANONICAL Build must publish Tools::Utilities::Entry in its source namespace")));
		ASSERT_THAT(IsNull(Root,
			TEXT("CANONICAL Tools::Utilities::Entry must not be silently published as root Entry")));
		bool bFoundNamespacedValue = false;
		for (asUINT Index = 0; Index < Module->GetGlobalVarCount(); ++Index)
		{
			const char* Name = nullptr;
			const char* NameSpace = nullptr;
			Module->GetGlobalVar(Index, &Name, &NameSpace);
			if (Name == nullptr || NameSpace == nullptr
				|| FCStringAnsi::Strcmp(Name, "Value") != 0
				|| FCStringAnsi::Strcmp(NameSpace, "Tools::Utilities") != 0)
			{
				continue;
			}
			int* const Address = static_cast<int*>(Module->GetAddressOfGlobalVar(Index));
			bFoundNamespacedValue = Address != nullptr && *Address == 41;
			break;
		}
		ASSERT_THAT(IsTrue(bFoundNamespacedValue,
			TEXT("CANONICAL namespace global must retain its exact runtime namespace and sealed constant value")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Tools::Utilities::Entry()", Value) && Value == 41,
			TEXT("CANONICAL Tools::Utilities::Entry must execute through its namespace-qualified declaration")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("successful namespaced Build must be published by Canonical CodeGen")));
	}

	TEST_METHOD(CanonicalNamespaceValueObjectMethodBuildsAndPublishesExactRuntimeNamespace)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("namespace value-object fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			namespace Tools::Utilities
			{
				struct FValue
				{
					int Value;

					FValue()
					{
						Value = 41;
					}

					int Read()
					{
						return Value;
					}
				}

				int Entry()
				{
					FValue Object;
					return Object.Read();
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdNamespaceValueObject",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL Build must retain namespace ownership for a script value type and its methods")));
		ASSERT_THAT(IsNotNull(Module,
			TEXT("successful namespaced value-object CANONICAL Build must return its module")));

		asITypeInfo* const TypeInfo = Module->GetTypeInfoByDecl("Tools::Utilities::FValue");
		ASSERT_THAT(IsNotNull(TypeInfo,
			TEXT("CANONICAL FValue must be queryable from its exact namespace-qualified declaration")));
		ASSERT_THAT(IsTrue(FCStringAnsi::Strcmp(TypeInfo->GetNamespace(), "Tools::Utilities") == 0,
			TEXT("CANONICAL FValue must not be silently published in the root namespace")));
		ASSERT_THAT(IsNotNull(TypeInfo->GetMethodByDecl("int Read()"),
			TEXT("CANONICAL namespace FValue must retain its member-method ownership")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Tools::Utilities::Entry()", Value) && Value == 41,
			TEXT("CANONICAL Tools::Utilities::Entry must construct and dispatch the namespace value-object method")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("successful namespaced value-object Build must be published by Canonical CodeGen")));
	}

	TEST_METHOD(CanonicalTypeOnlyModulePublishesCodeGenAtomically)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("type-only fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FTypeOnly
			{
				int Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdTypeOnly",
			ScriptSource.c_str(),
			Module),
			TEXT("a CANONICAL type-only module must build rather than return a partially installed success")));
		ASSERT_THAT(IsNotNull(Module,
			TEXT("a successful CANONICAL type-only module must return its module")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("FTypeOnly"),
			TEXT("a successful CANONICAL type-only module must publish FTypeOnly")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("a type-only Canonical Build must atomically commit and identify CodeGen as its publisher")));
		ASSERT_THAT(AreEqual(
			(asUINT)0,
			static_cast<asCModule*>(Module)->GetLastLegacyCompilerInvocationCount(),
			TEXT("a type-only Canonical Build must not route through asCCompiler")));
	}

	TEST_METHOD(CanonicalValueObjectCtorIsOwnedAndNotGlobal)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("value-object owner fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FValue
			{
				int Value;
				FValue()
				{
					Value = 41;
				}
			}

			int F()
			{
				FValue Object;
				return Object.Value + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdValueOwner",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL value-object Build must succeed from CodeGen, not fail closed")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("do not weaken FValue Object execute 42")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL value-object Build must not record asCCompiler")));

		asCModule* const ScriptModule = static_cast<asCModule*>(Module);
		asIScriptFunction* const GlobalF = Module->GetFunctionByDecl("int F()");
		ASSERT_THAT(IsTrue(GlobalF != nullptr,
			TEXT("int F() must be the unique global lookup")));
		int32 FMatches = 0;
		for (asUINT Index = 0; Index < ScriptModule->globalFunctionList.GetLength(); ++Index)
		{
			asCScriptFunction* const Func = ScriptModule->globalFunctionList[Index];
			if (Func != nullptr && Func->name == "F"
				&& Func->objectType == 0
				&& Func->parameterTypes.GetLength() == 0)
			{
				++FMatches;
			}
		}
		ASSERT_THAT(AreEqual(1, FMatches,
			TEXT("GetFunctionByDecl(\"int F()\") must be unique on the global list")));
		ASSERT_THAT(IsTrue(static_cast<asCScriptFunction*>(GlobalF)->objectType == 0,
			TEXT("int F() stays a module global, not a member")));

		asCObjectType* ObjectType = nullptr;
		for (asUINT Index = 0; Index < ScriptModule->classTypes.GetLength(); ++Index)
		{
			asCObjectType* const Candidate = ScriptModule->classTypes[Index];
			if (Candidate != nullptr && Candidate->name == "FValue")
			{
				ObjectType = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsTrue(ObjectType != nullptr,
			TEXT("FValue must be installed as a module class type")));

		asCScriptFunction* Ctor = nullptr;
		if (ObjectType->beh.construct > 0
			&& static_cast<asUINT>(ObjectType->beh.construct) < ScriptEngine->scriptFunctions.GetLength())
		{
			Ctor = ScriptEngine->scriptFunctions[ObjectType->beh.construct];
		}
		ASSERT_THAT(IsTrue(Ctor != nullptr,
			TEXT("FValue 0-arg ctor must be bound on objectType->beh.construct")));
		ASSERT_THAT(IsTrue(Ctor->objectType == ObjectType,
			TEXT("ctor objectType must be FValue, not null / default namespace global")));

		bool bCtorOnGlobalList = false;
		for (asUINT Index = 0; Index < ScriptModule->globalFunctionList.GetLength(); ++Index)
		{
			if (ScriptModule->globalFunctionList[Index] == Ctor)
			{
				bCtorOnGlobalList = true;
				break;
			}
		}
		ASSERT_THAT(IsFalse(bCtorOnGlobalList,
			TEXT("ctor must not appear in module globalFunctionList")));
	}

	TEST_METHOD(CanonicalEnumOnlyBuildPublishesSealedConstantWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("enum-only F1 fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			enum EProd
			{
				Value = 41
			}
			)AS");
		const int BuildResult = AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdEnumOnly",
			ScriptSource.c_str(),
			Module);

		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("the sealed enum constant is a complete Canonical input and must not need legacy compilation")));
		ASSERT_THAT(IsNotNull(Module,
			TEXT("successful Canonical enum-only Build must return its module")));
		if (Module == nullptr)
		{
			return;
		}

		asITypeInfo* const TypeInfo = Module->GetTypeInfoByName("EProd");
		int EnumValue = 0;
		const char* EnumName = nullptr;
		if (TypeInfo != nullptr && TypeInfo->GetEnumValueCount() > 0)
		{
			EnumName = TypeInfo->GetEnumValueByIndex(0, &EnumValue);
		}
		ASSERT_THAT(IsTrue(TypeInfo != nullptr
				&& EnumName != nullptr
				&& FCStringAnsi::Strcmp(EnumName, "Value") == 0
				&& EnumValue == 41,
			TEXT("Canonical CodeGen must install EProd::Value from the sealed VarDecl.constantValue=41")));

		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("enum-only publication must come from Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			(asUINT)0,
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("explicit enum initialization must not construct asCCompiler")));
		asQWORD CanonicalDigest = 0;
		ASSERT_THAT(IsTrue(
			InternalModule->GetLastCanonicalCodeGenAstDigest(CanonicalDigest)
				&& CanonicalDigest != 0,
			TEXT("enum-only publication must retain the exact sealed-AST digest")));
	}

	TEST_METHOD(CanonicalCapturingLambdaBuildPublishesCodeGenAndExecutes)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("capturing-lambda fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				int X = 41;
				return function()
				{
					return X + 1;
				}();
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdCaptureLambda",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL capturing lambda Build must succeed from CodeGen")));
		asIScriptFunction* const Published = GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(
			Published,
			*FString::Printf(
				TEXT("CANONICAL capturing lambda must publish int F(); have {%s}"),
				*CollectFunctionDeclarations(Module))));
		int32 Value = 0;
		const bool bExecuted = CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value);
		ASSERT_THAT(IsTrue(
			bExecuted && Value == 42,
			*FString::Printf(
				TEXT("CANONICAL capturing lambda F() must read enclosing X=41; executed=%s value=%d"),
				bExecuted ? TEXT("true") : TEXT("false"),
				Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL capturing lambda Build must not record asCCompiler")));
	}

	TEST_METHOD(CanonicalSiblingLambdasEachCaptureTheSameEnclosingX)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("sibling-capture fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				int X = 21;
				int A = function()
				{
					return X;
				}();
				int B = function()
				{
					return X;
				}();
				return A + B;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdSiblingCapture",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL sibling capturing lambdas Build must succeed from CodeGen")));
		asCModule* const ScriptModule = static_cast<asCModule*>(Module);
		int32 LambdaCount = 0;
		int32 LambdasWithCapture = 0;
		TArray<int32> LambdaIds;
		if (ScriptModule != nullptr)
		{
			for (asUINT Index = 0; Index < ScriptModule->scriptFunctions.GetLength(); ++Index)
			{
				asCScriptFunction* const Func = ScriptModule->scriptFunctions[Index];
				if (Func == nullptr
					|| Func->artifactInvocationKind
						!= asBUILD_ARTIFACT_INVOCATION_LAMBDA)
				{
					continue;
				}
				++LambdaCount;
				LambdaIds.Add(Func->GetId());
				if (Func->parameterTypes.GetLength() == 1)
				{
					++LambdasWithCapture;
				}
			}
		}
		ASSERT_THAT(AreEqual(2, LambdaCount,
			TEXT("F() must intern two sibling Runtime lambda functions")));
		ASSERT_THAT(AreEqual(2, LambdasWithCapture,
			TEXT("each sibling lambda must keep its own hidden capture of X; whole-module uniquing drops the second")));
		asIScriptFunction* const Published = GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(Published, TEXT("CANONICAL sibling capturing lambdas must publish int F()")));
		ASSERT_THAT(AreEqual(2, LambdaIds.Num(),
			TEXT("two interned Runtime lambda ids are required before CALL identity")));
		ASSERT_THAT(IsTrue(
			LambdaIds[0] != LambdaIds[1],
			TEXT("sibling IIFEs must bind two distinct function ids")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Published, LambdaIds[0])
				&& ProdBytecodeCallsFunction(Published, LambdaIds[1]),
			TEXT("F() must CALL both sibling lambdas; a shared ASSIGN intern emits X=21 three times and never CALL")));
		int32 Value = 0;
		const bool bExecuted = CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value);
		ASSERT_THAT(IsTrue(
			bExecuted && Value == 42,
			*FString::Printf(
				TEXT("sibling lambdas must each read X=21 so A+B=42; executed=%s value=%d"),
				bExecuted ? TEXT("true") : TEXT("false"),
				Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("sibling-capture Build must not record asCCompiler")));
	}

	TEST_METHOD(CanonicalOverloadBuildPublishesSelectedCalleeNotFirstName)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("overload identity fixture selects CANONICAL")));

		asIScriptModule* const Module = ScriptEngine->GetModule("ProdOverload", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("ProdOverload module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F(float a)
			{
				return 0;
			}

			int F(int a)
			{
				return 42;
			}

			int Entry()
			{
				return F(3);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection("ProdOverload.as", ScriptSource.c_str())));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("CANONICAL overload Build must succeed from CodeGen")));

		asCString Dump;
		ASSERT_THAT(IsTrue(static_cast<asCModule*>(Module)->GetCanonicalASTContext() != nullptr,
			TEXT("retain policy must keep the sealed AST CodeGen consumed")));
		asCASTDump(*static_cast<asCModule*>(Module)->GetCanonicalASTContext(), Dump);
		const FString Text = CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("callee=F(int)")),
			*FString::Printf(TEXT("sealed dump must select F(int), not first-name F(float). dump:\n%s"), *Text)));
		ASSERT_THAT(IsTrue(!Text.Contains(TEXT("callee=F(float)")),
			*FString::Printf(TEXT("integer literal must not bind F(float). dump:\n%s"), *Text)));

		asIScriptFunction* const Entry = GetNativeFunctionByDecl(Module, "int Entry()");
		asIScriptFunction* const IntF = GetNativeFunctionByDecl(Module, "int F(const int)");
		asIScriptFunction* const FloatF = GetNativeFunctionByDecl(Module, "int F(const float)");
		ASSERT_THAT(IsNotNull(Entry, TEXT("must publish int Entry()")));
		ASSERT_THAT(IsNotNull(IntF, TEXT("must publish int F(int)")));
		ASSERT_THAT(IsNotNull(FloatF, TEXT("must publish int F(float)")));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Entry, IntF->GetId()),
			TEXT("CodeGen must CALL the dump-selected int overload, not re-resolve by first name")));
		ASSERT_THAT(IsTrue(
			!ProdBytecodeCallsFunction(Entry, FloatF->GetId()),
			TEXT("CodeGen must not CALL the first-declared float overload")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Entry()", Value) && Value == 42,
			*FString::Printf(TEXT("selected int overload must execute 42; value=%d"), Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("overload identity must be published by CodeGen, not asCCompiler")));
	}

	TEST_METHOD(CanonicalScriptClassRegistersRefImplicitHandleNotValue)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("class vs struct flags fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class ActorLike
			{
				int Value;
			}

			struct FValue
			{
				int Value;
			}

			int F()
			{
				return 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdClassRefFlags",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL class/struct flags Build must succeed from CodeGen")));

		asITypeInfo* const ClassInfo = Module != nullptr ? Module->GetTypeInfoByName("ActorLike") : nullptr;
		asITypeInfo* const StructInfo = Module != nullptr ? Module->GetTypeInfoByName("FValue") : nullptr;
		ASSERT_THAT(IsNotNull(ClassInfo, TEXT("ActorLike must be registered")));
		ASSERT_THAT(IsNotNull(StructInfo, TEXT("FValue must be registered")));

		const asDWORD ClassFlags = ClassInfo->GetFlags();
		const asDWORD StructFlags = StructInfo->GetFlags();
		const FString ClassMsg = FString::Printf(
			TEXT("script class ActorLike must be asOBJ_REF|asOBJ_IMPLICIT_HANDLE, not asOBJ_VALUE. flags=0x%x"),
			ClassFlags);
		const FString StructMsg = FString::Printf(
			TEXT("script struct FValue must stay asOBJ_VALUE|asOBJ_NOINHERIT. flags=0x%x"),
			StructFlags);
		ASSERT_THAT(IsTrue((ClassFlags & asOBJ_REF) != 0, *ClassMsg));
		ASSERT_THAT(IsTrue((ClassFlags & asOBJ_IMPLICIT_HANDLE) != 0, *ClassMsg));
		ASSERT_THAT(IsTrue((ClassFlags & asOBJ_VALUE) == 0, *ClassMsg));
		ASSERT_THAT(IsTrue((StructFlags & asOBJ_VALUE) != 0, *StructMsg));
		ASSERT_THAT(IsTrue((StructFlags & asOBJ_REF) == 0, *StructMsg));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("class/struct flags Build must be published by CodeGen")));
	}

	TEST_METHOD(CanonicalUserCtorMemberDefaultFortyPlusOneExecutesBeforeBody)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("user-ctor InitPlan execute fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FValue
			{
				int Value = 40 + 1;

				FValue()
				{
					Value = Value + 1;
				}
			}

			int Entry()
			{
				FValue Object;
				return Object.Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdUserCtorInitPlan",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL user-ctor member default Build must succeed from CodeGen")));
		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int Entry()");
		asITypeInfo* const TypeInfo = Module != nullptr ? Module->GetTypeInfoByName("FValue") : nullptr;
		asIScriptFunction* FirstCtor = nullptr;
		asIScriptFunction* Ctor = nullptr;
		FString ConstructIds;
		if (TypeInfo != nullptr)
		{
			for (asUINT BehaviourIndex = 0; BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
			{
				asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
				asIScriptFunction* const Candidate = TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
				if (Candidate == nullptr || Behaviour != asBEHAVE_CONSTRUCT)
				{
					continue;
				}
				if (FirstCtor == nullptr)
				{
					FirstCtor = Candidate;
				}
				ConstructIds += FString::Printf(
					TEXT(" id=%d funcType=%d name=%s"),
					Candidate->GetId(),
					static_cast<int>(Candidate->GetFuncType()),
					UTF8_TO_TCHAR(Candidate->GetName() ? Candidate->GetName() : ""));
				if (Ctor == nullptr && Candidate->GetFuncType() == asFUNC_SCRIPT)
				{
					Ctor = Candidate;
				}
			}
		}
		if (Ctor == nullptr)
		{
			Ctor = FirstCtor;
		}
		asCString CtorDump;
		asCString EntryDump;
		asCBytecodeCodeGenDumpFunction(static_cast<asCScriptFunction*>(Ctor), CtorDump);
		asCBytecodeCodeGenDumpFunction(static_cast<asCScriptFunction*>(Published), EntryDump);
		const int CtorId = Ctor != nullptr ? Ctor->GetId() : 0;
		const bool EntryCallsUserCtor = ProdBytecodeCallsFunction(Published, CtorId);
		const FString EntryCallTargets = ProdBytecodeCallTargets(Published);
		const FString CtorImmediates = ProdBytecodeImmediates(Ctor);
		int32 Value = 0;
		const bool Executed = AngelscriptNativeTestSupport::CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int Entry()", Value);
		ASSERT_THAT(IsTrue(
			EntryCallsUserCtor && Executed && Value == 42,
			*FString::Printf(
				TEXT("typed 40+1 then user Value=Value+1 must execute 42, not atoi(40)+1=41; got=%d ctorId=%d entryCallsUserCtor=%d constructs{%s} entryCalls{%s} ctorImm{%s} ctor {%s} entry {%s}"),
				Value,
				CtorId,
				EntryCallsUserCtor ? 1 : 0,
				*ConstructIds,
				*EntryCallTargets,
				*CtorImmediates,
				UTF8_TO_TCHAR(CtorDump.AddressOf() ? CtorDump.AddressOf() : ""),
				UTF8_TO_TCHAR(EntryDump.AddressOf() ? EntryDump.AddressOf() : ""))));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("user-ctor InitPlan Build must be published by CodeGen")));
	}

	TEST_METHOD(CanonicalNativeMemberRefUsesSealedFieldNotFirstProperty)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdFields",
				sizeof(FProdFields),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_CONSTRUCTOR) >= 0,
			TEXT("FProdFields VALUE POD")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdFields",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdFieldsConstruct),
				asCALL_GENERIC) >= 0,
			TEXT("FProdFields construct")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FProdFields",
				"int Poison",
				asOFFSET(FProdFields, Poison)) >= 0,
			TEXT("Poison first")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FProdFields",
				"int Stored",
				asOFFSET(FProdFields, Stored)) >= 0,
			TEXT("Stored second")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("native MemberRef execute selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdMemberRef", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				FProdFields Object;
				Object.Stored = 41;
				return Object.Stored + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdMemberRef",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(TEXT("native MemberRef must Generate; ast {%s}"), *AstDump)));
		ASSERT_THAT(IsTrue(
			AstDump.Contains(TEXT("kind=MemberRef")) && AstDump.Contains(TEXT("Stored")),
			*FString::Printf(TEXT("sealed MemberRef must name Stored; ast {%s}"), *AstDump)));
		asIScriptFunction* const Published = Module->GetFunctionByDecl("int F()");
		asCString FuncDump;
		if (Published != nullptr)
		{
			asCBytecodeCodeGenDumpFunction(static_cast<asCScriptFunction*>(Published), FuncDump);
		}
		FString Exception;
		const int ExecStatus = AngelscriptNativeTestSupport::CanonicalExecuteStatus(
			Engine.Get(), Module, "int F()", Exception);
		int32 Value = 0;
		const bool Executed = AngelscriptNativeTestSupport::CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int F()", Value);
		ASSERT_THAT(IsTrue(
			ExecStatus == asEXECUTION_FINISHED && Executed && Value == 42,
			*FString::Printf(
				TEXT("Stored second field must execute 42, not Poison; got=%d execStatus=%d exception={%s} bc {%s} ast {%s}"),
				Value,
				ExecStatus,
				*Exception,
				UTF8_TO_TCHAR(FuncDump.AddressOf() ? FuncDump.AddressOf() : ""),
				*AstDump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher()));
	}

	TEST_METHOD(CanonicalNativeMemberRefPublishesExactPropertyLayoutDependency)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"FProdDependencyFields",
			sizeof(FProdFields),
			asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS
				| asOBJ_APP_CLASS_CONSTRUCTOR) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
			"FProdDependencyFields",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ProdFieldsConstruct),
			asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectProperty(
			"FProdDependencyFields",
			"int Poison",
			asOFFSET(FProdFields, Poison)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectProperty(
			"FProdDependencyFields",
			"int Stored",
			asOFFSET(FProdFields, Stored)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int ReadStored()
			{
				FProdDependencyFields Object;
				return Object.Stored;
			}
			)AS");
		ASSERT_THAT(AreEqual(asSUCCESS,
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(),
				"ProdMemberPropertyDependency",
				ScriptSource.c_str(),
				Module)));
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		asCScriptFunction* const Published = static_cast<asCScriptFunction*>(
			Module->GetFunctionByDecl("int ReadStored()"));
		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			ScriptEngine->GetTypeInfoByDecl("FProdDependencyFields"));
		ASSERT_THAT(IsNotNull(Published));
		ASSERT_THAT(IsNotNull(ObjectType));
		if (Published == nullptr || Published->scriptData == nullptr
			|| ObjectType == nullptr)
		{
			return;
		}

		asCObjectProperty* StoredProperty = nullptr;
		asCObjectProperty* PoisonProperty = nullptr;
		for (asUINT PropertyIndex = 0;
			PropertyIndex < ObjectType->properties.GetLength(); ++PropertyIndex)
		{
			asCObjectProperty* const Property =
				ObjectType->properties[PropertyIndex];
			if (Property != nullptr && Property->name == "Stored")
			{
				StoredProperty = Property;
			}
			else if (Property != nullptr && Property->name == "Poison")
			{
				PoisonProperty = Property;
			}
		}
		ASSERT_THAT(IsNotNull(StoredProperty));
		ASSERT_THAT(IsNotNull(PoisonProperty));

		int32 StoredDependencyCount = 0;
		int32 PoisonDependencyCount = 0;
		for (asUINT DependencyIndex = 0;
			DependencyIndex
				< Published->scriptData->artifactDependencies.GetLength();
			++DependencyIndex)
		{
			const asSBuildArtifactDependency& Dependency =
				Published->scriptData->artifactDependencies[DependencyIndex];
			if (Dependency.kind
					!= asBUILD_ARTIFACT_DEPENDENCY_PROPERTY_LAYOUT
				|| Dependency.referenceKind
					!= asBUILD_ARTIFACT_REFERENCE_PROPERTY
				|| Dependency.propertyOwnerType != ObjectType)
			{
				continue;
			}
			StoredDependencyCount +=
				Dependency.objectProperty == StoredProperty ? 1 : 0;
			PoisonDependencyCount +=
				Dependency.objectProperty == PoisonProperty ? 1 : 0;
		}
		ASSERT_THAT(AreEqual(1, StoredDependencyCount,
			TEXT("the member relocation must declare its exact native property-layout authority once")));
		ASSERT_THAT(AreEqual(0, PoisonDependencyCount,
			TEXT("the first same-shaped property must not enter the dependency envelope")));
	}

	TEST_METHOD(CanonicalPODTripleAssignmentCopiesAllTwelveBytes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdTriple",
				sizeof(FProdTriple),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_CONSTRUCTOR) >= 0,
			TEXT("FProdTriple twelve-byte VALUE POD")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdTriple",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdTripleConstruct),
				asCALL_GENERIC) >= 0,
			TEXT("FProdTriple default construct")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty("FProdTriple", "int First", asOFFSET(FProdTriple, First)) >= 0
			&& ScriptEngine->RegisterObjectProperty("FProdTriple", "int Second", asOFFSET(FProdTriple, Second)) >= 0
			&& ScriptEngine->RegisterObjectProperty("FProdTriple", "int Third", asOFFSET(FProdTriple, Third)) >= 0,
			TEXT("FProdTriple fields")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("twelve-byte POD assignment selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdPODTripleCopy", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				FProdTriple Left;
				Left.First = 1;
				Left.Second = 2;
				Left.Third = 3;
				FProdTriple Right;
				Right = Left;
				return Right.First * 100 + Right.Second * 10 + Right.Third;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdPODTripleCopy",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("twelve-byte POD assignment must compile through CodeGen")));
		asIScriptFunction* const Published = Module->GetFunctionByDecl("int F()");
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Published, asBC_COPY),
			TEXT("twelve-byte POD assignment must use the typed asBC_COPY protocol, not an eight-byte register move")));

		int32 Value = 0;
		const bool Executed = AngelscriptNativeTestSupport::CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int F()", Value);
		ASSERT_THAT(IsTrue(
			Executed && Value == 123,
			*FString::Printf(
				TEXT("twelve-byte POD assignment must preserve First/Second/Third (123), not truncate after eight bytes; got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("twelve-byte POD assignment must be published by CodeGen")));
	}

	TEST_METHOD(CanonicalWidePODGeneratedGetterCopiesBackingFieldIntoHiddenReturnStorage)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdTriple",
				sizeof(FProdTriple),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS
					| asOBJ_APP_CLASS_CONSTRUCTOR) >= 0,
			TEXT("the generated getter gate requires a wide POD VALUE type")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdTriple",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdTripleConstruct),
				asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FProdTriple", "int First",
				asOFFSET(FProdTriple, First)) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"FProdTriple", "int Second",
				asOFFSET(FProdTriple, Second)) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"FProdTriple", "int Third",
				asOFFSET(FProdTriple, Third)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdWidePODGeneratedGetter", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(
			asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FWidePODGetterOwner
			{
				FProdTriple Inner;

				// Isolate the generated getter; no setter body is needed here.
				void SetInner() {}
			}

			int Entry()
			{
				FWidePODGetterOwner Owner;
				FProdTriple Output = Owner.Inner;
				return Output.First * 100 + Output.Second * 10 + Output.Third;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdWidePODGeneratedGetter.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("a wide POD generated getter must compile without scalarizing its backing field")));

		asITypeInfo* const OwnerType =
			Module->GetTypeInfoByName("FWidePODGetterOwner");
		asIScriptFunction* const Getter = OwnerType != nullptr
			? OwnerType->GetMethodByName("GetInner") : nullptr;
		ASSERT_THAT(IsNotNull(Getter));
		ASSERT_THAT(IsTrue(
			static_cast<const asCScriptFunction*>(Getter)->DoesReturnOnStack(),
			TEXT("the wide POD getter ABI must expose caller-owned hidden return storage")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(
				Getter, asBC_COPY),
			TEXT("the generated getter must copy the exact sealed POD width into hidden return storage")));

		int32 Result = -1;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Result)
				&& Result == 0,
			*FString::Printf(
				TEXT("the detached wide POD getter result must execute intact; got=%d"),
				Result)));
	}

	TEST_METHOD(CanonicalWidePODGeneratedSetterCopiesOwnedParameterIntoBackingField)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdTriple",
				sizeof(FProdTriple),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS
					| asOBJ_APP_CLASS_CONSTRUCTOR) >= 0));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdTriple",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdTripleConstruct),
				asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FProdTriple", "int First",
				asOFFSET(FProdTriple, First)) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"FProdTriple", "int Second",
				asOFFSET(FProdTriple, Second)) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"FProdTriple", "int Third",
				asOFFSET(FProdTriple, Third)) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdWidePODGeneratedSetter", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(
			asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FWidePODSetterOwner
			{
				FProdTriple Inner;
			}

			int Entry()
			{
				FWidePODSetterOwner Owner;
				FProdTriple Input;
				Input.First = 1;
				Input.Second = 2;
				Input.Third = 3;
				Owner.Inner = Input;
				FProdTriple Output = Owner.Inner;
				return Output.First * 100 + Output.Second * 10 + Output.Third;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdWidePODGeneratedSetter.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("a wide POD generated setter must copy its complete owned parameter")));

		asITypeInfo* const OwnerType =
			Module->GetTypeInfoByName("FWidePODSetterOwner");
		asIScriptFunction* const Getter = OwnerType != nullptr
			? OwnerType->GetMethodByName("GetInner") : nullptr;
		asIScriptFunction* const Setter = OwnerType != nullptr
			? OwnerType->GetMethodByName("SetInner") : nullptr;
		ASSERT_THAT(IsNotNull(Getter));
		ASSERT_THAT(IsNotNull(Setter));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(
				Getter, asBC_COPY)
			&& AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(
				Setter, asBC_COPY),
			TEXT("both generated accessors must use the exact sealed POD copy protocol")));

		int32 Result = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Result)
				&& Result == 123,
			*FString::Printf(
				TEXT("the generated setter/getter round trip must preserve all twelve bytes; got=%d"),
				Result)));
	}

	TEST_METHOD(CanonicalPODTripleReturnUsesHiddenReturnStorage)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdTriple",
				sizeof(FProdTriple),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_CONSTRUCTOR) >= 0,
			TEXT("FProdTriple twelve-byte VALUE POD")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdTriple",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdTripleConstruct),
				asCALL_GENERIC) >= 0,
			TEXT("FProdTriple default construct")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty("FProdTriple", "int First", asOFFSET(FProdTriple, First)) >= 0
			&& ScriptEngine->RegisterObjectProperty("FProdTriple", "int Second", asOFFSET(FProdTriple, Second)) >= 0
			&& ScriptEngine->RegisterObjectProperty("FProdTriple", "int Third", asOFFSET(FProdTriple, Third)) >= 0,
			TEXT("FProdTriple fields")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("twelve-byte POD return selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdPODTripleReturn", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			FProdTriple MakeTriple()
			{
				FProdTriple Result;
				Result.First = 1;
				Result.Second = 2;
				Result.Third = 3;
				return Result;
			}

			int Entry()
			{
				FProdTriple Output;
				Output = MakeTriple();
				return Output.First * 100 + Output.Second * 10 + Output.Third;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdPODTripleReturn",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("twelve-byte POD return/call must compile through CodeGen")));

		asIScriptFunction* const MakeTriple = Module->GetFunctionByDecl("FProdTriple MakeTriple()");
		asIScriptFunction* const Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(MakeTriple, asBC_COPY)
			&& AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_PSF),
			TEXT("callee must COPY into hidden return storage and caller must reserve it before CALL")));

		int32 Value = 0;
		const bool Executed = AngelscriptNativeTestSupport::CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int Entry()", Value);
		ASSERT_THAT(IsTrue(
			Executed && Value == 123,
			*FString::Printf(
				TEXT("twelve-byte POD return/call must preserve all fields through hidden storage; got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("twelve-byte POD return/call must be published by CodeGen")));
	}

	TEST_METHOD(CanonicalPODTripleByValueParameterPreservesAllTwelveBytes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdTriple",
				sizeof(FProdTriple),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_CONSTRUCTOR) >= 0,
			TEXT("FProdTriple twelve-byte VALUE POD")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdTriple",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdTripleConstruct),
				asCALL_GENERIC) >= 0,
			TEXT("FProdTriple default construct")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty("FProdTriple", "int First", asOFFSET(FProdTriple, First)) >= 0
			&& ScriptEngine->RegisterObjectProperty("FProdTriple", "int Second", asOFFSET(FProdTriple, Second)) >= 0
			&& ScriptEngine->RegisterObjectProperty("FProdTriple", "int Third", asOFFSET(FProdTriple, Third)) >= 0,
			TEXT("FProdTriple fields")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("twelve-byte POD by-value parameter selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdPODTripleByValue", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int SumTriple(FProdTriple Input)
			{
				return Input.First * 100 + Input.Second * 10 + Input.Third;
			}

			int Entry()
			{
				FProdTriple Input;
				Input.First = 1;
				Input.Second = 2;
				Input.Third = 3;
				int FirstResult = SumTriple(Input);
				Input.Third = 4;
				int SecondResult = SumTriple(Input);
				return FirstResult * 1000 + SecondResult;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdPODTripleByValue",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("twelve-byte POD by-value call must compile through CodeGen")));
		asIScriptFunction* const SumTriple = Module->GetFunctionByName("SumTriple");
		asIScriptFunction* const Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_COPY)
			&& AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_PSF),
			*FString::Printf(
				TEXT("wide POD call must copy into caller-owned storage and pass its generic object pointer; bc {%s}"),
				*ProdBytecodeImmediates(Entry))));

		int32 Value = 0;
		const bool Executed = AngelscriptNativeTestSupport::CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int Entry()", Value);
		ASSERT_THAT(IsTrue(
			Executed && Value == 123124,
			*FString::Printf(
				TEXT("two twelve-byte POD by-value calls must preserve 123 then 124 without stack residue; got=%d caller {%s} callee {%s}"),
				Value,
				*ProdBytecodeImmediates(Entry),
				*ProdBytecodeImmediates(SumTriple))));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("twelve-byte POD by-value call must be published by CodeGen")));
	}

	TEST_METHOD(CanonicalPODTripleByValueParameterAcceptsExternalContextObject)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdTriple",
				sizeof(FProdTriple),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_CONSTRUCTOR) >= 0,
			TEXT("FProdTriple twelve-byte VALUE POD")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdTriple",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdTripleConstruct),
				asCALL_GENERIC) >= 0,
			TEXT("FProdTriple default construct")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty("FProdTriple", "int First", asOFFSET(FProdTriple, First)) >= 0
			&& ScriptEngine->RegisterObjectProperty("FProdTriple", "int Second", asOFFSET(FProdTriple, Second)) >= 0
			&& ScriptEngine->RegisterObjectProperty("FProdTriple", "int Third", asOFFSET(FProdTriple, Third)) >= 0,
			TEXT("FProdTriple fields")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("external twelve-byte POD parameter selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int SumTriple(FProdTriple Input)
			{
				return Input.First * 100 + Input.Second * 10 + Input.Third;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdPODTripleExternalByValue",
			ScriptSource.c_str(),
			Module),
			TEXT("external twelve-byte POD parameter Build must succeed through CodeGen")));

		asIScriptFunction* const SumTriple = Module->GetFunctionByName("SumTriple");
		ASSERT_THAT(IsNotNull(SumTriple, TEXT("SumTriple must publish for external context preparation")));
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("external context allocation")));
		ON_SCOPE_EXIT
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
		};

		FProdTriple Input;
		Input.First = 1;
		Input.Second = 2;
		Input.Third = 3;
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(SumTriple)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgObject(0, &Input)));
		const int ExecuteStatus = Context->Execute();
		ASSERT_THAT(IsTrue(
			ExecuteStatus == asEXECUTION_FINISHED && Context->GetReturnDWord() == 123,
			*FString::Printf(
				TEXT("external SetArgObject must preserve FProdTriple 1/2/3 through CANONICAL parameter ABI; status=%d value=%d"),
				ExecuteStatus,
				static_cast<int32>(Context->GetReturnDWord()))));
	}

	TEST_METHOD(CanonicalSmallPODGenericCallOwnsArgumentAndUsesHiddenReturnStorage)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdSmallPOD",
				sizeof(FProdSmallPOD),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_CONSTRUCTOR) >= 0,
			TEXT("FProdSmallPOD four-byte VALUE POD")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdSmallPOD",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdSmallPODConstruct),
				asCALL_GENERIC) >= 0,
			TEXT("FProdSmallPOD default construct")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"FProdSmallPOD",
				"int Value",
				asOFFSET(FProdSmallPOD, Value)) >= 0,
			TEXT("FProdSmallPOD observable field")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"FProdSmallPOD EchoSmallPOD(FProdSmallPOD Input)",
				asFUNCTION(ProdSmallPODEcho),
				asCALL_GENERIC) >= 0,
			TEXT("generic native function with four-byte POD input and return")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("small POD generic ABI fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				FProdSmallPOD Input;
				Input.Value = 42;
				FProdSmallPOD Output = EchoSmallPOD(Input);
				return Output.Value;
			}
			)AS");
		const int BuildResult = AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdSmallPODGenericABI",
			ScriptSource.c_str(),
			Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("small POD generic input/return must compile through Canonical CodeGen")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}

		asIScriptFunction* const Entry = Module->GetFunctionByDecl("int Entry()");
		const bool bHasOwnedArgumentABI =
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_ALLOC)
			&& AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_COPY)
			&& AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_CALLSYS);
		ASSERT_THAT(IsTrue(
			bHasOwnedArgumentABI,
			*FString::Printf(
				TEXT("small POD system call must allocate, copy, and transfer an owned object pointer; bc {%s}"),
				*ProdBytecodeImmediates(Entry))));
		if (!bHasOwnedArgumentABI)
		{
			// Keep the red test memory-safe: the old bytecode passes the integer
			// payload as an address and would make SetReturnObject write to 0x2a.
			return;
		}

		int32 Value = 0;
		const bool bExecuted = AngelscriptNativeTestSupport::CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int Entry()", Value);
		ASSERT_THAT(IsTrue(
			bExecuted && Value == 42,
			*FString::Printf(
				TEXT("generic small POD input/return must preserve 42 through owned argument and hidden-return ABI; got=%d bc {%s}"),
				Value,
				*ProdBytecodeImmediates(Entry))));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("small POD generic ABI must be published by Canonical CodeGen")));
	}

	TEST_METHOD(CanonicalPODTripleByValueParameterTransfersOwnedSystemArgument)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FProdTriple",
				sizeof(FProdTriple),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_CONSTRUCTOR) >= 0,
			TEXT("FProdTriple twelve-byte VALUE POD")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FProdTriple",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdTripleConstruct),
				asCALL_GENERIC) >= 0,
			TEXT("FProdTriple default construct")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty("FProdTriple", "int First", asOFFSET(FProdTriple, First)) >= 0
			&& ScriptEngine->RegisterObjectProperty("FProdTriple", "int Second", asOFFSET(FProdTriple, Second)) >= 0
			&& ScriptEngine->RegisterObjectProperty("FProdTriple", "int Third", asOFFSET(FProdTriple, Third)) >= 0,
			TEXT("FProdTriple fields")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"int Sum(FProdTriple)",
				asFUNCTION(ProdTripleSum),
				asCALL_GENERIC) >= 0,
			TEXT("generic native function with a twelve-byte by-value POD parameter")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("generic native twelve-byte POD ownership fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				FProdTriple Input;
				Input.First = 1;
				Input.Second = 2;
				Input.Third = 3;
				return Sum(Input);
			}
			)AS");
		const int BuildResult = AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdPODTripleGenericNativeByValue",
			ScriptSource.c_str(),
			Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("CANONICAL wide POD system call must compile with an owned heap argument")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}

		asIScriptFunction* const Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_ALLOC)
				&& AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_COPY)
				&& AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_CALLSYS),
			*FString::Printf(
				TEXT("wide POD system call must allocate, copy, and transfer an owned object pointer; bc {%s}"),
				*ProdBytecodeImmediates(Entry))));
		const asCScriptFunction* const InternalEntry =
			static_cast<const asCScriptFunction*>(Entry);
		bool bPublishedOwnedArgumentPointerSlot = false;
		if (InternalEntry != nullptr && InternalEntry->scriptData != nullptr)
		{
			const asCArray<asDWORD>& Bytecode = InternalEntry->scriptData->byteCode;
			for (asUINT ProgramCounter = 0; ProgramCounter < Bytecode.GetLength();)
			{
				const asEBCInstr Opcode = static_cast<asEBCInstr>(
					*reinterpret_cast<const asBYTE*>(&Bytecode[ProgramCounter]));
				if (Opcode == asBC_ClrVPtr
					&& InternalEntry->scriptData->pointerVariablePos.Exists(
						asBC_SWORDARG0(&Bytecode[ProgramCounter])))
				{
					bPublishedOwnedArgumentPointerSlot = true;
					break;
				}
				ProgramCounter += asBCTypeSize[asBCInfo[Opcode].type];
			}
		}
		ASSERT_THAT(IsTrue(
			bPublishedOwnedArgumentPointerSlot,
			TEXT("Canonical CodeGen must publish the ownership-neutral ClrVPtr slot for JIT operand typing")));

		int32 Value = 0;
		const bool bExecuted = AngelscriptNativeTestSupport::CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int Entry()", Value);
		ASSERT_THAT(IsTrue(
			bExecuted && Value == 123,
			*FString::Printf(
				TEXT("generic wide POD argument must preserve all twelve bytes and return 123; got=%d bc {%s}"),
				Value,
				*ProdBytecodeImmediates(Entry))));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("wide POD generic ownership call must be published by Canonical CodeGen")));
	}

	TEST_METHOD(CanonicalSwitchBreakExitsSwitchNotOuterWhile)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("switch-break-in-while execute fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				int n = 0;
				int i = 0;
				while (i < 3)
				{
					switch (i)
					{
					case 0:
						n = n + 1;
						break;
					default:
						n = n + 10;
						break;
					}
					i = i + 1;
				}
				return n;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdSwitchBreakWhile",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL switch-in-while Build must succeed from CodeGen")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 21,
			*FString::Printf(
				TEXT("switch break must stay in the while (1+10+10=21), not exit after case 0 (1); got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL switch-in-while Build must not record asCCompiler")));
	}

	TEST_METHOD(CanonicalSwitchBreakContinuesAfterSwitchInWhile)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("switch-break fallthrough-after-switch fixture selects CANONICAL")));
		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				int n = 0;
				int i = 0;
				while (i < 1)
				{
					i = 1;
					switch (0)
					{
					case 0:
						break;
					}
					n = 7;
				}
				return n;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdSwitchBreakAfter",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL switch-break after-switch Build must succeed from CodeGen")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 7,
			*FString::Printf(
				TEXT("switch break must fall through to n=7, not while-break (0); got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL switch-break after-switch Build must not record asCCompiler")));
	}

	TEST_METHOD(CanonicalPrimitivePostfixIncrementPreservesOldValueAndWritesOnce)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("primitive postfix fixture selects CANONICAL CodeGen")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				int Value = 1;
				int Previous = Value++;
				return Value * 10 + Previous;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdPrimitivePostfixIncrement",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL Build must lower primitive postfix increment")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("primitive postfix increment must not fall back to asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 21,
			*FString::Printf(
				TEXT("postfix increment must return the old value while writing the incremented value once; got=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalMixinMethodCallBuildsAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("mixin execution fixture selects CANONICAL CodeGen")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
			}

			mixin int AddOffset(T self, int Value)
			{
				return Value + 2;
			}

			int Entry()
			{
				T Object;
				return Object.AddOffset(40);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdCanonicalMixinMethodCall",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL Build must lower a script mixin declaration and receiver call")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("mixin method call must publish Canonical CodeGen")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 42,
			*FString::Printf(TEXT("mixin receiver must occupy formal zero and return 42; got=%d"), Value)));
	}

	TEST_METHOD(CanonicalSwitchFallthroughExecutesNextCase)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("switch fallthrough execution fixture selects CANONICAL CodeGen")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				int Value = 0;
				switch (Value)
				{
					case 0:
						Value = 1;
						fallthrough;
					case 1:
						return Value + 41;
				}
				return 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdCanonicalSwitchFallthrough",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL Build must lower fallthrough to the next switch case")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("fallthrough fixture must be published by Canonical CodeGen")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 42,
			*FString::Printf(TEXT("fallthrough must enter the next case and return 42; got=%d"), Value)));
	}

	TEST_METHOD(CanonicalConstMethodOverloadSelectsReceiverQualifier)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("const-method overload execution fixture selects CANONICAL CodeGen")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int F()
				{
					return 1;
				}

				int F() const
				{
					return 2;
				}
			}

			int Entry()
			{
				T Mutable;
				const T ConstObject;
				return Mutable.F() * 10 + ConstObject.F();
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdCanonicalConstMethodOverload",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL Build must select distinct const and mutable method overloads")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 12,
			*FString::Printf(TEXT("mutable F() must return 1 and const F() must return 2; got=%d"), Value)));
	}

	TEST_METHOD(CanonicalNativeEnumArgumentExecutesWithEnumType)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("ETeam") >= 0,
			TEXT("native enum fixture registers ETeam")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnumValue("ETeam", "Red", 41) >= 0,
			TEXT("native enum fixture registers ETeam::Red")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("native enum argument fixture selects CANONICAL CodeGen")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F(ETeam Team)
			{
				return 42;
			}

			int Entry()
			{
				return F(ETeam::Red);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdCanonicalNativeEnumArgument",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL Build must preserve ETeam::Red as an ETeam argument")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 42,
			*FString::Printf(TEXT("native enum call must dispatch F(ETeam) and return 42; got=%d"), Value)));
	}

	TEST_METHOD(CanonicalPrimitiveIncrementDecrementPreservesFormAndUnsignedMutation)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("primitive increment/decrement fixture selects CANONICAL CodeGen")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				int PrefixTarget = 1;
				int PrefixResult = ++PrefixTarget;
				int PostfixTarget = 5;
				int PostfixResult = PostfixTarget--;
				uint UnsignedTarget = 7;
				uint UnsignedResult = UnsignedTarget++;
				if (PrefixResult != 2 || PrefixTarget != 2
					|| PostfixResult != 5 || PostfixTarget != 4
					|| UnsignedResult != 7 || UnsignedTarget != 8)
				{
					return 0;
				}
				return 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ProdPrimitiveIncrementDecrement",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL Build must lower primitive prefix/postfix ++/--")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("primitive increment/decrement must not fall back to asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 1,
			*FString::Printf(
				TEXT("primitive prefix/postfix ++/-- must preserve result form and mutation, including uint; got=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalPrimitiveCompoundAssignmentsPreserveOperatorsAndExecute)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("primitive compound-assignment fixture selects CANONICAL CodeGen")));

		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProdPrimitiveCompoundAssignments", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("compound-assignment fixture needs a module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				int Arithmetic = 10;
				Arithmetic += 5;
				Arithmetic -= 3;
				Arithmetic *= 2;
				Arithmetic /= 4;
				Arithmetic %= 5;

				uint Bits = 0x0F;
				Bits |= 0xF0;
				Bits &= 0xFC;
				Bits ^= 0x0C;
				Bits <<= 1;
				Bits >>= 2;

				int Signed = -16;
				Signed >>>= 2;

				uint8 Wrap = 255;
				Wrap += 1;

				float Real = 8.0;
				Real /= 2.0;
				Real += 1.5;

				return Arithmetic == 1
					&& Bits == 0x78
					&& Signed == -4
					&& Wrap == 0
					&& Real == 5.5
					? 1 : 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdPrimitiveCompoundAssignments.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("CANONICAL Build must lower every primitive compound-assignment family")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("primitive compound assignments must not fall back to asCCompiler")));

		const asCASTContext* const Context = static_cast<asCModule*>(Module)->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("compound-assignment fixture must retain its sealed AST")));
		if (Context == nullptr)
		{
			return;
		}

		const char* const ExpectedOperators[] = {
			"+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", ">>>="
		};
		for (const char* const ExpectedOperator : ExpectedOperators)
		{
			bool bFound = false;
			for (asUINT Index = 1; Index <= Context->GetExprCount(); ++Index)
			{
				const asCExpr* const Expr = Context->GetExpr(asASTExprId(Index));
				if (Expr != nullptr
					&& Expr->kind == asAST_EXPR_ASSIGN
					&& Expr->literal.Equals(ExpectedOperator))
				{
					bFound = true;
					break;
				}
			}
			ASSERT_THAT(IsTrue(bFound,
				*FString::Printf(TEXT("sealed AST must preserve compound operator %s"),
					ANSI_TO_TCHAR(ExpectedOperator))));
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 1,
			*FString::Printf(
				TEXT("primitive compound assignments must apply their operators before one write-back; got=%d"),
				Value)));
	}

	TEST_METHOD(CanonicalNativeSameArityGlobalExecutesSelectedNotFirstRegistered)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		const int BoolPickId = ScriptEngine->RegisterGlobalFunction(
			"int HostPick(bool value)",
			asFUNCTION(ProdHostPickBoolPoison),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(BoolPickId >= 0, TEXT("poison HostPick(bool) must register first")));
		const int IntPickId = ScriptEngine->RegisterGlobalFunction(
			"int HostPick(int value)",
			asFUNCTION(ProdHostPickInt),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(IntPickId >= 0, TEXT("selected HostPick(int) must register second")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("same-arity host global selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule("ProdHostPickExactBind", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("same-arity HostPick needs a module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				return HostPick(41);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdHostPickExactBind",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(TEXT("exact HostPick Bind must Generate; ast {%s}"), *AstDump)));
		ASSERT_THAT(IsTrue(
			AstDump.Contains(TEXT("callee=HostPick(int)")),
			*AstDump));
		ASSERT_THAT(IsFalse(
			AstDump.Contains(TEXT("callee=HostPick(bool)")),
			*AstDump));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("exact HostPick must publish CodeGen")));

		asIScriptFunction* const IntPick = ScriptEngine->GetFunctionById(IntPickId);
		asIScriptFunction* const BoolPick = ScriptEngine->GetFunctionById(BoolPickId);
		ASSERT_THAT(IsNotNull(IntPick, TEXT("registered HostPick(int) must remain")));
		ASSERT_THAT(IsNotNull(BoolPick, TEXT("registered HostPick(bool) must remain")));

		asIScriptFunction* const Published = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(Published, TEXT("exact HostPick module must publish int F()")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
			&& Value == 42,
			*FString::Printf(
				TEXT("Sema-selected HostPick(int) must execute 42, not first same-arity HostPick(bool)=1; got=%d"),
				Value)));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunction(Published, IntPick->GetId()),
			TEXT("bytecode must CALLSYS HostPick(int), not the first same-arity bool")));
		ASSERT_THAT(IsFalse(
			ProdBytecodeCallsFunction(Published, BoolPick->GetId()),
			TEXT("bytecode must not CALLSYS poison HostPick(bool)")));
	}

	TEST_METHOD(CanonicalCallDispatchUsesSealedVirtualAndQualifiedDirectRoutes)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class DispatchBase
			{
				int Resolve()
				{
					return 50;
				}

				int ThroughVirtual()
				{
					return Resolve();
				}
			}

			class DispatchDerived : DispatchBase
			{
				int Resolve() override
				{
					return 60;
				}

				int ThroughDirect()
				{
					return DispatchBase::Resolve();
				}
			}

			int RunVirtual()
			{
				DispatchBase Object = DispatchDerived();
				return Object.ThroughVirtual();
			}

			int RunDirect()
			{
				DispatchDerived Object = DispatchDerived();
				return Object.ThroughDirect();
			}
			)AS");
		ASSERT_THAT(AreEqual(0, CompileNativeModule(
			Engine.Get(),
			"ProdCallDispatch",
			ScriptSource.c_str(),
			Module),
			TEXT("sealed call-dispatch fixture must build through Canonical CodeGen")));
		if (Module == nullptr)
		{
			return;
		}

		asITypeInfo* const BaseType = Module->GetTypeInfoByName("DispatchBase");
		asITypeInfo* const DerivedType = Module->GetTypeInfoByName("DispatchDerived");
		asIScriptFunction* const BaseResolve = BaseType != nullptr
			? BaseType->GetMethodByDecl("int Resolve()") : nullptr;
		asIScriptFunction* const ThroughVirtual = BaseType != nullptr
			? BaseType->GetMethodByDecl("int ThroughVirtual()") : nullptr;
		asIScriptFunction* const ThroughDirect = DerivedType != nullptr
			? DerivedType->GetMethodByDecl("int ThroughDirect()") : nullptr;
		ASSERT_THAT(IsNotNull(BaseResolve));
		ASSERT_THAT(IsNotNull(ThroughVirtual));
		ASSERT_THAT(IsNotNull(ThroughDirect));
		if (BaseResolve == nullptr || ThroughVirtual == nullptr || ThroughDirect == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunctionWithOpcode(
				ThroughVirtual, BaseResolve->GetId(), asBC_CALLINTF),
			*FString::Printf(
				TEXT("ordinary member call must lower through CALLINTF after Sema seals virtual dispatch; calls=%s"),
				*ProdBytecodeCallTargets(ThroughVirtual))));
		ASSERT_THAT(IsTrue(
			ProdBytecodeCallsFunctionWithOpcode(
				ThroughDirect, BaseResolve->GetId(), asBC_CALL),
			*FString::Printf(
				TEXT("qualified DispatchBase::Resolve must lower through direct CALL; calls=%s"),
				*ProdBytecodeCallTargets(ThroughDirect))));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int RunVirtual()", Value)
				&& Value == 60,
			*FString::Printf(
				TEXT("base-view virtual path must execute the derived override; got=%d"),
				Value)));
		Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int RunDirect()", Value)
				&& Value == 50,
			*FString::Printf(
				TEXT("qualified base call must bypass the derived override; got=%d"),
				Value)));
	}
};

#endif
