#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITExecutionProfile.h"

#if AS_CAN_GENERATE_JIT

namespace AngelscriptTypedASTJITExecutionProfile_Private
{
	using ECapability = EAngelscriptTypedASTJITDirectCapability;

	static constexpr uint16 KnownCapabilityBits =
		static_cast<uint16>(ECapability::FramePosition)
		| static_cast<uint16>(ECapability::LineCallback)
		| static_cast<uint16>(ECapability::DebuggerStep)
		| static_cast<uint16>(ECapability::DebuggerLocals)
		| static_cast<uint16>(ECapability::Coverage)
		| static_cast<uint16>(ECapability::LoopTimeout)
		| static_cast<uint16>(ECapability::AbortSuspend)
		| static_cast<uint16>(ECapability::RecursionBudget);

	static bool HasOnlyKnownCapabilities(const ECapability Capabilities)
	{
		return (static_cast<uint16>(Capabilities) & ~KnownCapabilityBits) == 0;
	}

	static FAngelscriptHash256 BuildCapabilityIdentity(
		const TCHAR* Domain,
		const uint32 SchemaRevision,
		const ECapability Capabilities)
	{
		FAngelscriptArtifactCanonicalWriter Writer(Domain);
		Writer.WriteUInt32(SchemaRevision);
		Writer.WriteUInt16(static_cast<uint16>(Capabilities));
		return Writer.FinalizeHash();
	}

	static ECapability BuildRequiredCapabilities(
		const FAngelscriptTypedASTJITExecutionRequirementsInput& Input)
	{
		ECapability Required = ECapability::None;
		if (Input.bRequireFramePosition)
		{
			Required |= ECapability::FramePosition;
		}
		if (Input.bBreakpointActive || Input.bStepActive)
		{
			Required |= ECapability::LineCallback
				| ECapability::DebuggerStep;
		}
		if (Input.bDebuggerLocalsRequired)
		{
			Required |= ECapability::DebuggerLocals;
		}
		if (Input.bCoverageRecording)
		{
			Required |= ECapability::LineCallback | ECapability::Coverage;
		}
		if (Input.bLoopTimeoutRequired)
		{
			Required |= ECapability::LoopTimeout;
		}
		if (Input.bAbortOrSuspendPollingRequired)
		{
			Required |= ECapability::AbortSuspend;
		}
		if (Input.bRecursionBudgetRequired)
		{
			Required |= ECapability::RecursionBudget;
		}
		return Required;
	}

	static bool IsControlCapability(const ECapability Capabilities)
	{
		constexpr ECapability ControlCapabilities =
			ECapability::LoopTimeout
			| ECapability::AbortSuspend
			| ECapability::RecursionBudget;
		return EnumHasAnyFlags(Capabilities, ControlCapabilities);
	}

	static const TCHAR* StableMissingDetail(
		const ECapability Missing,
		const bool bDirectCalleeMismatch)
	{
		if (bDirectCalleeMismatch)
		{
			return TEXT("DirectCalleeProfileMismatch");
		}
		// Control requirements win when several independent requirements are
		// missing because the fallback reason is classified as control too.
		if (EnumHasAnyFlags(Missing, ECapability::RecursionBudget))
		{
			return TEXT("RecursionGuardUnavailable");
		}
		if (EnumHasAnyFlags(Missing, ECapability::LoopTimeout))
		{
			return TEXT("LoopTimeoutSafepointUnavailable");
		}
		if (EnumHasAnyFlags(Missing, ECapability::AbortSuspend))
		{
			return TEXT("AbortSuspendSafepointUnavailable");
		}
		if (EnumHasAnyFlags(Missing, ECapability::Coverage))
		{
			return TEXT("CoverageHookUnavailable");
		}
		if (EnumHasAnyFlags(Missing, ECapability::DebuggerLocals))
		{
			return TEXT("DebuggerLocalsUnavailable");
		}
		if (EnumHasAnyFlags(Missing, ECapability::DebuggerStep))
		{
			return TEXT("DebuggerStepUnavailable");
		}
		if (EnumHasAnyFlags(Missing, ECapability::LineCallback))
		{
			return TEXT("LineCallbackUnavailable");
		}
		if (EnumHasAnyFlags(Missing, ECapability::FramePosition))
		{
			return TEXT("FramePositionUnavailable");
		}
		return TEXT("ExecutionCapabilityUnavailable");
	}
}

FString LexToString(
	const EAngelscriptTypedASTJITDirectCapability Capabilities)
{
	using ECapability = EAngelscriptTypedASTJITDirectCapability;
	TArray<FString> Tokens;
	auto Add = [&Tokens, Capabilities](
		const ECapability Capability,
		const TCHAR* Token)
	{
		if (EnumHasAnyFlags(Capabilities, Capability))
		{
			Tokens.Add(Token);
		}
	};
	Add(ECapability::FramePosition, TEXT("FramePosition"));
	Add(ECapability::LineCallback, TEXT("LineCallback"));
	Add(ECapability::DebuggerStep, TEXT("DebuggerStep"));
	Add(ECapability::DebuggerLocals, TEXT("DebuggerLocals"));
	Add(ECapability::Coverage, TEXT("Coverage"));
	Add(ECapability::LoopTimeout, TEXT("LoopTimeout"));
	Add(ECapability::AbortSuspend, TEXT("AbortSuspend"));
	Add(ECapability::RecursionBudget, TEXT("RecursionBudget"));
	return Tokens.IsEmpty() ? TEXT("None") : FString::Join(Tokens, TEXT("|"));
}

bool FAngelscriptTypedASTJITGenerationCapabilityProfile::IsValid() const
{
	using namespace AngelscriptTypedASTJITExecutionProfile_Private;
	return HasOnlyKnownCapabilities(AvailableCapabilities)
		&& !IdentityHash.IsZero()
		&& IdentityHash == BuildCapabilityIdentity(
			TEXT("typed-ast-jit-generation-capabilities"),
			SchemaRevision, AvailableCapabilities);
}

FString
FAngelscriptTypedASTJITGenerationCapabilityProfile::DumpNormalized() const
{
	return FString::Printf(TEXT("schema=%u available=%s identity=%s"),
		SchemaRevision,
		*LexToString(AvailableCapabilities),
		*IdentityHash.ToHexString());
}

bool FAngelscriptTypedASTJITExecutionRequirementsSnapshot::IsValid() const
{
	using namespace AngelscriptTypedASTJITExecutionProfile_Private;
	return HasOnlyKnownCapabilities(RequiredCapabilities)
		&& !IdentityHash.IsZero()
		&& IdentityHash == BuildCapabilityIdentity(
			TEXT("typed-ast-jit-execution-requirements"),
			SchemaRevision, RequiredCapabilities);
}

FString
FAngelscriptTypedASTJITExecutionRequirementsSnapshot::DumpNormalized() const
{
	return FString::Printf(TEXT("schema=%u required=%s identity=%s"),
		SchemaRevision,
		*LexToString(RequiredCapabilities),
		*IdentityHash.ToHexString());
}

bool BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
	const EAngelscriptTypedASTJITDirectCapability AvailableCapabilities,
	FAngelscriptTypedASTJITGenerationCapabilityProfile& OutProfile,
	FString* OutError)
{
	using namespace AngelscriptTypedASTJITExecutionProfile_Private;
	OutProfile = {};
	if (!HasOnlyKnownCapabilities(AvailableCapabilities))
	{
		if (OutError != nullptr)
		{
			*OutError = FString::Printf(TEXT(
				"TypedASTJITUnknownCapabilityBits: Available=0x%04x Known=0x%04x"),
				static_cast<uint16>(AvailableCapabilities),
				KnownCapabilityBits);
		}
		return false;
	}
	OutProfile.AvailableCapabilities = AvailableCapabilities;
	OutProfile.IdentityHash = BuildCapabilityIdentity(
		TEXT("typed-ast-jit-generation-capabilities"),
		FAngelscriptTypedASTJITGenerationCapabilityProfile::SchemaRevision,
		AvailableCapabilities);
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	return true;
}

bool BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
	const FAngelscriptTypedASTJITExecutionRequirementsInput& Input,
	FAngelscriptTypedASTJITExecutionRequirementsSnapshot& OutSnapshot,
	FString* OutError)
{
	using namespace AngelscriptTypedASTJITExecutionProfile_Private;
	return BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
		BuildRequiredCapabilities(Input), OutSnapshot, OutError);
}

bool BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
	const EAngelscriptTypedASTJITDirectCapability RequiredCapabilities,
	FAngelscriptTypedASTJITExecutionRequirementsSnapshot& OutSnapshot,
	FString* OutError)
{
	using namespace AngelscriptTypedASTJITExecutionProfile_Private;
	OutSnapshot = {};
	if (!HasOnlyKnownCapabilities(RequiredCapabilities))
	{
		if (OutError != nullptr)
		{
			*OutError = FString::Printf(TEXT(
				"TypedASTJITUnknownRequirementBits: Required=0x%04x Known=0x%04x"),
				static_cast<uint16>(RequiredCapabilities),
				KnownCapabilityBits);
		}
		return false;
	}
	OutSnapshot.RequiredCapabilities = RequiredCapabilities;
	OutSnapshot.IdentityHash = BuildCapabilityIdentity(
		TEXT("typed-ast-jit-execution-requirements"),
		FAngelscriptTypedASTJITExecutionRequirementsSnapshot::SchemaRevision,
		RequiredCapabilities);
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	return true;
}

FAngelscriptTypedASTJITExecutionRouteEligibility
EvaluateAngelscriptTypedASTJITExecutionRoute(
	const FAngelscriptTypedASTJITGenerationCapabilityProfile& Profile,
	const FAngelscriptTypedASTJITExecutionRequirementsSnapshot& Requirements,
	const bool bDirectCalleeMismatch)
{
	using namespace AngelscriptTypedASTJITExecutionProfile_Private;
	FAngelscriptTypedASTJITExecutionRouteEligibility Result;
	if (!Profile.IsValid())
	{
		Result.StableDetail = TEXT("InvalidExecutionCapabilityProfile");
		return Result;
	}
	if (!Requirements.IsValid())
	{
		Result.StableDetail = TEXT("InvalidExecutionRequirementsSnapshot");
		return Result;
	}

	const uint16 RequiredBits = static_cast<uint16>(
		Requirements.RequiredCapabilities);
	const uint16 AvailableBits = static_cast<uint16>(
		Profile.AvailableCapabilities);
	Result.MissingCapabilities =
		static_cast<EAngelscriptTypedASTJITDirectCapability>(
			RequiredBits & ~AvailableBits);
	if (Result.MissingCapabilities
		== EAngelscriptTypedASTJITDirectCapability::None)
	{
		Result.Route = EAngelscriptTypedASTJITExecutionRoute::TypedASTJIT;
		Result.Reason = EAngelscriptTypedASTJITFallbackReason::None;
		Result.StableDetail = TEXT("ExecutionCapabilitiesSatisfied");
		return Result;
	}

	Result.Reason = IsControlCapability(Result.MissingCapabilities)
		? EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionControl
		: EAngelscriptTypedASTJITFallbackReason::
			UnsupportedExecutionObservability;
	Result.StableDetail = StableMissingDetail(
		Result.MissingCapabilities, bDirectCalleeMismatch);
	return Result;
}

FAngelscriptHash256 BuildAngelscriptTypedASTJITProfiledContentIdentity(
	const FAngelscriptHash256& SourceExecutionHash,
	const FAngelscriptTypedASTJITGenerationCapabilityProfile& Profile)
{
	if (SourceExecutionHash.IsZero() || !Profile.IsValid())
	{
		return {};
	}
	FAngelscriptArtifactCanonicalWriter Writer(
		TEXT("typed-ast-jit-profiled-content"));
	Writer.WriteUInt32(
		FAngelscriptTypedASTJITGenerationCapabilityProfile::SchemaRevision);
	Writer.WriteHash(SourceExecutionHash);
	Writer.WriteHash(Profile.IdentityHash);
	return Writer.FinalizeHash();
}

#endif // AS_CAN_GENERATE_JIT
