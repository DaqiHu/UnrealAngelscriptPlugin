#pragma once

#include "CoreMinimal.h"

#include "Core/Artifacts/AngelscriptArtifactIdentity.h"
#include "StaticJIT/StaticJITConfig.h"

#if AS_CAN_GENERATE_JIT

struct FAngelscriptEngine;
class asIScriptFunction;

enum class EAngelscriptStaticJITNativeCallLinkage : uint8
{
	None = 0,
	ExportedSymbol,
	HeaderInline,
	ExportedRuntimeThunk,
	/** The exported Runtime function is the registered implementation itself. */
	ExportedRuntimeCallable,
	ProviderPrivate,
};

enum class EAngelscriptStaticJITNativeCallABIDomain : uint8
{
	None = 0,
	Scalar,
	/** Direct call with borrowed managed inputs and cleanup-free POD values. */
	BorrowedManagedPOD,
	/** Linkage is proven, but managed value/reference lowering is not yet enabled. */
	ManagedValuesDeferred,
	/**
	 * Provider-private target invoked only through the reviewed AS-visible VM
	 * scalar marshalling contract. It does not claim a native scalar call ABI.
	 */
	VMScalarBridge,
};

enum class EAngelscriptStaticJITNativeCallCompileOutPolicy : uint8
{
	None = 0,
	IfNoLog,
};

enum class EAngelscriptStaticJITNativeCallDefaultArgumentPolicy : uint8
{
	None = 0,
	/** Generated C++ must materialize the registered AS defaults before calling. */
	CallerMaterializes,
};

enum class EAngelscriptStaticJITNativeCallWorldContextPolicy : uint8
{
	None = 0,
	/** The exported callable resolves the current AS world; no hidden C++ arg. */
	RuntimeCurrentWorld,
};

enum class EAngelscriptStaticJITNativeCallExceptionPolicy : uint8
{
	Unspecified = 0,
	DoesNotSetScriptException,
	MaySetScriptException,
};

enum class EAngelscriptStaticJITNativeCallVisibility : uint8
{
	Unspecified = 0,
	External,
	HeaderInline,
	ProviderPrivate,
	Internal,
};

enum class EAngelscriptStaticJITNativeCallRouteFlags : uint8
{
	None = 0,
	DirectCall = 1 << 0,
	ScalarBridge = 1 << 1,
	RequiresProcessEvent = 1 << 2,
	MaySuspend = 1 << 3,
	/** Load the provider-private target from the current Engine Bind slot. */
	CurrentNativeCall = 1 << 4,
};
ENUM_CLASS_FLAGS(EAngelscriptStaticJITNativeCallRouteFlags);

enum class EAngelscriptStaticJITNativeCallLifetimeFlags : uint8
{
	None = 0,
	ByValueScalarOnly = 1 << 0,
	NoRetainedReferences = 1 << 1,
	NoEngineLocalCapture = 1 << 2,
	BorrowedInputReferences = 1 << 3,
	ByValueObjectArguments = 1 << 4,
};
ENUM_CLASS_FLAGS(EAngelscriptStaticJITNativeCallLifetimeFlags);

/**
 * Exact ABI of the function installed in the current AngelScript Engine.
 * Unlike the scalar lowering identity below, this also represents registered
 * object/reference/default/hidden shapes so external linkage can be recorded
 * before the Typed emitter supports those values.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITNativeABIIdentity
{
	FString Canonical;
	FAngelscriptHash256 Hash;

	static FAngelscriptStaticJITNativeABIIdentity FromCanonical(
		FStringView Canonical);
	bool IsValid() const;

	friend bool operator==(
		const FAngelscriptStaticJITNativeABIIdentity& Left,
		const FAngelscriptStaticJITNativeABIIdentity& Right)
	{
		return Left.Canonical == Right.Canonical
			&& Left.Hash == Right.Hash;
	}
};

ANGELSCRIPTRUNTIME_API bool TryBuildAngelscriptStaticJITNativeABIIdentity(
	const asIScriptFunction& ScriptFunction,
	FAngelscriptStaticJITNativeABIIdentity& OutIdentity,
	FString* OutError = nullptr);

struct ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITScalarABIIdentity
{
	FString Canonical;
	FAngelscriptHash256 Hash;

	static FAngelscriptStaticJITScalarABIIdentity FromCanonical(
		FStringView Canonical);
	bool IsValid() const;

	friend bool operator==(
		const FAngelscriptStaticJITScalarABIIdentity& Left,
		const FAngelscriptStaticJITScalarABIIdentity& Right)
	{
		return Left.Canonical == Right.Canonical
			&& Left.Hash == Right.Hash;
	}
};

ANGELSCRIPTRUNTIME_API bool TryBuildAngelscriptStaticJITScalarABIIdentity(
	const asIScriptFunction& ScriptFunction,
	FAngelscriptStaticJITScalarABIIdentity& OutIdentity,
	FString* OutError = nullptr);

/**
 * Explicit cross-module native-call contract. Legacy native-form spellings do
 * not imply this descriptor and therefore remain non-external by default.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITNativeCallDescriptor
{
	EAngelscriptStaticJITNativeCallLinkage Linkage =
		EAngelscriptStaticJITNativeCallLinkage::None;
	EAngelscriptStaticJITNativeCallVisibility Visibility =
		EAngelscriptStaticJITNativeCallVisibility::Unspecified;
	FString Symbol;
	FString Include;
	FName OwningModule;
	FString ApiMacro;
	/**
	 * Exact reviewed public C++ function type. Descriptor validation parses this
	 * into a bounded type-only signature before a direct emitter may consume it;
	 * it is never inferred from an AngelScript Runtime type name.
	 */
	FString CppCallableSignature;
	FAngelscriptStaticJITNativeABIIdentity NativeABI;
	EAngelscriptStaticJITNativeCallABIDomain ABIDomain =
		EAngelscriptStaticJITNativeCallABIDomain::None;
	EAngelscriptStaticJITNativeCallCompileOutPolicy CompileOutPolicy =
		EAngelscriptStaticJITNativeCallCompileOutPolicy::None;
	EAngelscriptStaticJITNativeCallDefaultArgumentPolicy DefaultArgumentPolicy =
		EAngelscriptStaticJITNativeCallDefaultArgumentPolicy::None;
	uint8 DefaultArgumentCount = 0;
	EAngelscriptStaticJITNativeCallWorldContextPolicy WorldContextPolicy =
		EAngelscriptStaticJITNativeCallWorldContextPolicy::None;
	EAngelscriptStaticJITNativeCallExceptionPolicy ExceptionPolicy =
		EAngelscriptStaticJITNativeCallExceptionPolicy::Unspecified;
	FAngelscriptStaticJITScalarABIIdentity ScalarABI;
	EAngelscriptStaticJITNativeCallRouteFlags RouteFlags =
		EAngelscriptStaticJITNativeCallRouteFlags::None;
	EAngelscriptStaticJITNativeCallLifetimeFlags LifetimeFlags =
		EAngelscriptStaticJITNativeCallLifetimeFlags::None;

	bool IsAbsent() const;
	FString DumpNormalized() const;
};

/**
 * Pointer-free, structurally validated projection of CppCallableSignature.
 * Spellings are retained exactly (apart from outer whitespace trimming) so a
 * direct C++ emitter consumes the reviewed descriptor ABI rather than trying
 * to reconstruct native names from Canonical or Runtime AngelScript types.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITNativeCallCppSignature
{
	FString ReturnType;
	TArray<FString> ParameterTypes;

	bool IsValid() const
	{
		return !ReturnType.IsEmpty();
	}
};

/**
 * Engine-owned descriptor attachment surface. Returned pointers are transient:
 * callers that cross an Engine/lifetime boundary must copy the descriptor into
 * their immutable generation snapshot.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITNativeCallRegistry
{
	static bool Attach(
		FAngelscriptEngine& Engine,
		asIScriptFunction* ScriptFunction,
		const FAngelscriptStaticJITNativeCallDescriptor& Descriptor);
	static const FAngelscriptStaticJITNativeCallDescriptor* Find(
		asIScriptFunction* ScriptFunction);
	static int32 NumDescriptors();
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITNativeCallValidationContext
{
	FAngelscriptStaticJITNativeABIIdentity ExpectedNativeABI;
	FAngelscriptStaticJITScalarABIIdentity ExpectedScalarABI;
	TSet<FName> KnownModules;
	TSet<FName> PublicDependencyModules;
};

enum class EAngelscriptStaticJITNativeCallValidationCode : uint8
{
	Valid = 0,
	Absent,
	UnknownLinkage,
	UnknownVisibility,
	InvalidAbsentMetadata,
	InvalidVisibility,
	MissingSymbol,
	InvalidSymbol,
	MissingInclude,
	InvalidInclude,
	MissingOwningModule,
	UnknownOwningModule,
	PrivateDependency,
	InternalLinkage,
	MissingApiMacro,
	InvalidApiMacro,
	UnexpectedApiMacro,
	MissingCppCallableSignature,
	InvalidCppCallableSignature,
	MissingNativeABI,
	InvalidNativeABI,
	NativeABIMismatch,
	UnknownABIDomain,
	InvalidCallSemantics,
	MissingScalarABI,
	InvalidScalarABI,
	ScalarABIMismatch,
	UnknownRouteFlags,
	UnsafeRouting,
	UnknownLifetimeFlags,
	UnsafeLifetime,
	TypedABIUnavailable,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITNativeCallValidation
{
	bool bMetadataValid = false;
	bool bDirectCallable = false;
	bool bCurrentNativeCallable = false;
	bool bScalarBridgeCallable = false;
	/** Present only after the descriptor's reviewed C++ type is validated. */
	FAngelscriptStaticJITNativeCallCppSignature ReviewedCppSignature;
	EAngelscriptStaticJITNativeCallValidationCode Code =
		EAngelscriptStaticJITNativeCallValidationCode::UnknownLinkage;
	FString Detail;
};

ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITNativeCallValidation
ValidateAngelscriptStaticJITNativeCallDescriptor(
	const FAngelscriptStaticJITNativeCallDescriptor& Descriptor,
	const FAngelscriptStaticJITNativeCallValidationContext& Context);

#endif // AS_CAN_GENERATE_JIT
