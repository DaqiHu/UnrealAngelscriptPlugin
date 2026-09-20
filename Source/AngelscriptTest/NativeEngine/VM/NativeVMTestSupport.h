#pragma once

#include "../Definitions/NativeDetachedDefinitionTestSupport.h"
#include "as_bytecode_image_builder.h"
#include "as_bytecode_image_codec.h"
#include "as_bytecode_linker.h"
#include "as_context.h"
#include "as_scriptengine.h"

struct FNativeEngineOwner
{
	asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
	~FNativeEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
	void Reset() { if (Ptr) Ptr->ShutDownAndRelease(); Ptr = nullptr; }
};

inline asSByteCodeOperand ByteImm32(int32 Value)
{
	asSByteCodeOperand Operand;
	Operand.Kind = asEByteCodeOperandKind::ImmediateS32;
	Operand.Immediate = Value;
	return Operand;
}

inline asSByteCodeOperand ByteImm64(int64 Value)
{
	asSByteCodeOperand Operand;
	Operand.Kind = asEByteCodeOperandKind::ImmediateS64;
	Operand.Immediate = Value;
	return Operand;
}

inline asSByteCodeOperand ByteImm16(int16 Value)
{
	asSByteCodeOperand Operand;
	Operand.Kind = asEByteCodeOperandKind::ImmediateS16;
	Operand.Immediate = Value;
	return Operand;
}

inline asSByteCodeOperand ByteLocal(int32 Offset)
{
	asSByteCodeOperand Operand;
	Operand.Kind = asEByteCodeOperandKind::Local;
	Operand.Immediate = Offset;
	return Operand;
}

inline asSByteCodeOperand ByteSlot(asEByteCodeOperandKind Kind, asUINT Slot)
{
	asSByteCodeOperand Operand;
	Operand.Kind = Kind;
	Operand.Slot = Slot;
	return Operand;
}

// Positive image fixtures use the declaration's complete argument storage.
inline int16 ByteArgumentDwords(const asCScriptFunction& Function)
{
 int32 Count = Function.objectType && !Function.IsMetadataStatic() ? AS_PTR_SIZE : 0;
 if (Function.DoesReturnOnStack()) Count += AS_PTR_SIZE;
 for (asUINT Index = 0; Index < Function.parameterTypes.GetLength(); ++Index)
  Count += Function.parameterTypes[Index].GetSizeOnStackDWords();
 return static_cast<int16>(Count);
}
