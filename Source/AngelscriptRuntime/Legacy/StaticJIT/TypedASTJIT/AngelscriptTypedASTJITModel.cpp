#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITModel.h"

namespace
{
	bool IsBoundCallRoute(
		const EAngelscriptTypedASTJITCallEmissionRoute Route)
	{
		return Route == EAngelscriptTypedASTJITCallEmissionRoute::Bridge
			|| Route
				== EAngelscriptTypedASTJITCallEmissionRoute::CurrentNativeBinding;
	}

	FString BuildBridgeCallSiteReadableStem(
		const FStringView CanonicalDeclaration)
	{
		FString Name(CanonicalDeclaration);
		int32 OpenParen = INDEX_NONE;
		if (Name.FindChar(TEXT('('), OpenParen))
		{
			Name.LeftInline(OpenParen);
		}
		Name.TrimStartAndEndInline();
		int32 LastSpace = INDEX_NONE;
		if (Name.FindLastChar(TEXT(' '), LastSpace))
		{
			Name.RightChopInline(LastSpace + 1);
		}
		for (TCHAR& Character : Name)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
			{
				Character = TEXT('_');
			}
		}
		if (Name.IsEmpty() || FChar::IsDigit(Name[0]))
		{
			Name = TEXT("Function_") + Name;
		}
		return Name;
	}
}

bool ValidateAngelscriptTypedASTJITFoldedGlobalDependency(
	const TConstArrayView<FAngelscriptTypedASTJITGlobalDependency>
		Dependencies,
	const int32 EngineLocalGlobalPropertyId,
	FString* OutError)
{
	const FAngelscriptTypedASTJITGlobalDependency* Match = nullptr;
	int32 MatchCount = 0;
	for (const FAngelscriptTypedASTJITGlobalDependency& Candidate
		: Dependencies)
	{
		if (Candidate.EngineLocalGlobalPropertyId
			== EngineLocalGlobalPropertyId)
		{
			Match = &Candidate;
			++MatchCount;
		}
	}
	if (EngineLocalGlobalPropertyId < 0 || MatchCount != 1
		|| Match == nullptr)
	{
		if (OutError != nullptr)
		{
			*OutError = FString::Printf(TEXT(
				"FoldedGlobalDependencyCardinality: PropertyId=%d Matches=%d"),
				EngineLocalGlobalPropertyId, MatchCount);
		}
		return false;
	}
	if (Match->Kind
			!= EAngelscriptArtifactSemanticDependencyKind::HardValue
		|| Match->Reference.Kind
			!= EAngelscriptArtifactReferenceKind::ScriptGlobal
		|| !Match->Reference.IsValid()
		|| Match->Reference.ExpectedAbi.IsZero()
		|| !Match->ExpectedContentOrValue.IsSet()
		|| Match->ExpectedContentOrValue.GetValue().IsZero())
	{
		if (OutError != nullptr)
		{
			*OutError = FString::Printf(TEXT(
				"FoldedGlobalDependencyInvalid: PropertyId=%d Kind=%u ReferenceKind=%u Valid=%s Abi=%s Value=%s"),
				EngineLocalGlobalPropertyId,
				static_cast<uint8>(Match->Kind),
				static_cast<uint8>(Match->Reference.Kind),
				Match->Reference.IsValid() ? TEXT("true") : TEXT("false"),
				Match->Reference.ExpectedAbi.IsZero()
					? TEXT("zero") : TEXT("set"),
				Match->ExpectedContentOrValue.IsSet()
					&& !Match->ExpectedContentOrValue.GetValue().IsZero()
					? TEXT("set") : TEXT("missing"));
		}
		return false;
	}
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	return true;
}

bool AssignAngelscriptTypedASTJITBridgeCallSiteSymbols(
	const TArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls,
	const FAngelscriptStableFunctionKey& OwningRootFunctionKey,
	FString* OutError)
{
	auto Fail = [OutError](FString Error)
	{
		if (OutError != nullptr)
		{
			*OutError = MoveTemp(Error);
		}
		return false;
	};
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	if (OwningRootFunctionKey.Hash.IsZero())
	{
		return Fail(TEXT(
			"TypedASTJITBridgeCallSiteOwnerInvalid: bridge row symbols require one stable owning root function key."));
	}
	const FString OwningRootHex =
		OwningRootFunctionKey.Hash.ToHexString();

	TArray<FString> Stems;
	TArray<FString> StableHex;
	Stems.SetNum(NativeCalls.Num());
	StableHex.SetNum(NativeCalls.Num());
	for (int32 Index = 0; Index < NativeCalls.Num(); ++Index)
	{
		FAngelscriptTypedASTJITNativeCallEmissionPlan& Call =
			NativeCalls[Index];
		if (!IsBoundCallRoute(Call.Route))
		{
			continue;
		}
		if (Call.CanonicalDeclaration.IsEmpty()
			|| Call.StableTargetKey.Hash.IsZero()
			|| Call.ExpectedABI.IsZero()
			|| Call.ReferenceSlotIndex < 0)
		{
			return Fail(FString::Printf(TEXT(
				"TypedASTJITBridgeCallSiteMetadataInvalid: Index=%d Declaration=%s Slot=%d"),
				Index, *Call.CanonicalDeclaration,
				Call.ReferenceSlotIndex));
		}
		Stems[Index] = BuildBridgeCallSiteReadableStem(
			Call.CanonicalDeclaration);
		StableHex[Index] = Call.StableTargetKey.Hash.ToHexString();

		for (int32 PreviousIndex = 0;
			PreviousIndex < Index; ++PreviousIndex)
		{
			const FAngelscriptTypedASTJITNativeCallEmissionPlan& Previous =
				NativeCalls[PreviousIndex];
			if (!IsBoundCallRoute(Previous.Route)
				|| !(Previous.StableTargetKey
					== Call.StableTargetKey))
			{
				continue;
			}
			if (Previous.CanonicalDeclaration != Call.CanonicalDeclaration
				|| Previous.RegisteredTarget != Call.RegisteredTarget
				|| !(Previous.ExpectedABI == Call.ExpectedABI)
				|| Previous.ReferenceSlotIndex != Call.ReferenceSlotIndex)
			{
				return Fail(FString::Printf(TEXT(
					"TypedASTJITBridgeDuplicateIdentityMismatch: StableKey=%s FirstIndex=%d DuplicateIndex=%d"),
					*StableHex[Index], PreviousIndex, Index));
			}
		}
	}

	for (int32 Index = 0; Index < NativeCalls.Num(); ++Index)
	{
		FAngelscriptTypedASTJITNativeCallEmissionPlan& Call =
			NativeCalls[Index];
		if (!IsBoundCallRoute(Call.Route))
		{
			continue;
		}

		int32 SuffixLength = 8;
		while (SuffixLength < StableHex[Index].Len())
		{
			bool bCollides = false;
			const FStringView Prefix(StableHex[Index].GetCharArray().GetData(),
				SuffixLength);
			for (int32 OtherIndex = 0;
				OtherIndex < NativeCalls.Num(); ++OtherIndex)
			{
				if (OtherIndex == Index
					|| !IsBoundCallRoute(NativeCalls[OtherIndex].Route)
					|| Stems[OtherIndex] != Stems[Index]
					|| NativeCalls[OtherIndex].StableTargetKey
						== Call.StableTargetKey)
				{
					continue;
				}
				if (FStringView(StableHex[OtherIndex]).Left(SuffixLength)
					.Equals(Prefix, ESearchCase::CaseSensitive))
				{
					bCollides = true;
					break;
				}
			}
			if (!bCollides)
			{
				break;
			}
			SuffixLength = FMath::Min(
				SuffixLength + 4, StableHex[Index].Len());
		}
		Call.CallSiteSymbol = FString::Printf(TEXT("ASJIT_Call_%s_%s_%s"),
			*Stems[Index], *StableHex[Index].Left(SuffixLength),
			*OwningRootHex);
	}

	return true;
}

bool AssignAngelscriptTypedASTJITBridgeCallSiteSymbols(
	const TArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls,
	FString* OutError)
{
	// Preserve the original pure-model API for already-built test consumers.
	// Production module emission always calls the owner-scoped overload below.
	static const FAngelscriptStableFunctionKey CompatibilityOwner{
		FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT(
			"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")))}};
	if (!AssignAngelscriptTypedASTJITBridgeCallSiteSymbols(
		NativeCalls, CompatibilityOwner, OutError))
	{
		return false;
	}

	const FString CompatibilitySuffix = TEXT("_")
		+ CompatibilityOwner.Hash.ToHexString();
	for (FAngelscriptTypedASTJITNativeCallEmissionPlan& Call : NativeCalls)
	{
		if (IsBoundCallRoute(Call.Route)
			&& !Call.CallSiteSymbol.RemoveFromEnd(CompatibilitySuffix))
		{
			if (OutError != nullptr)
			{
				*OutError = TEXT(
					"TypedASTJITBridgeCompatibilityScopeMissing: the scoped symbol did not retain its compatibility owner suffix.");
			}
			return false;
		}
	}
	return true;
}

const TCHAR* LexToString(
	const EAngelscriptTypedASTJITFallbackReason Reason)
{
	switch (Reason)
	{
	case EAngelscriptTypedASTJITFallbackReason::None: return TEXT("None");
	case EAngelscriptTypedASTJITFallbackReason::NotUFunctionRoot: return TEXT("NotUFunctionRoot");
	case EAngelscriptTypedASTJITFallbackReason::MissingCanonicalAST: return TEXT("MissingCanonicalAST");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionKind: return TEXT("UnsupportedFunctionKind");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionTrait: return TEXT("UnsupportedFunctionTrait");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver: return TEXT("UnsupportedReceiver");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedUFunctionFlags: return TEXT("UnsupportedUFunctionFlags");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature: return TEXT("UnsupportedSignature");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedType: return TEXT("UnsupportedType");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedStatement: return TEXT("UnsupportedStatement");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression: return TEXT("UnsupportedExpression");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedCall: return TEXT("UnsupportedCall");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedGlobalStorage: return TEXT("UnsupportedGlobalStorage");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedGlobalInitializer: return TEXT("UnsupportedGlobalInitializer");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedImportedRoute: return TEXT("UnsupportedImportedRoute");
	case EAngelscriptTypedASTJITFallbackReason::SemanticDependencyMismatch: return TEXT("SemanticDependencyMismatch");
	case EAngelscriptTypedASTJITFallbackReason::RuntimeTypeBindingMismatch: return TEXT("RuntimeTypeBindingMismatch");
	case EAngelscriptTypedASTJITFallbackReason::NonPortableNumericConversion: return TEXT("NonPortableNumericConversion");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionObservability: return TEXT("UnsupportedExecutionObservability");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionControl: return TEXT("UnsupportedExecutionControl");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime: return TEXT("UnsupportedLifetime");
	case EAngelscriptTypedASTJITFallbackReason::UnsupportedExceptionRegion: return TEXT("UnsupportedExceptionRegion");
	case EAngelscriptTypedASTJITFallbackReason::ExceptionPayloadUnavailable: return TEXT("ExceptionPayloadUnavailable");
	case EAngelscriptTypedASTJITFallbackReason::BridgeExceptionAdoptionUnavailable: return TEXT("BridgeExceptionAdoptionUnavailable");
	case EAngelscriptTypedASTJITFallbackReason::SuspendOrExceptionState: return TEXT("SuspendOrExceptionState");
	case EAngelscriptTypedASTJITFallbackReason::InvalidEffectiveReceiver: return TEXT("InvalidEffectiveReceiver");
	case EAngelscriptTypedASTJITFallbackReason::InvalidCallEvaluationSequence: return TEXT("InvalidCallEvaluationSequence");
	case EAngelscriptTypedASTJITFallbackReason::InvalidControlTarget: return TEXT("InvalidControlTarget");
	case EAngelscriptTypedASTJITFallbackReason::InvalidCleanupPlan: return TEXT("InvalidCleanupPlan");
	case EAngelscriptTypedASTJITFallbackReason::InvalidCanonicalAST: return TEXT("InvalidCanonicalAST");
	case EAngelscriptTypedASTJITFallbackReason::EmitterFailure: return TEXT("EmitterFailure");
	case EAngelscriptTypedASTJITFallbackReason::BackendUnavailable: return TEXT("BackendUnavailable");
	default: return TEXT("InvalidCanonicalAST");
	}
}
