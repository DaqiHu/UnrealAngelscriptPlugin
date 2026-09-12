/* Maintained Unreal AngelScript fork — shared canonical type ownership. */

#include "frontend/AST/as_type_context.h"

BEGIN_AS_NAMESPACE

asCTypeContext::asCTypeContext(const asSTypeContextOptions& InOptions)
		: asCTypeContext(
			InOptions,
			MakeShared<asCStableIdentityRegistry, ESPMode::ThreadSafe>())
	{
	}

	asCTypeContext::asCTypeContext(
		const asSTypeContextOptions& InOptions,
		TSharedRef<asCStableIdentityRegistry, ESPMode::ThreadSafe> InIdentityRegistry)
		: Options(InOptions)
		, IdentityRegistry(MoveTemp(InIdentityRegistry))
	{
	}

	asETypeIdentityStatus asCTypeContext::InternPrimitiveTypeUse(
		asEPrimitiveType Primitive,
		asSStableKey& OutKey)
	{
		asSStableTypeUseDescriptor Descriptor;
		Descriptor.Kind = asETypeUseKind::Primitive;
		Descriptor.Primitive = Primitive;
		return IdentityRegistry->InternTypeUse(Descriptor, OutKey);
	}

	asETypeIdentityStatus asCTypeContext::InternQualifiedTypeUse(
		const asSStableKey& BaseType,
		asETypeUseQualifiers AdditionalQualifiers,
		asSStableKey& OutKey)
	{
		return IdentityRegistry->InternQualifiedTypeUse(
			BaseType, AdditionalQualifiers, OutKey);
	}

END_AS_NAMESPACE
