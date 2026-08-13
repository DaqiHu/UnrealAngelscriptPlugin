#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptUASFunctionCurrentBindingReloadTests_Private
{
	struct FTwoIntParams
	{
		int32 Left = 0;
		int32 Right = 0;
		int32 ReturnValue = 0;
	};

	struct FIntReturnParams
	{
		int32 ReturnValue = 0;
	};

	static int32 InitialChangedVMCalls = 0;
	static int32 InitialChangedRawCalls = 0;
	static int32 InitialChangedParmsCalls = 0;
	static int32 InitialUnchangedVMCalls = 0;
	static int32 InitialUnchangedRawCalls = 0;
	static int32 InitialUnchangedParmsCalls = 0;
	static int32 ReloadedUnchangedVMCalls = 0;
	static int32 ReloadedUnchangedRawCalls = 0;
	static int32 ReloadedUnchangedParmsCalls = 0;

	static void ResetCounters()
	{
		InitialChangedVMCalls = 0;
		InitialChangedRawCalls = 0;
		InitialChangedParmsCalls = 0;
		InitialUnchangedVMCalls = 0;
		InitialUnchangedRawCalls = 0;
		InitialUnchangedParmsCalls = 0;
		ReloadedUnchangedVMCalls = 0;
		ReloadedUnchangedRawCalls = 0;
		ReloadedUnchangedParmsCalls = 0;
	}

	static void InitialChangedVMEntry(
		FScriptExecution&,
		asDWORD*,
		asQWORD* OutValue)
	{
		++InitialChangedVMCalls;
		if (OutValue != nullptr)
		{
			*OutValue = 103;
		}
	}

	static asDWORD InitialChangedRawEntry(
		FScriptExecution&,
		void*,
		asDWORD,
		asDWORD)
	{
		++InitialChangedRawCalls;
		return 103;
	}

	static void InitialChangedParmsEntry(
		FScriptExecution&,
		void*,
		void* Parms)
	{
		++InitialChangedParmsCalls;
		static_cast<FTwoIntParams*>(Parms)->ReturnValue = 103;
	}

	static void InitialUnchangedVMEntry(
		FScriptExecution&,
		asDWORD*,
		asQWORD* OutValue)
	{
		++InitialUnchangedVMCalls;
		if (OutValue != nullptr)
		{
			*OutValue = 22;
		}
	}

	static asDWORD InitialUnchangedRawEntry(
		FScriptExecution&,
		void*)
	{
		++InitialUnchangedRawCalls;
		return 22;
	}

	static void InitialUnchangedParmsEntry(
		FScriptExecution&,
		void*,
		void* Parms)
	{
		++InitialUnchangedParmsCalls;
		static_cast<FIntReturnParams*>(Parms)->ReturnValue = 22;
	}

	static void ReloadedUnchangedVMEntry(
		FScriptExecution&,
		asDWORD*,
		asQWORD* OutValue)
	{
		++ReloadedUnchangedVMCalls;
		if (OutValue != nullptr)
		{
			*OutValue = 22;
		}
	}

	static asDWORD ReloadedUnchangedRawEntry(
		FScriptExecution&,
		void*)
	{
		++ReloadedUnchangedRawCalls;
		return 22;
	}

	static void ReloadedUnchangedParmsEntry(
		FScriptExecution&,
		void*,
		void* Parms)
	{
		++ReloadedUnchangedParmsCalls;
		static_cast<FIntReturnParams*>(Parms)->ReturnValue = 22;
	}

	class FReloadCompiler final : public asIJITCompiler
	{
	public:
		bool bReloadGeneration = false;

		void OnFunctionReady(asIScriptFunction* Function) override
		{
			if (Function == nullptr)
			{
				return;
			}

			const char* const FunctionName = Function->GetName();
			if (FCStringAnsi::Strcmp(FunctionName, "ChangedValue") == 0)
			{
				if (!bReloadGeneration)
				{
					asSJITFunctionBinding Binding;
					Binding.VMEntry = &InitialChangedVMEntry;
					Binding.RawEntry = reinterpret_cast<asJITFunction_Raw>(
						&InitialChangedRawEntry);
					Binding.ParmsEntry = &InitialChangedParmsEntry;
					Function->SetJITBinding(Binding);
				}
				return;
			}

			if (FCStringAnsi::Strcmp(FunctionName, "UnchangedValue") != 0)
			{
				return;
			}

			asSJITFunctionBinding Binding;
			if (!bReloadGeneration)
			{
				Binding.VMEntry = &InitialUnchangedVMEntry;
				Binding.RawEntry = reinterpret_cast<asJITFunction_Raw>(
					&InitialUnchangedRawEntry);
				Binding.ParmsEntry = &InitialUnchangedParmsEntry;
			}
			else
			{
				Binding.VMEntry = &ReloadedUnchangedVMEntry;
				Binding.RawEntry = reinterpret_cast<asJITFunction_Raw>(
					&ReloadedUnchangedRawEntry);
				Binding.ParmsEntry = &ReloadedUnchangedParmsEntry;
			}
			Function->SetJITBinding(Binding);
		}

		void ReleaseFunctionBinding(
			asIScriptFunction*,
			const asSJITFunctionBinding&) override
		{
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptUASFunctionCurrentBindingReloadTests,
	"Angelscript.TestModule.StaticJIT.UASFunction.CurrentBindingReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ChangedMethodFallsBackWhileUnchangedMethodRemainsNative)
	{
		using namespace AngelscriptUASFunctionCurrentBindingReloadTests_Private;

		ResetCounters();
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptEngine* const ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("UASFunction reload test should retain its AS engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const asPWORD OriginalIncludeJIT = ScriptEngine->GetEngineProperty(
			asEP_INCLUDE_JIT_INSTRUCTIONS);
		asIJITCompiler* const OriginalCompiler = ScriptEngine->GetJITCompiler();
		FReloadCompiler Compiler;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, 1)));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetJITCompiler(&Compiler)));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(OriginalCompiler);
			ScriptEngine->SetEngineProperty(
				asEP_INCLUDE_JIT_INSTRUCTIONS,
				OriginalIncludeJIT);
		};

		const FName ModuleName(TEXT("UASFunctionCurrentBindingReload"));
		const FString Filename(TEXT("UASFunctionCurrentBindingReload.as"));
		const FName ClassName(TEXT("UUASFunctionCurrentBindingReloadTarget"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = ASTEST_AS(R"AS(
			UCLASS()
			class UUASFunctionCurrentBindingReloadTarget : UObject
			{
				UFUNCTION()
				int ChangedValue(int Left, int Right) final
				{
					return Left + Right + 100;
				}

				UFUNCTION()
				int UnchangedValue() final
				{
					return 22;
				}
			}
			)AS");

		const FString ReloadedSource = ASTEST_AS(R"AS(
			UCLASS()
			class UUASFunctionCurrentBindingReloadTarget : UObject
			{
				UFUNCTION()
				int ChangedValue(int Left, int Right) final
				{
					return Left + Right + 300;
				}

				UFUNCTION()
				int UnchangedValue() final
				{
					return 22;
				}
			}
			)AS");

		UClass* const InitialClass =
			AngelscriptFunctionalTestUtils::CompileScriptModule(
				*TestRunner,
				Engine,
				ModuleName,
				Filename,
				InitialSource,
				ClassName);
		ASSERT_THAT(IsNotNull(
			InitialClass,
			TEXT("UASFunction reload test should compile the initial class")));
		if (InitialClass == nullptr)
		{
			return;
		}

		UASFunction* const ChangedWrapper = Cast<UASFunction>(
			FindGeneratedFunction(InitialClass, TEXT("ChangedValue")));
		UASFunction* const UnchangedWrapper = Cast<UASFunction>(
			FindGeneratedFunction(InitialClass, TEXT("UnchangedValue")));
		UObject* const Instance = NewObject<UObject>(
			GetTransientPackage(),
			InitialClass,
			TEXT("UASFunctionCurrentBindingReloadInstance"));
		ASSERT_THAT(IsNotNull(ChangedWrapper));
		ASSERT_THAT(IsNotNull(UnchangedWrapper));
		ASSERT_THAT(IsNotNull(Instance));
		if (ChangedWrapper == nullptr
			|| UnchangedWrapper == nullptr
			|| Instance == nullptr)
		{
			return;
		}

		asIScriptFunction* const InitialChangedFunction =
			ChangedWrapper->ScriptFunction;
		asIScriptFunction* const InitialUnchangedFunction =
			UnchangedWrapper->ScriptFunction;
		ASSERT_THAT(IsNotNull(InitialChangedFunction));
		ASSERT_THAT(IsNotNull(InitialUnchangedFunction));
		if (InitialChangedFunction == nullptr
			|| InitialUnchangedFunction == nullptr)
		{
			return;
		}

		const asSJITFunctionBinding InitialChangedBinding =
			InitialChangedFunction->GetJITBinding();
		const asSJITFunctionBinding InitialUnchangedBinding =
			InitialUnchangedFunction->GetJITBinding();
		ASSERT_THAT(IsTrue(InitialChangedBinding.VMEntry != nullptr
			&& InitialChangedBinding.RawEntry != nullptr
			&& InitialChangedBinding.ParmsEntry != nullptr));
		ASSERT_THAT(IsTrue(InitialUnchangedBinding.VMEntry != nullptr
			&& InitialUnchangedBinding.RawEntry != nullptr
			&& InitialUnchangedBinding.ParmsEntry != nullptr));

		ASSERT_THAT(IsNull(
			ChangedWrapper->JitFunction,
			TEXT("Reloadable UASFunction must not cache its VM entry")));
		ASSERT_THAT(IsNull(
			ChangedWrapper->JitFunction_Raw,
			TEXT("Reloadable UASFunction must not cache its Raw entry")));
		ASSERT_THAT(IsNull(
			ChangedWrapper->JitFunction_ParmsEntry,
			TEXT("Reloadable UASFunction must not cache its Parms entry")));

		FTwoIntParams InitialChangedParams;
		InitialChangedParams.Left = 1;
		InitialChangedParams.Right = 2;
		ChangedWrapper->RuntimeCallEvent(Instance, &InitialChangedParams);
		FIntReturnParams InitialUnchangedParams;
		UnchangedWrapper->RuntimeCallEvent(Instance, &InitialUnchangedParams);
		ASSERT_THAT(AreEqual(103, InitialChangedParams.ReturnValue));
		ASSERT_THAT(AreEqual(22, InitialUnchangedParams.ReturnValue));
		ASSERT_THAT(AreEqual(1, InitialChangedParmsCalls));
		ASSERT_THAT(AreEqual(1, InitialUnchangedRawCalls));
		ASSERT_THAT(AreEqual(0, InitialUnchangedParmsCalls));

		Compiler.bReloadGeneration = true;
		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			Filename,
			ReloadedSource,
			ReloadResult)));
		ASSERT_THAT(IsTrue(
			ReloadResult == ECompileResult::FullyHandled
				|| ReloadResult == ECompileResult::PartiallyHandled,
			TEXT("Body-only UASFunction update should remain on soft reload")));

		UClass* const ReloadedClass = FindGeneratedClass(&Engine, ClassName);
		ASSERT_THAT(AreEqual(InitialClass, ReloadedClass));
		ASSERT_THAT(AreEqual(
			static_cast<UFunction*>(ChangedWrapper),
			FindGeneratedFunction(ReloadedClass, TEXT("ChangedValue"))));
		ASSERT_THAT(AreEqual(
			static_cast<UFunction*>(UnchangedWrapper),
			FindGeneratedFunction(ReloadedClass, TEXT("UnchangedValue"))));
		ASSERT_THAT(IsTrue(ChangedWrapper->ScriptFunction
			!= InitialChangedFunction));
		ASSERT_THAT(IsTrue(UnchangedWrapper->ScriptFunction
			!= InitialUnchangedFunction));

		const asSJITFunctionBinding ChangedBinding =
			ChangedWrapper->ScriptFunction->GetJITBinding();
		const asSJITFunctionBinding UnchangedBinding =
			UnchangedWrapper->ScriptFunction->GetJITBinding();
		ASSERT_THAT(IsNull(ChangedBinding.VMEntry));
		ASSERT_THAT(IsNull(ChangedBinding.RawEntry));
		ASSERT_THAT(IsNull(ChangedBinding.ParmsEntry));
		ASSERT_THAT(IsTrue(UnchangedBinding.VMEntry
			== &ReloadedUnchangedVMEntry));
		ASSERT_THAT(IsTrue(UnchangedBinding.RawEntry
			== reinterpret_cast<asJITFunction_Raw>(
				&ReloadedUnchangedRawEntry)));
		ASSERT_THAT(IsTrue(UnchangedBinding.ParmsEntry
			== &ReloadedUnchangedParmsEntry));

		FTwoIntParams ReloadedChangedParams;
		ReloadedChangedParams.Left = 1;
		ReloadedChangedParams.Right = 2;
		ChangedWrapper->RuntimeCallEvent(Instance, &ReloadedChangedParams);
		FIntReturnParams ReloadedUnchangedParams;
		UnchangedWrapper->RuntimeCallEvent(
			Instance,
			&ReloadedUnchangedParams);

		ASSERT_THAT(AreEqual(
			303,
			ReloadedChangedParams.ReturnValue,
			TEXT("Changed wrapper should execute the replacement VM body")));
		ASSERT_THAT(AreEqual(
			22,
			ReloadedUnchangedParams.ReturnValue,
			TEXT("Unchanged wrapper should preserve exact Native behavior")));
		ASSERT_THAT(AreEqual(
			1,
			InitialChangedParmsCalls,
			TEXT("Changed wrapper must not call the retired Parms entry")));
		ASSERT_THAT(AreEqual(0, InitialChangedVMCalls));
		ASSERT_THAT(AreEqual(0, InitialChangedRawCalls));
		ASSERT_THAT(AreEqual(1, InitialUnchangedRawCalls));
		ASSERT_THAT(AreEqual(0, InitialUnchangedParmsCalls));
		ASSERT_THAT(AreEqual(0, ReloadedUnchangedParmsCalls));
		ASSERT_THAT(AreEqual(0, ReloadedUnchangedVMCalls));
		ASSERT_THAT(AreEqual(1, ReloadedUnchangedRawCalls));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
