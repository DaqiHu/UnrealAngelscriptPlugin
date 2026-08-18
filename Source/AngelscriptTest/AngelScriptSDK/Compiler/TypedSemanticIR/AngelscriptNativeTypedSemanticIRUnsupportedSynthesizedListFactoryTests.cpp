#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRUnsupportedSynthesizedListFactoryTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr asPWORD ListFactoryStateSlot =
		static_cast<asPWORD>(0x4C49535446414354ull);

	struct FNativeListBox
	{
		int32 Marker = 73;
	};

	struct FListFactoryState
	{
		FNativeListBox Box;
		int32 CallCount = 0;
		int32 ArgumentCount = INDEX_NONE;
		int32 RegisteredFunctionId = asNO_FUNCTION;
		asDWORD ElementCount = 0;
		bool bSawListBuffer = false;
	};

	static void CreateNativeListBox(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		FListFactoryState* const State = static_cast<FListFactoryState*>(
			Generic->GetEngine()->GetUserData(ListFactoryStateSlot));
		if (State == nullptr)
		{
			Generic->SetReturnAddress(nullptr);
			return;
		}

		++State->CallCount;
		State->ArgumentCount = Generic->GetArgCount();
		const void* const ListBuffer = Generic->GetArgAddress(0);
		State->bSawListBuffer = ListBuffer != nullptr;
		if (ListBuffer != nullptr)
		{
			State->ElementCount = *static_cast<const asDWORD*>(ListBuffer);
		}
		Generic->SetReturnAddress(&State->Box);
	}

	static bool RegisterNativeListBox(
		asIScriptEngine* ScriptEngine,
		FListFactoryState& State)
	{
		if (ScriptEngine == nullptr
			|| ScriptEngine->SetUserData(&State, ListFactoryStateSlot) != nullptr)
		{
			return false;
		}

		if (ScriptEngine->RegisterObjectType(
			"FNativeListBox",
			0,
			asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) < 0
			|| ScriptEngine->RegisterObjectProperty(
				"FNativeListBox",
				"int Marker",
				asOFFSET(FNativeListBox, Marker)) < 0)
		{
			return false;
		}

		State.RegisteredFunctionId = ScriptEngine->RegisterObjectBehaviour(
				"FNativeListBox",
				asBEHAVE_LIST_FACTORY,
				"FNativeListBox f(int &in) {repeat int}",
				asFUNCTION(CreateNativeListBox),
				asCALL_GENERIC);
		return State.RegisteredFunctionId >= 0;
	}

	static asCScriptFunction* FindRegisteredListFactory(
		asCScriptEngine* ScriptEngine)
	{
		if (ScriptEngine == nullptr)
		{
			return nullptr;
		}

		asCObjectType* const ObjectType = static_cast<asCObjectType*>(
			ScriptEngine->GetTypeInfoByDecl("FNativeListBox"));
		if (ObjectType == nullptr
			|| ObjectType->beh.listFactory < 0
			|| static_cast<asUINT>(ObjectType->beh.listFactory)
				>= ScriptEngine->scriptFunctions.GetLength())
		{
			return nullptr;
		}
		return ScriptEngine->scriptFunctions[ObjectType->beh.listFactory];
	}

	static bool ExecuteGlobalInt(
		asIScriptEngine* ScriptEngine,
		asIScriptFunction* Function,
		int32& OutResult)
	{
		if (ScriptEngine == nullptr || Function == nullptr)
		{
			return false;
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		if (Context->Prepare(Function) < 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		OutResult = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

	static bool HasEngineLocalPointerOperand(asBYTE Opcode)
	{
		switch (Opcode)
		{
		case asBC_ALLOC:
		case asBC_REFCPY:
		case asBC_RefCpyV:
		case asBC_OBJTYPE:
		case asBC_FinConstruct:
		case asBC_DestructScript:
		case asBC_CopyScript:
		case asBC_JitEntry:
		case asBC_CALLSYS:
		case asBC_Thiscall1:
		case asBC_FuncPtr:
		case asBC_PGA:
		case asBC_PshGPtr:
		case asBC_LDG:
		case asBC_PshG4:
		case asBC_LdGRdR4:
		case asBC_CpyGtoV4:
		case asBC_CpyVtoG4:
		case asBC_SetG4:
		case asBC_FREE:
			return true;
		default:
			return false;
		}
	}

	static TArray<asDWORD> CopyBytecodeWithEngineLocalPointersNormalized(
		asIScriptFunction* Function,
		int32& OutSystemCallCount,
		int32& OutNormalizedPointerCount)
	{
		OutSystemCallCount = 0;
		OutNormalizedPointerCount = 0;
		TArray<asDWORD> Result =
			AngelscriptNativeTestSupport::CopyNativeBytecode(Function);
		asUINT Offset = 0;
		while (Offset < static_cast<asUINT>(Result.Num()))
		{
			const asBYTE Opcode = *reinterpret_cast<const asBYTE*>(&Result[Offset]);
			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| Offset + static_cast<asUINT>(InstructionSize)
					> static_cast<asUINT>(Result.Num()))
			{
				Result.Reset();
				OutSystemCallCount = 0;
				OutNormalizedPointerCount = 0;
				return Result;
			}

			if (HasEngineLocalPointerOperand(Opcode))
			{
				if (InstructionSize < 1 + AS_PTR_SIZE)
				{
					Result.Reset();
					OutSystemCallCount = 0;
					OutNormalizedPointerCount = 0;
					return Result;
				}
				for (int32 PointerWord = 1;
					PointerWord <= AS_PTR_SIZE;
					++PointerWord)
				{
					Result[Offset + static_cast<asUINT>(PointerWord)] = 0;
				}
				++OutNormalizedPointerCount;
				if (Opcode == asBC_CALLSYS)
				{
					++OutSystemCallCount;
				}
			}

			Offset += static_cast<asUINT>(InstructionSize);
		}
		return Result;
	}

public:
	TEST_METHOD(RegisteredListFactoryRemainsSystemFunctionAndCaptureInvariant)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SYNTHESIZED-LIST-FACTORY",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int BuildNativeList()
			{
				FNativeListBox Value = {11, 22, 33};
				return Value.Marker;
			}
			)AS");

		FNativeTestEngine CaptureOffEngine;
		FNativeTestEngine CaptureOnEngine;
		CaptureOffEngine.Create(*TestRunner);
		CaptureOnEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			if (CaptureOnEngine.Get() != nullptr)
			{
				CaptureOnEngine.Get()->SetUserData(nullptr, ListFactoryStateSlot);
			}
			if (CaptureOffEngine.Get() != nullptr)
			{
				CaptureOffEngine.Get()->SetUserData(nullptr, ListFactoryStateSlot);
			}
			CaptureOnEngine.Destroy();
			CaptureOffEngine.Destroy();
		};

		FListFactoryState CaptureOffState;
		FListFactoryState CaptureOnState;
		ASSERT_THAT(IsTrue(RegisterNativeListBox(
			CaptureOffEngine.Get(), CaptureOffState)));
		ASSERT_THAT(IsTrue(RegisterNativeListBox(
			CaptureOnEngine.Get(), CaptureOnState)));

		asCScriptEngine* const CaptureOffScriptEngine =
			static_cast<asCScriptEngine*>(CaptureOffEngine.Get());
		asCScriptEngine* const CaptureOnScriptEngine =
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get());
		ASSERT_THAT(IsNotNull(CaptureOffScriptEngine));
		ASSERT_THAT(IsNotNull(CaptureOnScriptEngine));
		if (CaptureOffScriptEngine == nullptr || CaptureOnScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(CaptureOnScriptEngine->SetTypedSemanticIRCapture(true)));

		asCScriptFunction* const RegisteredCaptureOffFactory =
			static_cast<asCScriptFunction*>(
				CaptureOffScriptEngine->GetFunctionById(
					CaptureOffState.RegisteredFunctionId));
		asCScriptFunction* const RegisteredCaptureOnFactory =
			static_cast<asCScriptFunction*>(
				CaptureOnScriptEngine->GetFunctionById(
					CaptureOnState.RegisteredFunctionId));
		asCScriptFunction* const TypeCaptureOffFactory =
			FindRegisteredListFactory(CaptureOffScriptEngine);
		asCScriptFunction* const TypeCaptureOnFactory =
			FindRegisteredListFactory(CaptureOnScriptEngine);
		ASSERT_THAT(IsNotNull(RegisteredCaptureOffFactory));
		ASSERT_THAT(IsNotNull(RegisteredCaptureOnFactory));
		ASSERT_THAT(IsTrue(RegisteredCaptureOffFactory == TypeCaptureOffFactory,
			TEXT("The type behavior slot must retain the returned list-factory ID")));
		ASSERT_THAT(IsTrue(RegisteredCaptureOnFactory == TypeCaptureOnFactory,
			TEXT("The capture Engine must retain its own returned list-factory ID")));
		if (RegisteredCaptureOffFactory == nullptr
			|| RegisteredCaptureOnFactory == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(RegisteredCaptureOffFactory->listPattern,
			TEXT("Successful list-factory registration must own its parsed pattern")));
		ASSERT_THAT(IsNotNull(RegisteredCaptureOnFactory->listPattern,
			TEXT("Capture does not participate in native list-pattern registration")));
		if (RegisteredCaptureOffFactory->listPattern == nullptr
			|| RegisteredCaptureOnFactory->listPattern == nullptr)
		{
			return;
		}

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRListFactoryOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRListFactoryOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid()));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid()));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asCScriptFunction* const CaptureOffFactory =
			FindRegisteredListFactory(CaptureOffScriptEngine);
		asCScriptFunction* const CaptureOnFactory =
			FindRegisteredListFactory(CaptureOnScriptEngine);
		ASSERT_THAT(IsNotNull(CaptureOffFactory));
		ASSERT_THAT(IsNotNull(CaptureOnFactory));
		if (CaptureOffFactory == nullptr || CaptureOnFactory == nullptr)
		{
			return;
		}

		for (asCScriptFunction* const Factory :
			{CaptureOffFactory, CaptureOnFactory})
		{
			ASSERT_THAT(AreEqual(asFUNC_SYSTEM, Factory->funcType));
			ASSERT_THAT(IsNull(Factory->scriptData,
				TEXT("A registered list factory must not own script compiler state")));
			ASSERT_THAT(AreEqual(
				asBUILD_ARTIFACT_INVOCATION_INVALID,
				Factory->artifactInvocationKind));
			ASSERT_THAT(IsNull(Factory->GetTypedSemanticFunction(),
				TEXT("System list factories do not enter a script HIR transaction")));
		}

		asIScriptFunction* const CaptureOffCaller = GetNativeFunctionByDecl(
			CaptureOffModule, "int BuildNativeList()");
		asIScriptFunction* const CaptureOnCaller = GetNativeFunctionByDecl(
			CaptureOnModule, "int BuildNativeList()");
		ASSERT_THAT(IsNotNull(CaptureOffCaller));
		ASSERT_THAT(IsNotNull(CaptureOnCaller));
		if (CaptureOffCaller == nullptr || CaptureOnCaller == nullptr)
		{
			return;
		}

		int32 CaptureOffSystemCallCount = 0;
		int32 CaptureOnSystemCallCount = 0;
		int32 CaptureOffNormalizedPointerCount = 0;
		int32 CaptureOnNormalizedPointerCount = 0;
		const TArray<asDWORD> CaptureOffBytecode =
			CopyBytecodeWithEngineLocalPointersNormalized(
				CaptureOffCaller,
				CaptureOffSystemCallCount,
				CaptureOffNormalizedPointerCount);
		const TArray<asDWORD> CaptureOnBytecode =
			CopyBytecodeWithEngineLocalPointersNormalized(
				CaptureOnCaller,
				CaptureOnSystemCallCount,
				CaptureOnNormalizedPointerCount);
		ASSERT_THAT(IsTrue(!CaptureOffBytecode.IsEmpty()));
		ASSERT_THAT(AreEqual(1, CaptureOffSystemCallCount,
			TEXT("The list initializer should contain one native factory call")));
		ASSERT_THAT(AreEqual(
			CaptureOffSystemCallCount,
			CaptureOnSystemCallCount));
		ASSERT_THAT(IsTrue(CaptureOffNormalizedPointerCount > 0));
		ASSERT_THAT(AreEqual(
			CaptureOffNormalizedPointerCount,
			CaptureOnNormalizedPointerCount));
		ASSERT_THAT(AreEqual(
			CaptureOffBytecode,
			CaptureOnBytecode,
			TEXT("HIR capture must not change list-factory bytecode after normalizing only documented Engine-local pointer operands")));

		int32 CaptureOffResult = 0;
		int32 CaptureOnResult = 0;
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOffEngine.Get(), CaptureOffCaller, CaptureOffResult)));
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOnEngine.Get(), CaptureOnCaller, CaptureOnResult)));
		ASSERT_THAT(AreEqual(73, CaptureOffResult));
		ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult));

		for (const FListFactoryState* const State :
			{&CaptureOffState, &CaptureOnState})
		{
			ASSERT_THAT(AreEqual(1, State->CallCount));
			ASSERT_THAT(AreEqual(1, State->ArgumentCount));
			ASSERT_THAT(IsTrue(State->bSawListBuffer));
			ASSERT_THAT(AreEqual(static_cast<asDWORD>(3), State->ElementCount));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
