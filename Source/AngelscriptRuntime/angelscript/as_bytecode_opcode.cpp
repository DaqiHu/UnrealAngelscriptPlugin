#include "as_bytecode_opcode.h"

BEGIN_AS_NAMESPACE

bool asByteCodeOpcodeIsExecutable(asEBCInstr Opcode)
{
	const asUINT Value = static_cast<asUINT>(Opcode);
	if (Value == asBC_STR) return false;
	if (Value > asBC_MAXBYTECODE) return false;
	return true;
}

namespace
{
	uint8 LocalStorageWidth(asEBCInstr Opcode, asUINT OperandIndex)
	{
		switch (Opcode)
		{
		case asBC_dTOi: case asBC_dTOu: case asBC_dTOf:
		case asBC_i64TOi: case asBC_i64TOf: case asBC_u64TOf:
			return OperandIndex == 0 ? 4 : 8;
		case asBC_iTOd: case asBC_uTOd: case asBC_fTOd:
		case asBC_uTOi64: case asBC_iTOi64: case asBC_fTOi64: case asBC_fTOu64:
			return OperandIndex == 0 ? 8 : 4;
		case asBC_BSLL64: case asBC_BSRL64: case asBC_BSRA64: case asBC_POWdi:
			return OperandIndex == 2 ? 4 : 8;
		case asBC_PshVPtr: case asBC_ClrVPtr: case asBC_CallPtr: case asBC_AddPtr:
		case asBC_RemovePtr: case asBC_ClearPtr: case asBC_ClonePtr:
		case asBC_LOADOBJ: case asBC_STOREOBJ: case asBC_FREE:
		case asBC_GETOBJ: case asBC_GETOBJREF: case asBC_CmpPtr: case asBC_ChkNullV:
		case asBC_LoadRObjR: case asBC_RefCpyV: case asBC_AllocMem:
		case asBC_SetListSize: case asBC_PshListElmnt: case asBC_SetListType:
		case asBC_TrackRef: case asBC_UntrackRef: case asBC_ValidateRef:
		case asBC_CmpPtrNull: case asBC_DestructScript: case asBC_FreeNullV8:
			return AS_PTR_SIZE * 4;
		default: break;
		}
		switch (Opcode)
		{
		case asBC_SetV1:
		case asBC_WRTV1:
		case asBC_RDR1:
		case asBC_CpyVtoR1:
			return 1;
		case asBC_SetV2:
		case asBC_WRTV2:
		case asBC_RDR2:
			return 2;
		case asBC_SetV8:
		case asBC_CpyVtoV8:
		case asBC_CpyVtoR8:
		case asBC_CpyRtoV8:
		case asBC_PshV8:
		case asBC_WRTV8:
		case asBC_RDR8:
		case asBC_NEGd:
		case asBC_INCd:
		case asBC_DECd:
		case asBC_CMPd:
		case asBC_dTOi:
		case asBC_dTOu:
		case asBC_dTOf:
		case asBC_iTOd:
		case asBC_uTOd:
		case asBC_fTOd:
		case asBC_ADDd:
		case asBC_SUBd:
		case asBC_MULd:
		case asBC_DIVd:
		case asBC_MODd:
		case asBC_dTOi64:
		case asBC_dTOu64:
		case asBC_i64TOf:
		case asBC_u64TOf:
		case asBC_i64TOd:
		case asBC_u64TOd:
		case asBC_NEGi64:
		case asBC_INCi64:
		case asBC_DECi64:
		case asBC_BNOT64:
		case asBC_ADDi64:
		case asBC_SUBi64:
		case asBC_MULi64:
		case asBC_DIVi64:
		case asBC_MODi64:
		case asBC_BAND64:
		case asBC_BOR64:
		case asBC_BXOR64:
		case asBC_BSLL64:
		case asBC_BSRL64:
		case asBC_BSRA64:
		case asBC_CMPi64:
		case asBC_CMPu64:
		case asBC_DIVu64:
		case asBC_MODu64:
		case asBC_POWd:
		case asBC_POWdi:
		case asBC_POWi64:
		case asBC_POWu64:
		case asBC_i64TOi:
		case asBC_uTOi64:
		case asBC_iTOi64:
		case asBC_fTOi64:
		case asBC_fTOu64:
		case asBC_FreeNullV8:
			return 8;
		default:
			return 4;
		}
	}

	void FillOpcodeContract(asSByteCodeOpcodeInfo& OutInfo)
	{
		OutInfo.Control = asEByteCodeControlKind::Fallthrough;
		const int StackInc = asBCInfo[OutInfo.Opcode].stackInc;
		if (OutInfo.Opcode == asBC_ALLOC)
		{
			OutInfo.CallEffect = 0;
			OutInfo.StackEffect = -static_cast<int16>(AS_PTR_SIZE);
		}
		else if (StackInc == 0xFFFF)
		{
			OutInfo.CallEffect = 0xFFFF;
			OutInfo.StackEffect = 0;
		}
		else
		{
			OutInfo.StackEffect = static_cast<int16>(StackInc);
			OutInfo.CallEffect = 0;
		}
		switch (OutInfo.Opcode)
		{
		case asBC_JMP:
			OutInfo.Control = asEByteCodeControlKind::Jump;
			break;
		case asBC_JZ:
		case asBC_JNZ:
		case asBC_JS:
		case asBC_JNS:
		case asBC_JP:
		case asBC_JNP:
		case asBC_JLowZ:
		case asBC_JLowNZ:
			OutInfo.Control = asEByteCodeControlKind::ConditionalJump;
			break;
		case asBC_JMPP:
			OutInfo.Control = asEByteCodeControlKind::IndexedJump;
			break;
		case asBC_CALL:
		case asBC_CALLSYS:
		case asBC_CALLINTF:
		case asBC_CALLBND:
		case asBC_CallPtr:
		case asBC_Thiscall1:
			OutInfo.Control = asEByteCodeControlKind::Call;
			break;
		case asBC_RET:
			OutInfo.Control = asEByteCodeControlKind::Return;
			break;
		case asBC_ThrowException:
			OutInfo.Control = asEByteCodeControlKind::Throw;
			break;
		default:
			break;
		}

		for (asUINT Index = 0; Index < OutInfo.OperandCount; ++Index)
		{
			asSByteCodeOpcodeOperandInfo& Operand = OutInfo.OperandInfo[Index];
			Operand.Kind = OutInfo.Operands[Index];
			switch (Operand.Kind)
			{
			case asEByteCodeOperandKind::ImmediateS16:
				Operand.SignedWidth = 2;
				break;
			case asEByteCodeOperandKind::ImmediateS32:
				Operand.SignedWidth = 4;
				break;
			case asEByteCodeOperandKind::ImmediateS64:
				Operand.SignedWidth = 8;
				break;
			case asEByteCodeOperandKind::Local:
				Operand.SignedWidth = 2;
				Operand.StorageWidth = LocalStorageWidth(OutInfo.Opcode, Index);
				break;
			case asEByteCodeOperandKind::Label:
				Operand.SignedWidth = 4;
				break;
			default:
				break;
			}
		}

		auto MarkWrite = [&](asUINT Index)
		{
			if (Index < OutInfo.OperandCount)
			{
				OutInfo.OperandInfo[Index].WriteWidth = OutInfo.OperandInfo[Index].StorageWidth;
			}
		};
		auto MarkRead = [&](asUINT Index)
		{
			if (Index < OutInfo.OperandCount)
			{
				OutInfo.OperandInfo[Index].ReadWidth = OutInfo.OperandInfo[Index].StorageWidth;
			}
		};
		switch (OutInfo.Type)
		{
		case asBCTYPE_wW_ARG:
			MarkWrite(0);
			break;
		case asBCTYPE_rW_ARG:
			MarkRead(0);
			break;
		case asBCTYPE_wW_rW_ARG:
			MarkWrite(0);
			MarkRead(1);
			break;
		case asBCTYPE_rW_rW_ARG:
			MarkRead(0);
			MarkRead(1);
			break;
		case asBCTYPE_wW_rW_rW_ARG:
			MarkWrite(0);
			MarkRead(1);
			MarkRead(2);
			break;
		case asBCTYPE_wW_DW_ARG:
		case asBCTYPE_wW_QW_ARG:
			MarkWrite(0);
			break;
		case asBCTYPE_rW_DW_ARG:
		case asBCTYPE_rW_QW_ARG:
			MarkRead(0);
			break;
		case asBCTYPE_wW_rW_DW_ARG:
			MarkWrite(0);
			MarkRead(1);
			break;
		case asBCTYPE_W_rW_ARG:
			MarkRead(1);
			break;
		case asBCTYPE_rW_W_DW_ARG:
			MarkRead(0);
			break;
		default:
			break;
		}
		if (OutInfo.Opcode == asBC_SetV4 || OutInfo.Opcode == asBC_SetV8
			|| OutInfo.Opcode == asBC_SetV1 || OutInfo.Opcode == asBC_SetV2)
		{
			MarkWrite(0);
		}
	}
}

bool asByteCodeOpcodeGetInfo(asEBCInstr Opcode, asSByteCodeOpcodeInfo& OutInfo)
{
	OutInfo = {};
	if (!asByteCodeOpcodeIsExecutable(Opcode))
	{
		return false;
	}
	OutInfo.Opcode = Opcode;
	OutInfo.Type = asBCInfo[Opcode].type;
	switch (OutInfo.Type)
	{
	case asBCTYPE_NO_ARG:
		OutInfo.OperandCount = 0;
		break;
	case asBCTYPE_W_ARG:
	case asBCTYPE_wW_ARG:
	case asBCTYPE_rW_ARG:
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = (Opcode == asBC_JMP || Opcode == asBC_JZ || Opcode == asBC_JNZ
			|| Opcode == asBC_JS || Opcode == asBC_JNS || Opcode == asBC_JP || Opcode == asBC_JNP)
			? asEByteCodeOperandKind::Label
			: asEByteCodeOperandKind::Local;
		if (OutInfo.Type == asBCTYPE_W_ARG && Opcode != asBC_JMP) OutInfo.Operands[0] = asEByteCodeOperandKind::ImmediateS16;
		break;
	case asBCTYPE_DW_ARG:
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = (Opcode == asBC_CALL || Opcode == asBC_CALLSYS || Opcode == asBC_CALLINTF || Opcode == asBC_CallPtr)
			? asEByteCodeOperandKind::FunctionSlot
			: asEByteCodeOperandKind::ImmediateS32;
		if (Opcode == asBC_ALLOC || Opcode == asBC_OBJTYPE || Opcode == asBC_TYPEID)
		{
			OutInfo.Operands[0] = asEByteCodeOperandKind::TypeSlot;
		}
		break;
	case asBCTYPE_wW_DW_ARG:
	case asBCTYPE_rW_DW_ARG:
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::ImmediateS32;
		if (Opcode == asBC_PshC4) { OutInfo.OperandCount = 1; OutInfo.Operands[0] = asEByteCodeOperandKind::ImmediateS32; }
		break;
	case asBCTYPE_QW_ARG:
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::ImmediateS64;
		break;
	case asBCTYPE_wW_rW_rW_ARG:
		OutInfo.OperandCount = 3;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[2] = asEByteCodeOperandKind::Local;
		break;
	case asBCTYPE_rW_rW_ARG:
	case asBCTYPE_wW_rW_ARG:
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::Local;
		break;
	case asBCTYPE_wW_QW_ARG:
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::ImmediateS64;
		break;
	case asBCTYPE_wW_rW_DW_ARG:
	case asBCTYPE_rW_W_DW_ARG:
		OutInfo.OperandCount = 3;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[2] = asEByteCodeOperandKind::ImmediateS32;
		break;
	case asBCTYPE_rW_DW_DW_ARG:
		OutInfo.OperandCount = 3;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::ImmediateS32;
		OutInfo.Operands[2] = asEByteCodeOperandKind::ImmediateS32;
		break;
	case asBCTYPE_W_rW_ARG:
	case asBCTYPE_wW_W_ARG:
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::Local;
		break;
	case asBCTYPE_DW_DW_ARG:
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::ImmediateS32;
		OutInfo.Operands[1] = asEByteCodeOperandKind::ImmediateS32;
		break;
	case asBCTYPE_W_DW_ARG:
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::ImmediateS16;
		OutInfo.Operands[1] = asEByteCodeOperandKind::ImmediateS32;
		break;
	default:
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::ImmediateS32;
		break;
	}
	if (Opcode == asBC_PshC4)
	{
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::ImmediateS32;
	}
	if (Opcode == asBC_RET)
	{
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::ImmediateS16;
	}
	if (Opcode == asBC_JMP || Opcode == asBC_JZ || Opcode == asBC_JNZ
		|| Opcode == asBC_JS || Opcode == asBC_JNS || Opcode == asBC_JP || Opcode == asBC_JNP
		|| Opcode == asBC_JLowZ || Opcode == asBC_JLowNZ)
	{
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Label;
	}
	if (Opcode == asBC_ADDi || Opcode == asBC_MULi || Opcode == asBC_SUBi)
	{
		OutInfo.OperandCount = 3;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[2] = asEByteCodeOperandKind::Local;
	}
	if (Opcode == asBC_SetV4)
	{
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::ImmediateS32;
	}
	if (Opcode == asBC_CpyVtoR4)
	{
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
	}
	if (Opcode == asBC_CALLSYS || Opcode == asBC_Thiscall1 || Opcode == asBC_FuncPtr
		|| Opcode == asBC_CALL || Opcode == asBC_CALLINTF || Opcode == asBC_CALLBND)
	{
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::FunctionSlot;
	}
	if (Opcode == asBC_CallPtr)
	{
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::FunctionSlot;
	}
	if (Opcode == asBC_Cast)
	{
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::TypeSlot;
	}
	if (Opcode == asBC_PGA || Opcode == asBC_PshGPtr || Opcode == asBC_PshG4 || Opcode == asBC_LDG)
	{
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::GlobalSlot;
	}
	if (Opcode == asBC_CpyVtoG4 || Opcode == asBC_CpyGtoV4 || Opcode == asBC_LdGRdR4)
	{
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::GlobalSlot;
	}
	if (Opcode == asBC_SetG4)
	{
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::GlobalSlot;
		OutInfo.Operands[1] = asEByteCodeOperandKind::ImmediateS32;
	}
	if (Opcode == asBC_GETOBJ || Opcode == asBC_GETOBJREF || Opcode == asBC_GETREF)
	{
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::ImmediateS16;
		OutInfo.Operands[1] = asEByteCodeOperandKind::Local;
	}
	if (Opcode == asBC_JMPP)
	{
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::ImmediateS32;
	}
	if (Opcode == asBC_ADDSi)
	{
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::ImmediateS16;
	}
	if (Opcode == asBC_ALLOC)
	{
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::TypeSlot;
		OutInfo.Operands[1] = asEByteCodeOperandKind::ImmediateS32;
	}
	if (Opcode == asBC_FREE || Opcode == asBC_DestructScript)
	{
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::TypeSlot;
	}
	if (Opcode == asBC_REFCPY || Opcode == asBC_FinConstruct || Opcode == asBC_CopyScript || Opcode == asBC_OBJTYPE)
	{
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::TypeSlot;
	}
	if (Opcode == asBC_RefCpyV)
	{
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::TypeSlot;
	}
	if (Opcode == asBC_JitEntry)
	{
		OutInfo.OperandCount = 0;
	}
	if (Opcode == asBC_ThrowException)
	{
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::ImmediateS16;
	}
	if (Opcode == asBC_LoadVObjR || Opcode == asBC_LoadRObjR)
	{
		OutInfo.OperandCount = 2;
		OutInfo.Operands[0] = asEByteCodeOperandKind::Local;
		OutInfo.Operands[1] = asEByteCodeOperandKind::ImmediateS16;
	}
	if (Opcode == asBC_COPY)
	{
		OutInfo.OperandCount = 1;
		OutInfo.Operands[0] = asEByteCodeOperandKind::ImmediateS16;
	}
	FillOpcodeContract(OutInfo);
	return true;
}

END_AS_NAMESPACE
