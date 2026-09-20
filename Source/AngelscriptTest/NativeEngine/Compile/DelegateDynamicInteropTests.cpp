#include "CQTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptDescriptors.h"
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

namespace DelegateDynamicInteropTests
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
		Module->ModuleName = FString::Printf(TEXT("DelegateDynamicInterop.%s"), Token);
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

	asIScriptFunction* FindGlobal(asIScriptEngine* Engine, const char* Name)
	{
		if (!Engine || !Name)
		{
			return nullptr;
		}
		const asUINT Count = Engine->GetGlobalFunctionCount();
		for (asUINT Index = 0; Index < Count; ++Index)
		{
			asIScriptFunction* Function = Engine->GetGlobalFunctionByIndex(Index);
			if (Function && FCStringAnsi::Strcmp(Function->GetName(), Name) == 0)
			{
				return Function;
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

	bool CallScriptMethod(
		asIScriptEngine* ScriptEngine,
		asIScriptFunction* Method,
		UObject* Object,
		UObject* Arg0,
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
		if (Arg0)
		{
			Context->SetArgObject(0, Arg0);
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
			TEXT("/Temp/DelegateDynamicInterop_%s_%s"),
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
			TEXT("DelegateDynamicInterop"));
	}
}

TEST_CLASS_WITH_FLAGS(DelegateDynamicInterop, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BlueprintListenerValues)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateDynamicInteropTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("package")));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateDynamicInteropTests::WriteAndPreprocess(
			TEXT("DelegateDynamicInterop/BlueprintListener.as"),
			TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealth, int, Current, int, Max);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegateDynamicHealthListener : UObject\n")
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
			TEXT("}\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegateDynamicHealthOwner : UObject\n")
			TEXT("{\n")
			TEXT("    FOnHealth Ev;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    UDelegateDynamicHealthListener@ Target;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
			TEXT("    void BindAndFire()\n")
			TEXT("    {\n")
			TEXT("        Ev.AddDynamic(Target, FunctionName);\n")
			TEXT("        Ev.Broadcast(100, 75);\n")
			TEXT("    }\n")
			TEXT("    void FireAgain()\n")
			TEXT("    {\n")
			TEXT("        Ev.Broadcast(9, 9);\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegateDynamicInteropTests::Compile(*Engine, InModules, Error), *Error));

		UClass* ListenerClass = DelegateDynamicInteropTests::FindDescribedClass(InModules, TEXT("UDelegateDynamicHealthListener"));
		UClass* OwnerClass = DelegateDynamicInteropTests::FindDescribedClass(InModules, TEXT("UDelegateDynamicHealthOwner"));
		ASSERT_THAT(IsNotNull(ListenerClass, TEXT("listener class")));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));

		UBlueprint* Blueprint = DelegateDynamicInteropTests::CreateTransientBlueprintChild(ListenerClass, TEXT("Listener"));
		ASSERT_THAT(IsNotNull(Blueprint, TEXT("CreateBlueprint")));
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		UClass* BlueprintClass = Blueprint->GeneratedClass;
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("GeneratedClass")));

		UObject* Listener = NewObject<UObject>(GetTransientPackage(), BlueprintClass);
		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Listener, TEXT("listener")));
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));

		auto FindOwnerMethod = [&](const char* Name) -> asIScriptFunction*
		{
			asIScriptFunction* Found = DelegateDynamicInteropTests::FindMethod(
				InModules[0]->ScriptModule, "UDelegateDynamicHealthOwner", Name);
			return Found ? Found : DelegateDynamicInteropTests::FindMethod(
				InModules[0]->ScriptModule, "DelegateDynamicHealthOwner", Name);
		};
		asIScriptFunction* BindAndFire = FindOwnerMethod("BindAndFire");
		asIScriptFunction* FireAgain = FindOwnerMethod("FireAgain");
		ASSERT_THAT(IsNotNull(BindAndFire, TEXT("BindAndFire")));
		ASSERT_THAT(IsNotNull(FireAgain, TEXT("FireAgain")));

		FObjectProperty* TargetProp = FindFProperty<FObjectProperty>(OwnerClass, TEXT("Target"));
		ASSERT_THAT(IsNotNull(TargetProp, TEXT("Target property")));
		TargetProp->SetObjectPropertyValue_InContainer(Owner, Listener);
		ASSERT_THAT(IsTrue(
			TargetProp->GetObjectPropertyValue_InContainer(Owner) == Listener,
			TEXT("Target writeback")));
		FNameProperty* NameProp = FindFProperty<FNameProperty>(OwnerClass, TEXT("FunctionName"));
		ASSERT_THAT(IsNotNull(NameProp, TEXT("FunctionName property")));
		NameProp->SetPropertyValue_InContainer(Owner, FName(TEXT("OnHealth")));
		ASSERT_THAT(IsTrue(
			NameProp->GetPropertyValue_InContainer(Owner) == FName(TEXT("OnHealth")),
			TEXT("FunctionName writeback")));
		ASSERT_THAT(IsNotNull(Listener->FindFunction(FName(TEXT("OnHealth"))), TEXT("listener OnHealth")));

		FString CallError;
		ASSERT_THAT(IsTrue(DelegateDynamicInteropTests::CallScriptMethod(
			Engine->GetScriptEngine(), BindAndFire, Owner, nullptr, CallError), *FString::Printf(TEXT("BindAndFire %s"), *CallError)));
		ASSERT_THAT(AreEqual(
			100,
			DelegateDynamicInteropTests::ReadIntProperty(Listener, TEXT("Current")),
			TEXT("after BindAndFire")));
		ASSERT_THAT(AreEqual(
			75,
			DelegateDynamicInteropTests::ReadIntProperty(Listener, TEXT("Max")),
			TEXT("after BindAndFire")));

		Listener->MarkAsGarbage();
		ASSERT_THAT(IsTrue(DelegateDynamicInteropTests::CallScriptMethod(
			Engine->GetScriptEngine(), FireAgain, Owner, nullptr, CallError), *CallError));
		ASSERT_THAT(AreEqual(100, DelegateDynamicInteropTests::ReadIntProperty(Listener, TEXT("Current"))));
		ASSERT_THAT(AreEqual(75, DelegateDynamicInteropTests::ReadIntProperty(Listener, TEXT("Max"))));
	}

	TEST_METHOD(ScriptUFunctionFromNative)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateDynamicInteropTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("package")));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateDynamicInteropTests::WriteAndPreprocess(
			TEXT("DelegateDynamicInterop/NativeInvoke.as"),
			TEXT("UCLASS()\n")
			TEXT("class UDelegateDynamicNativeSink : UObject\n")
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
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegateDynamicInteropTests::Compile(*Engine, InModules, Error), *Error));

		UClass* SinkClass = DelegateDynamicInteropTests::FindDescribedClass(InModules, TEXT("UDelegateDynamicNativeSink"));
		ASSERT_THAT(IsNotNull(SinkClass, TEXT("sink class")));
		UObject* Sink = NewObject<UObject>(GetTransientPackage(), SinkClass);
		ASSERT_THAT(IsNotNull(Sink, TEXT("sink")));

		FScriptDelegate One;
		One.BindUFunction(Sink, FName(TEXT("OnHealth")));
		ASSERT_THAT(IsTrue(One.IsBound(), TEXT("native unbound")));
		FMulticastScriptDelegate Multi;
		Multi.Add(One);
		struct FOnHealthParams { int32 Current = 0; int32 Max = 0; };
		FOnHealthParams Params;
		Params.Current = 100;
		Params.Max = 75;
		Multi.ProcessDelegate<UObject>(&Params);
		ASSERT_THAT(AreEqual(100, DelegateDynamicInteropTests::ReadIntProperty(Sink, TEXT("Current"))));
		ASSERT_THAT(AreEqual(75, DelegateDynamicInteropTests::ReadIntProperty(Sink, TEXT("Max"))));
	}

	TEST_METHOD(BindDynamicRequiresUFunction)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateDynamicInteropTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		InModules.Add(DelegateDynamicInteropTests::MakeModule(
			TEXT("RequiresUFunction"),
			TEXT("DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(int, FAdd, int, Value);\n")
			TEXT("int Add(int Value) { return Value + 1; }\n")
			TEXT("void Test(FAdd Handler) { Handler.BindDynamic(Add); }\n")));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		TestRunner->AddExpectedError(TEXT("bind-requires-ufunction"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		const ECompileResult Result = Engine->CompileModules(ECompileType::Initial, InModules, OutModules);
		const FString Combined = Engine->FormatDiagnostics();
		const bool bFailed = Result == ECompileResult::Error
			|| Result == ECompileResult::ErrorNeedFullReload
			|| InModules[0]->bCompileError;
		ASSERT_THAT(IsTrue(bFailed, *FString::Printf(TEXT("BindDynamic accepted compile=%d diagnostics=%s"), int32(Result), *Combined)));
		ASSERT_THAT(IsTrue(Combined.Contains(TEXT("bind-requires-ufunction")), *Combined));
	}
};

#endif
