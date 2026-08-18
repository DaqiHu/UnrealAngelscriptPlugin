#include "Support/AngelscriptNativeCoreTestSupport.h"
#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeCaseTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_compiler.h"
#include "source/as_module.h"
#include "source/as_property.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace TypedSemanticIRTest
{
	static int ObserveSystemCallTarget(
		const int A,
		const int B,
		const int C)
	{
		return A * 100 + B * 10 + C;
	}
}

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asSTypedSemanticSourceSpan MakeSpan()
	{
		asSTypedSemanticSourceSpan Span;
		Span.section = "input.as";
		Span.sourceOffset = 0;
		Span.sourceLength = 1;
		Span.row = 0;
		Span.column = 0;
		return Span;
	}

	static asTypedSemanticSymbolId AddSymbol(
		asCTypedSemanticFunction& Function,
		asETypedSemanticSymbolKind Kind,
		const char* Name)
	{
		asSTypedSemanticSymbol Symbol;
		Symbol.kind = Kind;
		Symbol.name = Name;
		Symbol.type = asCDataType::CreatePrimitive(ttInt, false);
		Symbol.span = MakeSpan();
		return Function.AddSymbol(Symbol);
	}

	static asTypedSemanticExpressionId AddSymbolExpression(
		asCTypedSemanticFunction& Function,
		asTypedSemanticSymbolId Symbol,
		bool bBoolean = false)
	{
		asSTypedSemanticExpression Expression;
		Expression.kind = asETypedSemanticExpressionKind::Symbol;
		Expression.type = asCDataType::CreatePrimitive(bBoolean ? ttBool : ttInt, false);
		Expression.span = MakeSpan();
		Expression.symbol = Symbol;
		return Function.AddExpression(Expression);
	}

	static asTypedSemanticExpressionId AddLiteral(
		asCTypedSemanticFunction& Function,
		const char* Value,
		const asQWORD LiteralBits)
	{
		asSTypedSemanticExpression Expression;
		Expression.kind = asETypedSemanticExpressionKind::Literal;
		Expression.type = asCDataType::CreatePrimitive(ttInt, false);
		Expression.span = MakeSpan();
		Expression.literal = Value;
		Expression.literalBits = LiteralBits;
		return Function.AddExpression(Expression);
	}

	static asTypedSemanticExpressionId AddBinary(
		asCTypedSemanticFunction& Function,
		asTypedSemanticExpressionId Left,
		asTypedSemanticExpressionId Right,
		const char* Operator,
		bool bBooleanResult = false)
	{
		asSTypedSemanticExpression Expression;
		Expression.kind = asETypedSemanticExpressionKind::Binary;
		Expression.type = asCDataType::CreatePrimitive(bBooleanResult ? ttBool : ttInt, false);
		Expression.span = MakeSpan();
		Expression.operatorToken = Operator;
		Expression.operands.PushLast(Left);
		Expression.operands.PushLast(Right);
		asSTypedSemanticEvaluationStep LeftStep;
		LeftStep.role = asETypedSemanticEvaluationRole::Operand;
		LeftStep.expression = Left;
		Expression.evaluationSteps.PushLast(LeftStep);
		asSTypedSemanticEvaluationStep RightStep;
		RightStep.role = asETypedSemanticEvaluationRole::Operand;
		RightStep.expression = Right;
		Expression.evaluationSteps.PushLast(RightStep);
		return Function.AddExpression(Expression);
	}

	static asTypedSemanticStatementId AddReturn(
		asCTypedSemanticFunction& Function,
		asTypedSemanticExpressionId Value,
		const asUINT ExitedScopeCount = 1)
	{
		asSTypedSemanticStatement Statement;
		Statement.kind = asETypedSemanticStatementKind::Return;
		Statement.span = MakeSpan();
		Statement.safePointRole = asETypedSemanticSafePointRole::Return;
		Statement.valueExpression = Value;
		Statement.exitedScopeCount = ExitedScopeCount;
		return Function.AddStatement(Statement);
	}

	static asCTypedSemanticFunction MakeScalarBranch()
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int SemanticScalarBranch(int A, int B)";

		const asTypedSemanticSymbolId A = AddSymbol(
			Function,
			asETypedSemanticSymbolKind::Parameter,
			"A");
		const asTypedSemanticSymbolId B = AddSymbol(
			Function,
			asETypedSemanticSymbolKind::Parameter,
			"B");
		const asTypedSemanticSymbolId Sum = AddSymbol(
			Function,
			asETypedSemanticSymbolKind::Local,
			"Sum");

		const asTypedSemanticExpressionId ReadA = AddSymbolExpression(Function, A);
		const asTypedSemanticExpressionId ReadB = AddSymbolExpression(Function, B);
		const asTypedSemanticExpressionId Add = AddBinary(Function, ReadA, ReadB, "+");
		const asTypedSemanticExpressionId ReadSumForCondition = AddSymbolExpression(Function, Sum);
		const asTypedSemanticExpressionId Ten = AddLiteral(Function, "10", 10);
		const asTypedSemanticExpressionId Greater = AddBinary(
			Function,
			ReadSumForCondition,
			Ten,
			">",
			true);
		const asTypedSemanticExpressionId ReadSumForMultiply = AddSymbolExpression(Function, Sum);
		const asTypedSemanticExpressionId Two = AddLiteral(Function, "2", 2);
		const asTypedSemanticExpressionId Multiply = AddBinary(
			Function,
			ReadSumForMultiply,
			Two,
			"*");
		const asTypedSemanticExpressionId ReadSumForSubtract = AddSymbolExpression(Function, Sum);
		const asTypedSemanticExpressionId One = AddLiteral(Function, "1", 1);
		const asTypedSemanticExpressionId Subtract = AddBinary(
			Function,
			ReadSumForSubtract,
			One,
			"-");

		asSTypedSemanticStatement LocalDeclaration;
		LocalDeclaration.kind = asETypedSemanticStatementKind::LocalDeclaration;
		LocalDeclaration.span = MakeSpan();
		LocalDeclaration.symbol = Sum;
		LocalDeclaration.initializerExpression = Add;
		const asTypedSemanticStatementId Local = Function.AddStatement(LocalDeclaration);

		const asTypedSemanticStatementId ThenReturn = AddReturn(Function, Multiply, 2);
		asSTypedSemanticStatement ThenBlock;
		ThenBlock.kind = asETypedSemanticStatementKind::Block;
		ThenBlock.span = MakeSpan();
		ThenBlock.children.PushLast(ThenReturn);
		const asTypedSemanticStatementId Then = Function.AddStatement(ThenBlock);

		asSTypedSemanticStatement If;
		If.kind = asETypedSemanticStatementKind::If;
		If.span = MakeSpan();
		If.conditionExpression = Greater;
		If.thenStatement = Then;
		const asTypedSemanticStatementId Branch = Function.AddStatement(If);

		const asTypedSemanticStatementId FinalReturn = AddReturn(Function, Subtract);
		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(Local);
		Root.children.PushLast(Branch);
		Root.children.PushLast(FinalReturn);
		Function.rootStatement = Function.AddStatement(Root);
		asSTypedSemanticCleanupPlan EmptyCleanup;
		Function.header.universalCleanupPlan =
			Function.AddCleanupPlan(EmptyCleanup);
		Function.header.cleanupPlanState =
			asETypedSemanticCleanupPlanState::VerifiedEmpty;
		Function.header.cleanupPlanCoversAllTransfers = true;
		Function.statements[ThenReturn.value].cleanupPlan =
			Function.header.universalCleanupPlan;
		Function.statements[FinalReturn.value].cleanupPlan =
			Function.header.universalCleanupPlan;
		return Function;
	}

	static const char* GetExpectedScalarBranchDump()
	{
		return R"HIR(function int SemanticScalarBranch(int A, int B)
header traits=0 traitKnown=0 traitUnknown=0 traitPolicy=0 traitReceiver=0 traitInvocation=0 traitDispatch=0 traitBody=0 traitLifetime=0 traitProfile=0 invocation=Global receiver=None receiverSymbol=none callingModule=none bodyAvailability=Unspecified bodyOwnership=Unspecified bodyOwner=none receiverParameter=-1 receiverType=none compileOut=CompileCalls hiddenArgument=-1 determinesOutputType=-1 returnsOnStack=false suspend=false cleanup=false cleanupState=VerifiedEmpty cleanupPlan=C0 allTransfers=true
root=T5
symbols:
  S0 kind=Parameter name=A type=int span=input.as@0+1
  S1 kind=Parameter name=B type=int span=input.as@0+1
  S2 kind=Local name=Sum type=int span=input.as@0+1
expressions:
  E0 kind=Symbol type=int symbol=S0 span=input.as@0+1
  E1 kind=Symbol type=int symbol=S1 span=input.as@0+1
  E2 kind=Binary type=int operator=+ operands=[E0,E1] steps=[Operand(E0,formal=-1,source=-1),Operand(E1,formal=-1,source=-1)] span=input.as@0+1
  E3 kind=Symbol type=int symbol=S2 span=input.as@0+1
  E4 kind=Literal type=int value=10 bits=10 span=input.as@0+1
  E5 kind=Binary type=bool operator=> operands=[E3,E4] steps=[Operand(E3,formal=-1,source=-1),Operand(E4,formal=-1,source=-1)] span=input.as@0+1
  E6 kind=Symbol type=int symbol=S2 span=input.as@0+1
  E7 kind=Literal type=int value=2 bits=2 span=input.as@0+1
  E8 kind=Binary type=int operator=* operands=[E6,E7] steps=[Operand(E6,formal=-1,source=-1),Operand(E7,formal=-1,source=-1)] span=input.as@0+1
  E9 kind=Symbol type=int symbol=S2 span=input.as@0+1
  E10 kind=Literal type=int value=1 bits=1 span=input.as@0+1
  E11 kind=Binary type=int operator=- operands=[E9,E10] steps=[Operand(E9,formal=-1,source=-1),Operand(E10,formal=-1,source=-1)] span=input.as@0+1
statements:
  T0 kind=LocalDeclaration symbol=S2 initializer=E2 span=input.as@0+1
  T1 kind=Return value=E8 cleanup=C0 exitedScopes=2 safePoint=Return span=input.as@0+1
  T2 kind=Block children=[T1] span=input.as@0+1
  T3 kind=If condition=E5 then=T2 else=none span=input.as@0+1
  T4 kind=Return value=E11 cleanup=C0 exitedScopes=1 safePoint=Return span=input.as@0+1
  T5 kind=Block children=[T0,T3,T4] span=input.as@0+1
)HIR";
	}

	static std::string GetScalarBranchSource()
	{
		return ASTEST_AS_ANSI(R"AS(
			int SemanticScalarBranch(int A, int B)
			{
				int Sum = A + B;
				if (Sum > 10)
				{
					return Sum * 2;
				}
				return Sum - 1;
			}
			)AS");
	}

	static TArray<asDWORD> CopyBytecode(asIScriptFunction* Function)
	{
		TArray<asDWORD> Result;
		asUINT Length = 0;
		const asDWORD* const Words = Function != nullptr
			? Function->GetByteCode(&Length)
			: nullptr;
		if (Words != nullptr && Length > 0)
		{
			Result.Append(Words, static_cast<int32>(Length));
		}
		return Result;
	}

	static TArray<asDWORD> CopyBytecodeWithSystemCallTargetsNormalized(
		asIScriptFunction* Function,
		int32& OutSystemCallCount)
	{
		OutSystemCallCount = 0;
		TArray<asDWORD> Result = CopyBytecode(Function);
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
				return Result;
			}
			if (Opcode == asBC_CALLSYS)
			{
				if (asBCInfo[Opcode].type != asBCTYPE_PTR_ARG
					|| InstructionSize != 1 + AS_PTR_SIZE)
				{
					Result.Reset();
					OutSystemCallCount = 0;
					return Result;
				}
				for (int32 PointerWord = 1;
					PointerWord < InstructionSize;
					++PointerWord)
				{
					Result[Offset + static_cast<asUINT>(PointerWord)] = 0;
				}
				++OutSystemCallCount;
			}
			Offset += static_cast<asUINT>(InstructionSize);
		}
		return Result;
	}

	static bool ExecuteScalar(
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		int32 A,
		int32 B,
		int32& OutResult)
	{
		if (Engine == nullptr || Function == nullptr)
		{
			return false;
		}
		asIScriptContext* const Context = Engine->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		if (Context->Prepare(Function) < 0
			|| Context->SetArgDWord(0, static_cast<asDWORD>(A)) < 0
			|| Context->SetArgDWord(1, static_cast<asDWORD>(B)) < 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		OutResult = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

	static bool ExecuteUnaryScalar(
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		int32 Input,
		int32& OutResult)
	{
		if (Engine == nullptr || Function == nullptr)
		{
			return false;
		}
		asIScriptContext* const Context = Engine->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		if (Context->Prepare(Function) < 0
			|| Context->SetArgDWord(0, static_cast<asDWORD>(Input)) < 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		OutResult = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

public:
	TEST_METHOD(CaptureDefaultsOffAndScalarCaptureIsObservational)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SCALAR-CAPTURE",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string Source = GetScalarBranchSource();
		FNativeTestEngine CaptureOffEngine;
		FNativeTestEngine CaptureOnEngine;
		FNativeTestEngine RepeatEngine;
		CaptureOffEngine.Create(*TestRunner);
		CaptureOnEngine.Create(*TestRunner);
		RepeatEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			RepeatEngine.Destroy();
			CaptureOnEngine.Destroy();
			CaptureOffEngine.Destroy();
		};

		asCScriptEngine* const CaptureOffRaw = static_cast<asCScriptEngine*>(CaptureOffEngine.Get());
		asCScriptEngine* const CaptureOnRaw = static_cast<asCScriptEngine*>(CaptureOnEngine.Get());
		asCScriptEngine* const RepeatRaw = static_cast<asCScriptEngine*>(RepeatEngine.Get());
		ASSERT_THAT(IsFalse(CaptureOffRaw->IsTypedSemanticIRCaptureEnabled(),
			TEXT("Typed semantic capture must default off on every engine")));
		CaptureOnRaw->SetTypedSemanticIRCapture(true);
		RepeatRaw->SetTypedSemanticIRCapture(true);
		ASSERT_THAT(IsTrue(CaptureOnRaw->IsTypedSemanticIRCaptureEnabled(),
			TEXT("The fork-private switch should enable capture before module build")));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRScalar",
			Source);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRScalar",
			Source);

		const FString PersistedHIRRoot = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/TypedSemanticIRNonReadback"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		ASSERT_THAT(IsTrue(
			IFileManager::Get().MakeDirectory(*PersistedHIRRoot, true)));
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(
				*PersistedHIRRoot, false, true);
		};
		const FString StaleTextPath = FPaths::Combine(
			PersistedHIRRoot, TEXT("SemanticScalarBranch.hir.txt"));
		const FString EditedJsonPath = FPaths::Combine(
			PersistedHIRRoot, TEXT("SemanticScalarBranch.hir.json"));
		const FString MaliciousTextPath = FPaths::Combine(
			PersistedHIRRoot, TEXT("Injected.hir.txt"));
		const FString MissingJsonPath = FPaths::Combine(
			PersistedHIRRoot, TEXT("Missing.hir.json"));
		const FString StaleText =
			TEXT("RequestKind=TestHIRSnapshot\n")
			TEXT("State=Verified\n")
			TEXT("function int SemanticScalarBranch(int A, int B) { return 777; }\n");
		const FString EditedJson =
			TEXT("{\"requestKind\":\"TestHIRSnapshot\",\"state\":\"Verified\",")
			TEXT("\"normalizedHIR\":\"return 888\"}");
		const FString MaliciousText =
			TEXT("RequestKind=StaticJITArtifact\n")
			TEXT("ProviderId=ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff\n")
			TEXT("void Injected() { *((int*)0x1) = 999; }\n");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			StaleText, *StaleTextPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			EditedJson, *EditedJsonPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			MaliciousText, *MaliciousTextPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(*MissingJsonPath)));

		FScopedNativeModule RepeatModule(
			*TestRunner,
			RepeatEngine,
			"TypedSemanticIRScalar",
			Source);
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid() || !RepeatModule.IsValid())
		{
			return;
		}

		asCScriptFunction* const CaptureOffFunction = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(CaptureOffModule, "int SemanticScalarBranch(int A, int B)"));
		asCScriptFunction* const CaptureOnFunction = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(CaptureOnModule, "int SemanticScalarBranch(int A, int B)"));
		asCScriptFunction* const RepeatFunction = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(RepeatModule, "int SemanticScalarBranch(int A, int B)"));
		ASSERT_THAT(IsNotNull(CaptureOffFunction,
			TEXT("Capture-off should still compile the scalar function")));
		ASSERT_THAT(IsNotNull(CaptureOnFunction,
			TEXT("Capture-on should still compile the scalar function")));
		ASSERT_THAT(IsNotNull(RepeatFunction,
			TEXT("Repeated capture should compile the scalar function")));
		if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr || RepeatFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNull(CaptureOffFunction->GetTypedSemanticFunction(),
			TEXT("Capture-off compilation must not publish a sidecar")));
		const asCTypedSemanticFunction* const Captured = CaptureOnFunction->GetTypedSemanticFunction();
		const asCTypedSemanticFunction* const Repeated = RepeatFunction->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(Captured,
			TEXT("Capture-on compilation should publish one verified function-owned HIR")));
		ASSERT_THAT(IsNotNull(Repeated,
			TEXT("Repeated capture should publish one verified function-owned HIR")));
		if (Captured == nullptr || Repeated == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*Captured).IsValid(),
			TEXT("The captured scalar HIR should pass the structural verifier after parser teardown")));
		ASSERT_THAT(IsTrue(
			Captured->header.cleanupPlanState
				== asETypedSemanticCleanupPlanState::VerifiedEmpty,
			TEXT("A scalar branch with nested lexical blocks but no object lifetimes should publish a compiler-verified empty cleanup plan")));
		ASSERT_THAT(AreEqual(
			FString(UTF8_TO_TCHAR(DumpTypedSemanticFunction(*Captured).AddressOf())),
			FString(UTF8_TO_TCHAR(DumpTypedSemanticFunction(*Repeated).AddressOf())),
			TEXT("Repeated source compilation should produce identical normalized HIR")));
		ASSERT_THAT(IsFalse(
			FString(UTF8_TO_TCHAR(DumpTypedSemanticFunction(*Captured).AddressOf())).Contains(TEXT("0x")),
			TEXT("Captured HIR must not retain or dump parser-node addresses")));
		const asSTypedSemanticSnapshotResult CapturedSnapshot =
			BuildTypedSemanticSnapshot(Captured);
		const asSTypedSemanticSnapshotResult RepeatedSnapshot =
			BuildTypedSemanticSnapshot(Repeated);
		ASSERT_THAT(IsTrue(CapturedSnapshot.IsValid(),
			TEXT("The compiler snapshot helper should accept verified captured HIR")));
		ASSERT_THAT(AreEqual(
			FString(UTF8_TO_TCHAR(CapturedSnapshot.text.AddressOf())),
			FString(UTF8_TO_TCHAR(RepeatedSnapshot.text.AddressOf())),
			TEXT("Independent test-owned Engines should produce identical HIR snapshot text")));
		ASSERT_THAT(AreEqual(
			FString(UTF8_TO_TCHAR(CapturedSnapshot.json.AddressOf())),
			FString(UTF8_TO_TCHAR(RepeatedSnapshot.json.AddressOf())),
			TEXT("Independent test-owned Engines should produce identical HIR snapshot JSON")));
		ASSERT_THAT(IsTrue(
			FString(UTF8_TO_TCHAR(CapturedSnapshot.text.AddressOf())).Contains(
				TEXT("RequestKind=TestHIRSnapshot")),
			TEXT("Compiler snapshot text should identify its non-Static request kind")));
		ASSERT_THAT(IsFalse(
			FString(UTF8_TO_TCHAR(CapturedSnapshot.json.AddressOf())).Contains(TEXT("0x")),
			TEXT("Compiler snapshot JSON must not contain process pointer spellings")));

		ASSERT_THAT(AreEqual(
			CopyBytecode(CaptureOffFunction),
			CopyBytecode(CaptureOnFunction),
			TEXT("Capture must not change any bytecode word")));
		ASSERT_THAT(AreEqual(
			CopyBytecode(CaptureOnFunction),
			CopyBytecode(RepeatFunction),
			TEXT("Persisted HIR files must not change later compiler bytecode")));
		FString PersistedTextAfterCompile;
		FString PersistedJsonAfterCompile;
		FString MaliciousTextAfterCompile;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			PersistedTextAfterCompile, *StaleTextPath)));
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			PersistedJsonAfterCompile, *EditedJsonPath)));
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			MaliciousTextAfterCompile, *MaliciousTextPath)));
		ASSERT_THAT(AreEqual(StaleText, PersistedTextAfterCompile));
		ASSERT_THAT(AreEqual(EditedJson, PersistedJsonAfterCompile));
		ASSERT_THAT(AreEqual(MaliciousText, MaliciousTextAfterCompile));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(*MissingJsonPath)));

		struct FCase
		{
			int32 A;
			int32 B;
			int32 Expected;
		};
		const FCase Cases[] = {{3, 4, 6}, {6, 7, 26}, {-5, 2, -4}, {10, 0, 9}};
		for (const FCase& Case : Cases)
		{
			int32 CaptureOffResult = 0;
			int32 CaptureOnResult = 0;
			int32 RepeatResult = 0;
			ASSERT_THAT(IsTrue(ExecuteScalar(
				CaptureOffEngine.Get(), CaptureOffFunction, Case.A, Case.B, CaptureOffResult),
				TEXT("Capture-off scalar execution should finish")));
			ASSERT_THAT(IsTrue(ExecuteScalar(
				CaptureOnEngine.Get(), CaptureOnFunction, Case.A, Case.B, CaptureOnResult),
				TEXT("Capture-on scalar execution should finish")));
			ASSERT_THAT(IsTrue(ExecuteScalar(
				RepeatEngine.Get(), RepeatFunction, Case.A, Case.B, RepeatResult),
				TEXT("Persisted-HIR-adjacent scalar execution should finish")));
			ASSERT_THAT(AreEqual(Case.Expected, CaptureOffResult,
				TEXT("Capture-off VM result should match the scalar oracle")));
			ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult,
				TEXT("Capture-on VM behavior must equal capture-off behavior")));
			ASSERT_THAT(AreEqual(CaptureOnResult, RepeatResult,
				TEXT("Persisted HIR files must not change later VM behavior")));
		}
	}

	TEST_METHOD(ResolvedGlobalCallCapturesFormalBindingsAndReverseEvaluation)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-RESOLVED-GLOBAL-CALL",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* const RawEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		RawEngine->SetTypedSemanticIRCapture(true);

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int ObserveCallTarget(int A, int B, int C)
			{
				return A * 100 + B * 10 + C;
			}

			int ObserveCallEntry(int A, int B, int C)
			{
				return ObserveCallTarget(A, B, C);
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedSemanticIRResolvedGlobalCall",
			Source);
		if (!Module.IsValid())
		{
			return;
		}

		asCScriptFunction* const Target = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				Module, "int ObserveCallTarget(int A, int B, int C)"));
		asCScriptFunction* const Entry = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				Module, "int ObserveCallEntry(int A, int B, int C)"));
		ASSERT_THAT(IsNotNull(Target));
		ASSERT_THAT(IsNotNull(Entry));
		if (Target == nullptr || Entry == nullptr)
		{
			return;
		}

		const asCTypedSemanticFunction* const HIR =
			Entry->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR,
			TEXT("The source caller should publish one verified HIR sidecar")));
		if (HIR == nullptr)
		{
			return;
		}

		const asSTypedSemanticExpression* Call = nullptr;
		for (asUINT Index = 0; Index < HIR->expressions.GetLength(); ++Index)
		{
			if (HIR->expressions[Index].kind
				== asETypedSemanticExpressionKind::ResolvedCall)
			{
				Call = &HIR->expressions[Index];
				break;
			}
		}
		ASSERT_THAT(IsNotNull(Call,
			TEXT("The final overload-selected source call must become ResolvedCall HIR")));
		if (Call == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(Target->GetId(), Call->resolvedFunctionId));
		ASSERT_THAT(AreEqual(3,
			static_cast<int32>(Call->operands.GetLength())));
		ASSERT_THAT(AreEqual(3,
			static_cast<int32>(Call->formalParameterIndices.GetLength())));
		ASSERT_THAT(AreEqual(3,
			static_cast<int32>(Call->evaluationSequence.GetLength())));
		for (asUINT FormalIndex = 0; FormalIndex < 3; ++FormalIndex)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(FormalIndex),
				Call->formalParameterIndices[FormalIndex]));
			const asTypedSemanticExpressionId Operand =
				Call->operands[FormalIndex];
			ASSERT_THAT(IsTrue(Operand.IsValid()));
			ASSERT_THAT(AreEqual(static_cast<int32>(FormalIndex),
				static_cast<int32>(
					HIR->expressions[Operand.value].symbol.value)));
			ASSERT_THAT(AreEqual(static_cast<int32>(2 - FormalIndex),
				static_cast<int32>(
					Call->evaluationSequence[FormalIndex])));
		}
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
	}

	TEST_METHOD(ResolvedSystemCallPreservesTargetKindAndVmBehavior)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-RESOLVED-SYSTEM-CALL",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		FNativeTestEngine CaptureOffEngine;
		FNativeTestEngine CaptureOnEngine;
		CaptureOffEngine.Create(*TestRunner);
		CaptureOnEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			CaptureOnEngine.Destroy();
			CaptureOffEngine.Destroy();
		};
		ASSERT_THAT(IsTrue(
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get())
				->SetTypedSemanticIRCapture(true),
			TEXT("Capture-on must be selected before the Engine's first build")));

		const char* const NativeDeclaration =
			"int ObserveSystemCallTarget(int A, int B, int C)";
		const ASAutoCaller::FunctionCaller NativeCaller =
			ASAutoCaller::MakeFunctionCaller(
				TypedSemanticIRTest::ObserveSystemCallTarget);
		const int CaptureOffNativeFunctionId =
			CaptureOffEngine.Get()->RegisterGlobalFunction(
			NativeDeclaration,
			asFUNCTION(TypedSemanticIRTest::ObserveSystemCallTarget),
			asCALL_CDECL,
			*(asFunctionCaller*)&NativeCaller);
		const int CaptureOnNativeFunctionId =
			CaptureOnEngine.Get()->RegisterGlobalFunction(
			NativeDeclaration,
			asFUNCTION(TypedSemanticIRTest::ObserveSystemCallTarget),
			asCALL_CDECL,
			*(asFunctionCaller*)&NativeCaller);
		ASSERT_THAT(IsTrue(CaptureOffNativeFunctionId >= 0));
		ASSERT_THAT(IsTrue(CaptureOnNativeFunctionId >= 0));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int ObserveSystemCallEntry(int Seed)
			{
				return ObserveSystemCallTarget(
					Seed,
					Seed + 1,
					Seed + 2);
			}
			)AS");
		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRSystemCallOff",
			Source);
		if (!CaptureOffModule.IsValid())
		{
			return;
		}
		asCScriptFunction* const CaptureOffFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOffModule,
				"int ObserveSystemCallEntry(int Seed)"));
		ASSERT_THAT(IsNotNull(CaptureOffFunction));
		if (CaptureOffFunction == nullptr)
		{
			return;
		}
		int32 CaptureOffSystemCallCount = 0;
		const TArray<asDWORD> CaptureOffBytecode =
			CopyBytecodeWithSystemCallTargetsNormalized(
				CaptureOffFunction,
				CaptureOffSystemCallCount);
		ASSERT_THAT(AreEqual(1, CaptureOffSystemCallCount,
			TEXT("Capture-off bytecode should contain one CALLSYS instruction")));
		int32 CaptureOffResult = 0;
		ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
			CaptureOffEngine.Get(), CaptureOffFunction, 4, CaptureOffResult)));

		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRSystemCallOn",
			Source);
		if (!CaptureOnModule.IsValid())
		{
			return;
		}

		asCScriptFunction* const CaptureOnFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOnModule,
				"int ObserveSystemCallEntry(int Seed)"));
		ASSERT_THAT(IsNotNull(CaptureOnFunction));
		if (CaptureOnFunction == nullptr)
		{
			return;
		}
		const asCTypedSemanticFunction* const PreExecutionHIR =
			CaptureOnFunction->GetTypedSemanticFunction();
		const FString PreExecutionDiagnostic = UTF8_TO_TCHAR(
			CaptureOnFunction->GetTypedSemanticCaptureDiagnostic().AddressOf());
		ASSERT_THAT(IsNotNull(PreExecutionHIR,
			*FString::Printf(
				TEXT("A resolved system call should publish verified HIR before VM execution: %s"),
				*PreExecutionDiagnostic)));

		int32 CaptureOnSystemCallCount = 0;
		const TArray<asDWORD> CaptureOnBytecode =
			CopyBytecodeWithSystemCallTargetsNormalized(
				CaptureOnFunction,
				CaptureOnSystemCallCount);
		ASSERT_THAT(AreEqual(1, CaptureOnSystemCallCount,
			TEXT("Capture-on bytecode should contain one CALLSYS instruction")));
		ASSERT_THAT(AreEqual(
			CaptureOffBytecode,
			CaptureOnBytecode,
			TEXT("System-call HIR capture must not change bytecode after normalizing only the Engine-local CALLSYS pointer")));
		int32 CaptureOnResult = 0;
		ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
			CaptureOnEngine.Get(), CaptureOnFunction, 4, CaptureOnResult)));
		ASSERT_THAT(AreEqual(456, CaptureOffResult));
		ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult,
			TEXT("System-call HIR capture must preserve VM behavior")));

		const asCTypedSemanticFunction* const HIR =
			CaptureOnFunction->GetTypedSemanticFunction();
		const FString CaptureDiagnostic = UTF8_TO_TCHAR(
			CaptureOnFunction->GetTypedSemanticCaptureDiagnostic().AddressOf());
		ASSERT_THAT(IsNotNull(HIR,
			*FString::Printf(
				TEXT("A resolved system call should publish verified HIR: %s"),
				*CaptureDiagnostic)));
		ASSERT_THAT(IsTrue(HIR == PreExecutionHIR,
			TEXT("VM execution must not replace or discard the compiler-owned HIR sidecar")));
		if (HIR == nullptr)
		{
			return;
		}

		const asSTypedSemanticExpression* Call = nullptr;
		for (asUINT Index = 0; Index < HIR->expressions.GetLength(); ++Index)
		{
			if (HIR->expressions[Index].kind
				== asETypedSemanticExpressionKind::ResolvedCall)
			{
				Call = &HIR->expressions[Index];
				break;
			}
		}
		ASSERT_THAT(IsNotNull(Call));
		if (Call == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			asETypedSemanticCallTargetKind::SystemFunction,
			Call->callTargetKind));
		ASSERT_THAT(AreEqual(
			CaptureOnNativeFunctionId,
			Call->resolvedFunctionId));
		ASSERT_THAT(AreEqual(3,
			static_cast<int32>(Call->operands.GetLength())));
		for (asUINT Index = 0; Index < 3; ++Index)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(Index),
				Call->formalParameterIndices[Index]));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(2 - Index),
				static_cast<int32>(Call->evaluationSequence[Index])));
		}
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("target=SystemFunction"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(ImportedCallCapturesMutableSlotWithoutFreezingBinding)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-IMPORTED-CALL-SLOT",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* const RawEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(RawEngine));
		if (RawEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(RawEngine->SetTypedSemanticIRCapture(true),
			TEXT("Imported-call HIR capture must be selected before source compilation")));

		FScopedNativeModule ProviderA(
			*TestRunner,
			Engine,
			"TypedSemanticIRImportProviderA",
			ASTEST_AS_ANSI(R"AS(
				int ImportedSemanticValue(int Value)
				{
					return Value + 10;
				}
			)AS"));
		FScopedNativeModule ProviderB(
			*TestRunner,
			Engine,
			"TypedSemanticIRImportProviderB",
			ASTEST_AS_ANSI(R"AS(
				int ImportedSemanticValue(int Value)
				{
					return Value + 20;
				}
			)AS"));
		FScopedNativeModule Consumer(
			*TestRunner,
			Engine,
			"TypedSemanticIRImportConsumer",
			ASTEST_AS_ANSI(R"AS(
				import int ImportedSemanticValue(int Value)
					from "TypedSemanticIRImportProviderA";

				int ImportedSemanticEntry(int Value)
				{
					return ImportedSemanticValue(Value);
				}
			)AS"));
		if (!ProviderA.IsValid() || !ProviderB.IsValid()
			|| !Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asCScriptFunction* const ProviderFunctionA =
			static_cast<asCScriptFunction*>(ProviderA->GetFunctionByDecl(
				"int ImportedSemanticValue(int Value)"));
		asCScriptFunction* const ProviderFunctionB =
			static_cast<asCScriptFunction*>(ProviderB->GetFunctionByDecl(
				"int ImportedSemanticValue(int Value)"));
		asCScriptFunction* const Entry = static_cast<asCScriptFunction*>(
			Consumer->GetFunctionByDecl(
				"int ImportedSemanticEntry(int Value)"));
		asCModule* const ConsumerModule =
			static_cast<asCModule*>(Consumer.Get());
		asCScriptFunction* const ImportedSignature =
			ConsumerModule != nullptr
				? ConsumerModule->GetImportedFunction(0)
				: nullptr;
		ASSERT_THAT(IsNotNull(ProviderFunctionA));
		ASSERT_THAT(IsNotNull(ProviderFunctionB));
		ASSERT_THAT(IsNotNull(Entry));
		ASSERT_THAT(IsNotNull(ImportedSignature));
		if (ProviderFunctionA == nullptr || ProviderFunctionB == nullptr
			|| Entry == nullptr || ImportedSignature == nullptr)
		{
			return;
		}

		const asCTypedSemanticFunction* const HIR =
			Entry->GetTypedSemanticFunction();
		const FString CaptureDiagnostic = UTF8_TO_TCHAR(
			Entry->GetTypedSemanticCaptureDiagnostic().AddressOf());
		ASSERT_THAT(IsNotNull(HIR,
			*FString::Printf(
				TEXT("An imported call must publish verified HIR before any binding is selected: %s"),
				*CaptureDiagnostic)));
		if (HIR == nullptr)
		{
			return;
		}

		const asSTypedSemanticExpression* ImportedCall = nullptr;
		for (asUINT Index = 0; Index < HIR->expressions.GetLength(); ++Index)
		{
			if (HIR->expressions[Index].kind
				== asETypedSemanticExpressionKind::ResolvedCall)
			{
				ImportedCall = &HIR->expressions[Index];
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ImportedCall,
			TEXT("The imported source call must remain an explicit ResolvedCall")));
		if (ImportedCall == nullptr)
		{
			return;
		}

		const int32 ImportedSlotFunctionId = ImportedSignature->GetId();
		ASSERT_THAT(IsTrue(
			ImportedCall->callTargetKind
				== asETypedSemanticCallTargetKind::ImportedFunction,
			TEXT("HIR must type an imported slot separately from concrete script/system functions")));
		ASSERT_THAT(IsTrue((ImportedSlotFunctionId & FUNC_IMPORTED) != 0,
			TEXT("The captured coordinate must identify an imported slot")));
		ASSERT_THAT(AreEqual(
			ImportedSlotFunctionId,
			ImportedCall->resolvedFunctionId,
			TEXT("HIR must retain the import slot coordinate instead of a mutable bound FunctionId")));
		ASSERT_THAT(IsTrue(
			ImportedCall->resolvedFunctionId != ProviderFunctionA->GetId()
				&& ImportedCall->resolvedFunctionId != ProviderFunctionB->GetId(),
			TEXT("HIR must not freeze either compatible provider target")));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("target=ImportedFunction"),
			ESearchCase::CaseSensitive),
			TEXT("The deterministic HIR dump must distinguish imported slots from concrete script/system targets")));

		int32 ImportedSignatureDependencyCount = 0;
		int32 ImportedContentDependencyCount = 0;
		for (asUINT DependencyIndex = 0;
			DependencyIndex < Entry->scriptData->artifactDependencies.GetLength();
			++DependencyIndex)
		{
			const asSBuildArtifactDependency& Dependency =
				Entry->scriptData->artifactDependencies[DependencyIndex];
			if (Dependency.referenceKind
					!= asBUILD_ARTIFACT_REFERENCE_FUNCTION
				|| Dependency.function != ImportedSignature)
			{
				continue;
			}
			ImportedSignatureDependencyCount += Dependency.kind
				== asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE ? 1 : 0;
			ImportedContentDependencyCount += Dependency.kind
				== asBUILD_ARTIFACT_DEPENDENCY_FUNCTION_CONTENT ? 1 : 0;
		}
		ASSERT_THAT(AreEqual(
			1,
			ImportedSignatureDependencyCount,
			TEXT("The compiler artifact facts must retain the imported slot signature")));
		ASSERT_THAT(AreEqual(
			0,
			ImportedContentDependencyCount,
			TEXT("A mutable imported slot must never freeze a provider implementation-content dependency")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Consumer->BindImportedFunction(0, ProviderFunctionA)));
		int32 ProviderAResult = 0;
		ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
			RawEngine, Entry, 5, ProviderAResult)));
		ASSERT_THAT(AreEqual(15, ProviderAResult));
		ASSERT_THAT(AreEqual(
			ImportedSlotFunctionId,
			ImportedCall->resolvedFunctionId));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Consumer->BindImportedFunction(0, ProviderFunctionB)));
		int32 ProviderBResult = 0;
		ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
			RawEngine, Entry, 5, ProviderBResult)));
		ASSERT_THAT(AreEqual(25, ProviderBResult));
		ASSERT_THAT(AreEqual(
			ImportedSlotFunctionId,
			ImportedCall->resolvedFunctionId));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Consumer->UnbindImportedFunction(0)));
		asIScriptContext* const Context = RawEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context != nullptr)
		{
			ON_SCOPE_EXIT { Context->Release(); };
			ASSERT_THAT(IsTrue(Context->Prepare(Entry) >= 0));
			ASSERT_THAT(IsTrue(Context->SetArgDWord(0, 5) >= 0));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_EXCEPTION),
				Context->Execute()));
			ASSERT_THAT(AreEqual(
				FString(TEXT("Unbound function called")),
				FString(UTF8_TO_TCHAR(Context->GetExceptionString()))));
		}
		ASSERT_THAT(AreEqual(
			ImportedSlotFunctionId,
			ImportedCall->resolvedFunctionId));
	}

	TEST_METHOD(CompilerCapturesSourceAndSafePointRolesWithoutBytecodeInspection)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SAFE-POINT-ROLES",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		static_cast<asCScriptEngine*>(Engine.Get())
			->SetTypedSemanticIRCapture(true);

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedSemanticIRSafePointRoles",
			ASTEST_AS_ANSI(R"AS(
				int SafePointTarget(int Value)
				{
					return Value + 1;
				}

				int SafePointEntry(int Value)
				{
					int Result = SafePointTarget(Value);
					if (Result > 0)
					{
						return Result;
					}
					else
					{
						return 0;
					}
				}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		asCScriptFunction* const Entry = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				Module,
				"int SafePointEntry(int Value)"));
		ASSERT_THAT(IsNotNull(Entry));
		if (Entry == nullptr)
		{
			return;
		}

		const asCTypedSemanticFunction* const HIR =
			Entry->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR));
		if (HIR == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
		ASSERT_THAT(IsTrue(HIR->rootStatement.IsValid()));
		const asSTypedSemanticStatement& Root =
			HIR->statements[HIR->rootStatement.value];
		ASSERT_THAT(AreEqual(
			asETypedSemanticSafePointRole::FunctionEntry,
			Root.safePointRole,
			TEXT("The compiler-owned root must retain an explicit function-entry role")));

		const asSTypedSemanticExpression* Call = nullptr;
		const asSTypedSemanticStatement* Local = nullptr;
		const asSTypedSemanticStatement* Branch = nullptr;
		int32 ReturnCount = 0;
		for (asUINT Index = 0; Index < HIR->expressions.GetLength(); ++Index)
		{
			if (HIR->expressions[Index].kind
				== asETypedSemanticExpressionKind::ResolvedCall)
			{
				Call = &HIR->expressions[Index];
				break;
			}
		}
		for (asUINT Index = 0; Index < HIR->statements.GetLength(); ++Index)
		{
			const asSTypedSemanticStatement& Statement = HIR->statements[Index];
			if (Statement.kind
				== asETypedSemanticStatementKind::LocalDeclaration)
			{
				Local = &Statement;
			}
			else if (Statement.kind == asETypedSemanticStatementKind::If)
			{
				Branch = &Statement;
			}
			else if (Statement.kind == asETypedSemanticStatementKind::Return)
			{
				++ReturnCount;
				ASSERT_THAT(AreEqual(
					asETypedSemanticSafePointRole::Return,
					Statement.safePointRole));
			}
		}

		ASSERT_THAT(IsNotNull(Call));
		ASSERT_THAT(IsNotNull(Local));
		ASSERT_THAT(IsNotNull(Branch));
		ASSERT_THAT(AreEqual(2, ReturnCount));
		if (Call == nullptr || Local == nullptr || Branch == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asETypedSemanticSafePointRole::Call,
			Call->safePointRole,
			TEXT("The resolved call must retain an explicit call-boundary role")));
		ASSERT_THAT(AreEqual(
			asETypedSemanticSafePointRole::Statement,
			Local->safePointRole,
			TEXT("The local declaration must retain its source-statement role")));
		ASSERT_THAT(AreEqual(
			asETypedSemanticSafePointRole::Statement,
			Branch->safePointRole,
			TEXT("The branch condition must retain its source-statement role")));
		ASSERT_THAT(IsTrue(Branch->thenStatement.IsValid()));
		ASSERT_THAT(IsTrue(Branch->elseStatement.IsValid()));
		if (Branch->thenStatement.IsValid()
			&& Branch->elseStatement.IsValid())
		{
			ASSERT_THAT(AreEqual(
				asETypedSemanticStatementKind::Block,
				HIR->statements[Branch->thenStatement.value].kind));
			ASSERT_THAT(AreEqual(
				asETypedSemanticStatementKind::Block,
				HIR->statements[Branch->elseStatement.value].kind));
		}

		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		const TCHAR* const RequiredDumpRoles[] = {
			TEXT("safePoint=FunctionEntry"),
			TEXT("safePoint=Statement"),
			TEXT("safePoint=Call"),
			TEXT("safePoint=Return"),
			TEXT("else=T"),
		};
		for (const TCHAR* const RequiredRole : RequiredDumpRoles)
		{
			ASSERT_THAT(IsTrue(Dump.Contains(
				RequiredRole,
				ESearchCase::CaseSensitive),
				*FString::Printf(
					TEXT("The pointer-free normalized dump must expose %s"),
					RequiredRole)));
		}
	}

	TEST_METHOD(CompilerCapturesReturnCleanupAcrossEveryExitedLexicalScope)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-RETURN-CLEANUP",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int NestedReturnCleanup(int Value)
			{
				if (Value > 0)
				{
					{
						int Nested = Value + 1;
						return Nested;
					}
				}
				return Value - 1;
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
		static_cast<asCScriptEngine*>(CaptureOnEngine.Get())
			->SetTypedSemanticIRCapture(true);

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRReturnCleanupOff",
			Source);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRReturnCleanupOn",
			Source);
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asCScriptFunction* const CaptureOffFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOffModule,
				"int NestedReturnCleanup(int Value)"));
		asCScriptFunction* const CaptureOnFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOnModule,
				"int NestedReturnCleanup(int Value)"));
		ASSERT_THAT(IsNotNull(CaptureOffFunction));
		ASSERT_THAT(IsNotNull(CaptureOnFunction));
		if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			CopyBytecode(CaptureOffFunction),
			CopyBytecode(CaptureOnFunction),
			TEXT("Return cleanup capture must not change any bytecode word")));
		const int32 Inputs[] = {3, -2};
		const int32 Expected[] = {4, -3};
		for (int32 Index = 0; Index < 2; ++Index)
		{
			int32 CaptureOffResult = 0;
			int32 CaptureOnResult = 0;
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOffEngine.Get(),
				CaptureOffFunction,
				Inputs[Index],
				CaptureOffResult)));
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOnEngine.Get(),
				CaptureOnFunction,
				Inputs[Index],
				CaptureOnResult)));
			ASSERT_THAT(AreEqual(Expected[Index], CaptureOffResult));
			ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult));
		}

		const asCTypedSemanticFunction* const HIR =
			CaptureOnFunction->GetTypedSemanticFunction();
		const FString CaptureDiagnostic = UTF8_TO_TCHAR(
			CaptureOnFunction->GetTypedSemanticCaptureDiagnostic().AddressOf());
		ASSERT_THAT(IsNotNull(
			HIR,
			*FString::Printf(
				TEXT("Nested scalar returns must publish HIR: %s"),
				*CaptureDiagnostic)));
		if (HIR == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
		ASSERT_THAT(AreEqual(
			asETypedSemanticCleanupPlanState::VerifiedEmpty,
			HIR->header.cleanupPlanState));
		ASSERT_THAT(IsTrue(HIR->header.universalCleanupPlan.IsValid()));

		int32 ReturnCount = 0;
		asUINT MinimumExitedScopes = MAX_uint32;
		asUINT MaximumExitedScopes = 0;
		asTypedSemanticStatementId FirstReturn;
		for (asUINT StatementIndex = 0;
			StatementIndex < HIR->statements.GetLength();
			++StatementIndex)
		{
			const asSTypedSemanticStatement& Statement =
				HIR->statements[StatementIndex];
			if (Statement.kind != asETypedSemanticStatementKind::Return)
			{
				continue;
			}

			if (!FirstReturn.IsValid())
			{
				FirstReturn = Statement.id;
			}
			++ReturnCount;
			MinimumExitedScopes = FMath::Min(
				MinimumExitedScopes,
				Statement.exitedScopeCount);
			MaximumExitedScopes = FMath::Max(
				MaximumExitedScopes,
				Statement.exitedScopeCount);
			ASSERT_THAT(AreEqual(
				asETypedSemanticSafePointRole::Return,
				Statement.safePointRole));
			ASSERT_THAT(IsTrue(Statement.cleanupPlan.IsValid()));
			if (Statement.cleanupPlan.IsValid())
			{
				ASSERT_THAT(AreEqual(
					HIR->header.universalCleanupPlan.value,
					Statement.cleanupPlan.value));
			}
		}

		ASSERT_THAT(AreEqual(2, ReturnCount));
		ASSERT_THAT(IsTrue(MinimumExitedScopes > 0));
		ASSERT_THAT(IsTrue(MaximumExitedScopes > MinimumExitedScopes));

		if (FirstReturn.IsValid())
		{
			asCTypedSemanticFunction MissingReturnCleanup = *HIR;
			MissingReturnCleanup.statements[FirstReturn.value].cleanupPlan =
				asTypedSemanticCleanupPlanId();
			ASSERT_THAT(AreEqual(
				asETypedSemanticVerificationError::InvalidCleanupPlan,
				VerifyTypedSemanticFunction(MissingReturnCleanup).error));
		}

		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("kind=Return"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("exitedScopes="),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(CompilerCapturesStructuredWhilePhasesAndSafePointTargets)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-STRUCTURED-WHILE",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int CountStructuredWhile(int Value)
			{
				int Current = Value;
				int Iterations = 0;
				while (Current > 0)
				{
					Current = Current - 1;
					Iterations = Iterations + 1;
				}
				return Iterations;
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
		ASSERT_THAT(IsTrue(
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get())
				->SetTypedSemanticIRCapture(true)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRStructuredWhileOff",
			Source);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRStructuredWhileOn",
			Source);
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		const char* const Declaration =
			"int CountStructuredWhile(int Value)";
		asCScriptFunction* const CaptureOffFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOffModule,
				Declaration));
		asCScriptFunction* const CaptureOnFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOnModule,
				Declaration));
		ASSERT_THAT(IsNotNull(CaptureOffFunction));
		ASSERT_THAT(IsNotNull(CaptureOnFunction));
		if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			CopyBytecode(CaptureOffFunction),
			CopyBytecode(CaptureOnFunction),
			TEXT("Structured while capture must not change any bytecode word")));
		const int32 Inputs[] = {0, 1, 4};
		for (const int32 Input : Inputs)
		{
			int32 CaptureOffResult = 0;
			int32 CaptureOnResult = 0;
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOffEngine.Get(),
				CaptureOffFunction,
				Input,
				CaptureOffResult)));
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOnEngine.Get(),
				CaptureOnFunction,
				Input,
				CaptureOnResult)));
			ASSERT_THAT(AreEqual(Input, CaptureOffResult));
			ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult));
		}

		const asCTypedSemanticFunction* const HIR =
			CaptureOnFunction->GetTypedSemanticFunction();
		const FString CaptureDiagnostic = UTF8_TO_TCHAR(
			CaptureOnFunction->GetTypedSemanticCaptureDiagnostic().AddressOf());
		ASSERT_THAT(IsNotNull(
			HIR,
			*FString::Printf(
				TEXT("A valid scalar while must publish HIR: %s"),
				*CaptureDiagnostic)));
		if (HIR == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));

		const asSTypedSemanticStatement* While = nullptr;
		int32 WhileCount = 0;
		for (asUINT Index = 0; Index < HIR->statements.GetLength(); ++Index)
		{
			if (HIR->statements[Index].kind
				== asETypedSemanticStatementKind::While)
			{
				While = &HIR->statements[Index];
				++WhileCount;
			}
		}
		ASSERT_THAT(AreEqual(
			1,
			WhileCount,
			TEXT("The compiler must retain one structured While node instead of flattening its body")));
		if (While == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(While->conditionExpression.IsValid()));
		ASSERT_THAT(IsTrue(While->bodyStatement.IsValid()));
		if (!While->conditionExpression.IsValid()
			|| !While->bodyStatement.IsValid())
		{
			return;
		}
		const asSTypedSemanticExpression& Condition =
			HIR->expressions[While->conditionExpression.value];
		const asSTypedSemanticStatement& Body =
			HIR->statements[While->bodyStatement.value];
		ASSERT_THAT(IsTrue(Condition.type.IsBooleanType()));
		ASSERT_THAT(AreEqual(
			asETypedSemanticStatementKind::Block,
			Body.kind));
		ASSERT_THAT(AreEqual(2,
			static_cast<int32>(Body.children.GetLength())));
		ASSERT_THAT(AreEqual(
			asETypedSemanticSafePointRole::LoopBackedge,
			While->safePointRole,
			TEXT("The while statement must identify its semantic backedge")));
		ASSERT_THAT(AreEqual(
			asETypedSemanticLoopPhase::Condition,
			While->loopPhase,
			TEXT("The while backedge must explicitly target condition evaluation")));
		ASSERT_THAT(AreEqual(
			asETypedSemanticSafePointRole::LoopEntry,
			Body.safePointRole,
			TEXT("The while body must retain its explicit loop-entry safe point")));
		ASSERT_THAT(AreEqual(
			asETypedSemanticLoopPhase::Body,
			Body.loopPhase));

		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("kind=While condition=E"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("phase=Condition safePoint=LoopBackedge"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("phase=Body safePoint=LoopEntry"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(CompilerCapturesStructuredDoWhileBodyBeforeTrailingCondition)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-STRUCTURED-DO-WHILE",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int CountStructuredDoWhile(int Limit)
			{
				int Current = 0;
				do
				{
					Current = Current + 1;
				}
				while (Current < Limit);
				return Current;
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
		ASSERT_THAT(IsTrue(
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get())
				->SetTypedSemanticIRCapture(true)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRStructuredDoWhileOff",
			Source);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRStructuredDoWhileOn",
			Source);
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		const char* const Declaration =
			"int CountStructuredDoWhile(int Limit)";
		asCScriptFunction* const CaptureOffFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOffModule,
				Declaration));
		asCScriptFunction* const CaptureOnFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOnModule,
				Declaration));
		ASSERT_THAT(IsNotNull(CaptureOffFunction));
		ASSERT_THAT(IsNotNull(CaptureOnFunction));
		if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			CopyBytecode(CaptureOffFunction),
			CopyBytecode(CaptureOnFunction),
			TEXT("Structured do-while capture must not change any bytecode word")));
		struct FCase
		{
			int32 Limit;
			int32 Expected;
		};
		const FCase Cases[] = {{0, 1}, {1, 1}, {4, 4}};
		for (const FCase& Case : Cases)
		{
			int32 CaptureOffResult = 0;
			int32 CaptureOnResult = 0;
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOffEngine.Get(),
				CaptureOffFunction,
				Case.Limit,
				CaptureOffResult)));
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOnEngine.Get(),
				CaptureOnFunction,
				Case.Limit,
				CaptureOnResult)));
			ASSERT_THAT(AreEqual(Case.Expected, CaptureOffResult));
			ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult));
		}

		const asCTypedSemanticFunction* const HIR =
			CaptureOnFunction->GetTypedSemanticFunction();
		const FString CaptureDiagnostic = UTF8_TO_TCHAR(
			CaptureOnFunction->GetTypedSemanticCaptureDiagnostic().AddressOf());
		ASSERT_THAT(IsNotNull(
			HIR,
			*FString::Printf(
				TEXT("A valid scalar do-while must publish HIR: %s"),
				*CaptureDiagnostic)));
		if (HIR == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));

		const asSTypedSemanticStatement* DoWhile = nullptr;
		int32 DoWhileCount = 0;
		for (asUINT Index = 0; Index < HIR->statements.GetLength(); ++Index)
		{
			if (HIR->statements[Index].kind
				== asETypedSemanticStatementKind::DoWhile)
			{
				DoWhile = &HIR->statements[Index];
				++DoWhileCount;
			}
		}
		ASSERT_THAT(AreEqual(
			1,
			DoWhileCount,
			TEXT("The compiler must retain one body-first DoWhile node")));
		if (DoWhile == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(DoWhile->conditionExpression.IsValid()));
		ASSERT_THAT(IsTrue(DoWhile->bodyStatement.IsValid()));
		if (!DoWhile->conditionExpression.IsValid()
			|| !DoWhile->bodyStatement.IsValid())
		{
			return;
		}
		const asSTypedSemanticExpression& Condition =
			HIR->expressions[DoWhile->conditionExpression.value];
		const asSTypedSemanticStatement& Body =
			HIR->statements[DoWhile->bodyStatement.value];
		ASSERT_THAT(IsTrue(Condition.type.IsBooleanType()));
		ASSERT_THAT(AreEqual(
			asETypedSemanticStatementKind::Block,
			Body.kind));
		ASSERT_THAT(AreEqual(1,
			static_cast<int32>(Body.children.GetLength())));
		ASSERT_THAT(AreEqual(
			asETypedSemanticLoopPhase::Condition,
			DoWhile->loopPhase));
		ASSERT_THAT(AreEqual(
			asETypedSemanticSafePointRole::LoopBackedge,
			DoWhile->safePointRole));
		ASSERT_THAT(AreEqual(
			asETypedSemanticLoopPhase::Body,
			Body.loopPhase));
		ASSERT_THAT(AreEqual(
			asETypedSemanticSafePointRole::LoopEntry,
			Body.safePointRole));

		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("kind=DoWhile body=T"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("phase=Condition safePoint=LoopBackedge"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(CompilerCapturesStructuredForPhasesAndOrderedIncrementList)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-STRUCTURED-FOR",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int SumStructuredFor(int Limit)
			{
				int Sum = 0;
				int Step = 1;
				for (int Index = 0;
					Index < Limit;
					Index = Index + Step, Step = Step + 1)
				{
					Sum = Sum + Index;
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
		ASSERT_THAT(IsTrue(
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get())
				->SetTypedSemanticIRCapture(true)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRStructuredForOff",
			Source);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRStructuredForOn",
			Source);
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		const char* const Declaration =
			"int SumStructuredFor(int Limit)";
		asCScriptFunction* const CaptureOffFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOffModule,
				Declaration));
		asCScriptFunction* const CaptureOnFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOnModule,
				Declaration));
		ASSERT_THAT(IsNotNull(CaptureOffFunction));
		ASSERT_THAT(IsNotNull(CaptureOnFunction));
		if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			CopyBytecode(CaptureOffFunction),
			CopyBytecode(CaptureOnFunction),
			TEXT("Structured for capture must not change any bytecode word")));
		struct FCase
		{
			int32 Limit;
			int32 Expected;
		};
		const FCase Cases[] = {{0, 0}, {1, 0}, {4, 4}, {7, 10}};
		for (const FCase& Case : Cases)
		{
			int32 CaptureOffResult = 0;
			int32 CaptureOnResult = 0;
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOffEngine.Get(),
				CaptureOffFunction,
				Case.Limit,
				CaptureOffResult)));
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOnEngine.Get(),
				CaptureOnFunction,
				Case.Limit,
				CaptureOnResult)));
			ASSERT_THAT(AreEqual(Case.Expected, CaptureOffResult));
			ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult));
		}

		const asCTypedSemanticFunction* const HIR =
			CaptureOnFunction->GetTypedSemanticFunction();
		const FString CaptureDiagnostic = UTF8_TO_TCHAR(
			CaptureOnFunction->GetTypedSemanticCaptureDiagnostic().AddressOf());
		ASSERT_THAT(IsNotNull(
			HIR,
			*FString::Printf(
				TEXT("A valid scalar for loop must publish HIR: %s"),
				*CaptureDiagnostic)));
		if (HIR == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));

		const asSTypedSemanticStatement* For = nullptr;
		int32 ForCount = 0;
		for (asUINT Index = 0; Index < HIR->statements.GetLength(); ++Index)
		{
			if (HIR->statements[Index].kind
				== asETypedSemanticStatementKind::For)
			{
				For = &HIR->statements[Index];
				++ForCount;
			}
		}
		ASSERT_THAT(AreEqual(
			1,
			ForCount,
			TEXT("The compiler must retain one structured For node")));
		if (For == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(For->initializerStatement.IsValid()));
		ASSERT_THAT(IsTrue(For->conditionExpression.IsValid()));
		ASSERT_THAT(IsTrue(For->incrementStatement.IsValid()));
		ASSERT_THAT(IsTrue(For->bodyStatement.IsValid()));
		ASSERT_THAT(AreEqual(2,
			static_cast<int32>(For->incrementExpressions.GetLength())));
		if (!For->initializerStatement.IsValid()
			|| !For->conditionExpression.IsValid()
			|| !For->incrementStatement.IsValid()
			|| !For->bodyStatement.IsValid()
			|| For->incrementExpressions.GetLength() != 2)
		{
			return;
		}

		const asSTypedSemanticStatement& Initializer =
			HIR->statements[For->initializerStatement.value];
		const asSTypedSemanticExpression& Condition =
			HIR->expressions[For->conditionExpression.value];
		const asSTypedSemanticStatement& Increment =
			HIR->statements[For->incrementStatement.value];
		const asSTypedSemanticStatement& Body =
			HIR->statements[For->bodyStatement.value];
		ASSERT_THAT(AreEqual(
			asETypedSemanticStatementKind::LocalDeclaration,
			Initializer.kind));
		ASSERT_THAT(AreEqual(
			asETypedSemanticLoopPhase::Initializer,
			Initializer.loopPhase));
		ASSERT_THAT(IsTrue(Condition.type.IsBooleanType()));
		ASSERT_THAT(AreEqual(
			asETypedSemanticStatementKind::Block,
			Increment.kind));
		ASSERT_THAT(AreEqual(
			asETypedSemanticLoopPhase::Increment,
			Increment.loopPhase));
		ASSERT_THAT(AreEqual(2,
			static_cast<int32>(Increment.children.GetLength())));
		ASSERT_THAT(AreEqual(
			asETypedSemanticStatementKind::Block,
			Body.kind));
		ASSERT_THAT(AreEqual(
			asETypedSemanticLoopPhase::Body,
			Body.loopPhase));
		ASSERT_THAT(AreEqual(
			asETypedSemanticSafePointRole::LoopEntry,
			Body.safePointRole));
		ASSERT_THAT(AreEqual(
			asETypedSemanticLoopPhase::Condition,
			For->loopPhase));
		ASSERT_THAT(AreEqual(
			asETypedSemanticSafePointRole::LoopBackedge,
			For->safePointRole));

		const TCHAR* const ExpectedIncrementTargets[] = {
			TEXT("Index"),
			TEXT("Step"),
		};
		for (asUINT IncrementIndex = 0;
			IncrementIndex < For->incrementExpressions.GetLength();
			++IncrementIndex)
		{
			const asSTypedSemanticStatement& IncrementStatement =
				HIR->statements[Increment.children[IncrementIndex].value];
			ASSERT_THAT(AreEqual(
				asETypedSemanticStatementKind::Expression,
				IncrementStatement.kind));
			ASSERT_THAT(AreEqual(
				For->incrementExpressions[IncrementIndex].value,
				IncrementStatement.valueExpression.value));
			const asSTypedSemanticExpression& Mutation =
				HIR->expressions[For->incrementExpressions[IncrementIndex].value];
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::Assignment,
				Mutation.kind));
			ASSERT_THAT(IsTrue(Mutation.mutationTarget.IsValid()));
			if (!Mutation.mutationTarget.IsValid())
			{
				continue;
			}
			const asSTypedSemanticExpression& Target =
				HIR->expressions[Mutation.mutationTarget.value];
			ASSERT_THAT(IsTrue(Target.symbol.IsValid()));
			if (Target.symbol.IsValid())
			{
				ASSERT_THAT(AreEqual(
					FString(ExpectedIncrementTargets[IncrementIndex]),
					FString(UTF8_TO_TCHAR(
						HIR->symbols[Target.symbol.value].name.AddressOf()))));
			}
		}

		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("kind=For initializer=T"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT(" increments=[E"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(CompilerCapturesNearestLoopTransfersAndExplicitEmptyCleanup)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-LOOP-TRANSFERS",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int NestedLoopTransfers(int Limit)
			{
				int Trace = 0;
				for (int Outer = 0; Outer < Limit; Outer = Outer + 1)
				{
					int Inner = 0;
					while (Inner < 4)
					{
						Inner = Inner + 1;
						if (Inner == 1)
							continue;
						if (Inner == 3)
							break;
						Trace = Trace + Outer + 1;
					}
					if (Outer == 1)
						continue;
					Trace = Trace + 10;
					if (Outer == 2)
						break;
				}
				return Trace;
			}

			int ForContinuePhase(int Unused)
			{
				int Result = 0;
				for (int Index = 0; Index < 4; Index = Index + 1)
				{
					if (Index < 2)
						continue;
					Result = Result * 10 + Index;
				}
				return Result;
			}

			int DoContinuePhase(int Unused)
			{
				int Current = 0;
				int Result = 0;
				do
				{
					Current = Current + 1;
					if (Current < 3)
						continue;
					Result = Current;
				}
				while (Current < 3);
				return Result;
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
		ASSERT_THAT(IsTrue(
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get())
				->SetTypedSemanticIRCapture(true)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRLoopTransfersOff",
			Source);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRLoopTransfersOn",
			Source);
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		struct FCase
		{
			const char* Declaration;
			int32 Input;
			int32 ExpectedResult;
			int32 ExpectedForCount;
			int32 ExpectedWhileCount;
			int32 ExpectedDoWhileCount;
			int32 ExpectedBreakForTargets;
			int32 ExpectedBreakWhileTargets;
			int32 ExpectedContinueForTargets;
			int32 ExpectedContinueWhileTargets;
			int32 ExpectedContinueDoWhileTargets;
		};
		const FCase Cases[] = {
			{"int NestedLoopTransfers(int Limit)", 5, 26,
				1, 1, 0, 1, 1, 1, 1, 0},
			{"int ForContinuePhase(int Unused)", 0, 23,
				1, 0, 0, 0, 0, 1, 0, 0},
			{"int DoContinuePhase(int Unused)", 0, 3,
				0, 0, 1, 0, 0, 0, 0, 1},
		};

		for (const FCase& Case : Cases)
		{
			asCScriptFunction* const CaptureOffFunction =
				static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
					CaptureOffModule,
					Case.Declaration));
			asCScriptFunction* const CaptureOnFunction =
				static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
					CaptureOnModule,
					Case.Declaration));
			ASSERT_THAT(IsNotNull(CaptureOffFunction));
			ASSERT_THAT(IsNotNull(CaptureOnFunction));
			if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
			{
				continue;
			}

			ASSERT_THAT(AreEqual(
				CopyBytecode(CaptureOffFunction),
				CopyBytecode(CaptureOnFunction),
				TEXT("Loop-transfer capture must not change any bytecode word")));
			int32 CaptureOffResult = 0;
			int32 CaptureOnResult = 0;
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOffEngine.Get(),
				CaptureOffFunction,
				Case.Input,
				CaptureOffResult)));
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOnEngine.Get(),
				CaptureOnFunction,
				Case.Input,
				CaptureOnResult)));
			ASSERT_THAT(AreEqual(Case.ExpectedResult, CaptureOffResult));
			ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult));

			const asCTypedSemanticFunction* const HIR =
				CaptureOnFunction->GetTypedSemanticFunction();
			const FString CaptureDiagnostic = UTF8_TO_TCHAR(
				CaptureOnFunction->GetTypedSemanticCaptureDiagnostic().AddressOf());
			ASSERT_THAT(IsNotNull(
				HIR,
				*FString::Printf(
					TEXT("A scalar loop transfer fixture must publish HIR: %s"),
					*CaptureDiagnostic)));
			if (HIR == nullptr)
			{
				continue;
			}
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
			ASSERT_THAT(AreEqual(
				asETypedSemanticCleanupPlanState::VerifiedEmpty,
				HIR->header.cleanupPlanState));
			ASSERT_THAT(IsTrue(HIR->header.universalCleanupPlan.IsValid()));

			int32 ForCount = 0;
			int32 WhileCount = 0;
			int32 DoWhileCount = 0;
			int32 BreakForTargets = 0;
			int32 BreakWhileTargets = 0;
			int32 ContinueForTargets = 0;
			int32 ContinueWhileTargets = 0;
			int32 ContinueDoWhileTargets = 0;
			asTypedSemanticStatementId InnerContinue;
			asTypedSemanticStatementId OuterFor;
			asTypedSemanticStatementId NonAncestorIncrement;
			for (asUINT StatementIndex = 0;
				StatementIndex < HIR->statements.GetLength();
				++StatementIndex)
			{
				const asSTypedSemanticStatement& Statement =
					HIR->statements[StatementIndex];
				if (Statement.kind == asETypedSemanticStatementKind::For)
				{
					++ForCount;
					OuterFor = Statement.id;
					NonAncestorIncrement = Statement.incrementStatement;
				}
				else if (Statement.kind == asETypedSemanticStatementKind::While)
					++WhileCount;
				else if (Statement.kind == asETypedSemanticStatementKind::DoWhile)
					++DoWhileCount;

				if (Statement.kind != asETypedSemanticStatementKind::Break
					&& Statement.kind
						!= asETypedSemanticStatementKind::Continue)
				{
					continue;
				}

				ASSERT_THAT(AreEqual(
					asETypedSemanticSafePointRole::Transfer,
					Statement.safePointRole));
				ASSERT_THAT(IsTrue(Statement.targetStatement.IsValid()));
				ASSERT_THAT(IsTrue(Statement.cleanupPlan.IsValid()));
				ASSERT_THAT(IsTrue(Statement.exitedScopeCount > 0));
				if (!Statement.targetStatement.IsValid()
					|| Statement.targetStatement.value
						>= HIR->statements.GetLength())
				{
					continue;
				}
				ASSERT_THAT(AreEqual(
					HIR->header.universalCleanupPlan.value,
					Statement.cleanupPlan.value));
				const asETypedSemanticStatementKind TargetKind =
					HIR->statements[Statement.targetStatement.value].kind;
				if (Statement.kind == asETypedSemanticStatementKind::Break)
				{
					if (TargetKind == asETypedSemanticStatementKind::For)
						++BreakForTargets;
					else if (TargetKind == asETypedSemanticStatementKind::While)
						++BreakWhileTargets;
				}
				else
				{
					if (TargetKind == asETypedSemanticStatementKind::For)
						++ContinueForTargets;
					else if (TargetKind == asETypedSemanticStatementKind::While)
					{
						++ContinueWhileTargets;
						InnerContinue = Statement.id;
					}
					else if (TargetKind == asETypedSemanticStatementKind::DoWhile)
						++ContinueDoWhileTargets;
				}
			}

			ASSERT_THAT(AreEqual(Case.ExpectedForCount, ForCount));
			ASSERT_THAT(AreEqual(Case.ExpectedWhileCount, WhileCount));
			ASSERT_THAT(AreEqual(Case.ExpectedDoWhileCount, DoWhileCount));
			ASSERT_THAT(AreEqual(
				Case.ExpectedBreakForTargets,
				BreakForTargets));
			ASSERT_THAT(AreEqual(
				Case.ExpectedBreakWhileTargets,
				BreakWhileTargets));
			ASSERT_THAT(AreEqual(
				Case.ExpectedContinueForTargets,
				ContinueForTargets));
			ASSERT_THAT(AreEqual(
				Case.ExpectedContinueWhileTargets,
				ContinueWhileTargets));
			ASSERT_THAT(AreEqual(
				Case.ExpectedContinueDoWhileTargets,
				ContinueDoWhileTargets));

			if (Case.ExpectedContinueWhileTargets != 0)
			{
				ASSERT_THAT(IsTrue(InnerContinue.IsValid()));
				ASSERT_THAT(IsTrue(OuterFor.IsValid()));
				ASSERT_THAT(IsTrue(NonAncestorIncrement.IsValid()));
				if (InnerContinue.IsValid()
					&& OuterFor.IsValid()
					&& NonAncestorIncrement.IsValid())
				{
					asCTypedSemanticFunction DanglingTarget = *HIR;
					DanglingTarget.statements[InnerContinue.value]
						.targetStatement = asTypedSemanticStatementId(
							HIR->statements.GetLength());
					ASSERT_THAT(AreEqual(
						asETypedSemanticVerificationError::InvalidControlTarget,
						VerifyTypedSemanticFunction(DanglingTarget).error));

					asCTypedSemanticFunction WrongKindTarget = *HIR;
					WrongKindTarget.statements[InnerContinue.value]
						.targetStatement = HIR->rootStatement;
					ASSERT_THAT(AreEqual(
						asETypedSemanticVerificationError::InvalidControlTarget,
						VerifyTypedSemanticFunction(WrongKindTarget).error));

					asCTypedSemanticFunction SkippedNearerTarget = *HIR;
					SkippedNearerTarget.statements[InnerContinue.value]
						.targetStatement = OuterFor;
					ASSERT_THAT(AreEqual(
						asETypedSemanticVerificationError::InvalidControlTarget,
						VerifyTypedSemanticFunction(SkippedNearerTarget).error));

					asCTypedSemanticFunction NonAncestorTarget = *HIR;
					NonAncestorTarget.statements[InnerContinue.value]
						.targetStatement = NonAncestorIncrement;
					ASSERT_THAT(AreEqual(
						asETypedSemanticVerificationError::InvalidControlTarget,
						VerifyTypedSemanticFunction(NonAncestorTarget).error));
				}
			}

			const FString Dump = UTF8_TO_TCHAR(
				DumpTypedSemanticFunction(*HIR).AddressOf());
			if (Case.ExpectedBreakForTargets
				+ Case.ExpectedBreakWhileTargets > 0)
			{
				ASSERT_THAT(IsTrue(Dump.Contains(
					TEXT("kind=Break target=T"),
					ESearchCase::CaseSensitive)));
			}
			ASSERT_THAT(IsTrue(Dump.Contains(
				TEXT("kind=Continue target=T"),
				ESearchCase::CaseSensitive)));
			ASSERT_THAT(IsTrue(Dump.Contains(
				TEXT(" cleanup=C"),
				ESearchCase::CaseSensitive)));
			ASSERT_THAT(IsTrue(Dump.Contains(
				TEXT(" exitedScopes="),
				ESearchCase::CaseSensitive)));
		}
	}

	TEST_METHOD(CompilerCapturesSwitchCasesFallthroughTargetsAndInvalidEnumEdge)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SWITCH",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Isolation);

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			enum ETypedSwitchChoice
			{
				Zero = 0,
				One = 1,
				Two = 2
			}

			int SwitchInsideFor(int Limit)
			{
				int Trace = 0;
				for (int Index = 0; Index < Limit; Index = Index + 1)
				{
					switch (Index)
					{
					case 0:
						Trace = Trace + 1;
						continue;
					case 1:
						Trace = Trace + 10;
						break;
					case 2:
						Trace = Trace + 100;
						fallthrough;
					case 3:
						Trace = Trace + 1000;
						break;
					default:
						Trace = Trace + 10000;
						break;
					}
					Trace = Trace + 5;
				}
				return Trace;
			}

			int LoopInsideSwitch(int Selector)
			{
				int Trace = 0;
				switch (Selector)
				{
				case 0:
				{
					int Index = 0;
					while (Index < 3)
					{
						Index = Index + 1;
						if (Index == 2)
							break;
						Trace = Trace + 10;
					}
				}
					Trace = Trace + 1;
					break;
				case 1:
					Trace = 20;
					break;
				default:
					Trace = 30;
					break;
				}
				return Trace;
			}

			int UnsignedGroupedSwitch(uint Selector)
			{
				int Trace = 0;
				switch (Selector)
				{
				case 0:
				case 1:
					Trace = 5;
					break;
				case 2:
					Trace = 10;
					break;
				default:
					Trace = 20;
					break;
				}
				return Trace;
			}

			int ExhaustiveEnumSwitch(ETypedSwitchChoice Value)
			{
				switch (Value)
				{
				case ETypedSwitchChoice::Zero:
					return 10;
				case ETypedSwitchChoice::One:
					return 20;
				case ETypedSwitchChoice::Two:
					return 30;
				}
				return -1;
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
		ASSERT_THAT(IsTrue(
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get())
				->SetTypedSemanticIRCapture(true)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRSwitchOff",
			Source);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRSwitchOn",
			Source);
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		struct FCase
		{
			const char* Declaration;
			int32 Input;
			int32 ExpectedResult;
			int32 ExpectedCaseCount;
			int32 ExpectedSwitchBreakTargets;
			int32 ExpectedWhileBreakTargets;
			int32 ExpectedForContinueTargets;
			int32 ExpectedFallthroughCases;
			int32 ExpectedExplicitFallthroughCases;
			asETypedSemanticSwitchNormalization ExpectedNormalization;
			bool bExpectedDefault;
			bool bExpectedInvalidEnumEdge;
		};
		const FCase Cases[] = {
			{"int SwitchInsideFor(int Limit)", 5, 12131, 5,
				3, 0, 1, 1, 1,
				asETypedSemanticSwitchNormalization::SignedInt32,
				true, false},
			{"int LoopInsideSwitch(int Selector)", 0, 11, 3,
				3, 1, 0, 0, 0,
				asETypedSemanticSwitchNormalization::SignedInt32,
				true, false},
			{"int UnsignedGroupedSwitch(uint Selector)", 0, 5, 4,
				3, 0, 0, 1, 0,
				asETypedSemanticSwitchNormalization::UnsignedInt32,
				true, false},
			{"int ExhaustiveEnumSwitch(ETypedSwitchChoice Value)", 1, 20, 3,
				0, 0, 0, 0, 0,
				asETypedSemanticSwitchNormalization::SignedInt32,
				false, true},
		};

		for (const FCase& Case : Cases)
		{
			asCScriptFunction* const CaptureOffFunction =
				static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
					CaptureOffModule,
					Case.Declaration));
			asCScriptFunction* const CaptureOnFunction =
				static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
					CaptureOnModule,
					Case.Declaration));
			ASSERT_THAT(IsNotNull(CaptureOffFunction));
			ASSERT_THAT(IsNotNull(CaptureOnFunction));
			if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
			{
				continue;
			}

			ASSERT_THAT(AreEqual(
				CopyBytecode(CaptureOffFunction),
				CopyBytecode(CaptureOnFunction),
				TEXT("Switch capture must not change any bytecode word")));
			int32 CaptureOffResult = 0;
			int32 CaptureOnResult = 0;
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOffEngine.Get(),
				CaptureOffFunction,
				Case.Input,
				CaptureOffResult)));
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOnEngine.Get(),
				CaptureOnFunction,
				Case.Input,
				CaptureOnResult)));
			ASSERT_THAT(AreEqual(Case.ExpectedResult, CaptureOffResult));
			ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult));

			const asCTypedSemanticFunction* const HIR =
				CaptureOnFunction->GetTypedSemanticFunction();
			const FString CaptureDiagnostic = UTF8_TO_TCHAR(
				CaptureOnFunction->GetTypedSemanticCaptureDiagnostic().AddressOf());
			ASSERT_THAT(IsNotNull(
				HIR,
				*FString::Printf(
					TEXT("A scalar switch fixture must publish HIR: %s"),
					*CaptureDiagnostic)));
			if (HIR == nullptr)
			{
				continue;
			}
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));

			asTypedSemanticStatementId SwitchId;
			asTypedSemanticStatementId DefaultCaseId;
			asTypedSemanticStatementId ExplicitFallthroughCaseId;
			int32 SwitchCount = 0;
			int32 CaseCount = 0;
			int32 SwitchBreakTargets = 0;
			int32 WhileBreakTargets = 0;
			int32 ForContinueTargets = 0;
			int32 FallthroughCases = 0;
			int32 ExplicitFallthroughCases = 0;
			for (asUINT StatementIndex = 0;
				StatementIndex < HIR->statements.GetLength();
				++StatementIndex)
			{
				const asSTypedSemanticStatement& Statement =
					HIR->statements[StatementIndex];
				if (Statement.kind == asETypedSemanticStatementKind::Switch)
				{
					++SwitchCount;
					SwitchId = Statement.id;
					ASSERT_THAT(AreEqual(
						Case.ExpectedNormalization,
						Statement.switchNormalization));
					ASSERT_THAT(AreEqual(
						Case.ExpectedCaseCount,
						static_cast<int32>(Statement.children.GetLength())));
					ASSERT_THAT(AreEqual(
						Case.bExpectedDefault,
						Statement.switchHasDefault));
					ASSERT_THAT(IsTrue(Statement.switchIsExhaustive));
					ASSERT_THAT(AreEqual(
						Case.bExpectedInvalidEnumEdge,
						Statement.switchHasInvalidEnumValueEdge));
					ASSERT_THAT(AreEqual(
						Case.bExpectedInvalidEnumEdge
							? asETypedSemanticSafePointRole::SwitchInvalidValue
							: asETypedSemanticSafePointRole::Statement,
						Statement.safePointRole));
				}
				else if (Statement.kind == asETypedSemanticStatementKind::Case)
				{
					++CaseCount;
					if (Statement.isDefaultCase)
						DefaultCaseId = Statement.id;
					if (Statement.hasExplicitFallthrough)
					{
						ExplicitFallthroughCaseId = Statement.id;
						++ExplicitFallthroughCases;
						ASSERT_THAT(IsTrue(Statement.fallsThrough));
					}
					if (Statement.fallsThrough)
						++FallthroughCases;
				}

				if ((Statement.kind != asETypedSemanticStatementKind::Break
						&& Statement.kind
							!= asETypedSemanticStatementKind::Continue)
					|| !Statement.targetStatement.IsValid()
					|| Statement.targetStatement.value
						>= HIR->statements.GetLength())
				{
					continue;
				}
				const asETypedSemanticStatementKind TargetKind =
					HIR->statements[Statement.targetStatement.value].kind;
				if (Statement.kind == asETypedSemanticStatementKind::Break)
				{
					if (TargetKind == asETypedSemanticStatementKind::Switch)
						++SwitchBreakTargets;
					else if (TargetKind == asETypedSemanticStatementKind::While)
						++WhileBreakTargets;
				}
				else if (TargetKind == asETypedSemanticStatementKind::For)
				{
					++ForContinueTargets;
				}
			}

			ASSERT_THAT(AreEqual(1, SwitchCount));
			ASSERT_THAT(AreEqual(Case.ExpectedCaseCount, CaseCount));
			ASSERT_THAT(AreEqual(
				Case.ExpectedSwitchBreakTargets,
				SwitchBreakTargets));
			ASSERT_THAT(AreEqual(
				Case.ExpectedWhileBreakTargets,
				WhileBreakTargets));
			ASSERT_THAT(AreEqual(
				Case.ExpectedForContinueTargets,
				ForContinueTargets));
			ASSERT_THAT(AreEqual(
				Case.ExpectedFallthroughCases,
				FallthroughCases));
			ASSERT_THAT(AreEqual(
				Case.ExpectedExplicitFallthroughCases,
				ExplicitFallthroughCases));
			ASSERT_THAT(AreEqual(
				Case.bExpectedDefault,
				DefaultCaseId.IsValid()));
			if (FCStringAnsi::Strcmp(
					Case.Declaration,
					"int SwitchInsideFor(int Limit)") == 0)
			{
				ASSERT_THAT(IsTrue(ExplicitFallthroughCaseId.IsValid()));
				if (SwitchId.IsValid())
				{
					const asSTypedSemanticStatement& SwitchStatement =
						HIR->statements[SwitchId.value];
					const int ExpectedConstants[] = {0, 1, 2, 3};
					for (int32 CaseIndex = 0; CaseIndex < 4; ++CaseIndex)
					{
						const asTypedSemanticStatementId Child =
							SwitchStatement.children[CaseIndex];
						ASSERT_THAT(AreEqual(
							ExpectedConstants[CaseIndex],
							HIR->statements[Child.value].caseConstantValue));
					}
				}
			}

			const FString Dump = UTF8_TO_TCHAR(
				DumpTypedSemanticFunction(*HIR).AddressOf());
			ASSERT_THAT(IsTrue(Dump.Contains(
				TEXT("kind=Switch selector=E"),
				ESearchCase::CaseSensitive)));
			ASSERT_THAT(IsTrue(Dump.Contains(
				Case.ExpectedNormalization
					== asETypedSemanticSwitchNormalization::UnsignedInt32
					? TEXT(" normalization=UnsignedInt32")
					: TEXT(" normalization=SignedInt32"),
				ESearchCase::CaseSensitive)));
			ASSERT_THAT(IsTrue(Dump.Contains(
				TEXT("kind=Case"),
				ESearchCase::CaseSensitive)));

			if (SwitchId.IsValid())
			{
				asCTypedSemanticFunction InvalidFallthrough = *HIR;
				const asTypedSemanticStatementId LastCase =
					InvalidFallthrough.statements[SwitchId.value].children[
						InvalidFallthrough.statements[SwitchId.value]
							.children.GetLength() - 1];
				InvalidFallthrough.statements[LastCase.value].fallsThrough = true;
				ASSERT_THAT(AreEqual(
					asETypedSemanticVerificationError::InvalidNodeShape,
					VerifyTypedSemanticFunction(InvalidFallthrough).error));

				if (FCStringAnsi::Strcmp(
						Case.Declaration,
						"int SwitchInsideFor(int Limit)") == 0)
				{
					asCTypedSemanticFunction DuplicateCase = *HIR;
					const asSTypedSemanticStatement& DuplicateSwitch =
						DuplicateCase.statements[SwitchId.value];
					DuplicateCase.statements[
							DuplicateSwitch.children[1].value]
						.caseConstantValue =
						DuplicateCase.statements[
							DuplicateSwitch.children[0].value]
						.caseConstantValue;
					ASSERT_THAT(AreEqual(
						asETypedSemanticVerificationError::InvalidNodeShape,
						VerifyTypedSemanticFunction(DuplicateCase).error));

					asCTypedSemanticFunction DefaultNotLast = *HIR;
					asSTypedSemanticStatement& ReorderedSwitch =
						DefaultNotLast.statements[SwitchId.value];
					const asTypedSemanticStatementId FirstCase =
						ReorderedSwitch.children[0];
					ReorderedSwitch.children[0] =
						ReorderedSwitch.children[
							ReorderedSwitch.children.GetLength() - 1];
					ReorderedSwitch.children[
						ReorderedSwitch.children.GetLength() - 1] = FirstCase;
					ASSERT_THAT(AreEqual(
						asETypedSemanticVerificationError::InvalidNodeShape,
						VerifyTypedSemanticFunction(DefaultNotLast).error));
				}
				else if (FCStringAnsi::Strcmp(
						Case.Declaration,
						"int UnsignedGroupedSwitch(uint Selector)") == 0)
				{
					asCTypedSemanticFunction WrongNormalization = *HIR;
					WrongNormalization.statements[SwitchId.value]
						.switchNormalization =
						asETypedSemanticSwitchNormalization::SignedInt32;
					ASSERT_THAT(AreEqual(
						asETypedSemanticVerificationError::InvalidNodeShape,
						VerifyTypedSemanticFunction(WrongNormalization).error));
				}
				else if (Case.bExpectedInvalidEnumEdge)
				{
					asCTypedSemanticFunction MissingInvalidEdge = *HIR;
					MissingInvalidEdge.statements[SwitchId.value]
						.switchHasInvalidEnumValueEdge = false;
					ASSERT_THAT(AreEqual(
						asETypedSemanticVerificationError::InvalidNodeShape,
						VerifyTypedSemanticFunction(MissingInvalidEdge).error));
				}
			}
		}

		asIScriptFunction* const ExhaustiveFunction = GetNativeFunctionByDecl(
			CaptureOnModule,
			"int ExhaustiveEnumSwitch(ETypedSwitchChoice Value)");
		ASSERT_THAT(IsNotNull(ExhaustiveFunction));
		if (ExhaustiveFunction != nullptr)
		{
			asIScriptContext* const Context = CaptureOnEngine.Get()->CreateContext();
			ASSERT_THAT(IsNotNull(Context));
			if (Context != nullptr)
			{
				ON_SCOPE_EXIT { Context->Release(); };
				ASSERT_THAT(IsTrue(Context->Prepare(ExhaustiveFunction) >= 0));
				ASSERT_THAT(IsTrue(Context->SetArgDWord(0, 99) >= 0));
				ASSERT_THAT(AreEqual(
					asEXECUTION_EXCEPTION,
					Context->Execute()));
				ASSERT_THAT(AreEqual(
					FString(TEXT("Invalid enum value passed to switch")),
					FString(UTF8_TO_TCHAR(Context->GetExceptionString()))));
			}
		}
	}

	TEST_METHOD(ShortCircuitInitializerPublishesVerifiedHIR)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SHORT-CIRCUIT",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		static_cast<asCScriptEngine*>(Engine.Get())
			->SetTypedSemanticIRCapture(true);

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedSemanticIRShortCircuit",
			ASTEST_AS_ANSI(R"AS(
				bool ShortCircuitInitializer(bool Left, bool Right)
				{
					bool Both = Left && Right;
					bool Either = Both || Right;
					return Either;
				}

				bool ShortCircuitMutationInitializer(
					bool Enabled,
					int Limit)
				{
					int Value = 0;
					bool Gate = Enabled && (++Value > Limit);
					return Gate;
				}

				bool ShortCircuitAfterMutationSequence(
					int A,
					int B,
					bool Enabled)
				{
					int Value = A + B;
					Value = Value - 1;
					Value += 3;
					Value -= 1;
					Value *= 2;
					int PrefixResult = ++Value;
					int PostfixResult = Value++;
					int Negated = -PrefixResult;
					int Inverted = ~PostfixResult;
					int ShiftedLeft = Value << 1;
					int ShiftedRight = Value >> 1;
					bool Gate = Enabled && (++Value > B);
					return Gate;
				}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		asCScriptFunction* const Function =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module,
				"bool ShortCircuitInitializer(bool Left, bool Right)"));
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		const asCTypedSemanticFunction* const HIR =
			Function->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR,
			TEXT("A local initialized by &&/|| must retain verified HIR")));
		if (HIR == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));

		int32 ShortCircuitCount = 0;
		for (asUINT Index = 0; Index < HIR->expressions.GetLength(); ++Index)
		{
			if (HIR->expressions[Index].kind
				== asETypedSemanticExpressionKind::ShortCircuit)
			{
				++ShortCircuitCount;
			}
		}
		ASSERT_THAT(AreEqual(2, ShortCircuitCount,
			TEXT("The compiler must retain both lazy boolean operators")));

		asCScriptFunction* const MutationFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module,
				"bool ShortCircuitMutationInitializer(bool Enabled, int Limit)"));
		ASSERT_THAT(IsNotNull(MutationFunction));
		if (MutationFunction == nullptr)
		{
			return;
		}

		const asCTypedSemanticFunction* const MutationHIR =
			MutationFunction->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(MutationHIR,
			TEXT("A short-circuit operand containing prefix mutation must retain verified HIR")));
		if (MutationHIR != nullptr)
		{
			ASSERT_THAT(IsTrue(
				VerifyTypedSemanticFunction(*MutationHIR).IsValid()));
		}

		asCScriptFunction* const SequenceFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module,
				"bool ShortCircuitAfterMutationSequence(int A, int B, bool Enabled)"));
		ASSERT_THAT(IsNotNull(SequenceFunction));
		if (SequenceFunction == nullptr)
		{
			return;
		}

		const asCTypedSemanticFunction* const SequenceHIR =
			SequenceFunction->GetTypedSemanticFunction();
		const FString SequenceDiagnostic = UTF8_TO_TCHAR(
			SequenceFunction->GetTypedSemanticCaptureDiagnostic().AddressOf());
		ASSERT_THAT(IsNotNull(SequenceHIR,
			*FString::Printf(
				TEXT("A short circuit after a mutation sequence must retain verified HIR: %s"),
				*SequenceDiagnostic)));
		if (SequenceHIR != nullptr)
		{
			ASSERT_THAT(IsTrue(
				VerifyTypedSemanticFunction(*SequenceHIR).IsValid()));
			bool bFoundNegate = false;
			bool bFoundBitwiseNot = false;
			for (asUINT ExpressionIndex = 0;
				ExpressionIndex < SequenceHIR->expressions.GetLength();
				++ExpressionIndex)
			{
				const asSTypedSemanticExpression& Expression =
					SequenceHIR->expressions[ExpressionIndex];
				if (Expression.kind
					!= asETypedSemanticExpressionKind::Unary)
				{
					continue;
				}
				bFoundNegate |= Expression.operatorToken == "-";
				bFoundBitwiseNot |= Expression.operatorToken == "~";
			}
			ASSERT_THAT(IsTrue(bFoundNegate,
				TEXT("Primitive negation must not collapse to its operand")));
			ASSERT_THAT(IsTrue(bFoundBitwiseNot,
				TEXT("Primitive bitwise-not must not collapse to its operand")));
		}
	}

	TEST_METHOD(MutationCapturePreservesRhsFirstAndPrefixPostfixResults)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-MUTATION-CAPTURE",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int MutationAssign(int Input)
			{
				int Value = 1;
				Value = Input;
				return Value;
			}

			int MutationCompound(int Input)
			{
				int Value = Input;
				Value += 3;
				return Value;
			}

			int MutationPrefix(int Input)
			{
				int Value = Input;
				return ++Value;
			}

			int MutationPostfix(int Input)
			{
				int Value = Input;
				return Value++;
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
		static_cast<asCScriptEngine*>(CaptureOnEngine.Get())
			->SetTypedSemanticIRCapture(true);

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRMutationOff",
			Source);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRMutationOn",
			Source);
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		struct FMutationCase
		{
			const char* Declaration;
			const TCHAR* Operator;
			int32 ExpectedResult;
			bool bHasValue;
			bool bReturnsOldValue;
			bool bValueIsInput;
			bool bIsExpressionStatement;
		};
		const FMutationCase Cases[] = {
			{"int MutationAssign(int Input)", TEXT("="), 5, true, false, true, true},
			{"int MutationCompound(int Input)", TEXT("+="), 8, true, false, false, true},
			{"int MutationPrefix(int Input)", TEXT("++"), 6, false, false, false, false},
			{"int MutationPostfix(int Input)", TEXT("++"), 5, false, true, false, false},
		};

		for (const FMutationCase& Case : Cases)
		{
			asCScriptFunction* const CaptureOffFunction =
				static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
					CaptureOffModule, Case.Declaration));
			asCScriptFunction* const CaptureOnFunction =
				static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
					CaptureOnModule, Case.Declaration));
			ASSERT_THAT(IsNotNull(CaptureOffFunction));
			ASSERT_THAT(IsNotNull(CaptureOnFunction));
			if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
			{
				continue;
			}

			ASSERT_THAT(AreEqual(
				CopyBytecode(CaptureOffFunction),
				CopyBytecode(CaptureOnFunction),
				TEXT("Mutation capture must not change any bytecode word")));
			int32 CaptureOffResult = 0;
			int32 CaptureOnResult = 0;
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOffEngine.Get(),
				CaptureOffFunction,
				5,
				CaptureOffResult)));
			ASSERT_THAT(IsTrue(ExecuteUnaryScalar(
				CaptureOnEngine.Get(),
				CaptureOnFunction,
				5,
				CaptureOnResult)));
			ASSERT_THAT(AreEqual(Case.ExpectedResult, CaptureOffResult));
			ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult));

			const asCTypedSemanticFunction* const HIR =
				CaptureOnFunction->GetTypedSemanticFunction();
			const FString MissingHIRMessage = FString::Printf(
				TEXT("%s should publish verified HIR"),
				UTF8_TO_TCHAR(Case.Declaration));
			ASSERT_THAT(IsNotNull(HIR,
				*MissingHIRMessage));
			if (HIR == nullptr)
			{
				continue;
			}
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));

			const asSTypedSemanticExpression* Mutation = nullptr;
			int32 MutationCount = 0;
			for (asUINT ExpressionIndex = 0;
				ExpressionIndex < HIR->expressions.GetLength();
				++ExpressionIndex)
			{
				if (HIR->expressions[ExpressionIndex].kind
					== asETypedSemanticExpressionKind::Assignment)
				{
					Mutation = &HIR->expressions[ExpressionIndex];
					++MutationCount;
				}
			}
			ASSERT_THAT(AreEqual(1, MutationCount,
				TEXT("A source mutation should produce exactly one mutation node")));
			ASSERT_THAT(IsNotNull(Mutation));
			if (Mutation == nullptr)
			{
				continue;
			}
			int32 OwningExpressionStatementCount = 0;
			for (asUINT StatementIndex = 0;
				StatementIndex < HIR->statements.GetLength();
				++StatementIndex)
			{
				const asSTypedSemanticStatement& Statement =
					HIR->statements[StatementIndex];
				if (Statement.kind
						== asETypedSemanticStatementKind::Expression
					&& Statement.valueExpression == Mutation->id)
				{
					++OwningExpressionStatementCount;
				}
			}
			ASSERT_THAT(AreEqual(
				Case.bIsExpressionStatement ? 1 : 0,
				OwningExpressionStatementCount,
				TEXT("Statement mutations must be owned by exactly one expression statement")));

			ASSERT_THAT(AreEqual(
				FString(Case.Operator),
				FString(UTF8_TO_TCHAR(Mutation->operatorToken.AddressOf()))));
			ASSERT_THAT(IsTrue(Mutation->mutationTarget.IsValid(),
				TEXT("A mutation should own one stable target expression")));
			ASSERT_THAT(AreEqual(
				Case.bHasValue,
				Mutation->mutationValue.IsValid()));
			ASSERT_THAT(AreEqual(
				Case.bReturnsOldValue,
				Mutation->mutationReturnsOldValue));
			if (!Mutation->mutationTarget.IsValid())
			{
				continue;
			}

			const asSTypedSemanticExpression& Target =
				HIR->expressions[Mutation->mutationTarget.value];
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::Symbol,
				Target.kind));
			ASSERT_THAT(AreEqual(
				FString(TEXT("Value")),
				FString(UTF8_TO_TCHAR(
					HIR->symbols[Target.symbol.value].name.AddressOf()))));

			if (Mutation->mutationValue.IsValid())
			{
				const asSTypedSemanticExpression& Value =
					HIR->expressions[Mutation->mutationValue.value];
				ASSERT_THAT(IsTrue(
					Mutation->mutationValue.value
						< Mutation->mutationTarget.value,
					TEXT("Assignment HIR must preserve the compiler's RHS-before-LHS evaluation")));
				if (Case.bValueIsInput)
				{
					ASSERT_THAT(AreEqual(
						asETypedSemanticExpressionKind::Symbol,
						Value.kind));
					ASSERT_THAT(AreEqual(
						FString(TEXT("Input")),
						FString(UTF8_TO_TCHAR(
							HIR->symbols[Value.symbol.value].name.AddressOf()))));
				}
				else
				{
					ASSERT_THAT(AreEqual(
						asETypedSemanticExpressionKind::Literal,
						Value.kind));
					ASSERT_THAT(AreEqual(
						FString(TEXT("3")),
						FString(UTF8_TO_TCHAR(Value.literal.AddressOf()))));
				}
			}

			const FString Dump = UTF8_TO_TCHAR(
				DumpTypedSemanticFunction(*HIR).AddressOf());
			ASSERT_THAT(IsTrue(Dump.Contains(TEXT("kind=Assignment"))));
			ASSERT_THAT(IsTrue(Dump.Contains(TEXT("target=E"))));
			ASSERT_THAT(IsTrue(Dump.Contains(
				Case.bReturnsOldValue
					? TEXT("returns=old")
					: TEXT("returns=updated"))));
			ASSERT_THAT(IsFalse(Dump.Contains(TEXT("0x"))));
		}
	}

	TEST_METHOD(TypedIdsDefaultInvalidAndArenasAreContiguous)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-INDEXED-ARENAS",
			ENativeEvidence::Metadata | ENativeEvidence::Isolation);

		ASSERT_THAT(IsFalse(asTypedSemanticSymbolId().IsValid(),
			TEXT("A default symbol ID must be invalid")));
		ASSERT_THAT(IsFalse(asTypedSemanticExpressionId().IsValid(),
			TEXT("A default expression ID must be invalid")));
		ASSERT_THAT(IsFalse(asTypedSemanticStatementId().IsValid(),
			TEXT("A default statement ID must be invalid")));

		asCTypedSemanticFunction Function = MakeScalarBranch();
		ASSERT_THAT(AreEqual(3, static_cast<int32>(Function.symbols.GetLength()),
			TEXT("The scalar model should own three symbols")));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Function.expressions.GetLength()),
			TEXT("The scalar model should own twelve expressions")));
		ASSERT_THAT(AreEqual(6, static_cast<int32>(Function.statements.GetLength()),
			TEXT("The scalar model should own six statements")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Function.symbols[0].id.value),
			TEXT("Symbol arena IDs should start at zero")));
		ASSERT_THAT(AreEqual(11, static_cast<int32>(Function.expressions[11].id.value),
			TEXT("Expression IDs should equal their arena index")));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(Function.rootStatement.value),
			TEXT("The root should reference the final statement arena entry")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("int")),
			FString(UTF8_TO_TCHAR(Function.symbols[0].type.Format(0, true).AddressOf())),
			TEXT("Symbols should retain the compiler's exact data type")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("input.as")),
			FString(UTF8_TO_TCHAR(Function.symbols[0].span.section.AddressOf())),
			TEXT("Source spans should own their section identity")));

		asSTypedSemanticExpression Call;
		Call.kind = asETypedSemanticExpressionKind::ResolvedCall;
		Call.type = asCDataType::CreatePrimitive(ttInt, false);
		Call.span = MakeSpan();
		Call.callTargetKind =
			asETypedSemanticCallTargetKind::ScriptFunction;
		Call.resolvedFunctionId = 17;
		Call.targetProvenance.canonicalDeclaration = "int Target()";
		Call.targetProvenance.declarationModule = "SyntheticCompiler";
		Call.targetProvenance.bodyAvailability =
			asETypedSemanticBodyAvailability::ScriptBody;
		Call.targetProvenance.bodyOwnership =
			asETypedSemanticBodyOwnership::CallingModule;
		Call.targetProvenance.bodyOwnerModule = "SyntheticCompiler";
		const asTypedSemanticExpressionId CallId = Function.AddExpression(Call);
		ASSERT_THAT(AreEqual(17, Function.expressions[CallId.value].resolvedFunctionId,
			TEXT("Resolved calls should retain an engine-local function ID")));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(Function).IsValid(),
			TEXT("An unused resolved-call record with a stable ID should remain structurally valid")));
		const FString CallDump = UTF8_TO_TCHAR(DumpTypedSemanticFunction(Function).AddressOf());
		ASSERT_THAT(IsTrue(CallDump.Contains(TEXT("functionId=17")),
			TEXT("The normalized dump should expose the resolved numeric target")));
		ASSERT_THAT(IsFalse(CallDump.Contains(TEXT("0x")),
			TEXT("Resolved targets must not introduce retained pointer spellings")));
	}

	TEST_METHOD(ScalarBranchVerifiesAndDumpsDeterministically)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-VERIFY-DUMP",
			ENativeEvidence::Metadata | ENativeEvidence::Diagnostic | ENativeEvidence::Isolation);

		const asCTypedSemanticFunction First = MakeScalarBranch();
		const asCTypedSemanticFunction Second = MakeScalarBranch();
		const asSTypedSemanticVerificationResult Verification = VerifyTypedSemanticFunction(First);
		ASSERT_THAT(IsTrue(Verification.IsValid(),
			TEXT("The scalar branch HIR should satisfy the structural verifier")));

		const asCString FirstDump = DumpTypedSemanticFunction(First);
		const asCString SecondDump = DumpTypedSemanticFunction(Second);
		const FString ExpectedDump = UTF8_TO_TCHAR(GetExpectedScalarBranchDump());
		const FString ActualDump = UTF8_TO_TCHAR(FirstDump.AddressOf());
		ASSERT_THAT(AreEqual(
			ActualDump,
			FString(UTF8_TO_TCHAR(SecondDump.AddressOf())),
			TEXT("Separately constructed scalar HIR must have the same normalized dump")));
		ASSERT_THAT(AreEqual(
			ExpectedDump,
			ActualDump,
			*FString::Printf(
				TEXT("The native normalized dump should match the committed scalar research oracle exactly.\nExpected:\n%s\nActual:\n%s"),
				*ExpectedDump,
				*ActualDump)));
		ASSERT_THAT(IsFalse(FString(UTF8_TO_TCHAR(FirstDump.AddressOf())).Contains(TEXT("0x")),
			TEXT("The normalized HIR dump must not contain pointer spellings")));
		ASSERT_THAT(IsTrue(FString(UTF8_TO_TCHAR(FirstDump.AddressOf())).Contains(
			TEXT("function int SemanticScalarBranch(int A, int B)")),
			TEXT("The normalized dump should retain the canonical function declaration")));
		ASSERT_THAT(IsTrue(FString(UTF8_TO_TCHAR(FirstDump.AddressOf())).Contains(
			TEXT("E11 kind=Binary type=int operator=- operands=[E9,E10]")),
			TEXT("The normalized dump should retain typed ordered expression operands")));
	}

	TEST_METHOD(VerifierRejectsDanglingExpressionsAndMultipleStatementOwnership)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-FAIL-CLOSED",
			ENativeEvidence::Diagnostic | ENativeEvidence::Metadata);

		asCTypedSemanticFunction Dangling = MakeScalarBranch();
		Dangling.statements[4].valueExpression = asTypedSemanticExpressionId(99);
		const asSTypedSemanticVerificationResult DanglingResult = VerifyTypedSemanticFunction(Dangling);
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::DanglingExpressionId,
			DanglingResult.error,
			TEXT("A dangling return expression should fail with the stable typed error")));
		const asSTypedSemanticSnapshotResult DanglingSnapshot =
			BuildTypedSemanticSnapshot(&Dangling);
		ASSERT_THAT(AreEqual(
			asETypedSemanticSnapshotState::VerificationFailed,
			DanglingSnapshot.state,
			TEXT("Invalid HIR should fail the compiler snapshot helper")));
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::DanglingExpressionId,
			DanglingSnapshot.verification.error,
			TEXT("Snapshot failure should preserve the exact verifier diagnostic")));
		ASSERT_THAT(IsTrue(
			FString(UTF8_TO_TCHAR(DanglingSnapshot.json.AddressOf())).Contains(
				TEXT("\"normalizedHIR\":null")),
			TEXT("A verifier-invalid snapshot must not publish normalized successful HIR")));

		asCTypedSemanticFunction SharedStatement = MakeScalarBranch();
		SharedStatement.statements[5].children.PushLast(asTypedSemanticStatementId(1));
		const asSTypedSemanticVerificationResult OwnershipResult = VerifyTypedSemanticFunction(SharedStatement);
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::StatementHasMultipleOwners,
			OwnershipResult.error,
			TEXT("A statement owned by two blocks should fail closed")));

		asCTypedSemanticFunction NonBlockRoot = MakeScalarBranch();
		NonBlockRoot.rootStatement = asTypedSemanticStatementId(0);
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::RootIsNotBlock,
			VerifyTypedSemanticFunction(NonBlockRoot).error,
			TEXT("A non-block function root should fail with a stable typed error")));

		asCTypedSemanticFunction MissingUnsupportedKind = MakeScalarBranch();
		MissingUnsupportedKind.expressions[0].kind = asETypedSemanticExpressionKind::Unsupported;
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::MissingUnsupportedCategory,
			VerifyTypedSemanticFunction(MissingUnsupportedKind).error,
			TEXT("An unsupported record must carry a stable unsupported category")));

		asCTypedSemanticFunction UnspecifiedCallTarget = MakeScalarBranch();
		asSTypedSemanticExpression Call;
		Call.kind = asETypedSemanticExpressionKind::ResolvedCall;
		Call.type = asCDataType::CreatePrimitive(ttInt, false);
		Call.span = MakeSpan();
		Call.resolvedFunctionId = 17;
		UnspecifiedCallTarget.AddExpression(Call);
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::InvalidNodeShape,
			VerifyTypedSemanticFunction(UnspecifiedCallTarget).error,
			TEXT("A resolved call without an authoritative target kind must fail closed")));

		asCTypedSemanticFunction NonContiguous = MakeScalarBranch();
		NonContiguous.expressions[1].id = asTypedSemanticExpressionId(41);
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::NonContiguousExpressionId,
			VerifyTypedSemanticFunction(NonContiguous).error,
			TEXT("Arena IDs that do not equal their indices should fail closed")));

		asCTypedSemanticFunction DanglingMutation = MakeScalarBranch();
		asSTypedSemanticExpression Mutation;
		Mutation.kind = asETypedSemanticExpressionKind::Assignment;
		Mutation.type = asCDataType::CreatePrimitive(ttInt, false);
		Mutation.span = MakeSpan();
		Mutation.operatorToken = "=";
		Mutation.mutationTarget = asTypedSemanticExpressionId(99);
		Mutation.mutationValue = asTypedSemanticExpressionId(0);
		DanglingMutation.AddExpression(Mutation);
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::DanglingExpressionId,
			VerifyTypedSemanticFunction(DanglingMutation).error,
			TEXT("A mutation target outside the expression arena must fail closed")));
	}

	TEST_METHOD(ScriptFunctionOwnsAndDiscardsTypedHIR)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-FUNCTION-LIFETIME",
			ENativeEvidence::Lifecycle | ENativeEvidence::Cleanup | ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int OwnedHIR()
			{
				return 42;
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "TypedSemanticIROwnership", ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(Module, "int OwnedHIR()"));
		ASSERT_THAT(IsNotNull(Function,
			TEXT("The ownership fixture should publish its exact script function")));
		if (Function == nullptr)
		{
			return;
		}

		Function->SetTypedSemanticFunction(asNEW(asCTypedSemanticFunction)(MakeScalarBranch()));
		ASSERT_THAT(IsNotNull(Function->GetTypedSemanticFunction(),
			TEXT("The script function should own the installed HIR")));
		Function->DiscardTypedSemanticFunction();
		ASSERT_THAT(IsNull(Function->GetTypedSemanticFunction(),
			TEXT("Discard should synchronously release and clear the function-owned HIR")));

		Function->SetTypedSemanticFunction(asNEW(asCTypedSemanticFunction)(MakeScalarBranch()));
		ASSERT_THAT(IsNotNull(Function->GetTypedSemanticFunction(),
			TEXT("A replacement HIR should be owned until normal function teardown")));
	}

	TEST_METHOD(FailedModuleBuildPublishesNoProvisionalTypedHIR)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-FAILED-BUILD-DISCARD",
			ENativeEvidence::Lifecycle | ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		ScriptEngine->SetTypedSemanticIRCapture(true);
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "TypedSemanticIRFailedBuildDiscard");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const char* const Source = R"AS(
			int CompilesBeforeFailure()
			{
				return 7;
			}

			int FailsAfterFirstBody()
			{
				return MissingTypedSemanticSymbol + 1;
			}
			)AS";
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module, "TypedSemanticIRFailedBuildDiscard.as", Source)));
		const int BuildResult = Module->Build();
		ASSERT_THAT(IsTrue(BuildResult < 0,
			TEXT("The fixture must fail after at least one earlier body was eligible for provisional HIR capture")));

		int32 ModuleOwnedHIRCount = 0;
		for (asUINT Index = 0; Index < Module->scriptFunctions.GetLength();
			++Index)
		{
			const asCScriptFunction* const Function =
				Module->scriptFunctions[Index];
			if (Function != nullptr
				&& Function->GetTypedSemanticFunction() != nullptr)
			{
				++ModuleOwnedHIRCount;
			}
		}
		int32 EngineOwnedHIRCount = 0;
		for (asUINT Index = 0; Index < ScriptEngine->scriptFunctions.GetLength();
			++Index)
		{
			const asCScriptFunction* const Function =
				ScriptEngine->scriptFunctions[Index];
			if (Function != nullptr && Function->module == Module
				&& Function->GetTypedSemanticFunction() != nullptr)
			{
				++EngineOwnedHIRCount;
			}
		}

		ASSERT_THAT(AreEqual(0, ModuleOwnedHIRCount,
			TEXT("A failed module transaction must retain no function-owned HIR in its module list")));
		ASSERT_THAT(AreEqual(0, EngineOwnedHIRCount,
			TEXT("A failed module transaction must retain no function-owned HIR in the Engine function table")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl(
			"int CompilesBeforeFailure()"),
			TEXT("A body compiled before the later error must not become a published module function")));
	}

	TEST_METHOD(ModuleReplacementKeepsHIRBoundToExactFunctionLifetime)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-MODULE-REPLACEMENT-LIFETIME",
			ENativeEvidence::Lifecycle | ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ScriptEngine->SetTypedSemanticIRCapture(true);

		static constexpr const char* ModuleName =
			"TypedSemanticIRModuleReplacement";
		asCModule* const FirstModule = CreateBuilderModule(
			ScriptEngine, ModuleName);
		ASSERT_THAT(IsNotNull(FirstModule));
		if (FirstModule == nullptr
			|| !AddBuilderSection(*FirstModule,
				"TypedSemanticIRModuleReplacementA.as",
				"int VersionedHIR() { return 11; }")
			|| FirstModule->Build() < 0)
		{
			return;
		}

		asCScriptFunction* const OldFunction =
			static_cast<asCScriptFunction*>(
				FirstModule->GetFunctionByDecl("int VersionedHIR()"));
		ASSERT_THAT(IsNotNull(OldFunction));
		if (OldFunction == nullptr
			|| OldFunction->GetTypedSemanticFunction() == nullptr)
		{
			return;
		}
		OldFunction->AddRef();
		bool bOldFunctionReleased = false;
		ON_SCOPE_EXIT
		{
			if (!bOldFunctionReleased)
			{
				OldFunction->Release();
			}
		};

		const int OldFunctionId = OldFunction->GetId();
		const asCTypedSemanticFunction* const OldHIR =
			OldFunction->GetTypedSemanticFunction();
		const FString OldDump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*OldHIR).AddressOf());

		asCModule* const ReplacementModule = CreateBuilderModule(
			ScriptEngine, ModuleName);
		ASSERT_THAT(IsNotNull(ReplacementModule));
		if (ReplacementModule == nullptr
			|| !AddBuilderSection(*ReplacementModule,
				"TypedSemanticIRModuleReplacementB.as",
				"int VersionedHIR() { return 22; }")
			|| ReplacementModule->Build() < 0)
		{
			return;
		}

		asCScriptFunction* const NewFunction =
			static_cast<asCScriptFunction*>(
				ReplacementModule->GetFunctionByDecl("int VersionedHIR()"));
		ASSERT_THAT(IsNotNull(NewFunction));
		ASSERT_THAT(IsTrue(NewFunction != OldFunction,
			TEXT("Replacing a module must publish a distinct current function object")));
		ASSERT_THAT(IsTrue(NewFunction != nullptr
			&& NewFunction->GetTypedSemanticFunction() != nullptr,
			TEXT("The replacement function must own HIR from its own source compilation")));
		ASSERT_THAT(IsTrue(NewFunction == nullptr
			|| NewFunction->GetTypedSemanticFunction() != OldHIR,
			TEXT("Replacement must not adopt the retired function's HIR pointer")));

		ASSERT_THAT(AreEqual(OldHIR,
			OldFunction->GetTypedSemanticFunction(),
			TEXT("An externally retained retired function must keep its own HIR until its exact lifetime ends")));
		ASSERT_THAT(AreEqual(OldDump,
			FString(UTF8_TO_TCHAR(DumpTypedSemanticFunction(
				*OldFunction->GetTypedSemanticFunction()).AddressOf())),
			TEXT("Module replacement must not mutate the retired function's owned HIR")));
		ASSERT_THAT(AreEqual(
			static_cast<asIScriptFunction*>(OldFunction),
			ScriptEngine->GetFunctionById(OldFunctionId),
			TEXT("The Engine function table must retain an externally referenced retired function until final release")));

		OldFunction->Release();
		bOldFunctionReleased = true;
		asCScriptFunction* const DiscardedModuleOwnedFunction =
			static_cast<asCScriptFunction*>(
				ScriptEngine->GetFunctionById(OldFunctionId));
		ASSERT_THAT(AreEqual(OldFunction, DiscardedModuleOwnedFunction,
			TEXT("Releasing the last external reference must not bypass the discarded module's internal function ownership")));
		ASSERT_THAT(IsTrue(DiscardedModuleOwnedFunction != nullptr
			&& DiscardedModuleOwnedFunction->GetTypedSemanticFunction() == OldHIR,
			TEXT("The discarded module must retain the exact old HIR until its documented GC cleanup boundary")));
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->GarbageCollect(asGC_FULL_CYCLE),
			TEXT("A full collection must process the maintained fork's discarded-module cleanup boundary")));
		ASSERT_THAT(IsNull(ScriptEngine->GetFunctionById(OldFunctionId),
			TEXT("Discarded-module cleanup must destroy the unreferenced retired function and remove its Engine-table identity")));
		ASSERT_THAT(AreEqual(
			static_cast<asIScriptFunction*>(NewFunction),
			ReplacementModule->GetFunctionByDecl("int VersionedHIR()"),
			TEXT("Destroying the retired function must not affect the replacement function")));
	}

	TEST_METHOD(GlobalUsesRetainOriginDependencyAndInitializerKind)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-GLOBAL-ORIGIN",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ScriptEngine->SetTypedSemanticIRCapture(true);

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			const int FoldedGlobal = 40 + 2;

			int BuildStoredGlobal()
			{
				return FoldedGlobal + 1;
			}

			const int StoredGlobal = BuildStoredGlobal();

			int ReadFoldedGlobal()
			{
				return FoldedGlobal;
			}

			int ReadStoredGlobal()
			{
				return StoredGlobal;
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedSemanticIRGlobalOrigin",
			Source);
		if (!Module.IsValid())
		{
			return;
		}

		asCModule* const InternalModule =
			static_cast<asCModule*>(Module.Get());
		asCGlobalProperty* const FoldedGlobal =
			InternalModule->GetGlobalProperty(
				InternalModule->defaultNamespace, "FoldedGlobal");
		asCGlobalProperty* const StoredGlobal =
			InternalModule->GetGlobalProperty(
				InternalModule->defaultNamespace, "StoredGlobal");
		asCScriptFunction* const FoldedReader =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module, "int ReadFoldedGlobal()"));
		asCScriptFunction* const StoredReader =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module, "int ReadStoredGlobal()"));
		ASSERT_THAT(IsNotNull(FoldedGlobal));
		ASSERT_THAT(IsNotNull(StoredGlobal));
		ASSERT_THAT(IsNotNull(FoldedReader));
		ASSERT_THAT(IsNotNull(StoredReader));
		if (FoldedGlobal == nullptr || StoredGlobal == nullptr
			|| FoldedReader == nullptr || StoredReader == nullptr)
		{
			return;
		}

		auto FindExpression = [](const asCScriptFunction& Function,
			const asETypedSemanticExpressionKind Kind)
			-> const asSTypedSemanticExpression*
		{
			const asCTypedSemanticFunction* const HIR =
				Function.GetTypedSemanticFunction();
			if (HIR == nullptr)
			{
				return nullptr;
			}
			for (asUINT Index = 0; Index < HIR->expressions.GetLength();
				++Index)
			{
				if (HIR->expressions[Index].kind == Kind)
				{
					return &HIR->expressions[Index];
				}
			}
			return nullptr;
		};
		auto HasGlobalDependency = [](const asCScriptFunction& Function,
			const asEBuildArtifactDependencyKind Kind,
			const asCGlobalProperty* const Property)
		{
			if (Function.scriptData == nullptr)
			{
				return false;
			}
			for (asUINT Index = 0;
				Index < Function.scriptData->artifactDependencies.GetLength();
				++Index)
			{
				const asSBuildArtifactDependency& Dependency =
					Function.scriptData->artifactDependencies[Index];
				if (Dependency.kind == Kind
					&& Dependency.referenceKind
						== asBUILD_ARTIFACT_REFERENCE_GLOBAL
					&& Dependency.globalProperty == Property)
				{
					return true;
				}
			}
			return false;
		};

		const asSTypedSemanticExpression* const FoldedUse = FindExpression(
			*FoldedReader,
			asETypedSemanticExpressionKind::FoldedGlobalConstant);
		ASSERT_THAT(IsNotNull(FoldedUse,
			TEXT("A compiler-folded global must not become an origin-free literal")));
		ASSERT_THAT(IsTrue(FoldedUse != nullptr
			&& FoldedUse->resolvedGlobalPropertyId
				== static_cast<int>(FoldedGlobal->id)));
		ASSERT_THAT(IsTrue(FoldedUse != nullptr
			&& FoldedUse->literalBits == asQWORD{42}));
		ASSERT_THAT(IsTrue(HasGlobalDependency(
			*FoldedReader,
			asBUILD_ARTIFACT_DEPENDENCY_HARD_VALUE,
			FoldedGlobal),
			TEXT("The folded HIR use must retain compiler hard-value authority")));

		const asSTypedSemanticExpression* const StorageUse = FindExpression(
			*StoredReader,
			asETypedSemanticExpressionKind::GlobalStorage);
		ASSERT_THAT(IsNotNull(StorageUse,
			TEXT("A non-folded global read must remain an explicit storage use")));
		ASSERT_THAT(IsTrue(StorageUse != nullptr
			&& StorageUse->resolvedGlobalPropertyId
				== static_cast<int>(StoredGlobal->id)));
		ASSERT_THAT(IsTrue(HasGlobalDependency(
			*StoredReader,
			asBUILD_ARTIFACT_DEPENDENCY_GLOBAL_STORAGE,
			StoredGlobal),
			TEXT("The storage HIR use must retain compiler storage authority")));

		asCScriptFunction* const Initializer = StoredGlobal->GetInitFunc();
		ASSERT_THAT(IsNotNull(Initializer,
			TEXT("The non-folded fixture must retain an anonymous initializer body")));
		const asCTypedSemanticFunction* const InitializerHIR =
			Initializer != nullptr
				? Initializer->GetTypedSemanticFunction()
				: nullptr;
		ASSERT_THAT(IsNotNull(InitializerHIR,
			TEXT("CompileGlobalVariable must publish a verified HIR sidecar")));
		ASSERT_THAT(IsTrue(InitializerHIR != nullptr
			&& InitializerHIR->header.invocationKind
				== asETypedSemanticInvocationKind::GlobalInitializer));
		ASSERT_THAT(IsTrue(InitializerHIR == nullptr
			|| VerifyTypedSemanticFunction(*InitializerHIR).IsValid()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
