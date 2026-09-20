#include "CQTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptDescriptors.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "StartAngelscriptHeaders.h"
#include "as_module.h"
#include "EndAngelscriptHeaders.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/ObjectMacros.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DelegateReflectionTests
{
	TUniquePtr<FAngelscriptEngine> MakeHostEngine(FString& OutDiagnostic)
	{
		if (!FAngelscriptBind::PrepareForEngineInitialization(OutDiagnostic))
		{
			return {};
		}
		FAngelscriptEngineConfig Config = FAngelscriptEngineConfig::FromCurrentProcess();
		Config.bSkipInitialCompile = true;
		Config.bOverrideCacheV2Enablement = true;
		Config.bEnableCacheV2 = false;
		return FAngelscriptEngine::Create(Config, FAngelscriptEngineDependencies::CreateDefault());
	}

	TSharedRef<FAngelscriptModuleDesc> MakeModule(const TCHAR* Token, const FString& Code)
	{
		TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
		Module->ModuleName = FString::Printf(TEXT("DelegateReflection.%s"), Token);
		FAngelscriptModuleDesc::FCodeSection Section;
		Section.AbsoluteFilename = Module->ModuleName + TEXT(".as");
		Section.RelativeFilename = Section.AbsoluteFilename;
		Section.Code = Code;
		Module->Code.Add(MoveTemp(Section));
		return Module;
	}

	bool Compile(
		FAngelscriptEngine& Engine,
		TArray<TSharedRef<FAngelscriptModuleDesc>>& InModules,
		FString& OutError)
	{
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		const ECompileResult Result = Engine.CompileModules(ECompileType::Initial, InModules, OutModules);
		if (Result != ECompileResult::FullyHandled && Result != ECompileResult::PartiallyHandled)
		{
			OutError = FString::Printf(TEXT("CompileModules=%d diagnostics=%s"), int32(Result), *Engine.FormatDiagnostics());
			return false;
		}
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : InModules)
		{
			if (Module->bCompileError || Module->ScriptModule == nullptr)
			{
				OutError = FString::Printf(
					TEXT("compileError=%d scriptModule=%d diagnostics=%s"),
					Module->bCompileError ? 1 : 0,
					Module->ScriptModule ? 1 : 0,
					*Engine.FormatDiagnostics());
				return false;
			}
		}
		return true;
	}

	FString SignatureObjectName(const TCHAR* DelegateName)
	{
		return FString(DelegateName) + HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX;
	}

	bool WriteAndPreprocess(
		const TCHAR* RelativeFilename,
		const FString& Script,
		TArray<TSharedRef<FAngelscriptModuleDesc>>& OutModules,
		FString& OutError)
	{
		const FString AbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), RelativeFilename);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteFilename), true);
		if (!FFileHelper::SaveStringToFile(Script, *AbsoluteFilename))
		{
			OutError = FString::Printf(TEXT("could not write %s"), *AbsoluteFilename);
			return false;
		}
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(RelativeFilename, AbsoluteFilename);
		if (!Preprocessor.Preprocess())
		{
			OutError = TEXT("preprocessor failed");
			return false;
		}
		OutModules = Preprocessor.GetModulesToCompile();
		if (OutModules.Num() != 1)
		{
			OutError = FString::Printf(TEXT("expected 1 module, got %d"), OutModules.Num());
			return false;
		}
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(DelegateReflection, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DynamicSignaturePublished)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateReflectionTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("package")));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		InModules.Add(DelegateReflectionTests::MakeModule(TEXT("Dynamic"),
			TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealth, int, NewHealth);\n")));
		FString Error;
		ASSERT_THAT(IsTrue(DelegateReflectionTests::Compile(*Engine, InModules, Error), *Error));

		const FString ObjectName = DelegateReflectionTests::SignatureObjectName(TEXT("FOnHealth"));
		UDelegateFunction* Function = FindObject<UDelegateFunction>(Engine->GetPackageInstance(), *ObjectName);
		ASSERT_THAT(IsNotNull(Function, *FString::Printf(TEXT("missing %s"), *ObjectName)));
		ASSERT_THAT(IsTrue(Function->HasAnyFunctionFlags(FUNC_Delegate | FUNC_MulticastDelegate), TEXT("flags")));

		asITypeInfo* ScriptType = InModules[0]->ScriptModule
			? InModules[0]->ScriptModule->GetTypeInfoByName("FOnHealth")
			: nullptr;
		ASSERT_THAT(IsNotNull(ScriptType, TEXT("FOnHealth type")));
		ASSERT_THAT(IsTrue(ScriptType->GetUserData() == Function, TEXT("UserData")));

		FProperty* Param = Function->FindPropertyByName(TEXT("NewHealth"));
		ASSERT_THAT(IsNotNull(Param, TEXT("NewHealth")));
	}

	TEST_METHOD(OrdinaryBlueprintAssignableRejected)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateReflectionTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateReflectionTests::WriteAndPreprocess(
			TEXT("DelegateReflection/OrdinaryAssignable.as"),
			TEXT("DECLARE_MULTICAST_DELEGATE_OneParam(FOnOrdinary, int);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegateReflectionOrdinaryOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY(BlueprintAssignable)\n")
			TEXT("    FOnOrdinary Ev;\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		TestRunner->AddExpectedError(TEXT("Ordinary multicast"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(
			TEXT("An error was encountered during angelscript hot reload. Keeping old angelscript code active."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		const ECompileResult Result = Engine->CompileModules(ECompileType::Initial, InModules, OutModules);
		const FString Combined = Engine->FormatDiagnostics();
		const bool bFailed = Result == ECompileResult::Error
			|| Result == ECompileResult::ErrorNeedFullReload
			|| InModules[0]->bCompileError
			|| !FindObject<UClass>(Engine->GetPackageInstance(), TEXT("DelegateReflectionOrdinaryOwner"));
		ASSERT_THAT(IsTrue(bFailed, *FString::Printf(
			TEXT("ordinary BlueprintAssignable published compile=%d error=%d diagnostics=%s"),
			int32(Result),
			InModules[0]->bCompileError ? 1 : 0,
			*Combined)));
		ASSERT_THAT(IsTrue(
			Combined.Contains(TEXT("Ordinary multicast"))
				|| Combined.Contains(TEXT("BlueprintAssignable")),
			*Combined));
	}

	TEST_METHOD(ShortNameNoCollision)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateReflectionTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("package")));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> FirstModules;
		FirstModules.Add(DelegateReflectionTests::MakeModule(TEXT("A"), TEXT("DECLARE_DYNAMIC_DELEGATE(FOnDone);\n")));
		FString FirstError;
		ASSERT_THAT(IsTrue(DelegateReflectionTests::Compile(*Engine, FirstModules, FirstError), *FirstError));
		const FString ObjectName = DelegateReflectionTests::SignatureObjectName(TEXT("FOnDone"));
		UDelegateFunction* First = FindObject<UDelegateFunction>(Engine->GetPackageInstance(), *ObjectName);
		ASSERT_THAT(IsNotNull(First, *FString::Printf(TEXT("missing first %s"), *ObjectName)));

		TArray<TSharedRef<FAngelscriptModuleDesc>> SecondModules;
		SecondModules.Add(DelegateReflectionTests::MakeModule(TEXT("B"), TEXT("DECLARE_DYNAMIC_DELEGATE(FOnDone);\n")));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		TestRunner->AddExpectedError(TEXT("Register failed"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		const ECompileResult Result = Engine->CompileModules(ECompileType::Initial, SecondModules, OutModules);
		UDelegateFunction* After = FindObject<UDelegateFunction>(Engine->GetPackageInstance(), *ObjectName);
		ASSERT_THAT(IsTrue(After == First, TEXT("second FOnDone overwrote the first signature")));
		const bool bSecondPublished = Result == ECompileResult::FullyHandled
			&& SecondModules[0]->bCompileError == false
			&& After != First;
		ASSERT_THAT(IsFalse(bSecondPublished, *FString::Printf(
			TEXT("colliding FOnDone published compile=%d b=%d diagnostics=%s"),
			int32(Result),
			SecondModules[0]->bCompileError ? 1 : 0,
			*Engine->FormatDiagnostics())));
	}
};

#endif
