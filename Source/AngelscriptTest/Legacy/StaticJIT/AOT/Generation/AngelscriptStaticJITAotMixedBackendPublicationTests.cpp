#include "CQTest.h"

#include "AngelscriptTestJITProbes.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS

namespace AngelscriptStaticJITAotMixedBackendPublicationTests_Private
{
	const FAngelscriptJITProviderCatalogEntry* FindEntry(
		const FAngelscriptJITProviderCatalog& Provider,
		const FAngelscriptStableFunctionKey& FunctionKey)
	{
		return Provider.Entries.FindByPredicate(
			[&FunctionKey](const FAngelscriptJITProviderCatalogEntry& Entry)
			{
				return Entry.FunctionKey == FunctionKey;
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotMixedBackendPublicationTests,
	"Angelscript.TestModule.StaticJIT.AOT.Generation.MixedBackendPublication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OneProviderModulePublishesTypedAndBytecodeEntries)
	{
		using namespace AngelscriptStaticJITAotMixedBackendPublicationTests_Private;

		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Snapshot =
			FAngelscriptJITProviderRegistry::Get().GetSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		if (!Snapshot.IsValid())
		{
			return;
		}

		const FAngelscriptJITProviderId& TestProviderId =
			AngelscriptTestJITProbes::GetStableProviderId();
		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>* ProviderPtr =
			Snapshot->Providers.FindByPredicate(
				[&TestProviderId](const TSharedPtr<
					const FAngelscriptJITProviderCatalog,
					ESPMode::ThreadSafe>& Candidate)
				{
					return Candidate.IsValid()
						&& Candidate->ProviderId == TestProviderId;
				});
		ASSERT_THAT(IsNotNull(ProviderPtr,
			TEXT("The installed AngelscriptTestJIT provider must be present")));
		if (ProviderPtr == nullptr || !ProviderPtr->IsValid())
		{
			return;
		}

		const FAngelscriptJITProviderCatalog& Provider = *ProviderPtr->Get();
		ASSERT_THAT(AreEqual(8u, FAngelscriptJITProviderAbi::Revision));
		ASSERT_THAT(AreEqual(FString(TEXT("AngelscriptTestJIT")),
			Provider.OwnerModuleName));
		ASSERT_THAT(IsTrue(Provider.ProviderId.IsValid()));
		ASSERT_THAT(IsTrue(Provider.ProviderGeneration.IsValid()));
		ASSERT_THAT(IsTrue(!Provider.ArtifactSetDigest.IsZero()));
		ASSERT_THAT(IsTrue(!Provider.ArtifactProfile.Hash.IsZero()));
		ASSERT_THAT(IsTrue(!Provider.NativeEnvironmentFingerprint.IsZero()));
		ASSERT_THAT(IsTrue(Provider.Diagnostics.IsSet(),
			TEXT("EditorDevelopment providers must retain backend diagnostics")));
		if (!Provider.Diagnostics.IsSet())
		{
			return;
		}

		const FAngelscriptJITProviderCatalogDiagnostics& Diagnostics =
			Provider.Diagnostics.GetValue();
		ASSERT_THAT(AreEqual(FString(TEXT("typed-ast")),
			Diagnostics.RequestedBackendId));

		int32 MissingEntryCount = 0;
		int32 DuplicateEntryCount = 0;
		for (const FAngelscriptJITProviderCatalogFunctionDiagnostic& Function :
			Diagnostics.Functions)
		{
			int32 MatchCount = 0;
			for (const FAngelscriptJITProviderCatalogEntry& Entry : Provider.Entries)
			{
				MatchCount += Entry.FunctionKey == Function.FunctionKey ? 1 : 0;
			}
			MissingEntryCount += MatchCount == 0 ? 1 : 0;
			DuplicateEntryCount += MatchCount > 1 ? 1 : 0;
		}
		ASSERT_THAT(AreEqual(0, MissingEntryCount,
			TEXT("Every backend diagnostic must name one published function entry")));
		ASSERT_THAT(AreEqual(0, DuplicateEntryCount,
			TEXT("Backend diagnostics must not create a second identity namespace")));

		const FAngelscriptJITProviderCatalogEntry* TypedEntry = nullptr;
		const FAngelscriptJITProviderCatalogEntry* BytecodeEntry = nullptr;
		for (const FAngelscriptJITProviderCatalogFunctionDiagnostic& TypedFunction :
			Diagnostics.Functions)
		{
			if (TypedFunction.ActualBackendId != TEXT("typed-ast"))
			{
				continue;
			}

			const FAngelscriptJITProviderCatalogEntry* CandidateTyped =
				FindEntry(Provider, TypedFunction.FunctionKey);
			if (CandidateTyped == nullptr
				|| CandidateTyped->EntryPoints.VMEntry == nullptr
				|| CandidateTyped->EntryPoints.RawEntry == nullptr
				|| CandidateTyped->EntryPoints.ParmsEntry == nullptr)
			{
				continue;
			}

			for (const FAngelscriptJITProviderCatalogFunctionDiagnostic&
				BytecodeFunction : Diagnostics.Functions)
			{
				if (BytecodeFunction.ActualBackendId != TEXT("bytecode"))
				{
					continue;
				}

				const FAngelscriptJITProviderCatalogEntry* CandidateBytecode =
					FindEntry(Provider, BytecodeFunction.FunctionKey);
				if (CandidateBytecode != nullptr
					&& CandidateBytecode->EntryPoints.VMEntry != nullptr
					&& CandidateBytecode->ModuleKey == CandidateTyped->ModuleKey)
				{
					TypedEntry = CandidateTyped;
					BytecodeEntry = CandidateBytecode;
					break;
				}
			}

			if (TypedEntry != nullptr)
			{
				break;
			}
		}

		ASSERT_THAT(IsNotNull(TypedEntry,
			TEXT("One AS module must contain a TypedASTJIT function with VM, Raw and Parms entries")));
		ASSERT_THAT(IsNotNull(BytecodeEntry,
			TEXT("That same AS module must retain a per-function BytecodeJIT VM fallback")));
		if (TypedEntry == nullptr || BytecodeEntry == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(TypedEntry->ModuleKey == BytecodeEntry->ModuleKey));
		ASSERT_THAT(IsTrue(TypedEntry->FunctionKey != BytecodeEntry->FunctionKey));
		ASSERT_THAT(IsTrue(TypedEntry->ArtifactProfile.Hash
			== Provider.ArtifactProfile.Hash));
		ASSERT_THAT(IsTrue(BytecodeEntry->ArtifactProfile.Hash
			== Provider.ArtifactProfile.Hash));
		ASSERT_THAT(IsTrue(TypedEntry->NativeEnvironmentFingerprint
			== Provider.NativeEnvironmentFingerprint));
		ASSERT_THAT(IsTrue(BytecodeEntry->NativeEnvironmentFingerprint
			== Provider.NativeEnvironmentFingerprint));
		ASSERT_THAT(IsNotNull(
			reinterpret_cast<const void*>(TypedEntry->EntryPoints.VMEntry)));
		ASSERT_THAT(IsNotNull(
			reinterpret_cast<const void*>(TypedEntry->EntryPoints.RawEntry)));
		ASSERT_THAT(IsNotNull(
			reinterpret_cast<const void*>(TypedEntry->EntryPoints.ParmsEntry)));
		ASSERT_THAT(IsNotNull(
			reinterpret_cast<const void*>(BytecodeEntry->EntryPoints.VMEntry)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS
