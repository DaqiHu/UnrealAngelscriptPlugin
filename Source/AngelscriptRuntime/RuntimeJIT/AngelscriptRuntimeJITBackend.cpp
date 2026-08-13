#include "JIT/AngelscriptRuntimeJITBackend.h"

namespace AngelscriptRuntimeJITBackend_Private
{
	static constexpr EAngelscriptRuntimeJITPlatform KnownPlatforms =
		EAngelscriptRuntimeJITPlatform::Win64
		| EAngelscriptRuntimeJITPlatform::Linux
		| EAngelscriptRuntimeJITPlatform::Mac;
	static constexpr EAngelscriptRuntimeJITBuildConfiguration
		KnownConfigurations =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment
			| EAngelscriptRuntimeJITBuildConfiguration::GameDevelopment
			| EAngelscriptRuntimeJITBuildConfiguration::Shipping;

	template <typename EnumType>
	static bool HasOnlyKnownFlags(const EnumType Value, const EnumType Known)
	{
		using Underlying = __underlying_type(EnumType);
		return (static_cast<Underlying>(Value)
			& ~static_cast<Underlying>(Known)) == 0;
	}

	template <typename EnumType>
	static bool IsOneFlag(const EnumType Value)
	{
		using Underlying = __underlying_type(EnumType);
		const Underlying Raw = static_cast<Underlying>(Value);
		return Raw != 0 && (Raw & (Raw - 1)) == 0;
	}

	static FAngelscriptRuntimeJITContractValidationResult Invalid(
		const EAngelscriptRuntimeJITContractValidationCode Code,
		const int32 ElementIndex = INDEX_NONE)
	{
		return {Code, ElementIndex};
	}
}

struct FAngelscriptRuntimeJITCodeLease::FState
{
	explicit FState(const FAngelscriptRuntimeJITCodeLeaseView& View)
		: UserData(View.UserData)
		, Release(View.Release)
	{
	}

	~FState()
	{
		Release(UserData);
	}

	void* UserData = nullptr;
	FAngelscriptRuntimeJITReleaseCodeLease Release = nullptr;
};

bool FAngelscriptRuntimeJITBackendId::TryParse(
	const FStringView Text,
	FAngelscriptRuntimeJITBackendId& OutBackendId)
{
	if (Text.IsEmpty()
		|| Text.Len() >= static_cast<int32>(
			FAngelscriptRuntimeJITBackendAbi::BackendIdCapacity))
	{
		return false;
	}

	FAngelscriptRuntimeJITBackendId Candidate;
	bool bPreviousDash = false;
	for (int32 Index = 0; Index < Text.Len(); ++Index)
	{
		const TCHAR Character = Text[Index];
		const bool bLower = Character >= TEXT('a')
			&& Character <= TEXT('z');
		const bool bDigit = Character >= TEXT('0')
			&& Character <= TEXT('9');
		const bool bDash = Character == TEXT('-');
		if ((!bLower && !bDigit && !bDash)
			|| (bDash && (Index == 0 || bPreviousDash)))
		{
			return false;
		}
		Candidate.Value[Index] = static_cast<ANSICHAR>(Character);
		bPreviousDash = bDash;
	}
	if (bPreviousDash)
	{
		return false;
	}
	OutBackendId = Candidate;
	return true;
}

bool FAngelscriptRuntimeJITBackendId::IsValid() const
{
	FAngelscriptRuntimeJITBackendId Parsed;
	return TryParse(ToString(), Parsed) && Parsed == *this;
}

FString FAngelscriptRuntimeJITBackendId::ToString() const
{
	FString Result;
	Result.Reserve(FAngelscriptRuntimeJITBackendAbi::BackendIdCapacity);
	for (uint32 Index = 0;
		Index < FAngelscriptRuntimeJITBackendAbi::BackendIdCapacity;
		++Index)
	{
		if (Value[Index] == '\0')
		{
			return Result;
		}
		Result.AppendChar(static_cast<TCHAR>(Value[Index]));
	}
	return {};
}

FAngelscriptRuntimeJITCodeLease FAngelscriptRuntimeJITCodeLease::Adopt(
	const FAngelscriptRuntimeJITCodeLeaseView& View)
{
	if (!FAngelscriptRuntimeJITContract::ValidateCodeLease(View).IsValid())
	{
		return {};
	}
	return FAngelscriptRuntimeJITCodeLease(
		MakeShared<FState, ESPMode::ThreadSafe>(View));
}

int32 FAngelscriptRuntimeJITCodeLease::GetSharedReferenceCount() const
{
	return State.IsValid() ? State.GetSharedReferenceCount() : 0;
}

FAngelscriptRuntimeJITContractValidationResult
	FAngelscriptRuntimeJITContract::ValidateFactoryMetadata(
		const FAngelscriptRuntimeJITBackendFactoryMetadata& Metadata)
{
	using namespace AngelscriptRuntimeJITBackend_Private;
	if (Metadata.StructSize != sizeof(Metadata))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidStructSize);
	}
	if (Metadata.AbiRevision != FAngelscriptRuntimeJITBackendAbi::Revision)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::UnsupportedRevision);
	}
	if (!Metadata.BackendId.IsValid())
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidBackendId);
	}
	if (Metadata.DisplayName == nullptr || Metadata.DisplayName[0] == TEXT('\0'))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::MissingDisplayName);
	}
	if (Metadata.SupportedPlatforms == EAngelscriptRuntimeJITPlatform::None
		|| !HasOnlyKnownFlags(
			Metadata.SupportedPlatforms, KnownPlatforms))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidPlatform);
	}
	if (Metadata.SupportedConfigurations
			== EAngelscriptRuntimeJITBuildConfiguration::None
		|| !HasOnlyKnownFlags(
			Metadata.SupportedConfigurations, KnownConfigurations))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidConfiguration);
	}
	if (Metadata.CompileConcurrency
			!= EAngelscriptRuntimeJITCompileConcurrency::SerializedPerSession
		&& Metadata.CompileConcurrency
			!= EAngelscriptRuntimeJITCompileConcurrency::ConcurrentPerSession)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidConcurrency);
	}
	return {};
}

FAngelscriptRuntimeJITContractValidationResult
	FAngelscriptRuntimeJITContract::ValidateSnapshot(
		const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot)
{
	using namespace AngelscriptRuntimeJITBackend_Private;
	if (Snapshot.StructSize != sizeof(Snapshot)
		|| Snapshot.Header.StructSize != sizeof(Snapshot.Header))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidStructSize);
	}
	if (Snapshot.AbiRevision != FAngelscriptRuntimeJITBackendAbi::Revision
		|| Snapshot.Header.AbiRevision
			!= FAngelscriptRuntimeJITBackendAbi::Revision)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::UnsupportedRevision);
	}
	if (!Snapshot.Header.BackendId.IsValid())
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidBackendId);
	}
	if (!HasOnlyKnownFlags(Snapshot.Header.Platform, KnownPlatforms)
		|| !IsOneFlag(Snapshot.Header.Platform))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidPlatform);
	}
	if (!HasOnlyKnownFlags(
			Snapshot.Header.Configuration, KnownConfigurations)
		|| !IsOneFlag(Snapshot.Header.Configuration))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidConfiguration);
	}
	if (Snapshot.Header.PointerWidth != 4
		&& Snapshot.Header.PointerWidth != 8)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidPointerWidth);
	}
	if (Snapshot.Header.Endianness != EAngelscriptRuntimeJITEndianness::Little
		&& Snapshot.Header.Endianness != EAngelscriptRuntimeJITEndianness::Big)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidEndianness);
	}
	if (Snapshot.Header.CallingConvention
		!= EAngelscriptRuntimeJITCallingConvention::AngelscriptVMEntry)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidCallingConvention);
	}
	if (Snapshot.Header.Invocation == EAngelscriptRuntimeJITInvocationKind::Unknown
		|| Snapshot.Header.Invocation
			> EAngelscriptRuntimeJITInvocationKind::ExternalImplicitThis)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidInvocation);
	}
	if (Snapshot.Header.Receiver == EAngelscriptRuntimeJITReceiverKind::Unknown
		|| Snapshot.Header.Receiver
			> EAngelscriptRuntimeJITReceiverKind::DeclaredParameterAlias)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidReceiver);
	}
	if (Snapshot.Header.FunctionProfile
			== EAngelscriptRuntimeJITFunctionProfile::Unknown
		|| Snapshot.Header.FunctionProfile
			> EAngelscriptRuntimeJITFunctionProfile::ExceptionCleanup)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidFunctionProfile);
	}
	if (Snapshot.Header.EngineNamespace == 0)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidEngineNamespace);
	}
	if (Snapshot.Header.ModuleKey.Hash.IsZero()
		|| Snapshot.Header.FunctionKey.Hash.IsZero()
		|| Snapshot.Header.FunctionRevision.IsZero())
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidFunctionIdentity);
	}
	if (Snapshot.Header.EntryAbiRevision == 0
		|| Snapshot.Header.EntryAbiHash.IsZero())
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidEntryAbi);
	}
	if (Snapshot.BytecodeWordCount == 0
		|| Snapshot.BytecodeWordCount
			> FAngelscriptRuntimeJITBackendAbi::MaximumBytecodeWordCount)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidBytecodeCount);
	}
	if (Snapshot.Bytecode == nullptr)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::MissingBytecode);
	}
	auto ValidateCountAndPointer = [](
		const uint32 Count,
		const uint32 Maximum,
		const void* Pointer,
		const EAngelscriptRuntimeJITContractValidationCode CountCode,
		const EAngelscriptRuntimeJITContractValidationCode PointerCode)
		-> FAngelscriptRuntimeJITContractValidationResult
	{
		if (Count > Maximum)
		{
			return Invalid(CountCode);
		}
		if (Count != 0 && Pointer == nullptr)
		{
			return Invalid(PointerCode);
		}
		return {};
	};
	FAngelscriptRuntimeJITContractValidationResult Result =
		ValidateCountAndPointer(
			Snapshot.FrameValueCount,
			FAngelscriptRuntimeJITBackendAbi::MaximumFrameValueCount,
			Snapshot.FrameValues,
			EAngelscriptRuntimeJITContractValidationCode::InvalidFrameValueCount,
			EAngelscriptRuntimeJITContractValidationCode::MissingFrameValues);
	if (!Result.IsValid()) return Result;
	for (uint32 Index = 0; Index < Snapshot.FrameValueCount; ++Index)
	{
		if (Snapshot.FrameValues[Index].StructSize
			!= sizeof(FAngelscriptRuntimeJITFrameValue))
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidStructSize,
				static_cast<int32>(Index));
		}
	}
	Result = ValidateCountAndPointer(
		Snapshot.InstructionCount,
		FAngelscriptRuntimeJITBackendAbi::MaximumInstructionCount,
		Snapshot.InstructionWordOffsets,
		EAngelscriptRuntimeJITContractValidationCode::InvalidInstructionCount,
		EAngelscriptRuntimeJITContractValidationCode::MissingInstructionOffsets);
	if (!Result.IsValid()) return Result;
	Result = ValidateCountAndPointer(
		Snapshot.ControlFlowEdgeCount,
		FAngelscriptRuntimeJITBackendAbi::MaximumControlFlowEdgeCount,
		Snapshot.ControlFlowEdges,
		EAngelscriptRuntimeJITContractValidationCode::InvalidControlFlowCount,
		EAngelscriptRuntimeJITContractValidationCode::MissingControlFlowEdges);
	if (!Result.IsValid()) return Result;
	for (uint32 Index = 0; Index < Snapshot.ControlFlowEdgeCount; ++Index)
	{
		if (Snapshot.ControlFlowEdges[Index].StructSize
			!= sizeof(FAngelscriptRuntimeJITControlFlowEdge))
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidStructSize,
				static_cast<int32>(Index));
		}
	}
	Result = ValidateCountAndPointer(
		Snapshot.HelperTokenCount,
		FAngelscriptRuntimeJITBackendAbi::MaximumHelperTokenCount,
		Snapshot.HelperTokens,
		EAngelscriptRuntimeJITContractValidationCode::InvalidHelperTokenCount,
		EAngelscriptRuntimeJITContractValidationCode::MissingHelperTokens);
	if (!Result.IsValid()) return Result;
	for (uint32 Index = 0; Index < Snapshot.HelperTokenCount; ++Index)
	{
		if (Snapshot.HelperTokens[Index].StructSize
			!= sizeof(FAngelscriptRuntimeJITHelperToken))
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidStructSize,
				static_cast<int32>(Index));
		}
	}
	return {};
}

FAngelscriptRuntimeJITContractValidationResult
	FAngelscriptRuntimeJITContract::ValidateCodeLease(
		const FAngelscriptRuntimeJITCodeLeaseView& Lease)
{
	using namespace AngelscriptRuntimeJITBackend_Private;
	if (Lease.StructSize != sizeof(Lease))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidStructSize);
	}
	if (Lease.AbiRevision != FAngelscriptRuntimeJITBackendAbi::Revision)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::UnsupportedRevision);
	}
	if (Lease.UserData == nullptr || Lease.Release == nullptr)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidCodeLease);
	}
	return {};
}

FAngelscriptRuntimeJITContractValidationResult
	FAngelscriptRuntimeJITContract::ValidateCompileResult(
		const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot,
		const FAngelscriptRuntimeJITCompileResult& Result)
{
	using namespace AngelscriptRuntimeJITBackend_Private;
	if (Result.StructSize != sizeof(Result))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidStructSize);
	}
	if (Result.AbiRevision != FAngelscriptRuntimeJITBackendAbi::Revision)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::UnsupportedRevision);
	}
	if (Result.Outcome > EAngelscriptRuntimeJITCompileOutcome::InvalidInput)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidCompileOutcome);
	}
	if (Result.Reason >
		EAngelscriptRuntimeJITDiagnosticReason::InvalidBackendResult)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidDiagnosticReason);
	}
	auto IsUnsupportedReason = [](const EAngelscriptRuntimeJITDiagnosticReason Reason)
	{
		switch (Reason)
		{
		case EAngelscriptRuntimeJITDiagnosticReason::UnsupportedPlatform:
		case EAngelscriptRuntimeJITDiagnosticReason::UnsupportedConfiguration:
		case EAngelscriptRuntimeJITDiagnosticReason::UnsupportedOpcode:
		case EAngelscriptRuntimeJITDiagnosticReason::UnsupportedManagedValue:
		case EAngelscriptRuntimeJITDiagnosticReason::UnsupportedReceiver:
		case EAngelscriptRuntimeJITDiagnosticReason::SuspendOrLatent:
		case EAngelscriptRuntimeJITDiagnosticReason::ExceptionCleanup:
		case EAngelscriptRuntimeJITDiagnosticReason::DebuggerActive:
		case EAngelscriptRuntimeJITDiagnosticReason::CoverageActive:
		case EAngelscriptRuntimeJITDiagnosticReason::EntryAbiMismatch:
			return true;
		default:
			return false;
		}
	};
	bool bOutcomeReasonMatches = false;
	switch (Result.Outcome)
	{
	case EAngelscriptRuntimeJITCompileOutcome::Compiled:
		bOutcomeReasonMatches = Result.Reason
			== EAngelscriptRuntimeJITDiagnosticReason::None;
		break;
	case EAngelscriptRuntimeJITCompileOutcome::Unsupported:
		bOutcomeReasonMatches = IsUnsupportedReason(Result.Reason);
		break;
	case EAngelscriptRuntimeJITCompileOutcome::Cancelled:
		bOutcomeReasonMatches = Result.Reason
			== EAngelscriptRuntimeJITDiagnosticReason::Cancelled;
		break;
	case EAngelscriptRuntimeJITCompileOutcome::Stale:
		bOutcomeReasonMatches = Result.Reason
			== EAngelscriptRuntimeJITDiagnosticReason::StaleRevision;
		break;
	case EAngelscriptRuntimeJITCompileOutcome::BackendFailure:
		bOutcomeReasonMatches = Result.Reason
			== EAngelscriptRuntimeJITDiagnosticReason::BackendFailure;
		break;
	case EAngelscriptRuntimeJITCompileOutcome::InvalidInput:
		bOutcomeReasonMatches = Result.Reason
			== EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot
			|| Result.Reason
				== EAngelscriptRuntimeJITDiagnosticReason::EntryAbiMismatch
			|| Result.Reason
				== EAngelscriptRuntimeJITDiagnosticReason::InvalidBackendResult;
		break;
	}
	if (!bOutcomeReasonMatches)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidOutcomeReason);
	}
	if (Result.Outcome == EAngelscriptRuntimeJITCompileOutcome::Unsupported)
	{
		if (Result.BytecodeOffset != MAX_uint32
			&& Result.BytecodeOffset >= Snapshot.BytecodeWordCount)
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidBytecodeOffset);
		}
	}
	else if (Result.BytecodeOffset != MAX_uint32)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidBytecodeOffset);
	}
	if (!(Result.FunctionRevision == Snapshot.Header.FunctionRevision))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidCompileResult);
	}
	if (Result.Outcome == EAngelscriptRuntimeJITCompileOutcome::Compiled)
	{
		if (Result.VMEntry == nullptr
			|| !Result.CodeLease.IsValid()
			|| Result.CodeSize == 0
			|| !(Result.EntryAbiHash == Snapshot.Header.EntryAbiHash))
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidCompileResult);
		}
	}
	else if (Result.VMEntry != nullptr
		|| Result.CodeLease.IsValid()
		|| Result.CodeSize != 0)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidCompileResult);
	}
	return {};
}

FName IAngelscriptRuntimeJITBackendFactory::FeatureName()
{
	static const FName Name(TEXT("AngelscriptRuntimeJITBackendFactory"));
	return Name;
}
