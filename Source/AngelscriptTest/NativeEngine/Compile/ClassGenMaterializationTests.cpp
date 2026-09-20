#include "CQTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptDescriptors.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"

#include "StartAngelscriptHeaders.h"
#include "as_module.h"
#include "as_objecttype.h"
#include "EndAngelscriptHeaders.h"

#include "UObject/Class.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(ClassGenMaterialization, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static TSharedRef<FAngelscriptModuleDesc> MakeJoinModule()
	{
		TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
		Module->ModuleName = TEXT("ClassGen.Join");
		FAngelscriptModuleDesc::FCodeSection Section;
		Section.AbsoluteFilename = TEXT("ClassGen.Join.as");
		Section.RelativeFilename = TEXT("ClassGen.Join.as");
		Section.Code = TEXT(
			"class ClassGenJoinActor {}\n"
			"struct ClassGenJoinItem { int Value; }\n"
			"enum ClassGenJoinColor { Red, Blue }\n");
		Module->Code.Add(MoveTemp(Section));

		TSharedRef<FAngelscriptClassDesc> Actor = MakeShared<FAngelscriptClassDesc>();
		Actor->ClassName = TEXT("ClassGenJoinActor");
		Actor->SuperClass = TEXT("UObject");
		Actor->bSuperIsCodeClass = true;
		Actor->CodeSuperClass = UObject::StaticClass();
		Module->Classes.Add(Actor);

		TSharedRef<FAngelscriptClassDesc> Item = MakeShared<FAngelscriptClassDesc>();
		Item->ClassName = TEXT("ClassGenJoinItem");
		Item->bIsStruct = true;
		Module->Classes.Add(Item);

		TSharedRef<FAngelscriptEnumDesc> Color = MakeShared<FAngelscriptEnumDesc>();
		Color->EnumName = TEXT("ClassGenJoinColor");
		Module->Enums.Add(Color);
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

	static asITypeInfo* FindType(asCModule* ScriptModule, const TCHAR* Name)
	{
		return ScriptModule ? ScriptModule->GetTypeInfoByName(TCHAR_TO_ANSI(Name)) : nullptr;
	}

public:
	TEST_METHOD(InitialMaterializesClassStructEnum)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		ASSERT_THAT(IsNotNull(Engine->GetPackageInstance(), TEXT("ClassGen needs a script package")));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		InModules.Add(MakeJoinModule());
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		const ECompileResult Result = Engine->CompileModules(ECompileType::Initial, InModules, OutModules);
		const TSharedRef<FAngelscriptModuleDesc>& Module = InModules[0];
		ASSERT_THAT(IsTrue(Result == ECompileResult::FullyHandled || Result == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("CompileModules Initial failed: %d compileError=%d"),
				int32(Result), Module->bCompileError ? 1 : 0)));
		ASSERT_THAT(IsFalse(Module->bCompileError, TEXT("Initial compile should not set bCompileError")));
		ASSERT_THAT(IsNotNull(Module->ScriptModule, TEXT("Initial compile should attach an asCModule shell")));

		asITypeInfo* ActorType = FindType(Module->ScriptModule, TEXT("ClassGenJoinActor"));
		asITypeInfo* ItemType = FindType(Module->ScriptModule, TEXT("ClassGenJoinItem"));
		asITypeInfo* ColorType = FindType(Module->ScriptModule, TEXT("ClassGenJoinColor"));
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
};

#endif
