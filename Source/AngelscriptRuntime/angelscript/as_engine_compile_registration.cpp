#include "as_engine_compile_registration.h"
#include "as_bytecode_linker.h"
#include "as_execution_snapshot.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"
#include "Containers/Set.h"

BEGIN_AS_NAMESPACE

namespace
{
	FString SubmittedTypeDisplayName(const asCTypeInfo& Type)
	{
		TArray<FString> Owners;
		for (const asCTypeInfo* Owner = Type.GetMetadataDeclarationOwner(); Owner; Owner = Owner->GetMetadataDeclarationOwner())
		{
			Owners.Insert(UTF8_TO_TCHAR(Owner->GetName()), 0);
		}
		FString Scope = UTF8_TO_TCHAR(Type.GetNamespace());
		if (!Owners.IsEmpty())
		{
			if (!Scope.IsEmpty()) Scope += TEXT("::");
			Scope += FString::Join(Owners, TEXT("::"));
		}
		return Scope + TEXT("::") + UTF8_TO_TCHAR(Type.GetName());
	}
}

asEMetadataRegistrationResult asCEngineCompileRegistration::Register(TArray<TUniquePtr<asCModuleDefinitionSet>> Sets)
{
	const TMap<const asCTypeInfo*, int> TypeIds;
	const TMap<const asCScriptFunction*, int> FunctionIds;
	return Register(MoveTemp(Sets), TypeIds, FunctionIds);
}

asEMetadataRegistrationResult asCEngineCompileRegistration::Register(
	TArray<TUniquePtr<asCModuleDefinitionSet>> Sets,
	const TMap<const asCTypeInfo*, int>& PublishedTypeIds,
	const TMap<const asCScriptFunction*, int>& PublishedFunctionIds)
{
	if (Sets.IsEmpty()) return asEMetadataRegistrationResult::InvalidArgument;
	TSet<FString> SubmittedNames;
	for (const TUniquePtr<asCModuleDefinitionSet>& Set : Sets)
	{
		if (!Set || !Set->IsImmutable()) return asEMetadataRegistrationResult::InvalidArgument;
		for (asCTypeInfo* Type : Set->GetTypes())
		{
			if (!Type) continue;
			const asCObjectType* Object = CastToObjectType(Type);
			if (Object && Object->GetMetadataTemplateDefinition()) continue;
			const FString Name = SubmittedTypeDisplayName(*Type);
			if (Name == TEXT("::")) continue;
			if (SubmittedNames.Contains(Name))
			{
				return asEMetadataRegistrationResult::NameConflict;
			}
			SubmittedNames.Add(Name);
		}
	}
	for (int32 Index = 0; Index < Sets.Num(); ++Index)
	{
		TUniquePtr<asCModuleDefinitionSet>& Set = Sets[Index];
		TArray<asCScriptFunction*> Functions;
		for (asCScriptFunction* Function : Set->GetFunctions()) Functions.Add(Function);
		TUniquePtr<asCByteCodeImage> ByteCode = Set->TakeEmittedByteCode();
		const asEMetadataRegistrationResult Installed = (Index + 1 == Sets.Num())
			? Engine.InstallDefinitionSet(MoveTemp(Set), PublishedTypeIds, PublishedFunctionIds)
			: Engine.InstallDefinitionSet(MoveTemp(Set));
		if (Installed != asEMetadataRegistrationResult::Succeeded) return Installed;
		if (ByteCode)
		{
			asCExecutableSnapshot* Snapshot = nullptr;
			if (asLinkByteCodeImage(Engine, *ByteCode, Snapshot) != asEByteCodeLinkStatus::Succeeded)
			{
				return asEMetadataRegistrationResult::InvalidLayout;
			}
			if (Snapshot) Snapshot->Release();
		}
		for (asCScriptFunction* Function : Functions)
		{
			if (!Function || Function->funcType != asFUNC_SCRIPT) continue;
			asCExecutableFunction* Executable = Engine.FindPublishedExecutable(Function->GetStableKey());
			if (!Executable || !Executable->scriptData) continue;
			Function->AllocateScriptFunctionData();
			Function->scriptData->byteCode = Executable->scriptData->byteCode;
			Function->scriptData->stackNeeded = Executable->scriptData->stackNeeded;
			Function->scriptData->variableSpace = Executable->scriptData->variableSpace;
			Function->scriptData->objVariablePos = Executable->scriptData->objVariablePos;
			Function->scriptData->objVariableTypes = Executable->scriptData->objVariableTypes;
			Function->scriptData->objVariableInfo = Executable->scriptData->objVariableInfo;
			Function->scriptData->objVariablesOnHeap = Executable->scriptData->objVariablesOnHeap;
		}
	}
	return asEMetadataRegistrationResult::Succeeded;
}

END_AS_NAMESPACE
