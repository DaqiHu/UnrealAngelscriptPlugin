#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeTypedSemanticIRCallMetadataTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRHiddenArgumentTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.CallMetadata.HiddenArgument",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(HiddenDefaultRemainsAHostOriginInFinalFormalOrder)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIRCallMetadataTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-HIDDEN-ARGUMENT",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		HiddenInvocationCount = 0;
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
		const FRegisteredCallMetadataFunctions Registered =
			RegisterCallMetadataFunctions(*ScriptEngine, false);
		ASSERT_THAT(IsTrue(Registered.HasHiddenFixture()));
		if (!Registered.HasHiddenFixture())
		{
			return;
		}

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedSemanticIRHiddenArgument",
			ASTEST_AS_ANSI(R"AS(
				int HiddenArgumentEntry(int Value)
				{
					return MetadataHidden(Value);
				}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid()));
		if (!Module.IsValid())
		{
			return;
		}

		asCScriptFunction* const Entry = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				Module, "int HiddenArgumentEntry(int Value)"));
		const asCTypedSemanticFunction* const HIR = Entry != nullptr
			? Entry->GetTypedSemanticFunction() : nullptr;
		ASSERT_THAT(IsNotNull(HIR));
		if (HIR == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
		const asSTypedSemanticExpression* const Call =
			FindResolvedCall(*HIR, Registered.Hidden);
		ASSERT_THAT(IsNotNull(Call));
		if (Call != nullptr)
		{
			ASSERT_THAT(AreEqual(2,
				static_cast<int32>(Call->operands.GetLength()),
				TEXT("The hidden value remains part of effective formal ABI order")));
			ASSERT_THAT(AreEqual(2,
				static_cast<int32>(Call->formalParameterIndices.GetLength())));
			ASSERT_THAT(AreEqual(2,
				static_cast<int32>(Call->evaluationSequence.GetLength())));
			ASSERT_THAT(AreEqual(0, Call->formalParameterIndices[0]));
			ASSERT_THAT(AreEqual(1, Call->formalParameterIndices[1]));
			ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
				Call->evaluationSequence[0]));
			ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
				Call->evaluationSequence[1]));
			ASSERT_THAT(AreEqual(2,
				static_cast<int32>(Call->argumentOrigins.GetLength())));
			ASSERT_THAT(IsTrue(Call->argumentOrigins[0]
				== asETypedSemanticArgumentOrigin::SourcePositional));
			ASSERT_THAT(IsTrue(Call->argumentOrigins[1]
				== asETypedSemanticArgumentOrigin::HostHidden));
			ASSERT_THAT(AreEqual(2,
				static_cast<int32>(Call->argumentProvenance.GetLength())));
			const asSTypedSemanticArgumentProvenance& HiddenOrigin =
				Call->argumentProvenance[1];
			ASSERT_THAT(IsTrue(HiddenOrigin.origin
				== asETypedSemanticArgumentOrigin::HostHidden));
			ASSERT_THAT(AreEqual(-1, HiddenOrigin.sourceArgumentOrdinal));
			ASSERT_THAT(AreEqual(Registered.Hidden,
				HiddenOrigin.calleeFunctionId));
			ASSERT_THAT(AreEqual(1, HiddenOrigin.calleeParameterIndex));
			ASSERT_THAT(IsTrue(HiddenOrigin.calleeParameterName == "Hidden"));
			ASSERT_THAT(IsTrue(HiddenOrigin.canonicalExpression == "4"));
			ASSERT_THAT(IsTrue(
				HiddenOrigin.processedSpan.section == Call->span.section
				&& HiddenOrigin.processedSpan.sourceOffset
					== Call->span.sourceOffset
				&& HiddenOrigin.processedSpan.sourceLength
					== Call->span.sourceLength));
			ASSERT_THAT(AreEqual(1, Call->hiddenArgumentIndex));
		}

		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("origins=[SourcePositional,HostHidden]")),
			TEXT("The host-inserted default must not masquerade as an AS-visible argument or receiver")));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("origin=HostHidden"))));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("canonical=4"))));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("hiddenArgument=1"))));

		int32 ReturnValue = 0;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteGlobalIntWithInt(
				*ScriptEngine, *Entry, 2, ReturnValue)));
		ASSERT_THAT(AreEqual(24, ReturnValue));
		ASSERT_THAT(AreEqual(1, HiddenInvocationCount));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
