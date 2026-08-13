#include "RuntimeJIT/AngelscriptRuntimeJITBackendRegistry.h"

#include "Features/IModularFeatures.h"

FAngelscriptRuntimeJITFactorySelection
	FAngelscriptRuntimeJITBackendRegistry::Select(
		const FAngelscriptRuntimeJITBackendId& BackendId,
		const EAngelscriptRuntimeJITPlatform Platform,
		const EAngelscriptRuntimeJITBuildConfiguration Configuration)
{
	FAngelscriptRuntimeJITFactorySelection Result;
	if (!BackendId.IsValid())
	{
		Result.Code =
			EAngelscriptRuntimeJITFactorySelectionCode::InvalidFactoryMetadata;
		Result.ValidationCode =
			EAngelscriptRuntimeJITContractValidationCode::InvalidBackendId;
		return Result;
	}

	const TArray<IAngelscriptRuntimeJITBackendFactory*> Factories =
		IModularFeatures::Get().GetModularFeatureImplementations<
			IAngelscriptRuntimeJITBackendFactory>(
				IAngelscriptRuntimeJITBackendFactory::FeatureName());
	struct FCandidate
	{
		IAngelscriptRuntimeJITBackendFactory* Factory = nullptr;
		const FAngelscriptRuntimeJITBackendFactoryMetadata* Metadata = nullptr;
	};
	TArray<FCandidate> Matches;
	for (IAngelscriptRuntimeJITBackendFactory* Factory : Factories)
	{
		if (Factory == nullptr)
		{
			continue;
		}
		const FAngelscriptRuntimeJITBackendFactoryMetadata* const Metadata =
			Factory->GetMetadata();
		if (Metadata == nullptr)
		{
			continue;
		}
		if (Metadata->BackendId == BackendId)
		{
			Matches.Add({Factory, Metadata});
		}
	}

	if (Matches.IsEmpty())
	{
		Result.Code =
			EAngelscriptRuntimeJITFactorySelectionCode::MissingBackend;
		return Result;
	}
	if (Matches.Num() != 1)
	{
		Result.Code =
			EAngelscriptRuntimeJITFactorySelectionCode::DuplicateBackendId;
		return Result;
	}
	const FAngelscriptRuntimeJITContractValidationResult Validation =
		FAngelscriptRuntimeJITContract::ValidateFactoryMetadata(
			*Matches[0].Metadata);
	if (!Validation.IsValid())
	{
		Result.Code = EAngelscriptRuntimeJITFactorySelectionCode::
			InvalidFactoryMetadata;
		Result.ValidationCode = Validation.Code;
		return Result;
	}
	const FAngelscriptRuntimeJITBackendFactoryMetadata& SelectedMetadata =
		*Matches[0].Metadata;
	if (!EnumHasAnyFlags(SelectedMetadata.SupportedPlatforms, Platform))
	{
		Result.Code =
			EAngelscriptRuntimeJITFactorySelectionCode::UnsupportedPlatform;
		return Result;
	}
	if (!EnumHasAnyFlags(
		SelectedMetadata.SupportedConfigurations, Configuration))
	{
		Result.Code = EAngelscriptRuntimeJITFactorySelectionCode::
			UnsupportedConfiguration;
		return Result;
	}

	Result.Code = EAngelscriptRuntimeJITFactorySelectionCode::Selected;
	Result.Factory = Matches[0].Factory;
	Result.Metadata.BackendId = SelectedMetadata.BackendId;
	Result.Metadata.DisplayName = SelectedMetadata.DisplayName;
	Result.Metadata.SupportedPlatforms =
		SelectedMetadata.SupportedPlatforms;
	Result.Metadata.SupportedConfigurations =
		SelectedMetadata.SupportedConfigurations;
	Result.Metadata.CompileConcurrency =
		SelectedMetadata.CompileConcurrency;
	return Result;
}

FAngelscriptRuntimeJITBackendSessionOwner::
	FAngelscriptRuntimeJITBackendSessionOwner(
		FAngelscriptRuntimeJITFactoryMetadataSnapshot InMetadata,
		const uint64 InEngineNamespace,
		const EAngelscriptRuntimeJITPlatform InPlatform,
		const EAngelscriptRuntimeJITBuildConfiguration InConfiguration,
		TUniquePtr<IAngelscriptRuntimeJITBackendSession> InSession)
	: Metadata(MoveTemp(InMetadata))
	, EngineNamespace(InEngineNamespace)
	, Platform(InPlatform)
	, Configuration(InConfiguration)
	, Session(MoveTemp(InSession))
{
}

FAngelscriptRuntimeJITBackendSessionOwner::
	~FAngelscriptRuntimeJITBackendSessionOwner() = default;

FAngelscriptRuntimeJITSessionCreateResult
	FAngelscriptRuntimeJITBackendSessionOwner::Create(
		const FAngelscriptRuntimeJITBackendSessionConfig& Config)
{
	FAngelscriptRuntimeJITSessionCreateResult Result;
	if (Config.StructSize != sizeof(Config))
	{
		Result.Code = EAngelscriptRuntimeJITFactorySelectionCode::
			InvalidFactoryMetadata;
		Result.ValidationCode =
			EAngelscriptRuntimeJITContractValidationCode::InvalidStructSize;
		return Result;
	}
	if (Config.AbiRevision != FAngelscriptRuntimeJITBackendAbi::Revision)
	{
		Result.Code = EAngelscriptRuntimeJITFactorySelectionCode::
			InvalidFactoryMetadata;
		Result.ValidationCode =
			EAngelscriptRuntimeJITContractValidationCode::UnsupportedRevision;
		return Result;
	}
	if (!Config.BackendId.IsValid())
	{
		Result.Code = EAngelscriptRuntimeJITFactorySelectionCode::
			InvalidFactoryMetadata;
		Result.ValidationCode =
			EAngelscriptRuntimeJITContractValidationCode::InvalidBackendId;
		return Result;
	}
	if (Config.Platform != EAngelscriptRuntimeJITPlatform::Win64
		&& Config.Platform != EAngelscriptRuntimeJITPlatform::Linux
		&& Config.Platform != EAngelscriptRuntimeJITPlatform::Mac)
	{
		Result.Code = EAngelscriptRuntimeJITFactorySelectionCode::
			InvalidFactoryMetadata;
		Result.ValidationCode =
			EAngelscriptRuntimeJITContractValidationCode::InvalidPlatform;
		return Result;
	}
	if (Config.Configuration
			!= EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment
		&& Config.Configuration
			!= EAngelscriptRuntimeJITBuildConfiguration::GameDevelopment
		&& Config.Configuration
			!= EAngelscriptRuntimeJITBuildConfiguration::Shipping)
	{
		Result.Code = EAngelscriptRuntimeJITFactorySelectionCode::
			InvalidFactoryMetadata;
		Result.ValidationCode =
			EAngelscriptRuntimeJITContractValidationCode::InvalidConfiguration;
		return Result;
	}
	if (Config.EngineNamespace == 0)
	{
		Result.Code = EAngelscriptRuntimeJITFactorySelectionCode::
			InvalidFactoryMetadata;
		Result.ValidationCode =
			EAngelscriptRuntimeJITContractValidationCode::InvalidEngineNamespace;
		return Result;
	}

	FAngelscriptRuntimeJITFactorySelection Selection =
		FAngelscriptRuntimeJITBackendRegistry::Select(
			Config.BackendId, Config.Platform, Config.Configuration);
	Result.Code = Selection.Code;
	Result.ValidationCode = Selection.ValidationCode;
	if (!Selection.IsSelected())
	{
		return Result;
	}
	TUniquePtr<IAngelscriptRuntimeJITBackendSession> Session =
		Selection.Factory->CreateSession(Config);
	if (!Session.IsValid())
	{
		Result.Code = EAngelscriptRuntimeJITFactorySelectionCode::
			InvalidFactoryMetadata;
		Result.ValidationCode = EAngelscriptRuntimeJITContractValidationCode::
			InvalidCompileResult;
		return Result;
	}
	Result.Owner = TUniquePtr<FAngelscriptRuntimeJITBackendSessionOwner>(
		new FAngelscriptRuntimeJITBackendSessionOwner(
			MoveTemp(Selection.Metadata),
			Config.EngineNamespace,
			Config.Platform,
			Config.Configuration,
			MoveTemp(Session)));
	return Result;
}

FAngelscriptRuntimeJITCompileResult
	FAngelscriptRuntimeJITBackendSessionOwner::Compile(
		const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot)
{
	auto InvalidResult = [&Snapshot](
		const EAngelscriptRuntimeJITDiagnosticReason Reason)
	{
		FAngelscriptRuntimeJITCompileResult Result;
		Result.Outcome = EAngelscriptRuntimeJITCompileOutcome::InvalidInput;
		Result.Reason = Reason;
		Result.FunctionRevision = Snapshot.Header.FunctionRevision;
		Result.EntryAbiHash = Snapshot.Header.EntryAbiHash;
		return Result;
	};
	const FAngelscriptRuntimeJITContractValidationResult SnapshotValidation =
		FAngelscriptRuntimeJITContract::ValidateSnapshot(Snapshot);
	if (!SnapshotValidation.IsValid())
	{
		return InvalidResult(
			SnapshotValidation.Code
				== EAngelscriptRuntimeJITContractValidationCode::InvalidEntryAbi
					? EAngelscriptRuntimeJITDiagnosticReason::EntryAbiMismatch
					: EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot);
	}
	if (Snapshot.Header.BackendId != Metadata.BackendId
		|| Snapshot.Header.EngineNamespace != EngineNamespace
		|| Snapshot.Header.Platform != Platform
		|| Snapshot.Header.Configuration != Configuration)
	{
		return InvalidResult(
			EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot);
	}
	if (Snapshot.Header.Receiver
		!= EAngelscriptRuntimeJITReceiverKind::None)
	{
		FAngelscriptRuntimeJITCompileResult Result;
		Result.Outcome = EAngelscriptRuntimeJITCompileOutcome::Unsupported;
		Result.Reason =
			EAngelscriptRuntimeJITDiagnosticReason::UnsupportedReceiver;
		Result.FunctionRevision = Snapshot.Header.FunctionRevision;
		Result.EntryAbiHash = Snapshot.Header.EntryAbiHash;
		return Result;
	}
	if (Snapshot.Header.FunctionProfile
		!= EAngelscriptRuntimeJITFunctionProfile::ScalarWholeFunction)
	{
		FAngelscriptRuntimeJITCompileResult Result;
		Result.Outcome = EAngelscriptRuntimeJITCompileOutcome::Unsupported;
		switch (Snapshot.Header.FunctionProfile)
		{
		case EAngelscriptRuntimeJITFunctionProfile::ManagedValues:
			Result.Reason = EAngelscriptRuntimeJITDiagnosticReason::
				UnsupportedManagedValue;
			break;
		case EAngelscriptRuntimeJITFunctionProfile::SuspendOrLatent:
			Result.Reason =
				EAngelscriptRuntimeJITDiagnosticReason::SuspendOrLatent;
			break;
		case EAngelscriptRuntimeJITFunctionProfile::ExceptionCleanup:
			Result.Reason =
				EAngelscriptRuntimeJITDiagnosticReason::ExceptionCleanup;
			break;
		default:
			return InvalidResult(
				EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot);
		}
		Result.FunctionRevision = Snapshot.Header.FunctionRevision;
		Result.EntryAbiHash = Snapshot.Header.EntryAbiHash;
		return Result;
	}

	auto CompileAndValidate = [this, &Snapshot, &InvalidResult]()
	{
		FAngelscriptRuntimeJITCompileResult BackendResult =
			Session->Compile(Snapshot);
		if (!FAngelscriptRuntimeJITContract::ValidateCompileResult(
			Snapshot, BackendResult).IsValid())
		{
			return InvalidResult(
				EAngelscriptRuntimeJITDiagnosticReason::InvalidBackendResult);
		}
		return BackendResult;
	};

	if (RequiresSerializedCompilation())
	{
		FScopeLock Lock(&CompileMutex);
		return CompileAndValidate();
	}
	return CompileAndValidate();
}

void FAngelscriptRuntimeJITBackendSessionOwner::Cancel(
	const uint64 CancellationGeneration)
{
	// CompileConcurrency constrains compile calls. Cancel is the interruption
	// channel and must remain callable while a serialized compile is blocked.
	Session->Cancel(CancellationGeneration);
}
