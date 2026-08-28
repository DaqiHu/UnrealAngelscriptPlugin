#pragma once

#include "CoreMinimal.h"
#include "StaticJIT/AngelscriptJITProvider.h"

struct FAngelscriptTypedASTJITBoundCallSite;
struct FScriptExecution;

namespace AngelscriptTestJITProbes
{
	enum class EIsolatedDifferentialBackend : uint8
	{
		BytecodeJIT = 0,
		TypedASTJIT,
	};

	enum class EIsolatedDifferentialExecutionBackend : uint8
	{
		InterpreterVM = 0,
		BytecodeJIT,
		TypedASTJIT,
	};

	enum class EIsolatedDifferentialEntryRoute : uint8
	{
		VM = 0,
		Raw,
		Parms,
	};

	/**
	 * Test-carrier view of the compiler-owned cleanup proof. The isolated
	 * Bytecode/interpreter route has no TypedAST cleanup plan, while the first
	 * TypedASTJIT scalar slice accepts only a verified plan with no actions.
	 */
	enum class EIsolatedDifferentialCleanupPlanState : uint8
	{
		NotCaptured = 0,
		VerifiedEmpty,
	};

	struct FIsolatedDifferentialFailurePayload
	{
		bool bHasFailure = false;
		bool bFromJITRecord = false;
		bool bReported = false;
		bool bAdopted = false;
		int32 FunctionId = INDEX_NONE;
		FString Message;
		FString StableFunctionIdentity;
		FString FunctionDeclaration;
		FString Section;
		int32 Row = INDEX_NONE;
		int32 Column = INDEX_NONE;
		FString RouteOrigin;
		FString BackendOrigin;
	};

	struct FIsolatedDifferentialRouteResult
	{
		EIsolatedDifferentialExecutionBackend Backend =
			EIsolatedDifferentialExecutionBackend::InterpreterVM;
		EIsolatedDifferentialEntryRoute Route =
			EIsolatedDifferentialEntryRoute::VM;
		int32 ExecuteStatus = 0;
		int32 Value = 0;
		bool bExceptionThrown = false;
		FIsolatedDifferentialFailurePayload Failure;
		EIsolatedDifferentialCleanupPlanState CleanupPlanState =
			EIsolatedDifferentialCleanupPlanState::NotCaptured;
		uint32 CleanupActionCount = 0;
		TArray<FString> CleanupTrace;
	};

	struct FIsolatedDifferentialEntryView
	{
		EIsolatedDifferentialBackend Backend =
			EIsolatedDifferentialBackend::BytecodeJIT;
		asJITFunction VMEntry = nullptr;
		asJITFunction_Raw RawEntry = nullptr;
		asJITFunction_ParmsEntry ParmsEntry = nullptr;
		FString SymbolPrefix;
		FString StableFunctionKey;
		FString CanonicalDeclaration;
		FString SourceSection;
		uint32 SourceLine = 0;
		uint32 SourceColumn = 0;
		EIsolatedDifferentialCleanupPlanState CleanupPlanState =
			EIsolatedDifferentialCleanupPlanState::NotCaptured;
		uint32 CleanupActionCount = 0;
		TArray<FAngelscriptJITReferenceSlot> ReferenceSlots;
	};

	using FTypedASTScalarBranchProbe = int32(*)(int32, int32);
	using FTypedASTMutationProbe = int32(*)(int32, int32);
	using FTypedASTPowerFloatProbe = float(*)(
		FScriptExecution&, float, float);
	using FTypedASTPowerDoubleProbe = double(*)(
		FScriptExecution&, double, double);
	using FTypedASTPowerDoubleIntProbe = double(*)(
		FScriptExecution&, double, int32);
	using FTypedASTStructuredControlFlowProbe = int32(*)(int32, int32);
	using FTypedASTExhaustiveEnumProbe = int32(*)(
		FScriptExecution&, int32);
	ANGELSCRIPTTESTJIT_API const FAngelscriptJITProviderId&
		GetStableProviderId();
	ANGELSCRIPTTESTJIT_API void Reset();
	ANGELSCRIPTTESTJIT_API void Record(const int32 Value);
	ANGELSCRIPTTESTJIT_API int32 GetCallCount();
	ANGELSCRIPTTESTJIT_API int32 GetLastValue();
	ANGELSCRIPTTESTJIT_API int32 ExecuteTypedASTScalarBranch(int32 A, int32 B);
	ANGELSCRIPTTESTJIT_API void ResetTypedASTScalarBranchProbeCount();
	ANGELSCRIPTTESTJIT_API int32 GetTypedASTScalarBranchProbeCount();
	ANGELSCRIPTTESTJIT_API void RegisterTypedASTScalarBranchProbe(
		FTypedASTScalarBranchProbe Probe);
	ANGELSCRIPTTESTJIT_API bool HasTypedASTMutationPowerProbes();
	ANGELSCRIPTTESTJIT_API void ResetTypedASTMutationPowerProbeCount();
	ANGELSCRIPTTESTJIT_API int32 GetTypedASTMutationPowerProbeCount();
	ANGELSCRIPTTESTJIT_API int32 ExecuteTypedASTMutation(
		int32 Input, int32 Source);
	ANGELSCRIPTTESTJIT_API float ExecuteTypedASTPowerFloat(
		float Base, float Exponent);
	ANGELSCRIPTTESTJIT_API double ExecuteTypedASTPowerDouble(
		double Base, double Exponent);
	ANGELSCRIPTTESTJIT_API double ExecuteTypedASTPowerDoubleInt(
		double Base, int32 Exponent);
	ANGELSCRIPTTESTJIT_API void RegisterTypedASTMutationPowerProbes(
		FTypedASTMutationProbe MutationProbe,
		FTypedASTPowerFloatProbe PowerFloatProbe,
		FTypedASTPowerDoubleProbe PowerDoubleProbe,
		FTypedASTPowerDoubleIntProbe PowerDoubleIntProbe);
	ANGELSCRIPTTESTJIT_API bool HasTypedASTControlFlowProbes();
	ANGELSCRIPTTESTJIT_API void ResetTypedASTControlFlowProbeCount();
	ANGELSCRIPTTESTJIT_API int32 GetTypedASTControlFlowProbeCount();
	ANGELSCRIPTTESTJIT_API int32 ExecuteTypedASTStructuredControlFlow(
		int32 Limit, int32 Selector);
	ANGELSCRIPTTESTJIT_API int32 ExecuteTypedASTExhaustiveEnum(
		int32 Mode, bool& bOutExceptionThrown);
	ANGELSCRIPTTESTJIT_API void RegisterTypedASTControlFlowProbes(
		FTypedASTStructuredControlFlowProbe StructuredControlFlowProbe,
		FTypedASTExhaustiveEnumProbe ExhaustiveEnumProbe);
	ANGELSCRIPTTESTJIT_API bool HasTypedASTScalarProviderEntries();
	ANGELSCRIPTTESTJIT_API int32 ExecuteTypedASTScalarProviderRaw(
		int32 A, int32 B);
	ANGELSCRIPTTESTJIT_API int32 ExecuteTypedASTScalarProviderVM(
		int32 A, int32 B);
	ANGELSCRIPTTESTJIT_API int32 ExecuteTypedASTScalarProviderParms(
		int32 A, int32 B);
	ANGELSCRIPTTESTJIT_API void RegisterTypedASTScalarProviderEntries(
		asJITFunction VMEntry,
		asJITFunction_Raw RawEntry,
		asJITFunction_ParmsEntry ParmsEntry);
	ANGELSCRIPTTESTJIT_API bool HasTypedASTCapabilityProviderEntries();
	ANGELSCRIPTTESTJIT_API int32 ExecuteTypedASTCapabilityProviderRaw(
		int32 A, int32 B, bool bEnabled, bool bExtraCheck);
	ANGELSCRIPTTESTJIT_API int32 ExecuteTypedASTCapabilityProviderVM(
		int32 A, int32 B, bool bEnabled, bool bExtraCheck);
	ANGELSCRIPTTESTJIT_API int32 ExecuteTypedASTCapabilityProviderParms(
		int32 A, int32 B, bool bEnabled, bool bExtraCheck);
	ANGELSCRIPTTESTJIT_API void RegisterTypedASTCapabilityProviderEntries(
		asJITFunction VMEntry,
		asJITFunction_Raw RawEntry,
		asJITFunction_ParmsEntry ParmsEntry);
	/**
	 * Test-only entries produced by independent StaticJIT backend tasks.
	 * VM and Raw are mandatory. Parms is optional because BytecodeJIT must
	 * fail closed for reflected layouts that contain a ClassGenerator-only
	 * generated WorldContext suffix.
	 */
	ANGELSCRIPTTESTJIT_API bool HasIsolatedBytecodeDifferentialEntries();
	ANGELSCRIPTTESTJIT_API bool HasIsolatedTypedASTDifferentialEntries();
	ANGELSCRIPTTESTJIT_API FString GetIsolatedBytecodeDifferentialSymbolPrefix();
	ANGELSCRIPTTESTJIT_API FString GetIsolatedTypedASTDifferentialSymbolPrefix();
	ANGELSCRIPTTESTJIT_API FString GetIsolatedBytecodeDifferentialFunctionKey();
	ANGELSCRIPTTESTJIT_API FString GetIsolatedTypedASTDifferentialFunctionKey();
	ANGELSCRIPTTESTJIT_API bool FindIsolatedDifferentialEntry(
		EIsolatedDifferentialBackend Backend,
		FStringView CanonicalDeclaration,
		FIsolatedDifferentialEntryView& OutEntry);
	ANGELSCRIPTTESTJIT_API void RegisterIsolatedDifferentialEntry(
		EIsolatedDifferentialBackend Backend,
		asJITFunction VMEntry,
		asJITFunction_Raw RawEntry,
		asJITFunction_ParmsEntry ParmsEntry,
		const TCHAR* SymbolPrefix,
		const TCHAR* StableFunctionKey,
		const TCHAR* CanonicalDeclaration,
		const TCHAR* SourceSection = TEXT(""),
		uint32 SourceLine = 0,
		uint32 SourceColumn = 0,
		EIsolatedDifferentialCleanupPlanState CleanupPlanState =
			EIsolatedDifferentialCleanupPlanState::NotCaptured,
		uint32 CleanupActionCount = 0,
		const FAngelscriptJITReferenceSlot* ReferenceSlots = nullptr,
		uint32 ReferenceSlotCount = 0);
	ANGELSCRIPTTESTJIT_API asJITFunction_Raw
		GetIsolatedBytecodeDifferentialRawEntry();
	ANGELSCRIPTTESTJIT_API asJITFunction_Raw
		GetIsolatedTypedASTDifferentialRawEntry();
	ANGELSCRIPTTESTJIT_API int32 ExecuteIsolatedBytecodeDifferentialRaw(
		int32 A, int32 B);
	ANGELSCRIPTTESTJIT_API int32 ExecuteIsolatedBytecodeDifferentialVM(
		int32 A, int32 B);
	ANGELSCRIPTTESTJIT_API int32 ExecuteIsolatedBytecodeDifferentialParms(
		int32 A, int32 B);
	ANGELSCRIPTTESTJIT_API int32 ExecuteIsolatedTypedASTDifferentialRaw(
		int32 A, int32 B);
	ANGELSCRIPTTESTJIT_API int32 ExecuteIsolatedTypedASTDifferentialVM(
		int32 A, int32 B);
	ANGELSCRIPTTESTJIT_API int32 ExecuteIsolatedTypedASTDifferentialParms(
		int32 A, int32 B);
	ANGELSCRIPTTESTJIT_API void RegisterIsolatedBytecodeDifferentialEntries(
		asJITFunction VMEntry,
		asJITFunction_Raw RawEntry,
		asJITFunction_ParmsEntry ParmsEntry,
		const TCHAR* SymbolPrefix,
		const TCHAR* StableFunctionKey,
		const TCHAR* CanonicalDeclaration);
	ANGELSCRIPTTESTJIT_API void RegisterIsolatedTypedASTDifferentialEntries(
		asJITFunction VMEntry,
		asJITFunction_Raw RawEntry,
		asJITFunction_ParmsEntry ParmsEntry,
		const TCHAR* SymbolPrefix,
		const TCHAR* StableFunctionKey,
		const TCHAR* CanonicalDeclaration);
	/** Separate-DLL instantiation/import proof for the typed Runtime bridge. */
	ANGELSCRIPTTESTJIT_API int32 ExecuteTypedASTNativeBridge(
		FScriptExecution& Execution,
		const FAngelscriptTypedASTJITBoundCallSite& CallSite,
		int32 Left,
		int32 Right);
}
