#include "StaticJIT/StaticJITHeader.h"
#include "AngelscriptStaticJIT.h"
#include "Artifacts/AngelscriptFunctionRoute.h"
#include "ClassGenerator/ASClass.h"
#include "StaticJIT/AngelscriptJITExecutionContext.h"
#include "UObject/Class.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_scriptengine.h"
//#include "as_scriptobject.h"
//#include "as_callfunc.h"
//#include "as_generic.h"
//#include "as_texts.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptobject.h"
#include "source/as_callfunc.h"
#include "source/as_generic.h"
#include "source/as_texts.h"
#include "EndAngelscriptHeaders.h"

FStaticJITCurrentFunctionCall::FStaticJITCurrentFunctionCall(
	FScriptExecution& Execution,
	const FAngelscriptStableFunctionKey& FunctionKey,
	asCScriptFunction* ReferencedFunction,
	void* CallObject,
	const bool bResolveVirtualFunction)
{
	asIScriptEngine* const ScriptEngine = Execution.jitEngine;
	FAngelscriptEngine* const RuntimeEngine =
		FAngelscriptEngine::TryGetByScriptEngine(ScriptEngine);

	asCScriptFunction* CurrentFunction = nullptr;
	if (RuntimeEngine != nullptr && !FunctionKey.Hash.IsZero())
	{
		FAngelscriptFunctionRoute Route;
		if (RuntimeEngine->ResolveFunctionRoute(FunctionKey, Route))
		{
			CurrentFunction = static_cast<asCScriptFunction*>(Route.Function);
		}
	}

	// Some bootstrap/test-only modules can execute before a stable route has
	// been published. A reference is usable only while it is still the exact
	// live function at its current id in this same Engine.
	if (CurrentFunction == nullptr
		&& ReferencedFunction != nullptr
		&& ReferencedFunction->GetEngine() == ScriptEngine
		&& ScriptEngine != nullptr
		&& ScriptEngine->GetFunctionById(ReferencedFunction->GetId())
			== ReferencedFunction)
	{
		CurrentFunction = ReferencedFunction;
	}

	if (CurrentFunction == nullptr
		|| CurrentFunction->GetEngine() != ScriptEngine)
	{
		return;
	}

	if (bResolveVirtualFunction
		&& CallObject != nullptr
		&& CurrentFunction->vfTableIdx >= 0)
	{
		UObject* const Object = static_cast<UObject*>(CallObject);
		UASClass* const ScriptClass = UASClass::GetFirstASClass(Object);
		asCObjectType* const ObjectType = ScriptClass != nullptr
			? static_cast<asCObjectType*>(ScriptClass->ScriptTypePtr)
			: nullptr;
		if (ObjectType != nullptr
			&& CurrentFunction->vfTableIdx
				< static_cast<int32>(ObjectType->virtualFunctionTable.GetLength()))
		{
			asCScriptFunction* const Override =
				ObjectType->virtualFunctionTable[CurrentFunction->vfTableIdx];
			if (Override != nullptr && Override->GetEngine() == ScriptEngine)
			{
				CurrentFunction = Override;
			}
		}
	}

	CurrentFunction->AddRef();
	Function = CurrentFunction;
}

FStaticJITCurrentFunctionCall::~FStaticJITCurrentFunctionCall()
{
	if (Function != nullptr)
	{
		Function->Release();
	}
}

namespace
{
	static const FAngelscriptJITResolvedReferenceSlot* GetGeneratedReferenceSlot(
		const FScriptExecution& Execution,
		const uint32 SlotIndex)
	{
		const FAngelscriptJITResolvedReferenceTable* const Table =
			FAngelscriptJITExecutionContext::GetReferenceTable(Execution);
		if (Table == nullptr || !Table->Slots.IsValidIndex(
			static_cast<int32>(SlotIndex)))
		{
			return nullptr;
		}
		const FAngelscriptJITResolvedReferenceSlot& Slot =
			Table->Slots[SlotIndex];
		return Slot.SlotIndex == SlotIndex ? &Slot : nullptr;
	}
}

asCScriptFunction* FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(
	const FScriptExecution& Execution,
	const uint32 SlotIndex)
{
	const FAngelscriptJITResolvedReferenceSlot* const Slot =
		GetGeneratedReferenceSlot(Execution, SlotIndex);
	if (Slot == nullptr
		|| (Slot->Kind != EAngelscriptArtifactReferenceKind::ScriptFunction
			&& Slot->Kind
				!= EAngelscriptArtifactReferenceKind::EnvironmentSymbol))
	{
		return nullptr;
	}
	return static_cast<asCScriptFunction*>(const_cast<void*>(Slot->Value));
}

FAngelscriptJITResolvedSystemFunctionPointer
FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(
	const FScriptExecution& Execution,
	const uint32 SlotIndex)
{
	const FAngelscriptJITResolvedReferenceSlot* const Slot =
		GetGeneratedReferenceSlot(Execution, SlotIndex);
	if (Slot == nullptr
		|| Slot->Kind != EAngelscriptArtifactReferenceKind::EnvironmentSymbol)
	{
		return {};
	}
	const asCScriptFunction* const Function =
		static_cast<const asCScriptFunction*>(Slot->Value);
	if (Function == nullptr || Function->sysFuncIntf == nullptr)
	{
		return {};
	}
	return {Function->sysFuncIntf->func, Function->sysFuncIntf->method};
}

asCTypeInfo* FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(
	const FScriptExecution& Execution,
	const uint32 SlotIndex)
{
	const FAngelscriptJITResolvedReferenceSlot* const Slot =
		GetGeneratedReferenceSlot(Execution, SlotIndex);
	if (Slot == nullptr
		|| (Slot->Kind != EAngelscriptArtifactReferenceKind::ScriptType
			&& Slot->Kind
				!= EAngelscriptArtifactReferenceKind::EnvironmentSymbol))
	{
		return nullptr;
	}
	return static_cast<asCTypeInfo*>(const_cast<void*>(Slot->Value));
}

asCObjectType* FAngelscriptJITGeneratedReferenceAccess::GetObjectType(
	const FScriptExecution& Execution,
	const uint32 SlotIndex)
{
	return CastToObjectType(GetTypeInfo(Execution, SlotIndex));
}

void* FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(
	const FScriptExecution& Execution,
	const uint32 SlotIndex)
{
	const FAngelscriptJITResolvedReferenceSlot* const Slot =
		GetGeneratedReferenceSlot(Execution, SlotIndex);
	if (Slot == nullptr
		|| (Slot->Kind != EAngelscriptArtifactReferenceKind::ScriptGlobal
			&& Slot->Kind
				!= EAngelscriptArtifactReferenceKind::StringLiteral))
	{
		return nullptr;
	}
	return const_cast<void*>(Slot->Value);
}

uint32 FAngelscriptJITGeneratedReferenceAccess::GetPropertyOffset(
	const FScriptExecution& Execution,
	const uint32 SlotIndex)
{
	const FAngelscriptJITResolvedReferenceSlot* const Slot =
		GetGeneratedReferenceSlot(Execution, SlotIndex);
	if (Slot == nullptr
		|| Slot->Kind != EAngelscriptArtifactReferenceKind::ScriptProperty)
	{
		return 0;
	}
	const asCObjectProperty* const Property =
		static_cast<const asCObjectProperty*>(Slot->Value);
	return Property != nullptr && Property->byteOffset >= 0
		? static_cast<uint32>(Property->byteOffset) : 0;
}

void FStaticJITFunction::SetException(FScriptExecution& Execution, EJITException Exception)
{
	Execution.bExceptionThrown = true;

	if (Exception == EJITException::NullPointer)
		FAngelscriptEngine::HandleExceptionFromJIT(TXT_NULL_POINTER_ACCESS);
	else if (Exception == EJITException::Div0)
		FAngelscriptEngine::HandleExceptionFromJIT(TXT_DIVIDE_BY_ZERO);
	else if (Exception == EJITException::Overflow)
		FAngelscriptEngine::HandleExceptionFromJIT(TXT_DIVIDE_OVERFLOW);
	else if (Exception == EJITException::UnboundFunction)
		FAngelscriptEngine::HandleExceptionFromJIT(TXT_UNBOUND_FUNCTION);
	else if (Exception == EJITException::OutOfBounds)
		FAngelscriptEngine::HandleExceptionFromJIT("Index out of bounds.");
	else
		FAngelscriptEngine::HandleExceptionFromJIT("Unknown exception.");
}

void FStaticJITFunction::SetNullPointerException(FScriptExecution& Execution)
{
	Execution.bExceptionThrown = true;
	FAngelscriptEngine::HandleExceptionFromJIT(TXT_NULL_POINTER_ACCESS);
}

void FStaticJITFunction::SetDivByZeroException(FScriptExecution& Execution)
{
	Execution.bExceptionThrown = true;
	FAngelscriptEngine::HandleExceptionFromJIT(TXT_DIVIDE_BY_ZERO);
}

void FStaticJITFunction::SetOverflowException(FScriptExecution& Execution)
{
	Execution.bExceptionThrown = true;
	FAngelscriptEngine::HandleExceptionFromJIT(TXT_DIVIDE_OVERFLOW);
}

void FStaticJITFunction::SetUnboundFunctionException(FScriptExecution& Execution)
{
	Execution.bExceptionThrown = true;
	FAngelscriptEngine::HandleExceptionFromJIT(TXT_UNBOUND_FUNCTION);
}

void FStaticJITFunction::SetOutOfBoundsException(FScriptExecution& Execution)
{
	Execution.bExceptionThrown = true;
	FAngelscriptEngine::HandleExceptionFromJIT("Index out of bounds.");
}

void FStaticJITFunction::SetSwitchValueInvalidException(FScriptExecution& Execution)
{
	Execution.bExceptionThrown = true;
	FAngelscriptEngine::HandleExceptionFromJIT("Invalid enum value passed to switch");
}

void FStaticJITFunction::SetUnknownException(FScriptExecution& Execution)
{
	Execution.bExceptionThrown = true;
	FAngelscriptEngine::HandleExceptionFromJIT("Unknown exception.");
}

void FStaticJITFunction::ScriptFinishConstruct(FScriptExecution& Execution, asIScriptObject* Object, asITypeInfo* TypeInfo)
{
	SCRIPT_ENGINE->FinishConstructObject(Object, TypeInfo);
}

void FStaticJITFunction::ScriptCallNative(FScriptExecution& Execution, asCScriptFunction* Function, asBYTE* l_sp, asQWORD* valueRegister, void** objectRegister)
{
	asCScriptFunction* descr = Function;
	asSSystemFunctionInterface *sysFunc = descr->sysFuncIntf;

	int callConv = sysFunc->callConv;
	if (callConv == ICC_GENERIC_FUNC || callConv == ICC_GENERIC_METHOD)
	{
		void (*func)(asIScriptGeneric*) = (void (*)(asIScriptGeneric*))sysFunc->func;
		asDWORD *args = (asDWORD*)l_sp;

		// Verify the object pointer if it is a class method
		void *currentObject = 0;
		check( sysFunc->callConv == ICC_GENERIC_FUNC || sysFunc->callConv == ICC_GENERIC_METHOD );
		if( sysFunc->callConv == ICC_GENERIC_METHOD )
		{
			// The object pointer should be popped from the context stack
			// Check for null pointer
			currentObject = (void*)*(asPWORD*)(args);
			if( currentObject == 0 )
			{
				FStaticJITFunction::SetException(Execution, EJITException::NullPointer);
				return;
			}

			check( sysFunc->baseOffset == 0 );
		}

		asCGeneric gen(SCRIPT_ENGINE, descr, currentObject, args);

		auto* tld = Execution.tld;
		auto* prevActiveFunction = tld->activeFunction;
		tld->activeFunction = descr;

		func(&gen);

		tld->activeFunction = prevActiveFunction;

		*valueRegister = gen.returnVal;
		*objectRegister = (void*)gen.objectRegister;

		return;
	}

	if (sysFunc->caller.IsBound())
	{ 
		asDWORD* StackArgs = (asDWORD*)l_sp;

		void* FunctionArgs[32];
		void* ReturnAddress;
		unsigned int ArgIndex = 0;
		const bool bPassObjectArgumentLast =
			sysFunc->callConv == ICC_CDECL_OBJLAST
			|| sysFunc->callConv == ICC_CDECL_OBJLAST_RETURNINMEM;
		void* ObjectArgument = nullptr;

		// Verify the object pointer if it is a class method
		if( sysFunc->callConv >= ICC_THISCALL )
		{
			// The object pointer should be popped from the context stack
			ObjectArgument = *(void**)&StackArgs[0];

			// The VM always stores the object before explicit script parameters.
			// CDECL_OBJLAST is the exception at the native call boundary.
			if (!bPassObjectArgumentLast)
			{
				FunctionArgs[ArgIndex] = ObjectArgument;
				ArgIndex++;
			}
		}

		// Some system functions want to know the script function that is being called
		if (sysFunc->passFirstParamMetaData != asEFirstParamMetaData::None)
		{
			if (sysFunc->passFirstParamMetaData == asEFirstParamMetaData::ScriptFunction)
			{
				FunctionArgs[ArgIndex] = descr;
				ArgIndex++;
			}
			else
			{
				FunctionArgs[ArgIndex] = descr->objectType;
				ArgIndex++;
			}
		}

		if( descr->DoesReturnOnStack() )
		{
			// Skip the address where the return value will be stored
			if (sysFunc->callConv >= ICC_THISCALL)
				ReturnAddress = *(void**)(StackArgs + AS_PTR_SIZE);
			else
				ReturnAddress = *(void**)StackArgs;

			// Return-on-stack uses caller-owned storage. If the native call throws, AngelScript unwinding is
			// responsible for cleaning up the return slot, so this bridge must not try to destroy it here.
		}
		else if (descr->returnType.IsObjectHandle() && !descr->returnType.IsReference())
		{
			ReturnAddress = objectRegister;
		}
		else
		{
			ReturnAddress = valueRegister;
		}

		for (int i = 0, paramCount = descr->parameterOffsets.GetLength(); i < paramCount; ++i)
		{
			auto& paramType = descr->parameterTypes[i];
			auto paramOffset = descr->parameterOffsets[i];

			if (paramType.GetTokenType() == ttQuestion)
			{
				FunctionArgs[ArgIndex] = *(void**)&StackArgs[paramOffset];
				++ArgIndex;
				FunctionArgs[ArgIndex] = &StackArgs[paramOffset+2];
			}
			else if (paramType.IsObject() || paramType.IsReference())
			{
				FunctionArgs[ArgIndex] = *(void**)&StackArgs[paramOffset];
			}
			else
			{
				FunctionArgs[ArgIndex] = &StackArgs[paramOffset];
			}

			++ArgIndex;
		}

		if (bPassObjectArgumentLast)
		{
			FunctionArgs[ArgIndex] = ObjectArgument;
			ArgIndex++;
		}

		auto* tld = Execution.tld;
		auto* prevActiveFunction = tld->activeFunction;
		tld->activeFunction = descr;

		if (sysFunc->caller.type == 1)
			sysFunc->caller.FunctionCaller(sysFunc->func, &FunctionArgs[0], ReturnAddress);
		else //if (sysFunc->caller.type == 2)
			sysFunc->caller.MethodCaller(sysFunc->method, &FunctionArgs[0], ReturnAddress);

		tld->activeFunction = prevActiveFunction;

		return;
	}

	checkf(false,
		TEXT("Function %s had no way to call it. Needs to be either generic or have a Caller."),
		ANSI_TO_TCHAR(Function->GetName()));
}
