#include "CQTest.h"

#include "../../../AngelScriptSDK/Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"

#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#include <cmath>
#include <limits>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITFloatingConversionTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.FloatingConversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asCScriptFunction* BuildFixture(
		FAutomationTestBase& Test,
		AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		TUniquePtr<AngelscriptNativeTestSupport::FScopedNativeModule>& OutModule)
	{
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		if (!Test.TestNotNull(TEXT("Floating conversion Engine must exist"),
			ScriptEngine))
		{
			return nullptr;
		}
		if (!Test.TestTrue(TEXT("Floating conversion fixture must enable HIR capture"),
			ScriptEngine->SetTypedSemanticIRCapture(true)))
		{
			return nullptr;
		}

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			float32 NarrowFloat64(float Value)
			{
				return float32(Value);
			}
			)AS");
		OutModule = MakeUnique<
			AngelscriptNativeTestSupport::FScopedNativeModule>(
				Test,
				Engine,
				"TypedASTJITFloatingConversions",
				ScriptSource);
		if (!Test.TestTrue(TEXT("Floating conversion fixture must compile"),
			OutModule->IsValid()))
		{
			return nullptr;
		}

		asCModule* const ScriptModule =
			static_cast<asCModule*>(OutModule->Get());
		if (!Test.TestNotNull(TEXT("Floating conversion module must exist"),
			ScriptModule))
		{
			return nullptr;
		}
		asCScriptFunction* const ScriptFunction =
			static_cast<asCScriptFunction*>(
				ScriptModule->GetFunctionByName("NarrowFloat64"));
		Test.TestNotNull(TEXT("Floating conversion function must exist"),
			ScriptFunction);
		return ScriptFunction;
	}

public:
	TEST_METHOD(Float64ToFloat32EmitsTheReviewedNativeConversion)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		TUniquePtr<FScopedNativeModule> Module;
		asCScriptFunction* const ScriptFunction =
			BuildFixture(*TestRunner, Engine, Module);
		ASSERT_THAT(IsNotNull(ScriptFunction));
		if (ScriptFunction == nullptr)
		{
			return;
		}

		const asCTypedSemanticFunction* const HIR =
			ScriptFunction->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR));
		if (HIR == nullptr)
		{
			return;
		}
		bool bSawNarrowingConversion = false;
		for (asUINT Index = 0; Index < HIR->expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression =
				HIR->expressions[Index];
			if (Expression.kind ==
					asETypedSemanticExpressionKind::Conversion
				&& Expression.type.GetTokenType() == ttFloat32
				&& Expression.operands.GetLength() == 1
				&& Expression.operands[0].value < HIR->expressions.GetLength()
				&& HIR->expressions[Expression.operands[0].value]
					.type.GetTokenType() == ttFloat64)
			{
				bSawNarrowingConversion = true;
			}
		}
		ASSERT_THAT(IsTrue(bSawNarrowingConversion));

		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("ASJIT_NarrowFloat64");
		Shape.ParameterTypes.Add(ScriptFunction->parameterTypes[0]);
		Shape.ReturnType = ScriptFunction->returnType;
		Shape.InvocationKind = HIR->header.invocationKind;
		Shape.ReceiverKind = HIR->header.receiver.kind;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(*HIR, Shape, {});
		if (!Emission.bSuccess)
		{
			const asCString Dump = DumpTypedSemanticFunction(*HIR);
			TestRunner->AddError(FString::Printf(
				TEXT("Float64-to-float32 emission failed: %s\n%s"),
				*Emission.FailureDetail,
				UTF8_TO_TCHAR(Dump.AddressOf())));
			return;
		}

		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("static_cast<float>(as_sem_s0)"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(Float64ToFloat32MatchesTheMaintainedVmEdgeDomain)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		TUniquePtr<FScopedNativeModule> Module;
		asCScriptFunction* const ScriptFunction =
			BuildFixture(*TestRunner, Engine, Module);
		ASSERT_THAT(IsNotNull(ScriptFunction));
		if (ScriptFunction == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		const double Values[] = {
			0.0,
			-0.0,
			std::numeric_limits<double>::denorm_min(),
			static_cast<double>(std::numeric_limits<float>::denorm_min()),
			static_cast<double>(std::numeric_limits<float>::max()),
			std::numeric_limits<double>::max(),
			std::numeric_limits<double>::infinity(),
			-std::numeric_limits<double>::infinity(),
			std::numeric_limits<double>::quiet_NaN(),
		};
		for (const double Value : Values)
		{
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->Prepare(ScriptFunction)));
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->SetArgDouble(0, Value)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,
				Context->Execute()));
			const float VmResult = Context->GetReturnFloat();
			const float NativeResult = static_cast<float>(Value);
			if (std::isnan(NativeResult))
			{
				ASSERT_THAT(IsTrue(std::isnan(VmResult)));
			}
			else
			{
				ASSERT_THAT(AreEqual(
					BitCast<uint32>(NativeResult),
					BitCast<uint32>(VmResult)));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
