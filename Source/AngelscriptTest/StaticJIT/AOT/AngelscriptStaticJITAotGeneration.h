#pragma once

#include "AngelscriptEngine.h"
#include "CoreMinimal.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"

namespace AngelscriptStaticJITAotGeneration
{
	enum class EStaticJITAotFixtureComposition : uint8
	{
		Core,
		Complete,
	};

	enum class EStaticJITAotGenerationMode : uint8
	{
		Generate,
		Verify,
	};

	/** Read-only fault/profile variants used by focused generation verification. */
	enum class EStaticJITAotGenerationTestVariant : uint8
	{
		Default,
		RecursionOnlyProfile,
		RequireCoverageWithoutCapability,
		AdvertiseUnsupportedEmitterCapability,
	};

	/** Pointer-free end-to-end observation for the emitter-failure fallback proof. */
	struct FStaticJITAotEmitterFailureFallbackObservation
	{
		bool bCaptured = false;
		bool bTypedEligibilityPassed = false;
		bool bFinalFunctionEmitted = false;
		bool bBytecodeAttemptEmitted = false;
		bool bTypedBodyPresent = false;
		int32 ProviderFunctionCountForKey = 0;
		FString FunctionKey;
		FString FinalBackendId;
		FString TypedAttemptDetail;
		FString BytecodeAttemptDetail;
	};

	/** Pointer-free identity returned only for the isolated backend oracle. */
	struct FStaticJITAotDifferentialIdentity
	{
		FString FunctionKey;
		FString ExecutionHash;
		FString DebugHash;
		FString ArtifactProfile;
		FString EntryAbiHash;

		friend bool operator==(
			const FStaticJITAotDifferentialIdentity& Left,
			const FStaticJITAotDifferentialIdentity& Right)
		{
			return Left.FunctionKey == Right.FunctionKey
				&& Left.ExecutionHash == Right.ExecutionHash
				&& Left.DebugHash == Right.DebugHash
				&& Left.ArtifactProfile == Right.ArtifactProfile
				&& Left.EntryAbiHash == Right.EntryAbiHash;
		}
	};

	struct FStaticJITAotGenerationResult
	{
		bool bSuccess = false;
		TArray<FString> WrittenFiles;
		TArray<FString> UnchangedFiles;
		TArray<FString> RemovedFiles;
		FString ProviderId;
		FString ProviderGeneration;
		FString ArtifactSetDigest;
		TArray<FString> GeneratedModules;
		TArray<FString> GeneratedFunctionKeys;
		FString TypedASTNormalizedHIR;
		FString TypedASTGeneratedCpp;
		FString IsolatedBytecodeDifferentialSymbolPrefix;
		FString IsolatedTypedASTDifferentialSymbolPrefix;
		FStaticJITAotDifferentialIdentity
			IsolatedBytecodeDifferentialIdentity;
		FStaticJITAotDifferentialIdentity
			IsolatedTypedASTDifferentialIdentity;
		FString IsolatedTypedASTImportedFallbackDetail;
		FString IsolatedTypedASTNonCloneableFallbackDetail;
		FStaticJITAotEmitterFailureFallbackObservation
			EmitterFailureFallback;
		double HirCaptureMilliseconds = 0.0;
		int64 HirWorkingSetDeltaBytes = 0;
		double IsolatedBytecodeGenerationMilliseconds = 0.0;
		double IsolatedTypedGenerationMilliseconds = 0.0;
		int32 IsolatedBytecodeGeneratedCppBytes = 0;
		int32 IsolatedTypedGeneratedCppBytes = 0;
		FString Error;
	};

	struct FStaticJITAotFirstVersionBenchmark
	{
		bool bSuccess = false;
		double HirCaptureMilliseconds = 0.0;
		int64 HirWorkingSetDeltaBytes = 0;
		double IsolatedBytecodeGenerationMilliseconds = 0.0;
		double IsolatedTypedGenerationMilliseconds = 0.0;
		int32 IsolatedBytecodeGeneratedCppBytes = 0;
		int32 IsolatedTypedGeneratedCppBytes = 0;
		double ScalarRawNanosecondsPerCall = 0.0;
		double DirectCapabilityRawNanosecondsPerCall = 0.0;
		double BridgedNativeNanosecondsPerCall = 0.0;
		int32 ScalarRawResult = 0;
		int32 DirectCapabilityRawResult = 0;
		int32 BridgedNativeResult = 0;
		FString Error;
	};

	struct FStaticJITAotCacheV2RestoreResult
	{
		bool bSuccess = false;
		bool bConsumedPersistedCandidate = false;
		int32 ExpectedCandidateModuleCount = 0;
		int32 CandidateModuleCount = 0;
		int32 ExpectedRestoredFunctionCount = 0;
		int32 RestoredFunctionCount = 0;
		int32 CompiledMissCount = 0;
		int32 NotCacheableCount = 0;
		int32 FrontendEventCount = 0;
		FString GenerationId;
		FString Detail;
	};

	// Owns the production-shaped source Engine used by both TestJIT generation
	// and runtime binding proofs. Construction materializes the committed
	// fixtures below an isolated project Script root, runs InitialCompile and
	// therefore publishes the same authoritative current Cache V2 DTO consumed
	// by Runtime provider selection. Destruction retires the Engine before
	// removing the isolated source/cache root.
	struct ANGELSCRIPTTEST_API FStaticJITAotFixtureEngineSession final
	{
		~FStaticJITAotFixtureEngineSession();

		bool IsValid() const
		{
			return Engine.IsValid() && Error.IsEmpty();
		}

		FStaticJITAotCacheV2RestoreResult RestoreFreshEngineFromCacheV2();

		TUniquePtr<FAngelscriptEngine> Engine;
		FString FixtureRoot;
		FString Error;
		EStaticJITAotFixtureComposition Composition =
			EStaticJITAotFixtureComposition::Complete;
		AngelscriptStaticJITAotFixture::
			ETypedASTPrivateBridgeRegistration PrivateBridgeRegistration =
				AngelscriptStaticJITAotFixture::
					ETypedASTPrivateBridgeRegistration::Primary;
	};

	ANGELSCRIPTTEST_API TUniquePtr<FStaticJITAotFixtureEngineSession>
		CreateFixtureEngineSession(
			bool bEnableCacheV2Persistence = true,
			EStaticJITAotFixtureComposition Composition =
				EStaticJITAotFixtureComposition::Complete,
			AngelscriptStaticJITAotFixture::
				ETypedASTPrivateBridgeRegistration PrivateBridgeRegistration =
					AngelscriptStaticJITAotFixture::
						ETypedASTPrivateBridgeRegistration::Primary);
	ANGELSCRIPTTEST_API TUniquePtr<FStaticJITAotFixtureEngineSession>
		CreateInterpreterFixtureEngineSession();
	/** Complete generation-purpose fixture used only as an interpreter oracle. */
	ANGELSCRIPTTEST_API TUniquePtr<FStaticJITAotFixtureEngineSession>
		CreateCompleteInterpreterFixtureEngineSession();
	/** Runtime fixture whose folded immutable global differs from the committed Provider. */
	ANGELSCRIPTTEST_API TUniquePtr<FStaticJITAotFixtureEngineSession>
		CreateFoldedGlobalInvalidationFixtureEngineSession();

	ANGELSCRIPTTEST_API FStaticJITAotGenerationResult Run(EStaticJITAotGenerationMode Mode);
	/**
	 * Read-only focused oracle that generates only DifferentialScalarValue once
	 * through BytecodeJIT and once through TypedASTJIT. It deliberately omits
	 * unrelated control-flow, bridge, lifetime, and checked-output fixtures.
	 */
	ANGELSCRIPTTEST_API FStaticJITAotGenerationResult
		RunBytecodeIsolationProbe();
	/** Isolated first-version timings for task 8.1. Does not publish files. */
	ANGELSCRIPTTEST_API FStaticJITAotFirstVersionBenchmark
		RunFirstVersionBenchmark();
	/** Non-default variants are deliberately Verify-only and never publish files. */
	ANGELSCRIPTTEST_API FStaticJITAotGenerationResult RunForTesting(
		EStaticJITAotGenerationMode Mode,
		EStaticJITAotGenerationTestVariant Variant);
}
