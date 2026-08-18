/*
   AngelCode Scripting Library
   Copyright (c) 2003-2026 Andreas Jonsson

   This source is part of the maintained Unreal AngelScript fork. The typed
   semantic model is a private compiler sidecar and is not public SDK ABI.
*/

#ifndef AS_TYPED_SEMANTIC_IR_H
#define AS_TYPED_SEMANTIC_IR_H

#include "as_array.h"
#include "as_datatype.h"
#include "as_string.h"

BEGIN_AS_NAMESPACE

template<typename Tag>
struct asTTypedSemanticId
{
	static const asUINT InvalidValue = asUINT(-1);

	asUINT value;

	asTTypedSemanticId()
		: value(InvalidValue)
	{
	}

	explicit asTTypedSemanticId(asUINT InValue)
		: value(InValue)
	{
	}

	bool IsValid() const
	{
		return value != InvalidValue;
	}

	bool operator==(const asTTypedSemanticId& Other) const
	{
		return value == Other.value;
	}

	bool operator!=(const asTTypedSemanticId& Other) const
	{
		return value != Other.value;
	}
};

struct asSTypedSemanticSymbolTag;
struct asSTypedSemanticExpressionTag;
struct asSTypedSemanticStatementTag;
struct asSTypedSemanticCleanupPlanTag;

typedef asTTypedSemanticId<asSTypedSemanticSymbolTag> asTypedSemanticSymbolId;
typedef asTTypedSemanticId<asSTypedSemanticExpressionTag> asTypedSemanticExpressionId;
typedef asTTypedSemanticId<asSTypedSemanticStatementTag> asTypedSemanticStatementId;
typedef asTTypedSemanticId<asSTypedSemanticCleanupPlanTag> asTypedSemanticCleanupPlanId;

enum class asETypedSemanticExpressionKind : asBYTE
{
	Literal,
	FoldedGlobalConstant,
	GlobalStorage,
	Symbol,
	Conversion,
	Assignment,
	Unary,
	Binary,
	ShortCircuit,
	ResolvedCall,
	CallRewrite,
	Unsupported,
};

enum class asETypedSemanticCallTargetKind : asBYTE
{
	Unspecified,
	ScriptFunction,
	SystemFunction,
	// Mutable module-import binding slot. resolvedFunctionId retains the
	// FUNC_IMPORTED coordinate and never the slot's current boundFunctionId.
	ImportedFunction,
};

// Body availability is semantic routing metadata only. RegisteredNative means
// that AngelScript has a current system-function target; it does not claim the
// callable has an exported symbol or a reviewed direct-native ABI.
enum class asETypedSemanticBodyAvailability : asBYTE
{
	Unspecified,
	ScriptBody,
	NoScriptBody,
	RegisteredNative,
};

// Keeps the declaration visible to the calling module separate from the
// module/runtime that owns the executable body. Mutable import slots use None:
// their current bound body is deliberately not captured in compiler HIR.
enum class asETypedSemanticBodyOwnership : asBYTE
{
	Unspecified,
	CallingModule,
	OtherModule,
	SharedOwner,
	SharedBorrowed,
	ExternalBorrowed,
	NativeRuntime,
	None,
};

struct asSTypedSemanticFunctionTargetProvenance
{
	// Stable declaration-shaped metadata. The Engine-local function/slot
	// coordinate remains in resolvedFunctionId for generation-time adoption.
	asCString canonicalDeclaration;
	asCString declarationModule;
	asDWORD declaredTraitBits = 0;
	asETypedSemanticBodyAvailability bodyAvailability =
		asETypedSemanticBodyAvailability::Unspecified;
	asETypedSemanticBodyOwnership bodyOwnership =
		asETypedSemanticBodyOwnership::Unspecified;
	asCString bodyOwnerModule;
	// Populated only for asFUNC_IMPORTED. Neither field contains or derives from
	// mutable sBindInfo::boundFunctionId.
	asCString importedSourceModule;
	asCString importedCanonicalSignature;
};

enum class asETypedSemanticStatementKind : asBYTE
{
	Block,
	LocalDeclaration,
	Expression,
	If,
	For,
	While,
	DoWhile,
	Switch,
	Case,
	Break,
	Continue,
	Return,
	Unsupported,
};

enum class asETypedSemanticSymbolKind : asBYTE
{
	Receiver,
	Parameter,
	Local,
};

enum class asETypedSemanticUnsupportedKind : asBYTE
{
	None,
	Ternary,
	ObjectAccess,
	PropertyAccess,
	Reference,
	Handle,
	Container,
	ConstructionOrLifetime,
	Lambda,
	ExceptionCleanup,
	SuspendPoint,
	CompilerSynthesizedFunction,
};

enum class asETypedSemanticReceiverKind : asBYTE
{
	None,
	NativeObjectThis,
	ExternalImplicitThis,
	MixinFirstParameter,
};

enum class asETypedSemanticInvocationKind : asBYTE
{
	Global,
	GlobalInitializer,
	InstanceMethod,
	Constructor,
	Destructor,
	Factory,
	Imported,
	System,
	Funcdef,
	Synthesized,
};

enum class asETypedSemanticCompileOutKind : asBYTE
{
	CompileCalls,
	CompileOutEntirely,
	ReplaceWithFirstParam,
	CompileOutAsMethodChain,
};

// The compiler's final operand list is not the same thing as the AS source
// argument list. In particular, hidden WorldContext/default values and a
// receiver aliased to a mixin formal must remain distinguishable without
// inserting host-only values into the source-visible parameter surface.
enum class asETypedSemanticArgumentOrigin : asBYTE
{
	None,
	SourcePositional,
	SourceNamed,
	Default,
	HostHidden,
	Receiver,
};

// The role of one compiler-authoritative expression evaluation. This is kept
// separate from source argument position and final formal binding: ordinary
// calls evaluate arguments in reverse formal order, instance receivers occupy
// a separate position, and mutations evaluate their value before their target.
enum class asETypedSemanticEvaluationRole : asBYTE
{
	Operand,
	Receiver,
	Argument,
	HiddenArgument,
	MutationValue,
	MutationTarget,
};

enum class asETypedSemanticFirstParamMetadata : asBYTE
{
	None,
	ScriptFunction,
	ScriptObjectType,
};

// Pointer-free capture of the maintained runtime's native call requirements.
// This describes why a system call needs an external descriptor or VM bridge;
// it never authorizes a direct call and never persists func/method/userData.
struct asSTypedSemanticNativeCallRequirements
{
	bool isSystemFunction = false;
	int callConvention = -1;
	int baseOffset = 0;
	int scriptReturnSize = 0;
	int parameterSize = 0;
	asETypedSemanticFirstParamMetadata firstParamMetadata =
		asETypedSemanticFirstParamMetadata::None;
	bool usesGenericContext = false;
	bool hasFunctionUserData = false;
	bool hasFunctionCaller = false;
	bool returnsOnStack = false;
	asUINT cleanupArgumentCount = 0;
};

enum class asETypedSemanticSafePointRole : asBYTE
{
	None,
	Position,
	FunctionEntry,
	Statement,
	Call,
	LoopEntry,
	LoopBackedge,
	Transfer,
	SwitchInvalidValue,
	Return,
};

enum class asETypedSemanticLoopPhase : asBYTE
{
	None,
	Initializer,
	Condition,
	Body,
	Increment,
};

enum class asETypedSemanticSwitchNormalization : asBYTE
{
	None,
	SignedInt32,
	UnsignedInt32,
};

/**
 * Compiler-authoritative cleanup proof for the captured semantic function.
 *
 * VerifiedEmpty is deliberately stronger than "no object syntax was seen":
 * the compiler may publish it only after final variable, exception-region,
 * construction, parameter, and return ownership state has been inspected.
 */
enum class asETypedSemanticCleanupPlanState : asBYTE
{
	Unverified,
	VerifiedEmpty,
	NonEmpty,
	PartialConstruction,
	ScriptDestructor,
	CompilerExceptionRegion,
};

struct asSTypedSemanticCleanupPlan
{
	asTypedSemanticCleanupPlanId id;
	// Full typed lifetime slots are introduced by the managed-value milestone.
	// The scalar milestone accepts only an explicitly empty list.
	asCArray<asUINT> reverseLiveLifetimeSlots;
};

/**
 * Optional source origin carried beside the compiler-authoritative processed
 * span. Offsets are UTF-8 byte offsets in the named source section.
 */
struct asSTypedSemanticSourceAnchor
{
	asCString section;
	asUINT sourceOffset = 0;
	asUINT sourceLength = 0;
	asUINT row = 0;
	asUINT column = 0;
};

enum class asETypedSemanticGeneratedSourceKind : asBYTE
{
	None,
	LiteralAsset,
	SubsystemAccessor,
	ClassHelper,
	StaticClassHelper,
	DelegateHelper,
	RangeBasedFor,
};

struct asSTypedSemanticGeneratedSourceOrigin
{
	asETypedSemanticGeneratedSourceKind kind =
		asETypedSemanticGeneratedSourceKind::None;
	asCString generatorName;
	bool hasAuthoredAnchor = false;
	asSTypedSemanticSourceAnchor authoredAnchor;
};

struct asSTypedSemanticSourceSpan
{
	// The processed source consumed by the maintained compiler. This tuple is
	// mandatory and is never relabelled as an authored-source coordinate.
	asCString section;
	asUINT sourceOffset = 0;
	asUINT sourceLength = 0;
	asUINT row = 0;
	asUINT column = 0;

	// Exact authored origin is optional. It is populated only when the
	// preprocessor can prove a one-to-one authored mapping.
	bool hasAuthoredOrigin = false;
	asSTypedSemanticSourceAnchor authoredOrigin;

	// Generated origin is independent from an exact authored origin. Its
	// optional authored anchor identifies the declaration that caused
	// generation; it does not claim that generated tokens were authored there.
	bool hasGeneratedOrigin = false;
	asSTypedSemanticGeneratedSourceOrigin generatedOrigin;
};

/** Source-section range installed before parsing and copied into HIR spans. */
struct asSTypedSemanticSourceProvenanceRange
{
	asUINT processedOffset = 0;
	asUINT processedLength = 0;
	bool hasAuthoredOrigin = false;
	asSTypedSemanticSourceAnchor authoredOrigin;
	bool hasGeneratedOrigin = false;
	asSTypedSemanticGeneratedSourceOrigin generatedOrigin;
};

enum class asETypedSemanticDiagnosticSourceKind : asBYTE
{
	Processed,
	Authored,
	GeneratedAnchor,
};

struct asSTypedSemanticDiagnosticSource
{
	asETypedSemanticDiagnosticSourceKind kind =
		asETypedSemanticDiagnosticSourceKind::Processed;
	asSTypedSemanticSourceAnchor source;
	asETypedSemanticGeneratedSourceKind generatedKind =
		asETypedSemanticGeneratedSourceKind::None;
	asCString generatorName;
};

// Pointer-free provenance for one final formal binding. This remains separate
// from evaluationSteps: source spelling/order, final formal binding, and the
// maintained compiler's execution order are three independent facts.
struct asSTypedSemanticArgumentProvenance
{
	asETypedSemanticArgumentOrigin origin =
		asETypedSemanticArgumentOrigin::None;
	// Zero-based position in the caller's written argument list. Defaults,
	// hidden values, and receiver aliases retain the explicit -1 sentinel.
	int sourceArgumentOrdinal = -1;
	// Present only for a written named argument.
	asCString sourceArgumentName;
	// Always a processed compiler span. For synthesized default/hidden values
	// this is the caller's call-site span, not the callee declaration span.
	asSTypedSemanticSourceSpan processedSpan;
	// Populated only when the value originates in the callee declaration.
	int calleeFunctionId = -1;
	asCString calleeDeclaration;
	int calleeParameterIndex = -1;
	asCString calleeParameterName;
	asCString canonicalExpression;
};

struct asSTypedSemanticEvaluationStep
{
	asETypedSemanticEvaluationRole role =
		asETypedSemanticEvaluationRole::Operand;
	asTypedSemanticExpressionId expression;
	// Non-negative only when this step supplies one declared call formal.
	int formalParameterIndex = -1;
	// Zero-based AS source-list ordinal. Defaults, hidden inputs, and receivers
	// that are not written in the source argument list retain -1. Task 2.14
	// adds the richer named/default declaration provenance around this field.
	int sourceArgumentOrdinal = -1;
};

struct asSTypedSemanticEffectiveReceiver
{
	asETypedSemanticReceiverKind kind = asETypedSemanticReceiverKind::None;
	asTypedSemanticSymbolId symbol;
	int parameterIndex = -1;
	asCDataType type;
};

// Private maintained-fork contract. Keep the table-driven trait test and the
// classifier's static assertion synchronized when asEFuncTrait grows.
static const asDWORD asTYPED_SEMANTIC_KNOWN_FUNC_TRAIT_BITS = 0x07FFFFFFu;

struct asSTypedSemanticFunctionTraitSummary
{
	asDWORD knownTraitBits = 0;
	asDWORD unknownTraitBits = 0;
	asDWORD sourcePolicyTraitBits = 0;
	asDWORD receiverTraitBits = 0;
	asDWORD invocationTraitBits = 0;
	asDWORD dispatchTraitBits = 0;
	asDWORD bodyAvailabilityTraitBits = 0;
	asDWORD lifetimeTraitBits = 0;
	asDWORD profileTraitBits = 0;
};

ANGELSCRIPTRUNTIME_API asSTypedSemanticFunctionTraitSummary
ClassifyTypedSemanticFunctionTraits(asDWORD DeclaredTraitBits);

struct asSTypedSemanticFunctionHeader
{
	asDWORD declaredTraitBits = 0;
	asSTypedSemanticFunctionTraitSummary traitSummary;
	// The module whose compiler transaction produced this HIR and the owner of
	// the body are separate so reused shared/external declarations cannot claim
	// a body merely because the declaration is visible in another module.
	asCString callingModule;
	asETypedSemanticBodyAvailability bodyAvailability =
		asETypedSemanticBodyAvailability::Unspecified;
	asETypedSemanticBodyOwnership bodyOwnership =
		asETypedSemanticBodyOwnership::Unspecified;
	asCString bodyOwnerModule;
	asETypedSemanticInvocationKind invocationKind = asETypedSemanticInvocationKind::Global;
	asSTypedSemanticEffectiveReceiver receiver;
	asETypedSemanticCompileOutKind compileOutKind = asETypedSemanticCompileOutKind::CompileCalls;
	int hiddenArgumentIndex = -1;
	int determinesOutputTypeArgumentIndex = -1;
	bool returnsOnStack = false;
	bool hasSuspendState = false;
	bool hasExceptionCleanup = false;
	asETypedSemanticCleanupPlanState cleanupPlanState =
		asETypedSemanticCleanupPlanState::Unverified;
	asTypedSemanticCleanupPlanId universalCleanupPlan;
	bool cleanupPlanCoversAllTransfers = false;
};

struct asSTypedSemanticSymbol
{
	asTypedSemanticSymbolId id;
	asETypedSemanticSymbolKind kind = asETypedSemanticSymbolKind::Local;
	asCString name;
	asCDataType type;
	asSTypedSemanticSourceSpan span;
};

struct asSTypedSemanticExpression
{
	asTypedSemanticExpressionId id;
	asETypedSemanticExpressionKind kind = asETypedSemanticExpressionKind::Unsupported;
	asCDataType type;
	asSTypedSemanticSourceSpan span;
	asETypedSemanticSafePointRole safePointRole = asETypedSemanticSafePointRole::None;
	asCArray<asTypedSemanticExpressionId> operands;
	// Calls keep the source receiver separate from declared formal operands.
	// This is required for instance syntax and parameter-aliased receivers such
	// as external_implicit_this without erasing or renumbering parameter zero.
	asTypedSemanticExpressionId receiverExpression;
	asTypedSemanticSymbolId symbol;
	asCString operatorToken;
	// Pointer-free compiler-resolved target detail for unsupported accesses.
	// The exact result type remains authoritative in `type`.
	asCString resolvedTarget;
	asCString literal;
	// Engine-local compiler coordinate retained only long enough for the UE-side
	// generation snapshot to reconcile this origin against stable global
	// dependency identity. It is never a serialized or generated-code identity.
	int resolvedGlobalPropertyId = -1;
	// Exact compiler constant payload for scalar literals and folded hard-value
	// globals. Keeping bits instead of formatted text preserves signed, enum,
	// float, and double values without host formatting or source reparsing.
	asQWORD literalBits = 0;
	asETypedSemanticCallTargetKind callTargetKind =
		asETypedSemanticCallTargetKind::Unspecified;
	int resolvedFunctionId = -1;
	asSTypedSemanticFunctionTargetProvenance targetProvenance;
	// CompileCalls belongs to executable ResolvedCall nodes. The other values
	// belong to non-executable CallRewrite nodes that retain only the compiler's
	// final value/erasure semantics and optional target provenance.
	asETypedSemanticCompileOutKind callRewriteKind =
		asETypedSemanticCompileOutKind::CompileCalls;
	asCArray<int> formalParameterIndices;
	// Parallel to operands/formalParameterIndices. This is source-list order,
	// not execution order; -1 means the input was compiler/host synthesized.
	asCArray<int> sourceArgumentOrdinals;
	// Compatibility projection used by the scalar call emitter: for each call
	// operand, stores its ordinal among effective formal-argument evaluations.
	// evaluationSteps is the complete authority and additionally contains an
	// independent instance receiver when the receiver is not a formal operand.
	asCArray<asUINT> evaluationSequence;
	// Compatibility projection for consumers that need only the broad role.
	// argumentProvenance is the authoritative source/default/hidden record.
	asCArray<asETypedSemanticArgumentOrigin> argumentOrigins;
	asCArray<asSTypedSemanticArgumentProvenance> argumentProvenance;
	asCArray<asSTypedSemanticEvaluationStep> evaluationSteps;
	int hiddenArgumentIndex = -1;
	int determinesOutputTypeArgumentIndex = -1;
	asSTypedSemanticNativeCallRequirements nativeCall;
	asTypedSemanticExpressionId mutationTarget;
	asTypedSemanticExpressionId mutationValue;
	bool mutationReturnsOldValue = false;
	asETypedSemanticUnsupportedKind unsupportedKind = asETypedSemanticUnsupportedKind::None;
};

struct asSTypedSemanticStatement
{
	asTypedSemanticStatementId id;
	asETypedSemanticStatementKind kind = asETypedSemanticStatementKind::Unsupported;
	asSTypedSemanticSourceSpan span;
	asETypedSemanticSafePointRole safePointRole = asETypedSemanticSafePointRole::None;
	asETypedSemanticLoopPhase loopPhase = asETypedSemanticLoopPhase::None;
	asCArray<asTypedSemanticStatementId> children;
	asTypedSemanticSymbolId symbol;
	asTypedSemanticExpressionId initializerExpression;
	asTypedSemanticExpressionId conditionExpression;
	asCArray<asTypedSemanticExpressionId> incrementExpressions;
	asTypedSemanticExpressionId valueExpression;
	asTypedSemanticStatementId initializerStatement;
	asTypedSemanticStatementId incrementStatement;
	asTypedSemanticStatementId thenStatement;
	asTypedSemanticStatementId elseStatement;
	asTypedSemanticStatementId bodyStatement;
	asTypedSemanticStatementId targetStatement;
	asTypedSemanticCleanupPlanId cleanupPlan;
	asUINT exitedScopeCount = 0;
	asETypedSemanticSwitchNormalization switchNormalization =
		asETypedSemanticSwitchNormalization::None;
	int caseConstantValue = 0;
	bool isDefaultCase = false;
	bool fallsThrough = false;
	bool hasExplicitFallthrough = false;
	bool switchHasDefault = false;
	bool switchIsExhaustive = false;
	bool switchHasInvalidEnumValueEdge = false;
	asETypedSemanticUnsupportedKind unsupportedKind = asETypedSemanticUnsupportedKind::None;
};

enum class asETypedSemanticVerificationError : asBYTE
{
	None,
	MissingRoot,
	RootIsNotBlock,
	NonContiguousSymbolId,
	NonContiguousExpressionId,
	NonContiguousStatementId,
	DanglingSymbolId,
	DanglingExpressionId,
	DanglingStatementId,
	StatementHasMultipleOwners,
	StatementOwnershipCycle,
	InvalidNodeShape,
	InvalidSourceSpan,
	InvalidSourceProvenance,
	MissingUnsupportedCategory,
	InvalidEffectiveReceiver,
	InvalidCleanupPlan,
	InvalidControlTarget,
	InvalidCallEvaluationSequence,
	InvalidArgumentProvenance,
	InvalidFunctionTraitSummary,
	InvalidFunctionBodyProvenance,
	InvalidCallTargetProvenance,
};

struct asSTypedSemanticVerificationResult
{
	asETypedSemanticVerificationError error = asETypedSemanticVerificationError::None;
	asTypedSemanticSymbolId symbol;
	asTypedSemanticExpressionId expression;
	asTypedSemanticStatementId statement;
	asSTypedSemanticSourceSpan span;
	asCString detail;

	bool IsValid() const
	{
		return error == asETypedSemanticVerificationError::None;
	}
};

class ANGELSCRIPTRUNTIME_API asCTypedSemanticFunction
{
public:
	asTypedSemanticSymbolId AddSymbol(const asSTypedSemanticSymbol& Symbol);
	asTypedSemanticExpressionId AddExpression(const asSTypedSemanticExpression& Expression);
	asTypedSemanticStatementId AddStatement(const asSTypedSemanticStatement& Statement);
	asTypedSemanticCleanupPlanId AddCleanupPlan(const asSTypedSemanticCleanupPlan& Plan);

	asCString declaration;
	asSTypedSemanticFunctionHeader header;
	asCArray<asSTypedSemanticSymbol> symbols;
	asCArray<asSTypedSemanticExpression> expressions;
	asCArray<asSTypedSemanticStatement> statements;
	asCArray<asSTypedSemanticCleanupPlan> cleanupPlans;
	asTypedSemanticStatementId rootStatement;
};

ANGELSCRIPTRUNTIME_API asSTypedSemanticVerificationResult VerifyTypedSemanticFunction(
	const asCTypedSemanticFunction& Function);

/** Authored exact origin -> generated authored anchor -> processed fallback. */
ANGELSCRIPTRUNTIME_API asSTypedSemanticDiagnosticSource
	SelectTypedSemanticDiagnosticSource(
		const asSTypedSemanticSourceSpan& Span);

ANGELSCRIPTRUNTIME_API asCString DumpTypedSemanticFunction(
	const asCTypedSemanticFunction& Function);

enum class asETypedSemanticSnapshotState : asBYTE
{
	Verified,
	MissingFunction,
	VerificationFailed,
};

struct asSTypedSemanticSnapshotResult
{
	asETypedSemanticSnapshotState state =
		asETypedSemanticSnapshotState::MissingFunction;
	asSTypedSemanticVerificationResult verification;
	asCString text;
	asCString json;

	bool IsValid() const
	{
		return state == asETypedSemanticSnapshotState::Verified
			&& verification.IsValid();
	}
};

// Fork-private diagnostic snapshot. The returned strings are outputs only;
// neither the compiler nor a StaticJIT backend reads them back as HIR.
ANGELSCRIPTRUNTIME_API asSTypedSemanticSnapshotResult BuildTypedSemanticSnapshot(
	const asCTypedSemanticFunction* Function);

END_AS_NAMESPACE

#endif
