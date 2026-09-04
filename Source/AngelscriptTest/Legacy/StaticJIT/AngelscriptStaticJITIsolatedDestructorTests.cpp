#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "StaticJIT/StaticJITHeader.h"

#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_thread.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITIsolatedDestructorTests,
	"Angelscript.TestModule.StaticJIT.ExceptionCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(IsolatedDestructorDoesNotReplacePrimaryException)
	{
		const FString ScriptSource = ASTEST_AS(R"AS(
			class FTrackedCleanupValue
			{
				int Value = 1;

				~FTrackedCleanupValue()
				{
					int Zero = 0;
					Value = 1 / Zero;
				}
			}
			)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			ASTEST_RESET_ENGINE(Engine);
		};

		const FName ModuleName = FName(TEXT("ASStaticJITIsolatedDestructor"));
		const FString SourceFilename = TEXT(
			"/Angelscript/Memory/StaticJITIsolatedDestructor/TrackedCleanup.as");
		ASSERT_THAT(IsTrue(
			CompileModuleFromMemory(
				&Engine,
				ModuleName,
				SourceFilename,
				ScriptSource),
			TEXT("Isolated-destructor fixture should compile")));

		asIScriptEngine* const ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		asIScriptModule* const Module = ScriptEngine->GetModule(
			TCHAR_TO_ANSI(*ModuleName.ToString()),
			asGM_ONLY_IF_EXISTS);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("Isolated-destructor fixture should publish its module")));
		if (Module == nullptr)
		{
			return;
		}

		asITypeInfo* const Type = Module->GetTypeInfoByName("FTrackedCleanupValue");
		ASSERT_THAT(IsNotNull(Type,
			TEXT("Isolated-destructor fixture should publish the tracked type")));
		if (Type == nullptr)
		{
			return;
		}

		void* const Object = ScriptEngine->CreateScriptObject(Type);
		ASSERT_THAT(IsNotNull(Object,
			TEXT("Isolated-destructor fixture should construct one live object")));
		if (Object == nullptr)
		{
			return;
		}

		asCThreadLocalData* const ThreadLocalData =
			asCThreadManager::GetLocalData();
		ASSERT_THAT(IsNotNull(ThreadLocalData,
			TEXT("Isolated destructor cleanup needs the current thread-local data")));
		if (ThreadLocalData == nullptr)
		{
			ScriptEngine->ReleaseScriptObject(Object, Type);
			return;
		}

		{
			FScriptExecution Execution(ThreadLocalData);
			Execution.bExceptionThrown = true;
			Execution.FailureRecord = &Execution.OwnedFailureRecord;
			Execution.FailureRecord->bHasFailure = true;
			Execution.FailureRecord->Message = "primary cleanup sentinel";
			ASSERT_THAT(IsTrue(Execution.bExceptionThrown));
			ASSERT_THAT(IsNotNull(Execution.FailureRecord));

			AngelscriptDestroyScriptObjectIsolated(
				Execution,
				ScriptEngine,
				Type,
				Object);

			ASSERT_THAT(IsTrue(
				Execution.bExceptionThrown,
				TEXT("Isolated destructor cleanup must leave the primary exception flag set")));
			ASSERT_THAT(IsNotNull(Execution.FailureRecord));
			if (Execution.FailureRecord != nullptr)
			{
				ASSERT_THAT(AreEqual(
					FString(TEXT("primary cleanup sentinel")),
					FString(UTF8_TO_TCHAR(
						Execution.FailureRecord->Message.AddressOf())),
					TEXT("Cleanup failure must not replace the primary exception")));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
