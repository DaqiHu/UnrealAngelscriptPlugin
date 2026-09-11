#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "StartAngelscriptHeaders.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassObjectConstructionTests,
	"Angelscript.TestModule.Generator.ASClass.ObjectConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ModuleName = FName(TEXT("ASClassObjectConstruction"));
	inline static const FString ScriptFilename = FString(TEXT("ASClassObjectConstruction.as"));
	inline static const FName GeneratedClassName = FName(TEXT("UObjectConstructionCarrier"));
	inline static const FName CtorCountPropertyName = FName(TEXT("CtorCount"));
	inline static const FName DefaultValuePropertyName = FName(TEXT("DefaultValue"));
	inline static const FName DefaultLabelPropertyName = FName(TEXT("DefaultLabel"));
	inline static const FString ExpectedDefaultLabel = FString(TEXT("ObjectDefaults"));
	inline static const FName AbortModuleName = FName(TEXT("ASClassCanonicalConstructionAbort"));
	inline static const FString AbortScriptFilename = FString(TEXT("ASClassCanonicalConstructionAbort.as"));
	inline static const FName AbortGeneratedClassName = FName(TEXT("UCanonicalConstructionAbortCarrier"));
	static constexpr int32 ExpectedCtorCount = 1;
	static constexpr int32 ExpectedDefaultValue = 7;
	static constexpr asPWORD CanonicalAbortStateUserDataSlot =
		static_cast<asPWORD>(0x55415341424F5254ull);

	struct FCanonicalAbortState
	{
		bool bFailConstruction = false;
		int32 ConstructorCalls = 0;
		int32 DefaultsCalls = 0;
		int32 CompleteDestructorCalls = 0;
		UObject* ObservedRawObject = nullptr;
		TWeakObjectPtr<UObject> ObservedObject;

		void ResetForRuntimeFailure()
		{
			bFailConstruction = true;
			ConstructorCalls = 0;
			DefaultsCalls = 0;
			CompleteDestructorCalls = 0;
			ObservedRawObject = nullptr;
			ObservedObject = nullptr;
		}
	};

	static FCanonicalAbortState* GetCanonicalAbortState(asIScriptEngine* ScriptEngine)
	{
		return ScriptEngine != nullptr
			? static_cast<FCanonicalAbortState*>(
				ScriptEngine->GetUserData(CanonicalAbortStateUserDataSlot))
			: nullptr;
	}

	static void MaybeFailCanonicalUASConstruction(asIScriptGeneric* Generic)
	{
		FCanonicalAbortState* const State = Generic != nullptr
			? GetCanonicalAbortState(Generic->GetEngine()) : nullptr;
		if (State == nullptr)
		{
			return;
		}

		++State->ConstructorCalls;
		State->ObservedRawObject = UASClass::GetConstructingASObject();
		State->ObservedObject = State->ObservedRawObject;
		if (State->bFailConstruction)
		{
			if (asIScriptContext* const Context = asGetActiveContext())
			{
				Context->SetException(
					"canonical UASClass construction abort sentinel");
			}
		}
	}

	static void RecordCanonicalUASDefaults(asIScriptGeneric* Generic)
	{
		FCanonicalAbortState* const State = Generic != nullptr
			? GetCanonicalAbortState(Generic->GetEngine()) : nullptr;
		if (State != nullptr)
		{
			++State->DefaultsCalls;
		}
		if (Generic != nullptr)
		{
			Generic->SetReturnDWord(7);
		}
	}

	static void RecordCanonicalUASCompleteDestructor(asIScriptGeneric* Generic)
	{
		FCanonicalAbortState* const State = Generic != nullptr
			? GetCanonicalAbortState(Generic->GetEngine()) : nullptr;
		if (State != nullptr)
		{
			++State->CompleteDestructorCalls;
		}
	}

	static bool RegisterCanonicalAbortBridge(
		asIScriptEngine& ScriptEngine,
		FCanonicalAbortState& State)
	{
		ScriptEngine.SetUserData(&State, CanonicalAbortStateUserDataSlot);
		return ScriptEngine.RegisterGlobalFunction(
			"void MaybeFailCanonicalUASConstruction()",
			asFUNCTION(MaybeFailCanonicalUASConstruction),
			asCALL_GENERIC) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"int RecordCanonicalUASDefaults()",
				asFUNCTION(RecordCanonicalUASDefaults),
				asCALL_GENERIC) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordCanonicalUASCompleteDestructor()",
				asFUNCTION(RecordCanonicalUASCompleteDestructor),
				asCALL_GENERIC) >= 0;
	}

	struct FObjectConstructionSnapshot
	{
		int32 CtorCount = INDEX_NONE;
		int32 DefaultValue = INDEX_NONE;
		FString DefaultLabel;
	};

	static UASClass* CompileObjectConstructionCarrier(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UObjectConstructionCarrier : UObject
			{
				UPROPERTY()
				int CtorCount = 0;

				UPROPERTY()
				int DefaultValue = 0;

				UPROPERTY()
				FString DefaultLabel;

				UObjectConstructionCarrier()
				{
					CtorCount += 1;
				}

				default DefaultValue = 7;
				default DefaultLabel = "ObjectDefaults";
			}
			)AS");

		UClass* GeneratedClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			GeneratedClassName);
		if (GeneratedClass == nullptr)
		{
			return nullptr;
		}

		UASClass* GeneratedASClass = Cast<UASClass>(GeneratedClass);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(
				GeneratedASClass,
				TEXT("ASClass object-construction test case should compile the carrier into a UASClass")))
		{
			return nullptr;
		}

		bool bHasRequiredFunctions = true;
		bHasRequiredFunctions &= LocalAssert.IsNotNull(
			GeneratedASClass->ConstructFunction,
			TEXT("ASClass object-construction test case should bind the script constructor function"));
		bHasRequiredFunctions &= LocalAssert.IsNotNull(
			GeneratedASClass->DefaultsFunction,
			TEXT("ASClass object-construction test case should bind the defaults function"));
		bHasRequiredFunctions &= LocalAssert.IsNotNull(
			GeneratedASClass->ScriptTypePtr,
			TEXT("ASClass object-construction test case should keep a live script type pointer"));
		if (!bHasRequiredFunctions)
		{
			return nullptr;
		}

		return GeneratedASClass;
	}

	static bool ReadConstructionSnapshot(
		FAutomationTestBase& Test,
		UObject* Object,
		FObjectConstructionSnapshot& OutSnapshot)
	{
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, CtorCountPropertyName, OutSnapshot.CtorCount))
		{
			return false;
		}

		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, DefaultValuePropertyName, OutSnapshot.DefaultValue))
		{
			return false;
		}

		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FStrProperty>(Test, Object, DefaultLabelPropertyName, OutSnapshot.DefaultLabel))
		{
			return false;
		}

		return true;
	}

	static bool VerifySnapshot(
		FAutomationTestBase& Test,
		const FString& ScopeLabel,
		const FObjectConstructionSnapshot& Snapshot,
		int32 ExpectedCtorCountForScope)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const bool bCtorCountMatches = LocalAssert.AreEqual(
			ExpectedCtorCountForScope,
			Snapshot.CtorCount,
			*FString::Printf(TEXT("%s should observe the expected constructor count"), *ScopeLabel));
		const bool bDefaultValueMatches = LocalAssert.AreEqual(
			ExpectedDefaultValue,
			Snapshot.DefaultValue,
			*FString::Printf(TEXT("%s should preserve the scripted integer default"), *ScopeLabel));
		const bool bDefaultLabelMatches = LocalAssert.AreEqual(
			ExpectedDefaultLabel,
			Snapshot.DefaultLabel,
			*FString::Printf(TEXT("%s should preserve the scripted string default"), *ScopeLabel));

		return bCtorCountMatches && bDefaultValueMatches && bDefaultLabelMatches;
	}

	static void ReleaseObject(TWeakObjectPtr<UObject>& WeakObject)
	{
		if (!WeakObject.IsValid())
		{
			return;
		}

		WeakObject->RemoveFromRoot();
		WeakObject->MarkAsGarbage();
		WeakObject = nullptr;
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(StaticObjectConstructorAppliesScriptConstructorAndDefaultsOnce)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FAngelscriptASClassObjectConstructionTests::ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		UASClass* GeneratedASClass = FAngelscriptASClassObjectConstructionTests::CompileObjectConstructionCarrier(*TestRunner, Engine);
		if (GeneratedASClass == nullptr)
		{
			return;
		}

		UObject* DefaultObject = GeneratedASClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(DefaultObject, TEXT("ASClass object-construction test case should expose a generated class default object")));
		if (DefaultObject == nullptr)
		{
			return;
		}

		FAngelscriptASClassObjectConstructionTests::FObjectConstructionSnapshot DefaultSnapshot;
		if (!FAngelscriptASClassObjectConstructionTests::ReadConstructionSnapshot(*TestRunner, DefaultObject, DefaultSnapshot))
		{
			return;
		}

		UObject* FirstInstance = NewObject<UObject>(GetTransientPackage(), GeneratedASClass, TEXT("ObjectConstructionCarrierA"));
		UObject* SecondInstance = NewObject<UObject>(GetTransientPackage(), GeneratedASClass, TEXT("ObjectConstructionCarrierB"));
		ASSERT_THAT(IsNotNull(FirstInstance, TEXT("ASClass object-construction test case should create the first generated UObject instance")));
		ASSERT_THAT(IsNotNull(SecondInstance, TEXT("ASClass object-construction test case should create the second generated UObject instance")));
		if (FirstInstance == nullptr || SecondInstance == nullptr)
		{
			return;
		}

		FirstInstance->AddToRoot();
		SecondInstance->AddToRoot();

		TWeakObjectPtr<UObject> WeakFirstInstance = FirstInstance;
		TWeakObjectPtr<UObject> WeakSecondInstance = SecondInstance;
		ON_SCOPE_EXIT
		{
			FAngelscriptASClassObjectConstructionTests::ReleaseObject(WeakSecondInstance);
			FAngelscriptASClassObjectConstructionTests::ReleaseObject(WeakFirstInstance);
		};

		FAngelscriptASClassObjectConstructionTests::FObjectConstructionSnapshot FirstSnapshot;
		FAngelscriptASClassObjectConstructionTests::FObjectConstructionSnapshot SecondSnapshot;
		if (!FAngelscriptASClassObjectConstructionTests::ReadConstructionSnapshot(*TestRunner, FirstInstance, FirstSnapshot)
			|| !FAngelscriptASClassObjectConstructionTests::ReadConstructionSnapshot(*TestRunner, SecondInstance, SecondSnapshot))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			GeneratedASClass->IsChildOf(UObject::StaticClass()),
			TEXT("ASClass object-construction test case should compile a plain UObject-generated class")));
		ASSERT_THAT(IsFalse(
			GeneratedASClass->IsChildOf(AActor::StaticClass()),
			TEXT("ASClass object-construction test case should keep the generated class out of the actor hierarchy")));
		ASSERT_THAT(IsTrue(
			FirstInstance != SecondInstance,
			TEXT("ASClass object-construction test case should create distinct runtime instances")));
		ASSERT_THAT(IsTrue(
			FirstInstance != DefaultObject && SecondInstance != DefaultObject,
			TEXT("ASClass object-construction test case should keep runtime instances distinct from the class default object")));

		FAngelscriptASClassObjectConstructionTests::VerifySnapshot(
			*TestRunner,
			TEXT("ASClass object-construction test case class default object"),
			DefaultSnapshot,
			FAngelscriptASClassObjectConstructionTests::ExpectedCtorCount);
		FAngelscriptASClassObjectConstructionTests::VerifySnapshot(
			*TestRunner,
			TEXT("ASClass object-construction test case first instance"),
			FirstSnapshot,
			FAngelscriptASClassObjectConstructionTests::ExpectedCtorCount);
		FAngelscriptASClassObjectConstructionTests::VerifySnapshot(
			*TestRunner,
			TEXT("ASClass object-construction test case second instance"),
			SecondSnapshot,
			FAngelscriptASClassObjectConstructionTests::ExpectedCtorCount);

		ASSERT_THAT(AreEqual(
			FAngelscriptASClassObjectConstructionTests::ExpectedCtorCount,
			SecondSnapshot.CtorCount,
			TEXT("ASClass object-construction test case should keep the second instance constructor count isolated from the first instance")));
		ASSERT_THAT(AreEqual(
			FirstSnapshot.DefaultValue,
			SecondSnapshot.DefaultValue,
			TEXT("ASClass object-construction test case should keep both runtime instances on the same scripted integer default")));
		ASSERT_THAT(AreEqual(
			FirstSnapshot.DefaultLabel,
			SecondSnapshot.DefaultLabel,
			TEXT("ASClass object-construction test case should keep both runtime instances on the same scripted string default")));
	}

	TEST_METHOD(CanonicalFactoryAbortRetiresIncompleteUObjectWithoutCompleteDestructor)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			Engine.GetScriptEngine());
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Canonical UASClass abort test should retain its script engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const asECompilerPipeline PreviousPipeline =
			ScriptEngine->GetCompilerPipeline();
		FCanonicalAbortState State;
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetUserData(nullptr, CanonicalAbortStateUserDataSlot);
			ScriptEngine->SetCompilerPipeline(PreviousPipeline);
			Engine.DiscardModule(*AbortModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(
			ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("Canonical UASClass abort test should explicitly select CANONICAL")));
		ASSERT_THAT(IsTrue(
			RegisterCanonicalAbortBridge(*ScriptEngine, State),
			TEXT("Canonical UASClass abort test should register its native observation bridge")));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCanonicalConstructionAbortCarrier : UObject
			{
				UPROPERTY()
					int DefaultProbe = 0;

				UCanonicalConstructionAbortCarrier()
				{
					MaybeFailCanonicalUASConstruction();
				}

				~UCanonicalConstructionAbortCarrier()
				{
					RecordCanonicalUASCompleteDestructor();
				}

				default DefaultProbe = RecordCanonicalUASDefaults();
			}

			void RunCanonicalConstructionAbort()
			{
				UCanonicalConstructionAbortCarrier Object =
					UCanonicalConstructionAbortCarrier();
			}
			)AS");

		UASClass* const GeneratedClass = Cast<UASClass>(
			AngelscriptFunctionalTestUtils::CompileScriptModule(
				*TestRunner,
				Engine,
				AbortModuleName,
				AbortScriptFilename,
				ScriptSource,
				AbortGeneratedClassName));
		ASSERT_THAT(IsNotNull(
			GeneratedClass,
			TEXT("Canonical UASClass abort test should generate its UASClass")));
		if (GeneratedClass == nullptr)
		{
			return;
		}

		const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc =
			Engine.GetModule(AbortModuleName.ToString());
		asIScriptModule* const Module = ModuleDesc.IsValid()
			? ModuleDesc->ScriptModule : nullptr;
		ASSERT_THAT(IsNotNull(
			Module,
			TEXT("Canonical UASClass abort test should expose its script module")));
		if (Module == nullptr)
		{
			return;
		}

		State.ResetForRuntimeFailure();
		TestRunner->AddExpectedError(
			TEXT("canonical UASClass construction abort sentinel"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedError(
			TEXT("ASClassCanonicalConstructionAbort"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedError(
			TEXT("UCanonicalConstructionAbortCarrier::UCanonicalConstructionAbortCarrier() | Line"),
			EAutomationExpectedErrorFlags::Contains,
			1,
			false);
		TestRunner->AddExpectedError(
			TEXT("void RunCanonicalConstructionAbort() | Line"),
			EAutomationExpectedErrorFlags::Contains,
			1,
			false);
		ASSERT_THAT(IsTrue(ExecuteFunctionExpectingScriptException(
			*TestRunner,
			Engine,
			*Module,
			TEXT("void RunCanonicalConstructionAbort()"),
			TEXT("Canonical UASClass factory construction should propagate its constructor exception"),
			TEXT("canonical UASClass construction abort sentinel"))));

		ASSERT_THAT(AreEqual(
			1,
			State.ConstructorCalls,
			TEXT("Canonical UASClass abort should enter the runtime constructor exactly once")));
		ASSERT_THAT(AreEqual(
			0,
			State.DefaultsCalls,
			TEXT("Canonical UASClass abort must not publish script defaults")));
		ASSERT_THAT(AreEqual(
			0,
			State.CompleteDestructorCalls,
			TEXT("Canonical UASClass abort must not call the complete-object destructor")));
		ASSERT_THAT(IsNotNull(
			State.ObservedRawObject,
			TEXT("Canonical UASClass constructor bridge should observe the in-flight UObject")));
		ASSERT_THAT(IsNull(
			UASClass::GetConstructingASObject(),
			TEXT("Canonical UASClass abort should restore the FObjectInitializer construction stack")));
		ASSERT_THAT(IsFalse(
			State.ObservedObject.IsValid(),
			TEXT("Canonical UASClass abort should immediately make the incomplete UObject unavailable")));
		ASSERT_THAT(IsNotNull(
			State.ObservedObject.GetEvenIfUnreachable(),
			TEXT("Canonical UASClass abort should leave the garbage-marked UObject available for the next GC pass")));

		CollectGarbage(RF_NoFlags, true);
		ASSERT_THAT(IsFalse(
			State.ObservedObject.IsValid(),
			TEXT("Canonical UASClass abort object should remain unavailable after GC")));
		ASSERT_THAT(IsNull(
			State.ObservedObject.GetEvenIfUnreachable(),
			TEXT("Canonical UASClass abort object should be reclaimed by GC")));
		ASSERT_THAT(AreEqual(
			0,
			State.CompleteDestructorCalls,
			TEXT("GC must reclaim the incomplete UObject without calling the complete script destructor")));
	}
};

#endif
