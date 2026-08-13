#include "RuntimeJIT/AngelscriptJITConfiguration.h"

bool FAngelscriptJITConfiguration::TryParseExecutionMode(
	const FStringView Text,
	EAngelscriptJITExecutionMode& OutMode)
{
	if (Text.Equals(TEXTVIEW("auto"), ESearchCase::IgnoreCase))
	{
		OutMode = EAngelscriptJITExecutionMode::Auto;
		return true;
	}
	if (Text.Equals(TEXTVIEW("vm"), ESearchCase::IgnoreCase))
	{
		OutMode = EAngelscriptJITExecutionMode::VMOnly;
		return true;
	}
	if (Text.Equals(TEXTVIEW("aot"), ESearchCase::IgnoreCase))
	{
		OutMode = EAngelscriptJITExecutionMode::StaticAOTOnly;
		return true;
	}
	if (Text.Equals(TEXTVIEW("runtime"), ESearchCase::IgnoreCase))
	{
		OutMode = EAngelscriptJITExecutionMode::RuntimeOnly;
		return true;
	}
	return false;
}

bool FAngelscriptJITConfiguration::TryParseCompilePolicy(
	const FStringView Text,
	EAngelscriptRuntimeJITCompilePolicy& OutPolicy)
{
	if (Text.Equals(TEXTVIEW("eager-sync"), ESearchCase::IgnoreCase))
	{
		OutPolicy = EAngelscriptRuntimeJITCompilePolicy::EagerSync;
		return true;
	}
	if (Text.Equals(TEXTVIEW("eager-background"), ESearchCase::IgnoreCase))
	{
		OutPolicy = EAngelscriptRuntimeJITCompilePolicy::EagerBackground;
		return true;
	}
	if (Text.Equals(TEXTVIEW("lazy-first-call"), ESearchCase::IgnoreCase))
	{
		OutPolicy = EAngelscriptRuntimeJITCompilePolicy::LazyFirstCall;
		return true;
	}
	return false;
}

bool FAngelscriptJITConfiguration::IsValidExecutionMode(
	const EAngelscriptJITExecutionMode Mode)
{
	switch (Mode)
	{
	case EAngelscriptJITExecutionMode::Auto:
	case EAngelscriptJITExecutionMode::VMOnly:
	case EAngelscriptJITExecutionMode::StaticAOTOnly:
	case EAngelscriptJITExecutionMode::RuntimeOnly:
		return true;
	default:
		return false;
	}
}

bool FAngelscriptJITConfiguration::IsValidCompilePolicy(
	const EAngelscriptRuntimeJITCompilePolicy Policy)
{
	switch (Policy)
	{
	case EAngelscriptRuntimeJITCompilePolicy::EagerSync:
	case EAngelscriptRuntimeJITCompilePolicy::EagerBackground:
	case EAngelscriptRuntimeJITCompilePolicy::LazyFirstCall:
		return true;
	default:
		return false;
	}
}

bool FAngelscriptJITConfiguration::IsValidRuntimePlatform(
	const EAngelscriptRuntimeJITPlatform Platform)
{
	return Platform == EAngelscriptRuntimeJITPlatform::Win64
		|| Platform == EAngelscriptRuntimeJITPlatform::Linux
		|| Platform == EAngelscriptRuntimeJITPlatform::Mac;
}

bool FAngelscriptJITConfiguration::IsValidRuntimeConfiguration(
	const EAngelscriptRuntimeJITBuildConfiguration Configuration)
{
	return Configuration
			== EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment
		|| Configuration
			== EAngelscriptRuntimeJITBuildConfiguration::GameDevelopment
		|| Configuration
			== EAngelscriptRuntimeJITBuildConfiguration::Shipping;
}

FAngelscriptJITTierSelection FAngelscriptJITConfiguration::SelectTier(
	const EAngelscriptJITExecutionMode Mode,
	const bool bHasExactStaticAOT,
	const bool bHasExactRuntime,
	const bool bConfigurationValid)
{
	FAngelscriptJITTierSelection Result;
	Result.bConfigurationValid = bConfigurationValid;
	if (!bConfigurationValid)
	{
		return Result;
	}

	switch (Mode)
	{
	case EAngelscriptJITExecutionMode::Auto:
		Result.Tier = bHasExactStaticAOT
			? EAngelscriptJITExecutionTier::StaticAOT
			: bHasExactRuntime
				? EAngelscriptJITExecutionTier::Runtime
				: EAngelscriptJITExecutionTier::VM;
		break;
	case EAngelscriptJITExecutionMode::VMOnly:
		Result.Tier = EAngelscriptJITExecutionTier::VM;
		break;
	case EAngelscriptJITExecutionMode::StaticAOTOnly:
		Result.Tier = bHasExactStaticAOT
			? EAngelscriptJITExecutionTier::StaticAOT
			: EAngelscriptJITExecutionTier::VM;
		break;
	case EAngelscriptJITExecutionMode::RuntimeOnly:
		Result.Tier = bHasExactRuntime
			? EAngelscriptJITExecutionTier::Runtime
			: EAngelscriptJITExecutionTier::VM;
		break;
	default:
		Result.bConfigurationValid = false;
		Result.Tier = EAngelscriptJITExecutionTier::VM;
		break;
	}
	return Result;
}
