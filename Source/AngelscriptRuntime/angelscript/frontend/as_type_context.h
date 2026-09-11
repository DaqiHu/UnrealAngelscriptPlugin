/* Maintained Unreal AngelScript fork — shared canonical type ownership. */
#pragma once

#include "frontend/as_type_identity.h"

#include "Templates/SharedPointer.h"

BEGIN_AS_NAMESPACE

/** Immutable language/target inputs shared by one or more frontend builders. */
	struct asSTypeContextOptions
	{
		bool FloatIsFloat64 = true;
		bool ExpandDefaultArrayToTemplate = true;
		asUINT PointerByteSize = sizeof(void*);
		FString TargetProfile = TEXT("native");
		// Resolve this declaration through the caller's type environment; a name
		// alone never creates a string implementation or an ambient Engine.
		FString StringLiteralTypeName = TEXT("FString");
		FString ArrayTemplateTypeName = TEXT("array");
		// Explicit frontend permission input; never read ambient Editor/cook state.
		bool AllowEditorOnlyAccess = false;
	};

	/**
	 * Explicit owner of type canonicalization state. This is deliberately not a
	 * process singleton: ASTs, builders and detached metadata retain a shared
	 * context for as long as their stable keys must be authenticated.
	 */
	class ANGELSCRIPTRUNTIME_API asCTypeContext final
	{
	public:
		explicit asCTypeContext(const asSTypeContextOptions& InOptions = {});
		asCTypeContext(
			const asSTypeContextOptions& InOptions,
			TSharedRef<asCStableIdentityRegistry, ESPMode::ThreadSafe> InIdentityRegistry);

		const asSTypeContextOptions& GetOptions() const { return Options; }
		asCStableIdentityRegistry& GetIdentityRegistry() { return IdentityRegistry.Get(); }
		const asCStableIdentityRegistry& GetIdentityRegistry() const { return IdentityRegistry.Get(); }
		TSharedRef<asCStableIdentityRegistry, ESPMode::ThreadSafe>
		GetIdentityRegistryOwner() const { return IdentityRegistry; }

		asETypeIdentityStatus InternPrimitiveTypeUse(
			asEPrimitiveType Primitive,
			asSStableKey& OutKey);
		asETypeIdentityStatus InternQualifiedTypeUse(
			const asSStableKey& BaseType,
			asETypeUseQualifiers AdditionalQualifiers,
			asSStableKey& OutKey);

	private:
		const asSTypeContextOptions Options;
		TSharedRef<asCStableIdentityRegistry, ESPMode::ThreadSafe> IdentityRegistry;
	};

END_AS_NAMESPACE
