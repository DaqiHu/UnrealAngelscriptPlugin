#include "RuntimeJIT/AngelscriptRuntimeJITBindingContext.h"

#include "Misc/ScopeLock.h"

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
	Binding.VMEntry = Published.VMEntry;
	Binding.UserData =
		const_cast<FAngelscriptRuntimeJITBindingContext*>(this);
	return Binding;
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
