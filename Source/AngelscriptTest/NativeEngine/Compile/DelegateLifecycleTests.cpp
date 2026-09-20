#include "CQTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "NativeEngine/Compile/DelegateNativeFixtures.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "StartAngelscriptHeaders.h"
#include "as_context.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DelegateLifecycleTests
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

	UClass* FindDescribedClass(const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules, const TCHAR* ClassName)
	{
		const FString Wanted = ClassName;
		const FString WantedNoPrefix = Wanted.StartsWith(TEXT("U")) ? Wanted.Mid(1) : Wanted;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
		{
			for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : Module->Classes)
			{
				if (ClassDesc->ClassName == Wanted
					|| ClassDesc->ClassName == WantedNoPrefix
					|| ClassDesc->ClassName == TEXT("U") + WantedNoPrefix)
				{
					return ClassDesc->Class;
				}
			}
		}
		return nullptr;
	}

	asIScriptFunction* FindMethod(asIScriptModule* ScriptModule, const char* TypeName, const char* MethodName)
	{
		if (!ScriptModule || !TypeName || !MethodName)
		{
			return nullptr;
		}
		asITypeInfo* Type = ScriptModule->GetTypeInfoByName(TypeName);
		return Type ? Type->GetMethodByName(MethodName) : nullptr;
	}

	asIScriptFunction* FindOwnerMethod(asIScriptModule* ScriptModule, const char* MethodName)
	{
		asIScriptFunction* Found = FindMethod(ScriptModule, "UDelegateLifecycleOwner", MethodName);
		return Found ? Found : FindMethod(ScriptModule, "DelegateLifecycleOwner", MethodName);
	}

	bool CallScriptMethod(
		asIScriptEngine* ScriptEngine,
		asIScriptFunction* Method,
		UObject* Object,
		FString& OutError)
	{
		if (!ScriptEngine || !Method || !Object)
		{
			OutError = TEXT("missing script call");
			return false;
		}
		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!Context)
		{
			OutError = TEXT("CreateContext failed");
			return false;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		if (Context->Prepare(Method) < 0)
		{
			OutError = TEXT("Prepare failed");
			return false;
		}
		if (Context->SetObject(Object) < 0)
		{
			OutError = TEXT("SetObject failed");
			return false;
		}
		const int Executed = Context->Execute();
		const char* Exception = Context->GetExceptionString();
		OutError = FString::Printf(
			TEXT("execute=%d exception=%s"),
			Executed,
			Exception ? UTF8_TO_TCHAR(Exception) : TEXT("none"));
		if (Executed != asEXECUTION_FINISHED)
		{
			return false;
		}
		OutError.Reset();
		return true;
	}

	int32 ReadIntProperty(UObject* Object, const TCHAR* Name)
	{
		if (!Object)
		{
			return 0;
		}
		if (const FIntProperty* Property = FindFProperty<FIntProperty>(Object->GetClass(), Name))
		{
			return Property->GetPropertyValue_InContainer(Object);
		}
		return 0;
	}

	UBlueprint* CreateTransientBlueprintChild(UClass* ParentClass, const TCHAR* Suffix)
	{
		const FString PackagePath = FString::Printf(
			TEXT("/Temp/DelegateLifecycle_%s_%s"),
			Suffix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!BlueprintPackage)
		{
			return nullptr;
		}
		BlueprintPackage->SetFlags(RF_Transient);
		const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));
		return FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("DelegateLifecycle"));
	}

	const TCHAR* OwnerScriptCompatible()
	{
		return TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealth, int, Current, int, Max);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegateLifecycleListener : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    int Current;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    int Max;\n")
			TEXT("    UFUNCTION()\n")
			TEXT("    void OnHealth(int NewCurrent, int NewMax)\n")
			TEXT("    {\n")
			TEXT("        Current = NewCurrent;\n")
			TEXT("        Max = NewMax;\n")
			TEXT("    }\n")
			TEXT("    UFUNCTION()\n")
			TEXT("    void OnEmpty()\n")
			TEXT("    {\n")
			TEXT("        Current = 1;\n")
			TEXT("        Max = 1;\n")
			TEXT("    }\n")
			TEXT("}\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegateLifecycleOwner : UObject\n")
			TEXT("{\n")
			TEXT("    FOnHealth Ev;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    UDelegateLifecycleListener@ Target;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
			TEXT("    void BindAndFire()\n")
			TEXT("    {\n")
			TEXT("        Ev.AddDynamic(Target, FunctionName);\n")
			TEXT("        Ev.Broadcast(100, 75);\n")
			TEXT("    }\n")
			TEXT("    void ClearBindings()\n")
			TEXT("    {\n")
			TEXT("        Ev.Clear();\n")
			TEXT("    }\n")
			TEXT("}\n");
	}

	bool SetupOwner(
		FAngelscriptEngine& Engine,
		const TCHAR* FileToken,
		TArray<TSharedRef<FAngelscriptModuleDesc>>& OutModules,
		UObject*& OutListener,
		UObject*& OutOwner,
		FString& OutError)
	{
		if (!WriteAndPreprocess(
			*FString::Printf(TEXT("DelegateLifecycle/%s.as"), FileToken),
			OwnerScriptCompatible(),
			OutModules,
			OutError))
		{
			return false;
		}
		if (!Compile(Engine, OutModules, OutError))
		{
			return false;
		}
		UClass* ListenerClass = FindDescribedClass(OutModules, TEXT("UDelegateLifecycleListener"));
		UClass* OwnerClass = FindDescribedClass(OutModules, TEXT("UDelegateLifecycleOwner"));
		if (!ListenerClass || !OwnerClass)
		{
			OutError = TEXT("missing generated classes");
			return false;
		}
		UBlueprint* Blueprint = CreateTransientBlueprintChild(ListenerClass, FileToken);
		if (!Blueprint)
		{
			OutError = TEXT("CreateBlueprint failed");
			return false;
		}
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (!Blueprint->GeneratedClass)
		{
			OutError = TEXT("GeneratedClass missing");
			return false;
		}
		OutListener = NewObject<UObject>(GetTransientPackage(), Blueprint->GeneratedClass);
		OutOwner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		if (!OutListener || !OutOwner)
		{
			OutError = TEXT("NewObject failed");
			return false;
		}
		FObjectProperty* TargetProp = FindFProperty<FObjectProperty>(OwnerClass, TEXT("Target"));
		FNameProperty* NameProp = FindFProperty<FNameProperty>(OwnerClass, TEXT("FunctionName"));
		if (!TargetProp || !NameProp)
		{
			OutError = TEXT("missing Target or FunctionName");
			return false;
		}
		TargetProp->SetObjectPropertyValue_InContainer(OutOwner, OutListener);
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(DelegateLifecycle, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EditorBroadcastAfterLoad)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateLifecycleTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("package")));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		UObject* Listener = nullptr;
		UObject* Owner = nullptr;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateLifecycleTests::SetupOwner(
			*Engine, TEXT("EditorLoad"), InModules, Listener, Owner, Error), *Error));
		FNameProperty* NameProp = FindFProperty<FNameProperty>(Owner->GetClass(), TEXT("FunctionName"));
		ASSERT_THAT(IsNotNull(NameProp, TEXT("FunctionName")));
		NameProp->SetPropertyValue_InContainer(Owner, FName(TEXT("OnHealth")));

		asIScriptFunction* BindAndFire = DelegateLifecycleTests::FindOwnerMethod(
			InModules[0]->ScriptModule, "BindAndFire");
		ASSERT_THAT(IsNotNull(BindAndFire, TEXT("BindAndFire")));
		FString CallError;
		ASSERT_THAT(IsTrue(DelegateLifecycleTests::CallScriptMethod(
			Engine->GetScriptEngine(), BindAndFire, Owner, CallError), *CallError));
		ASSERT_THAT(AreEqual(100, DelegateLifecycleTests::ReadIntProperty(Listener, TEXT("Current"))));
		ASSERT_THAT(AreEqual(75, DelegateLifecycleTests::ReadIntProperty(Listener, TEXT("Max"))));
	}

	TEST_METHOD(IncompatibleReentryRejected)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateLifecycleTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		UObject* Listener = nullptr;
		UObject* Owner = nullptr;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateLifecycleTests::SetupOwner(
			*Engine, TEXT("Incompatible"), InModules, Listener, Owner, Error), *Error));
		FNameProperty* NameProp = FindFProperty<FNameProperty>(Owner->GetClass(), TEXT("FunctionName"));
		ASSERT_THAT(IsNotNull(NameProp, TEXT("FunctionName")));
		NameProp->SetPropertyValue_InContainer(Owner, FName(TEXT("OnEmpty")));

		asIScriptFunction* BindAndFire = DelegateLifecycleTests::FindOwnerMethod(
			InModules[0]->ScriptModule, "BindAndFire");
		ASSERT_THAT(IsNotNull(BindAndFire, TEXT("BindAndFire")));
		FString CallError;
		ASSERT_THAT(IsFalse(DelegateLifecycleTests::CallScriptMethod(
			Engine->GetScriptEngine(), BindAndFire, Owner, CallError), TEXT("incompatible invoke succeeded")));
		ASSERT_THAT(IsTrue(CallError.Contains(TEXT("signature mismatch")), *CallError));
		ASSERT_THAT(AreEqual(0, DelegateLifecycleTests::ReadIntProperty(Listener, TEXT("Current"))));
		ASSERT_THAT(AreEqual(0, DelegateLifecycleTests::ReadIntProperty(Listener, TEXT("Max"))));
	}

	TEST_METHOD(TeardownReleases)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateLifecycleTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		TDelegate<int32(int32)> Native;
		{
			FAngelscriptEngineScope Scope(*Engine);

			TSharedPtr<FAngelscriptModuleDesc> Module;
			FString Error;
			ASSERT_THAT(IsTrue(DelegateNativeFixtures::Compile(*Engine, TEXT("LifecycleTeardown"),
				TEXT("int AddOffset(int Value, int Offset) { return Value + Offset; }\n"),
				Module, Error), *Error));
			asIScriptFunction* AddOffset = DelegateNativeFixtures::FindGlobal(Engine->GetScriptEngine(), "AddOffset");
			ASSERT_THAT(IsNotNull(AddOffset, TEXT("AddOffset")));

			FString BindError;
			ASSERT_THAT(IsTrue(DelegateNativeFixtures::BindIntTransform(
				Native, Engine->GetScriptEngine(), AddOffset, 40, BindError), *BindError));
			ASSERT_THAT(IsTrue(Native.IsBound(), TEXT("native unbound")));
			ASSERT_THAT(AreEqual(42, Native.Execute(2)));

			TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
			UObject* Listener = nullptr;
			UObject* Owner = nullptr;
			ASSERT_THAT(IsTrue(DelegateLifecycleTests::SetupOwner(
				*Engine, TEXT("Teardown"), InModules, Listener, Owner, Error), *Error));
			FNameProperty* NameProp = FindFProperty<FNameProperty>(Owner->GetClass(), TEXT("FunctionName"));
			ASSERT_THAT(IsNotNull(NameProp, TEXT("FunctionName")));
			NameProp->SetPropertyValue_InContainer(Owner, FName(TEXT("OnHealth")));
			asIScriptFunction* BindAndFire = DelegateLifecycleTests::FindOwnerMethod(
				InModules[0]->ScriptModule, "BindAndFire");
			asIScriptFunction* ClearBindings = DelegateLifecycleTests::FindOwnerMethod(
				InModules[0]->ScriptModule, "ClearBindings");
			ASSERT_THAT(IsNotNull(BindAndFire, TEXT("BindAndFire")));
			ASSERT_THAT(IsNotNull(ClearBindings, TEXT("ClearBindings")));
			FString CallError;
			ASSERT_THAT(IsTrue(DelegateLifecycleTests::CallScriptMethod(
				Engine->GetScriptEngine(), BindAndFire, Owner, CallError), *CallError));
			ASSERT_THAT(IsTrue(DelegateLifecycleTests::CallScriptMethod(
				Engine->GetScriptEngine(), ClearBindings, Owner, CallError), *CallError));

			Native.Unbind();
			ASSERT_THAT(IsFalse(Native.IsBound(), TEXT("native still bound after Unbind")));
		}
		Engine.Reset();
		ASSERT_THAT(IsFalse(Native.IsBound(), TEXT("native rebound after host teardown")));
	}
};

#endif
