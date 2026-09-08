#include "as_bytecode_linker.h"
#include "as_bytecode_verifier.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_memory.h"
#include "as_callfunc.h"
#include "as_metadata_fingerprint.h"
#include "as_datatype.h"
#include "frontend/as_canonical_encoding.h"
#include "frontend/as_type_context.h"
#include "frontend/as_type_identity.h"
#include "HAL/UnrealMemory.h"

BEGIN_AS_NAMESPACE

namespace
{
	void EmitDword(TArray<asDWORD>& Code, asDWORD Value)
	{
		Code.Add(Value);
	}

	void EmitPointer(TArray<asDWORD>& Code, asPWORD Value)
	{
		asDWORD Words[AS_PTR_SIZE] = {};
		FMemory::Memcpy(Words, &Value, sizeof(Value));
		for (int Index = 0; Index < AS_PTR_SIZE; ++Index)
		{
			EmitDword(Code, Words[Index]);
		}
	}

	asCScriptFunction* ResolveFunctionSlot(asCScriptEngine& Engine, const asCByteCodeImage& Image, asUINT Slot)
	{
		if (Slot >= static_cast<asUINT>(Image.Requirements.Num())) return nullptr;
		const asSByteCodeRequirement& Requirement = Image.Requirements[Slot];
		if (Requirement.Role != asEByteCodeRequirementRole::Function) return nullptr;
		return Engine.FindMetadataFunction(Requirement.Key);
	}

	asCObjectType* ResolveTypeSlot(asCScriptEngine& Engine, const asCByteCodeImage& Image, asUINT Slot)
	{
		if (Slot >= static_cast<asUINT>(Image.Requirements.Num())) return nullptr;
		const asSByteCodeRequirement& Requirement = Image.Requirements[Slot];
		if (Requirement.Role != asEByteCodeRequirementRole::Type) return nullptr;
		return CastToObjectType(Engine.FindMetadataType(Requirement.Key));
	}

	void* ResolveGlobalAddress(asCScriptEngine& Engine, const asCByteCodeImage& Image, asUINT Slot)
	{
		if (Slot >= static_cast<asUINT>(Image.Requirements.Num())) return nullptr;
		const asSByteCodeRequirement& Requirement = Image.Requirements[Slot];
		if (Requirement.Role != asEByteCodeRequirementRole::Global) return nullptr;
		return Engine.FindBoundGlobalStorage(Requirement.Key);
	}

	asEByteCodeLinkStatus LowerInstruction(
		const asSByteCodeInstruction& Instruction,
		TArray<asDWORD>& Code,
		asCScriptEngine& Engine,
		const asCByteCodeImage& Image)
	{
		asDWORD Words[4] = {};
		Words[0] = static_cast<asDWORD>(Instruction.Opcode);
		switch (Instruction.Opcode)
		{
		case asBC_PshC4:
			if (Instruction.Operands.Num() > 0) Words[1] = static_cast<asDWORD>(Instruction.Operands[0].Immediate);
			EmitDword(Code, Words[0]);
			EmitDword(Code, Words[1]);
			break;
		case asBC_PshC8:
		{
			asQWORD Immediate = Instruction.Operands.Num() > 0 ? static_cast<asQWORD>(Instruction.Operands[0].Immediate) : 0;
			EmitDword(Code, Words[0]);
			asDWORD ImmediateWords[2] = {};
			FMemory::Memcpy(ImmediateWords, &Immediate, sizeof(Immediate));
			EmitDword(Code, ImmediateWords[0]);
			EmitDword(Code, ImmediateWords[1]);
			break;
		}
		case asBC_SetV4:
			if (Instruction.Operands.Num() > 0) reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			if (Instruction.Operands.Num() > 1) Words[1] = static_cast<asDWORD>(Instruction.Operands[1].Immediate);
			else if (Instruction.Operands.Num() == 1) Words[1] = static_cast<asDWORD>(Instruction.Operands[0].Immediate);
			EmitDword(Code, Words[0]);
			EmitDword(Code, Words[1]);
			break;
		case asBC_CpyVtoR4:
		case asBC_CpyRtoV4:
		case asBC_PshV4:
		case asBC_PshVPtr:
		case asBC_PSF:
		case asBC_WRTV4:
		case asBC_RDR4:
		case asBC_RET:
			if (Instruction.Operands.Num() > 0) reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			EmitDword(Code, Words[0]);
			break;
		case asBC_PopRPtr:
		case asBC_RDSPtr:
		case asBC_SUSPEND:
		case asBC_SaveReturnValue:
			EmitDword(Code, Words[0]);
			break;
		case asBC_JitEntry:
			EmitDword(Code, Words[0]);
			EmitPointer(Code, 0);
			break;
		case asBC_ThrowException:
			if (Instruction.Operands.Num() > 0) reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			EmitDword(Code, Words[0]);
			break;
		case asBC_ADDSi:
			if (Instruction.Operands.Num() > 0) reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			EmitDword(Code, Words[0]);
			EmitDword(Code, 0);
			break;
		case asBC_CALLSYS:
		case asBC_Thiscall1:
		case asBC_FuncPtr:
		{
			if (Instruction.Operands.Num() == 0) return asEByteCodeLinkStatus::InvalidArgument;
			asCScriptFunction* Target = ResolveFunctionSlot(Engine, Image, Instruction.Operands[0].Slot);
			if (!Target) return asEByteCodeLinkStatus::MissingDeclaration;
			if ((Instruction.Opcode == asBC_CALLSYS || Instruction.Opcode == asBC_Thiscall1)
				&& Target->funcType == asFUNC_SYSTEM && Engine.AcquireSystemInterface(Target) == nullptr)
			{
				return asEByteCodeLinkStatus::MissingBinding;
			}
			EmitDword(Code, Words[0]);
			EmitPointer(Code, reinterpret_cast<asPWORD>(Target));
			break;
		}
		case asBC_CALL:
		case asBC_CALLINTF:
		case asBC_CALLBND:
		{
			if (Instruction.Operands.Num() == 0) return asEByteCodeLinkStatus::InvalidArgument;
			asCScriptFunction* Target = ResolveFunctionSlot(Engine, Image, Instruction.Operands[0].Slot);
			if (!Target) return asEByteCodeLinkStatus::MissingDeclaration;
			if (Instruction.Opcode == asBC_CALLBND
				&& Target->funcType == asFUNC_SYSTEM
				&& Engine.AcquireSystemInterface(Target) == nullptr)
			{
				return asEByteCodeLinkStatus::MissingBinding;
			}
			EmitDword(Code, Words[0]);
			EmitDword(Code, static_cast<asDWORD>(Target->GetId()));
			break;
		}
		case asBC_Cast:
		{
			if (Instruction.Operands.Num() == 0) return asEByteCodeLinkStatus::InvalidArgument;
			asCObjectType* Type = ResolveTypeSlot(Engine, Image, Instruction.Operands[0].Slot);
			if (!Type) return asEByteCodeLinkStatus::MissingDeclaration;
			EmitDword(Code, Words[0]);
			EmitDword(Code, static_cast<asDWORD>(Type->GetTypeId()));
			break;
		}
		case asBC_CallPtr:
		case asBC_ClrVPtr:
		case asBC_STOREOBJ:
		case asBC_LOADOBJ:
		case asBC_CpyRtoV8:
			if (Instruction.Operands.Num() > 0) reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			EmitDword(Code, Words[0]);
			break;
		case asBC_ALLOC:
		{
			if (Instruction.Operands.Num() == 0) return asEByteCodeLinkStatus::InvalidArgument;
			asCObjectType* Type = ResolveTypeSlot(Engine, Image, Instruction.Operands[0].Slot);
			if (!Type) return asEByteCodeLinkStatus::MissingDeclaration;
			EmitDword(Code, Words[0]);
			EmitPointer(Code, reinterpret_cast<asPWORD>(Type));
			EmitDword(Code, Instruction.Operands.Num() > 1 ? static_cast<asDWORD>(Instruction.Operands[1].Immediate) : 0);
			break;
		}
		case asBC_FREE:
		case asBC_DestructScript:
		{
			if (Instruction.Operands.Num() < 2) return asEByteCodeLinkStatus::InvalidArgument;
			if (Instruction.Operands.Num() > 0) reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			asCObjectType* Type = ResolveTypeSlot(Engine, Image, Instruction.Operands[1].Slot);
			if (!Type) return asEByteCodeLinkStatus::MissingDeclaration;
			EmitDword(Code, Words[0]);
			EmitPointer(Code, reinterpret_cast<asPWORD>(Type));
			break;
		}
		case asBC_REFCPY:
		case asBC_FinConstruct:
		case asBC_CopyScript:
		case asBC_OBJTYPE:
		{
			if (Instruction.Operands.Num() == 0) return asEByteCodeLinkStatus::InvalidArgument;
			asCObjectType* Type = ResolveTypeSlot(Engine, Image, Instruction.Operands[0].Slot);
			if (!Type) return asEByteCodeLinkStatus::MissingDeclaration;
			EmitDword(Code, Words[0]);
			EmitPointer(Code, reinterpret_cast<asPWORD>(Type));
			break;
		}
		case asBC_RefCpyV:
		{
			if (Instruction.Operands.Num() < 2) return asEByteCodeLinkStatus::InvalidArgument;
			reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			asCObjectType* Type = ResolveTypeSlot(Engine, Image, Instruction.Operands[1].Slot);
			if (!Type) return asEByteCodeLinkStatus::MissingDeclaration;
			EmitDword(Code, Words[0]);
			EmitPointer(Code, reinterpret_cast<asPWORD>(Type));
			break;
		}
		case asBC_LoadVObjR:
		case asBC_LoadRObjR:
			if (Instruction.Operands.Num() > 0) reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			if (Instruction.Operands.Num() > 1) reinterpret_cast<short*>(Words)[2] = static_cast<short>(Instruction.Operands[1].Immediate);
			EmitDword(Code, Words[0]);
			EmitDword(Code, Words[1]);
			EmitDword(Code, 0);
			break;
		case asBC_COPY:
			if (Instruction.Operands.Num() > 0) reinterpret_cast<asWORD*>(Words)[1] = static_cast<asWORD>(Instruction.Operands[0].Immediate);
			EmitDword(Code, Words[0]);
			EmitDword(Code, 0);
			break;
		case asBC_CHKREF:
		case asBC_PshNull:
			EmitDword(Code, Words[0]);
			break;
		case asBC_CpyVtoV4:
		case asBC_CMPi:
		case asBC_iTOd:
			if (Instruction.Operands.Num() > 0) reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			if (Instruction.Operands.Num() > 1) reinterpret_cast<short*>(Words)[2] = static_cast<short>(Instruction.Operands[1].Immediate);
			EmitDword(Code, Words[0]);
			EmitDword(Code, Words[1]);
			break;
		case asBC_SetV8:
		{
			if (Instruction.Operands.Num() > 0) reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			EmitDword(Code, Words[0]);
			asQWORD Immediate = Instruction.Operands.Num() > 1 ? static_cast<asQWORD>(Instruction.Operands[1].Immediate) : 0;
			asDWORD ImmediateWords[2] = {};
			FMemory::Memcpy(ImmediateWords, &Immediate, sizeof(Immediate));
			EmitDword(Code, ImmediateWords[0]);
			EmitDword(Code, ImmediateWords[1]);
			break;
		}
		case asBC_TZ:
		case asBC_TNZ:
		case asBC_TS:
		case asBC_TNS:
		case asBC_TP:
		case asBC_TNP:
		case asBC_NEGi:
		case asBC_BNOT:
		case asBC_CpyVtoR8:
			if (Instruction.Operands.Num() > 0) reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			EmitDword(Code, Words[0]);
			break;
		case asBC_MULi:
		case asBC_SUBi:
		case asBC_ADDi:
		case asBC_DIVi:
		case asBC_MODi:
		case asBC_POWi:
		case asBC_BOR:
		case asBC_ADDd:
			if (Instruction.Operands.Num() > 0) reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			if (Instruction.Operands.Num() > 1) reinterpret_cast<short*>(Words)[2] = static_cast<short>(Instruction.Operands[1].Immediate);
			if (Instruction.Operands.Num() > 2) reinterpret_cast<short*>(Words)[3] = static_cast<short>(Instruction.Operands[2].Immediate);
			EmitDword(Code, Words[0]);
			EmitDword(Code, Words[1]);
			break;
		case asBC_PGA:
		case asBC_PshGPtr:
		case asBC_PshG4:
		case asBC_LDG:
		{
			if (Instruction.Operands.Num() == 0) return asEByteCodeLinkStatus::InvalidArgument;
			void* Address = ResolveGlobalAddress(Engine, Image, Instruction.Operands[0].Slot);
			if (!Address) return asEByteCodeLinkStatus::MissingBinding;
			EmitDword(Code, Words[0]);
			EmitPointer(Code, reinterpret_cast<asPWORD>(Address));
			break;
		}
		case asBC_CpyVtoG4:
		case asBC_CpyGtoV4:
		case asBC_LdGRdR4:
		{
			if (Instruction.Operands.Num() < 2) return asEByteCodeLinkStatus::InvalidArgument;
			reinterpret_cast<short*>(Words)[1] = static_cast<short>(Instruction.Operands[0].Immediate);
			void* Address = ResolveGlobalAddress(Engine, Image, Instruction.Operands[1].Slot);
			if (!Address) return asEByteCodeLinkStatus::MissingBinding;
			EmitDword(Code, Words[0]);
			EmitPointer(Code, reinterpret_cast<asPWORD>(Address));
			break;
		}
		case asBC_SetG4:
		{
			if (Instruction.Operands.Num() < 2) return asEByteCodeLinkStatus::InvalidArgument;
			void* Address = ResolveGlobalAddress(Engine, Image, Instruction.Operands[0].Slot);
			if (!Address) return asEByteCodeLinkStatus::MissingBinding;
			EmitDword(Code, Words[0]);
			EmitPointer(Code, reinterpret_cast<asPWORD>(Address));
			EmitDword(Code, static_cast<asDWORD>(Instruction.Operands[1].Immediate));
			break;
		}
		case asBC_JMP:
		case asBC_JZ:
		case asBC_JNZ:
		case asBC_JS:
		case asBC_JNS:
		case asBC_JP:
		case asBC_JNP:
		case asBC_JLowZ:
		case asBC_JLowNZ:
		{
			EmitDword(Code, Words[0]);
			const int Target = Instruction.Operands.Num() > 0 ? static_cast<int>(Instruction.Operands[0].Immediate) : 0;
			EmitDword(Code, static_cast<asDWORD>(Target));
			break;
		}
		default:
		{
			const asEBCType Type = asBCInfo[Instruction.Opcode].type;
			auto Imm = [&](int32 Index) -> int64
			{
				return Instruction.Operands.Num() > Index ? Instruction.Operands[Index].Immediate : 0;
			};
			switch (Type)
			{
			case asBCTYPE_NO_ARG:
				EmitDword(Code, Words[0]);
				break;
			case asBCTYPE_W_ARG:
			case asBCTYPE_wW_ARG:
			case asBCTYPE_rW_ARG:
				reinterpret_cast<short*>(Words)[1] = static_cast<short>(Imm(0));
				EmitDword(Code, Words[0]);
				break;
			case asBCTYPE_DW_ARG:
				EmitDword(Code, Words[0]);
				EmitDword(Code, static_cast<asDWORD>(Imm(0)));
				break;
			case asBCTYPE_rW_DW_ARG:
			case asBCTYPE_wW_DW_ARG:
			case asBCTYPE_W_DW_ARG:
				reinterpret_cast<short*>(Words)[1] = static_cast<short>(Imm(0));
				EmitDword(Code, Words[0]);
				EmitDword(Code, static_cast<asDWORD>(Imm(1)));
				break;
			case asBCTYPE_QW_ARG:
			{
				asQWORD Immediate = static_cast<asQWORD>(Imm(0));
				asDWORD ImmediateWords[2] = {};
				FMemory::Memcpy(ImmediateWords, &Immediate, sizeof(Immediate));
				EmitDword(Code, Words[0]);
				EmitDword(Code, ImmediateWords[0]);
				EmitDword(Code, ImmediateWords[1]);
				break;
			}
			case asBCTYPE_DW_DW_ARG:
				EmitDword(Code, Words[0]);
				EmitDword(Code, static_cast<asDWORD>(Imm(0)));
				EmitDword(Code, static_cast<asDWORD>(Imm(1)));
				break;
			case asBCTYPE_wW_rW_rW_ARG:
				reinterpret_cast<short*>(Words)[1] = static_cast<short>(Imm(0));
				reinterpret_cast<short*>(Words)[2] = static_cast<short>(Imm(1));
				reinterpret_cast<short*>(Words)[3] = static_cast<short>(Imm(2));
				EmitDword(Code, Words[0]);
				EmitDword(Code, Words[1]);
				break;
			case asBCTYPE_wW_QW_ARG:
			{
				reinterpret_cast<short*>(Words)[1] = static_cast<short>(Imm(0));
				asQWORD Immediate = static_cast<asQWORD>(Imm(1));
				asDWORD ImmediateWords[2] = {};
				FMemory::Memcpy(ImmediateWords, &Immediate, sizeof(Immediate));
				EmitDword(Code, Words[0]);
				EmitDword(Code, ImmediateWords[0]);
				EmitDword(Code, ImmediateWords[1]);
				break;
			}
			case asBCTYPE_wW_rW_ARG:
			case asBCTYPE_rW_rW_ARG:
			case asBCTYPE_W_rW_ARG:
			case asBCTYPE_wW_W_ARG:
				reinterpret_cast<short*>(Words)[1] = static_cast<short>(Imm(0));
				reinterpret_cast<short*>(Words)[2] = static_cast<short>(Imm(1));
				EmitDword(Code, Words[0]);
				EmitDword(Code, Words[1]);
				break;
			case asBCTYPE_wW_rW_DW_ARG:
			case asBCTYPE_rW_W_DW_ARG:
				reinterpret_cast<short*>(Words)[1] = static_cast<short>(Imm(0));
				reinterpret_cast<short*>(Words)[2] = static_cast<short>(Imm(1));
				EmitDword(Code, Words[0]);
				EmitDword(Code, Words[1]);
				EmitDword(Code, static_cast<asDWORD>(Imm(2)));
				break;
			case asBCTYPE_rW_DW_DW_ARG:
				reinterpret_cast<short*>(Words)[1] = static_cast<short>(Imm(0));
				EmitDword(Code, Words[0]);
				EmitDword(Code, static_cast<asDWORD>(Imm(1)));
				EmitDword(Code, static_cast<asDWORD>(Imm(2)));
				break;
			default:
				EmitDword(Code, Words[0]);
				break;
			}
			break;
		}
		}
		return asEByteCodeLinkStatus::Succeeded;
	}

	asEByteCodeLinkStatus LowerFunction(
		const asSByteCodeFunctionBody& Body,
		asCExecutableFunction& Executable,
		asCScriptEngine& Engine,
		const asCByteCodeImage& Image,
		const asSByteCodeFunctionLifetime& Lifetime)
	{
		Executable.AllocateScriptData();
		Executable.scriptData->byteCode.SetLength(0);
		const asUINT LocalDwords = FMath::Max(Body.FrameSize, 2u);
		const uint64 StackDwords = static_cast<uint64>(LocalDwords) + Lifetime.TemporaryStackDwords;
		if (StackDwords > static_cast<uint64>(MAX_int32) - 64u) return asEByteCodeLinkStatus::InvalidArgument;
		Executable.scriptData->stackNeeded = FMath::Max(static_cast<asUINT>(StackDwords), 16u);
		Executable.scriptData->variableSpace = LocalDwords;
		Executable.scriptData->objVariablesOnHeap = 0;
		Executable.scriptData->objVariablePos.SetLength(0);
		Executable.scriptData->objVariableTypes.SetLength(0);
		Executable.scriptData->objVariableInfo.SetLength(0);
		if (!Executable.CallFrame.Prepare(Executable.Declaration)) return asEByteCodeLinkStatus::InvalidArgument;
		TArray<asSByteCodeInstruction> Instructions = Body.Instructions;
		if (Instructions.Num() == 2
			&& Instructions[0].Opcode == asBC_PshC4
			&& Instructions.Last().Opcode == asBC_RET)
		{
			asSByteCodeInstruction Set;
			Set.Opcode = asBC_SetV4;
			asSByteCodeOperand Offset;
			Offset.Kind = asEByteCodeOperandKind::Local;
			Offset.Immediate = 1;
			asSByteCodeOperand Value = Instructions[0].Operands.Num() ? Instructions[0].Operands[0] : asSByteCodeOperand{};
			Set.Operands.Add(Offset);
			Set.Operands.Add(Value);
			asSByteCodeInstruction Copy;
			Copy.Opcode = asBC_CpyVtoR4;
			Copy.Operands.Add(Offset);
			asSByteCodeInstruction Ret = Instructions.Last();
			Instructions = {Set, Copy, Ret};
		}
		TArray<TArray<asDWORD>> Parts;
		Parts.Reserve(Instructions.Num());
		for (const asSByteCodeInstruction& Instruction : Instructions)
		{
			TArray<asDWORD> Part;
			const asEByteCodeLinkStatus Status = LowerInstruction(Instruction, Part, Engine, Image);
			if (Status != asEByteCodeLinkStatus::Succeeded) return Status;
			Parts.Add(MoveTemp(Part));
		}
		TArray<int32> Offsets;
		int32 Cursor = 0;
		for (const TArray<asDWORD>& Part : Parts)
		{
			Offsets.Add(Cursor);
			Cursor += Part.Num();
		}
		for (int32 Index = 0; Index < Instructions.Num(); ++Index)
		{
			const asEBCInstr Opcode = Instructions[Index].Opcode;
			if (Opcode == asBC_JMPP)
			{
				const int32 Maximum = static_cast<int32>(Instructions[Index].Operands[1].Immediate);
				for (int32 Entry = 0; Entry <= Maximum; ++Entry)
					if (Parts[Index + 1 + Entry].Num() != 2) return asEByteCodeLinkStatus::InvalidArgument;
				Executable.IndexedJumpBounds.Add(static_cast<asUINT>(Offsets[Index]), Maximum);
			}
			if (Opcode == asBC_CallPtr)
			{
				asCScriptFunction* Expected = ResolveFunctionSlot(Engine, Image, Instructions[Index].Operands[1].Slot);
				if (!Expected) return asEByteCodeLinkStatus::MissingDeclaration;
				Executable.IndirectCallSignatures.Add(static_cast<asUINT>(Offsets[Index]), Expected);
			}
			if ((Opcode == asBC_JMP || Opcode == asBC_JZ || Opcode == asBC_JNZ
				|| Opcode == asBC_JS || Opcode == asBC_JNS || Opcode == asBC_JP || Opcode == asBC_JNP
				|| Opcode == asBC_JLowZ || Opcode == asBC_JLowNZ) && Parts[Index].Num() >= 2)
			{
				const int32 Target = Instructions[Index].Operands.Num() > 0
					? static_cast<int32>(Instructions[Index].Operands[0].Immediate)
					: 0;
				if (Target >= 0 && Target < Offsets.Num())
				{
					Parts[Index][1] = static_cast<asDWORD>(Offsets[Target] - (Offsets[Index] + 2));
				}
			}
		}
		for (const TArray<asDWORD>& Part : Parts)
		{
			for (asDWORD Word : Part)
			{
				Executable.scriptData->byteCode.PushLast(Word);
			}
		}
		for (int32 Slot = 0; Slot < Lifetime.FrameOffsets.Num(); ++Slot)
		{
			asCObjectType* Type = CastToObjectType(Engine.FindMetadataType(Image.Requirements[Lifetime.TypeSlots[Slot]].Key));
			if (!Type) return asEByteCodeLinkStatus::SchemaMismatch;
			Executable.scriptData->objVariablePos.PushLast(Lifetime.FrameOffsets[Slot]);
			Executable.scriptData->objVariableTypes.PushLast(Type);
		}
		Executable.scriptData->objVariablesOnHeap = static_cast<asUINT>(Lifetime.FrameOffsets.Num());
		// The verifier proves one compatible incoming lifetime state at every
		// reachable PC. Encode changes in physical instruction order: a source
		// marker's position alone cannot describe branches or reconstruction.
		TArray<bool> Previous;
		Previous.Init(false, Lifetime.FrameOffsets.Num());
		if (!Lifetime.FrameOffsets.IsEmpty())
		{
			for (int32 Index = 0; Index < Lifetime.Before.Num(); ++Index)
			{
				if (!Lifetime.Reachable[Index]) continue;
				for (int32 Slot = 0; Slot < Lifetime.FrameOffsets.Num(); ++Slot)
				{
					const bool bConstructed = Lifetime.Before[Index][Slot] == asEByteCodeObjectState::Constructed;
					if (Previous[Slot] == bConstructed) continue;
					asSObjectVariableInfo Info;
					Info.programPos = static_cast<asUINT>(Offsets[Index]);
					Info.variableOffset = Lifetime.FrameOffsets[Slot];
					Info.option = bConstructed ? asOBJ_INIT : asOBJ_UNINIT;
					Executable.scriptData->objVariableInfo.PushLast(Info);
					Previous[Slot] = bConstructed;
				}
			}
		}

		if (Body.Source.Num() > 0)
		{
			if (Body.Source[0].FileStringSlot < static_cast<asUINT>(Image.Strings.Num()))
			{
				Executable.scriptData->scriptSectionIdx = Engine.GetScriptSectionNameIndex(
					TCHAR_TO_UTF8(*Image.Strings[Body.Source[0].FileStringSlot]));
			}
			for (const asSByteCodeSourceRecord& Record : Body.Source)
			{
				const int32 ProgramPos = Record.InstructionIndex < static_cast<asUINT>(Offsets.Num())
					? Offsets[Record.InstructionIndex]
					: Cursor;
				const int Packed = (Record.Line & 0xFFFFF) | ((Record.Column & 0xFFF) << 20);
				Executable.scriptData->lineNumbers.PushLast(ProgramPos);
				Executable.scriptData->lineNumbers.PushLast(Packed);
			}
		}
		return asEByteCodeLinkStatus::Succeeded;
	}

	bool InternLiveTypeUse(const asCMetadataImage* Image, const asCDataType& Type, asSStableKey& OutKey)
	{
		OutKey = {};
		if (!Image || !Image->GetIdentityContext().IsValid()) return false;
		asCTypeContext* Context = const_cast<asCTypeContext*>(Image->GetIdentityContext().Get());
		asSStableTypeUseDescriptor Descriptor;
		if (Type.IsReadOnly() || Type.IsHandleToConst())
		{
			Descriptor.Qualifiers = Descriptor.Qualifiers | asETypeUseQualifiers::Const;
		}
		if (Type.IsReference())
		{
			Descriptor.Qualifiers = Descriptor.Qualifiers | asETypeUseQualifiers::Reference;
		}
		if (Type.IsObjectHandle())
		{
			Descriptor.Qualifiers = Descriptor.Qualifiers | asETypeUseQualifiers::Handle;
		}
		if (asCTypeInfo* Info = Type.GetTypeInfo())
		{
			Descriptor.Kind = asETypeUseKind::Nominal;
			Descriptor.Declaration = Info->GetStableKey();
		}
		else
		{
			Descriptor.Kind = asETypeUseKind::Primitive;
			switch (Type.GetTokenType())
			{
			case ttVoid: Descriptor.Primitive = asEPrimitiveType::Void; break;
			case ttBool: Descriptor.Primitive = asEPrimitiveType::Bool; break;
			case ttInt8: Descriptor.Primitive = asEPrimitiveType::Int8; break;
			case ttInt16: Descriptor.Primitive = asEPrimitiveType::Int16; break;
			case ttInt: Descriptor.Primitive = asEPrimitiveType::Int32; break;
			case ttInt64: Descriptor.Primitive = asEPrimitiveType::Int64; break;
			case ttUInt8: Descriptor.Primitive = asEPrimitiveType::UInt8; break;
			case ttUInt16: Descriptor.Primitive = asEPrimitiveType::UInt16; break;
			case ttUInt: Descriptor.Primitive = asEPrimitiveType::UInt32; break;
			case ttUInt64: Descriptor.Primitive = asEPrimitiveType::UInt64; break;
			case ttFloat32: Descriptor.Primitive = asEPrimitiveType::Float32; break;
			case ttFloat64: Descriptor.Primitive = asEPrimitiveType::Float64; break;
			case ttDouble: Descriptor.Primitive = asEPrimitiveType::Float64; break;
			case ttFloat: Descriptor.Primitive = asEPrimitiveType::Float32; break;
			default: return false;
			}
		}
		return Context->GetIdentityRegistry().InternTypeUse(Descriptor, OutKey) == asETypeIdentityStatus::Succeeded;
	}

	asEByteCodeLinkStatus CheckLiveImage(asCScriptEngine& Engine, const asCMetadataImage* Image)
	{
		if (!Image) return asEByteCodeLinkStatus::MissingDeclaration;
		const asEMetadataImageState State = Image->GetState();
		if (State == asEMetadataImageState::Retired) return asEByteCodeLinkStatus::Retired;
		if (State == asEMetadataImageState::Attaching || State == asEMetadataImageState::Building)
		{
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		if (asCScriptEngine* Bound = Image->GetBoundEngine())
		{
			if (Bound != &Engine) return asEByteCodeLinkStatus::InvalidArgument;
		}
		return asEByteCodeLinkStatus::Succeeded;
	}

	asEByteCodeLinkStatus CompareTypeRequirement(
		asCScriptEngine& Engine,
		const asSByteCodeRequirement& Requirement,
		asCTypeInfo& Type)
	{
		const asEByteCodeLinkStatus Attachment = CheckLiveImage(Engine, Type.GetMetadataImage());
		if (Attachment != asEByteCodeLinkStatus::Succeeded) return Attachment;
		asSSchemaHash Schema;
		asSLayoutHash Layout;
		TConstArrayView<uint8> SchemaWitness;
		TConstArrayView<uint8> LayoutWitness;
		if (Type.TryGetSchemaHash(Schema, &SchemaWitness) != asEFingerprintStatus::Succeeded)
		{
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		if (Type.TryGetLayoutHash(Layout, &LayoutWitness) != asEFingerprintStatus::Succeeded)
		{
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		if (CompareSchemaFingerprints(Requirement.Schema, Requirement.SchemaWitness, Schema, SchemaWitness)
			!= asEFingerprintEquality::Equal)
		{
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		if (CompareLayoutFingerprints(Requirement.Layout, Requirement.LayoutWitness, Layout, LayoutWitness)
			!= asEFingerprintEquality::Equal)
		{
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		return asEByteCodeLinkStatus::Succeeded;
	}

	asEByteCodeLinkStatus CompareFunctionRequirement(
		asCScriptEngine& Engine,
		const asSByteCodeRequirement& Requirement,
		asCScriptFunction& Function)
	{
		const asEByteCodeLinkStatus Attachment = CheckLiveImage(Engine, Function.GetMetadataImage());
		if (Attachment != asEByteCodeLinkStatus::Succeeded) return Attachment;
		asSStableKey ReturnTypeUse;
		if (!InternLiveTypeUse(Function.GetMetadataImage(), Function.GetReturnDataType(), ReturnTypeUse))
		{
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		if (Requirement.ReturnTypeUse != ReturnTypeUse
			|| Requirement.ReturnDwords != static_cast<asUINT>(Function.GetReturnDataType().GetSizeOnStackDWords())
			|| Requirement.ReturnStorageKind != asByteCodeStorageKind(Function.GetReturnDataType()))
		{
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		if (Requirement.Parameters.Num() != static_cast<int32>(Function.GetParamCount()))
		{
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		asCCanonicalEncoding Signature(asECanonicalIdentityDomain::Function);
		Signature.WriteBytes(ReturnTypeUse.GetBytes());
		Signature.WriteUInt32(static_cast<uint32>(Function.GetParamCount()));
		for (asUINT Index = 0; Index < Function.GetParamCount(); ++Index)
		{
			const asCDataType* ParameterType = Function.GetParameterDataType(Index);
			if (!ParameterType) return asEByteCodeLinkStatus::SchemaMismatch;
			asSStableKey TypeUse;
			if (!InternLiveTypeUse(Function.GetMetadataImage(), *ParameterType, TypeUse))
			{
				return asEByteCodeLinkStatus::SchemaMismatch;
			}
			uint8 PassingMode = 0;
			if (Index < Function.inOutFlags.GetLength())
			{
				PassingMode = static_cast<uint8>(Function.inOutFlags[Index]);
			}
			if (Requirement.Parameters[Index].TypeUse != TypeUse
				|| Requirement.Parameters[Index].PassingMode != PassingMode
				|| Requirement.Parameters[Index].StorageDwords != static_cast<asUINT>(ParameterType->GetSizeOnStackDWords())
				|| Requirement.Parameters[Index].StorageKind != asByteCodeStorageKind(*ParameterType))
			{
				return asEByteCodeLinkStatus::SchemaMismatch;
			}
			Signature.WriteBytes(TypeUse.GetBytes());
			Signature.WriteUInt8(PassingMode);
		}
		const asSStableKey SignatureKey = Signature.FinalizeDigest();
		if (Requirement.SignatureKey != SignatureKey)
		{
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		const uint32 CallableTraits = (Function.IsMetadataStatic() ? 1u : 0u)
			| (Function.IsFinal() ? 2u : 0u)
			| (Function.IsOverride() ? 4u : 0u)
			| (Function.IsProperty() ? 8u : 0u)
			| (Function.IsReadOnly() ? 16u : 0u)
		| (Function.GetMetadataDeclarationKind() == asEFunctionIdentityKind::Constructor ? 32u : 0u)
		| (Function.DoesReturnOnStack() ? 64u : 0u);
		if (Requirement.CallableTraits != CallableTraits)
		{
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		asSStableKey OwnerKey;
		if (Function.GetObjectType())
		{
			OwnerKey = static_cast<asCTypeInfo*>(Function.GetObjectType())->GetStableKey();
		}
		if (!Requirement.OwnerKey.IsZero() && Requirement.OwnerKey != OwnerKey)
		{
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		return asEByteCodeLinkStatus::Succeeded;
	}

	asCObjectProperty* FindPropertyByName(asCObjectType& Owner, const FString& Name)
	{
		auto Matches = [&Name](asCObjectProperty* Property) -> bool
		{
			return Property && UTF8_TO_TCHAR(Property->name.AddressOf()) == Name;
		};
		for (asUINT Index = 0; Index < Owner.localProperties.GetLength(); ++Index)
		{
			if (Matches(Owner.localProperties[Index])) return Owner.localProperties[Index];
		}
		for (asUINT Index = 0; Index < Owner.properties.GetLength(); ++Index)
		{
			if (Matches(Owner.properties[Index])) return Owner.properties[Index];
		}
		return nullptr;
	}

	asEByteCodeLinkStatus ComparePropertyRequirement(
		asCScriptEngine& Engine,
		const asSByteCodeRequirement& Requirement)
	{
		asCTypeInfo* OwnerInfo = Engine.FindMetadataType(Requirement.OwnerKey);
		if (!OwnerInfo) return asEByteCodeLinkStatus::MissingDeclaration;
		const asEByteCodeLinkStatus Attachment = CheckLiveImage(Engine, OwnerInfo->GetMetadataImage());
		if (Attachment != asEByteCodeLinkStatus::Succeeded) return Attachment;
		asCObjectType* Owner = CastToObjectType(OwnerInfo);
		if (!Owner) return asEByteCodeLinkStatus::MissingDeclaration;
		asCObjectProperty* Property = FindPropertyByName(*Owner, Requirement.Name);
		if (!Property) return asEByteCodeLinkStatus::MissingDeclaration;
		if (Property->byteOffset != Requirement.NativeOffset
			|| static_cast<uint8>(Property->nativeAddressMode) != Requirement.NativeAddressMode)
		{
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		return asEByteCodeLinkStatus::Succeeded;
	}
}

asEByteCodeLinkStatus asLinkByteCodeImage(
	asCScriptEngine& Engine,
	const asCByteCodeImage& Image,
	asCExecutableSnapshot*& OutSnapshot)
{
	OutSnapshot = nullptr;
	if (Image.GetPointerByteSize() != sizeof(void*)
		|| Image.GetFormatVersion() != 3
		|| Image.GetOpcodeAbi() != asBC_MAXBYTECODE)
	{
		return asEByteCodeLinkStatus::InvalidArgument;
	}
	asQWORD Generation = 0;
	const asEByteCodeLinkStatus Admission = Engine.BeginExecutablePublication(Generation);
	if (Admission != asEByteCodeLinkStatus::Succeeded) return Admission;
	TArray<asSByteCodeFunctionLifetime> Lifetimes;
	if (asCByteCodeVerifier::VerifyWithLifetimes(Image, Lifetimes).Status != asEByteCodeVerifyStatus::Succeeded)
	{
		return asEByteCodeLinkStatus::Unverified;
	}
	asCExecutableSnapshot* Snapshot = asNEW(asCExecutableSnapshot)();
	if (!Snapshot) return asEByteCodeLinkStatus::ResourceFailure;
	Snapshot->AddRef();
	Snapshot->Engine = &Engine;
	Snapshot->Image = MakeUnique<asCByteCodeImage>(Image);
	for (const asSByteCodeRequirement& Requirement : Image.Requirements)
	{
		if (Requirement.Role == asEByteCodeRequirementRole::Type)
		{
			asCTypeInfo* Type = Engine.FindMetadataType(Requirement.Key);
			if (!Type)
			{
				Snapshot->Release();
				return asEByteCodeLinkStatus::MissingDeclaration;
			}
			const asEByteCodeLinkStatus Compared = CompareTypeRequirement(Engine, Requirement, *Type);
			if (Compared != asEByteCodeLinkStatus::Succeeded)
			{
				Snapshot->Release();
				return Compared;
			}
			Type->AddRef();
			Snapshot->Types.Add(Requirement.Key, Type);
		}
		else if (Requirement.Role == asEByteCodeRequirementRole::Function)
		{
			asCScriptFunction* Function = Engine.FindMetadataFunction(Requirement.Key);
			if (!Function)
			{
				Snapshot->Release();
				return asEByteCodeLinkStatus::MissingDeclaration;
			}
			const asEByteCodeLinkStatus Compared = CompareFunctionRequirement(Engine, Requirement, *Function);
			if (Compared != asEByteCodeLinkStatus::Succeeded)
			{
				Snapshot->Release();
				return Compared;
			}
			Function->AddRef();
			Snapshot->Functions.Add(Requirement.Key, Function);
		}
		else if (Requirement.Role == asEByteCodeRequirementRole::Property)
		{
			const asEByteCodeLinkStatus Compared = ComparePropertyRequirement(Engine, Requirement);
			if (Compared != asEByteCodeLinkStatus::Succeeded)
			{
				Snapshot->Release();
				return Compared;
			}
		}
		else if (Requirement.Role == asEByteCodeRequirementRole::Global)
		{
			if (!Engine.FindMetadataGlobalProperty(Requirement.Key))
			{
				Snapshot->Release();
				return asEByteCodeLinkStatus::MissingDeclaration;
			}
			if (!Engine.FindBoundGlobalStorage(Requirement.Key))
			{
				Snapshot->Release();
				return asEByteCodeLinkStatus::MissingBinding;
			}
		}
	}
	for (const asSByteCodeFunctionBody& Body : Image.Functions)
	{
		asCScriptFunction* Function = Engine.FindMetadataFunction(Body.FunctionKey);
		if (!Function)
		{
			if (asCScriptFunction** Found = Snapshot->Functions.Find(Body.FunctionKey)) Function = *Found;
		}
		if (!Function)
		{
			Snapshot->Release();
			return asEByteCodeLinkStatus::MissingDeclaration;
		}
		int64 ArgumentDwords = Function->objectType && !Function->IsMetadataStatic() ? AS_PTR_SIZE : 0;
		if (Function->DoesReturnOnStack()) ArgumentDwords += AS_PTR_SIZE;
		for (asUINT Index = 0; Index < Function->parameterTypes.GetLength(); ++Index)
			ArgumentDwords += Function->parameterTypes[Index].GetSizeOnStackDWords();
		if (ArgumentDwords < 0 || ArgumentDwords > 32767 || Body.ParameterSize != ArgumentDwords)
		{
			Snapshot->Release();
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		if (Body.Instructions.Num() == 2 && Body.Instructions[0].Opcode == asBC_PshC4
			&& Body.Instructions.Last().Opcode == asBC_RET
			&& (Function->DoesReturnOnStack() || Function->returnType.GetSizeOnStackDWords() != 1
				|| asByteCodeStorageKind(Function->returnType) != asEByteCodeStorageKind::Value))
		{
			Snapshot->Release();
			return asEByteCodeLinkStatus::SchemaMismatch;
		}
		if (asCExecutableFunction* Existing = Engine.FindPublishedExecutable(Body.FunctionKey))
		{
			if (Existing->scriptData && Existing->scriptData->byteCode.GetLength() > 0)
			{
				Snapshot->Release();
				return asEByteCodeLinkStatus::BodyConflict;
			}
		}
		TUniquePtr<asCExecutableFunction> Executable = MakeUnique<asCExecutableFunction>();
		Function->AddRef();
		Executable->Declaration = Function;
		Executable->Key = Body.FunctionKey;
		Executable->Owner = Snapshot;
		if (auto Bound = Engine.AcquireSystemInterface(Function))
		{
			Executable->sysFuncIntf = asNEW(asSSystemFunctionInterface)(*Bound);
		}
		const asSByteCodeFunctionLifetime* Lifetime = Lifetimes.FindByPredicate([&](const asSByteCodeFunctionLifetime& Candidate)
		{
			return Candidate.FunctionKey == Body.FunctionKey;
		});
		const asEByteCodeLinkStatus Lowered = Lifetime
			? LowerFunction(Body, *Executable, Engine, Image, *Lifetime) : asEByteCodeLinkStatus::Unverified;
		if (Lowered != asEByteCodeLinkStatus::Succeeded)
		{
			Snapshot->Release();
			return Lowered;
		}
		if (!Snapshot->Functions.Contains(Body.FunctionKey))
		{
			Function->AddRef();
			Snapshot->Functions.Add(Body.FunctionKey, Function);
		}
		Snapshot->Executables.Add(Body.FunctionKey, MoveTemp(Executable));
	}
	const asEByteCodeLinkStatus Committed = Engine.CommitExecutablePublication(*Snapshot, Generation);
	if (Committed != asEByteCodeLinkStatus::Succeeded)
	{
		Snapshot->Release();
		return Committed;
	}
	OutSnapshot = Snapshot;
	return asEByteCodeLinkStatus::Succeeded;
}

asEByteCodeLinkStatus asCScriptEngine::LinkByteCodeImage(const asCByteCodeImage& Image, asCExecutableSnapshot*& OutSnapshot)
{
	if (shuttingDown || vmRetirementRequested || metadataRegistrationRetired) return asEByteCodeLinkStatus::Retired;
	return asLinkByteCodeImage(*this, Image, OutSnapshot);
}

asEByteCodeLinkStatus asCScriptEngine::BindNativeFunction(const asSStableKey& FunctionKey, const asSFuncPtr& Function, asDWORD CallConv, asFunctionCaller Caller, void* Auxiliary, int BaseOffset)
{
 asQWORD Generation = 0;
 const auto Admission = BeginExecutablePublication(Generation);
 if (Admission != asEByteCodeLinkStatus::Succeeded) return Admission;
 if (FunctionKey.IsZero() || Function.ptr.f.func == nullptr) return asEByteCodeLinkStatus::InvalidArgument;
 std::shared_ptr<asCScriptFunction> Declaration;
 {
  std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
  if (vmRetirementRequested || metadataRegistrationRetired) return asEByteCodeLinkStatus::Retired;
  asCScriptFunction* Target = metadataFunctionsByKey.FindRef(FunctionKey);
  if (!Target) return asEByteCodeLinkStatus::MissingDeclaration;
  if (Target->funcType != asFUNC_SYSTEM) return asEByteCodeLinkStatus::InvalidArgument;
  Target->AddRef();
  Declaration = std::shared_ptr<asCScriptFunction>(Target, [](asCScriptFunction* Value) { Value->Release(); });
 }
 asCScriptFunction* Target = Declaration.get();
 asSSystemFunctionInterface Internal;
 const int Detected = DetectCallingConvention(Target->objectType != nullptr, Function, static_cast<int>(CallConv), Auxiliary, Caller, &Internal);
 if (Detected < 0) return asEByteCodeLinkStatus::InvalidArgument;
 if (BaseOffset != 0) Internal.baseOffset = BaseOffset;
 if (!Caller.IsBound() && Internal.callConv != ICC_GENERIC_FUNC && Internal.callConv != ICC_GENERIC_METHOD)
  return asEByteCodeLinkStatus::InvalidArgument;
 if (!Internal.CallFrame.Prepare(Target)) return asEByteCodeLinkStatus::InvalidArgument;
 Internal.paramSize = Internal.CallFrame.ArgumentDwords;
 asSSystemFunctionInterface* Storage = asNEW(asSSystemFunctionInterface)(Internal);
 if (!Storage) return asEByteCodeLinkStatus::ResourceFailure;
 std::shared_ptr<asSSystemFunctionInterface> Candidate(Storage, [Declaration](asSSystemFunctionInterface* Value)
 {
  asDELETE(Value, asSSystemFunctionInterface);
 });
 if ((Internal.callConv == ICC_GENERIC_FUNC || Internal.callConv == ICC_GENERIC_METHOD)
  && PrepareSystemFunctionGeneric(Target, Storage, this) != 0) return asEByteCodeLinkStatus::InvalidArgument;
 std::shared_ptr<asSSystemFunctionInterface> Previous;
 {
  std::lock_guard<std::mutex> Lock(executablePublicationMutex);
  if (shuttingDown || vmRetirementRequested || metadataRegistrationRetired || executablePublicationGeneration != Generation)
   return asEByteCodeLinkStatus::Retired;
  Previous = nativeInterfaces.FindRef(FunctionKey);
  nativeInterfaces.Add(FunctionKey, Candidate);
 }
 // Release an unobserved prior generation outside publication/metadata locks.
 return asEByteCodeLinkStatus::Succeeded;
}

asEByteCodeLinkStatus asCScriptEngine::BindNativeFunction(const asSStableKey& FunctionKey, void* Native)
{
	if (!Native) return asEByteCodeLinkStatus::InvalidArgument;
	asSFuncPtr Pointer(1);
	union
	{
		asFUNCTION_t Function;
		void* Object;
	} Convert;
	Convert.Object = Native;
	Pointer.ptr.f.func = Convert.Function;
	return BindNativeFunction(FunctionKey, Pointer, asCALL_GENERIC);
}

asEByteCodeLinkStatus asCScriptEngine::BindGlobalStorage(const asSStableKey& DeclarationKey, void* Storage)
{
	if (DeclarationKey.IsZero() || !Storage) return asEByteCodeLinkStatus::InvalidArgument;
	std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
	if (shuttingDown || vmRetirementRequested || metadataRegistrationRetired) return asEByteCodeLinkStatus::Retired;
	if (!metadataGlobalsByKey.Contains(DeclarationKey)) return asEByteCodeLinkStatus::MissingDeclaration;
	metadataGlobalBindings.Add(DeclarationKey, Storage);
	return asEByteCodeLinkStatus::Succeeded;
}

void* asCScriptEngine::FindBoundGlobalStorage(const asSStableKey& DeclarationKey) const
{
	std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
	if (void* const* Found = metadataGlobalBindings.Find(DeclarationKey)) return *Found;
	return nullptr;
}

asCExecutableFunction* asCScriptEngine::FindPublishedExecutable(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(executablePublicationMutex);
	if (asCExecutableFunction* const* Found = publishedExecutables.Find(Key)) return *Found;
	return nullptr;
}

asCExecutableFunction* asCScriptEngine::AcquirePublishedExecutable(const asSStableKey& Key, asCExecutableSnapshot*& OutSnapshot)
{
	OutSnapshot = nullptr;
	std::lock_guard<std::mutex> Lock(executablePublicationMutex);
	asCExecutableFunction* const* Found = publishedExecutables.Find(Key);
	if (!Found || !*Found) return nullptr;
	asCExecutableFunction* Executable = *Found;
	if (Executable->Owner)
	{
		Executable->Owner->AddRef();
		OutSnapshot = Executable->Owner;
	}
	return Executable;
}

void asCScriptEngine::DetachPublishedSnapshot(asCExecutableSnapshot* Snapshot)
{
	if (!Snapshot) return;
	std::lock_guard<std::mutex> Lock(executablePublicationMutex);
	publishedSnapshotLeases.Remove(Snapshot);
	for (auto It = publishedExecutables.CreateIterator(); It; )
	{
		if (It.Value() && It.Value()->Owner == Snapshot)
		{
			It.RemoveCurrent();
		}
		else
		{
			++It;
		}
	}
}

std::shared_ptr<asSSystemFunctionInterface> asCScriptEngine::AcquireSystemInterface(asCScriptFunction* Function) const
{
 if (!Function) return {};
 std::lock_guard<std::mutex> Lock(executablePublicationMutex);
 if (const auto* Published = publishedExecutables.Find(Function->GetStableKey()))
 {
  auto* Executable = *Published;
  if (Executable && Executable->sysFuncIntf && Executable->Owner)
  {
   auto* Snapshot = Executable->Owner;
   Snapshot->AddRef();
   return std::shared_ptr<asSSystemFunctionInterface>(Executable->sysFuncIntf,
    [Snapshot](asSSystemFunctionInterface*) { Snapshot->Release(); });
  }
 }
 if (const auto* Bound = nativeInterfaces.Find(Function->GetStableKey())) return *Bound;
 return {};
}

asEByteCodeLinkStatus asCScriptEngine::BeginExecutablePublication(asQWORD& OutGeneration)
{
	std::lock_guard<std::mutex> Lock(executablePublicationMutex);
	if (shuttingDown || vmRetirementRequested || metadataRegistrationRetired) return asEByteCodeLinkStatus::Retired;
	OutGeneration = executablePublicationGeneration;
	return asEByteCodeLinkStatus::Succeeded;
}

asEByteCodeLinkStatus asCScriptEngine::CommitExecutablePublication(asCExecutableSnapshot& Snapshot, asQWORD Generation)
{
	std::lock_guard<std::mutex> Lock(executablePublicationMutex);
	if (shuttingDown || vmRetirementRequested || metadataRegistrationRetired) return asEByteCodeLinkStatus::Retired;
	if (executablePublicationGeneration != Generation) return asEByteCodeLinkStatus::Retired;
	for (auto& Pair : Snapshot.Executables)
	{
		if (asCExecutableFunction* const* Existing = publishedExecutables.Find(Pair.Key))
		{
			if (*Existing && (*Existing)->scriptData && (*Existing)->scriptData->byteCode.GetLength() > 0)
			{
				return asEByteCodeLinkStatus::BodyConflict;
			}
		}
	}
	for (auto& Pair : Snapshot.Executables)
	{
		if (Pair.Key.IsZero() || !Pair.Value.Get()) continue;
		publishedExecutables.Add(Pair.Key, Pair.Value.Get());
	}
	if (!publishedSnapshotLeases.Contains(&Snapshot))
	{
		publishedSnapshotLeases.Add(&Snapshot);
		Snapshot.AddRef();
	}
	return asEByteCodeLinkStatus::Succeeded;
}

void asCScriptEngine::PublishExecutable(const asSStableKey& Key, asCExecutableFunction* Executable)
{
	if (Key.IsZero() || !Executable) return;
	std::lock_guard<std::mutex> Lock(executablePublicationMutex);
	publishedExecutables.Add(Key, Executable);
}

void asCScriptEngine::UnpublishExecutable(const asSStableKey& Key, asCExecutableFunction* Executable)
{
	std::lock_guard<std::mutex> Lock(executablePublicationMutex);
	if (asCExecutableFunction* const* Found = publishedExecutables.Find(Key))
	{
		if (*Found == Executable) publishedExecutables.Remove(Key);
	}
}

END_AS_NAMESPACE
