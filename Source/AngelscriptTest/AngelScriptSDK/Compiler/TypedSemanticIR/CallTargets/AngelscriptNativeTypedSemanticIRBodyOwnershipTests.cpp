#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeTypedSemanticIRCallTargetTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	int NativeIncrement(const int Value)
	{
		return Value + 1;
	}

	asIScriptModule* BuildBorrowingModule(
		asCScriptEngine& Engine,
		asCScriptFunction& SharedTarget,
		const ANSICHAR* ModuleName,
		const bool bExternal)
	{
		asCModule* const Module = static_cast<asCModule*>(
			Engine.GetModule(ModuleName, asGM_ALWAYS_CREATE));
		if (Module == nullptr)
		{
			return nullptr;
		}

		Module->AddScriptFunction(&SharedTarget);
		Module->globalFunctions.Add(&SharedTarget);
		Module->globalFunctionList.PushLast(&SharedTarget);
		if (bExternal)
		{
			Module->externalFunctions.PushLast(&SharedTarget);
		}
		const ANSICHAR* const Source =
			"int RunBorrowed(int Value) { return SharedTarget(Value); }";
		asIScriptFunction* CompiledCaller = nullptr;
		const int CompileResult = Module->CompileFunction(
			ModuleName,
			Source,
			0,
			asCOMP_ADD_TO_MODULE,
			&CompiledCaller);
		if (CompiledCaller != nullptr)
		{
			CompiledCaller->Release();
		}
		if (CompileResult < 0)
		{
			return nullptr;
		}
		return Module;
	}
}

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRBodyOwnershipTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.CallTargets.BodyOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ConcreteScriptSystemSharedAndExternalTargetsRetainDistinctBodyOwnership)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIRCallTargetTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-CALL-TARGET-BODY-OWNERSHIP",
			ENativeEvidence::Compile
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
		const int NativeFunctionId = ScriptEngine->RegisterGlobalFunction(
			"int NativeIncrement(int Value)",
			asFUNCTION(NativeIncrement),
			asCALL_CDECL);
		ASSERT_THAT(IsTrue(NativeFunctionId >= 0));

		const ANSICHAR* const LocalModuleName =
			"TypedSemanticIRConcreteTargetModule";
		asIScriptModule* LocalModule = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(
			ScriptEngine,
			LocalModuleName,
			"int LocalTarget(int Value) { return Value + 2; }\n"
			"int RunLocal(int Value) { return LocalTarget(Value); }\n"
			"int RunNative(int Value) { return NativeIncrement(Value); }",
			LocalModule) >= 0));
		ASSERT_THAT(IsNotNull(LocalModule));
		if (LocalModule == nullptr)
		{
			return;
		}

		asCScriptFunction* const LocalTarget =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				LocalModule,
				"int LocalTarget(int Value)"));
		asCScriptFunction* const RunLocal =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				LocalModule,
				"int RunLocal(int Value)"));
		asCScriptFunction* const RunNative =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				LocalModule,
				"int RunNative(int Value)"));
		asCScriptFunction* const NativeTarget = NativeFunctionId >= 0
			? ScriptEngine->GetScriptFunction(NativeFunctionId)
			: nullptr;
		ASSERT_THAT(IsNotNull(LocalTarget));
		ASSERT_THAT(IsNotNull(RunLocal));
		ASSERT_THAT(IsNotNull(RunNative));
		ASSERT_THAT(IsNotNull(NativeTarget));
		if (LocalTarget == nullptr
			|| RunLocal == nullptr
			|| RunNative == nullptr
			|| NativeTarget == nullptr)
		{
			return;
		}

		const asCTypedSemanticFunction* const LocalHIR =
			RunLocal->GetTypedSemanticFunction();
		const asCTypedSemanticFunction* const NativeHIR =
			RunNative->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(LocalHIR));
		ASSERT_THAT(IsNotNull(NativeHIR));
		if (LocalHIR == nullptr || NativeHIR == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(LocalHIR->header.callingModule == LocalModuleName));
		ASSERT_THAT(AreEqual(
			asETypedSemanticBodyAvailability::ScriptBody,
			LocalHIR->header.bodyAvailability));
		ASSERT_THAT(AreEqual(
			asETypedSemanticBodyOwnership::CallingModule,
			LocalHIR->header.bodyOwnership));
		ASSERT_THAT(IsTrue(
			LocalHIR->header.bodyOwnerModule == LocalModuleName));

		const asSTypedSemanticExpression* const LocalCall =
			FindResolvedCall(*LocalHIR, LocalTarget->id);
		const asSTypedSemanticExpression* const NativeCall =
			FindResolvedCall(*NativeHIR, NativeTarget->id);
		ASSERT_THAT(IsNotNull(LocalCall));
		ASSERT_THAT(IsNotNull(NativeCall));
		if (LocalCall == nullptr || NativeCall == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asETypedSemanticCallTargetKind::ScriptFunction,
			LocalCall->callTargetKind));
		ASSERT_THAT(IsTrue(
			LocalCall->targetProvenance.canonicalDeclaration
				== "int LocalTarget(const int)"));
		ASSERT_THAT(IsTrue(
			LocalCall->targetProvenance.declarationModule == LocalModuleName));
		ASSERT_THAT(AreEqual(
			asETypedSemanticBodyAvailability::ScriptBody,
			LocalCall->targetProvenance.bodyAvailability));
		ASSERT_THAT(AreEqual(
			asETypedSemanticBodyOwnership::CallingModule,
			LocalCall->targetProvenance.bodyOwnership));
		ASSERT_THAT(IsTrue(
			LocalCall->targetProvenance.bodyOwnerModule == LocalModuleName));

		ASSERT_THAT(AreEqual(
			asETypedSemanticCallTargetKind::SystemFunction,
			NativeCall->callTargetKind));
		ASSERT_THAT(IsTrue(
			NativeCall->targetProvenance.canonicalDeclaration
				== "int NativeIncrement(int)"));
		ASSERT_THAT(AreEqual(
			asETypedSemanticBodyAvailability::RegisteredNative,
			NativeCall->targetProvenance.bodyAvailability));
		ASSERT_THAT(AreEqual(
			asETypedSemanticBodyOwnership::NativeRuntime,
			NativeCall->targetProvenance.bodyOwnership));
		ASSERT_THAT(IsTrue(
			NativeCall->targetProvenance.declarationModule == ""));
		ASSERT_THAT(IsTrue(
			NativeCall->targetProvenance.bodyOwnerModule == ""));

		const ANSICHAR* const ProviderName =
			"TypedSemanticIRSharedBodyProvider";
		asIScriptModule* Provider = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(
			ScriptEngine,
			ProviderName,
			"int SharedTarget(int Value) { return Value + 7; }",
			Provider) >= 0));
		ASSERT_THAT(IsNotNull(Provider));
		if (Provider == nullptr)
		{
			return;
		}
		asCScriptFunction* const SharedTarget =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Provider,
				"int SharedTarget(int Value)"));
		ASSERT_THAT(IsNotNull(SharedTarget));
		if (SharedTarget == nullptr)
		{
			return;
		}
		SharedTarget->SetShared(true);

		const ANSICHAR* const SharedBorrowerName =
			"TypedSemanticIRSharedBodyBorrower";
		const ANSICHAR* const ExternalBorrowerName =
			"TypedSemanticIRExternalBodyBorrower";
		asIScriptModule* const SharedBorrower = BuildBorrowingModule(
			*ScriptEngine,
			*SharedTarget,
			SharedBorrowerName,
			false);
		asIScriptModule* const ExternalBorrower = BuildBorrowingModule(
			*ScriptEngine,
			*SharedTarget,
			ExternalBorrowerName,
			true);
		ASSERT_THAT(IsNotNull(SharedBorrower));
		ASSERT_THAT(IsNotNull(ExternalBorrower));
		if (SharedBorrower == nullptr || ExternalBorrower == nullptr)
		{
			return;
		}

		asCScriptFunction* const SharedCaller =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				SharedBorrower,
				"int RunBorrowed(int Value)"));
		asCScriptFunction* const ExternalCaller =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				ExternalBorrower,
				"int RunBorrowed(int Value)"));
		const asCTypedSemanticFunction* const SharedHIR =
			SharedCaller != nullptr
				? SharedCaller->GetTypedSemanticFunction()
				: nullptr;
		const asCTypedSemanticFunction* const ExternalHIR =
			ExternalCaller != nullptr
				? ExternalCaller->GetTypedSemanticFunction()
				: nullptr;
		ASSERT_THAT(IsNotNull(SharedHIR));
		ASSERT_THAT(IsNotNull(ExternalHIR));
		if (SharedHIR == nullptr || ExternalHIR == nullptr)
		{
			return;
		}

		const asSTypedSemanticExpression* const SharedCall =
			FindResolvedCall(*SharedHIR, SharedTarget->id);
		const asSTypedSemanticExpression* const ExternalCall =
			FindResolvedCall(*ExternalHIR, SharedTarget->id);
		ASSERT_THAT(IsNotNull(SharedCall));
		ASSERT_THAT(IsNotNull(ExternalCall));
		if (SharedCall == nullptr || ExternalCall == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asETypedSemanticBodyOwnership::SharedBorrowed,
			SharedCall->targetProvenance.bodyOwnership));
		ASSERT_THAT(IsTrue(
			SharedCall->targetProvenance.declarationModule
				== SharedBorrowerName));
		ASSERT_THAT(IsTrue(
			SharedCall->targetProvenance.bodyOwnerModule == ProviderName));
		ASSERT_THAT(IsTrue(
			(SharedCall->targetProvenance.declaredTraitBits
				& asTRAIT_SHARED) != 0));
		ASSERT_THAT(AreEqual(
			asETypedSemanticBodyOwnership::ExternalBorrowed,
			ExternalCall->targetProvenance.bodyOwnership));
		ASSERT_THAT(IsTrue(
			ExternalCall->targetProvenance.declarationModule
				== ExternalBorrowerName));
		ASSERT_THAT(IsTrue(
			ExternalCall->targetProvenance.bodyOwnerModule == ProviderName));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*SharedHIR).IsValid()));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*ExternalHIR).IsValid()));

		asCTypedSemanticFunction SharedOwner =
			*SharedTarget->GetTypedSemanticFunction();
		SharedOwner.header.declaredTraitBits |= asTRAIT_SHARED;
		SharedOwner.header.traitSummary = ClassifyTypedSemanticFunctionTraits(
			SharedOwner.header.declaredTraitBits);
		SharedOwner.header.callingModule = ProviderName;
		SharedOwner.header.bodyAvailability =
			asETypedSemanticBodyAvailability::ScriptBody;
		SharedOwner.header.bodyOwnership =
			asETypedSemanticBodyOwnership::SharedOwner;
		SharedOwner.header.bodyOwnerModule = ProviderName;
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(SharedOwner).IsValid()));

		asCTypedSemanticFunction MissingSharedOwner = SharedOwner;
		MissingSharedOwner.header.bodyOwnerModule.Clear();
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::InvalidFunctionBodyProvenance,
			VerifyTypedSemanticFunction(MissingSharedOwner).error));

		asCTypedSemanticFunction MissingExternalOwner = *ExternalHIR;
		for (asUINT Index = 0;
			Index < MissingExternalOwner.expressions.GetLength(); ++Index)
		{
			asSTypedSemanticExpression& Expression =
				MissingExternalOwner.expressions[Index];
			if (Expression.kind == asETypedSemanticExpressionKind::ResolvedCall
				&& Expression.resolvedFunctionId == SharedTarget->id)
			{
				Expression.targetProvenance.bodyOwnerModule.Clear();
				break;
			}
		}
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::InvalidCallTargetProvenance,
			VerifyTypedSemanticFunction(MissingExternalOwner).error));

		const FString ExternalDump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*ExternalHIR).AddressOf());
		ASSERT_THAT(IsTrue(ExternalDump.Contains(TEXT("ExternalBorrowed"))));
		ASSERT_THAT(IsTrue(ExternalDump.Contains(
			TEXT("bodyOwner=TypedSemanticIRSharedBodyProvider"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
