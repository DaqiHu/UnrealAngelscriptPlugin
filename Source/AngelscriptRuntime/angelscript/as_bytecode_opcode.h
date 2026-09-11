/* Maintained Unreal AngelScript fork — symbolic bytecode operand descriptors. */
#pragma once

#include "as_config.h"
#include "angelscript.h"

BEGIN_AS_NAMESPACE

enum class asEByteCodeOperandKind : asBYTE
{
	None = 0,
	ImmediateS16,
	ImmediateS32,
	ImmediateS64,
	Local,
	Label,
	TypeSlot,
	FunctionSlot,
	PropertySlot,
	GlobalSlot,
	StringSlot
};

struct asSByteCodeOperand
{
	asEByteCodeOperandKind Kind = asEByteCodeOperandKind::None;
	int64 Immediate = 0;
	asUINT Slot = 0;
	asUINT Label = 0;
};

enum class asEByteCodeControlKind : asBYTE
{
	None = 0,
	Fallthrough,
	Jump,
	ConditionalJump,
	IndexedJump,
	Call,
	Return,
	Throw
};

struct asSByteCodeOpcodeOperandInfo
{
	asEByteCodeOperandKind Kind = asEByteCodeOperandKind::None;
	int8 SignedWidth = 0;
	uint8 ReadWidth = 0;
	uint8 WriteWidth = 0;
	uint8 StorageWidth = 0;
};

struct asSByteCodeOpcodeInfo
{
	asEBCInstr Opcode = asBC_MAXBYTECODE;
	asEBCType Type = asBCTYPE_INFO;
	asEByteCodeOperandKind Operands[3] = {
		asEByteCodeOperandKind::None,
		asEByteCodeOperandKind::None,
		asEByteCodeOperandKind::None
	};
	asUINT OperandCount = 0;
	asSByteCodeOpcodeOperandInfo OperandInfo[3];
	asEByteCodeControlKind Control = asEByteCodeControlKind::None;
	int16 StackEffect = 0;
	uint16 CallEffect = 0;
};

ANGELSCRIPTRUNTIME_API bool asByteCodeOpcodeIsExecutable(asEBCInstr Opcode);
ANGELSCRIPTRUNTIME_API bool asByteCodeOpcodeGetInfo(asEBCInstr Opcode, asSByteCodeOpcodeInfo& OutInfo);

END_AS_NAMESPACE
