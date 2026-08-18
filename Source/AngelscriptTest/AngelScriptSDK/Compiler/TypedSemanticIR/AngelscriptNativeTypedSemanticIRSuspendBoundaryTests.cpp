#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRSuspendBoundaryTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool BytecodeContainsOpcode(
		asIScriptFunction* Function,
		const asEBCInstr ExpectedOpcode)
	{
		if (Function == nullptr)
		{
			return false;
		}

		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		for (asUINT DwordIndex = 0; DwordIndex < BytecodeLength;)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(
				*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (Opcode == ExpectedOpcode)
			{
				return true;
			}
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				return false;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| DwordIndex + static_cast<asUINT>(InstructionSize)
					> BytecodeLength)
			{
				return false;
			}
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}
		return false;
	}

	static bool ExecuteGlobalInt(
		asIScriptEngine* ScriptEngine,
		asIScriptFunction* Function,
		const int32 Argument,
		int32& OutResult,
		int32* OutSuspendResult = nullptr)
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
			|| Context->SetArgDWord(0, static_cast<asDWORD>(Argument)) < 0)
		{
			return false;
		}
		if (OutSuspendResult != nullptr)
		{
			*OutSuspendResult = Context->Suspend();
		}
		if (Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		OutResult = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

public:
	TEST_METHOD(BytecodeSafePointsDoNotFabricateDisabledSuspendState)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SUSPEND-BOUNDARY",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int LoopSafePoint(int Limit)
			{
				int Sum = 0;
				for (int Index = 0; Index < Limit; ++Index)
				{
					Sum += Index;
				}
				return Sum;
			}
			)AS");

		FNativeTestEngine CaptureOffEngine;
		FNativeTestEngine CaptureOnEngine;
		CaptureOffEngine.Create(*TestRunner);
		CaptureOnEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			CaptureOnEngine.Destroy();
			CaptureOffEngine.Destroy();
		};

		asCScriptEngine* const CaptureOnScriptEngine =
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get());
		ASSERT_THAT(IsNotNull(CaptureOnScriptEngine));
		if (CaptureOnScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(CaptureOnScriptEngine->SetTypedSemanticIRCapture(true)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRSuspendBoundaryCaptureOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRSuspendBoundaryCaptureOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid()));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid()));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asCScriptFunction* const CaptureOffFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOffModule,
				"int LoopSafePoint(int Limit)"));
		asCScriptFunction* const CaptureOnFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOnModule,
				"int LoopSafePoint(int Limit)"));
		ASSERT_THAT(IsNotNull(CaptureOffFunction));
		ASSERT_THAT(IsNotNull(CaptureOnFunction));
		if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(BytecodeContainsOpcode(
			CaptureOnFunction,
			asBC_SUSPEND),
			TEXT("A real loop must retain its maintained VM line/loop safe point")));
		ASSERT_THAT(AreEqual(
			CopyNativeBytecode(CaptureOffFunction),
			CopyNativeBytecode(CaptureOnFunction),
			TEXT("Capturing safe-point roles must not rewrite bytecode")));

		int32 CaptureOffResult = 0;
		int32 CaptureOnResult = 0;
		int32 SuspendResult = asSUCCESS;
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOffEngine.Get(),
			CaptureOffFunction,
			5,
			CaptureOffResult)));
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOnEngine.Get(),
			CaptureOnFunction,
			5,
			CaptureOnResult,
			&SuspendResult)));
		ASSERT_THAT(AreEqual(10, CaptureOffResult));
		ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult));
		ASSERT_THAT(AreEqual(asERROR, SuspendResult,
			TEXT("The maintained fork still has no cooperative suspend state machine")));

		const asCTypedSemanticFunction* const HIR =
			CaptureOnFunction->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR));
		if (HIR == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(HIR->header.hasSuspendState,
			TEXT("Bytecode polling points alone are not resumable function state")));

		bool bHasLoopBackedge = false;
		bool bHasSuspendMarker = false;
		for (asUINT Index = 0; Index < HIR->statements.GetLength(); ++Index)
		{
			bHasLoopBackedge |= HIR->statements[Index].safePointRole
				== asETypedSemanticSafePointRole::LoopBackedge;
		}
		for (asUINT Index = 0; Index < HIR->expressions.GetLength(); ++Index)
		{
			bHasSuspendMarker |= HIR->expressions[Index].kind
					== asETypedSemanticExpressionKind::Unsupported
				&& HIR->expressions[Index].unsupportedKind
					== asETypedSemanticUnsupportedKind::SuspendPoint;
		}
		ASSERT_THAT(IsTrue(bHasLoopBackedge,
			TEXT("HIR keeps the loop safe-point role independently of instrumentation")));
		ASSERT_THAT(IsFalse(bHasSuspendMarker,
			TEXT("Disabled cooperative control must not fabricate a suspend fallback")));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
