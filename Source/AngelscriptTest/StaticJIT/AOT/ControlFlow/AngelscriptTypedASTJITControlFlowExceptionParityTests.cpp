#include "CQTest.h"

#include "AngelscriptEngine.h"
#include "AngelscriptTestJITProbes.h"
#include "AngelscriptTestMacros.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/StaticJITHeader.h"
#include "Testing/AngelscriptScriptTestRunner.h"

#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_module.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITControlFlowExceptionParityTests,
	"Angelscript.TestModule.StaticJIT.AOT.ControlFlow.Exceptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FGeneratedResult
	{
		int32 Value = MIN_int32;
		bool bExceptionThrown = false;
	};

	static asCScriptFunction* FindCloneableFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Engine.GetModuleByModuleName(
				AngelscriptStaticJITAotFixture::
					GetTypedControlFlowModuleName().ToString());
		if (!Test.TestTrue(
				TEXT("The interpreter oracle must compile the typed control-flow module"),
				Module.IsValid() && Module->ScriptModule != nullptr))
		{
			return nullptr;
		}

		FTCHARToUTF8 DeclarationUtf8(
			*AngelscriptStaticJITAotFixture::
				GetSemanticCloneableStateDeclaration());
		asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
			Module->ScriptModule->GetFunctionByDecl(DeclarationUtf8.Get()));
		Test.TestNotNull(
			TEXT("The interpreter oracle must resolve SemanticCloneableState"),
			Function);
		return Function;
	}

	static FGeneratedResult ExecuteGenerated(
		FAngelscriptEngine& Engine,
		asCScriptFunction& Function,
		const AngelscriptTestJITProbes::FIsolatedDifferentialEntryView& Entry,
		const AngelscriptTestJITProbes::EIsolatedDifferentialEntryRoute Route,
		const int32 Mode)
	{
		using namespace AngelscriptTestJITProbes;
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Execution.jitEngine = Engine.GetScriptEngine();
		Execution.jitFunction = &Function;

		FGeneratedResult Result;
		if (Route == EIsolatedDifferentialEntryRoute::Raw)
		{
			using FRawEntry = int32(*)(
				FScriptExecution&, asDWORD, asDWORD, asBYTE, asBYTE);
			Result.Value = reinterpret_cast<FRawEntry>(Entry.RawEntry)(
				Execution,
				0,
				static_cast<asDWORD>(Mode),
				0,
				0);
		}
		else if (Route == EIsolatedDifferentialEntryRoute::VM)
		{
			asDWORD Frame[] = {
				0,
				static_cast<asDWORD>(Mode),
				0,
				0,
			};
			asQWORD ReturnValue = 0;
			Entry.VMEntry(Execution, Frame, &ReturnValue);
			Result.Value = static_cast<int32>(
				static_cast<asDWORD>(ReturnValue));
		}
		else
		{
			struct alignas(void*) FParms
			{
				int32 Seed = 0;
				int32 Mode = 0;
				bool bLeft = false;
				bool bRight = false;
				uint8 Padding[6] = {};
				void* GeneratedWorldContext = nullptr;
				int32 ReturnValue = MIN_int32;
			};
			static_assert(STRUCT_OFFSET(FParms, Seed) == 0);
			static_assert(STRUCT_OFFSET(FParms, Mode) == 4);
			static_assert(STRUCT_OFFSET(FParms, bLeft) == 8);
			static_assert(STRUCT_OFFSET(FParms, bRight) == 9);
			static_assert(
				STRUCT_OFFSET(FParms, GeneratedWorldContext) == 16);
			static_assert(STRUCT_OFFSET(FParms, ReturnValue) == 24);

			FParms Parms;
			Parms.Mode = Mode;
			Entry.ParmsEntry(Execution, nullptr, &Parms);
			Result.Value = Parms.ReturnValue;
		}
		Result.bExceptionThrown = Execution.bExceptionThrown;
		return Result;
	}

public:
	TEST_METHOD(ConditionBodyContinueIncrementAndSwitchFailuresMatchAllRoutes)
	{
		using namespace AngelscriptTestJITProbes;
		const FStringView Declaration(TEXT(
			"int SemanticCloneableState(const int, const int, const bool, const bool)"));
		FIsolatedDifferentialEntryView Bytecode;
		FIsolatedDifferentialEntryView Typed;
		const bool bFoundBytecode = FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::BytecodeJIT,
			Declaration,
			Bytecode);
		const bool bFoundTyped = FindIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::TypedASTJIT,
			Declaration,
			Typed);
		ASSERT_THAT(IsTrue(bFoundBytecode,
			TEXT("The independent BytecodeJIT control-flow artifact must be registered")));
		ASSERT_THAT(IsTrue(bFoundTyped,
			TEXT("The independent TypedASTJIT control-flow artifact must be registered")));
		if (!bFoundBytecode || !bFoundTyped)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(Bytecode.RawEntry));
		ASSERT_THAT(IsNotNull(Bytecode.VMEntry));
		ASSERT_THAT(IsNull(Bytecode.ParmsEntry));
		ASSERT_THAT(IsNotNull(Typed.RawEntry));
		ASSERT_THAT(IsNotNull(Typed.VMEntry));
		ASSERT_THAT(IsNotNull(Typed.ParmsEntry));

		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> InterpreterSession =
				AngelscriptStaticJITAotGeneration::
					CreateInterpreterFixtureEngineSession();
		ASSERT_THAT(IsNotNull(InterpreterSession.Get()));
		ASSERT_THAT(IsTrue(
			InterpreterSession.IsValid() && InterpreterSession->IsValid(),
			InterpreterSession.IsValid()
				? InterpreterSession->Error
				: TEXT("The control-flow interpreter session was not allocated")));
		if (!InterpreterSession.IsValid() || !InterpreterSession->IsValid())
		{
			return;
		}

		FAngelscriptEngine& Engine = *InterpreterSession->Engine;
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptFunction* const InterpreterFunction =
			FindCloneableFunction(*TestRunner, Engine);
		ASSERT_THAT(IsNotNull(InterpreterFunction));
		if (InterpreterFunction == nullptr)
		{
			return;
		}

		struct FCase
		{
			int32 Mode;
			const TCHAR* Phase;
		};
		const FCase Cases[] = {
			{4, TEXT("while-condition")},
			{5, TEXT("for-body")},
			{6, TEXT("for-continue-to-increment")},
			{7, TEXT("switch-selector")},
			{8, TEXT("do-while-continue-to-condition")},
			{10, TEXT("do-while-body")},
			{11, TEXT("while-continue-to-condition")},
			{12, TEXT("for-condition")},
			{13, TEXT("while-body")},
		};
		for (const FCase& Case : Cases)
		{
			asIScriptContext* const Context =
				InterpreterFunction->GetEngine()->CreateContext();
			ASSERT_THAT(IsNotNull(Context));
			if (Context == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT { Context->Release(); };
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Prepare(InterpreterFunction)));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->SetArgDWord(0, 0)));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->SetArgDWord(1, static_cast<asDWORD>(Case.Mode))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->SetArgByte(2, 0)));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->SetArgByte(3, 0)));
			int32 InterpreterStatus = asERROR;
			{
				FAngelscriptScriptTestCallbackScope SuppressExpectedLog(true);
				InterpreterStatus = Context->Execute();
			}
			ASSERT_THAT(AreEqual(
				asEXECUTION_EXCEPTION,
				InterpreterStatus,
				FString::Printf(TEXT("%s interpreter phase"), Case.Phase)));
			ASSERT_THAT(AreEqual(
				FString(TEXT("Divide by zero")),
				FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
				FString::Printf(TEXT("%s interpreter message"), Case.Phase)));

			const TPair<const FIsolatedDifferentialEntryView*,
				EIsolatedDifferentialEntryRoute> Routes[] = {
				{&Bytecode, EIsolatedDifferentialEntryRoute::Raw},
				{&Bytecode, EIsolatedDifferentialEntryRoute::VM},
				{&Typed, EIsolatedDifferentialEntryRoute::Raw},
				{&Typed, EIsolatedDifferentialEntryRoute::VM},
				{&Typed, EIsolatedDifferentialEntryRoute::Parms},
			};
			for (const TPair<const FIsolatedDifferentialEntryView*,
				EIsolatedDifferentialEntryRoute>& Route : Routes)
			{
				FGeneratedResult Result;
				{
					FAngelscriptScriptTestCallbackScope SuppressExpectedLog(true);
					Result = ExecuteGenerated(
						Engine,
						*InterpreterFunction,
						*Route.Key,
						Route.Value,
						Case.Mode);
				}
				ASSERT_THAT(IsTrue(
					Result.bExceptionThrown,
					FString::Printf(TEXT(
						"%s generated route %u must preserve the first exception"),
						Case.Phase,
						static_cast<uint32>(Route.Value))));
				ASSERT_THAT(AreEqual(
					0,
					Result.Value,
					FString::Printf(TEXT(
						"%s generated route %u deterministic failure value"),
						Case.Phase,
						static_cast<uint32>(Route.Value))));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
