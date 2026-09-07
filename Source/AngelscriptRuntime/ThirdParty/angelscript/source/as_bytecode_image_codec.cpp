#include "as_bytecode_image_codec.h"
#include "as_bytecode_verifier.h"

#include "Containers/Set.h"
#include "Containers/StringConv.h"
#include "HAL/UnrealMemory.h"
#include "Hash/Blake3.h"

BEGIN_AS_NAMESPACE

namespace
{
	void WriteU8(TArray<uint8>& Bytes, uint8 Value) { Bytes.Add(Value); }
	void WriteU16(TArray<uint8>& Bytes, uint16 Value)
	{
		Bytes.Add(static_cast<uint8>(Value));
		Bytes.Add(static_cast<uint8>(Value >> 8));
	}
	void WriteU32(TArray<uint8>& Bytes, uint32 Value)
	{
		Bytes.Add(static_cast<uint8>(Value));
		Bytes.Add(static_cast<uint8>(Value >> 8));
		Bytes.Add(static_cast<uint8>(Value >> 16));
		Bytes.Add(static_cast<uint8>(Value >> 24));
	}
	void WriteI32(TArray<uint8>& Bytes, int32 Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		WriteU32(Bytes, Bits);
	}
	void WriteI64(TArray<uint8>& Bytes, int64 Value)
	{
		for (int Shift = 0; Shift < 64; Shift += 8)
		{
			Bytes.Add(static_cast<uint8>(static_cast<uint64>(Value) >> Shift));
		}
	}
	void WriteBytes32(TArray<uint8>& Bytes, TConstArrayView<uint8> Value)
	{
		check(Value.Num() == 32);
		Bytes.Append(Value.GetData(), 32);
	}
	void WriteBlob(TArray<uint8>& Bytes, TConstArrayView<uint8> Value)
	{
		WriteU32(Bytes, static_cast<uint32>(Value.Num()));
		if (Value.Num() > 0)
		{
			Bytes.Append(Value.GetData(), Value.Num());
		}
	}
	void AppendUtf8Codepoint(TArray<uint8>& Bytes, uint32 Codepoint)
	{
		if (Codepoint < 0x80)
		{
			Bytes.Add(static_cast<uint8>(Codepoint));
		}
		else if (Codepoint < 0x800)
		{
			Bytes.Add(static_cast<uint8>(0xC0 | (Codepoint >> 6)));
			Bytes.Add(static_cast<uint8>(0x80 | (Codepoint & 0x3F)));
		}
		else if (Codepoint < 0x10000)
		{
			Bytes.Add(static_cast<uint8>(0xE0 | (Codepoint >> 12)));
			Bytes.Add(static_cast<uint8>(0x80 | ((Codepoint >> 6) & 0x3F)));
			Bytes.Add(static_cast<uint8>(0x80 | (Codepoint & 0x3F)));
		}
		else
		{
			Bytes.Add(static_cast<uint8>(0xF0 | (Codepoint >> 18)));
			Bytes.Add(static_cast<uint8>(0x80 | ((Codepoint >> 12) & 0x3F)));
			Bytes.Add(static_cast<uint8>(0x80 | ((Codepoint >> 6) & 0x3F)));
			Bytes.Add(static_cast<uint8>(0x80 | (Codepoint & 0x3F)));
		}
	}

	void WriteString(TArray<uint8>& Bytes, const FString& Value)
	{
		TArray<uint8> Utf8;
		const TCHAR* Data = Value.GetCharArray().GetData();
		const int32 Count = Value.Len();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			uint32 Codepoint = static_cast<uint32>(Data[Index]);
			if (Codepoint >= 0xD800 && Codepoint <= 0xDBFF && Index + 1 < Count)
			{
				const uint32 Low = static_cast<uint32>(Data[Index + 1]);
				if (Low >= 0xDC00 && Low <= 0xDFFF)
				{
					Codepoint = 0x10000 + ((Codepoint - 0xD800) << 10) + (Low - 0xDC00);
					++Index;
				}
			}
			AppendUtf8Codepoint(Utf8, Codepoint);
		}
		WriteU32(Bytes, static_cast<uint32>(Utf8.Num()));
		if (Utf8.Num() > 0)
		{
			Bytes.Append(Utf8.GetData(), Utf8.Num());
		}
	}

	bool ReadU8(TConstArrayView<uint8> Bytes, int32& Cursor, uint8& Out)
	{
		if (Cursor < 0 || Cursor >= Bytes.Num()) return false;
		Out = Bytes[Cursor++];
		return true;
	}
	bool ReadU16(TConstArrayView<uint8> Bytes, int32& Cursor, uint16& Out)
	{
		if (Cursor < 0 || static_cast<uint64>(Cursor) + 1 >= static_cast<uint64>(Bytes.Num())) return false;
		Out = static_cast<uint16>(Bytes[Cursor] | (Bytes[Cursor + 1] << 8));
		Cursor += 2;
		return true;
	}
	bool ReadU32(TConstArrayView<uint8> Bytes, int32& Cursor, uint32& Out)
	{
		if (Cursor < 0 || static_cast<uint64>(Cursor) + 3 >= static_cast<uint64>(Bytes.Num())) return false;
		Out = static_cast<uint32>(Bytes[Cursor] | (Bytes[Cursor + 1] << 8) | (Bytes[Cursor + 2] << 16) | (Bytes[Cursor + 3] << 24));
		Cursor += 4;
		return true;
	}
	bool ReadI32(TConstArrayView<uint8> Bytes, int32& Cursor, int32& Out)
	{
		uint32 Bits = 0;
		if (!ReadU32(Bytes, Cursor, Bits)) return false;
		FMemory::Memcpy(&Out, &Bits, sizeof(Out));
		return true;
	}
	bool ReadI64(TConstArrayView<uint8> Bytes, int32& Cursor, int64& Out)
	{
		if (Cursor < 0 || static_cast<uint64>(Cursor) + 7 >= static_cast<uint64>(Bytes.Num())) return false;
		uint64 Value = 0;
		for (int Index = 0; Index < 8; ++Index)
		{
			Value |= static_cast<uint64>(Bytes[Cursor++]) << (Index * 8);
		}
		Out = static_cast<int64>(Value);
		return true;
	}
	bool ReadBytes32(TConstArrayView<uint8> Bytes, int32& Cursor, uint8* Out)
	{
		if (Cursor < 0 || static_cast<uint64>(Cursor) + 31 >= static_cast<uint64>(Bytes.Num())) return false;
		FMemory::Memcpy(Out, Bytes.GetData() + Cursor, 32);
		Cursor += 32;
		return true;
	}
	bool ReadBlob(TConstArrayView<uint8> Bytes, int32& Cursor, TArray<uint8>& Out)
	{
		uint32 Length = 0;
		if (!ReadU32(Bytes, Cursor, Length)) return false;
		if (Length > 1024 * 1024) return false;
		if (static_cast<uint64>(Cursor) + Length > static_cast<uint64>(Bytes.Num())) return false;
		Out.Reset();
		if (Length > 0)
		{
			Out.Append(Bytes.GetData() + Cursor, static_cast<int32>(Length));
		}
		Cursor += static_cast<int32>(Length);
		return true;
	}

	bool IsValidUtf8(TConstArrayView<uint8> Bytes)
	{
		int32 Index = 0;
		while (Index < Bytes.Num())
		{
			const uint8 Lead = Bytes[Index];
			int32 Need = 1;
			if ((Lead & 0x80) == 0) Need = 1;
			else if ((Lead & 0xE0) == 0xC0) Need = 2;
			else if ((Lead & 0xF0) == 0xE0) Need = 3;
			else if ((Lead & 0xF8) == 0xF0) Need = 4;
			else return false;
			if (Index + Need > Bytes.Num()) return false;
			for (int32 Trail = 1; Trail < Need; ++Trail)
			{
				if ((Bytes[Index + Trail] & 0xC0) != 0x80) return false;
			}
			Index += Need;
		}
		return true;
	}

	bool Utf8ToFStringExact(TConstArrayView<uint8> Bytes, FString& Out)
	{
		TArray<TCHAR> Chars;
		int32 Index = 0;
		while (Index < Bytes.Num())
		{
			const uint8 Lead = Bytes[Index];
			int32 Need = 1;
			uint32 Codepoint = 0;
			if ((Lead & 0x80) == 0) { Need = 1; Codepoint = Lead; }
			else if ((Lead & 0xE0) == 0xC0) { Need = 2; Codepoint = Lead & 0x1F; }
			else if ((Lead & 0xF0) == 0xE0) { Need = 3; Codepoint = Lead & 0x0F; }
			else if ((Lead & 0xF8) == 0xF0) { Need = 4; Codepoint = Lead & 0x07; }
			else return false;
			if (Index + Need > Bytes.Num()) return false;
			for (int32 Trail = 1; Trail < Need; ++Trail)
			{
				if ((Bytes[Index + Trail] & 0xC0) != 0x80) return false;
				Codepoint = (Codepoint << 6) | (Bytes[Index + Trail] & 0x3F);
			}
			Index += Need;
			if (Codepoint <= 0xFFFF)
			{
				Chars.Add(static_cast<TCHAR>(Codepoint));
			}
			else
			{
				Codepoint -= 0x10000;
				Chars.Add(static_cast<TCHAR>(0xD800 + (Codepoint >> 10)));
				Chars.Add(static_cast<TCHAR>(0xDC00 + (Codepoint & 0x3FF)));
			}
		}
		if (Chars.Num() == 0)
		{
			Out.Reset();
			return true;
		}
		Out = FString(Chars.Num(), Chars.GetData());
		return true;
	}

	asEByteCodeImageStatus ReadString(TConstArrayView<uint8> Bytes, int32& Cursor, FString& Out)
	{
		uint32 Length = 0;
		if (!ReadU32(Bytes, Cursor, Length)) return asEByteCodeImageStatus::Truncated;
		if (Length > 1024 * 1024) return asEByteCodeImageStatus::Overflow;
		if (static_cast<uint64>(Cursor) + Length > static_cast<uint64>(Bytes.Num())) return asEByteCodeImageStatus::Truncated;
		TConstArrayView<uint8> View = MakeArrayView(Bytes.GetData() + Cursor, static_cast<int32>(Length));
		if (!IsValidUtf8(View) || !Utf8ToFStringExact(View, Out)) return asEByteCodeImageStatus::InvalidArgument;
		Cursor += static_cast<int32>(Length);
		return asEByteCodeImageStatus::Succeeded;
	}

	bool DigestMatchesWitness(const asSFingerprintDigest& Digest, TConstArrayView<uint8> Witness)
	{
		if (Witness.Num() == 0 && Digest.IsZero()) return true;
		if (Witness.Num() == 0 || Digest.IsZero()) return false;
		const FBlake3Hash Hash = FBlake3::HashBuffer(Witness.GetData(), static_cast<uint64>(Witness.Num()));
		asSFingerprintDigest Computed;
		if (!asSFingerprintDigest::TryCreate(MakeArrayView(Hash.GetBytes(), asSFingerprintDigest::ByteCount), Computed))
		{
			return false;
		}
		return Computed == Digest;
	}

	asEByteCodeImageStatus ReadStableKey(TConstArrayView<uint8> Bytes, int32& Cursor, asSStableKey& Out)
	{
		uint8 KeyBytes[32];
		if (!ReadBytes32(Bytes, Cursor, KeyBytes)) return asEByteCodeImageStatus::Truncated;
		if (!asSStableKey::TryCreate(MakeArrayView(KeyBytes, 32), Out)) return asEByteCodeImageStatus::InvalidArgument;
		return asEByteCodeImageStatus::Succeeded;
	}

	asEByteCodeImageStatus ReadDigest(TConstArrayView<uint8> Bytes, int32& Cursor, asSFingerprintDigest& Out)
	{
		uint8 DigestBytes[32];
		if (!ReadBytes32(Bytes, Cursor, DigestBytes)) return asEByteCodeImageStatus::Truncated;
		if (!asSFingerprintDigest::TryCreate(MakeArrayView(DigestBytes, 32), Out)) return asEByteCodeImageStatus::InvalidArgument;
		return asEByteCodeImageStatus::Succeeded;
	}
}

asEByteCodeImageStatus asCByteCodeImageCodec::Encode(const asCByteCodeImage& Image, TArray<uint8>& OutBytes)
{
	OutBytes.Reset();
	OutBytes.Append(reinterpret_cast<const uint8*>("ASBC"), 4);
	WriteU16(OutBytes, CurrentVersion);
	WriteU16(OutBytes, static_cast<uint16>(Image.OpcodeAbi));
	WriteU8(OutBytes, static_cast<uint8>(Image.PointerByteSize));
	WriteU8(OutBytes, Image.Endian);
	WriteU8(OutBytes, Image.bFloatIsFloat64 ? 1 : 0);
	WriteU8(OutBytes, Image.CallPolicy);
	WriteU8(OutBytes, Image.StoragePolicy);
	WriteU8(OutBytes, 0);
	WriteU32(OutBytes, static_cast<uint32>(Image.Requirements.Num()));
	for (const asSByteCodeRequirement& Requirement : Image.Requirements)
	{
		WriteU8(OutBytes, static_cast<uint8>(Requirement.Role));
		WriteBytes32(OutBytes, Requirement.Key.GetBytes());
		WriteBytes32(OutBytes, Requirement.Schema.Digest.GetBytes());
		WriteBytes32(OutBytes, Requirement.Layout.Digest.GetBytes());
		WriteBlob(OutBytes, Requirement.SchemaWitness);
		WriteBlob(OutBytes, Requirement.LayoutWitness);
		WriteBytes32(OutBytes, Requirement.SignatureKey.GetBytes());
		WriteBytes32(OutBytes, Requirement.ReturnTypeUse.GetBytes());
		WriteU32(OutBytes, static_cast<uint32>(Requirement.Parameters.Num()));
		for (const asSByteCodeParameterContract& Parameter : Requirement.Parameters)
		{
			WriteBytes32(OutBytes, Parameter.TypeUse.GetBytes());
			WriteU8(OutBytes, Parameter.PassingMode);
		}
		WriteU32(OutBytes, Requirement.CallableTraits);
		WriteBytes32(OutBytes, Requirement.OwnerKey.GetBytes());
		WriteI32(OutBytes, Requirement.NativeOffset);
		WriteU8(OutBytes, Requirement.NativeAddressMode);
		WriteU32(OutBytes, static_cast<uint32>(Requirement.ReferencedKeys.Num()));
		for (const asSStableKey& Key : Requirement.ReferencedKeys)
		{
			WriteBytes32(OutBytes, Key.GetBytes());
		}
		WriteString(OutBytes, Requirement.Name);
		WriteBlob(OutBytes, Requirement.ContractWitness);
	}
	WriteU32(OutBytes, static_cast<uint32>(Image.Strings.Num()));
	for (const FString& String : Image.Strings)
	{
		WriteString(OutBytes, String);
	}
	WriteU32(OutBytes, static_cast<uint32>(Image.Functions.Num()));
	for (const asSByteCodeFunctionBody& Function : Image.Functions)
	{
		WriteBytes32(OutBytes, Function.FunctionKey.GetBytes());
		WriteBytes32(OutBytes, Function.SignatureKey.GetBytes());
		WriteU32(OutBytes, Function.FrameSize);
		WriteU32(OutBytes, Function.ParameterSize);
		WriteU32(OutBytes, Function.Frame.LocalDwords);
		WriteU32(OutBytes, Function.Frame.ParameterDwords);
		WriteU32(OutBytes, Function.Frame.ResultDwords);
		WriteI32(OutBytes, Function.Frame.ResultOffset);
		WriteU32(OutBytes, static_cast<uint32>(Function.Frame.InitializedObjectSlots.Num()));
		for (int32 Slot : Function.Frame.InitializedObjectSlots) WriteI32(OutBytes, Slot);
		WriteU32(OutBytes, static_cast<uint32>(Function.Instructions.Num()));
		for (const asSByteCodeInstruction& Instruction : Function.Instructions)
		{
			WriteU16(OutBytes, static_cast<uint16>(Instruction.Opcode));
			WriteU8(OutBytes, static_cast<uint8>(Instruction.Operands.Num()));
			for (const asSByteCodeOperand& Operand : Instruction.Operands)
			{
				WriteU8(OutBytes, static_cast<uint8>(Operand.Kind));
				WriteI64(OutBytes, Operand.Immediate);
				WriteU32(OutBytes, Operand.Slot);
				WriteU32(OutBytes, Operand.Label);
			}
		}
		WriteU32(OutBytes, static_cast<uint32>(Function.Cleanup.Num()));
		for (const asSByteCodeCleanupAction& Cleanup : Function.Cleanup)
		{
			WriteU8(OutBytes, static_cast<uint8>(Cleanup.Kind));
			WriteU8(OutBytes, static_cast<uint8>(Cleanup.Path));
			WriteI32(OutBytes, Cleanup.FrameOffset);
			WriteU32(OutBytes, Cleanup.TypeSlot);
			WriteU32(OutBytes, Cleanup.InstructionIndex);
		}
		WriteU32(OutBytes, static_cast<uint32>(Function.Source.Num()));
		for (const asSByteCodeSourceRecord& Source : Function.Source)
		{
			WriteU32(OutBytes, Source.InstructionIndex);
			WriteI32(OutBytes, Source.Line);
			WriteI32(OutBytes, Source.Column);
			WriteU32(OutBytes, Source.FileStringSlot);
		}
		WriteU32(OutBytes, static_cast<uint32>(Function.DebugOffsets.Num()));
		for (int32 Offset : Function.DebugOffsets) WriteI32(OutBytes, Offset);
	}
	return asEByteCodeImageStatus::Succeeded;
}

asEByteCodeImageStatus asCByteCodeImageCodec::Decode(TConstArrayView<uint8> Bytes, TUniquePtr<asCByteCodeImage>& OutImage)
{
	OutImage.Reset();
	if (Bytes.Num() < 14) return asEByteCodeImageStatus::Truncated;
	if (Bytes[0] != 'A' || Bytes[1] != 'S' || Bytes[2] != 'B' || Bytes[3] != 'C') return asEByteCodeImageStatus::InvalidArgument;
	int32 Cursor = 4;
	uint16 Version = 0;
	uint16 OpcodeAbi = 0;
	uint8 PointerSize = 0;
	uint8 Endian = 0;
	uint8 Float64 = 0;
	uint8 CallPolicy = 0;
	uint8 StoragePolicy = 0;
	uint8 Reserved = 0;
	if (!ReadU16(Bytes, Cursor, Version)) return asEByteCodeImageStatus::Truncated;
	if (Version == 1 || Version != CurrentVersion) return asEByteCodeImageStatus::UnknownVersion;
	if (!ReadU16(Bytes, Cursor, OpcodeAbi) || OpcodeAbi != asBC_MAXBYTECODE) return asEByteCodeImageStatus::UnknownVersion;
	if (!ReadU8(Bytes, Cursor, PointerSize) || !ReadU8(Bytes, Cursor, Endian) || !ReadU8(Bytes, Cursor, Float64)
		|| !ReadU8(Bytes, Cursor, CallPolicy) || !ReadU8(Bytes, Cursor, StoragePolicy) || !ReadU8(Bytes, Cursor, Reserved))
	{
		return asEByteCodeImageStatus::Truncated;
	}
	if (Endian != 1 || (PointerSize != 4 && PointerSize != 8)) return asEByteCodeImageStatus::InvalidArgument;
	auto Image = MakeUnique<asCByteCodeImage>();
	Image->FormatVersion = Version;
	Image->OpcodeAbi = OpcodeAbi;
	Image->PointerByteSize = PointerSize;
	Image->Endian = Endian;
	Image->bFloatIsFloat64 = Float64 != 0;
	Image->CallPolicy = CallPolicy;
	Image->StoragePolicy = StoragePolicy;
	uint32 RequirementCount = 0;
	if (!ReadU32(Bytes, Cursor, RequirementCount)) return asEByteCodeImageStatus::Truncated;
	if (RequirementCount > 100000) return asEByteCodeImageStatus::Overflow;
	TSet<TPair<uint8, FString>> SeenKeys;
	for (uint32 Index = 0; Index < RequirementCount; ++Index)
	{
		asSByteCodeRequirement Requirement;
		uint8 Role = 0;
		if (!ReadU8(Bytes, Cursor, Role)) return asEByteCodeImageStatus::Truncated;
		Requirement.Role = static_cast<asEByteCodeRequirementRole>(Role);
		const asEByteCodeImageStatus KeyStatus = ReadStableKey(Bytes, Cursor, Requirement.Key);
		if (KeyStatus != asEByteCodeImageStatus::Succeeded) return KeyStatus;
		const asEByteCodeImageStatus SchemaStatus = ReadDigest(Bytes, Cursor, Requirement.Schema.Digest);
		if (SchemaStatus != asEByteCodeImageStatus::Succeeded) return SchemaStatus;
		const asEByteCodeImageStatus LayoutStatus = ReadDigest(Bytes, Cursor, Requirement.Layout.Digest);
		if (LayoutStatus != asEByteCodeImageStatus::Succeeded) return LayoutStatus;
		if (!ReadBlob(Bytes, Cursor, Requirement.SchemaWitness) || !ReadBlob(Bytes, Cursor, Requirement.LayoutWitness))
		{
			return asEByteCodeImageStatus::Truncated;
		}
		if (!DigestMatchesWitness(Requirement.Schema.Digest, Requirement.SchemaWitness)
			|| !DigestMatchesWitness(Requirement.Layout.Digest, Requirement.LayoutWitness))
		{
			return asEByteCodeImageStatus::CorruptWitness;
		}
		const asEByteCodeImageStatus SignatureStatus = ReadStableKey(Bytes, Cursor, Requirement.SignatureKey);
		if (SignatureStatus != asEByteCodeImageStatus::Succeeded) return SignatureStatus;
		const asEByteCodeImageStatus ReturnStatus = ReadStableKey(Bytes, Cursor, Requirement.ReturnTypeUse);
		if (ReturnStatus != asEByteCodeImageStatus::Succeeded) return ReturnStatus;
		uint32 ParamCount = 0;
		if (!ReadU32(Bytes, Cursor, ParamCount) || ParamCount > 10000) return ParamCount > 10000 ? asEByteCodeImageStatus::Overflow : asEByteCodeImageStatus::Truncated;
		for (uint32 Param = 0; Param < ParamCount; ++Param)
		{
			asSByteCodeParameterContract Parameter;
			const asEByteCodeImageStatus TypeStatus = ReadStableKey(Bytes, Cursor, Parameter.TypeUse);
			if (TypeStatus != asEByteCodeImageStatus::Succeeded) return TypeStatus;
			if (!ReadU8(Bytes, Cursor, Parameter.PassingMode)) return asEByteCodeImageStatus::Truncated;
			Requirement.Parameters.Add(Parameter);
		}
		if (!ReadU32(Bytes, Cursor, Requirement.CallableTraits)) return asEByteCodeImageStatus::Truncated;
		const asEByteCodeImageStatus OwnerStatus = ReadStableKey(Bytes, Cursor, Requirement.OwnerKey);
		if (OwnerStatus != asEByteCodeImageStatus::Succeeded) return OwnerStatus;
		if (!ReadI32(Bytes, Cursor, Requirement.NativeOffset) || !ReadU8(Bytes, Cursor, Requirement.NativeAddressMode))
		{
			return asEByteCodeImageStatus::Truncated;
		}
		uint32 Referenced = 0;
		if (!ReadU32(Bytes, Cursor, Referenced) || Referenced > 10000) return Referenced > 10000 ? asEByteCodeImageStatus::Overflow : asEByteCodeImageStatus::Truncated;
		for (uint32 Ref = 0; Ref < Referenced; ++Ref)
		{
			asSStableKey RefKey;
			const asEByteCodeImageStatus RefStatus = ReadStableKey(Bytes, Cursor, RefKey);
			if (RefStatus != asEByteCodeImageStatus::Succeeded) return RefStatus;
			Requirement.ReferencedKeys.Add(RefKey);
		}
		const asEByteCodeImageStatus NameStatus = ReadString(Bytes, Cursor, Requirement.Name);
		if (NameStatus != asEByteCodeImageStatus::Succeeded) return NameStatus;
		if (!ReadBlob(Bytes, Cursor, Requirement.ContractWitness)) return asEByteCodeImageStatus::Truncated;
		const TPair<uint8, FString> Identity{ static_cast<uint8>(Requirement.Role), Requirement.Key.ToHexString() };
		if (SeenKeys.Contains(Identity)) return asEByteCodeImageStatus::Duplicate;
		SeenKeys.Add(Identity);
		Image->Requirements.Add(MoveTemp(Requirement));
	}
	uint32 StringCount = 0;
	if (!ReadU32(Bytes, Cursor, StringCount)) return asEByteCodeImageStatus::Truncated;
	if (StringCount > 100000) return asEByteCodeImageStatus::Overflow;
	for (uint32 Index = 0; Index < StringCount; ++Index)
	{
		FString Value;
		const asEByteCodeImageStatus Status = ReadString(Bytes, Cursor, Value);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Image->Strings.Add(MoveTemp(Value));
	}
	uint32 FunctionCount = 0;
	if (!ReadU32(Bytes, Cursor, FunctionCount)) return asEByteCodeImageStatus::Truncated;
	if (FunctionCount > 100000) return asEByteCodeImageStatus::Overflow;
	for (uint32 Index = 0; Index < FunctionCount; ++Index)
	{
		asSByteCodeFunctionBody Function;
		const asEByteCodeImageStatus FnKeyStatus = ReadStableKey(Bytes, Cursor, Function.FunctionKey);
		if (FnKeyStatus != asEByteCodeImageStatus::Succeeded) return FnKeyStatus;
		const asEByteCodeImageStatus SigStatus = ReadStableKey(Bytes, Cursor, Function.SignatureKey);
		if (SigStatus != asEByteCodeImageStatus::Succeeded) return SigStatus;
		uint32 InstructionCount = 0;
		uint32 InitCount = 0;
		if (!ReadU32(Bytes, Cursor, Function.FrameSize) || !ReadU32(Bytes, Cursor, Function.ParameterSize)
			|| !ReadU32(Bytes, Cursor, Function.Frame.LocalDwords) || !ReadU32(Bytes, Cursor, Function.Frame.ParameterDwords)
			|| !ReadU32(Bytes, Cursor, Function.Frame.ResultDwords) || !ReadI32(Bytes, Cursor, Function.Frame.ResultOffset)
			|| !ReadU32(Bytes, Cursor, InitCount))
		{
			return asEByteCodeImageStatus::Truncated;
		}
		if (InitCount > 100000) return asEByteCodeImageStatus::Overflow;
		for (uint32 Init = 0; Init < InitCount; ++Init)
		{
			int32 Slot = 0;
			if (!ReadI32(Bytes, Cursor, Slot)) return asEByteCodeImageStatus::Truncated;
			Function.Frame.InitializedObjectSlots.Add(Slot);
		}
		if (!ReadU32(Bytes, Cursor, InstructionCount)) return asEByteCodeImageStatus::Truncated;
		if (InstructionCount > 1000000) return asEByteCodeImageStatus::Overflow;
		for (uint32 InstructionIndex = 0; InstructionIndex < InstructionCount; ++InstructionIndex)
		{
			uint16 Opcode = 0;
			uint8 OperandCount = 0;
			if (!ReadU16(Bytes, Cursor, Opcode) || !ReadU8(Bytes, Cursor, OperandCount)) return asEByteCodeImageStatus::Truncated;
			if (Opcode == asBC_STR || Opcode > asBC_MAXBYTECODE) return asEByteCodeImageStatus::UnknownOpcode;
			asSByteCodeInstruction Instruction;
			Instruction.Opcode = static_cast<asEBCInstr>(Opcode);
			for (uint8 OperandIndex = 0; OperandIndex < OperandCount; ++OperandIndex)
			{
				asSByteCodeOperand Operand;
				uint8 Kind = 0;
				if (!ReadU8(Bytes, Cursor, Kind) || !ReadI64(Bytes, Cursor, Operand.Immediate)
					|| !ReadU32(Bytes, Cursor, Operand.Slot) || !ReadU32(Bytes, Cursor, Operand.Label))
				{
					return asEByteCodeImageStatus::Truncated;
				}
				Operand.Kind = static_cast<asEByteCodeOperandKind>(Kind);
				Instruction.Operands.Add(Operand);
			}
			Function.Instructions.Add(MoveTemp(Instruction));
		}
		uint32 CleanupCount = 0;
		if (!ReadU32(Bytes, Cursor, CleanupCount)) return asEByteCodeImageStatus::Truncated;
		if (CleanupCount > 100000) return asEByteCodeImageStatus::Overflow;
		for (uint32 CleanupIndex = 0; CleanupIndex < CleanupCount; ++CleanupIndex)
		{
			asSByteCodeCleanupAction Cleanup;
			uint8 Kind = 0;
			uint8 Path = 0;
			if (!ReadU8(Bytes, Cursor, Kind) || !ReadU8(Bytes, Cursor, Path) || !ReadI32(Bytes, Cursor, Cleanup.FrameOffset)
				|| !ReadU32(Bytes, Cursor, Cleanup.TypeSlot) || !ReadU32(Bytes, Cursor, Cleanup.InstructionIndex))
			{
				return asEByteCodeImageStatus::Truncated;
			}
			Cleanup.Kind = static_cast<asEByteCodeCleanupKind>(Kind);
			Cleanup.Path = static_cast<asEByteCodeCleanupPath>(Path);
			Function.Cleanup.Add(Cleanup);
		}
		uint32 SourceCount = 0;
		if (!ReadU32(Bytes, Cursor, SourceCount)) return asEByteCodeImageStatus::Truncated;
		if (SourceCount > 100000) return asEByteCodeImageStatus::Overflow;
		for (uint32 SourceIndex = 0; SourceIndex < SourceCount; ++SourceIndex)
		{
			asSByteCodeSourceRecord Source;
			if (!ReadU32(Bytes, Cursor, Source.InstructionIndex) || !ReadI32(Bytes, Cursor, Source.Line)
				|| !ReadI32(Bytes, Cursor, Source.Column) || !ReadU32(Bytes, Cursor, Source.FileStringSlot))
			{
				return asEByteCodeImageStatus::Truncated;
			}
			Function.Source.Add(Source);
		}
		uint32 DebugCount = 0;
		if (!ReadU32(Bytes, Cursor, DebugCount)) return asEByteCodeImageStatus::Truncated;
		if (DebugCount > 100000) return asEByteCodeImageStatus::Overflow;
		for (uint32 DebugIndex = 0; DebugIndex < DebugCount; ++DebugIndex)
		{
			int32 Offset = 0;
			if (!ReadI32(Bytes, Cursor, Offset)) return asEByteCodeImageStatus::Truncated;
			Function.DebugOffsets.Add(Offset);
		}
		Image->Functions.Add(MoveTemp(Function));
	}
	if (Cursor != Bytes.Num()) return asEByteCodeImageStatus::InvalidArgument;
	if (asCByteCodeVerifier::Verify(*Image) != asEByteCodeVerifyStatus::Succeeded)
	{
		return asEByteCodeImageStatus::InvalidOperand;
	}
	OutImage = MoveTemp(Image);
	return asEByteCodeImageStatus::Succeeded;
}

asEByteCodeImageStatus asCByteCodeImageCodec::Dump(const asCByteCodeImage& Image, FString& OutText)
{
	OutText.Reset();
	OutText += FString::Printf(TEXT("ASBC v%u abi=%u ptr=%u\n"), Image.FormatVersion, Image.OpcodeAbi, Image.PointerByteSize);
	for (const asSByteCodeRequirement& Requirement : Image.Requirements)
	{
		OutText += FString::Printf(TEXT("req role=%u name=%s key=%s schema=%s layout=%s\n"),
			static_cast<uint32>(Requirement.Role),
			*Requirement.Name,
			*Requirement.Key.ToHexString(),
			*Requirement.Schema.Digest.ToHexString(),
			*Requirement.Layout.Digest.ToHexString());
	}
	for (const asSByteCodeFunctionBody& Function : Image.Functions)
	{
		OutText += FString::Printf(TEXT("fn %s frame=%u cleanup=%d source=%d\n"),
			*Function.FunctionKey.ToHexString(),
			Function.FrameSize,
			Function.Cleanup.Num(),
			Function.Source.Num());
		for (const asSByteCodeInstruction& Instruction : Function.Instructions)
		{
			OutText += FString::Printf(TEXT("  op %u\n"), static_cast<uint32>(Instruction.Opcode));
		}
	}
	return asEByteCodeImageStatus::Succeeded;
}

END_AS_NAMESPACE
