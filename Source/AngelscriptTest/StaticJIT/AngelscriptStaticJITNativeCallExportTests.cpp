#include "CQTest.h"

#include "AngelscriptEngine.h"
#include "CoreGlobals.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h"

#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#include "as_callfunc.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

namespace AngelscriptStaticJITNativeCallExportTests_Private
{
	struct FFixture
	{
		FString Root;
		TUniquePtr<FAngelscriptEngine> Engine;

		~FFixture()
		{
			Engine.Reset();
			if (Root.Contains(
				TEXT("/Saved/Automation/StaticJITNativeCallExport/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}
	};

	static TUniquePtr<FFixture> CreateFixture(FAutomationTestBase& Test)
	{
		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		FString ProfileError;
		if (!Test.TestTrue(
			TEXT("Native-call export fixture resolves the Bytecode profile"),
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::Bytecode(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				GenerationProfile, &ProfileError)))
		{
			Test.AddError(ProfileError);
			return nullptr;
		}

		TUniquePtr<FFixture> Fixture = MakeUnique<FFixture>();
		Fixture->Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/StaticJITNativeCallExport"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		FPaths::NormalizeDirectoryName(Fixture->Root);
		const FString ScriptRoot = FPaths::Combine(
			Fixture->Root, TEXT("Script/StaticJIT/NativeCallExport"));
		const FString ScriptPath = FPaths::Combine(
			ScriptRoot, TEXT("NativeCallExport.as"));
		if (!Test.TestTrue(TEXT("Native-call export fixture creates Script root"),
				IFileManager::Get().MakeDirectory(*ScriptRoot, true))
			|| !Test.TestTrue(TEXT("Native-call export fixture writes source"),
				FFileHelper::SaveStringToFile(TEXT(R"AS(
int NativeCallExportProbe()
{
	return 42;
}
)AS"), *ScriptPath)))
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
		Fixture->Engine = FAngelscriptEngine::Create(Config, Dependencies);
		if (!Test.TestNotNull(TEXT("Native-call export fixture creates Engine"),
				Fixture->Engine.Get())
			|| !Test.TestTrue(TEXT("Native-call export fixture source compiles"),
				Fixture->Engine->DidInitialCompileSucceed()))
		{
			return nullptr;
		}
		return Fixture;
	}

	static bool IsDirectLinkage(
		const EAngelscriptStaticJITNativeCallLinkage Linkage)
	{
		return Linkage == EAngelscriptStaticJITNativeCallLinkage::ExportedSymbol
			|| Linkage
				== EAngelscriptStaticJITNativeCallLinkage::HeaderInline
			|| Linkage
				== EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeThunk
			|| Linkage
				== EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITNativeCallExportTests,
	"Angelscript.TestModule.StaticJIT.NativeCallExport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CompleteDirectInventoryLinksFromTheConsumerModule)
	{
		using namespace AngelscriptStaticJITNativeCallExportTests_Private;
		TUniquePtr<FFixture> Fixture = CreateFixture(*TestRunner);
		ASSERT_THAT(IsNotNull(Fixture.Get()));
		if (!Fixture.IsValid())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		if (!Snapshot.IsValid())
		{
			return;
		}

		TSet<FString> DirectIdentities;
		int32 ExportedCallableCount = 0;
		int32 HeaderInlineCount = 0;
		bool bSawPrint = false;
		bool bSawDaysInMonth = false;
		bool bSawIsRunningCommandlet = false;
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			Fixture->Engine->GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine));

		for (const FAngelscriptStaticJITNativeCallInventoryEntry& Entry
			: Snapshot->NativeCallInventory)
		{
			if (!Entry.bHasExternalDescriptor
				|| !IsDirectLinkage(Entry.ExternalDescriptor.Linkage))
			{
				continue;
			}

			const FAngelscriptStaticJITNativeCallDescriptor& Descriptor =
				Entry.ExternalDescriptor;
			const FString Identity = FString::Printf(TEXT("%s|%s|%s"),
				*Entry.NamespaceCanonicalName,
				*Entry.ReceiverCanonicalName,
				*Entry.CanonicalDeclaration);
			ASSERT_THAT(IsFalse(DirectIdentities.Contains(Identity),
				*FString::Printf(TEXT("Duplicate direct AS identity: %s"),
					*Identity)));
			DirectIdentities.Add(Identity);

			ASSERT_THAT(IsFalse(Entry.CanonicalDeclaration.IsEmpty()));
			ASSERT_THAT(IsTrue(Entry.bHasStableReference));
			ASSERT_THAT(IsTrue(Entry.bHasExpectedNativeABI));
			ASSERT_THAT(IsFalse(Descriptor.Symbol.IsEmpty()));
			ASSERT_THAT(IsFalse(Descriptor.Include.IsEmpty()));
			ASSERT_THAT(IsFalse(Descriptor.CppCallableSignature.IsEmpty()));
			ASSERT_THAT(IsFalse(Descriptor.OwningModule.IsNone()));
			ASSERT_THAT(IsTrue(EnumHasAnyFlags(Descriptor.RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::DirectCall)));
			ASSERT_THAT(IsTrue(Descriptor.ExceptionPolicy
				!= EAngelscriptStaticJITNativeCallExceptionPolicy::Unspecified));
			ASSERT_THAT(IsTrue(Descriptor.LifetimeFlags
				!= EAngelscriptStaticJITNativeCallLifetimeFlags::None));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(Descriptor.DefaultArgumentCount),
				Entry.DefaultArgumentCount));
			ASSERT_THAT(AreEqual(
				Descriptor.WorldContextPolicy
					== EAngelscriptStaticJITNativeCallWorldContextPolicy::RuntimeCurrentWorld,
				Entry.bUsesWorldContext));

			FString DispatchError;
			ASSERT_THAT(IsTrue(
				ValidateAngelscriptStaticJITInstalledNativeCallDispatch(
					Entry, &DispatchError), DispatchError));
			ASSERT_THAT(IsTrue(Entry.DispatchKind
				!= EAngelscriptStaticJITInstalledDispatchKind::GenericFunction
				&& Entry.DispatchKind
					!= EAngelscriptStaticJITInstalledDispatchKind::GenericMethod));

			FAngelscriptStaticJITNativeCallValidationContext Context;
			Context.ExpectedNativeABI = Entry.ExpectedNativeABI;
			Context.ExpectedScalarABI = Entry.ExpectedScalarABI;
			Context.KnownModules.Add(Descriptor.OwningModule);
			Context.PublicDependencyModules.Add(Descriptor.OwningModule);
			const FAngelscriptStaticJITNativeCallValidation Validation =
				ValidateAngelscriptStaticJITNativeCallDescriptor(
					Descriptor, Context);
			ASSERT_THAT(IsTrue(Validation.bMetadataValid,
				Validation.Detail));
			ASSERT_THAT(IsTrue(Validation.bDirectCallable,
				Validation.Detail));

			asCScriptFunction* const RegisteredFunction =
				static_cast<asCScriptFunction*>(
					ScriptEngine->GetFunctionById(Entry.EngineLocalFunctionId));
			ASSERT_THAT(IsNotNull(RegisteredFunction));
			ASSERT_THAT(IsNotNull(RegisteredFunction != nullptr
				? RegisteredFunction->sysFuncIntf : nullptr));
			if (RegisteredFunction == nullptr
				|| RegisteredFunction->sysFuncIntf == nullptr)
			{
				continue;
			}

			if (Descriptor.Linkage
				== EAngelscriptStaticJITNativeCallLinkage::HeaderInline)
			{
				++HeaderInlineCount;
				ASSERT_THAT(IsTrue(Descriptor.ApiMacro.IsEmpty()));
				if (Descriptor.Symbol == TEXT("IsRunningCommandlet"))
				{
					using FCallable = bool(*)();
					const FCallable ImportedInline = &IsRunningCommandlet;
					ASSERT_THAT(IsNotNull(ImportedInline));
					static_cast<void>(ImportedInline());
					bSawIsRunningCommandlet = true;
				}
				else
				{
					TestRunner->AddError(FString::Printf(TEXT(
						"Unreviewed header-inline inventory symbol: %s"),
						*Descriptor.Symbol));
				}
				continue;
			}

			++ExportedCallableCount;
			ASSERT_THAT(IsFalse(Descriptor.ApiMacro.IsEmpty()));
			if (Descriptor.Symbol
				== TEXT("AngelscriptStaticJITNativeCallables::Print"))
			{
				using FCallable = void(*)(
					const FString&, float, FLinearColor);
				const FCallable ImportedCallable =
					&AngelscriptStaticJITNativeCallables::Print;
				const asSFuncPtr Expected = asFunctionPtr(ImportedCallable);
				ASSERT_THAT(IsTrue(
					RegisteredFunction->sysFuncIntf->func
						== Expected.ptr.f.func));
				AngelscriptStaticJITNativeCallables::
					ResetPrintCallCountForTesting();
				ImportedCallable(
					TEXT("TypedASTJIT native-call export contract probe"),
					0.0f, FLinearColor::Transparent);
				ASSERT_THAT(AreEqual(1,
					AngelscriptStaticJITNativeCallables::
						GetPrintCallCountForTesting()));
				bSawPrint = true;
			}
			else if (Descriptor.Symbol == TEXT("FDateTime::DaysInMonth"))
			{
				using FCallable = int32(*)(int32, int32);
				const FCallable ImportedCallable = &FDateTime::DaysInMonth;
				const asSFuncPtr Expected = asFunctionPtr(ImportedCallable);
				ASSERT_THAT(IsTrue(
					RegisteredFunction->sysFuncIntf->func
						== Expected.ptr.f.func));
				ASSERT_THAT(AreEqual(29, ImportedCallable(2024, 2)));
				bSawDaysInMonth = true;
			}
			else
			{
				TestRunner->AddError(FString::Printf(TEXT(
					"Unreviewed exported inventory symbol: %s"),
					*Descriptor.Symbol));
			}
		}

		ASSERT_THAT(AreEqual(2, ExportedCallableCount));
		ASSERT_THAT(AreEqual(1, HeaderInlineCount));
		ASSERT_THAT(IsTrue(bSawPrint));
		ASSERT_THAT(IsTrue(bSawDaysInMonth));
		ASSERT_THAT(IsTrue(bSawIsRunningCommandlet));
	}

	TEST_METHOD(GenericDispatchCannotAdvertiseDirectDllLinkage)
	{
		FAngelscriptStaticJITNativeCallInventoryEntry Entry;
		Entry.CanonicalDeclaration = TEXT("int InvalidGenericDirect(int Value)");
		Entry.DispatchKind =
			EAngelscriptStaticJITInstalledDispatchKind::GenericFunction;
		Entry.bHasSystemCaller = false;
		Entry.bHasExternalDescriptor = true;
		Entry.ExternalDescriptor.Linkage =
			EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable;
		Entry.ExternalDescriptor.RouteFlags =
			EAngelscriptStaticJITNativeCallRouteFlags::DirectCall;

		FString Error;
		ASSERT_THAT(IsFalse(
			ValidateAngelscriptStaticJITInstalledNativeCallDispatch(
				Entry, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(
			TEXT("DirectDescriptorUsesGenericDispatch"))));
	}

	TEST_METHOD(UnrealRoutedUFunctionsCannotAdvertiseRawDirectDllLinkage)
	{
		struct FRouteCase
		{
			EAngelscriptNativeUFunctionRouteFlags RouteFlags;
			const TCHAR* Label;
		};
		const FRouteCase Cases[] = {
			{EAngelscriptNativeUFunctionRouteFlags::RpcOrNet,
				TEXT("RPC/net")},
			{EAngelscriptNativeUFunctionRouteFlags::Event,
				TEXT("Unreal event")},
			{EAngelscriptNativeUFunctionRouteFlags::BlueprintEvent,
				TEXT("Blueprint event")},
		};

		for (const FRouteCase& Case : Cases)
		{
			FAngelscriptStaticJITNativeCallInventoryEntry Entry;
			Entry.CanonicalDeclaration = FString::Printf(
				TEXT("void Invalid%sDirect()"), Case.Label);
			Entry.DispatchKind =
				EAngelscriptStaticJITInstalledDispatchKind::FunctionCaller;
			Entry.bHasSystemCaller = true;
			Entry.bHasNativeForm = true;
			Entry.NativeFormKind = EAngelscriptNativeFormKind::UFunction;
			Entry.NativeUFunctionRouteFlags = Case.RouteFlags;
			Entry.bHasExternalDescriptor = true;
			Entry.ExternalDescriptor.Linkage =
				EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable;
			Entry.ExternalDescriptor.RouteFlags =
				EAngelscriptStaticJITNativeCallRouteFlags::DirectCall;

			FString Error;
			ASSERT_THAT(IsFalse(
				ValidateAngelscriptStaticJITInstalledNativeCallDispatch(
					Entry, &Error), Case.Label));
			ASSERT_THAT(IsTrue(Error.Contains(
				TEXT("DirectDescriptorBypassesUnrealRoute")), Error));
		}
	}

	TEST_METHOD(OrdinaryUFunctionMayUseAReviewedRawDirectDescriptor)
	{
		FAngelscriptStaticJITNativeCallInventoryEntry Entry;
		Entry.CanonicalDeclaration = TEXT("int SafeNativeUFunction(int Value)");
		Entry.DispatchKind =
			EAngelscriptStaticJITInstalledDispatchKind::FunctionCaller;
		Entry.bHasSystemCaller = true;
		Entry.bHasNativeForm = true;
		Entry.NativeFormKind = EAngelscriptNativeFormKind::UFunction;
		Entry.NativeUFunctionRouteFlags =
			EAngelscriptNativeUFunctionRouteFlags::None;
		Entry.bHasExternalDescriptor = true;
		Entry.ExternalDescriptor.Linkage =
			EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable;
		Entry.ExternalDescriptor.RouteFlags =
			EAngelscriptStaticJITNativeCallRouteFlags::DirectCall;

		FString Error;
		ASSERT_THAT(IsTrue(
			ValidateAngelscriptStaticJITInstalledNativeCallDispatch(
				Entry, &Error), Error));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT
