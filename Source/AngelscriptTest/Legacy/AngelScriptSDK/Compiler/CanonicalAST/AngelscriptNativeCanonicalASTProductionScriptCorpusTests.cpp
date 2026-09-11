#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeCompilerBytecodeTestSupport.h"
#include "Core/AngelscriptEngine.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "as_module.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

namespace CanonicalASTProductionScriptCorpusTest
{
	struct FScopedCanonicalPipeline
	{
		asCScriptEngine* ScriptEngine = nullptr;
		asECompilerPipeline Previous = asCOMPILER_PIPELINE_LEGACY;

		explicit FScopedCanonicalPipeline(asCScriptEngine* InEngine)
			: ScriptEngine(InEngine)
		{
			if (ScriptEngine != nullptr)
			{
				Previous = ScriptEngine->GetCompilerPipeline();
				ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL);
			}
		}

		~FScopedCanonicalPipeline()
		{
			if (ScriptEngine != nullptr)
			{
				ScriptEngine->SetCompilerPipeline(Previous);
			}
		}
	};

	static int CompileDiscarding(
		asCScriptEngine* ScriptEngine,
		const char* ModuleName,
		const std::string& Source,
		asIScriptModule*& OutModule)
	{
		OutModule = nullptr;
		if (ScriptEngine == nullptr || ModuleName == nullptr)
		{
			return asINVALID_ARG;
		}
		const int Result = AngelscriptNativeTestSupport::CompileNativeModule(
			ScriptEngine,
			ModuleName,
			Source.c_str(),
			OutModule);
		return Result;
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTProductionScriptCorpusTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen.ScriptCorpus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ClassDefaultAssignsInheritedBehaviorTreeNodeName)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusBTNodeNameDefault";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class UCorpusBTDecorator : UBTDecorator_BlueprintBase
			{
				default NodeName = "Example Decorator";
			}

			class UCorpusBTService : UBTService_BlueprintBase
			{
				default NodeName = "Example Service";
			}

			class UCorpusBTTask : UBTTask_BlueprintBase
			{
				default NodeName = "Example Task";
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("class default NodeName must resolve the inherited native BT property")));
	}

	TEST_METHOD(EnumFormatSpecifierUsesApplyFormat)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusEnumApplyFormat";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			FString FormatInt()
			{
				return FString::ApplyFormat(1, "d");
			}

			FString FormatCollapsedName()
			{
				return FString::ApplyFormat((ESlateVisibility::Collapsed), "n");
			}

			void PrintFormattedLocation()
			{
				FVector ExampleLocation(100.0, 50.0, 1.0);
				Print("Called from actor at location " + ExampleLocation);
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("enum ApplyFormat and Print of FString concatenation must compile")));
	}

	TEST_METHOD(SetTimerReturnCopiesIntoLocalHandle)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusTimerHandleCopy";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			FTimerHandle CopyHandle(FTimerHandle Handle)
			{
				FTimerHandle Local = Handle;
				return Local;
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("SetTimer must copy FTimerHandle into a local by Canonical CodeGen")));
	}

	TEST_METHOD(CreateWidgetAcceptsDerivedUserWidgetSubclass)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusCreateWidgetSubclass";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void AddToHud(APlayerController OwningPlayer, TSubclassOf<UUserWidget> WidgetClass)
			{
				UUserWidget UserWidget = WidgetBlueprint::CreateWidget(WidgetClass, OwningPlayer);
				UserWidget.AddToViewport();
			}

			void AddDerivedToHud(APlayerController OwningPlayer, TSubclassOf<UUserWidget> BaseClass)
			{
				UUserWidget UserWidget = WidgetBlueprint::CreateWidget(BaseClass, OwningPlayer);
				UserWidget.AddToViewport();
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("CreateWidget must accept TSubclassOf<UUserWidget> through Canonical CodeGen")));
	}

	TEST_METHOD(NewObjectAcceptsThisOuterAndStaticClass)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusNewObjectStaticClass";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class UCorpusObjectFactory : UObject
			{
				UObject MakeChild()
				{
					return NewObject(this, AActor::StaticClass());
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("NewObject(this, T::StaticClass()) must resolve outer and generated StaticClass")));
	}

	TEST_METHOD(ForwardReferencedSuiteInheritsNativeAssertNotNull)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusForwardRefAssertNotNull";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class UCorpusLateAssertSuite : UAngelscriptTestSuite
			{
			}

			class UCorpusEarlyAssertCommand : ULatentAutomationCommand
			{
				void Before()
				{
					UCorpusLateAssertSuite Suite =
						Cast<UCorpusLateAssertSuite>(GetCurrentSuite());
					Suite.AssertNotNull(Suite);
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("a later script suite must still expose inherited AssertNotNull to an earlier class")));
	}

	TEST_METHOD(ArrayAndMapIndexSelectMutableOverload)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusArrayMapIndex";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void IndexArrayAndMap()
			{
				TArray<int> LocalIntArray;
				LocalIntArray.Add(2);
				int Value = LocalIntArray[0];
				Log("Array[" + 0 + "] = " + Value);

				TMap<FString, int> LocalStringToIntMap;
				LocalStringToIntMap.Add("ChangeThis", 5);
				if (LocalStringToIntMap.Contains("ChangeThis"))
				{
					int& MapValue = LocalStringToIntMap["ChangeThis"];
					LocalStringToIntMap["ChangeThis"] = 12;
				}
				LocalStringToIntMap.FindOrAdd("Newb") = 9;
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("mutable TArray/TMap opIndex must select the non-const overload")));
	}

	TEST_METHOD(RangeForOverIntArrayAndNumCompiles)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusArrayRangeFor";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void ExecuteExampleArray()
			{
				TArray<int> LocalIntArray;
				LocalIntArray.Add(2);
				LocalIntArray.Add(8);
				for (auto Iterator = LocalIntArray.Iterator(); Iterator.CanProceed; )
				{
					int Value = Iterator.Proceed();
					Log("Array contained " + Value);
				}

				for (int Index = 0, Count = LocalIntArray.Num(); Index < Count; ++Index)
				{
					int Value = LocalIntArray[Index];
					Log("Array[" + Index + "] = " + Value);
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("TArray range-for, Num, and mutating helpers must compile through Canonical CodeGen")));
	}

	TEST_METHOD(RangeForOverMapIteratorGetKeyCompiles)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusMapRangeFor";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class ACorpusMapActor : AActor
			{
				TMap<AActor, FString> ActorMap;

				void PrintActorMap()
				{
					for (auto Iterator = ActorMap.Iterator(); Iterator.CanProceed; )
					{
						auto Element = Iterator.Proceed();
						Log("Actor Map: " + Element.GetKey() + " => " + Element.GetValue());
					}
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("TMap range-for must expose iterator GetKey/GetValue through Canonical CodeGen")));
	}

	TEST_METHOD(RangeForOverMapIteratorSealsMemberRefNotGeneratedGet)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusMapIteratorMemberRef";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* const Module = ScriptEngine->GetModule(
			ModuleName, asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class ACorpusMapMemberRefActor : AActor
			{
				TMap<AActor, FString> ActorMap;
			}

			void PrintActorMap(ACorpusMapMemberRefActor Owner)
			{
				for (auto Iterator = Owner.ActorMap.Iterator(); Iterator.CanProceed; )
				{
					auto Element = Iterator.Proceed();
					Log("Actor Map: " + Element.GetKey() + " => " + Element.GetValue());
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdCorpusMapIteratorMemberRef.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCString Dump;
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const asCASTContext* const Context =
			Internal != nullptr ? Internal->GetCanonicalASTContext() : nullptr;
		if (Context != nullptr)
		{
			asCASTDump(*Context, Dump);
		}
		const FString Text =
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		const FString DumpMsg = FString::Printf(
			TEXT("ScriptCorpus Owner.ActorMap.Iterator() on a complete type must seal MEMBER_REF, not GetActorMap(). build=%d dump:\n%s"),
			BuildResult,
			*Text);
		ASSERT_THAT(AreEqual(0, BuildResult, *DumpMsg));
		ASSERT_THAT(IsNotNull(Context, *DumpMsg));
		int32 GetActorMapCalls = 0;
		bool bMemberRefActorMap = false;
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			if (Line.Contains(TEXT("kind=Call")) && Line.Contains(TEXT("GetActorMap()")))
			{
				++GetActorMapCalls;
			}
			if (Line.Contains(TEXT("kind=MemberRef"))
				&& Line.Contains(TEXT("ActorMap"))
				&& !Line.Contains(TEXT("GetActorMap")))
			{
				bMemberRefActorMap = true;
			}
		}
		ASSERT_THAT(AreEqual(0, GetActorMapCalls, *DumpMsg));
		ASSERT_THAT(IsTrue(bMemberRefActorMap, *DumpMsg));
	}

	TEST_METHOD(RangeForOverMapIteratorConstRefElementCompiles)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusMapRangeForConstRef";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class ACorpusMapConstRefActor : AActor
			{
				TMap<AActor, FString> ActorMap;

				void PrintActorMap()
				{
					for (auto Iterator = ActorMap.Iterator(); Iterator.CanProceed; )
					{
						auto __auto_constref_type Element = Iterator.Proceed();
						Log("Actor Map: " + Element.GetKey() + " => " + Element.GetValue());
					}
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("production range-for const-ref TMapIterator locals must compile through Canonical CodeGen")));
	}

	TEST_METHOD(NestedThisPropertyStructMemberAccessCompiles)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusStructThisProperty";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FCorpusExampleStruct
			{
				float ExampleNumber = 4.0;
				FString ExampleString = "Example String";
			}

			class ACorpusStructActor : AActor
			{
				FCorpusExampleStruct ExampleStruct;

				void ShowStruct()
				{
					Print(ExampleStruct.ExampleString + ", " + ExampleStruct.ExampleNumber);
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("this-property struct members must be addressed as lvalues, not wide reads")));
	}

	TEST_METHOD(ForwardReferencedSuiteFieldAssignCompiles)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusForwardRefFieldAssign";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class UCorpusLateSuite : UAngelscriptTestSuite
			{
				int Value = 0;
			}

			class UCorpusEarlyCommand : ULatentAutomationCommand
			{
				void Before()
				{
					UCorpusLateSuite Suite =
						Cast<UCorpusLateSuite>(GetCurrentSuite());
					Suite.AssertNotNull(Suite);
					Suite.Value = 1;
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("a later script suite field must be writable from an earlier command")));
	}

	TEST_METHOD(ForwardReferencedSuiteFieldAssignBeforeSuiteDeclCompiles)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusForwardRefFieldAssignEarly";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class UCorpusEarlyFieldCommand : ULatentAutomationCommand
			{
				void Before()
				{
					UCorpusLateFieldSuite Suite =
						Cast<UCorpusLateFieldSuite>(GetCurrentSuite());
					Suite.AssertNotNull(Suite);
					Suite.Value = 1;
				}
			}

			class UCorpusLateFieldSuite : UAngelscriptTestSuite
			{
				int Value = 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("Suite.Value must resolve after the later suite body is parsed")));
	}

	TEST_METHOD(ForwardReferencedSuiteFieldIncrementCompiles)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusForwardRefFieldIncrement";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class UCorpusEarlyIncrementCommand : ULatentAutomationCommand
			{
				bool Update()
				{
					UCorpusLateIncrementSuite Suite =
						Cast<UCorpusLateIncrementSuite>(GetCurrentSuite());
					Suite.Value += 1;
					return Suite.Value >= 3;
				}
			}

			class UCorpusLateIncrementSuite : UAngelscriptTestSuite
			{
				int Value = 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("Suite.Value += 1 must compound-assign a later suite field")));
	}

	TEST_METHOD(SpawnActorAcceptsDerivedActorSubclassByValue)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusSpawnActorSubclass";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			AActor SpawnExactActor()
			{
				return FAngelscriptTest::SpawnActor(
					AActor::StaticClass(),
					FVector(10.0, 20.0, 30.0));
			}

			AActor SpawnDerivedActor()
			{
				return FAngelscriptTest::SpawnActor(
					APawn::StaticClass(),
					FVector(10.0, 20.0, 30.0));
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("SpawnActor must copy TSubclassOf<derived> into TSubclassOf<AActor> by value")));
	}

	TEST_METHOD(ConstStringGlobalInitializesFromLiteral)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusConstStringGlobal";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			namespace CorpusGlobals
			{
				const int DefaultStartValue = 100;
				const FString WelcomeMessage = "Hello from AngelScript";
			}

			void UseWelcome()
			{
				Print(CorpusGlobals::WelcomeMessage);
			}

			int ReadStart()
			{
				return CorpusGlobals::DefaultStartValue;
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("namespace const FString globals must accept a string-literal initializer")));
	}

	TEST_METHOD(FormatSpecifiersAndNamedPrintCompile)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		CanonicalASTProductionScriptCorpusTest::FScopedCanonicalPipeline Pipeline(ScriptEngine);
		static const char* const ModuleName = "ProdCorpusFormatAndNamedPrint";
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class ACorpusFormatActor : AActor
			{
				void ShowFormatted(float32 DeltaSeconds)
				{
					FVector ExampleLocation(100.0, 50.0, 1.0);
					Print("Three Decimals: " + FString::ApplyFormat((ExampleLocation.Z), ".3f"));
					Print(FString::ApplyFormat((GetName()), ">40"));
					Print(FString::ApplyFormat((400), "010d"));
					Print("DeltaSeconds = " + FString::ApplyFormat((DeltaSeconds), ".0f"));
					Print("Jump was pressed!", Duration = 5.0);
				}
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			CanonicalASTProductionScriptCorpusTest::CompileDiscarding(
				ScriptEngine, ModuleName, ScriptSource, Module),
			TEXT("preprocessed f-string ApplyFormat and named Print Duration must compile")));
	}
};

#endif
