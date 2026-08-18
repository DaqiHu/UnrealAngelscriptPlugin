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

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRUnsupportedLifetimeTests,
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
	TEST_METHOD(ManagedConstructionAndCleanupRemainExplicitFallbacks)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-UNSUPPORTED-LIFETIME",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class FManagedLifetime
			{
				int Value;
			}

			int ConstructManagedLifetime()
			{
				FManagedLifetime Value = FManagedLifetime();
				return 31;
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
			"TypedSemanticIRUnsupportedLifetimeOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRUnsupportedLifetimeOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid(),
			TEXT("Managed construction must remain valid with capture off")));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid(),
			TEXT("Managed construction must remain valid with capture on")));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asCScriptFunction* const CaptureOffFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOffModule,
				"int ConstructManagedLifetime()"));
		asCScriptFunction* const CaptureOnFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOnModule,
				"int ConstructManagedLifetime()"));
		ASSERT_THAT(IsNotNull(CaptureOffFunction));
		ASSERT_THAT(IsNotNull(CaptureOnFunction));
		if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
		{
			return;
		}

		asUINT CaptureOffBytecodeLength = 0;
		asUINT CaptureOnBytecodeLength = 0;
		ASSERT_THAT(IsNotNull(CaptureOffFunction->GetByteCode(
			&CaptureOffBytecodeLength)));
		ASSERT_THAT(IsNotNull(CaptureOnFunction->GetByteCode(
			&CaptureOnBytecodeLength)));
		ASSERT_THAT(IsTrue(CaptureOffBytecodeLength > 0));
		ASSERT_THAT(AreEqual(CaptureOffBytecodeLength,
			CaptureOnBytecodeLength,
			TEXT("HIR capture must not change managed-lifetime bytecode length")));
		ASSERT_THAT(IsNull(CaptureOffFunction->GetTypedSemanticFunction()));

		int32 CaptureOffResult = 0;
		int32 CaptureOnResult = 0;
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOffEngine.Get(), CaptureOffFunction, CaptureOffResult)));
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOnEngine.Get(), CaptureOnFunction, CaptureOnResult)));
		ASSERT_THAT(AreEqual(31, CaptureOffResult));
		ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult,
			TEXT("Fallback metadata must not alter construction or cleanup behavior")));

		const asCTypedSemanticFunction* const HIR =
			CaptureOnFunction->GetTypedSemanticFunction();
		const FString CaptureDiagnostic = UTF8_TO_TCHAR(
			CaptureOnFunction->GetTypedSemanticCaptureDiagnostic().AddressOf());
		ASSERT_THAT(IsNotNull(HIR,
			*FString::Printf(
				TEXT("Managed construction should publish inspectable fallback HIR; diagnostic=%s"),
				*CaptureDiagnostic)));
		if (HIR == nullptr)
		{
			return;
		}

		const asSTypedSemanticExpression* ConstructionMarker = nullptr;
		const asSTypedSemanticExpression* CleanupMarker = nullptr;
		for (asUINT ExpressionIndex = 0;
			ExpressionIndex < HIR->expressions.GetLength();
			++ExpressionIndex)
		{
			const asSTypedSemanticExpression& Expression =
				HIR->expressions[ExpressionIndex];
			if (Expression.kind != asETypedSemanticExpressionKind::Unsupported)
			{
				continue;
			}
			if (Expression.unsupportedKind
				== asETypedSemanticUnsupportedKind::ConstructionOrLifetime)
			{
				ASSERT_THAT(IsNull(ConstructionMarker));
				ConstructionMarker = &Expression;
			}
			else if (Expression.unsupportedKind
				== asETypedSemanticUnsupportedKind::ExceptionCleanup)
			{
				ASSERT_THAT(IsNull(CleanupMarker));
				CleanupMarker = &Expression;
			}
		}
		ASSERT_THAT(IsNotNull(ConstructionMarker,
			TEXT("The constructor expression must remain an explicit lifetime fallback")));
		ASSERT_THAT(IsNotNull(CleanupMarker,
			TEXT("Owned managed storage must retain an explicit cleanup fallback")));
		if (ConstructionMarker == nullptr || CleanupMarker == nullptr)
		{
			return;
		}

		const asSTypedSemanticStatement* LocalDeclaration = nullptr;
		for (asUINT StatementIndex = 0;
			StatementIndex < HIR->statements.GetLength();
			++StatementIndex)
		{
			if (HIR->statements[StatementIndex].kind
				== asETypedSemanticStatementKind::LocalDeclaration)
			{
				ASSERT_THAT(IsNull(LocalDeclaration));
				LocalDeclaration = &HIR->statements[StatementIndex];
			}
		}
		ASSERT_THAT(IsNotNull(LocalDeclaration));
		if (LocalDeclaration == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(ConstructionMarker->id,
			LocalDeclaration->initializerExpression,
			TEXT("The local declaration must own the constructor fallback expression")));
		ASSERT_THAT(IsTrue(
			FString(UTF8_TO_TCHAR(
				ConstructionMarker->resolvedTarget.AddressOf())).Contains(
				TEXT("FManagedLifetime")),
			TEXT("The marker must preserve pointer-free constructed-type detail")));
		ASSERT_THAT(AreEqual(
			asETypedSemanticCleanupPlanState::NonEmpty,
			HIR->header.cleanupPlanState,
			TEXT("Owned managed storage must publish an explicit non-empty cleanup plan")));
		ASSERT_THAT(IsTrue(HIR->header.hasExceptionCleanup,
			TEXT("Owned managed storage must make exception cleanup visible")));
		ASSERT_THAT(AreEqual(ttVoid, CleanupMarker->type.GetTokenType()));
		ASSERT_THAT(IsTrue(CleanupMarker->span.sourceLength > 0));

		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("unsupported=ConstructionOrLifetime"))));
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("unsupported=ExceptionCleanup"))));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
