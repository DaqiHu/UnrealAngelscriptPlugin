#include "StaticJIT/AngelscriptJITProvider.h"
#include "StaticJIT/AngelscriptJITProviderDiagnostics.h"

namespace
{
	static FAngelscriptJITProviderValidationResult Failure(
		const EAngelscriptJITProviderValidationCode Code,
		const int32 EntryIndex = INDEX_NONE,
		const int32 ReferenceIndex = INDEX_NONE,
		const int32 NativeCallSiteIndex = INDEX_NONE)
	{
		return {Code, EntryIndex, ReferenceIndex, NativeCallSiteIndex};
	}

	static bool IsNonEmpty(const TCHAR* const Text)
	{
		return Text != nullptr && Text[0] != TEXT('\0');
	}

	static int32 CompareEntryIdentity(
		const FAngelscriptJITArtifactEntry& Left,
		const FAngelscriptJITArtifactEntry& Right)
	{
		if (!(Left.ModuleKey == Right.ModuleKey))
		{
			return Left.ModuleKey.Hash < Right.ModuleKey.Hash ? -1 : 1;
		}
		if (!(Left.FunctionKey == Right.FunctionKey))
		{
			return Left.FunctionKey.Hash < Right.FunctionKey.Hash ? -1 : 1;
		}
		return 0;
	}

	static bool IsBoolean(const uint32 Value)
	{
		return Value <= 1;
	}

	static bool IsPresent(const TCHAR* const Text)
	{
		return Text != nullptr;
	}

	static bool RangeFits(
		const uint32 Start,
		const uint32 Count,
		const uint32 Total)
	{
		return Start <= Total && Count <= Total - Start;
	}

	static FAngelscriptJITProviderValidationResult DiagnosticFailure(
		const EAngelscriptJITProviderValidationCode Code,
		const int32 FunctionIndex = INDEX_NONE,
		const int32 AttemptIndex = INDEX_NONE,
		const int32 SemanticIndex = INDEX_NONE,
		const int32 CallIndex = INDEX_NONE)
	{
		FAngelscriptJITProviderValidationResult Result;
		Result.Code = Code;
		Result.DiagnosticFunctionIndex = FunctionIndex;
		Result.DiagnosticAttemptIndex = AttemptIndex;
		Result.DiagnosticSemanticIndex = SemanticIndex;
		Result.DiagnosticCallIndex = CallIndex;
		return Result;
	}

	static void WriteDiagnosticSource(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const FAngelscriptJITProviderDiagnosticSource& Source)
	{
		Writer.WriteString(Source.Section != nullptr ? Source.Section : TEXT(""));
		Writer.WriteUInt32(Source.Row);
		Writer.WriteUInt32(Source.Column);
	}
}

FName IAngelscriptJITArtifactProvider::FeatureName()
{
	static const FName Name(TEXT("AngelscriptJITArtifactProvider"));
	return Name;
}

bool FAngelscriptJITProviderManifest::SemanticDependencyRequiresExpectedContent(
	const EAngelscriptArtifactSemanticDependencyKind Kind)
{
	return Kind == EAngelscriptArtifactSemanticDependencyKind::FunctionContent
		|| Kind == EAngelscriptArtifactSemanticDependencyKind::HardValue;
}

void FAngelscriptJITProviderManifest::FillSemanticDependencyRow(
	const FAngelscriptArtifactSemanticDependency& Dependency,
	const uint32 RowIndex,
	FAngelscriptJITSemanticDependencyRow& OutRow)
{
	OutRow = {};
	OutRow.RowIndex = RowIndex;
	OutRow.Kind = Dependency.Kind;
	OutRow.TargetKind = Dependency.Target.Kind;
	OutRow.TargetStableKey = Dependency.Target.StableKey;
	OutRow.ExpectedAbi = Dependency.Target.ExpectedAbi;
	if (Dependency.ExpectedContentOrValue.IsSet())
	{
		OutRow.Flags =
			EAngelscriptJITSemanticDependencyFlags::HasExpectedContentOrValue;
		OutRow.ExpectedContentOrValue =
			Dependency.ExpectedContentOrValue.GetValue();
	}
}

FAngelscriptJITSemanticDependencyCanonicalResult
FAngelscriptJITProviderManifest::CanonicalizeSemanticDependencies(
	const TConstArrayView<FAngelscriptArtifactSemanticDependency> Rows)
{
	FAngelscriptJITSemanticDependencyCanonicalResult Result;
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FAngelscriptArtifactSemanticDependency& Row = Rows[Index];
		if (Row.Kind <= EAngelscriptArtifactSemanticDependencyKind::Invalid
			|| static_cast<uint8>(Row.Kind)
				> static_cast<uint8>(
					EAngelscriptArtifactSemanticDependencyKind::FunctionContent))
		{
			Result.Code =
				EAngelscriptJITSemanticDependencyCanonicalCode::InvalidKind;
			Result.RowIndex = Index;
			return Result;
		}
		if (!Row.Target.IsValid() || Row.Target.ExpectedAbi.IsZero())
		{
			Result.Code =
				EAngelscriptJITSemanticDependencyCanonicalCode::InvalidTarget;
			Result.RowIndex = Index;
			return Result;
		}
		if (SemanticDependencyRequiresExpectedContent(Row.Kind)
			&& (!Row.ExpectedContentOrValue.IsSet()
				|| Row.ExpectedContentOrValue.GetValue().IsZero()))
		{
			Result.Code =
				EAngelscriptJITSemanticDependencyCanonicalCode::
					MissingExpectedContentOrValue;
			Result.RowIndex = Index;
			return Result;
		}
	}

	TArray<FAngelscriptArtifactSemanticDependency> Sorted(Rows);
	Sorted.Sort();
	for (const FAngelscriptArtifactSemanticDependency& Row : Sorted)
	{
		if (Result.Rows.Num() > 0)
		{
			const FAngelscriptArtifactSemanticDependency& Previous =
				Result.Rows.Last();
			if (Previous.Kind == Row.Kind
				&& Previous.Target.Kind == Row.Target.Kind
				&& Previous.Target.StableKey == Row.Target.StableKey)
			{
				if (Previous.Target.ExpectedAbi != Row.Target.ExpectedAbi
					|| Previous.ExpectedContentOrValue
						!= Row.ExpectedContentOrValue)
				{
					Result.Code =
						EAngelscriptJITSemanticDependencyCanonicalCode::
							ConflictingDuplicate;
					Result.RowIndex = Result.Rows.Num();
					Result.Rows.Reset();
					return Result;
				}
				continue;
			}
		}
		Result.Rows.Add(Row);
	}
	return Result;
}

FAngelscriptHash256 FAngelscriptJITProviderManifest::BuildArtifactSetDigest(
	const TConstArrayView<FAngelscriptJITArtifactEntry> Entries,
	const TConstArrayView<FAngelscriptJITSemanticDependencyRow>
		SemanticDependencies)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("jit-provider-artifact-set-v2"));
	Writer.WriteUInt32(FAngelscriptJITProviderAbi::Revision);
	Writer.WriteUInt32(static_cast<uint32>(Entries.Num()));
	for (const FAngelscriptJITArtifactEntry& Entry : Entries)
	{
		Writer.WriteHash(Entry.ModuleKey.Hash);
		Writer.WriteHash(Entry.FunctionKey.Hash);
		Writer.WriteHash(Entry.ExecutionHash);
		Writer.WriteHash(Entry.DebugHash);
		Writer.WriteHash(Entry.ArtifactProfile.Hash);
		Writer.WriteHash(Entry.EntryAbiHash);
		Writer.WriteHash(Entry.NativeEnvironmentFingerprint);
		Writer.WriteUInt32(static_cast<uint32>(Entry.Flags));
		Writer.WriteUInt32(static_cast<uint32>(
			Entry.AvailableExecutionCapabilities));
		Writer.WriteHash(Entry.ExecutionCapabilityProfileHash);
		Writer.WriteUInt32(static_cast<uint32>(
			Entry.RuntimeExecutionRequirementTriggers));
		Writer.WriteBool(Entry.EntryPoints.VMEntry != nullptr);
		Writer.WriteBool(Entry.EntryPoints.RawEntry != nullptr);
		Writer.WriteBool(Entry.EntryPoints.ParmsEntry != nullptr);
		Writer.WriteUInt32(Entry.ReferenceSlotCount);
		for (uint32 ReferenceIndex = 0;
			ReferenceIndex < Entry.ReferenceSlotCount;
			++ReferenceIndex)
		{
			const FAngelscriptJITReferenceSlot& Slot =
				Entry.ReferenceSlots[ReferenceIndex];
			Writer.WriteUInt32(Slot.SlotIndex);
			Writer.WriteUInt32(static_cast<uint32>(Slot.Flags));
			Writer.WriteUInt8(static_cast<uint8>(Slot.Reference.Kind));
			Writer.WriteHash(Slot.Reference.StableKey);
			Writer.WriteHash(Slot.Reference.ExpectedAbi);
		}
		Writer.WriteUInt32(Entry.NativeCallSiteCount);
		for (uint32 CallSiteIndex = 0;
			CallSiteIndex < Entry.NativeCallSiteCount;
			++CallSiteIndex)
		{
			const FAngelscriptJITNativeCallSite& CallSite =
				Entry.NativeCallSites[CallSiteIndex];
			Writer.WriteUInt32(CallSite.CallSiteIndex);
			Writer.WriteUInt32(static_cast<uint32>(CallSite.Route));
			Writer.WriteString(CallSite.CanonicalDeclaration);
			Writer.WriteString(CallSite.EmittedCppCallee);
			Writer.WriteString(CallSite.RuntimeDllCore != nullptr
				? CallSite.RuntimeDllCore : TEXT(""));
			Writer.WriteString(CallSite.MetadataRowSymbol != nullptr
				? CallSite.MetadataRowSymbol : TEXT(""));
			Writer.WriteString(CallSite.RegisteredTarget);
			Writer.WriteHash(CallSite.StableTargetKey);
			Writer.WriteHash(CallSite.ExpectedAbi);
			Writer.WriteUInt32(
				static_cast<uint32>(CallSite.ReferenceSlotIndex));
		}
		Writer.WriteUInt32(Entry.SemanticDependencyStartIndex);
		Writer.WriteUInt32(Entry.SemanticDependencyCount);
		for (uint32 DependencyIndex = 0;
			DependencyIndex < Entry.SemanticDependencyCount;
			++DependencyIndex)
		{
			const uint32 TableIndex =
				Entry.SemanticDependencyStartIndex + DependencyIndex;
			if (TableIndex >= static_cast<uint32>(SemanticDependencies.Num()))
			{
				continue;
			}
			const FAngelscriptJITSemanticDependencyRow& Row =
				SemanticDependencies[static_cast<int32>(TableIndex)];
			Writer.WriteUInt8(static_cast<uint8>(Row.Kind));
			Writer.WriteUInt8(static_cast<uint8>(Row.TargetKind));
			Writer.WriteUInt32(static_cast<uint32>(Row.Flags));
			Writer.WriteHash(Row.TargetStableKey);
			Writer.WriteHash(Row.ExpectedAbi);
			Writer.WriteHash(Row.ExpectedContentOrValue);
		}
	}
	return Writer.FinalizeHash();
}

FAngelscriptHash256 FAngelscriptJITProviderManifest::BuildDiagnosticsDigest(
	const FAngelscriptJITProviderDiagnostics& Diagnostics)
{
	FAngelscriptArtifactCanonicalWriter Writer(
		TEXT("jit-provider-diagnostics-v2"));
	Writer.WriteUInt32(Diagnostics.SchemaRevision);
	Writer.WriteString(Diagnostics.RequestKind != nullptr
		? Diagnostics.RequestKind : TEXT(""));
	Writer.WriteString(Diagnostics.RequestedBackendId != nullptr
		? Diagnostics.RequestedBackendId : TEXT(""));
	Writer.WriteString(Diagnostics.CaptureState != nullptr
		? Diagnostics.CaptureState : TEXT(""));
	Writer.WriteUInt32(Diagnostics.FallbackChainCount);
	for (uint32 Index = 0;
		Diagnostics.FallbackChain != nullptr
			&& Index < Diagnostics.FallbackChainCount;
		++Index)
	{
		Writer.WriteString(Diagnostics.FallbackChain[Index] != nullptr
			? Diagnostics.FallbackChain[Index] : TEXT(""));
	}
	Writer.WriteUInt32(Diagnostics.SourceCompilePassCount);
	Writer.WriteUInt32(Diagnostics.CompiledModuleCount);
	Writer.WriteUInt32(Diagnostics.CompiledFunctionCount);
	Writer.WriteUInt32(Diagnostics.CompiledTypeCount);
	Writer.WriteUInt32(Diagnostics.CompiledGlobalCount);
	Writer.WriteUInt32(Diagnostics.CompiledDescriptorCount);
	Writer.WriteUInt32(Diagnostics.CompiledDependencyCount);
	Writer.WriteUInt32(Diagnostics.CompiledExternalNativeCallCount);
	Writer.WriteUInt32(Diagnostics.EmitModuleCount);
	Writer.WriteUInt32(Diagnostics.bHasVerifiedCanonicalAST);
	Writer.WriteString(Diagnostics.TaskError != nullptr
		? Diagnostics.TaskError : TEXT(""));

	Writer.WriteUInt32(Diagnostics.FunctionCount);
	for (uint32 Index = 0;
		Diagnostics.Functions != nullptr && Index < Diagnostics.FunctionCount;
		++Index)
	{
		const FAngelscriptJITProviderFunctionDiagnostic& Function =
			Diagnostics.Functions[Index];
		Writer.WriteHash(Function.FunctionKey.Hash);
		Writer.WriteString(Function.ActualBackendId != nullptr
			? Function.ActualBackendId : TEXT(""));
		Writer.WriteString(Function.Disposition != nullptr
			? Function.Disposition : TEXT(""));
		Writer.WriteString(Function.Detail != nullptr
			? Function.Detail : TEXT(""));
		Writer.WriteUInt32(Function.BackendAttemptStart);
		Writer.WriteUInt32(Function.BackendAttemptCount);
	}

	Writer.WriteUInt32(Diagnostics.BackendAttemptCount);
	for (uint32 Index = 0;
		Diagnostics.BackendAttempts != nullptr
			&& Index < Diagnostics.BackendAttemptCount;
		++Index)
	{
		const FAngelscriptJITProviderBackendAttemptDiagnostic& Attempt =
			Diagnostics.BackendAttempts[Index];
		Writer.WriteString(Attempt.BackendId != nullptr
			? Attempt.BackendId : TEXT(""));
		Writer.WriteString(Attempt.Disposition != nullptr
			? Attempt.Disposition : TEXT(""));
		Writer.WriteString(Attempt.Detail != nullptr
			? Attempt.Detail : TEXT(""));
		Writer.WriteUInt32(static_cast<uint32>(Attempt.Flags));
		Writer.WriteString(Attempt.FallbackReason != nullptr
			? Attempt.FallbackReason : TEXT(""));
		Writer.WriteString(Attempt.TypedDetail != nullptr
			? Attempt.TypedDetail : TEXT(""));
		WriteDiagnosticSource(Writer, Attempt.ProcessedSource);
		Writer.WriteUInt32(static_cast<uint32>(
			Attempt.PrimarySemanticFunctionIndex));
		Writer.WriteUInt32(Attempt.SemanticFunctionStart);
		Writer.WriteUInt32(Attempt.SemanticFunctionCount);
		Writer.WriteUInt32(Attempt.CallStart);
		Writer.WriteUInt32(Attempt.CallCount);
		Writer.WriteUInt32(static_cast<uint32>(Attempt.RequiredCapabilities));
		Writer.WriteUInt32(static_cast<uint32>(Attempt.AvailableCapabilities));
		Writer.WriteHash(Attempt.ExecutionRequirementsIdentity);
		Writer.WriteHash(Attempt.AvailableCapabilityProfileIdentity);
		Writer.WriteString(Attempt.ProfileKind != nullptr
			? Attempt.ProfileKind : TEXT(""));
		Writer.WriteString(Attempt.ExceptionControlState != nullptr
			? Attempt.ExceptionControlState : TEXT(""));
		Writer.WriteString(Attempt.ExceptionPayloadState != nullptr
			? Attempt.ExceptionPayloadState : TEXT(""));
		Writer.WriteUInt32(Attempt.bRecursionGuardAvailable);
	}

	Writer.WriteUInt32(Diagnostics.SemanticFunctionCount);
	for (uint32 Index = 0;
		Diagnostics.SemanticFunctions != nullptr
			&& Index < Diagnostics.SemanticFunctionCount;
		++Index)
	{
		const FAngelscriptJITProviderCanonicalFunctionDiagnostic& Function =
			Diagnostics.SemanticFunctions[Index];
		Writer.WriteHash(Function.FunctionKey.Hash);
		Writer.WriteString(Function.Role != nullptr ? Function.Role : TEXT(""));
		Writer.WriteUInt32(Function.RawTraitBits);
		Writer.WriteUInt32(Function.UnknownTraitBits);
		Writer.WriteString(Function.InvocationKind != nullptr
			? Function.InvocationKind : TEXT(""));
		Writer.WriteString(Function.ReceiverKind != nullptr
			? Function.ReceiverKind : TEXT(""));
		Writer.WriteUInt32(static_cast<uint32>(Function.ReceiverParameterIndex));
		Writer.WriteString(Function.CanonicalASTState != nullptr
			? Function.CanonicalASTState : TEXT(""));
		Writer.WriteString(Function.CleanupPlanState != nullptr
			? Function.CleanupPlanState : TEXT(""));
		Writer.WriteUInt32(Function.LifetimeProtocolRevision);
		Writer.WriteHash(Function.LifetimeStableKey);
		Writer.WriteHash(Function.LifetimeABIKey);
		Writer.WriteUInt32(Function.LifetimeActionFlags);
		Writer.WriteUInt32(Function.LifetimeExitPlanFlags);
		Writer.WriteUInt32(Function.LifetimeSupportedExitMask);
		Writer.WriteUInt32(Function.LifetimeRecordCount);
		Writer.WriteUInt32(Function.LifetimeExitPlanCount);
		Writer.WriteUInt32(Function.bNativeObjectFrameABIAvailable);
		Writer.WriteUInt32(Function.bHasSuspendState);
		Writer.WriteUInt32(Function.bHasExceptionCleanup);
		Writer.WriteUInt32(Function.bCleanupPlanCoversAllTransfers);
		Writer.WriteUInt32(static_cast<uint32>(Function.ComponentIndex));
		Writer.WriteUInt32(Function.bRecursiveComponent);
		Writer.WriteString(Function.EligibilityReason != nullptr
			? Function.EligibilityReason : TEXT(""));
		Writer.WriteString(Function.EligibilityDetail != nullptr
			? Function.EligibilityDetail : TEXT(""));
		WriteDiagnosticSource(Writer, Function.EligibilitySource);
	}

	Writer.WriteUInt32(Diagnostics.CallCount);
	for (uint32 Index = 0;
		Diagnostics.Calls != nullptr && Index < Diagnostics.CallCount;
		++Index)
	{
		const FAngelscriptJITProviderTypedCallDiagnostic& Call =
			Diagnostics.Calls[Index];
		Writer.WriteHash(Call.CallerFunctionKey.Hash);
		Writer.WriteHash(Call.CalleeFunctionKey.Hash);
		Writer.WriteUInt32(Call.ExpressionIndex);
		Writer.WriteUInt32(static_cast<uint32>(Call.SourceFlags));
		WriteDiagnosticSource(Writer, Call.ProcessedSource);
		WriteDiagnosticSource(Writer, Call.AuthoredSource);
		WriteDiagnosticSource(Writer, Call.GeneratedSource);
		Writer.WriteString(Call.TargetKind != nullptr
			? Call.TargetKind : TEXT(""));
		Writer.WriteString(Call.Disposition != nullptr
			? Call.Disposition : TEXT(""));
		Writer.WriteString(Call.CalleeEligibilityReason != nullptr
			? Call.CalleeEligibilityReason : TEXT(""));
		Writer.WriteString(Call.CalleeEligibilityDetail != nullptr
			? Call.CalleeEligibilityDetail : TEXT(""));
		WriteDiagnosticSource(Writer, Call.CalleeEligibilitySource);
		Writer.WriteUInt32(Call.bHasNativeLinkageValidation);
		Writer.WriteString(Call.NativeLinkageValidation != nullptr
			? Call.NativeLinkageValidation : TEXT(""));
		Writer.WriteString(Call.NativeLinkageValidationDetail != nullptr
			? Call.NativeLinkageValidationDetail : TEXT(""));
		Writer.WriteString(Call.CanonicalDeclaration != nullptr
			? Call.CanonicalDeclaration : TEXT(""));
		Writer.WriteUInt32(static_cast<uint32>(Call.EmittedRoute));
		Writer.WriteString(Call.EmittedCppCallee != nullptr
			? Call.EmittedCppCallee : TEXT(""));
		Writer.WriteString(Call.RuntimeDllCore != nullptr
			? Call.RuntimeDllCore : TEXT(""));
		Writer.WriteString(Call.MetadataRowSymbol != nullptr
			? Call.MetadataRowSymbol : TEXT(""));
		Writer.WriteString(Call.RegisteredTarget != nullptr
			? Call.RegisteredTarget : TEXT(""));
		Writer.WriteString(Call.DirectCppSymbol != nullptr
			? Call.DirectCppSymbol : TEXT(""));
		Writer.WriteHash(Call.StableTargetKey);
		Writer.WriteHash(Call.ExpectedAbi);
		Writer.WriteUInt32(static_cast<uint32>(Call.ReferenceSlotIndex));
	}
	return Writer.FinalizeHash();
}

namespace
{
	static bool IsValidDiagnosticSource(
		const FAngelscriptJITProviderDiagnosticSource& Source)
	{
		return Source.StructSize
				== sizeof(FAngelscriptJITProviderDiagnosticSource)
			&& IsPresent(Source.Section);
	}

	static FAngelscriptJITProviderValidationResult ValidateDiagnostics(
		const FAngelscriptJITProviderView& View)
	{
		const FAngelscriptJITProviderDiagnostics* const Diagnostics =
			View.Diagnostics;
		if (Diagnostics == nullptr)
		{
			return {};
		}
		if (Diagnostics->StructSize != sizeof(*Diagnostics))
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::InvalidDiagnosticSize);
		}
		if (Diagnostics->SchemaRevision
			!= FAngelscriptJITProviderDiagnosticsAbi::SchemaRevision)
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::
					UnsupportedDiagnosticSchema);
		}
		if (Diagnostics->DiagnosticDigest.IsZero())
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::InvalidDiagnosticDigest);
		}
		if (!IsNonEmpty(Diagnostics->RequestKind)
			|| !IsNonEmpty(Diagnostics->RequestedBackendId)
			|| !IsNonEmpty(Diagnostics->CaptureState)
			|| !IsPresent(Diagnostics->TaskError)
			|| !IsBoolean(Diagnostics->bHasVerifiedCanonicalAST))
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::InvalidDiagnosticString);
		}
		if (Diagnostics->FallbackChainCount
				> FAngelscriptJITProviderDiagnosticsAbi::
					MaximumFallbackChainCount
			|| ((Diagnostics->FallbackChainCount == 0)
				!= (Diagnostics->FallbackChain == nullptr)))
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::
					InvalidDiagnosticFallbackChain);
		}
		for (uint32 Index = 0;
			Index < Diagnostics->FallbackChainCount; ++Index)
		{
			if (!IsNonEmpty(Diagnostics->FallbackChain[Index]))
			{
				return DiagnosticFailure(
					EAngelscriptJITProviderValidationCode::
						InvalidDiagnosticFallbackChain);
			}
		}

		if (Diagnostics->FunctionCount != View.EntryCount)
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::
					InvalidDiagnosticFunctionCount);
		}
		if ((Diagnostics->FunctionCount == 0)
			!= (Diagnostics->Functions == nullptr))
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::
					MissingDiagnosticFunctionTable);
		}
		if (Diagnostics->BackendAttemptCount
			> FAngelscriptJITProviderDiagnosticsAbi::
				MaximumBackendAttemptCount)
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::
					InvalidDiagnosticAttemptCount);
		}
		if ((Diagnostics->BackendAttemptCount == 0)
			!= (Diagnostics->BackendAttempts == nullptr))
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::
					MissingDiagnosticAttemptTable);
		}
		if (Diagnostics->SemanticFunctionCount
			> FAngelscriptJITProviderDiagnosticsAbi::
				MaximumSemanticFunctionCount)
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::
					InvalidDiagnosticSemanticCount);
		}
		if ((Diagnostics->SemanticFunctionCount == 0)
			!= (Diagnostics->SemanticFunctions == nullptr))
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::
					MissingDiagnosticSemanticTable);
		}
		if (Diagnostics->CallCount
			> FAngelscriptJITProviderDiagnosticsAbi::MaximumCallCount)
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::InvalidDiagnosticCallCount);
		}
		if ((Diagnostics->CallCount == 0) != (Diagnostics->Calls == nullptr))
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::MissingDiagnosticCallTable);
		}

		uint32 NextAttempt = 0;
		for (uint32 FunctionIndex = 0;
			FunctionIndex < Diagnostics->FunctionCount; ++FunctionIndex)
		{
			const FAngelscriptJITProviderFunctionDiagnostic& Function =
				Diagnostics->Functions[FunctionIndex];
			const int32 SignedFunctionIndex = static_cast<int32>(FunctionIndex);
			if (Function.StructSize != sizeof(Function))
			{
				return DiagnosticFailure(
					EAngelscriptJITProviderValidationCode::
						InvalidDiagnosticFunctionSize,
					SignedFunctionIndex);
			}
			if (Function.FunctionKey.Hash.IsZero())
			{
				return DiagnosticFailure(
					EAngelscriptJITProviderValidationCode::
						InvalidDiagnosticFunctionKey,
					SignedFunctionIndex);
			}
			if (!(Function.FunctionKey == View.Entries[FunctionIndex].FunctionKey)
				|| Function.BackendAttemptStart != NextAttempt
				|| !RangeFits(Function.BackendAttemptStart,
					Function.BackendAttemptCount,
					Diagnostics->BackendAttemptCount))
			{
				return DiagnosticFailure(
					EAngelscriptJITProviderValidationCode::
						InvalidDiagnosticFunctionOrder,
					SignedFunctionIndex);
			}
			if (!IsPresent(Function.ActualBackendId)
				|| !IsNonEmpty(Function.Disposition)
				|| !IsPresent(Function.Detail))
			{
				return DiagnosticFailure(
					EAngelscriptJITProviderValidationCode::InvalidDiagnosticString,
					SignedFunctionIndex);
			}
			NextAttempt += Function.BackendAttemptCount;
		}
		if (NextAttempt != Diagnostics->BackendAttemptCount)
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::InvalidDiagnosticAttemptRange);
		}

		static constexpr uint32 ValidAttemptFlags =
			static_cast<uint32>(
				EAngelscriptJITProviderDiagnosticAttemptFlags::HasTypedDiagnostic)
			| static_cast<uint32>(
				EAngelscriptJITProviderDiagnosticAttemptFlags::HasSemanticFunction)
			| static_cast<uint32>(
				EAngelscriptJITProviderDiagnosticAttemptFlags::HasCallClosure);
		uint32 NextSemantic = 0;
		uint32 NextCall = 0;
		for (uint32 AttemptIndex = 0;
			AttemptIndex < Diagnostics->BackendAttemptCount; ++AttemptIndex)
		{
			const FAngelscriptJITProviderBackendAttemptDiagnostic& Attempt =
				Diagnostics->BackendAttempts[AttemptIndex];
			const int32 SignedAttemptIndex = static_cast<int32>(AttemptIndex);
			if (Attempt.StructSize != sizeof(Attempt))
			{
				return DiagnosticFailure(
					EAngelscriptJITProviderValidationCode::
						InvalidDiagnosticAttemptSize,
					INDEX_NONE, SignedAttemptIndex);
			}
			const uint32 Flags = static_cast<uint32>(Attempt.Flags);
			const bool bHasSemantic = EnumHasAnyFlags(
				Attempt.Flags,
				EAngelscriptJITProviderDiagnosticAttemptFlags::HasSemanticFunction);
			const bool bHasCalls = EnumHasAnyFlags(
				Attempt.Flags,
				EAngelscriptJITProviderDiagnosticAttemptFlags::HasCallClosure);
			if ((Flags & ~ValidAttemptFlags) != 0
				|| bHasSemantic != (Attempt.SemanticFunctionCount > 0)
				|| bHasCalls != (Attempt.CallCount > 0)
				|| Attempt.SemanticFunctionStart != NextSemantic
				|| Attempt.CallStart != NextCall
				|| !RangeFits(Attempt.SemanticFunctionStart,
					Attempt.SemanticFunctionCount,
					Diagnostics->SemanticFunctionCount)
				|| !RangeFits(Attempt.CallStart,
					Attempt.CallCount, Diagnostics->CallCount)
				|| (Attempt.PrimarySemanticFunctionIndex != INDEX_NONE
					&& (Attempt.PrimarySemanticFunctionIndex
						< static_cast<int32>(Attempt.SemanticFunctionStart)
						|| Attempt.PrimarySemanticFunctionIndex
							>= static_cast<int32>(Attempt.SemanticFunctionStart
								+ Attempt.SemanticFunctionCount))))
			{
				return DiagnosticFailure(
					EAngelscriptJITProviderValidationCode::
						InvalidDiagnosticAttemptRange,
					INDEX_NONE, SignedAttemptIndex);
			}
			if (!IsNonEmpty(Attempt.BackendId)
				|| !IsNonEmpty(Attempt.Disposition)
				|| !IsPresent(Attempt.Detail)
				|| !IsPresent(Attempt.FallbackReason)
				|| !IsPresent(Attempt.TypedDetail)
				|| !IsValidDiagnosticSource(Attempt.ProcessedSource)
				|| !IsNonEmpty(Attempt.ProfileKind)
				|| !IsNonEmpty(Attempt.ExceptionControlState)
				|| !IsNonEmpty(Attempt.ExceptionPayloadState)
				|| !IsBoolean(Attempt.bRecursionGuardAvailable)
				|| (static_cast<uint32>(Attempt.RequiredCapabilities)
					& ~FAngelscriptJITExecutionCapabilityMask::All) != 0
				|| (static_cast<uint32>(Attempt.AvailableCapabilities)
					& ~FAngelscriptJITExecutionCapabilityMask::All) != 0)
			{
				return DiagnosticFailure(
					EAngelscriptJITProviderValidationCode::InvalidDiagnosticString,
					INDEX_NONE, SignedAttemptIndex);
			}
			NextSemantic += Attempt.SemanticFunctionCount;
			NextCall += Attempt.CallCount;
		}
		if (NextSemantic != Diagnostics->SemanticFunctionCount)
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::InvalidDiagnosticSemanticRange);
		}
		if (NextCall != Diagnostics->CallCount)
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::InvalidDiagnosticCallRange);
		}

		for (uint32 SemanticIndex = 0;
			SemanticIndex < Diagnostics->SemanticFunctionCount; ++SemanticIndex)
		{
			const FAngelscriptJITProviderCanonicalFunctionDiagnostic& Function =
				Diagnostics->SemanticFunctions[SemanticIndex];
			const int32 SignedSemanticIndex = static_cast<int32>(SemanticIndex);
			if (Function.StructSize != sizeof(Function))
			{
				return DiagnosticFailure(
					EAngelscriptJITProviderValidationCode::
						InvalidDiagnosticSemanticSize,
					INDEX_NONE, INDEX_NONE, SignedSemanticIndex);
			}
			static constexpr uint32 ValidLifetimeActionFlags = 0x0fu;
			static constexpr uint32 ValidLifetimeExitPlanFlags = 0x07u;
			static constexpr uint32 ValidLifetimeExitMask = 0x3fu;
			const bool bEmptyLifetime = Function.LifetimeRecordCount == 0;
			const bool bAuthenticatedLifetimeSummary =
				Function.LifetimeProtocolRevision != 0
				&& !Function.LifetimeStableKey.IsZero()
				&& !Function.LifetimeABIKey.IsZero()
				&& (Function.LifetimeActionFlags
					& ~ValidLifetimeActionFlags) == 0
				&& (Function.LifetimeExitPlanFlags
					& ~ValidLifetimeExitPlanFlags) == 0
				&& (Function.LifetimeSupportedExitMask
					& ~ValidLifetimeExitMask) == 0
				&& IsBoolean(Function.bNativeObjectFrameABIAvailable)
				&& (bEmptyLifetime
					? Function.LifetimeActionFlags == 0
						&& Function.LifetimeExitPlanFlags == 0
						&& Function.LifetimeSupportedExitMask == 0
						&& Function.LifetimeExitPlanCount == 0
						&& Function.bNativeObjectFrameABIAvailable == 1
					: Function.LifetimeActionFlags != 0
						&& Function.LifetimeExitPlanFlags != 0
						&& Function.LifetimeSupportedExitMask != 0
						&& Function.LifetimeExitPlanCount != 0);
			const bool bAbsentLifetimeSummary =
				Function.LifetimeProtocolRevision == 0
				&& Function.LifetimeStableKey.IsZero()
				&& Function.LifetimeABIKey.IsZero()
				&& Function.LifetimeActionFlags == 0
				&& Function.LifetimeExitPlanFlags == 0
				&& Function.LifetimeSupportedExitMask == 0
				&& Function.LifetimeRecordCount == 0
				&& Function.LifetimeExitPlanCount == 0
				&& Function.bNativeObjectFrameABIAvailable == 0;
			const bool bVerifiedCanonicalAST =
				Function.CanonicalASTState != nullptr
					&& FCString::Strcmp(
						Function.CanonicalASTState, TEXT("Verified")) == 0;
			const bool bUnverifiedCleanupPlan =
				Function.CleanupPlanState != nullptr
					&& FCString::Strcmp(
						Function.CleanupPlanState, TEXT("Unverified")) == 0;
			const bool bLifetimeSummaryValid =
				(bAuthenticatedLifetimeSummary
					&& bVerifiedCanonicalAST
					&& !bUnverifiedCleanupPlan)
				|| (bAbsentLifetimeSummary && bUnverifiedCleanupPlan);
			if (Function.FunctionKey.Hash.IsZero()
				|| !IsNonEmpty(Function.Role)
				|| !IsNonEmpty(Function.InvocationKind)
				|| !IsNonEmpty(Function.ReceiverKind)
				|| !IsNonEmpty(Function.CanonicalASTState)
				|| !IsNonEmpty(Function.CleanupPlanState)
				|| !bLifetimeSummaryValid
				|| !IsBoolean(Function.bHasSuspendState)
				|| !IsBoolean(Function.bHasExceptionCleanup)
				|| !IsBoolean(Function.bCleanupPlanCoversAllTransfers)
				|| !IsBoolean(Function.bRecursiveComponent)
				|| !IsNonEmpty(Function.EligibilityReason)
				|| !IsPresent(Function.EligibilityDetail)
				|| !IsValidDiagnosticSource(Function.EligibilitySource))
			{
				return DiagnosticFailure(
					EAngelscriptJITProviderValidationCode::InvalidDiagnosticString,
					INDEX_NONE, INDEX_NONE, SignedSemanticIndex);
			}
		}

		static constexpr uint32 ValidSourceFlags =
			static_cast<uint32>(
				EAngelscriptJITProviderDiagnosticSourceFlags::HasAuthoredSource)
			| static_cast<uint32>(
				EAngelscriptJITProviderDiagnosticSourceFlags::HasGeneratedSource);
		for (uint32 CallIndex = 0; CallIndex < Diagnostics->CallCount; ++CallIndex)
		{
			const FAngelscriptJITProviderTypedCallDiagnostic& Call =
				Diagnostics->Calls[CallIndex];
			const int32 SignedCallIndex = static_cast<int32>(CallIndex);
			const bool bScriptFunctionTarget = Call.TargetKind != nullptr
				&& FCString::Strcmp(Call.TargetKind, TEXT("ScriptFunction")) == 0;
			if (Call.StructSize != sizeof(Call))
			{
				return DiagnosticFailure(
					EAngelscriptJITProviderValidationCode::InvalidDiagnosticCallSize,
					INDEX_NONE, INDEX_NONE, INDEX_NONE, SignedCallIndex);
			}
			if (Call.CallerFunctionKey.Hash.IsZero()
				|| (bScriptFunctionTarget
					&& Call.CalleeFunctionKey.Hash.IsZero())
				|| (static_cast<uint32>(Call.SourceFlags)
					& ~ValidSourceFlags) != 0
				|| !IsValidDiagnosticSource(Call.ProcessedSource)
				|| !IsValidDiagnosticSource(Call.AuthoredSource)
				|| !IsValidDiagnosticSource(Call.GeneratedSource)
				|| !IsNonEmpty(Call.TargetKind)
				|| !IsNonEmpty(Call.Disposition)
				|| !IsNonEmpty(Call.CalleeEligibilityReason)
				|| !IsPresent(Call.CalleeEligibilityDetail)
				|| !IsValidDiagnosticSource(Call.CalleeEligibilitySource)
				|| !IsBoolean(Call.bHasNativeLinkageValidation)
				|| !IsNonEmpty(Call.NativeLinkageValidation)
				|| !IsPresent(Call.NativeLinkageValidationDetail)
				|| !IsPresent(Call.CanonicalDeclaration)
				|| !IsPresent(Call.EmittedCppCallee)
				|| !IsPresent(Call.RuntimeDllCore)
				|| !IsPresent(Call.MetadataRowSymbol)
				|| !IsPresent(Call.RegisteredTarget)
				|| !IsPresent(Call.DirectCppSymbol))
			{
				return DiagnosticFailure(
					EAngelscriptJITProviderValidationCode::InvalidDiagnosticString,
					INDEX_NONE, INDEX_NONE, INDEX_NONE, SignedCallIndex);
			}
		}

		if (!(FAngelscriptJITProviderManifest::BuildDiagnosticsDigest(*Diagnostics)
			== Diagnostics->DiagnosticDigest))
		{
			return DiagnosticFailure(
				EAngelscriptJITProviderValidationCode::DiagnosticDigestMismatch);
		}
		return {};
	}
}

FAngelscriptJITProviderGeneration
FAngelscriptJITProviderManifest::BuildProviderGeneration(
	const FAngelscriptJITProviderView& View)
{
	FAngelscriptArtifactCanonicalWriter Writer(
		TEXT("jit-provider-generation-v1"));
	Writer.WriteUInt32(View.AbiRevision);
	Writer.WriteHash(View.ProviderId.Hash);
	Writer.WriteHash(View.ArtifactSetDigest);
	Writer.WriteHash(View.ArtifactProfile.Hash);
	Writer.WriteHash(View.NativeEnvironmentFingerprint);
	Writer.WriteUInt32(View.EntryCount);
	return FAngelscriptJITProviderGeneration{Writer.FinalizeHash()};
}

FAngelscriptJITProviderValidationResult
FAngelscriptJITProviderManifest::Validate(
	const FAngelscriptJITProviderView& View)
{
	if (View.StructSize != sizeof(FAngelscriptJITProviderView))
	{
		return Failure(EAngelscriptJITProviderValidationCode::InvalidViewSize);
	}
	if (View.AbiRevision != FAngelscriptJITProviderAbi::Revision)
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::UnsupportedAbiRevision);
	}
	if (!View.ProviderId.IsValid())
	{
		return Failure(EAngelscriptJITProviderValidationCode::InvalidProviderId);
	}
	if (!View.ProviderGeneration.IsValid())
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::InvalidProviderGeneration);
	}
	if (View.ProviderName == nullptr || View.ProviderName[0] == TEXT('\0'))
	{
		return Failure(EAngelscriptJITProviderValidationCode::MissingProviderName);
	}
	if (View.OwnerModuleName == nullptr
		|| View.OwnerModuleName[0] == TEXT('\0'))
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::MissingOwnerModuleName);
	}
	if (View.ArtifactSetDigest.IsZero())
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::InvalidArtifactSetDigest);
	}
	if (View.ArtifactProfile.Hash.IsZero())
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::InvalidArtifactProfile);
	}
	if (View.NativeEnvironmentFingerprint.IsZero())
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::InvalidEnvironmentFingerprint);
	}
	if (View.EntryCount == 0
		|| View.EntryCount > FAngelscriptJITProviderAbi::MaximumEntryCount)
	{
		return Failure(EAngelscriptJITProviderValidationCode::InvalidEntryCount);
	}
	if (View.Entries == nullptr)
	{
		return Failure(EAngelscriptJITProviderValidationCode::MissingEntryTable);
	}
	static constexpr uint32 ValidEntryFlags =
		static_cast<uint32>(
			EAngelscriptJITArtifactEntryFlags::RequireExactDebugIdentity)
		| static_cast<uint32>(
			EAngelscriptJITArtifactEntryFlags::ImmutableDirectCallSet)
		| static_cast<uint32>(
			EAngelscriptJITArtifactEntryFlags::HasExecutionCapabilityProfile);
	static constexpr uint32 ValidReferenceFlags =
		static_cast<uint32>(EAngelscriptJITReferenceSlotFlags::Optional);

	for (uint32 EntryIndex = 0; EntryIndex < View.EntryCount; ++EntryIndex)
	{
		const FAngelscriptJITArtifactEntry& Entry = View.Entries[EntryIndex];
		const int32 SignedEntryIndex = static_cast<int32>(EntryIndex);
		if (Entry.StructSize != sizeof(FAngelscriptJITArtifactEntry))
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidEntrySize,
				SignedEntryIndex);
		}
		if ((static_cast<uint32>(Entry.Flags) & ~ValidEntryFlags) != 0)
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidEntryFlags,
				SignedEntryIndex);
		}
		if (Entry.ModuleKey.Hash.IsZero())
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidModuleKey,
				SignedEntryIndex);
		}
		if (Entry.FunctionKey.Hash.IsZero())
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidFunctionKey,
				SignedEntryIndex);
		}
		if (Entry.ExecutionHash.IsZero())
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidExecutionHash,
				SignedEntryIndex);
		}
		if (Entry.DebugHash.IsZero())
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidDebugHash,
				SignedEntryIndex);
		}
		if (!(Entry.ArtifactProfile.Hash == View.ArtifactProfile.Hash))
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidEntryProfile,
				SignedEntryIndex);
		}
		if (!(Entry.NativeEnvironmentFingerprint
			== View.NativeEnvironmentFingerprint))
		{
			return Failure(
				EAngelscriptJITProviderValidationCode::InvalidEntryEnvironment,
				SignedEntryIndex);
		}
		if (Entry.EntryAbiHash.IsZero())
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidEntryAbi,
				SignedEntryIndex);
		}
		if (!Entry.EntryPoints.HasAny())
		{
			return Failure(EAngelscriptJITProviderValidationCode::MissingEntryPoints,
				SignedEntryIndex);
		}
		const bool bHasExecutionCapabilityProfile = EnumHasAnyFlags(
			Entry.Flags,
			EAngelscriptJITArtifactEntryFlags::HasExecutionCapabilityProfile);
		const uint32 AvailableExecutionCapabilities = static_cast<uint32>(
			Entry.AvailableExecutionCapabilities);
		if ((AvailableExecutionCapabilities
				& ~FAngelscriptJITExecutionCapabilityMask::All) != 0
			|| (!bHasExecutionCapabilityProfile
				&& AvailableExecutionCapabilities != 0))
		{
			return Failure(
				EAngelscriptJITProviderValidationCode::InvalidExecutionCapabilities,
				SignedEntryIndex);
		}
		if (bHasExecutionCapabilityProfile
				== Entry.ExecutionCapabilityProfileHash.IsZero())
		{
			return Failure(
				EAngelscriptJITProviderValidationCode::InvalidExecutionCapabilityProfile,
				SignedEntryIndex);
		}
		const uint32 RuntimeExecutionRequirementTriggers =
			static_cast<uint32>(
				Entry.RuntimeExecutionRequirementTriggers);
		if ((RuntimeExecutionRequirementTriggers
				& ~FAngelscriptJITExecutionCapabilityMask::All) != 0
			|| (!bHasExecutionCapabilityProfile
				&& RuntimeExecutionRequirementTriggers != 0))
		{
			return Failure(
				EAngelscriptJITProviderValidationCode::
					InvalidExecutionRequirementTriggers,
				SignedEntryIndex);
		}
		if (Entry.ReferenceSlotCount
			> FAngelscriptJITProviderAbi::MaximumReferenceSlotCount)
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidReferenceCount,
				SignedEntryIndex);
		}
		if ((Entry.ReferenceSlotCount == 0) != (Entry.ReferenceSlots == nullptr))
		{
			return Failure(EAngelscriptJITProviderValidationCode::MissingReferenceTable,
				SignedEntryIndex);
		}
		for (uint32 ReferenceIndex = 0;
			ReferenceIndex < Entry.ReferenceSlotCount;
			++ReferenceIndex)
		{
			const FAngelscriptJITReferenceSlot& Slot =
				Entry.ReferenceSlots[ReferenceIndex];
			const int32 SignedReferenceIndex =
				static_cast<int32>(ReferenceIndex);
			if (Slot.StructSize != sizeof(FAngelscriptJITReferenceSlot))
			{
				return Failure(
					EAngelscriptJITProviderValidationCode::InvalidReferenceSize,
					SignedEntryIndex, SignedReferenceIndex);
			}
			if ((static_cast<uint32>(Slot.Flags) & ~ValidReferenceFlags) != 0)
			{
				return Failure(
					EAngelscriptJITProviderValidationCode::InvalidReferenceFlags,
					SignedEntryIndex, SignedReferenceIndex);
			}
			if (!Slot.Reference.IsValid())
			{
				return Failure(EAngelscriptJITProviderValidationCode::InvalidReference,
					SignedEntryIndex, SignedReferenceIndex);
			}
			if (Slot.Reference.ExpectedAbi.IsZero())
			{
				return Failure(EAngelscriptJITProviderValidationCode::InvalidReferenceAbi,
					SignedEntryIndex, SignedReferenceIndex);
			}
			if (Slot.SlotIndex != ReferenceIndex)
			{
				return Failure(EAngelscriptJITProviderValidationCode::InvalidReferenceOrder,
					SignedEntryIndex, SignedReferenceIndex);
			}
		}

		if (Entry.NativeCallSiteCount
			> FAngelscriptJITProviderAbi::MaximumNativeCallSiteCount)
		{
			return Failure(
				EAngelscriptJITProviderValidationCode::InvalidNativeCallSiteCount,
				SignedEntryIndex);
		}
		if ((Entry.NativeCallSiteCount == 0)
			!= (Entry.NativeCallSites == nullptr))
		{
			return Failure(
				EAngelscriptJITProviderValidationCode::MissingNativeCallSiteTable,
				SignedEntryIndex);
		}
		for (uint32 CallSiteIndex = 0;
			CallSiteIndex < Entry.NativeCallSiteCount;
			++CallSiteIndex)
		{
			const FAngelscriptJITNativeCallSite& CallSite =
				Entry.NativeCallSites[CallSiteIndex];
			const int32 SignedCallSiteIndex =
				static_cast<int32>(CallSiteIndex);
			auto CallSiteFailure = [SignedEntryIndex, SignedCallSiteIndex](
				const EAngelscriptJITProviderValidationCode Code)
			{
				return Failure(Code, SignedEntryIndex, INDEX_NONE,
					SignedCallSiteIndex);
			};
			if (CallSite.StructSize != sizeof(FAngelscriptJITNativeCallSite))
			{
				return CallSiteFailure(
					EAngelscriptJITProviderValidationCode::InvalidNativeCallSiteSize);
			}
			if (CallSite.CallSiteIndex != CallSiteIndex)
			{
				return CallSiteFailure(
					EAngelscriptJITProviderValidationCode::InvalidNativeCallSiteOrder);
			}
			if (CallSite.Route
					< EAngelscriptJITNativeCallSiteRoute::DirectExported
				|| CallSite.Route
					> EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding)
			{
				return CallSiteFailure(
					EAngelscriptJITProviderValidationCode::InvalidNativeCallSiteRoute);
			}
			if (!IsNonEmpty(CallSite.CanonicalDeclaration))
			{
				return CallSiteFailure(
					EAngelscriptJITProviderValidationCode::MissingNativeCallSiteDeclaration);
			}
			if (!IsNonEmpty(CallSite.EmittedCppCallee))
			{
				return CallSiteFailure(
					EAngelscriptJITProviderValidationCode::MissingNativeCallSiteCallee);
			}
			if (!IsNonEmpty(CallSite.RegisteredTarget))
			{
				return CallSiteFailure(
					EAngelscriptJITProviderValidationCode::MissingNativeCallSiteTarget);
			}

			const bool bBoundCallSite =
				CallSite.Route == EAngelscriptJITNativeCallSiteRoute::Bridge
				|| CallSite.Route
					== EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding;
			if (bBoundCallSite)
			{
				const bool bValidSlot = CallSite.ReferenceSlotIndex >= 0
					&& static_cast<uint32>(CallSite.ReferenceSlotIndex)
						< Entry.ReferenceSlotCount;
				const FAngelscriptJITReferenceSlot* const Slot = bValidSlot
					? &Entry.ReferenceSlots[CallSite.ReferenceSlotIndex]
					: nullptr;
				const TCHAR* const ExpectedRuntimeCore =
					CallSite.Route == EAngelscriptJITNativeCallSiteRoute::Bridge
						? TEXT("AngelscriptTypedASTJIT::InvokeBoundViaVM")
						: TEXT("AngelscriptTypedASTJIT::ResolveBoundNative");
				if (!IsNonEmpty(CallSite.RuntimeDllCore)
					|| FCString::Strcmp(CallSite.RuntimeDllCore,
						ExpectedRuntimeCore) != 0
					|| !IsNonEmpty(CallSite.MetadataRowSymbol)
					|| CallSite.StableTargetKey.IsZero()
					|| CallSite.ExpectedAbi.IsZero()
					|| Slot == nullptr
					|| Slot->Reference.Kind
						!= EAngelscriptArtifactReferenceKind::EnvironmentSymbol
					|| !(Slot->Reference.StableKey
						== CallSite.StableTargetKey)
					|| !(Slot->Reference.ExpectedAbi == CallSite.ExpectedAbi))
				{
					return CallSiteFailure(CallSite.Route
						== EAngelscriptJITNativeCallSiteRoute::Bridge
							? EAngelscriptJITProviderValidationCode::InvalidBridgeNativeCallSite
							: EAngelscriptJITProviderValidationCode::InvalidCurrentNativeBindingCallSite);
				}
			}
			else if (IsNonEmpty(CallSite.RuntimeDllCore)
				|| IsNonEmpty(CallSite.MetadataRowSymbol)
				|| !CallSite.StableTargetKey.IsZero()
				|| !CallSite.ExpectedAbi.IsZero()
				|| CallSite.ReferenceSlotIndex != INDEX_NONE)
			{
				return CallSiteFailure(
					EAngelscriptJITProviderValidationCode::InvalidDirectNativeCallSite);
			}
		}

		if (EntryIndex > 0)
		{
			const int32 Comparison =
				CompareEntryIdentity(View.Entries[EntryIndex - 1], Entry);
			if (Comparison == 0)
			{
				return Failure(EAngelscriptJITProviderValidationCode::DuplicateEntry,
					SignedEntryIndex);
			}
			if (Comparison > 0)
			{
				return Failure(EAngelscriptJITProviderValidationCode::UnsortedEntries,
					SignedEntryIndex);
			}
		}
	}

	if (View.SemanticDependencyCount
		> FAngelscriptJITProviderAbi::MaximumSemanticDependencyCount)
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::InvalidSemanticDependencyCount);
	}
	if ((View.SemanticDependencyCount == 0)
		!= (View.SemanticDependencies == nullptr))
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::MissingSemanticDependencyTable);
	}

	auto SemanticFailure = [](
		const EAngelscriptJITProviderValidationCode Code,
		const int32 EntryIndex = INDEX_NONE,
		const int32 DependencyIndex = INDEX_NONE)
	{
		FAngelscriptJITProviderValidationResult Result;
		Result.Code = Code;
		Result.EntryIndex = EntryIndex;
		Result.SemanticDependencyIndex = DependencyIndex;
		return Result;
	};

	for (uint32 DependencyIndex = 0;
		DependencyIndex < View.SemanticDependencyCount;
		++DependencyIndex)
	{
		const FAngelscriptJITSemanticDependencyRow& Row =
			View.SemanticDependencies[DependencyIndex];
		const int32 SignedDependencyIndex =
			static_cast<int32>(DependencyIndex);
		if (Row.StructSize != sizeof(FAngelscriptJITSemanticDependencyRow))
		{
			return SemanticFailure(
				EAngelscriptJITProviderValidationCode::
					InvalidSemanticDependencySize,
				INDEX_NONE,
				SignedDependencyIndex);
		}
		if (Row.RowIndex != DependencyIndex)
		{
			return SemanticFailure(
				EAngelscriptJITProviderValidationCode::
					InvalidSemanticDependencyOrder,
				INDEX_NONE,
				SignedDependencyIndex);
		}
		if (Row.Kind <= EAngelscriptArtifactSemanticDependencyKind::Invalid
			|| static_cast<uint8>(Row.Kind)
				> static_cast<uint8>(
					EAngelscriptArtifactSemanticDependencyKind::FunctionContent))
		{
			return SemanticFailure(
				EAngelscriptJITProviderValidationCode::
					InvalidSemanticDependencyKind,
				INDEX_NONE,
				SignedDependencyIndex);
		}
		if (Row.TargetKind <= EAngelscriptArtifactReferenceKind::Invalid
			|| static_cast<uint8>(Row.TargetKind)
				> static_cast<uint8>(
					EAngelscriptArtifactReferenceKind::RuntimeHelper))
		{
			return SemanticFailure(
				EAngelscriptJITProviderValidationCode::
					InvalidSemanticDependencyReference,
				INDEX_NONE,
				SignedDependencyIndex);
		}
		if (Row.TargetStableKey.IsZero())
		{
			return SemanticFailure(
				EAngelscriptJITProviderValidationCode::
					InvalidSemanticDependencyKey,
				INDEX_NONE,
				SignedDependencyIndex);
		}
		if (Row.ExpectedAbi.IsZero())
		{
			return SemanticFailure(
				EAngelscriptJITProviderValidationCode::
					InvalidSemanticDependencyAbi,
				INDEX_NONE,
				SignedDependencyIndex);
		}
		const bool bHasExpectedContent = EnumHasAnyFlags(
			Row.Flags,
			EAngelscriptJITSemanticDependencyFlags::HasExpectedContentOrValue);
		if (SemanticDependencyRequiresExpectedContent(Row.Kind)
			&& (!bHasExpectedContent || Row.ExpectedContentOrValue.IsZero()))
		{
			return SemanticFailure(
				EAngelscriptJITProviderValidationCode::
					MissingSemanticDependencyExpectedContent,
				INDEX_NONE,
				SignedDependencyIndex);
		}
	}

	for (uint32 EntryIndex = 0; EntryIndex < View.EntryCount; ++EntryIndex)
	{
		const FAngelscriptJITArtifactEntry& Entry = View.Entries[EntryIndex];
		const int32 SignedEntryIndex = static_cast<int32>(EntryIndex);
		if (!RangeFits(
				Entry.SemanticDependencyStartIndex,
				Entry.SemanticDependencyCount,
				View.SemanticDependencyCount))
		{
			return SemanticFailure(
				EAngelscriptJITProviderValidationCode::
					InvalidSemanticDependencyRange,
				SignedEntryIndex);
		}
		for (uint32 SliceIndex = 0;
			SliceIndex < Entry.SemanticDependencyCount;
			++SliceIndex)
		{
			const uint32 TableIndex =
				Entry.SemanticDependencyStartIndex + SliceIndex;
			const FAngelscriptJITSemanticDependencyRow& Row =
				View.SemanticDependencies[TableIndex];
			if (SliceIndex > 0)
			{
				const FAngelscriptJITSemanticDependencyRow& Previous =
					View.SemanticDependencies[TableIndex - 1];
				const bool bSameIdentity =
					Previous.Kind == Row.Kind
					&& Previous.TargetKind == Row.TargetKind
					&& Previous.TargetStableKey == Row.TargetStableKey;
				if (bSameIdentity)
				{
					return SemanticFailure(
						EAngelscriptJITProviderValidationCode::
							DuplicateSemanticDependency,
						SignedEntryIndex,
						static_cast<int32>(TableIndex));
				}
				const bool bOrdered =
					static_cast<uint8>(Previous.Kind)
						< static_cast<uint8>(Row.Kind)
					|| (Previous.Kind == Row.Kind
						&& (static_cast<uint8>(Previous.TargetKind)
								< static_cast<uint8>(Row.TargetKind)
							|| (Previous.TargetKind == Row.TargetKind
								&& (Previous.TargetStableKey < Row.TargetStableKey
									|| (Previous.TargetStableKey
											== Row.TargetStableKey
										&& Previous.ExpectedAbi
											< Row.ExpectedAbi)))));
				if (!bOrdered)
				{
					return SemanticFailure(
						EAngelscriptJITProviderValidationCode::
							InvalidSemanticDependencyOrder,
						SignedEntryIndex,
						static_cast<int32>(TableIndex));
				}
			}
		}
	}

	const FAngelscriptJITProviderValidationResult DiagnosticValidation =
		ValidateDiagnostics(View);
	if (!DiagnosticValidation.IsValid())
	{
		return DiagnosticValidation;
	}

	const TConstArrayView<FAngelscriptJITArtifactEntry> Entries(
		View.Entries, static_cast<int32>(View.EntryCount));
	const TConstArrayView<FAngelscriptJITSemanticDependencyRow> Dependencies(
		View.SemanticDependencies,
		static_cast<int32>(View.SemanticDependencyCount));
	if (!(BuildArtifactSetDigest(Entries, Dependencies)
		== View.ArtifactSetDigest))
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::ArtifactSetDigestMismatch);
	}
	if (BuildProviderGeneration(View) != View.ProviderGeneration)
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::ProviderGenerationDigestMismatch);
	}

	return {};
}
