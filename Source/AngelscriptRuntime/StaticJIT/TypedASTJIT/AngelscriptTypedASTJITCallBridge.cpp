#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"

#include "AngelscriptEngine.h"
#include "StaticJIT/AngelscriptJITExecutionContext.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_callfunc.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptTypedASTJIT
{
	namespace
	{
		EAngelscriptTypedASTJITBridgeResult Fail(
			FScriptExecution& Execution,
			const EAngelscriptTypedASTJITBridgeResult Result)
		{
			Execution.bExceptionThrown = true;
			return Result;
		}

		int32 GetAngelScriptTypeId(
			const EAngelscriptTypedASTJITScalarKind Kind)
		{
			switch (Kind)
			{
			case EAngelscriptTypedASTJITScalarKind::Void:
				return asTYPEID_VOID;
			case EAngelscriptTypedASTJITScalarKind::Bool:
				return asTYPEID_BOOL;
			case EAngelscriptTypedASTJITScalarKind::Int8:
				return asTYPEID_INT8;
			case EAngelscriptTypedASTJITScalarKind::UInt8:
				return asTYPEID_UINT8;
			case EAngelscriptTypedASTJITScalarKind::Int16:
				return asTYPEID_INT16;
			case EAngelscriptTypedASTJITScalarKind::UInt16:
				return asTYPEID_UINT16;
			case EAngelscriptTypedASTJITScalarKind::Int32:
				return asTYPEID_INT32;
			case EAngelscriptTypedASTJITScalarKind::UInt32:
				return asTYPEID_UINT32;
			case EAngelscriptTypedASTJITScalarKind::Int64:
				return asTYPEID_INT64;
			case EAngelscriptTypedASTJITScalarKind::UInt64:
				return asTYPEID_UINT64;
			case EAngelscriptTypedASTJITScalarKind::Float:
				return asTYPEID_FLOAT32;
			case EAngelscriptTypedASTJITScalarKind::Double:
				return asTYPEID_FLOAT64;
			case EAngelscriptTypedASTJITScalarKind::Invalid:
			default:
				return asINVALID_TYPE;
			}
		}

		int32 SetContextArgument(
			asIScriptContext& Context,
			const uint32 ArgumentIndex,
			const FAngelscriptTypedASTJITScalarValue& Argument)
		{
			switch (Argument.Kind)
			{
			case EAngelscriptTypedASTJITScalarKind::Bool:
			case EAngelscriptTypedASTJITScalarKind::Int8:
			case EAngelscriptTypedASTJITScalarKind::UInt8:
				return Context.SetArgByte(
					ArgumentIndex, static_cast<asBYTE>(Argument.Payload));
			case EAngelscriptTypedASTJITScalarKind::Int16:
			case EAngelscriptTypedASTJITScalarKind::UInt16:
				return Context.SetArgWord(
					ArgumentIndex, static_cast<asWORD>(Argument.Payload));
			case EAngelscriptTypedASTJITScalarKind::Int32:
			case EAngelscriptTypedASTJITScalarKind::UInt32:
				return Context.SetArgDWord(
					ArgumentIndex, static_cast<asDWORD>(Argument.Payload));
			case EAngelscriptTypedASTJITScalarKind::Int64:
			case EAngelscriptTypedASTJITScalarKind::UInt64:
				return Context.SetArgQWord(ArgumentIndex, Argument.Payload);
			case EAngelscriptTypedASTJITScalarKind::Float:
			{
				float Value = 0.0f;
				FMemory::Memcpy(&Value, &Argument.Payload, sizeof(Value));
				return Context.SetArgFloat(ArgumentIndex, Value);
			}
			case EAngelscriptTypedASTJITScalarKind::Double:
			{
				double Value = 0.0;
				FMemory::Memcpy(&Value, &Argument.Payload, sizeof(Value));
				return Context.SetArgDouble(ArgumentIndex, Value);
			}
			case EAngelscriptTypedASTJITScalarKind::Void:
			case EAngelscriptTypedASTJITScalarKind::Invalid:
			default:
				return asINVALID_TYPE;
			}
		}

		void ReadContextReturn(
			asIScriptContext& Context,
			const EAngelscriptTypedASTJITScalarKind Kind,
			FAngelscriptTypedASTJITScalarValue& OutValue)
		{
			OutValue = {};
			OutValue.Kind = Kind;
			switch (Kind)
			{
			case EAngelscriptTypedASTJITScalarKind::Bool:
			case EAngelscriptTypedASTJITScalarKind::Int8:
			case EAngelscriptTypedASTJITScalarKind::UInt8:
				OutValue.Payload = Context.GetReturnByte();
				break;
			case EAngelscriptTypedASTJITScalarKind::Int16:
			case EAngelscriptTypedASTJITScalarKind::UInt16:
				OutValue.Payload = Context.GetReturnWord();
				break;
			case EAngelscriptTypedASTJITScalarKind::Int32:
			case EAngelscriptTypedASTJITScalarKind::UInt32:
				OutValue.Payload = Context.GetReturnDWord();
				break;
			case EAngelscriptTypedASTJITScalarKind::Int64:
			case EAngelscriptTypedASTJITScalarKind::UInt64:
				OutValue.Payload = Context.GetReturnQWord();
				break;
			case EAngelscriptTypedASTJITScalarKind::Float:
			{
				const float Value = Context.GetReturnFloat();
				FMemory::Memcpy(&OutValue.Payload, &Value, sizeof(Value));
				break;
			}
			case EAngelscriptTypedASTJITScalarKind::Double:
			{
				const double Value = Context.GetReturnDouble();
				FMemory::Memcpy(&OutValue.Payload, &Value, sizeof(Value));
				break;
			}
			case EAngelscriptTypedASTJITScalarKind::Void:
			case EAngelscriptTypedASTJITScalarKind::Invalid:
			default:
				break;
			}
		}
	}

	const TCHAR* LexToString(
		const EAngelscriptTypedASTJITBridgeResult Result)
	{
		switch (Result)
		{
		case EAngelscriptTypedASTJITBridgeResult::Success:
			return TEXT("Success");
		case EAngelscriptTypedASTJITBridgeResult::ExistingException:
			return TEXT("ExistingException");
		case EAngelscriptTypedASTJITBridgeResult::MissingExecutionEngine:
			return TEXT("MissingExecutionEngine");
		case EAngelscriptTypedASTJITBridgeResult::MissingReferenceTable:
			return TEXT("MissingReferenceTable");
		case EAngelscriptTypedASTJITBridgeResult::MissingReferenceSlot:
			return TEXT("MissingReferenceSlot");
		case EAngelscriptTypedASTJITBridgeResult::WrongReferenceKind:
			return TEXT("WrongReferenceKind");
		case EAngelscriptTypedASTJITBridgeResult::MissingFunction:
			return TEXT("MissingFunction");
		case EAngelscriptTypedASTJITBridgeResult::WrongEngine:
			return TEXT("WrongEngine");
		case EAngelscriptTypedASTJITBridgeResult::UnsupportedFunctionKind:
			return TEXT("UnsupportedFunctionKind");
		case EAngelscriptTypedASTJITBridgeResult::UnsupportedNativeCallingForm:
			return TEXT("UnsupportedNativeCallingForm");
		case EAngelscriptTypedASTJITBridgeResult::MissingNativeAddress:
			return TEXT("MissingNativeAddress");
		case EAngelscriptTypedASTJITBridgeResult::InvalidScalarCarrier:
			return TEXT("InvalidScalarCarrier");
		case EAngelscriptTypedASTJITBridgeResult::SignatureMismatch:
			return TEXT("SignatureMismatch");
		case EAngelscriptTypedASTJITBridgeResult::ContextUnavailable:
			return TEXT("ContextUnavailable");
		case EAngelscriptTypedASTJITBridgeResult::PrepareFailed:
			return TEXT("PrepareFailed");
		case EAngelscriptTypedASTJITBridgeResult::SetArgumentFailed:
			return TEXT("SetArgumentFailed");
		case EAngelscriptTypedASTJITBridgeResult::ExecutionFailed:
			return TEXT("ExecutionFailed");
		default:
			return TEXT("Unknown");
		}
	}

	EAngelscriptTypedASTJITBridgeResult ResolveBoundNative(
		FScriptExecution& Execution,
		const FAngelscriptTypedASTJITBoundCallSite& CallSite,
		const EAngelscriptTypedASTJITScalarKind* ArgumentKinds,
		const uint32 ArgumentCount,
		const EAngelscriptTypedASTJITScalarKind ReturnKind,
		asFUNCTION_t& OutFunction)
	{
		OutFunction = nullptr;
		if (Execution.bExceptionThrown)
		{
			return EAngelscriptTypedASTJITBridgeResult::ExistingException;
		}
		if (Execution.jitEngine == nullptr)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::MissingExecutionEngine);
		}
		if ((ArgumentCount > 0 && ArgumentKinds == nullptr)
			|| ReturnKind == EAngelscriptTypedASTJITScalarKind::Invalid)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::InvalidScalarCarrier);
		}

		const FAngelscriptJITResolvedReferenceTable* const Table =
			FAngelscriptJITExecutionContext::GetReferenceTable(Execution);
		if (Table == nullptr)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::MissingReferenceTable);
		}
		if (CallSite.ReferenceSlotIndex < 0
			|| !Table->Slots.IsValidIndex(CallSite.ReferenceSlotIndex))
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::MissingReferenceSlot);
		}
		const FAngelscriptJITResolvedReferenceSlot& Slot =
			Table->Slots[CallSite.ReferenceSlotIndex];
		if (Slot.SlotIndex != static_cast<uint32>(CallSite.ReferenceSlotIndex))
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::MissingReferenceSlot);
		}
		if (Slot.Kind != EAngelscriptArtifactReferenceKind::EnvironmentSymbol)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::WrongReferenceKind);
		}

		asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
			const_cast<void*>(Slot.Value));
		if (Function == nullptr)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::MissingFunction);
		}
		if (Function->GetEngine() != Execution.jitEngine)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::WrongEngine);
		}
		if (Function->GetFuncType() != asFUNC_SYSTEM
			|| Function->sysFuncIntf == nullptr)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::UnsupportedFunctionKind);
		}

		if (Function->GetParamCount() != ArgumentCount)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::SignatureMismatch);
		}
		for (uint32 ArgumentIndex = 0;
			ArgumentIndex < ArgumentCount; ++ArgumentIndex)
		{
			const int32 ExpectedTypeId = GetAngelScriptTypeId(
				ArgumentKinds[ArgumentIndex]);
			int32 ActualTypeId = asINVALID_TYPE;
			asDWORD Modifiers = asTM_NONE;
			if (ExpectedTypeId < 0
				|| Function->GetParam(
					ArgumentIndex, &ActualTypeId, &Modifiers) < 0
				|| ActualTypeId != ExpectedTypeId
				|| Modifiers != asTM_NONE)
			{
				return Fail(Execution,
					EAngelscriptTypedASTJITBridgeResult::SignatureMismatch);
			}
		}
		asDWORD ReturnModifiers = asTM_NONE;
		if (Function->GetReturnTypeId(&ReturnModifiers)
				!= GetAngelScriptTypeId(ReturnKind)
			|| ReturnModifiers != asTM_NONE)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::SignatureMismatch);
		}

		const asSSystemFunctionInterface& Interface =
			*Function->sysFuncIntf;
		const bool bReviewedCallingConvention =
			Interface.callConv == ICC_CDECL
			|| Interface.callConv == ICC_STDCALL
			;
		if (!bReviewedCallingConvention
			|| Function->DoesReturnOnStack()
			|| Interface.passFirstParamMetaData
				!= asEFirstParamMetaData::None)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::UnsupportedNativeCallingForm);
		}
		if (Interface.func == nullptr)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::MissingNativeAddress);
		}

		OutFunction = Interface.func;
		return EAngelscriptTypedASTJITBridgeResult::Success;
	}

	EAngelscriptTypedASTJITBridgeResult InvokeBoundViaVM(
		FScriptExecution& Execution,
		const FAngelscriptTypedASTJITBoundCallSite& CallSite,
		const FAngelscriptTypedASTJITScalarValue* Arguments,
		const uint32 ArgumentCount,
		const EAngelscriptTypedASTJITScalarKind ReturnKind,
		FAngelscriptTypedASTJITScalarValue* OutReturnValue)
	{
		if (Execution.bExceptionThrown)
		{
			return EAngelscriptTypedASTJITBridgeResult::ExistingException;
		}
		if (Execution.jitEngine == nullptr)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::MissingExecutionEngine);
		}
		if (ArgumentCount > 0 && Arguments == nullptr)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::InvalidScalarCarrier);
		}
		if (ReturnKind == EAngelscriptTypedASTJITScalarKind::Invalid
			|| (ReturnKind == EAngelscriptTypedASTJITScalarKind::Void
				? OutReturnValue != nullptr
				: OutReturnValue == nullptr))
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::InvalidScalarCarrier);
		}

		const FAngelscriptJITResolvedReferenceTable* const Table =
			FAngelscriptJITExecutionContext::GetReferenceTable(Execution);
		if (Table == nullptr)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::MissingReferenceTable);
		}
		if (CallSite.ReferenceSlotIndex < 0
			|| !Table->Slots.IsValidIndex(CallSite.ReferenceSlotIndex))
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::MissingReferenceSlot);
		}
		const FAngelscriptJITResolvedReferenceSlot& Slot =
			Table->Slots[CallSite.ReferenceSlotIndex];
		if (Slot.SlotIndex != static_cast<uint32>(CallSite.ReferenceSlotIndex))
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::MissingReferenceSlot);
		}
		if (Slot.Kind != EAngelscriptArtifactReferenceKind::EnvironmentSymbol)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::WrongReferenceKind);
		}

		asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
			const_cast<void*>(Slot.Value));
		if (Function == nullptr)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::MissingFunction);
		}
		if (Function->GetEngine() != Execution.jitEngine)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::WrongEngine);
		}
		if (Function->GetFuncType() != asFUNC_SYSTEM)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::UnsupportedFunctionKind);
		}

		if (Function->GetParamCount() != ArgumentCount)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::SignatureMismatch);
		}
		for (uint32 ArgumentIndex = 0;
			ArgumentIndex < ArgumentCount;
			++ArgumentIndex)
		{
			const int32 ExpectedTypeId = GetAngelScriptTypeId(
				Arguments[ArgumentIndex].Kind);
			int32 ActualTypeId = asINVALID_TYPE;
			asDWORD Modifiers = asTM_NONE;
			if (ExpectedTypeId < 0
				|| Function->GetParam(
					ArgumentIndex, &ActualTypeId, &Modifiers) < 0
				|| ActualTypeId != ExpectedTypeId
				|| Modifiers != asTM_NONE)
			{
				return Fail(Execution,
					EAngelscriptTypedASTJITBridgeResult::SignatureMismatch);
			}
		}
		asDWORD ReturnModifiers = asTM_NONE;
		if (Function->GetReturnTypeId(&ReturnModifiers)
				!= GetAngelScriptTypeId(ReturnKind)
			|| ReturnModifiers != asTM_NONE)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::SignatureMismatch);
		}

		FAngelscriptContext Context(Execution.jitEngine);
		asIScriptContext* const ScriptContext = Context;
		if (ScriptContext == nullptr)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::ContextUnavailable);
		}
		if (ScriptContext->Prepare(Function) < 0)
		{
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::PrepareFailed);
		}
		for (uint32 ArgumentIndex = 0;
			ArgumentIndex < ArgumentCount;
			++ArgumentIndex)
		{
			if (SetContextArgument(
					*ScriptContext, ArgumentIndex, Arguments[ArgumentIndex]) < 0)
			{
				return Fail(Execution,
					EAngelscriptTypedASTJITBridgeResult::SetArgumentFailed);
			}
		}

		int32 ExecuteResult = asERROR;
		{
			FScopeStaticJITNestedExceptionAdoption ExceptionAdoption(
				*ScriptContext);
			ExecuteResult = ScriptContext->Execute();
		}
		if (Execution.bExceptionThrown
			|| ExecuteResult != asEXECUTION_FINISHED)
		{
			FStaticJITFunction::AdoptContextException(
				Execution, *ScriptContext);
			return Fail(Execution,
				EAngelscriptTypedASTJITBridgeResult::ExecutionFailed);
		}
		if (ReturnKind != EAngelscriptTypedASTJITScalarKind::Void)
		{
			ReadContextReturn(*ScriptContext, ReturnKind, *OutReturnValue);
		}
		return EAngelscriptTypedASTJITBridgeResult::Success;
	}
}
