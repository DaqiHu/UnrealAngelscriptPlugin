#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AOT/RuntimeRoutes/AngelscriptStaticJITAotRuntimeRouteTestSupport.h"
#include "StaticJIT/AngelscriptJITExecutionContext.h"
#include "Testing/AngelscriptScriptTestRunner.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	using namespace AngelscriptStaticJITAotRuntimeRouteTestSupport;

	struct FRecursiveRouteCase
	{
		const FString& Declaration;
		const TCHAR* Label;
		int32 Input;
		int32 Expected;
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotFrameRecursionRouteTests,
	"Angelscript.TestModule.StaticJIT.AOT.RuntimeRoutes.FrameRecursion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NestedRootAndHelperPublishAndRestoreLiveSourceFrames)
	{
		using FObservation = AngelscriptStaticJITAotFixture::
			FTypedASTFrameObservation;
		AngelscriptStaticJITAotFixture::ResetTypedASTFrameObservations();

		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> Session =
			AngelscriptStaticJITAotGeneration::CreateFixtureEngineSession(false);
		FAngelscriptEngine* const EnginePtr = RequireValidEngine(
			*TestRunner, Session.Get(), TEXT("TypedASTJIT live frame routing"));
		ASSERT_THAT(IsNotNull(EnginePtr));
		if (EnginePtr == nullptr)
		{
			return;
		}

		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const Function = FindGlobalFunction(
			*TestRunner,
			Engine,
			AngelscriptStaticJITAotFixture::GetTypedRecursionModuleName(),
			AngelscriptStaticJITAotFixture::GetTypedASTFrameEntryDeclaration());
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		const FAngelscriptJITBindingContext* const BindingContext =
			RequireBindingContext(
				*TestRunner, *Function, TEXT("TypedASTJIT live frame root"));
		ASSERT_THAT(IsNotNull(BindingContext));
		if (BindingContext == nullptr)
		{
			return;
		}
		const FEntryExecutionCounts CountsBefore =
			FEntryExecutionCounts::Capture(*BindingContext);

		asCThreadLocalData* const ThreadLocalData =
			asCThreadManager::GetLocalData();
		ASSERT_THAT(IsNotNull(ThreadLocalData));
		if (ThreadLocalData == nullptr)
		{
			return;
		}
		asIScriptFunction* const PreviousActiveFunction =
			ThreadLocalData->activeFunction;
		FScriptExecution* const PreviousActiveExecution =
			ThreadLocalData->activeExecution;

		asIScriptContext* const Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		constexpr int32 Input = 5;
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->SetArgDWord(0, static_cast<asDWORD>(Input))));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::
				GetExpectedTypedASTFrameResult(Input),
			static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(
			CountsBefore.VM + 1,
			BindingContext->GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::VM)));
		ASSERT_THAT(IsTrue(
			ThreadLocalData->activeFunction == PreviousActiveFunction));
		ASSERT_THAT(IsTrue(
			ThreadLocalData->activeExecution == PreviousActiveExecution));
		ASSERT_THAT(AreEqual(
			3,
			AngelscriptStaticJITAotFixture::
				GetTypedASTFrameObservationCount()));

		FObservation RootBefore;
		FObservation Helper;
		FObservation RootAfter;
		ASSERT_THAT(IsTrue(
			AngelscriptStaticJITAotFixture::TryGetTypedASTFrameObservation(
				0, RootBefore)));
		ASSERT_THAT(IsTrue(
			AngelscriptStaticJITAotFixture::TryGetTypedASTFrameObservation(
				1, Helper)));
		ASSERT_THAT(IsTrue(
			AngelscriptStaticJITAotFixture::TryGetTypedASTFrameObservation(
				2, RootAfter)));

		const FString ExpectedFilename(
			TEXT("/Angelscript/Game/ASStaticJITTypedRecursionFixture.as"));
		const FString ExpectedRootFunction(
			TEXT("int TypedASTFrameEntry(const int)"));
		const FString ExpectedHelperFunction(
			TEXT("int TypedASTFrameHelper(const int)"));
		auto VerifyRoot = [this, &ExpectedFilename, &ExpectedRootFunction](
			const FObservation& Observation,
			const int32 Marker)
		{
			ASSERT_THAT(AreEqual(Marker, Observation.Marker));
			ASSERT_THAT(AreEqual(
				ExpectedFilename, Observation.PublicFilename));
			ASSERT_THAT(AreEqual(
				AngelscriptStaticJITAotFixture::
					GetExpectedTypedASTFrameObservationLine(Marker),
				Observation.PublicLine));
			ASSERT_THAT(AreEqual(ExpectedFilename, Observation.TopFilename));
			ASSERT_THAT(AreEqual(ExpectedRootFunction, Observation.TopFunction));
			ASSERT_THAT(AreEqual(Observation.PublicLine, Observation.TopLine));
			ASSERT_THAT(AreEqual(1, Observation.FrameDepth));
			ASSERT_THAT(IsTrue(Observation.ParentFunction.IsEmpty()));
		};
		VerifyRoot(RootBefore, 1);
		VerifyRoot(RootAfter, 3);

		ASSERT_THAT(AreEqual(2, Helper.Marker));
		ASSERT_THAT(AreEqual(ExpectedFilename, Helper.PublicFilename));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::
				GetExpectedTypedASTFrameObservationLine(2),
			Helper.PublicLine));
		ASSERT_THAT(AreEqual(ExpectedFilename, Helper.TopFilename));
		ASSERT_THAT(AreEqual(ExpectedHelperFunction, Helper.TopFunction));
		ASSERT_THAT(AreEqual(Helper.PublicLine, Helper.TopLine));
		ASSERT_THAT(AreEqual(2, Helper.FrameDepth));
		ASSERT_THAT(AreEqual(ExpectedFilename, Helper.ParentFilename));
		ASSERT_THAT(AreEqual(ExpectedRootFunction, Helper.ParentFunction));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::GetExpectedTypedASTFrameHelperCallLine(),
			Helper.ParentLine));
	}

	TEST_METHOD(SelfAndMutualRecursionExecuteInstalledEntriesAndRestoreFrames)
	{
		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> Session =
			AngelscriptStaticJITAotGeneration::CreateFixtureEngineSession(false);
		FAngelscriptEngine* const EnginePtr = RequireValidEngine(
			*TestRunner, Session.Get(), TEXT("TypedASTJIT recursive frame routing"));
		ASSERT_THAT(IsNotNull(EnginePtr));
		if (EnginePtr == nullptr)
		{
			return;
		}

		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope EngineScope(Engine);
		const FRecursiveRouteCase Cases[] = {
			{
				AngelscriptStaticJITAotFixture::
					GetTypedASTSelfRecursionDeclaration(),
				TEXT("TypedAST self recursion"),
				5,
				AngelscriptStaticJITAotFixture::
					GetExpectedTypedASTSelfRecursionResult(),
			},
			{
				AngelscriptStaticJITAotFixture::
					GetTypedASTMutualRecursionDeclaration(),
				TEXT("TypedAST mutual recursion"),
				6,
				AngelscriptStaticJITAotFixture::
					GetExpectedTypedASTMutualRecursionResult(),
			},
		};

		asCThreadLocalData* const ThreadLocalData =
			asCThreadManager::GetLocalData();
		ASSERT_THAT(IsNotNull(ThreadLocalData));
		if (ThreadLocalData == nullptr)
		{
			return;
		}

		for (const FRecursiveRouteCase& Case : Cases)
		{
			asCScriptFunction* const Function = FindGlobalFunction(
				*TestRunner,
				Engine,
				AngelscriptStaticJITAotFixture::GetTypedRecursionModuleName(),
				Case.Declaration);
			ASSERT_THAT(IsNotNull(Function));
			if (Function == nullptr)
			{
				return;
			}

			const asSJITFunctionBinding Binding = Function->GetJITBinding();
			ASSERT_THAT(IsNotNull(Binding.VMEntry));
			ASSERT_THAT(IsNotNull(Binding.RawEntry));
			const FAngelscriptJITBindingContext* const BindingContext =
				RequireBindingContext(*TestRunner, *Function, Case.Label);
			ASSERT_THAT(IsNotNull(BindingContext));
			if (Binding.VMEntry == nullptr
				|| Binding.RawEntry == nullptr
				|| BindingContext == nullptr)
			{
				return;
			}

			asIScriptFunction* const PreviousActiveFunction =
				ThreadLocalData->activeFunction;
			FScriptExecution* const PreviousActiveExecution =
				ThreadLocalData->activeExecution;
			asIScriptContext* const Context = Engine.CreateContext();
			ASSERT_THAT(IsNotNull(Context));
			if (Context == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT { Context->Release(); };

			const FEntryExecutionCounts BeforeContext =
				FEntryExecutionCounts::Capture(*BindingContext);
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->SetArgDWord(0, static_cast<asDWORD>(Case.Input))));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(
				Case.Expected,
				static_cast<int32>(Context->GetReturnDWord())));
			ASSERT_THAT(AreEqual(
				BeforeContext.VM + 1,
				BindingContext->GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::VM)));
			ASSERT_THAT(AreEqual(
				BeforeContext.Raw,
				BindingContext->GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::Raw)));
			ASSERT_THAT(IsTrue(
				ThreadLocalData->activeFunction == PreviousActiveFunction));
			ASSERT_THAT(IsTrue(
				ThreadLocalData->activeExecution == PreviousActiveExecution));

			{
				using FRawEntry = int32(*)(FScriptExecution&, asDWORD);
				FScriptExecution RawExecution(ThreadLocalData);
				*RawExecution.NativeJITFrameLimit = 8;
				const FEntryExecutionCounts BeforeRaw =
					FEntryExecutionCounts::Capture(*BindingContext);
				bool bInvokedRaw = false;
				{
					FAngelscriptScriptTestCallbackScope
						SuppressExpectedExceptionLog(true);
					bInvokedRaw = FAngelscriptJITExecutionContext::InvokeRaw(
						RawExecution,
						Engine.GetScriptEngine(),
						Function,
						Binding,
						[](asJITFunction_Raw RawEntry,
							FScriptExecution& Execution)
						{
							reinterpret_cast<FRawEntry>(RawEntry)(
								Execution, 32u);
						});
				}
				ASSERT_THAT(IsTrue(
					bInvokedRaw,
					TEXT("The generated Raw entry should accept the explicit recursion budget")));
				ASSERT_THAT(AreEqual(
					BeforeRaw.Raw + 1,
					BindingContext->GetExecutionCount(
						FAngelscriptJITBindingContext::EEntryKind::Raw)));
				ASSERT_THAT(IsTrue(
					RawExecution.bExceptionThrown,
					TEXT("The generated Raw entry should stop at the explicit recursion budget")));
				ASSERT_THAT(IsTrue(
					RawExecution.FailureRecord != nullptr
						&& RawExecution.FailureRecord->bHasFailure,
					TEXT("The generated Raw entry should retain the recursion failure record")));
				ASSERT_THAT(AreEqual(
					FString(TEXT(
						"Stack overflow: potential infinite recursion detected?")),
					FString(UTF8_TO_TCHAR(
						RawExecution.FailureRecord != nullptr
							? RawExecution.FailureRecord->Message.AddressOf() : "")),
					TEXT("The generated Raw entry should retain the maintained exception message")));
				ASSERT_THAT(AreEqual(
					0u,
					RawExecution.GetNativeJITFrameDepth(),
					TEXT("Every recursive Raw helper exit should restore the native frame depth")));
				ASSERT_THAT(IsTrue(
					ThreadLocalData->activeFunction == PreviousActiveFunction));
			}
			ASSERT_THAT(IsTrue(
				ThreadLocalData->activeExecution == PreviousActiveExecution));

			const FEntryExecutionCounts BeforeOverflowContext =
				FEntryExecutionCounts::Capture(*BindingContext);
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare()));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->SetArgDWord(
					0,
					FScriptExecution::DefaultNativeJITFrameLimit + 8)));
			int32 RecursiveExecuteResult = asERROR;
			{
				FAngelscriptScriptTestCallbackScope
					SuppressExpectedExceptionLog(true);
				RecursiveExecuteResult = Context->Execute();
			}
			ASSERT_THAT(AreEqual(
				asEXECUTION_EXCEPTION,
				RecursiveExecuteResult));
			ASSERT_THAT(AreEqual(
				BeforeOverflowContext.VM + 1,
				BindingContext->GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::VM)));
			ASSERT_THAT(IsTrue(
				ThreadLocalData->activeFunction == PreviousActiveFunction));
			ASSERT_THAT(AreEqual(
				FString(TEXT(
					"Stack overflow: potential infinite recursion detected?")),
				FString(UTF8_TO_TCHAR(Context->GetExceptionString()))));
			ASSERT_THAT(IsTrue(
				ThreadLocalData->activeExecution == PreviousActiveExecution));
		}
	}
};

#endif
