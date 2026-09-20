#include "CQTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptDescriptors.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASFunction.h"

#include "StartAngelscriptHeaders.h"
#include "as_module.h"
#include "EndAngelscriptHeaders.h"

#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(ClassGenCall, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static TSharedRef<FAngelscriptModuleDesc> MakeCallModule()
	{
		TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
		Module->ModuleName = TEXT("ClassGen.Call");
		FAngelscriptModuleDesc::FCodeSection Section;
		Section.AbsoluteFilename = TEXT("ClassGen.Call.as");
		Section.RelativeFilename = TEXT("ClassGen.Call.as");
		Section.Code = TEXT(
			"class ClassGenCallActor\n"
			"{\n"
			"	UFUNCTION()\n"
			"	int GetValue() { return 7; }\n"
			"}\n");
		Module->Code.Add(MoveTemp(Section));

		TSharedRef<FAngelscriptClassDesc> Actor = MakeShared<FAngelscriptClassDesc>();
		Actor->ClassName = TEXT("ClassGenCallActor");
		Actor->SuperClass = TEXT("UObject");
		Actor->bSuperIsCodeClass = true;
		Actor->CodeSuperClass = UObject::StaticClass();

		TSharedRef<FAngelscriptFunctionDesc> GetValue = MakeShared<FAngelscriptFunctionDesc>();
		GetValue->FunctionName = TEXT("GetValue");
		GetValue->ScriptFunctionName = TEXT("GetValue");
		GetValue->bBlueprintCallable = true;
		Actor->Methods.Add(GetValue);
		Module->Classes.Add(Actor);
		return Module;
	}

	static TUniquePtr<FAngelscriptEngine> MakeHostEngine(FString& OutDiagnostic)
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

public:
	TEST_METHOD(InitialProcessEventReturnsValue)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("ClassGen needs a script package")));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		InModules.Add(MakeCallModule());
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		const ECompileResult Result = Engine->CompileModules(ECompileType::Initial, InModules, OutModules);
		const TSharedRef<FAngelscriptModuleDesc>& Module = InModules[0];
		ASSERT_THAT(IsTrue(Result == ECompileResult::FullyHandled || Result == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("CompileModules Initial failed: %d compileError=%d"),
				int32(Result), Module->bCompileError ? 1 : 0)));
		ASSERT_THAT(IsFalse(Module->bCompileError, TEXT("Initial compile should not set bCompileError")));
		ASSERT_THAT(IsNotNull(Module->ScriptModule, TEXT("Initial compile should attach an asCModule shell")));

		asITypeInfo* ActorType = Module->ScriptModule->GetTypeInfoByName("ClassGenCallActor");
		ASSERT_THAT(IsNotNull(ActorType, TEXT("ClassGenCallActor TypeInfo")));
		UASClass* ActorClass = Cast<UASClass>(static_cast<UObject*>(ActorType->GetUserData()));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("UCLASS UserData")));
		ASSERT_THAT(IsTrue(ActorClass->ScriptTypePtr == ActorType, TEXT("UASClass.ScriptTypePtr")));

		ASSERT_THAT(IsTrue(Module->Classes.Num() == 1));
		ASSERT_THAT(IsTrue(Module->Classes[0]->Methods.Num() == 1));
		UASFunction* GetValue = Cast<UASFunction>(Module->Classes[0]->Methods[0]->Function);
		ASSERT_THAT(IsNotNull(GetValue, TEXT("UASFunction on Method desc")));
		ASSERT_THAT(IsNotNull(GetValue->ScriptFunction, TEXT("UASFunction.ScriptFunction")));

		UObject* Actor = NewObject<UObject>(Engine->GetPackageInstance(), ActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("NewObject ClassGenCallActor")));
		UFunction* Function = ActorClass->FindFunctionByName(TEXT("GetValue"));
		ASSERT_THAT(IsNotNull(Function, TEXT("FindFunctionByName GetValue")));
		int32 ReturnValue = 0;
		Actor->ProcessEvent(Function, &ReturnValue);
		ASSERT_THAT(IsTrue(ReturnValue == 7,
			*FString::Printf(TEXT("ProcessEvent GetValue returned %d"), ReturnValue)));
	}
};

#endif
