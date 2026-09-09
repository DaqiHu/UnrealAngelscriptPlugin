// Native binding batches for detached metadata. Altered project implementation.
#include "as_scriptengine.h"
#include "as_property.h"

BEGIN_AS_NAMESPACE
asEByteCodeLinkStatus asCScriptEngine::BindNativeFunction(TConstArrayView<asSNativeFunctionBinding> Functions,
	TConstArrayView<asSNativeGlobalBinding> Globals, int& FailedIndex)
{
	FailedIndex = -1;
	asQWORD Generation = 0;
	auto Status = BeginExecutablePublication(Generation);
	if (Status != asEByteCodeLinkStatus::Succeeded) return Status;
	TMap<asSStableKey, std::shared_ptr<asSSystemFunctionInterface>> Candidates;
	TMap<asSStableKey, void*> GlobalCandidates;
	for (int32 Index = 0; Index < Functions.Num(); ++Index)
	{
		FailedIndex = Index;
		const auto& Request = Functions[Index];
		if (!Request.Declaration || !Request.Function.ptr.f.func) return asEByteCodeLinkStatus::InvalidArgument;
		std::shared_ptr<asCScriptFunction> Declaration;
		{
			std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
			if (shuttingDown || vmRetirementRequested || metadataRegistrationRetired) return asEByteCodeLinkStatus::Retired;
			auto* Target = metadataFunctionsByKey.FindRef(Request.Declaration->GetStableKey());
			if (Target != Request.Declaration) return asEByteCodeLinkStatus::MissingDeclaration;
			if (Target->funcType != asFUNC_SYSTEM || Candidates.Contains(Target->GetStableKey())) return asEByteCodeLinkStatus::InvalidArgument;
			Target->AddRef(); Declaration = std::shared_ptr<asCScriptFunction>(Target, [](auto* Value) { Value->Release(); });
		}
		auto* Target = Declaration.get();
		asSSystemFunctionInterface Internal;
		// Recorded method user data is separate from the receiver supplied by the VM.
		void* ConventionAuxiliary = Request.CallConv == asCALL_THISCALL ? nullptr : Request.Auxiliary;
		if (DetectCallingConvention(Target->objectType != nullptr, Request.Function, int(Request.CallConv), ConventionAuxiliary, Request.Caller, &Internal) < 0)
			return asEByteCodeLinkStatus::InvalidArgument;
		Internal.auxiliary = Request.Auxiliary;
		if (Request.BaseOffset != 0) Internal.baseOffset = Request.BaseOffset;
		const bool Generic = Internal.callConv == ICC_GENERIC_FUNC || Internal.callConv == ICC_GENERIC_METHOD;
		if (!Generic && !Request.Caller.IsBound()) return asEByteCodeLinkStatus::InvalidArgument;
		if (Request.FirstParameter != asEFirstParamMetaData::None &&
			(Generic || (Request.FirstParameter == asEFirstParamMetaData::ScriptObjectType && !Target->objectType))) return asEByteCodeLinkStatus::InvalidArgument;
		Internal.passFirstParamMetaData = Request.FirstParameter;
		int Slots = Target->objectType ? 1 : 0;
		if (Request.FirstParameter != asEFirstParamMetaData::None) ++Slots;
		for (asUINT ParameterIndex = 0; ParameterIndex < Target->parameterTypes.GetLength(); ++ParameterIndex)
			Slots += Target->parameterTypes[ParameterIndex].GetTokenType() == ttQuestion ? 2 : 1;
		if (!Generic && (Slots > 32 || (Request.NativeParameterCount >= 0 && Request.NativeParameterCount != Slots))) return asEByteCodeLinkStatus::InvalidArgument;
		if (!Internal.CallFrame.Prepare(Target)) return asEByteCodeLinkStatus::InvalidArgument;
		Internal.paramSize = Internal.CallFrame.ArgumentDwords;
		auto* Storage = asNEW(asSSystemFunctionInterface)(Internal);
		if (!Storage) return asEByteCodeLinkStatus::ResourceFailure;
		std::shared_ptr<asSSystemFunctionInterface> Candidate(Storage, [Declaration](auto* Value) { asDELETE(Value, asSSystemFunctionInterface); });
		if (Generic && PrepareSystemFunctionGeneric(Target, Storage, this) != 0) return asEByteCodeLinkStatus::InvalidArgument;
		Candidates.Add(Target->GetStableKey(), MoveTemp(Candidate));
	}
	// Keep metadata -> publication lock order identical to engine retirement.
	// No callable or global address becomes visible until the entire batch is valid.
	TArray<std::shared_ptr<asSSystemFunctionInterface>> Previous;
	{
		std::lock_guard<std::mutex> MetadataLock(metadataRegistrationMutex);
		std::lock_guard<std::mutex> PublicationLock(executablePublicationMutex);
		if (shuttingDown || vmRetirementRequested || metadataRegistrationRetired || executablePublicationGeneration != Generation) return asEByteCodeLinkStatus::Retired;
		for (int32 Index = 0; Index < Globals.Num(); ++Index)
		{
			FailedIndex = Functions.Num() + Index;
			const auto& Request = Globals[Index];
			if (!Request.Declaration || !Request.Storage) return asEByteCodeLinkStatus::InvalidArgument;
			const auto Key = Request.Declaration->GetStableKey();
			if (metadataGlobalsByKey.FindRef(Key) != Request.Declaration) return asEByteCodeLinkStatus::MissingDeclaration;
			if (GlobalCandidates.Contains(Key)) return asEByteCodeLinkStatus::InvalidArgument;
			GlobalCandidates.Add(Key, Request.Storage);
		}
		for (auto& Entry : Candidates)
		{
			if (auto* Old = nativeInterfaces.Find(Entry.Key)) Previous.Add(*Old);
			nativeInterfaces.Add(Entry.Key, MoveTemp(Entry.Value));
		}
		for (const auto& Entry : GlobalCandidates) metadataGlobalBindings.Add(Entry.Key, Entry.Value);
	}
	FailedIndex = -1;
	return asEByteCodeLinkStatus::Succeeded;
}
END_AS_NAMESPACE
