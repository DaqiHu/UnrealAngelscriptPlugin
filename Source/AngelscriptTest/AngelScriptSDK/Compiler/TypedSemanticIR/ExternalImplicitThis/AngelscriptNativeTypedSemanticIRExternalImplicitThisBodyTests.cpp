#include "../../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeTypedSemanticIRExternalImplicitThisTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRExternalImplicitThisBodyTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.ExternalImplicitThis.Body",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ValidBodyRetainsParameterAliasAndExplicitMemberReceivers)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIRExternalImplicitThisTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-EXTERNAL-IMPLICIT-THIS-BODY",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FExternalReceiver
			{
				int RawValue = 0;
				int OtherValue = 0;
				int BackingValue = 0;

				int GetPropertyValue() const
				{
					return BackingValue;
				}

				int MethodValue() const
				{
					return RawValue + OtherValue;
				}
			}

			int EvaluateExternal(
				FExternalReceiver& Self,
				int RawValue,
				int LocalSeed) external_implicit_this
			{
				int LocalShadow = LocalSeed + 1;
				return this.RawValue
					+ OtherValue
					+ PropertyValue
					+ MethodValue()
					+ RawValue
					+ LocalShadow;
			}

			int RunExternal()
			{
				FExternalReceiver Receiver;
				Receiver.RawValue = 11;
				Receiver.OtherValue = 13;
				Receiver.BackingValue = 17;
				return EvaluateExternal(Receiver, 19, 23);
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
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			CaptureOffEngine.Get()->SetEngineProperty(
				asEP_PROPERTY_ACCESSOR_MODE,
				2),
			TEXT("The capture-off fixture must enable the maintained explicit GetX accessor mode")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			CaptureOnEngine.Get()->SetEngineProperty(
				asEP_PROPERTY_ACCESSOR_MODE,
				2),
			TEXT("The capture-on fixture must enable the maintained explicit GetX accessor mode")));

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
			"TypedSemanticIRExternalImplicitThisOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRExternalImplicitThisOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid()));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid()));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		const char* const ExternalDeclaration =
			"int EvaluateExternal(FExternalReceiver&inout Self, const int RawValue, const int LocalSeed)";
		asCScriptFunction* const CaptureOffExternal =
			static_cast<asCScriptFunction*>(GetNativeFunctionByExactDecl(
				CaptureOffModule,
				ExternalDeclaration));
		asCScriptFunction* const CaptureOnExternal =
			static_cast<asCScriptFunction*>(GetNativeFunctionByExactDecl(
				CaptureOnModule,
				ExternalDeclaration));
		asIScriptFunction* const CaptureOffRun = GetNativeFunctionByExactDecl(
			CaptureOffModule,
			"int RunExternal()");
		asIScriptFunction* const CaptureOnRun = GetNativeFunctionByExactDecl(
			CaptureOnModule,
			"int RunExternal()");
		ASSERT_THAT(IsNotNull(CaptureOffExternal));
		ASSERT_THAT(IsNotNull(CaptureOnExternal));
		ASSERT_THAT(IsNotNull(CaptureOffRun));
		ASSERT_THAT(IsNotNull(CaptureOnRun));
		if (CaptureOffExternal == nullptr || CaptureOnExternal == nullptr
			|| CaptureOffRun == nullptr || CaptureOnRun == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(asUINT{3}, CaptureOnExternal->GetParamCount(),
			TEXT("External implicit this must retain parameter zero in its public ABI")));
		ASSERT_THAT(IsNull(CaptureOnExternal->GetObjectType(),
			TEXT("External implicit this remains a global function")));
		ASSERT_THAT(IsTrue(CaptureOnExternal->traits.GetTrait(
			asTRAIT_EXTERNAL_IMPLICIT_THIS)));
		ASSERT_THAT(AreEqual(
			CopyNormalizedBytecode(CaptureOffExternal),
			CopyNormalizedBytecode(CaptureOnExternal),
			TEXT("Receiver capture must not alter the authoritative VM body after normalizing only Engine-local pointer operands")));

		const FExecutionObservation CaptureOffExecution = ExecuteGlobalInt(
			CaptureOffEngine.Get(),
			CaptureOffRun);
		const FExecutionObservation CaptureOnExecution = ExecuteGlobalInt(
			CaptureOnEngine.Get(),
			CaptureOnRun);
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, CaptureOffExecution.State));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, CaptureOnExecution.State));
		ASSERT_THAT(AreEqual(108, CaptureOffExecution.ReturnValue));
		ASSERT_THAT(AreEqual(
			CaptureOffExecution.ReturnValue,
			CaptureOnExecution.ReturnValue,
			TEXT("HIR receiver metadata must not change VM name lookup or calls")));

		ASSERT_THAT(IsNull(CaptureOffExternal->GetTypedSemanticFunction()));
		const asCTypedSemanticFunction* const HIR =
			CaptureOnExternal->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR,
			TEXT("A valid external receiver body must publish verified HIR")));
		if (HIR == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			asETypedSemanticInvocationKind::Global,
			HIR->header.invocationKind));
		ASSERT_THAT(AreEqual(
			asETypedSemanticReceiverKind::ExternalImplicitThis,
			HIR->header.receiver.kind));
		ASSERT_THAT(AreEqual(0, HIR->header.receiver.parameterIndex));
		const asSTypedSemanticSymbol* const ReceiverParameter = FindSymbol(
			*HIR,
			asETypedSemanticSymbolKind::Parameter,
			"Self");
		const asSTypedSemanticSymbol* const ShadowingParameter = FindSymbol(
			*HIR,
			asETypedSemanticSymbolKind::Parameter,
			"RawValue");
		const asSTypedSemanticSymbol* const LocalShadow = FindSymbol(
			*HIR,
			asETypedSemanticSymbolKind::Local,
			"LocalShadow");
		ASSERT_THAT(IsNotNull(ReceiverParameter));
		ASSERT_THAT(IsNotNull(ShadowingParameter));
		ASSERT_THAT(IsNotNull(LocalShadow));
		if (ReceiverParameter == nullptr || ShadowingParameter == nullptr
			|| LocalShadow == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			ReceiverParameter->id.value,
			HIR->header.receiver.symbol.value,
			TEXT("The effective receiver must alias the exact declared parameter")));

		asITypeInfo* const ReceiverType =
			CaptureOnModule->GetTypeInfoByDecl("FExternalReceiver");
		asCScriptFunction* const Getter = static_cast<asCScriptFunction*>(
			GetNativeMethodByDecl(
				ReceiverType,
				"int GetPropertyValue() const"));
		asCScriptFunction* const Method = static_cast<asCScriptFunction*>(
			GetNativeMethodByDecl(ReceiverType, "int MethodValue() const"));
		ASSERT_THAT(IsNotNull(Getter));
		ASSERT_THAT(IsNotNull(Method));
		if (Getter == nullptr || Method == nullptr)
		{
			return;
		}

		const asSTypedSemanticExpression* const GetterCall =
			FindResolvedCall(*HIR, Getter->id);
		const asSTypedSemanticExpression* const MethodCall =
			FindResolvedCall(*HIR, Method->id);
		ASSERT_THAT(IsNotNull(GetterCall,
			TEXT("The unqualified property accessor must retain its resolved target")));
		ASSERT_THAT(IsNotNull(MethodCall,
			TEXT("The unqualified method must retain its resolved target")));
		if (GetterCall != nullptr)
		{
			ASSERT_THAT(IsTrue(ExpressionAliasesReceiver(
				*HIR,
				GetterCall->receiverExpression,
				ReceiverParameter->id),
				TEXT("The accessor call must retain parameter zero as receiver")));
		}
		if (MethodCall != nullptr)
		{
			ASSERT_THAT(IsTrue(ExpressionAliasesReceiver(
				*HIR,
				MethodCall->receiverExpression,
				ReceiverParameter->id),
				TEXT("The method call must retain parameter zero as receiver")));
		}

		asUINT ExplicitFieldCount = 0;
		asUINT ImplicitFieldCount = 0;
		asUINT ShadowParameterUseCount = 0;
		asUINT LocalUseCount = 0;
		for (asUINT Index = 0; Index < HIR->expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression = HIR->expressions[Index];
			if (Expression.kind == asETypedSemanticExpressionKind::Symbol)
			{
				ShadowParameterUseCount += Expression.symbol == ShadowingParameter->id;
				LocalUseCount += Expression.symbol == LocalShadow->id;
				continue;
			}
			if (Expression.kind != asETypedSemanticExpressionKind::Unsupported
				|| Expression.unsupportedKind
					!= asETypedSemanticUnsupportedKind::PropertyAccess)
			{
				continue;
			}

			if (Expression.resolvedTarget == "FExternalReceiver::RawValue")
			{
				++ExplicitFieldCount;
			}
			else if (Expression.resolvedTarget == "FExternalReceiver::OtherValue")
			{
				++ImplicitFieldCount;
			}
			else
			{
				continue;
			}
			ASSERT_THAT(IsTrue(Expression.operands.GetLength() == 1
				&& ExpressionAliasesReceiver(
					*HIR,
					Expression.operands[0],
					ReceiverParameter->id)));
		}

		ASSERT_THAT(AreEqual(asUINT{1}, ExplicitFieldCount));
		ASSERT_THAT(AreEqual(asUINT{1}, ImplicitFieldCount));
		ASSERT_THAT(IsTrue(ShadowParameterUseCount > 0,
			TEXT("Normal parameter lookup must win over the same-named receiver field")));
		ASSERT_THAT(IsTrue(LocalUseCount > 0,
			TEXT("Local lookup must remain a normal symbol access")));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
