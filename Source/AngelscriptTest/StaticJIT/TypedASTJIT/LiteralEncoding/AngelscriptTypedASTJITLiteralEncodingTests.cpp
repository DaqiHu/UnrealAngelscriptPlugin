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

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITLiteralEncodingTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.LiteralEncoding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const asSTypedSemanticExpression* FindLiteral(
		const asCTypedSemanticFunction& Function)
	{
		for (asUINT Index = 0; Index < Function.expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression =
				Function.expressions[Index];
			if (Expression.kind == asETypedSemanticExpressionKind::Literal)
			{
				return &Expression;
			}
		}
		return nullptr;
	}

	static void AssertLiteralEmission(
		FAutomationTestBase& Test,
		asCModule& Module,
		const char* const FunctionName,
		const TCHAR* const Symbol,
		const char* const SourceSpelling,
		const asQWORD ExpectedBits,
		const TCHAR* const ExpectedCpp)
	{
		asCScriptFunction* const ScriptFunction =
			static_cast<asCScriptFunction*>(
				Module.GetFunctionByName(FunctionName));
		Test.TestNotNull(TEXT("Literal fixture function must exist"),
			ScriptFunction);
		if (ScriptFunction == nullptr)
		{
			return;
		}

		const asCTypedSemanticFunction* const HIR =
			ScriptFunction->GetTypedSemanticFunction();
		Test.TestNotNull(TEXT("Literal fixture must publish verified HIR"), HIR);
		if (HIR == nullptr)
		{
			return;
		}

		const asSTypedSemanticExpression* const Literal = FindLiteral(*HIR);
		Test.TestNotNull(TEXT("Literal fixture must retain its source literal"),
			Literal);
		if (Literal == nullptr)
		{
			return;
		}
		Test.TestEqual(TEXT("Literal diagnostic spelling must be retained"),
			FString(UTF8_TO_TCHAR(Literal->literal.AddressOf())),
			FString(UTF8_TO_TCHAR(SourceSpelling)));
		Test.TestEqual(TEXT("Literal must retain exact compiler value bits"),
			Literal->literalBits, ExpectedBits);

		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = Symbol;
		Shape.ReturnType = ScriptFunction->returnType;
		Shape.InvocationKind = HIR->header.invocationKind;
		Shape.ReceiverKind = HIR->header.receiver.kind;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(*HIR, Shape, {});
		if (!Emission.bSuccess)
		{
			const asCString Dump = DumpTypedSemanticFunction(*HIR);
			Test.AddError(FString::Printf(
				TEXT("%s literal emission failed: %s\n%s"),
				UTF8_TO_TCHAR(FunctionName),
				*Emission.FailureDetail,
				UTF8_TO_TCHAR(Dump.AddressOf())));
			return;
		}
		Test.TestTrue(TEXT("Reviewed scalar literal must emit"), true);
		if (Emission.bSuccess)
		{
			Test.TestTrue(TEXT("Generated literal must use exact canonical bits"),
				Emission.Definition.Contains(
					ExpectedCpp, ESearchCase::CaseSensitive));
			Test.TestFalse(TEXT("Generated literal must not require the generic context"),
				Emission.Definition.Contains(
					TEXT("FAngelscriptJITExecutionContext"),
					ESearchCase::CaseSensitive));
		}
	}

public:
	TEST_METHOD(CompilerBitsDriveExactBoolFloatDoubleAndEnumCpp)
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

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedASTJITLiteralEncoding",
			ASTEST_AS_ANSI(R"AS(
				bool BoolLiteral()
				{
					return true;
				}

				float FloatLiteral()
				{
					return 1.5f;
				}

				double DoubleLiteral()
				{
					return 2.25;
				}

				enum ELiteralMode
				{
					Sparse = 100
				}

				ELiteralMode EnumLiteral()
				{
					return ELiteralMode::Sparse;
				}
			)AS"));
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

		AssertLiteralEmission(
			*TestRunner,
			*ScriptModule,
			"BoolLiteral",
			TEXT("ASJIT_BoolLiteral"),
			"true",
			asQWORD{1},
			TEXT("AngelscriptTypedASTJIT::FromCanonicalBits<bool>(UINT64_C(1))"));
		AssertLiteralEmission(
			*TestRunner,
			*ScriptModule,
			"FloatLiteral",
			TEXT("ASJIT_FloatLiteral"),
			"1.5f",
			static_cast<asQWORD>(BitCast<uint32>(1.5f)),
			TEXT("AngelscriptTypedASTJIT::FromCanonicalBits<float>(UINT64_C(1069547520))"));
		AssertLiteralEmission(
			*TestRunner,
			*ScriptModule,
			"DoubleLiteral",
			TEXT("ASJIT_DoubleLiteral"),
			"2.25",
			BitCast<uint64>(2.25),
			TEXT("AngelscriptTypedASTJIT::FromCanonicalBits<double>(UINT64_C(4612248968380809216))"));
		AssertLiteralEmission(
			*TestRunner,
			*ScriptModule,
			"EnumLiteral",
			TEXT("ASJIT_EnumLiteral"),
			"100",
			asQWORD{100},
			TEXT("AngelscriptTypedASTJIT::FromCanonicalBits<int8>(UINT64_C(100))"));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
