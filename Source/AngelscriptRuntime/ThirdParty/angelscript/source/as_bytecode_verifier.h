/* Maintained Unreal AngelScript fork — structural bytecode image verifier. */
#pragma once

#include "as_bytecode_image.h"

BEGIN_AS_NAMESPACE

enum class asEByteCodeVerifyStatus : asBYTE
{
	Succeeded,
	UnknownOpcode,
	InvalidOperand,
	InvalidJump,
	MissingRequirement,
	StackUnderflow,
	InvalidFrame,
	CorruptClosure
};

struct asSByteCodeVerifyFault
{
	asEByteCodeVerifyStatus Status = asEByteCodeVerifyStatus::Succeeded;
	asSStableKey FunctionKey;
	int32 InstructionIndex = -1;
	int32 OperandIndex = -1;
};

class ANGELSCRIPTRUNTIME_API asCByteCodeVerifier
{
public:
	static asEByteCodeVerifyStatus Verify(const asCByteCodeImage& Image);
	static asSByteCodeVerifyFault VerifyWithFault(const asCByteCodeImage& Image);
};

END_AS_NAMESPACE
