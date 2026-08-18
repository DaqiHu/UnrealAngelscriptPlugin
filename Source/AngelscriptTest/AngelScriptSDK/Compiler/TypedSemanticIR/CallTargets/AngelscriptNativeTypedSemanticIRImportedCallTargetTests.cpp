#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeTypedSemanticIRCallTargetTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRImportedCallTargetTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.CallTargets.Imported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(BindRebindAndUnbindRetainTheImportedSlotInsteadOfTheMutableBoundFunction)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIRCallTargetTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-IMPORTED-CALL-TARGET",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Isolation);

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

		const ANSICHAR* const ProviderAName =
			"TypedSemanticIRImportProviderA";
		const ANSICHAR* const ProviderBName =
			"TypedSemanticIRImportProviderB";
		const ANSICHAR* const ConsumerName =
			"TypedSemanticIRImportConsumer";
		asIScriptModule* ProviderA = nullptr;
		asIScriptModule* ProviderB = nullptr;
		asIScriptModule* Consumer = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(
			ScriptEngine,
			ProviderAName,
			"int ImportedValue() { return 11; }",
			ProviderA) >= 0));
		ASSERT_THAT(IsTrue(CompileNativeModule(
			ScriptEngine,
			ProviderBName,
			"int ImportedValue() { return 29; }",
			ProviderB) >= 0));
		ASSERT_THAT(IsTrue(CompileNativeModule(
			ScriptEngine,
			ConsumerName,
			"import int ImportedValue() from "
			"\"TypedSemanticIRImportProviderA\";\n"
			"int RunImported() { return ImportedValue(); }",
			Consumer) >= 0));
		ASSERT_THAT(IsNotNull(ProviderA));
		ASSERT_THAT(IsNotNull(ProviderB));
		ASSERT_THAT(IsNotNull(Consumer));
		if (ProviderA == nullptr || ProviderB == nullptr || Consumer == nullptr)
		{
			return;
		}

		asCScriptFunction* const FirstTarget =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				ProviderA,
				"int ImportedValue()"));
		asCScriptFunction* const SecondTarget =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				ProviderB,
				"int ImportedValue()"));
		asCScriptFunction* const Caller =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Consumer,
				"int RunImported()"));
		ASSERT_THAT(IsNotNull(FirstTarget));
		ASSERT_THAT(IsNotNull(SecondTarget));
		ASSERT_THAT(IsNotNull(Caller));
		if (FirstTarget == nullptr || SecondTarget == nullptr || Caller == nullptr)
		{
			return;
		}

		const asCTypedSemanticFunction* const HIR =
			Caller->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR));
		if (HIR == nullptr)
		{
			return;
		}
		const asSTypedSemanticExpression* const ImportedCall =
			FindResolvedCall(
				*HIR,
				asETypedSemanticCallTargetKind::ImportedFunction);
		ASSERT_THAT(IsNotNull(ImportedCall));
		if (ImportedCall == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue((ImportedCall->resolvedFunctionId & FUNC_IMPORTED) != 0));
		ASSERT_THAT(AreNotEqual(
			FirstTarget->id,
			ImportedCall->resolvedFunctionId));
		ASSERT_THAT(AreNotEqual(
			SecondTarget->id,
			ImportedCall->resolvedFunctionId));
		ASSERT_THAT(IsTrue(
			ImportedCall->targetProvenance.canonicalDeclaration
				== "int ImportedValue()"));
		ASSERT_THAT(IsTrue(
			ImportedCall->targetProvenance.declarationModule == ConsumerName));
		ASSERT_THAT(AreEqual(
			asETypedSemanticBodyAvailability::NoScriptBody,
			ImportedCall->targetProvenance.bodyAvailability));
		ASSERT_THAT(AreEqual(
			asETypedSemanticBodyOwnership::None,
			ImportedCall->targetProvenance.bodyOwnership));
		ASSERT_THAT(IsTrue(
			ImportedCall->targetProvenance.bodyOwnerModule == ""));
		ASSERT_THAT(IsTrue(
			ImportedCall->targetProvenance.importedSourceModule
				== ProviderAName));
		ASSERT_THAT(IsTrue(
			ImportedCall->targetProvenance.importedCanonicalSignature
				== "int ImportedValue()"));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));

		const int ImportedSlot =
			ImportedCall->resolvedFunctionId & ~FUNC_IMPORTED;
		asCModule* const InternalConsumer = static_cast<asCModule*>(Consumer);
		ASSERT_THAT(AreEqual(1,
			static_cast<int32>(InternalConsumer->bindInformations.GetLength())));
		if (InternalConsumer->bindInformations.GetLength() != 1)
		{
			return;
		}
		sBindInfo* const Binding = InternalConsumer->bindInformations[0];
		ASSERT_THAT(IsNotNull(Binding));
		ASSERT_THAT(IsTrue(ImportedSlot >= 0));
		ASSERT_THAT(IsTrue(
			static_cast<asUINT>(ImportedSlot)
				< ScriptEngine->importedFunctions.GetLength()));
		if (Binding == nullptr
			|| ImportedSlot < 0
			|| static_cast<asUINT>(ImportedSlot)
				>= ScriptEngine->importedFunctions.GetLength())
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ScriptEngine->importedFunctions[ImportedSlot] == Binding));
		ASSERT_THAT(AreEqual(-1, Binding->boundFunctionId));

		const asSTypedSemanticFunctionTargetProvenance StableProvenance =
			ImportedCall->targetProvenance;
		const FString StableDump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		ASSERT_THAT(IsTrue(StableDump.Contains(
			TEXT("importSource=TypedSemanticIRImportProviderA"))));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Consumer->BindImportedFunction(0, FirstTarget)));
		ASSERT_THAT(AreEqual(FirstTarget->id, Binding->boundFunctionId));
		int32 FirstValue = INDEX_NONE;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteIntNoArgs(*ScriptEngine, *Caller, FirstValue)));
		ASSERT_THAT(AreEqual(11, FirstValue));
		ASSERT_THAT(AreEqual(
			ImportedSlot | FUNC_IMPORTED,
			ImportedCall->resolvedFunctionId));
		ASSERT_THAT(IsTrue(AreTargetProvenancesEqual(
			StableProvenance,
			ImportedCall->targetProvenance)));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Consumer->BindImportedFunction(0, SecondTarget)));
		ASSERT_THAT(AreEqual(SecondTarget->id, Binding->boundFunctionId));
		int32 SecondValue = INDEX_NONE;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteIntNoArgs(*ScriptEngine, *Caller, SecondValue)));
		ASSERT_THAT(AreEqual(29, SecondValue));
		ASSERT_THAT(IsTrue(AreTargetProvenancesEqual(
			StableProvenance,
			ImportedCall->targetProvenance)));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Consumer->UnbindImportedFunction(0)));
		ASSERT_THAT(AreEqual(-1, Binding->boundFunctionId));
		int32 UnboundValue = INDEX_NONE;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_EXCEPTION),
			ExecuteIntNoArgs(*ScriptEngine, *Caller, UnboundValue)));
		ASSERT_THAT(IsTrue(AreTargetProvenancesEqual(
			StableProvenance,
			ImportedCall->targetProvenance)));
		ASSERT_THAT(AreEqual(
			StableDump,
			FString(UTF8_TO_TCHAR(DumpTypedSemanticFunction(*HIR).AddressOf()))));

		asCTypedSemanticFunction MissingSourceModule = *HIR;
		asSTypedSemanticExpression* InvalidImportedCall = nullptr;
		for (asUINT Index = 0;
			Index < MissingSourceModule.expressions.GetLength(); ++Index)
		{
			if (MissingSourceModule.expressions[Index].kind
					== asETypedSemanticExpressionKind::ResolvedCall
				&& MissingSourceModule.expressions[Index].callTargetKind
					== asETypedSemanticCallTargetKind::ImportedFunction)
			{
				InvalidImportedCall = &MissingSourceModule.expressions[Index];
				break;
			}
		}
		ASSERT_THAT(IsNotNull(InvalidImportedCall));
		if (InvalidImportedCall != nullptr)
		{
			InvalidImportedCall->targetProvenance.importedSourceModule.Clear();
			ASSERT_THAT(AreEqual(
				asETypedSemanticVerificationError::InvalidCallTargetProvenance,
				VerifyTypedSemanticFunction(MissingSourceModule).error));

			asCTypedSemanticFunction FrozenBoundFunction = *HIR;
			for (asUINT Index = 0;
				Index < FrozenBoundFunction.expressions.GetLength(); ++Index)
			{
				if (FrozenBoundFunction.expressions[Index].kind
						== asETypedSemanticExpressionKind::ResolvedCall
					&& FrozenBoundFunction.expressions[Index].callTargetKind
						== asETypedSemanticCallTargetKind::ImportedFunction)
				{
					FrozenBoundFunction.expressions[Index].resolvedFunctionId =
						FirstTarget->id;
					break;
				}
			}
			ASSERT_THAT(AreEqual(
				asETypedSemanticVerificationError::InvalidCallTargetProvenance,
				VerifyTypedSemanticFunction(FrozenBoundFunction).error));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
