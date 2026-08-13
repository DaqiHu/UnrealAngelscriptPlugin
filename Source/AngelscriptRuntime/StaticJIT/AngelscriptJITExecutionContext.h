#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITBindingContext.h"
#include "StaticJIT/AngelscriptJITReferenceResolver.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

/**
 * One current-function Binding snapshot retained for an external Runtime call.
 * The function and every retired provider payload remain alive until this
 * lease leaves scope.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptJITBindingExecutionLease final
{
public:
	explicit FAngelscriptJITBindingExecutionLease(
		asIScriptFunction* InFunction);
	~FAngelscriptJITBindingExecutionLease();

	FAngelscriptJITBindingExecutionLease(
		const FAngelscriptJITBindingExecutionLease&) = delete;
	FAngelscriptJITBindingExecutionLease& operator=(
		const FAngelscriptJITBindingExecutionLease&) = delete;

	const asSJITFunctionBinding& GetBinding() const
	{
		return Binding;
	}

private:
	asCScriptFunction* Function = nullptr;
	asSJITFunctionBinding Binding;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptJITExecutionScope final
{
public:
	FAngelscriptJITExecutionScope(
		FScriptExecution& InExecution,
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		const asSJITFunctionBinding& Binding,
		const FAngelscriptJITResolvedReferenceTable& ReferenceTable);
	FAngelscriptJITExecutionScope(
		FScriptExecution& InExecution,
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		const asSJITFunctionBinding& Binding,
		const FAngelscriptJITResolvedReferenceTable* ReferenceTable);

	~FAngelscriptJITExecutionScope();

	FAngelscriptJITExecutionScope(const FAngelscriptJITExecutionScope&) = delete;
	FAngelscriptJITExecutionScope& operator=(
		const FAngelscriptJITExecutionScope&) = delete;

private:
	FScriptExecution& Execution;
	asIScriptEngine* PreviousEngine = nullptr;
	asIScriptFunction* PreviousFunction = nullptr;
	const asSJITFunctionBinding* PreviousBinding = nullptr;
	void* PreviousBindingUserData = nullptr;
	const void* PreviousReferenceTable = nullptr;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptJITExecutionContext final
{
public:
	static bool InvokeVM(
		FScriptExecution& Execution,
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		const asSJITFunctionBinding& Binding,
		const FAngelscriptJITResolvedReferenceTable& ReferenceTable,
		asDWORD* FramePointer,
		asQWORD* OutValue);
	static bool InvokeVM(
		FScriptExecution& Execution,
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		const asSJITFunctionBinding& Binding,
		asDWORD* FramePointer,
		asQWORD* OutValue);

	template <typename RawInvoker>
	static bool InvokeRaw(
		FScriptExecution& Execution,
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		const asSJITFunctionBinding& Binding,
		const FAngelscriptJITResolvedReferenceTable& ReferenceTable,
		RawInvoker&& Invoker)
	{
		if (Function == nullptr || Binding.RawEntry == nullptr)
		{
			return false;
		}
		MarkInvocation(Binding,
			FAngelscriptJITBindingContext::EEntryKind::Raw);
		FAngelscriptJITExecutionScope Scope(
			Execution, Engine, Function, Binding, ReferenceTable);
		Forward<RawInvoker>(Invoker)(Binding.RawEntry, Execution);
		return true;
	}

	template <typename RawInvoker>
	static bool InvokeRaw(
		FScriptExecution& Execution,
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		const asSJITFunctionBinding& Binding,
		RawInvoker&& Invoker)
	{
		if (Function == nullptr || Binding.RawEntry == nullptr)
		{
			return false;
		}
		MarkInvocation(Binding,
			FAngelscriptJITBindingContext::EEntryKind::Raw);
		FAngelscriptJITExecutionScope Scope(
			Execution, Engine, Function, Binding, GetReferenceTable(Binding));
		Forward<RawInvoker>(Invoker)(Binding.RawEntry, Execution);
		return true;
	}

	static bool InvokeParms(
		FScriptExecution& Execution,
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		const asSJITFunctionBinding& Binding,
		const FAngelscriptJITResolvedReferenceTable& ReferenceTable,
		void* Object,
		void* Parms);
	static bool InvokeParms(
		FScriptExecution& Execution,
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		const asSJITFunctionBinding& Binding,
		void* Object,
		void* Parms);

	static const FAngelscriptJITResolvedReferenceTable* GetReferenceTable(
		const FScriptExecution& Execution);
	static const FAngelscriptJITResolvedReferenceTable* GetReferenceTable(
		const asSJITFunctionBinding& Binding);

	static void MarkInvocation(
		const asSJITFunctionBinding& Binding,
		FAngelscriptJITBindingContext::EEntryKind Kind);
};
