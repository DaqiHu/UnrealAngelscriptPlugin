#include "StaticJIT/AngelscriptJITExecutionContext.h"

#include "StaticJIT/AngelscriptJITBindingContext.h"

FAngelscriptJITBindingExecutionLease::FAngelscriptJITBindingExecutionLease(
	asIScriptFunction* InFunction)
	: Function(static_cast<asCScriptFunction*>(InFunction))
{
	if (Function != nullptr)
	{
		Function->AddRef();
		Binding = Function->AcquireJITBindingForExecution();
	}
}

FAngelscriptJITBindingExecutionLease::~FAngelscriptJITBindingExecutionLease()
{
	if (Function != nullptr)
	{
		Function->ReleaseJITBindingAfterExecution();
		Function->Release();
	}
}

FAngelscriptJITExecutionScope::FAngelscriptJITExecutionScope(
	FScriptExecution& InExecution,
	asIScriptEngine* Engine,
	asIScriptFunction* Function,
	const asSJITFunctionBinding& Binding,
	const FAngelscriptJITResolvedReferenceTable& ReferenceTable)
	: FAngelscriptJITExecutionScope(
		InExecution, Engine, Function, Binding, &ReferenceTable)
{
}

FAngelscriptJITExecutionScope::FAngelscriptJITExecutionScope(
	FScriptExecution& InExecution,
	asIScriptEngine* Engine,
	asIScriptFunction* Function,
	const asSJITFunctionBinding& Binding,
	const FAngelscriptJITResolvedReferenceTable* ReferenceTable)
	: Execution(InExecution)
	, PreviousEngine(InExecution.jitEngine)
	, PreviousFunction(InExecution.jitFunction)
	, PreviousBinding(InExecution.jitBinding)
	, PreviousBindingUserData(InExecution.jitBindingUserData)
	, PreviousReferenceTable(InExecution.jitReferenceTable)
{
	Execution.jitEngine = Engine;
	Execution.jitFunction = Function;
	Execution.jitBinding = &Binding;
	Execution.jitBindingUserData = Binding.UserData;
	Execution.jitReferenceTable = ReferenceTable;
}

FAngelscriptJITExecutionScope::~FAngelscriptJITExecutionScope()
{
	Execution.jitEngine = PreviousEngine;
	Execution.jitFunction = PreviousFunction;
	Execution.jitBinding = PreviousBinding;
	Execution.jitBindingUserData = PreviousBindingUserData;
	Execution.jitReferenceTable = PreviousReferenceTable;
}

bool FAngelscriptJITExecutionContext::InvokeVM(
	FScriptExecution& Execution,
	asIScriptEngine* Engine,
	asIScriptFunction* Function,
	const asSJITFunctionBinding& Binding,
	const FAngelscriptJITResolvedReferenceTable& ReferenceTable,
	asDWORD* FramePointer,
	asQWORD* OutValue)
{
	if (Function == nullptr || Binding.VMEntry == nullptr)
	{
		return false;
	}
	MarkInvocation(Binding,
		FAngelscriptJITBindingContext::EEntryKind::VM);
	FAngelscriptJITExecutionScope Scope(
		Execution, Engine, Function, Binding, ReferenceTable);
	Binding.VMEntry(Execution, FramePointer, OutValue);
	return true;
}

bool FAngelscriptJITExecutionContext::InvokeVM(
	FScriptExecution& Execution,
	asIScriptEngine* Engine,
	asIScriptFunction* Function,
	const asSJITFunctionBinding& Binding,
	asDWORD* FramePointer,
	asQWORD* OutValue)
{
	if (Function == nullptr || Binding.VMEntry == nullptr)
	{
		return false;
	}
	MarkInvocation(Binding,
		FAngelscriptJITBindingContext::EEntryKind::VM);
	FAngelscriptJITExecutionScope Scope(
		Execution, Engine, Function, Binding, GetReferenceTable(Binding));
	Binding.VMEntry(Execution, FramePointer, OutValue);
	return true;
}

bool FAngelscriptJITExecutionContext::InvokeParms(
	FScriptExecution& Execution,
	asIScriptEngine* Engine,
	asIScriptFunction* Function,
	const asSJITFunctionBinding& Binding,
	const FAngelscriptJITResolvedReferenceTable& ReferenceTable,
	void* Object,
	void* Parms)
{
	if (Function == nullptr || Binding.ParmsEntry == nullptr)
	{
		return false;
	}
	MarkInvocation(Binding,
		FAngelscriptJITBindingContext::EEntryKind::Parms);
	FAngelscriptJITExecutionScope Scope(
		Execution, Engine, Function, Binding, ReferenceTable);
	Binding.ParmsEntry(Execution, Object, Parms);
	return true;
}

bool FAngelscriptJITExecutionContext::InvokeParms(
	FScriptExecution& Execution,
	asIScriptEngine* Engine,
	asIScriptFunction* Function,
	const asSJITFunctionBinding& Binding,
	void* Object,
	void* Parms)
{
	if (Function == nullptr || Binding.ParmsEntry == nullptr)
	{
		return false;
	}
	MarkInvocation(Binding,
		FAngelscriptJITBindingContext::EEntryKind::Parms);
	FAngelscriptJITExecutionScope Scope(
		Execution, Engine, Function, Binding, GetReferenceTable(Binding));
	Binding.ParmsEntry(Execution, Object, Parms);
	return true;
}

const FAngelscriptJITResolvedReferenceTable*
FAngelscriptJITExecutionContext::GetReferenceTable(
	const FScriptExecution& Execution)
{
	if (Execution.jitReferenceTable != nullptr)
	{
		return static_cast<const FAngelscriptJITResolvedReferenceTable*>(
			Execution.jitReferenceTable);
	}
	const FAngelscriptJITBindingContext* BindingContext =
		FAngelscriptJITBindingContext::FromBindingUserData(
			Execution.jitBindingUserData);
	return BindingContext != nullptr
		? BindingContext->GetReferenceTable() : nullptr;
}

void FAngelscriptJITExecutionContext::MarkInvocation(
	const asSJITFunctionBinding& Binding,
	const FAngelscriptJITBindingContext::EEntryKind Kind)
{
	if (const FAngelscriptJITBindingContext* const BindingContext =
		FAngelscriptJITBindingContext::FromBindingUserData(Binding.UserData))
	{
		BindingContext->MarkExecution(Kind);
	}
}

const FAngelscriptJITResolvedReferenceTable*
FAngelscriptJITExecutionContext::GetReferenceTable(
	const asSJITFunctionBinding& Binding)
{
	const FAngelscriptJITBindingContext* BindingContext =
		FAngelscriptJITBindingContext::FromBindingUserData(Binding.UserData);
	return BindingContext != nullptr
		? BindingContext->GetReferenceTable() : nullptr;
}
