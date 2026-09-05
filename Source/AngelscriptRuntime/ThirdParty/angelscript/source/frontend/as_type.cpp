#include "frontend/as_type.h"

BEGIN_AS_NAMESPACE

asCType::asCType(asETypeKind Kind)
		: asCASTNode(asEASTNodeFamily::Type, static_cast<uint16>(Kind), {})
	{
	}

	asCType::asCType(asETypeKind Kind, const asSStableKey& Identity)
		: asCASTNode(asEASTNodeFamily::Type, static_cast<uint16>(Kind), {})
		, StableIdentity(Identity)
	{
		if (Identity.IsZero()) MarkInvalid();
	}

	bool asCQualType::TryCreate(asCType* Type, asETypeQualifiers Qualifiers, asCQualType& Out)
	{
		return TryCreateRaw(Type, static_cast<uint8>(Qualifiers), Out);
	}

	asETypeIdentityStatus asCQualType::GetStableIdentity(asCStableIdentityRegistry& Registry, asSStableKey& Out) const
	{
		return Registry.InternQualifiedTypeUse(Type ? Type->GetStableIdentity() : asSStableKey{},
			static_cast<asETypeUseQualifiers>(Qualifiers), Out);
	}

	asCQualType asCVarDecl::GetResolvedQualType() const
	{
		if (SourceInfo && SourceInfo->bHasSemanticQualifiers)
		{
			asCQualType Result;
			asCQualType::TryCreateRaw(ResolvedType, SourceInfo->SemanticQualifiers, Result);
			return Result;
		}
		const asSTypeSyntax& Syntax = GetTypeSyntax();
		asETypeQualifiers Qualifiers = asETypeQualifiers::None;
		if (Syntax.bConst) Qualifiers = Qualifiers | asETypeQualifiers::Const;
		if (Syntax.bHandle) Qualifiers = Qualifiers | asETypeQualifiers::Handle;
		if (Syntax.Reference != asETypeReferenceDirection::None) Qualifiers = Qualifiers | asETypeQualifiers::Reference;
		asCQualType Result;
		asCQualType::TryCreate(ResolvedType, Qualifiers, Result);
		return Result;
	}

	bool asCVarDecl::SetResolvedQualType(asCQualType Type)
	{
		if (!CanMutate() || !Type.IsValid() || Type.GetType()->GetOwnerContext() != GetOwnerContext()) return false;
		if (!SourceInfo) SourceInfo = MakeUnique<asSVariableSourceInfo>();
		ResolvedType = Type.GetType();
		SourceInfo->SemanticQualifiers = static_cast<uint8>(Type.GetQualifiers());
		SourceInfo->bHasSemanticQualifiers = true;
		return true;
	}

	bool asCQualType::TryCreateRaw(asCType* Type, uint8 Qualifiers, asCQualType& Out)
	{
		constexpr uint8 Legal = static_cast<uint8>(asETypeQualifiers::Const) |
			static_cast<uint8>(asETypeQualifiers::Reference) |
			static_cast<uint8>(asETypeQualifiers::Handle) |
			static_cast<uint8>(asETypeQualifiers::Nullable);
		Out = {};
		if (!Type || (Qualifiers & ~Legal) != 0) return false;
		Out.Type = Type;
		Out.Qualifiers = Qualifiers;
		return true;
	}

	asCBuiltinType::asCBuiltinType(asEPrimitiveType InPrimitive, asSStableKey Identity)
		: asCType(asETypeKind::Builtin, Identity)
		, Primitive(InPrimitive)
	{
	}

END_AS_NAMESPACE
