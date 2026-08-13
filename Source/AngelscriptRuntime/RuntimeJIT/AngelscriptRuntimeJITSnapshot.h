#pragma once

#include "CoreMinimal.h"

#include "Core/Artifacts/AngelscriptFunctionRoute.h"
#include "JIT/AngelscriptRuntimeJITBackend.h"

enum class EAngelscriptRuntimeJITSnapshotCaptureCode : uint8
{
	Captured = 0,
	InvalidInput,
	InvalidRouteIdentity,
	UnsupportedReferenceOperand,
	MalformedBytecode,
	InvalidFrameLayout,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITSnapshotCaptureResult
{
	EAngelscriptRuntimeJITSnapshotCaptureCode Code =
		EAngelscriptRuntimeJITSnapshotCaptureCode::InvalidInput;
	EAngelscriptRuntimeJITDiagnosticReason Reason =
		EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot;
	uint32 BytecodeOffset = MAX_uint32;
	FAngelscriptRuntimeJITContractValidationResult Validation;

	bool IsCaptured() const
	{
		return Code == EAngelscriptRuntimeJITSnapshotCaptureCode::Captured;
	}
};

/**
 * Engine-thread-only input. Every pointer is consumed synchronously by
 * Capture and is absent from the resulting worker-owned snapshot.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITSnapshotCaptureInput
{
	asIScriptFunction* Function = nullptr;
	const FAngelscriptFunctionRoute* Route = nullptr;
	FAngelscriptRuntimeJITBackendId BackendId;
	EAngelscriptRuntimeJITPlatform Platform =
		EAngelscriptRuntimeJITPlatform::None;
	EAngelscriptRuntimeJITBuildConfiguration Configuration =
		EAngelscriptRuntimeJITBuildConfiguration::None;
	uint64 EngineNamespace = 0;
	uint64 PublicationOrdinal = 0;
	uint64 CancellationGeneration = 0;
};

/**
 * Value-owned compile input. GetView() creates a transient view whose pointers
 * refer exclusively to arrays owned by this object, so copying or moving the
 * snapshot cannot retain pointers into another instance.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITCompileSnapshot
{
public:
	FAngelscriptRuntimeJITCompileSnapshot() = default;
	FAngelscriptRuntimeJITCompileSnapshot(
		const FAngelscriptRuntimeJITCompileSnapshot&) = default;
	FAngelscriptRuntimeJITCompileSnapshot(
		FAngelscriptRuntimeJITCompileSnapshot&&) = default;
	FAngelscriptRuntimeJITCompileSnapshot& operator=(
		const FAngelscriptRuntimeJITCompileSnapshot&) = default;
	FAngelscriptRuntimeJITCompileSnapshot& operator=(
		FAngelscriptRuntimeJITCompileSnapshot&&) = default;

	static FAngelscriptRuntimeJITSnapshotCaptureResult Capture(
		const FAngelscriptRuntimeJITSnapshotCaptureInput& Input,
		FAngelscriptRuntimeJITCompileSnapshot& OutSnapshot);

	static FAngelscriptRuntimeJITCompileSnapshot FromOwnedData(
		FAngelscriptRuntimeJITCompileSnapshotHeader Header,
		TArray<asDWORD> Bytecode,
		TArray<FAngelscriptRuntimeJITFrameValue> FrameValues,
		TArray<uint32> InstructionWordOffsets,
		TArray<FAngelscriptRuntimeJITControlFlowEdge> ControlFlowEdges,
		TArray<FAngelscriptRuntimeJITHelperToken> HelperTokens,
		FString DiagnosticDeclaration = {},
		FString DiagnosticSourceIdentity = {});

	FAngelscriptRuntimeJITCompileSnapshotView GetView() const;
	FAngelscriptRuntimeJITContractValidationResult Validate() const;

	const FAngelscriptRuntimeJITCompileSnapshotHeader& GetHeader() const
	{
		return Header;
	}

	const TArray<asDWORD>& GetBytecode() const { return Bytecode; }
	const TArray<FAngelscriptRuntimeJITFrameValue>& GetFrameValues() const
	{
		return FrameValues;
	}
	const TArray<uint32>& GetInstructionWordOffsets() const
	{
		return InstructionWordOffsets;
	}
	const TArray<FAngelscriptRuntimeJITControlFlowEdge>& GetControlFlowEdges()
		const
	{
		return ControlFlowEdges;
	}
	const TArray<FAngelscriptRuntimeJITHelperToken>& GetHelperTokens() const
	{
		return HelperTokens;
	}
	const FString& GetDiagnosticDeclaration() const
	{
		return DiagnosticDeclaration;
	}
	const FString& GetDiagnosticSourceIdentity() const
	{
		return DiagnosticSourceIdentity;
	}

	static FAngelscriptRuntimeJITContractValidationResult ValidateStructure(
		const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot);

private:
	FAngelscriptRuntimeJITCompileSnapshotHeader Header;
	TArray<asDWORD> Bytecode;
	TArray<FAngelscriptRuntimeJITFrameValue> FrameValues;
	TArray<uint32> InstructionWordOffsets;
	TArray<FAngelscriptRuntimeJITControlFlowEdge> ControlFlowEdges;
	TArray<FAngelscriptRuntimeJITHelperToken> HelperTokens;
	FString DiagnosticDeclaration;
	FString DiagnosticSourceIdentity;
};
