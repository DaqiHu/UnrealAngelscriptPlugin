#ifndef AS_RUNTIME_TYPE_BINDING_H
#define AS_RUNTIME_TYPE_BINDING_H

#include "as_array.h"
#include "as_datatype.h"
#include "as_string.h"
#include "frontend/as_type_identity.h"

BEGIN_AS_NAMESPACE

class asCScriptEngine;
class asCTypeInfo;
class asCObjectProperty;
class asCScriptFunction;

// Stable result categories for the candidate-install boundary. These values
// are diagnostic/API categories, not VM bytecode operands or persisted IDs.
enum asERuntimeTypeBindingResult
{
	asRUNTIME_TYPE_BIND_OK = 0,
	asRUNTIME_TYPE_BIND_INVALID = -1,
	asRUNTIME_TYPE_BIND_MISSING = -2,
	asRUNTIME_TYPE_BIND_AMBIGUOUS = -3,
	asRUNTIME_TYPE_BIND_WRONG_KIND = -4,
	asRUNTIME_TYPE_BIND_WRONG_PROFILE = -5,
	asRUNTIME_TYPE_BIND_WRONG_NATIVE_ENVIRONMENT = -6,
	asRUNTIME_TYPE_BIND_ABI_MISMATCH = -7,
	asRUNTIME_TYPE_BIND_MEMBER_MISSING = -8,
	asRUNTIME_TYPE_BIND_ALREADY_FROZEN = -9,
	asRUNTIME_TYPE_BIND_INVALID_OWNER = -10,
	asRUNTIME_TYPE_BIND_INVALID_DURABLE_ID = -11
};

struct asSRuntimeTypeGeneration
{
	asQWORD Value = 0;
	bool IsValid() const { return Value != 0; }
	bool operator==(const asSRuntimeTypeGeneration& Other) const { return Value == Other.Value; }
};

struct asSRuntimeTypeId
{
	asSRuntimeTypeGeneration Generation;
	int Value = -1;
	bool IsValid() const { return Generation.IsValid() && Value >= 0; }
	bool Equals(const asSRuntimeTypeId& Other) const
	{
		return IsValid() && Other.IsValid()
			&& Generation == Other.Generation
			&& Value == Other.Value;
	}
};

ANGELSCRIPTRUNTIME_API int asValidateRuntimeTypeIdOwner(
	const asSRuntimeTypeId& RuntimeTypeId,
	const asSRuntimeTypeGeneration& ExpectedGeneration);
ANGELSCRIPTRUNTIME_API int asRejectRuntimeTypeIdForDurableEncoding(
	const asSRuntimeTypeId& RuntimeTypeId);

enum asERuntimeTypeRelocationUse
{
	asTYPE_RELOC_METADATA_ONLY = 0,
	asTYPE_RELOC_RUNTIME_TYPE_TARGET,
	asTYPE_RELOC_PUBLIC_TYPE_ID,
	asTYPE_RELOC_PROPERTY,
	asTYPE_RELOC_FUNCTION,
	// FREE for an initialization-list buffer targets an anonymous Engine helper
	// type. Its durable identity is the exact list-factory function plus the
	// owning canonical object ABI, never the helper pointer or a fabricated name.
	asTYPE_RELOC_LIST_PATTERN_TARGET,
	// ABI authentication row for one exact Canonical function-signature
	// coordinate. It owns no bytecode coordinate and is never patched into an
	// instruction; the generation-local binding table retains it for StaticJIT
	// admission and other same-generation consumers.
	asTYPE_RELOC_FUNCTION_SIGNATURE,
	asTYPE_RELOC_USE_COUNT
};

enum asERuntimeTypeBindingSignatureRole
{
	asRUNTIME_TYPE_SIGNATURE_NONE = 0,
	asRUNTIME_TYPE_SIGNATURE_RETURN,
	asRUNTIME_TYPE_SIGNATURE_PARAMETER,
	asRUNTIME_TYPE_SIGNATURE_RECEIVER
};

struct ANGELSCRIPTRUNTIME_API asSRuntimeTypeBindingEnvironment
{
	asCString targetProfileKey;
	asCString nativeEnvironmentKey;
};

// Process-local compilation identity used by canonical VM bytecode artifacts.
// Detached/offline producers may supply a stronger external profile instead.
ANGELSCRIPTRUNTIME_API asSRuntimeTypeBindingEnvironment
asMakeCurrentRuntimeTypeBindingEnvironment();

// Complete compatibility identity for one target. The two signature strings
// carry the authoritative layout/member and behaviour/call ABI facts. The
// hash is only an index/checksum accelerator and is always compared together
// with those complete values.
struct ANGELSCRIPTRUNTIME_API asSTypeABIKey
{
	asSStableKey canonicalTypeUse;
	asCString stableTypeKey;
	asCString targetProfileKey;
	asCString nativeEnvironmentKey;
	asEASTTypeKind typeKind = asAST_TYPE_INVALID;
	asDWORD qualifiers = 0;
	asQWORD objectFlags = 0;
	int byteSize = -1;
	int byteAlignment = -1;
	asCString layoutSignature;
	asCString behaviourSignature;
	asQWORD layoutHash = 0;

	bool Equals(const asSTypeABIKey& other) const;
};

ANGELSCRIPTRUNTIME_API int asProjectCanonicalTypeUse(
	const asCStableIdentityRegistry& Registry,
	const asSStableKey& CanonicalTypeUse,
	asSTypeABIKey& OutKey);

// Detached artifacts carry this record. It deliberately contains no
// snapshot-local TypeRef, Engine pointer, TypeInfo pointer, or numeric typeId.
struct ANGELSCRIPTRUNTIME_API asSTypeRelocation
{
	asSTypeABIKey expectedType;
	asCString stableMemberKey;
	// Exact producer-carried owner identity for signature rows. This is kept
	// separate from stableMemberKey, whose existing meaning is the property or
	// callable target patched by non-signature relocations.
	asCString ownerFunctionStableKey;
	asERuntimeTypeRelocationUse use = asTYPE_RELOC_METADATA_ONLY;
	asERuntimeTypeBindingSignatureRole signatureRole =
		asRUNTIME_TYPE_SIGNATURE_NONE;
	asUINT formalIndex = asUINT(-1);
	asUINT functionArtifactIndex = 0;
	asUINT bytecodeDwordOffset = 0;
	// Generation-local ordinal of the exact function operand captured from the
	// detached bytecode. The persisted identity remains `stableMemberKey`; this
	// ordinal prevents same-signature/same-arity candidates in the same batch
	// from being re-selected by a later name search.
	asUINT targetFunctionArtifactIndex = asUINT(-1);
};

// Candidate-owned installation result. All pointer/integer fields are valid
// only for the generation that owns the frozen table.
struct ANGELSCRIPTRUNTIME_API asSResolvedRuntimeTypeBinding
{
	// Preserve both sides of the validate-all comparison. `expectedKey` is the
	// detached producer requirement; `key` is the exact generation-local ABI
	// captured after resolution.
	asSTypeABIKey expectedKey;
	asSTypeABIKey key;
	asCString ownerFunctionStableKey;
	asERuntimeTypeRelocationUse relocationUse = asTYPE_RELOC_METADATA_ONLY;
	asERuntimeTypeBindingSignatureRole signatureRole =
		asRUNTIME_TYPE_SIGNATURE_NONE;
	asUINT formalIndex = asUINT(-1);
	asCDataType dataType;
	asCTypeInfo* typeInfo = 0;
	asCScriptFunction* ownerFunction = 0;
	asCObjectProperty* property = 0;
	asCScriptFunction* function = 0;
	int publicTypeId = -1;
	int propertyOffset = -1;
};

class ANGELSCRIPTRUNTIME_API asCRuntimeTypeBindingTable
{
public:
	asCRuntimeTypeBindingTable();

	// Refresh only the process-local numeric projections after the aggregate
	// artifact has assigned its candidate-local TypeIds. Stable identity, ABI,
	// member and function targets remain frozen by Build(). A binding that does
	// not require a public TypeId may legitimately retain -1.
	int RefreshPublicTypeIdProjections(asCScriptEngine* engine);

	const asSResolvedRuntimeTypeBinding* GetBinding(asUINT index) const;
	asUINT GetBindingCount() const { return bindings.GetLength(); }
	bool IsFrozen() const { return frozen; }
	// Transfer a complete frozen table without copying generation-local
	// pointers.  This is used only by the aggregate module publication
	// transaction; neither side may be visible to readers while swapping.
	void SwapWith(asCRuntimeTypeBindingTable& other);

private:
	asCArray<asSResolvedRuntimeTypeBinding> bindings;
	bool frozen;
};

END_AS_NAMESPACE

#endif
