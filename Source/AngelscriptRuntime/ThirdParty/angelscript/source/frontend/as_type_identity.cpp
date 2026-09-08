/* Maintained Unreal AngelScript fork — canonical stable identity ownership. */

#include "frontend/as_type_identity.h"

#include "Hash/Blake3.h"
#include "HAL/UnrealMemory.h"
#include "Misc/ScopeLock.h"

BEGIN_AS_NAMESPACE

namespace
	{
		constexpr uint16 LegalTypeUseQualifierMask =
			static_cast<uint16>(asETypeUseQualifiers::Const)
			| static_cast<uint16>(asETypeUseQualifiers::Reference)
			| static_cast<uint16>(asETypeUseQualifiers::Handle)
			| static_cast<uint16>(asETypeUseQualifiers::Nullable);
		constexpr uint16 LegalFunctionModifierMask =
			static_cast<uint16>(asEFunctionIdentityModifiers::ConstMethod);
		constexpr uint16 LegalFunctionDefinitionModifierMask =
			static_cast<uint16>(asEFunctionDefinitionModifiers::Static)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::Final)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::Override)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::Property)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::Explicit)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::Mixin)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::Local)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::AcceptTemporary)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::ExternalImplicitThis)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::NoDiscard)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::AllowDiscard)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::Generated)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::Deprecated)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::Defaults)
			| static_cast<uint16>(asEFunctionDefinitionModifiers::UnsafeDuringConstruction);

		bool HasValidStablePath(TConstArrayView<FString> Path)
		{
			for (const FString& Segment : Path)
			{
				if (Segment.IsEmpty())
				{
					return false;
				}
			}
			return true;
		}

		bool IsConversionFunctionName(FStringView Name)
		{
			return Name.Equals(TEXT("opConv"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("opImplConv"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("opCast"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("opImplCast"), ESearchCase::CaseSensitive);
		}

		bool EqualBytes(TConstArrayView<uint8> Left, TConstArrayView<uint8> Right)
		{
			return Left.Num() == Right.Num()
				&& (Left.IsEmpty()
					|| FMemory::Memcmp(Left.GetData(), Right.GetData(), Left.Num()) == 0);
		}

		asSStableKey HashDescription(TConstArrayView<uint8> Description)
		{
			const FBlake3Hash Hash = FBlake3::HashBuffer(
				Description.GetData(), static_cast<uint64>(Description.Num()));
			asSStableKey Result;
			const bool bCreated = asSStableKey::TryCreate(
				MakeArrayView(Hash.GetBytes(), asSStableKey::ByteCount), Result);
			check(bCreated);
			return Result;
		}

		asETypeIdentityStatus FinishEncoding(
			const asCCanonicalEncoding& Encoding,
			TArray<uint8>& OutDescription)
		{
			if (Encoding.GetBytes().Num()
				> static_cast<int32>(asCStableIdentityRegistry::MaxCanonicalDescriptionBytes))
			{
				return asETypeIdentityStatus::BudgetExceeded;
			}
			OutDescription.Append(Encoding.GetBytes());
			return asETypeIdentityStatus::Succeeded;
		}

		asETypeIdentityStatus EncodeNominalIdentity(
			const asSNominalTypeDescriptor& Descriptor,
			TArray<uint8>& OutDescription)
		{
			OutDescription.Reset();
			if (Descriptor.Origin == asETypeDeclOrigin::Local
				|| Descriptor.Origin == asETypeDeclOrigin::Anonymous
				|| Descriptor.Origin == asETypeDeclOrigin::Recovery)
			{
				return asETypeIdentityStatus::NonCacheable;
			}
			if (Descriptor.NamespacePath.Num()
					> static_cast<int32>(asCStableIdentityRegistry::MaxDescriptorEdges)
				|| Descriptor.GenericArity > asCStableIdentityRegistry::MaxDescriptorEdges)
			{
				return asETypeIdentityStatus::BudgetExceeded;
			}
			if (Descriptor.StableScope.IsEmpty()
				|| Descriptor.Name.IsEmpty()
				|| !HasValidStablePath(Descriptor.NamespacePath)
				|| Descriptor.Kind <= asETypeDeclKind::Invalid
				|| Descriptor.Kind > asETypeDeclKind::Event
				|| Descriptor.Origin > asETypeDeclOrigin::StableGenerated
				|| (Descriptor.Origin == asETypeDeclOrigin::StableGenerated
					&& Descriptor.StableGeneratorIdentity.IsEmpty()))
			{
				return asETypeIdentityStatus::InvalidDescriptor;
			}

			asCCanonicalEncoding Encoding(asECanonicalIdentityDomain::TypeDecl);
			Encoding.WriteString(Descriptor.StableScope);
			Encoding.WriteStringSequence(Descriptor.NamespacePath);
			Encoding.WriteBytes(Descriptor.Owner.GetBytes());
			Encoding.WriteUInt8(static_cast<uint8>(Descriptor.Kind));
			Encoding.WriteString(Descriptor.Name);
			Encoding.WriteUInt32(Descriptor.GenericArity);
			Encoding.WriteUInt8(static_cast<uint8>(Descriptor.Origin));
			Encoding.WriteString(Descriptor.StableGeneratorIdentity);
			return FinishEncoding(Encoding, OutDescription);
		}

		asETypeIdentityStatus EncodeTypeUseIdentity(
			const asSStableTypeUseDescriptor& Descriptor,
			TArray<uint8>& OutDescription)
		{
			OutDescription.Reset();
			const uint16 QualifierBits = static_cast<uint16>(Descriptor.Qualifiers);
			if ((QualifierBits & ~LegalTypeUseQualifierMask) != 0)
			{
				return asETypeIdentityStatus::InvalidRole;
			}
			if ((QualifierBits & static_cast<uint16>(asETypeUseQualifiers::Nullable)) != 0
				&& (QualifierBits & static_cast<uint16>(asETypeUseQualifiers::Handle)) == 0)
			{
				return asETypeIdentityStatus::InvalidDescriptor;
			}
			if (Descriptor.Arguments.Num()
					> static_cast<int32>(asCStableIdentityRegistry::MaxDescriptorEdges)
				|| Descriptor.Parameters.Num()
					> static_cast<int32>(asCStableIdentityRegistry::MaxDescriptorEdges))
			{
				return asETypeIdentityStatus::BudgetExceeded;
			}

			if (Descriptor.Kind == asETypeUseKind::Primitive)
			{
				if (Descriptor.Primitive <= asEPrimitiveType::Invalid
					|| Descriptor.Primitive > asEPrimitiveType::Float64
					|| !Descriptor.Declaration.IsZero()
					|| !Descriptor.Arguments.IsEmpty()
					|| !Descriptor.ReturnType.IsZero()
					|| !Descriptor.Parameters.IsEmpty())
				{
					return asETypeIdentityStatus::InvalidDescriptor;
				}
			}
			else if (Descriptor.Kind == asETypeUseKind::Nominal)
			{
				if (Descriptor.Primitive != asEPrimitiveType::Invalid
					|| Descriptor.Declaration.IsZero()
					|| !Descriptor.ReturnType.IsZero()
					|| !Descriptor.Parameters.IsEmpty())
				{
					return asETypeIdentityStatus::InvalidDescriptor;
				}
			}
			else if (Descriptor.Kind == asETypeUseKind::Callable)
			{
				if (Descriptor.Primitive != asEPrimitiveType::Invalid
					|| !Descriptor.Declaration.IsZero()
					|| !Descriptor.Arguments.IsEmpty()
					|| Descriptor.ReturnType.IsZero())
				{
					return asETypeIdentityStatus::InvalidDescriptor;
				}
			}
			else
			{
				return asETypeIdentityStatus::InvalidDescriptor;
			}

			for (const asSStableKey& Argument : Descriptor.Arguments)
			{
				if (Argument.IsZero())
				{
					return asETypeIdentityStatus::InvalidRecord;
				}
			}
			for (const asSStableParameter& Parameter : Descriptor.Parameters)
			{
				if (Parameter.PassingMode > asEParameterPassingMode::InOut)
				{
					return asETypeIdentityStatus::InvalidRole;
				}
				if (Parameter.Type.IsZero())
				{
					return asETypeIdentityStatus::InvalidRecord;
				}
			}

			asCCanonicalEncoding Encoding(asECanonicalIdentityDomain::TypeUse);
			Encoding.WriteUInt8(static_cast<uint8>(Descriptor.Kind));
			Encoding.WriteUInt16(QualifierBits);
			Encoding.WriteUInt8(static_cast<uint8>(Descriptor.Primitive));
			Encoding.WriteBytes(Descriptor.Declaration.GetBytes());
			Encoding.WriteUInt32(static_cast<uint32>(Descriptor.Arguments.Num()));
			for (const asSStableKey& Argument : Descriptor.Arguments)
			{
				Encoding.WriteBytes(Argument.GetBytes());
			}
			Encoding.WriteBytes(Descriptor.ReturnType.GetBytes());
			Encoding.WriteUInt32(static_cast<uint32>(Descriptor.Parameters.Num()));
			for (const asSStableParameter& Parameter : Descriptor.Parameters)
			{
				Encoding.WriteBytes(Parameter.Type.GetBytes());
				Encoding.WriteUInt8(static_cast<uint8>(Parameter.PassingMode));
			}
			return FinishEncoding(Encoding, OutDescription);
		}

		asETypeIdentityStatus EncodeFunctionIdentity(
			const asSFunctionIdentityDescriptor& Descriptor,
			TArray<uint8>& OutDescription)
		{
			OutDescription.Reset();
			const bool ClassDefault = Descriptor.Kind == asEFunctionIdentityKind::ClassDefault;
			const bool Conversion = Descriptor.Kind == asEFunctionIdentityKind::Conversion;
			const uint16 ModifierBits = static_cast<uint16>(Descriptor.Modifiers);
			const uint16 DefinitionModifierBits =
				static_cast<uint16>(Descriptor.DefinitionModifiers);
			if (Descriptor.StableScope.IsEmpty()
				|| (ClassDefault ? !Descriptor.Name.IsEmpty() : Descriptor.Name.IsEmpty())
				|| !HasValidStablePath(Descriptor.NamespacePath)
				|| Descriptor.NamespacePath.Num()
					> static_cast<int32>(asCStableIdentityRegistry::MaxDescriptorEdges)
				|| Descriptor.Kind <= asEFunctionIdentityKind::Invalid
				|| Descriptor.Kind > asEFunctionIdentityKind::Conversion
				|| Descriptor.Kind == asEFunctionIdentityKind::ReservedAnonymousFunction
				|| Descriptor.GenericArity > asCStableIdentityRegistry::MaxDescriptorEdges
				|| Descriptor.ReturnType.IsZero()
				|| Descriptor.Parameters.Num()
					> static_cast<int32>(asCStableIdentityRegistry::MaxDescriptorEdges))
			{
				return asETypeIdentityStatus::InvalidDescriptor;
			}
			if ((ModifierBits & ~LegalFunctionModifierMask) != 0)
			{
				return asETypeIdentityStatus::InvalidRole;
			}
			if ((DefinitionModifierBits & ~LegalFunctionDefinitionModifierMask) != 0)
			{
				return asETypeIdentityStatus::InvalidDescriptor;
			}
			if ((Descriptor.Kind == asEFunctionIdentityKind::Global)
				!= Descriptor.Owner.IsZero())
			{
				return asETypeIdentityStatus::InvalidOwner;
			}
			if ((ClassDefault && (Descriptor.GenericArity || ModifierBits || DefinitionModifierBits)) ||
				(ClassDefault && !Descriptor.Parameters.IsEmpty()) ||
				(Conversion && (!IsConversionFunctionName(Descriptor.Name)
					|| !Descriptor.Parameters.IsEmpty()
					|| Descriptor.ConversionDestination.IsZero()
					|| Descriptor.ConversionDestination != Descriptor.ReturnType)) ||
				(!Conversion && !Descriptor.ConversionDestination.IsZero()))
			{
				return asETypeIdentityStatus::InvalidDescriptor;
			}
			for (const asSStableParameter& Parameter : Descriptor.Parameters)
			{
				if (Parameter.PassingMode > asEParameterPassingMode::InOut)
				{
					return asETypeIdentityStatus::InvalidRole;
				}
				if (Parameter.Type.IsZero())
				{
					return asETypeIdentityStatus::InvalidRecord;
				}
			}

			asCCanonicalEncoding Encoding(asECanonicalIdentityDomain::Function);
			Encoding.WriteString(Descriptor.StableScope);
			Encoding.WriteStringSequence(Descriptor.NamespacePath);
			Encoding.WriteBytes(Descriptor.Owner.GetBytes());
			Encoding.WriteUInt8(static_cast<uint8>(Descriptor.Kind));
			Encoding.WriteString(Descriptor.Name);
			Encoding.WriteUInt32(Descriptor.GenericArity);
			Encoding.WriteUInt16(ModifierBits);
			Encoding.WriteUInt32(static_cast<uint32>(Descriptor.Parameters.Num()));
			for (const asSStableParameter& Parameter : Descriptor.Parameters)
			{
				Encoding.WriteBytes(Parameter.Type.GetBytes());
				Encoding.WriteUInt8(static_cast<uint8>(Parameter.PassingMode));
			}
			if (Conversion)
			{
				// This conditional append preserves every pre-Conversion function key.
				Encoding.WriteBytes(Descriptor.ConversionDestination.GetBytes());
			}
			return FinishEncoding(Encoding, OutDescription);
		}

		asETypeIdentityStatus EncodeDeclarationIdentity(
			const asSDeclarationIdentityDescriptor& Descriptor,
			TArray<uint8>& OutDescription)
		{
			OutDescription.Reset();
			if (Descriptor.StableScope.IsEmpty()
				|| Descriptor.Kind <= asEDeclarationIdentityKind::Invalid
				|| Descriptor.Kind > asEDeclarationIdentityKind::EnumConstant)
			{
				return asETypeIdentityStatus::InvalidDescriptor;
			}
			const bool bParameter = Descriptor.Kind == asEDeclarationIdentityKind::Parameter;
			if (bParameter)
			{
				if (!Descriptor.Name.IsEmpty()
					|| Descriptor.Ordinal == asSDeclarationIdentityDescriptor::InvalidOrdinal
					|| Descriptor.Ordinal >= asCStableIdentityRegistry::MaxDescriptorEdges)
				{
					return asETypeIdentityStatus::InvalidDescriptor;
				}
			}
			else if (Descriptor.Name.IsEmpty()
				|| Descriptor.Ordinal != asSDeclarationIdentityDescriptor::InvalidOrdinal)
			{
				return asETypeIdentityStatus::InvalidDescriptor;
			}

			asCCanonicalEncoding Encoding(asECanonicalIdentityDomain::Declaration);
			Encoding.WriteString(Descriptor.StableScope);
			Encoding.WriteBytes(Descriptor.Owner.GetBytes());
			Encoding.WriteUInt8(static_cast<uint8>(Descriptor.Kind));
			Encoding.WriteString(Descriptor.Name);
			Encoding.WriteUInt32(Descriptor.Ordinal);
			return FinishEncoding(Encoding, OutDescription);
		}
	}

	asCStableIdentityRegistry::asCStableIdentityRegistry() = default;

	asCStableIdentityRegistry::asCStableIdentityRegistry(
		const asIStableKeyDigestProvider& InDigestProvider)
		: DigestProvider(&InDigestProvider)
	{
	}

	asETypeIdentityStatus asCStableIdentityRegistry::InternNominalType(
		const asSNominalTypeDescriptor& Descriptor,
		asSStableKey& OutKey)
	{
		OutKey = {};
		TArray<uint8> Description;
		const asETypeIdentityStatus EncodeStatus =
			EncodeNominalIdentity(Descriptor, Description);
		if (EncodeStatus != asETypeIdentityStatus::Succeeded)
		{
			return EncodeStatus;
		}

		FScopeLock Lock(&Mutex);
		const asETypeIdentityStatus OwnerStatus = ValidateNominalOwnerLocked(Descriptor);
		if (OwnerStatus != asETypeIdentityStatus::Succeeded)
		{
			return OwnerStatus;
		}
		asSCanonicalIdentityRecord Candidate;
		Candidate.Role = asEStableIdentityRole::NominalType;
		Candidate.CanonicalDescription = MoveTemp(Description);
		Candidate.NominalType = Descriptor;
		return InternCanonicalLocked(MoveTemp(Candidate), OutKey);
	}

	asETypeIdentityStatus asCStableIdentityRegistry::InternTypeUse(
		const asSStableTypeUseDescriptor& Descriptor,
		asSStableKey& OutKey)
	{
		OutKey = {};
		TArray<uint8> Description;
		const asETypeIdentityStatus EncodeStatus =
			EncodeTypeUseIdentity(Descriptor, Description);
		if (EncodeStatus != asETypeIdentityStatus::Succeeded)
		{
			return EncodeStatus;
		}

		FScopeLock Lock(&Mutex);
		const asETypeIdentityStatus EdgeStatus = ValidateTypeUseEdgesLocked(Descriptor);
		if (EdgeStatus != asETypeIdentityStatus::Succeeded)
		{
			return EdgeStatus;
		}
		asSCanonicalIdentityRecord Candidate;
		Candidate.Role = asEStableIdentityRole::TypeUse;
		Candidate.CanonicalDescription = MoveTemp(Description);
		Candidate.TypeUse = Descriptor;
		return InternCanonicalLocked(MoveTemp(Candidate), OutKey);
	}

	asETypeIdentityStatus asCStableIdentityRegistry::InternQualifiedTypeUse(
		const asSStableKey& BaseType,
		asETypeUseQualifiers AdditionalQualifiers,
		asSStableKey& OutKey)
	{
		OutKey = {};
		asSStableTypeUseDescriptor Descriptor;
		const asETypeIdentityStatus ResolveStatus = ResolveTypeUse(BaseType, Descriptor);
		if (ResolveStatus != asETypeIdentityStatus::Succeeded)
		{
			return ResolveStatus;
		}
		if (AdditionalQualifiers == asETypeUseQualifiers::None)
		{
			OutKey = BaseType;
			return asETypeIdentityStatus::Succeeded;
		}
		Descriptor.Qualifiers = static_cast<asETypeUseQualifiers>(
			static_cast<uint16>(Descriptor.Qualifiers)
			| static_cast<uint16>(AdditionalQualifiers));
		return InternTypeUse(Descriptor, OutKey);
	}

	asETypeIdentityStatus asCStableIdentityRegistry::InternFunction(
		const asSFunctionIdentityDescriptor& Descriptor,
		asSStableKey& OutKey)
	{
		OutKey = {};
		TArray<uint8> Description;
		const asETypeIdentityStatus EncodeStatus =
			EncodeFunctionIdentity(Descriptor, Description);
		if (EncodeStatus != asETypeIdentityStatus::Succeeded)
		{
			return EncodeStatus;
		}

		FScopeLock Lock(&Mutex);
		const asETypeIdentityStatus EdgeStatus = ValidateFunctionEdgesLocked(Descriptor);
		if (EdgeStatus != asETypeIdentityStatus::Succeeded)
		{
			return EdgeStatus;
		}
		asSCanonicalIdentityRecord Candidate;
		Candidate.Role = asEStableIdentityRole::Function;
		Candidate.CanonicalDescription = MoveTemp(Description);
		Candidate.ReturnType = Descriptor.ReturnType;
		Candidate.Function = Descriptor;
		return InternCanonicalLocked(MoveTemp(Candidate), OutKey);
	}

	asETypeIdentityStatus asCStableIdentityRegistry::InternDeclaration(
		const asSDeclarationIdentityDescriptor& Descriptor,
		asSStableKey& OutKey)
	{
		OutKey = {};
		TArray<uint8> Description;
		const asETypeIdentityStatus EncodeStatus =
			EncodeDeclarationIdentity(Descriptor, Description);
		if (EncodeStatus != asETypeIdentityStatus::Succeeded)
		{
			return EncodeStatus;
		}

		FScopeLock Lock(&Mutex);
		const asETypeIdentityStatus OwnerStatus =
			ValidateDeclarationOwnerLocked(Descriptor);
		if (OwnerStatus != asETypeIdentityStatus::Succeeded)
		{
			return OwnerStatus;
		}
		asSCanonicalIdentityRecord Candidate;
		Candidate.Role = asEStableIdentityRole::Declaration;
		Candidate.CanonicalDescription = MoveTemp(Description);
		Candidate.Declaration = Descriptor;
		return InternCanonicalLocked(MoveTemp(Candidate), OutKey);
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ValidateNominalType(
		const asSStableKey& Key,
		const asSNominalTypeDescriptor& Descriptor) const
	{
		TArray<uint8> Description;
		const asETypeIdentityStatus EncodeStatus =
			EncodeNominalIdentity(Descriptor, Description);
		if (EncodeStatus != asETypeIdentityStatus::Succeeded)
		{
			return EncodeStatus;
		}
		FScopeLock Lock(&Mutex);
		const asETypeIdentityStatus OwnerStatus = ValidateNominalOwnerLocked(Descriptor);
		if (OwnerStatus != asETypeIdentityStatus::Succeeded)
		{
			return OwnerStatus;
		}
		return ValidateCanonicalLocked(
			asEStableIdentityRole::NominalType, Key, Description, {});
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ValidateTypeUse(
		const asSStableKey& Key,
		const asSStableTypeUseDescriptor& Descriptor) const
	{
		TArray<uint8> Description;
		const asETypeIdentityStatus EncodeStatus =
			EncodeTypeUseIdentity(Descriptor, Description);
		if (EncodeStatus != asETypeIdentityStatus::Succeeded)
		{
			return EncodeStatus;
		}
		FScopeLock Lock(&Mutex);
		const asETypeIdentityStatus EdgeStatus = ValidateTypeUseEdgesLocked(Descriptor);
		if (EdgeStatus != asETypeIdentityStatus::Succeeded)
		{
			return EdgeStatus;
		}
		return ValidateCanonicalLocked(
			asEStableIdentityRole::TypeUse, Key, Description, {});
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ValidateFunction(
		const asSStableKey& Key,
		const asSFunctionIdentityDescriptor& Descriptor) const
	{
		TArray<uint8> Description;
		const asETypeIdentityStatus EncodeStatus =
			EncodeFunctionIdentity(Descriptor, Description);
		if (EncodeStatus != asETypeIdentityStatus::Succeeded)
		{
			return EncodeStatus;
		}
		FScopeLock Lock(&Mutex);
		const asETypeIdentityStatus EdgeStatus = ValidateFunctionEdgesLocked(Descriptor);
		if (EdgeStatus != asETypeIdentityStatus::Succeeded)
		{
			return EdgeStatus;
		}
		const asETypeIdentityStatus IdentityStatus = ValidateCanonicalLocked(
			asEStableIdentityRole::Function, Key, Description, Descriptor.ReturnType);
		if (IdentityStatus != asETypeIdentityStatus::Succeeded)
		{
			return IdentityStatus;
		}
		const asSCanonicalIdentityRecord& Existing =
			IdentityRecords[*IdentityRecordByKey.Find(Key)];
		return Existing.Function.DefinitionModifiers == Descriptor.DefinitionModifiers
			? asETypeIdentityStatus::Succeeded
			: asETypeIdentityStatus::DescriptorMismatch;
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ValidateDeclaration(
		const asSStableKey& Key,
		const asSDeclarationIdentityDescriptor& Descriptor) const
	{
		TArray<uint8> Description;
		const asETypeIdentityStatus EncodeStatus =
			EncodeDeclarationIdentity(Descriptor, Description);
		if (EncodeStatus != asETypeIdentityStatus::Succeeded)
		{
			return EncodeStatus;
		}
		FScopeLock Lock(&Mutex);
		const asETypeIdentityStatus OwnerStatus =
			ValidateDeclarationOwnerLocked(Descriptor);
		if (OwnerStatus != asETypeIdentityStatus::Succeeded)
		{
			return OwnerStatus;
		}
		return ValidateCanonicalLocked(
			asEStableIdentityRole::Declaration, Key, Description, {});
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ResolveNominalType(
		const asSStableKey& Key,
		asSNominalTypeDescriptor& OutDescriptor) const
	{
		OutDescriptor = {};
		FScopeLock Lock(&Mutex);
		const asETypeIdentityStatus RoleStatus = ValidateIdentityRoleLocked(
			Key, asEStableIdentityRole::NominalType);
		if (RoleStatus != asETypeIdentityStatus::Succeeded)
		{
			return RoleStatus;
		}
		OutDescriptor = IdentityRecords[*IdentityRecordByKey.Find(Key)].NominalType;
		return asETypeIdentityStatus::Succeeded;
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ResolveTypeUse(
		const asSStableKey& Key,
		asSStableTypeUseDescriptor& OutDescriptor) const
	{
		OutDescriptor = {};
		FScopeLock Lock(&Mutex);
		const asETypeIdentityStatus RoleStatus = ValidateIdentityRoleLocked(
			Key, asEStableIdentityRole::TypeUse);
		if (RoleStatus != asETypeIdentityStatus::Succeeded)
		{
			return RoleStatus;
		}
		OutDescriptor = IdentityRecords[*IdentityRecordByKey.Find(Key)].TypeUse;
		return asETypeIdentityStatus::Succeeded;
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ResolveFunction(
		const asSStableKey& Key,
		asSFunctionIdentityDescriptor& OutDescriptor) const
	{
		OutDescriptor = {};
		FScopeLock Lock(&Mutex);
		const asETypeIdentityStatus RoleStatus = ValidateIdentityRoleLocked(
			Key, asEStableIdentityRole::Function);
		if (RoleStatus != asETypeIdentityStatus::Succeeded)
		{
			return RoleStatus;
		}
		OutDescriptor = IdentityRecords[*IdentityRecordByKey.Find(Key)].Function;
		return asETypeIdentityStatus::Succeeded;
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ResolveDeclaration(
		const asSStableKey& Key,
		asSDeclarationIdentityDescriptor& OutDescriptor) const
	{
		OutDescriptor = {};
		FScopeLock Lock(&Mutex);
		const asETypeIdentityStatus RoleStatus = ValidateIdentityRoleLocked(
			Key, asEStableIdentityRole::Declaration);
		if (RoleStatus != asETypeIdentityStatus::Succeeded)
		{
			return RoleStatus;
		}
		OutDescriptor = IdentityRecords[*IdentityRecordByKey.Find(Key)].Declaration;
		return asETypeIdentityStatus::Succeeded;
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ResolveIdentityRole(
		const asSStableKey& Key,
		asEStableIdentityRole& OutRole) const
	{
		OutRole = asEStableIdentityRole::Invalid;
		FScopeLock Lock(&Mutex);
		if (Key.IsZero())
		{
			return asETypeIdentityStatus::InvalidRecord;
		}
		const asUINT* Index = IdentityRecordByKey.Find(Key);
		if (Index == nullptr || !IdentityRecords.IsValidIndex(static_cast<int32>(*Index)))
		{
			return asETypeIdentityStatus::InvalidRecord;
		}
		OutRole = IdentityRecords[*Index].Role;
		return asETypeIdentityStatus::Succeeded;
	}

	asUINT asCStableIdentityRegistry::GetIdentityCount() const
	{
		FScopeLock Lock(&Mutex);
		return static_cast<asUINT>(IdentityRecords.Num());
	}

	asETypeIdentityStatus asCStableIdentityRegistry::InternCanonicalLocked(
		asSCanonicalIdentityRecord&& Candidate,
		asSStableKey& OutKey)
	{
		OutKey = {};
		if (Candidate.Role <= asEStableIdentityRole::Invalid
			|| Candidate.Role > asEStableIdentityRole::Declaration
			|| Candidate.CanonicalDescription.IsEmpty())
		{
			return asETypeIdentityStatus::InvalidDescriptor;
		}
		const asSStableKey Key = DigestProvider != nullptr
			? DigestProvider->ComputeDigest(Candidate.CanonicalDescription)
			: HashDescription(Candidate.CanonicalDescription);
		if (Key.IsZero())
		{
			return asETypeIdentityStatus::InvalidDescriptor;
		}

		if (const asUINT* ExistingIndex = IdentityRecordByKey.Find(Key))
		{
			if (!IdentityRecords.IsValidIndex(static_cast<int32>(*ExistingIndex)))
			{
				return asETypeIdentityStatus::InvalidRecord;
			}
			const asSCanonicalIdentityRecord& Existing = IdentityRecords[*ExistingIndex];
			if (Existing.Role != Candidate.Role
				|| !EqualBytes(
					MakeArrayView(Existing.CanonicalDescription),
					MakeArrayView(Candidate.CanonicalDescription)))
			{
				return asETypeIdentityStatus::Collision;
			}
			if (Candidate.Role == asEStableIdentityRole::Function
				&& Existing.ReturnType != Candidate.ReturnType)
			{
				return asETypeIdentityStatus::ReturnTypeConflict;
			}
			OutKey = Key;
			return asETypeIdentityStatus::Succeeded;
		}

		Candidate.Key = Key;
		IdentityRecords.Add(MoveTemp(Candidate));
		IdentityRecordByKey.Add(Key, static_cast<asUINT>(IdentityRecords.Num() - 1));
		OutKey = Key;
		return asETypeIdentityStatus::Succeeded;
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ValidateCanonicalLocked(
		asEStableIdentityRole Role,
		const asSStableKey& Key,
		TConstArrayView<uint8> CanonicalDescription,
		const asSStableKey& ReturnType) const
	{
		const asETypeIdentityStatus RoleStatus = ValidateIdentityRoleLocked(Key, Role);
		if (RoleStatus != asETypeIdentityStatus::Succeeded)
		{
			return RoleStatus;
		}
		const asSCanonicalIdentityRecord& Existing =
			IdentityRecords[*IdentityRecordByKey.Find(Key)];
		if (!EqualBytes(
			MakeArrayView(Existing.CanonicalDescription), CanonicalDescription))
		{
			return asETypeIdentityStatus::DescriptorMismatch;
		}
		if (Role == asEStableIdentityRole::Function
			&& Existing.ReturnType != ReturnType)
		{
			return asETypeIdentityStatus::ReturnTypeConflict;
		}
		return asETypeIdentityStatus::Succeeded;
	}

	bool asCStableIdentityRegistry::HasIdentityRoleLocked(
		const asSStableKey& Key,
		asEStableIdentityRole Role) const
	{
		const asUINT* Index = IdentityRecordByKey.Find(Key);
		return Index != nullptr
			&& IdentityRecords.IsValidIndex(static_cast<int32>(*Index))
			&& IdentityRecords[*Index].Role == Role;
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ValidateIdentityRoleLocked(
		const asSStableKey& Key,
		asEStableIdentityRole Role) const
	{
		if (Key.IsZero())
		{
			return asETypeIdentityStatus::InvalidRecord;
		}
		const asUINT* Index = IdentityRecordByKey.Find(Key);
		if (Index == nullptr || !IdentityRecords.IsValidIndex(static_cast<int32>(*Index)))
		{
			return asETypeIdentityStatus::InvalidRecord;
		}
		return IdentityRecords[*Index].Role == Role
			? asETypeIdentityStatus::Succeeded
			: asETypeIdentityStatus::InvalidRole;
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ValidateNominalOwnerLocked(
		const asSNominalTypeDescriptor& Descriptor) const
	{
		if (Descriptor.Owner.IsZero())
		{
			return asETypeIdentityStatus::Succeeded;
		}
		return HasIdentityRoleLocked(Descriptor.Owner, asEStableIdentityRole::NominalType)
			? asETypeIdentityStatus::Succeeded
			: asETypeIdentityStatus::InvalidOwner;
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ValidateTypeUseEdgesLocked(
		const asSStableTypeUseDescriptor& Descriptor) const
	{
		if (Descriptor.Kind == asETypeUseKind::Nominal)
		{
			const asETypeIdentityStatus DeclarationStatus = ValidateIdentityRoleLocked(
				Descriptor.Declaration, asEStableIdentityRole::NominalType);
			if (DeclarationStatus != asETypeIdentityStatus::Succeeded)
			{
				return DeclarationStatus;
			}
		}
		for (const asSStableKey& Argument : Descriptor.Arguments)
		{
			const asETypeIdentityStatus ArgumentStatus = ValidateIdentityRoleLocked(
				Argument, asEStableIdentityRole::TypeUse);
			if (ArgumentStatus != asETypeIdentityStatus::Succeeded)
			{
				return ArgumentStatus;
			}
		}
		if (Descriptor.Kind == asETypeUseKind::Callable)
		{
			const asETypeIdentityStatus ReturnStatus = ValidateIdentityRoleLocked(
				Descriptor.ReturnType, asEStableIdentityRole::TypeUse);
			if (ReturnStatus != asETypeIdentityStatus::Succeeded)
			{
				return ReturnStatus;
			}
		}
		for (const asSStableParameter& Parameter : Descriptor.Parameters)
		{
			const asETypeIdentityStatus ParameterStatus = ValidateIdentityRoleLocked(
				Parameter.Type, asEStableIdentityRole::TypeUse);
			if (ParameterStatus != asETypeIdentityStatus::Succeeded)
			{
				return ParameterStatus;
			}
		}
		return asETypeIdentityStatus::Succeeded;
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ValidateFunctionEdgesLocked(
		const asSFunctionIdentityDescriptor& Descriptor) const
	{
		if (Descriptor.Kind != asEFunctionIdentityKind::Global
			&& !HasIdentityRoleLocked(Descriptor.Owner, asEStableIdentityRole::NominalType))
		{
			return asETypeIdentityStatus::InvalidOwner;
		}
		const asETypeIdentityStatus ReturnStatus = ValidateIdentityRoleLocked(
			Descriptor.ReturnType, asEStableIdentityRole::TypeUse);
		if (ReturnStatus != asETypeIdentityStatus::Succeeded)
		{
			return ReturnStatus;
		}
		if (Descriptor.Kind == asEFunctionIdentityKind::ClassDefault)
		{
			const auto& Return = IdentityRecords[*IdentityRecordByKey.Find(Descriptor.ReturnType)].TypeUse;
			if (Return.Kind != asETypeUseKind::Primitive || Return.Primitive != asEPrimitiveType::Void || Return.Qualifiers != asETypeUseQualifiers::None) return asETypeIdentityStatus::InvalidDescriptor;
		}
		if (Descriptor.Kind == asEFunctionIdentityKind::Conversion)
		{
			const asETypeIdentityStatus DestinationStatus = ValidateIdentityRoleLocked(
				Descriptor.ConversionDestination, asEStableIdentityRole::TypeUse);
			if (DestinationStatus != asETypeIdentityStatus::Succeeded)
			{
				return DestinationStatus;
			}
			const auto& Destination = IdentityRecords[
				*IdentityRecordByKey.Find(Descriptor.ConversionDestination)].TypeUse;
			if (Destination.Kind == asETypeUseKind::Primitive
				&& Destination.Primitive == asEPrimitiveType::Void)
			{
				return asETypeIdentityStatus::InvalidDescriptor;
			}
		}
		for (const asSStableParameter& Parameter : Descriptor.Parameters)
		{
			const asETypeIdentityStatus ParameterStatus = ValidateIdentityRoleLocked(
				Parameter.Type, asEStableIdentityRole::TypeUse);
			if (ParameterStatus != asETypeIdentityStatus::Succeeded)
			{
				return ParameterStatus;
			}
		}
		return asETypeIdentityStatus::Succeeded;
	}

	asETypeIdentityStatus asCStableIdentityRegistry::ValidateDeclarationOwnerLocked(
		const asSDeclarationIdentityDescriptor& Descriptor) const
	{
		if (Descriptor.Kind == asEDeclarationIdentityKind::Module)
		{
			return Descriptor.Owner.IsZero()
				? asETypeIdentityStatus::Succeeded
				: asETypeIdentityStatus::InvalidOwner;
		}
		if (Descriptor.Kind == asEDeclarationIdentityKind::Namespace
			|| Descriptor.Kind == asEDeclarationIdentityKind::GlobalVariable)
		{
			if (Descriptor.Owner.IsZero())
			{
				return asETypeIdentityStatus::Succeeded;
			}
			const asUINT* Index = IdentityRecordByKey.Find(Descriptor.Owner);
			if (Index == nullptr || !IdentityRecords.IsValidIndex(static_cast<int32>(*Index)))
			{
				return asETypeIdentityStatus::InvalidOwner;
			}
			const asSCanonicalIdentityRecord& Owner = IdentityRecords[*Index];
			return Owner.Role == asEStableIdentityRole::Declaration
				&& (Owner.Declaration.Kind == asEDeclarationIdentityKind::Module
					|| Owner.Declaration.Kind == asEDeclarationIdentityKind::Namespace)
				? asETypeIdentityStatus::Succeeded
				: asETypeIdentityStatus::InvalidOwner;
		}
		if (Descriptor.Kind == asEDeclarationIdentityKind::Property
			|| Descriptor.Kind == asEDeclarationIdentityKind::EnumConstant)
		{
			return HasIdentityRoleLocked(
				Descriptor.Owner, asEStableIdentityRole::NominalType)
				? asETypeIdentityStatus::Succeeded
				: asETypeIdentityStatus::InvalidOwner;
		}
		if (Descriptor.Kind == asEDeclarationIdentityKind::Parameter)
		{
			const asUINT* Index = IdentityRecordByKey.Find(Descriptor.Owner);
			if (Index == nullptr || !IdentityRecords.IsValidIndex(static_cast<int32>(*Index)))
			{
				return asETypeIdentityStatus::InvalidOwner;
			}
			const asSCanonicalIdentityRecord& Owner = IdentityRecords[*Index];
			if (Owner.Role == asEStableIdentityRole::Function)
			{
				return asETypeIdentityStatus::Succeeded;
			}
			if (Owner.Role == asEStableIdentityRole::NominalType
				&& (Owner.NominalType.Kind == asETypeDeclKind::Delegate
					|| Owner.NominalType.Kind == asETypeDeclKind::Event))
			{
				return asETypeIdentityStatus::Succeeded;
			}
			return asETypeIdentityStatus::InvalidOwner;
		}
		return asETypeIdentityStatus::InvalidOwner;
	}

END_AS_NAMESPACE
