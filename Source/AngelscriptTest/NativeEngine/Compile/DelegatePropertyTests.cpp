#include "CQTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Binds/Bind_Delegates.h"
#include "AngelscriptDescriptors.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "StartAngelscriptHeaders.h"
#include "as_context.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DelegatePropertyTests
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

	asIScriptFunction* FindMethod(asIScriptModule* ScriptModule, const char* TypeName, const char* MethodName)
	{
		if (!ScriptModule || !TypeName || !MethodName)
		{
			return nullptr;
		}
		asITypeInfo* Type = ScriptModule->GetTypeInfoByName(TypeName);
		return Type ? Type->GetMethodByName(MethodName) : nullptr;
	}

	asIScriptFunction* FindOwnerMethod(
		asIScriptModule* ScriptModule,
		const char* PrefixedName,
		const char* BareName,
		const char* MethodName)
	{
		asIScriptFunction* Found = FindMethod(ScriptModule, PrefixedName, MethodName);
		return Found ? Found : FindMethod(ScriptModule, BareName, MethodName);
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

	bool WriteIntProperty(UObject* Object, const TCHAR* Name, int32 Value, FString& OutError)
	{
		if (!Object)
		{
			OutError = TEXT("missing object");
			return false;
		}
		FIntProperty* Property = FindFProperty<FIntProperty>(Object->GetClass(), Name);
		if (!Property)
		{
			OutError = FString::Printf(TEXT("missing int %s"), Name);
			return false;
		}
		Property->SetPropertyValue_InContainer(Object, Value);
		OutError.Reset();
		return true;
	}

	bool WriteListenerTarget(
		UObject* Owner,
		UClass* OwnerClass,
		UObject* Listener,
		FString& OutError)
	{
		FObjectProperty* TargetProp = FindFProperty<FObjectProperty>(OwnerClass, TEXT("Target"));
		if (!TargetProp)
		{
			OutError = TEXT("missing Target");
			return false;
		}
		TargetProp->SetObjectPropertyValue_InContainer(Owner, Listener);
		FNameProperty* NameProp = FindFProperty<FNameProperty>(OwnerClass, TEXT("FunctionName"));
		if (!NameProp)
		{
			OutError = TEXT("missing FunctionName");
			return false;
		}
		NameProp->SetPropertyValue_InContainer(Owner, FName(TEXT("OnHealth")));
		OutError.Reset();
		return true;
	}

	FName ReadNameProperty(UObject* Object, const TCHAR* Name)
	{
		if (!Object)
		{
			return NAME_None;
		}
		if (const FNameProperty* Property = FindFProperty<FNameProperty>(Object->GetClass(), Name))
		{
			return Property->GetPropertyValue_InContainer(Object);
		}
		return NAME_None;
	}

	bool WriteFunctionName(UObject* Owner, UClass* OwnerClass, const TCHAR* FunctionName, FString& OutError)
	{
		FNameProperty* NameProp = FindFProperty<FNameProperty>(OwnerClass, TEXT("FunctionName"));
		if (!NameProp)
		{
			OutError = TEXT("missing FunctionName");
			return false;
		}
		NameProp->SetPropertyValue_InContainer(Owner, FName(FunctionName));
		OutError.Reset();
		return true;
	}

	FString MakeHealthScript(const TCHAR* DelegateName, const TCHAR* ListenerName, const TCHAR* OwnerName)
	{
		return FString::Printf(
			TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(%s, int, Current, int, Max);\n")
			TEXT("UCLASS()\n")
			TEXT("class %s : UObject\n")
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
			TEXT("class %s : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    %s Ev;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    %s@ Target;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
			TEXT("    void Bind()\n")
			TEXT("    {\n")
			TEXT("        Ev.AddDynamic(Target, FunctionName);\n")
			TEXT("    }\n")
			TEXT("    void Fire()\n")
			TEXT("    {\n")
			TEXT("        Ev.Broadcast(100, 75);\n")
			TEXT("    }\n")
			TEXT("    void BindAndFire()\n")
			TEXT("    {\n")
			TEXT("        Bind();\n")
			TEXT("        Fire();\n")
			TEXT("    }\n")
			TEXT("}\n"),
			DelegateName,
			ListenerName,
			OwnerName,
			DelegateName,
			ListenerName);
	}
}

TEST_CLASS_WITH_FLAGS(DelegateProperty, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DynamicMulticastPropertyFires)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("package")));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/DynamicMulticastPropertyFires.as"),
			DelegatePropertyTests::MakeHealthScript(
				TEXT("FOnPropertyHealth"),
				TEXT("UDelegatePropertyHealthListener"),
				TEXT("UDelegatePropertyHealthOwner")),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* ListenerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyHealthListener"));
		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyHealthOwner"));
		ASSERT_THAT(IsNotNull(ListenerClass, TEXT("listener class")));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));

		FMulticastInlineDelegateProperty* EvProp = FindFProperty<FMulticastInlineDelegateProperty>(OwnerClass, TEXT("Ev"));
		ASSERT_THAT(IsNotNull(EvProp, TEXT("Ev should be FMulticastInlineDelegateProperty")));
		ASSERT_THAT(IsNotNull(EvProp->SignatureFunction, TEXT("Ev signature")));
		ASSERT_THAT(IsTrue(
			EvProp->SignatureFunction->GetName().Contains(TEXT("FOnPropertyHealth")),
			*EvProp->SignatureFunction->GetName()));

		UObject* Listener = NewObject<UObject>(GetTransientPackage(), ListenerClass);
		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Listener, TEXT("listener")));
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteListenerTarget(Owner, OwnerClass, Listener, Error), *Error));

		asIScriptFunction* BindAndFire = DelegatePropertyTests::FindOwnerMethod(
			InModules[0]->ScriptModule,
			"UDelegatePropertyHealthOwner",
			"DelegatePropertyHealthOwner",
			"BindAndFire");
		ASSERT_THAT(IsNotNull(BindAndFire, TEXT("BindAndFire")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::CallScriptMethod(
			Engine->GetScriptEngine(), BindAndFire, Owner, Error), *Error));
		ASSERT_THAT(AreEqual(100, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Current")), TEXT("Current")));
		ASSERT_THAT(AreEqual(75, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Max")), TEXT("Max")));
	}

	TEST_METHOD(NativeProcessUsingPublishedSignature)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/NativeProcessUsingPublishedSignature.as"),
			DelegatePropertyTests::MakeHealthScript(
				TEXT("FOnPropertyNativeSig"),
				TEXT("UDelegatePropertyNativeSigListener"),
				TEXT("UDelegatePropertyNativeSigOwner")),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* ListenerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyNativeSigListener"));
		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyNativeSigOwner"));
		ASSERT_THAT(IsNotNull(ListenerClass, TEXT("listener class")));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));

		FMulticastInlineDelegateProperty* EvProp = FindFProperty<FMulticastInlineDelegateProperty>(OwnerClass, TEXT("Ev"));
		ASSERT_THAT(IsNotNull(EvProp, TEXT("Ev property")));
		ASSERT_THAT(IsNotNull(EvProp->SignatureFunction, TEXT("published signature")));
		ASSERT_THAT(IsTrue(
			EvProp->SignatureFunction->HasAnyFunctionFlags(FUNC_Delegate | FUNC_MulticastDelegate),
			TEXT("signature flags")));

		UObject* Listener = NewObject<UObject>(GetTransientPackage(), ListenerClass);
		ASSERT_THAT(IsNotNull(Listener, TEXT("listener")));
		ASSERT_THAT(IsNotNull(Listener->FindFunction(FName(TEXT("OnHealth"))), TEXT("listener OnHealth")));

		FScriptDelegate One;
		One.BindUFunction(Listener, FName(TEXT("OnHealth")));
		ASSERT_THAT(IsTrue(One.IsBound(), TEXT("native BindUFunction")));
		FMulticastScriptDelegate Multi;
		Multi.Add(One);
		struct FOnHealthParams
		{
			int32 Current = 0;
			int32 Max = 0;
		};
		FOnHealthParams Params;
		Params.Current = 100;
		Params.Max = 75;
		Multi.ProcessDelegate<UObject>(&Params);
		ASSERT_THAT(AreEqual(100, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Current")), TEXT("native ProcessDelegate Current")));
		ASSERT_THAT(AreEqual(75, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Max")), TEXT("native ProcessDelegate Max")));
	}

	TEST_METHOD(DynamicSingleCastPropertyExecute)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/DynamicSingleCastPropertyExecute.as"),
			TEXT("DECLARE_DYNAMIC_DELEGATE_OneParam(FPropertyAdd, int, Value);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyAddOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FPropertyAdd Handler;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    UDelegatePropertyAddOwner@ Target;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    int Result;\n")
			TEXT("    UFUNCTION()\n")
			TEXT("    void AddOne(int Value)\n")
			TEXT("    {\n")
			TEXT("        Result = Value + 1;\n")
			TEXT("    }\n")
			TEXT("    void BindAndExecute()\n")
			TEXT("    {\n")
			TEXT("        Handler.BindDynamic(Target, FunctionName);\n")
			TEXT("        Handler.Execute(41);\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyAddOwner"));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));

		FDelegateProperty* HandlerProp = FindFProperty<FDelegateProperty>(OwnerClass, TEXT("Handler"));
		ASSERT_THAT(IsNotNull(HandlerProp, TEXT("Handler should be FDelegateProperty")));
		ASSERT_THAT(IsNotNull(HandlerProp->SignatureFunction, TEXT("Handler signature")));

		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		FObjectProperty* TargetProp = FindFProperty<FObjectProperty>(OwnerClass, TEXT("Target"));
		ASSERT_THAT(IsNotNull(TargetProp, TEXT("Target")));
		TargetProp->SetObjectPropertyValue_InContainer(Owner, Owner);
		FNameProperty* NameProp = FindFProperty<FNameProperty>(OwnerClass, TEXT("FunctionName"));
		ASSERT_THAT(IsNotNull(NameProp, TEXT("FunctionName")));
		NameProp->SetPropertyValue_InContainer(Owner, FName(TEXT("AddOne")));

		asIScriptFunction* BindAndExecute = DelegatePropertyTests::FindOwnerMethod(
			InModules[0]->ScriptModule,
			"UDelegatePropertyAddOwner",
			"DelegatePropertyAddOwner",
			"BindAndExecute");
		ASSERT_THAT(IsNotNull(BindAndExecute, TEXT("BindAndExecute")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::CallScriptMethod(
			Engine->GetScriptEngine(), BindAndExecute, Owner, Error), *Error));
		ASSERT_THAT(AreEqual(42, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("Result")), TEXT("Execute through UPROPERTY")));
	}

	TEST_METHOD(BlueprintAssignableDynamicAccepted)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/BlueprintAssignableDynamicAccepted.as"),
			TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPropertyAssignable, int, Current, int, Max);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyAssignableOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY(BlueprintAssignable)\n")
			TEXT("    FOnPropertyAssignable Ev;\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyAssignableOwner"));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));
		FMulticastInlineDelegateProperty* EvProp = FindFProperty<FMulticastInlineDelegateProperty>(OwnerClass, TEXT("Ev"));
		ASSERT_THAT(IsNotNull(EvProp, TEXT("Ev should be FMulticastInlineDelegateProperty")));
		ASSERT_THAT(IsTrue(EvProp->HasAnyPropertyFlags(CPF_BlueprintAssignable), TEXT("CPF_BlueprintAssignable")));
		ASSERT_THAT(IsTrue(EvProp->HasAnyPropertyFlags(CPF_BlueprintCallable), TEXT("CPF_BlueprintCallable")));
	}

	TEST_METHOD(PropertyIsBoundClearRemove)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/PropertyIsBoundClearRemove.as"),
			TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBoundHealth, int, Current, int, Max);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyBoundListener : UObject\n")
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
			TEXT("class UDelegatePropertyBoundOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FOnBoundHealth Ev;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    UDelegatePropertyBoundListener@ Target;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
			TEXT("    uint64 Handle;\n")
			TEXT("    void BindAndFire()\n")
			TEXT("    {\n")
			TEXT("        Handle = Ev.AddDynamic(Target, FunctionName);\n")
			TEXT("        Ev.Broadcast(100, 75);\n")
			TEXT("    }\n")
			TEXT("    void ClearAndFire()\n")
			TEXT("    {\n")
			TEXT("        Ev.Clear();\n")
			TEXT("        Ev.Broadcast(1, 1);\n")
			TEXT("    }\n")
			TEXT("    void RebindRemoveAndFire()\n")
			TEXT("    {\n")
			TEXT("        Handle = Ev.AddDynamic(Target, FunctionName);\n")
			TEXT("        Ev.Remove(Handle);\n")
			TEXT("        Ev.Broadcast(2, 2);\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* ListenerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyBoundListener"));
		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyBoundOwner"));
		ASSERT_THAT(IsNotNull(ListenerClass, TEXT("listener class")));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));
		ASSERT_THAT(IsNotNull(
			FindFProperty<FMulticastInlineDelegateProperty>(OwnerClass, TEXT("Ev")),
			TEXT("Ev should be FMulticastInlineDelegateProperty")));

		UObject* Listener = NewObject<UObject>(GetTransientPackage(), ListenerClass);
		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Listener, TEXT("listener")));
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteListenerTarget(Owner, OwnerClass, Listener, Error), *Error));
		ASSERT_THAT(AreEqual(0, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Current")), TEXT("unbound Current")));

		asIScriptModule* ScriptModule = InModules[0]->ScriptModule;
		asIScriptEngine* ScriptEngine = Engine->GetScriptEngine();
		auto CallOwner = [&](const char* MethodName) -> bool
		{
			asIScriptFunction* Method = DelegatePropertyTests::FindOwnerMethod(
				ScriptModule, "UDelegatePropertyBoundOwner", "DelegatePropertyBoundOwner", MethodName);
			if (!Method)
			{
				Error = FString::Printf(TEXT("missing %s"), UTF8_TO_TCHAR(MethodName));
				return false;
			}
			return DelegatePropertyTests::CallScriptMethod(ScriptEngine, Method, Owner, Error);
		};

		ASSERT_THAT(IsTrue(CallOwner("BindAndFire"), *Error));
		ASSERT_THAT(AreEqual(100, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Current")), TEXT("bound Current")));
		ASSERT_THAT(AreEqual(75, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Max")), TEXT("bound Max")));

		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteIntProperty(Listener, TEXT("Current"), 0, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteIntProperty(Listener, TEXT("Max"), 0, Error), *Error));
		ASSERT_THAT(IsTrue(CallOwner("ClearAndFire"), *Error));
		ASSERT_THAT(AreEqual(0, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Current")), TEXT("after Clear Current")));
		ASSERT_THAT(AreEqual(0, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Max")), TEXT("after Clear Max")));

		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteIntProperty(Listener, TEXT("Current"), 0, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteIntProperty(Listener, TEXT("Max"), 0, Error), *Error));
		ASSERT_THAT(IsTrue(CallOwner("RebindRemoveAndFire"), *Error));
		ASSERT_THAT(AreEqual(0, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Current")), TEXT("after Remove Current")));
		ASSERT_THAT(AreEqual(0, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Max")), TEXT("after Remove Max")));
	}

	TEST_METHOD(ScriptBindUFunctionFires)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/ScriptBindUFunctionFires.as"),
			TEXT("DECLARE_DYNAMIC_DELEGATE_OneParam(FPropertyBindFn, int, Value);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyBindFnOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FPropertyBindFn Handler;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    UDelegatePropertyBindFnOwner@ Target;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    int Result;\n")
			TEXT("    UFUNCTION()\n")
			TEXT("    void AddOne(int Value)\n")
			TEXT("    {\n")
			TEXT("        Result = Value + 1;\n")
			TEXT("    }\n")
			TEXT("    void BindAndExecute()\n")
			TEXT("    {\n")
			TEXT("        Handler.BindUFunction(Target, FunctionName);\n")
			TEXT("        Handler.Execute(41);\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyBindFnOwner"));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));
		ASSERT_THAT(IsNotNull(
			FindFProperty<FDelegateProperty>(OwnerClass, TEXT("Handler")),
			TEXT("Handler should be FDelegateProperty")));

		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		FObjectProperty* TargetProp = FindFProperty<FObjectProperty>(OwnerClass, TEXT("Target"));
		ASSERT_THAT(IsNotNull(TargetProp, TEXT("Target")));
		TargetProp->SetObjectPropertyValue_InContainer(Owner, Owner);
		FNameProperty* NameProp = FindFProperty<FNameProperty>(OwnerClass, TEXT("FunctionName"));
		ASSERT_THAT(IsNotNull(NameProp, TEXT("FunctionName")));
		NameProp->SetPropertyValue_InContainer(Owner, FName(TEXT("AddOne")));

		asIScriptFunction* BindAndExecute = DelegatePropertyTests::FindOwnerMethod(
			InModules[0]->ScriptModule,
			"UDelegatePropertyBindFnOwner",
			"DelegatePropertyBindFnOwner",
			"BindAndExecute");
		ASSERT_THAT(IsNotNull(BindAndExecute, TEXT("BindAndExecute")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::CallScriptMethod(
			Engine->GetScriptEngine(), BindAndExecute, Owner, Error), *Error));
		ASSERT_THAT(AreEqual(42, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("Result")), TEXT("BindUFunction Execute")));
	}

	TEST_METHOD(DynamicMulticastOneParamFires)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/DynamicMulticastOneParamFires.as"),
			TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPropertyOne, int, NewHealth);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyOneListener : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    int Current;\n")
			TEXT("    UFUNCTION()\n")
			TEXT("    void OnHealth(int NewHealth)\n")
			TEXT("    {\n")
			TEXT("        Current = NewHealth;\n")
			TEXT("    }\n")
			TEXT("}\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyOneOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FOnPropertyOne Ev;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    UDelegatePropertyOneListener@ Target;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
			TEXT("    void BindAndFire()\n")
			TEXT("    {\n")
			TEXT("        Ev.AddDynamic(Target, FunctionName);\n")
			TEXT("        Ev.Broadcast(42);\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* ListenerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyOneListener"));
		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyOneOwner"));
		ASSERT_THAT(IsNotNull(ListenerClass, TEXT("listener class")));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));
		ASSERT_THAT(IsNotNull(
			FindFProperty<FMulticastInlineDelegateProperty>(OwnerClass, TEXT("Ev")),
			TEXT("Ev should be FMulticastInlineDelegateProperty")));

		UObject* Listener = NewObject<UObject>(GetTransientPackage(), ListenerClass);
		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Listener, TEXT("listener")));
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteListenerTarget(Owner, OwnerClass, Listener, Error), *Error));

		asIScriptFunction* BindAndFire = DelegatePropertyTests::FindOwnerMethod(
			InModules[0]->ScriptModule,
			"UDelegatePropertyOneOwner",
			"DelegatePropertyOneOwner",
			"BindAndFire");
		ASSERT_THAT(IsNotNull(BindAndFire, TEXT("BindAndFire")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::CallScriptMethod(
			Engine->GetScriptEngine(), BindAndFire, Owner, Error), *Error));
		ASSERT_THAT(AreEqual(42, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Current")), TEXT("OneParam Current")));
	}

	TEST_METHOD(PropertyIsBoundReports)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/PropertyIsBoundReports.as"),
			TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBoundReport, int, Current, int, Max);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyBoundReportListener : UObject\n")
			TEXT("{\n")
			TEXT("    UFUNCTION()\n")
			TEXT("    void OnHealth(int NewCurrent, int NewMax)\n")
			TEXT("    {\n")
			TEXT("    }\n")
			TEXT("}\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyBoundReportOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FOnBoundReport Ev;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    UDelegatePropertyBoundReportListener@ Target;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    int Before;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    int AfterBind;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    int AfterClear;\n")
			TEXT("    void Probe()\n")
			TEXT("    {\n")
			TEXT("        if (Ev.IsBound()) { Before = 1; } else { Before = 0; }\n")
			TEXT("        Ev.AddDynamic(Target, FunctionName);\n")
			TEXT("        if (Ev.IsBound()) { AfterBind = 1; } else { AfterBind = 0; }\n")
			TEXT("        Ev.Clear();\n")
			TEXT("        if (Ev.IsBound()) { AfterClear = 1; } else { AfterClear = 0; }\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* ListenerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyBoundReportListener"));
		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyBoundReportOwner"));
		ASSERT_THAT(IsNotNull(ListenerClass, TEXT("listener class")));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));

		UObject* Listener = NewObject<UObject>(GetTransientPackage(), ListenerClass);
		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Listener, TEXT("listener")));
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteListenerTarget(Owner, OwnerClass, Listener, Error), *Error));

		asIScriptFunction* Probe = DelegatePropertyTests::FindOwnerMethod(
			InModules[0]->ScriptModule,
			"UDelegatePropertyBoundReportOwner",
			"DelegatePropertyBoundReportOwner",
			"Probe");
		ASSERT_THAT(IsNotNull(Probe, TEXT("Probe")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::CallScriptMethod(
			Engine->GetScriptEngine(), Probe, Owner, Error), *Error));
		ASSERT_THAT(AreEqual(0, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("Before")), TEXT("IsBound before bind")));
		ASSERT_THAT(AreEqual(1, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("AfterBind")), TEXT("IsBound after AddDynamic")));
		ASSERT_THAT(AreEqual(0, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("AfterClear")), TEXT("IsBound after Clear")));
	}

	TEST_METHOD(AddDynamicThisFires)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/AddDynamicThisFires.as"),
			TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnThisHealth, int, Current, int, Max);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyThisOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FOnThisHealth Ev;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
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
			TEXT("    void BindAndFire()\n")
			TEXT("    {\n")
			TEXT("        Ev.AddDynamic(this, FunctionName);\n")
			TEXT("        Ev.Broadcast(100, 75);\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyThisOwner"));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));
		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteFunctionName(Owner, OwnerClass, TEXT("OnHealth"), Error), *Error));

		asIScriptFunction* BindAndFire = DelegatePropertyTests::FindOwnerMethod(
			InModules[0]->ScriptModule,
			"UDelegatePropertyThisOwner",
			"DelegatePropertyThisOwner",
			"BindAndFire");
		ASSERT_THAT(IsNotNull(BindAndFire, TEXT("BindAndFire")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::CallScriptMethod(
			Engine->GetScriptEngine(), BindAndFire, Owner, Error), *Error));
		ASSERT_THAT(AreEqual(100, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("Current")), TEXT("this Current")));
		ASSERT_THAT(AreEqual(75, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("Max")), TEXT("this Max")));
	}

	TEST_METHOD(ExecuteIfBoundUnboundQuiet)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/ExecuteIfBoundUnboundQuiet.as"),
			TEXT("DECLARE_DYNAMIC_DELEGATE_OneParam(FPropertyQuietAdd, int, Value);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyQuietOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FPropertyQuietAdd Handler;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    int Current;\n")
			TEXT("    UFUNCTION()\n")
			TEXT("    void AddValue(int Value)\n")
			TEXT("    {\n")
			TEXT("        Current = Value;\n")
			TEXT("    }\n")
			TEXT("    void Probe()\n")
			TEXT("    {\n")
			TEXT("        Handler.ExecuteIfBound(42);\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyQuietOwner"));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));
		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteIntProperty(Owner, TEXT("Current"), 7, Error), *Error));

		asIScriptFunction* Probe = DelegatePropertyTests::FindOwnerMethod(
			InModules[0]->ScriptModule,
			"UDelegatePropertyQuietOwner",
			"DelegatePropertyQuietOwner",
			"Probe");
		ASSERT_THAT(IsNotNull(Probe, TEXT("Probe")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::CallScriptMethod(
			Engine->GetScriptEngine(), Probe, Owner, Error), *Error));
		ASSERT_THAT(AreEqual(7, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("Current")), TEXT("unbound ExecuteIfBound Current")));
	}

	TEST_METHOD(RemoveAllStopsBroadcast)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/RemoveAllStopsBroadcast.as"),
			TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRemoveAllHealth, int, Current, int, Max);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyRemoveAllListener : UObject\n")
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
			TEXT("class UDelegatePropertyRemoveAllOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FOnRemoveAllHealth Ev;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    UDelegatePropertyRemoveAllListener@ Target;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
			TEXT("    void BindRemoveAllAndFire()\n")
			TEXT("    {\n")
			TEXT("        Ev.AddDynamic(Target, FunctionName);\n")
			TEXT("        Ev.RemoveAll();\n")
			TEXT("        Ev.Broadcast(100, 75);\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* ListenerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyRemoveAllListener"));
		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyRemoveAllOwner"));
		ASSERT_THAT(IsNotNull(ListenerClass, TEXT("listener class")));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));

		UObject* Listener = NewObject<UObject>(GetTransientPackage(), ListenerClass);
		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Listener, TEXT("listener")));
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteListenerTarget(Owner, OwnerClass, Listener, Error), *Error));

		asIScriptFunction* BindRemoveAllAndFire = DelegatePropertyTests::FindOwnerMethod(
			InModules[0]->ScriptModule,
			"UDelegatePropertyRemoveAllOwner",
			"DelegatePropertyRemoveAllOwner",
			"BindRemoveAllAndFire");
		ASSERT_THAT(IsNotNull(BindRemoveAllAndFire, TEXT("BindRemoveAllAndFire")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::CallScriptMethod(
			Engine->GetScriptEngine(), BindRemoveAllAndFire, Owner, Error), *Error));
		ASSERT_THAT(AreEqual(0, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Current")), TEXT("after RemoveAll Current")));
	}

	TEST_METHOD(BindMissingUFunctionRejected)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/BindMissingUFunctionRejected.as"),
			TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMissingHealth, int, Current, int, Max);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyMissingOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FOnMissingHealth Ev;\n")
			TEXT("    void MissingHealth(int NewCurrent, int NewMax)\n")
			TEXT("    {\n")
			TEXT("    }\n")
			TEXT("    void Bind()\n")
			TEXT("    {\n")
			TEXT("        Ev.AddDynamic(MissingHealth);\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));

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
		ASSERT_THAT(IsTrue(bFailed, *FString::Printf(TEXT("missing bind accepted compile=%d diagnostics=%s"), int32(Result), *Combined)));
		ASSERT_THAT(IsTrue(Combined.Contains(TEXT("bind-requires-ufunction")), *Combined));
	}

	TEST_METHOD(RetValWriteback)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/RetValWriteback.as"),
			TEXT("DECLARE_DYNAMIC_DELEGATE_RetVal(int, FPropertyGet);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyRetValOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FPropertyGet Handler;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    UDelegatePropertyRetValOwner@ Target;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    int Result;\n")
			TEXT("    UFUNCTION()\n")
			TEXT("    int GetValue()\n")
			TEXT("    {\n")
			TEXT("        return 42;\n")
			TEXT("    }\n")
			TEXT("    void BindAndExecute()\n")
			TEXT("    {\n")
			TEXT("        Handler.BindUFunction(Target, FunctionName);\n")
			TEXT("        Result = Handler.Execute();\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyRetValOwner"));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));
		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		FObjectProperty* TargetProp = FindFProperty<FObjectProperty>(OwnerClass, TEXT("Target"));
		ASSERT_THAT(IsNotNull(TargetProp, TEXT("Target")));
		TargetProp->SetObjectPropertyValue_InContainer(Owner, Owner);
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteFunctionName(Owner, OwnerClass, TEXT("GetValue"), Error), *Error));

		asIScriptFunction* BindAndExecute = DelegatePropertyTests::FindOwnerMethod(
			InModules[0]->ScriptModule,
			"UDelegatePropertyRetValOwner",
			"DelegatePropertyRetValOwner",
			"BindAndExecute");
		ASSERT_THAT(IsNotNull(BindAndExecute, TEXT("BindAndExecute")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::CallScriptMethod(
			Engine->GetScriptEngine(), BindAndExecute, Owner, Error), *Error));
		ASSERT_THAT(AreEqual(42, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("Result")), TEXT("RetVal Result")));
	}

	TEST_METHOD(FNameParamFires)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/FNameParamFires.as"),
			TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPropertyName, FName, Label);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyNameOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FOnPropertyName Ev;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName Label;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName LastLabel;\n")
			TEXT("    UFUNCTION()\n")
			TEXT("    void OnName(FName NewLabel)\n")
			TEXT("    {\n")
			TEXT("        LastLabel = NewLabel;\n")
			TEXT("    }\n")
			TEXT("    void BindAndFire()\n")
			TEXT("    {\n")
			TEXT("        Ev.AddDynamic(this, FunctionName);\n")
			TEXT("        Ev.Broadcast(Label);\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyNameOwner"));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));
		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteFunctionName(Owner, OwnerClass, TEXT("OnName"), Error), *Error));
		FNameProperty* LabelProp = FindFProperty<FNameProperty>(OwnerClass, TEXT("Label"));
		ASSERT_THAT(IsNotNull(LabelProp, TEXT("Label")));
		LabelProp->SetPropertyValue_InContainer(Owner, FName(TEXT("Health")));

		asIScriptFunction* BindAndFire = DelegatePropertyTests::FindOwnerMethod(
			InModules[0]->ScriptModule,
			"UDelegatePropertyNameOwner",
			"DelegatePropertyNameOwner",
			"BindAndFire");
		ASSERT_THAT(IsNotNull(BindAndFire, TEXT("BindAndFire")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::CallScriptMethod(
			Engine->GetScriptEngine(), BindAndFire, Owner, Error), *Error));
		ASSERT_THAT(IsTrue(
			DelegatePropertyTests::ReadNameProperty(Owner, TEXT("LastLabel")) == FName(TEXT("Health")),
			TEXT("LastLabel")));
	}

	TEST_METHOD(ZeroParamFires)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/ZeroParamFires.as"),
			TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPropertyZero);\n")
			TEXT("UCLASS()\n")
			TEXT("class UDelegatePropertyZeroOwner : UObject\n")
			TEXT("{\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FOnPropertyZero Ev;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    FName FunctionName;\n")
			TEXT("    UPROPERTY()\n")
			TEXT("    int Current;\n")
			TEXT("    UFUNCTION()\n")
			TEXT("    void OnZero()\n")
			TEXT("    {\n")
			TEXT("        Current = 1;\n")
			TEXT("    }\n")
			TEXT("    void BindAndFire()\n")
			TEXT("    {\n")
			TEXT("        Ev.AddDynamic(this, FunctionName);\n")
			TEXT("        Ev.Broadcast();\n")
			TEXT("    }\n")
			TEXT("}\n"),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyZeroOwner"));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));
		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteFunctionName(Owner, OwnerClass, TEXT("OnZero"), Error), *Error));

		asIScriptFunction* BindAndFire = DelegatePropertyTests::FindOwnerMethod(
			InModules[0]->ScriptModule,
			"UDelegatePropertyZeroOwner",
			"DelegatePropertyZeroOwner",
			"BindAndFire");
		ASSERT_THAT(IsNotNull(BindAndFire, TEXT("BindAndFire")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::CallScriptMethod(
			Engine->GetScriptEngine(), BindAndFire, Owner, Error), *Error));
		ASSERT_THAT(AreEqual(1, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("Current")), TEXT("ZeroParam Current")));
	}

	TEST_METHOD(ConvertCallPtrFires)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/ConvertCallPtrFires.as"),
			DelegatePropertyTests::MakeHealthScript(
				TEXT("FOnPropertyConvert"),
				TEXT("UDelegatePropertyConvertListener"),
				TEXT("UDelegatePropertyConvertOwner")),
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* ListenerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyConvertListener"));
		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertyConvertOwner"));
		ASSERT_THAT(IsNotNull(ListenerClass, TEXT("listener class")));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));

		FMulticastInlineDelegateProperty* EvProp = FindFProperty<FMulticastInlineDelegateProperty>(OwnerClass, TEXT("Ev"));
		ASSERT_THAT(IsNotNull(EvProp, TEXT("Ev property")));
		UDelegateFunction* Signature = Cast<UDelegateFunction>(EvProp->SignatureFunction);
		ASSERT_THAT(IsNotNull(Signature, TEXT("published signature")));

		UObject* Listener = NewObject<UObject>(GetTransientPackage(), ListenerClass);
		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Listener, TEXT("listener")));
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteListenerTarget(Owner, OwnerClass, Listener, Error), *Error));

		asIScriptFunction* Bind = DelegatePropertyTests::FindOwnerMethod(
			InModules[0]->ScriptModule,
			"UDelegatePropertyConvertOwner",
			"DelegatePropertyConvertOwner",
			"Bind");
		ASSERT_THAT(IsNotNull(Bind, TEXT("Bind")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::CallScriptMethod(
			Engine->GetScriptEngine(), Bind, Owner, Error), *Error));
		ASSERT_THAT(AreEqual(0, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Current")), TEXT("unfired Current")));

		const asPWORD Slot = *reinterpret_cast<const asPWORD*>(
			reinterpret_cast<const uint8*>(Owner) + EvProp->GetOffset_ForUFunction());
		struct FOnHealthParams
		{
			int32 Current = 0;
			int32 Max = 0;
		};
		FOnHealthParams Params;
		Params.Current = 100;
		Params.Max = 75;
		FAngelscriptDelegateOperations::ProcessCallPtr(Slot, Signature, &Params);
		ASSERT_THAT(AreEqual(100, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Current")), TEXT("ProcessCallPtr Current")));
		ASSERT_THAT(AreEqual(75, DelegatePropertyTests::ReadIntProperty(Listener, TEXT("Max")), TEXT("ProcessCallPtr Max")));
	}

	TEST_METHOD(SoftReloadCopiesCallPtrSlot)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePropertyTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		const FString Script = TEXT(
			"DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPropertySoft, int, NewCurrent, int, NewMax);\n"
			"UCLASS()\n"
			"class UDelegatePropertySoftOwner : UObject\n"
			"{\n"
			"    UPROPERTY()\n"
			"    FOnPropertySoft Ev;\n"
			"    UPROPERTY()\n"
			"    int Current;\n"
			"    UPROPERTY()\n"
			"    FName FunctionName;\n"
			"    UFUNCTION()\n"
			"    void OnHealth(int NewCurrent, int NewMax)\n"
			"    {\n"
			"        Current = NewCurrent;\n"
			"    }\n"
			"    void Bind()\n"
			"    {\n"
			"        Ev.AddDynamic(this, FunctionName);\n"
			"    }\n"
			"    bool Bound()\n"
			"    {\n"
			"        return Ev.IsBound();\n"
			"    }\n"
			"    void Fire()\n"
			"    {\n"
			"        Ev.Broadcast(100, 75);\n"
			"    }\n"
			"}\n");

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/SoftReloadCopiesCallPtrSlot.as"),
			Script,
			InModules, Error), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::Compile(*Engine, InModules, Error), *Error));

		UClass* OwnerClass = DelegatePropertyTests::FindDescribedClass(InModules, TEXT("UDelegatePropertySoftOwner"));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("owner class")));
		ASSERT_THAT(IsNotNull(
			FindFProperty<FMulticastInlineDelegateProperty>(OwnerClass, TEXT("Ev")),
			TEXT("Ev should be FMulticastInlineDelegateProperty")));

		UObject* Owner = NewObject<UObject>(GetTransientPackage(), OwnerClass);
		ASSERT_THAT(IsNotNull(Owner, TEXT("owner")));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteFunctionName(Owner, OwnerClass, TEXT("OnHealth"), Error), *Error));

		asIScriptModule* ScriptModule = InModules[0]->ScriptModule;
		asIScriptEngine* ScriptEngine = Engine->GetScriptEngine();
		auto CallOwner = [&](const char* MethodName) -> bool
		{
			asIScriptFunction* Method = DelegatePropertyTests::FindOwnerMethod(
				ScriptModule, "UDelegatePropertySoftOwner", "DelegatePropertySoftOwner", MethodName);
			if (!Method)
			{
				Error = FString::Printf(TEXT("missing %s"), UTF8_TO_TCHAR(MethodName));
				return false;
			}
			return DelegatePropertyTests::CallScriptMethod(ScriptEngine, Method, Owner, Error);
		};

		ASSERT_THAT(IsTrue(CallOwner("Bind"), *Error));
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteIntProperty(Owner, TEXT("Current"), 7, Error), *Error));
		ASSERT_THAT(AreEqual(7, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("Current")), TEXT("pre-reload Current")));

		TArray<TSharedRef<FAngelscriptModuleDesc>> ReloadModules;
		ASSERT_THAT(IsTrue(DelegatePropertyTests::WriteAndPreprocess(
			TEXT("DelegateProperty/SoftReloadCopiesCallPtrSlot.as"),
			Script,
			ReloadModules, Error), *Error));
		TArray<TSharedRef<FAngelscriptModuleDesc>> ReloadOut;
		const ECompileResult ReloadResult = Engine->CompileModules(
			ECompileType::SoftReloadOnly,
			ReloadModules,
			ReloadOut);
		ASSERT_THAT(IsTrue(
			ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("SoftReload failed: %d compileError=%d diagnostics=%s"),
				int32(ReloadResult),
				ReloadModules[0]->bCompileError ? 1 : 0,
				*Engine->FormatDiagnostics())));
		ASSERT_THAT(IsFalse(ReloadModules[0]->bCompileError, TEXT("SoftReload bCompileError")));

		UClass* ClassAfter = DelegatePropertyTests::FindDescribedClass(ReloadModules, TEXT("UDelegatePropertySoftOwner"));
		ASSERT_THAT(IsNotNull(ClassAfter, TEXT("SoftReload UClass")));
		ASSERT_THAT(AreEqual(OwnerClass, ClassAfter, TEXT("SoftReload should keep the same UClass*")));
		ASSERT_THAT(AreEqual(7, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("Current")), TEXT("post-reload Current")));

		asIScriptModule* ReloadedModule = ReloadModules[0]->ScriptModule;
		ASSERT_THAT(IsNotNull(ReloadedModule, TEXT("SoftReload ScriptModule")));
		asIScriptFunction* Bound = DelegatePropertyTests::FindOwnerMethod(
			ReloadedModule,
			"UDelegatePropertySoftOwner",
			"DelegatePropertySoftOwner",
			"Bound");
		ASSERT_THAT(IsNotNull(Bound, TEXT("Bound")));
		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Bound context")));
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(IsTrue(Context->Prepare(Bound) >= 0, TEXT("Bound Prepare")));
		ASSERT_THAT(IsTrue(Context->SetObject(Owner) >= 0, TEXT("Bound SetObject")));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute(), TEXT("Bound Execute")));
		ASSERT_THAT(IsTrue(Context->GetReturnDWord() != 0, TEXT("Ev.IsBound after SoftReload")));

		asIScriptFunction* Fire = DelegatePropertyTests::FindOwnerMethod(
			ReloadedModule,
			"UDelegatePropertySoftOwner",
			"DelegatePropertySoftOwner",
			"Fire");
		ASSERT_THAT(IsNotNull(Fire, TEXT("Fire")));
		asIScriptContext* FireContext = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(FireContext, TEXT("Fire context")));
		ON_SCOPE_EXIT { FireContext->Release(); };
		ASSERT_THAT(IsTrue(FireContext->Prepare(Fire) >= 0, TEXT("Fire Prepare")));
		ASSERT_THAT(IsTrue(FireContext->SetObject(Owner) >= 0, TEXT("Fire SetObject")));
		const int FireExecuted = FireContext->Execute();
		const char* FireException = FireContext->GetExceptionString();
		ASSERT_THAT(AreEqual(
			asEXECUTION_FINISHED,
			FireExecuted,
			*FString::Printf(
				TEXT("Fire Execute=%d exception=%s"),
				FireExecuted,
				FireException && FireException[0] ? UTF8_TO_TCHAR(FireException) : TEXT("none"))));
		ASSERT_THAT(AreEqual(100, DelegatePropertyTests::ReadIntProperty(Owner, TEXT("Current")), TEXT("post-reload Broadcast Current")));
	}
};

#endif
