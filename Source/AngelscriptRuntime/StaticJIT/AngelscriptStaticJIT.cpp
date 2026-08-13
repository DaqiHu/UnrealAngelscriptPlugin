#include "StaticJIT/AngelscriptStaticJIT.h"

#include "StaticJIT/AngelscriptJITBindingContext.h"
#include "StaticJIT/AngelscriptJITImmutableArtifactSet.h"

void FAngelscriptStaticJIT::OnFunctionReady(
	asIScriptFunction* ScriptFunction)
{
	BytecodeGenerator.ObserveFunctionReady(ScriptFunction);

	// Runtime bindings are selected exclusively by the Engine-local Provider
	// router after authoritative compilation. This facade never emits code or
	// reattaches a function by numeric FunctionId.
}

void FAngelscriptStaticJIT::ReleaseFunctionBinding(
	asIScriptFunction*,
	const asSJITFunctionBinding& Binding)
{
	if (Binding.UserData != nullptr)
	{
		FScopeLock Lock(&ProviderBindingMutex);
		ActiveProviderBindingContexts.Remove(Binding.UserData);
	}
}

bool FAngelscriptStaticJIT::PublishProviderSelection(
	asIScriptFunction* ScriptFunction,
	const FAngelscriptJITArtifactSelection& Selection,
	const FAngelscriptJITImmutableArtifactSetValidationResult*
		ImmutableValidation)
{
	if (ScriptFunction == nullptr
		|| ScriptFunction->GetEngine() == nullptr
		|| ScriptFunction->GetEngine()->GetJITCompiler() != this)
	{
		return false;
	}
	if (!Selection.Provider.IsValid()
		|| !Selection.Provider->Entries.IsValidIndex(Selection.EntryIndex))
	{
		return false;
	}
	const FAngelscriptJITProviderCatalogEntry& Entry =
		Selection.Provider->Entries[Selection.EntryIndex];
	if (EnumHasAnyFlags(
			Entry.Flags,
			EAngelscriptJITArtifactEntryFlags::ImmutableDirectCallSet)
		&& (ImmutableValidation == nullptr
			|| !ImmutableValidation->AllowsDirectSelection(Selection)))
	{
		return false;
	}
	TSharedRef<const FAngelscriptJITBindingContext, ESPMode::ThreadSafe>
		Context = MakeShared<FAngelscriptJITBindingContext,
			ESPMode::ThreadSafe>(Selection, ImmutableValidation);
	const asSJITFunctionBinding Binding = Context->BuildBinding();
	if (!Context->IsValid()
		|| (Binding.VMEntry == nullptr
			&& Binding.RawEntry == nullptr
			&& Binding.ParmsEntry == nullptr))
	{
		return false;
	}
	{
		FScopeLock Lock(&ProviderBindingMutex);
		ActiveProviderBindingContexts.Add(Binding.UserData, Context);
	}
	if (ScriptFunction->SetJITBinding(Binding) != asSUCCESS)
	{
		FScopeLock Lock(&ProviderBindingMutex);
		ActiveProviderBindingContexts.Remove(Binding.UserData);
		return false;
	}
	return true;
}

bool FAngelscriptStaticJIT::ClearProviderSelection(
	asIScriptFunction* ScriptFunction)
{
	if (ScriptFunction == nullptr
		|| ScriptFunction->GetEngine() == nullptr
		|| ScriptFunction->GetEngine()->GetJITCompiler() != this)
	{
		return false;
	}
	const asSJITFunctionBinding Current = ScriptFunction->GetJITBinding();
	if (FAngelscriptJITBindingContext::FromBindingUserData(
		Current.UserData) == nullptr)
	{
		return false;
	}
	return ScriptFunction->SetJITBinding(asSJITFunctionBinding())
		== asSUCCESS;
}
