#pragma once

#include "CoreMinimal.h"

#include "Artifacts/AngelscriptArtifactIdentity.h"
#include "Features/IModularFeatures.h"

#include "AngelscriptInclude.h"

namespace FAngelscriptRuntimeJITBackendAbi
{
	inline constexpr uint32 Revision = 2;
	inline constexpr uint32 EntryAbiRevision = 1;
	inline constexpr uint32 BackendIdCapacity = 64;
	inline constexpr uint32 MaximumBytecodeWordCount = 1u << 24;
	inline constexpr uint32 MaximumFrameValueCount = 1u << 16;
	inline constexpr uint32 MaximumInstructionCount = 1u << 24;
	inline constexpr uint32 MaximumControlFlowEdgeCount = 1u << 24;
	inline constexpr uint32 MaximumHelperTokenCount = 1u << 16;
}

/** Stable, fixed-storage identifier owned by a Runtime JIT plugin. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITBackendId
{
	ANSICHAR Value[FAngelscriptRuntimeJITBackendAbi::BackendIdCapacity]{};

	static bool TryParse(
		FStringView Text,
		FAngelscriptRuntimeJITBackendId& OutBackendId);

	bool IsValid() const;
	FString ToString() const;

	friend bool operator==(
		const FAngelscriptRuntimeJITBackendId& Left,
		const FAngelscriptRuntimeJITBackendId& Right)
	{
		return FMemory::Memcmp(
			Left.Value,
			Right.Value,
			sizeof(Left.Value)) == 0;
	}

	friend bool operator!=(
		const FAngelscriptRuntimeJITBackendId& Left,
		const FAngelscriptRuntimeJITBackendId& Right)
	{
		return !(Left == Right);
	}

	friend bool operator<(
		const FAngelscriptRuntimeJITBackendId& Left,
		const FAngelscriptRuntimeJITBackendId& Right)
	{
		return FCStringAnsi::Strcmp(Left.Value, Right.Value) < 0;
	}
};

enum class EAngelscriptRuntimeJITPlatform : uint32
{
	None = 0,
	Win64 = 1u << 0,
	Linux = 1u << 1,
	Mac = 1u << 2,
};
ENUM_CLASS_FLAGS(EAngelscriptRuntimeJITPlatform)

enum class EAngelscriptRuntimeJITBuildConfiguration : uint32
{
	None = 0,
	EditorDevelopment = 1u << 0,
	GameDevelopment = 1u << 1,
	Shipping = 1u << 2,
};
ENUM_CLASS_FLAGS(EAngelscriptRuntimeJITBuildConfiguration)

enum class EAngelscriptRuntimeJITCompileConcurrency : uint8
{
	SerializedPerSession = 0,
	ConcurrentPerSession = 1,
};

enum class EAngelscriptRuntimeJITEndianness : uint8
{
	Unknown = 0,
	Little,
	Big,
};

enum class EAngelscriptRuntimeJITCallingConvention : uint8
{
	Unknown = 0,
	AngelscriptVMEntry,
};

enum class EAngelscriptRuntimeJITInvocationKind : uint8
{
	Unknown = 0,
	Global,
	ScriptMethod,
	Mixin,
	ExternalImplicitThis,
};

enum class EAngelscriptRuntimeJITReceiverKind : uint8
{
	Unknown = 0,
	None,
	NativeObjectSlot,
	DeclaredParameterAlias,
};

enum class EAngelscriptRuntimeJITFunctionProfile : uint8
{
	Unknown = 0,
	ScalarWholeFunction,
	ManagedValues,
	SuspendOrLatent,
	ExceptionCleanup,
};

enum class EAngelscriptRuntimeJITScalarKind : uint8
{
	Invalid = 0,
	Bool,
	Int8,
	UInt8,
	Int16,
	UInt16,
	Int32,
	UInt32,
	Int64,
	UInt64,
	Float32,
	Float64,
	OpaquePointer,
};

enum class EAngelscriptRuntimeJITFrameValueFlags : uint8
{
	None = 0,
	Parameter = 1u << 0,
	Result = 1u << 1,
	Local = 1u << 2,
	ReceiverAlias = 1u << 3,
};
ENUM_CLASS_FLAGS(EAngelscriptRuntimeJITFrameValueFlags)

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITFrameValue
{
	uint32 StructSize = sizeof(FAngelscriptRuntimeJITFrameValue);
	EAngelscriptRuntimeJITScalarKind Kind =
		EAngelscriptRuntimeJITScalarKind::Invalid;
	EAngelscriptRuntimeJITFrameValueFlags Flags =
		EAngelscriptRuntimeJITFrameValueFlags::None;
	uint16 Reserved = 0;
	uint32 ByteOffset = 0;
	uint32 ByteSize = 0;
	uint32 Alignment = 0;
	uint32 SourceIndex = 0;
};

enum class EAngelscriptRuntimeJITControlFlowEdgeKind : uint8
{
	Fallthrough = 0,
	Branch,
	ConditionalBranch,
	Switch,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITControlFlowEdge
{
	uint32 StructSize = sizeof(FAngelscriptRuntimeJITControlFlowEdge);
	uint32 FromInstruction = 0;
	uint32 ToInstruction = 0;
	EAngelscriptRuntimeJITControlFlowEdgeKind Kind =
		EAngelscriptRuntimeJITControlFlowEdgeKind::Fallthrough;
	uint8 Reserved[3]{};
};

enum class EAngelscriptRuntimeJITHelperTokenKind : uint8
{
	Invalid = 0,
	RuntimeHelper,
	ScriptFunction,
	SystemFunction,
	GlobalAddress,
	StringLiteral,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITHelperToken
{
	uint32 StructSize = sizeof(FAngelscriptRuntimeJITHelperToken);
	EAngelscriptRuntimeJITHelperTokenKind Kind =
		EAngelscriptRuntimeJITHelperTokenKind::Invalid;
	uint8 Reserved[3]{};
	uint32 SlotIndex = 0;
	FAngelscriptHash256 StableKey;
	FAngelscriptHash256 ExpectedAbi;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITBackendFactoryMetadata
{
	uint32 StructSize = sizeof(FAngelscriptRuntimeJITBackendFactoryMetadata);
	uint32 AbiRevision = FAngelscriptRuntimeJITBackendAbi::Revision;
	FAngelscriptRuntimeJITBackendId BackendId;
	const TCHAR* DisplayName = nullptr;
	EAngelscriptRuntimeJITPlatform SupportedPlatforms =
		EAngelscriptRuntimeJITPlatform::None;
	EAngelscriptRuntimeJITBuildConfiguration SupportedConfigurations =
		EAngelscriptRuntimeJITBuildConfiguration::None;
	EAngelscriptRuntimeJITCompileConcurrency CompileConcurrency =
		EAngelscriptRuntimeJITCompileConcurrency::SerializedPerSession;
	uint8 Reserved[7]{};
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITCompileSnapshotHeader
{
	uint32 StructSize = sizeof(FAngelscriptRuntimeJITCompileSnapshotHeader);
	uint32 AbiRevision = FAngelscriptRuntimeJITBackendAbi::Revision;
	FAngelscriptRuntimeJITBackendId BackendId;
	EAngelscriptRuntimeJITPlatform Platform =
		EAngelscriptRuntimeJITPlatform::None;
	EAngelscriptRuntimeJITBuildConfiguration Configuration =
		EAngelscriptRuntimeJITBuildConfiguration::None;
	uint8 PointerWidth = 0;
	EAngelscriptRuntimeJITEndianness Endianness =
		EAngelscriptRuntimeJITEndianness::Unknown;
	EAngelscriptRuntimeJITCallingConvention CallingConvention =
		EAngelscriptRuntimeJITCallingConvention::Unknown;
	EAngelscriptRuntimeJITInvocationKind Invocation =
		EAngelscriptRuntimeJITInvocationKind::Global;
	EAngelscriptRuntimeJITReceiverKind Receiver =
		EAngelscriptRuntimeJITReceiverKind::None;
	EAngelscriptRuntimeJITFunctionProfile FunctionProfile =
		EAngelscriptRuntimeJITFunctionProfile::ScalarWholeFunction;
	uint8 Reserved = 0;
	int32 ReceiverParameterIndex = INDEX_NONE;
	uint32 ParameterFrameByteSize = 0;
	uint32 LocalFrameByteSize = 0;
	uint32 ResultByteSize = 0;
	uint32 EntryAbiRevision = 0;
	uint64 EngineNamespace = 0;
	uint64 PublicationOrdinal = 0;
	uint64 CancellationGeneration = 0;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptHash256 FunctionRevision;
	FAngelscriptHash256 EntryAbiHash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITCompileSnapshotView
{
	uint32 StructSize = sizeof(FAngelscriptRuntimeJITCompileSnapshotView);
	uint32 AbiRevision = FAngelscriptRuntimeJITBackendAbi::Revision;
	FAngelscriptRuntimeJITCompileSnapshotHeader Header;
	const asDWORD* Bytecode = nullptr;
	uint32 BytecodeWordCount = 0;
	const FAngelscriptRuntimeJITFrameValue* FrameValues = nullptr;
	uint32 FrameValueCount = 0;
	const uint32* InstructionWordOffsets = nullptr;
	uint32 InstructionCount = 0;
	const FAngelscriptRuntimeJITControlFlowEdge* ControlFlowEdges = nullptr;
	uint32 ControlFlowEdgeCount = 0;
	const FAngelscriptRuntimeJITHelperToken* HelperTokens = nullptr;
	uint32 HelperTokenCount = 0;
};

using FAngelscriptRuntimeJITReleaseCodeLease = void (*)(void* UserData);

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITCodeLeaseView
{
	uint32 StructSize = sizeof(FAngelscriptRuntimeJITCodeLeaseView);
	uint32 AbiRevision = FAngelscriptRuntimeJITBackendAbi::Revision;
	void* UserData = nullptr;
	FAngelscriptRuntimeJITReleaseCodeLease Release = nullptr;
};

/** Thread-safe shared ownership for executable code and backend resources. */
class ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITCodeLease
{
public:
	FAngelscriptRuntimeJITCodeLease() = default;

	static FAngelscriptRuntimeJITCodeLease Adopt(
		const FAngelscriptRuntimeJITCodeLeaseView& View);

	bool IsValid() const
	{
		return State.IsValid();
	}

	int32 GetSharedReferenceCount() const;

	void Reset()
	{
		State.Reset();
	}

private:
	struct FState;
	explicit FAngelscriptRuntimeJITCodeLease(
		TSharedPtr<FState, ESPMode::ThreadSafe> InState)
		: State(MoveTemp(InState))
	{
	}

	TSharedPtr<FState, ESPMode::ThreadSafe> State;
};

enum class EAngelscriptRuntimeJITCompileOutcome : uint8
{
	Compiled = 0,
	Unsupported,
	Cancelled,
	Stale,
	BackendFailure,
	InvalidInput,
};

enum class EAngelscriptRuntimeJITDiagnosticReason : uint16
{
	None = 0,
	BackendUnavailable,
	DuplicateBackendId,
	IncompatibleFactoryAbi,
	UnsupportedPlatform,
	UnsupportedConfiguration,
	UnsupportedOpcode,
	UnsupportedManagedValue,
	UnsupportedReceiver,
	SuspendOrLatent,
	ExceptionCleanup,
	DebuggerActive,
	CoverageActive,
	EntryAbiMismatch,
	MalformedSnapshot,
	Cancelled,
	StaleRevision,
	BackendFailure,
	InvalidBackendResult,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITCompileResult
{
	uint32 StructSize = sizeof(FAngelscriptRuntimeJITCompileResult);
	uint32 AbiRevision = FAngelscriptRuntimeJITBackendAbi::Revision;
	EAngelscriptRuntimeJITCompileOutcome Outcome =
		EAngelscriptRuntimeJITCompileOutcome::InvalidInput;
	EAngelscriptRuntimeJITDiagnosticReason Reason =
		EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot;
	uint16 Reserved = 0;
	uint32 BytecodeOffset = MAX_uint32;
	FAngelscriptHash256 FunctionRevision;
	FAngelscriptHash256 EntryAbiHash;
	asJITFunction VMEntry = nullptr;
	FAngelscriptRuntimeJITCodeLease CodeLease;
	uint64 CompileLatencyMicroseconds = 0;
	uint64 CodeSize = 0;
};

enum class EAngelscriptRuntimeJITContractValidationCode : uint8
{
	Valid = 0,
	InvalidStructSize,
	UnsupportedRevision,
	InvalidBackendId,
	MissingDisplayName,
	InvalidPlatform,
	InvalidConfiguration,
	InvalidConcurrency,
	InvalidPointerWidth,
	InvalidEndianness,
	InvalidCallingConvention,
	InvalidInvocation,
	InvalidReceiver,
	InvalidReceiverParameter,
	InvalidFunctionProfile,
	InvalidEngineNamespace,
	InvalidFunctionIdentity,
	InvalidEntryAbi,
	InvalidBytecodeCount,
	MissingBytecode,
	InvalidFrameValueCount,
	MissingFrameValues,
	InvalidFrameValue,
	InvalidFrameLayout,
	InvalidInstructionCount,
	MissingInstructionOffsets,
	InvalidInstructionBoundary,
	InvalidControlFlowCount,
	MissingControlFlowEdges,
	InvalidControlFlowEdge,
	InvalidHelperTokenCount,
	MissingHelperTokens,
	InvalidHelperToken,
	InvalidCodeLease,
	InvalidCompileOutcome,
	InvalidDiagnosticReason,
	InvalidOutcomeReason,
	InvalidBytecodeOffset,
	InvalidCompileResult,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITContractValidationResult
{
	EAngelscriptRuntimeJITContractValidationCode Code =
		EAngelscriptRuntimeJITContractValidationCode::Valid;
	int32 ElementIndex = INDEX_NONE;

	bool IsValid() const
	{
		return Code == EAngelscriptRuntimeJITContractValidationCode::Valid;
	}
};

class ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITContract
{
public:
	static FAngelscriptRuntimeJITContractValidationResult
		ValidateFactoryMetadata(
			const FAngelscriptRuntimeJITBackendFactoryMetadata& Metadata);

	static FAngelscriptRuntimeJITContractValidationResult ValidateSnapshot(
		const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot);

	static FAngelscriptRuntimeJITContractValidationResult ValidateCodeLease(
		const FAngelscriptRuntimeJITCodeLeaseView& Lease);

	static FAngelscriptRuntimeJITContractValidationResult ValidateCompileResult(
		const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot,
		const FAngelscriptRuntimeJITCompileResult& Result);
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITBackendSessionConfig
{
	uint32 StructSize = sizeof(FAngelscriptRuntimeJITBackendSessionConfig);
	uint32 AbiRevision = FAngelscriptRuntimeJITBackendAbi::Revision;
	FAngelscriptRuntimeJITBackendId BackendId;
	EAngelscriptRuntimeJITPlatform Platform =
		EAngelscriptRuntimeJITPlatform::None;
	EAngelscriptRuntimeJITBuildConfiguration Configuration =
		EAngelscriptRuntimeJITBuildConfiguration::None;
	uint64 EngineNamespace = 0;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptRuntimeJITBackendSession
{
public:
	virtual ~IAngelscriptRuntimeJITBackendSession() = default;

	virtual FAngelscriptRuntimeJITCompileResult Compile(
		const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot) = 0;
	virtual void Cancel(uint64 CancellationGeneration) = 0;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptRuntimeJITBackendFactory
	: public IModularFeature
{
public:
	virtual ~IAngelscriptRuntimeJITBackendFactory() = default;

	virtual const FAngelscriptRuntimeJITBackendFactoryMetadata*
		GetMetadata() const = 0;
	virtual TUniquePtr<IAngelscriptRuntimeJITBackendSession> CreateSession(
		const FAngelscriptRuntimeJITBackendSessionConfig& Config) = 0;

	static FName FeatureName();
};

static_assert(sizeof(FAngelscriptRuntimeJITBackendId)
	== FAngelscriptRuntimeJITBackendAbi::BackendIdCapacity,
	"Runtime BackendId storage is part of the current factory ABI.");
