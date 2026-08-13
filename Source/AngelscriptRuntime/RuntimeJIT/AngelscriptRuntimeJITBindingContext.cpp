#include "RuntimeJIT/AngelscriptRuntimeJITBindingContext.h"

#include "Misc/ScopeLock.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	FCriticalSection& GetRuntimeBindingContextRegistryMutex()
	{
		static FCriticalSection* Mutex = new FCriticalSection();
		return *Mutex;
	}

	TSet<const FAngelscriptRuntimeJITBindingContext*>&
		GetRuntimeBindingContextRegistry()
	{
		static TSet<const FAngelscriptRuntimeJITBindingContext*>* Contexts =
			new TSet<const FAngelscriptRuntimeJITBindingContext*>();
		return *Contexts;
	}
}

FAngelscriptRuntimeJITBindingContext::FAngelscriptRuntimeJITBindingContext(
	const FAngelscriptRuntimeJITPublishedCode& InPublished)
	: Published(InPublished)
{
	FScopeLock Lock(&GetRuntimeBindingContextRegistryMutex());
	GetRuntimeBindingContextRegistry().Add(this);
}

FAngelscriptRuntimeJITBindingContext::~FAngelscriptRuntimeJITBindingContext()
{
	{
		FScopeLock Lock(&GetRuntimeBindingContextRegistryMutex());
		GetRuntimeBindingContextRegistry().Remove(this);
	}
	Magic = 0;
}

bool FAngelscriptRuntimeJITBindingContext::IsValid() const
{
	return Magic == LiveMagic && Published.IsValid();
}

asSJITFunctionBinding FAngelscriptRuntimeJITBindingContext::BuildBinding() const
{
	asSJITFunctionBinding Binding;
	if (!IsValid())
	{
		return Binding;
	}
	Binding.VMEntry = &FAngelscriptRuntimeJITBindingContext::InvokeRuntimeEntry;
	Binding.UserData =
		const_cast<FAngelscriptRuntimeJITBindingContext*>(this);
	return Binding;
}

void FAngelscriptRuntimeJITBindingContext::InvokeRuntimeEntry(
	FScriptExecution& Execution,
	asDWORD* FramePointer,
	asQWORD* OutValue)
{
	const FAngelscriptRuntimeJITBindingContext* const Context =
		FromBindingUserData(Execution.jitBindingUserData);
	if (Context == nullptr || !Context->IsValid()
		|| Context->Published.VMEntry == nullptr)
	{
		return;
	}
	++Context->ExecutionCount;
	Context->Published.VMEntry(Execution, FramePointer, OutValue);
}

const FAngelscriptRuntimeJITBindingContext*
	FAngelscriptRuntimeJITBindingContext::FromBindingUserData(
		const void* UserData)
{
	const FAngelscriptRuntimeJITBindingContext* Context =
		static_cast<const FAngelscriptRuntimeJITBindingContext*>(UserData);
	if (Context == nullptr)
	{
		return nullptr;
	}
	FScopeLock Lock(&GetRuntimeBindingContextRegistryMutex());
	return GetRuntimeBindingContextRegistry().Contains(Context)
		? Context
		: nullptr;
}
