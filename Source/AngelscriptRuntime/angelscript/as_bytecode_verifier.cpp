#include "as_bytecode_verifier.h"
#include "as_bytecode_opcode.h"
#include "Containers/Set.h"

BEGIN_AS_NAMESPACE

namespace
{
	constexpr int64 kInt16Min = -32768;
	constexpr int64 kInt16Max = 32767;

	bool FitsSignedWidth(int64 Value, int8 Width)
	{
		switch (Width)
		{
		case 1: return Value >= -128 && Value <= 127;
		case 2: return Value >= kInt16Min && Value <= kInt16Max;
		case 4: return Value >= (-2147483647LL - 1) && Value <= 2147483647LL;
		default: return true;
		}
	}

	int32 OccupiedDwordsFromWidth(uint8 StorageWidth)
	{
		if (StorageWidth == 0) return 1;
		return static_cast<int32>((StorageWidth + 3) / 4);
	}

	bool FrameAccessValid(const asSByteCodeFunctionBody& Function, int64 Offset, int32 Occupied)
	{
		if (Occupied <= 0 || Offset < kInt16Min || Offset > kInt16Max) return false;
		// The interpreter starts at Frame - Offset and advances in memory.
		// Locals occupy offsets [1, FrameSize]; arguments start at offset zero.
		const int64 Last = Offset - (static_cast<int64>(Occupied) - 1);
		if (Last < kInt16Min) return false;
		if (Offset > 0) return Last > 0 && static_cast<uint64>(Offset) <= Function.FrameSize;
		return static_cast<uint64>(-Last) < Function.ParameterSize;
	}

	asEByteCodeRequirementRole SlotRole(asEByteCodeOperandKind Kind)
	{
		switch (Kind)
		{
		case asEByteCodeOperandKind::FunctionSlot: return asEByteCodeRequirementRole::Function;
		case asEByteCodeOperandKind::PropertySlot: return asEByteCodeRequirementRole::Property;
		case asEByteCodeOperandKind::GlobalSlot: return asEByteCodeRequirementRole::Global;
		default: return asEByteCodeRequirementRole::Type;
		}
	}

	bool SlotRoleMatches(asEByteCodeOperandKind Kind, asEByteCodeRequirementRole Role)
	{
		if (Kind == asEByteCodeOperandKind::TypeSlot)
		{
			return Role == asEByteCodeRequirementRole::Type || Role == asEByteCodeRequirementRole::TypeUse;
		}
		return Role == SlotRole(Kind);
	}

	asSByteCodeVerifyFault MakeFault(
		asEByteCodeVerifyStatus Status,
		const asSStableKey& FunctionKey,
		int32 InstructionIndex,
		int32 OperandIndex)
	{
		asSByteCodeVerifyFault Fault;
		Fault.Status = Status;
		Fault.FunctionKey = FunctionKey;
		Fault.InstructionIndex = InstructionIndex;
		Fault.OperandIndex = OperandIndex;
		return Fault;
	}

	constexpr int32 kMaxStackDwords = 65536;
	constexpr int32 kUnknownLocal = MIN_int32;

	struct asSAbstractDword
	{
		asEByteCodeStorageKind Kind = asEByteCodeStorageKind::Value;
		int32 Local = kUnknownLocal;
		int32 ObjectLocal = kUnknownLocal;
	};

	bool StorageContractValid(asUINT Dwords, asEByteCodeStorageKind Kind, bool bAllowVoid)
	{
		switch (Kind)
		{
		case asEByteCodeStorageKind::Void: return bAllowVoid && Dwords == 0;
		case asEByteCodeStorageKind::Value: return Dwords > 0 && Dwords <= 32768;
		case asEByteCodeStorageKind::Address: return Dwords == AS_PTR_SIZE;
		default: return false;
		}
	}

	const asSByteCodeRequirement* CallRequirement(const asCByteCodeImage& Image, const asSByteCodeInstruction& Instruction)
	{
		for (const asSByteCodeOperand& Operand : Instruction.Operands)
		{
			if (Operand.Kind != asEByteCodeOperandKind::FunctionSlot) continue;
			if (Operand.Slot >= static_cast<asUINT>(Image.Requirements.Num())) return nullptr;
			const asSByteCodeRequirement& Requirement = Image.Requirements[Operand.Slot];
			return Requirement.Role == asEByteCodeRequirementRole::Function ? &Requirement : nullptr;
		}
		return nullptr;
	}

	asEByteCodeVerifyStatus ApplyCallContract(const asSByteCodeRequirement& Requirement, TArray<asSAbstractDword>& Stack)
	{
		if (!StorageContractValid(Requirement.ReturnDwords, Requirement.ReturnStorageKind, true))
			return asEByteCodeVerifyStatus::InvalidOperand;
		int64 Consumed = !Requirement.OwnerKey.IsZero() && (Requirement.CallableTraits & 1u) == 0 ? AS_PTR_SIZE : 0;
		if ((Requirement.CallableTraits & 64u) != 0) Consumed += AS_PTR_SIZE;
		for (const asSByteCodeParameterContract& Parameter : Requirement.Parameters)
		{
			if (!StorageContractValid(Parameter.StorageDwords, Parameter.StorageKind, false)
				|| Parameter.PassingMode > 3
				|| (Parameter.PassingMode != 0 && Parameter.StorageKind != asEByteCodeStorageKind::Address))
				return asEByteCodeVerifyStatus::InvalidOperand;
			Consumed += Parameter.StorageDwords;
			if (Consumed > kMaxStackDwords) return asEByteCodeVerifyStatus::InvalidOperand;
		}
		if (Consumed > Stack.Num()) return asEByteCodeVerifyStatus::StackUnderflow;
		int32 Cursor = Stack.Num();
		auto Consume = [&](asUINT Width, asEByteCodeStorageKind Kind)
		{
			for (asUINT Index = 0; Index < Width; ++Index)
				if (Stack[--Cursor].Kind != Kind) return false;
			return true;
		};
		if (!Requirement.OwnerKey.IsZero() && (Requirement.CallableTraits & 1u) == 0
			&& !Consume(AS_PTR_SIZE, asEByteCodeStorageKind::Address)) return asEByteCodeVerifyStatus::InvalidOperand;
		if ((Requirement.CallableTraits & 64u) != 0 && !Consume(AS_PTR_SIZE, asEByteCodeStorageKind::Address))
			return asEByteCodeVerifyStatus::InvalidOperand;
		for (const asSByteCodeParameterContract& Parameter : Requirement.Parameters)
			if (!Consume(Parameter.StorageDwords, Parameter.StorageKind)) return asEByteCodeVerifyStatus::InvalidOperand;
		Stack.SetNum(Cursor);
		return asEByteCodeVerifyStatus::Succeeded;
	}

	asEByteCodeVerifyStatus ApplyStackEffect(const asSByteCodeInstruction& Instruction, const asSByteCodeOpcodeInfo& Info,
		TArray<asSAbstractDword>& Stack)
	{
		if (Instruction.Opcode == asBC_SwapPtr)
		{
			if (Stack.Num() < 2 * AS_PTR_SIZE) return asEByteCodeVerifyStatus::StackUnderflow;
			for (int32 Index = 0; Index < AS_PTR_SIZE; ++Index)
				Swap(Stack[Stack.Num() - 1 - Index], Stack[Stack.Num() - 1 - AS_PTR_SIZE - Index]);
		}
		if (Instruction.Opcode == asBC_RDSPtr)
		{
			if (Stack.Num() < AS_PTR_SIZE) return asEByteCodeVerifyStatus::StackUnderflow;
			for (int32 Index = Stack.Num() - AS_PTR_SIZE; Index < Stack.Num(); ++Index)
			{
				// RDSPtr is an explicit pointer load, including the VM's guarded
				// null-input path. Its result has pointer representation.
				Stack[Index].Kind = asEByteCodeStorageKind::Address;
				Stack[Index].Local = kUnknownLocal;
				Stack[Index].ObjectLocal = kUnknownLocal;
			}
		}
		const int32 Effect = Info.CallEffect == 0xFFFF ? 0 : Info.StackEffect;
		if (Effect < 0)
		{
			if (Stack.Num() < -Effect) return asEByteCodeVerifyStatus::StackUnderflow;
			Stack.SetNum(Stack.Num() + Effect);
		}
		else if (Effect > 0)
		{
			if (Stack.Num() > kMaxStackDwords - Effect) return asEByteCodeVerifyStatus::InvalidFrame;
			asSAbstractDword Value;
			switch (Instruction.Opcode)
			{
			case asBC_PSF:
				Value.Local = static_cast<int32>(Instruction.Operands[0].Immediate);
				Value.Kind = asEByteCodeStorageKind::Address;
				break;
			case asBC_PshVPtr:
				Value.ObjectLocal = static_cast<int32>(Instruction.Operands[0].Immediate);
				Value.Kind = asEByteCodeStorageKind::Address;
				break;
			case asBC_PshGPtr: case asBC_PshRPtr: case asBC_PshNull:
			case asBC_OBJTYPE: case asBC_PGA: case asBC_VAR: case asBC_PshListElmnt: case asBC_FuncPtr:
				Value.Kind = asEByteCodeStorageKind::Address;
				break;
			default: break;
			}
			for (int32 Index = 0; Index < Effect; ++Index) Stack.Add(Value);
		}
		return asEByteCodeVerifyStatus::Succeeded;
	}

}

namespace
{
asSByteCodeVerifyFault VerifyImage(const asCByteCodeImage& Image, TArray<asSByteCodeFunctionLifetime>* OutLifetimes)
{
	Image.bValidated = false;
	TArray<asSByteCodeFunctionLifetime> Lifetimes;
	TSet<asSStableKey> Known;
	TSet<TPair<uint8, asSStableKey>> RequirementIdentities;
	for (const asSByteCodeRequirement& Requirement : Image.Requirements)
	{
		const TPair<uint8, asSStableKey> Identity(static_cast<uint8>(Requirement.Role), Requirement.Key);
		if (RequirementIdentities.Contains(Identity))
			return MakeFault(asEByteCodeVerifyStatus::CorruptClosure, Requirement.Key, -1, -1);
		RequirementIdentities.Add(Identity);
		Known.Add(Requirement.Key);
	}
	for (const asSByteCodeRequirement& Requirement : Image.Requirements)
	{
		for (const asSStableKey& Referenced : Requirement.ReferencedKeys)
		{
			if (!Referenced.IsZero() && !Known.Contains(Referenced))
			{
				return MakeFault(asEByteCodeVerifyStatus::CorruptClosure, Requirement.Key, -1, -1);
			}
		}
	}
	TSet<asSStableKey> FunctionIdentities;
	for (const asSByteCodeFunctionBody& Function : Image.Functions)
	{
		if (Function.FunctionKey.IsZero())
			return MakeFault(asEByteCodeVerifyStatus::InvalidOperand, Function.FunctionKey, -1, -1);
		if (FunctionIdentities.Contains(Function.FunctionKey))
			return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
		FunctionIdentities.Add(Function.FunctionKey);
		// Local coordinates are positive signed 16-bit offsets. An oversized
		// declaration cannot create an unaddressable tail or wrap VM reservation.
		if (Function.FrameSize > static_cast<asUINT>(kInt16Max))
			return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
		if (Function.Instructions.IsEmpty())
		{
			return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
		}
		if (Function.ParameterSize > static_cast<asUINT>(kInt16Max))
			return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
		for (const asSByteCodeRequirement& Requirement : Image.Requirements)
		{
			if (Requirement.Key != Function.FunctionKey || Requirement.Role != asEByteCodeRequirementRole::Function) continue;
			int64 ArgumentDwords = !Requirement.OwnerKey.IsZero() && (Requirement.CallableTraits & 1u) == 0 ? AS_PTR_SIZE : 0;
			if ((Requirement.CallableTraits & 64u) != 0) ArgumentDwords += AS_PTR_SIZE;
			for (const asSByteCodeParameterContract& Parameter : Requirement.Parameters)
			{
				if (!StorageContractValid(Parameter.StorageDwords, Parameter.StorageKind, false)
					|| Parameter.PassingMode > 3 || (Parameter.PassingMode != 0 && Parameter.StorageKind != asEByteCodeStorageKind::Address))
					return MakeFault(asEByteCodeVerifyStatus::InvalidOperand, Function.FunctionKey, -1, -1);
				ArgumentDwords += Parameter.StorageDwords;
			}
			if (ArgumentDwords != Function.ParameterSize)
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
		}
		if ((Function.Frame.LocalDwords != 0 || Function.Frame.ParameterDwords != 0 || Function.Frame.ResultDwords != 0)
			&& (Function.Frame.LocalDwords != Function.FrameSize || Function.Frame.ParameterDwords != Function.ParameterSize))
		{
			return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
		}
		if (Function.Frame.ResultDwords > 0)
		{
			if (Function.Frame.ResultDwords > static_cast<asUINT>(kInt16Max) + 1u)
			{
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
			}
			if (Function.Frame.ResultOffset <= 0 || !FrameAccessValid(Function, Function.Frame.ResultOffset, static_cast<int32>(Function.Frame.ResultDwords)))
			{
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
			}
		}
		for (int32 SlotIndex = 0; SlotIndex < Function.Frame.InitializedObjectSlots.Num(); ++SlotIndex)
		{
			if (!FrameAccessValid(Function, Function.Frame.InitializedObjectSlots[SlotIndex], OccupiedDwordsFromWidth(4)))
			{
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
			}
		}
		for (int32 CleanupIndex = 0; CleanupIndex < Function.Cleanup.Num(); ++CleanupIndex)
		{
			const asSByteCodeCleanupAction& Cleanup = Function.Cleanup[CleanupIndex];
			if (Cleanup.InstructionIndex >= static_cast<asUINT>(Function.Instructions.Num()))
			{
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, static_cast<int32>(Cleanup.InstructionIndex), -1);
			}
			if (!FrameAccessValid(Function, Cleanup.FrameOffset, OccupiedDwordsFromWidth(4)))
			{
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, static_cast<int32>(Cleanup.InstructionIndex), -1);
			}
			if (Cleanup.Kind != asEByteCodeCleanupKind::None)
			{
				if (Cleanup.TypeSlot >= static_cast<asUINT>(Image.Requirements.Num())
					|| !SlotRoleMatches(asEByteCodeOperandKind::TypeSlot, Image.Requirements[Cleanup.TypeSlot].Role))
				{
					return MakeFault(asEByteCodeVerifyStatus::MissingRequirement, Function.FunctionKey, static_cast<int32>(Cleanup.InstructionIndex), -1);
				}
			}
		}
		asSByteCodeFunctionLifetime Lifetime;
		Lifetime.FunctionKey = Function.FunctionKey;
		Lifetime.FrameOffsets = Function.Frame.InitializedObjectSlots;
		if (static_cast<int64>(Function.Instructions.Num()) * Lifetime.FrameOffsets.Num() > 4000000)
			return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
		Lifetime.TypeSlots.Init(MAX_uint32, Lifetime.FrameOffsets.Num());
		TMap<int32, int32> ObjectIndex;
		TSet<int32> ObjectStorage;
		for (int32 Slot = 0; Slot < Lifetime.FrameOffsets.Num(); ++Slot)
		{
			const int32 Offset = Lifetime.FrameOffsets[Slot];
			if (Offset <= 0 || !FrameAccessValid(Function, Offset, AS_PTR_SIZE))
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
			for (int32 Dword = 0; Dword < AS_PTR_SIZE; ++Dword)
			{
				if (ObjectStorage.Contains(Offset - Dword))
					return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
				ObjectStorage.Add(Offset - Dword);
			}
			ObjectIndex.Add(Offset, Slot);
		}
		TMap<int32, TArray<int32>> InitAt;
		TMap<int32, TArray<int32>> DestroyAt;
		for (const asSByteCodeCleanupAction& Action : Function.Cleanup)
		{
			if (Action.Kind == asEByteCodeCleanupKind::None) continue;
			if (Action.Kind != asEByteCodeCleanupKind::DestroyObject && Action.Kind != asEByteCodeCleanupKind::ReleaseHandle)
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Action.InstructionIndex, -1);
			const int32* Slot = ObjectIndex.Find(Action.FrameOffset);
			if (!Slot || (Action.Path != asEByteCodeCleanupPath::Normal && Action.Path != asEByteCodeCleanupPath::Exception)
				|| Image.Requirements[Action.TypeSlot].Role != asEByteCodeRequirementRole::Type)
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Action.InstructionIndex, -1);
			if (Lifetime.TypeSlots[*Slot] != MAX_uint32 && Lifetime.TypeSlots[*Slot] != Action.TypeSlot)
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Action.InstructionIndex, -1);
			Lifetime.TypeSlots[*Slot] = Action.TypeSlot;
			const asSByteCodeInstruction& Instruction = Function.Instructions[Action.InstructionIndex];
			TArray<int32>& Events = Action.Path == asEByteCodeCleanupPath::Normal
				? DestroyAt.FindOrAdd(Action.InstructionIndex) : InitAt.FindOrAdd(Action.InstructionIndex);
			if (Events.Contains(*Slot))
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Action.InstructionIndex, -1);
			Events.Add(*Slot);
			if (Action.Path == asEByteCodeCleanupPath::Normal)
			{
				if (Instruction.Opcode != asBC_FREE || Instruction.Operands.Num() != 2
					|| Instruction.Operands[0].Immediate != Action.FrameOffset || Instruction.Operands[1].Slot != Action.TypeSlot)
					return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Action.InstructionIndex, -1);
			}
			else if (Instruction.Opcode != asBC_ALLOC && Instruction.Opcode != asBC_CALL
				&& Instruction.Opcode != asBC_CALLSYS && Instruction.Opcode != asBC_COPY)
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Action.InstructionIndex, -1);
		}
		for (asUINT TypeSlot : Lifetime.TypeSlots)
			if (TypeSlot == MAX_uint32)
				return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);

		for (int32 SourceIndex = 0; SourceIndex < Function.Source.Num(); ++SourceIndex)
		{
			const asSByteCodeSourceRecord& Source = Function.Source[SourceIndex];
			if (Source.InstructionIndex >= static_cast<asUINT>(Function.Instructions.Num()))
			{
				return MakeFault(asEByteCodeVerifyStatus::InvalidOperand, Function.FunctionKey, static_cast<int32>(Source.InstructionIndex), -1);
			}
			if (Image.Strings.Num() > 0 && Source.FileStringSlot >= static_cast<asUINT>(Image.Strings.Num()))
			{
				return MakeFault(asEByteCodeVerifyStatus::MissingRequirement, Function.FunctionKey, static_cast<int32>(Source.InstructionIndex), -1);
			}
		}
		for (int32 Index = 0; Index < Function.Instructions.Num(); ++Index)
		{
			const asSByteCodeInstruction& Instruction = Function.Instructions[Index];
			const asUINT OpcodeValue = static_cast<asUINT>(Instruction.Opcode);
			if (OpcodeValue == asBC_STR || OpcodeValue > asBC_MAXBYTECODE)
			{
				return MakeFault(asEByteCodeVerifyStatus::UnknownOpcode, Function.FunctionKey, Index, -1);
			}
			asSByteCodeOpcodeInfo Info;
			if (!asByteCodeOpcodeGetInfo(Instruction.Opcode, Info))
			{
				return MakeFault(asEByteCodeVerifyStatus::UnknownOpcode, Function.FunctionKey, Index, -1);
			}
			if (Instruction.Operands.Num() != static_cast<int32>(Info.OperandCount))
			{
				return MakeFault(asEByteCodeVerifyStatus::InvalidOperand, Function.FunctionKey, Index, -1);
			}
			if (Instruction.Opcode == asBC_JMPP)
			{
				const int64 Maximum = Instruction.Operands[1].Immediate;
				const int64 Remaining = static_cast<int64>(Function.Instructions.Num()) - Index - 1;
				if (Maximum < 0 || Maximum >= Remaining)
					return MakeFault(asEByteCodeVerifyStatus::InvalidJump, Function.FunctionKey, Index, 1);
				for (int64 EntryIndex = 0; EntryIndex <= Maximum; ++EntryIndex)
				{
					const auto& Entry = Function.Instructions[Index + 1 + static_cast<int32>(EntryIndex)];
					if (Entry.Opcode != asBC_JMP || asBCTypeSize[asBCInfo[Entry.Opcode].type] != 2)
						return MakeFault(asEByteCodeVerifyStatus::InvalidJump, Function.FunctionKey, Index, 1);
				}
			}
			for (int32 OperandIndex = 0; OperandIndex < Instruction.Operands.Num(); ++OperandIndex)
			{
				const asSByteCodeOperand& Operand = Instruction.Operands[OperandIndex];
				const asSByteCodeOpcodeOperandInfo& OperandInfo = Info.OperandInfo[OperandIndex];
				if (Operand.Kind != Info.Operands[OperandIndex])
				{
					return MakeFault(asEByteCodeVerifyStatus::InvalidOperand, Function.FunctionKey, Index, OperandIndex);
				}
				if (OperandInfo.SignedWidth > 0 && !FitsSignedWidth(Operand.Immediate, OperandInfo.SignedWidth))
				{
					return MakeFault(asEByteCodeVerifyStatus::InvalidOperand, Function.FunctionKey, Index, OperandIndex);
				}
				if (Operand.Kind == asEByteCodeOperandKind::Label)
				{
					if (Operand.Immediate < 0 || Operand.Immediate >= Function.Instructions.Num())
					{
						return MakeFault(asEByteCodeVerifyStatus::InvalidJump, Function.FunctionKey, Index, OperandIndex);
					}
				}
				if (Operand.Kind == asEByteCodeOperandKind::StringSlot)
				{
					if (Operand.Slot >= static_cast<asUINT>(Image.Strings.Num()))
					{
						return MakeFault(asEByteCodeVerifyStatus::MissingRequirement, Function.FunctionKey, Index, OperandIndex);
					}
				}
				if (Operand.Kind == asEByteCodeOperandKind::TypeSlot
					|| Operand.Kind == asEByteCodeOperandKind::FunctionSlot
					|| Operand.Kind == asEByteCodeOperandKind::PropertySlot
					|| Operand.Kind == asEByteCodeOperandKind::GlobalSlot)
				{
					if (Operand.Slot >= static_cast<asUINT>(Image.Requirements.Num()))
					{
						return MakeFault(asEByteCodeVerifyStatus::MissingRequirement, Function.FunctionKey, Index, OperandIndex);
					}
					if (!SlotRoleMatches(Operand.Kind, Image.Requirements[Operand.Slot].Role))
					{
						return MakeFault(asEByteCodeVerifyStatus::InvalidOperand, Function.FunctionKey, Index, OperandIndex);
					}
				}
				if (Operand.Kind == asEByteCodeOperandKind::Local)
				{
					if (!FrameAccessValid(Function, Operand.Immediate, OccupiedDwordsFromWidth(OperandInfo.StorageWidth)))
					{
						return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Index, OperandIndex);
					}
				}
			}
		}
		{
			const int32 Count = Function.Instructions.Num();
			TArray<TArray<asSAbstractDword>> InStack;
			InStack.SetNum(Count);
			TArray<bool> Seen;
			Seen.Init(false, Count);
			Lifetime.Before.SetNum(Count);
			Lifetime.Before[0].Init(asEByteCodeObjectState::Uninitialized, Lifetime.FrameOffsets.Num());
			int64 StoredStackDwords = 0;
			TArray<int32> Work;
			Seen[0] = true;
			Work.Add(0);
			int32 Steps = 0;
			while (Work.Num() > 0)
			{
				if (++Steps > 1000000)
				{
					return MakeFault(asEByteCodeVerifyStatus::InvalidJump, Function.FunctionKey, -1, -1);
				}
				const int32 Index = Work.Pop();
				asSByteCodeOpcodeInfo Info;
				if (!asByteCodeOpcodeGetInfo(Function.Instructions[Index].Opcode, Info))
				{
					return MakeFault(asEByteCodeVerifyStatus::UnknownOpcode, Function.FunctionKey, Index, -1);
				}
				TArray<asSAbstractDword> OutStack = InStack[Index];
				const asSByteCodeInstruction& Instruction = Function.Instructions[Index];
				if (Instruction.Opcode == asBC_RET && (Instruction.Operands[0].Immediate < 0
					|| Instruction.Operands[0].Immediate != Function.ParameterSize))
					return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Index, 0);
				TArray<asEByteCodeObjectState> OutObjects = Lifetime.Before[Index];
				int32 ProducedSlot = INDEX_NONE;
				if (Instruction.Opcode == asBC_ALLOC && !OutStack.IsEmpty())
				{
					if (const int32* Slot = ObjectIndex.Find(OutStack.Last().Local))
					{
						if (OutObjects[*Slot] != asEByteCodeObjectState::Uninitialized
							|| Instruction.Operands[0].Slot != Lifetime.TypeSlots[*Slot])
							return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Index, -1);
						OutObjects[*Slot] = asEByteCodeObjectState::Allocated;
						ProducedSlot = *Slot;
					}
				}
				const asSByteCodeRequirement* Callee = CallRequirement(Image, Instruction);
				if (Callee && (Callee->CallableTraits & 32u) && !OutStack.IsEmpty())
				{
					if (const int32* Slot = ObjectIndex.Find(OutStack.Last().ObjectLocal))
					{
						if (OutObjects[*Slot] != asEByteCodeObjectState::Allocated
							|| Callee->OwnerKey != Image.Requirements[Lifetime.TypeSlots[*Slot]].Key)
							return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Index, -1);
						ProducedSlot = *Slot;
					}
				}
				if (Instruction.Opcode == asBC_COPY && !OutStack.IsEmpty())
					if (const int32* Slot = ObjectIndex.Find(OutStack.Last().ObjectLocal)) ProducedSlot = *Slot;
				if (Instruction.Opcode == asBC_FREE)
				{
					if (const int32* Slot = ObjectIndex.Find(static_cast<int32>(Instruction.Operands[0].Immediate)))
					{
						const TArray<int32>* Destroy = DestroyAt.Find(Index);
						if (OutObjects[*Slot] != asEByteCodeObjectState::Constructed || !Destroy || !Destroy->Contains(*Slot)
							|| Instruction.Operands[1].Slot != Lifetime.TypeSlots[*Slot])
							return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Index, -1);
						OutObjects[*Slot] = asEByteCodeObjectState::Uninitialized;
					}
				}
				if (const TArray<int32>* Init = InitAt.Find(Index))
				{
					for (int32 Slot : *Init)
					{
						if (Slot != ProducedSlot || OutObjects[Slot] != asEByteCodeObjectState::Allocated)
							return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Index, -1);
						OutObjects[Slot] = asEByteCodeObjectState::Constructed;
					}
				}
				if (Info.Control == asEByteCodeControlKind::Return)
					for (asEByteCodeObjectState State : OutObjects)
						if (State != asEByteCodeObjectState::Uninitialized)
							return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Index, -1);
				asEByteCodeVerifyStatus StackStatus = asEByteCodeVerifyStatus::Succeeded;
				if (Info.CallEffect == 0xFFFF && CallRequirement(Image, Instruction))
					StackStatus = ApplyCallContract(*CallRequirement(Image, Instruction), OutStack);
				else
					StackStatus = ApplyStackEffect(Instruction, Info, OutStack);
				if (StackStatus != asEByteCodeVerifyStatus::Succeeded)
					return MakeFault(StackStatus, Function.FunctionKey, Index, -1);
				Lifetime.TemporaryStackDwords = FMath::Max(Lifetime.TemporaryStackDwords, static_cast<asUINT>(OutStack.Num()));

				auto Enqueue = [&](int32 Target) -> asSByteCodeVerifyFault
				{
					if (Target < 0 || Target >= Count)
					{
						return MakeFault(asEByteCodeVerifyStatus::InvalidJump, Function.FunctionKey, Index, 0);
					}
					
					if (!Seen[Target])
					{
						StoredStackDwords += OutStack.Num();
						if (StoredStackDwords > 4000000)
							return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Index, -1);
						InStack[Target] = OutStack;
						Lifetime.Before[Target] = OutObjects;
						Seen[Target] = true;
						Work.Add(Target);
						return asSByteCodeVerifyFault{};
					}
					if (InStack[Target].Num() != OutStack.Num())
						return MakeFault(asEByteCodeVerifyStatus::InvalidJump, Function.FunctionKey, Index, 0);
					if (Lifetime.Before[Target] != OutObjects)
						return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Index, -1);
					bool bChanged = false;
					for (int32 Slot = 0; Slot < OutStack.Num(); ++Slot)
					{
						if (InStack[Target][Slot].Kind != OutStack[Slot].Kind)
							return MakeFault(asEByteCodeVerifyStatus::InvalidJump, Function.FunctionKey, Index, 0);
						if (InStack[Target][Slot].ObjectLocal != OutStack[Slot].ObjectLocal && InStack[Target][Slot].ObjectLocal != kUnknownLocal)
						{
							InStack[Target][Slot].ObjectLocal = kUnknownLocal;
							bChanged = true;
						}
						if (InStack[Target][Slot].Local != OutStack[Slot].Local && InStack[Target][Slot].Local != kUnknownLocal)
						{
							InStack[Target][Slot].Local = kUnknownLocal;
							bChanged = true;
						}
					}
					if (bChanged) Work.AddUnique(Target);
					return asSByteCodeVerifyFault{};
				};
				if (Info.Control == asEByteCodeControlKind::Jump || Info.Control == asEByteCodeControlKind::ConditionalJump)
				{
					if (Function.Instructions[Index].Operands.Num() < 1)
					{
						return MakeFault(asEByteCodeVerifyStatus::InvalidOperand, Function.FunctionKey, Index, -1);
					}
					const asSByteCodeVerifyFault JumpFault = Enqueue(static_cast<int32>(Function.Instructions[Index].Operands[0].Immediate));
					if (JumpFault.Status != asEByteCodeVerifyStatus::Succeeded) return JumpFault;
					if (Info.Control == asEByteCodeControlKind::ConditionalJump)
					{
						const asSByteCodeVerifyFault NextFault = Enqueue(Index + 1);
						if (NextFault.Status != asEByteCodeVerifyStatus::Succeeded) return NextFault;
					}
				}
				else if (Info.Control == asEByteCodeControlKind::IndexedJump)
				{
					const int32 Maximum = static_cast<int32>(Instruction.Operands[1].Immediate);
					for (int32 Entry = 0; Entry <= Maximum; ++Entry)
					{
						const asSByteCodeVerifyFault EntryFault = Enqueue(Index + 1 + Entry);
						if (EntryFault.Status != asEByteCodeVerifyStatus::Succeeded) return EntryFault;
					}
				}
				else if (Info.Control == asEByteCodeControlKind::Return || Info.Control == asEByteCodeControlKind::Throw)
				{
				}
				else if (Index + 1 < Count)
				{
					const asSByteCodeVerifyFault NextFault = Enqueue(Index + 1);
					if (NextFault.Status != asEByteCodeVerifyStatus::Succeeded) return NextFault;
				}
				else
				{
					return MakeFault(asEByteCodeVerifyStatus::InvalidJump, Function.FunctionKey, Index, -1);
				}
			}
			Lifetime.Reachable = MoveTemp(Seen);
		}
		if (OutLifetimes) Lifetimes.Add(MoveTemp(Lifetime));
	}

	Image.bValidated = true;
	if (OutLifetimes) *OutLifetimes = MoveTemp(Lifetimes);
	return asSByteCodeVerifyFault{};
}
}

asSByteCodeVerifyFault asCByteCodeVerifier::VerifyWithFault(const asCByteCodeImage& Image)
{
	return VerifyImage(Image, nullptr);
}

asSByteCodeVerifyFault asCByteCodeVerifier::VerifyWithLifetimes(const asCByteCodeImage& Image, TArray<asSByteCodeFunctionLifetime>& Out)
{
	Out.Reset();
	return VerifyImage(Image, &Out);
}

asEByteCodeVerifyStatus asCByteCodeVerifier::Verify(const asCByteCodeImage& Image)
{
	return VerifyWithFault(Image).Status;
}

END_AS_NAMESPACE
