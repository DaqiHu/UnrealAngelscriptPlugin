#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptStaticJITCaptureProfile.h"
#include "StaticJIT/Backends/AngelscriptStaticJITBackend.h"

#if AS_CAN_GENERATE_JIT

struct FAngelscriptEngineConfig;

/**
 * Frozen build-time selection for one StaticJIT generation task.
 *
 * The profile is resolved before a generation Engine is created so an invalid
 * backend cannot trigger source compilation and Canonical AST capture cannot
 * be enabled late by a backend.
 */
struct FAngelscriptStaticJITGenerationProfile
{
	FAngelscriptStaticJITBackendId BackendId =
		FAngelscriptStaticJITBackendId::Bytecode();
	EAngelscriptStaticJITCaptureProfile CaptureProfile =
		EAngelscriptStaticJITCaptureProfile::Bytecode;
	FAngelscriptTypedASTJITGenerationCapabilityProfile
		TypedASTJITCapabilityProfile;
	FAngelscriptTypedASTJITExecutionRequirementsSnapshot
		TypedASTJITExecutionRequirements;

	static bool TryCreate(
		const FAngelscriptStaticJITBackendId& InBackendId,
		const EAngelscriptJITTargetProfile InTargetProfile,
		FAngelscriptStaticJITGenerationProfile& OutProfile,
		FString* OutError = nullptr)
	{
		OutProfile = {};
		if (!InBackendId.IsValid())
		{
			if (OutError != nullptr)
			{
				*OutError = TEXT(
					"StaticJIT generation BackendId must be 'bytecode' or 'typed-ast'.");
			}
			return false;
		}
		if (InTargetProfile != EAngelscriptJITTargetProfile::EditorDevelopment
			&& InTargetProfile
				!= EAngelscriptJITTargetProfile::GameDevelopment
			&& InTargetProfile != EAngelscriptJITTargetProfile::GameShipping)
		{
			if (OutError != nullptr)
			{
				*OutError = TEXT(
					"StaticJIT generation requires one concrete target profile: EditorDevelopment, GameDevelopment, or GameShipping.");
			}
			return false;
		}

		OutProfile.BackendId = InBackendId;
		const bool bTypedAST = InBackendId
			== FAngelscriptStaticJITBackendId::TypedAST();
		OutProfile.CaptureProfile = bTypedAST
			? EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST
			: EAngelscriptStaticJITCaptureProfile::Bytecode;
		EAngelscriptTypedASTJITDirectCapability AvailableCapabilities =
			EAngelscriptTypedASTJITDirectCapability::None;
		if (bTypedAST)
		{
			AvailableCapabilities =
				EAngelscriptTypedASTJITDirectCapability::RecursionBudget;
			if (InTargetProfile
				!= EAngelscriptJITTargetProfile::GameShipping)
			{
				AvailableCapabilities |=
					EAngelscriptTypedASTJITDirectCapability::FramePosition;
			}
		}
		FString CapabilityError;
		if (!BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
			AvailableCapabilities,
			OutProfile.TypedASTJITCapabilityProfile, &CapabilityError)
			|| !BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
				EAngelscriptTypedASTJITDirectCapability::None,
				OutProfile.TypedASTJITExecutionRequirements,
				&CapabilityError))
		{
			if (OutError != nullptr)
			{
				*OutError = CapabilityError;
			}
			return false;
		}
		if (OutError != nullptr)
		{
			OutError->Reset();
		}
		return true;
	}

	bool RequiresCanonicalASTSnapshot() const
	{
		return CaptureProfile
			== EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST;
	}

	/** Apply the frozen selection before FAngelscriptEngine::Create(). */
	ANGELSCRIPTRUNTIME_API void ApplyToEngineConfig(
		FAngelscriptEngineConfig& InOutConfig) const;
};

#endif
