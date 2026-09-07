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
		if (Occupied <= 0) return false;
		for (int32 Index = 0; Index < Occupied; ++Index)
		{
			const int64 Dword = Offset + Index;
			if (Dword < kInt16Min || Dword > kInt16Max) return false;
			if (Dword >= 0)
			{
				if (static_cast<uint64>(Dword) >= Function.FrameSize) return false;
			}
			else
			{
				const int64 ParamBegin = -static_cast<int64>(Function.ParameterSize);
				if (Dword < ParamBegin) return false;
			}
		}
		return true;
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

	int32 TypeUseValueDwords(const asCByteCodeImage& Image, const asSStableKey& TypeUse)
	{
		for (const asSByteCodeRequirement& Requirement : Image.Requirements)
		{
			if (Requirement.Role != asEByteCodeRequirementRole::TypeUse) continue;
			if (Requirement.Key != TypeUse) continue;
			if (Requirement.NativeOffset > 0) return Requirement.NativeOffset;
			break;
		}
		return 1;
	}

	int32 CallArgumentDwords(const asCByteCodeImage& Image, const asSByteCodeInstruction& Instruction)
	{
		for (const asSByteCodeOperand& Operand : Instruction.Operands)
		{
			if (Operand.Kind != asEByteCodeOperandKind::FunctionSlot) continue;
			if (Operand.Slot >= static_cast<asUINT>(Image.Requirements.Num())) return -1;
			const asSByteCodeRequirement& Requirement = Image.Requirements[Operand.Slot];
			if (Requirement.Role != asEByteCodeRequirementRole::Function) return -1;
			int32 Consumed = 0;
			for (const asSByteCodeParameterContract& Parameter : Requirement.Parameters)
			{
				if (Parameter.PassingMode != 0)
				{
					Consumed += static_cast<int32>(AS_PTR_SIZE);
				}
				else
				{
					Consumed += TypeUseValueDwords(Image, Parameter.TypeUse);
				}
			}
			if (!Requirement.OwnerKey.IsZero() && (Requirement.CallableTraits & 1u) == 0)
			{
				Consumed += static_cast<int32>(AS_PTR_SIZE);
			}
			return Consumed;
		}
		return 0;
	}

	bool InstructionCanReach(const TArray<TArray<int32>>& Successors, int32 From, int32 To)
	{
		if (From < 0 || To < 0) return false;
		TArray<int32> Work;
		TSet<int32> Seen;
		Work.Add(From);
		Seen.Add(From);
		while (Work.Num() > 0)
		{
			const int32 Index = Work.Pop();
			if (Index >= Successors.Num()) continue;
			for (int32 Next : Successors[Index])
			{
				if (Next == To) return true;
				if (!Seen.Contains(Next))
				{
					Seen.Add(Next);
					Work.Add(Next);
				}
			}
		}
		return false;
	}

	bool PathIncludesNormal(asEByteCodeCleanupPath Path)
	{
		return Path == asEByteCodeCleanupPath::Normal || Path == asEByteCodeCleanupPath::Both;
	}

	bool PathIncludesException(asEByteCodeCleanupPath Path)
	{
		return Path == asEByteCodeCleanupPath::Exception || Path == asEByteCodeCleanupPath::Both;
	}
}

asSByteCodeVerifyFault asCByteCodeVerifier::VerifyWithFault(const asCByteCodeImage& Image)
{
	Image.bValidated = false;
	TSet<asSStableKey> Known;
	for (const asSByteCodeRequirement& Requirement : Image.Requirements)
	{
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
	for (const asSByteCodeFunctionBody& Function : Image.Functions)
	{
		if (Function.Instructions.IsEmpty())
		{
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
			if (!FrameAccessValid(Function, Function.Frame.ResultOffset, static_cast<int32>(Function.Frame.ResultDwords)))
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
		TArray<TPair<int32, int32>> NormalDestroySites;
		{
			TSet<int32> DeclaredLive;
			for (int32 Slot : Function.Frame.InitializedObjectSlots)
			{
				DeclaredLive.Add(Slot);
			}
			TSet<int32> DestroyedNormal;
			TSet<int32> DestroyedException;
			for (int32 CleanupIndex = 0; CleanupIndex < Function.Cleanup.Num(); ++CleanupIndex)
			{
				const asSByteCodeCleanupAction& Cleanup = Function.Cleanup[CleanupIndex];
				if (Cleanup.Kind != asEByteCodeCleanupKind::DestroyObject && Cleanup.Kind != asEByteCodeCleanupKind::ReleaseHandle)
				{
					continue;
				}
				if (!DeclaredLive.Contains(Cleanup.FrameOffset))
				{
					return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, static_cast<int32>(Cleanup.InstructionIndex), -1);
				}
				if (PathIncludesNormal(Cleanup.Path))
				{
					DestroyedNormal.Add(Cleanup.FrameOffset);
					NormalDestroySites.Emplace(Cleanup.FrameOffset, static_cast<int32>(Cleanup.InstructionIndex));
				}
				if (PathIncludesException(Cleanup.Path))
				{
					if (DestroyedException.Contains(Cleanup.FrameOffset))
					{
						return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, static_cast<int32>(Cleanup.InstructionIndex), -1);
					}
					DestroyedException.Add(Cleanup.FrameOffset);
				}
			}
			for (int32 Slot : DeclaredLive)
			{
				if (!DestroyedNormal.Contains(Slot))
				{
					return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, -1, -1);
				}
			}
		}
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
			TArray<int32> InStack;
			InStack.Init(-1, Count);
			TArray<TArray<int32>> Successors;
			Successors.SetNum(Count);
			TArray<int32> Work;
			InStack[0] = 0;
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
				int32 OutStack = InStack[Index];
				if (Info.CallEffect == 0xFFFF)
				{
					const int32 Consumed = CallArgumentDwords(Image, Function.Instructions[Index]);
					if (Consumed < 0)
					{
						return MakeFault(asEByteCodeVerifyStatus::MissingRequirement, Function.FunctionKey, Index, -1);
					}
					OutStack -= Consumed;
				}
				else
				{
					OutStack += static_cast<int32>(Info.StackEffect);
				}
				if (OutStack < 0)
				{
					return MakeFault(asEByteCodeVerifyStatus::StackUnderflow, Function.FunctionKey, Index, -1);
				}
				auto Enqueue = [&](int32 Target) -> asSByteCodeVerifyFault
				{
					if (Target < 0 || Target >= Count)
					{
						return MakeFault(asEByteCodeVerifyStatus::InvalidJump, Function.FunctionKey, Index, 0);
					}
					Successors[Index].AddUnique(Target);
					if (InStack[Target] < 0)
					{
						InStack[Target] = OutStack;
						Work.Add(Target);
						return asSByteCodeVerifyFault{};
					}
					if (InStack[Target] != OutStack)
					{
						return MakeFault(asEByteCodeVerifyStatus::InvalidJump, Function.FunctionKey, Index, 0);
					}
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
			for (int32 Left = 0; Left < NormalDestroySites.Num(); ++Left)
			{
				for (int32 Right = Left + 1; Right < NormalDestroySites.Num(); ++Right)
				{
					if (NormalDestroySites[Left].Key != NormalDestroySites[Right].Key) continue;
					const int32 First = NormalDestroySites[Left].Value;
					const int32 Second = NormalDestroySites[Right].Value;
					if (First == Second
						|| InstructionCanReach(Successors, First, Second)
						|| InstructionCanReach(Successors, Second, First))
					{
						return MakeFault(asEByteCodeVerifyStatus::InvalidFrame, Function.FunctionKey, Second, -1);
					}
				}
			}
		}
	}
	Image.bValidated = true;
	return asSByteCodeVerifyFault{};
}

asEByteCodeVerifyStatus asCByteCodeVerifier::Verify(const asCByteCodeImage& Image)
{
	return VerifyWithFault(Image).Status;
}

END_AS_NAMESPACE
