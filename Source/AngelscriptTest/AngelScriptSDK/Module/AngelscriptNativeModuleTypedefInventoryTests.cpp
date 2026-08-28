#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleTypedefInventoryTests,
	"Angelscript.TestModule.AngelScriptSDK.Module.ApiContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static void PrintSource(
		FAutomationTestBase& Test,
		const TCHAR* Id,
		const TCHAR* ModuleName,
		const char* Source)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(
			Test,
			Id,
			ModuleName,
			FString(UTF8_TO_TCHAR(Source != nullptr ? Source : "")));
	}

	static void PrintSource(
		FAutomationTestBase& Test,
		const TCHAR* Id,
		const TCHAR* ModuleName,
		const std::string& Source)
	{
		PrintSource(Test, Id, ModuleName, Source.c_str());
	}

public:
	TEST_METHOD(TypedefInventoryPublishesStableModuleAlias)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-TYPEDEF-INVENTORY-BOUNDS",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Module typedef inventory contract should have a raw engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const std::string TypedefSource = ASTEST_AS_ANSI(R"AS(
			typedef int ModuleAlias;
		)AS");
		PrintSource(
			*TestRunner,
			TEXT("MOD-TYPEDEF-INVENTORY-BOUNDS-PUBLISHED-ALIAS"),
			TEXT("ModuleApiTypedefPublished"),
			TypedefSource);
		FScopedNativeModuleName TypedefScope(Engine, "ModuleApiTypedefPublished");
		asIScriptModule* const TypedefModule =
			ScriptEngine->GetModule(TypedefScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(
			TypedefModule,
			TEXT("Typedef publication should create an isolated module")));
		if (TypedefModule == nullptr)
		{
			return;
		}

		Engine.ResetMessages();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			TypedefModule->AddScriptSection(
				"ModuleApiTypedefPublished.as",
				TypedefSource.c_str()),
			TEXT("Typedef source should be accepted as a script section")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			TypedefModule->Build(),
			TEXT("The canonical declaration model should accept script-level typedef syntax")));
		ASSERT_THAT(AreEqual(
			1,
			static_cast<int32>(TypedefModule->GetTypedefCount()),
			TEXT("Successful typedef build should publish exactly one module alias")));
		asITypeInfo* const PublishedTypedef = TypedefModule->GetTypedefByIndex(0);
		ASSERT_THAT(IsNotNull(
			PublishedTypedef,
			TEXT("Successful typedef build should expose its alias at index zero")));
		if (PublishedTypedef != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("ModuleAlias")),
				FString(UTF8_TO_TCHAR(PublishedTypedef->GetName())),
				TEXT("Published typedef should retain its declared name")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asTYPEID_INT32),
				PublishedTypedef->GetTypedefTypeId(),
				TEXT("Published typedef should retain its aliased primitive type")));
		}

		PrintSource(
			*TestRunner,
			TEXT("MOD-TYPEDEF-INVENTORY-BOUNDS-EMPTY-ZERO"),
			TEXT("ModuleApiTypedefEmpty"),
			ASTEST_AS_ANSI(R"AS(
				// A module without typedef declarations keeps an empty public inventory.
			)AS"));
		FScopedNativeModuleName EmptyScope(Engine, "ModuleApiTypedefEmpty");
		asIScriptModule* const EmptyModule =
			ScriptEngine->GetModule(EmptyScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(
			EmptyModule,
			TEXT("Empty typedef inventory should create an isolated module")));
		if (EmptyModule == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(EmptyModule->GetTypedefCount()),
			TEXT("A module without typedef declarations should keep its inventory empty")));
		ASSERT_THAT(IsNull(
			EmptyModule->GetTypedefByIndex(0),
			TEXT("Empty typedef inventory should return null at index zero")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->DiscardModule(TypedefScope.Get()),
			TEXT("Typedef publication cleanup should discard the compiled module")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->DiscardModule(EmptyScope.Get()),
			TEXT("Typedef inventory cleanup should discard the empty module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(TypedefScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Typedef publication cleanup should remove the compiled module lookup")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(EmptyScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Typedef inventory cleanup should remove the empty module lookup")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
