#include "StaticJIT/StaticJITHeader.h"
#include "AngelscriptStaticJIT.h"
#include "Artifacts/AngelscriptFunctionRoute.h"
#include "ClassGenerator/ASClass.h"
#include "StaticJIT/AngelscriptJITBindingContext.h"
#include "StaticJIT/AngelscriptJITExecutionContext.h"
#include "UObject/Class.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_scriptengine.h"
//#include "as_scriptobject.h"
//#include "as_callfunc.h"
//#include "as_generic.h"
//#include "as_texts.h"
#include "as_context.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptobject.h"
#include "as_thread.h"
#include "as_callfunc.h"
#include "as_generic.h"
#include "as_texts.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	void CaptureFirstJITFailureStack(FScriptExecution& Execution)
	{
#if AS_JIT_DEBUG_CALLSTACKS
		asSJITFailureRecord* const FailureRecord = Execution.FailureRecord;
		if (FailureRecord == nullptr
			|| !FailureRecord->bHasFailure
			|| FailureRecord->CapturedStackFrames.GetLength() != 0)
		{
			return;
		}

		auto AppendExecution = [FailureRecord](FScriptExecution* ActiveExecution)
		{
			if (ActiveExecution == nullptr)
			{
				return;
			}

			const FScopeJITDebugCallstack* DebugFrame =
				static_cast<const FScopeJITDebugCallstack*>(
					ActiveExecution->debugCallStack);
			while (DebugFrame != nullptr)
			{
				asCString Frame;
				Frame.Format("  %s | Line %d",
					DebugFrame->FunctionName != nullptr
						? DebugFrame->FunctionName : "",
					DebugFrame->LineNumber);
				FailureRecord->CapturedStackFrames.PushLast(Frame);
				if (FailureRecord->CapturedStackModule.GetLength() == 0
					&& DebugFrame->Filename != nullptr)
				{
					FailureRecord->CapturedStackModule = DebugFrame->Filename;
				}
				DebugFrame = DebugFrame->PrevFrame;
			}
		};

		auto AppendContext = [FailureRecord](asCContext* Context)
		{
			if (Context == nullptr)
			{
				return;
			}

			const asUINT FrameCount = FMath::Min(
				Context->GetCallstackSize(), asUINT(64));
			for (asUINT FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
			{
				asIScriptFunction* const ScriptFunction =
					Context->GetFunction(FrameIndex);
				if (ScriptFunction == nullptr)
				{
					continue;
				}

				int Column = 0;
				const int Line = Context->GetLineNumber(
					FrameIndex, &Column, nullptr);
				asCString Frame;
				Frame.Format("  %s | Line %d | Col %d",
					ScriptFunction->GetDeclaration(
						true, false, false, true),
					Line,
					Column);
				FailureRecord->CapturedStackFrames.PushLast(Frame);
				if (FailureRecord->CapturedStackModule.GetLength() == 0)
				{
					FailureRecord->CapturedStackModule =
						TCHAR_TO_UTF8(*static_cast<asCScriptFunction*>(ScriptFunction)->GetStableKey().ToHexString());
				}
			}
		};

		if (Execution.tld != nullptr
			&& Execution.tld->activeActivation != nullptr)
		{
			for (FScriptActivation* Activation =
					Execution.tld->activeActivation;
				Activation != nullptr;
				Activation = Activation->Previous)
			{
				if (Activation->Execution != nullptr)
				{
					AppendExecution(Activation->Execution);
				}
				else
				{
					AppendContext(Activation->Context);
				}
			}
		}
		else
		{
			asCContext* PreviousContext = nullptr;
			for (FScriptExecution* ActiveExecution = &Execution;
				ActiveExecution != nullptr;
				ActiveExecution = ActiveExecution->prevExecution)
			{
				AppendExecution(ActiveExecution);
				PreviousContext = ActiveExecution->prevContext;
			}
			AppendContext(PreviousContext);
		}
#else
		static_cast<void>(Execution);
#endif
	}

	bool TryResolveStableSectionName(
		FAngelscriptEngine& RuntimeEngine,
		asIScriptFunction* Function,
		const ANSICHAR* SectionName,
		FString& OutSectionName)
	{
		OutSectionName.Reset();
		if (Function == nullptr || static_cast<asCScriptFunction*>(Function)->module == nullptr
			|| SectionName == nullptr || SectionName[0] == '\0')
		{
			return false;
		}

		const TSharedPtr<FAngelscriptModuleDesc> Module =
			RuntimeEngine.GetModule(static_cast<asCScriptFunction*>(Function)->module);
		if (!Module.IsValid())
		{
			return false;
		}

		auto TryMatchSection = [&Module, &OutSectionName](
			const FStringView SourceSection)
		{
			for (const FAngelscriptModuleDesc::FCodeSection& CodeSection
				: Module->Code)
			{
				if (SourceSection.Equals(CodeSection.AbsoluteFilename,
						ESearchCase::CaseSensitive)
					|| SourceSection.Equals(CodeSection.VirtualPath,
						ESearchCase::CaseSensitive)
					|| SourceSection.Equals(CodeSection.RelativeFilename,
						ESearchCase::CaseSensitive)
					|| (!CodeSection.AbsoluteFilename.IsEmpty()
						&& SourceSection.Equals(CodeSection.AbsoluteFilename,
							ESearchCase::IgnoreCase)))
				{
					OutSectionName = CodeSection.VirtualPath;
					return !OutSectionName.IsEmpty();
				}
			}
			return false;
		};

		if (TryMatchSection(FStringView(ANSI_TO_TCHAR(SectionName))))
		{
			return true;
		}

		const ANSICHAR* const FunctionSectionName =
			Function->GetScriptSectionName();
		return FunctionSectionName != nullptr
			&& FunctionSectionName[0] != '\0'
			&& TryMatchSection(FStringView(
				ANSI_TO_TCHAR(FunctionSectionName)));
	}

	bool RecordJITException(
		FScriptExecution& Execution,
		const ANSICHAR* Message,
		const ANSICHAR* RouteOrigin,
		asIScriptFunction* FunctionOverride = nullptr,
		const ANSICHAR* SectionOverride = nullptr,
		const int32 LineOverride = 0)
	{
		asIScriptFunction* const Function = FunctionOverride != nullptr
			? FunctionOverride
			: (Execution.tld != nullptr
				&& Execution.tld->activeFunction != nullptr
					? Execution.tld->activeFunction
					: Execution.jitFunction);
		FString StableIdentity;
		FAngelscriptEngine* const RuntimeEngine =
			FAngelscriptEngine::TryGetByScriptEngine(Execution.jitEngine);
		if (RuntimeEngine != nullptr)
		{
			FAngelscriptStableFunctionKey FunctionKey;
			if (RuntimeEngine->TryResolveStableFunctionKey(
				Function, FunctionKey))
			{
				StableIdentity = FunctionKey.Hash.ToHexString();
			}
		}

		FString BackendOrigin(TEXT("static-jit"));
		if (const FAngelscriptJITBindingContext* const BindingContext =
			FAngelscriptJITBindingContext::FromBindingUserData(
				Execution.jitBindingUserData))
		{
			const TSharedPtr<const FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>& Provider = BindingContext->GetProvider();
			if (Provider.IsValid() && !Provider->ProviderName.IsEmpty())
			{
				BackendOrigin = Provider->ProviderName;
			}
		}

		int32 Line = LineOverride;
		int32 Column = 0;
		const char* SectionName = SectionOverride;
		if (SectionName == nullptr && Execution.prevContext != nullptr)
		{
			const int32 ContextLine = Execution.prevContext->GetLineNumber(
				0, &Column, &SectionName);
			if (Line <= 0 && ContextLine > 0)
			{
				Line = ContextLine;
			}
		}
#if AS_JIT_DEBUG_CALLSTACKS
		if (const FScopeJITDebugCallstack* const DebugFrame =
			static_cast<const FScopeJITDebugCallstack*>(
				Execution.debugCallStack))
		{
			Line = DebugFrame->LineNumber;
			Column = DebugFrame->ColumnNumber;
			if (DebugFrame->Filename != nullptr
				&& DebugFrame->Filename[0] != '\0')
			{
				SectionName = DebugFrame->Filename;
			}
		}
#endif

		FString StableSectionName;
		if (RuntimeEngine != nullptr)
		{
			TryResolveStableSectionName(
				*RuntimeEngine, Function, SectionName, StableSectionName);
		}

		FTCHARToUTF8 StableIdentityUtf8(*StableIdentity);
		FTCHARToUTF8 StableSectionNameUtf8(*StableSectionName);
		FTCHARToUTF8 BackendOriginUtf8(*BackendOrigin);
		// A context-owned entry publishes through that context so its configured
		// exception callback observes the same one-shot event as VM execution.
		// Context-free raw/parameter entries have no later publication point and
		// therefore report the primary failure here.
		const bool bReportImmediately =
			!Execution.HasContextExceptionPublisher();
		if (!Execution.TryRecordException(
			Message,
			Function,
			StableIdentityUtf8.Get(),
			!StableSectionName.IsEmpty()
				? StableSectionNameUtf8.Get() : SectionName,
			Line,
			Column,
			RouteOrigin,
			BackendOriginUtf8.Get(),
			bReportImmediately))
		{
			return false;
		}
		CaptureFirstJITFailureStack(Execution);

		if (bReportImmediately)
		{
			FAngelscriptEngine::HandleExceptionFromJIT(Message);
		}
		return true;
	}
}

FScopeTypedASTJITExecutionFrame::FScopeTypedASTJITExecutionFrame(
	FScriptExecution& InExecution,
	asIScriptFunction* InFunction,
	const ANSICHAR* InFilename,
	const ANSICHAR* InFunctionName,
	const int32 InLineNumber,
	void* InThisObject,
	const bool bPublishSourcePosition)
	: Execution(InExecution)
	, Function(InFunction)
#if AS_JIT_DEBUG_CALLSTACKS
	, DebugFrame(
		InExecution,
		InFilename,
		InFunctionName,
		InLineNumber,
		InThisObject,
		bPublishSourcePosition)
#endif
{
	if (Execution.tld != nullptr)
	{
		PreviousActiveFunction = Execution.tld->activeFunction;
		if (Function != nullptr)
		{
			Execution.tld->activeFunction = Function;
		}
	}

	bActive = Execution.TryEnterNativeJITFrame();
	if (!bActive)
	{
		RecordJITException(
			Execution,
			"Stack overflow: potential infinite recursion detected?",
			"typed-ast-recursion-budget",
			Function,
			InFilename,
			InLineNumber);
	}
}

FScopeTypedASTJITExecutionFrame::~FScopeTypedASTJITExecutionFrame()
{
	if (bActive)
	{
		Execution.LeaveNativeJITFrame();
	}
	if (Execution.tld != nullptr)
	{
		Execution.tld->activeFunction = PreviousActiveFunction;
	}
}

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
			&& Slot->Kind != EAngelscriptArtifactReferenceKind::ScriptImport
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

FScopeStaticJITNestedExceptionAdoption::
	FScopeStaticJITNestedExceptionAdoption(asIScriptContext& InContext)
	: Context(static_cast<asCContext&>(InContext))
	, bPreviousExceptionCallback(Context.m_exceptionCallback)
	, PreviousExceptionCallbackFunction(Context.m_exceptionCallbackFunc)
	, PreviousExceptionCallbackObject(Context.m_exceptionCallbackObj)
{
	Context.m_exceptionCallback = false;
}

FScopeStaticJITNestedExceptionAdoption::
	~FScopeStaticJITNestedExceptionAdoption()
{
	Context.m_exceptionCallback = bPreviousExceptionCallback;
	Context.m_exceptionCallbackFunc = PreviousExceptionCallbackFunction;
	Context.m_exceptionCallbackObj = PreviousExceptionCallbackObject;
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

void FStaticJITFunction::AdoptContextException(
	FScriptExecution& Execution,
	asIScriptContext& Context)
{
	const bool bAdopted = Execution.AdoptException(Context);
	if (Execution.FailureRecord == nullptr
		|| !Execution.FailureRecord->bHasFailure)
	{
		return;
	}

	if (bAdopted)
	{
		asIScriptFunction* const Function = Context.GetExceptionFunction();
		FAngelscriptEngine* const RuntimeEngine =
			FAngelscriptEngine::TryGetByScriptEngine(Execution.jitEngine);
		if (RuntimeEngine != nullptr)
		{
			FAngelscriptStableFunctionKey FunctionKey;
			if (RuntimeEngine->TryResolveStableFunctionKey(Function, FunctionKey))
			{
				FTCHARToUTF8 StableIdentity(*FunctionKey.Hash.ToHexString());
				Execution.FailureRecord->StableFunctionIdentity =
					StableIdentity.Get();
			}

			FString StableSectionName;
			if (TryResolveStableSectionName(
				*RuntimeEngine,
				Function,
				Execution.FailureRecord->SectionName.AddressOf(),
				StableSectionName))
			{
				FTCHARToUTF8 StableSectionNameUtf8(*StableSectionName);
				Execution.FailureRecord->SectionName =
					StableSectionNameUtf8.Get();
			}
		}
	}

	// A public caller publishes the shared first failure through its own
	// configured callback.  Context-free Raw/Parms/bridge calls have no later
	// publication point, including when a nested JIT entry populated the record
	// before this adoption attempt, so report that still-unreported failure here.
	if (!Execution.HasContextExceptionPublisher()
		&& !Execution.FailureRecord->bReported)
	{
		Execution.FailureRecord->bReported = true;
		FAngelscriptEngine::HandleExceptionFromJIT(
			Execution.FailureRecord->Message.AddressOf());
	}
}

void FStaticJITFunction::SetExternalException(
	FScriptExecution& Execution,
	const ANSICHAR* Message)
{
	RecordJITException(Execution, Message, "generated-native-call");
}

void FStaticJITFunction::SetException(FScriptExecution& Execution, EJITException Exception)
{
	if (Exception == EJITException::NullPointer)
		SetExternalException(Execution, TXT_NULL_POINTER_ACCESS);
	else if (Exception == EJITException::Div0)
		SetExternalException(Execution, TXT_DIVIDE_BY_ZERO);
	else if (Exception == EJITException::Overflow)
		SetExternalException(Execution, TXT_DIVIDE_OVERFLOW);
	else if (Exception == EJITException::UnboundFunction)
		SetExternalException(Execution, TXT_UNBOUND_FUNCTION);
	else if (Exception == EJITException::OutOfBounds)
		SetExternalException(Execution, "Index out of bounds.");
	else
		SetExternalException(Execution, "Unknown exception.");
}

void FStaticJITFunction::SetNullPointerException(FScriptExecution& Execution)
{
	SetExternalException(Execution, TXT_NULL_POINTER_ACCESS);
}

void FStaticJITFunction::SetDivByZeroException(FScriptExecution& Execution)
{
	SetExternalException(Execution, TXT_DIVIDE_BY_ZERO);
}

void FStaticJITFunction::SetOverflowException(FScriptExecution& Execution)
{
	SetExternalException(Execution, TXT_DIVIDE_OVERFLOW);
}

void FStaticJITFunction::SetPowerOverflowException(FScriptExecution& Execution)
{
	SetExternalException(Execution, TXT_POW_OVERFLOW);
}

void FStaticJITFunction::SetUnboundFunctionException(FScriptExecution& Execution)
{
	SetExternalException(Execution, TXT_UNBOUND_FUNCTION);
}

void FStaticJITFunction::SetOutOfBoundsException(FScriptExecution& Execution)
{
	SetExternalException(Execution, "Index out of bounds.");
}

void FStaticJITFunction::SetSwitchValueInvalidException(FScriptExecution& Execution)
{
	SetExternalException(Execution, "Invalid enum value passed to switch");
}

void FStaticJITFunction::SetUnknownException(FScriptExecution& Execution)
{
	SetExternalException(Execution, "Unknown exception.");
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

void AngelscriptDestroyScriptObjectIsolated(
	FScriptExecution& OuterExecution,
	asIScriptEngine* Engine,
	asITypeInfo* ObjectType,
	void* Object)
{
	// The outer execution already owns the primary exception. This helper
	// must not adopt a destructor failure into that record, and must not
	// reuse the JIT-active context (its generated prolog assumes no
	// incoming exception).
	(void)OuterExecution;
	if (Object == nullptr || ObjectType == nullptr)
	{
		return;
	}

	asCObjectType* const TypedObjectType =
		static_cast<asCObjectType*>(ObjectType);
	if (TypedObjectType->beh.destruct == 0)
	{
		return;
	}

	asCScriptEngine* const ScriptEngine = Engine != nullptr
		? static_cast<asCScriptEngine*>(Engine)
		: TypedObjectType->engine;
	if (ScriptEngine == nullptr
		|| TypedObjectType->beh.destruct
			>= static_cast<int>(ScriptEngine->scriptFunctions.GetLength()))
	{
		return;
	}

	asCScriptFunction* const Destructor =
		ScriptEngine->scriptFunctions[TypedObjectType->beh.destruct];
	if (Destructor == nullptr)
	{
		return;
	}

	if (Destructor->sysFuncIntf != nullptr)
	{
		ScriptEngine->CallObjectMethod(Object, TypedObjectType->beh.destruct);
		return;
	}

	asIScriptContext* const IsolatedContext = ScriptEngine->RequestContext();
	if (IsolatedContext == nullptr)
	{
		return;
	}

	asCContext* const IsolatedScriptContext =
		static_cast<asCContext*>(IsolatedContext);
	IsolatedScriptContext->ClearExceptionCallback();

	asCThreadLocalData* const ThreadLocalData =
		asCThreadManager::GetLocalData();
	FScriptExecution* SavedExecution = nullptr;
	asCContext* SavedContext = nullptr;
	if (ThreadLocalData != nullptr)
	{
		SavedExecution = ThreadLocalData->activeExecution;
		SavedContext = ThreadLocalData->activeContext;
		ThreadLocalData->activeExecution = nullptr;
		ThreadLocalData->activeContext = IsolatedScriptContext;
	}

	if (IsolatedContext->Prepare(Destructor) >= 0)
	{
		IsolatedContext->SetObject(Object);
		IsolatedContext->Execute();
	}

	if (ThreadLocalData != nullptr)
	{
		ThreadLocalData->activeExecution = SavedExecution;
		ThreadLocalData->activeContext = SavedContext;
	}
	ScriptEngine->ReturnContext(IsolatedContext);
}
