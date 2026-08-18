#include "CQTest.h"

#include "../../../AngelScriptSDK/Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"

#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITScalarConversionTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.ScalarConversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const asSTypedSemanticExpression* FindConversion(
		const asCTypedSemanticFunction& Function)
	{
		for (asUINT Index = 0; Index < Function.expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression =
				Function.expressions[Index];
			if (Expression.kind ==
				asETypedSemanticExpressionKind::Conversion)
			{
				return &Expression;
			}
		}
		return nullptr;
	}

	static void AssertConversionEmission(
		FAutomationTestBase& Test,
		asCModule& Module,
		const char* const FunctionName,
		const TCHAR* const Symbol,
		const TCHAR* const ExpectedCpp)
	{
		asCScriptFunction* const ScriptFunction =
			static_cast<asCScriptFunction*>(
				Module.GetFunctionByName(FunctionName));
		if (!Test.TestNotNull(TEXT("Conversion fixture function must exist"),
			ScriptFunction))
		{
			return;
		}

		const asCTypedSemanticFunction* const HIR =
			ScriptFunction->GetTypedSemanticFunction();
		if (!Test.TestNotNull(TEXT("Conversion fixture must publish verified HIR"),
			HIR))
		{
			return;
		}

		const asSTypedSemanticExpression* const Conversion =
			FindConversion(*HIR);
		if (!Test.TestNotNull(TEXT("Conversion fixture must retain its conversion"),
			Conversion))
		{
			return;
		}

		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = Symbol;
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
			Test.AddError(FString::Printf(
				TEXT("%s conversion emission failed: %s\n%s"),
				UTF8_TO_TCHAR(FunctionName),
				*Emission.FailureDetail,
				UTF8_TO_TCHAR(Dump.AddressOf())));
			return;
		}

		Test.TestTrue(TEXT("Integer conversion must use the reviewed C++ form"),
			Emission.Definition.Contains(
				ExpectedCpp, ESearchCase::CaseSensitive));
		Test.TestFalse(TEXT("Pure integer conversion must stay context-free"),
			Emission.Definition.Contains(
				TEXT("FAngelscriptJITExecutionContext"),
				ESearchCase::CaseSensitive));
	}

public:
	TEST_METHOD(IntegerConversionPrimitiveMatchesVmBitDomainRules)
	{
		using namespace AngelscriptTypedASTJIT;

		ASSERT_THAT(AreEqual(int8{127},
			ConvertInteger<int8>(int32{-129})));
		ASSERT_THAT(AreEqual(uint8{255},
			ConvertInteger<uint8>(int32{-1})));
		ASSERT_THAT(AreEqual(int32{-1},
			ConvertInteger<int32>(MAX_uint32)));
		ASSERT_THAT(AreEqual(int64{4294967295},
			ConvertInteger<int64>(MAX_uint32)));
		ASSERT_THAT(AreEqual(MAX_uint64,
			ConvertInteger<uint64>(int32{-1})));
		ASSERT_THAT(AreEqual(int8{-1},
			ConvertInteger<int8>(uint64{0x1ff})));
	}

	TEST_METHOD(CompilerIntegralConversionsUseVmDefinedBitDomainCpp)
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
			int8 NarrowSigned(int Value)
			{
				return int8(Value);
			}

			uint8 NarrowUnsigned(int Value)
			{
				return uint8(Value);
			}

			uint SignedToUnsigned(int Value)
			{
				return uint(Value);
			}

			int UnsignedToSigned(uint Value)
			{
				return int(Value);
			}

			uint64 WidenSignedToUnsigned(int Value)
			{
				return uint64(Value);
			}

			int64 WidenUnsignedToSigned(uint Value)
			{
				return int64(Value);
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedASTJITScalarConversions",
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

		AssertConversionEmission(
			*TestRunner,
			*ScriptModule,
			"NarrowSigned",
			TEXT("ASJIT_NarrowSigned"),
			TEXT("AngelscriptTypedASTJIT::WrapNarrow<int8>(as_sem_s0)"));
		AssertConversionEmission(
			*TestRunner,
			*ScriptModule,
			"NarrowUnsigned",
			TEXT("ASJIT_NarrowUnsigned"),
			TEXT("AngelscriptTypedASTJIT::WrapNarrow<uint8>(as_sem_s0)"));
		AssertConversionEmission(
			*TestRunner,
			*ScriptModule,
			"SignedToUnsigned",
			TEXT("ASJIT_SignedToUnsigned"),
			TEXT("AngelscriptTypedASTJIT::ConvertInteger<uint32>(as_sem_s0)"));
		AssertConversionEmission(
			*TestRunner,
			*ScriptModule,
			"UnsignedToSigned",
			TEXT("ASJIT_UnsignedToSigned"),
			TEXT("AngelscriptTypedASTJIT::ConvertInteger<int32>(as_sem_s0)"));
		AssertConversionEmission(
			*TestRunner,
			*ScriptModule,
			"WidenSignedToUnsigned",
			TEXT("ASJIT_WidenSignedToUnsigned"),
			TEXT("AngelscriptTypedASTJIT::ConvertInteger<uint64>(as_sem_s0)"));
		AssertConversionEmission(
			*TestRunner,
			*ScriptModule,
			"WidenUnsignedToSigned",
			TEXT("ASJIT_WidenUnsignedToSigned"),
			TEXT("AngelscriptTypedASTJIT::ConvertInteger<int64>(as_sem_s0)"));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
