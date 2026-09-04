#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheRestore.h"
#include "Cache/AngelscriptCacheService.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheMultiModuleGenerationTests_Private
{
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheMultiModuleGeneration"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheMultiModuleGeneration/")));
			ScriptRoot = Root / TEXT("Script");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheMultiModuleGeneration/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(
			const FString& RelativePath,
			const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source, *(ScriptRoot / RelativePath));
		}

		FString Root;
		FString ScriptRoot;
	};

	static TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FString& ProjectRoot)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.bOverrideCacheV2Enablement = true;
		Config.bEnableCacheV2 = true;
		Config.CacheV2RootOverride = ProjectRoot / TEXT("CacheV2");

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [ProjectRoot]()
		{
			return ProjectRoot;
		};
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	}

	class FPackSource final : public IAngelscriptCachePackSource
	{
	public:
		explicit FPackSource(
			const TConstArrayView<FAngelscriptEncodedPack> InPacks)
			: Packs(InPacks)
		{
		}

		virtual bool TryGetCompletePack(
			const FAngelscriptHash256& PackId,
			TConstArrayView<uint8>& OutBytes) override
		{
			for (const FAngelscriptEncodedPack& Pack : Packs)
			{
				if (Pack.PackId == PackId)
				{
					OutBytes = Pack.Bytes;
					return true;
				}
			}
			OutBytes = {};
			return false;
		}

	private:
		TConstArrayView<FAngelscriptEncodedPack> Packs;
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheMultiModuleGenerationTests,
	"Angelscript.TestModule.Cache.MultiModuleGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ProductionPublicationBuildsOneDeduplicatedCompleteGeneration)
	{
		using namespace AngelscriptCacheMultiModuleGenerationTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString First = FString::Printf(TEXT(R"AS(
enum ECacheMultiFirst%sState
{
	Ready = 1,
}

int ReadCacheMultiFirst%sValue()
{
	return 101;
}
)AS"), *Unique, *Unique);
		const FString Second = FString::Printf(TEXT(R"AS(
enum ECacheMultiSecond%sState
{
	Ready = 1,
}

int ReadCacheMultiSecond%sValue()
{
	return 102;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("First.as"), First)));
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("Second.as"), Second)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project.Root);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 64);
		Engine->InitialCompile();
		const FAngelscriptCacheLifecyclePublications Publications =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		ASSERT_THAT(AreEqual(2, Publications.Current->Modules.Num()));

		FAngelscriptCacheCleanCaptureOptions Options;
		Options.Compatibility = Publications.Current->Compatibility;
		Options.Context = Publications.Current->Context;
		Options.Profile = Publications.Current->Profile;
		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::Auto;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCachePreparedColdGeneration Prepared;
		const FAngelscriptCacheCleanCaptureResult Preparation =
			PrepareAngelscriptCacheColdGeneration(
				Publications.Current->Modules,
				Options,
				PackPolicy,
				Codec,
				Prepared);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.3 multi-module generation: Error=%u Modules=%d Records=%d Packs=%d Generation=%s Detail=%s"),
			static_cast<uint32>(Preparation.Error),
			Prepared.Manifest.ModuleSnapshots.Num(),
			Prepared.Manifest.Records.Num(), Prepared.Packs.Num(),
			*Prepared.EncodedManifest.ComputedGenerationId.ToHexString(),
			*Preparation.Detail));
		ASSERT_THAT(IsTrue(Preparation.IsSuccess()));
		ASSERT_THAT(AreEqual(2, Prepared.Manifest.ModuleSnapshots.Num()));
		ASSERT_THAT(AreEqual(13, Prepared.Manifest.Records.Num()));
		ASSERT_THAT(IsTrue(
			Prepared.Manifest.SourceSnapshot
				== Publications.Current->SourceSnapshot));

		int32 SourceIndexCount = 0;
		for (const FAngelscriptCacheRecordIndexEntry& Entry
			: Prepared.Manifest.Records)
		{
			SourceIndexCount += Entry.RecordId.Kind
				== EAngelscriptCacheRecordKind::SourceIndex;
		}
		ASSERT_THAT(AreEqual(1, SourceIndexCount));

		FPackSource Packs(Prepared.Packs);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Validated;
		const FAngelscriptCacheValidationResult Validation =
			ValidateAngelscriptCacheGeneration(
				Prepared.EncodedManifest.CompleteBytes,
				Prepared.EncodedManifest.ComputedGenerationId,
				Packs, Limits, Budget, Codec, Validated);
		ASSERT_THAT(IsTrue(Validation.IsSuccess()));
		ASSERT_THAT(IsTrue(Validated.IsSet()));
		ASSERT_THAT(AreEqual(2,
			Validated->Manifest.ModuleSnapshots.Num()));
		ASSERT_THAT(AreEqual(13,
			Validated->ReachableRecords.Num()));
	}

	TEST_METHOD(CrossModuleFunctionDependencyPublishesStableProviderReference)
	{
		using namespace AngelscriptCacheMultiModuleGenerationTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Provider = FString::Printf(TEXT(R"AS(
enum ECacheCrossProvider%sState
{
	Ready = 1,
}

int ReadCacheCrossProvider%sValue(int Value)
{
	return Value + 32;
}
)AS"), *Unique, *Unique);
		const FString Consumer = FString::Printf(TEXT(R"AS(
import int ReadCacheCrossProvider%sValue(int Value) from "Provider";

enum ECacheCrossConsumer%sState
{
	Ready = 1,
}

int ReadCacheCrossConsumer%sValue()
{
	return ReadCacheCrossProvider%sValue(10);
}
)AS"), *Unique, *Unique, *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("Provider.as"), Provider)));
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("Consumer.as"), Consumer)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project.Root);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 64);
		Engine->InitialCompile();

		const FAngelscriptCacheLifecyclePublications Publications =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Cache V2 cross-module publication: Modules=%d"),
			Publications.Current->Modules.Num()));
		ASSERT_THAT(AreEqual(2, Publications.Current->Modules.Num()));

		const FAngelscriptCacheCleanModuleArtifacts* ProviderArtifacts =
			Publications.Current->Modules.FindByPredicate(
				[](const FAngelscriptCacheCleanModuleArtifacts& Module)
				{
					return Module.CanonicalModuleName == TEXT("Provider");
				});
		const FAngelscriptCacheCleanModuleArtifacts* ConsumerArtifacts =
			Publications.Current->Modules.FindByPredicate(
				[](const FAngelscriptCacheCleanModuleArtifacts& Module)
				{
					return Module.CanonicalModuleName == TEXT("Consumer");
				});
		ASSERT_THAT(IsNotNull(ProviderArtifacts));
		ASSERT_THAT(IsNotNull(ConsumerArtifacts));
		ASSERT_THAT(AreEqual(1,
			ProviderArtifacts->ValidatedFunctionArtifactIdentities.Num()));
		ASSERT_THAT(AreEqual(1,
			ConsumerArtifacts->ValidatedFunctionArtifactReferenceSets.Num()));

		const FAngelscriptStableFunctionKey ProviderFunctionKey =
			ProviderArtifacts->ValidatedFunctionArtifactIdentities[0].FunctionKey;
		const FAngelscriptFunctionArtifactReferenceSet& ConsumerReferences =
			ConsumerArtifacts->ValidatedFunctionArtifactReferenceSets[0];
		const FAngelscriptArtifactReference* ProviderReference =
			ConsumerReferences.RequiredReferences.FindByPredicate(
				[&ProviderFunctionKey](
					const FAngelscriptArtifactReference& Reference)
				{
					return Reference.Kind
							== EAngelscriptArtifactReferenceKind::ScriptFunction
						&& Reference.StableKey == ProviderFunctionKey.Hash;
				});
		ASSERT_THAT(IsNotNull(ProviderReference));
		ASSERT_THAT(IsFalse(ProviderReference->ExpectedAbi.IsZero()));
	}

	TEST_METHOD(ProviderBodyReloadCarriesUnchangedAutomaticImportConsumer)
	{
		using namespace AngelscriptCacheMultiModuleGenerationTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString FunctionName = FString::Printf(
			TEXT("ReadCacheReloadProvider%sValue"), *Unique);
		const FString ConsumerName = FString::Printf(
			TEXT("ReadCacheReloadConsumer%sValue"), *Unique);
		const FString ProviderV1 = FString::Printf(TEXT(R"AS(
int %s(int Value)
{
	return Value + 32;
}
)AS"), *FunctionName);
		const FString ProviderV2 = FString::Printf(TEXT(R"AS(
int %s(int Value)
{
	return Value + 333;
}
)AS"), *FunctionName);
		const FString Consumer = FString::Printf(TEXT(R"AS(
int %s()
{
	return %s(10);
}
)AS"), *ConsumerName, *FunctionName);
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("ReloadProvider.as"), ProviderV1)));
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("ReloadConsumer.as"), Consumer)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project.Root);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Engine->InitialCompile();

		const FAngelscriptCacheLifecyclePublications Initial =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Initial.Current.IsValid()));
		ASSERT_THAT(AreEqual(2, Initial.Current->Modules.Num()));
		const FAngelscriptCacheCleanModuleArtifacts* InitialProvider =
			Initial.Current->Modules.FindByPredicate(
				[](const FAngelscriptCacheCleanModuleArtifacts& Module)
				{
					return Module.CanonicalModuleName
						== TEXT("ReloadProvider");
				});
		const FAngelscriptCacheCleanModuleArtifacts* InitialConsumer =
			Initial.Current->Modules.FindByPredicate(
				[](const FAngelscriptCacheCleanModuleArtifacts& Module)
				{
					return Module.CanonicalModuleName
						== TEXT("ReloadConsumer");
				});
		ASSERT_THAT(IsNotNull(InitialProvider));
		ASSERT_THAT(IsNotNull(InitialConsumer));
		if (InitialProvider == nullptr || InitialConsumer == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			1, InitialProvider->ValidatedFunctionArtifactIdentities.Num()));
		ASSERT_THAT(AreEqual(
			1, InitialConsumer->ValidatedFunctionArtifactIdentities.Num()));
		const FAngelscriptFunctionArtifactIdentity InitialProviderIdentity =
			InitialProvider->ValidatedFunctionArtifactIdentities[0];
		const FAngelscriptFunctionArtifactIdentity InitialConsumerIdentity =
			InitialConsumer->ValidatedFunctionArtifactIdentities[0];

		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("ReloadProvider.as"), ProviderV2)));
		const FAngelscriptCacheForceCleanApiResult Reload =
			ForceCleanAngelscriptCache(Engine.Get(), TEXT("ReloadProvider"));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Cross-module provider reload: Error=%u Outcome=%u Modules=%s Detail=%s"),
			static_cast<uint32>(Reload.Error),
			static_cast<uint32>(Reload.Outcome),
			*FString::Join(Reload.SelectedModuleNames, TEXT(",")),
			*Reload.Detail));
		ASSERT_THAT(IsTrue(Reload.IsSuccess()));

		const FAngelscriptCacheLifecyclePublications Reloaded =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Reloaded.Current.IsValid()));
		ASSERT_THAT(AreEqual(2, Reloaded.Current->Modules.Num()));
		const FAngelscriptCacheCleanModuleArtifacts* ReloadedProvider =
			Reloaded.Current->Modules.FindByPredicate(
				[](const FAngelscriptCacheCleanModuleArtifacts& Module)
				{
					return Module.CanonicalModuleName
						== TEXT("ReloadProvider");
				});
		const FAngelscriptCacheCleanModuleArtifacts* ReloadedConsumer =
			Reloaded.Current->Modules.FindByPredicate(
				[](const FAngelscriptCacheCleanModuleArtifacts& Module)
				{
					return Module.CanonicalModuleName
						== TEXT("ReloadConsumer");
				});
		ASSERT_THAT(IsNotNull(ReloadedProvider));
		ASSERT_THAT(IsNotNull(ReloadedConsumer));
		if (ReloadedProvider == nullptr || ReloadedConsumer == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			1, ReloadedProvider->ValidatedFunctionArtifactIdentities.Num()));
		ASSERT_THAT(AreEqual(
			1, ReloadedConsumer->ValidatedFunctionArtifactIdentities.Num()));
		const FAngelscriptFunctionArtifactIdentity& ReloadedProviderIdentity =
			ReloadedProvider->ValidatedFunctionArtifactIdentities[0];
		const FAngelscriptFunctionArtifactIdentity& ReloadedConsumerIdentity =
			ReloadedConsumer->ValidatedFunctionArtifactIdentities[0];
		ASSERT_THAT(IsTrue(
			ReloadedProviderIdentity.FunctionKey
				== InitialProviderIdentity.FunctionKey));
		ASSERT_THAT(IsFalse(
			ReloadedProviderIdentity.Content.Execution
				== InitialProviderIdentity.Content.Execution));
		ASSERT_THAT(IsTrue(
			ReloadedConsumerIdentity.FunctionKey
				== InitialConsumerIdentity.FunctionKey));
		ASSERT_THAT(IsTrue(
			ReloadedConsumerIdentity.Content.Execution
				== InitialConsumerIdentity.Content.Execution));
		ASSERT_THAT(IsTrue(
			ReloadedConsumerIdentity.Content.Debug
				== InitialConsumerIdentity.Content.Debug));
	}

	TEST_METHOD(CrossModuleFunctionDependencyRestoresAndExecutesInFreshEngine)
	{
		using namespace AngelscriptCacheMultiModuleGenerationTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Provider = FString::Printf(TEXT(R"AS(
enum ECacheCrossRestoreProvider%sState
{
	Ready = 1,
}

int ReadCacheCrossRestoreProvider%sValue(int Value)
{
	return Value + 32;
}
)AS"), *Unique, *Unique);
		const FString ConsumerSource = FString::Printf(TEXT(R"AS(
import int ReadCacheCrossRestoreProvider%sValue(int Value) from "Provider";

enum ECacheCrossRestoreConsumer%sState
{
	Ready = 1,
}

int ReadCacheCrossRestoreConsumer%sValue()
{
	return ReadCacheCrossRestoreProvider%sValue(10);
}
)AS"), *Unique, *Unique, *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("Provider.as"), Provider)));
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("Consumer.as"), ConsumerSource)));

		FAngelscriptCachePreparedColdGeneration Prepared;
		FAngelscriptStableModuleKey ProviderKey;
		FAngelscriptStableModuleKey ConsumerKey;
		{
			TUniquePtr<FAngelscriptEngine> Producer = CreateEngine(Project.Root);
			ASSERT_THAT(IsNotNull(Producer.Get()));
			FAngelscriptEngineScope EngineScope(*Producer);
			FAngelscriptCacheService* Service = Producer->GetCacheService();
			ASSERT_THAT(IsNotNull(Service));
			Producer->InitialCompile();
			const FAngelscriptCacheLifecyclePublications Publications =
				Service->GetLifecyclePublications();
			ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
			ASSERT_THAT(AreEqual(2, Publications.Current->Modules.Num()));
			const FAngelscriptCacheCleanModuleArtifacts* ProviderArtifacts =
				Publications.Current->Modules.FindByPredicate(
					[](const FAngelscriptCacheCleanModuleArtifacts& Module)
					{
						return Module.CanonicalModuleName == TEXT("Provider");
					});
			const FAngelscriptCacheCleanModuleArtifacts* ConsumerArtifacts =
				Publications.Current->Modules.FindByPredicate(
					[](const FAngelscriptCacheCleanModuleArtifacts& Module)
					{
						return Module.CanonicalModuleName == TEXT("Consumer");
					});
			ASSERT_THAT(IsNotNull(ProviderArtifacts));
			ASSERT_THAT(IsNotNull(ConsumerArtifacts));
			ProviderKey = ProviderArtifacts->ModuleKey;
			ConsumerKey = ConsumerArtifacts->ModuleKey;

			FAngelscriptCacheCleanCaptureOptions Options;
			Options.Compatibility = Publications.Current->Compatibility;
			Options.Context = Publications.Current->Context;
			Options.Profile = Publications.Current->Profile;
			FAngelscriptCachePackPolicy PackPolicy;
			PackPolicy.CompressionPolicy =
				EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
			FAngelscriptUnrealZlibCacheStorageCodec Codec;
			const FAngelscriptCacheCleanCaptureResult Preparation =
				PrepareAngelscriptCacheColdGeneration(
					Publications.Current->Modules,
					Options,
					PackPolicy,
					Codec,
					Prepared);
			ASSERT_THAT(IsTrue(Preparation.IsSuccess()));
		}

		// No producer Engine or live function pointer survives this boundary.
		FPackSource Packs(Prepared.Packs);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TOptional<FAngelscriptValidatedGeneration> Validated;
		ASSERT_THAT(IsTrue(ValidateAngelscriptCacheGeneration(
			Prepared.EncodedManifest.CompleteBytes,
			Prepared.EncodedManifest.ComputedGenerationId,
			Packs,
			Limits,
			Budget,
			Codec,
			Validated).IsSuccess()));
		ASSERT_THAT(IsTrue(Validated.IsSet()));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		const TArray<FAngelscriptStableModuleKey> ModuleKeys = {
			ProviderKey,
			ConsumerKey,
		};
		const FAngelscriptCacheRestoreResult Restore =
			RestoreAngelscriptCacheModules(
				Consumer.GetEngine(), Validated.GetValue(), ModuleKeys, Limits);
		TestRunner->AddInfo(FString::Printf(
			TEXT("Cache V2 fresh cross-module restore: Error=%u Stage=%u Modules=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Restore.RestoredModuleCount,
			Restore.RestoredFunctionCount,
			*Restore.Detail));
		ASSERT_THAT(IsTrue(Restore.IsSuccess()));

		const TSharedPtr<FAngelscriptModuleDesc> RestoredConsumer =
			Consumer.GetEngine().GetModuleByModuleName(TEXT("Consumer"));
		ASSERT_THAT(IsTrue(RestoredConsumer.IsValid()));
		ASSERT_THAT(IsNotNull(RestoredConsumer->ScriptModule));
		const FString ConsumerDeclaration = FString::Printf(
			TEXT("int ReadCacheCrossRestoreConsumer%sValue()"), *Unique);
		asIScriptFunction* ConsumerFunction =
			RestoredConsumer->ScriptModule->GetFunctionByDecl(
				TCHAR_TO_UTF8(*ConsumerDeclaration));
		ASSERT_THAT(IsNotNull(ConsumerFunction));
		int32 Result = 0;
		ASSERT_THAT(IsTrue(Consumer.ExecuteInt(*ConsumerFunction, Result)));
		ASSERT_THAT(AreEqual(42, Result));
	}

	TEST_METHOD(UnsupportedModuleDoesNotDiscardEligibleModulePublication)
	{
		using namespace AngelscriptCacheMultiModuleGenerationTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString First = FString::Printf(TEXT(R"AS(
enum ECachePartialFirst%sState
{
	Ready = 1,
}

int ReadCachePartialFirst%sValue()
{
	return 201;
}
)AS"), *Unique, *Unique);
		const FString Unsupported = FString::Printf(TEXT(R"AS(
enum ECachePartialUnsupported%sFirst
{
	Ready = 1,
}

enum ECachePartialUnsupported%sSecond
{
	Ready = 2,
}

int ReadCachePartialUnsupported%sValue()
{
	return 202;
}
)AS"), *Unique, *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("First.as"), First)));
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("Unsupported.as"), Unsupported)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project.Root);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 64);
		Engine->InitialCompile();

		const TSharedPtr<FAngelscriptModuleDesc> FirstModule =
			Engine->GetModuleByModuleName(TEXT("First"));
		const TSharedPtr<FAngelscriptModuleDesc> UnsupportedModule =
			Engine->GetModuleByModuleName(TEXT("Unsupported"));
		ASSERT_THAT(IsTrue(FirstModule.IsValid()));
		ASSERT_THAT(IsTrue(UnsupportedModule.IsValid()));

		const FAngelscriptCacheLifecyclePublications Publications =
			Service->GetLifecyclePublications();
		TestRunner->AddInfo(FString::Printf(
			TEXT("Cache V2 partial capture: Current=%d Modules=%d FirstActive=%d UnsupportedActive=%d"),
			Publications.Current.IsValid() ? 1 : 0,
			Publications.Current.IsValid()
				? Publications.Current->Modules.Num() : 0,
			FirstModule.IsValid() ? 1 : 0,
			UnsupportedModule.IsValid() ? 1 : 0));
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		ASSERT_THAT(AreEqual(1, Publications.Current->Modules.Num()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("First")),
			Publications.Current->Modules[0].CanonicalModuleName));

		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service->CaptureDecisionTrace();
		const FAngelscriptCacheDecisionEvent* SkipEvent = Trace.Events.FindByPredicate(
			[](const FAngelscriptCacheDecisionEvent& Event)
			{
				return Event.Outcome
						== EAngelscriptCacheDecisionOutcome::NotCacheable
					&& Event.ReasonDomain
						== EAngelscriptCacheDecisionReasonDomain::CleanCapture
					&& Event.ReasonCode == static_cast<uint32>(
						EAngelscriptCacheCleanCaptureError::NotCacheable);
			});
		ASSERT_THAT(IsNotNull(SkipEvent));
		ASSERT_THAT(AreEqual(1, SkipEvent->ModuleKeys.Num()));
		ASSERT_THAT(IsTrue(SkipEvent->Profile.Hash
			== Publications.Current->Profile.Hash));
		ASSERT_THAT(IsTrue(SkipEvent->SourceSnapshot
			== Publications.Current->SourceSnapshot));
	}
};

#endif
