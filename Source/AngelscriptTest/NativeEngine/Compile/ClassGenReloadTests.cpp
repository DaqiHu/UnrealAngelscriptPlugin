#include "CQTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptDescriptors.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"

#include "StartAngelscriptHeaders.h"
#include "as_module.h"
#include "as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

#include "UObject/Class.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(ClassGenReload, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static TSharedRef<FAngelscriptModuleDesc> MakeClassModule(const TCHAR* ModuleName, const TCHAR* ClassName)
	{
		TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
		Module->ModuleName = ModuleName;
		FAngelscriptModuleDesc::FCodeSection Section;
		Section.AbsoluteFilename = FString(ModuleName) + TEXT(".as");
		Section.RelativeFilename = Section.AbsoluteFilename;
		Section.Code = FString::Printf(TEXT("class %s {}\n"), ClassName);
		Module->Code.Add(MoveTemp(Section));

		TSharedRef<FAngelscriptClassDesc> ClassDesc = MakeShared<FAngelscriptClassDesc>();
		ClassDesc->ClassName = ClassName;
		ClassDesc->SuperClass = TEXT("UObject");
		ClassDesc->bSuperIsCodeClass = true;
		ClassDesc->CodeSuperClass = UObject::StaticClass();
		Module->Classes.Add(ClassDesc);
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

	static TSharedRef<FAngelscriptModuleDesc> MakeReloadModule(
		const TCHAR* Token,
		const TCHAR* Code)
	{
		TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
		Module->ModuleName = FString::Printf(TEXT("ClassGen.Reload.%s"), Token);
		FAngelscriptModuleDesc::FCodeSection Section;
		Section.AbsoluteFilename = Module->ModuleName + TEXT(".as");
		Section.RelativeFilename = Section.AbsoluteFilename;
		Section.Code = Code;
		Module->Code.Add(MoveTemp(Section));

		TSharedRef<FAngelscriptClassDesc> Actor = MakeShared<FAngelscriptClassDesc>();
		Actor->ClassName = FString::Printf(TEXT("ClassGenReload%sActor"), Token);
		Actor->SuperClass = TEXT("UObject");
		Actor->bSuperIsCodeClass = true;
		Actor->CodeSuperClass = UObject::StaticClass();
		Module->Classes.Add(Actor);

		TSharedRef<FAngelscriptClassDesc> Item = MakeShared<FAngelscriptClassDesc>();
		Item->ClassName = FString::Printf(TEXT("ClassGenReload%sItem"), Token);
		Item->bIsStruct = true;
		Module->Classes.Add(Item);

		TSharedRef<FAngelscriptEnumDesc> Color = MakeShared<FAngelscriptEnumDesc>();
		Color->EnumName = FString::Printf(TEXT("ClassGenReload%sColor"), Token);
		Module->Enums.Add(Color);
		return Module;
	}

	static asITypeInfo* FindType(asCModule* ScriptModule, const TCHAR* Name)
	{
		return ScriptModule ? ScriptModule->GetTypeInfoByName(TCHAR_TO_ANSI(Name)) : nullptr;
	}

	static bool CompileInitialSetup(FAngelscriptEngine& Engine, const TCHAR* Token)
	{
		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		InModules.Add(MakeReloadModule(
			Token,
			*FString::Printf(
				TEXT("class ClassGenReload%sActor { ClassGenReload%sActor() {} int GetValue() { return 1; } }\n")
				TEXT("struct ClassGenReload%sItem { int Value; }\n")
				TEXT("enum ClassGenReload%sColor { Red, Blue }\n"),
				Token, Token, Token, Token)));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		const ECompileResult Result = Engine.CompileModules(ECompileType::Initial, InModules, OutModules);
		return (Result == ECompileResult::FullyHandled || Result == ECompileResult::PartiallyHandled)
			&& !InModules[0]->bCompileError
			&& InModules[0]->ScriptModule != nullptr;
	}

public:
	TEST_METHOD(InitialRegistersPerFileDefinitionSets)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("ClassGen needs a script package")));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		InModules.Add(MakeClassModule(TEXT("ClassGen.Reload.Alpha"), TEXT("ClassGenReloadAlpha")));
		InModules.Add(MakeClassModule(TEXT("ClassGen.Reload.Beta"), TEXT("ClassGenReloadBeta")));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		const ECompileResult Result = Engine->CompileModules(ECompileType::Initial, InModules, OutModules);
		ASSERT_THAT(IsTrue(Result == ECompileResult::FullyHandled || Result == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("CompileModules Initial failed: %d"), int32(Result))));
		ASSERT_THAT(IsFalse(InModules[0]->bCompileError, TEXT("Alpha compileError")));
		ASSERT_THAT(IsFalse(InModules[1]->bCompileError, TEXT("Beta compileError")));
		ASSERT_THAT(IsNotNull(InModules[0]->ScriptModule, TEXT("Alpha ScriptModule")));
		ASSERT_THAT(IsNotNull(InModules[1]->ScriptModule, TEXT("Beta ScriptModule")));

		asITypeInfo* AlphaType = FindType(InModules[0]->ScriptModule, TEXT("ClassGenReloadAlpha"));
		asITypeInfo* BetaType = FindType(InModules[1]->ScriptModule, TEXT("ClassGenReloadBeta"));
		ASSERT_THAT(IsNotNull(AlphaType));
		ASSERT_THAT(IsNotNull(BetaType));
		ASSERT_THAT(IsNotNull(Cast<UASClass>(static_cast<UObject*>(AlphaType->GetUserData())), TEXT("Alpha UCLASS")));
		ASSERT_THAT(IsNotNull(Cast<UASClass>(static_cast<UObject*>(BetaType->GetUserData())), TEXT("Beta UCLASS")));

		const asCDefinitions* AlphaSet = static_cast<asCTypeInfo*>(AlphaType)->GetDefinitions();
		const asCDefinitions* BetaSet = static_cast<asCTypeInfo*>(BetaType)->GetDefinitions();
		ASSERT_THAT(IsNotNull(AlphaSet, TEXT("Alpha definition set")));
		ASSERT_THAT(IsNotNull(BetaSet, TEXT("Beta definition set")));
		ASSERT_THAT(IsTrue(AlphaSet != BetaSet, TEXT("Each preprocessor module must own its asCDefinitions")));
	}

	TEST_METHOD(FullReloadUpdatesClassStructEnum)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("ClassGen needs a script package")));
		FAngelscriptEngineScope Scope(*Engine);
		ASSERT_THAT(IsTrue(CompileInitialSetup(*Engine, TEXT("Full")), TEXT("Initial setup")));

		TArray<TSharedRef<FAngelscriptModuleDesc>> Replacement;
		TSharedRef<FAngelscriptModuleDesc> ReplacementModule = MakeReloadModule(
			TEXT("Full"),
			TEXT("class ClassGenReloadFullActor { ClassGenReloadFullActor() {} int Extra; int GetValue() { return 1; } }\n")
			TEXT("struct ClassGenReloadFullItem { int Value; int Extra; }\n")
			TEXT("enum ClassGenReloadFullColor { Red, Blue }\n"));
		auto AddIntProperty = [](const TSharedRef<FAngelscriptClassDesc>& ClassDesc, const TCHAR* Name)
		{
			TSharedRef<FAngelscriptPropertyDesc> Property = MakeShared<FAngelscriptPropertyDesc>();
			Property->PropertyName = Name;
			Property->LiteralType = TEXT("int");
			ClassDesc->Properties.Add(Property);
		};
		AddIntProperty(ReplacementModule->Classes[0], TEXT("Extra"));
		AddIntProperty(ReplacementModule->Classes[1], TEXT("Extra"));
		Replacement.Add(ReplacementModule);
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		const ECompileResult Result = Engine->CompileModules(ECompileType::FullReload, Replacement, OutModules);
		const TSharedRef<FAngelscriptModuleDesc>& Module = Replacement[0];
		ASSERT_THAT(IsTrue(Result == ECompileResult::FullyHandled || Result == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("FullReload failed: %d compileError=%d"),
				int32(Result), Module->bCompileError ? 1 : 0)));
		ASSERT_THAT(IsFalse(Module->bCompileError, TEXT("FullReload compileError")));
		ASSERT_THAT(IsNotNull(Module->ScriptModule, TEXT("FullReload ScriptModule")));

		asITypeInfo* ActorType = FindType(Module->ScriptModule, TEXT("ClassGenReloadFullActor"));
		asITypeInfo* ItemType = FindType(Module->ScriptModule, TEXT("ClassGenReloadFullItem"));
		asITypeInfo* ColorType = FindType(Module->ScriptModule, TEXT("ClassGenReloadFullColor"));
		ASSERT_THAT(IsNotNull(ActorType));
		ASSERT_THAT(IsNotNull(ItemType));
		ASSERT_THAT(IsNotNull(ColorType));
		UASClass* ActorClass = Cast<UASClass>(static_cast<UObject*>(ActorType->GetUserData()));
		UASStruct* ItemStruct = Cast<UASStruct>(static_cast<UObject*>(ItemType->GetUserData()));
		UEnum* ColorEnum = Cast<UEnum>(static_cast<UObject*>(ColorType->GetUserData()));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("UCLASS UserData")));
		ASSERT_THAT(IsNotNull(ItemStruct, TEXT("USTRUCT UserData")));
		ASSERT_THAT(IsNotNull(ColorEnum, TEXT("UENUM UserData")));
		ASSERT_THAT(IsTrue(ActorClass->ScriptTypePtr == ActorType, TEXT("UASClass.ScriptTypePtr")));
		ASSERT_THAT(IsTrue(ItemStruct->ScriptType == ItemType, TEXT("UASStruct.ScriptType")));
		ASSERT_THAT(IsNotNull(Module->Enums[0]->Enum, TEXT("EnumDesc.Enum")));
	}

	TEST_METHOD(SoftReloadOnlyUpdatesUserData)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("ClassGen needs a script package")));
		FAngelscriptEngineScope Scope(*Engine);
		ASSERT_THAT(IsTrue(CompileInitialSetup(*Engine, TEXT("Soft")), TEXT("Initial setup")));

		TArray<TSharedRef<FAngelscriptModuleDesc>> Replacement;
		Replacement.Add(MakeReloadModule(
			TEXT("Soft"),
			TEXT("class ClassGenReloadSoftActor { ClassGenReloadSoftActor() {} int GetValue() { return 2; } }\n")
			TEXT("struct ClassGenReloadSoftItem { int Value; }\n")
			TEXT("enum ClassGenReloadSoftColor { Red, Blue }\n")));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		const ECompileResult Result = Engine->CompileModules(ECompileType::SoftReloadOnly, Replacement, OutModules);
		const TSharedRef<FAngelscriptModuleDesc>& Module = Replacement[0];
		ASSERT_THAT(IsTrue(Result == ECompileResult::FullyHandled || Result == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("SoftReloadOnly failed: %d compileError=%d"),
				int32(Result), Module->bCompileError ? 1 : 0)));
		ASSERT_THAT(IsFalse(Module->bCompileError, TEXT("SoftReloadOnly compileError")));
		ASSERT_THAT(IsNotNull(Module->ScriptModule, TEXT("SoftReloadOnly ScriptModule")));
		asITypeInfo* ActorType = FindType(Module->ScriptModule, TEXT("ClassGenReloadSoftActor"));
		ASSERT_THAT(IsNotNull(ActorType));
		ASSERT_THAT(IsNotNull(Cast<UASClass>(static_cast<UObject*>(ActorType->GetUserData())), TEXT("Soft UCLASS")));
	}

	TEST_METHOD(FailedReloadKeepsLastGeneration)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("ClassGen needs a script package")));
		FAngelscriptEngineScope Scope(*Engine);
		ASSERT_THAT(IsTrue(CompileInitialSetup(*Engine, TEXT("Keep")), TEXT("Initial setup")));

		TArray<TSharedRef<FAngelscriptModuleDesc>> Broken;
		Broken.Add(MakeReloadModule(TEXT("Keep"), TEXT("this is not valid angelscript\n")));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		TestRunner->AddExpectedError(
			TEXT("stage-3-failed"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		(void)Engine->CompileModules(ECompileType::FullReload, Broken, OutModules);
		ASSERT_THAT(IsTrue(Broken[0]->bCompileError, TEXT("Broken FullReload must set bCompileError")));
		asIScriptModule* LastGood = Engine->GetScriptEngine()->GetModule("ClassGen.Reload.Keep");
		ASSERT_THAT(IsNotNull(LastGood, TEXT("Last-good asCModule must stay lookup-able")));
		asITypeInfo* ActorType = LastGood->GetTypeInfoByName("ClassGenReloadKeepActor");
		ASSERT_THAT(IsNotNull(ActorType));
		ASSERT_THAT(IsNotNull(Cast<UASClass>(static_cast<UObject*>(ActorType->GetUserData())), TEXT("Last-good UCLASS")));
	}
};

#endif
