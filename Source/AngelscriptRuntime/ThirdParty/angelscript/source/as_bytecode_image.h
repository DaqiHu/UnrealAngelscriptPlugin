/* Maintained Unreal AngelScript fork — engine-free symbolic executable image. */
#pragma once

#include "as_bytecode_opcode.h"
#include "as_metadata_fingerprint.h"
#include "frontend/as_stable_key.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

BEGIN_AS_NAMESPACE

enum class asEByteCodeImageStatus : asBYTE
{
	Succeeded,
	InvalidArgument,
	Incomplete,
	Duplicate,
	UnknownVersion,
	Truncated,
	Overflow,
	UnknownOpcode,
	InvalidOperand,
	MissingLabel,
	CorruptWitness
};

enum class asEByteCodeRequirementRole : asBYTE
{
	Type = 1,
	TypeUse = 2,
	Function = 3,
	Property = 4,
	Global = 5,
	String = 6
};

struct asSByteCodeParameterContract
{
	asSStableKey TypeUse;
	uint8 PassingMode = 0;
};

struct asSByteCodeRequirement
{
	asEByteCodeRequirementRole Role = asEByteCodeRequirementRole::Type;
	asSStableKey Key;
	asSSchemaHash Schema;
	asSLayoutHash Layout;
	TArray<uint8> SchemaWitness;
	TArray<uint8> LayoutWitness;
	TArray<uint8> ContractWitness;
	asSStableKey SignatureKey;
	asSStableKey ReturnTypeUse;
	TArray<asSByteCodeParameterContract> Parameters;
	uint32 CallableTraits = 0;
	asSStableKey OwnerKey;
	int32 NativeOffset = 0;
	uint8 NativeAddressMode = 0;
	TArray<asSStableKey> ReferencedKeys;
	FString Name;
};

// Canonical contract bytes (schema 1): u8 Role, Key[32], Schema digest[32],
// Layout digest[32], u32 SchemaWitnessLen+bytes, u32 LayoutWitnessLen+bytes,
// SignatureKey[32], ReturnTypeUse[32], u32 ParamCount, {TypeUse[32], u8 Mode}*,
// u32 CallableTraits, OwnerKey[32], i32 NativeOffset, u8 NativeAddressMode,
// u32 RefCount, sorted ReferencedKeys[32]*.
ANGELSCRIPTRUNTIME_API void asEncodeByteCodeRequirementContract(
	const asSByteCodeRequirement& Requirement,
	TArray<uint8>& Out);

struct asSByteCodeInstruction
{
	asEBCInstr Opcode = asBC_MAXBYTECODE;
	TArray<asSByteCodeOperand> Operands;
};

enum class asEByteCodeCleanupKind : asBYTE
{
	None = 0,
	DestroyObject = 1,
	ReleaseHandle = 2
};

enum class asEByteCodeCleanupPath : asBYTE
{
	Normal = 1,
	Exception = 2,
	Both = 3
};

struct asSByteCodeCleanupAction
{
	asEByteCodeCleanupKind Kind = asEByteCodeCleanupKind::None;
	asEByteCodeCleanupPath Path = asEByteCodeCleanupPath::Both;
	int32 FrameOffset = 0;
	asUINT TypeSlot = 0;
	asUINT InstructionIndex = 0;
};

struct asSByteCodeSourceRecord
{
	asUINT InstructionIndex = 0;
	int32 Line = 0;
	int32 Column = 0;
	asUINT FileStringSlot = 0;
};

struct asSByteCodeFrameDescriptor
{
	asUINT LocalDwords = 0;
	asUINT ParameterDwords = 0;
	asUINT ResultDwords = 0;
	int32 ResultOffset = 0;
	TArray<int32> InitializedObjectSlots;
};

struct asSByteCodeFunctionBody
{
	asSStableKey FunctionKey;
	asSStableKey SignatureKey;
	asUINT FrameSize = 0;
	asUINT ParameterSize = 0;
	asSByteCodeFrameDescriptor Frame;
	TArray<asSByteCodeInstruction> Instructions;
	TArray<asSByteCodeCleanupAction> Cleanup;
	TArray<asSByteCodeSourceRecord> Source;
	TArray<int32> DebugOffsets;
};

class ANGELSCRIPTRUNTIME_API asCByteCodeImage
{
public:
	asUINT GetFormatVersion() const { return FormatVersion; }
	asUINT GetOpcodeAbi() const { return OpcodeAbi; }
	asUINT GetPointerByteSize() const { return PointerByteSize; }
	TConstArrayView<asSByteCodeRequirement> GetRequirements() const { return Requirements; }
	TConstArrayView<asSByteCodeFunctionBody> GetFunctions() const { return Functions; }
	TConstArrayView<FString> GetStrings() const { return Strings; }

	TArray<asSByteCodeRequirement> Requirements;
	TArray<asSByteCodeFunctionBody> Functions;
	TArray<FString> Strings;
	asUINT FormatVersion = 2;
	asUINT OpcodeAbi = asBC_MAXBYTECODE;
	asUINT PointerByteSize = sizeof(void*);
	bool bFloatIsFloat64 = true;
	uint8 Endian = 1;
	uint8 CallPolicy = 0;
	uint8 StoragePolicy = 0;
	mutable bool bValidated = false;
};

END_AS_NAMESPACE
