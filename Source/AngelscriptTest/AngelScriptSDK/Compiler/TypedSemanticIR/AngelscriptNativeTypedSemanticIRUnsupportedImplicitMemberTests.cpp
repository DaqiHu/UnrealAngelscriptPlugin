#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRUnsupportedImplicitMemberTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		if (Context->Prepare(Function) < 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		OutResult = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

public:
	TEST_METHOD(ImplicitAndExplicitThisPropertyAccessRetainOneEffectiveReceiver)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-UNSUPPORTED-IMPLICIT-MEMBER",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class FTypedImplicitMemberCarrier
			{
				int Value;

				int ReadBoth()
				{
					return Value + this.Value;
				}
			}

			int RunTypedImplicitMember()
			{
				FTypedImplicitMemberCarrier Carrier = FTypedImplicitMemberCarrier();
				Carrier.Value = 19;
				return Carrier.ReadBoth();
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
		ASSERT_THAT(IsTrue(
			CaptureOnScriptEngine->SetTypedSemanticIRCapture(true),
			TEXT("The case-owned capture Engine should enable HIR before build")));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRUnsupportedImplicitMemberOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRUnsupportedImplicitMemberOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid(),
			TEXT("Implicit member syntax must remain valid with capture off")));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid(),
			TEXT("Implicit member syntax must remain valid with capture on")));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asITypeInfo* const CaptureOffType =
			CaptureOffModule->GetTypeInfoByDecl("FTypedImplicitMemberCarrier");
		asITypeInfo* const CaptureOnType =
			CaptureOnModule->GetTypeInfoByDecl("FTypedImplicitMemberCarrier");
		asCScriptFunction* const CaptureOffRead =
			static_cast<asCScriptFunction*>(GetNativeMethodByDecl(
				CaptureOffType,
				"int ReadBoth()"));
		asCScriptFunction* const CaptureOnRead =
			static_cast<asCScriptFunction*>(GetNativeMethodByDecl(
				CaptureOnType,
				"int ReadBoth()"));
		asIScriptFunction* const CaptureOffRun = GetNativeFunctionByDecl(
			CaptureOffModule,
			"int RunTypedImplicitMember()");
		asIScriptFunction* const CaptureOnRun = GetNativeFunctionByDecl(
			CaptureOnModule,
			"int RunTypedImplicitMember()");
		ASSERT_THAT(IsNotNull(CaptureOffRead));
		ASSERT_THAT(IsNotNull(CaptureOnRead));
		ASSERT_THAT(IsNotNull(CaptureOffRun));
		ASSERT_THAT(IsNotNull(CaptureOnRun));
		if (CaptureOffRead == nullptr || CaptureOnRead == nullptr
			|| CaptureOffRun == nullptr || CaptureOnRun == nullptr)
		{
			return;
		}

		asUINT CaptureOffBytecodeLength = 0;
		asUINT CaptureOnBytecodeLength = 0;
		ASSERT_THAT(IsNotNull(CaptureOffRead->GetByteCode(
			&CaptureOffBytecodeLength)));
		ASSERT_THAT(IsNotNull(CaptureOnRead->GetByteCode(
			&CaptureOnBytecodeLength)));
		ASSERT_THAT(IsTrue(CaptureOffBytecodeLength > 0));
		ASSERT_THAT(AreEqual(CaptureOffBytecodeLength,
			CaptureOnBytecodeLength,
			TEXT("Capture must not alter the method bytecode shape")));
		ASSERT_THAT(IsNull(CaptureOffRead->GetTypedSemanticFunction(),
			TEXT("Capture-off compilation must not attach a semantic sidecar")));

		int32 CaptureOffResult = 0;
		int32 CaptureOnResult = 0;
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOffEngine.Get(),
			CaptureOffRun,
			CaptureOffResult)));
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOnEngine.Get(),
			CaptureOnRun,
			CaptureOnResult)));
		ASSERT_THAT(AreEqual(38, CaptureOffResult));
		ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult,
			TEXT("HIR receiver metadata must not change VM member behavior")));

		const asCTypedSemanticFunction* const HIR =
			CaptureOnRead->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR,
			TEXT("Valid implicit-member fallback should publish inspectable HIR")));
		if (HIR == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			asETypedSemanticReceiverKind::NativeObjectThis,
			HIR->header.receiver.kind,
			TEXT("The class method should retain its synthetic native receiver")));
		ASSERT_THAT(IsTrue(HIR->header.receiver.symbol.IsValid(),
			TEXT("The effective receiver must name one function-owned symbol")));

		asUINT ObjectAccessCount = 0;
		asUINT PropertyAccessCount = 0;
		for (asUINT ExpressionIndex = 0;
			ExpressionIndex < HIR->expressions.GetLength();
			++ExpressionIndex)
		{
			const asSTypedSemanticExpression& Expression =
				HIR->expressions[ExpressionIndex];
			if (Expression.kind == asETypedSemanticExpressionKind::Unsupported
				&& Expression.unsupportedKind
					== asETypedSemanticUnsupportedKind::ObjectAccess)
			{
				++ObjectAccessCount;
				ASSERT_THAT(AreEqual(asUINT{1}, Expression.operands.GetLength(),
					TEXT("Each object marker should retain its effective receiver")));
				ASSERT_THAT(IsTrue(Expression.operands.GetLength() == 1
					&& Expression.operands[0].value < Expression.id.value,
					TEXT("The effective receiver should precede its object marker")));
				if (Expression.operands.GetLength() != 1
					|| Expression.operands[0].value >= HIR->expressions.GetLength())
				{
					continue;
				}

				const asSTypedSemanticExpression& ReceiverSymbol =
					HIR->expressions[Expression.operands[0].value];
				ASSERT_THAT(AreEqual(
					asETypedSemanticExpressionKind::Symbol,
					ReceiverSymbol.kind));
				ASSERT_THAT(AreEqual(
					HIR->header.receiver.symbol.value,
					ReceiverSymbol.symbol.value,
					TEXT("Both member spellings must start from the same receiver symbol")));
				ASSERT_THAT(AreEqual(
					FString(TEXT("FTypedImplicitMemberCarrier::this")),
					FString(UTF8_TO_TCHAR(Expression.resolvedTarget.AddressOf()))));
				continue;
			}
			if (Expression.kind != asETypedSemanticExpressionKind::Unsupported
				|| Expression.unsupportedKind
					!= asETypedSemanticUnsupportedKind::PropertyAccess)
			{
				continue;
			}

			++PropertyAccessCount;
			ASSERT_THAT(AreEqual(asUINT{1}, Expression.operands.GetLength(),
				TEXT("Each member marker should retain exactly one receiver")));
			ASSERT_THAT(IsTrue(Expression.operands.GetLength() == 1
				&& Expression.operands[0].value < HIR->expressions.GetLength(),
				TEXT("Each receiver operand should name a prior HIR expression")));
			if (Expression.operands.GetLength() != 1
				|| Expression.operands[0].value >= HIR->expressions.GetLength())
			{
				continue;
			}

			const asSTypedSemanticExpression& Receiver =
				HIR->expressions[Expression.operands[0].value];
			ASSERT_THAT(AreEqual(
				asETypedSemanticExpressionKind::Unsupported,
				Receiver.kind,
				TEXT("A member marker must retain the resolved object access")));
			ASSERT_THAT(AreEqual(
				asETypedSemanticUnsupportedKind::ObjectAccess,
				Receiver.unsupportedKind,
				TEXT("A member marker must not erase the object-access boundary")));
			ASSERT_THAT(AreEqual(
				FString(TEXT("FTypedImplicitMemberCarrier::Value")),
				FString(UTF8_TO_TCHAR(Expression.resolvedTarget.AddressOf()))));
			ASSERT_THAT(IsTrue(Expression.type.IsEqualExceptRefAndConst(
				asCDataType::CreatePrimitive(ttInt, false))));
			ASSERT_THAT(IsTrue(Expression.span.sourceLength > 0));
		}

		ASSERT_THAT(AreEqual(asUINT{2}, ObjectAccessCount,
			TEXT("Unqualified and explicit this should each retain object access")));
		ASSERT_THAT(AreEqual(asUINT{2}, PropertyAccessCount,
			TEXT("Unqualified and explicit-this access should each retain a marker")));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid(),
			TEXT("The complete implicit-member receiver graph should verify")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
