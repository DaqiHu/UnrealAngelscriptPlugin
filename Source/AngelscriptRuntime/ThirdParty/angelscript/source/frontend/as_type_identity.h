/* Maintained Unreal AngelScript fork — canonical stable identity ownership. */

#pragma once

#include "frontend/as_canonical_encoding.h"

#include "Containers/Map.h"
#include "HAL/CriticalSection.h"

BEGIN_AS_NAMESPACE

enum class asETypeIdentityStatus : uint8
	{
		Succeeded,
		NonCacheable,
		InvalidDescriptor,
		UnsupportedSchema,
		UnsupportedDomain,
		MalformedWitness,
		DigestMismatch,
		InvalidRole,
		BudgetExceeded,
		InvalidOwner,
		InvalidRecord,
		Collision,
		ReturnTypeConflict,
		DescriptorMismatch,
	};

	enum class asETypeDeclKind : uint8
	{
		Invalid,
		Class,
		Struct,
		Interface,
		Enum,
		Delegate,
		Typedef,
		Funcdef,
		Primitive,
		Event,
	};

	enum class asETypeDeclOrigin : uint8
	{
		StableNamed,
		StableGenerated,
		Local,
		Anonymous,
		Recovery,
	};

	enum class asETypeUseKind : uint8
	{
		Invalid,
		Primitive,
		Nominal,
		Callable,
	};

	enum class asEPrimitiveType : uint8
	{
		Invalid,
		Void,
		Bool,
		Int8,
		Int16,
		Int32,
		Int64,
		UInt8,
		UInt16,
		UInt32,
		UInt64,
		Float32,
		Float64,
	};

	enum class asETypeUseQualifiers : uint16
	{
		None = 0,
		Const = 1 << 0,
		Reference = 1 << 1,
		Handle = 1 << 2,
		Nullable = 1 << 3,
		ParameterOut = 1 << 8,
		OwnershipTransfer = 1 << 9,
		TargetABILowering = 1 << 10,
	};

	inline asETypeUseQualifiers operator|(
		asETypeUseQualifiers Left,
		asETypeUseQualifiers Right)
	{
		return static_cast<asETypeUseQualifiers>(
			static_cast<uint16>(Left) | static_cast<uint16>(Right));
	}

	/** The registry-owned semantic category for one fixed stable key. */
	enum class asEStableIdentityRole : uint8
	{
		Invalid,
		NominalType,
		TypeUse,
		Function,
		Declaration,
	};

	enum class asEParameterPassingMode : uint8
	{
		Value,
		In,
		Out,
		InOut,
	};

	struct asSStableParameter
	{
		asSStableKey Type;
		asEParameterPassingMode PassingMode = asEParameterPassingMode::Value;
	};

	/**
	 * Canonical nominal identity inputs. Members, layout, source order, Engine
	 * IDs and object addresses deliberately have no representation here.
	 */
	struct asSNominalTypeDescriptor
	{
		FString StableScope;
		TArray<FString> NamespacePath;
		asSStableKey Owner;
		asETypeDeclKind Kind = asETypeDeclKind::Invalid;
		FString Name;
		asUINT GenericArity = 0;
		asETypeDeclOrigin Origin = asETypeDeclOrigin::StableNamed;
		FString StableGeneratorIdentity;
	};

	/**
	 * Canonical type-use identity inputs. Nominals, arguments and callable
	 * signature components are registry-owned stable-key edges, not witnesses.
	 */
	struct asSStableTypeUseDescriptor
	{
		asETypeUseKind Kind = asETypeUseKind::Invalid;
		asEPrimitiveType Primitive = asEPrimitiveType::Invalid;
		asSStableKey Declaration;
		TArray<asSStableKey> Arguments;
		asSStableKey ReturnType;
		TArray<asSStableParameter> Parameters;
		asETypeUseQualifiers Qualifiers = asETypeUseQualifiers::None;
	};

	enum class asEFunctionIdentityKind : uint8
	{
		Invalid,
		Global,
		Method,
		Constructor,
		Destructor,
		Factory,
		Behaviour,
		Lambda,
		ClassDefault,
		Conversion,
	};

	enum class asEFunctionIdentityModifiers : uint16
	{
		None = 0,
		ConstMethod = 1 << 0,
	};

	inline asEFunctionIdentityModifiers operator|(
		asEFunctionIdentityModifiers Left,
		asEFunctionIdentityModifiers Right)
	{
		return static_cast<asEFunctionIdentityModifiers>(
			static_cast<uint16>(Left) | static_cast<uint16>(Right));
	}

	/** Function definition facts which never create a distinct overload key. */
	enum class asEFunctionDefinitionModifiers : uint16
	{
		None = 0,
		Static = 1 << 0,
		Final = 1 << 1,
		Override = 1 << 2,
		Property = 1 << 3,
		Explicit = 1 << 4,
		Mixin = 1 << 5,
		Local = 1 << 6,
		AcceptTemporary = 1 << 7,
		ExternalImplicitThis = 1 << 8,
		NoDiscard = 1 << 9,
		AllowDiscard = 1 << 10,
		Generated = 1 << 11,
		Deprecated = 1 << 12,
		Defaults = 1 << 13,
		UnsafeDuringConstruction = 1 << 14,
	};

	inline asEFunctionDefinitionModifiers operator|(
		asEFunctionDefinitionModifiers Left,
		asEFunctionDefinitionModifiers Right)
	{
		return static_cast<asEFunctionDefinitionModifiers>(
			static_cast<uint16>(Left) | static_cast<uint16>(Right));
	}

	/**
	 * Parsed semantic function identity. ReturnType is retained for conflict
	 * validation but is intentionally excluded from ordinary overload identity.
	 * Conversion declarations instead encode their actual resolved destination
	 * TypeUse edge, which must exactly equal ReturnType.
	 */
	struct asSFunctionIdentityDescriptor
	{
		FString StableScope;
		TArray<FString> NamespacePath;
		asSStableKey Owner;
		asEFunctionIdentityKind Kind = asEFunctionIdentityKind::Invalid;
		FString Name;
		asUINT GenericArity = 0;
		TArray<asSStableParameter> Parameters;
		asEFunctionIdentityModifiers Modifiers = asEFunctionIdentityModifiers::None;
		asEFunctionDefinitionModifiers DefinitionModifiers =
			asEFunctionDefinitionModifiers::None;
		asSStableKey ReturnType;
		asSStableKey EnclosingFunction;
		asUINT LambdaOrdinal = 0;
		asSStableKey ConversionDestination;
	};

	/** Persistent non-type declarations required by host semantic projections. */
	enum class asEDeclarationIdentityKind : uint8
	{
		Invalid,
		Module,
		Namespace,
		GlobalVariable,
		Property,
		Parameter,
		EnumConstant,
	};

	/**
	 * Stable coordinates for externally addressed declarations. Local variables
	 * remain source/context-local. Parameter names, defaults and types are not
	 * part of their coordinate; the owning function/callable and ordinal are.
	 */
	struct asSDeclarationIdentityDescriptor
	{
		static constexpr asUINT InvalidOrdinal = MAX_uint32;

		FString StableScope;
		asSStableKey Owner;
		asEDeclarationIdentityKind Kind = asEDeclarationIdentityKind::Invalid;
		FString Name;
		asUINT Ordinal = InvalidOrdinal;
	};

	/**
	 * Explicit digest dependency for canonical identity ownership. Production
	 * contexts use BLAKE3; a deterministic provider can prove fail-closed hash
	 * collision handling through the same admission path.
	 */
	class ANGELSCRIPTRUNTIME_API asIStableKeyDigestProvider
	{
	public:
		virtual ~asIStableKeyDigestProvider() = default;
		virtual asSStableKey ComputeDigest(
			TConstArrayView<uint8> CanonicalDescription) const = 0;
	};

	/**
	 * Sole owner of canonical identity descriptions. A stable key without this
	 * registry cannot authenticate a semantic descriptor.
	 */
	class ANGELSCRIPTRUNTIME_API asCStableIdentityRegistry
	{
	public:
		static constexpr asUINT MaxDescriptorEdges = 1024;
		static constexpr asUINT MaxCanonicalDescriptionBytes = 1024 * 1024;

		asCStableIdentityRegistry();
		explicit asCStableIdentityRegistry(
			const asIStableKeyDigestProvider& InDigestProvider);

		asETypeIdentityStatus InternNominalType(
			const asSNominalTypeDescriptor& Descriptor,
			asSStableKey& OutKey);
		asETypeIdentityStatus InternTypeUse(
			const asSStableTypeUseDescriptor& Descriptor,
			asSStableKey& OutKey);
		asETypeIdentityStatus InternQualifiedTypeUse(
			const asSStableKey& BaseType,
			asETypeUseQualifiers AdditionalQualifiers,
			asSStableKey& OutKey);
		asETypeIdentityStatus InternFunction(
			const asSFunctionIdentityDescriptor& Descriptor,
			asSStableKey& OutKey);
		asETypeIdentityStatus InternDeclaration(
			const asSDeclarationIdentityDescriptor& Descriptor,
			asSStableKey& OutKey);

		asETypeIdentityStatus ValidateNominalType(
			const asSStableKey& Key,
			const asSNominalTypeDescriptor& Descriptor) const;
		asETypeIdentityStatus ValidateTypeUse(
			const asSStableKey& Key,
			const asSStableTypeUseDescriptor& Descriptor) const;
		asETypeIdentityStatus ValidateFunction(
			const asSStableKey& Key,
			const asSFunctionIdentityDescriptor& Descriptor) const;
		asETypeIdentityStatus ValidateDeclaration(
			const asSStableKey& Key,
			const asSDeclarationIdentityDescriptor& Descriptor) const;

		asETypeIdentityStatus ResolveNominalType(
			const asSStableKey& Key,
			asSNominalTypeDescriptor& OutDescriptor) const;
		asETypeIdentityStatus ResolveTypeUse(
			const asSStableKey& Key,
			asSStableTypeUseDescriptor& OutDescriptor) const;
		asETypeIdentityStatus ResolveFunction(
			const asSStableKey& Key,
			asSFunctionIdentityDescriptor& OutDescriptor) const;
		asETypeIdentityStatus ResolveDeclaration(
			const asSStableKey& Key,
			asSDeclarationIdentityDescriptor& OutDescriptor) const;
		asETypeIdentityStatus ResolveIdentityRole(
			const asSStableKey& Key,
			asEStableIdentityRole& OutRole) const;
		asUINT GetIdentityCount() const;

	private:
		struct asSCanonicalIdentityRecord
		{
			asSStableKey Key;
			asEStableIdentityRole Role = asEStableIdentityRole::Invalid;
			TArray<uint8> CanonicalDescription;
			asSStableKey ReturnType;
			asSNominalTypeDescriptor NominalType;
			asSStableTypeUseDescriptor TypeUse;
			asSFunctionIdentityDescriptor Function;
			asSDeclarationIdentityDescriptor Declaration;
		};

		asETypeIdentityStatus InternCanonicalLocked(
			asSCanonicalIdentityRecord&& Candidate,
			asSStableKey& OutKey);
		asETypeIdentityStatus ValidateCanonicalLocked(
			asEStableIdentityRole Role,
			const asSStableKey& Key,
			TConstArrayView<uint8> CanonicalDescription,
			const asSStableKey& ReturnType) const;
		bool HasIdentityRoleLocked(
			const asSStableKey& Key,
			asEStableIdentityRole Role) const;
		asETypeIdentityStatus ValidateIdentityRoleLocked(
			const asSStableKey& Key,
			asEStableIdentityRole Role) const;
		asETypeIdentityStatus ValidateNominalOwnerLocked(
			const asSNominalTypeDescriptor& Descriptor) const;
		asETypeIdentityStatus ValidateTypeUseEdgesLocked(
			const asSStableTypeUseDescriptor& Descriptor) const;
		asETypeIdentityStatus ValidateFunctionEdgesLocked(
			const asSFunctionIdentityDescriptor& Descriptor) const;
		asETypeIdentityStatus ValidateDeclarationOwnerLocked(
			const asSDeclarationIdentityDescriptor& Descriptor) const;

		mutable FCriticalSection Mutex;
		TArray<asSCanonicalIdentityRecord> IdentityRecords;
		TMap<asSStableKey, asUINT> IdentityRecordByKey;
		const asIStableKeyDigestProvider* DigestProvider = nullptr;
	};

END_AS_NAMESPACE
