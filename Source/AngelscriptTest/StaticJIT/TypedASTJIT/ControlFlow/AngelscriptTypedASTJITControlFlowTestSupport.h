#pragma once

#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptTypedASTJITControlFlowTestSupport
{
	inline asCScriptFunction* FindFunction(
		FAutomationTestBase& Test,
		asCModule& Module,
		const char* const FunctionName)
	{
		asCScriptFunction* const ScriptFunction =
			static_cast<asCScriptFunction*>(
				Module.GetFunctionByName(FunctionName));
		Test.TestNotNull(
			TEXT("Control-flow fixture function must exist"),
			ScriptFunction);
		return ScriptFunction;
	}

	inline const asCTypedSemanticFunction* FindVerifiedHIR(
		FAutomationTestBase& Test,
		asCModule& Module,
		const char* const FunctionName)
	{
		asCScriptFunction* const ScriptFunction =
			FindFunction(Test, Module, FunctionName);
		if (ScriptFunction == nullptr)
		{
			return nullptr;
		}

		const asCTypedSemanticFunction* const HIR =
			ScriptFunction->GetTypedSemanticFunction();
		if (!Test.TestNotNull(
				TEXT("Control-flow fixture must publish verified HIR"), HIR))
		{
			return nullptr;
		}
		const asSTypedSemanticVerificationResult Verification =
			VerifyTypedSemanticFunction(*HIR);
		if (!Test.TestTrue(
				TEXT("Control-flow fixture HIR must pass structural verification"),
				Verification.IsValid()))
		{
			Test.AddError(FString::Printf(
				TEXT("%s HIR verification failed: %s"),
				UTF8_TO_TCHAR(FunctionName),
				UTF8_TO_TCHAR(Verification.detail.AddressOf())));
			return nullptr;
		}
		return HIR;
	}

	inline bool TryEmitFunction(
		FAutomationTestBase& Test,
		asCModule& Module,
		const char* const FunctionName,
		const TCHAR* const SymbolName,
		FAngelscriptTypedASTJITEmission& OutEmission)
	{
		asCScriptFunction* const ScriptFunction =
			FindFunction(Test, Module, FunctionName);
		if (ScriptFunction == nullptr)
		{
			return false;
		}

		const asCTypedSemanticFunction* const HIR =
			FindVerifiedHIR(Test, Module, FunctionName);
		if (HIR == nullptr)
		{
			return false;
		}

		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = SymbolName;
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
		OutEmission = EmitTypedASTJITFunction(*HIR, Shape, {});
		if (!OutEmission.bSuccess)
		{
			const asCString Dump = DumpTypedSemanticFunction(*HIR);
			Test.AddError(FString::Printf(
				TEXT("%s control-flow emission failed: %s\n%s"),
				UTF8_TO_TCHAR(FunctionName),
				*OutEmission.FailureDetail,
				UTF8_TO_TCHAR(Dump.AddressOf())));
			return false;
		}

		return true;
	}
}
