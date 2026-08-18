#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptTestEngineAcquisition.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptType.h"
#include "Artifacts/AngelscriptFunctionRoute.h"
#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"
#include "Compilation/AngelscriptCompilationEvents.h"
#include "Dump/AngelscriptCrashSnapshot.h"
#include "Dump/AngelscriptStateDiff.h"
#include "Dump/AngelscriptStateDump.h"
#include "Dump/AngelscriptStateSnapshot.h"
#if WITH_AS_COVERAGE
#include "Extension/CodeCoverage/AngelscriptCodeCoverage.h"
#endif
#include "StaticJIT/BytecodeJIT/AngelscriptBytecodeJIT.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AngelscriptHIRDumpCommand.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallTarget.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITBackend.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallClosure.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"
#include "Testing/AngelscriptBindExecutionObservation.h"
#include "Testing/AngelscriptScriptTestRegistry.h"

#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectGlobals.h"

#include "StartAngelscriptHeaders.h"
#include "angelscript.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_thread.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

ANGELSCRIPTRUNTIME_API int32
	GetBlueprintEventsByScriptNameTotalCount();
ANGELSCRIPTRUNTIME_API int32
	GetCachedEditorClassCountForTesting();

namespace AngelscriptStaticJITGenerationEngineTests
{
	static const TCHAR* ClassName =
		TEXT("UJITGenerationOnly_65C28A");
	static const TCHAR* StructName =
		TEXT("FJITGenerationOnlyPayload_65C28A");
	static const TCHAR* DelegateName =
		TEXT("FJITGenerationOnlySignal_65C28A");
	static const TCHAR* MethodName = TEXT("EvaluateGenerationOnly");
	static const TCHAR* RawClassName =
		TEXT("FJITGenerationOnlyRawBox_65C28A");
	static const TCHAR* StaticsClassName =
		TEXT("Module_StaticJIT_GenerationOnly_StaticJITGenerationOnlyStatics");
	static const TCHAR* GlobalUFunctionName =
		TEXT("EvaluateGlobalGenerationOnly_65C28A");
	static const TCHAR* EmitModuleName =
		TEXT("StaticJITGenerationEmit");
	static const TCHAR* MissingNativeCDODiagnostic =
		TEXT("Static JIT generation cannot analyze component metadata because native superclass");

	struct FPrimaryContainmentSnapshot
	{
		FAngelscriptStateSnapshot EngineState;
		FAngelscriptProcessPackageReferenceCounts PackageReferences;
		TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> ProviderRegistry;
		TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot>
			ScriptTestRegistry;
		asCContext* PrimaryContext = nullptr;
		int32 PrimaryPooledContextCount = 0;
		int32 BlueprintEventCount = 0;
		int32 CachedEditorClassCount = 0;
		int32 UObjectCount = 0;
		int32 PermanentUObjectCount = 0;
		int32 WorldContextCount = 0;
		int32 CrashSnapshotActiveEngineCount = 0;
		bool bPrimaryFullReloadBound = false;
		bool bPrimaryPostReloadBound = false;
		bool bPrimaryClassReloadBound = false;
		bool bPrimaryStructReloadBound = false;
		bool bPrimaryDelegateReloadBound = false;
		bool bPrimaryOnScreenMessagesBound = false;
#if WITH_AS_COVERAGE
		FAngelscriptCodeCoverage* PrimaryCoverage = nullptr;
#endif
	};

	static FPrimaryContainmentSnapshot CapturePrimaryContainmentSnapshot()
	{
		FAngelscriptEngine& PrimaryEngine = FAngelscriptEngine::Get();
		asIScriptEngine* const PrimaryScriptEngine =
			PrimaryEngine.GetScriptEngine();
		FPrimaryContainmentSnapshot Snapshot;
		Snapshot.EngineState = FAngelscriptStateDump::CaptureSnapshot(
			PrimaryEngine);
		Snapshot.PackageReferences =
			FAngelscriptEngine::GetProcessPackageReferenceCountsForTesting();
		Snapshot.ProviderRegistry =
			FAngelscriptJITProviderRegistry::Get().GetSnapshot();
		Snapshot.ScriptTestRegistry =
			FAngelscriptScriptTestRegistry::Get().GetSnapshot();
		Snapshot.PrimaryContext = FAngelscriptEngine::GameThreadTLD != nullptr
			? FAngelscriptEngine::GameThreadTLD->primaryContext
			: nullptr;
		Snapshot.PrimaryPooledContextCount =
			FAngelscriptEngine::GetLocalPooledContextCountForTesting(
				PrimaryScriptEngine);
		Snapshot.BlueprintEventCount =
			GetBlueprintEventsByScriptNameTotalCount();
		Snapshot.CachedEditorClassCount =
			GetCachedEditorClassCountForTesting();
		Snapshot.UObjectCount = GUObjectArray.GetObjectArrayNum();
		Snapshot.PermanentUObjectCount =
			GUObjectArray.GetObjectArrayNumPermanent();
		Snapshot.WorldContextCount = GEngine != nullptr
			? GEngine->GetWorldContexts().Num() : 0;
		Snapshot.CrashSnapshotActiveEngineCount =
			FAngelscriptCrashSnapshotExtension::
				GetActiveEngineCountForTesting();
		Snapshot.bPrimaryFullReloadBound =
			PrimaryEngine.GetOnFullReload().IsBound();
		Snapshot.bPrimaryPostReloadBound =
			PrimaryEngine.GetOnPostReload().IsBound();
		Snapshot.bPrimaryClassReloadBound =
			PrimaryEngine.GetOnClassReload().IsBound();
		Snapshot.bPrimaryStructReloadBound =
			PrimaryEngine.GetOnStructReload().IsBound();
		Snapshot.bPrimaryDelegateReloadBound =
			PrimaryEngine.GetOnDelegateReload().IsBound();
#if !UE_BUILD_SHIPPING
		Snapshot.bPrimaryOnScreenMessagesBound =
			FCoreDelegates::OnGetOnScreenMessages.IsBoundToObject(
				&PrimaryEngine);
#endif
#if WITH_AS_COVERAGE
		Snapshot.PrimaryCoverage =
			FAngelscriptCodeCoverageExtension::GetForEngine(PrimaryEngine);
#endif
		return Snapshot;
	}

	static bool AssertPrimaryContainmentUnchanged(
		FAutomationTestBase& Test,
		const FPrimaryContainmentSnapshot& Before,
		const FPrimaryContainmentSnapshot& After,
		const FStringView Stage)
	{
		bool bPassed = true;
		auto Exact = [&Test, &Stage, &bPassed](
			const bool bCondition, const TCHAR* Field)
		{
			bPassed &= Test.TestTrue(
				*FString::Printf(TEXT("%.*s preserves %s"),
					Stage.Len(), Stage.GetData(), Field),
				bCondition);
		};

		const FAngelscriptStateDiff EngineDiff =
			FAngelscriptStateDump::DiffSnapshots(
				Before.EngineState, After.EngineState);
		if (!EngineDiff.Rows.IsEmpty())
		{
			FString Detail;
			const int32 Limit = FMath::Min(EngineDiff.Rows.Num(), 12);
			for (int32 Index = 0; Index < Limit; ++Index)
			{
				const FAngelscriptStateDiffRow& Row = EngineDiff.Rows[Index];
				Detail += FString::Printf(
					TEXT("%s/%s/%s:%s->%s; "),
					*Row.Category, *Row.Identity, *Row.Field,
					*Row.BeforeValue, *Row.AfterValue);
			}
			Test.AddError(FString::Printf(
				TEXT("%.*s changed primary Engine state: %s"),
				Stage.Len(), Stage.GetData(), *Detail));
			bPassed = false;
		}
		Exact(Before.PackageReferences.AngelscriptPackage
			== After.PackageReferences.AngelscriptPackage,
			TEXT("/Script/Angelscript ownership"));
		Exact(Before.PackageReferences.AngelscriptAssetsPackage
			== After.PackageReferences.AngelscriptAssetsPackage,
			TEXT("/Script/AngelscriptAssets ownership"));
		Exact(Before.ProviderRegistry.Get() == After.ProviderRegistry.Get(),
			TEXT("Provider registry snapshot identity"));
		Exact(Before.ProviderRegistry.IsValid()
			&& After.ProviderRegistry.IsValid()
			&& Before.ProviderRegistry->PublicationOrdinal
				== After.ProviderRegistry->PublicationOrdinal
			&& Before.ProviderRegistry->Providers.Num()
				== After.ProviderRegistry->Providers.Num(),
			TEXT("Provider registry publication"));
		Exact(Before.ScriptTestRegistry.Get()
			== After.ScriptTestRegistry.Get(),
			TEXT("script test registry snapshot identity"));
		Exact(Before.ScriptTestRegistry.IsValid()
			&& After.ScriptTestRegistry.IsValid()
			&& Before.ScriptTestRegistry->Generation
				== After.ScriptTestRegistry->Generation
			&& Before.ScriptTestRegistry->Tests.Num()
				== After.ScriptTestRegistry->Tests.Num(),
			TEXT("script test discovery generation"));
		Exact(Before.PrimaryContext == After.PrimaryContext,
			TEXT("primary context identity"));
		Exact(Before.PrimaryPooledContextCount
			== After.PrimaryPooledContextCount,
			TEXT("primary TLS context pool"));
		Exact(Before.BlueprintEventCount == After.BlueprintEventCount,
			TEXT("Blueprint event registry"));
		Exact(Before.CachedEditorClassCount
			== After.CachedEditorClassCount,
			TEXT("Editor class cache"));
		Exact(Before.UObjectCount == After.UObjectCount,
			TEXT("live UObject count"));
		Exact(Before.PermanentUObjectCount == After.PermanentUObjectCount,
			TEXT("permanent UObject count"));
		Exact(Before.WorldContextCount == After.WorldContextCount,
			TEXT("world context count"));
		Exact(Before.CrashSnapshotActiveEngineCount
			== After.CrashSnapshotActiveEngineCount,
			TEXT("crash extension Engine set"));
		Exact(Before.bPrimaryFullReloadBound
			== After.bPrimaryFullReloadBound,
			TEXT("primary full-reload delegates"));
		Exact(Before.bPrimaryPostReloadBound
			== After.bPrimaryPostReloadBound,
			TEXT("primary post-reload delegates"));
		Exact(Before.bPrimaryClassReloadBound
			== After.bPrimaryClassReloadBound,
			TEXT("primary class-reload delegates"));
		Exact(Before.bPrimaryStructReloadBound
			== After.bPrimaryStructReloadBound,
			TEXT("primary struct-reload delegates"));
		Exact(Before.bPrimaryDelegateReloadBound
			== After.bPrimaryDelegateReloadBound,
			TEXT("primary delegate-reload delegates"));
		Exact(Before.bPrimaryOnScreenMessagesBound
			== After.bPrimaryOnScreenMessagesBound,
			TEXT("primary on-screen delegate"));
#if WITH_AS_COVERAGE
		Exact(Before.PrimaryCoverage == After.PrimaryCoverage,
			TEXT("primary coverage recorder"));
#endif
		return bPassed;
	}

	struct FFixture
	{
		FString Root;
		FString ModuleName;
		TUniquePtr<FAngelscriptEngine> Engine;
		int32 FullReloadCount = 0;
		int32 PostReloadCount = 0;
		int32 ClassReloadCount = 0;
		int32 StructReloadCount = 0;
		int32 DelegateReloadCount = 0;
		int32 CompilationEventCount = 0;
		int32 StartupCompileFailureExitCount = 0;
		bool bStartupCompileFailureExitForce = false;
		uint8 StartupCompileFailureExitStatus = 0;

		~FFixture()
		{
			Engine.Reset();
			if (Root.Contains(
				TEXT("/Saved/Automation/StaticJITGenerationEngine/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}
	};

	static bool AssertGenerationServicesSuppressed(
		FAutomationTestBase& Test,
		const FFixture& Fixture)
	{
		bool bPassed = true;
		bPassed &= Test.TestFalse(
			TEXT("Generation must not create a DebugServer"),
			Fixture.Engine->HasDebugServerForTesting());
		bPassed &= Test.TestFalse(
			TEXT("Generation must not attach Runtime extensions"),
			Fixture.Engine->HasAttachedExtensionsForTesting());
		bPassed &= Test.TestFalse(
			TEXT("Generation must not start a Hot Reload thread"),
			Fixture.Engine->HasHotReloadThreadStartedForTesting());
		bPassed &= Test.TestNull(
			TEXT("Generation must not create a script-test Hot Reload runner"),
			Fixture.Engine->GetScriptTestHotReloadRunnerForTesting());
#if WITH_AS_COVERAGE
		bPassed &= Test.TestNull(
			TEXT("Generation must not create a coverage recorder"),
			FAngelscriptCodeCoverageExtension::GetForEngine(*Fixture.Engine));
#endif
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes =
			Fixture.Engine->GetFunctionRouteSnapshot();
		bPassed &= Test.TestTrue(
			TEXT("Generation must not publish Runtime function routes"),
			!Routes.IsValid() || Routes->FunctionRoutes.IsEmpty());
		return bPassed;
	}

	static bool AssertNoGenerationPersistence(
		FAutomationTestBase& Test,
		const FString& Root,
		const FStringView Stage)
	{
		bool bPassed = true;
		auto RequireEmpty = [&Test, &Stage, &bPassed](
			const TArray<FString>& Files,
			const TCHAR* Kind)
		{
			bPassed &= Test.TestTrue(
				*FString::Printf(TEXT("%.*s writes no %s"),
					Stage.Len(), Stage.GetData(), Kind),
				Files.IsEmpty());
		};

		TArray<FString> BindCacheFiles;
		IFileManager::Get().FindFilesRecursive(
			BindCacheFiles, *Root, TEXT("Binds.Cache"),
			true, false, false);
		RequireEmpty(BindCacheFiles, TEXT("BindDB cache"));

		TArray<FString> CacheV2Files;
		const FString CacheV2Root = FPaths::Combine(Root, TEXT("CacheV2"));
		if (IFileManager::Get().DirectoryExists(*CacheV2Root))
		{
			IFileManager::Get().FindFilesRecursive(
				CacheV2Files, *CacheV2Root, TEXT("*"),
				true, false, false);
		}
		RequireEmpty(CacheV2Files, TEXT("Cache V2 artifact"));

		TArray<FString> HIRFiles;
		IFileManager::Get().FindFilesRecursive(
			HIRFiles, *Root, TEXT("*.hir*"),
			true, false, false);
		RequireEmpty(HIRFiles, TEXT("HIR substitute/readback artifact"));
		return bPassed;
	}

	static FString MakeSource()
	{
		return FString::Printf(TEXT(R"AS(
USTRUCT()
struct %s
{
	UPROPERTY()
	int Value;
}

delegate void %s(%s Payload);

const int GenerationOnlyGlobal_65C28A = 7;

UFUNCTION()
int EvaluateGlobalGenerationOnly_65C28A(int Value)
{
	return Value + GenerationOnlyGlobal_65C28A;
}

class %s
{
	int Value;
}

UCLASS()
class %s : UObject
{
	UPROPERTY()
	int StoredValue;

	UFUNCTION()
	int %s(%s Payload)
	{
		StoredValue = Payload.Value;
		return StoredValue + GenerationOnlyGlobal_65C28A;
	}
}
)AS"),
			StructName,
			DelegateName,
			StructName,
			RawClassName,
			ClassName,
			MethodName,
			StructName);
	}

	static FString MakeEmitSource()
	{
		return TEXT(R"AS(
enum EJITGenerationEmitMode_65C28A
{
	Default = 0,
}

int IncrementGenerationEmit_65C28A(int Value)
{
	return Value + 1;
}

int EvaluateGenerationEmit_65C28A(int Value)
{
	return Math::Abs(IncrementGenerationEmit_65C28A(Value));
}
)AS");
	}

	static UClass* CreateUnmaterializedNativeActorClass(
		FAutomationTestBase& Test,
		const FStringView StableStem,
		UClass* const SuperClass)
	{
		if (!Test.TestNotNull(
			TEXT("Generation CDO isolation should have a native superclass"),
			SuperClass))
		{
			return nullptr;
		}
		const FName DynamicClassName = MakeUniqueObjectName(
			GetTransientPackage(),
			UClass::StaticClass(),
			FName(StableStem));
		UClass* const TestClass = NewObject<UClass>(
			GetTransientPackage(),
			DynamicClassName,
			RF_Transient);
		if (!Test.TestNotNull(
			TEXT("Generation CDO isolation should create a transient native class"),
			TestClass))
		{
			return nullptr;
		}

		TestClass->ClassFlags = SuperClass->ClassFlags & CLASS_ScriptInherit;
		TestClass->ClassConfigName = SuperClass->ClassConfigName;
		TestClass->PropertyLink = SuperClass->PropertyLink;
		TestClass->SetSuperStruct(SuperClass);
		TestClass->SetPropertiesSize(SuperClass->GetPropertiesSize());
		TestClass->ClassWithin = UObject::StaticClass();
		TestClass->Bind();
		TestClass->ClassFlags |= CLASS_Native;
		TestClass->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		TestClass->AddToRoot();

		if (!Test.TestNull(
			TEXT("Transient native class should begin without a CDO"),
			TestClass->GetDefaultObject(false)))
		{
			TestClass->RemoveFromRoot();
			return nullptr;
		}
		return TestClass;
	}

	static void ReleaseTransientNativeClass(UClass* TestClass)
	{
		if (TestClass == nullptr)
		{
			return;
		}
		TestClass->RemoveMetaData(TEXT("BlueprintType"));
		TestClass->ClassFlags &= ~CLASS_Native;
		TestClass->RemoveFromRoot();
	}

	static TUniquePtr<FFixture> CreateFixture(
		FAutomationTestBase& Test,
		const FStringView Suffix,
		const FAngelscriptStaticJITBackendId& BackendId =
			FAngelscriptStaticJITBackendId::Bytecode(),
		const FStringView AdditionalSource = FStringView(),
		const bool bRequireSuccessfulInitialCompile = true,
		TFunction<void(FAngelscriptEngineConfig&)> ConfigureEngine = {},
		const FStringView AdditionalEmitSource = FStringView(),
		TFunction<bool(FAngelscriptEngine&)>
			PrepareEngineBeforeInitialCompile = {},
		const bool bReplaceEmitSource = false)
	{
		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		FString ProfileError;
		if (!Test.TestTrue(
			TEXT("Generation fixture should resolve its backend profile"),
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				BackendId,
				EAngelscriptJITTargetProfile::EditorDevelopment,
				GenerationProfile, &ProfileError)))
		{
			Test.AddError(ProfileError);
			return nullptr;
		}

		TUniquePtr<FFixture> Fixture = MakeUnique<FFixture>();
		Fixture->ModuleName = TEXT("StaticJITGenerationOnly");
		Fixture->Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/StaticJITGenerationEngine"),
			FString(Suffix),
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		FPaths::NormalizeDirectoryName(Fixture->Root);
		const FString ScriptRoot = FPaths::Combine(
			Fixture->Root, TEXT("Script/StaticJIT/GenerationOnly"));
		const FString ScriptPath = FPaths::Combine(
			ScriptRoot, Fixture->ModuleName + TEXT(".as"));
		const FString EmitScriptPath = FPaths::Combine(
			ScriptRoot, FString(EmitModuleName) + TEXT(".as"));
		FString PrimarySource = MakeSource();
		if (!AdditionalSource.IsEmpty())
		{
			PrimarySource += TEXT("\n\n");
			PrimarySource.AppendChars(
				AdditionalSource.GetData(), AdditionalSource.Len());
		}
		FString EmitSource = bReplaceEmitSource
			? FString(AdditionalEmitSource) : MakeEmitSource();
		if (!bReplaceEmitSource && !AdditionalEmitSource.IsEmpty())
		{
			EmitSource += TEXT("\n\n");
			EmitSource.AppendChars(
				AdditionalEmitSource.GetData(), AdditionalEmitSource.Len());
		}
		if (!Test.TestTrue(TEXT("Generation fixture should create Script root"),
			IFileManager::Get().MakeDirectory(*ScriptRoot, true))
			|| !Test.TestTrue(TEXT("Generation fixture should write source"),
				FFileHelper::SaveStringToFile(PrimarySource, *ScriptPath))
			|| !Test.TestTrue(TEXT("Generation fixture should write emit source"),
				FFileHelper::SaveStringToFile(
					EmitSource, *EmitScriptPath)))
		{
			return nullptr;
		}

		FAngelscriptEngineConfig Config;
		Config.Purpose = EAngelscriptEnginePurpose::StaticJITGeneration;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.bRunningCommandlet = true;
		Config.bIsUnattended = true;
		Config.bDisableCacheV2Persistence = true;
		Config.bForceSerialCacheV2Preparation = true;
		Config.CacheV2RootOverride = FPaths::Combine(
			Fixture->Root, TEXT("CacheV2"));
		GenerationProfile.ApplyToEngineConfig(Config);
		if (PrepareEngineBeforeInitialCompile)
		{
			Config.bSkipInitialCompile = true;
		}
		if (ConfigureEngine)
		{
			ConfigureEngine(Config);
		}

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		const FString ProjectRoot = Fixture->Root;
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
		Dependencies.HandleStartupCompileFailureExit =
			[FixturePtr = Fixture.Get()](
				const FAngelscriptStartupCompileFailureExitRequest& Request)
			{
				++FixturePtr->StartupCompileFailureExitCount;
				FixturePtr->bStartupCompileFailureExitForce = Request.bForce;
				FixturePtr->StartupCompileFailureExitStatus = Request.Status;
			};

		const FDelegateHandle CompilationEventHandle =
			FAngelscriptCompilationEvents::RegisterListener(
				[FixturePtr = Fixture.Get()](const auto&)
				{
					++FixturePtr->CompilationEventCount;
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(
				CompilationEventHandle);
		};
		Fixture->Engine = FAngelscriptEngine::Create(Config, Dependencies);
		if (!Test.TestNotNull(TEXT("Generation fixture should create Engine"),
			Fixture->Engine.Get()))
		{
			return nullptr;
		}
		if (PrepareEngineBeforeInitialCompile)
		{
			if (!Test.TestTrue(
					TEXT("Generation fixture should prepare its native surface before source compilation"),
					PrepareEngineBeforeInitialCompile(*Fixture->Engine)))
			{
				return nullptr;
			}
			FAngelscriptEngineScope EngineScope(*Fixture->Engine);
			Fixture->Engine->InitialCompile();
		}
		Fixture->Engine->GetOnFullReload().AddLambda([FixturePtr = Fixture.Get()]()
		{
			++FixturePtr->FullReloadCount;
		});
		Fixture->Engine->GetOnPostReload().AddLambda(
			[FixturePtr = Fixture.Get()](bool)
			{
				++FixturePtr->PostReloadCount;
			});
		Fixture->Engine->GetOnClassReload().AddLambda(
			[FixturePtr = Fixture.Get()](UClass*, UClass*)
			{
				++FixturePtr->ClassReloadCount;
			});
		Fixture->Engine->GetOnStructReload().AddLambda(
			[FixturePtr = Fixture.Get()](UScriptStruct*, UScriptStruct*)
			{
				++FixturePtr->StructReloadCount;
			});
		Fixture->Engine->GetOnDelegateReload().AddLambda(
			[FixturePtr = Fixture.Get()](UDelegateFunction*, UDelegateFunction*)
			{
				++FixturePtr->DelegateReloadCount;
			});

		if (bRequireSuccessfulInitialCompile
			&& !Test.TestTrue(TEXT("Generation fixture source should compile"),
				Fixture->Engine->DidInitialCompileSucceed()))
		{
			return nullptr;
		}
		return Fixture;
	}

	static const FAngelscriptStaticJITGenerationDescriptor*
	FindDescriptor(
		const FAngelscriptStaticJITGenerationSnapshot& Snapshot,
		const FStringView Name,
		const EAngelscriptStaticJITGenerationDescriptorKind Kind)
	{
		return Snapshot.Descriptors.FindByPredicate(
			[Name, Kind](
				const FAngelscriptStaticJITGenerationDescriptor& Candidate)
			{
				return Candidate.Kind == Kind
					&& FStringView(Candidate.CanonicalName).Equals(
						Name, ESearchCase::CaseSensitive);
			});
	}

	static FString EscapeInventoryCsvCell(FString Value)
	{
		Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		Value.ReplaceInline(TEXT("\""), TEXT("\"\""));
		return TEXT("\"") + Value + TEXT("\"");
	}

	static FString BuildNativeCallInventoryCsv(
		const FAngelscriptStaticJITGenerationSnapshot& Snapshot)
	{
		const TCHAR* const Header = TEXT(
			"EngineLocalFunctionId,CanonicalDeclaration,NamespaceCanonicalName,ReceiverCanonicalName,HasStableReference,StableReferenceKind,StableReferenceKey,StableReferenceExpectedABI,StableReferenceError,Origin,OwnerModule,Provider,Phase,ProviderSourceFile,ProviderSourceLine,CallableKind,FunctionTraitBits,CompileOutKind,DefaultArgumentCount,HiddenArgumentIndex,DeterminesOutputTypeArgumentIndex,FirstParamMetadata,RegisteredCallConv,DispatchKind,RegisteredParameterSize,RegisteredReturnSize,ReturnsOnStack,UsesWorldContext,HasSystemCaller,HasNativeForm,NativeFormKind,NativeCallableDisplay,NativeCustomForm,NativeTargetType,NativeHeader,NativeUnrealFunctionPath,NativeUFunctionRouteFlags,NativeFormTrivial,NativeFormGuaranteed,NativeFormNeedsCompare,NativeFormNeedsCopy,HasExternalDescriptor,DescriptorLinkage,DescriptorVisibility,DescriptorSymbol,DescriptorInclude,DescriptorOwningModule,DescriptorApiMacro,DescriptorCppCallableSignature,DescriptorABIDomain,DescriptorCompileOutPolicy,DescriptorDefaultArgumentPolicy,DescriptorDefaultArgumentCount,DescriptorWorldContextPolicy,DescriptorExceptionPolicy,DescriptorRouteFlags,DescriptorLifetimeFlags,DescriptorNormalized,HasExpectedNativeABI,ExpectedNativeABI,ExpectedNativeABIHash,NativeABIError,HasExpectedScalarABI,ExpectedScalarABI,ExpectedScalarABIHash,ScalarABIError");
		FString Result(Header);
		Result += TEXT("\r\n");
		for (const FAngelscriptStaticJITNativeCallInventoryEntry& Entry
			: Snapshot.NativeCallInventory)
		{
			const FAngelscriptStaticJITNativeCallDescriptor& Descriptor =
				Entry.ExternalDescriptor;
			TArray<FString> Cells;
			Cells.Reserve(64);
			auto Add = [&Cells](FString Value)
			{
				Cells.Add(EscapeInventoryCsvCell(MoveTemp(Value)));
			};
			auto AddNumber = [&Add](const int64 Value)
			{
				Add(LexToString(Value));
			};
			auto AddBool = [&Add](const bool bValue)
			{
				Add(bValue ? TEXT("true") : TEXT("false"));
			};

			AddNumber(Entry.EngineLocalFunctionId);
			Add(Entry.CanonicalDeclaration);
			Add(Entry.NamespaceCanonicalName);
			Add(Entry.ReceiverCanonicalName);
			AddBool(Entry.bHasStableReference);
			AddNumber(static_cast<uint8>(Entry.StableReference.Kind));
			Add(Entry.StableReference.StableKey.ToHexString());
			Add(Entry.StableReference.ExpectedAbi.ToHexString());
			Add(Entry.StableReferenceError);
			AddNumber(static_cast<uint8>(Entry.Origin));
			Add(Entry.OwnerModule.ToString());
			Add(Entry.Provider.ToString());
			AddNumber(static_cast<uint8>(Entry.Phase));
			Add(Entry.ProviderSourceFile);
			AddNumber(Entry.ProviderSourceLine);
			AddNumber(static_cast<uint8>(Entry.CallableKind));
			AddNumber(Entry.FunctionTraitBits);
			AddNumber(static_cast<uint8>(Entry.CompileOutKind));
			AddNumber(Entry.DefaultArgumentCount);
			AddNumber(Entry.HiddenArgumentIndex);
			AddNumber(Entry.DeterminesOutputTypeArgumentIndex);
			AddNumber(static_cast<uint8>(Entry.FirstParamMetadata));
			AddNumber(Entry.RegisteredCallConv);
			AddNumber(static_cast<uint8>(Entry.DispatchKind));
			AddNumber(Entry.RegisteredParameterSize);
			AddNumber(Entry.RegisteredReturnSize);
			AddBool(Entry.bReturnsOnStack);
			AddBool(Entry.bUsesWorldContext);
			AddBool(Entry.bHasSystemCaller);
			AddBool(Entry.bHasNativeForm);
			AddNumber(static_cast<uint8>(Entry.NativeFormKind));
			Add(Entry.NativeCallableDisplay);
			Add(Entry.NativeCustomForm);
			Add(Entry.NativeTargetType);
			Add(Entry.NativeHeader);
			Add(Entry.NativeUnrealFunctionPath);
			AddNumber(static_cast<uint8>(
				Entry.NativeUFunctionRouteFlags));
			AddBool(Entry.bNativeFormTrivial);
			AddBool(Entry.bNativeFormGuaranteed);
			AddBool(Entry.bNativeFormNeedsCompare);
			AddBool(Entry.bNativeFormNeedsCopy);
			AddBool(Entry.bHasExternalDescriptor);
			AddNumber(static_cast<uint8>(Descriptor.Linkage));
			AddNumber(static_cast<uint8>(Descriptor.Visibility));
			Add(Descriptor.Symbol);
			Add(Descriptor.Include);
			Add(Descriptor.OwningModule.ToString());
			Add(Descriptor.ApiMacro);
			Add(Descriptor.CppCallableSignature);
			AddNumber(static_cast<uint8>(Descriptor.ABIDomain));
			AddNumber(static_cast<uint8>(Descriptor.CompileOutPolicy));
			AddNumber(static_cast<uint8>(Descriptor.DefaultArgumentPolicy));
			AddNumber(Descriptor.DefaultArgumentCount);
			AddNumber(static_cast<uint8>(Descriptor.WorldContextPolicy));
			AddNumber(static_cast<uint8>(Descriptor.ExceptionPolicy));
			AddNumber(static_cast<uint8>(Descriptor.RouteFlags));
			AddNumber(static_cast<uint8>(Descriptor.LifetimeFlags));
			Add(Entry.bHasExternalDescriptor
				? Descriptor.DumpNormalized() : FString());
			AddBool(Entry.bHasExpectedNativeABI);
			Add(Entry.ExpectedNativeABI.Canonical);
			Add(Entry.ExpectedNativeABI.Hash.ToHexString());
			Add(Entry.NativeABIError);
			AddBool(Entry.bHasExpectedScalarABI);
			Add(Entry.ExpectedScalarABI.Canonical);
			Add(Entry.ExpectedScalarABI.Hash.ToHexString());
			Add(Entry.ScalarABIError);
			Result += FString::Join(Cells, TEXT(","));
			Result += TEXT("\r\n");
		}
		if (!Snapshot.NativeCallInventory.IsEmpty())
		{
			Result.LeftChopInline(2, EAllowShrinking::No);
		}
		return Result;
	}

	static EAngelscriptStaticJITDescriptorKind MapDescriptorKind(
		const EAngelscriptStaticJITGenerationDescriptorKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptStaticJITGenerationDescriptorKind::Class:
			return EAngelscriptStaticJITDescriptorKind::Class;
		case EAngelscriptStaticJITGenerationDescriptorKind::Struct:
			return EAngelscriptStaticJITDescriptorKind::Struct;
		case EAngelscriptStaticJITGenerationDescriptorKind::Enum:
			return EAngelscriptStaticJITDescriptorKind::Enum;
		case EAngelscriptStaticJITGenerationDescriptorKind::Delegate:
			return EAngelscriptStaticJITDescriptorKind::Delegate;
		case EAngelscriptStaticJITGenerationDescriptorKind::Property:
			return EAngelscriptStaticJITDescriptorKind::Property;
		case EAngelscriptStaticJITGenerationDescriptorKind::Function:
			return EAngelscriptStaticJITDescriptorKind::Function;
		}
		checkNoEntry();
		return EAngelscriptStaticJITDescriptorKind::Class;
	}

	struct FCompiledGraphStorage
	{
		TArray<asIScriptModule*> Modules;
		TArray<const FAngelscriptModuleDesc*> ResolvedModuleDescriptors;
		TArray<FAngelscriptStaticJITCompiledFunctionView> Functions;
		TArray<FAngelscriptStaticJITCompiledTypeView> Types;
		TArray<FAngelscriptStaticJITCompiledGlobalView> Globals;
		TArray<FAngelscriptStaticJITDescriptorView> Descriptors;
		TArray<FAngelscriptStaticJITDependencyView> Dependencies;
		TArray<FAngelscriptStaticJITSemanticDependencyView>
			SemanticDependencies;
		TArray<FAngelscriptStaticJITDependencyView> ExternalNativeCalls;
		TArray<FAngelscriptStaticJITNativeCallTarget> NativeCallTargets;
		FAngelscriptStaticJITCompiledSourceGraph Graph;
	};

	static FCompiledGraphStorage BuildCompiledGraphStorage(
		const FAngelscriptStaticJITGenerationSnapshot& Snapshot,
		const FStringView ProviderName)
	{
		FCompiledGraphStorage Storage;
		Storage.Modules.Reserve(Snapshot.Modules.Num());
		for (const FAngelscriptStaticJITGenerationModule& Module
			: Snapshot.Modules)
		{
			Storage.Modules.Add(Module.EngineLocalModule);
			Storage.ResolvedModuleDescriptors.Add(
				Module.EngineLocalDescriptor);
		}

		Storage.Functions.Reserve(Snapshot.Functions.Num());
		for (const FAngelscriptStaticJITGenerationFunction& Function
			: Snapshot.Functions)
		{
			Storage.Functions.Add({
				Function.EngineLocalFunction,
				Function.ModuleKey,
				Function.FunctionKey,
				Function.ExecutionHash,
				Function.DebugHash,
				Function.EntryAbiHash,
				Function.CanonicalDeclaration,
				Function.Bytecode,
				Function.VerifiedTypedHIR,
				Function.bHasVerifiedArtifactIdentity,
				Function.bHasVerifiedArtifactReferences,
				Function.bHasVMEntry,
				Function.bHasRawEntry,
				Function.bHasParmsEntry,
				Function.LiteralAssetRole,
				&Function.EntryPlan});
		}

		Storage.Types.Reserve(Snapshot.Types.Num());
		for (const FAngelscriptStaticJITGenerationType& Type : Snapshot.Types)
		{
			Storage.Types.Add({
				Type.EngineLocalType,
				Type.TypeKey.Hash,
				Type.CanonicalDeclaration,
				Type.bTargetMaterializesScriptReflection});
		}

		Storage.Globals.Reserve(Snapshot.Globals.Num());
		for (const FAngelscriptStaticJITGenerationGlobal& Global
			: Snapshot.Globals)
		{
			Storage.Globals.Add({
				Global.EngineLocalGlobal,
				Global.EngineLocalGlobalPropertyId,
				Global.GlobalKey.Hash,
				Global.CanonicalDeclaration});
		}

		Storage.Descriptors.Reserve(Snapshot.Descriptors.Num());
		for (const FAngelscriptStaticJITGenerationDescriptor& Descriptor
			: Snapshot.Descriptors)
		{
			Storage.Descriptors.Add({
				Descriptor.EngineLocalDescriptor,
				Descriptor.StableDescriptorKey,
				Descriptor.EntryPlan.EntryPlanHash,
				Descriptor.CanonicalDeclaration,
				MapDescriptorKind(Descriptor.Kind),
				Descriptor.CanonicalName,
				Descriptor.ReceiverCanonicalName,
				Descriptor.bUFunctionRoot,
				Descriptor.EntryPlan.bHasVMEntry,
				Descriptor.EntryPlan.bHasRawEntry,
				Descriptor.EntryPlan.bHasParmsEntry,
				&Descriptor.EntryPlan});
		}

		auto AppendDependencies = [](
			const TConstArrayView<FAngelscriptStaticJITGenerationDependency>
				Source,
			TArray<FAngelscriptStaticJITDependencyView>& Target)
		{
			Target.Reserve(Source.Num());
			for (const FAngelscriptStaticJITGenerationDependency& Dependency
				: Source)
			{
				Target.Add({
					Dependency.OwnerFunctionKey,
					Dependency.Reference,
					Dependency.CanonicalDeclaration});
			}
		};
		AppendDependencies(Snapshot.Dependencies, Storage.Dependencies);
		for (const FAngelscriptStaticJITGenerationSemanticDependency&
			Dependency : Snapshot.SemanticDependencies)
		{
			Storage.SemanticDependencies.Add({
				Dependency.OwnerFunctionKey,
				Dependency.Kind,
				Dependency.Reference,
				Dependency.ExpectedContentOrValue,
				Dependency.EngineLocalGlobalPropertyId});
		}
		AppendDependencies(
			Snapshot.ExternalNativeCalls, Storage.ExternalNativeCalls);
		Storage.NativeCallTargets = Snapshot.NativeCallTargets;

		Storage.Graph.Settings.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		Storage.Graph.Settings.ProviderId.Hash =
			Snapshot.Modules[0].ModuleKey.Hash;
		Storage.Graph.Settings.ProviderName = FString(ProviderName);
		Storage.Graph.Settings.OwnerModuleName = TEXT("AngelscriptTest");
		Storage.Graph.Settings.ArtifactProfile = Snapshot.ArtifactProfile;
		Storage.Graph.Settings.NativeEnvironmentFingerprint =
			FAngelscriptJITTargetProfiles::
				BuildCurrentNativeEnvironmentFingerprint(
					Storage.Graph.Settings.TargetProfile);
		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		FString ProfileError;
		check(FAngelscriptStaticJITGenerationProfile::TryCreate(
			FAngelscriptStaticJITBackendId::TypedAST(),
			Storage.Graph.Settings.TargetProfile,
			GenerationProfile, &ProfileError));
		check(ProfileError.IsEmpty());
		Storage.Graph.TypedASTJITCapabilityProfile =
			GenerationProfile.TypedASTJITCapabilityProfile;
		Storage.Graph.TypedASTJITExecutionRequirements =
			GenerationProfile.TypedASTJITExecutionRequirements;
		Storage.Graph.Modules = Storage.Modules;
		Storage.Graph.ResolvedModuleDescriptors =
			Storage.ResolvedModuleDescriptors;
		Storage.Graph.Functions = Storage.Functions;
		Storage.Graph.Types = Storage.Types;
		Storage.Graph.Globals = Storage.Globals;
		Storage.Graph.Descriptors = Storage.Descriptors;
		Storage.Graph.Dependencies = Storage.Dependencies;
		Storage.Graph.SemanticDependencies = Storage.SemanticDependencies;
		Storage.Graph.ExternalNativeCalls = Storage.ExternalNativeCalls;
		Storage.Graph.NativeCallTargets = Storage.NativeCallTargets;
		Storage.Graph.SourceCompilePassCount = Snapshot.SourceCompilePassCount;
		Storage.Graph.CaptureProfile = Snapshot.CaptureProfile;
		return Storage;
	}

	enum class EContainmentBackendMode : uint8
	{
		FailEmission = 0,
		EmitForPackaging,
	};

	class FContainmentBackendFactory;

	class FContainmentBackend final : public IAngelscriptStaticJITBackend
	{
	public:
		explicit FContainmentBackend(
			const FContainmentBackendFactory& InFactory)
			: Factory(InFactory)
		{
		}

		~FContainmentBackend() override;

		bool Generate(
			const FAngelscriptStaticJITBackendRequest& Request,
			FAngelscriptStaticJITBackendOutput& OutOutput,
			FString* OutError) override;

	private:
		const FContainmentBackendFactory& Factory;
	};

	class FContainmentBackendFactory final
		: public IAngelscriptStaticJITBackendFactory
	{
	public:
		explicit FContainmentBackendFactory(
			const EContainmentBackendMode InMode)
			: Mode(InMode)
		{
		}

		FAngelscriptStaticJITBackendId GetBackendId() const override
		{
			return FAngelscriptStaticJITBackendId::TypedAST();
		}

		TUniquePtr<IAngelscriptStaticJITBackend> CreateBackend() const override
		{
			++CreateCount;
			return MakeUnique<FContainmentBackend>(*this);
		}

		EContainmentBackendMode Mode =
			EContainmentBackendMode::FailEmission;
		mutable int32 CreateCount = 0;
		mutable int32 GenerateCount = 0;
		mutable int32 DestroyCount = 0;
		mutable int32 SeenModuleCount = 0;
		mutable int32 SeenFunctionCount = 0;
		mutable int32 SeenVerifiedHIRCount = 0;
	};

	FContainmentBackend::~FContainmentBackend()
	{
		++Factory.DestroyCount;
	}

	bool FContainmentBackend::Generate(
		const FAngelscriptStaticJITBackendRequest& Request,
		FAngelscriptStaticJITBackendOutput& OutOutput,
		FString* OutError)
	{
		++Factory.GenerateCount;
		Factory.SeenModuleCount = Request.CompiledSourceGraph.Modules.Num();
		Factory.SeenFunctionCount = Request.CompiledSourceGraph.Functions.Num();
		Factory.SeenVerifiedHIRCount =
			Request.CompiledSourceGraph.Functions.FilterByPredicate(
				[](const FAngelscriptStaticJITCompiledFunctionView& Function)
				{
					return Function.VerifiedTypedHIR != nullptr;
				}).Num();
		if (Factory.Mode == EContainmentBackendMode::FailEmission)
		{
			if (OutError != nullptr)
			{
				*OutError = TEXT("injected contained backend emission failure");
			}
			return false;
		}

		if (Request.EmitModuleSet.IsEmpty())
		{
			if (OutError != nullptr)
			{
				*OutError = TEXT("containment backend requires one emit module");
			}
			return false;
		}
		const FAngelscriptStableModuleKey& EmitModuleKey =
			Request.EmitModuleSet[0];
		const FAngelscriptStaticJITCompiledFunctionView* const Function =
			Request.CompiledSourceGraph.Functions.FindByPredicate(
				[&EmitModuleKey](
					const FAngelscriptStaticJITCompiledFunctionView& Candidate)
				{
					return Candidate.ModuleKey == EmitModuleKey
						&& Candidate.bHasVMEntry;
				});
		if (Function == nullptr)
		{
			if (OutError != nullptr)
			{
				*OutError = TEXT("containment backend found no emit function");
			}
			return false;
		}

		FAngelscriptJITGenerationModule Module;
		Module.ModuleKey = EmitModuleKey;
		Module.CanonicalModuleName = TEXT("ContainmentEmitModule");
		Module.VirtualSourcePath =
			TEXT("/Angelscript/Game/ContainmentEmitModule.as");
		OutOutput.Modules.Add(MoveTemp(Module));

		FAngelscriptJITGenerationFunction EmittedFunction;
		EmittedFunction.ModuleKey = Function->ModuleKey;
		EmittedFunction.FunctionKey = Function->FunctionKey;
		EmittedFunction.ExecutionHash = Function->ExecutionHash;
		EmittedFunction.DebugHash = Function->DebugHash;
		EmittedFunction.EntryAbiHash = Function->EntryAbiHash;
		EmittedFunction.CanonicalDeclaration =
			FString(Function->CanonicalDeclaration);
		EmittedFunction.SourceSection =
			TEXT("/Angelscript/Game/ContainmentEmitModule.as");
		EmittedFunction.SourceLine = 1;
		EmittedFunction.bHasVMEntry = true;
		EmittedFunction.ImplementationTemplate =
			TEXT("{VM_ENTRY_COMMENT}void {VM_SYMBOL}(FScriptExecution&, asDWORD*, asQWORD*) {}\n");

		FAngelscriptStaticJITFunctionResult FunctionResult;
		FunctionResult.Disposition =
			EAngelscriptStaticJITFunctionDisposition::Emitted;
		FunctionResult.ActualBackendId = Request.RequestedBackendId;
		FunctionResult.FunctionKey = Function->FunctionKey;
		FunctionResult.EmittedFunction = MoveTemp(EmittedFunction);
		OutOutput.Functions.Add(MoveTemp(FunctionResult));
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITGenerationEngineTests,
	"Angelscript.TestModule.StaticJIT.ProjectGeneration.Engine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BackendProfileIsFrozenBeforeTargetSourceCompile)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		TUniquePtr<FFixture> BytecodeFixture = CreateFixture(
			*TestRunner, TEXT("BytecodeCaptureProfile"),
			FAngelscriptStaticJITBackendId::Bytecode());
		ASSERT_THAT(IsNotNull(BytecodeFixture.Get()));
		asCScriptEngine* const BytecodeScriptEngine =
			static_cast<asCScriptEngine*>(
				BytecodeFixture->Engine->GetScriptEngine());
		ASSERT_THAT(IsFalse(
			BytecodeScriptEngine->IsTypedSemanticIRCaptureEnabled()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> BytecodeSnapshot =
			BytecodeFixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(BytecodeSnapshot.IsValid()));
		ASSERT_THAT(IsTrue(BytecodeSnapshot->CaptureProfile
			== EAngelscriptStaticJITCaptureProfile::Bytecode));
		ASSERT_THAT(IsFalse(BytecodeSnapshot->bHasVerifiedTypedHIR));
		for (const FAngelscriptStaticJITGenerationFunction& Function
			: BytecodeSnapshot->Functions)
		{
			ASSERT_THAT(IsNull(Function.VerifiedTypedHIR));
		}

		TUniquePtr<FFixture> TypedASTFixture = CreateFixture(
			*TestRunner, TEXT("TypedASTCaptureProfile"),
			FAngelscriptStaticJITBackendId::TypedAST());
		ASSERT_THAT(IsNotNull(TypedASTFixture.Get()));
		asCScriptEngine* const TypedASTScriptEngine =
			static_cast<asCScriptEngine*>(
				TypedASTFixture->Engine->GetScriptEngine());
		ASSERT_THAT(IsTrue(
			TypedASTScriptEngine->IsTypedSemanticIRCaptureEnabled()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> TypedASTSnapshot =
			TypedASTFixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(TypedASTSnapshot.IsValid()));
		ASSERT_THAT(IsTrue(TypedASTSnapshot->CaptureProfile
			== EAngelscriptStaticJITCaptureProfile::VerifiedTypedHIR));
		ASSERT_THAT(IsTrue(TypedASTSnapshot->bHasVerifiedTypedHIR));
		const FAngelscriptStaticJITGenerationFunction* const CapturedFunction =
			TypedASTSnapshot->Functions.FindByPredicate([](const auto& Function)
			{
				// This profile test deliberately selects the fully representable scalar
				// function. EvaluateGenerationEmit_65C28A also calls native Math::Abs,
				// whose typed call capture belongs to a later call-form milestone.
				return Function.CanonicalDeclaration.Contains(
					TEXT("IncrementGenerationEmit_65C28A"));
			});
		ASSERT_THAT(IsNotNull(CapturedFunction));
		ASSERT_THAT(IsNotNull(CapturedFunction->VerifiedTypedHIR));
		const asCScriptFunction* const EngineOwnedFunction =
			static_cast<const asCScriptFunction*>(
				CapturedFunction->EngineLocalFunction);
		ASSERT_THAT(IsTrue(CapturedFunction->VerifiedTypedHIR
			== EngineOwnedFunction->GetTypedSemanticFunction(),
			TEXT("The backend view must use HIR owned by the same source compile")));
		TArray<FString> PersistedHIRFiles;
		IFileManager::Get().FindFilesRecursive(
			PersistedHIRFiles, *TypedASTFixture->Root,
			TEXT("*.hir.txt"), true, false, false);
		IFileManager::Get().FindFilesRecursive(
			PersistedHIRFiles, *TypedASTFixture->Root,
			TEXT("*.hir.json"), true, false, false);
		ASSERT_THAT(IsTrue(PersistedHIRFiles.IsEmpty(),
			TEXT("Generation must not create a persisted HIR dump or a second HIR input")));
	}

	TEST_METHOD(FoldedGlobalKeepsStableHardValueDependencyThroughTypedASTEmission)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("FoldedGlobalHardValueDependency"),
			FAngelscriptStaticJITBackendId::TypedAST(), FStringView(), true, {},
			TEXTVIEW(R"AS(
const int TypedASTFoldedGlobal_65C28A = 7;

UCLASS()
class UTypedASTFoldedGlobalOwner_65C28A : UObject
{
	UFUNCTION()
	int KeepTypedASTFoldedGlobalSchema_65C28A(int Value)
	{
		return Value;
	}

	UFUNCTION()
	int ReadTypedASTFoldedGlobal_65C28A() final
	{
		return TypedASTFoldedGlobal_65C28A;
	}
}
)AS"), {}, true);
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));

		const FAngelscriptStaticJITGenerationFunction* const Reader =
			Snapshot->Functions.FindByPredicate([](const auto& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("ReadTypedASTFoldedGlobal_65C28A"));
			});
		const FAngelscriptStaticJITGenerationGlobal* const Global =
			Snapshot->Globals.FindByPredicate([](const auto& Candidate)
			{
				return Candidate.CanonicalDeclaration.Contains(
					TEXT("TypedASTFoldedGlobal_65C28A"));
			});
		ASSERT_THAT(IsNotNull(Reader));
		ASSERT_THAT(IsNotNull(Reader->VerifiedTypedHIR));
		ASSERT_THAT(IsTrue(Reader->bHasVerifiedArtifactReferences));
		ASSERT_THAT(IsNotNull(Global));
		ASSERT_THAT(IsTrue(Global->EngineLocalGlobalPropertyId >= 0));
		ASSERT_THAT(IsFalse(Global->GlobalKey.Hash.IsZero()));

		const asSTypedSemanticExpression* FoldedUse = nullptr;
		for (asUINT Index = 0;
			Index < Reader->VerifiedTypedHIR->expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression =
				Reader->VerifiedTypedHIR->expressions[Index];
			if (Expression.kind
				== asETypedSemanticExpressionKind::FoldedGlobalConstant)
			{
				FoldedUse = &Expression;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(FoldedUse,
			TEXT("The production compiler must retain the folded global origin in HIR")));
		ASSERT_THAT(IsTrue(FoldedUse->resolvedGlobalPropertyId
			== Global->EngineLocalGlobalPropertyId));
		ASSERT_THAT(AreEqual(asQWORD{7}, FoldedUse->literalBits));

		const FAngelscriptStaticJITGenerationDependency* const FlatGlobal =
			Snapshot->Dependencies.FindByPredicate(
				[Reader](const auto& Dependency)
				{
					return Dependency.OwnerFunctionKey == Reader->FunctionKey
						&& Dependency.Reference.Kind
							== EAngelscriptArtifactReferenceKind::ScriptGlobal;
				});
		ASSERT_THAT(IsNotNull(FlatGlobal,
			TEXT("The verified flat reference set must retain the folded global")));
		const FAngelscriptStaticJITGenerationSemanticDependency* const
			AnyHardValue = Snapshot->SemanticDependencies.FindByPredicate(
				[](const auto& Dependency)
				{
					return Dependency.Kind
						== EAngelscriptArtifactSemanticDependencyKind::HardValue;
				});
		ASSERT_THAT(IsNotNull(AnyHardValue,
			TEXT("At least one Cache hard-value row must survive snapshot freeze")));
		const FAngelscriptStaticJITGenerationSemanticDependency* const
			HardValue = Snapshot->SemanticDependencies.FindByPredicate(
				[Reader](const auto& Dependency)
				{
					return Dependency.OwnerFunctionKey == Reader->FunctionKey
						&& Dependency.Kind
							== EAngelscriptArtifactSemanticDependencyKind::HardValue;
				});
		ASSERT_THAT(IsNotNull(HardValue,
			TEXT("The immutable snapshot must retain the Cache-authoritative hard-value row")));
		ASSERT_THAT(IsTrue(HardValue->Reference.Kind
			== EAngelscriptArtifactReferenceKind::ScriptGlobal));
		ASSERT_THAT(IsTrue(HardValue->Reference.StableKey
			== Global->GlobalKey.Hash,
			TEXT("Generation global identity must exactly match the Cache-authoritative dependency key")));
		ASSERT_THAT(IsTrue(HardValue->EngineLocalGlobalPropertyId
			== Global->EngineLocalGlobalPropertyId));
		ASSERT_THAT(IsFalse(HardValue->Reference.ExpectedAbi.IsZero()));
		ASSERT_THAT(IsTrue(HardValue->ExpectedContentOrValue.IsSet()));
		ASSERT_THAT(IsFalse(
			HardValue->ExpectedContentOrValue.GetValue().IsZero()));

		FCompiledGraphStorage GraphStorage = BuildCompiledGraphStorage(
			*Snapshot, TEXTVIEW("Folded Global Hard Value"));
		const FAngelscriptStableModuleKey EmitModuleSet[] = {
			Reader->ModuleKey};
		const FAngelscriptStaticJITBackendRequest BackendRequest{
			FAngelscriptStaticJITBackendId::TypedAST(),
			GraphStorage.Graph,
			EmitModuleSet,
		};
		FAngelscriptTypedASTJIT Backend;
		FAngelscriptStaticJITBackendOutput BackendOutput;
		FString Error;
		ASSERT_THAT(IsTrue(Backend.Generate(
			BackendRequest, BackendOutput, &Error), Error));
		const FAngelscriptStaticJITFunctionResult* const Result =
			BackendOutput.Functions.FindByPredicate(
				[Reader](const auto& Candidate)
				{
					return Candidate.FunctionKey == Reader->FunctionKey;
				});
		ASSERT_THAT(IsNotNull(Result));
		ASSERT_THAT(IsTrue(Result->Disposition
			== EAngelscriptStaticJITFunctionDisposition::Emitted,
			Result->Detail));
		ASSERT_THAT(IsTrue(Result->EmittedFunction.IsSet()));
		ASSERT_THAT(IsTrue(
			Result->EmittedFunction->ImplementationTemplate.Contains(
				TEXT("FromCanonicalBits<int32>(UINT64_C(7))"))));
		ASSERT_THAT(IsTrue(Result->EmittedFunction->References.ContainsByPredicate(
			[Global](const FAngelscriptJITGenerationReference& Reference)
			{
				return Reference.Reference.Kind
						== EAngelscriptArtifactReferenceKind::ScriptGlobal
					&& Reference.Reference.StableKey == Global->GlobalKey.Hash
					&& !Reference.Reference.ExpectedAbi.IsZero();
			})));
	}

	TEST_METHOD(LiteralAssetRolesUseAuthoritativePostInitPairs)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("LiteralAssetRoles"),
			FAngelscriptStaticJITBackendId::TypedAST(),
			TEXTVIEW(R"AS(
namespace LiteralAssetRoleDecoy
{
	UObject GetPrimaryTypedAsset()
	{
		return nullptr;
	}

	void __Init_PrimaryTypedAsset(UObject Value) external_implicit_this
	{
	}
}

asset PrimaryTypedAsset of UObject
{
}

asset SecondaryTypedAsset of UObject
{
}
)AS"));
		ASSERT_THAT(IsNotNull(Fixture.Get()));

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		const FAngelscriptStaticJITGenerationModule* const Module =
			Snapshot->Modules.FindByPredicate(
				[](const FAngelscriptStaticJITGenerationModule& Candidate)
				{
					return Candidate.EngineLocalDescriptor != nullptr
						&& Candidate.EngineLocalDescriptor->
							LiteralAssetFunctions.Num() == 2;
				});
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(IsNotNull(Module->EngineLocalDescriptor));
		ASSERT_THAT(AreEqual(2,
			Module->EngineLocalDescriptor->LiteralAssetFunctions.Num()));
		ASSERT_THAT(AreEqual(2,
			Module->EngineLocalDescriptor->PostInitFunctions.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("GetPrimaryTypedAsset")),
			Module->EngineLocalDescriptor->PostInitFunctions[0]));
		ASSERT_THAT(AreEqual(FString(TEXT("GetSecondaryTypedAsset")),
			Module->EngineLocalDescriptor->PostInitFunctions[1]));
		ASSERT_THAT(AreEqual(0,
			Module->EngineLocalDescriptor->LiteralAssetFunctions[0].
				PostInitFunctionIndex));
		ASSERT_THAT(AreEqual(FString(TEXT("GetPrimaryTypedAsset")),
			Module->EngineLocalDescriptor->LiteralAssetFunctions[0].
				GetterFunctionName));
		ASSERT_THAT(AreEqual(FString(TEXT("__Init_PrimaryTypedAsset")),
			Module->EngineLocalDescriptor->LiteralAssetFunctions[0].
				InitializerFunctionName));

		auto FindFunction = [&Snapshot](
			const ANSICHAR* Name,
			const ANSICHAR* Namespace)
			-> const FAngelscriptStaticJITGenerationFunction*
		{
			return Snapshot->Functions.FindByPredicate(
				[Name, Namespace](
					const FAngelscriptStaticJITGenerationFunction& Candidate)
				{
					const asIScriptFunction* const Function =
						Candidate.EngineLocalFunction;
					return Function != nullptr
						&& FCStringAnsi::Strcmp(Function->GetName(), Name) == 0
						&& FCStringAnsi::Strcmp(
							Function->GetNamespace(), Namespace) == 0;
				});
		};

		const FAngelscriptStaticJITGenerationFunction* const Getter =
			FindFunction("GetPrimaryTypedAsset", "");
		const FAngelscriptStaticJITGenerationFunction* const Initializer =
			FindFunction("__Init_PrimaryTypedAsset", "");
		const FAngelscriptStaticJITGenerationFunction* const DecoyGetter =
			FindFunction("GetPrimaryTypedAsset", "LiteralAssetRoleDecoy");
		const FAngelscriptStaticJITGenerationFunction* const DecoyInitializer =
			FindFunction("__Init_PrimaryTypedAsset", "LiteralAssetRoleDecoy");
		ASSERT_THAT(IsNotNull(Getter));
		ASSERT_THAT(IsNotNull(Initializer));
		ASSERT_THAT(IsNotNull(DecoyGetter));
		ASSERT_THAT(IsNotNull(DecoyInitializer));
		ASSERT_THAT(IsTrue(Getter->LiteralAssetRole
			== EAngelscriptStaticJITLiteralAssetFunctionRole::Getter));
		ASSERT_THAT(IsTrue(Initializer->LiteralAssetRole
			== EAngelscriptStaticJITLiteralAssetFunctionRole::Initializer));
		ASSERT_THAT(IsTrue(DecoyGetter->LiteralAssetRole
			== EAngelscriptStaticJITLiteralAssetFunctionRole::None));
		ASSERT_THAT(IsTrue(DecoyInitializer->LiteralAssetRole
			== EAngelscriptStaticJITLiteralAssetFunctionRole::None));

		const asCScriptFunction* const InitializerFunction =
			static_cast<const asCScriptFunction*>(
				Initializer->EngineLocalFunction);
		ASSERT_THAT(IsTrue(InitializerFunction->traits.GetTrait(
			asTRAIT_EXTERNAL_IMPLICIT_THIS)));
		ASSERT_THAT(IsNotNull(Initializer->VerifiedTypedHIR));
		ASSERT_THAT(IsTrue(Initializer->VerifiedTypedHIR->header.receiver.kind
			== asETypedSemanticReceiverKind::ExternalImplicitThis));
		ASSERT_THAT(AreEqual(0,
			Initializer->VerifiedTypedHIR->header.receiver.parameterIndex));

		FCompiledGraphStorage GraphStorage = BuildCompiledGraphStorage(
			*Snapshot, TEXTVIEW("LiteralAssetRoleProvider"));
		for (const FAngelscriptStaticJITCompiledFunctionView& Function
			: GraphStorage.Functions)
		{
			if (Function.LiteralAssetRole
				== EAngelscriptStaticJITLiteralAssetFunctionRole::None)
			{
				continue;
			}
			FAngelscriptTypedASTJITEligibilityInput Input =
				BuildAngelscriptTypedASTJITEligibilityInput(nullptr, Function);
			Input.Purpose = EAngelscriptTypedASTJITEligibilityPurpose::
				InternalHelperCallClosure;
			const FAngelscriptTypedASTJITEligibility Eligibility =
				EvaluateAngelscriptTypedASTJITEligibility(Input);
			ASSERT_THAT(IsFalse(Eligibility.bEligible,
				TEXT("The scalar profile must keep literal-asset helpers on VM fallback")));
		}
	}

	TEST_METHOD(ReviewedUEHeaderInlineCallIsFrozenIntoGenerationSnapshot)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("ReviewedUEHeaderInlineCall"),
			FAngelscriptStaticJITBackendId::TypedAST(),
			TEXTVIEW(R"AS(
UFUNCTION()
bool InvokeReviewedUEHeaderInline_65C28A(int Ignored)
{
	return IsRunningCommandlet();
}
)AS"));
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));

		const FAngelscriptStaticJITGenerationFunction* const Caller =
			Snapshot->Functions.FindByPredicate([](const auto& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("InvokeReviewedUEHeaderInline_65C28A"));
			});
		ASSERT_THAT(IsNotNull(Caller));
		ASSERT_THAT(IsNotNull(Caller->VerifiedTypedHIR));
		int32 NativeFunctionId = INDEX_NONE;
		asTypedSemanticExpressionId NativeCallExpression;
		for (asUINT Index = 0;
			Index < Caller->VerifiedTypedHIR->expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression =
				Caller->VerifiedTypedHIR->expressions[Index];
			if (Expression.kind
					== asETypedSemanticExpressionKind::ResolvedCall
				&& Expression.callTargetKind
					== asETypedSemanticCallTargetKind::SystemFunction)
			{
				NativeFunctionId = Expression.resolvedFunctionId;
				NativeCallExpression = Expression.id;
				break;
			}
		}
		ASSERT_THAT(IsTrue(NativeFunctionId >= 0));
		const FAngelscriptStaticJITNativeCallTarget* const NativeTarget =
			Snapshot->NativeCallTargets.FindByPredicate(
				[NativeFunctionId](const auto& Candidate)
				{
					return Candidate.EngineLocalFunctionId
						== NativeFunctionId;
				});
		ASSERT_THAT(IsNotNull(NativeTarget));
		ASSERT_THAT(AreEqual(
			FString(TEXT("IsRunningCommandlet")),
			NativeTarget->Descriptor.Symbol));
		ASSERT_THAT(AreEqual(
			FString(TEXT("CoreGlobals.h")),
			NativeTarget->Descriptor.Include));
		ASSERT_THAT(IsTrue(
			NativeTarget->Descriptor.OwningModule == FName(TEXT("Core"))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("cdecl:bool()")),
			NativeTarget->ExpectedScalarABI.Canonical));
		ASSERT_THAT(IsTrue(
			NativeTarget->Descriptor.ScalarABI
				== NativeTarget->ExpectedScalarABI));

		FCompiledGraphStorage GraphStorage = BuildCompiledGraphStorage(
			*Snapshot, TEXTVIEW("Reviewed UE Header Inline"));
		GraphStorage.Graph.Settings.KnownNativeCallModules.Add(TEXT("Core"));
		GraphStorage.Graph.Settings.PublicNativeCallDependencyModules.Add(
			TEXT("Core"));
		FAngelscriptTypedASTJITRootIndex RootIndex;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITRootIndex(
			GraphStorage.Graph, RootIndex, &Error), Error));
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput>
			ClosureInputs;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosureInputs(
			GraphStorage.Graph, RootIndex, ClosureInputs, &Error), Error));
		FAngelscriptTypedASTJITCallClosureOptions ClosureOptions;
		ClosureOptions.ExecutionRequirements =
			GraphStorage.Graph.TypedASTJITExecutionRequirements;
		ClosureOptions.NativeCallTargets = Snapshot->NativeCallTargets;
		ClosureOptions.KnownNativeCallModules =
			GraphStorage.Graph.Settings.KnownNativeCallModules;
		ClosureOptions.PublicNativeCallDependencyModules =
			GraphStorage.Graph.Settings.PublicNativeCallDependencyModules;
		FAngelscriptTypedASTJITCallClosurePlan ClosurePlan;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			ClosureInputs, Caller->FunctionKey, ClosureOptions,
			ClosurePlan, &Error), Error));
		ASSERT_THAT(IsTrue(ClosurePlan.bEligible));
		const FAngelscriptTypedASTJITCallClosureCall* const PlannedCall =
			ClosurePlan.Calls.FindByPredicate(
				[NativeCallExpression](const auto& Candidate)
				{
					return Candidate.Expression == NativeCallExpression;
				});
		ASSERT_THAT(IsNotNull(PlannedCall));
		ASSERT_THAT(IsTrue(PlannedCall->Disposition
			== EAngelscriptTypedASTJITCallDisposition::DirectInline));

		FAngelscriptTypedASTJITNativeCallEmissionPlan NativeEmissionPlan;
		NativeEmissionPlan.Expression = PlannedCall->Expression;
		NativeEmissionPlan.EngineLocalTargetFunctionId =
			PlannedCall->EngineLocalTargetFunctionId;
		NativeEmissionPlan.Symbol =
			PlannedCall->ExternalCallDescriptor.Symbol;
		NativeEmissionPlan.Include =
			PlannedCall->ExternalCallDescriptor.Include;
		FAngelscriptTypedASTJITEmitOptions EmitOptions;
		EmitOptions.NativeCalls =
			MakeArrayView(&NativeEmissionPlan, 1);
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("AS_Test_Semantic_InvokeReviewedUEHeaderInline");
		const asCScriptFunction* const CallerFunction =
			static_cast<const asCScriptFunction*>(Caller->EngineLocalFunction);
		Shape.ReturnType = CallerFunction->returnType;
		Shape.InvocationKind = Caller->VerifiedTypedHIR->header.invocationKind;
		Shape.ReceiverKind = Caller->VerifiedTypedHIR->header.receiver.kind;
		Shape.bReturnsOnStack = Caller->VerifiedTypedHIR->header.returnsOnStack;
		for (asUINT Index = 0;
			Index < CallerFunction->parameterTypes.GetLength(); ++Index)
		{
			Shape.ParameterTypes.Add(CallerFunction->parameterTypes[Index]);
		}
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(
				*Caller->VerifiedTypedHIR, Shape, EmitOptions);
		ASSERT_THAT(IsTrue(Emission.bSuccess, Emission.FailureDetail));
		ASSERT_THAT(IsTrue(Emission.RequiredIncludes.Contains(
			TEXT("CoreGlobals.h"))));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("return IsRunningCommandlet();"))));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(
			TEXT("FAngelscriptJITExecutionContext"))));

		const FAngelscriptStableModuleKey EmitModuleSet[] = {
			Caller->ModuleKey};
		const FAngelscriptStaticJITBackendRequest BackendRequest{
			FAngelscriptStaticJITBackendId::TypedAST(),
			GraphStorage.Graph,
			EmitModuleSet,
		};
		FAngelscriptTypedASTJIT Backend;
		FAngelscriptStaticJITBackendOutput BackendOutput;
		ASSERT_THAT(IsTrue(Backend.Generate(
			BackendRequest, BackendOutput, &Error), Error));
		const FAngelscriptStaticJITFunctionResult* const BackendResult =
			BackendOutput.Functions.FindByPredicate(
				[Caller](const auto& Candidate)
				{
					return Candidate.FunctionKey == Caller->FunctionKey;
				});
		ASSERT_THAT(IsNotNull(BackendResult));
		ASSERT_THAT(IsTrue(BackendResult->Detail.Contains(
			TEXT("TypedASTJITCallClosurePassed")),
			TEXT("The production backend must consume the generated consumer's exact public dependencies")));
		ASSERT_THAT(IsTrue(BackendResult->Disposition
			== EAngelscriptStaticJITFunctionDisposition::Emitted,
			TEXT("An eligible typed root must become a real Provider function")));
		ASSERT_THAT(IsTrue(BackendResult->EmittedFunction.IsSet()));
		const FAngelscriptJITGenerationFunction& ProviderFunction =
			BackendResult->EmittedFunction.GetValue();
		ASSERT_THAT(IsTrue(ProviderFunction.bHasVMEntry
			== Caller->bHasVMEntry));
		ASSERT_THAT(IsTrue(ProviderFunction.bHasRawEntry
			== Caller->bHasRawEntry));
		ASSERT_THAT(IsTrue(ProviderFunction.bHasParmsEntry
			== Caller->bHasParmsEntry));
		ASSERT_THAT(IsTrue(ProviderFunction.EntryAbiHash
			== Caller->EntryAbiHash));
		ASSERT_THAT(IsTrue(ProviderFunction.ImplementationTemplate.Contains(
			TEXT("{SYMBOL_PREFIX}_TypedBody"))));
		ASSERT_THAT(IsTrue(ProviderFunction.ImplementationTemplate.Contains(
			TEXT("return IsRunningCommandlet();"))));
		ASSERT_THAT(IsTrue(ProviderFunction.ImplementationTemplate.Contains(
			TEXT("{RAW_ENTRY_COMMENT}"))));
		ASSERT_THAT(IsTrue(ProviderFunction.ImplementationTemplate.Contains(
			TEXT("{VM_ENTRY_COMMENT}"))));
		ASSERT_THAT(IsTrue(ProviderFunction.ImplementationTemplate.Contains(
			TEXT("{PARMS_ENTRY_COMMENT}"))
			== ProviderFunction.bHasParmsEntry));
		ASSERT_THAT(IsTrue(BackendOutput.ModuleSourcePreamble.Contains(
			TEXT("#include \"CoreGlobals.h\""))));
		ASSERT_THAT(IsFalse(ProviderFunction.ImplementationTemplate.Contains(
			TEXT("FAngelscriptJITExecutionContext"))));

		ASSERT_THAT(IsTrue(RegisterAngelscriptTypedASTJITBackend(&Error), Error));
		FAngelscriptStaticJITGenerator Generator(
			GetAngelscriptStaticJITBackendRegistry());
		FAngelscriptStaticJITGeneratorOutput GeneratorOutput;
		ASSERT_THAT(IsTrue(Generator.Generate(
			BackendRequest, GeneratorOutput, &Error), Error));
		ASSERT_THAT(IsTrue(GeneratorOutput.bHasProviderOutput));
		const FAngelscriptJITGeneratedFunction* const PackagedFunction =
			GeneratorOutput.ProviderOutput.Functions.FindByPredicate(
				[Caller](const auto& Candidate)
				{
					return Candidate.FunctionKey == Caller->FunctionKey;
				});
		ASSERT_THAT(IsNotNull(PackagedFunction));
		const FAngelscriptJITGeneratedFile* const PackagedModuleSource =
			GeneratorOutput.ProviderOutput.Files.FindByPredicate(
				[](const auto& Candidate)
				{
					return Candidate.Kind
						== EAngelscriptJITGeneratedFileKind::ModuleSource;
				});
		ASSERT_THAT(IsNotNull(PackagedModuleSource));
		ASSERT_THAT(IsTrue(PackagedModuleSource->Content.Contains(
			TEXT("#include \"CoreGlobals.h\""))));
		ASSERT_THAT(IsTrue(PackagedModuleSource->Content.Contains(
			TEXT("return IsRunningCommandlet();"))));
		ASSERT_THAT(IsTrue(PackagedModuleSource->Content.Contains(
			PackagedFunction->RawEntrySymbol)));
		ASSERT_THAT(IsTrue(PackagedModuleSource->Content.Contains(
			PackagedFunction->VMEntrySymbol)));
		ASSERT_THAT(IsTrue(PackagedModuleSource->Content.Contains(
			PackagedFunction->ParmsEntrySymbol)
			== PackagedFunction->bHasParmsEntry));
		ASSERT_THAT(IsFalse(PackagedModuleSource->Content.Contains(
			TEXT("{SYMBOL_PREFIX}"))));
		ASSERT_THAT(IsFalse(PackagedModuleSource->Content.Contains(
			TEXT("{RAW_SYMBOL}"))));
		ASSERT_THAT(IsFalse(PackagedModuleSource->Content.Contains(
			TEXT("FAngelscriptJITExecutionContext"))));
	}

	TEST_METHOD(HiddenFirstParameterMetadataRemainsOnAuthoritativeVMFallback)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("HiddenFirstParameterMetadataFallback"),
			FAngelscriptStaticJITBackendId::TypedAST(),
			FStringView(), true, {}, TEXTVIEW(R"AS(
UFUNCTION()
int TypedASTHiddenFirstParameterMetadata_65C28A(int Value)
{
	UObject::StaticClass();
	return Value;
}
)AS"));
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));

		const FAngelscriptStaticJITGenerationFunction* const Root =
			Snapshot->Functions.FindByPredicate([](const auto& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("TypedASTHiddenFirstParameterMetadata_65C28A"));
			});
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Root->VerifiedTypedHIR));

		int32 MetadataFunctionId = INDEX_NONE;
		for (asUINT ExpressionIndex = 0;
			ExpressionIndex < Root->VerifiedTypedHIR->expressions.GetLength();
			++ExpressionIndex)
		{
			const asSTypedSemanticExpression& Expression =
				Root->VerifiedTypedHIR->expressions[ExpressionIndex];
			if (Expression.kind
					== asETypedSemanticExpressionKind::ResolvedCall
				&& Expression.callTargetKind
					== asETypedSemanticCallTargetKind::SystemFunction)
			{
				MetadataFunctionId = Expression.resolvedFunctionId;
				break;
			}
		}
		ASSERT_THAT(AreNotEqual(INDEX_NONE, MetadataFunctionId));
		const FAngelscriptStaticJITNativeCallInventoryEntry* const MetadataEntry =
			Snapshot->NativeCallInventory.FindByPredicate(
				[MetadataFunctionId](const auto& Entry)
				{
					return Entry.EngineLocalFunctionId == MetadataFunctionId;
				});
		ASSERT_THAT(IsNotNull(MetadataEntry));
		ASSERT_THAT(IsTrue(MetadataEntry->CanonicalDeclaration.Contains(
			TEXT("StaticClass"))));
		ASSERT_THAT(IsTrue(MetadataEntry->FirstParamMetadata
			== EAngelscriptStaticJITInstalledFirstParamMetadata::ScriptFunction));
		ASSERT_THAT(IsTrue(MetadataEntry->bHasExpectedNativeABI,
			*MetadataEntry->NativeABIError));
		ASSERT_THAT(IsTrue(MetadataEntry->ExpectedNativeABI.Canonical.Contains(
			TEXT("firstMetadata=1"))));
		ASSERT_THAT(IsFalse(MetadataEntry->bHasExpectedScalarABI));
		ASSERT_THAT(IsTrue(MetadataEntry->ScalarABIError.Contains(
			TEXT("NativeScalarABIUnsupportedHiddenState"))));
		ASSERT_THAT(IsNull(Snapshot->NativeCallTargets.FindByPredicate(
			[MetadataEntry](const auto& Candidate)
			{
				return Candidate.EngineLocalFunctionId
					== MetadataEntry->EngineLocalFunctionId;
			}),
			TEXT("A hidden VM metadata argument must not be admitted as an automatic scalar or current-native target")));

		FCompiledGraphStorage GraphStorage = BuildCompiledGraphStorage(
			*Snapshot, TEXTVIEW("Hidden First Parameter Metadata Fallback"));
		const FAngelscriptStableModuleKey EmitModuleSet[] = {
			Root->ModuleKey};
		const FAngelscriptStaticJITBackendRequest BackendRequest{
			FAngelscriptStaticJITBackendId::TypedAST(),
			GraphStorage.Graph,
			EmitModuleSet,
		};
		FAngelscriptTypedASTJIT Backend;
		FAngelscriptStaticJITBackendOutput BackendOutput;
		FString Error;
		ASSERT_THAT(IsTrue(Backend.Generate(
			BackendRequest, BackendOutput, &Error), Error));
		const FAngelscriptStaticJITFunctionResult* const RootResult =
			BackendOutput.Functions.FindByPredicate(
				[Root](const auto& Candidate)
				{
					return Candidate.FunctionKey == Root->FunctionKey;
				});
		ASSERT_THAT(IsNotNull(RootResult));
		ASSERT_THAT(IsTrue(RootResult->Disposition
			== EAngelscriptStaticJITFunctionDisposition::Unsupported,
			RootResult->Detail));
		ASSERT_THAT(IsTrue(!RootResult->Detail.IsEmpty()));
		ASSERT_THAT(IsFalse(RootResult->EmittedFunction.IsSet(),
			TEXT("Fallback must emit no C++ body that could bake a function pointer or fabricate an AS-visible metadata parameter")));
		ASSERT_THAT(IsFalse(BackendOutput.ModuleSourcePreamble.Contains(
			TEXT("StaticClass"))));
		ASSERT_THAT(IsFalse(BackendOutput.ModuleSourcePreamble.Contains(
			TEXT("asCScriptFunction"))));
	}

	TEST_METHOD(ProductionTypedASTBackendEmitsDirectScriptHelperInOwningModule)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("TypedASTDirectScriptHelper"),
			FAngelscriptStaticJITBackendId::TypedAST(),
			FStringView(), true, {}, TEXTVIEW(R"AS(
int TypedASTDirectHelper_65C28A(int A, int B)
{
	return A * 10 + B;
}

UFUNCTION()
int TypedASTDirectHelperEntry_65C28A(int A, int B)
{
	return TypedASTDirectHelper_65C28A(B, A) + 3;
}
)AS"));
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));

		const FAngelscriptStaticJITGenerationFunction* const Root =
			Snapshot->Functions.FindByPredicate([](const auto& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("TypedASTDirectHelperEntry_65C28A"));
			});
		const FAngelscriptStaticJITGenerationFunction* const Helper =
			Snapshot->Functions.FindByPredicate([](const auto& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("TypedASTDirectHelper_65C28A"))
					&& !Function.CanonicalDeclaration.Contains(
						TEXT("TypedASTDirectHelperEntry_65C28A"));
			});
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Helper));
		ASSERT_THAT(IsNotNull(Root->VerifiedTypedHIR));
		ASSERT_THAT(IsNotNull(Helper->VerifiedTypedHIR));
		const FAngelscriptStaticJITGenerationDependency* const HelperReference =
			Snapshot->Dependencies.FindByPredicate(
				[Root, Helper](const auto& Dependency)
				{
					return Dependency.OwnerFunctionKey == Root->FunctionKey
						&& Dependency.Reference.Kind
							== EAngelscriptArtifactReferenceKind::ScriptFunction
						&& Dependency.Reference.StableKey
							== Helper->FunctionKey.Hash;
				});
		ASSERT_THAT(IsNotNull(HelperReference,
			TEXT("The frozen root dependency set must authoritatively identify its direct helper")));
		ASSERT_THAT(IsTrue(!HelperReference->Reference.ExpectedAbi.IsZero()));

		FCompiledGraphStorage GraphStorage = BuildCompiledGraphStorage(
			*Snapshot, TEXTVIEW("TypedAST Direct Script Helper"));
		const FAngelscriptStableModuleKey EmitModuleSet[] = {
			Root->ModuleKey};
		const FAngelscriptStaticJITBackendRequest BackendRequest{
			FAngelscriptStaticJITBackendId::TypedAST(),
			GraphStorage.Graph,
			EmitModuleSet,
		};
		FAngelscriptTypedASTJIT Backend;
		FAngelscriptStaticJITBackendOutput BackendOutput;
		FString Error;
		ASSERT_THAT(IsTrue(Backend.Generate(
			BackendRequest, BackendOutput, &Error), Error));
		const FAngelscriptStaticJITFunctionResult* const RootResult =
			BackendOutput.Functions.FindByPredicate(
				[Root](const auto& Candidate)
				{
					return Candidate.FunctionKey == Root->FunctionKey;
				});
		const FAngelscriptStaticJITFunctionResult* const HelperResult =
			BackendOutput.Functions.FindByPredicate(
				[Helper](const auto& Candidate)
				{
					return Candidate.FunctionKey == Helper->FunctionKey;
				});
		ASSERT_THAT(IsNotNull(RootResult));
		ASSERT_THAT(IsNotNull(HelperResult));
		ASSERT_THAT(IsTrue(RootResult->Disposition
			== EAngelscriptStaticJITFunctionDisposition::Emitted,
			RootResult->Detail));
		ASSERT_THAT(IsTrue(RootResult->EmittedFunction.IsSet()));
		ASSERT_THAT(IsTrue(HelperResult->Disposition
			!= EAngelscriptStaticJITFunctionDisposition::Emitted,
			TEXT("An internal helper must not publish an independent Provider entry")));
		ASSERT_THAT(IsFalse(HelperResult->EmittedFunction.IsSet()));

		const FAngelscriptJITGenerationFunction& ProviderFunction =
			RootResult->EmittedFunction.GetValue();
		const FAngelscriptJITGenerationReference* const GeneratedHelperReference =
			ProviderFunction.References.FindByPredicate(
				[Helper](const auto& Candidate)
				{
					return Candidate.Reference.Kind
							== EAngelscriptArtifactReferenceKind::ScriptFunction
						&& Candidate.Reference.StableKey
							== Helper->FunctionKey.Hash;
				});
		ASSERT_THAT(IsNotNull(GeneratedHelperReference));
		ASSERT_THAT(IsTrue(ProviderFunction.ImplementationTemplate.Contains(
			TEXT("ASJIT_Helper_TypedASTDirectHelper_65C28A_"))));
		ASSERT_THAT(IsTrue(ProviderFunction.ImplementationTemplate.Contains(
			TEXT("ASJIT_HelperBody_TypedASTDirectHelper_65C28A_"))));
		ASSERT_THAT(IsTrue(ProviderFunction.ImplementationTemplate.Contains(
			TEXT("GetScriptFunction(\n\t\t\tExecution,"))));
		ASSERT_THAT(IsTrue(ProviderFunction.ImplementationTemplate.Contains(
			TEXT("FScopeTypedASTJITExecutionFrame TypedFrame"))));
		ASSERT_THAT(IsTrue(ProviderFunction.ImplementationTemplate.Contains(
			TEXT("Execution.bExceptionThrown"))));
		ASSERT_THAT(IsFalse(ProviderFunction.ImplementationTemplate.Contains(
			TEXT("FAngelscriptJITExecutionContext"))));

		ASSERT_THAT(IsTrue(RegisterAngelscriptTypedASTJITBackend(&Error), Error));
		FAngelscriptStaticJITGenerator Generator(
			GetAngelscriptStaticJITBackendRegistry());
		FAngelscriptStaticJITGeneratorOutput GeneratorOutput;
		ASSERT_THAT(IsTrue(Generator.Generate(
			BackendRequest, GeneratorOutput, &Error), Error));
		const FAngelscriptJITGeneratedFunction* const PackagedRoot =
			GeneratorOutput.ProviderOutput.Functions.FindByPredicate(
				[Root](const auto& Candidate)
				{
					return Candidate.FunctionKey == Root->FunctionKey;
				});
		const FAngelscriptJITGeneratedFunction* const PackagedHelperFallback =
			GeneratorOutput.ProviderOutput.Functions.FindByPredicate(
				[Helper](const auto& Candidate)
				{
					return Candidate.FunctionKey == Helper->FunctionKey;
				});
		const FAngelscriptStaticJITFunctionResult* const PackagedRootResult =
			GeneratorOutput.Functions.FindByPredicate(
				[Root](const auto& Candidate)
				{
					return Candidate.FunctionKey == Root->FunctionKey;
				});
		const FAngelscriptStaticJITFunctionResult* const PackagedHelperResult =
			GeneratorOutput.Functions.FindByPredicate(
				[Helper](const auto& Candidate)
				{
					return Candidate.FunctionKey == Helper->FunctionKey;
				});
		ASSERT_THAT(IsNotNull(PackagedRoot));
		ASSERT_THAT(IsNotNull(PackagedHelperFallback));
		ASSERT_THAT(IsNotNull(PackagedRootResult));
		ASSERT_THAT(IsNotNull(PackagedHelperResult));
		ASSERT_THAT(IsTrue(PackagedRootResult->ActualBackendId
			== FAngelscriptStaticJITBackendId::TypedAST()));
		ASSERT_THAT(IsTrue(PackagedHelperResult->ActualBackendId
			== FAngelscriptStaticJITBackendId::Bytecode(),
			TEXT("The ordinary helper may retain its independent BytecodeJIT compatibility entry, but must not publish an independent TypedASTJIT entry")));
		ASSERT_THAT(IsTrue(PackagedHelperResult->BackendAttempts.Num() >= 2));
		ASSERT_THAT(IsTrue(PackagedHelperResult->BackendAttempts[0].BackendId
			== FAngelscriptStaticJITBackendId::TypedAST()));
		ASSERT_THAT(IsTrue(PackagedHelperResult->BackendAttempts[0].Disposition
			== EAngelscriptStaticJITFunctionDisposition::Unsupported));
		const FAngelscriptJITGeneratedFile* const ModuleSource =
			GeneratorOutput.ProviderOutput.Files.FindByPredicate(
				[PackagedRoot](const auto& Candidate)
				{
					return Candidate.RelativePath
						== PackagedRoot->ModuleSourceRelativePath;
				});
		ASSERT_THAT(IsNotNull(ModuleSource));
		ASSERT_THAT(IsTrue(ModuleSource->Content.Contains(
			TEXT("ASJIT_Helper_TypedASTDirectHelper_65C28A_"))));
		ASSERT_THAT(IsTrue(ModuleSource->Content.Contains(
			TEXT("ASJIT_HelperBody_TypedASTDirectHelper_65C28A_"))));
		ASSERT_THAT(IsFalse(ModuleSource->Content.Contains(
			TEXT("FAngelscriptJITExecutionContext"))));
	}

	TEST_METHOD(ProductionTypedASTClosureSharesOneCurrentNativeRowAcrossRootAndHelper)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("TypedASTHelperSharedBridge"),
			FAngelscriptStaticJITBackendId::TypedAST(),
			FStringView(), true, {}, TEXTVIEW(R"AS(
int TypedASTSharedBridgeHelper_65C28A(int A, int B)
{
	return TypedASTProviderPrivateAdd(A, B);
}

UFUNCTION()
int TypedASTSharedBridgeEntry_65C28A(int A, int B)
{
	return TypedASTProviderPrivateAdd(A, B)
		+ TypedASTSharedBridgeHelper_65C28A(B, A);
}
)AS"),
			[](FAngelscriptEngine& Engine)
			{
				return AngelscriptStaticJITAotFixture::
					RegisterTypedASTPrivateBridgeSurface(
						Engine,
						AngelscriptStaticJITAotFixture::
							ETypedASTPrivateBridgeRegistration::Primary);
			});
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		const FAngelscriptStaticJITGenerationFunction* const Root =
			Snapshot->Functions.FindByPredicate([](const auto& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("TypedASTSharedBridgeEntry_65C28A"));
			});
		const FAngelscriptStaticJITGenerationFunction* const Helper =
			Snapshot->Functions.FindByPredicate([](const auto& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("TypedASTSharedBridgeHelper_65C28A"));
			});
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Helper));
		ASSERT_THAT(IsNotNull(Root->VerifiedTypedHIR));
		ASSERT_THAT(IsNotNull(Helper->VerifiedTypedHIR));

		FCompiledGraphStorage GraphStorage = BuildCompiledGraphStorage(
			*Snapshot, TEXTVIEW("TypedAST Helper Shared Bridge"));
		GraphStorage.Graph.Settings.KnownNativeCallModules.Add(
			TEXT("AngelscriptTest"));
		const FAngelscriptStableModuleKey EmitModuleSet[] = {
			Root->ModuleKey};
		const FAngelscriptStaticJITBackendRequest BackendRequest{
			FAngelscriptStaticJITBackendId::TypedAST(),
			GraphStorage.Graph,
			EmitModuleSet,
		};
		FAngelscriptTypedASTJIT Backend;
		FAngelscriptStaticJITBackendOutput BackendOutput;
		FString Error;
		ASSERT_THAT(IsTrue(Backend.Generate(
			BackendRequest, BackendOutput, &Error), Error));
		const FAngelscriptStaticJITFunctionResult* const RootResult =
			BackendOutput.Functions.FindByPredicate(
				[Root](const auto& Candidate)
				{
					return Candidate.FunctionKey == Root->FunctionKey;
				});
		ASSERT_THAT(IsNotNull(RootResult));
		ASSERT_THAT(IsTrue(RootResult->Disposition
			== EAngelscriptStaticJITFunctionDisposition::Emitted,
			RootResult->Detail));
		ASSERT_THAT(IsTrue(RootResult->EmittedFunction.IsSet()));
		const FAngelscriptJITGenerationFunction& GeneratedFunction =
			RootResult->EmittedFunction.GetValue();
		ASSERT_THAT(AreEqual(2, GeneratedFunction.NativeCallSites.Num()));
		ASSERT_THAT(IsTrue(GeneratedFunction.NativeCallSites[0].Route
			== EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding));
		ASSERT_THAT(IsTrue(GeneratedFunction.NativeCallSites[1].Route
			== EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding));
		ASSERT_THAT(AreEqual(
			GeneratedFunction.NativeCallSites[0].MetadataRowSymbol,
			GeneratedFunction.NativeCallSites[1].MetadataRowSymbol));
		ASSERT_THAT(AreEqual(
			GeneratedFunction.NativeCallSites[0].ReferenceSlotIndex,
			GeneratedFunction.NativeCallSites[1].ReferenceSlotIndex));

		int32 HelperReferenceCount = 0;
		int32 NativeReferenceCount = 0;
		for (const FAngelscriptJITGenerationReference& Reference
			: GeneratedFunction.References)
		{
			if (Reference.Reference.Kind
					== EAngelscriptArtifactReferenceKind::ScriptFunction
				&& Reference.Reference.StableKey == Helper->FunctionKey.Hash)
			{
				++HelperReferenceCount;
			}
			if (Reference.Reference.Kind
					== EAngelscriptArtifactReferenceKind::EnvironmentSymbol
				&& Reference.Reference.StableKey
					== GeneratedFunction.NativeCallSites[0].StableTargetKey)
			{
				++NativeReferenceCount;
			}
		}
		ASSERT_THAT(AreEqual(1, HelperReferenceCount));
		ASSERT_THAT(AreEqual(1, NativeReferenceCount));

		const FString CallSiteRowDefinition = FString::Printf(
			TEXT("static const FAngelscriptTypedASTJITBoundCallSite %s = {"),
			*GeneratedFunction.NativeCallSites[0].MetadataRowSymbol);
		int32 CallSiteRowDefinitionCount = 0;
		int32 SearchFrom = 0;
		while ((SearchFrom = GeneratedFunction.ImplementationTemplate.Find(
			CallSiteRowDefinition,
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			SearchFrom)) != INDEX_NONE)
		{
			++CallSiteRowDefinitionCount;
			SearchFrom += CallSiteRowDefinition.Len();
		}
		ASSERT_THAT(AreEqual(1, CallSiteRowDefinitionCount,
			TEXT("One root closure must own exactly one immutable row for one exact current-native target, even when root and helper both call it")));
		ASSERT_THAT(IsTrue(GeneratedFunction.ImplementationTemplate.Contains(
			TEXT("ASJIT_Helper_TypedASTSharedBridgeHelper_65C28A_"))));
		ASSERT_THAT(IsTrue(GeneratedFunction.ImplementationTemplate.Contains(
			TEXT("AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>"))));
		ASSERT_THAT(IsFalse(GeneratedFunction.ImplementationTemplate.Contains(
			TEXT("AngelscriptTypedASTJIT::InvokeBound<int32, int32, int32>"))));
		ASSERT_THAT(IsFalse(GeneratedFunction.ImplementationTemplate.Contains(
			TEXT("InvokeBoundViaVM"))));
		ASSERT_THAT(IsFalse(GeneratedFunction.ImplementationTemplate.Contains(
			TEXT("FAngelscriptJITExecutionContext"))));
	}

	TEST_METHOD(ProviderPrivateGenericVMBridgeIsFrozenAndEmittedByTypedAST)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		const FString ScriptSource = ASTEST_AS(R"AS(
			UFUNCTION()
			int TypedASTGenericBridgeException_65C28A(int Value)
			{
				int Result = TypedASTProviderPrivateFail(Value);
				TypedASTProviderPrivateAdd(1, 2);
				return Result;
			}
			)AS");
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("TypedASTGenericVMBridge"),
			FAngelscriptStaticJITBackendId::TypedAST(),
			FStringView(), true, {}, FStringView(ScriptSource),
			[](FAngelscriptEngine& Engine)
			{
				return AngelscriptStaticJITAotFixture::
					RegisterTypedASTPrivateBridgeSurface(
						Engine,
						AngelscriptStaticJITAotFixture::
							ETypedASTPrivateBridgeRegistration::Primary);
			});
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));

		const FAngelscriptStaticJITGenerationFunction* const Root =
			Snapshot->Functions.FindByPredicate([](const auto& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("TypedASTGenericBridgeException_65C28A"));
			});
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Root->VerifiedTypedHIR));

		int32 GenericTargetFunctionId = INDEX_NONE;
		for (asUINT ExpressionIndex = 0;
			ExpressionIndex < Root->VerifiedTypedHIR->expressions.GetLength();
			++ExpressionIndex)
		{
			const asSTypedSemanticExpression& Expression =
				Root->VerifiedTypedHIR->expressions[ExpressionIndex];
			if (Expression.kind
					!= asETypedSemanticExpressionKind::ResolvedCall
				|| Expression.callTargetKind
					!= asETypedSemanticCallTargetKind::SystemFunction)
			{
				continue;
			}
			asIScriptFunction* const Target = Fixture->Engine->GetScriptEngine()
				->GetFunctionById(Expression.resolvedFunctionId);
			if (Target != nullptr
				&& FString(UTF8_TO_TCHAR(Target->GetName()))
					== TEXT("TypedASTProviderPrivateFail"))
			{
				GenericTargetFunctionId = Expression.resolvedFunctionId;
				break;
			}
		}
		ASSERT_THAT(AreNotEqual(INDEX_NONE, GenericTargetFunctionId));
		const FAngelscriptStaticJITNativeCallTarget* const GenericTarget =
			Snapshot->NativeCallTargets.FindByPredicate(
				[GenericTargetFunctionId](const auto& Candidate)
				{
					return Candidate.EngineLocalFunctionId
						== GenericTargetFunctionId;
				});
		ASSERT_THAT(IsNotNull(GenericTarget));
		ASSERT_THAT(IsTrue(GenericTarget->Descriptor.ABIDomain
			== EAngelscriptStaticJITNativeCallABIDomain::VMScalarBridge));
		ASSERT_THAT(IsTrue(GenericTarget->bScalarBridgeCallable));
		ASSERT_THAT(IsFalse(GenericTarget->ExpectedScalarABI.IsValid()));
		ASSERT_THAT(IsTrue(GenericTarget->ExpectedVMBridgeABI.IsValid()));
		ASSERT_THAT(IsTrue(GenericTarget->Descriptor.NativeABI
			== GenericTarget->ExpectedNativeABI,
			FString::Printf(TEXT(
				"DescriptorNativeABI=%s\nExpectedNativeABI=%s"),
				*GenericTarget->Descriptor.NativeABI.Canonical,
				*GenericTarget->ExpectedNativeABI.Canonical)));

		FCompiledGraphStorage GraphStorage = BuildCompiledGraphStorage(
			*Snapshot, TEXTVIEW("TypedAST Generic VM Bridge"));
		GraphStorage.Graph.Settings.KnownNativeCallModules.Add(
			TEXT("AngelscriptTest"));
		const FAngelscriptStableModuleKey EmitModuleSet[] = {
			Root->ModuleKey};
		const FAngelscriptStaticJITBackendRequest BackendRequest{
			FAngelscriptStaticJITBackendId::TypedAST(),
			GraphStorage.Graph,
			EmitModuleSet,
		};
		FAngelscriptTypedASTJIT Backend;
		FAngelscriptStaticJITBackendOutput BackendOutput;
		FString Error;
		ASSERT_THAT(IsTrue(Backend.Generate(
			BackendRequest, BackendOutput, &Error), Error));
		const FAngelscriptStaticJITFunctionResult* const RootResult =
			BackendOutput.Functions.FindByPredicate(
				[Root](const auto& Candidate)
				{
					return Candidate.FunctionKey == Root->FunctionKey;
				});
		ASSERT_THAT(IsNotNull(RootResult));
		ASSERT_THAT(IsTrue(RootResult->Disposition
			== EAngelscriptStaticJITFunctionDisposition::Emitted,
			RootResult->Detail));
		ASSERT_THAT(IsTrue(RootResult->EmittedFunction.IsSet()));
		const FAngelscriptJITGenerationFunction& GeneratedFunction =
			RootResult->EmittedFunction.GetValue();
		ASSERT_THAT(IsTrue(GeneratedFunction.ImplementationTemplate.Contains(
			TEXT("ASJIT_Call_TypedASTProviderPrivateFail_"))));
		ASSERT_THAT(IsTrue(GeneratedFunction.ImplementationTemplate.Contains(
			TEXT("AngelscriptTypedASTJIT::InvokeBound<int32, int32>"))));
		ASSERT_THAT(IsTrue(GeneratedFunction.ImplementationTemplate.Contains(
			TEXT("Execution.bExceptionThrown"))));
		ASSERT_THAT(IsFalse(GeneratedFunction.ImplementationTemplate.Contains(
			TEXT("FAngelscriptJITExecutionContext"))));
	}

	TEST_METHOD(ProductionTypedASTSelfRecursionOwnsOneBoundedRootWrapper)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		const FString SelfRecursiveSource = ASTEST_AS(R"AS(
			UFUNCTION()
			int TypedASTSelfRecursive_65C28A(int Value)
			{
				if (Value > 0)
				{
					return Value * TypedASTSelfRecursive_65C28A(Value - 1);
				}

				return 1;
			}
			)AS");
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("TypedASTSelfRecursion"),
			FAngelscriptStaticJITBackendId::TypedAST(),
			FStringView(), true, {}, FStringView(SelfRecursiveSource));
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		const FAngelscriptStaticJITGenerationFunction* const Root =
			Snapshot->Functions.FindByPredicate([](const auto& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("TypedASTSelfRecursive_65C28A"));
			});
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Root->VerifiedTypedHIR));
		const FAngelscriptStaticJITGenerationDependency* const SelfReference =
			Snapshot->Dependencies.FindByPredicate(
				[Root](const auto& Dependency)
				{
					return Dependency.OwnerFunctionKey == Root->FunctionKey
						&& Dependency.Reference.Kind
							== EAngelscriptArtifactReferenceKind::ScriptFunction
						&& Dependency.Reference.StableKey
							== Root->FunctionKey.Hash;
				});
		ASSERT_THAT(IsNotNull(SelfReference,
			TEXT("A self-recursive root must freeze its exact ScriptFunction identity and declaration ABI")));
		ASSERT_THAT(IsTrue(!SelfReference->Reference.ExpectedAbi.IsZero()));

		FCompiledGraphStorage GraphStorage = BuildCompiledGraphStorage(
			*Snapshot, TEXTVIEW("TypedAST Self Recursion"));
		const FAngelscriptStableModuleKey EmitModuleSet[] = {
			Root->ModuleKey};
		const FAngelscriptStaticJITBackendRequest BackendRequest{
			FAngelscriptStaticJITBackendId::TypedAST(),
			GraphStorage.Graph,
			EmitModuleSet,
		};
		FAngelscriptTypedASTJIT Backend;
		FAngelscriptStaticJITBackendOutput BackendOutput;
		FString Error;
		ASSERT_THAT(IsTrue(Backend.Generate(
			BackendRequest, BackendOutput, &Error), Error));
		const FAngelscriptStaticJITFunctionResult* const RootResult =
			BackendOutput.Functions.FindByPredicate(
				[Root](const auto& Candidate)
				{
					return Candidate.FunctionKey == Root->FunctionKey;
				});
		ASSERT_THAT(IsNotNull(RootResult));
		ASSERT_THAT(IsTrue(RootResult->Disposition
			== EAngelscriptStaticJITFunctionDisposition::Emitted,
			RootResult->Detail));
		ASSERT_THAT(IsTrue(RootResult->EmittedFunction.IsSet()));
		const FAngelscriptJITGenerationFunction& GeneratedFunction =
			RootResult->EmittedFunction.GetValue();
		int32 SelfReferenceCount = 0;
		for (const FAngelscriptJITGenerationReference& Reference
			: GeneratedFunction.References)
		{
			if (Reference.Reference.Kind
					== EAngelscriptArtifactReferenceKind::ScriptFunction
				&& Reference.Reference.StableKey == Root->FunctionKey.Hash)
			{
				++SelfReferenceCount;
			}
		}
		ASSERT_THAT(AreEqual(1, SelfReferenceCount));
		const FString& Source = GeneratedFunction.ImplementationTemplate;
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("ASJIT_Helper_TypedASTSelfRecursive_65C28A_"))));
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("{SYMBOL_PREFIX}_TypedBody"),
			ESearchCase::CaseSensitive),
			TEXT("The Provider root keeps its stable entry ABI body symbol while the readable helper symbol represents recursive re-entry")));
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("FScopeTypedASTJITExecutionFrame TypedFrame"))));
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("if (UNLIKELY(!TypedFrame.IsActive()))"))));
		ASSERT_THAT(IsTrue(Source.Contains(TEXT("Execution.bExceptionThrown"))));
		ASSERT_THAT(IsFalse(Source.Contains(
			TEXT("FAngelscriptJITExecutionContext"))));
	}

	TEST_METHOD(ProductionTypedASTMutualRecursionEmitsTheCompleteBoundedSCC)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		const FString MutualRecursiveSource = ASTEST_AS(R"AS(
			int TypedASTMutualOdd_65C28A(int Value)
			{
				if (Value > 0)
				{
					return TypedASTMutualEven_65C28A(Value - 1);
				}

				return 0;
			}

			UFUNCTION()
			int TypedASTMutualEven_65C28A(int Value)
			{
				if (Value > 0)
				{
					return TypedASTMutualOdd_65C28A(Value - 1);
				}

				return 1;
			}
			)AS");
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("TypedASTMutualRecursion"),
			FAngelscriptStaticJITBackendId::TypedAST(),
			FStringView(), true, {}, FStringView(MutualRecursiveSource));
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		const FAngelscriptStaticJITGenerationFunction* const Root =
			Snapshot->Functions.FindByPredicate([](const auto& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("TypedASTMutualEven_65C28A"));
			});
		const FAngelscriptStaticJITGenerationFunction* const Helper =
			Snapshot->Functions.FindByPredicate([](const auto& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("TypedASTMutualOdd_65C28A"));
			});
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Helper));
		ASSERT_THAT(IsNotNull(Root->VerifiedTypedHIR));
		ASSERT_THAT(IsNotNull(Helper->VerifiedTypedHIR));
		const FAngelscriptStaticJITGenerationDependency* const RootToHelper =
			Snapshot->Dependencies.FindByPredicate(
				[Root, Helper](const auto& Dependency)
				{
					return Dependency.OwnerFunctionKey == Root->FunctionKey
						&& Dependency.Reference.Kind
							== EAngelscriptArtifactReferenceKind::ScriptFunction
						&& Dependency.Reference.StableKey
							== Helper->FunctionKey.Hash;
				});
		const FAngelscriptStaticJITGenerationDependency* const HelperToRoot =
			Snapshot->Dependencies.FindByPredicate(
				[Root, Helper](const auto& Dependency)
				{
					return Dependency.OwnerFunctionKey == Helper->FunctionKey
						&& Dependency.Reference.Kind
							== EAngelscriptArtifactReferenceKind::ScriptFunction
						&& Dependency.Reference.StableKey
							== Root->FunctionKey.Hash;
				});
		ASSERT_THAT(IsNotNull(RootToHelper));
		ASSERT_THAT(IsNotNull(HelperToRoot,
			TEXT("A mutual SCC must freeze the helper-to-root back edge instead of treating the root as an implicit symbol")));

		FCompiledGraphStorage GraphStorage = BuildCompiledGraphStorage(
			*Snapshot, TEXTVIEW("TypedAST Mutual Recursion"));
		const FAngelscriptStableModuleKey EmitModuleSet[] = {
			Root->ModuleKey};
		const FAngelscriptStaticJITBackendRequest BackendRequest{
			FAngelscriptStaticJITBackendId::TypedAST(),
			GraphStorage.Graph,
			EmitModuleSet,
		};
		FAngelscriptTypedASTJIT Backend;
		FAngelscriptStaticJITBackendOutput BackendOutput;
		FString Error;
		ASSERT_THAT(IsTrue(Backend.Generate(
			BackendRequest, BackendOutput, &Error), Error));
		const FAngelscriptStaticJITFunctionResult* const RootResult =
			BackendOutput.Functions.FindByPredicate(
				[Root](const auto& Candidate)
				{
					return Candidate.FunctionKey == Root->FunctionKey;
				});
		ASSERT_THAT(IsNotNull(RootResult));
		ASSERT_THAT(IsTrue(RootResult->Disposition
			== EAngelscriptStaticJITFunctionDisposition::Emitted,
			RootResult->Detail));
		ASSERT_THAT(IsTrue(RootResult->EmittedFunction.IsSet()));
		const FAngelscriptJITGenerationFunction& GeneratedFunction =
			RootResult->EmittedFunction.GetValue();
		int32 RootReferenceCount = 0;
		int32 HelperReferenceCount = 0;
		for (const FAngelscriptJITGenerationReference& Reference
			: GeneratedFunction.References)
		{
			if (Reference.Reference.Kind
				!= EAngelscriptArtifactReferenceKind::ScriptFunction)
			{
				continue;
			}
			RootReferenceCount += Reference.Reference.StableKey
				== Root->FunctionKey.Hash ? 1 : 0;
			HelperReferenceCount += Reference.Reference.StableKey
				== Helper->FunctionKey.Hash ? 1 : 0;
		}
		ASSERT_THAT(AreEqual(1, RootReferenceCount));
		ASSERT_THAT(AreEqual(1, HelperReferenceCount));
		const FString& Source = GeneratedFunction.ImplementationTemplate;
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("ASJIT_Helper_TypedASTMutualEven_65C28A_"))));
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("{SYMBOL_PREFIX}_TypedBody"),
			ESearchCase::CaseSensitive),
			TEXT("The selected root keeps the Provider ABI body symbol and exposes a readable wrapper for the SCC back edge")));
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("ASJIT_Helper_TypedASTMutualOdd_65C28A_"))));
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("ASJIT_HelperBody_TypedASTMutualOdd_65C28A_"))));
		int32 TypedFrameCount = 0;
		int32 SearchFrom = 0;
		const FString FrameNeedle =
			TEXT("FScopeTypedASTJITExecutionFrame TypedFrame");
		while ((SearchFrom = Source.Find(
			FrameNeedle,
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			SearchFrom)) != INDEX_NONE)
		{
			++TypedFrameCount;
			SearchFrom += FrameNeedle.Len();
		}
		ASSERT_THAT(IsTrue(TypedFrameCount >= 3,
			TEXT("The public root plus both mutually recursive call targets must each enter the bounded TypedAST frame contract")));
		ASSERT_THAT(IsTrue(Source.Contains(TEXT("Execution.bExceptionThrown"))));
		ASSERT_THAT(IsFalse(Source.Contains(
			TEXT("FAngelscriptJITExecutionContext"))));
	}

	TEST_METHOD(GenerationNativeCallInventoryOwnsCompleteBindProvenance)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("CompleteNativeCallInventory"),
			FAngelscriptStaticJITBackendId::TypedAST());
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		const FAngelscriptBindState* const BindState =
			Fixture->Engine->GetBindState();
		ASSERT_THAT(IsNotNull(BindState));
		ASSERT_THAT(IsNotNull(
			BindState->GenerationFunctionProvenance.Get(),
			TEXT("A StaticJITGeneration Engine must own the optional broad provenance table")));

		ASSERT_THAT(AreEqual(
			BindState->GenerationFunctionProvenance->Functions.Num(),
			Snapshot->NativeCallInventory.Num(),
			TEXT("The generation inventory must contain every function captured by the generation-only Bind provenance table")));
		ASSERT_THAT(AreEqual(
			BindState->FunctionProvenance.Num(),
			BindState->GenerationFunctionProvenance->Functions.Num(),
			TEXT("Every compact Bind provenance row must have exactly one generation-only broad provenance row")));
		ASSERT_THAT(IsTrue(Snapshot->NativeCallInventory.Num() > 1000,
			TEXT("The inventory must describe the final installed Bind surface rather than only calls referenced by the fixture")));

		TSet<int32> SeenFunctionIds;
		TMap<FString, int32> SeenStableReferenceKeys;
		TMap<FString, int32> ManualProviderDeclarationCounts;
		FString DuplicateInstalledIdentity;
		bool bSawManualDynamicProviderExpansion = false;
		bool bSawFunctionCallerDispatch = false;
		bool bSawGenericFunctionDispatch = false;
		bool bSawGenericMethodDispatch = false;
		int32 ScriptFunctionMetadataCount = 0;
		int32 ScriptObjectTypeMetadataCount = 0;
		TSet<EAngelscriptStaticJITInstalledCallableKind> SeenCallableKinds;
		for (const auto& Entry : Snapshot->NativeCallInventory)
		{
			ASSERT_THAT(IsTrue(Entry.EngineLocalFunctionId >= 0));
			ASSERT_THAT(IsTrue(!Entry.CanonicalDeclaration.IsEmpty()));
			ASSERT_THAT(IsTrue(Entry.Origin
				!= EAngelscriptFunctionBindingOrigin::Unknown,
				TEXT("No installed Bind callable may remain origin=unknown")));
			ASSERT_THAT(IsTrue(!Entry.OwnerModule.IsNone()));
			ASSERT_THAT(IsTrue(!Entry.Provider.IsNone()));
			ASSERT_THAT(IsTrue(!Entry.ProviderSourceFile.IsEmpty()));
			ASSERT_THAT(IsFalse(Entry.ProviderSourceFile.Contains(TEXT("\\")),
				TEXT("Generation provenance must own a normalized slash-separated path")));
			ASSERT_THAT(IsTrue(Entry.ProviderSourceLine > 0));
			ASSERT_THAT(IsFalse(SeenFunctionIds.Contains(
				Entry.EngineLocalFunctionId),
				TEXT("Each Engine-local registered function must have exactly one inventory row")));
			SeenFunctionIds.Add(Entry.EngineLocalFunctionId);
			ASSERT_THAT(IsTrue(Entry.bHasStableReference,
				*Entry.StableReferenceError));
			ASSERT_THAT(IsTrue(Entry.StableReference.IsValid()));
			ASSERT_THAT(IsTrue(Entry.StableReference.Kind
				== EAngelscriptArtifactReferenceKind::EnvironmentSymbol));
			ASSERT_THAT(IsFalse(Entry.StableReference.ExpectedAbi.IsZero()));
			const FString InstalledIdentity =
				Entry.StableReference.StableKey.ToHexString();
			if (const int32* const ExistingFunctionId =
				SeenStableReferenceKeys.Find(InstalledIdentity))
			{
				if (DuplicateInstalledIdentity.IsEmpty())
				{
					DuplicateInstalledIdentity = FString::Printf(TEXT(
						"Identity=%s ExistingFunctionId=%d DuplicateFunctionId=%d Provider=%s NativeDisplay=%s"),
						*InstalledIdentity, *ExistingFunctionId,
						Entry.EngineLocalFunctionId,
						*Entry.Provider.ToString(),
						*Entry.NativeCallableDisplay);
				}
			}
			else
			{
				SeenStableReferenceKeys.Add(
					InstalledIdentity, Entry.EngineLocalFunctionId);
			}
			SeenCallableKinds.Add(Entry.CallableKind);
			switch (Entry.DispatchKind)
			{
			case EAngelscriptStaticJITInstalledDispatchKind::FunctionCaller:
				bSawFunctionCallerDispatch = true;
				ASSERT_THAT(IsTrue(Entry.bHasSystemCaller));
				break;
			case EAngelscriptStaticJITInstalledDispatchKind::GenericFunction:
				bSawGenericFunctionDispatch = true;
				ASSERT_THAT(IsFalse(Entry.bHasSystemCaller));
				break;
			case EAngelscriptStaticJITInstalledDispatchKind::GenericMethod:
				bSawGenericMethodDispatch = true;
				ASSERT_THAT(IsFalse(Entry.bHasSystemCaller));
				break;
			case EAngelscriptStaticJITInstalledDispatchKind::NativeCallingConvention:
				ASSERT_THAT(IsFalse(Entry.bHasSystemCaller));
				break;
			default:
				ASSERT_THAT(IsTrue(false,
					TEXT("Every installed system function must expose its authoritative VM dispatch route")));
				break;
			}
			if (Entry.FirstParamMetadata
				!= EAngelscriptStaticJITInstalledFirstParamMetadata::None)
			{
				const uint8 MetadataValue = static_cast<uint8>(
					Entry.FirstParamMetadata);
				ASSERT_THAT(IsTrue(Entry.bHasExpectedNativeABI,
					*Entry.NativeABIError));
				ASSERT_THAT(IsTrue(Entry.ExpectedNativeABI.Canonical.Contains(
					FString::Printf(TEXT("firstMetadata=%u"),
						static_cast<uint32>(MetadataValue)))));
				ASSERT_THAT(IsFalse(Entry.bHasExpectedScalarABI,
					TEXT("Hidden native metadata must never enter the scalar direct/current-native ABI")));
				ASSERT_THAT(IsTrue(Entry.ScalarABIError.Contains(
					TEXT("NativeScalarABIUnsupportedHiddenState"))));
				if (Entry.FirstParamMetadata
					== EAngelscriptStaticJITInstalledFirstParamMetadata::ScriptFunction)
				{
					++ScriptFunctionMetadataCount;
				}
				else if (Entry.FirstParamMetadata
					== EAngelscriptStaticJITInstalledFirstParamMetadata::ScriptObjectType)
				{
					++ScriptObjectTypeMetadataCount;
				}
				else
				{
					ASSERT_THAT(IsTrue(false,
						TEXT("The installed inventory must reject unknown first-parameter metadata")));
				}
			}
			if (Entry.Origin == EAngelscriptFunctionBindingOrigin::Manual)
			{
				const FString ProviderDeclarationIdentity = FString::Printf(
					TEXT("%s|%s|%s|%d"), *Entry.OwnerModule.ToString(),
					*Entry.Provider.ToString(), *Entry.ProviderSourceFile,
					Entry.ProviderSourceLine);
				int32& DeclarationCount = ManualProviderDeclarationCounts.FindOrAdd(
					ProviderDeclarationIdentity);
				++DeclarationCount;
				bSawManualDynamicProviderExpansion |= DeclarationCount > 1;
			}
		}
		ASSERT_THAT(IsTrue(bSawManualDynamicProviderExpansion,
			TEXT("A dynamic/manual Provider expansion must retain one Provider declaration provenance row on every separately registered function")));
		ASSERT_THAT(IsTrue(bSawFunctionCallerDispatch,
			TEXT("The final Engine inventory must retain FunctionCaller-backed Bind targets")));
		ASSERT_THAT(IsTrue(bSawGenericFunctionDispatch,
			TEXT("The final Engine inventory must retain generic global Bind targets")));
		ASSERT_THAT(IsTrue(bSawGenericMethodDispatch,
			TEXT("The final Engine inventory must retain generic method Bind targets")));
		ASSERT_THAT(IsTrue(ScriptFunctionMetadataCount > 0,
			TEXT("The final Engine inventory must retain real PassScriptFunctionAsFirstParam Binds")));
		ASSERT_THAT(IsTrue(ScriptObjectTypeMetadataCount > 0,
			TEXT("The final Engine inventory must retain real PassScriptObjectTypeAsFirstParam Binds")));
		ASSERT_THAT(IsTrue(SeenCallableKinds.Contains(
			EAngelscriptStaticJITInstalledCallableKind::Global)));
		ASSERT_THAT(IsTrue(SeenCallableKinds.Contains(
			EAngelscriptStaticJITInstalledCallableKind::Method)));
		ASSERT_THAT(IsTrue(SeenCallableKinds.Contains(
			EAngelscriptStaticJITInstalledCallableKind::Constructor)));
		ASSERT_THAT(IsTrue(SeenCallableKinds.Contains(
			EAngelscriptStaticJITInstalledCallableKind::Destructor),
			TEXT("Installed destruct behaviours must not be misclassified as methods")));
		ASSERT_THAT(IsTrue(SeenCallableKinds.Contains(
			EAngelscriptStaticJITInstalledCallableKind::Behavior),
			TEXT("Installed non-lifetime behaviours must not be misclassified as methods or globals")));
		ASSERT_THAT(IsTrue(SeenCallableKinds.Contains(
			EAngelscriptStaticJITInstalledCallableKind::Generic)));

		const auto* const IsRunningCommandletEntry =
			Snapshot->NativeCallInventory.FindByPredicate([](const auto& Entry)
			{
				return Entry.CanonicalDeclaration.Contains(
					TEXT("IsRunningCommandlet("));
			});
		ASSERT_THAT(IsNotNull(IsRunningCommandletEntry));
		ASSERT_THAT(IsTrue(IsRunningCommandletEntry->Origin
			== EAngelscriptFunctionBindingOrigin::Manual));
		ASSERT_THAT(IsTrue(IsRunningCommandletEntry->OwnerModule
			== FName(TEXT("AngelscriptRuntime"))));
		ASSERT_THAT(IsTrue(IsRunningCommandletEntry->Provider
			== FName(TEXT("CoreGlobals"))));
		ASSERT_THAT(IsTrue(IsRunningCommandletEntry->Phase
			== EAngelscriptBindPhase::ExplicitBindings));
		ASSERT_THAT(IsTrue(IsRunningCommandletEntry->ProviderSourceFile.EndsWith(
			TEXT("Binds/Bind_CoreGlobals.cpp"))));
		ASSERT_THAT(IsTrue(IsRunningCommandletEntry->ProviderSourceLine > 0));
		ASSERT_THAT(AreEqual(
			FString(TEXT("IsRunningCommandlet")),
			IsRunningCommandletEntry->NativeCallableDisplay));
		ASSERT_THAT(IsTrue(
			IsRunningCommandletEntry->NamespaceCanonicalName.IsEmpty()));
		ASSERT_THAT(IsTrue(
			IsRunningCommandletEntry->ReceiverCanonicalName.IsEmpty()));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITInstalledCompileOutKind::CompileCalls,
			IsRunningCommandletEntry->CompileOutKind));
		ASSERT_THAT(AreEqual(0,
			IsRunningCommandletEntry->DefaultArgumentCount));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITInstalledFirstParamMetadata::None,
			IsRunningCommandletEntry->FirstParamMetadata));
		ASSERT_THAT(IsTrue(IsRunningCommandletEntry->bHasNativeForm));
		ASSERT_THAT(IsTrue(IsRunningCommandletEntry->NativeFormKind
			== EAngelscriptNativeFormKind::Function));
		ASSERT_THAT(IsTrue(IsRunningCommandletEntry->bNativeFormTrivial));
		ASSERT_THAT(IsTrue(IsRunningCommandletEntry->bHasExternalDescriptor));
		ASSERT_THAT(IsTrue(IsRunningCommandletEntry->bHasExpectedNativeABI));
		ASSERT_THAT(IsTrue(
			IsRunningCommandletEntry->ExternalDescriptor.NativeABI
				== IsRunningCommandletEntry->ExpectedNativeABI));
		ASSERT_THAT(AreEqual(
			FString(TEXT("IsRunningCommandlet")),
			IsRunningCommandletEntry->ExternalDescriptor.Symbol));
		ASSERT_THAT(AreEqual(
			FString(TEXT("cdecl:bool()")),
			IsRunningCommandletEntry->ExpectedScalarABI.Canonical));

		const auto* const PrintEntry =
			Snapshot->NativeCallInventory.FindByPredicate([](const auto& Entry)
			{
				return Entry.CanonicalDeclaration.StartsWith(TEXT("void Print("));
			});
		ASSERT_THAT(IsNotNull(PrintEntry));
		ASSERT_THAT(IsTrue(PrintEntry->Origin
			== EAngelscriptFunctionBindingOrigin::Manual));
		ASSERT_THAT(IsTrue(PrintEntry->OwnerModule
			== FName(TEXT("AngelscriptRuntime"))));
		ASSERT_THAT(IsTrue(PrintEntry->Provider
			== FName(TEXT("Logging.Functions"))));
		ASSERT_THAT(IsTrue(PrintEntry->Phase
			== EAngelscriptBindPhase::ExplicitBindings));
		ASSERT_THAT(IsTrue(PrintEntry->ProviderSourceFile.EndsWith(
			TEXT("Binds/Bind_Logging.cpp"))));
		ASSERT_THAT(IsTrue(PrintEntry->ProviderSourceLine > 0));
		ASSERT_THAT(IsTrue(PrintEntry->NamespaceCanonicalName.IsEmpty()));
		ASSERT_THAT(IsTrue(PrintEntry->ReceiverCanonicalName.IsEmpty()));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITInstalledCompileOutKind::CompileCalls,
			PrintEntry->CompileOutKind,
			TEXT("EditorDevelopment generation must retain Print's non-rewritten call path")));
		ASSERT_THAT(AreEqual(2, PrintEntry->DefaultArgumentCount));
		ASSERT_THAT(AreEqual(-1,
			static_cast<int32>(PrintEntry->HiddenArgumentIndex)));
		ASSERT_THAT(AreEqual(-1,
			static_cast<int32>(PrintEntry->DeterminesOutputTypeArgumentIndex)));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITInstalledFirstParamMetadata::None,
			PrintEntry->FirstParamMetadata));
		ASSERT_THAT(IsTrue(PrintEntry->bUsesWorldContext));
		ASSERT_THAT(IsTrue(PrintEntry->RegisteredCallConv > 0));
		ASSERT_THAT(IsTrue(PrintEntry->bHasSystemCaller));
		ASSERT_THAT(IsTrue(PrintEntry->bHasExternalDescriptor));
		ASSERT_THAT(IsTrue(PrintEntry->bHasExpectedNativeABI));
		ASSERT_THAT(IsTrue(PrintEntry->ExternalDescriptor.NativeABI
			== PrintEntry->ExpectedNativeABI));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable,
			PrintEntry->ExternalDescriptor.Linkage));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallABIDomain::BorrowedManagedPOD,
			PrintEntry->ExternalDescriptor.ABIDomain));
		ASSERT_THAT(IsFalse(PrintEntry->bHasExpectedScalarABI));
		ASSERT_THAT(IsFalse(PrintEntry->ScalarABIError.IsEmpty()));

		const FString InventoryCsv = BuildNativeCallInventoryCsv(*Snapshot);
		TArray<FString> InventoryLines;
		InventoryCsv.ParseIntoArrayLines(InventoryLines, false);
		ASSERT_THAT(AreEqual(
			Snapshot->NativeCallInventory.Num() + 1,
			InventoryLines.Num(),
			TEXT("The deterministic installed inventory export must contain one header plus every generation row")));
		ASSERT_THAT(IsTrue(InventoryLines[0].Contains(
			TEXT("ReceiverCanonicalName"))));
		ASSERT_THAT(IsTrue(InventoryLines[0].Contains(
			TEXT("DescriptorNormalized"))));

		FString InventoryOutputPath;
		if (FParse::Value(FCommandLine::Get(),
			TEXT("AngelscriptNativeCallInventoryOutput="),
			InventoryOutputPath))
		{
			ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
				InventoryCsv, *InventoryOutputPath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM),
				TEXT("The explicit research output path must receive the fresh installed inventory")));
		}
		ASSERT_THAT(IsTrue(DuplicateInstalledIdentity.IsEmpty(),
			*FString::Printf(TEXT(
				"CacheV2 stable environment reference must uniquely identify every installed Engine function without retaining FunctionId. %s"),
				*DuplicateInstalledIdentity)));
	}

	TEST_METHOD(GenerationFunctionProvenanceRejectsDuplicateFunctionIds)
	{
		FAngelscriptGenerationFunctionProvenanceState State;
		FAngelscriptGenerationFunctionProvenance Provenance;
		Provenance.Origin = EAngelscriptFunctionBindingOrigin::Manual;
		Provenance.OwnerModule = FName(TEXT("AngelscriptTest"));
		Provenance.Provider = FName(TEXT("Duplicate.FunctionId.Probe"));

		ASSERT_THAT(IsTrue(State.TryAddFunction(73, Provenance)));
		ASSERT_THAT(IsFalse(State.TryAddFunction(73, Provenance),
			TEXT("The generation table must reject a repeated Engine-local FunctionId instead of silently reusing its first row")));
		ASSERT_THAT(AreEqual(1, State.Functions.Num()));
		ASSERT_THAT(IsTrue(State.DuplicateFunctionIds.Contains(73),
			TEXT("A rejected duplicate must remain observable so snapshot freezing can fail closed")));
	}

	TEST_METHOD(GenerationNativeCallInventoryIsEngineOwnedAndSurvivesOwnerTeardown)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		const int32 BaselineNativeFormCount =
			FScriptFunctionNativeForm::NumNativeForms();
		TUniquePtr<FFixture> First = CreateFixture(
			*TestRunner, TEXT("NativeInventoryEngineA"),
			FAngelscriptStaticJITBackendId::TypedAST());
		ASSERT_THAT(IsNotNull(First.Get()));
		const int32 FirstOnlyNativeFormCount =
			FScriptFunctionNativeForm::NumNativeForms();
		ASSERT_THAT(IsTrue(
			FirstOnlyNativeFormCount > BaselineNativeFormCount));

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> FirstSnapshot =
			First->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(FirstSnapshot.IsValid()));
		const int32 FirstInventoryCount =
			FirstSnapshot->NativeCallInventory.Num();
		const auto* const FirstPrint =
			FirstSnapshot->NativeCallInventory.FindByPredicate(
				[](const auto& Entry)
				{
					return Entry.CanonicalDeclaration.StartsWith(
						TEXT("void Print("));
				});
		ASSERT_THAT(IsNotNull(FirstPrint));
		const FString FirstPrintDeclaration =
			FirstPrint->CanonicalDeclaration;
		const FString FirstPrintSource = FirstPrint->ProviderSourceFile;

		TUniquePtr<FFixture> Second = CreateFixture(
			*TestRunner, TEXT("NativeInventoryEngineB"),
			FAngelscriptStaticJITBackendId::TypedAST());
		ASSERT_THAT(IsNotNull(Second.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> SecondSnapshot =
			Second->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(SecondSnapshot.IsValid()));
		ASSERT_THAT(AreEqual(
			FirstInventoryCount,
			SecondSnapshot->NativeCallInventory.Num()));
		ASSERT_THAT(IsTrue(
			First->Engine->GetBindState()->GenerationFunctionProvenance.Get()
				!= Second->Engine->GetBindState()->GenerationFunctionProvenance.Get(),
			TEXT("Each generation Engine must own a distinct broad provenance table")));
		const int32 BothNativeFormCount =
			FScriptFunctionNativeForm::NumNativeForms();
		const int32 OneEngineNativeFormDelta =
			FirstOnlyNativeFormCount - BaselineNativeFormCount;
		ASSERT_THAT(AreEqual(
			FirstOnlyNativeFormCount + OneEngineNativeFormDelta,
			BothNativeFormCount,
			TEXT("Two generation Engines must own two independent native-form tables")));

		First.Reset();
		ASSERT_THAT(AreEqual(
			FirstOnlyNativeFormCount,
			FScriptFunctionNativeForm::NumNativeForms(),
			TEXT("Destroying Engine A must release only A's native forms")));
		ASSERT_THAT(AreEqual(
			FirstInventoryCount,
			FirstSnapshot->NativeCallInventory.Num(),
			TEXT("The pointer-free inventory must remain owned by its immutable snapshot after Engine teardown")));
		const auto* const RetainedFirstPrint =
			FirstSnapshot->NativeCallInventory.FindByPredicate(
				[&FirstPrintDeclaration](const auto& Entry)
				{
					return Entry.CanonicalDeclaration
						== FirstPrintDeclaration;
				});
		ASSERT_THAT(IsNotNull(RetainedFirstPrint));
		ASSERT_THAT(AreEqual(
			FirstPrintSource, RetainedFirstPrint->ProviderSourceFile));
		ASSERT_THAT(AreEqual(
			FirstInventoryCount,
			Second->Engine->GetBindState()
				->GenerationFunctionProvenance->Functions.Num(),
			TEXT("Destroying Engine A must not mutate Engine B's provenance")));

		Second.Reset();
		ASSERT_THAT(AreEqual(
			BaselineNativeFormCount,
			FScriptFunctionNativeForm::NumNativeForms(),
			TEXT("Destroying both generation Engines must restore native-form ownership to baseline")));
	}

	TEST_METHOD(ProgrammaticGenerationRejectsUnknownBackendBeforeGeneration)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("InvalidBackend"),
			FAngelscriptStaticJITBackendId::Bytecode());
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		TArray<asIScriptModule*> CompleteGraph;
		for (const FAngelscriptStaticJITGenerationModule& Module
			: Snapshot->Modules)
		{
			CompleteGraph.Add(Module.EngineLocalModule);
		}

		FAngelscriptJITProviderGenerationSettings Settings;
		Settings.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		FAngelscriptArtifactCanonicalWriter ProviderWriter(
			TEXT("static-jit-invalid-backend-test-provider"));
		ProviderWriter.WriteString(TEXT("InvalidBackend"));
		Settings.ProviderId.Hash = ProviderWriter.FinalizeHash();
		Settings.ProviderName = TEXT("Invalid Backend Test");
		Settings.OwnerModuleName = TEXT("AngelscriptTest");
		Settings.ArtifactProfile = Snapshot->ArtifactProfile;
		Settings.NativeEnvironmentFingerprint =
			FAngelscriptJITTargetProfiles::
				BuildCurrentNativeEnvironmentFingerprint(
					Settings.TargetProfile);

		FAngelscriptStaticJITProviderGenerationRequest Request;
		Request.BackendId = FAngelscriptStaticJITBackendId();
		Request.Settings = Settings;
		Request.CompiledSourceGraph = CompleteGraph;
		Request.GenerationSnapshot = Snapshot;
		FAngelscriptStaticJITGeneratorOutput Output;
		FString Error;
		ASSERT_THAT(IsFalse(GenerateStaticJITProviderArtifacts(
			Request, Output, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("bytecode"))));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("typed-ast"))));
		ASSERT_THAT(IsFalse(Output.bHasProviderOutput));
	}

	TEST_METHOD(ProgrammaticGenerationRequiresExactCaptureProfile)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		auto ExpectMismatch = [this](
			const FAngelscriptStaticJITBackendId& EngineBackendId,
			const FAngelscriptStaticJITBackendId& RequestedBackendId,
			const FStringView Suffix)
		{
			TUniquePtr<FFixture> Fixture = CreateFixture(
				*TestRunner, Suffix, EngineBackendId);
			ASSERT_THAT(IsNotNull(Fixture.Get()));
			const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
				ESPMode::ThreadSafe> Snapshot =
				Fixture->Engine->GetStaticJITGenerationSnapshot();
			ASSERT_THAT(IsTrue(Snapshot.IsValid()));
			TArray<asIScriptModule*> CompleteGraph;
			for (const FAngelscriptStaticJITGenerationModule& Module
				: Snapshot->Modules)
			{
				CompleteGraph.Add(Module.EngineLocalModule);
			}

			FAngelscriptJITProviderGenerationSettings Settings;
			Settings.TargetProfile =
				EAngelscriptJITTargetProfile::EditorDevelopment;
			FAngelscriptArtifactCanonicalWriter ProviderWriter(
				TEXT("static-jit-capture-mismatch-test-provider"));
			ProviderWriter.WriteString(Suffix);
			Settings.ProviderId.Hash = ProviderWriter.FinalizeHash();
			Settings.ProviderName = TEXT("Capture Mismatch Test");
			Settings.OwnerModuleName = TEXT("AngelscriptTest");
			Settings.ArtifactProfile = Snapshot->ArtifactProfile;
			Settings.NativeEnvironmentFingerprint =
				FAngelscriptJITTargetProfiles::
					BuildCurrentNativeEnvironmentFingerprint(
						Settings.TargetProfile);

			FAngelscriptStaticJITProviderGenerationRequest Request;
			Request.BackendId = RequestedBackendId;
			Request.Settings = Settings;
			FAngelscriptStaticJITGenerationProfile RequestProfile;
			FString RequestProfileError;
			ASSERT_THAT(IsTrue(
				FAngelscriptStaticJITGenerationProfile::TryCreate(
					RequestedBackendId, Settings.TargetProfile,
					RequestProfile,
					&RequestProfileError), RequestProfileError));
			Request.TypedASTJITCapabilityProfile =
				RequestProfile.TypedASTJITCapabilityProfile;
			Request.TypedASTJITExecutionRequirements =
				RequestProfile.TypedASTJITExecutionRequirements;
			Request.CompiledSourceGraph = CompleteGraph;
			Request.GenerationSnapshot = Snapshot;
			FAngelscriptStaticJITGeneratorOutput Output;
			FString Error;
			ASSERT_THAT(IsFalse(GenerateStaticJITProviderArtifacts(
				Request, Output, &Error)));
			ASSERT_THAT(IsTrue(Error.Contains(
				TEXT("CaptureProfileMismatch")), Error));
			ASSERT_THAT(IsFalse(Output.bHasProviderOutput));
		};

		ExpectMismatch(
			FAngelscriptStaticJITBackendId::Bytecode(),
			FAngelscriptStaticJITBackendId::TypedAST(),
			TEXT("TypedASTAgainstBytecodeCapture"));
		ExpectMismatch(
			FAngelscriptStaticJITBackendId::TypedAST(),
			FAngelscriptStaticJITBackendId::Bytecode(),
			TEXT("BytecodeAgainstTypedASTCapture"));
	}

	TEST_METHOD(DefaultComponentAnalysisFailsClosedWithoutCreatingNativeParentCDO)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		UClass* const CodeParent = CreateUnmaterializedNativeActorClass(
			*TestRunner,
			TEXT("JITGenerationDefaultComponentParent_65C28A"),
			AActor::StaticClass());
		ASSERT_THAT(IsNotNull(CodeParent));
		ON_SCOPE_EXIT
		{
			ReleaseTransientNativeClass(CodeParent);
		};
		const FPrimaryContainmentSnapshot PrimaryBefore =
			CapturePrimaryContainmentSnapshot();

		const FString SourceTemplate = ASTEST_AS(R"AS(
			UCLASS()
			class AJITGenerationDefaultComponentProbe_65C28A : __NATIVE_PARENT__
			{
				UPROPERTY(DefaultComponent)
				USceneComponent GeneratedComponent;
			}
			)AS");
		FString AdditionalSource = SourceTemplate;
		AdditionalSource.ReplaceInline(
			TEXT("__NATIVE_PARENT__"),
			*FAngelscriptType::GetBoundClassName(CodeParent),
			ESearchCase::CaseSensitive);
		TestRunner->AddExpectedErrorPlain(
			MissingNativeCDODiagnostic,
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("[StartupCompileFailure] Cannot run after startup compile failure"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner,
			TEXT("DefaultComponentMissingNativeCDO"),
			FAngelscriptStaticJITBackendId::Bytecode(),
			AdditionalSource,
			false);
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		ASSERT_THAT(IsFalse(
			Fixture->Engine->DidInitialCompileSucceed(),
			TEXT("Generation analysis must fail closed when native component metadata requires an absent CDO")));
		ASSERT_THAT(IsNull(
			CodeParent->GetDefaultObject(false),
			TEXT("Generation analysis must not create the native parent CDO")));
		ASSERT_THAT(IsTrue(
			Fixture->Engine->FormatDiagnostics().Contains(
				MissingNativeCDODiagnostic),
			TEXT("Generation analysis should explain that missing native CDO state is intentionally not materialized")));
		ASSERT_THAT(AreEqual(1, Fixture->StartupCompileFailureExitCount));
		ASSERT_THAT(IsTrue(Fixture->bStartupCompileFailureExitForce));
		ASSERT_THAT(AreEqual(
			static_cast<uint8>(3),
			Fixture->StartupCompileFailureExitStatus));
		ASSERT_THAT(IsTrue(AssertGenerationServicesSuppressed(
			*TestRunner, *Fixture)));
		ASSERT_THAT(IsTrue(AssertNoGenerationPersistence(
			*TestRunner, Fixture->Root,
			TEXTVIEW("descriptor analysis failure"))));
		Fixture.Reset();

		const FPrimaryContainmentSnapshot PrimaryAfter =
			CapturePrimaryContainmentSnapshot();
		ASSERT_THAT(IsTrue(AssertPrimaryContainmentUnchanged(
			*TestRunner, PrimaryBefore, PrimaryAfter,
			TEXTVIEW("descriptor analysis failure"))));
	}

	TEST_METHOD(OverrideComponentAnalysisFailsClosedWithoutCreatingNativeParentCDO)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		UClass* const CodeParent = CreateUnmaterializedNativeActorClass(
			*TestRunner,
			TEXT("JITGenerationOverrideComponentParent_65C28A"),
			ACharacter::StaticClass());
		ASSERT_THAT(IsNotNull(CodeParent));
		ON_SCOPE_EXIT
		{
			ReleaseTransientNativeClass(CodeParent);
		};

		const FString SourceTemplate = ASTEST_AS(R"AS(
			UCLASS()
			class AJITGenerationOverrideComponentProbe_65C28A : __NATIVE_PARENT__
			{
				UPROPERTY(OverrideComponent = MissingJITGenerationComponent_65C28A)
				UCapsuleComponent ReplacementCapsule;
			}
			)AS");
		FString AdditionalSource = SourceTemplate;
		AdditionalSource.ReplaceInline(
			TEXT("__NATIVE_PARENT__"),
			*FAngelscriptType::GetBoundClassName(CodeParent),
			ESearchCase::CaseSensitive);
		TestRunner->AddExpectedErrorPlain(
			MissingNativeCDODiagnostic,
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("[StartupCompileFailure] Cannot run after startup compile failure"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner,
			TEXT("OverrideComponentMissingNativeCDO"),
			FAngelscriptStaticJITBackendId::Bytecode(),
			AdditionalSource,
			false);
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		ASSERT_THAT(IsFalse(
			Fixture->Engine->DidInitialCompileSucceed(),
			TEXT("Generation analysis must fail closed when override metadata requires an absent native CDO")));
		ASSERT_THAT(IsNull(
			CodeParent->GetDefaultObject(false),
			TEXT("Generation analysis must not create the native parent CDO while resolving an override target")));
		ASSERT_THAT(IsTrue(
			Fixture->Engine->FormatDiagnostics().Contains(
				MissingNativeCDODiagnostic),
			TEXT("Generation analysis should report that override metadata could not be proven without materialization")));
		ASSERT_THAT(AreEqual(1, Fixture->StartupCompileFailureExitCount));
		ASSERT_THAT(IsTrue(Fixture->bStartupCompileFailureExitForce));
		ASSERT_THAT(AreEqual(
			static_cast<uint8>(3),
			Fixture->StartupCompileFailureExitStatus));
	}

	TEST_METHOD(OverrideComponentUsesExistingAncestorCDOWithoutCreatingNativeParentCDO)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		UObject* const ExistingCharacterCDO =
			ACharacter::StaticClass()->GetDefaultObject();
		ASSERT_THAT(IsNotNull(ExistingCharacterCDO));
		ASSERT_THAT(IsNotNull(ExistingCharacterCDO->GetDefaultSubobjectByName(
			ACharacter::CapsuleComponentName)));

		UClass* const CodeParent = CreateUnmaterializedNativeActorClass(
			*TestRunner,
			TEXT("JITGenerationExistingAncestorCDOParent_65C28A"),
			ACharacter::StaticClass());
		ASSERT_THAT(IsNotNull(CodeParent));
		ON_SCOPE_EXIT
		{
			ReleaseTransientNativeClass(CodeParent);
		};

		const FString SourceTemplate = ASTEST_AS(R"AS(
			UCLASS()
			class AJITGenerationExistingAncestorCDOProbe_65C28A : __NATIVE_PARENT__
			{
				UPROPERTY(OverrideComponent = __OVERRIDE_TARGET__)
				UCapsuleComponent ReplacementCapsule;
			}
			)AS");
		FString AdditionalSource = SourceTemplate;
		AdditionalSource.ReplaceInline(
			TEXT("__NATIVE_PARENT__"),
			*FAngelscriptType::GetBoundClassName(CodeParent),
			ESearchCase::CaseSensitive);
		AdditionalSource.ReplaceInline(
			TEXT("__OVERRIDE_TARGET__"),
			*ACharacter::CapsuleComponentName.ToString(),
			ESearchCase::CaseSensitive);
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner,
			TEXT("OverrideComponentExistingAncestorCDO"),
			FAngelscriptStaticJITBackendId::Bytecode(),
			AdditionalSource);
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		ASSERT_THAT(IsTrue(Fixture->Engine->DidInitialCompileSucceed()));
		ASSERT_THAT(IsNull(
			CodeParent->GetDefaultObject(false),
			TEXT("Generation analysis may read an existing ancestor CDO but must not materialize the direct native parent")));
		ASSERT_THAT(AreEqual(0, Fixture->StartupCompileFailureExitCount));
		ASSERT_THAT(IsFalse(Fixture->Engine->FormatDiagnostics().Contains(
			MissingNativeCDODiagnostic)));
	}

	TEST_METHOD(DescriptorAnalysisDoesNotMaterializeScriptReflection)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		ASSERT_THAT(IsNull(FindObject<UObject>(
			FAngelscriptEngine::GetPackage(), ClassName)));
		ASSERT_THAT(IsNull(FindObject<UObject>(
			FAngelscriptEngine::GetPackage(), StructName)));
		ASSERT_THAT(IsNull(FindObject<UObject>(
			FAngelscriptEngine::GetPackage(), DelegateName)));

		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("NoReflection"));
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		ASSERT_THAT(AreEqual(
			EAngelscriptEnginePurpose::StaticJITGeneration,
			Fixture->Engine->GetPurpose()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		ASSERT_THAT(IsTrue(Snapshot->IsComplete()));
		ASSERT_THAT(IsFalse(Snapshot->Modules.IsEmpty()));
		ASSERT_THAT(IsFalse(Snapshot->Functions.IsEmpty()));
		ASSERT_THAT(IsFalse(Snapshot->Types.IsEmpty()));
		ASSERT_THAT(IsFalse(Snapshot->Globals.IsEmpty()));
		ASSERT_THAT(AreEqual(
			0u, Snapshot->InputCacheRecordCount));

		const FAngelscriptStaticJITGenerationDescriptor* Class =
			FindDescriptor(*Snapshot, ClassName,
				EAngelscriptStaticJITGenerationDescriptorKind::Class);
		const FAngelscriptStaticJITGenerationDescriptor* Struct =
			FindDescriptor(*Snapshot, StructName,
				EAngelscriptStaticJITGenerationDescriptorKind::Struct);
		const FAngelscriptStaticJITGenerationDescriptor* Delegate =
			FindDescriptor(*Snapshot, DelegateName,
				EAngelscriptStaticJITGenerationDescriptorKind::Delegate);
		const FAngelscriptStaticJITGenerationDescriptor* Method =
			FindDescriptor(*Snapshot, MethodName,
				EAngelscriptStaticJITGenerationDescriptorKind::Function);
		const FAngelscriptStaticJITGenerationDescriptor* StaticsClass =
			FindDescriptor(*Snapshot, StaticsClassName,
				EAngelscriptStaticJITGenerationDescriptorKind::Class);
		const FAngelscriptStaticJITGenerationDescriptor* GlobalUFunction =
			FindDescriptor(*Snapshot, GlobalUFunctionName,
				EAngelscriptStaticJITGenerationDescriptorKind::Function);
		ASSERT_THAT(IsNotNull(Class));
		ASSERT_THAT(IsNotNull(Struct));
		ASSERT_THAT(IsNotNull(Delegate));
		ASSERT_THAT(IsNotNull(Method));
		ASSERT_THAT(IsNotNull(StaticsClass));
		ASSERT_THAT(IsNotNull(GlobalUFunction));
		ASSERT_THAT(IsFalse(StaticsClass->StableDescriptorKey.IsZero()));
		ASSERT_THAT(AreEqual(
			FString::Printf(TEXT("class %s"), StaticsClassName),
			StaticsClass->CanonicalDeclaration));
		ASSERT_THAT(IsTrue(GlobalUFunction->bUFunctionRoot));
		ASSERT_THAT(IsTrue(GlobalUFunction->ReceiverCanonicalName.IsEmpty()));
		ASSERT_THAT(IsTrue(Method->bUFunctionRoot));
		ASSERT_THAT(AreEqual(FString(ClassName),
			Method->ReceiverCanonicalName));
		ASSERT_THAT(IsFalse(Method->EntryPlan.EntryPlanHash.IsZero()));
		ASSERT_THAT(IsTrue(Method->EntryPlan.bHasVMEntry));
		ASSERT_THAT(IsTrue(Method->EntryPlan.bHasRawEntry));
		const FAngelscriptStaticJITGenerationFunction* const EmitFunction =
			Snapshot->Functions.FindByPredicate([](const auto& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("EvaluateGenerationEmit_65C28A"));
			});
		ASSERT_THAT(IsNotNull(EmitFunction));
		ASSERT_THAT(IsTrue(EmitFunction->bHasVerifiedArtifactIdentity));
		ASSERT_THAT(IsTrue(EmitFunction->bHasVerifiedArtifactReferences));
		for (const FAngelscriptStaticJITGenerationFunction& Function
			: Snapshot->Functions)
		{
			ASSERT_THAT(IsFalse(Function.EntryAbiHash.IsZero()));
			ASSERT_THAT(IsTrue(Function.bHasVMEntry));
			ASSERT_THAT(IsTrue(Function.bHasRawEntry));
			ASSERT_THAT(AreEqual(
				FAngelscriptBytecodeJIT::CanGenerateParmsEntry(
					Function.EngineLocalFunction),
				Function.bHasParmsEntry));
		}

		const FAngelscriptStaticJITGenerationType* ReflectedClassType =
			Snapshot->Types.FindByPredicate([](const auto& Type)
			{
				return Type.CanonicalDeclaration.Contains(ClassName);
			});
		const FAngelscriptStaticJITGenerationType* RawClassType =
			Snapshot->Types.FindByPredicate([](const auto& Type)
			{
				return Type.CanonicalDeclaration.Contains(RawClassName);
			});
		ASSERT_THAT(IsNotNull(ReflectedClassType));
		ASSERT_THAT(IsNotNull(RawClassType));
		ASSERT_THAT(IsTrue(
			ReflectedClassType->bTargetMaterializesScriptReflection));
		ASSERT_THAT(IsTrue(
			RawClassType->bTargetMaterializesScriptReflection));
		FAngelscriptBytecodeJIT TargetJIT;
		for (const FAngelscriptStaticJITGenerationType& Type : Snapshot->Types)
		{
			if (Type.bTargetMaterializesScriptReflection && Type.EngineLocalType != nullptr)
			{
				TargetJIT.TargetMaterializedScriptTypes.Add(
					static_cast<const asCObjectType*>(Type.EngineLocalType));
			}
		}
		ASSERT_THAT(IsFalse(TargetJIT.IsRawScriptReference(
			static_cast<const asCObjectType*>(
				ReflectedClassType->EngineLocalType))));
		ASSERT_THAT(IsFalse(TargetJIT.IsRawScriptReference(
			static_cast<const asCObjectType*>(
				RawClassType->EngineLocalType))));
		asCObjectType SyntheticRawType(static_cast<asCScriptEngine*>(
			Fixture->Engine->GetScriptEngine()));
		SyntheticRawType.flags = asOBJ_SCRIPT_OBJECT;
		ASSERT_THAT(IsTrue(TargetJIT.IsRawScriptReference(
			&SyntheticRawType)));

		ASSERT_THAT(IsNull(FindObject<UObject>(
			FAngelscriptEngine::GetPackage(), ClassName)));
		ASSERT_THAT(IsNull(FindObject<UObject>(
			FAngelscriptEngine::GetPackage(), StructName)));
		ASSERT_THAT(IsNull(FindObject<UObject>(
			FAngelscriptEngine::GetPackage(), DelegateName)));
		ASSERT_THAT(AreEqual(0, Fixture->FullReloadCount));
		ASSERT_THAT(AreEqual(0, Fixture->PostReloadCount));
		ASSERT_THAT(AreEqual(0, Fixture->ClassReloadCount));
		ASSERT_THAT(AreEqual(0, Fixture->StructReloadCount));
		ASSERT_THAT(AreEqual(0, Fixture->DelegateReloadCount));
		ASSERT_THAT(AreEqual(0, Fixture->CompilationEventCount));
		TArray<FString> BindCacheFiles;
		IFileManager::Get().FindFilesRecursive(
			BindCacheFiles, *Fixture->Root, TEXT("Binds.Cache"),
			true, false, false);
		ASSERT_THAT(IsTrue(BindCacheFiles.IsEmpty()));
#if !UE_BUILD_SHIPPING
		ASSERT_THAT(IsFalse(
			FCoreDelegates::OnGetOnScreenMessages.IsBoundToObject(
				Fixture->Engine.Get())));
#endif
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes =
			Fixture->Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(!Routes.IsValid()
			|| Routes->FunctionRoutes.IsEmpty()));
	}

	TEST_METHOD(TwoGenerationEnginesReplayBindsAndKeepStableIdentityIsolated)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		FAngelscriptBindExecutionObservation::Reset();
		asCContext* const PrimaryContextBefore =
			FAngelscriptEngine::GameThreadTLD != nullptr
			? FAngelscriptEngine::GameThreadTLD->primaryContext
			: nullptr;
		const int32 BlueprintEventCountBefore =
			GetBlueprintEventsByScriptNameTotalCount();
		TUniquePtr<FFixture> First = CreateFixture(
			*TestRunner, TEXT("First"));
		TUniquePtr<FFixture> Second = CreateFixture(
			*TestRunner, TEXT("Second"));
		ASSERT_THAT(IsNotNull(First.Get()));
		ASSERT_THAT(IsNotNull(Second.Get()));
		ASSERT_THAT(AreEqual(2,
			FAngelscriptBindExecutionObservation::GetInvocationCount()));
		ASSERT_THAT(IsTrue(FAngelscriptEngine::GameThreadTLD == nullptr
			|| FAngelscriptEngine::GameThreadTLD->primaryContext
				== PrimaryContextBefore));
		ASSERT_THAT(AreEqual(BlueprintEventCountBefore,
			GetBlueprintEventsByScriptNameTotalCount()));

		asIScriptEngine* FirstScriptEngine = First->Engine->GetScriptEngine();
		asIScriptEngine* SecondScriptEngine = Second->Engine->GetScriptEngine();
		ASSERT_THAT(IsTrue(FirstScriptEngine != SecondScriptEngine));
		asITypeInfo* FirstNativeType =
			FirstScriptEngine->GetTypeInfoByDecl("FString");
		asITypeInfo* SecondNativeType =
			SecondScriptEngine->GetTypeInfoByDecl("FString");
		ASSERT_THAT(IsNotNull(FirstNativeType));
		ASSERT_THAT(IsNotNull(SecondNativeType));
		ASSERT_THAT(IsTrue(FirstNativeType != SecondNativeType));

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> FirstSnapshot =
			First->Engine->GetStaticJITGenerationSnapshot();
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> SecondSnapshot =
			Second->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(FirstSnapshot.IsValid()));
		ASSERT_THAT(IsTrue(SecondSnapshot.IsValid()));
		ASSERT_THAT(AreEqual(FirstSnapshot->Modules.Num(),
			SecondSnapshot->Modules.Num()));
		ASSERT_THAT(AreEqual(FirstSnapshot->Functions.Num(),
			SecondSnapshot->Functions.Num()));

		const FAngelscriptStaticJITGenerationType* FirstClass =
			FirstSnapshot->Types.FindByPredicate([](const auto& Type)
			{
				return Type.CanonicalDeclaration.Contains(ClassName);
			});
		const FAngelscriptStaticJITGenerationType* SecondClass =
			SecondSnapshot->Types.FindByPredicate([](const auto& Type)
			{
				return Type.CanonicalDeclaration.Contains(ClassName);
			});
		ASSERT_THAT(IsNotNull(FirstClass));
		ASSERT_THAT(IsNotNull(SecondClass));
		ASSERT_THAT(IsTrue(FirstClass->EngineLocalType
			!= SecondClass->EngineLocalType));
		ASSERT_THAT(IsTrue(FirstClass->EngineLocalType->GetEngine()
			== FirstScriptEngine));
		ASSERT_THAT(IsTrue(SecondClass->EngineLocalType->GetEngine()
			== SecondScriptEngine));
		ASSERT_THAT(IsTrue(FirstClass->TypeKey == SecondClass->TypeKey));

		const FAngelscriptStaticJITGenerationFunction& FirstFunction =
			FirstSnapshot->Functions[0];
		const FAngelscriptStaticJITGenerationFunction* SecondFunction =
			SecondSnapshot->Functions.FindByPredicate(
				[&FirstFunction](const auto& Candidate)
				{
					return Candidate.FunctionKey
						== FirstFunction.FunctionKey;
				});
		ASSERT_THAT(IsNotNull(SecondFunction));
		ASSERT_THAT(IsTrue(FirstFunction.EngineLocalFunction
			!= SecondFunction->EngineLocalFunction));
		ASSERT_THAT(IsTrue(FirstFunction.ExecutionHash
			== SecondFunction->ExecutionHash));
		ASSERT_THAT(IsTrue(FirstFunction.DebugHash
			== SecondFunction->DebugHash));

		First.Reset();
		ASSERT_THAT(IsTrue(FAngelscriptEngine::GameThreadTLD == nullptr
			|| FAngelscriptEngine::GameThreadTLD->primaryContext
				== PrimaryContextBefore));
		ASSERT_THAT(AreEqual(BlueprintEventCountBefore,
			GetBlueprintEventsByScriptNameTotalCount()));
		ASSERT_THAT(IsTrue(SecondFunction->EngineLocalFunction->GetEngine()
			== SecondScriptEngine));
		ASSERT_THAT(IsTrue(SecondClass->EngineLocalType->GetEngine()
			== SecondScriptEngine));
		ASSERT_THAT(IsNotNull(SecondScriptEngine->GetTypeInfoByDecl(
			"FString")));
		Second.Reset();
		ASSERT_THAT(IsTrue(FAngelscriptEngine::GameThreadTLD == nullptr
			|| FAngelscriptEngine::GameThreadTLD->primaryContext
				== PrimaryContextBefore));
		ASSERT_THAT(AreEqual(BlueprintEventCountBefore,
			GetBlueprintEventsByScriptNameTotalCount()));
	}

	TEST_METHOD(PlainStructPropertiesHaveStableDescriptorIdentity)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		static constexpr TCHAR PlainStructName[] =
			TEXT("FJITGenerationOnlyPlainPayload_65C28A");
		static constexpr TCHAR PlainPropertyName[] =
			TEXT("PlainHiddenValue_65C28A");
		const FString AdditionalSource = FString::Printf(TEXT(R"AS(
struct %s
{
	int %s;
}
)AS"), PlainStructName, PlainPropertyName);

		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("PlainStructPropertyIdentity"),
			FAngelscriptStaticJITBackendId::Bytecode(), AdditionalSource);
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		ASSERT_THAT(IsTrue(Snapshot->IsComplete()));

		const FAngelscriptStaticJITGenerationDescriptor* PlainStruct =
			FindDescriptor(*Snapshot, PlainStructName,
				EAngelscriptStaticJITGenerationDescriptorKind::Struct);
		const FAngelscriptStaticJITGenerationDescriptor* PlainProperty =
			FindDescriptor(*Snapshot, PlainPropertyName,
				EAngelscriptStaticJITGenerationDescriptorKind::Property);
		ASSERT_THAT(IsNotNull(PlainStruct));
		ASSERT_THAT(IsNotNull(PlainProperty));
		ASSERT_THAT(IsFalse(PlainStruct->StableDescriptorKey.IsZero()));
		ASSERT_THAT(IsFalse(PlainProperty->StableDescriptorKey.IsZero()));
		ASSERT_THAT(AreEqual(FString(TEXT("int")),
			PlainProperty->CanonicalDeclaration));
		ASSERT_THAT(AreEqual(FString(PlainStructName),
			PlainProperty->ReceiverCanonicalName));
	}

	TEST_METHOD(SuccessfulGenerationDoesNotAcquirePackagesOrSweepPrimaryContextPool)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		FAngelscriptEngine& PrimaryEngine = FAngelscriptEngine::Get();
		asIScriptEngine* const PrimaryScriptEngine =
			PrimaryEngine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(PrimaryScriptEngine));

		asIScriptContext* const SeedContext =
			PrimaryScriptEngine->RequestContext();
		ASSERT_THAT(IsNotNull(SeedContext));
		PrimaryScriptEngine->ReturnContext(SeedContext);
		const int32 PrimaryPoolCountBefore =
			FAngelscriptEngine::GetLocalPooledContextCountForTesting(
				PrimaryScriptEngine);
		ASSERT_THAT(IsTrue(PrimaryPoolCountBefore > 0));
		const FAngelscriptProcessPackageReferenceCounts PackageCountsBefore =
			FAngelscriptEngine::GetProcessPackageReferenceCountsForTesting();
		const FPrimaryContainmentSnapshot PrimaryBefore =
			CapturePrimaryContainmentSnapshot();

		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("PrimaryContainment"));
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		ASSERT_THAT(IsTrue(AssertGenerationServicesSuppressed(
			*TestRunner, *Fixture)));
		ASSERT_THAT(AreEqual(
			PackageCountsBefore.AngelscriptPackage,
			FAngelscriptEngine::GetProcessPackageReferenceCountsForTesting()
				.AngelscriptPackage,
			TEXT("Generation must not acquire /Script/Angelscript ownership")));
		ASSERT_THAT(AreEqual(
			PackageCountsBefore.AngelscriptAssetsPackage,
			FAngelscriptEngine::GetProcessPackageReferenceCountsForTesting()
				.AngelscriptAssetsPackage,
			TEXT("Generation must not acquire /Script/AngelscriptAssets ownership")));
		ASSERT_THAT(AreEqual(
			PrimaryPoolCountBefore,
			FAngelscriptEngine::GetLocalPooledContextCountForTesting(
				PrimaryScriptEngine),
			TEXT("Generation startup must not sweep another Engine's pooled contexts")));
		ASSERT_THAT(IsTrue(AssertNoGenerationPersistence(
			*TestRunner, Fixture->Root,
			TEXTVIEW("successful generation"))));

		Fixture.Reset();
		ASSERT_THAT(AreEqual(
			PackageCountsBefore.AngelscriptPackage,
			FAngelscriptEngine::GetProcessPackageReferenceCountsForTesting()
				.AngelscriptPackage));
		ASSERT_THAT(AreEqual(
			PackageCountsBefore.AngelscriptAssetsPackage,
			FAngelscriptEngine::GetProcessPackageReferenceCountsForTesting()
				.AngelscriptAssetsPackage));
		ASSERT_THAT(AreEqual(
			PrimaryPoolCountBefore,
			FAngelscriptEngine::GetLocalPooledContextCountForTesting(
				PrimaryScriptEngine),
			TEXT("Generation teardown must not sweep another Engine's pooled contexts")));
		const FPrimaryContainmentSnapshot PrimaryAfter =
			CapturePrimaryContainmentSnapshot();
		ASSERT_THAT(IsTrue(AssertPrimaryContainmentUnchanged(
			*TestRunner, PrimaryBefore, PrimaryAfter,
			TEXTVIEW("successful generation"))));
	}

	TEST_METHOD(BindReplayFailureRestoresCompletePrimaryContainmentSnapshot)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		const FPrimaryContainmentSnapshot PrimaryBefore =
			CapturePrimaryContainmentSnapshot();

		FAngelscriptEngineConfig Config;
		Config.Purpose = EAngelscriptEnginePurpose::StaticJITGeneration;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.bRunningCommandlet = true;
		Config.bIsUnattended = true;
		Config.bDisableCacheV2Persistence = true;
		Config.bInjectDirectBindFailureForTesting = true;
		const FString FailureRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/StaticJITGenerationEngine"),
				TEXT("BindReplayFailureContainment"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		Config.CacheV2RootOverride = FPaths::Combine(
			FailureRoot, TEXT("CacheV2"));
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*FailureRoot, false, true);
		};
		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		FString ProfileError;
		ASSERT_THAT(IsTrue(
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::Bytecode(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				GenerationProfile, &ProfileError),
			ProfileError));
		GenerationProfile.ApplyToEngineConfig(Config);

		TestRunner->AddExpectedErrorPlain(
			TEXT("Failed in call to function 'RegisterGlobalFunction' with 'void __InjectedDirectBindPublicationFailure()'"),
			EAutomationExpectedErrorFlags::Contains,
			0);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Direct AngelScript binding execution failed:"),
			EAutomationExpectedErrorFlags::Contains,
			0);
		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [FailureRoot]()
		{
			return FailureRoot;
		};
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		TUniquePtr<FAngelscriptEngine> FailedEngine =
			FAngelscriptEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsNull(FailedEngine.Get()));
		ASSERT_THAT(IsTrue(AssertNoGenerationPersistence(
			*TestRunner, FailureRoot,
			TEXTVIEW("Bind replay failure"))));

		const FPrimaryContainmentSnapshot PrimaryAfter =
			CapturePrimaryContainmentSnapshot();
		ASSERT_THAT(IsTrue(AssertPrimaryContainmentUnchanged(
			*TestRunner, PrimaryBefore, PrimaryAfter,
			TEXTVIEW("Bind replay failure"))));
	}

	TEST_METHOD(SourceCompileFailureRestoresCompletePrimaryContainmentSnapshot)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		const FPrimaryContainmentSnapshot PrimaryBefore =
			CapturePrimaryContainmentSnapshot();
		TestRunner->AddExpectedErrorPlain(
			TEXT("Expected expression value"),
			EAutomationExpectedErrorFlags::Contains,
			0);
		TestRunner->AddExpectedErrorPlain(
			TEXT("StaticJITGenerationOnly.as:"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Instead found ';'"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("[StartupCompileFailure] Cannot run after startup compile failure"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		const FString InvalidSource = ASTEST_AS(R"AS(
			int InvalidGenerationSource_65C28A()
			{
				return 1 + ;
			}
			)AS");
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner,
			TEXT("SourceCompileFailureContainment"),
			FAngelscriptStaticJITBackendId::TypedAST(),
			InvalidSource,
			false);
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		ASSERT_THAT(IsFalse(Fixture->Engine->DidInitialCompileSucceed()));
		ASSERT_THAT(IsFalse(
			Fixture->Engine->GetStaticJITGenerationSnapshot().IsValid()));
		ASSERT_THAT(IsTrue(AssertGenerationServicesSuppressed(
			*TestRunner, *Fixture)));
		ASSERT_THAT(IsTrue(AssertNoGenerationPersistence(
			*TestRunner, Fixture->Root,
			TEXTVIEW("source compile failure"))));
		Fixture.Reset();

		const FPrimaryContainmentSnapshot PrimaryAfter =
			CapturePrimaryContainmentSnapshot();
		ASSERT_THAT(IsTrue(AssertPrimaryContainmentUnchanged(
			*TestRunner, PrimaryBefore, PrimaryAfter,
			TEXTVIEW("source compile failure"))));
	}

	TEST_METHOD(TypedHIRVerificationFailureRestoresCompletePrimaryContainmentSnapshot)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		const FPrimaryContainmentSnapshot PrimaryBefore =
			CapturePrimaryContainmentSnapshot();
		TestRunner->AddExpectedErrorPlain(
			TEXT("published invalid typed HIR"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("[StartupCompileFailure] Cannot run after startup compile failure"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner,
			TEXT("TypedHIRVerificationFailureContainment"),
			FAngelscriptStaticJITBackendId::TypedAST(),
			FStringView(),
			false,
			[](FAngelscriptEngineConfig& Config)
			{
				Config.bInjectInvalidTypedSemanticIRForTesting = true;
			});
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		ASSERT_THAT(IsFalse(Fixture->Engine->DidInitialCompileSucceed()));
		ASSERT_THAT(IsFalse(
			Fixture->Engine->GetStaticJITGenerationSnapshot().IsValid()));
		ASSERT_THAT(IsTrue(AssertGenerationServicesSuppressed(
			*TestRunner, *Fixture)));
		ASSERT_THAT(IsTrue(AssertNoGenerationPersistence(
			*TestRunner, Fixture->Root,
			TEXTVIEW("typed HIR verification failure"))));
		Fixture.Reset();

		const FPrimaryContainmentSnapshot PrimaryAfter =
			CapturePrimaryContainmentSnapshot();
		ASSERT_THAT(IsTrue(AssertPrimaryContainmentUnchanged(
			*TestRunner, PrimaryBefore, PrimaryAfter,
			TEXTVIEW("typed HIR verification failure"))));
	}

	TEST_METHOD(BackendEmissionFailureReleasesGenerationRequestState)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		const FPrimaryContainmentSnapshot PrimaryBefore =
			CapturePrimaryContainmentSnapshot();
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner,
			TEXT("BackendEmissionFailureContainment"),
			FAngelscriptStaticJITBackendId::TypedAST());
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		ASSERT_THAT(IsTrue(AssertGenerationServicesSuppressed(
			*TestRunner, *Fixture)));
		TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		ASSERT_THAT(IsTrue(Snapshot->bHasVerifiedTypedHIR));

		{
			FCompiledGraphStorage Storage = BuildCompiledGraphStorage(
				*Snapshot, TEXTVIEW("Emission Failure Containment"));
			const FAngelscriptStaticJITGenerationModule* const EmitModule =
				Snapshot->Modules.FindByPredicate([](const auto& Candidate)
				{
					return Candidate.CanonicalModuleName.Contains(
						EmitModuleName, ESearchCase::CaseSensitive);
				});
			ASSERT_THAT(IsNotNull(EmitModule));
			const FAngelscriptStableModuleKey EmitModuleSet[] = {
				EmitModule->ModuleKey};
			const FAngelscriptStaticJITBackendRequest Request{
				FAngelscriptStaticJITBackendId::TypedAST(),
				Storage.Graph,
				EmitModuleSet};

			FContainmentBackendFactory TypedAST(
				EContainmentBackendMode::FailEmission);
			FAngelscriptStaticJITBackendRegistry Registry;
			ASSERT_THAT(IsTrue(
				Registry.RegisterFactory(TypedAST).IsAccepted()));
			FAngelscriptStaticJITGenerator Generator(Registry);
			FAngelscriptStaticJITGeneratorOutput Output;
			FString Error;
			ASSERT_THAT(IsFalse(Generator.Generate(Request, Output, &Error)));
			ASSERT_THAT(AreEqual(
				FString(TEXT("injected contained backend emission failure")),
				Error));
			ASSERT_THAT(AreEqual(Error, Output.Diagnostics.TaskError));
			ASSERT_THAT(IsFalse(Output.bHasProviderOutput));
			ASSERT_THAT(IsTrue(Output.ProviderOutput.Files.IsEmpty()));
			ASSERT_THAT(AreEqual(1, TypedAST.CreateCount));
			ASSERT_THAT(AreEqual(1, TypedAST.GenerateCount));
			ASSERT_THAT(AreEqual(1, TypedAST.DestroyCount));
			ASSERT_THAT(AreEqual(
				Snapshot->Modules.Num(), TypedAST.SeenModuleCount));
			ASSERT_THAT(AreEqual(
				Snapshot->Functions.Num(), TypedAST.SeenFunctionCount));
			ASSERT_THAT(IsTrue(TypedAST.SeenVerifiedHIRCount > 0));
		}

		ASSERT_THAT(IsTrue(AssertNoGenerationPersistence(
			*TestRunner, Fixture->Root,
			TEXTVIEW("backend emission failure"))));
		Snapshot.Reset();
		Fixture.Reset();
		const FPrimaryContainmentSnapshot PrimaryAfter =
			CapturePrimaryContainmentSnapshot();
		ASSERT_THAT(IsTrue(AssertPrimaryContainmentUnchanged(
			*TestRunner, PrimaryBefore, PrimaryAfter,
			TEXTVIEW("backend emission failure"))));
	}

	TEST_METHOD(PackagingFailureReleasesGenerationRequestState)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		const FPrimaryContainmentSnapshot PrimaryBefore =
			CapturePrimaryContainmentSnapshot();
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner,
			TEXT("PackagingFailureContainment"),
			FAngelscriptStaticJITBackendId::TypedAST());
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		ASSERT_THAT(IsTrue(AssertGenerationServicesSuppressed(
			*TestRunner, *Fixture)));
		TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		ASSERT_THAT(IsTrue(Snapshot->bHasVerifiedTypedHIR));

		{
			FCompiledGraphStorage Storage = BuildCompiledGraphStorage(
				*Snapshot, TEXTVIEW("Packaging Failure Containment"));
			Storage.Graph.Settings.ProviderId = {};
			const FAngelscriptStaticJITGenerationModule* const EmitModule =
				Snapshot->Modules.FindByPredicate([](const auto& Candidate)
				{
					return Candidate.CanonicalModuleName.Contains(
						EmitModuleName, ESearchCase::CaseSensitive);
				});
			ASSERT_THAT(IsNotNull(EmitModule));
			const FAngelscriptStableModuleKey EmitModuleSet[] = {
				EmitModule->ModuleKey};
			const FAngelscriptStaticJITBackendRequest Request{
				FAngelscriptStaticJITBackendId::TypedAST(),
				Storage.Graph,
				EmitModuleSet};

			FContainmentBackendFactory TypedAST(
				EContainmentBackendMode::EmitForPackaging);
			FAngelscriptStaticJITBackendRegistry Registry;
			ASSERT_THAT(IsTrue(
				Registry.RegisterFactory(TypedAST).IsAccepted()));
			FAngelscriptStaticJITGenerator Generator(Registry);
			FAngelscriptStaticJITGeneratorOutput Output;
			FString Error;
			ASSERT_THAT(IsFalse(Generator.Generate(Request, Output, &Error)));
			ASSERT_THAT(IsTrue(Error.Contains(TEXT("non-zero ProviderId")), Error));
			ASSERT_THAT(AreEqual(Error, Output.Diagnostics.TaskError));
			ASSERT_THAT(IsFalse(Output.bHasProviderOutput));
			ASSERT_THAT(IsTrue(Output.ProviderOutput.Files.IsEmpty()));
			ASSERT_THAT(AreEqual(1, TypedAST.CreateCount));
			ASSERT_THAT(AreEqual(1, TypedAST.GenerateCount));
			ASSERT_THAT(AreEqual(1, TypedAST.DestroyCount));
			ASSERT_THAT(AreEqual(
				Snapshot->Modules.Num(), TypedAST.SeenModuleCount));
			ASSERT_THAT(AreEqual(
				Snapshot->Functions.Num(), TypedAST.SeenFunctionCount));
			ASSERT_THAT(IsTrue(TypedAST.SeenVerifiedHIRCount > 0));
		}

		ASSERT_THAT(IsTrue(AssertNoGenerationPersistence(
			*TestRunner, Fixture->Root,
			TEXTVIEW("packaging failure"))));
		Snapshot.Reset();
		Fixture.Reset();
		const FPrimaryContainmentSnapshot PrimaryAfter =
			CapturePrimaryContainmentSnapshot();
		ASSERT_THAT(IsTrue(AssertPrimaryContainmentUnchanged(
			*TestRunner, PrimaryBefore, PrimaryAfter,
			TEXTVIEW("packaging failure"))));
	}

	TEST_METHOD(GenerationSnapshotDrivesCompleteGraphWithSeparateEmitSet)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		FAngelscriptBindExecutionObservation::Reset();
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner,
			TEXT("BackendGraph"),
			FAngelscriptStaticJITBackendId::TypedAST());
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		ASSERT_THAT(AreEqual(
			1, FAngelscriptBindExecutionObservation::GetInvocationCount()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(1u, Snapshot->SourceCompilePassCount));
		ASSERT_THAT(AreEqual(2, Snapshot->Modules.Num()));
		ASSERT_THAT(IsFalse(Snapshot->Functions.IsEmpty()));
		ASSERT_THAT(IsFalse(Snapshot->Types.IsEmpty()));
		ASSERT_THAT(IsFalse(Snapshot->Globals.IsEmpty()));
		ASSERT_THAT(IsFalse(Snapshot->Descriptors.IsEmpty()));
		ASSERT_THAT(IsFalse(Snapshot->Dependencies.IsEmpty()));
		ASSERT_THAT(IsFalse(Snapshot->ExternalNativeCalls.IsEmpty()));
		ASSERT_THAT(IsTrue(Snapshot->Functions.ContainsByPredicate(
			[](const FAngelscriptStaticJITGenerationFunction& Function)
			{
				return Function.VerifiedTypedHIR != nullptr;
			})));

		TArray<asIScriptModule*> CompleteGraph;
		asIScriptModule* EmitModule = nullptr;
		FAngelscriptStableModuleKey EmitModuleKey;
		for (const FAngelscriptStaticJITGenerationModule& Module
			: Snapshot->Modules)
		{
			CompleteGraph.Add(Module.EngineLocalModule);
			if (Module.CanonicalModuleName.Contains(EmitModuleName))
			{
				EmitModule = Module.EngineLocalModule;
				EmitModuleKey = Module.ModuleKey;
			}
		}
		ASSERT_THAT(IsNotNull(EmitModule));

		FAngelscriptJITProviderGenerationSettings Settings;
		Settings.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		FAngelscriptArtifactCanonicalWriter ProviderWriter(
			TEXT("static-jit-generation-engine-test-provider"));
		ProviderWriter.WriteString(TEXT("BackendGraph"));
		Settings.ProviderId.Hash = ProviderWriter.FinalizeHash();
		Settings.ProviderName = TEXT("Generation Engine Backend Graph");
		Settings.OwnerModuleName = TEXT("AngelscriptTest");
		Settings.ArtifactProfile = Snapshot->ArtifactProfile;
		Settings.NativeEnvironmentFingerprint =
			FAngelscriptJITTargetProfiles::
				BuildCurrentNativeEnvironmentFingerprint(
					Settings.TargetProfile);

		FAngelscriptStaticJITProviderGenerationRequest Request;
		Request.BackendId = FAngelscriptStaticJITBackendId::TypedAST();
		Request.Settings = Settings;
		FAngelscriptStaticJITGenerationProfile RequestProfile;
		FString RequestProfileError;
		ASSERT_THAT(IsTrue(
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				Request.BackendId, Settings.TargetProfile,
				RequestProfile,
				&RequestProfileError), RequestProfileError));
		Request.TypedASTJITCapabilityProfile =
			RequestProfile.TypedASTJITCapabilityProfile;
		Request.TypedASTJITExecutionRequirements =
			RequestProfile.TypedASTJITExecutionRequirements;
		Request.CompiledSourceGraph = CompleteGraph;
		asIScriptModule* EmitModules[] = {EmitModule};
		Request.EmitModuleSet = EmitModules;
		Request.GenerationSnapshot = Snapshot;
		FAngelscriptStaticJITGeneratorOutput Output;
		FString Error;
		ASSERT_THAT(IsTrue(GenerateStaticJITProviderArtifacts(
			Request, Output, &Error), Error));
		ASSERT_THAT(IsTrue(Output.bHasProviderOutput));
		ASSERT_THAT(AreEqual(1, Output.ProviderOutput.Modules.Num()));
		ASSERT_THAT(IsTrue(Output.ProviderOutput.Modules[0].ModuleKey
			== EmitModuleKey));
		ASSERT_THAT(IsTrue(Output.Functions.Num()
			== Snapshot->Functions.Num()));
		const FAngelscriptStaticJITCompiledSourceGraphSummary& GraphSummary =
			Output.Diagnostics.CompiledSourceGraph;
		ASSERT_THAT(AreEqual(1u, GraphSummary.SourceCompilePassCount));
		ASSERT_THAT(AreEqual(
			Snapshot->Modules.Num(), GraphSummary.ModuleCount));
		ASSERT_THAT(AreEqual(
			Snapshot->Functions.Num(), GraphSummary.FunctionCount));
		ASSERT_THAT(AreEqual(
			Snapshot->Types.Num(), GraphSummary.TypeCount));
		ASSERT_THAT(AreEqual(
			Snapshot->Globals.Num(), GraphSummary.GlobalCount));
		ASSERT_THAT(AreEqual(
			Snapshot->Descriptors.Num(), GraphSummary.DescriptorCount));
		ASSERT_THAT(AreEqual(
			Snapshot->Dependencies.Num(), GraphSummary.DependencyCount));
		ASSERT_THAT(AreEqual(
			Snapshot->ExternalNativeCalls.Num(),
			GraphSummary.ExternalNativeCallCount));
		ASSERT_THAT(AreEqual(1, GraphSummary.EmitModuleCount));
		ASSERT_THAT(IsTrue(GraphSummary.bHasVerifiedTypedHIR));

		TArray<asIScriptModule*> NonExactGraph = CompleteGraph;
		NonExactGraph.Add(CompleteGraph[0]);
		Request.CompiledSourceGraph = NonExactGraph;
		Output = {};
		Error.Reset();
		ASSERT_THAT(IsFalse(GenerateStaticJITProviderArtifacts(
			Request, Output, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("exactly match")), Error));
	}

	TEST_METHOD(TypedASTBackendRejectsReuseForeignEngineAndDetachedHIR)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		TUniquePtr<FFixture> FirstFixture = CreateFixture(
			*TestRunner, TEXT("TypedASTTaskOwnershipFirst"),
			FAngelscriptStaticJITBackendId::TypedAST());
		TUniquePtr<FFixture> SecondFixture = CreateFixture(
			*TestRunner, TEXT("TypedASTTaskOwnershipSecond"),
			FAngelscriptStaticJITBackendId::TypedAST());
		ASSERT_THAT(IsNotNull(FirstFixture.Get()));
		ASSERT_THAT(IsNotNull(SecondFixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> FirstSnapshot =
			FirstFixture->Engine->GetStaticJITGenerationSnapshot();
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> SecondSnapshot =
			SecondFixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(FirstSnapshot.IsValid()));
		ASSERT_THAT(IsTrue(SecondSnapshot.IsValid()));

		FCompiledGraphStorage FirstStorage = BuildCompiledGraphStorage(
			*FirstSnapshot, TEXTVIEW("TypedAST Task Ownership First"));
		FCompiledGraphStorage SecondStorage = BuildCompiledGraphStorage(
			*SecondSnapshot, TEXTVIEW("TypedAST Task Ownership Second"));
		const int32 FirstHIRIndex = FirstStorage.Functions.IndexOfByPredicate(
			[](const FAngelscriptStaticJITCompiledFunctionView& Function)
			{
				return Function.VerifiedTypedHIR != nullptr;
			});
		const int32 SecondHIRIndex = SecondStorage.Functions.IndexOfByPredicate(
			[](const FAngelscriptStaticJITCompiledFunctionView& Function)
			{
				return Function.VerifiedTypedHIR != nullptr;
			});
		ASSERT_THAT(IsTrue(FirstHIRIndex != INDEX_NONE));
		ASSERT_THAT(IsTrue(SecondHIRIndex != INDEX_NONE));

		const FAngelscriptStaticJITBackendRequest FirstRequest{
			FAngelscriptStaticJITBackendId::TypedAST(),
			FirstStorage.Graph,
			{}};
		const FAngelscriptStaticJITBackendRequest SecondRequest{
			FAngelscriptStaticJITBackendId::TypedAST(),
			SecondStorage.Graph,
			{}};
		FAngelscriptTypedASTJIT OneShotBackend;
		FAngelscriptStaticJITBackendOutput Output;
		FString Error;
		ASSERT_THAT(IsTrue(
			OneShotBackend.Generate(FirstRequest, Output, &Error), Error));
		Output = {};
		Error.Reset();
		ASSERT_THAT(IsFalse(
			OneShotBackend.Generate(SecondRequest, Output, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(
			TEXT("TypedASTJITTaskInstanceAlreadyConsumed")), Error));

		FCompiledGraphStorage ForeignEngineStorage =
			BuildCompiledGraphStorage(
				*FirstSnapshot, TEXTVIEW("TypedAST Foreign Engine"));
		ForeignEngineStorage.Functions[FirstHIRIndex].EngineLocalFunction =
			SecondStorage.Functions[SecondHIRIndex].EngineLocalFunction;
		ForeignEngineStorage.Functions[FirstHIRIndex].VerifiedTypedHIR =
			SecondStorage.Functions[SecondHIRIndex].VerifiedTypedHIR;
		ForeignEngineStorage.Graph.Functions = ForeignEngineStorage.Functions;
		const FAngelscriptStaticJITBackendRequest ForeignEngineRequest{
			FAngelscriptStaticJITBackendId::TypedAST(),
			ForeignEngineStorage.Graph,
			{}};
		FAngelscriptTypedASTJIT ForeignEngineBackend;
		Output = {};
		Error.Reset();
		ASSERT_THAT(IsFalse(ForeignEngineBackend.Generate(
			ForeignEngineRequest, Output, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(
			TEXT("TypedASTJITEngineMismatch")), Error));

		FCompiledGraphStorage DetachedHIRStorage = BuildCompiledGraphStorage(
			*FirstSnapshot, TEXTVIEW("TypedAST Detached HIR"));
		asCTypedSemanticFunction DetachedHIR;
		DetachedHIRStorage.Functions[FirstHIRIndex].VerifiedTypedHIR =
			&DetachedHIR;
		DetachedHIRStorage.Graph.Functions = DetachedHIRStorage.Functions;
		const FAngelscriptStaticJITBackendRequest DetachedHIRRequest{
			FAngelscriptStaticJITBackendId::TypedAST(),
			DetachedHIRStorage.Graph,
			{}};
		FAngelscriptTypedASTJIT DetachedHIRBackend;
		Output = {};
		Error.Reset();
		ASSERT_THAT(IsFalse(DetachedHIRBackend.Generate(
			DetachedHIRRequest, Output, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(
			TEXT("TypedASTJITSameCompilationHIRRequired")), Error));
	}

	TEST_METHOD(HIRDumpSuccessAndCompileFailurePreserveCompletePrimaryContainmentSnapshot)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptStaticJITGenerationEngineTests;

		const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/StaticJITHIRDumpContainment"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(*Root, true)));

		const FString ProjectFile = FPaths::Combine(
			Root, TEXT("HIRDumpContainment.uproject"));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			TEXT("{\n  \"FileVersion\": 3,\n  \"Modules\": []\n}\n"),
			*ProjectFile,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		const FString ScriptRoot = FPaths::Combine(Root, TEXT("Script"));
		ASSERT_THAT(IsTrue(
			IFileManager::Get().MakeDirectory(*ScriptRoot, true)));
		const FString SourceFile = FPaths::Combine(
			ScriptRoot, TEXT("HIRDumpContainment.as"));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			TEXT("int HIRContainmentHelper(int Value)\n")
			TEXT("{\n")
			TEXT("\treturn Value + 1;\n")
			TEXT("}\n\n")
			TEXT("UFUNCTION()\n")
			TEXT("int HIRContainmentEntry(int Value)\n")
			TEXT("{\n")
			TEXT("\treturn HIRContainmentHelper(Value);\n")
			TEXT("}\n"),
			*SourceFile,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));

		const FString OutputRoot = FPaths::Combine(Root, TEXT("Output"));
		FAngelscriptHIRDumpRequest Request;
		FString ParseError;
		ASSERT_THAT(IsTrue(FAngelscriptHIRDumpCommand::Parse(
			FString::Printf(
				TEXT("Profile=EditorDevelopment Format=Both Output=\"%s\""),
				*OutputRoot),
			ProjectFile, Request, ParseError), ParseError));

		const FPrimaryContainmentSnapshot PrimaryBeforeSuccess =
			CapturePrimaryContainmentSnapshot();
		const FAngelscriptHIRDumpResult Success =
			FAngelscriptHIRDumpCommand::Execute(Request);
		ASSERT_THAT(IsTrue(Success.IsSuccess(), Success.Error));
		ASSERT_THAT(AreEqual(1u, Success.SourceCompilePassCount));
		ASSERT_THAT(IsTrue(Success.CapturedFunctionCount >= 2));
		FString SuccessfulText;
		FString SuccessfulJson;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			SuccessfulText, *Success.TextOutputPath)));
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			SuccessfulJson, *Success.JsonOutputPath)));
		const FPrimaryContainmentSnapshot PrimaryAfterSuccess =
			CapturePrimaryContainmentSnapshot();
		ASSERT_THAT(IsTrue(AssertPrimaryContainmentUnchanged(
			*TestRunner, PrimaryBeforeSuccess, PrimaryAfterSuccess,
			TEXTVIEW("HIR dump success"))));

		TestRunner->AddExpectedErrorPlain(
			TEXT("Expected expression value"),
			EAutomationExpectedErrorFlags::Contains,
			0);
		TestRunner->AddExpectedErrorPlain(
			TEXT("HIRDumpContainment.as:"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Instead found ';'"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("[StartupCompileFailure] Cannot run after startup compile failure"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			TEXT("int InvalidHIRDumpContainment()\n")
			TEXT("{\n")
			TEXT("\treturn 1 + ;\n")
			TEXT("}\n"),
			*SourceFile,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));

		const FPrimaryContainmentSnapshot PrimaryBeforeFailure =
			CapturePrimaryContainmentSnapshot();
		const FAngelscriptHIRDumpResult Failure =
			FAngelscriptHIRDumpCommand::Execute(Request);
		ASSERT_THAT(IsFalse(Failure.IsSuccess()));
		ASSERT_THAT(AreEqual(
			EAngelscriptHIRDumpExitCode::ExecutionFailure,
			Failure.ExitCode));
		ASSERT_THAT(IsTrue(Failure.Error.Contains(
			TEXT("Project scripts failed to compile")), Failure.Error));
		FString TextAfterFailure;
		FString JsonAfterFailure;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			TextAfterFailure, *Success.TextOutputPath)));
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			JsonAfterFailure, *Success.JsonOutputPath)));
		ASSERT_THAT(AreEqual(SuccessfulText, TextAfterFailure,
			TEXT("failed dump must not replace the last successful text snapshot")));
		ASSERT_THAT(AreEqual(SuccessfulJson, JsonAfterFailure,
			TEXT("failed dump must not replace the last successful JSON snapshot")));
		TArray<FString> ScratchFiles;
		IFileManager::Get().FindFilesRecursive(
			ScratchFiles,
			*FPaths::Combine(Root, TEXT("Saved/Angelscript/HIRDump/Scratch")),
			TEXT("*"), true, false, false);
		ASSERT_THAT(AreEqual(0, ScratchFiles.Num(),
			TEXT("HIR dump success/failure must release transient compile state")));
		const FPrimaryContainmentSnapshot PrimaryAfterFailure =
			CapturePrimaryContainmentSnapshot();
		ASSERT_THAT(IsTrue(AssertPrimaryContainmentUnchanged(
			*TestRunner, PrimaryBeforeFailure, PrimaryAfterFailure,
			TEXTVIEW("HIR dump compile failure"))));
	}
};

#endif
