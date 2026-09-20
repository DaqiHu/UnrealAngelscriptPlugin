#include "CQTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptDescriptors.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASFunction.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "StartAngelscriptHeaders.h"
#include "as_module.h"
#include "EndAngelscriptHeaders.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(ClassGenUClassReload, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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

	static bool WriteAndPreprocess(
		const TCHAR* RelativeFilename,
		const FString& Script,
		TArray<TSharedRef<FAngelscriptModuleDesc>>& OutModules,
		FString& OutError)
	{
		const FString AbsoluteFilename = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			RelativeFilename);
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

	static int32 ProcessEventGetVersion(UClass* Class, UObject* Object)
	{
		UFunction* Function = Class->FindFunctionByName(TEXT("GetVersion"));
		if (Function == nullptr || Object == nullptr)
		{
			return -1;
		}
		int32 ReturnValue = 0;
		Object->ProcessEvent(Function, &ReturnValue);
		return ReturnValue;
	}

	static UClass* FindDescribedClass(
		const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules,
		const TCHAR* ClassName)
	{
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
		{
			for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : Module->Classes)
			{
				if (ClassDesc->ClassName == ClassName)
				{
					return ClassDesc->Class;
				}
			}
		}
		return nullptr;
	}

	static UBlueprint* CreateTransientBlueprintChild(UClass* ParentClass, const TCHAR* Suffix)
	{
		const FString PackagePath = FString::Printf(
			TEXT("/Temp/ClassGenUClassReload_%s_%s"),
			Suffix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (BlueprintPackage == nullptr)
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
			TEXT("ClassGenUClassReload"));
	}

public:
	TEST_METHOD(SoftReloadProcessEventUpdatesBody)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("ClassGen needs a script package")));
		FAngelscriptEngineScope Scope(*Engine);

		const FString ScriptV1 = TEXT(
			"UCLASS()\n"
			"class ClassGenUClassReloadSoftActor : UObject\n"
			"{\n"
			"	UPROPERTY()\n"
			"	int Version;\n"
			"	default Version = 1;\n"
			"	UFUNCTION()\n"
			"	int GetVersion()\n"
			"	{\n"
			"		return Version;\n"
			"	}\n"
			"}\n");
		const FString ScriptV2 = TEXT(
			"UCLASS()\n"
			"class ClassGenUClassReloadSoftActor : UObject\n"
			"{\n"
			"	UPROPERTY()\n"
			"	int Version;\n"
			"	default Version = 1;\n"
			"	UFUNCTION()\n"
			"	int GetVersion()\n"
			"	{\n"
			"		return Version + 1;\n"
			"	}\n"
			"}\n");

		TArray<TSharedRef<FAngelscriptModuleDesc>> InitialModules;
		FString PreprocessError;
		ASSERT_THAT(IsTrue(
			WriteAndPreprocess(TEXT("ClassGenUClassReloadSoft.as"), ScriptV1, InitialModules, PreprocessError),
			*PreprocessError));

		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		const ECompileResult InitialResult = Engine->CompileModules(
			ECompileType::Initial,
			InitialModules,
			OutModules);
		ASSERT_THAT(IsTrue(
			InitialResult == ECompileResult::FullyHandled || InitialResult == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("Initial compile failed: %d"), int32(InitialResult))));
		ASSERT_THAT(IsFalse(InitialModules[0]->bCompileError, TEXT("Initial bCompileError")));
		ASSERT_THAT(IsNotNull(InitialModules[0]->ScriptModule, TEXT("Initial ScriptModule")));
		ASSERT_THAT(IsTrue(InitialModules[0]->Classes.Num() == 1, TEXT("preprocessor should fill one class")));
		ASSERT_THAT(IsTrue(InitialModules[0]->Classes[0]->Methods.Num() >= 1, TEXT("preprocessor should fill GetVersion")));

		UClass* ClassV1 = nullptr;
		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : InitialModules[0]->Classes)
		{
			if (ClassDesc->ClassName == TEXT("ClassGenUClassReloadSoftActor"))
			{
				ClassV1 = ClassDesc->Class;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ClassV1, TEXT("Initial UClass")));
		ASSERT_THAT(IsNotNull(Cast<UASClass>(ClassV1), TEXT("Initial UClass should be UASClass")));
		ASSERT_THAT(IsTrue(
			ClassV1->GetName().Contains(TEXT("ClassGenUClassReloadSoftActor")),
			*FString::Printf(TEXT("Initial UClass name was %s"), *ClassV1->GetName())));
		TSharedPtr<FAngelscriptFunctionDesc> GetVersionDesc;
		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : InitialModules[0]->Classes)
		{
			if (ClassDesc->ClassName != TEXT("ClassGenUClassReloadSoftActor"))
			{
				continue;
			}
			for (const TSharedRef<FAngelscriptFunctionDesc>& Method : ClassDesc->Methods)
			{
				if (Method->FunctionName == TEXT("GetVersion"))
				{
					GetVersionDesc = Method;
					break;
				}
			}
		}
		ASSERT_THAT(IsTrue(GetVersionDesc.IsValid(), TEXT("GetVersion function desc")));
		ASSERT_THAT(IsTrue(
			GetVersionDesc->ReturnType.IsValid(),
			TEXT("GetVersion ReturnType should resolve from the script function")));
		UFunction* GetVersionFn = ClassV1->FindFunctionByName(TEXT("GetVersion"));
		ASSERT_THAT(IsNotNull(GetVersionFn, TEXT("Initial UClass should expose GetVersion")));
		ASSERT_THAT(IsNotNull(
			GetVersionDesc->Function,
			TEXT("ClassGen should attach UASFunction to GetVersion")));
		ASSERT_THAT(IsNotNull(
			Cast<UASFunction_DWordReturn>(GetVersionFn),
			*FString::Printf(TEXT("GetVersion UFunction class %s"), *GetVersionFn->GetClass()->GetName())));
		UASClass* SoftASClass = Cast<UASClass>(ClassV1);
		ASSERT_THAT(IsNotNull(SoftASClass->DefaultsFunction,
			TEXT("UASClass should bind __InitDefaults from default Version = 1")));

		UObject* Actor = NewObject<UObject>(Engine->GetPackageInstance(), ClassV1);
		ASSERT_THAT(IsNotNull(Actor, TEXT("NewObject SoftActor")));
		const FIntProperty* VersionProp = FindFProperty<FIntProperty>(ClassV1, TEXT("Version"));
		ASSERT_THAT(IsNotNull(VersionProp, TEXT("Initial UClass should expose Version")));
		const int32 VersionOnObject = VersionProp->GetPropertyValue_InContainer(Actor);
		ASSERT_THAT(AreEqual(
			1,
			VersionOnObject,
			*FString::Printf(TEXT("NewObject should apply default Version=1, got %d offset=%d size=%d"),
				VersionOnObject,
				VersionProp->GetOffset_ForUFunction(),
				ClassV1->GetPropertiesSize())));
		const int32 InitialVersion = ProcessEventGetVersion(ClassV1, Actor);
		ASSERT_THAT(AreEqual(
			1,
			InitialVersion,
			*FString::Printf(TEXT("Initial ProcessEvent should return 1, got %d"), InitialVersion)));

		TArray<TSharedRef<FAngelscriptModuleDesc>> ReloadModules;
		ASSERT_THAT(IsTrue(
			WriteAndPreprocess(TEXT("ClassGenUClassReloadSoft.as"), ScriptV2, ReloadModules, PreprocessError),
			*PreprocessError));

		TArray<TSharedRef<FAngelscriptModuleDesc>> ReloadOut;
		const ECompileResult ReloadResult = Engine->CompileModules(
			ECompileType::SoftReloadOnly,
			ReloadModules,
			ReloadOut);
		ASSERT_THAT(IsTrue(
			ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("SoftReload failed: %d compileError=%d"),
				int32(ReloadResult), ReloadModules[0]->bCompileError ? 1 : 0)));
		ASSERT_THAT(IsFalse(ReloadModules[0]->bCompileError, TEXT("SoftReload bCompileError")));

		UClass* ClassAfter = nullptr;
		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : ReloadModules[0]->Classes)
		{
			if (ClassDesc->ClassName == TEXT("ClassGenUClassReloadSoftActor"))
			{
				ClassAfter = ClassDesc->Class;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ClassAfter, TEXT("SoftReload UClass")));
		ASSERT_THAT(AreEqual(ClassV1, ClassAfter, TEXT("SoftReload should keep the same UClass*")));
		ASSERT_THAT(AreEqual(2, ProcessEventGetVersion(ClassAfter, Actor), TEXT("SoftReload ProcessEvent should return 2")));
	}

	TEST_METHOD(SoftReloadBlueprintChildCallsNewBody)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("ClassGen needs a script package")));
		FAngelscriptEngineScope Scope(*Engine);

		const FString ScriptV1 = TEXT(
			"UCLASS()\n"
			"class ClassGenUClassReloadBpParent : UObject\n"
			"{\n"
			"	UPROPERTY()\n"
			"	int Version;\n"
			"	default Version = 30;\n"
			"	UFUNCTION()\n"
			"	int GetVersion()\n"
			"	{\n"
			"		return Version;\n"
			"	}\n"
			"}\n");
		const FString ScriptV2 = TEXT(
			"UCLASS()\n"
			"class ClassGenUClassReloadBpParent : UObject\n"
			"{\n"
			"	UPROPERTY()\n"
			"	int Version;\n"
			"	default Version = 30;\n"
			"	UFUNCTION()\n"
			"	int GetVersion()\n"
			"	{\n"
			"		return Version + 12;\n"
			"	}\n"
			"}\n");

		TArray<TSharedRef<FAngelscriptModuleDesc>> InitialModules;
		FString PreprocessError;
		ASSERT_THAT(IsTrue(
			WriteAndPreprocess(TEXT("ClassGenUClassReloadBp.as"), ScriptV1, InitialModules, PreprocessError),
			*PreprocessError));

		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		const ECompileResult InitialResult = Engine->CompileModules(
			ECompileType::Initial,
			InitialModules,
			OutModules);
		ASSERT_THAT(IsTrue(
			InitialResult == ECompileResult::FullyHandled || InitialResult == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("Initial compile failed: %d"), int32(InitialResult))));
		ASSERT_THAT(IsFalse(InitialModules[0]->bCompileError, TEXT("Initial bCompileError")));

		UClass* ParentV1 = FindDescribedClass(InitialModules, TEXT("ClassGenUClassReloadBpParent"));
		ASSERT_THAT(IsNotNull(ParentV1, TEXT("Initial parent UClass")));
		ASSERT_THAT(IsNotNull(Cast<UASClass>(ParentV1), TEXT("Parent should be UASClass")));

		UBlueprint* Blueprint = CreateTransientBlueprintChild(ParentV1, TEXT("BpParent"));
		ASSERT_THAT(IsNotNull(Blueprint, TEXT("CreateBlueprint child")));
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		UClass* BlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Compiled Blueprint generated class")));
		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(ParentV1), TEXT("Blueprint class should be a child of the script parent")));
		ASSERT_THAT(IsNull(Cast<UASClass>(BlueprintClass), TEXT("Blueprint generated class must not be UASClass")));

		UObject* Child = NewObject<UObject>(Engine->GetPackageInstance(), BlueprintClass);
		ASSERT_THAT(IsNotNull(Child, TEXT("NewObject Blueprint child")));
		ASSERT_THAT(AreEqual(30, ProcessEventGetVersion(BlueprintClass, Child), TEXT("Initial Blueprint ProcessEvent should return 30")));

		TArray<TSharedRef<FAngelscriptModuleDesc>> ReloadModules;
		ASSERT_THAT(IsTrue(
			WriteAndPreprocess(TEXT("ClassGenUClassReloadBp.as"), ScriptV2, ReloadModules, PreprocessError),
			*PreprocessError));
		TArray<TSharedRef<FAngelscriptModuleDesc>> ReloadOut;
		const ECompileResult ReloadResult = Engine->CompileModules(
			ECompileType::SoftReloadOnly,
			ReloadModules,
			ReloadOut);
		ASSERT_THAT(IsTrue(
			ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("SoftReload failed: %d compileError=%d"),
				int32(ReloadResult), ReloadModules[0]->bCompileError ? 1 : 0)));
		ASSERT_THAT(IsFalse(ReloadModules[0]->bCompileError, TEXT("SoftReload bCompileError")));

		UClass* ParentAfter = FindDescribedClass(ReloadModules, TEXT("ClassGenUClassReloadBpParent"));
		ASSERT_THAT(IsNotNull(ParentAfter, TEXT("SoftReload parent UClass")));
		ASSERT_THAT(AreEqual(ParentV1, ParentAfter, TEXT("SoftReload should keep the same parent UClass*")));
		ASSERT_THAT(IsTrue(
			BlueprintClass->IsChildOf(ParentAfter),
			TEXT("Blueprint class should remain a child of the same parent")));
		ASSERT_THAT(IsNull(Cast<UASClass>(BlueprintClass), TEXT("Blueprint class stays non-UASClass after SoftReload")));
		ASSERT_THAT(AreEqual(
			42,
			ProcessEventGetVersion(BlueprintClass, Child),
			TEXT("SoftReload Blueprint ProcessEvent should return 42")));
	}

	TEST_METHOD(FullReloadAddsPropertyProcessEvent)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("ClassGen needs a script package")));
		FAngelscriptEngineScope Scope(*Engine);

		const FString ScriptV1 = TEXT(
			"UCLASS()\n"
			"class ClassGenUClassReloadFullActor : UObject\n"
			"{\n"
			"	UFUNCTION()\n"
			"	int GetVersion()\n"
			"	{\n"
			"		return 1;\n"
			"	}\n"
			"}\n");
		const FString ScriptV2 = TEXT(
			"UCLASS()\n"
			"class ClassGenUClassReloadFullActor : UObject\n"
			"{\n"
			"	UPROPERTY()\n"
			"	int Extra;\n"
			"	UFUNCTION()\n"
			"	int GetVersion()\n"
			"	{\n"
			"		return 1;\n"
			"	}\n"
			"}\n");

		TArray<TSharedRef<FAngelscriptModuleDesc>> InitialModules;
		FString PreprocessError;
		ASSERT_THAT(IsTrue(
			WriteAndPreprocess(TEXT("ClassGenUClassReloadFull.as"), ScriptV1, InitialModules, PreprocessError),
			*PreprocessError));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		const ECompileResult InitialResult = Engine->CompileModules(
			ECompileType::Initial,
			InitialModules,
			OutModules);
		ASSERT_THAT(IsTrue(
			InitialResult == ECompileResult::FullyHandled || InitialResult == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("Initial compile failed: %d"), int32(InitialResult))));
		ASSERT_THAT(IsFalse(InitialModules[0]->bCompileError, TEXT("Initial bCompileError")));

		UClass* ClassV1 = FindDescribedClass(InitialModules, TEXT("ClassGenUClassReloadFullActor"));
		ASSERT_THAT(IsNotNull(ClassV1, TEXT("Initial UClass")));
		UObject* Actor = NewObject<UObject>(Engine->GetPackageInstance(), ClassV1);
		ASSERT_THAT(IsNotNull(Actor, TEXT("NewObject FullActor")));
		ASSERT_THAT(AreEqual(1, ProcessEventGetVersion(ClassV1, Actor), TEXT("Initial ProcessEvent should return 1")));

		TArray<TSharedRef<FAngelscriptModuleDesc>> ReloadModules;
		ASSERT_THAT(IsTrue(
			WriteAndPreprocess(TEXT("ClassGenUClassReloadFull.as"), ScriptV2, ReloadModules, PreprocessError),
			*PreprocessError));
		TArray<TSharedRef<FAngelscriptModuleDesc>> ReloadOut;
		const ECompileResult ReloadResult = Engine->CompileModules(
			ECompileType::FullReload,
			ReloadModules,
			ReloadOut);
		ASSERT_THAT(IsTrue(
			ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("FullReload failed: %d compileError=%d"),
				int32(ReloadResult), ReloadModules[0]->bCompileError ? 1 : 0)));
		ASSERT_THAT(IsFalse(ReloadModules[0]->bCompileError, TEXT("FullReload bCompileError")));

		UClass* ClassAfter = FindDescribedClass(ReloadModules, TEXT("ClassGenUClassReloadFullActor"));
		ASSERT_THAT(IsNotNull(ClassAfter, TEXT("FullReload UClass")));
		ASSERT_THAT(IsNotNull(
			FindFProperty<FIntProperty>(ClassAfter, TEXT("Extra")),
			TEXT("FullReload should expose Extra")));
		UObject* AfterActor = NewObject<UObject>(Engine->GetPackageInstance(), ClassAfter);
		ASSERT_THAT(IsNotNull(AfterActor, TEXT("NewObject after FullReload")));
		ASSERT_THAT(AreEqual(1, ProcessEventGetVersion(ClassAfter, AfterActor), TEXT("FullReload ProcessEvent should return 1")));
	}

	TEST_METHOD(FailedReloadKeepsProcessEvent)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("ClassGen needs a script package")));
		FAngelscriptEngineScope Scope(*Engine);

		const FString ScriptV1 = TEXT(
			"UCLASS()\n"
			"class ClassGenUClassReloadKeepActor : UObject\n"
			"{\n"
			"	UFUNCTION()\n"
			"	int GetVersion()\n"
			"	{\n"
			"		return 1;\n"
			"	}\n"
			"}\n");

		TArray<TSharedRef<FAngelscriptModuleDesc>> InitialModules;
		FString PreprocessError;
		ASSERT_THAT(IsTrue(
			WriteAndPreprocess(TEXT("ClassGenUClassReloadKeep.as"), ScriptV1, InitialModules, PreprocessError),
			*PreprocessError));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		const ECompileResult InitialResult = Engine->CompileModules(
			ECompileType::Initial,
			InitialModules,
			OutModules);
		ASSERT_THAT(IsTrue(
			InitialResult == ECompileResult::FullyHandled || InitialResult == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("Initial compile failed: %d"), int32(InitialResult))));
		ASSERT_THAT(IsFalse(InitialModules[0]->bCompileError, TEXT("Initial bCompileError")));

		UClass* ClassV1 = FindDescribedClass(InitialModules, TEXT("ClassGenUClassReloadKeepActor"));
		ASSERT_THAT(IsNotNull(ClassV1, TEXT("Initial UClass")));
		UObject* Actor = NewObject<UObject>(Engine->GetPackageInstance(), ClassV1);
		ASSERT_THAT(IsNotNull(Actor, TEXT("NewObject KeepActor")));
		ASSERT_THAT(AreEqual(1, ProcessEventGetVersion(ClassV1, Actor), TEXT("Initial ProcessEvent should return 1")));

		TSharedRef<FAngelscriptModuleDesc> Broken = MakeShared<FAngelscriptModuleDesc>();
		Broken->ModuleName = InitialModules[0]->ModuleName;
		FAngelscriptModuleDesc::FCodeSection Section;
		Section.AbsoluteFilename = TEXT("ClassGenUClassReloadKeep.as");
		Section.RelativeFilename = Section.AbsoluteFilename;
		Section.Code = TEXT("this is not valid angelscript\n");
		Broken->Code.Add(MoveTemp(Section));
		TSharedRef<FAngelscriptClassDesc> KeepClass = MakeShared<FAngelscriptClassDesc>();
		KeepClass->ClassName = TEXT("ClassGenUClassReloadKeepActor");
		KeepClass->SuperClass = TEXT("UObject");
		KeepClass->bSuperIsCodeClass = true;
		KeepClass->CodeSuperClass = UObject::StaticClass();
		Broken->Classes.Add(KeepClass);

		TArray<TSharedRef<FAngelscriptModuleDesc>> BrokenModules;
		BrokenModules.Add(Broken);
		TArray<TSharedRef<FAngelscriptModuleDesc>> BrokenOut;
		TestRunner->AddExpectedError(
			TEXT("stage-3-failed"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		(void)Engine->CompileModules(ECompileType::FullReload, BrokenModules, BrokenOut);
		ASSERT_THAT(IsTrue(Broken->bCompileError, TEXT("Broken FullReload must set bCompileError")));

		asIScriptEngine* ScriptEngine = Engine->GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Host script engine")));
		asIScriptModule* LastGood = ScriptEngine->GetModule(TCHAR_TO_ANSI(*InitialModules[0]->ModuleName));
		ASSERT_THAT(IsNotNull(LastGood, TEXT("Last-good module must stay lookup-able")));
		asITypeInfo* ActorType = LastGood->GetTypeInfoByName("ClassGenUClassReloadKeepActor");
		ASSERT_THAT(IsNotNull(ActorType, TEXT("Last-good type")));
		ASSERT_THAT(IsNotNull(
			Cast<UASClass>(static_cast<UObject*>(ActorType->GetUserData())),
			TEXT("Last-good UASClass UserData")));
		ASSERT_THAT(AreEqual(1, ProcessEventGetVersion(ClassV1, Actor), TEXT("Failed reload must keep ProcessEvent 1")));
	}
};

#endif
