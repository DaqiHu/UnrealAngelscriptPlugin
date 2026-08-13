#pragma once

#include "CoreMinimal.h"

#include "JIT/AngelscriptRuntimeJITBackend.h"
#include "RuntimeJIT/AngelscriptRuntimeJITState.h"

enum class EAngelscriptJITExecutionMode : uint8
{
	Auto = 0,
	VMOnly,
	StaticAOTOnly,
	RuntimeOnly,
};

enum class EAngelscriptJITExecutionTier : uint8
{
	VM = 0,
	StaticAOT,
	Runtime,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITTierSelection
{
	EAngelscriptJITExecutionTier Tier = EAngelscriptJITExecutionTier::VM;
	bool bConfigurationValid = true;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITCoordinatorConfig
{
	EAngelscriptJITExecutionMode ExecutionMode =
		EAngelscriptJITExecutionMode::Auto;
	FAngelscriptRuntimeJITBackendId RuntimeBackendId;
	EAngelscriptRuntimeJITCompilePolicy RuntimeCompilePolicy =
		EAngelscriptRuntimeJITCompilePolicy::EagerSync;
	EAngelscriptRuntimeJITPlatform RuntimePlatform =
		EAngelscriptRuntimeJITPlatform::None;
	EAngelscriptRuntimeJITBuildConfiguration RuntimeConfiguration =
		EAngelscriptRuntimeJITBuildConfiguration::None;
	bool bExecutionModeValid = true;
	bool bRuntimeConfigurationValid = true;
	FString ConfigurationError;
};

namespace FAngelscriptJITConfiguration
{
	ANGELSCRIPTRUNTIME_API bool TryParseExecutionMode(
		FStringView Text,
		EAngelscriptJITExecutionMode& OutMode);

	ANGELSCRIPTRUNTIME_API bool TryParseCompilePolicy(
		FStringView Text,
		EAngelscriptRuntimeJITCompilePolicy& OutPolicy);

	ANGELSCRIPTRUNTIME_API bool IsValidExecutionMode(
		EAngelscriptJITExecutionMode Mode);

	ANGELSCRIPTRUNTIME_API bool IsValidCompilePolicy(
		EAngelscriptRuntimeJITCompilePolicy Policy);

	ANGELSCRIPTRUNTIME_API bool IsValidRuntimePlatform(
		EAngelscriptRuntimeJITPlatform Platform);

	ANGELSCRIPTRUNTIME_API bool IsValidRuntimeConfiguration(
		EAngelscriptRuntimeJITBuildConfiguration Configuration);

	ANGELSCRIPTRUNTIME_API FAngelscriptJITTierSelection SelectTier(
		EAngelscriptJITExecutionMode Mode,
		bool bHasExactStaticAOT,
		bool bHasExactRuntime,
		bool bConfigurationValid = true);
}
