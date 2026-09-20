#include "as_engine_compile_registration.h"
#include "as_bytecode_linker.h"
#include "as_execution_snapshot.h"
#include "as_module.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"
#include "Containers/Set.h"
#include "Logging/LogMacros.h"

BEGIN_AS_NAMESPACE

namespace
{
	FString SubmittedTypeDisplayName(const asCTypeInfo& Type)
	{
		TArray<FString> Owners;
		for (const asCTypeInfo* Owner = Type.GetDeclarationOwner(); Owner; Owner = Owner->GetDeclarationOwner())
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

	void AttachRegisteredType(asCModule& Shell, asCTypeInfo* Type)
	{
		if (!Type || Type->module) return;
		if (!Type->GetDefinitions())
		{
			Type->AddRefInternal();
		}
		Type->module = &Shell;
		if (asCObjectType* Object = CastToObjectType(Type))
		{
			Shell.AddClassType(Object);
			return;
		}
		if (asCEnumType* Enum = CastToEnumType(Type))
		{
			Shell.AddEnumType(Enum);
			return;
		}
		if (asCCallableType* Callable = CastToCallableType(Type))
		{
			Shell.AddCallableType(Callable);
			return;
		}
		if (asCTypedefType* Typedef = CastToTypedefType(Type))
		{
			Shell.AddTypeDef(Typedef);
		}
	}

	void AttachDeclaredType(asCScriptEngine& Engine, asCModule& Shell, const asSStableKey& Key)
	{
		if (Key.IsZero()) return;
		AttachRegisteredType(Shell, Engine.FindType(Key));
	}

	asERegistrationResult AttachCompileOutputModules(asCScriptEngine& Engine, const asCCompileOutput& Output)
	{
		const TConstArrayView<TSharedRef<FAngelscriptModuleDesc>> Modules = Output.GetDefinitionOutput().GetModules();
		if (Modules.IsEmpty()) return asERegistrationResult::Succeeded;
		for (const TSharedRef<FAngelscriptModuleDesc>& ModuleDesc : Modules)
		{
			if (ModuleDesc->ModuleName.IsEmpty()) continue;
			const FTCHARToUTF8 NameUtf8(*ModuleDesc->ModuleName);
			asCModule* Shell = asNEW(asCModule)(NameUtf8.Get(), &Engine);
			if (!Shell) return asERegistrationResult::InvalidState;
			Engine.scriptModules.PushLast(Shell);
			Engine.scriptModulesByName.Add(Shell);
			ModuleDesc->ScriptModule = Shell;
			for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : ModuleDesc->Classes)
			{
				AttachDeclaredType(Engine, *Shell, ClassDesc->Semantic.StableDeclarationKey);
			}
			for (const TSharedRef<FAngelscriptEnumDesc>& EnumDesc : ModuleDesc->Enums)
			{
				AttachDeclaredType(Engine, *Shell, EnumDesc->Semantic.StableDeclarationKey);
			}
			for (const TSharedRef<FAngelscriptDelegateDesc>& DelegateDesc : ModuleDesc->Delegates)
			{
				AttachDeclaredType(Engine, *Shell, DelegateDesc->Semantic.StableDeclarationKey);
			}
		}
		return asERegistrationResult::Succeeded;
	}

	asERegistrationResult ValidateCompileOutputModuleNames(asCScriptEngine& Engine, const asCCompileOutput& Output)
	{
		TSet<FString> ModuleNames;
		for (const TSharedRef<FAngelscriptModuleDesc>& ModuleDesc : Output.GetDefinitionOutput().GetModules())
		{
			if (ModuleDesc->ModuleName.IsEmpty()) continue;
			if (ModuleNames.Contains(ModuleDesc->ModuleName)) return asERegistrationResult::NameConflict;
			ModuleNames.Add(ModuleDesc->ModuleName);
			const FTCHARToUTF8 NameUtf8(*ModuleDesc->ModuleName);
			if (Engine.GetModule(NameUtf8.Get())) return asERegistrationResult::NameConflict;
		}
		return asERegistrationResult::Succeeded;
	}
}

asERegistrationResult asCEngineCompileRegistration::Register(TArray<TUniquePtr<asCDefinitions>> Sets)
{
	const TMap<const asCTypeInfo*, int> TypeIds;
	const TMap<const asCScriptFunction*, int> FunctionIds;
	return Register(MoveTemp(Sets), TypeIds, FunctionIds);
}

asERegistrationResult asCEngineCompileRegistration::Register(
	TArray<TUniquePtr<asCDefinitions>> Sets,
	const asCCompileOutput& Output)
{
	const asERegistrationResult Validated = ValidateCompileOutputModuleNames(Engine, Output);
	if (Validated != asERegistrationResult::Succeeded) return Validated;
	const asERegistrationResult Installed = Register(MoveTemp(Sets));
	if (Installed != asERegistrationResult::Succeeded) return Installed;
	return AttachCompileOutputModules(Engine, Output);
}

asERegistrationResult asCEngineCompileRegistration::Register(
	TArray<TUniquePtr<asCDefinitions>> Sets,
	const TMap<const asCTypeInfo*, int>& PublishedTypeIds,
	const TMap<const asCScriptFunction*, int>& PublishedFunctionIds)
{
	if (Sets.IsEmpty()) return asERegistrationResult::InvalidArgument;
	TSet<FString> SubmittedNames;
	for (const TUniquePtr<asCDefinitions>& Set : Sets)
	{
		if (!Set || !Set->IsImmutable()) return asERegistrationResult::InvalidArgument;
		for (asCTypeInfo* Type : Set->GetTypes())
		{
			if (!Type) continue;
			const asCObjectType* Object = CastToObjectType(Type);
			if (Object && Object->GetTemplateDefinition()) continue;
			const FString Name = SubmittedTypeDisplayName(*Type);
			if (Name == TEXT("::")) continue;
			if (SubmittedNames.Contains(Name))
			{
				return asERegistrationResult::NameConflict;
			}
			SubmittedNames.Add(Name);
		}
	}
	for (int32 Index = 0; Index < Sets.Num(); ++Index)
	{
		TUniquePtr<asCDefinitions>& Set = Sets[Index];
		TArray<asCScriptFunction*> Functions;
		for (asCScriptFunction* Function : Set->GetFunctions()) Functions.Add(Function);
		TUniquePtr<asCByteCodeImage> ByteCode = Set->TakeEmittedByteCode();
		const asERegistrationResult Installed = (Index + 1 == Sets.Num())
			? Engine.RegisterExternalDefinitions(MoveTemp(Set), PublishedTypeIds, PublishedFunctionIds)
			: Engine.RegisterExternalDefinitions(MoveTemp(Set));
		if (Installed != asERegistrationResult::Succeeded) return Installed;
		if (ByteCode)
		{
			asCExecutableSnapshot* Snapshot = nullptr;
			const asEByteCodeLinkStatus Linked = asLinkByteCodeImage(Engine, *ByteCode, Snapshot);
			if (Linked != asEByteCodeLinkStatus::Succeeded)
			{
				return asERegistrationResult::InvalidLayout;
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
	return asERegistrationResult::Succeeded;
}

END_AS_NAMESPACE
