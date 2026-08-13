#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/BytecodeJIT/AngelscriptBytecodeJIT.h"

/**
 * Temporary Engine-local lifecycle facade retained while compiler ownership
 * moves to FAngelscriptJITCoordinator. Static generation is delegated to the
 * independent bytecode generator and never installs it as an asIJITCompiler.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptStaticJIT final : public asIJITCompiler
{
	void OnFunctionReady(asIScriptFunction* ScriptFunction) override;
	void ReleaseFunctionBinding(
		asIScriptFunction* ScriptFunction,
		const asSJITFunctionBinding& Binding) override;

	bool PublishProviderSelection(
		asIScriptFunction* ScriptFunction,
		const struct FAngelscriptJITArtifactSelection& Selection,
		const class FAngelscriptJITImmutableArtifactSetValidationResult*
			ImmutableValidation = nullptr);

	bool ClearProviderSelection(asIScriptFunction* ScriptFunction);

	static bool CanGenerateParmsEntry(
		const asIScriptFunction* ScriptFunction)
	{
		return FAngelscriptBytecodeJIT::CanGenerateParmsEntry(ScriptFunction);
	}

	FAngelscriptBytecodeJIT& GetBytecodeGenerator()
	{
		return BytecodeGenerator;
	}

	const FAngelscriptBytecodeJIT& GetBytecodeGenerator() const
	{
		return BytecodeGenerator;
	}

private:
	FCriticalSection ProviderBindingMutex;
	TMap<const void*, TSharedPtr<const class FAngelscriptJITBindingContext,
		ESPMode::ThreadSafe>> ActiveProviderBindingContexts;
	FAngelscriptBytecodeJIT BytecodeGenerator;
};

#if AS_WITH_STATIC_JIT_DIAGNOSTICS && AS_CAN_GENERATE_JIT
ANGELSCRIPTRUNTIME_API bool GenerateStaticJITSourceTextForDiagnostics(
	class asIScriptModule* Module,
	FString& OutSourceText,
	bool bEmitDebugMetadata,
	FString* OutError = nullptr);
#endif
