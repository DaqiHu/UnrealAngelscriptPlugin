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

// Verified incoming states for frame-owned object slots. The linker lowers
// these states into the existing PC-indexed exception-cleanup directory.
enum class asEByteCodeObjectState : uint8
{
	Uninitialized,
	Allocated,
	Constructed
};

struct asSByteCodeFunctionLifetime
{
	asSStableKey FunctionKey;
	TArray<int32> FrameOffsets;
	TArray<asUINT> TypeSlots;
	TArray<TArray<asEByteCodeObjectState>> Before;
	TArray<bool> Reachable;
	// Maximum reachable expression/argument stack extent below the local frame.
	asUINT TemporaryStackDwords = 0;
};

class ANGELSCRIPTRUNTIME_API asCByteCodeVerifier
{
public:
	static asEByteCodeVerifyStatus Verify(const asCByteCodeImage& Image);
	static asSByteCodeVerifyFault VerifyWithFault(const asCByteCodeImage& Image);
	static asSByteCodeVerifyFault VerifyWithLifetimes(const asCByteCodeImage& Image, TArray<asSByteCodeFunctionLifetime>& Out);
};

END_AS_NAMESPACE
