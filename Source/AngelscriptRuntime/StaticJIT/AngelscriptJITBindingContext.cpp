#include "StaticJIT/AngelscriptJITBindingContext.h"

#include "StaticJIT/AngelscriptJITImmutableArtifactSet.h"

#include "Misc/ScopeLock.h"

namespace
{
	// Process-lifetime storage avoids static-destruction ordering between a late
	// Binding release and the Runtime module's context-recognition state.
	FCriticalSection& GetBindingContextRegistryMutex()
	{
		static FCriticalSection* Mutex = new FCriticalSection();
		return *Mutex;
	}

	TSet<const FAngelscriptJITBindingContext*>& GetBindingContextRegistry()
	{
		static TSet<const FAngelscriptJITBindingContext*>* Contexts =
			new TSet<const FAngelscriptJITBindingContext*>();
		return *Contexts;
	}
}

FAngelscriptJITBindingContext::FAngelscriptJITBindingContext(
	const FAngelscriptJITArtifactSelection& InSelection,
	const FAngelscriptJITImmutableArtifactSetValidationResult*
		InImmutableValidation)
	: Selection(InSelection)
	, bImmutableCookedDirectDispatch(
		InImmutableValidation != nullptr
		&& InImmutableValidation->AllowsDirectSelection(InSelection))
{
	FScopeLock Lock(&GetBindingContextRegistryMutex());
	GetBindingContextRegistry().Add(this);
}

FAngelscriptJITBindingContext::~FAngelscriptJITBindingContext()
{
	{
		FScopeLock Lock(&GetBindingContextRegistryMutex());
		GetBindingContextRegistry().Remove(this);
	}
	Magic = 0;
}

bool FAngelscriptJITBindingContext::IsValid() const
{
	return Magic == LiveMagic
		&& Selection.Provider.IsValid()
		&& Selection.Provider->Entries.IsValidIndex(Selection.EntryIndex)
		&& Selection.ReferenceTable.IsValid();
}

asSJITFunctionBinding FAngelscriptJITBindingContext::BuildBinding() const
{
	asSJITFunctionBinding Binding;
	if (!IsValid())
	{
		return Binding;
	}
	const FAngelscriptJITProviderCatalogEntry& Entry =
		Selection.Provider->Entries[Selection.EntryIndex];
	Binding.VMEntry = Entry.EntryPoints.VMEntry;
	Binding.RawEntry = Entry.EntryPoints.RawEntry;
	Binding.ParmsEntry = Entry.EntryPoints.ParmsEntry;
	Binding.UserData = const_cast<FAngelscriptJITBindingContext*>(this);
	return Binding;
}

void FAngelscriptJITBindingContext::MarkExecution(const EEntryKind Kind) const
{
	switch (Kind)
	{
	case EEntryKind::VM:
		++VMExecutionCount;
		break;
	case EEntryKind::Raw:
		++RawExecutionCount;
		break;
	case EEntryKind::Parms:
		++ParmsExecutionCount;
		break;
	}
}

uint64 FAngelscriptJITBindingContext::GetExecutionCount(
	const EEntryKind Kind) const
{
	switch (Kind)
	{
	case EEntryKind::VM:
		return VMExecutionCount.Load();
	case EEntryKind::Raw:
		return RawExecutionCount.Load();
	case EEntryKind::Parms:
		return ParmsExecutionCount.Load();
	default:
		return 0;
	}
}

const FAngelscriptJITBindingContext*
FAngelscriptJITBindingContext::FromBindingUserData(const void* UserData)
{
	const FAngelscriptJITBindingContext* Context =
		static_cast<const FAngelscriptJITBindingContext*>(UserData);
	if (Context == nullptr)
	{
		return nullptr;
	}
	FScopeLock Lock(&GetBindingContextRegistryMutex());
	return GetBindingContextRegistry().Contains(Context) ? Context : nullptr;
}
