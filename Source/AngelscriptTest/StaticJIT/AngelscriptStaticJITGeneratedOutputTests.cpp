#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "Artifacts/AngelscriptArtifactIdentity.h"
#include "Artifacts/AngelscriptFunctionRoute.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITProviderMatcher.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/StaticJITHeader.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_thread.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITGeneratedOutputTests,
	"Angelscript.TestModule.StaticJIT.GeneratedOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static constexpr TCHAR SourceFilename[] = TEXT(
		"/Angelscript/Memory/StaticJITGeneratedOutput/DebugMetadata.as");
	inline static const FName ModuleName = FName(TEXT("ASStaticJITGeneratedOutputDebugMetadata"));
	inline static int32 NativeIncrementMetadata = 0;

	static FString MakeScriptSource()
	{
		return
			TEXT("int AddOne(int Value)\n")
			TEXT("{\n")
			TEXT("    return Value + 1;\n")
			TEXT("}\n")
			TEXT("\n")
			TEXT("int AddTwo(int Value)\n")
			TEXT("{\n")
			TEXT("    return Value + 2;\n")
			TEXT("}\n")
			TEXT("\n")
			TEXT("int Entry()\n")
			TEXT("{\n")
			TEXT("    int First = AddOne(1);\n")
			TEXT("    int Second = AddTwo(First);\n")
			TEXT("    return Second;\n")
			TEXT("}\n");
	}

	class FBoundRecordingJitCompiler final : public asIJITCompiler
	{
	public:
		int32 ReadyCount = 0;
		int32 ReleaseCount = 0;

		void OnFunctionReady(asIScriptFunction* Function) override
		{
			++ReadyCount;
			if (Function != nullptr)
			{
				asSJITFunctionBinding Binding;
				Binding.VMEntry = &SentinelJitFunction;
				Binding.UserData = this;
				Function->SetJITBinding(Binding);
			}
		}

		void ReleaseFunctionBinding(
			asIScriptFunction*,
			const asSJITFunctionBinding&) override
		{
			++ReleaseCount;
		}

		static void SentinelJitFunction(
			FScriptExecution&,
			asDWORD*,
			asQWORD*)
		{
		}
	};

	static void FirstNativeEntry(
		FScriptExecution&,
		asDWORD*,
		asQWORD* OutValue)
	{
		if (OutValue != nullptr)
		{
			*OutValue = 1001;
		}
	}

	static void RefreshedNativeEntry(
		FScriptExecution&,
		asDWORD*,
		asQWORD* OutValue)
	{
		if (OutValue != nullptr)
		{
			*OutValue = 2002;
		}
	}

	static int32 NativeIncrement(const int32 Value)
	{
		return Value + 1;
	}

	static FAngelscriptJITArtifactSelection MakeSelection(
		asJITFunction Entry)
	{
		TSharedPtr<FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> Catalog =
			MakeShared<FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>();
		Catalog->Entries.SetNum(1);
		Catalog->Entries[0].EntryPoints.VMEntry = Entry;
		FAngelscriptJITArtifactSelection Selection;
		Selection.Provider = Catalog;
		Selection.EntryIndex = 0;
		Selection.ReferenceTable =
			MakeShared<FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>();
		return Selection;
	}

	static int32 ExecuteCurrentIntNoArgs(
		FScriptExecution& Execution,
		const FAngelscriptStableFunctionKey& FunctionKey,
		asIScriptFunction* ReferencedFunction,
		asDWORD& OutValue)
	{
		FStaticJITCurrentFunctionCall CurrentCall(
			Execution,
			FunctionKey,
			static_cast<asCScriptFunction*>(ReferencedFunction),
			nullptr,
			/*bResolveVirtualFunction=*/false);
		asCScriptFunction* const CurrentFunction = CurrentCall.Get();
		if (CurrentFunction == nullptr || Execution.jitEngine == nullptr)
		{
			return asERROR;
		}

		asIScriptContext* const Context =
			Execution.jitEngine->CreateContext();
		if (Context == nullptr)
		{
			return asOUT_OF_MEMORY;
		}
		const int32 PrepareResult = Context->Prepare(CurrentFunction);
		if (PrepareResult < 0)
		{
			Context->Release();
			return PrepareResult;
		}
		const int32 ExecuteResult = Context->Execute();
		if (ExecuteResult == asEXECUTION_FINISHED)
		{
			OutValue = Context->GetReturnDWord();
		}
		Context->Release();
		return ExecuteResult;
	}

	static int32 FindScriptLineNumberContaining(const FString& ScriptSource, const FString& Needle)
	{
		TArray<FString> Lines;
		ScriptSource.ParseIntoArrayLines(Lines, false);
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].Contains(Needle))
			{
				return Index + 1;
			}
		}

		return INDEX_NONE;
	}

	static TArray<int32> ExtractGeneratedLineMarkers(const FString& GeneratedSource)
	{
		TArray<int32> LineMarkers;
		const FString Needle = TEXT("SCRIPT_DEBUG_CALLSTACK_LINE(");
		int32 SearchFrom = 0;
		while (SearchFrom < GeneratedSource.Len())
		{
			int32 MarkerStart = GeneratedSource.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
			if (MarkerStart == INDEX_NONE)
			{
				break;
			}

			const int32 ValueStart = MarkerStart + Needle.Len();
			int32 ValueEnd = GeneratedSource.Find(TEXT(");"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
			if (ValueEnd == INDEX_NONE)
			{
				break;
			}

			const FString ValueText = GeneratedSource.Mid(ValueStart, ValueEnd - ValueStart);
			LineMarkers.Add(FCString::Atoi(*ValueText));
			SearchFrom = ValueEnd + 2;
		}

		return LineMarkers;
	}

	static FString JoinLineMarkers(const TArray<int32>& LineMarkers)
	{
		TArray<FString> MarkerStrings;
		MarkerStrings.Reserve(LineMarkers.Num());
		for (int32 Marker : LineMarkers)
		{
			MarkerStrings.Add(LexToString(Marker));
		}

		return FString::Join(MarkerStrings, TEXT(", "));
	}

	static bool RunGeneratedOutputDebugMetadataHooks(FAutomationTestBase& Test);
	static bool RunReloadableCallsUseRuntimeRouteBridge(FAutomationTestBase& Test);

public:
	TEST_METHOD(DebugMetadataHooks)
	{
		ASSERT_THAT(IsTrue(RunGeneratedOutputDebugMetadataHooks(*TestRunner)));
	}

	TEST_METHOD(ReloadableCallsUseRuntimeRouteBridge)
	{
		ASSERT_THAT(IsTrue(RunReloadableCallsUseRuntimeRouteBridge(*TestRunner)));
	}

	TEST_METHOD(CurrentFunctionBridgeUsesCurrentStableRouteAndExactEngine)
	{
		const FString VirtualPath = TEXT(
			"/Angelscript/Memory/StaticJITCurrentCall/CurrentRoute.as");
		const FString CurrentModuleName = TEXT(
			"Angelscript.Memory.StaticJITCurrentCall.CurrentRoute");
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ASSERT_THAT(IsTrue(CompileMemorySource(
			&Engine,
			VirtualPath,
			TEXT("int Target() { return 1; }\nint Other() { return 2; }\n"))));
		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule(
			TCHAR_TO_ANSI(*CurrentModuleName),
			asGM_ONLY_IF_EXISTS);
		ASSERT_THAT(IsNotNull(Module));
		asIScriptFunction* FirstTarget = Module != nullptr
			? Module->GetFunctionByDecl("int Target()")
			: nullptr;
		ASSERT_THAT(IsNotNull(FirstTarget));
		FAngelscriptStableFunctionKey TargetKey;
		ASSERT_THAT(IsTrue(
			Engine.TryResolveStableFunctionKey(FirstTarget, TargetKey)));

		FAngelscriptFunctionRoute FirstRoute;
		ASSERT_THAT(IsTrue(Engine.ResolveFunctionRoute(TargetKey, FirstRoute),
			TEXT("Stable source compilation should publish a current Engine route")));
		ASSERT_THAT(IsTrue(FirstRoute.Function == FirstTarget));

		ASSERT_THAT(IsTrue(CompileMemorySource(
			&Engine,
			VirtualPath,
			TEXT("int Target() { return 101; }\nint Other() { return 202; }\n"))));
		Module = Engine.GetScriptEngine()->GetModule(
			TCHAR_TO_ANSI(*CurrentModuleName),
			asGM_ONLY_IF_EXISTS);
		asIScriptFunction* CurrentTarget = Module != nullptr
			? Module->GetFunctionByDecl("int Target()")
			: nullptr;
		asIScriptFunction* CurrentOther = Module != nullptr
			? Module->GetFunctionByDecl("int Other()")
			: nullptr;
		ASSERT_THAT(IsNotNull(CurrentTarget));
		ASSERT_THAT(IsNotNull(CurrentOther));

		FAngelscriptFunctionRoute CurrentRoute;
		ASSERT_THAT(IsTrue(Engine.ResolveFunctionRoute(TargetKey, CurrentRoute)));
		ASSERT_THAT(IsTrue(CurrentRoute.Function == CurrentTarget));

		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Execution.jitEngine = Engine.GetScriptEngine();
		{
			FStaticJITCurrentFunctionCall CurrentCall(
				Execution,
				TargetKey,
				static_cast<asCScriptFunction*>(CurrentOther),
				nullptr,
				/*bResolveVirtualFunction=*/false);
			ASSERT_THAT(IsTrue(CurrentCall.Get() == CurrentTarget,
				TEXT("The stable route should win over a different live fallback reference")));
		}

		Execution.jitEngine = nullptr;
		FStaticJITCurrentFunctionCall WrongEngineCall(
			Execution,
			TargetKey,
			static_cast<asCScriptFunction*>(CurrentTarget),
			nullptr,
			/*bResolveVirtualFunction=*/false);
		ASSERT_THAT(IsNull(WrongEngineCall.Get(),
			TEXT("A function must never be accepted without the exact execution Engine")));
		}
	}

	TEST_METHOD(CurrentRouteExecutesNativeMissAndRefreshedNative)
	{
		const FString VirtualPath = TEXT(
			"/Angelscript/Memory/StaticJITCurrentExecution/Entry.as");
		const FString CurrentModuleName = TEXT(
			"Angelscript.Memory.StaticJITCurrentExecution.Entry");
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptEngine* const ScriptEngine = Engine.GetScriptEngine();
		FAngelscriptStaticJIT Compiler;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler)));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};

		ASSERT_THAT(IsTrue(CompileMemorySource(
			&Engine,
			VirtualPath,
			TEXT("int Entry() { return 1; }\n"))));
		asIScriptModule* Module = ScriptEngine->GetModule(
			TCHAR_TO_ANSI(*CurrentModuleName),
			asGM_ONLY_IF_EXISTS);
		asIScriptFunction* FirstFunction = Module != nullptr
			? Module->GetFunctionByDecl("int Entry()")
			: nullptr;
		ASSERT_THAT(IsNotNull(FirstFunction));
		FAngelscriptStableFunctionKey FunctionKey;
		ASSERT_THAT(IsTrue(
			Engine.TryResolveStableFunctionKey(FirstFunction, FunctionKey)));
		ASSERT_THAT(IsTrue(Compiler.PublishProviderSelection(
			FirstFunction,
			MakeSelection(&FirstNativeEntry))));

		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Execution.jitEngine = ScriptEngine;
		asDWORD Result = 0;
		ASSERT_THAT(AreEqual(
			asEXECUTION_FINISHED,
			ExecuteCurrentIntNoArgs(
				Execution, FunctionKey, FirstFunction, Result)));
		ASSERT_THAT(AreEqual(1001u, Result,
			TEXT("The exact first Provider Binding should execute Native")));

		ASSERT_THAT(IsTrue(CompileMemorySource(
			&Engine,
			VirtualPath,
			TEXT("int Entry() { return 2; }\n"))));
		Module = ScriptEngine->GetModule(
			TCHAR_TO_ANSI(*CurrentModuleName),
			asGM_ONLY_IF_EXISTS);
		asIScriptFunction* CurrentFunction = Module != nullptr
			? Module->GetFunctionByDecl("int Entry()")
			: nullptr;
		ASSERT_THAT(IsNotNull(CurrentFunction));
		Result = 0;
		ASSERT_THAT(AreEqual(
			asEXECUTION_FINISHED,
			ExecuteCurrentIntNoArgs(
				Execution, FunctionKey, CurrentFunction, Result)));
		ASSERT_THAT(AreEqual(2u, Result,
			TEXT("A changed body should execute the current VM function before refresh")));

		ASSERT_THAT(IsTrue(Compiler.PublishProviderSelection(
			CurrentFunction,
			MakeSelection(&RefreshedNativeEntry))));
		Result = 0;
		ASSERT_THAT(AreEqual(
			asEXECUTION_FINISHED,
			ExecuteCurrentIntNoArgs(
				Execution, FunctionKey, CurrentFunction, Result)));
		ASSERT_THAT(AreEqual(2002u, Result,
			TEXT("Publishing the refreshed exact Binding should restore Native")));
		}
	}

	TEST_METHOD(GenerationPreservesInstalledExecutionCompiler)
	{
		FBoundRecordingJitCompiler Compiler;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptEngine* const ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("StaticJIT generation observer test should expose the script engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler),
			TEXT("StaticJIT generation observer test should install the execution compiler")));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};

		ASSERT_THAT(IsTrue(
			CompileModuleFromMemory(
				&Engine,
				ModuleName,
				SourceFilename,
				MakeScriptSource()),
			TEXT("StaticJIT generation observer test should compile the fixture module")));

		asIScriptModule* const Module = ScriptEngine->GetModule(
			TCHAR_TO_ANSI(*ModuleName.ToString()),
			asGM_ONLY_IF_EXISTS);
		ASSERT_THAT(IsNotNull(Module, TEXT("StaticJIT generation observer test should retain the fixture module")));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* const EntryFunction = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("StaticJIT generation observer test should resolve Entry")));
		if (EntryFunction == nullptr)
		{
			return;
		}
		asSJITFunctionBinding ExecutionBinding;
		ExecutionBinding.VMEntry = &FBoundRecordingJitCompiler::SentinelJitFunction;
		ExecutionBinding.UserData = &Compiler;
		EntryFunction->SetJITBinding(ExecutionBinding);

		const int32 ReadyCountBeforeGeneration = Compiler.ReadyCount;
		const int32 ReleaseCountBeforeGeneration = Compiler.ReleaseCount;
		const asSJITFunctionBinding BindingBeforeGeneration = EntryFunction->GetJITBinding();

		FString GeneratedSource;
		FString GenerationError;
		ASSERT_THAT(IsTrue(
			GenerateStaticJITSourceText(
				&Engine,
				ModuleName,
				GeneratedSource,
				/*bEmitDebugMetadata=*/true,
				&GenerationError),
			GenerationError.IsEmpty()
				? TEXT("StaticJIT generation observer test should generate files")
				: GenerationError));

		ASSERT_THAT(AreEqual(
			static_cast<asIJITCompiler*>(&Compiler),
			ScriptEngine->GetJITCompiler(),
			TEXT("StaticJIT generation should preserve the installed execution compiler")));
		ASSERT_THAT(AreEqual(
			ReadyCountBeforeGeneration,
			Compiler.ReadyCount,
			TEXT("StaticJIT generation should not replay functions into the execution compiler")));
		ASSERT_THAT(AreEqual(
			ReleaseCountBeforeGeneration,
			Compiler.ReleaseCount,
			TEXT("StaticJIT generation should not retire execution bindings")));

		const asSJITFunctionBinding BindingAfterGeneration = EntryFunction->GetJITBinding();
		ASSERT_THAT(AreEqual(
			BindingBeforeGeneration.VMEntry,
			BindingAfterGeneration.VMEntry,
			TEXT("StaticJIT generation should preserve the execution VM entry")));
		ASSERT_THAT(AreEqual(
			BindingBeforeGeneration.UserData,
			BindingAfterGeneration.UserData,
			TEXT("StaticJIT generation should preserve execution binding user data")));
		}
	}

	TEST_METHOD(CompiledFunctionsEmitProviderSlicesAndExactlyThirtyTwoBuckets)
	{
		const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/StaticJITProviderGeneration"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		const FString ScriptRoot = Root / TEXT("Script");
		const FString CacheRoot = Root / TEXT("CacheV2");
		ASSERT_THAT(IsTrue(Root.Contains(
			TEXT("/Saved/Automation/StaticJITProviderGeneration/"))));
		ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(*ScriptRoot, true)));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			TEXT("int Helper() { return 11; }\n")
			TEXT("int Alpha() { return NativeIncrement(Helper()); }\n")
			TEXT("int Beta() { return 22; }\n"),
			*(ScriptRoot / TEXT("Provider.as")))));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			TEXT("int Gamma() { return 33; }\n"),
			*(ScriptRoot / TEXT("SecondProviderModule.as")))));

		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.CacheV2RootOverride = CacheRoot;
		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [Root]() { return Root; };
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		TUniquePtr<FAngelscriptEngine> Engine =
			CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		const int32 NativeIncrementFunctionId = Engine.IsValid()
			? Engine->GetScriptEngine()->RegisterGlobalFunction(
				"int NativeIncrement(int Value)",
				asFUNCTION(NativeIncrement),
				asCALL_CDECL)
			: asERROR;
		ASSERT_THAT(IsTrue(NativeIncrementFunctionId >= 0,
			TEXT("The fixture native global should register before compilation")));
		asCScriptFunction* const NativeIncrementFunction =
			NativeIncrementFunctionId >= 0
			? static_cast<asCScriptFunction*>(
				Engine->GetScriptEngine()->GetFunctionById(
					NativeIncrementFunctionId))
			: nullptr;
		ASSERT_THAT(IsNotNull(NativeIncrementFunction));
		if (NativeIncrementFunction != nullptr)
		{
			// Match application bindings that carry reflection/call metadata and
			// therefore require FScopeInformSystemFunction in generated code.
			NativeIncrementFunction->userData = &NativeIncrementMetadata;
		}
		ON_SCOPE_EXIT
		{
			Engine.Reset();
			if (Root.Contains(
				TEXT("/Saved/Automation/StaticJITProviderGeneration/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		};
		FAngelscriptEngineScope _AutoEngineScope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc =
			Engine->GetModuleByModuleName(TEXT("Provider"));
		asIScriptModule* const Module = ModuleDesc.IsValid()
			? ModuleDesc->ScriptModule : nullptr;
		ASSERT_THAT(IsNotNull(Module));
		const TSharedPtr<FAngelscriptModuleDesc> SecondModuleDesc =
			Engine->GetModuleByModuleName(TEXT("SecondProviderModule"));
		asIScriptModule* const SecondModule = SecondModuleDesc.IsValid()
			? SecondModuleDesc->ScriptModule : nullptr;
		ASSERT_THAT(IsNotNull(SecondModule));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Snapshot = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		const FAngelscriptFunctionRoute* AlphaRoute = Snapshot.IsValid()
			? Snapshot->FunctionRoutes.FindByPredicate(
				[Module](const FAngelscriptFunctionRoute& Route)
				{
					return Route.Function != nullptr
						&& Route.Function->GetModule() == Module
						&& FString(ANSI_TO_TCHAR(Route.Function->GetName()))
							== TEXT("Alpha");
				})
			: nullptr;
		ASSERT_THAT(IsNotNull(AlphaRoute));

		FAngelscriptJITProviderGenerationSettings Settings;
		Settings.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		FAngelscriptArtifactCanonicalWriter ProviderWriter(
			TEXT("static-jit-provider-generation-test"));
		ProviderWriter.WriteString(TEXT("ProviderId"));
		Settings.ProviderId.Hash = ProviderWriter.FinalizeHash();
		Settings.ProviderName = TEXT("GeneratedOutputProvider");
		Settings.OwnerModuleName = TEXT("GeneratedOutputProviderModule");
		Settings.ArtifactProfile = AlphaRoute->Identity.Profile;
		FAngelscriptArtifactCanonicalWriter EnvironmentWriter(
			TEXT("static-jit-provider-generation-test"));
		EnvironmentWriter.WriteString(TEXT("Environment"));
		Settings.NativeEnvironmentFingerprint =
			EnvironmentWriter.FinalizeHash();

		FAngelscriptJITGenerationOutput Output;
		FString Error;
		asIScriptModule* ProviderModules[] = {Module, SecondModule};
		ASSERT_THAT(IsTrue(GenerateStaticJITProviderArtifacts(
			MakeArrayView(ProviderModules), Settings, Output, &Error), Error));
		ASSERT_THAT(AreEqual(4, Output.Functions.Num()));
		ASSERT_THAT(AreEqual(2, Output.Modules.Num()));
		ASSERT_THAT(AreEqual(6, Output.Files.Num(),
			TEXT("Two AS-module sources plus four provider metadata files")));
		TArray<FAngelscriptStableModuleKey> GeneratedModuleKeys;
		for (const FAngelscriptJITGeneratedFunction& Function : Output.Functions)
		{
			GeneratedModuleKeys.AddUnique(Function.ModuleKey);
		}
		ASSERT_THAT(AreEqual(2, GeneratedModuleKeys.Num(),
			TEXT("One Provider generation should retain both AS module identities")));

		int32 ModuleSourceCount = 0;
		for (const FAngelscriptJITGeneratedFile& File : Output.Files)
		{
			if (File.Kind
				== EAngelscriptJITGeneratedFileKind::ModuleSource)
			{
				++ModuleSourceCount;
				ASSERT_THAT(IsFalse(File.RelativePath.StartsWith(TEXT("Modules/"))));
				ASSERT_THAT(IsTrue(File.RelativePath.EndsWith(TEXT(".jit.cpp"))));
				ASSERT_THAT(IsTrue(File.Content.Contains(
					TEXT("AngelScript Static JIT Module"))));
				ASSERT_THAT(IsTrue(File.Content.Contains(
					TEXT("// AS Function : "))));
				ASSERT_THAT(IsTrue(File.Content.Contains(
					TEXT("// JIT Entry   : Raw"))));
				ASSERT_THAT(IsTrue(File.Content.Contains(
					TEXT("#if WITH_EDITOR && UE_BUILD_DEVELOPMENT")),
					TEXT("EditorDevelopment output must carry its complete target guard")));
				ASSERT_THAT(IsFalse(File.Content.Contains(
					TEXT("\n#if UE_BUILD_DEVELOPMENT\n#ifndef AS_SKIP_JITTED_CODE")),
					TEXT("Provider output must not inherit the commandlet process configuration as a second guard")));
				ASSERT_THAT(IsFalse(File.Content.Contains(TEXT("FStaticJITFunction")),
					TEXT("Provider module sources must not retain numeric FunctionId registration")));
			}
		}
		ASSERT_THAT(AreEqual(2, ModuleSourceCount));

		const FAngelscriptJITGeneratedFile* ProviderSource =
			Output.Files.FindByPredicate([](const FAngelscriptJITGeneratedFile& File)
			{
				return File.RelativePath == TEXT("Provider.generated.cpp");
			});
		ASSERT_THAT(IsNotNull(ProviderSource));
		ASSERT_THAT(IsTrue(ProviderSource->Content.Contains(
			TEXT("FAngelscriptJITArtifactEntry GEntries["))));
		ASSERT_THAT(IsTrue(ProviderSource->Content.Contains(
			TEXT("FAngelscriptJITProviderView GetGeneratedAngelscriptJITProviderView()")),
			TEXT("The generated accessor must rebuild the current view by value after a Live Coding patch")));
		ASSERT_THAT(IsFalse(ProviderSource->Content.Contains(
			TEXT("static const FAngelscriptJITProviderView View")),
			TEXT("An initialized local-static view survives Live Coding and exposes the previous ProviderGeneration")));
		ASSERT_THAT(IsFalse(ProviderSource->Content.Contains(
			TEXT("static const FAngelscriptJITReferenceSlot GReferences_")),
			TEXT("Initialized reference arrays survive Live Coding and can mix old slots with a new generation")));
		ASSERT_THAT(IsFalse(ProviderSource->Content.Contains(
			TEXT("static const FAngelscriptJITArtifactEntry GEntries[]")),
			TEXT("An initialized entry array survives Live Coding and fails the new manifest digest")));
		ASSERT_THAT(IsTrue(ProviderSource->Content.Contains(
			TEXT("static thread_local FAngelscriptJITArtifactEntry GEntries[")),
			TEXT("The generated accessor must own per-thread entry scratch storage")));
		ASSERT_THAT(IsTrue(ProviderSource->Content.Contains(
			TEXT("GEntries[0] =")),
			TEXT("Every Provider observation must repopulate the currently patched entry metadata")));
		const FAngelscriptJITGeneratedFunction* GeneratedAlpha =
			Output.Functions.FindByPredicate(
				[AlphaRoute](const FAngelscriptJITGeneratedFunction& Function)
				{
					return Function.FunctionKey
						== AlphaRoute->Identity.FunctionKey;
				});
		ASSERT_THAT(IsNotNull(GeneratedAlpha));
		ASSERT_THAT(IsTrue(!GeneratedAlpha->ReferenceSlots.IsEmpty(),
			TEXT("A generated caller must publish its stable Provider reference slots")));
		ASSERT_THAT(IsTrue(GeneratedAlpha->ReferenceSlots.ContainsByPredicate(
			[](const FAngelscriptJITReferenceSlot& Slot)
			{
				return Slot.Reference.Kind
					== EAngelscriptArtifactReferenceKind::ScriptFunction;
			}), TEXT("Alpha should describe its Helper call as a stable ScriptFunction slot")));
		const FAngelscriptJITReferenceSlot* const NativeIncrementSlot =
			GeneratedAlpha->ReferenceSlots.FindByPredicate(
				[](const FAngelscriptJITReferenceSlot& Slot)
				{
					return Slot.Reference.Kind
						== EAngelscriptArtifactReferenceKind::EnvironmentSymbol;
				});
		ASSERT_THAT(IsNotNull(NativeIncrementSlot,
			TEXT("Alpha should describe its native global call as an EnvironmentSymbol slot")));
		const FAngelscriptJITGeneratedFile* const AlphaModuleSource =
			Output.Files.FindByPredicate(
				[GeneratedAlpha](const FAngelscriptJITGeneratedFile& File)
				{
					return File.RelativePath
						== GeneratedAlpha->ModuleSourceRelativePath;
				});
		ASSERT_THAT(IsNotNull(AlphaModuleSource));
		if (NativeIncrementSlot != nullptr && AlphaModuleSource != nullptr)
		{
			const FString FunctionObjectAccess = FString::Printf(
				TEXT("GetScriptFunction(Execution, %uu)"),
				NativeIncrementSlot->SlotIndex);
			const FString NativePointerAccess = FString::Printf(
				TEXT("GetSystemFunctionPointer(Execution, %uu)"),
				NativeIncrementSlot->SlotIndex);
			ASSERT_THAT(IsTrue(AlphaModuleSource->Content.Contains(
				FunctionObjectAccess),
				TEXT("CALLSYS should resolve the application function object from the EnvironmentSymbol slot")));
			ASSERT_THAT(IsTrue(AlphaModuleSource->Content.Contains(
				NativePointerAccess),
				TEXT("CALLSYS should resolve the native entry from that same EnvironmentSymbol slot")));
		}
		ASSERT_THAT(IsTrue(ProviderSource->Content.Contains(
			TEXT("FAngelscriptJITReferenceSlot GReferences_"))));
		for (const FAngelscriptJITGeneratedFunction& Function : Output.Functions)
		{
			ASSERT_THAT(IsTrue(Function.ModuleSourceRelativePath.Contains(
				Function.ModuleKey.Hash.ToHexString())));
			ASSERT_THAT(IsFalse(Function.ModuleSourceRelativePath.Contains(
				Function.FunctionKey.Hash.ToHexString())));
			ASSERT_THAT(IsTrue(ProviderSource->Content.Contains(
				Function.SymbolPrefix)));
		}
	}
};

bool FAngelscriptStaticJITGeneratedOutputTests::RunReloadableCallsUseRuntimeRouteBridge(FAutomationTestBase& Test)
{
	const FString VirtualPath = TEXT(
		"/Angelscript/Memory/StaticJITGeneratedOutput/ReloadableCalls.as");
	const FName RouteModuleName = FName(TEXT(
		"Angelscript.Memory.StaticJITGeneratedOutput.ReloadableCalls"));
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	do
	{
		if (!Test.TestTrue(
			TEXT("StaticJIT.GeneratedOutput.ReloadableCallsUseRuntimeRouteBridge should compile the fixture module"),
			CompileMemorySource(&Engine, VirtualPath, MakeScriptSource())))
		{
			break;
		}

		asIScriptEngine* const ScriptEngine = Engine.GetScriptEngine();
		if (!Test.TestEqual(
			TEXT("Generated route lookup should recover the exact Runtime Engine owner"),
			FAngelscriptEngine::TryGetByScriptEngine(ScriptEngine),
			&Engine))
		{
			break;
		}
		asIScriptModule* const Module = ScriptEngine->GetModule(
			TCHAR_TO_ANSI(*RouteModuleName.ToString()),
			asGM_ONLY_IF_EXISTS);
		asIScriptFunction* const AddOneFunction = Module != nullptr
			? Module->GetFunctionByDecl("int AddOne(int)")
			: nullptr;
		FAngelscriptStableFunctionKey AddOneKey;
		if (!Test.TestNotNull(
			TEXT("Generated route lookup should resolve AddOne"),
			AddOneFunction)
			|| !Test.TestTrue(
				TEXT("Generated route lookup should build AddOne's stable key without a Cache route snapshot"),
				Engine.TryResolveStableFunctionKey(AddOneFunction, AddOneKey)))
		{
			break;
		}

		FString GeneratedSource;
		FString GenerationError;
		if (!Test.TestTrue(
			TEXT("StaticJIT.GeneratedOutput.ReloadableCallsUseRuntimeRouteBridge should generate source text"),
			GenerateStaticJITSourceText(
				&Engine,
				RouteModuleName,
				GeneratedSource,
				/*bEmitDebugMetadata=*/false,
				&GenerationError)))
		{
			if (!GenerationError.IsEmpty())
			{
				Test.AddError(GenerationError);
			}
			break;
		}

		if (!Test.TestTrue(
			TEXT("Reloadable generated script calls should acquire the current callee through the Runtime route bridge"),
			GeneratedSource.Contains(TEXT("FStaticJITCurrentFunctionCall")))
			|| !Test.TestFalse(
				TEXT("Reloadable generated script calls should never read a naked RawEntry"),
				GeneratedSource.Contains(TEXT("GetJITBinding().RawEntry"))))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

bool FAngelscriptStaticJITGeneratedOutputTests::RunGeneratedOutputDebugMetadataHooks(FAutomationTestBase& Test)
{

	const FString ScriptSource = MakeScriptSource();
	const int32 FirstCallLine = FindScriptLineNumberContaining(ScriptSource, TEXT("AddOne(1);"));
	const int32 SecondCallLine = FindScriptLineNumberContaining(ScriptSource, TEXT("AddTwo(First);"));
	if (!Test.TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should locate the first call line marker"), FirstCallLine != INDEX_NONE)
		|| !Test.TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should locate the second call line marker"), SecondCallLine != INDEX_NONE))
	{
		return false;
	}

	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	do
	{
		const bool bCompiled = CompileModuleFromMemory(
			&Engine,
			ModuleName,
			SourceFilename,
			ScriptSource);
		if (!Test.TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should compile the fixture module"), bCompiled))
		{
			break;
		}

		FString SourceWithDebugMetadata;
		FString WithDebugError;
		const bool bGeneratedWithDebugMetadata = GenerateStaticJITSourceText(
			&Engine,
			ModuleName,
			SourceWithDebugMetadata,
			/*bEmitDebugMetadata=*/true,
			&WithDebugError);
		if (!Test.TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should generate source text with debug metadata enabled"), bGeneratedWithDebugMetadata))
		{
			if (!WithDebugError.IsEmpty())
			{
				Test.AddError(WithDebugError);
			}
			break;
		}

		const FString FrameNeedle = TEXT("SCRIPT_DEBUG_CALLSTACK_FRAME(\"int Entry()\"");
		const FString FirstCallLineNeedle = FString::Printf(TEXT("SCRIPT_DEBUG_CALLSTACK_LINE(%d);"), FirstCallLine);
		const FString SecondCallLineNeedle = FString::Printf(TEXT("SCRIPT_DEBUG_CALLSTACK_LINE(%d);"), SecondCallLine);
		const TArray<int32> ObservedLineMarkers = ExtractGeneratedLineMarkers(SourceWithDebugMetadata);

		if (!Test.TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should emit a frame macro when debug metadata is enabled"), SourceWithDebugMetadata.Contains(FrameNeedle))
			|| !Test.TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should emit the first call line marker when debug metadata is enabled"), SourceWithDebugMetadata.Contains(FirstCallLineNeedle))
			|| !Test.TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should emit the second call line marker when debug metadata is enabled"), SourceWithDebugMetadata.Contains(SecondCallLineNeedle)))
		{
			Test.AddInfo(FString::Printf(TEXT("Observed generated line markers: [%s]"), *JoinLineMarkers(ObservedLineMarkers)));
			break;
		}

		FString SourceWithoutDebugMetadata;
		FString WithoutDebugError;
		const bool bGeneratedWithoutDebugMetadata = GenerateStaticJITSourceText(
			&Engine,
			ModuleName,
			SourceWithoutDebugMetadata,
			/*bEmitDebugMetadata=*/false,
			&WithoutDebugError);
		if (!Test.TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should generate source text with debug metadata disabled"), bGeneratedWithoutDebugMetadata))
		{
			if (!WithoutDebugError.IsEmpty())
			{
				Test.AddError(WithoutDebugError);
			}
			break;
		}

		if (!Test.TestFalse(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should omit frame macros when debug metadata is disabled"), SourceWithoutDebugMetadata.Contains(TEXT("SCRIPT_DEBUG_CALLSTACK_FRAME(")))
			|| !Test.TestFalse(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should omit line markers when debug metadata is disabled"), SourceWithoutDebugMetadata.Contains(TEXT("SCRIPT_DEBUG_CALLSTACK_LINE("))))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

#endif
