#include "StaticJIT/StaticJITDiagnostics.h"

#include "Cache/AngelscriptCacheEnvironment.h"
#include "StaticJIT/AngelscriptJITBindingContext.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITProviderMatcher.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptJITProviderRouter.h"
#include "StaticJIT/AngelscriptJITReferenceResolver.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"
#include "StaticJIT/AngelscriptStaticJIT.h"

#include "Artifacts/AngelscriptArtifactIdentity.h"

#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "Misc/FileHelper.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

DEFINE_LOG_CATEGORY_STATIC(LogStaticJITDiagnostics, Log, All);

namespace
{
	using FDiagnosticJsonWriter =
		TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

	void SetError(FString* OutError, const FString& Message)
	{
		if (OutError != nullptr)
		{
			*OutError = Message;
		}
	}

	FAutoConsoleCommand GStaticJITDumpDiagnosticsCommand(
		TEXT("as.StaticJIT.DumpDiagnostics"),
		TEXT("Dump deterministic StaticJIT Provider/Route JSON. Usage: as.StaticJIT.DumpDiagnostics [-Function=<canonical-declaration-or-stable-key>] [-Output=<filename>]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FStaticJITDiagnostics::DumpDiagnostics));
	const TCHAR* RouteName(const EAngelscriptFunctionExecutionRoute Route)
	{
		return Route == EAngelscriptFunctionExecutionRoute::Native
			? TEXT("Native") : TEXT("Vm");
	}

	const TCHAR* MatchName(const EAngelscriptArtifactMatchResult Result)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Exact"), TEXT("MissingArtifactIdentity"),
			TEXT("MissingProviderEntry"), TEXT("ContentMismatch"),
			TEXT("DebugMismatch"), TEXT("ProfileMismatch"),
			TEXT("EnvironmentMismatch"), TEXT("EntryAbiMismatch"),
			TEXT("MissingReference"), TEXT("AmbiguousReference"),
			TEXT("WrongReferenceKind"), TEXT("ReferenceAbiMismatch"),
			TEXT("StaleProviderGeneration"), TEXT("DuplicateProviderEntry"),
			TEXT("AmbiguousExactProvider"), TEXT("ArtifactSetMismatch"),
			TEXT("InvalidProvider"), TEXT("MissingEntryPoint"),
			TEXT("SemanticDependencyMismatch"),
		};
		const uint8 Index = static_cast<uint8>(Result);
		return Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("Unknown");
	}

	const TCHAR* ReferenceKindName(
		const EAngelscriptArtifactReferenceKind Kind)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Invalid"), TEXT("ScriptModule"), TEXT("ScriptType"),
			TEXT("ScriptFunction"), TEXT("ScriptGlobal"),
			TEXT("ScriptProperty"), TEXT("ScriptImport"),
			TEXT("EnvironmentSymbol"), TEXT("CanonicalName"),
			TEXT("StringLiteral"),
		};
		const uint8 Index = static_cast<uint8>(Kind);
		return Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("Unknown");
	}

	const TCHAR* ArtifactRequestKindName(
		const FStaticJITDiagnostics::EArtifactRequestKind RequestKind)
	{
		switch (RequestKind)
		{
		case FStaticJITDiagnostics::EArtifactRequestKind::StaticJITArtifact:
			return TEXT("StaticJITArtifact");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* PrePublicationFailureCategoryName(
		const FStaticJITDiagnostics::EPrePublicationFailureCategory Category)
	{
		switch (Category)
		{
		case FStaticJITDiagnostics::EPrePublicationFailureCategory::Invalid:
			return TEXT("Invalid");
		case FStaticJITDiagnostics::EPrePublicationFailureCategory::
			AuthoritativeEngineStale:
			return TEXT("AuthoritativeEngineStale");
		case FStaticJITDiagnostics::EPrePublicationFailureCategory::
			GenerationContainmentFailure:
			return TEXT("GenerationContainmentFailure");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* PrePublicationBoundaryName(
		const FStaticJITDiagnostics::EPrePublicationBoundary Boundary)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Invalid"),
			TEXT("PrimarySourceAuthority"),
			TEXT("PrimaryPackageOwnership"),
			TEXT("ProviderRegistry"),
			TEXT("PrimaryCache"),
			TEXT("PrimaryRoute"),
			TEXT("UObjectState"),
			TEXT("DelegateState"),
			TEXT("WorldState"),
			TEXT("PooledContext"),
		};
		const uint8 Index = static_cast<uint8>(Boundary);
		return Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("Unknown");
	}

	FString ExecutionCapabilityNames(
		const EAngelscriptJITExecutionCapability Capabilities)
	{
		if (Capabilities == EAngelscriptJITExecutionCapability::None)
		{
			return TEXT("None");
		}
		TArray<FString> Names;
		const auto AddIfPresent = [&Names, Capabilities](
			const EAngelscriptJITExecutionCapability Capability,
			const TCHAR* Name)
		{
			if (EnumHasAnyFlags(Capabilities, Capability))
			{
				Names.Emplace(Name);
			}
		};
		AddIfPresent(EAngelscriptJITExecutionCapability::FramePosition,
			TEXT("FramePosition"));
		AddIfPresent(EAngelscriptJITExecutionCapability::LineCallback,
			TEXT("LineCallback"));
		AddIfPresent(EAngelscriptJITExecutionCapability::DebuggerStep,
			TEXT("DebuggerStep"));
		AddIfPresent(EAngelscriptJITExecutionCapability::DebuggerLocals,
			TEXT("DebuggerLocals"));
		AddIfPresent(EAngelscriptJITExecutionCapability::Coverage,
			TEXT("Coverage"));
		AddIfPresent(EAngelscriptJITExecutionCapability::LoopTimeout,
			TEXT("LoopTimeout"));
		AddIfPresent(EAngelscriptJITExecutionCapability::AbortSuspend,
			TEXT("AbortSuspend"));
		AddIfPresent(EAngelscriptJITExecutionCapability::RecursionBudget,
			TEXT("RecursionBudget"));
		return FString::Join(Names, TEXT("|"));
	}

	const TCHAR* ArtifactCaptureStateName(
		const FStaticJITDiagnostics::EArtifactCaptureState CaptureState)
	{
		switch (CaptureState)
		{
		case FStaticJITDiagnostics::EArtifactCaptureState::Bytecode:
			return TEXT("Bytecode");
		case FStaticJITDiagnostics::EArtifactCaptureState::VerifiedCanonicalAST:
			return TEXT("VerifiedCanonicalAST");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* ArtifactFunctionDispositionName(
		const FStaticJITDiagnostics::EArtifactFunctionDisposition Disposition)
	{
		switch (Disposition)
		{
		case FStaticJITDiagnostics::EArtifactFunctionDisposition::Emitted:
			return TEXT("Emitted");
		case FStaticJITDiagnostics::EArtifactFunctionDisposition::Unsupported:
			return TEXT("Unsupported");
		case FStaticJITDiagnostics::EArtifactFunctionDisposition::Failed:
			return TEXT("Failed");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* ReferenceResolutionName(
		const EAngelscriptJITReferenceResolutionCode Code)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Exact"), TEXT("Missing"), TEXT("Ambiguous"),
			TEXT("WrongKind"), TEXT("AbiMismatch"),
		};
		const uint8 Index = static_cast<uint8>(Code);
		return Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("Unknown");
	}

	const TCHAR* ReferenceAdoptionStateName(
		const FStaticJITDiagnostics::EReferenceAdoptionState State)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Unavailable"), TEXT("Unbound"),
			TEXT("ResolvedNotAdopted"), TEXT("Bound"), TEXT("Stale"),
			TEXT("Rejected"),
		};
		const uint8 Index = static_cast<uint8>(State);
		return Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("Unknown");
	}

	const TCHAR* NativeCallSiteRouteName(
		const EAngelscriptJITNativeCallSiteRoute Route)
	{
		switch (Route)
		{
		case EAngelscriptJITNativeCallSiteRoute::DirectExported:
			return TEXT("DirectExported");
		case EAngelscriptJITNativeCallSiteRoute::DirectInline:
			return TEXT("DirectInline");
		case EAngelscriptJITNativeCallSiteRoute::RuntimeThunk:
			return TEXT("RuntimeThunk");
		case EAngelscriptJITNativeCallSiteRoute::Bridge:
			return TEXT("Bridge");
		case EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding:
			return TEXT("CurrentNativeBinding");
		default:
			return TEXT("Invalid");
		}
	}

#if AS_CAN_GENERATE_JIT
	const TCHAR* TypedCallTargetKindName(
		const EAngelscriptTypedASTJITCallTargetKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptTypedASTJITCallTargetKind::ScriptFunction:
			return TEXT("ScriptFunction");
		case EAngelscriptTypedASTJITCallTargetKind::SystemFunction:
			return TEXT("SystemFunction");
		case EAngelscriptTypedASTJITCallTargetKind::ImportedFunction:
			return TEXT("ImportedFunction");
		case EAngelscriptTypedASTJITCallTargetKind::Unspecified:
		default:
			return TEXT("Unspecified");
		}
	}

	const TCHAR* TypedCallDispositionName(
		const EAngelscriptStaticJITTypedCallDisposition Disposition)
	{
		switch (Disposition)
		{
		case EAngelscriptStaticJITTypedCallDisposition::DirectScript:
			return TEXT("DirectScript");
		case EAngelscriptStaticJITTypedCallDisposition::
			InternalSemanticHelper:
			return TEXT("InternalSemanticHelper");
		case EAngelscriptStaticJITTypedCallDisposition::DirectExported:
			return TEXT("DirectExported");
		case EAngelscriptStaticJITTypedCallDisposition::DirectInline:
			return TEXT("DirectInline");
		case EAngelscriptStaticJITTypedCallDisposition::RuntimeThunk:
			return TEXT("RuntimeThunk");
		case EAngelscriptStaticJITTypedCallDisposition::CurrentNativeBinding:
			return TEXT("CurrentNativeBinding");
		case EAngelscriptStaticJITTypedCallDisposition::Bridge:
			return TEXT("Bridge");
		case EAngelscriptStaticJITTypedCallDisposition::Unsupported:
		default:
			return TEXT("Unsupported");
		}
	}

	const TCHAR* NativeCallValidationName(
		const EAngelscriptStaticJITNativeCallValidationCode Code)
	{
		switch (Code)
		{
#define AS_TYPED_CALL_VALIDATION_NAME(Value) \
		case EAngelscriptStaticJITNativeCallValidationCode::Value: \
			return TEXT(#Value)
		AS_TYPED_CALL_VALIDATION_NAME(Valid);
		AS_TYPED_CALL_VALIDATION_NAME(Absent);
		AS_TYPED_CALL_VALIDATION_NAME(UnknownLinkage);
		AS_TYPED_CALL_VALIDATION_NAME(UnknownVisibility);
		AS_TYPED_CALL_VALIDATION_NAME(InvalidAbsentMetadata);
		AS_TYPED_CALL_VALIDATION_NAME(InvalidVisibility);
		AS_TYPED_CALL_VALIDATION_NAME(MissingSymbol);
		AS_TYPED_CALL_VALIDATION_NAME(InvalidSymbol);
		AS_TYPED_CALL_VALIDATION_NAME(MissingInclude);
		AS_TYPED_CALL_VALIDATION_NAME(InvalidInclude);
		AS_TYPED_CALL_VALIDATION_NAME(MissingOwningModule);
		AS_TYPED_CALL_VALIDATION_NAME(UnknownOwningModule);
		AS_TYPED_CALL_VALIDATION_NAME(PrivateDependency);
		AS_TYPED_CALL_VALIDATION_NAME(InternalLinkage);
		AS_TYPED_CALL_VALIDATION_NAME(MissingApiMacro);
		AS_TYPED_CALL_VALIDATION_NAME(InvalidApiMacro);
		AS_TYPED_CALL_VALIDATION_NAME(UnexpectedApiMacro);
		AS_TYPED_CALL_VALIDATION_NAME(MissingCppCallableSignature);
		AS_TYPED_CALL_VALIDATION_NAME(InvalidCppCallableSignature);
		AS_TYPED_CALL_VALIDATION_NAME(MissingNativeABI);
		AS_TYPED_CALL_VALIDATION_NAME(InvalidNativeABI);
		AS_TYPED_CALL_VALIDATION_NAME(NativeABIMismatch);
		AS_TYPED_CALL_VALIDATION_NAME(UnknownABIDomain);
		AS_TYPED_CALL_VALIDATION_NAME(InvalidCallSemantics);
		AS_TYPED_CALL_VALIDATION_NAME(MissingScalarABI);
		AS_TYPED_CALL_VALIDATION_NAME(InvalidScalarABI);
		AS_TYPED_CALL_VALIDATION_NAME(ScalarABIMismatch);
		AS_TYPED_CALL_VALIDATION_NAME(UnknownRouteFlags);
		AS_TYPED_CALL_VALIDATION_NAME(UnsafeRouting);
		AS_TYPED_CALL_VALIDATION_NAME(UnknownLifetimeFlags);
		AS_TYPED_CALL_VALIDATION_NAME(UnsafeLifetime);
		AS_TYPED_CALL_VALIDATION_NAME(TypedABIUnavailable);
#undef AS_TYPED_CALL_VALIDATION_NAME
		default:
			return TEXT("Unknown");
		}
	}
#endif

	FString BuildTypedCallMapping(
		const FStaticJITDiagnostics::FTypedCallDiagnosticRecord& Call)
	{
		const FString Target = Call.RegisteredTarget.IsEmpty()
			? TEXT("<unresolved>") : Call.RegisteredTarget;
		if (Call.EmittedRoute == TEXT("Bridge")
			|| Call.EmittedRoute == TEXT("CurrentNativeBinding"))
		{
			return FString::Printf(TEXT("%s -> %s -> %s -> slot %d -> %s"),
				*Call.CanonicalDeclaration,
				*Call.EmittedCppCallee,
				*Call.RuntimeDllCore,
				Call.ReferenceSlotIndex,
				*Target);
		}
		return FString::Printf(TEXT("%s -> %s -> %s -> %s"),
			*Call.CanonicalDeclaration,
			*Call.EmittedCppCallee,
			*Call.Disposition,
			*Target);
	}

	FString BuildNativeCallSiteMapping(
		const FStaticJITDiagnostics::FNativeCallSiteRecord& CallSite)
	{
		const FString EffectiveTarget =
			!CallSite.CurrentTargetDeclaration.IsEmpty()
				? CallSite.CurrentTargetDeclaration
				: (!CallSite.RegisteredTarget.IsEmpty()
					? CallSite.RegisteredTarget : TEXT("<unresolved>"));
		if (CallSite.Route == EAngelscriptJITNativeCallSiteRoute::Bridge
			|| CallSite.Route
				== EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding)
		{
			return FString::Printf(TEXT("%s -> %s -> %s -> slot[%d] -> %s"),
				*CallSite.CanonicalDeclaration,
				*CallSite.EmittedCppCallee,
				*CallSite.RuntimeDllCore,
				CallSite.ReferenceSlotIndex,
				*EffectiveTarget);
		}
		return FString::Printf(TEXT("%s -> %s -> %s -> %s"),
			*CallSite.CanonicalDeclaration,
			*CallSite.EmittedCppCallee,
			NativeCallSiteRouteName(CallSite.Route),
			*EffectiveTarget);
	}

	void SynchronizeNativeCallSiteState(
		FStaticJITDiagnostics::FProviderEntryRecord& Entry)
	{
		for (FStaticJITDiagnostics::FNativeCallSiteRecord& CallSite
			: Entry.NativeCallSites)
		{
			if (CallSite.Route != EAngelscriptJITNativeCallSiteRoute::Bridge
				&& CallSite.Route
					!= EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding)
			{
				CallSite.bResolved = true;
				CallSite.CurrentResolution =
					EAngelscriptJITReferenceResolutionCode::Exact;
				CallSite.AdoptionState =
					FStaticJITDiagnostics::EReferenceAdoptionState::Bound;
				CallSite.CurrentTargetDeclaration = CallSite.RegisteredTarget;
				switch (CallSite.Route)
				{
				case EAngelscriptJITNativeCallSiteRoute::DirectExported:
					CallSite.CurrentTargetKind = TEXT("ImportedSymbol");
					break;
				case EAngelscriptJITNativeCallSiteRoute::DirectInline:
					CallSite.CurrentTargetKind = TEXT("HeaderInline");
					break;
				case EAngelscriptJITNativeCallSiteRoute::RuntimeThunk:
					CallSite.CurrentTargetKind = TEXT("RuntimeThunk");
					break;
				default:
					CallSite.bResolved = false;
					CallSite.CurrentResolution =
						EAngelscriptJITReferenceResolutionCode::Missing;
					CallSite.AdoptionState =
						FStaticJITDiagnostics::EReferenceAdoptionState::Unavailable;
					CallSite.CurrentTargetDeclaration.Reset();
					CallSite.CurrentTargetKind.Reset();
					break;
				}
				continue;
			}

			const FStaticJITDiagnostics::FReferenceRecord* const Reference =
				Entry.References.FindByPredicate(
					[&CallSite](
						const FStaticJITDiagnostics::FReferenceRecord& Candidate)
					{
						return static_cast<int32>(Candidate.SlotIndex)
								== CallSite.ReferenceSlotIndex
							&& Candidate.Reference.StableKey
								== CallSite.StableTargetKey
							&& Candidate.Reference.ExpectedAbi
								== CallSite.ExpectedAbi;
					});
			if (Reference == nullptr)
			{
				CallSite.bResolved = false;
				CallSite.CurrentResolution =
					EAngelscriptJITReferenceResolutionCode::Missing;
				CallSite.AdoptionState =
					FStaticJITDiagnostics::EReferenceAdoptionState::Unavailable;
				CallSite.CurrentTargetDeclaration.Reset();
				CallSite.CurrentTargetKind.Reset();
				continue;
			}
			CallSite.bResolved = Reference->bResolved;
			CallSite.CurrentResolution = Reference->CurrentResolution;
			CallSite.AdoptionState = Reference->AdoptionState;
			CallSite.CurrentTargetDeclaration =
				Reference->CurrentTargetDeclaration;
			CallSite.CurrentTargetKind = Reference->CurrentTargetKind;
		}
	}

	FAngelscriptHash256 BuildModuleArtifactDigest(
		const FAngelscriptStableModuleKey& ModuleKey,
		const TArray<FStaticJITDiagnostics::FProviderEntryRecord>& Entries)
	{
		TArray<const FStaticJITDiagnostics::FProviderEntryRecord*> ModuleEntries;
		for (const FStaticJITDiagnostics::FProviderEntryRecord& Entry : Entries)
		{
			if (Entry.ModuleKey == ModuleKey)
			{
				ModuleEntries.Add(&Entry);
			}
		}
		ModuleEntries.Sort([](
			const FStaticJITDiagnostics::FProviderEntryRecord& Left,
			const FStaticJITDiagnostics::FProviderEntryRecord& Right)
		{
			return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
		});

		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("Angelscript.StaticJIT.Diagnostics.ModuleArtifact.v2"));
		Writer.WriteHash(ModuleKey.Hash);
		Writer.WriteUInt32(static_cast<uint32>(ModuleEntries.Num()));
		for (const FStaticJITDiagnostics::FProviderEntryRecord* Entry
			: ModuleEntries)
		{
			Writer.WriteHash(Entry->FunctionKey.Hash);
			Writer.WriteHash(Entry->ExecutionHash);
			Writer.WriteHash(Entry->DebugHash);
			Writer.WriteHash(Entry->ArtifactProfile.Hash);
			Writer.WriteHash(Entry->EntryAbiHash);
			Writer.WriteHash(Entry->NativeEnvironmentFingerprint);
			Writer.WriteUInt32(static_cast<uint32>(Entry->Flags));
			Writer.WriteBool(Entry->bHasVMEntry);
			Writer.WriteBool(Entry->bHasRawEntry);
			Writer.WriteBool(Entry->bHasParmsEntry);
			Writer.WriteUInt32(static_cast<uint32>(Entry->References.Num()));
			for (const FStaticJITDiagnostics::FReferenceRecord& Reference
				: Entry->References)
			{
				Writer.WriteUInt32(Reference.SlotIndex);
				Writer.WriteUInt32(static_cast<uint32>(Reference.Flags));
				Writer.WriteUInt8(static_cast<uint8>(Reference.Reference.Kind));
				Writer.WriteHash(Reference.Reference.StableKey);
				Writer.WriteHash(Reference.Reference.ExpectedAbi);
			}
			Writer.WriteUInt32(static_cast<uint32>(
				Entry->NativeCallSites.Num()));
			for (const FStaticJITDiagnostics::FNativeCallSiteRecord& CallSite
				: Entry->NativeCallSites)
			{
				Writer.WriteUInt32(CallSite.CallSiteIndex);
				Writer.WriteUInt32(static_cast<uint32>(CallSite.Route));
				Writer.WriteString(CallSite.CanonicalDeclaration);
				Writer.WriteString(CallSite.EmittedCppCallee);
				Writer.WriteString(CallSite.RuntimeDllCore);
				Writer.WriteString(CallSite.MetadataRowSymbol);
				Writer.WriteString(CallSite.RegisteredTarget);
				Writer.WriteHash(CallSite.StableTargetKey);
				Writer.WriteHash(CallSite.ExpectedAbi);
				Writer.WriteUInt32(static_cast<uint32>(
					CallSite.ReferenceSlotIndex));
			}
		}
		return Writer.FinalizeHash();
	}

	bool RouteMatchesQuery(
		const FStaticJITDiagnostics::FRouteRecord& Route,
		const FStringView Query)
	{
		const FString QueryString(Query);
		return Route.CanonicalDeclaration.Equals(
			QueryString, ESearchCase::CaseSensitive)
			|| Route.Identity.FunctionKey.Hash.ToHexString().Equals(
				QueryString, ESearchCase::IgnoreCase);
	}

	bool EntryMatchesQuery(
		const FStaticJITDiagnostics::FProviderEntryRecord& Entry,
		const FStringView Query)
	{
		const FString QueryString(Query);
		return Entry.FunctionKey.Hash.ToHexString().Equals(
			QueryString, ESearchCase::IgnoreCase);
	}

	FStaticJITDiagnostics::FSnapshot BuildOrderedSnapshot(
		const FStaticJITDiagnostics::FSnapshot& Source)
	{
		FStaticJITDiagnostics::FSnapshot Ordered = Source;
		if (Ordered.ArtifactRequest.IsSet())
		{
			Ordered.ArtifactRequest->Functions.Sort([](
				const FStaticJITDiagnostics::FArtifactFunctionRecord& Left,
				const FStaticJITDiagnostics::FArtifactFunctionRecord& Right)
			{
				return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
			});
		}
		for (FStaticJITDiagnostics::FProviderRecord& Provider
			: Ordered.Providers)
		{
			if (Provider.GenerationDiagnostics.IsSet())
			{
				Provider.GenerationDiagnostics->Functions.Sort([](
					const FAngelscriptJITProviderCatalogFunctionDiagnostic& Left,
					const FAngelscriptJITProviderCatalogFunctionDiagnostic& Right)
				{
					return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
				});
			}
			Provider.Modules.Sort([](
				const FStaticJITDiagnostics::FProviderModuleRecord& Left,
				const FStaticJITDiagnostics::FProviderModuleRecord& Right)
			{
				return Left.ModuleKey.Hash < Right.ModuleKey.Hash;
			});
			Provider.Entries.Sort([](
				const FStaticJITDiagnostics::FProviderEntryRecord& Left,
				const FStaticJITDiagnostics::FProviderEntryRecord& Right)
			{
				if (!(Left.ModuleKey == Right.ModuleKey))
				{
					return Left.ModuleKey.Hash < Right.ModuleKey.Hash;
				}
				if (!(Left.FunctionKey == Right.FunctionKey))
				{
					return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
				}
				return Left.ProviderEntryIndex < Right.ProviderEntryIndex;
			});
			for (FStaticJITDiagnostics::FProviderEntryRecord& Entry
				: Provider.Entries)
			{
				Entry.References.Sort([](
					const FStaticJITDiagnostics::FReferenceRecord& Left,
					const FStaticJITDiagnostics::FReferenceRecord& Right)
				{
					if (Left.SlotIndex != Right.SlotIndex)
					{
						return Left.SlotIndex < Right.SlotIndex;
					}
					if (Left.Reference.Kind != Right.Reference.Kind)
					{
						return static_cast<uint8>(Left.Reference.Kind)
							< static_cast<uint8>(Right.Reference.Kind);
					}
					return Left.Reference.StableKey
						< Right.Reference.StableKey;
				});
				Entry.NativeCallSites.Sort([](
					const FStaticJITDiagnostics::FNativeCallSiteRecord& Left,
					const FStaticJITDiagnostics::FNativeCallSiteRecord& Right)
				{
					return Left.CallSiteIndex < Right.CallSiteIndex;
				});
			}
		}
		Ordered.Providers.Sort([](
			const FStaticJITDiagnostics::FProviderRecord& Left,
			const FStaticJITDiagnostics::FProviderRecord& Right)
		{
			if (!(Left.ProviderId == Right.ProviderId))
			{
				return Left.ProviderId.Hash < Right.ProviderId.Hash;
			}
			return Left.ProviderGeneration.Hash
				< Right.ProviderGeneration.Hash;
		});
		for (FStaticJITDiagnostics::FRouteRecord& Route : Ordered.Routes)
		{
			Route.Candidates.Sort([](
				const FStaticJITDiagnostics::FRouteCandidateRecord& Left,
				const FStaticJITDiagnostics::FRouteCandidateRecord& Right)
			{
				if (!(Left.ProviderId == Right.ProviderId))
				{
					return Left.ProviderId.Hash < Right.ProviderId.Hash;
				}
				if (!(Left.ProviderGeneration
					== Right.ProviderGeneration))
				{
					return Left.ProviderGeneration.Hash
						< Right.ProviderGeneration.Hash;
				}
				return Left.ProviderEntryIndex
					< Right.ProviderEntryIndex;
			});
		}
		Ordered.Routes.Sort([](
			const FStaticJITDiagnostics::FRouteRecord& Left,
			const FStaticJITDiagnostics::FRouteRecord& Right)
		{
			if (!(Left.ModuleKey == Right.ModuleKey))
			{
				return Left.ModuleKey.Hash < Right.ModuleKey.Hash;
			}
			return Left.Identity.FunctionKey.Hash
				< Right.Identity.FunctionKey.Hash;
		});
		return Ordered;
	}

	FStaticJITDiagnostics::FSnapshot FilterSnapshot(
		const FStaticJITDiagnostics::FSnapshot& Source,
		const FStringView Query)
	{
		FStaticJITDiagnostics::FSnapshot Filtered = Source;
		Filtered.Query = FString(Query);
		TArray<FAngelscriptStableFunctionKey> SelectedKeys;
		for (const FStaticJITDiagnostics::FRouteRecord& Route : Source.Routes)
		{
			if (RouteMatchesQuery(Route, Query))
			{
				SelectedKeys.AddUnique(Route.Identity.FunctionKey);
			}
		}
		for (const FStaticJITDiagnostics::FProviderRecord& Provider
			: Source.Providers)
		{
			for (const FStaticJITDiagnostics::FProviderEntryRecord& Entry
				: Provider.Entries)
			{
				if (EntryMatchesQuery(Entry, Query))
				{
					SelectedKeys.AddUnique(Entry.FunctionKey);
				}
			}
		}
		Filtered.bQueryMatched = !SelectedKeys.IsEmpty();
		Filtered.Routes.RemoveAll(
			[&SelectedKeys](const FStaticJITDiagnostics::FRouteRecord& Route)
			{
				return !SelectedKeys.Contains(Route.Identity.FunctionKey);
			});
		Filtered.VmRouteCount = 0;
		Filtered.NativeRouteCount = 0;
		for (const FStaticJITDiagnostics::FRouteRecord& Route : Filtered.Routes)
		{
			if (Route.SelectedExecutionRoute
				== EAngelscriptFunctionExecutionRoute::Native)
			{
				++Filtered.NativeRouteCount;
			}
			else
			{
				++Filtered.VmRouteCount;
			}
		}

		for (FStaticJITDiagnostics::FProviderRecord& Provider
			: Filtered.Providers)
		{
			if (Provider.GenerationDiagnostics.IsSet())
			{
				Provider.GenerationDiagnostics->Functions.RemoveAll(
					[&SelectedKeys](
						const FAngelscriptJITProviderCatalogFunctionDiagnostic&
							Function)
					{
						return !SelectedKeys.Contains(Function.FunctionKey);
					});
			}
			Provider.Entries.RemoveAll(
				[&SelectedKeys](
					const FStaticJITDiagnostics::FProviderEntryRecord& Entry)
				{
					return !SelectedKeys.Contains(Entry.FunctionKey);
				});
			Provider.Modules.RemoveAll(
				[&Provider](
					const FStaticJITDiagnostics::FProviderModuleRecord& Module)
				{
					return !Provider.Entries.ContainsByPredicate(
						[&Module](
							const FStaticJITDiagnostics::FProviderEntryRecord& Entry)
						{
							return Entry.ModuleKey == Module.ModuleKey;
						});
				});
			for (FStaticJITDiagnostics::FProviderModuleRecord& Module
				: Provider.Modules)
			{
				Module.FunctionCount = 0;
				for (const FStaticJITDiagnostics::FProviderEntryRecord& Entry
					: Provider.Entries)
				{
					Module.FunctionCount += Entry.ModuleKey == Module.ModuleKey
						? 1u : 0u;
				}
				Module.ModuleArtifactDigest = BuildModuleArtifactDigest(
					Module.ModuleKey, Provider.Entries);
			}
			Provider.CompatibleEntryCount = 0;
			Provider.RejectedEntryCount = 0;
			Provider.ConflictingEntryCount = 0;
			for (const FStaticJITDiagnostics::FRouteRecord& Route
				: Filtered.Routes)
			{
				for (const FStaticJITDiagnostics::FRouteCandidateRecord& Candidate
					: Route.Candidates)
				{
					if (!(Candidate.ProviderId == Provider.ProviderId)
						|| !(Candidate.ProviderGeneration
							== Provider.ProviderGeneration))
					{
						continue;
					}
					if (Candidate.MatchResult
						== EAngelscriptArtifactMatchResult::Exact)
					{
						++Provider.CompatibleEntryCount;
						if (Route.MatchResult ==
							EAngelscriptArtifactMatchResult::
								AmbiguousExactProvider)
						{
							++Provider.ConflictingEntryCount;
						}
					}
					else
					{
						++Provider.RejectedEntryCount;
					}
				}
			}
		}
		Filtered.Providers.RemoveAll([](
			const FStaticJITDiagnostics::FProviderRecord& Provider)
		{
			return Provider.Entries.IsEmpty();
		});
		return Filtered;
	}

	void WriteReference(
		const TSharedRef<FDiagnosticJsonWriter>& Writer,
		const FStaticJITDiagnostics::FReferenceRecord& Reference)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("slotIndex"), Reference.SlotIndex);
		Writer->WriteValue(TEXT("flags"),
			static_cast<uint32>(Reference.Flags));
		Writer->WriteValue(TEXT("kind"),
			ReferenceKindName(Reference.Reference.Kind));
		Writer->WriteValue(TEXT("stableKey"),
			Reference.Reference.StableKey.ToHexString());
		Writer->WriteValue(TEXT("expectedAbi"),
			Reference.Reference.ExpectedAbi.ToHexString());
		Writer->WriteValue(TEXT("resolved"), Reference.bResolved);
		Writer->WriteValue(TEXT("currentResolution"),
			ReferenceResolutionName(Reference.CurrentResolution));
		Writer->WriteValue(TEXT("adoptionState"),
			ReferenceAdoptionStateName(Reference.AdoptionState));
		Writer->WriteValue(TEXT("currentTargetDeclaration"),
			Reference.CurrentTargetDeclaration);
		Writer->WriteValue(TEXT("currentTargetKind"),
			Reference.CurrentTargetKind);
		Writer->WriteObjectEnd();
	}

	void WriteNativeCallSite(
		const TSharedRef<FDiagnosticJsonWriter>& Writer,
		const FStaticJITDiagnostics::FNativeCallSiteRecord& CallSite)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("callSiteIndex"), CallSite.CallSiteIndex);
		Writer->WriteValue(TEXT("route"),
			NativeCallSiteRouteName(CallSite.Route));
		Writer->WriteValue(TEXT("canonicalDeclaration"),
			CallSite.CanonicalDeclaration);
		Writer->WriteValue(TEXT("emittedCppCallee"),
			CallSite.EmittedCppCallee);
		Writer->WriteValue(TEXT("runtimeDllCore"), CallSite.RuntimeDllCore);
		Writer->WriteValue(TEXT("metadataRowSymbol"),
			CallSite.MetadataRowSymbol);
		Writer->WriteValue(TEXT("registeredTarget"),
			CallSite.RegisteredTarget);
		Writer->WriteValue(TEXT("stableTargetKey"),
			CallSite.StableTargetKey.ToHexString());
		Writer->WriteValue(TEXT("expectedAbi"),
			CallSite.ExpectedAbi.ToHexString());
		Writer->WriteValue(TEXT("referenceSlotIndex"),
			CallSite.ReferenceSlotIndex);
		Writer->WriteValue(TEXT("resolved"), CallSite.bResolved);
		Writer->WriteValue(TEXT("currentResolution"),
			ReferenceResolutionName(CallSite.CurrentResolution));
		Writer->WriteValue(TEXT("adoptionState"),
			ReferenceAdoptionStateName(CallSite.AdoptionState));
		Writer->WriteValue(TEXT("currentTargetDeclaration"),
			CallSite.CurrentTargetDeclaration);
		Writer->WriteValue(TEXT("currentTargetKind"),
			CallSite.CurrentTargetKind);
		Writer->WriteValue(TEXT("mapping"),
			BuildNativeCallSiteMapping(CallSite));
		Writer->WriteObjectEnd();
	}

	void WriteProcessedSourceRecord(
		const TSharedRef<FDiagnosticJsonWriter>& Writer,
		const TCHAR* FieldName,
		const FStaticJITDiagnostics::FProcessedSourceRecord& Source)
	{
		Writer->WriteObjectStart(FieldName);
		Writer->WriteValue(TEXT("section"), Source.Section);
		Writer->WriteValue(TEXT("row"), Source.Row);
		Writer->WriteValue(TEXT("column"), Source.Column);
		Writer->WriteObjectEnd();
	}

	void WriteCanonicalFunctionDiagnostic(
		const TSharedRef<FDiagnosticJsonWriter>& Writer,
		const FStaticJITDiagnostics::FCanonicalFunctionDiagnosticRecord&
			Diagnostic,
		const TCHAR* FieldName = nullptr)
	{
		if (FieldName != nullptr)
		{
			Writer->WriteObjectStart(FieldName);
		}
		else
		{
			Writer->WriteObjectStart();
		}
		Writer->WriteValue(TEXT("functionKey"),
			Diagnostic.FunctionKey.Hash.ToHexString());
		Writer->WriteValue(TEXT("role"), Diagnostic.Role);
		Writer->WriteValue(TEXT("rawTraitBits"), FString::Printf(
			TEXT("0x%08x"), Diagnostic.RawTraitBits));
		Writer->WriteValue(TEXT("unknownTraitBits"), FString::Printf(
			TEXT("0x%08x"), Diagnostic.UnknownTraitBits));
		Writer->WriteValue(TEXT("invocationKind"),
			Diagnostic.InvocationKind);
		Writer->WriteValue(TEXT("receiverKind"), Diagnostic.ReceiverKind);
		Writer->WriteValue(TEXT("receiverParameterIndex"),
			Diagnostic.ReceiverParameterIndex);
		Writer->WriteValue(TEXT("canonicalASTState"),
			Diagnostic.CanonicalASTState);
		Writer->WriteValue(TEXT("cleanupPlanState"),
			Diagnostic.CleanupPlanState);
		Writer->WriteValue(TEXT("lifetimeProtocolRevision"),
			Diagnostic.LifetimeProtocolRevision);
		Writer->WriteValue(TEXT("lifetimeStableKey"),
			Diagnostic.LifetimeStableKey.ToHexString());
		Writer->WriteValue(TEXT("lifetimeABIKey"),
			Diagnostic.LifetimeABIKey.ToHexString());
		Writer->WriteValue(TEXT("lifetimeActionFlags"), FString::Printf(
			TEXT("0x%08x"), Diagnostic.LifetimeActionFlags));
		Writer->WriteValue(TEXT("lifetimeExitPlanFlags"), FString::Printf(
			TEXT("0x%08x"), Diagnostic.LifetimeExitPlanFlags));
		Writer->WriteValue(TEXT("lifetimeSupportedExitMask"), FString::Printf(
			TEXT("0x%08x"), Diagnostic.LifetimeSupportedExitMask));
		Writer->WriteValue(TEXT("lifetimeRecordCount"),
			Diagnostic.LifetimeRecordCount);
		Writer->WriteValue(TEXT("lifetimeExitPlanCount"),
			Diagnostic.LifetimeExitPlanCount);
		Writer->WriteValue(TEXT("nativeObjectFrameABIAvailable"),
			Diagnostic.bNativeObjectFrameABIAvailable);
		Writer->WriteValue(TEXT("hasSuspendState"),
			Diagnostic.bHasSuspendState);
		Writer->WriteValue(TEXT("hasExceptionCleanup"),
			Diagnostic.bHasExceptionCleanup);
		Writer->WriteValue(TEXT("cleanupPlanCoversAllTransfers"),
			Diagnostic.bCleanupPlanCoversAllTransfers);
		Writer->WriteValue(TEXT("componentIndex"),
			Diagnostic.ComponentIndex);
		Writer->WriteValue(TEXT("recursiveComponent"),
			Diagnostic.bRecursiveComponent);
		Writer->WriteValue(TEXT("eligibilityReason"),
			Diagnostic.EligibilityReason);
		Writer->WriteValue(TEXT("eligibilityDetail"),
			Diagnostic.EligibilityDetail);
		WriteProcessedSourceRecord(Writer, TEXT("eligibilitySource"),
			Diagnostic.EligibilitySource);
		Writer->WriteObjectEnd();
	}

	void WriteTypedCallDiagnostic(
		const TSharedRef<FDiagnosticJsonWriter>& Writer,
		const FStaticJITDiagnostics::FTypedCallDiagnosticRecord& Diagnostic)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("callerFunctionKey"),
			Diagnostic.CallerFunctionKey.Hash.ToHexString());
		Writer->WriteValue(TEXT("calleeFunctionKey"),
			Diagnostic.CalleeFunctionKey.Hash.ToHexString());
		Writer->WriteValue(TEXT("expressionIndex"),
			Diagnostic.ExpressionIndex);
		WriteProcessedSourceRecord(Writer, TEXT("processedSource"),
			Diagnostic.ProcessedSource);
		if (Diagnostic.AuthoredSource.IsSet())
		{
			WriteProcessedSourceRecord(Writer, TEXT("authoredSource"),
				Diagnostic.AuthoredSource.GetValue());
		}
		if (Diagnostic.GeneratedSource.IsSet())
		{
			WriteProcessedSourceRecord(Writer, TEXT("generatedSource"),
				Diagnostic.GeneratedSource.GetValue());
		}
		Writer->WriteValue(TEXT("targetKind"), Diagnostic.TargetKind);
		Writer->WriteValue(TEXT("disposition"), Diagnostic.Disposition);
		Writer->WriteValue(TEXT("calleeEligibilityReason"),
			Diagnostic.CalleeEligibilityReason);
		Writer->WriteValue(TEXT("calleeEligibilityDetail"),
			Diagnostic.CalleeEligibilityDetail);
		WriteProcessedSourceRecord(Writer, TEXT("calleeEligibilitySource"),
			Diagnostic.CalleeEligibilitySource);
		Writer->WriteValue(TEXT("nativeLinkageValidation"),
			Diagnostic.NativeLinkageValidation);
		Writer->WriteValue(TEXT("nativeLinkageValidationDetail"),
			Diagnostic.NativeLinkageValidationDetail);
		Writer->WriteValue(TEXT("canonicalDeclaration"),
			Diagnostic.CanonicalDeclaration);
		Writer->WriteValue(TEXT("emittedRoute"), Diagnostic.EmittedRoute);
		Writer->WriteValue(TEXT("emittedCppCallee"),
			Diagnostic.EmittedCppCallee);
		Writer->WriteValue(TEXT("runtimeDllCore"),
			Diagnostic.RuntimeDllCore);
		Writer->WriteValue(TEXT("metadataRowSymbol"),
			Diagnostic.MetadataRowSymbol);
		Writer->WriteValue(TEXT("registeredTarget"),
			Diagnostic.RegisteredTarget);
		Writer->WriteValue(TEXT("directCppSymbol"),
			Diagnostic.DirectCppSymbol);
		Writer->WriteValue(TEXT("stableTargetKey"),
			Diagnostic.StableTargetKey.ToHexString());
		Writer->WriteValue(TEXT("expectedAbi"),
			Diagnostic.ExpectedAbi.ToHexString());
		Writer->WriteValue(TEXT("referenceSlotIndex"),
			Diagnostic.ReferenceSlotIndex);
		Writer->WriteValue(TEXT("mapping"),
			BuildTypedCallMapping(Diagnostic));
		Writer->WriteObjectEnd();
	}

	void WriteTypedFunctionDiagnostic(
		const TSharedRef<FDiagnosticJsonWriter>& Writer,
		const FStaticJITDiagnostics::FTypedFunctionDiagnosticRecord& Diagnostic)
	{
		Writer->WriteObjectStart(TEXT("typedDiagnostic"));
		Writer->WriteValue(TEXT("fallbackReason"),
			Diagnostic.FallbackReason);
		Writer->WriteValue(TEXT("detail"), Diagnostic.Detail);
		WriteProcessedSourceRecord(Writer, TEXT("processedSource"),
			Diagnostic.ProcessedSource);
		if (Diagnostic.SemanticFunction.IsSet())
		{
			WriteCanonicalFunctionDiagnostic(Writer,
				Diagnostic.SemanticFunction.GetValue(),
				TEXT("semanticFunction"));
		}
		if (Diagnostic.CallClosure.IsSet())
		{
			const FStaticJITDiagnostics::FTypedCallClosureDiagnosticRecord&
				Closure = Diagnostic.CallClosure.GetValue();
			Writer->WriteObjectStart(TEXT("callClosure"));
			Writer->WriteValue(TEXT("eligible"), Closure.bEligible);
			Writer->WriteValue(TEXT("requiredCapabilities"),
				Closure.RequiredCapabilities);
			Writer->WriteValue(TEXT("availableCapabilities"),
				Closure.AvailableCapabilities);
			Writer->WriteValue(TEXT("executionRequirementsIdentity"),
				Closure.ExecutionRequirementsIdentity);
			Writer->WriteValue(TEXT("availableCapabilityProfileIdentity"),
				Closure.AvailableCapabilityProfileIdentity);
			Writer->WriteValue(TEXT("profileKind"), Closure.ProfileKind);
			Writer->WriteValue(TEXT("exceptionControlState"),
				Closure.ExceptionControlState);
			Writer->WriteValue(TEXT("exceptionPayloadState"),
				Closure.ExceptionPayloadState);
			Writer->WriteValue(TEXT("recursionGuardAvailable"),
				Closure.bRecursionGuardAvailable);
			Writer->WriteArrayStart(TEXT("functions"));
			for (const FStaticJITDiagnostics::
				FCanonicalFunctionDiagnosticRecord& Function
				: Closure.Functions)
			{
				WriteCanonicalFunctionDiagnostic(Writer, Function);
			}
			Writer->WriteArrayEnd();
			Writer->WriteArrayStart(TEXT("calls"));
			for (const FStaticJITDiagnostics::FTypedCallDiagnosticRecord& Call
				: Closure.Calls)
			{
				WriteTypedCallDiagnostic(Writer, Call);
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteObjectEnd();
	}

	void WriteProviderDiagnosticSource(
		const TSharedRef<FDiagnosticJsonWriter>& Writer,
		const TCHAR* FieldName,
		const FAngelscriptJITProviderCatalogDiagnosticSource& Source)
	{
		Writer->WriteObjectStart(FieldName);
		Writer->WriteValue(TEXT("section"), Source.Section);
		Writer->WriteValue(TEXT("row"), Source.Row);
		Writer->WriteValue(TEXT("column"), Source.Column);
		Writer->WriteObjectEnd();
	}

	void WriteProviderSemanticDiagnostic(
		const TSharedRef<FDiagnosticJsonWriter>& Writer,
		const FAngelscriptJITProviderCatalogCanonicalFunctionDiagnostic&
			Diagnostic,
		const TCHAR* FieldName = nullptr)
	{
		if (FieldName != nullptr)
		{
			Writer->WriteObjectStart(FieldName);
		}
		else
		{
			Writer->WriteObjectStart();
		}
		Writer->WriteValue(TEXT("functionKey"),
			Diagnostic.FunctionKey.Hash.ToHexString());
		Writer->WriteValue(TEXT("role"), Diagnostic.Role);
		Writer->WriteValue(TEXT("rawTraitBits"), Diagnostic.RawTraitBits);
		Writer->WriteValue(TEXT("unknownTraitBits"), Diagnostic.UnknownTraitBits);
		Writer->WriteValue(TEXT("invocationKind"), Diagnostic.InvocationKind);
		Writer->WriteValue(TEXT("receiverKind"), Diagnostic.ReceiverKind);
		Writer->WriteValue(TEXT("receiverParameterIndex"),
			Diagnostic.ReceiverParameterIndex);
		Writer->WriteValue(TEXT("canonicalASTState"),
			Diagnostic.CanonicalASTState);
		Writer->WriteValue(TEXT("cleanupPlanState"),
			Diagnostic.CleanupPlanState);
		Writer->WriteValue(TEXT("lifetimeProtocolRevision"),
			Diagnostic.LifetimeProtocolRevision);
		Writer->WriteValue(TEXT("lifetimeStableKey"),
			Diagnostic.LifetimeStableKey.ToHexString());
		Writer->WriteValue(TEXT("lifetimeABIKey"),
			Diagnostic.LifetimeABIKey.ToHexString());
		Writer->WriteValue(TEXT("lifetimeActionFlags"),
			Diagnostic.LifetimeActionFlags);
		Writer->WriteValue(TEXT("lifetimeExitPlanFlags"),
			Diagnostic.LifetimeExitPlanFlags);
		Writer->WriteValue(TEXT("lifetimeSupportedExitMask"),
			Diagnostic.LifetimeSupportedExitMask);
		Writer->WriteValue(TEXT("lifetimeRecordCount"),
			Diagnostic.LifetimeRecordCount);
		Writer->WriteValue(TEXT("lifetimeExitPlanCount"),
			Diagnostic.LifetimeExitPlanCount);
		Writer->WriteValue(TEXT("nativeObjectFrameABIAvailable"),
			Diagnostic.bNativeObjectFrameABIAvailable);
		Writer->WriteValue(TEXT("hasSuspendState"),
			Diagnostic.bHasSuspendState);
		Writer->WriteValue(TEXT("hasExceptionCleanup"),
			Diagnostic.bHasExceptionCleanup);
		Writer->WriteValue(TEXT("cleanupPlanCoversAllTransfers"),
			Diagnostic.bCleanupPlanCoversAllTransfers);
		Writer->WriteValue(TEXT("componentIndex"), Diagnostic.ComponentIndex);
		Writer->WriteValue(TEXT("recursiveComponent"),
			Diagnostic.bRecursiveComponent);
		Writer->WriteValue(TEXT("eligibilityReason"),
			Diagnostic.EligibilityReason);
		Writer->WriteValue(TEXT("eligibilityDetail"),
			Diagnostic.EligibilityDetail);
		WriteProviderDiagnosticSource(Writer, TEXT("eligibilitySource"),
			Diagnostic.EligibilitySource);
		Writer->WriteObjectEnd();
	}

	FString BuildProviderCallMapping(
		const FAngelscriptJITProviderCatalogTypedCallDiagnostic& Call)
	{
		const FString Target = !Call.RegisteredTarget.IsEmpty()
			? Call.RegisteredTarget
			: (!Call.DirectCppSymbol.IsEmpty()
				? Call.DirectCppSymbol : TEXT("<unresolved>"));
		if (Call.EmittedRoute == EAngelscriptJITNativeCallSiteRoute::Bridge
			|| Call.EmittedRoute
				== EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding)
		{
			return FString::Printf(TEXT("%s -> %s -> %s -> slot[%d] -> %s"),
				*Call.CanonicalDeclaration,
				*Call.EmittedCppCallee,
				*Call.RuntimeDllCore,
				Call.ReferenceSlotIndex,
				*Target);
		}
		return FString::Printf(TEXT("%s -> %s -> %s -> %s"),
			*Call.CanonicalDeclaration,
			*Call.EmittedCppCallee,
			NativeCallSiteRouteName(Call.EmittedRoute),
			*Target);
	}

	void WriteProviderCallDiagnostic(
		const TSharedRef<FDiagnosticJsonWriter>& Writer,
		const FAngelscriptJITProviderCatalogTypedCallDiagnostic& Diagnostic)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("callerFunctionKey"),
			Diagnostic.CallerFunctionKey.Hash.ToHexString());
		Writer->WriteValue(TEXT("calleeFunctionKey"),
			Diagnostic.CalleeFunctionKey.Hash.ToHexString());
		Writer->WriteValue(TEXT("expressionIndex"),
			Diagnostic.ExpressionIndex);
		WriteProviderDiagnosticSource(Writer, TEXT("processedSource"),
			Diagnostic.ProcessedSource);
		if (EnumHasAnyFlags(Diagnostic.SourceFlags,
			EAngelscriptJITProviderDiagnosticSourceFlags::HasAuthoredSource))
		{
			WriteProviderDiagnosticSource(Writer, TEXT("authoredSource"),
				Diagnostic.AuthoredSource);
		}
		if (EnumHasAnyFlags(Diagnostic.SourceFlags,
			EAngelscriptJITProviderDiagnosticSourceFlags::HasGeneratedSource))
		{
			WriteProviderDiagnosticSource(Writer, TEXT("generatedSource"),
				Diagnostic.GeneratedSource);
		}
		Writer->WriteValue(TEXT("targetKind"), Diagnostic.TargetKind);
		Writer->WriteValue(TEXT("disposition"), Diagnostic.Disposition);
		Writer->WriteValue(TEXT("calleeEligibilityReason"),
			Diagnostic.CalleeEligibilityReason);
		Writer->WriteValue(TEXT("calleeEligibilityDetail"),
			Diagnostic.CalleeEligibilityDetail);
		WriteProviderDiagnosticSource(Writer, TEXT("calleeEligibilitySource"),
			Diagnostic.CalleeEligibilitySource);
		Writer->WriteValue(TEXT("hasNativeLinkageValidation"),
			Diagnostic.bHasNativeLinkageValidation);
		Writer->WriteValue(TEXT("nativeLinkageValidation"),
			Diagnostic.NativeLinkageValidation);
		Writer->WriteValue(TEXT("nativeLinkageValidationDetail"),
			Diagnostic.NativeLinkageValidationDetail);
		Writer->WriteValue(TEXT("canonicalDeclaration"),
			Diagnostic.CanonicalDeclaration);
		Writer->WriteValue(TEXT("emittedRoute"),
			NativeCallSiteRouteName(Diagnostic.EmittedRoute));
		Writer->WriteValue(TEXT("emittedCppCallee"),
			Diagnostic.EmittedCppCallee);
		Writer->WriteValue(TEXT("runtimeDllCore"), Diagnostic.RuntimeDllCore);
		Writer->WriteValue(TEXT("metadataRowSymbol"),
			Diagnostic.MetadataRowSymbol);
		Writer->WriteValue(TEXT("registeredTarget"),
			Diagnostic.RegisteredTarget);
		Writer->WriteValue(TEXT("directCppSymbol"),
			Diagnostic.DirectCppSymbol);
		Writer->WriteValue(TEXT("stableTargetKey"),
			Diagnostic.StableTargetKey.ToHexString());
		Writer->WriteValue(TEXT("expectedAbi"),
			Diagnostic.ExpectedAbi.ToHexString());
		Writer->WriteValue(TEXT("referenceSlotIndex"),
			Diagnostic.ReferenceSlotIndex);
		Writer->WriteValue(TEXT("mapping"),
			BuildProviderCallMapping(Diagnostic));
		Writer->WriteObjectEnd();
	}

	void WriteProviderGenerationDiagnostics(
		const TSharedRef<FDiagnosticJsonWriter>& Writer,
		const FAngelscriptJITProviderCatalogDiagnostics& Diagnostics)
	{
		Writer->WriteObjectStart(TEXT("generationDiagnostics"));
		Writer->WriteValue(TEXT("schemaRevision"), Diagnostics.SchemaRevision);
		Writer->WriteValue(TEXT("diagnosticDigest"),
			Diagnostics.DiagnosticDigest.ToHexString());
		Writer->WriteValue(TEXT("requestKind"), Diagnostics.RequestKind);
		Writer->WriteValue(TEXT("requestedBackend"),
			Diagnostics.RequestedBackendId);
		Writer->WriteValue(TEXT("captureState"), Diagnostics.CaptureState);
		Writer->WriteArrayStart(TEXT("fallbackChain"));
		for (const FString& BackendId : Diagnostics.FallbackChain)
		{
			Writer->WriteValue(BackendId);
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectStart(TEXT("compiledSourceGraph"));
		Writer->WriteValue(TEXT("sourceCompilePassCount"),
			Diagnostics.SourceCompilePassCount);
		Writer->WriteValue(TEXT("moduleCount"),
			Diagnostics.CompiledModuleCount);
		Writer->WriteValue(TEXT("functionCount"),
			Diagnostics.CompiledFunctionCount);
		Writer->WriteValue(TEXT("typeCount"), Diagnostics.CompiledTypeCount);
		Writer->WriteValue(TEXT("globalCount"),
			Diagnostics.CompiledGlobalCount);
		Writer->WriteValue(TEXT("descriptorCount"),
			Diagnostics.CompiledDescriptorCount);
		Writer->WriteValue(TEXT("dependencyCount"),
			Diagnostics.CompiledDependencyCount);
		Writer->WriteValue(TEXT("externalNativeCallCount"),
			Diagnostics.CompiledExternalNativeCallCount);
		Writer->WriteValue(TEXT("emitModuleCount"),
			Diagnostics.EmitModuleCount);
		Writer->WriteValue(TEXT("hasVerifiedCanonicalAST"),
			Diagnostics.bHasVerifiedCanonicalAST);
		Writer->WriteObjectEnd();
		Writer->WriteValue(TEXT("taskError"), Diagnostics.TaskError);
		Writer->WriteArrayStart(TEXT("functions"));
		for (const FAngelscriptJITProviderCatalogFunctionDiagnostic& Function
			: Diagnostics.Functions)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("functionKey"),
				Function.FunctionKey.Hash.ToHexString());
			Writer->WriteValue(TEXT("actualBackend"),
				Function.ActualBackendId);
			Writer->WriteValue(TEXT("disposition"), Function.Disposition);
			Writer->WriteValue(TEXT("detail"), Function.Detail);
			Writer->WriteArrayStart(TEXT("backendAttempts"));
			for (uint32 Offset = 0; Offset < Function.BackendAttemptCount;
				++Offset)
			{
				const int32 AttemptIndex = static_cast<int32>(
					Function.BackendAttemptStart + Offset);
				if (!Diagnostics.BackendAttempts.IsValidIndex(AttemptIndex))
				{
					break;
				}
				const FAngelscriptJITProviderCatalogBackendAttemptDiagnostic&
					Attempt = Diagnostics.BackendAttempts[AttemptIndex];
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("backend"), Attempt.BackendId);
				Writer->WriteValue(TEXT("disposition"), Attempt.Disposition);
				Writer->WriteValue(TEXT("detail"), Attempt.Detail);
				if (EnumHasAnyFlags(Attempt.Flags,
					EAngelscriptJITProviderDiagnosticAttemptFlags::
						HasTypedDiagnostic))
				{
					Writer->WriteObjectStart(TEXT("typedDiagnostic"));
					Writer->WriteValue(TEXT("fallbackReason"),
						Attempt.FallbackReason);
					Writer->WriteValue(TEXT("detail"), Attempt.TypedDetail);
					WriteProviderDiagnosticSource(Writer,
						TEXT("processedSource"), Attempt.ProcessedSource);
					if (EnumHasAnyFlags(Attempt.Flags,
						EAngelscriptJITProviderDiagnosticAttemptFlags::
							HasSemanticFunction)
						&& Diagnostics.SemanticFunctions.IsValidIndex(
							Attempt.PrimarySemanticFunctionIndex))
					{
						WriteProviderSemanticDiagnostic(Writer,
							Diagnostics.SemanticFunctions[
								Attempt.PrimarySemanticFunctionIndex],
							TEXT("semanticFunction"));
					}
					if (EnumHasAnyFlags(Attempt.Flags,
						EAngelscriptJITProviderDiagnosticAttemptFlags::
							HasCallClosure))
					{
						Writer->WriteObjectStart(TEXT("callClosure"));
						Writer->WriteValue(TEXT("eligible"),
							Attempt.Disposition == TEXT("Emitted"));
						Writer->WriteValue(TEXT("requiredCapabilities"),
							ExecutionCapabilityNames(
								Attempt.RequiredCapabilities));
						Writer->WriteValue(TEXT("availableCapabilities"),
							ExecutionCapabilityNames(
								Attempt.AvailableCapabilities));
						Writer->WriteValue(
							TEXT("executionRequirementsIdentity"),
							Attempt.ExecutionRequirementsIdentity.ToHexString());
						Writer->WriteValue(
							TEXT("availableCapabilityProfileIdentity"),
							Attempt.AvailableCapabilityProfileIdentity
								.ToHexString());
						Writer->WriteValue(TEXT("profileKind"),
							Attempt.ProfileKind);
						Writer->WriteValue(TEXT("exceptionControlState"),
							Attempt.ExceptionControlState);
						Writer->WriteValue(TEXT("exceptionPayloadState"),
							Attempt.ExceptionPayloadState);
						Writer->WriteValue(TEXT("recursionGuardAvailable"),
							Attempt.bRecursionGuardAvailable);
						Writer->WriteArrayStart(TEXT("functions"));
						for (uint32 SemanticOffset = 0;
							SemanticOffset < Attempt.SemanticFunctionCount;
							++SemanticOffset)
						{
							const int32 SemanticIndex = static_cast<int32>(
								Attempt.SemanticFunctionStart + SemanticOffset);
							if (!Diagnostics.SemanticFunctions.IsValidIndex(
								SemanticIndex))
							{
								break;
							}
							WriteProviderSemanticDiagnostic(Writer,
								Diagnostics.SemanticFunctions[SemanticIndex]);
						}
						Writer->WriteArrayEnd();
						Writer->WriteArrayStart(TEXT("calls"));
						for (uint32 CallOffset = 0;
							CallOffset < Attempt.CallCount; ++CallOffset)
						{
							const int32 CallIndex = static_cast<int32>(
								Attempt.CallStart + CallOffset);
							if (!Diagnostics.Calls.IsValidIndex(CallIndex))
							{
								break;
							}
							WriteProviderCallDiagnostic(Writer,
								Diagnostics.Calls[CallIndex]);
						}
						Writer->WriteArrayEnd();
						Writer->WriteObjectEnd();
					}
					Writer->WriteObjectEnd();
				}
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
	}

	FString SerializeSnapshot(
		const FStaticJITDiagnostics::FSnapshot& Snapshot)
	{
		FString Json;
		const TSharedRef<FDiagnosticJsonWriter> Writer =
			TJsonWriterFactory<TCHAR,
				TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("schemaVersion"), Snapshot.SchemaVersion);
		Writer->WriteValue(TEXT("providerAbiRevision"),
			Snapshot.ProviderAbiRevision);
		Writer->WriteValue(TEXT("hasCurrentEngine"),
			Snapshot.bHasCurrentEngine);
		Writer->WriteValue(TEXT("hasScriptEngine"),
			Snapshot.bHasScriptEngine);
		Writer->WriteValue(TEXT("query"), Snapshot.Query);
		Writer->WriteValue(TEXT("queryMatched"), Snapshot.bQueryMatched);
		Writer->WriteValue(TEXT("targetProfile"),
			FAngelscriptJITTargetProfiles::ToString(Snapshot.TargetProfile));
		Writer->WriteValue(TEXT("providerPublicationOrdinal"),
			FString::Printf(TEXT("%llu"),
				Snapshot.ProviderPublicationOrdinal));
		Writer->WriteValue(TEXT("routeGeneration"),
			FString::Printf(TEXT("%llu"), Snapshot.RouteGeneration));
		Writer->WriteValue(TEXT("vmRouteCount"), Snapshot.VmRouteCount);
		Writer->WriteValue(TEXT("nativeRouteCount"), Snapshot.NativeRouteCount);
		if (Snapshot.PrePublicationFailure.IsSet())
		{
			const FStaticJITDiagnostics::FPrePublicationFailureRecord& Failure =
				Snapshot.PrePublicationFailure.GetValue();
			Writer->WriteObjectStart(TEXT("prePublicationFailure"));
			Writer->WriteValue(TEXT("category"),
				PrePublicationFailureCategoryName(Failure.Category));
			Writer->WriteValue(TEXT("affectedBoundary"),
				PrePublicationBoundaryName(Failure.AffectedBoundary));
			Writer->WriteValue(TEXT("detail"), Failure.Detail);
			Writer->WriteValue(TEXT("generationEngineCreated"),
				Failure.bGenerationEngineCreated);
			Writer->WriteValue(TEXT("providerPackagingStarted"),
				Failure.bProviderPackagingStarted);
			Writer->WriteValue(TEXT("providerPublished"),
				Failure.bProviderPublished);
			Writer->WriteObjectEnd();
		}
		if (Snapshot.ArtifactRequest.IsSet())
		{
			const FStaticJITDiagnostics::FArtifactRequestRecord& Request =
				Snapshot.ArtifactRequest.GetValue();
			Writer->WriteObjectStart(TEXT("artifactRequest"));
			Writer->WriteValue(TEXT("requestKind"),
				ArtifactRequestKindName(Request.RequestKind));
			Writer->WriteValue(TEXT("requestedBackend"),
				Request.RequestedBackendId);
			Writer->WriteValue(TEXT("captureState"),
				ArtifactCaptureStateName(Request.CaptureState));
			Writer->WriteValue(TEXT("canonicalASTCaptureEnabled"),
				Request.bCanonicalASTCaptureEnabled);
			Writer->WriteArrayStart(TEXT("fallbackChain"));
			for (const FString& BackendId : Request.FallbackChain)
			{
				Writer->WriteValue(BackendId);
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectStart(TEXT("compiledSourceGraph"));
			Writer->WriteValue(TEXT("sourceCompilePassCount"),
				Request.CompiledSourceGraph.SourceCompilePassCount);
			Writer->WriteValue(TEXT("moduleCount"),
				Request.CompiledSourceGraph.ModuleCount);
			Writer->WriteValue(TEXT("functionCount"),
				Request.CompiledSourceGraph.FunctionCount);
			Writer->WriteValue(TEXT("typeCount"),
				Request.CompiledSourceGraph.TypeCount);
			Writer->WriteValue(TEXT("globalCount"),
				Request.CompiledSourceGraph.GlobalCount);
			Writer->WriteValue(TEXT("descriptorCount"),
				Request.CompiledSourceGraph.DescriptorCount);
			Writer->WriteValue(TEXT("dependencyCount"),
				Request.CompiledSourceGraph.DependencyCount);
			Writer->WriteValue(TEXT("externalNativeCallCount"),
				Request.CompiledSourceGraph.ExternalNativeCallCount);
			Writer->WriteValue(TEXT("emitModuleCount"),
				Request.CompiledSourceGraph.EmitModuleCount);
			Writer->WriteValue(TEXT("hasVerifiedCanonicalAST"),
				Request.CompiledSourceGraph.bHasVerifiedCanonicalAST);
			Writer->WriteObjectEnd();
			Writer->WriteValue(TEXT("taskError"), Request.TaskError);
			Writer->WriteArrayStart(TEXT("functions"));
			for (const FStaticJITDiagnostics::FArtifactFunctionRecord& Function
				: Request.Functions)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("functionKey"),
					Function.FunctionKey.Hash.ToHexString());
				Writer->WriteValue(TEXT("actualBackend"),
					Function.ActualBackendId);
				Writer->WriteValue(TEXT("disposition"),
					ArtifactFunctionDispositionName(Function.Disposition));
				Writer->WriteValue(TEXT("detail"), Function.Detail);
				Writer->WriteValue(TEXT("hasVMEntry"),
					Function.bHasVMEntry);
				Writer->WriteValue(TEXT("hasRawEntry"),
					Function.bHasRawEntry);
				Writer->WriteValue(TEXT("hasParmsEntry"),
					Function.bHasParmsEntry);
				if (Function.TypedDiagnostic.IsSet())
				{
					WriteTypedFunctionDiagnostic(Writer,
						Function.TypedDiagnostic.GetValue());
				}
				Writer->WriteArrayStart(TEXT("backendAttempts"));
				for (const FStaticJITDiagnostics::FArtifactBackendAttemptRecord& Attempt
					: Function.BackendAttempts)
				{
					Writer->WriteObjectStart();
					Writer->WriteValue(TEXT("backend"), Attempt.BackendId);
					Writer->WriteValue(TEXT("disposition"),
						ArtifactFunctionDispositionName(Attempt.Disposition));
					Writer->WriteValue(TEXT("detail"), Attempt.Detail);
					if (Attempt.TypedDiagnostic.IsSet())
					{
						WriteTypedFunctionDiagnostic(Writer,
							Attempt.TypedDiagnostic.GetValue());
					}
					Writer->WriteObjectEnd();
				}
				Writer->WriteArrayEnd();
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteObjectStart(TEXT("matchResultCounts"));
		for (uint8 MatchIndex = 0;
			MatchIndex <= static_cast<uint8>(
				EAngelscriptArtifactMatchResult::MissingEntryPoint);
			++MatchIndex)
		{
			uint32 Count = 0;
			for (const FStaticJITDiagnostics::FRouteRecord& Route
				: Snapshot.Routes)
			{
				Count += static_cast<uint8>(Route.MatchResult) == MatchIndex
					? 1u : 0u;
			}
			Writer->WriteValue(MatchName(
				static_cast<EAngelscriptArtifactMatchResult>(MatchIndex)), Count);
		}
		Writer->WriteObjectEnd();

		Writer->WriteArrayStart(TEXT("providers"));
		for (const FStaticJITDiagnostics::FProviderRecord& Provider
			: Snapshot.Providers)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("providerId"),
				Provider.ProviderId.Hash.ToHexString());
			Writer->WriteValue(TEXT("providerGeneration"),
				Provider.ProviderGeneration.Hash.ToHexString());
			Writer->WriteValue(TEXT("artifactSetDigest"),
				Provider.ArtifactSetDigest.ToHexString());
			Writer->WriteValue(TEXT("artifactProfile"),
				Provider.ArtifactProfile.Hash.ToHexString());
			Writer->WriteValue(TEXT("nativeEnvironment"),
				Provider.NativeEnvironmentFingerprint.ToHexString());
			Writer->WriteValue(TEXT("providerName"), Provider.ProviderName);
			Writer->WriteValue(TEXT("ownerModuleName"), Provider.OwnerModuleName);
			Writer->WriteValue(TEXT("compatibleEntryCount"),
				Provider.CompatibleEntryCount);
			Writer->WriteValue(TEXT("rejectedEntryCount"),
				Provider.RejectedEntryCount);
			Writer->WriteValue(TEXT("conflictingEntryCount"),
				Provider.ConflictingEntryCount);
			if (Provider.GenerationDiagnostics.IsSet())
			{
				WriteProviderGenerationDiagnostics(Writer,
					Provider.GenerationDiagnostics.GetValue());
			}
			Writer->WriteArrayStart(TEXT("modules"));
			for (const FStaticJITDiagnostics::FProviderModuleRecord& Module
				: Provider.Modules)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("moduleKey"),
					Module.ModuleKey.Hash.ToHexString());
				Writer->WriteValue(TEXT("generatedModuleSource"),
					Module.GeneratedModuleSource);
				Writer->WriteValue(TEXT("moduleArtifactDigest"),
					Module.ModuleArtifactDigest.ToHexString());
				Writer->WriteValue(TEXT("functionCount"),
					Module.FunctionCount);
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();
			Writer->WriteArrayStart(TEXT("entries"));
			for (const FStaticJITDiagnostics::FProviderEntryRecord& Entry
				: Provider.Entries)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("providerEntryIndex"),
					Entry.ProviderEntryIndex);
				Writer->WriteValue(TEXT("flags"),
					static_cast<uint32>(Entry.Flags));
				Writer->WriteValue(TEXT("moduleKey"),
					Entry.ModuleKey.Hash.ToHexString());
				Writer->WriteValue(TEXT("functionKey"),
					Entry.FunctionKey.Hash.ToHexString());
				Writer->WriteValue(TEXT("executionHash"),
					Entry.ExecutionHash.ToHexString());
				Writer->WriteValue(TEXT("debugHash"),
					Entry.DebugHash.ToHexString());
				Writer->WriteValue(TEXT("artifactProfile"),
					Entry.ArtifactProfile.Hash.ToHexString());
				Writer->WriteValue(TEXT("entryAbiHash"),
					Entry.EntryAbiHash.ToHexString());
				Writer->WriteValue(TEXT("nativeEnvironment"),
					Entry.NativeEnvironmentFingerprint.ToHexString());
				Writer->WriteValue(TEXT("hasVmEntry"), Entry.bHasVMEntry);
				Writer->WriteValue(TEXT("hasRawEntry"), Entry.bHasRawEntry);
				Writer->WriteValue(TEXT("hasParmsEntry"),
					Entry.bHasParmsEntry);
				Writer->WriteValue(TEXT("generatedModuleSource"),
					Entry.GeneratedModuleSource);
				Writer->WriteArrayStart(TEXT("references"));
				for (const FStaticJITDiagnostics::FReferenceRecord& Reference
					: Entry.References)
				{
					WriteReference(Writer, Reference);
				}
				Writer->WriteArrayEnd();
				Writer->WriteArrayStart(TEXT("nativeCallSites"));
				for (const FStaticJITDiagnostics::FNativeCallSiteRecord& CallSite
					: Entry.NativeCallSites)
				{
					WriteNativeCallSite(Writer, CallSite);
				}
				Writer->WriteArrayEnd();
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("routes"));
		for (const FStaticJITDiagnostics::FRouteRecord& Route : Snapshot.Routes)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("moduleKey"),
				Route.ModuleKey.Hash.ToHexString());
			Writer->WriteValue(TEXT("functionKey"),
				Route.Identity.FunctionKey.Hash.ToHexString());
			Writer->WriteValue(TEXT("executionHash"),
				Route.Identity.Content.Execution.ToHexString());
			Writer->WriteValue(TEXT("debugHash"),
				Route.Identity.Content.Debug.ToHexString());
			Writer->WriteValue(TEXT("artifactProfile"),
				Route.Identity.Profile.Hash.ToHexString());
			Writer->WriteValue(TEXT("canonicalDeclaration"),
				Route.CanonicalDeclaration);
			Writer->WriteValue(TEXT("numericFunctionId"),
				Route.NumericFunctionId);
			Writer->WriteValue(TEXT("selectedRoute"),
				RouteName(Route.SelectedExecutionRoute));
			Writer->WriteValue(TEXT("matchResult"),
				MatchName(Route.MatchResult));
			Writer->WriteValue(TEXT("verifiedArtifactIdentity"),
				Route.bHasVerifiedArtifactIdentity);
			Writer->WriteValue(TEXT("hasVmEntry"), Route.bHasVMEntry);
			Writer->WriteValue(TEXT("hasRawEntry"), Route.bHasRawEntry);
			Writer->WriteValue(TEXT("hasParmsEntry"), Route.bHasParmsEntry);
			Writer->WriteValue(TEXT("immutableCookedDirectDispatch"),
				Route.bImmutableCookedDirectDispatch);
			Writer->WriteValue(TEXT("providerId"),
				Route.ProviderId.Hash.ToHexString());
			Writer->WriteValue(TEXT("providerGeneration"),
				Route.ProviderGeneration.Hash.ToHexString());
			Writer->WriteValue(TEXT("providerEntryIndex"),
				Route.ProviderEntryIndex);
			Writer->WriteValue(TEXT("referenceSlotCount"),
				Route.ReferenceSlotCount);
			Writer->WriteValue(TEXT("resolvedReferenceSlotCount"),
				Route.ResolvedReferenceSlotCount);
			Writer->WriteValue(TEXT("vmExecutionCount"),
				FString::Printf(TEXT("%llu"), Route.VMExecutionCount));
			Writer->WriteValue(TEXT("rawExecutionCount"),
				FString::Printf(TEXT("%llu"), Route.RawExecutionCount));
			Writer->WriteValue(TEXT("parmsExecutionCount"),
				FString::Printf(TEXT("%llu"), Route.ParmsExecutionCount));
			Writer->WriteArrayStart(TEXT("candidates"));
			for (const FStaticJITDiagnostics::FRouteCandidateRecord& Candidate
				: Route.Candidates)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("providerId"),
					Candidate.ProviderId.Hash.ToHexString());
				Writer->WriteValue(TEXT("providerGeneration"),
					Candidate.ProviderGeneration.Hash.ToHexString());
				Writer->WriteValue(TEXT("providerEntryIndex"),
					Candidate.ProviderEntryIndex);
				Writer->WriteValue(TEXT("matchResult"),
					MatchName(Candidate.MatchResult));
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
		return Writer->Close() ? Json : FString();
	}
}

bool FStaticJITDiagnostics::HasJitFunction(asCScriptFunction* Function)
{
	return Function != nullptr && Function->GetJITBinding().VMEntry != nullptr;
}

#if AS_CAN_GENERATE_JIT
FStaticJITDiagnostics::FArtifactRequestRecord
FStaticJITDiagnostics::BuildArtifactRequestRecord(
	const FAngelscriptStaticJITGeneratorOutput& GeneratorOutput)
{
	const auto ConvertDisposition = [](
		const EAngelscriptStaticJITFunctionDisposition Disposition)
	{
		switch (Disposition)
		{
		case EAngelscriptStaticJITFunctionDisposition::Emitted:
			return EArtifactFunctionDisposition::Emitted;
		case EAngelscriptStaticJITFunctionDisposition::Failed:
			return EArtifactFunctionDisposition::Failed;
		case EAngelscriptStaticJITFunctionDisposition::Unsupported:
		default:
			return EArtifactFunctionDisposition::Unsupported;
		}
	};
	const auto CanonicalASTStateName = [](
		const EAngelscriptStaticJITCanonicalASTState State)
	{
		switch (State)
		{
		case EAngelscriptStaticJITCanonicalASTState::Verified:
			return TEXT("Verified");
		case EAngelscriptStaticJITCanonicalASTState::Invalid:
			return TEXT("Invalid");
		case EAngelscriptStaticJITCanonicalASTState::Missing:
		default:
			return TEXT("Missing");
		}
	};
	const auto TypedFunctionRoleName = [](
		const EAngelscriptStaticJITTypedClosureFunctionRole Role)
	{
		switch (Role)
		{
		case EAngelscriptStaticJITTypedClosureFunctionRole::UFunctionRoot:
			return TEXT("UFunctionRoot");
		case EAngelscriptStaticJITTypedClosureFunctionRole::DirectScript:
			return TEXT("DirectScript");
		case EAngelscriptStaticJITTypedClosureFunctionRole::
			InternalSemanticHelper:
			return TEXT("InternalSemanticHelper");
		case EAngelscriptStaticJITTypedClosureFunctionRole::Bridge:
			return TEXT("Bridge");
		case EAngelscriptStaticJITTypedClosureFunctionRole::Unsupported:
			return TEXT("Unsupported");
		case EAngelscriptStaticJITTypedClosureFunctionRole::NonRoot:
		default:
			return TEXT("NonRoot");
		}
	};
	const auto InvocationKindName = [](
		const EAngelscriptTypedASTJITInvocationKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptTypedASTJITInvocationKind::GlobalInitializer:
			return TEXT("GlobalInitializer");
		case EAngelscriptTypedASTJITInvocationKind::InstanceMethod:
			return TEXT("InstanceMethod");
		case EAngelscriptTypedASTJITInvocationKind::Constructor:
			return TEXT("Constructor");
		case EAngelscriptTypedASTJITInvocationKind::Destructor:
			return TEXT("Destructor");
		case EAngelscriptTypedASTJITInvocationKind::Factory:
			return TEXT("Factory");
		case EAngelscriptTypedASTJITInvocationKind::Imported:
			return TEXT("Imported");
		case EAngelscriptTypedASTJITInvocationKind::System:
			return TEXT("System");
		case EAngelscriptTypedASTJITInvocationKind::Funcdef:
			return TEXT("Funcdef");
		case EAngelscriptTypedASTJITInvocationKind::Synthesized:
			return TEXT("Synthesized");
		case EAngelscriptTypedASTJITInvocationKind::Global:
		default:
			return TEXT("Global");
		}
	};
	const auto ReceiverKindName = [](
		const EAngelscriptTypedASTJITReceiverKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptTypedASTJITReceiverKind::NativeObjectThis:
			return TEXT("NativeObjectThis");
		case EAngelscriptTypedASTJITReceiverKind::ExternalImplicitThis:
			return TEXT("ExternalImplicitThis");
		case EAngelscriptTypedASTJITReceiverKind::MixinFirstParameter:
			return TEXT("MixinFirstParameter");
		case EAngelscriptTypedASTJITReceiverKind::None:
		default:
			return TEXT("None");
		}
	};
	const auto CleanupPlanStateName = [](
		const EAngelscriptTypedASTJITCleanupPlanState State)
	{
		switch (State)
		{
		case EAngelscriptTypedASTJITCleanupPlanState::VerifiedEmpty:
			return TEXT("VerifiedEmpty");
		case EAngelscriptTypedASTJITCleanupPlanState::NonEmpty:
			return TEXT("NonEmpty");
		case EAngelscriptTypedASTJITCleanupPlanState::PartialConstruction:
			return TEXT("PartialConstruction");
		case EAngelscriptTypedASTJITCleanupPlanState::ScriptDestructor:
			return TEXT("ScriptDestructor");
		case EAngelscriptTypedASTJITCleanupPlanState::CompilerExceptionRegion:
			return TEXT("CompilerExceptionRegion");
		case EAngelscriptTypedASTJITCleanupPlanState::Unverified:
		default:
			return TEXT("Unverified");
		}
	};
	const auto ProfileKindName = [](
		const EAngelscriptStaticJITTypedExecutionProfileKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptStaticJITTypedExecutionProfileKind::PositionOnly:
			return TEXT("PositionOnly");
		case EAngelscriptStaticJITTypedExecutionProfileKind::Instrumented:
			return TEXT("Instrumented");
		case EAngelscriptStaticJITTypedExecutionProfileKind::Unavailable:
		default:
			return TEXT("Unavailable");
		}
	};
	const auto ExceptionControlStateName = [](
		const EAngelscriptStaticJITExceptionControlState State)
	{
		switch (State)
		{
		case EAngelscriptStaticJITExceptionControlState::NotRequired:
			return TEXT("NotRequired");
		case EAngelscriptStaticJITExceptionControlState::
			DirectExecutionState:
			return TEXT("DirectExecutionState");
		case EAngelscriptStaticJITExceptionControlState::Unavailable:
		default:
			return TEXT("Unavailable");
		}
	};
	const auto ExceptionPayloadStateName = [](
		const EAngelscriptStaticJITExceptionPayloadState State)
	{
		switch (State)
		{
		case EAngelscriptStaticJITExceptionPayloadState::NotRequired:
			return TEXT("NotRequired");
		case EAngelscriptStaticJITExceptionPayloadState::DirectFailureRecord:
			return TEXT("DirectFailureRecord");
		case EAngelscriptStaticJITExceptionPayloadState::NestedBridgeAdoption:
			return TEXT("NestedBridgeAdoption");
		case EAngelscriptStaticJITExceptionPayloadState::
			DirectFailureRecordAndNestedBridgeAdoption:
			return TEXT("DirectFailureRecordAndNestedBridgeAdoption");
		case EAngelscriptStaticJITExceptionPayloadState::Unavailable:
		default:
			return TEXT("Unavailable");
		}
	};
	const auto ConvertProcessedSource = [](
		const FAngelscriptStaticJITProcessedSource& Source)
	{
		FProcessedSourceRecord Record;
		Record.Section = Source.Section;
		Record.Row = Source.Row;
		Record.Column = Source.Column;
		return Record;
	};
	const auto ConvertSemanticFunction = [&]
		(const FAngelscriptStaticJITCanonicalFunctionDiagnostic& Source)
	{
		FCanonicalFunctionDiagnosticRecord Record;
		Record.FunctionKey = Source.FunctionKey;
		Record.Role = TypedFunctionRoleName(Source.Role);
		Record.RawTraitBits = Source.RawTraitBits;
		Record.UnknownTraitBits = Source.UnknownTraitBits;
		Record.InvocationKind = InvocationKindName(Source.InvocationKind);
		Record.ReceiverKind = ReceiverKindName(Source.ReceiverKind);
		Record.ReceiverParameterIndex = Source.ReceiverParameterIndex;
		Record.CanonicalASTState =
			CanonicalASTStateName(Source.CanonicalASTState);
		Record.CleanupPlanState = CleanupPlanStateName(
			Source.CleanupPlanState);
		Record.LifetimeProtocolRevision =
			Source.LifetimeSummary.ProtocolRevision;
		Record.LifetimeStableKey = Source.LifetimeSummary.StableKey;
		Record.LifetimeABIKey = Source.LifetimeSummary.ABIKey;
		Record.LifetimeActionFlags = static_cast<uint32>(
			Source.LifetimeSummary.ActionFlags);
		Record.LifetimeExitPlanFlags = static_cast<uint32>(
			Source.LifetimeSummary.ExitPlanFlags);
		Record.LifetimeSupportedExitMask =
			Source.LifetimeSummary.SupportedExitMask;
		Record.LifetimeRecordCount = Source.LifetimeSummary.RecordCount;
		Record.LifetimeExitPlanCount =
			Source.LifetimeSummary.ExitPlanCount;
		Record.bNativeObjectFrameABIAvailable =
			Source.LifetimeSummary.bNativeObjectFrameABIAvailable;
		Record.bHasSuspendState = Source.bHasSuspendState;
		Record.bHasExceptionCleanup = Source.bHasExceptionCleanup;
		Record.bCleanupPlanCoversAllTransfers =
			Source.bCleanupPlanCoversAllTransfers;
		Record.ComponentIndex = Source.ComponentIndex;
		Record.bRecursiveComponent = Source.bRecursiveComponent;
		Record.EligibilityReason = LexToString(Source.EligibilityReason);
		Record.EligibilityDetail = Source.EligibilityDetail;
		Record.EligibilitySource = ConvertProcessedSource(
			Source.EligibilitySource);
		return Record;
	};
	const auto ConvertTypedCall = [&]
		(const FAngelscriptStaticJITTypedCallDiagnostic& Source)
	{
		FTypedCallDiagnosticRecord Record;
		Record.CallerFunctionKey = Source.CallerFunctionKey;
		Record.CalleeFunctionKey = Source.CalleeFunctionKey;
		Record.ExpressionIndex = Source.ExpressionIndex;
		Record.ProcessedSource = ConvertProcessedSource(Source.ProcessedSource);
		if (Source.AuthoredSource.IsSet())
		{
			Record.AuthoredSource = ConvertProcessedSource(
				Source.AuthoredSource.GetValue());
		}
		if (Source.GeneratedSource.IsSet())
		{
			Record.GeneratedSource = ConvertProcessedSource(
				Source.GeneratedSource.GetValue());
		}
		Record.TargetKind = TypedCallTargetKindName(Source.TargetKind);
		Record.Disposition = TypedCallDispositionName(Source.Disposition);
		Record.CalleeEligibilityReason = LexToString(
			Source.CalleeEligibilityReason);
		Record.CalleeEligibilityDetail = Source.CalleeEligibilityDetail;
		Record.CalleeEligibilitySource = ConvertProcessedSource(
			Source.CalleeEligibilitySource);
		Record.NativeLinkageValidation = Source.bHasNativeLinkageValidation
			? NativeCallValidationName(Source.NativeLinkageValidation)
			: TEXT("NotApplicable");
		Record.NativeLinkageValidationDetail =
			Source.NativeLinkageValidationDetail;
		Record.CanonicalDeclaration = Source.CanonicalDeclaration;
		Record.EmittedRoute = NativeCallSiteRouteName(Source.EmittedRoute);
		Record.EmittedCppCallee = Source.EmittedCppCallee;
		Record.RuntimeDllCore = Source.RuntimeDllCore;
		Record.MetadataRowSymbol = Source.MetadataRowSymbol;
		Record.RegisteredTarget = Source.RegisteredTarget;
		Record.DirectCppSymbol = Source.DirectCppSymbol;
		Record.StableTargetKey = Source.StableTargetKey;
		Record.ExpectedAbi = Source.ExpectedAbi;
		Record.ReferenceSlotIndex = Source.ReferenceSlotIndex;
		return Record;
	};
	const auto ConvertCallClosure = [&]
		(const FAngelscriptStaticJITTypedCallClosureDiagnostic& Source)
	{
		FTypedCallClosureDiagnosticRecord Record;
		Record.bEligible = Source.bEligible;
		Record.RequiredCapabilities = LexToString(
			Source.RequiredCapabilities);
		Record.AvailableCapabilities = LexToString(
			Source.AvailableCapabilities);
		Record.ExecutionRequirementsIdentity =
			Source.ExecutionRequirementsIdentityHash.ToHexString();
		Record.AvailableCapabilityProfileIdentity =
			Source.AvailableCapabilityProfileIdentityHash.ToHexString();
		Record.ProfileKind = ProfileKindName(Source.ProfileKind);
		Record.ExceptionControlState = ExceptionControlStateName(
			Source.ExceptionControlState);
		Record.ExceptionPayloadState = ExceptionPayloadStateName(
			Source.ExceptionPayloadState);
		Record.bRecursionGuardAvailable =
			Source.bRecursionGuardAvailable;
		Record.Functions.Reserve(Source.Functions.Num());
		for (const FAngelscriptStaticJITCanonicalFunctionDiagnostic&
			Function : Source.Functions)
		{
			Record.Functions.Add(ConvertSemanticFunction(Function));
		}
		Record.Functions.Sort([](const auto& Left, const auto& Right)
		{
			return Left.FunctionKey.Hash.ToHexString()
				< Right.FunctionKey.Hash.ToHexString();
		});
		Record.Calls.Reserve(Source.Calls.Num());
		for (const FAngelscriptStaticJITTypedCallDiagnostic& Call
			: Source.Calls)
		{
			Record.Calls.Add(ConvertTypedCall(Call));
		}
		Record.Calls.Sort([](const auto& Left, const auto& Right)
		{
			if (Left.CallerFunctionKey.Hash != Right.CallerFunctionKey.Hash)
			{
				return Left.CallerFunctionKey.Hash
					< Right.CallerFunctionKey.Hash;
			}
			if (Left.ExpressionIndex != Right.ExpressionIndex)
			{
				return Left.ExpressionIndex < Right.ExpressionIndex;
			}
			return Left.CalleeFunctionKey.Hash
				< Right.CalleeFunctionKey.Hash;
		});
		return Record;
	};
	const auto ConvertTypedDiagnostic = [&](
		const FAngelscriptStaticJITTypedFunctionDiagnostic& Source)
	{
		FTypedFunctionDiagnosticRecord Record;
		Record.FallbackReason = LexToString(Source.FallbackReason);
		Record.Detail = Source.Detail;
		Record.ProcessedSource = ConvertProcessedSource(
			Source.ProcessedSource);
		if (Source.SemanticFunction.IsSet())
		{
			Record.SemanticFunction = ConvertSemanticFunction(
				Source.SemanticFunction.GetValue());
		}
		if (Source.CallClosure.IsSet())
		{
			Record.CallClosure = ConvertCallClosure(
				Source.CallClosure.GetValue());
		}
		return Record;
	};

	FArtifactRequestRecord Record;
	Record.RequestedBackendId =
		GeneratorOutput.Diagnostics.RequestedBackendId.ToString();
	Record.CaptureState = GeneratorOutput.Diagnostics.CaptureProfile
		== EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST
		? EArtifactCaptureState::VerifiedCanonicalAST
		: EArtifactCaptureState::Bytecode;
	Record.bCanonicalASTCaptureEnabled = Record.CaptureState
		== EArtifactCaptureState::VerifiedCanonicalAST;
	Record.FallbackChain.Reserve(
		GeneratorOutput.Diagnostics.FallbackChain.Num());
	for (const FAngelscriptStaticJITBackendId& BackendId
		: GeneratorOutput.Diagnostics.FallbackChain)
	{
		Record.FallbackChain.Add(BackendId.ToString());
	}
	Record.CompiledSourceGraph.SourceCompilePassCount =
		GeneratorOutput.Diagnostics.CompiledSourceGraph.SourceCompilePassCount;
	Record.CompiledSourceGraph.ModuleCount =
		GeneratorOutput.Diagnostics.CompiledSourceGraph.ModuleCount;
	Record.CompiledSourceGraph.FunctionCount =
		GeneratorOutput.Diagnostics.CompiledSourceGraph.FunctionCount;
	Record.CompiledSourceGraph.TypeCount =
		GeneratorOutput.Diagnostics.CompiledSourceGraph.TypeCount;
	Record.CompiledSourceGraph.GlobalCount =
		GeneratorOutput.Diagnostics.CompiledSourceGraph.GlobalCount;
	Record.CompiledSourceGraph.DescriptorCount =
		GeneratorOutput.Diagnostics.CompiledSourceGraph.DescriptorCount;
	Record.CompiledSourceGraph.DependencyCount =
		GeneratorOutput.Diagnostics.CompiledSourceGraph.DependencyCount;
	Record.CompiledSourceGraph.ExternalNativeCallCount =
		GeneratorOutput.Diagnostics.CompiledSourceGraph.ExternalNativeCallCount;
	Record.CompiledSourceGraph.EmitModuleCount =
		GeneratorOutput.Diagnostics.CompiledSourceGraph.EmitModuleCount;
	Record.CompiledSourceGraph.bHasVerifiedCanonicalAST =
		GeneratorOutput.Diagnostics.CompiledSourceGraph.bHasVerifiedCanonicalAST;
	Record.TaskError = GeneratorOutput.Diagnostics.TaskError;
	Record.Functions.Reserve(GeneratorOutput.Functions.Num());
	for (const FAngelscriptStaticJITFunctionResult& Function
		: GeneratorOutput.Functions)
	{
		FArtifactFunctionRecord& FunctionRecord =
			Record.Functions.AddDefaulted_GetRef();
		FunctionRecord.FunctionKey = Function.FunctionKey;
		FunctionRecord.ActualBackendId = Function.ActualBackendId.ToString();
		FunctionRecord.Disposition = ConvertDisposition(Function.Disposition);
		FunctionRecord.Detail = Function.Detail;
		if (Function.TypedDiagnostic.IsSet())
		{
			FunctionRecord.TypedDiagnostic = ConvertTypedDiagnostic(
				Function.TypedDiagnostic.GetValue());
		}
		if (Function.EmittedFunction.IsSet())
		{
			FunctionRecord.bHasVMEntry =
				Function.EmittedFunction->bHasVMEntry;
			FunctionRecord.bHasRawEntry =
				Function.EmittedFunction->bHasRawEntry;
			FunctionRecord.bHasParmsEntry =
				Function.EmittedFunction->bHasParmsEntry;
		}
		FunctionRecord.BackendAttempts.Reserve(
			Function.BackendAttempts.Num());
		for (const FAngelscriptStaticJITBackendAttemptDiagnostic& Attempt
			: Function.BackendAttempts)
		{
			FArtifactBackendAttemptRecord& AttemptRecord =
				FunctionRecord.BackendAttempts.AddDefaulted_GetRef();
			AttemptRecord.BackendId = Attempt.BackendId.ToString();
			AttemptRecord.Disposition =
				ConvertDisposition(Attempt.Disposition);
			AttemptRecord.Detail = Attempt.Detail;
			if (Attempt.TypedDiagnostic.IsSet())
			{
				AttemptRecord.TypedDiagnostic = ConvertTypedDiagnostic(
					Attempt.TypedDiagnostic.GetValue());
			}
		}
	}
	return Record;
}

FAngelscriptJITGenerationDiagnostics
BuildAngelscriptStaticJITGenerationDiagnostics(
	const FAngelscriptStaticJITGeneratorOutput& GeneratorOutput,
	const TConstArrayView<FAngelscriptStableFunctionKey> EmittedFunctionKeys)
{
	const FStaticJITDiagnostics::FArtifactRequestRecord Record =
		FStaticJITDiagnostics::BuildArtifactRequestRecord(GeneratorOutput);
	FAngelscriptJITGenerationDiagnostics Result;
	Result.RequestKind = TEXT("StaticJITArtifact");
	Result.RequestedBackendId = Record.RequestedBackendId;
	Result.CaptureState = Record.CaptureState
		== FStaticJITDiagnostics::EArtifactCaptureState::VerifiedCanonicalAST
		? TEXT("VerifiedCanonicalAST") : TEXT("Bytecode");
	Result.FallbackChain = Record.FallbackChain;
	Result.SourceCompilePassCount =
		Record.CompiledSourceGraph.SourceCompilePassCount;
	Result.CompiledModuleCount = Record.CompiledSourceGraph.ModuleCount;
	Result.CompiledFunctionCount = Record.CompiledSourceGraph.FunctionCount;
	Result.CompiledTypeCount = Record.CompiledSourceGraph.TypeCount;
	Result.CompiledGlobalCount = Record.CompiledSourceGraph.GlobalCount;
	Result.CompiledDescriptorCount = Record.CompiledSourceGraph.DescriptorCount;
	Result.CompiledDependencyCount = Record.CompiledSourceGraph.DependencyCount;
	Result.CompiledExternalNativeCallCount =
		Record.CompiledSourceGraph.ExternalNativeCallCount;
	Result.EmitModuleCount = Record.CompiledSourceGraph.EmitModuleCount;
	Result.bHasVerifiedCanonicalAST =
		Record.CompiledSourceGraph.bHasVerifiedCanonicalAST;
	Result.TaskError = Record.TaskError;

	const auto CopySource = [](
		const FStaticJITDiagnostics::FProcessedSourceRecord& Source)
	{
		FAngelscriptJITGenerationDiagnosticSource Copy;
		Copy.Section = Source.Section;
		Copy.Row = Source.Row;
		Copy.Column = Source.Column;
		return Copy;
	};
	const auto CopySemantic = [&](const auto& Source)
	{
		FAngelscriptJITGenerationCanonicalFunctionDiagnostic Copy;
		Copy.FunctionKey = Source.FunctionKey;
		Copy.Role = Source.Role;
		Copy.RawTraitBits = Source.RawTraitBits;
		Copy.UnknownTraitBits = Source.UnknownTraitBits;
		Copy.InvocationKind = Source.InvocationKind;
		Copy.ReceiverKind = Source.ReceiverKind;
		Copy.ReceiverParameterIndex = Source.ReceiverParameterIndex;
		Copy.CanonicalASTState = Source.CanonicalASTState;
		Copy.CleanupPlanState = Source.CleanupPlanState;
		Copy.LifetimeProtocolRevision = Source.LifetimeProtocolRevision;
		Copy.LifetimeStableKey = Source.LifetimeStableKey;
		Copy.LifetimeABIKey = Source.LifetimeABIKey;
		Copy.LifetimeActionFlags = Source.LifetimeActionFlags;
		Copy.LifetimeExitPlanFlags = Source.LifetimeExitPlanFlags;
		Copy.LifetimeSupportedExitMask = Source.LifetimeSupportedExitMask;
		Copy.LifetimeRecordCount = Source.LifetimeRecordCount;
		Copy.LifetimeExitPlanCount = Source.LifetimeExitPlanCount;
		Copy.bNativeObjectFrameABIAvailable =
			Source.bNativeObjectFrameABIAvailable;
		Copy.bHasSuspendState = Source.bHasSuspendState;
		Copy.bHasExceptionCleanup = Source.bHasExceptionCleanup;
		Copy.bCleanupPlanCoversAllTransfers =
			Source.bCleanupPlanCoversAllTransfers;
		Copy.ComponentIndex = Source.ComponentIndex;
		Copy.bRecursiveComponent = Source.bRecursiveComponent;
		Copy.EligibilityReason = Source.EligibilityReason;
		Copy.EligibilityDetail = Source.EligibilityDetail;
		Copy.EligibilitySource = CopySource(Source.EligibilitySource);
		return Copy;
	};

	TArray<FAngelscriptStableFunctionKey> SortedKeys(EmittedFunctionKeys);
	SortedKeys.Sort([](const auto& Left, const auto& Right)
	{
		return Left.Hash < Right.Hash;
	});
	for (const FAngelscriptStableFunctionKey& FunctionKey : SortedKeys)
	{
		const FStaticJITDiagnostics::FArtifactFunctionRecord* const Source =
			Record.Functions.FindByPredicate([&](const auto& Candidate)
			{
				return Candidate.FunctionKey == FunctionKey;
			});
		const FAngelscriptStaticJITFunctionResult* const RawSource =
			GeneratorOutput.Functions.FindByPredicate([&](const auto& Candidate)
			{
				return Candidate.FunctionKey == FunctionKey;
			});
		if (Source == nullptr || RawSource == nullptr)
		{
			continue;
		}

		FAngelscriptJITGenerationFunctionDiagnostic& Function =
			Result.Functions.AddDefaulted_GetRef();
		Function.FunctionKey = FunctionKey;
		Function.ActualBackendId = Source->ActualBackendId;
		Function.Disposition = ArtifactFunctionDispositionName(
			Source->Disposition);
		Function.Detail = Source->Detail;
		for (int32 AttemptIndex = 0;
			AttemptIndex < Source->BackendAttempts.Num(); ++AttemptIndex)
		{
			const FStaticJITDiagnostics::FArtifactBackendAttemptRecord&
				SourceAttempt = Source->BackendAttempts[AttemptIndex];
			if (!RawSource->BackendAttempts.IsValidIndex(AttemptIndex))
			{
				continue;
			}
			const FAngelscriptStaticJITBackendAttemptDiagnostic& RawAttempt =
				RawSource->BackendAttempts[AttemptIndex];
			FAngelscriptJITGenerationBackendAttemptDiagnostic& Attempt =
				Function.BackendAttempts.AddDefaulted_GetRef();
			Attempt.BackendId = SourceAttempt.BackendId;
			Attempt.Disposition = ArtifactFunctionDispositionName(
				SourceAttempt.Disposition);
			Attempt.Detail = SourceAttempt.Detail;
			if (!SourceAttempt.TypedDiagnostic.IsSet()
				|| !RawAttempt.TypedDiagnostic.IsSet())
			{
				Attempt.FallbackReason = TEXT("");
				Attempt.TypedDetail = TEXT("");
				Attempt.ProfileKind = TEXT("Unavailable");
				Attempt.ExceptionControlState = TEXT("Unavailable");
				Attempt.ExceptionPayloadState = TEXT("Unavailable");
				continue;
			}

			Attempt.bHasTypedDiagnostic = true;
			const FStaticJITDiagnostics::FTypedFunctionDiagnosticRecord&
				Typed = SourceAttempt.TypedDiagnostic.GetValue();
			const FAngelscriptStaticJITTypedFunctionDiagnostic& RawTyped =
				RawAttempt.TypedDiagnostic.GetValue();
			Attempt.FallbackReason = Typed.FallbackReason;
			Attempt.TypedDetail = Typed.Detail;
			Attempt.ProcessedSource = CopySource(Typed.ProcessedSource);
			if (Typed.CallClosure.IsSet() && RawTyped.CallClosure.IsSet())
			{
				const FStaticJITDiagnostics::FTypedCallClosureDiagnosticRecord&
					Closure = Typed.CallClosure.GetValue();
				const FAngelscriptStaticJITTypedCallClosureDiagnostic& RawClosure =
					RawTyped.CallClosure.GetValue();
				Attempt.RequiredCapabilities =
					static_cast<EAngelscriptJITExecutionCapability>(
						static_cast<uint32>(RawClosure.RequiredCapabilities));
				Attempt.AvailableCapabilities =
					static_cast<EAngelscriptJITExecutionCapability>(
						static_cast<uint32>(RawClosure.AvailableCapabilities));
				Attempt.ExecutionRequirementsIdentity =
					RawClosure.ExecutionRequirementsIdentityHash;
				Attempt.AvailableCapabilityProfileIdentity =
					RawClosure.AvailableCapabilityProfileIdentityHash;
				Attempt.ProfileKind = Closure.ProfileKind;
				Attempt.ExceptionControlState = Closure.ExceptionControlState;
				Attempt.ExceptionPayloadState = Closure.ExceptionPayloadState;
				Attempt.bRecursionGuardAvailable =
					Closure.bRecursionGuardAvailable;
				for (const auto& Semantic : Closure.Functions)
				{
					Attempt.SemanticFunctions.Add(CopySemantic(Semantic));
				}
				if (Typed.SemanticFunction.IsSet())
				{
					Attempt.PrimarySemanticFunctionIndex =
						Attempt.SemanticFunctions.IndexOfByPredicate(
							[&](const auto& Candidate)
							{
								return Candidate.FunctionKey
									== Typed.SemanticFunction->FunctionKey;
							});
				}
				for (const auto& Call : Closure.Calls)
				{
					const FAngelscriptStaticJITTypedCallDiagnostic* const RawCall =
						RawClosure.Calls.FindByPredicate([&](const auto& Candidate)
						{
							return Candidate.CallerFunctionKey
								== Call.CallerFunctionKey
								&& Candidate.CalleeFunctionKey
									== Call.CalleeFunctionKey
								&& Candidate.ExpressionIndex
									== Call.ExpressionIndex;
						});
					if (RawCall == nullptr)
					{
						continue;
					}
					FAngelscriptJITGenerationTypedCallDiagnostic& Copy =
						Attempt.Calls.AddDefaulted_GetRef();
					Copy.CallerFunctionKey = Call.CallerFunctionKey;
					Copy.CalleeFunctionKey = Call.CalleeFunctionKey;
					Copy.ExpressionIndex = Call.ExpressionIndex;
					Copy.ProcessedSource = CopySource(Call.ProcessedSource);
					if (Call.AuthoredSource.IsSet())
					{
						Copy.AuthoredSource =
							CopySource(Call.AuthoredSource.GetValue());
					}
					if (Call.GeneratedSource.IsSet())
					{
						Copy.GeneratedSource =
							CopySource(Call.GeneratedSource.GetValue());
					}
					Copy.TargetKind = Call.TargetKind;
					Copy.Disposition = Call.Disposition;
					Copy.CalleeEligibilityReason =
						Call.CalleeEligibilityReason;
					Copy.CalleeEligibilityDetail =
						Call.CalleeEligibilityDetail;
					Copy.CalleeEligibilitySource =
						CopySource(Call.CalleeEligibilitySource);
					Copy.bHasNativeLinkageValidation =
						RawCall->bHasNativeLinkageValidation;
					Copy.NativeLinkageValidation =
						Call.NativeLinkageValidation;
					Copy.NativeLinkageValidationDetail =
						Call.NativeLinkageValidationDetail;
					Copy.CanonicalDeclaration = Call.CanonicalDeclaration;
					Copy.EmittedRoute = RawCall->EmittedRoute;
					Copy.EmittedCppCallee = Call.EmittedCppCallee;
					Copy.RuntimeDllCore = Call.RuntimeDllCore;
					Copy.MetadataRowSymbol = Call.MetadataRowSymbol;
					Copy.RegisteredTarget = Call.RegisteredTarget;
					Copy.DirectCppSymbol = Call.DirectCppSymbol;
					Copy.StableTargetKey = Call.StableTargetKey;
					Copy.ExpectedAbi = Call.ExpectedAbi;
					Copy.ReferenceSlotIndex = Call.ReferenceSlotIndex;
				}
			}
			else
			{
				Attempt.ProfileKind = TEXT("Unavailable");
				Attempt.ExceptionControlState = TEXT("Unavailable");
				Attempt.ExceptionPayloadState = TEXT("Unavailable");
				if (Typed.SemanticFunction.IsSet())
				{
					Attempt.SemanticFunctions.Add(
						CopySemantic(Typed.SemanticFunction.GetValue()));
					Attempt.PrimarySemanticFunctionIndex = 0;
				}
			}
		}
	}
	return Result;
}
#endif

FStaticJITDiagnostics::FSnapshot FStaticJITDiagnostics::CaptureSnapshot(FAngelscriptEngine* Engine)
{
	FAngelscriptEngine* CurrentEngine = Engine != nullptr ? Engine : FAngelscriptEngine::TryGetCurrentEngine();

	FSnapshot Snapshot;
	Snapshot.ArtifactRequest = FArtifactRequestRecord();
	Snapshot.bHasCurrentEngine = CurrentEngine != nullptr;
	Snapshot.bHasScriptEngine = CurrentEngine != nullptr && CurrentEngine->GetScriptEngine() != nullptr;

	if (CurrentEngine != nullptr)
	{
		Snapshot.TargetProfile =
			FAngelscriptJITProviderRouter::ResolveTargetProfile(
				CurrentEngine->GetRuntimeConfig());
	}

	const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
		ESPMode::ThreadSafe> ProviderSnapshot =
		FAngelscriptJITProviderRegistry::Get().GetSnapshot();
	if (ProviderSnapshot.IsValid())
	{
		Snapshot.ProviderPublicationOrdinal =
			ProviderSnapshot->PublicationOrdinal;
		Snapshot.Providers.Reserve(ProviderSnapshot->Providers.Num());
		for (const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>& Catalog : ProviderSnapshot->Providers)
		{
			if (!Catalog.IsValid())
			{
				continue;
			}
			FProviderRecord& Provider = Snapshot.Providers.AddDefaulted_GetRef();
			Provider.ProviderId = Catalog->ProviderId;
			Provider.ProviderGeneration = Catalog->ProviderGeneration;
			Provider.ArtifactSetDigest = Catalog->ArtifactSetDigest;
			Provider.ArtifactProfile = Catalog->ArtifactProfile;
			Provider.NativeEnvironmentFingerprint =
				Catalog->NativeEnvironmentFingerprint;
			Provider.ProviderName = Catalog->ProviderName;
			Provider.OwnerModuleName = Catalog->OwnerModuleName;
			Provider.GenerationDiagnostics = Catalog->Diagnostics;
			Provider.Entries.Reserve(Catalog->Entries.Num());
			for (const FAngelscriptJITProviderCatalogEntry& CatalogEntry
				: Catalog->Entries)
			{
				FProviderEntryRecord& Entry =
					Provider.Entries.AddDefaulted_GetRef();
				Entry.ProviderEntryIndex = Provider.Entries.Num() - 1;
				Entry.Flags = CatalogEntry.Flags;
				Entry.ModuleKey = CatalogEntry.ModuleKey;
				Entry.FunctionKey = CatalogEntry.FunctionKey;
				Entry.ExecutionHash = CatalogEntry.ExecutionHash;
				Entry.DebugHash = CatalogEntry.DebugHash;
				Entry.ArtifactProfile = CatalogEntry.ArtifactProfile;
				Entry.EntryAbiHash = CatalogEntry.EntryAbiHash;
				Entry.NativeEnvironmentFingerprint =
					CatalogEntry.NativeEnvironmentFingerprint;
				Entry.bHasVMEntry =
					CatalogEntry.EntryPoints.VMEntry != nullptr;
				Entry.bHasRawEntry =
					CatalogEntry.EntryPoints.RawEntry != nullptr;
				Entry.bHasParmsEntry =
					CatalogEntry.EntryPoints.ParmsEntry != nullptr;
				if (Snapshot.TargetProfile
					!= EAngelscriptJITTargetProfile::Invalid)
				{
					Entry.GeneratedModuleSource =
						FAngelscriptJITGeneration::BuildModuleSourceRelativePath(
							CatalogEntry.ModuleKey,
							Snapshot.TargetProfile);
				}
				Entry.References.Reserve(
					CatalogEntry.ReferenceSlots.Num());
				for (const FAngelscriptJITReferenceSlot& CatalogReference
					: CatalogEntry.ReferenceSlots)
				{
					FReferenceRecord& Reference =
						Entry.References.AddDefaulted_GetRef();
					Reference.SlotIndex = CatalogReference.SlotIndex;
					Reference.Flags = CatalogReference.Flags;
					Reference.Reference = CatalogReference.Reference;
				}
				Entry.NativeCallSites.Reserve(
					CatalogEntry.NativeCallSites.Num());
				for (const FAngelscriptJITProviderCatalogNativeCallSite&
					CatalogCallSite : CatalogEntry.NativeCallSites)
				{
					FNativeCallSiteRecord& CallSite =
						Entry.NativeCallSites.AddDefaulted_GetRef();
					CallSite.CallSiteIndex = CatalogCallSite.CallSiteIndex;
					CallSite.Route = CatalogCallSite.Route;
					CallSite.CanonicalDeclaration =
						CatalogCallSite.CanonicalDeclaration;
					CallSite.EmittedCppCallee =
						CatalogCallSite.EmittedCppCallee;
					CallSite.RuntimeDllCore = CatalogCallSite.RuntimeDllCore;
					CallSite.MetadataRowSymbol =
						CatalogCallSite.MetadataRowSymbol;
					CallSite.RegisteredTarget =
						CatalogCallSite.RegisteredTarget;
					CallSite.StableTargetKey =
						CatalogCallSite.StableTargetKey;
					CallSite.ExpectedAbi = CatalogCallSite.ExpectedAbi;
					CallSite.ReferenceSlotIndex =
						CatalogCallSite.ReferenceSlotIndex;
				}
			}

			TArray<FAngelscriptStableModuleKey> ModuleKeys;
			for (const FProviderEntryRecord& Entry : Provider.Entries)
			{
				ModuleKeys.AddUnique(Entry.ModuleKey);
			}
			ModuleKeys.Sort([](
				const FAngelscriptStableModuleKey& Left,
				const FAngelscriptStableModuleKey& Right)
			{
				return Left.Hash < Right.Hash;
			});
			Provider.Modules.Reserve(ModuleKeys.Num());
			for (const FAngelscriptStableModuleKey& ModuleKey : ModuleKeys)
			{
				FProviderModuleRecord& Module =
					Provider.Modules.AddDefaulted_GetRef();
				Module.ModuleKey = ModuleKey;
				Module.ModuleArtifactDigest = BuildModuleArtifactDigest(
					ModuleKey, Provider.Entries);
				if (Snapshot.TargetProfile
					!= EAngelscriptJITTargetProfile::Invalid)
				{
					Module.GeneratedModuleSource =
						FAngelscriptJITGeneration::BuildModuleSourceRelativePath(
							ModuleKey, Snapshot.TargetProfile);
				}
				for (const FProviderEntryRecord& Entry : Provider.Entries)
				{
					Module.FunctionCount += Entry.ModuleKey == ModuleKey
						? 1u : 0u;
				}
			}
		}
	}

	if (CurrentEngine != nullptr)
	{
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> RouteSnapshot =
			CurrentEngine->GetFunctionRouteSnapshot();
		if (RouteSnapshot.IsValid())
		{
			TArray<FAngelscriptArtifactReference> RequiredReferences;
			if (ProviderSnapshot.IsValid())
			{
				for (const TSharedPtr<const FAngelscriptJITProviderCatalog,
					ESPMode::ThreadSafe>& Catalog : ProviderSnapshot->Providers)
				{
					if (!Catalog.IsValid())
					{
						continue;
					}
					for (const FAngelscriptJITProviderCatalogEntry& Entry
						: Catalog->Entries)
					{
						for (const FAngelscriptJITReferenceSlot& Slot
							: Entry.ReferenceSlots)
						{
							RequiredReferences.AddUnique(Slot.Reference);
						}
					}
				}
			}
			const FAngelscriptJITCurrentEngineReferenceBuildResult
				ReferenceBuild =
					FAngelscriptJITCurrentEngineReferenceResolverBuilder::Build(
						*CurrentEngine,
						RequiredReferences);
			TMap<const void*, FString> CurrentSystemFunctionDeclarations;
			if (ReferenceBuild.IsUsable())
			{
				TArray<asCScriptFunction*> CurrentSystemFunctions;
				FAngelscriptCacheEnvironmentIdentity::
					CollectCurrentRegisteredFunctions(
						*static_cast<asCScriptEngine*>(
							CurrentEngine->GetScriptEngine()),
						CurrentSystemFunctions);
				CurrentSystemFunctionDeclarations.Reserve(
					CurrentSystemFunctions.Num());
				for (asCScriptFunction* const Function : CurrentSystemFunctions)
				{
					if (Function != nullptr)
					{
						CurrentSystemFunctionDeclarations.Add(
							Function,
							UTF8_TO_TCHAR(Function->GetDeclaration(
								false, false, false)));
					}
				}

				for (FProviderRecord& Provider : Snapshot.Providers)
				{
					for (FProviderEntryRecord& Entry : Provider.Entries)
					{
						for (FReferenceRecord& Reference : Entry.References)
						{
							const FAngelscriptJITReferenceResolution Resolution =
								ReferenceBuild.Resolver->Resolve(
									Reference.Reference);
							Reference.CurrentResolution = Resolution.Code;
							Reference.bResolved =
								Resolution.Code
									== EAngelscriptJITReferenceResolutionCode::Exact
								&& Resolution.Value != nullptr;
							Reference.AdoptionState = Reference.bResolved
								? EReferenceAdoptionState::ResolvedNotAdopted
								: (Resolution.Code
										== EAngelscriptJITReferenceResolutionCode::Missing
									? EReferenceAdoptionState::Unbound
									: EReferenceAdoptionState::Rejected);
							if (Reference.bResolved)
							{
								if (const FString* const Declaration =
									CurrentSystemFunctionDeclarations.Find(
										Resolution.Value))
								{
									Reference.CurrentTargetDeclaration =
										*Declaration;
									Reference.CurrentTargetKind =
										TEXT("SystemFunction");
								}
							}
						}
					}
				}
			}
			const FAngelscriptHash256 NativeEnvironmentFingerprint =
				Snapshot.TargetProfile != EAngelscriptJITTargetProfile::Invalid
					? FAngelscriptJITTargetProfiles::
						BuildCurrentNativeEnvironmentFingerprint(
							Snapshot.TargetProfile)
					: FAngelscriptHash256();

			Snapshot.RouteGeneration = RouteSnapshot->Generation.Value;
			Snapshot.VmRouteCount = RouteSnapshot->VmRouteCount;
			Snapshot.NativeRouteCount = RouteSnapshot->NativeRouteCount;
			Snapshot.Routes.Reserve(RouteSnapshot->FunctionRoutes.Num());
			for (const FAngelscriptFunctionRoute& CurrentRoute
				: RouteSnapshot->FunctionRoutes)
			{
				FRouteRecord& Route = Snapshot.Routes.AddDefaulted_GetRef();
				Route.ModuleKey = CurrentRoute.ModuleKey;
				Route.Identity = CurrentRoute.Identity;
				Route.CanonicalDeclaration =
					CurrentRoute.CanonicalDeclaration;
				Route.NumericFunctionId = CurrentRoute.NumericFunctionId;
				Route.SelectedExecutionRoute =
					CurrentRoute.SelectedExecutionRoute;
				Route.MatchResult = CurrentRoute.MatchResult;
				Route.bHasVerifiedArtifactIdentity =
					CurrentRoute.bHasVerifiedArtifactIdentity;

				if (CurrentRoute.Function != nullptr
					&& ProviderSnapshot.IsValid()
					&& ReferenceBuild.IsUsable()
					&& CurrentRoute.bHasVerifiedArtifactIdentity)
				{
					const bool bHasParmsEntry =
						FAngelscriptBytecodeJIT::CanGenerateParmsEntry(
							CurrentRoute.Function);
					FAngelscriptJITFunctionMatchInput Input;
					Input.ModuleKey = CurrentRoute.ModuleKey;
					Input.Identity = CurrentRoute.Identity;
					Input.EntryAbiHash =
						FAngelscriptJITGeneration::BuildEntryAbiHash(
							CurrentRoute.CanonicalDeclaration,
							true,
							true,
							bHasParmsEntry);
					Input.NativeEnvironmentFingerprint =
						NativeEnvironmentFingerprint;
					Input.RequiredEntryPoints =
						EAngelscriptJITRequiredEntryPoints::VM
						| EAngelscriptJITRequiredEntryPoints::Raw;
					if (bHasParmsEntry)
					{
						Input.RequiredEntryPoints |=
							EAngelscriptJITRequiredEntryPoints::Parms;
					}
					FAngelscriptJITFunctionMatchResult Match;
					if (bHasParmsEntry)
					{
						FAngelscriptJITFunctionMatchInput
							VMRawFallbackInput = Input;
						VMRawFallbackInput.EntryAbiHash =
							FAngelscriptJITGeneration::BuildEntryAbiHash(
								CurrentRoute.CanonicalDeclaration,
								true, true, false);
						VMRawFallbackInput.RequiredEntryPoints =
							EAngelscriptJITRequiredEntryPoints::VM
							| EAngelscriptJITRequiredEntryPoints::Raw;
						Match = FAngelscriptJITProviderMatcher::
							MatchPreferredThenFallback(
								*ProviderSnapshot,
								Input,
								VMRawFallbackInput,
								*ReferenceBuild.Resolver);
					}
					else
					{
						Match = FAngelscriptJITProviderMatcher::Match(
							*ProviderSnapshot,
							Input,
							*ReferenceBuild.Resolver);
					}
					Route.Candidates.Reserve(Match.Candidates.Num());
					for (const FAngelscriptJITArtifactMatchCandidate& Candidate
						: Match.Candidates)
					{
						FRouteCandidateRecord& CandidateRecord =
							Route.Candidates.AddDefaulted_GetRef();
						CandidateRecord.ProviderId = Candidate.ProviderId;
						CandidateRecord.ProviderGeneration =
							Candidate.ProviderGeneration;
						CandidateRecord.ProviderEntryIndex =
							Candidate.EntryIndex;
						CandidateRecord.MatchResult = Candidate.Result;

						for (FProviderRecord& Provider : Snapshot.Providers)
						{
							if (Provider.ProviderId == Candidate.ProviderId
								&& Provider.ProviderGeneration
									== Candidate.ProviderGeneration)
							{
								if (Candidate.Result
									== EAngelscriptArtifactMatchResult::Exact)
								{
									++Provider.CompatibleEntryCount;
									if (Match.Result ==
										EAngelscriptArtifactMatchResult::
											AmbiguousExactProvider)
									{
										++Provider.ConflictingEntryCount;
									}
								}
								else
								{
									++Provider.RejectedEntryCount;
								}
								break;
							}
						}
					}
				}
				if (CurrentRoute.Function == nullptr)
				{
					continue;
				}
				const asSJITFunctionBinding Binding =
					CurrentRoute.Function->GetJITBinding();
				Route.bHasVMEntry = Binding.VMEntry != nullptr;
				Route.bHasRawEntry = Binding.RawEntry != nullptr;
				Route.bHasParmsEntry = Binding.ParmsEntry != nullptr;
				const FAngelscriptJITBindingContext* const Context =
					FAngelscriptJITBindingContext::FromBindingUserData(
						Binding.UserData);
				if (Context == nullptr || !Context->GetProvider().IsValid())
				{
					continue;
				}
				Route.ProviderId = Context->GetProvider()->ProviderId;
				Route.ProviderGeneration =
					Context->GetProvider()->ProviderGeneration;
				Route.ProviderEntryIndex = Context->GetEntryIndex();
				Route.bImmutableCookedDirectDispatch =
					Context->IsImmutableCookedDirectDispatch();
				if (Context->GetProvider()->Entries.IsValidIndex(
					Route.ProviderEntryIndex))
				{
					Route.ReferenceSlotCount = static_cast<uint32>(
						Context->GetProvider()->Entries[
							Route.ProviderEntryIndex].ReferenceSlots.Num());
				}
				if (const FAngelscriptJITResolvedReferenceTable* const Table =
					Context->GetReferenceTable())
				{
					Route.ResolvedReferenceSlotCount =
						static_cast<uint32>(Table->Slots.Num());
					for (FProviderRecord& Provider : Snapshot.Providers)
					{
						if (!(Provider.ProviderId == Route.ProviderId)
							|| !(Provider.ProviderGeneration
								== Route.ProviderGeneration)
							|| !Provider.Entries.IsValidIndex(
								Route.ProviderEntryIndex))
						{
							continue;
						}
						for (FReferenceRecord& Reference
							: Provider.Entries[
								Route.ProviderEntryIndex].References)
						{
							const FAngelscriptJITResolvedReferenceSlot* const Slot =
								Table->Slots.FindByPredicate(
								[&Reference](
									const FAngelscriptJITResolvedReferenceSlot& Slot)
								{
									return Slot.SlotIndex == Reference.SlotIndex;
								});
							if (Slot == nullptr || Slot->Value == nullptr)
							{
								continue;
							}
							if (!ReferenceBuild.IsUsable())
							{
								Reference.AdoptionState =
									EReferenceAdoptionState::Stale;
								continue;
							}
							const FAngelscriptJITReferenceResolution Resolution =
								ReferenceBuild.Resolver->Resolve(Reference.Reference);
							Reference.CurrentResolution = Resolution.Code;
							Reference.bResolved =
								Resolution.Code
									== EAngelscriptJITReferenceResolutionCode::Exact
								&& Resolution.Value != nullptr;
							Reference.AdoptionState = Reference.bResolved
								&& Resolution.Value == Slot->Value
								? EReferenceAdoptionState::Bound
								: EReferenceAdoptionState::Stale;
						}
						break;
					}
				}
				Route.VMExecutionCount = Context->GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::VM);
				Route.RawExecutionCount = Context->GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::Raw);
				Route.ParmsExecutionCount = Context->GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::Parms);
			}
			Snapshot.Routes.Sort([](const FRouteRecord& Left,
				const FRouteRecord& Right)
			{
				if (!(Left.ModuleKey == Right.ModuleKey))
				{
					return Left.ModuleKey.Hash < Right.ModuleKey.Hash;
				}
				return Left.Identity.FunctionKey.Hash
					< Right.Identity.FunctionKey.Hash;
			});
		}
	}

	for (FProviderRecord& Provider : Snapshot.Providers)
	{
		for (FProviderEntryRecord& Entry : Provider.Entries)
		{
			SynchronizeNativeCallSiteState(Entry);
		}
	}

	return Snapshot;
}

FStaticJITDiagnostics::FJsonResult FStaticJITDiagnostics::CaptureJson(
	FAngelscriptEngine* Engine)
{
	return Serialize(CaptureSnapshot(Engine));
}

FStaticJITDiagnostics::FJsonResult FStaticJITDiagnostics::Serialize(
	const FSnapshot& Snapshot)
{
	FJsonResult Result;
	Result.Json = SerializeSnapshot(BuildOrderedSnapshot(Snapshot));
	if (Result.Json.IsEmpty())
	{
		Result.Detail = TEXT("StaticJIT diagnostics JSON serialization failed.");
	}
	return Result;
}

bool FStaticJITDiagnostics::WriteJson(
	const FString& Filename,
	FAngelscriptEngine* Engine,
	FString* OutError)
{
	if (Filename.IsEmpty())
	{
		SetError(OutError,
			TEXT("StaticJIT diagnostics output filename must not be empty."));
		return false;
	}

	const FJsonResult Result = CaptureJson(Engine);
	if (!Result.IsSuccess())
	{
		SetError(OutError, Result.Detail);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
		Result.Json,
		*Filename,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		SetError(OutError, FString::Printf(
			TEXT("StaticJIT diagnostics failed to write JSON to '%s'."),
			*Filename));
		return false;
	}

	return true;
}

void FStaticJITDiagnostics::DumpDiagnostics(const TArray<FString>& Args)
{
	FString FunctionQuery;
	FString OutputFilename;
	bool bInvalidArguments = false;
	for (const FString& Arg : Args)
	{
		if (Arg.StartsWith(TEXT("-Function="), ESearchCase::IgnoreCase))
		{
			if (!FunctionQuery.IsEmpty())
			{
				bInvalidArguments = true;
				break;
			}
			FunctionQuery = Arg.RightChop(10);
		}
		else if (Arg.StartsWith(TEXT("-Output="), ESearchCase::IgnoreCase))
		{
			if (!OutputFilename.IsEmpty())
			{
				bInvalidArguments = true;
				break;
			}
			OutputFilename = Arg.RightChop(8);
		}
		else if (!Arg.StartsWith(TEXT("-"))
			&& OutputFilename.IsEmpty()
			&& Arg.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase))
		{
			OutputFilename = Arg;
		}
		else if (!Arg.StartsWith(TEXT("-"))
			&& FunctionQuery.IsEmpty())
		{
			FunctionQuery = Arg;
		}
		else
		{
			bInvalidArguments = true;
			break;
		}
	}
	FunctionQuery.TrimStartAndEndInline();
	OutputFilename.TrimStartAndEndInline();
	if (bInvalidArguments
		|| (Args.ContainsByPredicate([](const FString& Arg)
		{
			return Arg.StartsWith(TEXT("-Function="),
				ESearchCase::IgnoreCase);
		}) && FunctionQuery.IsEmpty())
		|| (Args.ContainsByPredicate([](const FString& Arg)
		{
			return Arg.StartsWith(TEXT("-Output="),
				ESearchCase::IgnoreCase);
		}) && OutputFilename.IsEmpty()))
	{
		UE_LOG(LogStaticJITDiagnostics, Error,
			TEXT("Usage: as.StaticJIT.DumpDiagnostics [-Function=<canonical-declaration-or-stable-key>] [-Output=<filename>]"));
		return;
	}

	FAngelscriptEngine* const CurrentEngine =
		FAngelscriptEngine::TryGetCurrentEngine();
	FSnapshot Snapshot = CaptureSnapshot(CurrentEngine);
	if (!FunctionQuery.IsEmpty())
	{
		Snapshot = FilterSnapshot(Snapshot, FunctionQuery);
	}
	const FJsonResult Result = Serialize(Snapshot);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogStaticJITDiagnostics, Error, TEXT("%s"), *Result.Detail);
		return;
	}

	uint32 ModuleCount = 0;
	uint32 EntryCount = 0;
	for (const FProviderRecord& Provider : Snapshot.Providers)
	{
		ModuleCount += static_cast<uint32>(Provider.Modules.Num());
		EntryCount += static_cast<uint32>(Provider.Entries.Num());
	}
	UE_LOG(LogStaticJITDiagnostics, Log,
		TEXT("StaticJIT diagnostics: schema=%u abi=%u engine=%s providers=%d modules=%u entries=%u routes=%d native=%u vm=%u query='%s' matched=%s"),
		Snapshot.SchemaVersion,
		Snapshot.ProviderAbiRevision,
		Snapshot.bHasCurrentEngine ? TEXT("yes") : TEXT("no"),
		Snapshot.Providers.Num(),
		ModuleCount,
		EntryCount,
		Snapshot.Routes.Num(),
		Snapshot.NativeRouteCount,
		Snapshot.VmRouteCount,
		*Snapshot.Query,
		Snapshot.bQueryMatched ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogStaticJITDiagnostics, Log,
		TEXT("StaticJIT diagnostics JSON: %s"), *Result.Json);
	if (!FunctionQuery.IsEmpty() && !Snapshot.bQueryMatched)
	{
		UE_LOG(LogStaticJITDiagnostics, Warning,
			TEXT("StaticJIT diagnostics could not resolve function query '%s'."),
			*FunctionQuery);
	}
	if (OutputFilename.IsEmpty())
	{
		return;
	}

	if (!FFileHelper::SaveStringToFile(
		Result.Json,
		*OutputFilename,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogStaticJITDiagnostics, Error,
			TEXT("StaticJIT diagnostics failed to write JSON to '%s'."),
			*OutputFilename);
		return;
	}

	UE_LOG(LogStaticJITDiagnostics, Log,
		TEXT("StaticJIT diagnostics JSON written to '%s'."),
		*OutputFilename);
}
