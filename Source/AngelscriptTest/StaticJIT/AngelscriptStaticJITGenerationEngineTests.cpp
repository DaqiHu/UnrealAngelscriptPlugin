#include "AngelscriptEngine.h"
#include "AngelscriptTestEngineAcquisition.h"
#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"
#include "Compilation/AngelscriptCompilationEvents.h"
#include "StaticJIT/BytecodeJIT/AngelscriptBytecodeJIT.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"
#include "Testing/AngelscriptBindExecutionObservation.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectGlobals.h"

#include "StartAngelscriptHeaders.h"
#include "angelscript.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_thread.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

ANGELSCRIPTRUNTIME_API int32
	GetBlueprintEventsByScriptNameTotalCount();

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
	static const TCHAR* EmitModuleName =
		TEXT("StaticJITGenerationEmit");

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
	return IncrementGenerationEmit_65C28A(Value);
}
)AS");
	}

	static TUniquePtr<FFixture> CreateFixture(
		FAutomationTestBase& Test,
		const FStringView Suffix)
	{
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
		if (!Test.TestTrue(TEXT("Generation fixture should create Script root"),
			IFileManager::Get().MakeDirectory(*ScriptRoot, true))
			|| !Test.TestTrue(TEXT("Generation fixture should write source"),
				FFileHelper::SaveStringToFile(MakeSource(), *ScriptPath))
			|| !Test.TestTrue(TEXT("Generation fixture should write emit source"),
				FFileHelper::SaveStringToFile(
					MakeEmitSource(), *EmitScriptPath)))
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

		if (!Test.TestTrue(TEXT("Generation fixture source should compile"),
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITGenerationEngineTests,
	"Angelscript.TestModule.StaticJIT.ProjectGeneration.Engine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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
		ASSERT_THAT(IsNotNull(Class));
		ASSERT_THAT(IsNotNull(Struct));
		ASSERT_THAT(IsNotNull(Delegate));
		ASSERT_THAT(IsNotNull(Method));
		ASSERT_THAT(IsTrue(Method->bUFunctionRoot));
		ASSERT_THAT(AreEqual(FString(ClassName),
			Method->ReceiverCanonicalName));
		ASSERT_THAT(IsFalse(Method->EntryPlan.EntryPlanHash.IsZero()));
		ASSERT_THAT(IsTrue(Method->EntryPlan.bHasVMEntry));
		ASSERT_THAT(IsTrue(Method->EntryPlan.bHasRawEntry));
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

	TEST_METHOD(GenerationSnapshotDrivesCompleteGraphWithSeparateEmitSet)
	{
		using namespace AngelscriptStaticJITGenerationEngineTests;
		TUniquePtr<FFixture> Fixture = CreateFixture(
			*TestRunner, TEXT("BackendGraph"));
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(2, Snapshot->Modules.Num()));

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
		Request.Settings = Settings;
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
	}
};

#endif
