#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "CQTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptDelegateReservedParameterBindingTests,
	"Angelscript.TestModule.Bindings.DelegateReservedParameter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(PackageStyleEngineAcceptsReservedUnrealParameterName)
	{
		const FString ProjectRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/DelegateReservedParameter"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));

		FAngelscriptEngineConfig Config;
		Config.bIsEditor = false;
		Config.bDevelopmentMode = false;
		Config.bSkipThreadedInitialize = true;

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [ProjectRoot]()
		{
			return ProjectRoot;
		};
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
		ASSERT_THAT(IsNotNull(
			Engine.Get(),
			TEXT("A package-style Engine must not reject an Unreal delegate whose reflected parameter is named 'float'")));
		if (!Engine.IsValid() || Engine->GetScriptEngine() == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			FString(TEXT("__ASArg0")),
			FAngelscriptType::BuildSafeFunctionArgumentName(
				*Engine->GetScriptEngine(), TEXT("float"), 0),
			TEXT("The target AngelScript tokenizer must reject the lowercase reflected keyword")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Float")),
			FAngelscriptType::BuildSafeFunctionArgumentName(
				*Engine->GetScriptEngine(), TEXT("Float"), 0),
			TEXT("A legal reflected identifier must retain its diagnostic spelling")));
		const TArray<FString> ExistingNames{TEXT("__ASArg0")};
		ASSERT_THAT(AreEqual(
			FString(TEXT("__ASArg0_")),
			FAngelscriptType::BuildSafeFunctionArgumentName(
				*Engine->GetScriptEngine(), TEXT("float"), 0, ExistingNames),
			TEXT("A generated diagnostic identifier must not collide with an existing reflected argument")));

		asITypeInfo* const DelegateType =
			Engine->GetScriptEngine()->GetTypeInfoByName(
				"FTestDynamicDelegateFloat");
		ASSERT_THAT(IsNotNull(
			DelegateType,
			TEXT("The CoreUObject reserved-name delegate fixture should be bound")));
		if (DelegateType != nullptr)
		{
			asIScriptFunction* const Execute =
				DelegateType->GetMethodByName("Execute");
			ASSERT_THAT(IsNotNull(
				Execute,
				TEXT("The reserved Unreal parameter name must be escaped in the AngelScript Execute declaration")));
			if (Execute != nullptr)
			{
				const FString Declaration = UTF8_TO_TCHAR(
					Execute->GetDeclaration(false, false, true, true));
				ASSERT_THAT(IsFalse(
					Declaration.Contains(
						TEXT("float32 float"), ESearchCase::CaseSensitive),
					TEXT("A reflected AngelScript keyword must never be emitted as a raw parameter identifier")));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
