#include "as_bytecode_image_builder.h"
#include "as_metadata_image.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_property.h"
#include "as_datatype.h"
#include "frontend/as_canonical_encoding.h"
#include "frontend/as_type_context.h"
#include "frontend/as_type_identity.h"
#include "Containers/StringConv.h"
#include "HAL/UnrealMemory.h"

BEGIN_AS_NAMESPACE

namespace
{
	int32 CompareKeys(const asSStableKey& Left, const asSStableKey& Right)
	{
		return FMemory::Memcmp(Left.GetBytes().GetData(), Right.GetBytes().GetData(), asSStableKey::ByteCount);
	}

	void WriteKey(asCCanonicalEncoding& Encoding, const asSStableKey& Key)
	{
		Encoding.WriteBytes(Key.GetBytes());
	}

	void CopyWitness(TConstArrayView<uint8> Source, TArray<uint8>& Out)
	{
		Out.Reset();
		Out.Append(Source.GetData(), Source.Num());
	}

	bool InternDataTypeUse(const asCMetadataImage* Image, const asCDataType& Type, asSStableKey& OutKey)
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

	asCTypeInfo* FindTypeInImageOrDependencies(const asCMetadataImage* Image, const asSStableKey& Key)
	{
		if (!Image) return nullptr;
		if (asCTypeInfo* Found = const_cast<asCMetadataImage*>(Image)->FindType(Key)) return Found;
		for (const std::shared_ptr<asCMetadataImage>& Dependency : Image->GetDependencies())
		{
			if (Dependency)
			{
				if (asCTypeInfo* Found = FindTypeInImageOrDependencies(Dependency.get(), Key)) return Found;
			}
		}
		return nullptr;
	}

	asEByteCodeImageStatus CaptureTypeFingerprints(const asCTypeInfo& Type, asSByteCodeRequirement& Requirement)
	{
		TConstArrayView<uint8> SchemaWitness;
		TConstArrayView<uint8> LayoutWitness;
		if (Type.TryGetSchemaHash(Requirement.Schema, &SchemaWitness) != asEFingerprintStatus::Succeeded)
		{
			return asEByteCodeImageStatus::CorruptWitness;
		}
		CopyWitness(SchemaWitness, Requirement.SchemaWitness);
		const asEFingerprintStatus LayoutStatus = Type.TryGetLayoutHash(Requirement.Layout, &LayoutWitness);
		if (LayoutStatus != asEFingerprintStatus::Succeeded)
		{
			return asEByteCodeImageStatus::CorruptWitness;
		}
		CopyWitness(LayoutWitness, Requirement.LayoutWitness);
		return asEByteCodeImageStatus::Succeeded;
	}

	asSByteCodeRequirement MakeTypeRequirement(const asCTypeInfo& Type)
	{
		asSByteCodeRequirement Requirement;
		Requirement.Role = asEByteCodeRequirementRole::Type;
		Requirement.Key = Type.GetStableKey();
		Requirement.Name = UTF8_TO_TCHAR(Type.GetName());
		if (const asCObjectType* Object = CastToObjectType(const_cast<asCTypeInfo*>(&Type)))
		{
			for (asUINT Index = 0; Index < Object->localProperties.GetLength(); ++Index)
			{
				if (asCTypeInfo* Member = Object->localProperties[Index]->type.GetTypeInfo())
				{
					Requirement.ReferencedKeys.AddUnique(Member->GetStableKey());
				}
			}
			if (Object->derivedFrom)
			{
				Requirement.ReferencedKeys.AddUnique(Object->derivedFrom->GetStableKey());
			}
			for (asUINT Index = 0; Index < Object->interfaces.GetLength(); ++Index)
			{
				if (Object->interfaces[Index])
				{
					Requirement.ReferencedKeys.AddUnique(Object->interfaces[Index]->GetStableKey());
				}
			}
		}
		return Requirement;
	}

	bool SameContract(const asSByteCodeRequirement& Left, const asSByteCodeRequirement& Right)
	{
		if (Left.Schema != Right.Schema || Left.Layout != Right.Layout
			|| Left.SchemaWitness != Right.SchemaWitness || Left.LayoutWitness != Right.LayoutWitness
			|| Left.SignatureKey != Right.SignatureKey || Left.ReturnTypeUse != Right.ReturnTypeUse
			|| Left.ReturnDwords != Right.ReturnDwords || Left.ReturnStorageKind != Right.ReturnStorageKind
			|| Left.CallableTraits != Right.CallableTraits || Left.OwnerKey != Right.OwnerKey
			|| Left.NativeOffset != Right.NativeOffset || Left.NativeAddressMode != Right.NativeAddressMode
			|| Left.Parameters.Num() != Right.Parameters.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Parameters.Num(); ++Index)
		{
			if (Left.Parameters[Index].TypeUse != Right.Parameters[Index].TypeUse
				|| Left.Parameters[Index].PassingMode != Right.Parameters[Index].PassingMode
				|| Left.Parameters[Index].StorageDwords != Right.Parameters[Index].StorageDwords
				|| Left.Parameters[Index].StorageKind != Right.Parameters[Index].StorageKind)
			{
				return false;
			}
		}
		return true;
	}
}

asEByteCodeStorageKind asByteCodeStorageKind(const asCDataType& Type)
{
	if (Type.GetSizeOnStackDWords() == 0) return asEByteCodeStorageKind::Void;
	if (Type.IsReference() || (Type.GetTypeInfo() && !Type.IsEnumType())) return asEByteCodeStorageKind::Address;
	return asEByteCodeStorageKind::Value;
}

void asEncodeByteCodeRequirementContract(const asSByteCodeRequirement& Requirement, TArray<uint8>& Out)
{
	asCCanonicalEncoding Encoding(asECanonicalIdentityDomain::Schema);
	Encoding.WriteUInt8(static_cast<uint8>(Requirement.Role));
	Encoding.WriteBytes(Requirement.Key.GetBytes());
	Encoding.WriteBytes(Requirement.Schema.Digest.GetBytes());
	Encoding.WriteBytes(Requirement.Layout.Digest.GetBytes());
	Encoding.WriteUInt32(static_cast<uint32>(Requirement.SchemaWitness.Num()));
	Encoding.WriteBytes(MakeArrayView(Requirement.SchemaWitness));
	Encoding.WriteUInt32(static_cast<uint32>(Requirement.LayoutWitness.Num()));
	Encoding.WriteBytes(MakeArrayView(Requirement.LayoutWitness));
	Encoding.WriteBytes(Requirement.SignatureKey.GetBytes());
	Encoding.WriteBytes(Requirement.ReturnTypeUse.GetBytes());
	Encoding.WriteUInt32(Requirement.ReturnDwords);
	Encoding.WriteUInt8(static_cast<uint8>(Requirement.ReturnStorageKind));
	Encoding.WriteUInt32(static_cast<uint32>(Requirement.Parameters.Num()));
	for (const asSByteCodeParameterContract& Parameter : Requirement.Parameters)
	{
		Encoding.WriteBytes(Parameter.TypeUse.GetBytes());
		Encoding.WriteUInt8(Parameter.PassingMode);
		Encoding.WriteUInt32(Parameter.StorageDwords);
		Encoding.WriteUInt8(static_cast<uint8>(Parameter.StorageKind));
	}
	Encoding.WriteUInt32(Requirement.CallableTraits);
	Encoding.WriteBytes(Requirement.OwnerKey.GetBytes());
	Encoding.WriteUInt32(static_cast<uint32>(Requirement.NativeOffset));
	Encoding.WriteUInt8(Requirement.NativeAddressMode);
	TArray<asSStableKey> Sorted = Requirement.ReferencedKeys;
	Sorted.Sort([](const asSStableKey& Left, const asSStableKey& Right)
	{
		return FMemory::Memcmp(Left.GetBytes().GetData(), Right.GetBytes().GetData(), asSStableKey::ByteCount) < 0;
	});
	Encoding.WriteUInt32(static_cast<uint32>(Sorted.Num()));
	for (const asSStableKey& Key : Sorted)
	{
		Encoding.WriteBytes(Key.GetBytes());
	}
	Out.Reset();
	Out.Append(Encoding.GetBytes().GetData(), Encoding.GetBytes().Num());
}

asCByteCodeImageBuilder::asCByteCodeImageBuilder() = default;

asEByteCodeImageStatus asCByteCodeImageBuilder::AddRequirement(const asSByteCodeRequirement& Requirement, asUINT& OutSlot)
{
	OutSlot = 0;
	if (Requirement.Key.IsZero() && Requirement.Role != asEByteCodeRequirementRole::String)
	{
		return asEByteCodeImageStatus::InvalidArgument;
	}
	asSByteCodeRequirement Stored = Requirement;
	if (Stored.ContractWitness.Num() == 0)
	{
		asEncodeByteCodeRequirementContract(Stored, Stored.ContractWitness);
	}
	for (int32 Index = 0; Index < Requirements.Num(); ++Index)
	{
		if (Requirements[Index].Role == Stored.Role && Requirements[Index].Key == Stored.Key)
		{
			if (Requirements[Index].Schema == Stored.Schema
				&& Requirements[Index].SchemaWitness != Stored.SchemaWitness
				&& Stored.SchemaWitness.Num() > 0 && Requirements[Index].SchemaWitness.Num() > 0)
			{
				return asEByteCodeImageStatus::CorruptWitness;
			}
			if (!SameContract(Requirements[Index], Stored)
				|| Requirements[Index].ContractWitness != Stored.ContractWitness)
			{
				return asEByteCodeImageStatus::Duplicate;
			}
			OutSlot = static_cast<asUINT>(Index);
			return asEByteCodeImageStatus::Succeeded;
		}
	}
	OutSlot = static_cast<asUINT>(Requirements.Num());
	Requirements.Add(MoveTemp(Stored));
	return asEByteCodeImageStatus::Succeeded;
}

asEByteCodeImageStatus asCByteCodeImageBuilder::AddTypeRequirement(const asCTypeInfo& Type, asUINT& OutSlot)
{
	asSByteCodeRequirement Requirement = MakeTypeRequirement(Type);
	const asEByteCodeImageStatus Fingerprint = CaptureTypeFingerprints(Type, Requirement);
	if (Fingerprint != asEByteCodeImageStatus::Succeeded) return Fingerprint;
	asEncodeByteCodeRequirementContract(Requirement, Requirement.ContractWitness);
	const asEByteCodeImageStatus Status = AddRequirement(Requirement, OutSlot);
	if (Status != asEByteCodeImageStatus::Succeeded) return Status;
	TArray<asSStableKey> Pending = Requirement.ReferencedKeys;
	const asCMetadataImage* Image = Type.GetMetadataImage();
	while (Pending.Num() > 0)
	{
		const asSStableKey Key = Pending.Pop();
		asCTypeInfo* Referenced = FindTypeInImageOrDependencies(Image, Key);
		if (!Referenced) return asEByteCodeImageStatus::Incomplete;
		asUINT Nested = 0;
		asSByteCodeRequirement NestedRequirement = MakeTypeRequirement(*Referenced);
		const asEByteCodeImageStatus NestedFingerprint = CaptureTypeFingerprints(*Referenced, NestedRequirement);
		if (NestedFingerprint != asEByteCodeImageStatus::Succeeded) return NestedFingerprint;
		asEncodeByteCodeRequirementContract(NestedRequirement, NestedRequirement.ContractWitness);
		const asEByteCodeImageStatus NestedStatus = AddRequirement(NestedRequirement, Nested);
		if (NestedStatus != asEByteCodeImageStatus::Succeeded) return NestedStatus;
		for (const asSStableKey& Child : NestedRequirement.ReferencedKeys)
		{
			bool bKnown = Child == Type.GetStableKey();
			for (const asSByteCodeRequirement& Existing : Requirements)
			{
				if (Existing.Key == Child) { bKnown = true; break; }
			}
			if (!bKnown) Pending.Add(Child);
		}
	}
	return asEByteCodeImageStatus::Succeeded;
}

asEByteCodeImageStatus asCByteCodeImageBuilder::AddFunctionRequirement(const asCScriptFunction& Function, asUINT& OutSlot)
{
	asSByteCodeRequirement Requirement;
	Requirement.Role = asEByteCodeRequirementRole::Function;
	Requirement.Key = Function.GetStableKey();
	Requirement.Name = UTF8_TO_TCHAR(Function.GetName());
	const asCMetadataImage* Image = Function.GetMetadataImage();
	if (!InternDataTypeUse(Image, Function.GetReturnDataType(), Requirement.ReturnTypeUse))
	{
		return asEByteCodeImageStatus::CorruptWitness;
	}
	Requirement.ReturnDwords = static_cast<asUINT>(Function.GetReturnDataType().GetSizeOnStackDWords());
	Requirement.ReturnStorageKind = asByteCodeStorageKind(Function.GetReturnDataType());
	if (Function.GetReturnDataType().GetTypeInfo())
	{
		Requirement.ReferencedKeys.AddUnique(Function.GetReturnDataType().GetTypeInfo()->GetStableKey());
	}
	for (asUINT Index = 0; Index < Function.GetParamCount(); ++Index)
	{
		const asCDataType* ParameterType = Function.GetParameterDataType(Index);
		if (!ParameterType) return asEByteCodeImageStatus::InvalidArgument;
		asSByteCodeParameterContract Parameter;
		if (!InternDataTypeUse(Image, *ParameterType, Parameter.TypeUse))
		{
			return asEByteCodeImageStatus::CorruptWitness;
		}
		if (Index < Function.inOutFlags.GetLength())
		{
			Parameter.PassingMode = static_cast<uint8>(Function.inOutFlags[Index]);
		}
		Parameter.StorageDwords = static_cast<asUINT>(ParameterType->GetSizeOnStackDWords());
		Parameter.StorageKind = asByteCodeStorageKind(*ParameterType);
		Requirement.Parameters.Add(Parameter);
		if (ParameterType->GetTypeInfo())
		{
			Requirement.ReferencedKeys.AddUnique(ParameterType->GetTypeInfo()->GetStableKey());
		}
	}
	Requirement.CallableTraits = (Function.IsMetadataStatic() ? 1u : 0u)
		| (Function.IsFinal() ? 2u : 0u)
		| (Function.IsOverride() ? 4u : 0u)
		| (Function.IsProperty() ? 8u : 0u)
		| (Function.IsReadOnly() ? 16u : 0u)
		| (Function.GetMetadataDeclarationKind() == asEFunctionIdentityKind::Constructor ? 32u : 0u)
		| (Function.DoesReturnOnStack() ? 64u : 0u);
	{
		asCCanonicalEncoding Signature(asECanonicalIdentityDomain::Function);
		Signature.WriteBytes(Requirement.ReturnTypeUse.GetBytes());
		Signature.WriteUInt32(static_cast<uint32>(Requirement.Parameters.Num()));
		for (const asSByteCodeParameterContract& Parameter : Requirement.Parameters)
		{
			Signature.WriteBytes(Parameter.TypeUse.GetBytes());
			Signature.WriteUInt8(Parameter.PassingMode);
		}
		Requirement.SignatureKey = Signature.FinalizeDigest();
	}
	if (Function.GetObjectType())
	{
		Requirement.OwnerKey = static_cast<asCTypeInfo*>(Function.GetObjectType())->GetStableKey();
		Requirement.ReferencedKeys.AddUnique(Requirement.OwnerKey);
	}
	asEncodeByteCodeRequirementContract(Requirement, Requirement.ContractWitness);
	const asEByteCodeImageStatus Status = AddRequirement(Requirement, OutSlot);
	if (Status != asEByteCodeImageStatus::Succeeded) return Status;
	for (const asSStableKey& Key : Requirement.ReferencedKeys)
	{
		if (asCTypeInfo* Referenced = FindTypeInImageOrDependencies(Image, Key))
		{
			asUINT Nested = 0;
			const asEByteCodeImageStatus NestedStatus = AddTypeRequirement(*Referenced, Nested);
			if (NestedStatus != asEByteCodeImageStatus::Succeeded) return NestedStatus;
		}
	}
	return asEByteCodeImageStatus::Succeeded;
}

asEByteCodeImageStatus asCByteCodeImageBuilder::AddPropertyRequirement(
	const asCObjectType& Owner,
	const asCObjectProperty& Property,
	asUINT& OutSlot)
{
	asSByteCodeRequirement Requirement;
	Requirement.Role = asEByteCodeRequirementRole::Property;
	Requirement.OwnerKey = Owner.GetStableKey();
	Requirement.Name = UTF8_TO_TCHAR(Property.name.AddressOf());
	Requirement.NativeOffset = Property.byteOffset;
	Requirement.NativeAddressMode = static_cast<uint8>(Property.nativeAddressMode);
	{
		asCCanonicalEncoding PropertyKey(asECanonicalIdentityDomain::Declaration);
		PropertyKey.WriteBytes(Owner.GetStableKey().GetBytes());
		PropertyKey.WriteString(Requirement.Name);
		Requirement.Key = PropertyKey.FinalizeDigest();
	}
	if (Property.type.GetTypeInfo())
	{
		Requirement.ReferencedKeys.AddUnique(Property.type.GetTypeInfo()->GetStableKey());
	}
	asEncodeByteCodeRequirementContract(Requirement, Requirement.ContractWitness);
	return AddRequirement(Requirement, OutSlot);
}

asEByteCodeImageStatus asCByteCodeImageBuilder::AddString(const FString& Value, asUINT& OutSlot)
{
	for (int32 Index = 0; Index < Strings.Num(); ++Index)
	{
		if (Strings[Index] == Value)
		{
			OutSlot = static_cast<asUINT>(Index);
			return asEByteCodeImageStatus::Succeeded;
		}
	}
	OutSlot = static_cast<asUINT>(Strings.Num());
	Strings.Add(Value);
	return asEByteCodeImageStatus::Succeeded;
}

asUINT asCByteCodeImageBuilder::NewLabel()
{
	return NextLabel++;
}

asEByteCodeImageStatus asCByteCodeImageBuilder::BeginFunction(const asSStableKey& FunctionKey, const asSStableKey& SignatureKey, asUINT FrameSize, asUINT ParameterSize)
{
	if (Current) return asEByteCodeImageStatus::InvalidArgument;
	if (FunctionKey.IsZero()) return asEByteCodeImageStatus::InvalidArgument;
	for (const asSDraftFunction& Existing : Functions)
	{
		if (Existing.Body.FunctionKey == FunctionKey) return asEByteCodeImageStatus::Duplicate;
	}
	asSDraftFunction Draft;
	Draft.Body.FunctionKey = FunctionKey;
	Draft.Body.SignatureKey = SignatureKey;
	Draft.Body.FrameSize = FrameSize;
	Draft.Body.ParameterSize = ParameterSize;
	Draft.Body.Frame.LocalDwords = FrameSize;
	Draft.Body.Frame.ParameterDwords = ParameterSize;
	Draft.bOpen = true;
	Functions.Add(MoveTemp(Draft));
	Current = &Functions.Last();
	return asEByteCodeImageStatus::Succeeded;
}

asEByteCodeImageStatus asCByteCodeImageBuilder::SetFrameDescriptor(const asSByteCodeFrameDescriptor& Frame)
{
	if (!Current) return asEByteCodeImageStatus::InvalidArgument;
	Current->Body.Frame = Frame;
	Current->Body.FrameSize = Frame.LocalDwords;
	Current->Body.ParameterSize = Frame.ParameterDwords;
	return asEByteCodeImageStatus::Succeeded;
}

asEByteCodeImageStatus asCByteCodeImageBuilder::AddCleanupAction(const asSByteCodeCleanupAction& Action)
{
	if (!Current) return asEByteCodeImageStatus::InvalidArgument;
	Current->Body.Cleanup.Add(Action);
	return asEByteCodeImageStatus::Succeeded;
}

asEByteCodeImageStatus asCByteCodeImageBuilder::AddSourceRecord(const asSByteCodeSourceRecord& Record)
{
	if (!Current) return asEByteCodeImageStatus::InvalidArgument;
	Current->Body.Source.Add(Record);
	return asEByteCodeImageStatus::Succeeded;
}

asEByteCodeImageStatus asCByteCodeImageBuilder::BindLabel(asUINT Label)
{
	if (!Current || Label == 0) return asEByteCodeImageStatus::InvalidArgument;
	if (Current->LabelToInstruction.Contains(Label)) return asEByteCodeImageStatus::Duplicate;
	Current->LabelToInstruction.Add(Label, static_cast<asUINT>(Current->Body.Instructions.Num()));
	Current->UnboundLabels.Remove(Label);
	return asEByteCodeImageStatus::Succeeded;
}

asEByteCodeImageStatus asCByteCodeImageBuilder::Emit(asEBCInstr Opcode, TConstArrayView<asSByteCodeOperand> Operands)
{
	if (!Current) return asEByteCodeImageStatus::InvalidArgument;
	asSByteCodeOpcodeInfo Info;
	if (!asByteCodeOpcodeGetInfo(Opcode, Info) || !asByteCodeOpcodeIsExecutable(Opcode))
	{
		return asEByteCodeImageStatus::UnknownOpcode;
	}
	if (Operands.Num() != static_cast<int32>(Info.OperandCount))
	{
		return asEByteCodeImageStatus::InvalidOperand;
	}
	for (int32 Index = 0; Index < Operands.Num(); ++Index)
	{
		if (Operands[Index].Kind != Info.Operands[Index] && Info.Operands[Index] != asEByteCodeOperandKind::None)
		{
			return asEByteCodeImageStatus::InvalidOperand;
		}
		if (Operands[Index].Kind == asEByteCodeOperandKind::Label)
		{
			Current->UnboundLabels.Add(Operands[Index].Label);
		}
	}
	asSByteCodeInstruction Instruction;
	Instruction.Opcode = Opcode;
	Instruction.Operands.Append(Operands.GetData(), Operands.Num());
	Current->Body.Instructions.Add(MoveTemp(Instruction));
	return asEByteCodeImageStatus::Succeeded;
}

asUINT asCByteCodeImageBuilder::GetInstructionCount() const
{
	return Current ? static_cast<asUINT>(Current->Body.Instructions.Num()) : 0;
}

asEByteCodeImageStatus asCByteCodeImageBuilder::EndFunction()
{
	if (!Current) return asEByteCodeImageStatus::Incomplete;
	if (Current->Body.Instructions.IsEmpty()) return asEByteCodeImageStatus::Incomplete;
	for (asUINT Label : Current->UnboundLabels)
	{
		if (!Current->LabelToInstruction.Contains(Label)) return asEByteCodeImageStatus::MissingLabel;
	}
	for (asSByteCodeInstruction& Instruction : Current->Body.Instructions)
	{
		for (asSByteCodeOperand& Operand : Instruction.Operands)
		{
			if (Operand.Kind == asEByteCodeOperandKind::Label)
			{
				const asUINT* Target = Current->LabelToInstruction.Find(Operand.Label);
				if (!Target) return asEByteCodeImageStatus::MissingLabel;
				Operand.Immediate = static_cast<int64>(*Target);
				Operand.Label = *Target;
			}
		}
	}
	Current->bOpen = false;
	Current = nullptr;
	return asEByteCodeImageStatus::Succeeded;
}

asEByteCodeImageStatus asCByteCodeImageBuilder::Finish(TUniquePtr<asCByteCodeImage>& OutImage)
{
	OutImage.Reset();
	if (Current) return asEByteCodeImageStatus::Incomplete;
	for (const asSDraftFunction& Function : Functions)
	{
		if (Function.bOpen || Function.Body.Instructions.IsEmpty()) return asEByteCodeImageStatus::Incomplete;
	}
	TArray<asSByteCodeRequirement> SortedRequirements = Requirements;
	SortedRequirements.Sort([](const asSByteCodeRequirement& Left, const asSByteCodeRequirement& Right)
	{
		if (Left.Role != Right.Role) return static_cast<uint8>(Left.Role) < static_cast<uint8>(Right.Role);
		return CompareKeys(Left.Key, Right.Key) < 0;
	});
	TMap<TPair<uint8, FString>, asUINT> SlotRemap;
	for (int32 Index = 0; Index < Requirements.Num(); ++Index)
	{
		const FString KeyHex = Requirements[Index].Key.ToHexString();
		for (int32 Sorted = 0; Sorted < SortedRequirements.Num(); ++Sorted)
		{
			if (SortedRequirements[Sorted].Role == Requirements[Index].Role
				&& SortedRequirements[Sorted].Key == Requirements[Index].Key)
			{
				SlotRemap.Add({static_cast<uint8>(Requirements[Index].Role), KeyHex}, static_cast<asUINT>(Sorted));
				break;
			}
		}
	}
	TArray<asSByteCodeFunctionBody> SortedFunctions;
	for (const asSDraftFunction& Function : Functions)
	{
		SortedFunctions.Add(Function.Body);
	}
	SortedFunctions.Sort([](const asSByteCodeFunctionBody& Left, const asSByteCodeFunctionBody& Right)
	{
		return CompareKeys(Left.FunctionKey, Right.FunctionKey) < 0;
	});
	auto RemapSlot = [&](asEByteCodeOperandKind Kind, asUINT Slot) -> asUINT
	{
		asEByteCodeRequirementRole Role = asEByteCodeRequirementRole::Type;
		if (Kind == asEByteCodeOperandKind::FunctionSlot) Role = asEByteCodeRequirementRole::Function;
		else if (Kind == asEByteCodeOperandKind::PropertySlot) Role = asEByteCodeRequirementRole::Property;
		else if (Kind == asEByteCodeOperandKind::GlobalSlot) Role = asEByteCodeRequirementRole::Global;
		else if (Kind == asEByteCodeOperandKind::StringSlot) return Slot;
		if (Slot >= static_cast<asUINT>(Requirements.Num())) return Slot;
		const FString KeyHex = Requirements[Slot].Key.ToHexString();
		if (const asUINT* Found = SlotRemap.Find({static_cast<uint8>(Role), KeyHex})) return *Found;
		return Slot;
	};
	TArray<FString> SortedStrings = Strings;
	SortedStrings.Sort([](const FString& Left, const FString& Right)
	{
		const FTCHARToUTF8 LeftUtf8(*Left, Left.Len());
		const FTCHARToUTF8 RightUtf8(*Right, Right.Len());
		const int32 CompareLength = LeftUtf8.Length() < RightUtf8.Length() ? LeftUtf8.Length() : RightUtf8.Length();
		const int Cmp = FMemory::Memcmp(LeftUtf8.Get(), RightUtf8.Get(), CompareLength);
		if (Cmp != 0) return Cmp < 0;
		return LeftUtf8.Length() < RightUtf8.Length();
	});
	TMap<int32, asUINT> StringRemap;
	for (int32 Index = 0; Index < Strings.Num(); ++Index)
	{
		for (int32 Sorted = 0; Sorted < SortedStrings.Num(); ++Sorted)
		{
			if (Strings[Index] == SortedStrings[Sorted])
			{
				StringRemap.Add(Index, static_cast<asUINT>(Sorted));
				break;
			}
		}
	}
	for (asSByteCodeFunctionBody& Function : SortedFunctions)
	{
		for (asSByteCodeInstruction& Instruction : Function.Instructions)
		{
			for (asSByteCodeOperand& Operand : Instruction.Operands)
			{
				if (Operand.Kind == asEByteCodeOperandKind::TypeSlot
					|| Operand.Kind == asEByteCodeOperandKind::FunctionSlot
					|| Operand.Kind == asEByteCodeOperandKind::PropertySlot
					|| Operand.Kind == asEByteCodeOperandKind::GlobalSlot)
				{
					Operand.Slot = RemapSlot(Operand.Kind, Operand.Slot);
				}
				else if (Operand.Kind == asEByteCodeOperandKind::StringSlot)
				{
					if (const asUINT* Found = StringRemap.Find(static_cast<int32>(Operand.Slot)))
					{
						Operand.Slot = *Found;
					}
				}
			}
		}
		for (asSByteCodeSourceRecord& Source : Function.Source)
		{
			if (const asUINT* Found = StringRemap.Find(static_cast<int32>(Source.FileStringSlot)))
			{
				Source.FileStringSlot = *Found;
			}
		}
		for (asSByteCodeCleanupAction& Action : Function.Cleanup)
		{
			Action.TypeSlot = RemapSlot(asEByteCodeOperandKind::TypeSlot, Action.TypeSlot);
		}
	}
	OutImage = MakeUnique<asCByteCodeImage>();
	OutImage->Requirements = MoveTemp(SortedRequirements);
	OutImage->Functions = MoveTemp(SortedFunctions);
	OutImage->Strings = MoveTemp(SortedStrings);
	OutImage->FormatVersion = 3;
	return asEByteCodeImageStatus::Succeeded;
}

END_AS_NAMESPACE
