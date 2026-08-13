#include "StaticJIT/StaticJITDiagnostics.h"

#include "StaticJIT/AngelscriptJITBindingContext.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITProviderMatcher.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptJITProviderRouter.h"
#include "StaticJIT/AngelscriptJITReferenceResolver.h"
#include "StaticJIT/AngelscriptStaticJIT.h"

#include "Artifacts/AngelscriptArtifactIdentity.h"

#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "Misc/FileHelper.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

#include "StartAngelscriptHeaders.h"
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
			TEXT("Angelscript.StaticJIT.Diagnostics.ModuleArtifact.v1"));
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
		for (FStaticJITDiagnostics::FProviderRecord& Provider
			: Ordered.Providers)
		{
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

FStaticJITDiagnostics::FSnapshot FStaticJITDiagnostics::CaptureSnapshot(FAngelscriptEngine* Engine)
{
	FAngelscriptEngine* CurrentEngine = Engine != nullptr ? Engine : FAngelscriptEngine::TryGetCurrentEngine();

	FSnapshot Snapshot;
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
					const FAngelscriptJITFunctionMatchResult Match =
						FAngelscriptJITProviderMatcher::Match(
							*ProviderSnapshot,
							Input,
							*ReferenceBuild.Resolver);
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
							Reference.bResolved = Table->Slots.ContainsByPredicate(
								[&Reference](
									const FAngelscriptJITResolvedReferenceSlot& Slot)
								{
									return Slot.SlotIndex == Reference.SlotIndex;
								});
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
