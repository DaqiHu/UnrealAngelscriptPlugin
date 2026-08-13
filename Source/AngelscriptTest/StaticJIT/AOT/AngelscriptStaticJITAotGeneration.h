#pragma once

#include "AngelscriptEngine.h"
#include "CoreMinimal.h"

namespace AngelscriptStaticJITAotGeneration
{
	enum class EStaticJITAotGenerationMode : uint8
	{
		Generate,
		Verify,
	};

	struct FStaticJITAotGenerationResult
	{
		bool bSuccess = false;
		TArray<FString> WrittenFiles;
		TArray<FString> UnchangedFiles;
		TArray<FString> RemovedFiles;
		FString ProviderId;
		FString ProviderGeneration;
		TArray<FString> GeneratedModules;
		TArray<FString> GeneratedFunctionKeys;
		FString Error;
	};

	struct FStaticJITAotCacheV2RestoreResult
	{
		bool bSuccess = false;
		bool bConsumedPersistedCandidate = false;
		int32 CandidateModuleCount = 0;
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
	};

	ANGELSCRIPTTEST_API TUniquePtr<FStaticJITAotFixtureEngineSession>
		CreateFixtureEngineSession(
			bool bEnableCacheV2Persistence = true);

	ANGELSCRIPTTEST_API FStaticJITAotGenerationResult Run(EStaticJITAotGenerationMode Mode);
}
