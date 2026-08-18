#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheEnvironmentProfile.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptengine.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheEnvironmentTests_Private
{
	struct FRegisteredPropertyProbe
	{
		int32 Value = 0;
		int32 Padding = 0;
	};

	static int32 EnvironmentFunctionProbe(const int32 Value)
	{
		return Value;
	}

	static bool RegisterPropertyProbe(
		asIScriptEngine& Engine,
		const int32 PropertyOffset,
		asCObjectType*& OutType,
		asCObjectProperty*& OutProperty)
	{
		OutType = nullptr;
		OutProperty = nullptr;
		constexpr const char* TypeName = "FCacheV2EnvironmentPropertyProbe";
		if (Engine.RegisterObjectType(
				TypeName,
				sizeof(FRegisteredPropertyProbe),
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE) < 0
			|| Engine.RegisterObjectProperty(
				TypeName, "int Value", PropertyOffset) < 0)
		{
			return false;
		}

		OutType = static_cast<asCObjectType*>(
			Engine.GetTypeInfoByDecl(TypeName));
		if (OutType == nullptr || OutType->properties.GetLength() != 1)
		{
			return false;
		}
		OutProperty = OutType->properties[0];
		return OutProperty != nullptr;
	}

	static FAngelscriptCacheEnvironmentProfile BuildIdentity(
		FAutomationTestBase& Test,
		const FAngelscriptEngine& Engine,
		const FAngelscriptPreprocessorContext& PreprocessorContext,
		const TArray<FAngelscriptSourceRoot>& Roots)
	{
		FAngelscriptCacheEnvironmentProfile Identity;
		const FAngelscriptCacheEnvironmentProfileResult Result =
			BuildAngelscriptCacheEnvironmentProfile(
				Engine, PreprocessorContext, Roots, Identity);
		Test.AddInfo(FString::Printf(
			TEXT("V6.2 environment identity: Success=%d Compatibility=%s Context=%s Profile=%s CompileOptions=%d DiscoveryOptions=%d Detail=%s"),
			Result.IsSuccess() ? 1 : 0,
			*Identity.CaptureOptions.Compatibility.Hash.ToHexString(),
			*Identity.CaptureOptions.Context.Hash.ToHexString(),
			*Identity.CaptureOptions.Profile.Hash.ToHexString(),
			Identity.CaptureOptions.CanonicalCompileOptions.Num(),
			Identity.DiscoveryConfig.Options.Num(), *Result.Detail));
		if (!Result.IsSuccess())
		{
			Test.AddError(Result.Detail);
		}
		return Identity;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheEnvironmentTests,
	"Angelscript.TestModule.Cache.EnvironmentIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CurrentRegisteredFunctionCollectionExcludesRetainedHistoricalFunction)
	{
		using namespace AngelscriptCacheEnvironmentTests_Private;
		asIScriptEngine* const PublicEngine = asCreateScriptEngine();
		ASSERT_THAT(IsNotNull(PublicEngine));
		if (PublicEngine == nullptr)
		{
			return;
		}

		asIScriptFunction* RetainedRemovedFunction = nullptr;
		bool bRemovedFromCurrentRegistry = false;
		ON_SCOPE_EXIT
		{
			asCScriptEngine* const Engine =
				static_cast<asCScriptEngine*>(PublicEngine);
			if (bRemovedFromCurrentRegistry
				&& RetainedRemovedFunction != nullptr)
			{
				Engine->registeredGlobalFuncs.PushLast(
					static_cast<asCScriptFunction*>(RetainedRemovedFunction));
				Engine->registeredGlobalFuncTable.Add(
					static_cast<asCScriptFunction*>(RetainedRemovedFunction));
			}
			if (RetainedRemovedFunction != nullptr)
			{
				RetainedRemovedFunction->Release();
			}
			PublicEngine->ShutDownAndRelease();
		};

		const int32 CurrentFunctionId = PublicEngine->RegisterGlobalFunction(
			"int CurrentEnvironmentFunction(int)",
			asFUNCTION(EnvironmentFunctionProbe), asCALL_CDECL);
		ASSERT_THAT(IsTrue(CurrentFunctionId >= 0));
		const int32 RemovedFunctionId = PublicEngine->RegisterGlobalFunction(
			"int RemovedEnvironmentFunction(int)",
			asFUNCTION(EnvironmentFunctionProbe), asCALL_CDECL);
		ASSERT_THAT(IsTrue(RemovedFunctionId >= 0));
		RetainedRemovedFunction = PublicEngine->GetFunctionById(
			RemovedFunctionId);
		ASSERT_THAT(IsNotNull(RetainedRemovedFunction));
		if (RetainedRemovedFunction == nullptr)
		{
			return;
		}
		RetainedRemovedFunction->AddRef();

		asCScriptEngine* const Engine =
			static_cast<asCScriptEngine*>(PublicEngine);
		const int RegistryIndex = Engine->registeredGlobalFuncs.IndexOf(
			static_cast<asCScriptFunction*>(RetainedRemovedFunction));
		ASSERT_THAT(IsTrue(RegistryIndex >= 0));
		if (RegistryIndex < 0)
		{
			return;
		}
		Engine->registeredGlobalFuncs.RemoveIndex(RegistryIndex);
		Engine->registeredGlobalFuncTable.Remove(
			static_cast<asCScriptFunction*>(RetainedRemovedFunction));
		bRemovedFromCurrentRegistry = true;
		ASSERT_THAT(IsTrue(Engine->scriptFunctions[RemovedFunctionId]
			== RetainedRemovedFunction));

		TArray<asCScriptFunction*> CurrentFunctions;
		FAngelscriptCacheEnvironmentIdentity::CollectCurrentRegisteredFunctions(
			*Engine, CurrentFunctions);
		ASSERT_THAT(IsTrue(CurrentFunctions.Contains(
			static_cast<asCScriptFunction*>(
				PublicEngine->GetFunctionById(CurrentFunctionId)))));
		ASSERT_THAT(IsFalse(CurrentFunctions.Contains(
			static_cast<asCScriptFunction*>(RetainedRemovedFunction))));
	}

	TEST_METHOD(CurrentRegisteredFunctionCollectionIncludesEngineOwnedBuiltinBehaviours)
	{
		asIScriptEngine* const PublicEngine = asCreateScriptEngine();
		ASSERT_THAT(IsNotNull(PublicEngine));
		if (PublicEngine == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			PublicEngine->ShutDownAndRelease();
		};

		asCScriptEngine* const Engine =
			static_cast<asCScriptEngine*>(PublicEngine);
		asCScriptFunction* BuiltinBehaviour = nullptr;
		for (asUINT Index = 0;
			Index < Engine->scriptTypeBehaviours.GetMethodCount(); ++Index)
		{
			BuiltinBehaviour = static_cast<asCScriptFunction*>(
				Engine->scriptTypeBehaviours.GetMethodByIndex(Index));
			if (BuiltinBehaviour != nullptr)
			{
				break;
			}
		}
		ASSERT_THAT(IsNotNull(BuiltinBehaviour));
		if (BuiltinBehaviour == nullptr)
		{
			return;
		}

		TArray<asCScriptFunction*> CurrentFunctions;
		FAngelscriptCacheEnvironmentIdentity::CollectCurrentRegisteredFunctions(
			*Engine, CurrentFunctions);
		ASSERT_THAT(IsTrue(CurrentFunctions.Contains(BuiltinBehaviour)));
	}

	TEST_METHOD(IsStableAcrossEnginesAndHostRootRelocation)
	{
		using namespace AngelscriptCacheEnvironmentTests_Private;
		FAngelscriptTestFixture FirstFixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		FAngelscriptTestFixture SecondFixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(FirstFixture.IsValid()));
		ASSERT_THAT(IsTrue(SecondFixture.IsValid()));

		FAngelscriptPreprocessorContext Context =
			FAngelscriptPreprocessorContext::CreateFromCurrentEngineContext();
		const TArray<FAngelscriptSourceRoot> FirstRoots{
			FAngelscriptSourceRoot::FromGameRoot(
				TEXT("D:/Workspace/FirstProject/Script")),
			FAngelscriptSourceRoot::FromPluginRoot(
				TEXT("Inventory"),
				TEXT("D:/Workspace/FirstProject/Plugins/Inventory/Script")),
		};
		const TArray<FAngelscriptSourceRoot> RelocatedRoots{
			FAngelscriptSourceRoot::FromGameRoot(
				TEXT("E:/Relocated/SecondProject/Script")),
			FAngelscriptSourceRoot::FromPluginRoot(
				TEXT("Inventory"),
				TEXT("E:/Relocated/SecondProject/Plugins/Inventory/Script")),
		};

		const FAngelscriptCacheEnvironmentProfile First = BuildIdentity(
			*TestRunner, FirstFixture.GetEngine(), Context, FirstRoots);
		const FAngelscriptCacheEnvironmentProfile Second = BuildIdentity(
			*TestRunner, SecondFixture.GetEngine(), Context, RelocatedRoots);

		ASSERT_THAT(IsFalse(First.CaptureOptions.Profile.Hash.IsZero()));
		ASSERT_THAT(IsTrue(First.CaptureOptions.Compatibility.Hash
			== Second.CaptureOptions.Compatibility.Hash));
		ASSERT_THAT(IsTrue(First.CaptureOptions.Context.Hash
			== Second.CaptureOptions.Context.Hash));
		ASSERT_THAT(IsTrue(First.CaptureOptions.Profile.Hash
			== Second.CaptureOptions.Profile.Hash));
		ASSERT_THAT(AreEqual(
			First.CaptureOptions.CanonicalCompileOptions,
			Second.CaptureOptions.CanonicalCompileOptions));
		ASSERT_THAT(AreEqual(
			First.DiscoveryConfig.Options.Num(),
			Second.DiscoveryConfig.Options.Num()));
	}

	TEST_METHOD(ContextTracksEffectiveOptionsAndLogicalMountsOnly)
	{
		using namespace AngelscriptCacheEnvironmentTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));

		FAngelscriptPreprocessorContext BaseContext =
			FAngelscriptPreprocessorContext::CreateFromCurrentEngineContext();
		const TArray<FAngelscriptSourceRoot> BaseRoots{
			FAngelscriptSourceRoot::FromGameRoot(
				TEXT("D:/Workspace/IdentityProject/Script")),
		};
		const FAngelscriptCacheEnvironmentProfile Base = BuildIdentity(
			*TestRunner, Fixture.GetEngine(), BaseContext, BaseRoots);

		FAngelscriptPreprocessorContext ChangedOption = BaseContext;
		ChangedOption.bUseAutomaticImportMethod =
			!BaseContext.bUseAutomaticImportMethod;
		const FAngelscriptCacheEnvironmentProfile OptionVariant = BuildIdentity(
			*TestRunner, Fixture.GetEngine(), ChangedOption, BaseRoots);

		const TArray<FAngelscriptSourceRoot> ChangedMountRoots{
			FAngelscriptSourceRoot::FromGameRoot(
				TEXT("D:/Workspace/IdentityProject/Script")),
			FAngelscriptSourceRoot::FromPluginRoot(
				TEXT("NewLogicalPlugin"),
				TEXT("D:/Workspace/IdentityProject/Plugins/NewLogicalPlugin/Script")),
		};
		const FAngelscriptCacheEnvironmentProfile MountVariant = BuildIdentity(
			*TestRunner, Fixture.GetEngine(), BaseContext, ChangedMountRoots);

		ASSERT_THAT(IsTrue(Base.CaptureOptions.Compatibility.Hash
			== OptionVariant.CaptureOptions.Compatibility.Hash));
		ASSERT_THAT(IsTrue(Base.CaptureOptions.Compatibility.Hash
			== MountVariant.CaptureOptions.Compatibility.Hash));
		ASSERT_THAT(IsFalse(Base.CaptureOptions.Context.Hash
			== OptionVariant.CaptureOptions.Context.Hash));
		ASSERT_THAT(IsFalse(Base.CaptureOptions.Profile.Hash
			== OptionVariant.CaptureOptions.Profile.Hash));
		ASSERT_THAT(IsFalse(Base.CaptureOptions.Context.Hash
			== MountVariant.CaptureOptions.Context.Hash));
		ASSERT_THAT(IsFalse(Base.CaptureOptions.Profile.Hash
			== MountVariant.CaptureOptions.Profile.Hash));
	}

	TEST_METHOD(TargetOverrideContributesIndependentlyOfPreprocessorOptions)
	{
		using namespace AngelscriptCacheEnvironmentTests_Private;
		FAngelscriptEngineConfig GameConfig;
		GameConfig.bSkipThreadedInitialize = true;
		GameConfig.bOverrideTargetPreprocessorFlags = true;
		GameConfig.bTargetEditor = false;
		GameConfig.bTargetEditorOnlyData = false;
		GameConfig.bTargetRelease = false;
		GameConfig.bTargetTest = true;
		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> GameEngine =
			CreateScriptScanFreeFullEngineForTesting(GameConfig, Dependencies);
		TUniquePtr<FAngelscriptEngine> HostEngine =
			CreateIsolatedFullEngine();
		ASSERT_THAT(IsNotNull(GameEngine.Get()));
		ASSERT_THAT(IsNotNull(HostEngine.Get()));
		if (!GameEngine.IsValid() || !HostEngine.IsValid())
		{
			return;
		}

		FAngelscriptPreprocessorContext SharedContext;
		{
			FAngelscriptEngineScope Scope(*GameEngine);
			SharedContext =
				FAngelscriptPreprocessorContext::CreateFromCurrentEngineContext();
		}
		const TArray<FAngelscriptSourceRoot> Roots{
			FAngelscriptSourceRoot::FromGameRoot(
				TEXT("D:/Workspace/TargetProfileProject/Script")),
		};
		const FAngelscriptCacheEnvironmentProfile Game = BuildIdentity(
			*TestRunner, *GameEngine, SharedContext, Roots);
		const FAngelscriptCacheEnvironmentProfile Host = BuildIdentity(
			*TestRunner, *HostEngine, SharedContext, Roots);

		ASSERT_THAT(IsFalse(Game.CaptureOptions.Compatibility.Hash
			== Host.CaptureOptions.Compatibility.Hash));
		ASSERT_THAT(AreEqual(
			Game.CaptureOptions.CanonicalCompileOptions,
			Host.CaptureOptions.CanonicalCompileOptions));
		ASSERT_THAT(IsFalse(Game.CaptureOptions.Context.Hash
			== Host.CaptureOptions.Context.Hash));
		ASSERT_THAT(IsFalse(Game.CaptureOptions.Profile.Hash
			== Host.CaptureOptions.Profile.Hash));
	}

	TEST_METHOD(ShippingTargetOverrideUsesShippingEngineProperties)
	{
		using namespace AngelscriptCacheEnvironmentTests_Private;
		FAngelscriptEngineConfig ShippingConfig;
		ShippingConfig.bSkipThreadedInitialize = true;
		ShippingConfig.bOverrideTargetPreprocessorFlags = true;
		ShippingConfig.bTargetEditor = false;
		ShippingConfig.bTargetEditorOnlyData = false;
		ShippingConfig.bTargetRelease = true;
		ShippingConfig.bTargetTest = false;
		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> ShippingEngine =
			CreateScriptScanFreeFullEngineForTesting(
				ShippingConfig, Dependencies);
		ASSERT_THAT(IsNotNull(ShippingEngine.Get()));
		if (!ShippingEngine.IsValid())
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			static_cast<asPWORD>(1),
			ShippingEngine->GetScriptEngine()->GetEngineProperty(
				asEP_BUILD_WITHOUT_LINE_CUES)));

		FAngelscriptPreprocessorContext ShippingContext;
		{
			FAngelscriptEngineScope Scope(*ShippingEngine);
			ShippingContext = FAngelscriptPreprocessorContext::
				CreateFromCurrentEngineContext();
		}
		const TArray<FAngelscriptSourceRoot> Roots{
			FAngelscriptSourceRoot::FromGameRoot(
				TEXT("D:/Workspace/ShippingProfileProject/Script")),
		};
		const FAngelscriptCacheEnvironmentProfile WithoutLineCues =
			BuildIdentity(*TestRunner, *ShippingEngine, ShippingContext, Roots);
		ShippingEngine->GetScriptEngine()->SetEngineProperty(
			asEP_BUILD_WITHOUT_LINE_CUES, 0);
		const FAngelscriptCacheEnvironmentProfile WithLineCues =
			BuildIdentity(*TestRunner, *ShippingEngine, ShippingContext, Roots);
		ASSERT_THAT(IsFalse(WithoutLineCues.CaptureOptions.Context.Hash
			== WithLineCues.CaptureOptions.Context.Hash));

		ShippingEngine->GetScriptEngine()->SetEngineProperty(
			asEP_BUILD_WITHOUT_LINE_CUES, 1);
	}

	TEST_METHOD(RegisteredPropertyIdentityIsStableAndLayoutSensitive)
	{
		using namespace AngelscriptCacheEnvironmentTests_Private;
		asIScriptEngine* FirstEngine = asCreateScriptEngine();
		asIScriptEngine* SecondEngine = asCreateScriptEngine();
		asIScriptEngine* ShiftedEngine = asCreateScriptEngine();
		ASSERT_THAT(IsNotNull(FirstEngine));
		ASSERT_THAT(IsNotNull(SecondEngine));
		ASSERT_THAT(IsNotNull(ShiftedEngine));
		ON_SCOPE_EXIT
		{
			if (FirstEngine != nullptr)
			{
				FirstEngine->ShutDownAndRelease();
			}
			if (SecondEngine != nullptr)
			{
				SecondEngine->ShutDownAndRelease();
			}
			if (ShiftedEngine != nullptr)
			{
				ShiftedEngine->ShutDownAndRelease();
			}
		};

		asCObjectType* FirstType = nullptr;
		asCObjectProperty* FirstProperty = nullptr;
		asCObjectType* SecondType = nullptr;
		asCObjectProperty* SecondProperty = nullptr;
		asCObjectType* ShiftedType = nullptr;
		asCObjectProperty* ShiftedProperty = nullptr;
		ASSERT_THAT(IsTrue(RegisterPropertyProbe(
			*FirstEngine,
			STRUCT_OFFSET(FRegisteredPropertyProbe, Value),
			FirstType, FirstProperty)));
		ASSERT_THAT(IsTrue(RegisterPropertyProbe(
			*SecondEngine,
			STRUCT_OFFSET(FRegisteredPropertyProbe, Value),
			SecondType, SecondProperty)));
		ASSERT_THAT(IsTrue(RegisterPropertyProbe(
			*ShiftedEngine,
			STRUCT_OFFSET(FRegisteredPropertyProbe, Padding),
			ShiftedType, ShiftedProperty)));

		FAngelscriptArtifactReference FirstReference;
		FAngelscriptArtifactReference SecondReference;
		FAngelscriptArtifactReference ShiftedReference;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheEnvironmentIdentity::TryBuildPropertyReference(
				*FirstType, *FirstProperty, FirstReference)));
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheEnvironmentIdentity::TryBuildPropertyReference(
				*SecondType, *SecondProperty, SecondReference)));
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheEnvironmentIdentity::TryBuildPropertyReference(
				*ShiftedType, *ShiftedProperty, ShiftedReference)));

		ASSERT_THAT(IsTrue(FirstReference.StableKey
			== SecondReference.StableKey));
		ASSERT_THAT(IsTrue(FirstReference.ExpectedAbi
			== SecondReference.ExpectedAbi));
		ASSERT_THAT(IsTrue(FirstReference.StableKey
			== ShiftedReference.StableKey));
		ASSERT_THAT(IsFalse(FirstReference.ExpectedAbi
			== ShiftedReference.ExpectedAbi));

		const FAngelscriptCacheEngineEnvironmentResolver MatchingResolver(
			*static_cast<asCScriptEngine*>(SecondEngine));
		const TOptional<FAngelscriptCacheCurrentSymbol> Matching =
			MatchingResolver.Resolve(
				FirstReference.Kind, FirstReference.StableKey);
		ASSERT_THAT(IsTrue(Matching.IsSet()));
		ASSERT_THAT(IsTrue(Matching->CurrentAbi
			== FirstReference.ExpectedAbi));
		asCTypeInfo* ResolvedOwner = nullptr;
		ASSERT_THAT(IsTrue(MatchingResolver.ResolvePropertyReference(
			FirstReference, &ResolvedOwner) == SecondProperty));
		ASSERT_THAT(IsTrue(ResolvedOwner == SecondType));

		const FAngelscriptCacheEngineEnvironmentResolver ShiftedResolver(
			*static_cast<asCScriptEngine*>(ShiftedEngine));
		const TOptional<FAngelscriptCacheCurrentSymbol> Shifted =
			ShiftedResolver.Resolve(
				FirstReference.Kind, FirstReference.StableKey);
		ASSERT_THAT(IsTrue(Shifted.IsSet()));
		ASSERT_THAT(IsFalse(Shifted->CurrentAbi
			== FirstReference.ExpectedAbi));
		ASSERT_THAT(IsNull(ShiftedResolver.ResolvePropertyReference(
			FirstReference)));
	}

	TEST_METHOD(EnvironmentResolverBuildsOneStableSymbolIndexPerSession)
	{
		using namespace AngelscriptCacheEnvironmentTests_Private;
		asIScriptEngine* const PublicEngine = asCreateScriptEngine();
		ASSERT_THAT(IsNotNull(PublicEngine));
		if (PublicEngine == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			PublicEngine->ShutDownAndRelease();
		};

		asCObjectType* RegisteredType = nullptr;
		asCObjectProperty* RegisteredProperty = nullptr;
		ASSERT_THAT(IsTrue(RegisterPropertyProbe(
			*PublicEngine,
			STRUCT_OFFSET(FRegisteredPropertyProbe, Value),
			RegisteredType,
			RegisteredProperty)));
		const int32 FunctionId = PublicEngine->RegisterGlobalFunction(
			"int IndexedEnvironmentFunction(int)",
			asFUNCTION(EnvironmentFunctionProbe), asCALL_CDECL);
		ASSERT_THAT(IsTrue(FunctionId >= 0));

		FAngelscriptArtifactReference PropertyReference;
		FAngelscriptArtifactReference FunctionReference;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheEnvironmentIdentity::TryBuildPropertyReference(
				*RegisteredType, *RegisteredProperty, PropertyReference)));
		asCScriptFunction* const RegisteredFunction =
			static_cast<asCScriptFunction*>(
				PublicEngine->GetFunctionById(FunctionId));
		ASSERT_THAT(IsNotNull(RegisteredFunction));
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
				*RegisteredFunction, FunctionReference)));

		const FAngelscriptCacheEngineEnvironmentResolver Resolver(
			*static_cast<asCScriptEngine*>(PublicEngine));
		ASSERT_THAT(AreEqual(
			0u, Resolver.GetSymbolIndexBuildCountForDiagnostics()));
		ASSERT_THAT(AreEqual(
			0, Resolver.GetIndexedSymbolCountForDiagnostics()));
		const TOptional<FAngelscriptCacheCurrentSymbol> FirstProperty =
			Resolver.Resolve(
				PropertyReference.Kind, PropertyReference.StableKey);
		const TOptional<FAngelscriptCacheCurrentSymbol> Function =
			Resolver.Resolve(
				FunctionReference.Kind, FunctionReference.StableKey);
		const TOptional<FAngelscriptCacheCurrentSymbol> SecondProperty =
			Resolver.Resolve(
				PropertyReference.Kind, PropertyReference.StableKey);
		ASSERT_THAT(IsTrue(FirstProperty.IsSet()));
		ASSERT_THAT(IsTrue(Function.IsSet()));
		ASSERT_THAT(IsTrue(SecondProperty.IsSet()));
		ASSERT_THAT(IsTrue(FirstProperty->CurrentAbi
			== PropertyReference.ExpectedAbi));
		ASSERT_THAT(IsTrue(Function->CurrentAbi
			== FunctionReference.ExpectedAbi));
		ASSERT_THAT(AreEqual(
			1u, Resolver.GetSymbolIndexBuildCountForDiagnostics()));
		ASSERT_THAT(IsTrue(
			Resolver.GetIndexedSymbolCountForDiagnostics() >= 2));
	}
};

#endif
