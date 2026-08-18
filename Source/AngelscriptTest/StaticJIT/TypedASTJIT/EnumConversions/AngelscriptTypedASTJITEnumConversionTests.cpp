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

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITEnumConversionTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.EnumConversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ExplicitIntegerToEnumConstructionRemainsAScalarConversion)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScriptEngine->SetTypedSemanticIRCapture(true)));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			enum EMode
			{
				Minimum = -128,
				Zero = 0,
				One = 1,
				Maximum = 127,
			}

			int ConvertAndSwitch(int RawMode)
			{
				EMode Mode = EMode(RawMode);
				switch (Mode)
				{
				case EMode::Zero:
					return 10;
				case EMode::One:
					return 20;
				case EMode::Minimum:
					return 30;
				case EMode::Maximum:
					return 40;
				}
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedASTJITEnumConversions",
			ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid()));
		if (!Module.IsValid())
		{
			return;
		}

		asCModule* const ScriptModule = static_cast<asCModule*>(Module.Get());
		ASSERT_THAT(IsNotNull(ScriptModule));
		if (ScriptModule == nullptr)
		{
			return;
		}
		asCScriptFunction* const ScriptFunction =
			static_cast<asCScriptFunction*>(
				ScriptModule->GetFunctionByName("ConvertAndSwitch"));
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

		bool bSawIntegerToEnumConversion = false;
		bool bSawUnsupportedConstruction = false;
		for (asUINT Index = 0; Index < HIR->expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression =
				HIR->expressions[Index];
			if (Expression.kind ==
					asETypedSemanticExpressionKind::Conversion
				&& Expression.type.IsEnumType()
				&& Expression.operands.GetLength() == 1
				&& Expression.operands[0].value < HIR->expressions.GetLength()
				&& !HIR->expressions[
					Expression.operands[0].value].type.IsEnumType())
			{
				bSawIntegerToEnumConversion = true;
			}
			if (Expression.kind ==
					asETypedSemanticExpressionKind::Unsupported
				&& Expression.unsupportedKind ==
					asETypedSemanticUnsupportedKind::ConstructionOrLifetime)
			{
				bSawUnsupportedConstruction = true;
			}
		}

		ASSERT_THAT(IsTrue(bSawIntegerToEnumConversion));
		ASSERT_THAT(IsFalse(bSawUnsupportedConstruction));

		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("ASJIT_ConvertAndSwitch");
		for (asUINT ParameterIndex = 0;
			ParameterIndex < ScriptFunction->parameterTypes.GetLength();
			++ParameterIndex)
		{
			Shape.ParameterTypes.Add(
				ScriptFunction->parameterTypes[ParameterIndex]);
		}
		Shape.ReturnType = ScriptFunction->returnType;
		Shape.InvocationKind = HIR->header.invocationKind;
		Shape.ReceiverKind = HIR->header.receiver.kind;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(*HIR, Shape, {});
		if (!Emission.bSuccess)
		{
			const asCString Dump = DumpTypedSemanticFunction(*HIR);
			TestRunner->AddError(FString::Printf(
				TEXT("Enum conversion emission failed: %s\n%s"),
				*Emission.FailureDetail,
				UTF8_TO_TCHAR(Dump.AddressOf())));
			return;
		}

		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("switch ("), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("AngelscriptTypedASTJIT::WrapNarrow<int8>"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("case int32(-128):"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("case int32(127):"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("default:"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("SetSwitchValueInvalidException"),
			ESearchCase::CaseSensitive)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
