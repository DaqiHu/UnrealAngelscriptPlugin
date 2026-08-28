#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITProvider.h"

#include <type_traits>

namespace FAngelscriptJITProviderDiagnosticsAbi
{
	inline constexpr uint32 SchemaRevision = 1;
	inline constexpr uint32 MaximumFallbackChainCount = 8;
	inline constexpr uint32 MaximumBackendAttemptCount = 1u << 20;
	inline constexpr uint32 MaximumSemanticFunctionCount = 1u << 20;
	inline constexpr uint32 MaximumCallCount = 1u << 20;
}

enum class EAngelscriptJITProviderDiagnosticSourceFlags : uint32
{
	None = 0,
	HasAuthoredSource = 1u << 0,
	HasGeneratedSource = 1u << 1,
};
ENUM_CLASS_FLAGS(EAngelscriptJITProviderDiagnosticSourceFlags)

enum class EAngelscriptJITProviderDiagnosticAttemptFlags : uint32
{
	None = 0,
	HasTypedDiagnostic = 1u << 0,
	HasSemanticFunction = 1u << 1,
	HasCallClosure = 1u << 2,
};
ENUM_CLASS_FLAGS(EAngelscriptJITProviderDiagnosticAttemptFlags)

/** One owned-by-provider source coordinate; empty Section means unavailable. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderDiagnosticSource
{
	uint32 StructSize = sizeof(FAngelscriptJITProviderDiagnosticSource);
	const TCHAR* Section = TEXT("");
	uint32 Row = 0;
	uint32 Column = 0;
};

/** Flat semantic-function row referenced by one backend attempt. */
struct ANGELSCRIPTRUNTIME_API
FAngelscriptJITProviderCanonicalFunctionDiagnostic
{
	uint32 StructSize =
		sizeof(FAngelscriptJITProviderCanonicalFunctionDiagnostic);
	FAngelscriptStableFunctionKey FunctionKey;
	const TCHAR* Role = TEXT("NonRoot");
	uint32 RawTraitBits = 0;
	uint32 UnknownTraitBits = 0;
	const TCHAR* InvocationKind = TEXT("Global");
	const TCHAR* ReceiverKind = TEXT("None");
	int32 ReceiverParameterIndex = INDEX_NONE;
	const TCHAR* CanonicalASTState = TEXT("Missing");
	const TCHAR* CleanupPlanState = TEXT("Unverified");
	uint32 bHasSuspendState = 0;
	uint32 bHasExceptionCleanup = 0;
	uint32 bCleanupPlanCoversAllTransfers = 0;
	int32 ComponentIndex = INDEX_NONE;
	uint32 bRecursiveComponent = 0;
	const TCHAR* EligibilityReason = TEXT("None");
	const TCHAR* EligibilityDetail = TEXT("");
	FAngelscriptJITProviderDiagnosticSource EligibilitySource;
};

/** Flat call row retaining both planner facts and actual emitted metadata. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderTypedCallDiagnostic
{
	uint32 StructSize = sizeof(FAngelscriptJITProviderTypedCallDiagnostic);
	FAngelscriptStableFunctionKey CallerFunctionKey;
	FAngelscriptStableFunctionKey CalleeFunctionKey;
	uint32 ExpressionIndex = MAX_uint32;
	EAngelscriptJITProviderDiagnosticSourceFlags SourceFlags =
		EAngelscriptJITProviderDiagnosticSourceFlags::None;
	FAngelscriptJITProviderDiagnosticSource ProcessedSource;
	FAngelscriptJITProviderDiagnosticSource AuthoredSource;
	FAngelscriptJITProviderDiagnosticSource GeneratedSource;
	const TCHAR* TargetKind = TEXT("Unspecified");
	const TCHAR* Disposition = TEXT("Unsupported");
	const TCHAR* CalleeEligibilityReason = TEXT("None");
	const TCHAR* CalleeEligibilityDetail = TEXT("");
	FAngelscriptJITProviderDiagnosticSource CalleeEligibilitySource;
	uint32 bHasNativeLinkageValidation = 0;
	const TCHAR* NativeLinkageValidation = TEXT("NotApplicable");
	const TCHAR* NativeLinkageValidationDetail = TEXT("");
	const TCHAR* CanonicalDeclaration = TEXT("");
	EAngelscriptJITNativeCallSiteRoute EmittedRoute =
		EAngelscriptJITNativeCallSiteRoute::Invalid;
	const TCHAR* EmittedCppCallee = TEXT("");
	const TCHAR* RuntimeDllCore = TEXT("");
	const TCHAR* MetadataRowSymbol = TEXT("");
	const TCHAR* RegisteredTarget = TEXT("");
	const TCHAR* DirectCppSymbol = TEXT("");
	FAngelscriptHash256 StableTargetKey;
	FAngelscriptHash256 ExpectedAbi;
	int32 ReferenceSlotIndex = INDEX_NONE;
};

/** One ordered backend attempt and its optional Typed diagnostic ranges. */
struct ANGELSCRIPTRUNTIME_API
FAngelscriptJITProviderBackendAttemptDiagnostic
{
	uint32 StructSize =
		sizeof(FAngelscriptJITProviderBackendAttemptDiagnostic);
	const TCHAR* BackendId = TEXT("");
	const TCHAR* Disposition = TEXT("Unsupported");
	const TCHAR* Detail = TEXT("");
	EAngelscriptJITProviderDiagnosticAttemptFlags Flags =
		EAngelscriptJITProviderDiagnosticAttemptFlags::None;
	const TCHAR* FallbackReason = TEXT("");
	const TCHAR* TypedDetail = TEXT("");
	FAngelscriptJITProviderDiagnosticSource ProcessedSource;
	int32 PrimarySemanticFunctionIndex = INDEX_NONE;
	uint32 SemanticFunctionStart = 0;
	uint32 SemanticFunctionCount = 0;
	uint32 CallStart = 0;
	uint32 CallCount = 0;
	EAngelscriptJITExecutionCapability RequiredCapabilities =
		EAngelscriptJITExecutionCapability::None;
	EAngelscriptJITExecutionCapability AvailableCapabilities =
		EAngelscriptJITExecutionCapability::None;
	FAngelscriptHash256 ExecutionRequirementsIdentity;
	FAngelscriptHash256 AvailableCapabilityProfileIdentity;
	const TCHAR* ProfileKind = TEXT("Unavailable");
	const TCHAR* ExceptionControlState = TEXT("Unavailable");
	const TCHAR* ExceptionPayloadState = TEXT("Unavailable");
	uint32 bRecursionGuardAvailable = 0;
};

/** One packaged function row and its contiguous backend-attempt range. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderFunctionDiagnostic
{
	uint32 StructSize = sizeof(FAngelscriptJITProviderFunctionDiagnostic);
	FAngelscriptStableFunctionKey FunctionKey;
	const TCHAR* ActualBackendId = TEXT("");
	const TCHAR* Disposition = TEXT("Unsupported");
	const TCHAR* Detail = TEXT("");
	uint32 BackendAttemptStart = 0;
	uint32 BackendAttemptCount = 0;
};

/**
 * Optional immutable flat diagnostic catalog attached to one Provider view.
 * It is diagnostic metadata only and does not participate in artifact,
 * function, profile, entry-ABI, reference-slot, or route identity.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderDiagnostics
{
	uint32 StructSize = sizeof(FAngelscriptJITProviderDiagnostics);
	uint32 SchemaRevision =
		FAngelscriptJITProviderDiagnosticsAbi::SchemaRevision;
	FAngelscriptHash256 DiagnosticDigest;
	const TCHAR* RequestKind = TEXT("StaticJITArtifact");
	const TCHAR* RequestedBackendId = TEXT("");
	const TCHAR* CaptureState = TEXT("Bytecode");
	const TCHAR* const* FallbackChain = nullptr;
	uint32 FallbackChainCount = 0;
	uint32 SourceCompilePassCount = 0;
	uint32 CompiledModuleCount = 0;
	uint32 CompiledFunctionCount = 0;
	uint32 CompiledTypeCount = 0;
	uint32 CompiledGlobalCount = 0;
	uint32 CompiledDescriptorCount = 0;
	uint32 CompiledDependencyCount = 0;
	uint32 CompiledExternalNativeCallCount = 0;
	uint32 EmitModuleCount = 0;
	uint32 bHasVerifiedCanonicalAST = 0;
	const TCHAR* TaskError = TEXT("");
	const FAngelscriptJITProviderFunctionDiagnostic* Functions = nullptr;
	uint32 FunctionCount = 0;
	const FAngelscriptJITProviderBackendAttemptDiagnostic* BackendAttempts =
		nullptr;
	uint32 BackendAttemptCount = 0;
	const FAngelscriptJITProviderCanonicalFunctionDiagnostic*
		SemanticFunctions = nullptr;
	uint32 SemanticFunctionCount = 0;
	const FAngelscriptJITProviderTypedCallDiagnostic* Calls = nullptr;
	uint32 CallCount = 0;
};

static_assert(std::is_standard_layout_v<FAngelscriptJITProviderDiagnosticSource>);
static_assert(std::is_standard_layout_v<
	FAngelscriptJITProviderCanonicalFunctionDiagnostic>);
static_assert(std::is_standard_layout_v<
	FAngelscriptJITProviderTypedCallDiagnostic>);
static_assert(std::is_standard_layout_v<
	FAngelscriptJITProviderBackendAttemptDiagnostic>);
static_assert(std::is_standard_layout_v<
	FAngelscriptJITProviderFunctionDiagnostic>);
static_assert(std::is_standard_layout_v<FAngelscriptJITProviderDiagnostics>);
