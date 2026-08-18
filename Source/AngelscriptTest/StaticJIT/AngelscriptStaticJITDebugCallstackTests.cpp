#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "ClassGenerator/ASFunction.h"
#include "StaticJIT/StaticJITHeader.h"

#include "Curves/CurveFloat.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_JIT_DEBUG_CALLSTACKS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITDebugCallstackTests,
	"Angelscript.TestModule.StaticJIT.DebugCallstack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedDebugFrame
	{
		const ANSICHAR* Filename = "";
		const ANSICHAR* FunctionName = "";
		int32 LineNumber = 0;
		void* ThisObject = nullptr;
		FScopeJITDebugCallstack* PrevFrame = nullptr;
	};

	static FString DescribeObservedFrame(const FScopeJITDebugCallstack* Frame)
	{
		if (Frame == nullptr)
		{
			return TEXT("<null>");
		}

		const FString Filename = ANSI_TO_TCHAR(Frame->Filename != nullptr ? Frame->Filename : "");
		const FString FunctionName = ANSI_TO_TCHAR(Frame->FunctionName != nullptr ? Frame->FunctionName : "");
		return FString::Printf(
			TEXT("Filename=%s Function=%s Line=%d This=%p Prev=%p"),
			*Filename,
			*FunctionName,
			Frame->LineNumber,
			Frame->ThisObject,
			Frame->PrevFrame);
	}

	static bool VerifyTopFrame(
		FAutomationTestBase& Test,
		const FScopeJITDebugCallstack* ActualFrame,
		const FExpectedDebugFrame& ExpectedFrame,
		const TCHAR* StageLabel)
	{
		const FString FrameExistsMessage = FString::Printf(TEXT("%s should leave a live debug-callstack frame"), StageLabel);
		const bool bFrameExists = Test.TestNotNull(
			*FrameExistsMessage,
			ActualFrame);
		if (!bFrameExists)
		{
			return false;
		}

		const FString FilenameMessage = FString::Printf(TEXT("%s should record the expected filename"), StageLabel);
		const bool bFilenameMatches = Test.TestTrue(
			*FilenameMessage,
			FCStringAnsi::Strcmp(ActualFrame->Filename, ExpectedFrame.Filename) == 0);
		const FString FunctionMessage = FString::Printf(TEXT("%s should record the expected function name"), StageLabel);
		const bool bFunctionMatches = Test.TestTrue(
			*FunctionMessage,
			FCStringAnsi::Strcmp(ActualFrame->FunctionName, ExpectedFrame.FunctionName) == 0);
		const FString LineMessage = FString::Printf(TEXT("%s should record the expected line number"), StageLabel);
		const bool bLineMatches = Test.TestEqual(
			*LineMessage,
			ActualFrame->LineNumber,
			ExpectedFrame.LineNumber);
		const FString ThisMessage = FString::Printf(TEXT("%s should keep the expected ThisObject pointer"), StageLabel);
		const bool bThisMatches = Test.TestTrue(
			*ThisMessage,
			ActualFrame->ThisObject == ExpectedFrame.ThisObject);
		const FString PrevFrameMessage = FString::Printf(TEXT("%s should keep the expected previous frame link"), StageLabel);
		const bool bPrevMatches = Test.TestTrue(
			*PrevFrameMessage,
			ActualFrame->PrevFrame == ExpectedFrame.PrevFrame);

		const bool bAllMatched = bFilenameMatches && bFunctionMatches && bLineMatches && bThisMatches && bPrevMatches;
		if (!bAllMatched)
		{
			Test.AddInfo(FString::Printf(TEXT("%s observed frame: %s"), StageLabel, *DescribeObservedFrame(ActualFrame)));
		}

		return bAllMatched;
	}

	static bool RunDebugCallstackScopePushPop(FAutomationTestBase& Test);

public:
	TEST_METHOD(ScopePushPop)
	{
		ASSERT_THAT(IsTrue(RunDebugCallstackScopePushPop(*TestRunner)));
	}
};

bool FAngelscriptStaticJITDebugCallstackTests::RunDebugCallstackScopePushPop(FAutomationTestBase& Test)
{

	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	do
	{
		if (!Test.TestTrue(
				TEXT("StaticJIT.DebugCallstack.ScopePushPop should run with a current Angelscript engine scope"),
				FAngelscriptEngine::TryGetCurrentEngine() == &Engine))
		{
			break;
		}

		asCThreadLocalData* ThreadLocalData = FAngelscriptEngine::GameThreadTLD;
		if (!Test.TestNotNull(
				TEXT("StaticJIT.DebugCallstack.ScopePushPop should expose a valid game-thread local data pointer"),
				ThreadLocalData))
		{
			break;
		}

		FScriptExecution* PreviousExecution = ThreadLocalData->activeExecution;
		asCContext* PreviousContext = ThreadLocalData->activeContext;

		int32 OuterSentinel = 11;
		int32 InnerSentinel = 27;
		void* OuterThis = &OuterSentinel;
		void* InnerThis = &InnerSentinel;

		{
			FScriptExecution Execution(ThreadLocalData);
			if (!Test.TestTrue(
					TEXT("StaticJIT.DebugCallstack.ScopePushPop should register the local execution on the thread-local data"),
					ThreadLocalData->activeExecution == &Execution))
			{
				break;
			}

			if (!Test.TestNull(
					TEXT("StaticJIT.DebugCallstack.ScopePushPop should start with an empty debug callstack"),
					Execution.debugCallStack))
			{
				break;
			}

			FScopeJITDebugCallstack* OuterFrame = nullptr;
			{
				FScopeJITDebugCallstack OuterScope(Execution, "Outer.as", "Outer", 11, OuterThis);
				OuterFrame = &OuterScope;

				const FExpectedDebugFrame ExpectedOuterFrame
				{
					"Outer.as",
					"Outer",
					11,
					OuterThis,
					nullptr
				};
				if (!VerifyTopFrame(
						Test,
						static_cast<FScopeJITDebugCallstack*>(Execution.debugCallStack),
						ExpectedOuterFrame,
						TEXT("StaticJIT.DebugCallstack.ScopePushPop outer scope")))
				{
					break;
				}

				{
					FScopeJITDebugCallstack InnerScope(Execution, "Inner.as", "Inner", 27, InnerThis);

					const FExpectedDebugFrame ExpectedInnerFrame
					{
						"Inner.as",
						"Inner",
						27,
						InnerThis,
						OuterFrame
					};
					if (!VerifyTopFrame(
							Test,
							static_cast<FScopeJITDebugCallstack*>(Execution.debugCallStack),
							ExpectedInnerFrame,
							TEXT("StaticJIT.DebugCallstack.ScopePushPop inner scope")))
					{
						break;
					}
				}

				if (!VerifyTopFrame(
						Test,
						static_cast<FScopeJITDebugCallstack*>(Execution.debugCallStack),
						ExpectedOuterFrame,
						TEXT("StaticJIT.DebugCallstack.ScopePushPop after inner scope")))
				{
					break;
				}
			}

			if (!Test.TestNull(
					TEXT("StaticJIT.DebugCallstack.ScopePushPop should restore a null debug callstack after the outer scope exits"),
					Execution.debugCallStack)
				|| !Test.TestFalse(
					TEXT("StaticJIT.DebugCallstack.ScopePushPop should not mark the execution as throwing an exception"),
					Execution.bExceptionThrown))
			{
				break;
			}
		}

		if (!Test.TestTrue(
				TEXT("StaticJIT.DebugCallstack.ScopePushPop should restore the previous active execution after FScriptExecution exits"),
				ThreadLocalData->activeExecution == PreviousExecution)
			|| !Test.TestTrue(
				TEXT("StaticJIT.DebugCallstack.ScopePushPop should restore the previous active context after FScriptExecution exits"),
				ThreadLocalData->activeContext == PreviousContext))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITExecutionPositionTests,
	"Angelscript.TestModule.StaticJIT.DebugCallstack.ExecutionPosition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

	TEST_METHOD(FileAndLineUsesLiveTopFrame)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asCThreadLocalData* ThreadLocalData = FAngelscriptEngine::GameThreadTLD;
		ASSERT_THAT(IsNotNull(
			ThreadLocalData,
			TEXT("StaticJIT execution-position lookup requires game-thread local data")));

		FScriptExecution Execution(ThreadLocalData);
		FScopeJITDebugCallstack Frame(
			Execution,
			"ExecutionPosition.as",
			"ObservePosition",
			73,
			nullptr);

		FString Filename = TEXT("Unchanged");
		int32 LineNumber = -2;
		FAngelscriptEngine::GetAngelscriptExecutionFileAndLine(
			Filename,
			LineNumber);

		ASSERT_THAT(AreEqual(
			FString(TEXT("ExecutionPosition.as")),
			Filename,
			TEXT("JIT execution-position lookup should return the live top-frame filename")));
		ASSERT_THAT(AreEqual(
			73,
			LineNumber,
			TEXT("JIT execution-position lookup should return the live top-frame line")));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITExecutionFrameTests,
	"Angelscript.TestModule.StaticJIT.DebugCallstack.TypedASTJITFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static constexpr asPWORD PublicQueryRecorderUserDataSlot =
		static_cast<asPWORD>(0x5459504544515259ull);

	struct FPublicExecutionQueries
	{
		FString Filename;
		int32 LineNumber = INDEX_NONE;
		FString Position;
		TArray<FString> Callstack;
		FString FormattedCallstack;
		UObject* ThisObject = nullptr;
		asIScriptFunction* ActiveFunction = nullptr;
	};

	struct FVMQueryObservation
	{
		FPublicExecutionQueries Queries;
		FString ContextFilename;
		int32 ContextLineNumber = INDEX_NONE;
		UObject* OuterThisObject = nullptr;
		bool bCaptured = false;
	};

	struct FPublicQueryMatrixRow
	{
		FString Label;
		FPublicExecutionQueries Queries;
		FString ExpectedFilename;
		int32 ExpectedLineNumber = INDEX_NONE;
		int32 ExpectedCallstackCount = INDEX_NONE;
		TArray<FString> RequiredCallstackFragments;
		UObject* ExpectedThisObject = nullptr;
		asIScriptFunction* ExpectedActiveFunction = nullptr;
	};

	static FPublicExecutionQueries CapturePublicExecutionQueries(
		const int32 ThisStackFrame = 0)
	{
		FPublicExecutionQueries Result;
		FAngelscriptEngine::GetAngelscriptExecutionFileAndLine(
			Result.Filename,
			Result.LineNumber);
		Result.Position =
			FAngelscriptEngine::GetAngelscriptExecutionPosition();
		Result.Callstack = FAngelscriptEngine::GetAngelscriptCallstack();
		Result.FormattedCallstack =
			FAngelscriptEngine::FormatAngelscriptCallstack();
		Result.ThisObject =
			FAngelscriptEngine::GetAngelscriptExecutionThisObject(
				ThisStackFrame);
		Result.ActiveFunction = asGetActiveFunction();
		return Result;
	}

	static void CaptureVMQueryLine(asCContext* Context)
	{
		if (Context == nullptr)
		{
			return;
		}

		FVMQueryObservation* const Observation =
			static_cast<FVMQueryObservation*>(Context->GetUserData(
				PublicQueryRecorderUserDataSlot));
		asIScriptFunction* const Function = Context->GetFunction(0);
		if (Observation == nullptr
			|| Function == nullptr
			|| FCStringAnsi::Strcmp(Function->GetName(), "Probe") != 0)
		{
			return;
		}

		const ANSICHAR* ContextFilename = nullptr;
		const int32 ContextLineNumber = Context->GetLineNumber(
			0, nullptr, &ContextFilename);
		if (ContextFilename == nullptr || ContextLineNumber <= 0)
		{
			return;
		}

		Observation->Queries = CapturePublicExecutionQueries();
		Observation->ContextFilename = ANSI_TO_TCHAR(ContextFilename);
		Observation->ContextLineNumber = ContextLineNumber;
		Observation->OuterThisObject =
			FAngelscriptEngine::GetAngelscriptExecutionThisObject(1);
		Observation->bCaptured = true;
	}

	static bool CallstackContains(
		const TArray<FString>& Callstack,
		const FString& Fragment)
	{
		return Callstack.ContainsByPredicate(
			[&Fragment](const FString& Line)
			{
				return Line.Contains(Fragment);
			});
	}

	static bool VerifyPublicQueryMatrixRow(
		FAutomationTestBase& Test,
		const FPublicQueryMatrixRow& Row)
	{
		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s filename"), *Row.Label),
			Row.Queries.Filename,
			Row.ExpectedFilename);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s line"), *Row.Label),
			Row.Queries.LineNumber,
			Row.ExpectedLineNumber);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s formatted position"), *Row.Label),
			Row.Queries.Position,
			FString::Printf(
				TEXT("%s::%d"),
				*Row.ExpectedFilename,
				Row.ExpectedLineNumber));
		if (Row.ExpectedCallstackCount != INDEX_NONE)
		{
			bPassed &= Test.TestEqual(
				*FString::Printf(TEXT("%s callstack depth"), *Row.Label),
				Row.Queries.Callstack.Num(),
				Row.ExpectedCallstackCount);
		}
		for (const FString& Fragment : Row.RequiredCallstackFragments)
		{
			bPassed &= Test.TestTrue(
				*FString::Printf(
					TEXT("%s callstack should contain %s"),
					*Row.Label,
					*Fragment),
				CallstackContains(Row.Queries.Callstack, Fragment));
		}
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s formatted callstack"), *Row.Label),
			Row.Queries.FormattedCallstack,
			FString::Join(Row.Queries.Callstack, TEXT("\n")));
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s this object"), *Row.Label),
			Row.Queries.ThisObject == Row.ExpectedThisObject);
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s active function"), *Row.Label),
			Row.Queries.ActiveFunction == Row.ExpectedActiveFunction);
		return bPassed;
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

	TEST_METHOD(InstrumentationProfileControlsOnlyPositionFrameLinkage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		asCThreadLocalData* const ThreadLocalData =
			FAngelscriptEngine::GameThreadTLD;
		ASSERT_THAT(IsNotNull(ThreadLocalData));

		FScriptExecution Execution(ThreadLocalData);
		*Execution.NativeJITFrameLimit = 4;
		void* const PreviousDebugCallStack = Execution.debugCallStack;

		{
			FScopeTypedASTJITExecutionFrame RecursionOnlyFrame(
				Execution,
				nullptr,
				"/Angelscript/Game/TypedFrame.as",
				"void RecursionOnly()",
				11,
				nullptr,
				false);
			ASSERT_THAT(IsTrue(RecursionOnlyFrame.IsActive()));
			ASSERT_THAT(IsTrue(
				Execution.debugCallStack == PreviousDebugCallStack));
			RecursionOnlyFrame.SetLineNumber(19);
			ASSERT_THAT(IsTrue(
				Execution.debugCallStack == PreviousDebugCallStack));
		}

		{
			FScopeTypedASTJITExecutionFrame PositionFrame(
				Execution,
				nullptr,
				"/Angelscript/Game/TypedFrame.as",
				"void PositionAware()",
				23,
				nullptr,
				true);
			ASSERT_THAT(IsTrue(PositionFrame.IsActive()));
#if AS_JIT_DEBUG_CALLSTACKS
			FScopeJITDebugCallstack* const PublishedFrame =
				static_cast<FScopeJITDebugCallstack*>(
					Execution.debugCallStack);
			ASSERT_THAT(IsNotNull(PublishedFrame));
			ASSERT_THAT(AreEqual(23, PublishedFrame->LineNumber));
			PositionFrame.SetLineNumber(29);
			ASSERT_THAT(AreEqual(29, PublishedFrame->LineNumber));
#else
			PositionFrame.SetLineNumber(29);
			ASSERT_THAT(IsTrue(
				Execution.debugCallStack == PreviousDebugCallStack));
#endif
		}

		ASSERT_THAT(IsTrue(
			Execution.debugCallStack == PreviousDebugCallStack));
		ASSERT_THAT(AreEqual(0u, Execution.GetNativeJITFrameDepth()));
	}

	TEST_METHOD(DepthAndActiveFunctionAreSharedAndEveryExitRestoresOuterState)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		asCThreadLocalData* const ThreadLocalData =
			FAngelscriptEngine::GameThreadTLD;
		ASSERT_THAT(IsNotNull(ThreadLocalData));

		FScriptExecution* const PreviousExecution =
			ThreadLocalData->activeExecution;
		asCContext* const PreviousContext = ThreadLocalData->activeContext;
		asIScriptFunction* const PreviousActiveFunction =
			asGetActiveFunction();
		int32 RootFunctionSentinel = 17;
		int32 HelperFunctionSentinel = 23;
		int32 NestedFunctionSentinel = 29;
		int32 SystemFunctionSentinel = 31;
		asIScriptFunction* const RootFunction =
			reinterpret_cast<asIScriptFunction*>(&RootFunctionSentinel);
		asIScriptFunction* const HelperFunction =
			reinterpret_cast<asIScriptFunction*>(&HelperFunctionSentinel);
		asIScriptFunction* const NestedFunction =
			reinterpret_cast<asIScriptFunction*>(&NestedFunctionSentinel);
		asIScriptFunction* const SystemFunction =
			reinterpret_cast<asIScriptFunction*>(&SystemFunctionSentinel);

		{
			FScriptExecution Execution(ThreadLocalData);
			*Execution.NativeJITFrameLimit = 2;
			ASSERT_THAT(AreEqual(0u, Execution.GetNativeJITFrameDepth()));

			{
				FScopeTypedASTJITExecutionFrame RootFrame(
					Execution,
					RootFunction,
					"/Angelscript/Game/TypedFrame.as",
					"int Root()",
					17,
					nullptr);
				ASSERT_THAT(IsTrue(RootFrame.IsActive()));
				ASSERT_THAT(AreEqual(
					1u,
					Execution.GetNativeJITFrameDepth()));
				ASSERT_THAT(IsTrue(asGetActiveFunction() == RootFunction));

				{
					FScopeTypedASTJITExecutionFrame HelperFrame(
						Execution,
						HelperFunction,
						"/Angelscript/Game/TypedFrame.as",
						"int Helper()",
						23,
						nullptr);
					ASSERT_THAT(IsTrue(HelperFrame.IsActive()));
					ASSERT_THAT(AreEqual(
						2u,
						Execution.GetNativeJITFrameDepth()));
					ASSERT_THAT(IsTrue(
						asGetActiveFunction() == HelperFunction));

					{
						FScopeInformSystemFunction SystemScope(
							Execution,
							SystemFunction);
						ASSERT_THAT(IsTrue(
							asGetActiveFunction() == SystemFunction));
					}
					ASSERT_THAT(IsTrue(
						asGetActiveFunction() == HelperFunction,
						TEXT("A system/native call must restore the active Typed helper")));

					TestRunner->AddExpectedErrorPlain(
						TEXT("Stack overflow: potential infinite recursion detected?"),
						EAutomationExpectedErrorFlags::Contains,
						1);
					TestRunner->AddExpectedErrorPlain(
						TEXT("/Angelscript/Game/TypedFrame.as"),
						EAutomationExpectedErrorFlags::Contains,
						1);
					TestRunner->AddExpectedErrorPlain(
						TEXT("int Root() | Line 17"),
						EAutomationExpectedErrorFlags::Contains,
						1);
					TestRunner->AddExpectedErrorPlain(
						TEXT("int Helper() | Line 23"),
						EAutomationExpectedErrorFlags::Contains,
						1);
					TestRunner->AddExpectedErrorPlain(
						TEXT("int Overflow() | Line 31"),
						EAutomationExpectedErrorFlags::Contains,
						1);
					{
						FScopeTypedASTJITExecutionFrame OverflowFrame(
							Execution,
							nullptr,
							"/Angelscript/Game/TypedFrame.as",
							"int Overflow()",
							31,
							nullptr);
						ASSERT_THAT(IsFalse(OverflowFrame.IsActive()));
						ASSERT_THAT(AreEqual(
							2u,
							Execution.GetNativeJITFrameDepth()));
					}
					ASSERT_THAT(IsTrue(
						asGetActiveFunction() == HelperFunction));
				}

				ASSERT_THAT(AreEqual(
					1u,
					Execution.GetNativeJITFrameDepth()));
				ASSERT_THAT(IsTrue(asGetActiveFunction() == RootFunction));

				{
					FScriptExecution NestedExecution(ThreadLocalData);
					ASSERT_THAT(IsTrue(
						NestedExecution.NativeJITFrameDepth
							== Execution.NativeJITFrameDepth));
					ASSERT_THAT(IsTrue(
						NestedExecution.NativeJITFrameLimit
							== Execution.NativeJITFrameLimit));
					FScopeTypedASTJITExecutionFrame NestedFrame(
						NestedExecution,
						NestedFunction,
						"/Angelscript/Game/NestedTypedFrame.as",
						"int Nested()",
						29,
						nullptr);
					ASSERT_THAT(IsTrue(NestedFrame.IsActive()));
					ASSERT_THAT(AreEqual(
						2u,
						Execution.GetNativeJITFrameDepth()));
					ASSERT_THAT(IsTrue(
						asGetActiveFunction() == NestedFunction));
				}

				ASSERT_THAT(AreEqual(
					1u,
					Execution.GetNativeJITFrameDepth()));
				ASSERT_THAT(IsTrue(asGetActiveFunction() == RootFunction));
			}

			ASSERT_THAT(AreEqual(0u, Execution.GetNativeJITFrameDepth()));
			ASSERT_THAT(IsTrue(
				asGetActiveFunction() == PreviousActiveFunction));
			ASSERT_THAT(IsNull(Execution.debugCallStack));
			ASSERT_THAT(IsTrue(Execution.bExceptionThrown));
			ASSERT_THAT(IsNotNull(Execution.FailureRecord));
			ASSERT_THAT(IsTrue(Execution.FailureRecord->bHasFailure));
			ASSERT_THAT(AreEqual(
				FString(TEXT("typed-ast-recursion-budget")),
				FString(ANSI_TO_TCHAR(
					Execution.FailureRecord->RouteOrigin.AddressOf()))));
		}

		ASSERT_THAT(IsTrue(
			ThreadLocalData->activeExecution == PreviousExecution));
		ASSERT_THAT(IsTrue(
			ThreadLocalData->activeContext == PreviousContext));
		ASSERT_THAT(IsTrue(
			asGetActiveFunction() == PreviousActiveFunction));
	}

	TEST_METHOD(PublicQueriesTraverseNestedJITFramesAndSystemCalls)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		asCThreadLocalData* const ThreadLocalData =
			FAngelscriptEngine::GameThreadTLD;
		ASSERT_THAT(IsNotNull(ThreadLocalData));

		FScriptExecution* const PreviousExecution =
			ThreadLocalData->activeExecution;
		asCContext* const PreviousContext = ThreadLocalData->activeContext;
		asIScriptFunction* const PreviousActiveFunction =
			asGetActiveFunction();

		int32 RootFunctionSentinel = 41;
		int32 HelperFunctionSentinel = 43;
		int32 NestedFunctionSentinel = 47;
		int32 SystemFunctionSentinel = 53;
		asIScriptFunction* const RootFunction =
			reinterpret_cast<asIScriptFunction*>(&RootFunctionSentinel);
		asIScriptFunction* const HelperFunction =
			reinterpret_cast<asIScriptFunction*>(&HelperFunctionSentinel);
		asIScriptFunction* const NestedFunction =
			reinterpret_cast<asIScriptFunction*>(&NestedFunctionSentinel);
		asIScriptFunction* const SystemFunction =
			reinterpret_cast<asIScriptFunction*>(&SystemFunctionSentinel);

		UObject* const RootThis = NewObject<UCurveFloat>();
		UObject* const HelperThis = NewObject<UCurveFloat>();
		UObject* const NestedThis = NewObject<UCurveFloat>();
		ASSERT_THAT(IsNotNull(RootThis));
		ASSERT_THAT(IsNotNull(HelperThis));
		ASSERT_THAT(IsNotNull(NestedThis));
		TArray<FPublicQueryMatrixRow> QueryRows;

		{
			FScriptExecution Execution(ThreadLocalData);
			{
				FScopeTypedASTJITExecutionFrame RootFrame(
					Execution,
					RootFunction,
					"/Angelscript/Game/PublicQueries.as",
					"int Root()",
					17,
					RootThis);
				ASSERT_THAT(IsTrue(RootFrame.IsActive()));

				const FPublicExecutionQueries RootQueries =
					CapturePublicExecutionQueries();
				QueryRows.Add({
					TEXT("top-level TypedAST JIT root"),
					RootQueries,
					TEXT("/Angelscript/Game/PublicQueries.as"),
					17,
					2,
					{TEXT("int Root() | Line 17")},
					RootThis,
					RootFunction});

				{
					FScopeTypedASTJITExecutionFrame HelperFrame(
						Execution,
						HelperFunction,
						"/Angelscript/Game/PublicQueries.as",
						"int Helper()",
						23,
						HelperThis);
					ASSERT_THAT(IsTrue(HelperFrame.IsActive()));

					const FPublicExecutionQueries HelperQueries =
						CapturePublicExecutionQueries();
					QueryRows.Add({
						TEXT("direct TypedAST JIT helper"),
						HelperQueries,
						TEXT("/Angelscript/Game/PublicQueries.as"),
						23,
						3,
						{
							TEXT("int Helper() | Line 23"),
							TEXT("int Root() | Line 17")
						},
						HelperThis,
						HelperFunction});
					ASSERT_THAT(IsTrue(
						CapturePublicExecutionQueries(1).ThisObject
							== RootThis));

					{
						FScopeInformSystemFunction SystemScope(
							Execution,
							SystemFunction);
						const FPublicExecutionQueries SystemQueries =
							CapturePublicExecutionQueries();
						QueryRows.Add({
							TEXT("system call inside TypedAST helper"),
							SystemQueries,
							TEXT("/Angelscript/Game/PublicQueries.as"),
							23,
							3,
							{
								TEXT("int Helper() | Line 23"),
								TEXT("int Root() | Line 17")
							},
							HelperThis,
							SystemFunction});
					}

					{
						FScriptExecution NestedExecution(ThreadLocalData);
						FScopeTypedASTJITExecutionFrame NestedFrame(
							NestedExecution,
							NestedFunction,
							"/Angelscript/Game/NestedPublicQueries.as",
							"int Nested()",
							29,
							NestedThis);
						ASSERT_THAT(IsTrue(NestedFrame.IsActive()));

						const FPublicExecutionQueries NestedQueries =
							CapturePublicExecutionQueries();
						QueryRows.Add({
							TEXT("nested TypedAST JIT execution"),
							NestedQueries,
							TEXT("/Angelscript/Game/NestedPublicQueries.as"),
							29,
							4,
							{
								TEXT("int Nested() | Line 29"),
								TEXT("int Helper() | Line 23"),
								TEXT("int Root() | Line 17")
							},
							NestedThis,
							NestedFunction});
						ASSERT_THAT(IsTrue(
							CapturePublicExecutionQueries(1).ThisObject
								== HelperThis));
						ASSERT_THAT(IsTrue(
							CapturePublicExecutionQueries(2).ThisObject
								== RootThis));
					}

					const FPublicExecutionQueries RestoredHelperQueries =
						CapturePublicExecutionQueries();
					QueryRows.Add({
						TEXT("restored TypedAST helper"),
						RestoredHelperQueries,
						TEXT("/Angelscript/Game/PublicQueries.as"),
						23,
						3,
						{
							TEXT("int Helper() | Line 23"),
							TEXT("int Root() | Line 17")
						},
						HelperThis,
						HelperFunction});
				}

				const FPublicExecutionQueries RestoredRootQueries =
					CapturePublicExecutionQueries();
				QueryRows.Add({
					TEXT("restored TypedAST root"),
					RestoredRootQueries,
					TEXT("/Angelscript/Game/PublicQueries.as"),
					17,
					2,
					{TEXT("int Root() | Line 17")},
					RootThis,
					RootFunction});
			}
		}

		for (const FPublicQueryMatrixRow& Row : QueryRows)
		{
			ASSERT_THAT(IsTrue(VerifyPublicQueryMatrixRow(
				*TestRunner,
				Row)));
		}

		ASSERT_THAT(IsTrue(
			ThreadLocalData->activeExecution == PreviousExecution));
		ASSERT_THAT(IsTrue(
			ThreadLocalData->activeContext == PreviousContext));
		ASSERT_THAT(IsTrue(
			asGetActiveFunction() == PreviousActiveFunction));
	}

	TEST_METHOD(PublicQueryMatrixIncludesVMAndMixedJITToVMFrames)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		asCThreadLocalData* const ThreadLocalData =
			FAngelscriptEngine::GameThreadTLD;
		ASSERT_THAT(IsNotNull(ThreadLocalData));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UTypedASTJITVMQueryCarrier : UObject
			{
				UFUNCTION()
				int Probe()
				{
					int Value = 41;
					Value += 1;
					return Value;
				}
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTypedASTJITVMQueryMatrix"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		UClass* const GeneratedClass = FindGeneratedClass(
			&Engine, TEXT("UTypedASTJITVMQueryCarrier"));
		ASSERT_THAT(IsNotNull(GeneratedClass));
		if (GeneratedClass == nullptr)
		{
			return;
		}

		UASFunction* const ProbeWrapper = Cast<UASFunction>(
			FindGeneratedFunction(GeneratedClass, TEXT("Probe")));
		ASSERT_THAT(IsNotNull(ProbeWrapper));
		ASSERT_THAT(IsNotNull(
			ProbeWrapper != nullptr ? ProbeWrapper->ScriptFunction : nullptr));
		if (ProbeWrapper == nullptr || ProbeWrapper->ScriptFunction == nullptr)
		{
			return;
		}

		UObject* const VMThis = NewObject<UObject>(
			GetTransientPackage(),
			GeneratedClass,
			TEXT("TypedASTJITVMQueryCarrier"));
		UObject* const JITThis = NewObject<UCurveFloat>();
		ASSERT_THAT(IsNotNull(VMThis));
		ASSERT_THAT(IsNotNull(JITThis));

		auto ExecuteVMProbe = [this, &Engine, VMThis, ProbeWrapper](
			FVMQueryObservation& Observation) -> int32
		{
			asIScriptContext* const Context =
				Engine.GetScriptEngine()->CreateContext();
			if (Context == nullptr)
			{
				return asOUT_OF_MEMORY;
			}

			asCContext* const RawContext = static_cast<asCContext*>(Context);
			Context->SetUserData(
				&Observation, PublicQueryRecorderUserDataSlot);
			const int32 CallbackResult = RawContext->SetLineCallback(
				CaptureVMQueryLine);
			const int32 PrepareResult = CallbackResult >= 0
				? Context->Prepare(ProbeWrapper->ScriptFunction)
				: CallbackResult;
			const int32 SetObjectResult = PrepareResult >= 0
				? Context->SetObject(VMThis)
				: PrepareResult;
			const int32 ExecuteResult = SetObjectResult >= 0
				? Context->Execute()
				: SetObjectResult;
			RawContext->ClearLineCallback();
			Context->SetUserData(nullptr, PublicQueryRecorderUserDataSlot);
			Context->Release();
			return ExecuteResult;
		};

		FScriptExecution* const PreviousExecution =
			ThreadLocalData->activeExecution;
		asCContext* const PreviousContext = ThreadLocalData->activeContext;
		asIScriptFunction* const PreviousActiveFunction =
			asGetActiveFunction();
		TGuardValue<bool> CanRunLineCallbackGuard(
			asCContext::CanEverRunLineCallback, true);
		TGuardValue<bool> AlwaysRunLineCallbackGuard(
			asCContext::ShouldAlwaysRunLineCallback, true);

		FVMQueryObservation VMObservation;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteVMProbe(VMObservation)));
		ASSERT_THAT(IsTrue(VMObservation.bCaptured));
		ASSERT_THAT(IsNull(VMObservation.OuterThisObject));
		TArray<FPublicQueryMatrixRow> QueryRows;
		QueryRows.Add({
			TEXT("pure VM"),
			VMObservation.Queries,
			VMObservation.ContextFilename,
			VMObservation.ContextLineNumber,
			INDEX_NONE,
			{TEXT("Probe")},
			VMThis,
			PreviousActiveFunction});

		int32 JITFunctionSentinel = 59;
		asIScriptFunction* const JITFunction =
			reinterpret_cast<asIScriptFunction*>(&JITFunctionSentinel);
		FVMQueryObservation MixedObservation;
		{
			FScriptExecution Execution(ThreadLocalData);
			FScopeTypedASTJITExecutionFrame JITFrame(
				Execution,
				JITFunction,
				"/Angelscript/Game/MixedPublicQueries.as",
				"int MixedRoot()",
				37,
				JITThis);
			ASSERT_THAT(IsTrue(JITFrame.IsActive()));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				ExecuteVMProbe(MixedObservation)));
			ASSERT_THAT(IsTrue(MixedObservation.bCaptured));
			ASSERT_THAT(IsTrue(
				MixedObservation.OuterThisObject == JITThis));
			QueryRows.Add({
				TEXT("mixed TypedAST JIT to VM"),
				MixedObservation.Queries,
				MixedObservation.ContextFilename,
				MixedObservation.ContextLineNumber,
				INDEX_NONE,
				{TEXT("Probe"), TEXT("MixedRoot")},
				VMThis,
				JITFunction});

			const FPublicExecutionQueries RestoredJITQueries =
				CapturePublicExecutionQueries();
			QueryRows.Add({
				TEXT("restored TypedAST root after VM"),
				RestoredJITQueries,
				TEXT("/Angelscript/Game/MixedPublicQueries.as"),
				37,
				2,
				{TEXT("int MixedRoot() | Line 37")},
				JITThis,
				JITFunction});
		}

		for (const FPublicQueryMatrixRow& Row : QueryRows)
		{
			ASSERT_THAT(IsTrue(VerifyPublicQueryMatrixRow(
				*TestRunner,
				Row)));
		}

		ASSERT_THAT(IsTrue(
			ThreadLocalData->activeExecution == PreviousExecution));
		ASSERT_THAT(IsTrue(
			ThreadLocalData->activeContext == PreviousContext));
		ASSERT_THAT(IsTrue(
			asGetActiveFunction() == PreviousActiveFunction));
	}
};

#endif
