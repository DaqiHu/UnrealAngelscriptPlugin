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

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRUnsupportedExceptionRegionTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	// Reserved only by the maintained-fork compiler metadata seam. A non-null
	// value injects one already-finalized internal try/catch record so capture
	// can prove its future-backport disposition without enabling source syntax.
	static constexpr asPWORD ExceptionRegionMetadataUserDataSlot =
		static_cast<asPWORD>(0x5453495245584350ull);

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

public:
	TEST_METHOD(CompilerExceptionRegionMetadataPublishesPreciseFallbackHIR)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-EXCEPTION-REGION",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int ExceptionRegionFallback()
			{
				return 42;
			}
			)AS");

		FNativeTestEngine CaptureOffEngine;
		FNativeTestEngine ExceptionRegionEngine;
		CaptureOffEngine.Create(*TestRunner);
		ExceptionRegionEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			if (ExceptionRegionEngine.Get() != nullptr)
			{
				ExceptionRegionEngine.Get()->SetUserData(
					nullptr,
					ExceptionRegionMetadataUserDataSlot);
			}
			ExceptionRegionEngine.Destroy();
			CaptureOffEngine.Destroy();
		};

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(ExceptionRegionEngine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScriptEngine->SetTypedSemanticIRCapture(true)));
		uint8 MetadataSentinel = 1;
		ASSERT_THAT(IsNull(ScriptEngine->SetUserData(
			&MetadataSentinel,
			ExceptionRegionMetadataUserDataSlot)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRExceptionRegionCaptureOff",
			ScriptSource);
		FScopedNativeModule ExceptionRegionModule(
			*TestRunner,
			ExceptionRegionEngine,
			"TypedSemanticIRExceptionRegionCaptureOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid()));
		ASSERT_THAT(IsTrue(ExceptionRegionModule.IsValid(),
			TEXT("Internal exception metadata must not reject otherwise valid bytecode")));
		if (!CaptureOffModule.IsValid() || !ExceptionRegionModule.IsValid())
		{
			return;
		}

		asCScriptFunction* const CaptureOffFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOffModule,
				"int ExceptionRegionFallback()"));
		asCScriptFunction* const ExceptionRegionFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				ExceptionRegionModule,
				"int ExceptionRegionFallback()"));
		ASSERT_THAT(IsNotNull(CaptureOffFunction));
		ASSERT_THAT(IsNotNull(ExceptionRegionFunction));
		if (CaptureOffFunction == nullptr || ExceptionRegionFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			CopyNativeBytecode(CaptureOffFunction),
			CopyNativeBytecode(ExceptionRegionFunction),
			TEXT("Exception-region capture metadata must not rewrite bytecode")));
		int32 CaptureOffResult = 0;
		int32 ExceptionRegionResult = 0;
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOffEngine.Get(),
			CaptureOffFunction,
			CaptureOffResult)));
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			ExceptionRegionEngine.Get(),
			ExceptionRegionFunction,
			ExceptionRegionResult)));
		ASSERT_THAT(AreEqual(42, CaptureOffResult));
		ASSERT_THAT(AreEqual(CaptureOffResult, ExceptionRegionResult));

		const asCTypedSemanticFunction* const HIR =
			ExceptionRegionFunction->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR,
			TEXT("Encountered internal exception metadata should publish inspectable fallback HIR")));
		if (HIR == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			asETypedSemanticCleanupPlanState::CompilerExceptionRegion,
			HIR->header.cleanupPlanState));
		ASSERT_THAT(IsTrue(HIR->header.hasExceptionCleanup));
		ASSERT_THAT(IsFalse(HIR->header.cleanupPlanCoversAllTransfers,
			TEXT("Unknown future handler cleanup must not claim universal coverage")));
		ASSERT_THAT(IsTrue(HIR->header.universalCleanupPlan.IsValid()));

		const asSTypedSemanticExpression* ExceptionRegionMarker = nullptr;
		for (asUINT Index = 0; Index < HIR->expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression = HIR->expressions[Index];
			if (Expression.kind == asETypedSemanticExpressionKind::Unsupported
				&& Expression.unsupportedKind
					== asETypedSemanticUnsupportedKind::ExceptionCleanup
				&& Expression.resolvedTarget == "compiler-exception-region")
			{
				ASSERT_THAT(IsNull(ExceptionRegionMarker));
				ExceptionRegionMarker = &Expression;
			}
		}
		ASSERT_THAT(IsNotNull(ExceptionRegionMarker,
			TEXT("Internal handler metadata needs a stable typed unsupported marker")));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("cleanupState=CompilerExceptionRegion"))));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("target=compiler-exception-region"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
