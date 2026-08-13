#include "AngelscriptStaticJIT.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheEnvironmentProfile.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "StaticJIT/PrecompiledData.h"
#include "StaticJIT/AngelscriptJITBindingContext.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITImmutableArtifactSet.h"
#include "StaticJIT/AngelscriptJITProviderRouter.h"
#include "StaticJIT/AngelscriptJITReferenceResolver.h"
#include "StaticJIT/StaticJITHeader.h"

#include "AngelscriptBytecodes.h"
#include "AngelscriptEngine.h"

#include "StartAngelscriptHeaders.h"
//#include "as_datatype.h"
//#include "as_scriptfunction.h"
//#include "as_module.h"
//#include "as_scriptengine.h"
//#include "as_objecttype.h"
#include "source/as_datatype.h"
#include "source/as_scriptfunction.h"
#include "source/as_module.h"
#include "source/as_property.h"
#include "source/as_scriptengine.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

asCString GetScriptClassName(asITypeInfo* Type)
{
	asCString Str = Type->GetName();

	int32 SubTypeCount = Type->GetSubTypeCount();
	if (SubTypeCount != 0)
	{
		Str += "<";
		for (int32 i = 0; i < SubTypeCount; ++i)
		{
			if (i != 0)
				Str += ",";
			Str += FAngelscriptEngine::Get().Engine->GetTypeDeclaration(Type->GetSubTypeId(i));
		}
		Str += ">";
	}
	return Str;
}

void FAngelscriptStaticJIT::OnFunctionReady(asIScriptFunction* ScriptFunction)
{
#if AS_CAN_GENERATE_JIT
	// In generate mode, output the C++ that represents the script function
	if (bGenerateOutputCode)
	{
		FunctionsToGenerate.Add((asCScriptFunction*)ScriptFunction, FGenerateFunction());
		return;
	}
#endif

	// Runtime bindings are selected exclusively by the engine-local Provider
	// router after authoritative compilation. This compiler is only an
	// observer when generating C++ and never reattaches by numeric FunctionId.
}

void FAngelscriptStaticJIT::ReleaseFunctionBinding(
	asIScriptFunction*,
	const asSJITFunctionBinding& Binding)
{
	// Provider bindings retire only the exact immutable context stored in
	// UserData.
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

bool FAngelscriptStaticJIT::CanGenerateParmsEntry(
	const asIScriptFunction* ScriptFunction)
{
	if (ScriptFunction == nullptr
		|| ScriptFunction->GetFuncType() != asFUNC_SCRIPT)
	{
		return false;
	}
	const asCScriptFunction* const Function =
		static_cast<const asCScriptFunction*>(ScriptFunction);
	for (asUINT ParameterIndex = 0;
		ParameterIndex < Function->parameterTypes.GetLength();
		++ParameterIndex)
	{
		const asCDataType& DataType =
			Function->parameterTypes[ParameterIndex];
		if ((DataType.IsObject() || DataType.IsReference())
			&& DataType.GetSizeInMemoryBytes() < 4
			&& !DataType.IsObject())
		{
			return false;
		}
	}
	return true;
}

#if AS_CAN_GENERATE_JIT
void FAngelscriptStaticJIT::CollectModuleFunctionsForGeneration(asIScriptModule* Module)
{
	asCModule* const ScriptModule = reinterpret_cast<asCModule*>(Module);
	if (ScriptModule == nullptr)
	{
		return;
	}

	asCScriptEngine* const ModuleEngine =
		reinterpret_cast<asCScriptEngine*>(ScriptModule->GetEngine());
	if (Engine == nullptr)
	{
		Engine = ModuleEngine;
	}
	else
	{
		check(Engine == ModuleEngine);
	}

	for (asUINT Index = 0; Index < ScriptModule->scriptFunctions.GetLength(); ++Index)
	{
		asCScriptFunction* const Function = ScriptModule->scriptFunctions[Index];
		if (Function != nullptr
			&& Function->funcType == asFUNC_SCRIPT
			&& Function->scriptData != nullptr)
		{
			FunctionsToGenerate.FindOrAdd(Function);
		}
	}
}

const FString STANDARD_HEADER =
TEXT("#include \"StaticJIT/StaticJITConfig.h\"\n")
#if UE_BUILD_DEBUG
TEXT("#if UE_BUILD_DEBUG\n")
#elif UE_BUILD_DEVELOPMENT
TEXT("#if UE_BUILD_DEVELOPMENT\n")
#elif UE_BUILD_TEST
TEXT("#if UE_BUILD_TEST\n")
#elif UE_BUILD_SHIPPING
TEXT("#if UE_BUILD_SHIPPING\n")
#endif
TEXT("#ifndef AS_SKIP_JITTED_CODE\n\n")
;

const FString STANDARD_INCLUDES =
TEXT("#include \"StaticJIT/StaticJITHeader.h\"\n")
;

const FString STANDARD_FOOTER =
TEXT("#endif\n")
TEXT("#endif\n")
;

// Provider module sources already have an explicit target-profile guard from
// FAngelscriptJITGeneration. Reusing STANDARD_HEADER here would freeze the
// Editor commandlet's own build configuration as a second, contradictory
// guard (for example UE_BUILD_DEVELOPMENT inside GameShipping output).
const FString PROVIDER_STANDARD_HEADER =
TEXT("#include \"StaticJIT/StaticJITConfig.h\"\n")
TEXT("#ifndef AS_SKIP_JITTED_CODE\n\n")
;

const FString PROVIDER_STANDARD_FOOTER =
TEXT("#endif\n")
;

// Hack because some of the properties in NoExportTypes.h
// are named differently from how they're named in the real C++ types. :sweeney:
static const TSet<FString> GTypesExcludedFromPropertyNativization =
{
	TEXT("/Script/CoreUObject.Matrix"),
};

FJITFile& FAngelscriptStaticJIT::GetFile(asIScriptModule* Module)
{
	if (!JITFiles.Contains(Module))
	{
		auto File = MakeShared<FJITFile>();
		if (Module != nullptr)
		{
			FString ModuleSymbol = GetUniqueModuleName(*File, Module);
			File->Filename = FString::Printf(TEXT("%s.as.jit.hpp"), *ModuleSymbol);
			File->JITPrefix = TEXT("AS_") + ModuleSymbol + TEXT("__");
			File->ModuleName = ANSI_TO_TCHAR(Module->GetName());
		}
		else
		{
			File->Filename = TEXT("__no_module__.as.jit.hpp");
			File->JITPrefix = TEXT("AS_NA__");
			File->ModuleName = TEXT("NO_MODULE");
		}

		JITFiles.Add(Module, File);
		return *File;
	}
	else
	{
		return *JITFiles[Module];
	}
}

FString FAngelscriptStaticJIT::GetUniqueFunctionName(FJITFile& File, asIScriptFunction* ScriptFunction, int32 EntryCount)
{
	FString BaseName;

	auto* ObjectType = ScriptFunction->GetObjectType();
	if (ObjectType != nullptr)
	{
		BaseName = TEXT("AS_");
		BaseName += ANSI_TO_TCHAR(ObjectType->GetName());
		BaseName += TEXT("__");
		BaseName += ANSI_TO_TCHAR(ScriptFunction->GetName());
	}
	else
	{
		BaseName = File.JITPrefix + ANSI_TO_TCHAR(ScriptFunction->GetName());
	}

	SanitizeSymbolName(BaseName);

	FString Name;
	int Num = 0;
	do
	{
		Name = BaseName;
		if (Num != 0)
			Name += FString::Printf(TEXT("__%d"), Num);
		Num++;
	} while (UsedUniqueNames.Contains(Name));


	UsedUniqueNames.Add(Name);
	return Name;
}

FString FAngelscriptStaticJIT::GetUniqueLabelName(FJITFile& File, asIScriptFunction* ScriptFunction, FStringView LabelSuffix)
{
	FString BaseName;

	auto* ObjectType = ScriptFunction->GetObjectType();
	if (ObjectType != nullptr)
	{
		BaseName = TEXT("LABEL_");
		BaseName += ANSI_TO_TCHAR(ObjectType->GetName());
		BaseName += TEXT("__");
		BaseName += ANSI_TO_TCHAR(ScriptFunction->GetName());

		if (LabelSuffix.Len() != 0)
		{
			BaseName += TEXT("_");
			BaseName += LabelSuffix;
		}
	}
	else
	{
		BaseName = TEXT("LABEL_");
		BaseName += ANSI_TO_TCHAR(ScriptFunction->GetName());

		if (LabelSuffix.Len() != 0)
		{
			BaseName += TEXT("_");
			BaseName += LabelSuffix;
		}
	}

	SanitizeSymbolName(BaseName);

	FString Name;
	int Num = 0;
	do
	{
		Name = BaseName;
		if (Num != 0)
			Name += FString::Printf(TEXT("__%d"), Num);
		Num++;
	} while (UsedUniqueNames.Contains(Name));

	UsedUniqueNames.Add(Name);
	return Name;
}

FString FAngelscriptStaticJIT::GetUniqueModuleName(FJITFile& File, asIScriptModule* ScriptModule)
{
	FString BaseName = ANSI_TO_TCHAR(ScriptModule->GetName());

	int32 DotIndex;
	if (BaseName.FindLastChar((TCHAR)'.', DotIndex))
		BaseName = BaseName.Mid(DotIndex + 1);

	SanitizeSymbolName(BaseName);

	FString Name;
	int Num = 0;
	do
	{
		Name = BaseName;
		if (Num != 0)
			Name += FString::Printf(TEXT("__%d"), Num);
		Num++;
	} while (UsedUniqueNames.Contains(Name));

	UsedUniqueNames.Add(Name);
	return Name;
}

FString FAngelscriptStaticJIT::GetUniqueSymbolName(const FString& InBaseName)
{
	FString BaseName = InBaseName;
	SanitizeSymbolName(BaseName);

	FString Name;
	int Num = 0;
	do
	{
		Name = BaseName;
		if (Num != 0)
			Name += FString::Printf(TEXT("__%d"), Num);
		Num++;
	} while (UsedUniqueNames.Contains(Name));

	UsedUniqueNames.Add(Name);
	return Name;
}

TOptional<uint32> FAngelscriptStaticJIT::FindStableProviderReferenceSlot(
	asCScriptFunction* Caller,
	const FAngelscriptArtifactReference& Reference,
	const bool bRequireExpectedAbi) const
{
	const TArray<FAngelscriptArtifactReference>* const References =
		StableProviderReferences.Find(Caller);
	if (References == nullptr)
	{
		return {};
	}
	for (int32 Index = 0; Index < References->Num(); ++Index)
	{
		const FAngelscriptArtifactReference& Candidate = (*References)[Index];
		if (Candidate.Kind == Reference.Kind
			&& Candidate.StableKey == Reference.StableKey
			&& (!bRequireExpectedAbi
				|| Candidate.ExpectedAbi == Reference.ExpectedAbi))
		{
			return static_cast<uint32>(Index);
		}
	}
	return {};
}

TOptional<uint32> FAngelscriptStaticJIT::FindStableProviderReferenceSlot(
	asCScriptFunction* Caller,
	const EAngelscriptArtifactReferenceKind Kind,
	const FAngelscriptHash256& StableKey) const
{
	return FindStableProviderReferenceSlot(
		Caller, FAngelscriptArtifactReference{Kind, StableKey, {}}, false);
}

void FStaticJITContext::AdvanceBC(int32 AdvanceAmount)
{
	BC += AdvanceAmount;
}

void FStaticJITContext::AdvanceBC()
{
	asEBCInstr Instr = (asEBCInstr)*(asBYTE*)BC;
	int32 InstrSize = asBCTypeSize[asBCInfo[Instr].type];
	AdvanceBC(InstrSize);
}

asDWORD* FStaticJITContext::GetNextBC(asDWORD* FromBC)
{
	asEBCInstr Instr = (asEBCInstr)*(asBYTE*)FromBC;
	int32 InstrSize = asBCTypeSize[asBCInfo[Instr].type];
	return FromBC + InstrSize;
}

asEBCInstr FStaticJITContext::GetInstr(asDWORD* FromBC)
{
	return (asEBCInstr)*(asBYTE*)FromBC;
}

asEBCInstr FStaticJITContext::GetInstr()
{
	return (asEBCInstr)*(asBYTE*)BC;
}

int32 FStaticJITContext::GetInstrSize()
{
	asEBCInstr Instr = (asEBCInstr)*(asBYTE*)BC;
	return asBCTypeSize[asBCInfo[Instr].type];
}

void FStaticJITContext::Line(const TCHAR* Content)
{
	FunctionContent.Add(FEntryContent{ Content, BC, 0, LocalStackOffset, EEntryType::Line });
}

void FStaticJITContext::Entry(EEntryType Type)
{
	FunctionContent.Add(FEntryContent{ TEXT(""), nullptr, 0, LocalStackOffset, Type });
}

int32 FStaticJITContext::GetBCOffset()
{
	return (int32)(BC - BC_EntryStart);
}

FString FStaticJITContext::AllocateLocalVariable(const ANSICHAR* Type)
{
	if (!bInLocalState)
	{
		Line("{");
		bInLocalState = true;
	}

	FString Name = FString::Printf(TEXT("LOCAL_%d"), LocalVariableCount++);
	Line("{0} {1};", Type, Name);
	return Name;
}

void FStaticJITContext::AddHeader(const FString& Header)
{
	File->Headers.Add(Header);
}

void FStaticJITContext::DebugLineNumber()
{
	if (!JIT->bEmitDebugMetadataInOutput)
	{
		return;
	}

	int32 LineNumber = ScriptFunction->GetLineNumber(
		(BC - BC_FunctionStart), nullptr
	) & 0xFFFFF;

	Line("SCRIPT_DEBUG_CALLSTACK_LINE({0});", LineNumber);
}

void FStaticJITContext::GenerateNewFunction(asIScriptFunction* InScriptFunction)
{
	// Init the state for this entry
	BC_EntryStart = BC;
	UsageFlags = 0;
	ScriptFunction = (asCScriptFunction*)InScriptFunction;

	// Generate the surrounding code for the new function
	check(FunctionHead.Len() == 0);
	check(FunctionFoot.Len() == 0);
	check(FunctionContent.Num() == 0);
	check(BC == BC_FunctionStart); // We no longer support sharded JIT functions

	// Populate the parameters with their offsets
	FString VMEntryOutArgs;
	FString VMEntryFooter;
	FString VMEntryAssignment;
	FString ParmsEntryOutArgs;
	FString ParmsEntryHeader;
	FString ParmsEntryFooter;
	FString ParmsEntryAssignment;
	FString CppEntryInArgs;
	FString FunctionReturnType = TEXT("void");

	bool bParmsEntryValid = true;

	// Generate the actual C++ code for the main function
	FunctionHead += FString::Printf(TEXT("// == Jit at BC %d ==\n"), (asDWORD)(BC - BC_FunctionStart));
	if (JIT->bEmitDebugMetadataInOutput)
	{
		FunctionHead += FString::Printf(TEXT("%s(\"%s\", %d);\n"),
			(ScriptFunction->objectType != nullptr && (ScriptFunction->objectType->GetFlags() & asOBJ_REF) != 0)
				? TEXT("SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT")
				: TEXT("SCRIPT_DEBUG_CALLSTACK_FRAME"),
			ANSI_TO_TCHAR(ScriptFunction->GetDeclarationStr(true, true, false, true, false).AddressOf()),
			(ScriptFunction->GetLineNumber(0, nullptr) & 0xFFFFF));
	}
	FunctionHead += TEXT("SCRIPT_ASSUME_NO_EXCEPTION()\n");
	// Create the local stack space
	if (LocalStackSize > 0)
		FunctionHead += FString::Printf(TEXT("alignas(8) asBYTE l_stack[%d];\n"), LocalStackSize * 4);

	FunctionHead += TEXT("asQWORD l_valueRegister = 0;\n");
	FunctionHead += TEXT("asBYTE l_byteRegister = 0;\n");
	FunctionHead += TEXT("asDWORD l_dwordRegister = 0;\n");
	FunctionHead += TEXT("float l_floatRegister = 0;\n");
	FunctionHead += TEXT("double l_doubleRegister = 0;\n");
	FunctionHead += TEXT("void* l_objectRegister = nullptr;\n");

	// Populate the object pointer if we have it
	if (ScriptFunction->objectType != nullptr)
	{
		FAngelscriptTypeUsage ObjectType = FAngelscriptTypeUsage::FromTypeId(ScriptFunction->objectType->GetTypeId());
		FString LiteralType;

		if (ObjectType.IsValid())
		{
			FAngelscriptType::FCppForm CppForm;
			ObjectType.GetCppForm(CppForm);

			if (CppForm.CppHeader.Len() != 0)
				AddHeader(CppForm.CppHeader);

			if (CppForm.CppType.Len() != 0)
				LiteralType = CppForm.CppType;
			else if (CppForm.CppGenericType.Len() != 0)
				LiteralType = CppForm.CppGenericType;
		}

		if (LiteralType.Len() == 0)
			LiteralType = TEXT("void*");
		else if (!LiteralType.EndsWith(TEXT("*")))
			LiteralType += TEXT("*");

		CppEntryInArgs += FString::Printf(TEXT(", %s l_This"), *LiteralType);
		VMEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(%s*)l_fp"), *LiteralType);
		ParmsEntryOutArgs += FString::Printf(TEXT(",\n\t\t(%s)Object"), *LiteralType);

		LiteralObjectType = LiteralType;
		bHasThisPointer = true;
	}

	// Populate the parameters with their offsets
	ParmsEntryHeader += TEXT("\tSIZE_T ParmsOffset = 0;\n");

	for (int i = 0, Count = ScriptFunction->parameterTypes.GetLength(); i < Count; ++i)
	{
		int32 Offset = -ScriptFunction->parameterOffsets[i];
		auto DataType = ScriptFunction->parameterTypes[i];

		FString ParmsEntryParamOffsetVar = FString::Printf(TEXT("ParmOffset_%d_%s"), i, ANSI_TO_TCHAR(ScriptFunction->parameterNames[i].AddressOf()));

		FVariableAsLocal Param;
		Param.LocalName = ANSI_TO_TCHAR(ScriptFunction->parameterNames[i].AddressOf());
		Param.LocalName = TEXT("p_") + Param.LocalName;
		Param.bIsConstant = true;

		Param.Type = FAngelscriptTypeUsage::FromParam(ScriptFunction, i);

		bool bHasNativeCppForm = false;

		if (Param.Type.IsValid())
		{
			FAngelscriptType::FCppForm CppForm;
			Param.Type.GetCppForm(CppForm);

			if (CppForm.CppHeader.Len() != 0)
				AddHeader(CppForm.CppHeader);

			if (CppForm.CppType.Len() != 0)
			{
				Param.LiteralType = CppForm.CppType;
				bHasNativeCppForm = true;
			}
			else if (CppForm.CppGenericType.Len() != 0)
			{
				Param.LiteralType = CppForm.CppGenericType;
			}
		}

		if (DataType.IsReference() && DataType.IsReferenceType())
		{
			if (Param.LiteralType.Len() == 0)
				Param.LiteralType = TEXT("UObject*");

			Param.bIsPointer = true;
			Param.bIsReference = true;
			Param.RepresentedType = EVariableType::Pointer;

			CppEntryInArgs += FString::Printf(TEXT(", %s* %s"), *Param.LiteralType, *Param.LocalName);
			VMEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(%s**)(l_fp + %d)"), *Param.LiteralType, -Offset);
			ParmsEntryOutArgs += FString::Printf(TEXT(",\n\t\t(%s*)(((SIZE_T)Parms) + %s)"), *Param.LiteralType, *ParmsEntryParamOffsetVar);

			if (i != 0)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(void*));\n"));
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += sizeof(void*);\n"));
		}
		else if (DataType.IsReferenceType())
		{
			if (Param.LiteralType.Len() == 0)
				Param.LiteralType = TEXT("UObject*");

			Param.bIsPointer = true;
			Param.RepresentedType = EVariableType::Pointer;

			CppEntryInArgs += FString::Printf(TEXT(", %s %s"), *Param.LiteralType, *Param.LocalName);
			VMEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(%s*)(l_fp + %d)"), *Param.LiteralType, -Offset);
			ParmsEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(%s*)(((SIZE_T)Parms) + %s)"), *Param.LiteralType, *ParmsEntryParamOffsetVar);

			if (i != 0)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(void*));\n"));
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += sizeof(void*);\n"));
		}
		else if (DataType.IsObject() || DataType.IsReference())
		{
			if (Param.LiteralType.Len() == 0)
				Param.LiteralType = TEXT("FUnknownValueType");

			Param.bIsReference = true;
			Param.RepresentedType = EVariableType::Pointer;

			if (DataType.GetSizeInMemoryBytes() < 4 && !DataType.IsObject())
			{
				Param.LiteralType = TEXT("asDWORD");

				// We can't take a reference to a sub-dword primitive from parms,
				// because angelscript expands it to a dword and unreal does not
				bParmsEntryValid = false;
			}
			else
			{
				ParmsEntryOutArgs += FString::Printf(TEXT(",\n\t\t(%s*)(((SIZE_T)Parms) + %s)"), *Param.LiteralType, *ParmsEntryParamOffsetVar);

				if (DataType.IsObject())
				{
					auto* ObjectType = (asCObjectType*)DataType.GetTypeInfo();
					if (bHasNativeCppForm && (ObjectType->flags & asOBJ_SCRIPT_OBJECT) == 0)
					{
						// Native C++ form is available, use size and align from that
						if (i != 0)
							ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *Param.LiteralType);
						ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += sizeof(%s);\n"), *Param.LiteralType);
					}
					else if (!JIT->IsTypePotentiallyDifferent(ObjectType))
					{
						// Datatype is guaranteed the same on all platforms, so we can hardcode the alignment
						if (i != 0)
							ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, %d);\n"), ObjectType->alignment);
						ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += %d;\n"), ObjectType->size);
					}
					else
					{
						// We don't know at compile time the size and align of this struct, so we need to look it up dynamically
						if (i != 0)
							ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, %s->alignment);\n"), *ReferenceTypeInfo(ObjectType));
						ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += %s;\n"), *ReferenceTypeSize(ObjectType));
					}
				}
				else
				{
					// This is a reference to a primitive
					if (i != 0)
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *Param.LiteralType);
					ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
					ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += sizeof(%s);\n"), *Param.LiteralType);
				}
			}

			CppEntryInArgs += FString::Printf(TEXT(", %s* %s"), *Param.LiteralType, *Param.LocalName);
			VMEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(%s**)(l_fp + %d)"), *Param.LiteralType, -Offset);
		}
		else if (DataType.IsFloatExtendedToDouble())
		{
			Param.LiteralType = TEXT("double");
			Param.bIsValue = true;
			Param.RepresentedType = EVariableType::Double;

			CppEntryInArgs += FString::Printf(TEXT(", double %s"), *Param.LocalName);
			VMEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(double*)(l_fp + %d)"), -Offset);
			ParmsEntryOutArgs += FString::Printf(TEXT(",\n\t\t(double)*(float*)(((SIZE_T)Parms) + %s)"), *ParmsEntryParamOffsetVar);

			if (i != 0)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(float));\n"));
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += sizeof(float);\n"));
		}
		else
		{
			FString VMType;
			switch (DataType.GetSizeInMemoryBytes())
			{
			case 1:
				VMType = TEXT("asBYTE");
				Param.RepresentedType = EVariableType::Byte;
			break;
			case 2:
				VMType = TEXT("asWORD");
				Param.RepresentedType = EVariableType::Word;
			break;
			case 4:
				if (DataType.GetTokenType() == ttFloat32)
				{
					VMType = TEXT("float");
					Param.RepresentedType = EVariableType::Float;
				}
				else
				{
					VMType = TEXT("asDWORD");
					Param.RepresentedType = EVariableType::DWord;
				}
			break;
			case 8:
				if (DataType.GetTokenType() == ttFloat64)
				{
					VMType = TEXT("double");
					Param.RepresentedType = EVariableType::Double;
				}
				else
				{
					VMType = TEXT("asQWORD");
					Param.RepresentedType = EVariableType::QWord;
				}
			break;
			default: check(false); break;
			}

			if (Param.LiteralType.Len() == 0)
				Param.LiteralType = VMType;
			Param.bIsValue = true;

			// This is a primitive value with a defined C++ type
			if (i != 0)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *Param.LiteralType);
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += sizeof(%s);\n"), *Param.LiteralType);

			CppEntryInArgs += FString::Printf(TEXT(", %s %s"), *VMType, *Param.LocalName);
			VMEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(%s*)(l_fp + %d)"), *VMType, -Offset);
			ParmsEntryOutArgs += FString::Printf(TEXT(",\n\t\t(%s)*(%s*)(((SIZE_T)Parms) + %s)"), *VMType, *Param.LiteralType, *ParmsEntryParamOffsetVar);
		}

		LocalVariables.Add(Offset, Param);
	}

	// Populate the return pointer if we have it
	if (ScriptFunction->returnType.GetTokenType() != ttVoid)
	{
		bool bHasParameters = ScriptFunction->parameterTypes.GetLength() != 0;

		auto DataType = ScriptFunction->returnType;
		FAngelscriptTypeUsage ReturnType = FAngelscriptTypeUsage::FromReturn(ScriptFunction);
		FString LiteralType;

		int32 VMOffset = (ScriptFunction->objectType != nullptr) ? 2 : 0;

		bool bHasNativeCppForm = false;
		FString ParmsEntryParamOffsetVar = TEXT("ReturnParmOffset");

		if (ReturnType.IsValid())
		{
			FAngelscriptType::FCppForm CppForm;
			ReturnType.GetCppForm(CppForm);

			if (CppForm.CppHeader.Len() != 0)
				AddHeader(CppForm.CppHeader);

			if (CppForm.CppType.Len() != 0)
			{
				LiteralType = CppForm.CppType;
				bHasNativeCppForm = true;
			}
			else if (CppForm.CppGenericType.Len() != 0)
			{
				LiteralType = CppForm.CppGenericType;
			}
		}

		if (ScriptFunction->returnType.IsReference() && ScriptFunction->returnType.IsReferenceType())
		{
			bReturnIsReference = true;
			bReturnIsPointer = true;
			bReturnValueDirectly = true;
			ReturnSizeBytes = sizeof(void*);
			ReturnVariableType = EVariableType::Pointer;

			if (LiteralType.Len() == 0)
				LiteralType = TEXT("UObject*");

			VMEntryAssignment = FString::Printf(TEXT("*(%s**)l_outValue = "), *LiteralType);
			ParmsEntryAssignment = FString::Printf(TEXT("*(%s**)(((SIZE_T)Parms) + %s) = "), *LiteralType, *ParmsEntryParamOffsetVar);

			if (bHasParameters)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(void*));\n"));
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);

			LiteralType += TEXT("*");
			FunctionReturnType = LiteralType;
		}
		else if (ScriptFunction->returnType.IsReferenceType())
		{
			bReturnIsPointer = true;
			bReturnValueDirectly = true;
			ReturnSizeBytes = sizeof(void*);
			ReturnVariableType = EVariableType::Pointer;

			if (LiteralType.Len() == 0)
				LiteralType = TEXT("UObject*");

			VMEntryAssignment = FString::Printf(TEXT("*(%s*)l_outValue = "), *LiteralType);
			ParmsEntryAssignment = FString::Printf(TEXT("*(%s*)(((SIZE_T)Parms) + %s) = "), *LiteralType, *ParmsEntryParamOffsetVar);

			if (bHasParameters)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(void*));\n"));
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);

			FunctionReturnType = LiteralType;
		}
		else if (ScriptFunction->returnType.IsReference())
		{
			bReturnIsReference = true;
			bReturnValueDirectly = true;
			ReturnSizeBytes = sizeof(void*);
			ReturnVariableType = EVariableType::Pointer;

			if (LiteralType.Len() == 0)
				LiteralType = TEXT("FUnknownValueType");

			VMEntryAssignment = FString::Printf(TEXT("*(%s**)l_outValue = "), *LiteralType);
			ParmsEntryAssignment = FString::Printf(TEXT("*(%s**)(((SIZE_T)Parms) + %s) = "), *LiteralType, *ParmsEntryParamOffsetVar);

			if (DataType.IsObject())
			{
				auto* ObjectType = (asCObjectType*)DataType.GetTypeInfo();
				if (bHasNativeCppForm && (ObjectType->flags & asOBJ_SCRIPT_OBJECT) == 0)
				{
					// Native C++ form is available, use size and align from that
					if (bHasParameters)
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *LiteralType);
					ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
				}
				else if (!JIT->IsTypePotentiallyDifferent(ObjectType))
				{
					// Datatype is guaranteed the same on all platforms, so we can hardcode the alignment
					if (bHasParameters)
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, %d);\n"), ObjectType->alignment);
					ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
				}
				else
				{
					// We don't know at compile time the size and align of this struct, so we need to look it up dynamically
					if (bHasParameters)
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, %s->alignment);\n"), *ReferenceTypeInfo(ObjectType));
					ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
				}
			}
			else
			{
				// This is a reference to a primitive
				if (bHasParameters)
					ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *LiteralType);
				ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			}

			LiteralType += TEXT("*");
			FunctionReturnType = LiteralType;
		}
		else if (ScriptFunction->returnType.IsObject())
		{
			bReturnIsReference = true;
			bReturnOnStack = true;
			bReturnValueDirectly = false;
			ReturnVariableType = EVariableType::Pointer;

			check(ScriptFunction->DoesReturnOnStack());
			if (LiteralType.Len() == 0)
				LiteralType = TEXT("FUnknownValueType");

			CppEntryInArgs = FString::Printf(TEXT(", %s* l_ReturnValue"), *LiteralType) + CppEntryInArgs;
			VMEntryOutArgs = FString::Printf(TEXT(",\n\t\t*(%s**)(l_fp + %d)"), *LiteralType, VMOffset) + VMEntryOutArgs;
			ParmsEntryOutArgs = FString::Printf(TEXT(",\n\t\t(%s*)(((SIZE_T)Parms) + %s)"), *LiteralType, *ParmsEntryParamOffsetVar) + ParmsEntryOutArgs;

			auto* ObjectType = (asCObjectType*)DataType.GetTypeInfo();
			if (bHasNativeCppForm)
			{
				// Native C++ form is available, use size and align from that
				if (bHasParameters)
					ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *LiteralType);
				ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			}
			else if (!JIT->IsTypePotentiallyDifferent(ObjectType))
			{
				// Datatype is guaranteed the same on all platforms, so we can hardcode the alignment
				if (bHasParameters)
					ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, %d);\n"), ObjectType->alignment);
				ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			}
			else
			{
				// We don't know at compile time the size and align of this struct, so we need to look it up dynamically
				if (bHasParameters)
					ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, %s->alignment);\n"), *ReferenceTypeInfo(ObjectType));
				ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			}

			LiteralType += TEXT("*");
		}
		else if (ScriptFunction->returnType.IsFloatExtendedToDouble())
		{
			VMEntryAssignment = TEXT("*(float*)l_outValue = (float)");
			ParmsEntryAssignment = FString::Printf(TEXT("*(float*)(((SIZE_T)Parms) + %s) = "), *ParmsEntryParamOffsetVar);

			if (bHasParameters)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(float));\n"));
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);

			bReturnIsValue = true;
			bReturnIsFloatingPoint = true;
			bReturnValueDirectly = true;
			ReturnSizeBytes = sizeof(double);
			ReturnVariableType = EVariableType::Double;

			LiteralType = TEXT("double");
			FunctionReturnType = LiteralType;
		}
		else
		{
			if (LiteralType.Len() == 0)
			{
				switch (ScriptFunction->returnType.GetSizeInMemoryBytes())
				{
				case 1:
					LiteralType = TEXT("asBYTE");
					ReturnVariableType = EVariableType::Byte;
				break;
				case 2:
					LiteralType = TEXT("asWORD");
					ReturnVariableType = EVariableType::Word;
				break;
				case 4:
					if (ScriptFunction->returnType.GetTokenType() == ttFloat32)
					{
						LiteralType = TEXT("float");
						ReturnVariableType = EVariableType::Float;
					}
					else
					{
						LiteralType = TEXT("asDWORD");
						ReturnVariableType = EVariableType::DWord;
					}
				break;
				case 8:
					if (ScriptFunction->returnType.GetTokenType() == ttFloat64)
					{
						LiteralType = TEXT("double");
						ReturnVariableType = EVariableType::Double;
					}
					else
					{
						LiteralType = TEXT("asQWORD");
						ReturnVariableType = EVariableType::QWord;
					}
				break;
				default: check(false); break;
				}
			}

			if (bHasParameters)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *LiteralType);
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);

			VMEntryAssignment = FString::Printf(TEXT("*(%s*)l_outValue = "), *LiteralType);
			ParmsEntryAssignment = FString::Printf(TEXT("*(%s*)(((SIZE_T)Parms) + %s) = "), *LiteralType, *ParmsEntryParamOffsetVar);

			if (ScriptFunction->returnType.GetTokenType() == ttFloat32 || ScriptFunction->returnType.GetTokenType() == ttFloat64)
				bReturnIsFloatingPoint = true;

			bReturnIsValue = true;
			bReturnValueDirectly = true;
			ReturnSizeBytes = ScriptFunction->returnType.GetSizeInMemoryBytes();
			FunctionReturnType = LiteralType;
		}

		LiteralReturnType = LiteralType;
	}

	// Populate the names for named variables
	TArray<bool> VariablesRepresented;
	int32 VariableSpace = ScriptFunction->scriptData->variableSpace;
	VariablesRepresented.AddZeroed(VariableSpace);
	
	auto MarkOffset = [&](int32 Offset, int32 Size)
	{
		for (int i = 0; i < Size; ++i)
		{
			check(!VariablesRepresented[VariableSpace - Offset + i]);
			VariablesRepresented[VariableSpace - Offset + i] = true;
		}
	};

	auto IsOffsetMarked = [&](int32 Offset)
	{
		return VariablesRepresented[VariableSpace - Offset];
	};

	auto RepresentStackVariable = [&](int32 Offset, const asCDataType& DataType, const asCString& VarName)
	{
		if (LocalVariables.Contains(Offset))
			return;
		if (Offset <= 0)
			return;

		FVariableAsLocal LocalVar;

		if (VarName.GetLength() == 0)
		{
			FString VarType;
			if (DataType.IsObject())
			{
				if (DataType.IsReference() || DataType.IsReferenceType())
					VarType = TEXT("ptr_");
			}
			else
			{
				switch (DataType.GetSizeInMemoryBytes())
				{
				case 1: VarType = TEXT("byte_"); break;
				case 2: VarType = TEXT("word_"); break;
				case 4:
					if (DataType.GetTokenType() == ttFloat32)
						VarType = TEXT("float_");
					else
						VarType = TEXT("dword_");
				break;
				case 8:
					if (DataType.GetTokenType() == ttFloat64)
						VarType = TEXT("double_");
					else
						VarType = TEXT("qword_");
				break;
				default: check(false); break;
				}
			}

			LocalVar.LocalName = FString::Printf(TEXT("v_TEMP_%s%d"), *VarType, Offset);
		}
		else
		{
			LocalVar.LocalName = ANSI_TO_TCHAR(VarName.AddressOf());
			LocalVar.LocalName = TEXT("v_") + LocalVar.LocalName;
		}

		LocalVar.Type = FAngelscriptTypeUsage::FromDataType(DataType);

		if (LocalNamesUsed.Contains(LocalVar.LocalName))
		{
			FString BaseName = LocalVar.LocalName;
			int32 NameIndex = 0;

			do
			{
				NameIndex += 1;
				LocalVar.LocalName = FString::Printf(TEXT("%s_%d"), *BaseName, NameIndex);
			} while (LocalNamesUsed.Contains(LocalVar.LocalName));
		}
		LocalNamesUsed.Add(LocalVar.LocalName);
		check(!IsOffsetMarked(Offset));

		if (LocalVar.Type.IsValid())
		{
			FAngelscriptType::FCppForm CppForm;
			LocalVar.Type.GetCppForm(CppForm);

			if (CppForm.CppHeader.Len() != 0)
				AddHeader(CppForm.CppHeader);

			if (CppForm.CppType.Len() != 0)
				LocalVar.LiteralType = CppForm.CppType;
			else if (CppForm.CppGenericType.Len() != 0)
				LocalVar.LiteralType = CppForm.CppGenericType;
		}

		if (DataType.IsReference() && DataType.IsReferenceType())
		{
			if (LocalVar.LiteralType.Len() == 0)
				LocalVar.LiteralType = TEXT("UObject*");

			LocalVar.bIsPointer = true;
			LocalVar.bIsReference = true;

			FunctionHead += FString::Printf(TEXT("%s* %s = nullptr;\n"),
				*LocalVar.LiteralType, *LocalVar.LocalName);

			MarkOffset(Offset, 2);

			LocalVar.RepresentedType = EVariableType::Pointer;
			LocalVar.LiteralType += TEXT("*");
		}
		else if (DataType.IsNullHandle())
		{
			LocalVar.LiteralType = TEXT("void*");
			LocalVar.bIsPointer = true;
			LocalVar.RepresentedType = EVariableType::Pointer;
			FunctionHead += FString::Printf(TEXT("%s %s = nullptr;\n"),
				*LocalVar.LiteralType, *LocalVar.LocalName);

			MarkOffset(Offset, 2);
		}
		else if (DataType.IsReferenceType())
		{
			if (LocalVar.LiteralType.Len() == 0)
				LocalVar.LiteralType = TEXT("UObject*");

			LocalVar.bIsPointer = true;
			LocalVar.RepresentedType = EVariableType::Pointer;
			FunctionHead += FString::Printf(TEXT("%s %s = nullptr;\n"),
				*LocalVar.LiteralType, *LocalVar.LocalName);

			MarkOffset(Offset, 2);
		}
		else if (DataType.IsReference())
		{
			if (LocalVar.LiteralType.Len() == 0)
				LocalVar.LiteralType = TEXT("FUnknownValueType");

			LocalVar.bIsReference = true;
			LocalVar.RepresentedType = EVariableType::Pointer;
			FunctionHead += FString::Printf(TEXT("%s* %s = nullptr;\n"),
				*LocalVar.LiteralType, *LocalVar.LocalName);
			LocalVar.LiteralType += TEXT("*");

			MarkOffset(Offset, 2);
		}
		else if (DataType.IsObject())
		{
			FString NativeType;
			if (LocalVar.LiteralType.Len() == 0)
				LocalVar.LiteralType = TEXT("FUnknownValueType");
			else
				NativeType = LocalVar.LiteralType;

			LocalVar.bIsReference = true;
			LocalVar.RepresentedType = EVariableType::Object;

			int32 Align = DataType.GetAlignment();

			if (NativeType.Len() != 0 && !DataType.IsScriptObject())
			{
				if (Align > 1)
					FunctionHead += FString::Printf(TEXT("alignas(%d) "), Align);
				FunctionHead += FString::Printf(TEXT("asBYTE MEM_%s[sizeof(%s)];\n"),
					*LocalVar.LocalName, *NativeType);
			}
			else
			{
				bool bPotentiallyDifferent = JIT->IsTypePotentiallyDifferent((asCObjectType*)DataType.GetTypeInfo());
				if (bPotentiallyDifferent)
				{
					auto ComputedOffsets = JIT->GetComputedOffsets((asCObjectType*)DataType.GetTypeInfo());
					if (ComputedOffsets.IsValid() && (ComputedOffsets->bCanHardcodeSize || ComputedOffsets->bHasComputedSize))
					{
						FunctionHead += FString::Printf(TEXT("alignas(%s) asBYTE MEM_%s[%s];\n"),
							*ReferenceTypeAlignment(DataType.GetTypeInfo()),
							*LocalVar.LocalName,
							*ReferenceTypeSize(DataType.GetTypeInfo()));
					}
					else
					{
						// Don't need to do alignment here, Unreal Alloca is always aligned to 16 bytes already
						FunctionHead += FString::Printf(TEXT("asDWORD* MEM_%s = (asDWORD*)FMemory_Alloca(%s->size);\n"),
							*LocalVar.LocalName, *ReferenceTypeInfo(DataType.GetTypeInfo()));
					}
				}
				else
				{
					if (Align > 1)
						FunctionHead += FString::Printf(TEXT("alignas(%d) "), Align);
					FunctionHead += FString::Printf(TEXT("asBYTE MEM_%s[%d];\n"),
						*LocalVar.LocalName, FMath::Max(DataType.GetSizeInMemoryDWords() * 4, 1));
				}
			}

			MarkOffset(Offset, DataType.GetSizeInMemoryDWords());

			FunctionHead += FString::Printf(TEXT("%s& %s = (%s&)MEM_%s[0];\n"),
				*LocalVar.LiteralType, *LocalVar.LocalName, *LocalVar.LiteralType, *LocalVar.LocalName);

			LocalVar.LiteralType += TEXT("&");
		}
		else
		{
			FString VMType;
			switch (DataType.GetSizeInMemoryBytes())
			{
			case 1:
				LocalVar.LiteralType = TEXT("asBYTE");
				LocalVar.RepresentedType = EVariableType::Byte;
			break;
			case 2:
				LocalVar.LiteralType = TEXT("asWORD");
				LocalVar.RepresentedType = EVariableType::Word;
			break;
			case 4:
				if (DataType.GetTokenType() == ttFloat32)
				{
					LocalVar.LiteralType = TEXT("float");
					LocalVar.RepresentedType = EVariableType::Float;
				}
				else
				{
					LocalVar.LiteralType = TEXT("asDWORD");
					LocalVar.RepresentedType = EVariableType::DWord;
				}
			break;
			case 8:
				if (DataType.GetTokenType() == ttFloat64)
				{
					LocalVar.LiteralType = TEXT("double");
					LocalVar.RepresentedType = EVariableType::Double;
				}
				else
				{
					LocalVar.LiteralType = TEXT("asQWORD");
					LocalVar.RepresentedType = EVariableType::QWord;
				}
			break;
			default: check(false); break;
			}

			MarkOffset(Offset, DataType.GetSizeInMemoryDWords());

			LocalVar.bIsValue = true;
			FunctionHead += FString::Printf(TEXT("%s %s = {};\n"),
				*LocalVar.LiteralType, *LocalVar.LocalName);
		}

		LocalVariables.Add(Offset, LocalVar);
	};

	// Create variables for any explicitly declared variables in the function
	for (int i = 0, Count = ScriptFunction->scriptData->variables.GetLength(); i < Count; ++i)
	{
		asSScriptVariable* Var = ScriptFunction->scriptData->variables[i];

		int32 Offset = Var->stackOffset;
		auto DataType = Var->type;

		RepresentStackVariable(Offset, DataType, Var->name);
	}

	// Create variables for any implicitly declared variables in the function
	for (int i = 0, Count = ScriptFunction->scriptData->objVariableTypes.GetLength(); i < Count; ++i)
	{
		int32 Offset = ScriptFunction->scriptData->objVariablePos[i];
		asCDataType DataType;

		auto* VarType = ScriptFunction->scriptData->objVariableTypes[i];
		if ( VarType != nullptr )
		{
			// Don't know why this is here but it's in the equivalent code in the angelscript library so I assume it's needed for something
			int idx = ScriptFunction->scriptData->objVariablePos.IndexOf(ScriptFunction->scriptData->objVariablePos[i]);

			bool isOnHeap = asUINT(idx) < ScriptFunction->scriptData->objVariablesOnHeap ? true : false;

			DataType = asCDataType::CreateType(VarType, false);
			if (isOnHeap)
			{
				if (VarType->GetFlags() & asOBJ_VALUE)
					DataType.MakeReference(true);
				else
					DataType.MakeHandle(true);
			}
		}
		else
		{
			DataType = asCDataType::CreateNullHandle();
		}

		RepresentStackVariable(Offset, DataType, asCString());
	}

	// Create variables for any temporary variables in the function
	for (int i = 0, Count = ScriptFunction->scriptData->temporaryVariables.GetLength(); i < Count; ++i)
	{
		int32 Offset = ScriptFunction->scriptData->temporaryVariables[i].Offset;
		asCDataType DataType = asCDataType::CreatePrimitive(ScriptFunction->scriptData->temporaryVariables[i].Token, false);

		RepresentStackVariable(Offset, DataType, asCString());
	}

	const TCHAR* RawEntryComment = JIT->bEmitProviderEntryLinkage
		? TEXT("{RAW_ENTRY_COMMENT}") : TEXT("");
	FunctionHead = FString(RawEntryComment)
		+ FString::Printf(TEXT("%s %s(FScriptExecution& Execution%s)\n{\n"), *FunctionReturnType, *FunctionName, *CppEntryInArgs)
		+ FunctionHead;
	FunctionDecl = FString::Printf(TEXT("%s %s(FScriptExecution& Execution%s)"), *FunctionReturnType, *FunctionName, *CppEntryInArgs);

	// Generate the stub entries
	FunctionFoot += TEXT("}\n");
	const TCHAR* EntryLinkage = JIT->bEmitProviderEntryLinkage
		? TEXT("") : TEXT("static ");
	if (JIT->bEmitProviderEntryLinkage)
	{
		FunctionFoot += TEXT("{VM_ENTRY_COMMENT}");
	}
	FunctionFoot += FString::Printf(TEXT("%svoid %s_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)\n{\n"), EntryLinkage, *FunctionName);
	FunctionFoot += FString::Printf(TEXT("\t%s%s(Execution%s);\n"), *VMEntryAssignment, *FunctionName, *VMEntryOutArgs);
	FunctionFoot += VMEntryFooter;
	FunctionFoot += TEXT("}\n");

	if (bParmsEntryValid)
	{
		if (JIT->bEmitProviderEntryLinkage)
		{
			FunctionFoot += TEXT("{PARMS_ENTRY_COMMENT}");
		}
		FunctionFoot += FString::Printf(TEXT("%svoid %s_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)\n{\n"), EntryLinkage, *FunctionName);
		FunctionFoot += ParmsEntryHeader;
		FunctionFoot += FString::Printf(TEXT("\t%s%s(Execution%s);\n"), *ParmsEntryAssignment, *FunctionName, *ParmsEntryOutArgs);
		FunctionFoot += ParmsEntryFooter;
		FunctionFoot += TEXT("}\n");
	}
}

void FStaticJITContext::NewInstruction()
{
	auto& Instr = Instructions[CurrentInstructionIndex];
	if (Instr.Label.Len() != 0)
	{
		MaterializeWholeStack();

		// Don't allow any use of the value register after landing a jump,
		// since it will have been optimized away by the jump instruction
		//MaterializeValueRegister();
		if (Instr.bHasMultipleValueRegisterStates)
		{
			ValueRegisterState = EValueRegisterState::Indeterminate;
		}
		else if (ValueRegisterState != EValueRegisterState::Indeterminate)
		{
			if (ValueRegisterState != Instr.ValueRegisterStateFromJump)
				ValueRegisterState = EValueRegisterState::Indeterminate;
		}
		else
		{
			ValueRegisterState = Instr.ValueRegisterStateFromJump;
		}

		LocalStackOffset = Instr.StackOffsetBeforeInstr;
		Line("{0}:", Instr.Label);
	}
	else if (!Instr.bJumpPartOfSwitch)
	{
		check(Instr.bMarked);
		check(Instr.StackOffsetBeforeInstr == LocalStackOffset);
	}

	// If this instruction modifies any variables that we've pushed to the stack before,
	// make sure those parts of the stack are materialized
	auto BCInstr = (asEBCInstr)*(asBYTE*)Instr.BC;
	const auto& BCInfo = asBCInfo[BCInstr];
	if((BCInfo.type == asBCTYPE_wW_rW_rW_ARG ||
		BCInfo.type == asBCTYPE_wW_rW_ARG    ||
		BCInfo.type == asBCTYPE_wW_rW_DW_ARG ||
		BCInfo.type == asBCTYPE_wW_ARG       ||
		BCInfo.type == asBCTYPE_wW_W_ARG     ||
		BCInfo.type == asBCTYPE_wW_DW_ARG    ||
		BCInfo.type == asBCTYPE_wW_QW_ARG))
	{
		int VariableOffset = asBC_SWORDARG0(Instr.BC);
		MaterializePushedVariableAtOffset(VariableOffset);
	}

	// If this instruction has the potential to modify anything volatile, materialize
	// the parts of the stack that could be edited by it
	if (Instr.Bytecode->CanModifyStackVolatiles(*this))
	{
		MaterializeStackVolatiles();
	}
}

bool FStaticJITContext::IsPartOfSwitch()
{
	return Instructions[CurrentInstructionIndex].bJumpPartOfSwitch;
}

void FStaticJITContext::AddLabel(int32 RelativeBCOffset, bool bConditional)
{
	asDWORD* TargetBC = BC + RelativeBCOffset;
	for (int32 i = 0, Count = Instructions.Num(); i < Count; ++i)
	{
		auto& TargetInstr = Instructions[i];
		if (TargetInstr.BC == TargetBC)
		{
			Instructions[CurrentInstructionIndex].JumpTargets.Add(i);
			Instructions[CurrentInstructionIndex].bIsConditionalJump = bConditional;
			TargetInstr.ReceivedJumps.Add(CurrentInstructionIndex);

			if (TargetInstr.Label.Len() == 0)
			{
				TargetInstr.Label = JIT->GetUniqueLabelName(
					*File, ScriptFunction,
					LexToString((int)(TargetBC - BC_FunctionStart)));
			}
			return;
		}
	}

	checkf(false, TEXT("Couldn't find instruction to jump to for label."));
}

FStaticJITContext::FInstruction& FStaticJITContext::GetLabelInstruction(int32 RelativeBCOffset)
{
	auto& CurrentInstr = Instructions[CurrentInstructionIndex];
	asDWORD* TargetBC = BC + RelativeBCOffset;
	for (int32 i = 0, Count = CurrentInstr.JumpTargets.Num(); i < Count; ++i)
	{
		auto& TargetInstr = Instructions[CurrentInstr.JumpTargets[i]];
		if (TargetInstr.BC == TargetBC)
		{
			return TargetInstr;
		}
	}

	check(false);
	return Instructions[0];
}

void FStaticJITContext::Jump(FInstruction& TargetInstruction)
{
	check(FloatingStackExpressions.Num() == 0);
	check(LocalStackOffset == TargetInstruction.StackOffsetBeforeInstr);

	if (ValueRegisterState != EValueRegisterState::Indeterminate)
	{
		if (TargetInstruction.ValueRegisterStateFromJump == EValueRegisterState::Indeterminate)
			TargetInstruction.ValueRegisterStateFromJump = ValueRegisterState;
		else if (TargetInstruction.ValueRegisterStateFromJump != ValueRegisterState)
			TargetInstruction.bHasMultipleValueRegisterStates = true;
	}
	else
	{
		TargetInstruction.bHasMultipleValueRegisterStates = true;
	}

	Line("goto {0};", TargetInstruction.Label);
}

void FStaticJITContext::ReturnEmptyValue()
{
	if (bReturnValueDirectly)
		Line("return {};");
	else
		Line("return;");
}

void MarkLiveObjectsForExceptionCleanup(FStaticJITContext& Context, bool bAfterCurrentOp, TArray<int, TInlineAllocator<64>>& liveObjects)
{
	// Destruct any objects that are alive at this point in the bytecode
	asDWORD* CleanupBC = Context.BC;
	if (bAfterCurrentOp)
	{
		// Move to next instruction for determining object live status
		CleanupBC = Context.GetNextBC(CleanupBC);
	}

	asUINT bcPos = CleanupBC - Context.BC_FunctionStart;

	auto& objVariableInfo = Context.ScriptFunction->scriptData->objVariableInfo;
	auto& objVariablePos = Context.ScriptFunction->scriptData->objVariablePos;

	liveObjects.SetNumZeroed(objVariablePos.GetLength());
	for( int n = 0; n < (int)objVariableInfo.GetLength(); n++ )
	{
		// Find the first variable info with a larger position than the current
		// As the variable info are always placed on the instruction right after the
		// one that initialized or freed the object, the current position needs to be
		// considered as valid.
		if( objVariableInfo[n].programPos > bcPos )
		{
			// We've determined how far the execution ran, now determine which variables are alive
			for( --n; n >= 0; n-- )
			{
				switch( objVariableInfo[n].option )
				{
				case asOBJ_UNINIT: // Object was destroyed
					{
						// TODO: optimize: This should have been done by the compiler already
						// Which variable is this?
						asUINT var = 0;
						for (asUINT v = 0; v < objVariablePos.GetLength(); v++)
						{
							if (objVariablePos[v] == objVariableInfo[n].variableOffset)
							{
								var = v;
								break;
							}
						}

						liveObjects[var] -= 1;
					}
					break;
				case asOBJ_INIT: // Object was created
					{
						// Which variable is this?
						asUINT var = 0;
						for (asUINT v = 0; v < objVariablePos.GetLength(); v++)
						{
							if (objVariablePos[v] == objVariableInfo[n].variableOffset)
							{
								var = v;
								break;
							}
						}
						liveObjects[var] += 1;
					}
					break;
				case asBLOCK_BEGIN: // Start block
					// We should ignore start blocks, since it just means the
					// program was within the block when the exception ocurred
					break;
				case asBLOCK_END: // End block
					// We need to skip the entire block, as the objects created
					// and destroyed inside this block are already out of scope
					{
						int nested = 1;
						while( nested > 0 )
						{
							int option = objVariableInfo[--n].option;
							if( option == 3 )
								nested++;
							if( option == 2 )
								nested--;
						}
					}
					break;
				}
			}

			// We're done with the investigation
			break;
		}
	}
}

void FStaticJITContext::ExceptionCleanupAndReturn(bool bAfterCurrentOp, bool bSetNullptrException)
{
	auto& objVariablePos = ScriptFunction->scriptData->objVariablePos;

	TArray<int, TInlineAllocator<64>> liveObjects;
	MarkLiveObjectsForExceptionCleanup(*this, bAfterCurrentOp, liveObjects);

	// Determine which variables need cleanup
	FExceptionCleanup Cleanup;

	for( asUINT n = 0; n < objVariablePos.GetLength(); n++ )
	{
		int pos = objVariablePos[n];
		if( n < ScriptFunction->scriptData->objVariablesOnHeap )
		{
			continue;
		}
		else
		{
			// Only destroy the object if it is truly alive
			if( liveObjects[n] > 0 )
			{
				auto* objType = (asCObjectType*)ScriptFunction->scriptData->objVariableTypes[n];
				if (objType->beh.destruct == 0)
					continue;

				Cleanup.Positions.Add(pos);
				Cleanup.Types.Add(objType);
			}
		}
	}

	// If we don't have any cleanup to do, just return
	if (Cleanup.Positions.Num() == 0)
	{
		if (bSetNullptrException)
			Line("SCRIPT_NULL_POINTER_EXCEPTION();");
		ReturnEmptyValue();
		return;
	}

	// Check if we have an existing cleanup label for this
	for (auto& ExistingLabel : ExceptionCleanupLabels)
	{
		if (ExistingLabel.Positions == Cleanup.Positions
			&& ExistingLabel.Types == Cleanup.Types)
		{
			if (bSetNullptrException)
				ExistingLabel.bCalledAddNullptrException = true;
			else
				ExistingLabel.bCalledWithoutAddNullptrException = true;

			Line("goto {0};", ExistingLabel.Label);
			return;
		}
	}

	// Create a new cleanup label
	Cleanup.Label = JIT->GetUniqueLabelName(
		*File, ScriptFunction,
		FString::Printf(TEXT("EXCEPTION_%d"), ExceptionCleanupLabels.Num()));
	if (bSetNullptrException)
		Cleanup.bCalledAddNullptrException = true;
	else
		Cleanup.bCalledWithoutAddNullptrException = true;
	ExceptionCleanupLabels.Add(Cleanup);

	Line("goto {0};", Cleanup.Label);
}

void FStaticJITContext::WriteOutFunction()
{
	File->Content.Add(FunctionHead);

	for (auto& StateVar : StateVars)
		File->Content.Add(FString::Printf(TEXT("%s %s;\n"), *StateVar.Type, *StateVar.Name));

	if (ParmStructSize != 0 || bParmIsExecuted)
		File->Content.Add(FString::Printf(TEXT("alignas(16) asBYTE l_ParmStruct[%d];\n"), FMath::Max(ParmStructSize, 1)));

	TArray<FEntryContent> ActualContent = MoveTemp(FunctionContent);
	FunctionContent.Empty();

	int32 Index = 0;
	int32 Count = ActualContent.Num();

	for (; Index < Count; ++Index)
	{
		const FEntryContent& Entry = ActualContent[Index];
		switch(Entry.Type)
		{
		case EEntryType::Line:
			File->Content.Add(Entry.Line + TEXT("\n"));
		break;
		}
	}
	File->Content.Add(FunctionFoot);

	check(FunctionContent.Num() == 0);
	FunctionHead.Empty();
	FunctionFoot.Empty();
	FunctionContent.Empty();
	StateVars.Empty();
}

FString FStaticJITContext::BCConstant_QWord()
{
	return FString::Printf(TEXT("(asQWORD)0x%llxu"), asBC_PTRARG(BC));
}

FString FStaticJITContext::BCConstant_DWord(int32 Offset)
{
	return FString::Printf(TEXT("(asDWORD)0x%xu"), asBC_DWORDARG(BC + Offset));
}

inline FString GetSymbolNameForObjectType(asCTypeInfo* TypeInfo)
{
	int32 SubTypes = TypeInfo->GetSubTypeCount();
	if (SubTypes == 0)
		return ANSI_TO_TCHAR(TypeInfo->name.AddressOf());

	FString SymbolName = ANSI_TO_TCHAR(TypeInfo->name.AddressOf());
	for (int32 i = 0; i < SubTypes; ++i)
	{
		SymbolName += TEXT("_");
		SymbolName += ANSI_TO_TCHAR(
			TypeInfo->GetEngine()->GetTypeDeclaration(
				TypeInfo->GetSubTypeId(i), false
			)
		);
	}
	return SymbolName;
}

namespace
{
	static void SetStableProviderReferenceError(
		FAngelscriptStaticJIT& JIT,
		const asCScriptFunction* Caller,
		const TCHAR* UseKind,
		const FStringView Target)
	{
		if (JIT.StableProviderReferenceError.IsEmpty())
		{
			JIT.StableProviderReferenceError = FString::Printf(
				TEXT("Provider generation has no stable %s slot for caller '%s' and target '%.*s'."),
				UseKind,
				Caller != nullptr
					? ANSI_TO_TCHAR(Caller->GetDeclaration()) : TEXT("<null>"),
				Target.Len(), Target.GetData());
		}
	}

	static bool TryBuildStableProviderTypeReference(
		const FAngelscriptStaticJIT& JIT,
		asITypeInfo* TypeInfo,
		FAngelscriptArtifactReference& OutReference,
		bool& bOutHasCompleteAbi)
	{
		OutReference = {};
		bOutHasCompleteAbi = false;
		asCTypeInfo* const Type = static_cast<asCTypeInfo*>(TypeInfo);
		if (Type == nullptr)
		{
			return false;
		}
		if (Type->module == nullptr)
		{
			bOutHasCompleteAbi =
				FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
					*Type, OutReference);
			return bOutHasCompleteAbi;
		}

		const FAngelscriptStableModuleKey* const ModuleKey =
			JIT.StableProviderModuleKeys.Find(Type->module);
		FAngelscriptStableTypeKey TypeKey;
		if (ModuleKey == nullptr
			|| !FAngelscriptCacheStableSymbolIdentity::TryBuildLocalTypeKey(
				*ModuleKey, *Type, TypeKey))
		{
			return false;
		}
		OutReference.Kind = EAngelscriptArtifactReferenceKind::ScriptType;
		OutReference.StableKey = TypeKey.Hash;
		return true;
	}

	static bool TryBuildStableProviderGlobalReference(
		const FAngelscriptStaticJIT& JIT,
		void* GlobalStorage,
		FAngelscriptArtifactReference& OutReference,
		bool& bOutHasCompleteAbi)
	{
		OutReference = {};
		bOutHasCompleteAbi = false;
		if (GlobalStorage == nullptr || JIT.Engine == nullptr)
		{
			return false;
		}

		asCGlobalProperty* const* const PropertyPtr =
			JIT.Engine->varAddressMap.Find(GlobalStorage);
		if (PropertyPtr == nullptr)
		{
			const FString& Literal = *static_cast<const FString*>(GlobalStorage);
			OutReference =
				FAngelscriptJITReferenceDescriptorBuilder::ForStringLiteral(Literal);
			bOutHasCompleteAbi = true;
			return OutReference.IsValid() && !OutReference.ExpectedAbi.IsZero();
		}

		const asCGlobalProperty* const Property = *PropertyPtr;
		const FAngelscriptStableModuleKey* const ModuleKey = Property != nullptr
			? JIT.StableProviderModuleKeys.Find(Property->module) : nullptr;
		if (Property == nullptr || ModuleKey == nullptr)
		{
			return false;
		}
		const asCString CanonicalType = Property->type.Format(
			Property->nameSpace, false, false);
		FAngelscriptGlobalIdentityDescriptor Identity;
		Identity.ModuleKey = *ModuleKey;
		Identity.Namespace = Property->nameSpace != nullptr
			? UTF8_TO_TCHAR(Property->nameSpace->name.AddressOf()) : FString();
		Identity.Kind = EAngelscriptArtifactEntityKind::GlobalVariable;
		Identity.Name = UTF8_TO_TCHAR(Property->name.AddressOf());
		Identity.CanonicalType = UTF8_TO_TCHAR(CanonicalType.AddressOf());
		OutReference.Kind = EAngelscriptArtifactReferenceKind::ScriptGlobal;
		OutReference.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(Identity).Hash;
		return !OutReference.StableKey.IsZero();
	}

	static bool TryBuildStableProviderPropertyReference(
		const FAngelscriptStaticJIT& JIT,
		const asCTypeInfo* OwnerType,
		const asCObjectProperty* Property,
		FAngelscriptArtifactReference& OutReference)
	{
		OutReference = {};
		const FAngelscriptStableModuleKey* const ModuleKey = OwnerType != nullptr
			? JIT.StableProviderModuleKeys.Find(OwnerType->module) : nullptr;
		FAngelscriptStableTypeKey OwnerTypeKey;
		if (ModuleKey == nullptr || Property == nullptr
			|| !FAngelscriptCacheStableSymbolIdentity::TryBuildLocalTypeKey(
				*ModuleKey, *OwnerType, OwnerTypeKey))
		{
			return false;
		}
		const asCString CanonicalType = Property->type.Format(
			OwnerType->nameSpace, false, false);
		FAngelscriptPropertyIdentityDescriptor Identity;
		Identity.OwnerTypeKey = OwnerTypeKey;
		Identity.Kind = EAngelscriptArtifactEntityKind::Property;
		Identity.Name = UTF8_TO_TCHAR(Property->name.AddressOf());
		Identity.CanonicalType = UTF8_TO_TCHAR(CanonicalType.AddressOf());
		OutReference.Kind = EAngelscriptArtifactReferenceKind::ScriptProperty;
		OutReference.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(Identity).Hash;
		return !OutReference.StableKey.IsZero();
	}
}

FString FStaticJITContext::ReferenceFunction(asCScriptFunction* Function)
{
	if (Function != nullptr && Function->module == nullptr)
	{
		FAngelscriptArtifactReference Reference;
		const bool bBuilt =
			FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
				*Function, Reference);
		const TOptional<uint32> SlotIndex = bBuilt
			? JIT->FindStableProviderReferenceSlot(
				ScriptFunction, Reference, true)
			: TOptional<uint32>();
		if (!SlotIndex.IsSet())
		{
			SetStableProviderReferenceError(
				*JIT, ScriptFunction,
				TEXT("EnvironmentSymbol function object"),
				FStringView(ANSI_TO_TCHAR(Function->GetDeclaration())));
			return TEXT("nullptr");
		}
		return FString::Printf(
			TEXT("FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, %uu)"),
			SlotIndex.GetValue());
	}

	const TMap<asCScriptFunction*, uint32>* const FunctionSlots =
		JIT->StableProviderFunctionReferenceSlots.Find(ScriptFunction);
	const uint32* const SlotIndex = FunctionSlots != nullptr
		&& Function != nullptr ? FunctionSlots->Find(Function) : nullptr;
	if (SlotIndex == nullptr)
	{
		SetStableProviderReferenceError(
			*JIT, ScriptFunction, TEXT("ScriptFunction"),
			Function != nullptr
				? FStringView(ANSI_TO_TCHAR(Function->GetDeclaration()))
				: FStringView(TEXT("<null>")));
		return TEXT("nullptr");
	}
	return FString::Printf(
		TEXT("FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, %uu)"),
		*SlotIndex);

}
FString FStaticJITContext::ReferenceSystemFunctionPointer(asCScriptFunction* Function)
{
	FAngelscriptArtifactReference Reference;
	const bool bBuilt = Function != nullptr
		&& FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
			*Function, Reference);
	const TOptional<uint32> SlotIndex = bBuilt
		? JIT->FindStableProviderReferenceSlot(
			ScriptFunction, Reference, true) : TOptional<uint32>();
	if (!SlotIndex.IsSet())
	{
		SetStableProviderReferenceError(
			*JIT, ScriptFunction, TEXT("EnvironmentSymbol function"),
			Function != nullptr
				? FStringView(ANSI_TO_TCHAR(Function->GetDeclaration()))
				: FStringView(TEXT("<null>")));
		return TEXT("asSFuncPtr()");
	}
	return FString::Printf(
		TEXT("FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, %uu)"),
		SlotIndex.GetValue());

}
FString FStaticJITContext::ReferenceTypeInfo(asITypeInfo* TypeInfo)
{
	FAngelscriptArtifactReference Reference;
	bool bHasCompleteAbi = false;
	const bool bBuilt = TryBuildStableProviderTypeReference(
		*JIT, TypeInfo, Reference, bHasCompleteAbi);
	const TOptional<uint32> SlotIndex = bBuilt
		? JIT->FindStableProviderReferenceSlot(
			ScriptFunction, Reference, bHasCompleteAbi)
		: TOptional<uint32>();
	if (!SlotIndex.IsSet())
	{
		SetStableProviderReferenceError(
			*JIT, ScriptFunction, TEXT("type"),
			TypeInfo != nullptr
				? FStringView(UTF8_TO_TCHAR(TypeInfo->GetName()))
				: FStringView(TEXT("<null>")));
		return TEXT("nullptr");
	}
	return FString::Printf(
		TEXT("FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, %uu)"),
		SlotIndex.GetValue());

}
FString FStaticJITContext::ReferenceTypeSize(asITypeInfo* TypeInfo)
{
	if (!JIT->IsTypePotentiallyDifferent(TypeInfo))
		return FString::Printf(TEXT("%d"), TypeInfo->GetSize());

	asCObjectType* ObjectType = CastToObjectType((asCTypeInfo*)TypeInfo);
	if (ObjectType != nullptr)
	{
		auto ComputedOffsets = JIT->GetComputedOffsets(ObjectType);
		if (ComputedOffsets.IsValid() && (ComputedOffsets->bCanHardcodeSize || ComputedOffsets->bHasComputedSize))
		{
			FString SizeExpr;

			if (ComputedOffsets->bCanHardcodeSize)
			{
				SizeExpr = FString::Printf(TEXT("%d"), ComputedOffsets->HardcodedSize);
			}
			else if (ComputedOffsets->bHasComputedSize)
			{
				SizeExpr = ComputedOffsets->ComputedSizeVariable;

				if (!ComputedOffsets->ContainingSharedHeader.IsEmpty())
					File->SharedHeaderDependencies.Add(ComputedOffsets->ContainingSharedHeader);
				if (!ComputedOffsets->AdditionalHeader.IsEmpty())
					File->Headers.Add(ComputedOffsets->AdditionalHeader);
			}
			return SizeExpr;
		}
	}

	return FString::Printf(TEXT("%s->size"), *ReferenceTypeInfo(TypeInfo));
}

FString FStaticJITContext::ReferenceTypeAlignment(asITypeInfo* TypeInfo)
{
	if (!JIT->IsTypePotentiallyDifferent(TypeInfo))
		return FString::Printf(TEXT("%d"), TypeInfo->alignment);

	asCObjectType* ObjectType = CastToObjectType((asCTypeInfo*)TypeInfo);
	if (ObjectType != nullptr)
	{
		auto ComputedOffsets = JIT->GetComputedOffsets(ObjectType);
		if (ComputedOffsets.IsValid() && (ComputedOffsets->bCanHardcodeSize || ComputedOffsets->bHasComputedSize))
		{
			FString AlignmentExpr;

			if (ComputedOffsets->bCanHardcodeSize)
			{
				AlignmentExpr = FString::Printf(TEXT("%d"), ComputedOffsets->HardcodedAlignment);
			}
			else if (ComputedOffsets->bHasComputedSize)
			{
				AlignmentExpr = ComputedOffsets->ComputedAlignmentVariable;

				if (!ComputedOffsets->ContainingSharedHeader.IsEmpty())
					File->SharedHeaderDependencies.Add(ComputedOffsets->ContainingSharedHeader);
				if (!ComputedOffsets->AdditionalHeader.IsEmpty())
					File->Headers.Add(ComputedOffsets->AdditionalHeader);
			}
			return AlignmentExpr;
		}
	}

	return FString::Printf(TEXT("%s->alignment"), *ReferenceTypeInfo(TypeInfo));
}

FString FStaticJITContext::ReferenceObjectType(asITypeInfo* TypeInfo)
{
	check((TypeInfo->GetFlags() & (asOBJ_VALUE | asOBJ_REF)) != 0);
		FAngelscriptArtifactReference Reference;
	bool bHasCompleteAbi = false;
	const bool bBuilt = TryBuildStableProviderTypeReference(
		*JIT, TypeInfo, Reference, bHasCompleteAbi);
	const TOptional<uint32> SlotIndex = bBuilt
		? JIT->FindStableProviderReferenceSlot(
			ScriptFunction, Reference, bHasCompleteAbi)
		: TOptional<uint32>();
	if (!SlotIndex.IsSet())
	{
		SetStableProviderReferenceError(
			*JIT, ScriptFunction, TEXT("object type"),
			TypeInfo != nullptr
				? FStringView(UTF8_TO_TCHAR(TypeInfo->GetName()))
				: FStringView(TEXT("<null>")));
		return TEXT("nullptr");
	}
	return FString::Printf(
		TEXT("FAngelscriptJITGeneratedReferenceAccess::GetObjectType(Execution, %uu)"),
		SlotIndex.GetValue());

}
FString FStaticJITContext::ReferenceTypeId(asDWORD TypeId)
{
	if (TypeId <= asTYPEID_LAST_PRIMITIVE)
		return FString::Printf(TEXT("0x%x"), TypeId);

	auto* TypeInfo = GetEngine()->GetTypeInfoById(TypeId);

	asQWORD ExtraFlags = asQWORD(asDWORD(TypeId) & ~(asTYPEID_MASK_SEQNBR | asTYPEID_MASK_OBJECT));
	if (ExtraFlags != 0)
	{
		return FString::Printf(TEXT("(%s->typeId | 0x%x)"),
			*ReferenceTypeInfo(TypeInfo),
			ExtraFlags);
	}
	else
	{
		return FString::Printf(TEXT("%s->typeId"),
			*ReferenceTypeInfo(TypeInfo));
	}
}

FString FStaticJITContext::ReferenceGlobalVariable(void* GlobalVar)
{
		FAngelscriptArtifactReference Reference;
	bool bHasCompleteAbi = false;
	const bool bBuilt = TryBuildStableProviderGlobalReference(
		*JIT, GlobalVar, Reference, bHasCompleteAbi);
	const TOptional<uint32> SlotIndex = bBuilt
		? JIT->FindStableProviderReferenceSlot(
			ScriptFunction, Reference, bHasCompleteAbi)
		: TOptional<uint32>();
	if (!SlotIndex.IsSet())
	{
		SetStableProviderReferenceError(
			*JIT, ScriptFunction, TEXT("global storage"),
			Reference.IsValid()
				? FStringView(Reference.StableKey.ToHexString())
				: FStringView(TEXT("<unresolved>")));
		return TEXT("nullptr");
	}
	return FString::Printf(
		TEXT("FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, %uu)"),
		SlotIndex.GetValue());

}
FString FStaticJITContext::ReferencePropertyOffset(short Offset, int TypeId)
{
	asCObjectType* ObjectType = (asCObjectType*)GetEngine()->GetTypeInfoById(TypeId);
	asCObjectType* InType = nullptr;
	asCObjectProperty* ASProperty = ObjectType->GetPropertyByOffset(Offset, &InType);

	// Try to turn this into a C++ offsetof() expression
	FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(InType->GetTypeId());
	if (Usage.IsValid())
	{
		UStruct* UnrealType = nullptr;
		if (auto* UnrealClass = Usage.GetClass())
			UnrealType = UnrealClass;
		else if (auto* UnrealStruct = Usage.GetUnrealStruct())
			UnrealType = UnrealStruct;

		FAngelscriptType::FCppForm CppForm;
		Usage.GetCppForm(CppForm);

		if (CppForm.CppType.Len() != 0 && UnrealType != nullptr)
		{
			// Find the FProperty that represents this
			for (TFieldIterator<FProperty> It(UnrealType); It; ++It)
			{
				FProperty* Property = *It;
				if (Property->GetOffset_ForUFunction() == Offset
					&& !Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected)
					&& Property->HasAllPropertyFlags(CPF_NativeAccessSpecifierPublic))
				{
					FString ASName = ANSI_TO_TCHAR(ASProperty->name.AddressOf());
					if (Property->GetNameCPP().Equals(ASName, ESearchCase::IgnoreCase)
						&& !GTypesExcludedFromPropertyNativization.Contains(UnrealType->GetPathName()))
					{
						if (CppForm.CppHeader.Len() != 0)
							AddHeader(CppForm.CppHeader);
						CppForm.CppType.RemoveFromEnd(TEXT("*"));
						return FString::Printf(TEXT("offsetof(%s, %s)"), *CppForm.CppType, *ASName);
					}
				}
			}
		}
	}

	// Script structs always have their properties in the same place
	bool bPotentiallyDifferent = JIT->IsTypePotentiallyDifferent(InType);
	if (!bPotentiallyDifferent)
		return FString::Printf(TEXT("(short)%d"), Offset);

	// We might be able to use a computed offset instead of a stored one
	TSharedPtr<FAngelscriptStaticJIT::FComputedTypeOffsets> ComputedOffsets = JIT->GetComputedOffsets(InType);
	if (ComputedOffsets.IsValid())
	{
		FString PropertyName = ANSI_TO_TCHAR(ASProperty->name.AddressOf());
		FAngelscriptStaticJIT::FComputedPropertyOffset* PropOffset = ComputedOffsets->Properties.Find(PropertyName);
		if (PropOffset != nullptr)
		{
			FString OffsetExpr;
			if (PropOffset->bCanHardcodeOffset)
			{
				OffsetExpr = FString::Printf(TEXT("%d"), PropOffset->HardcodedOffset);
			}
			else
			{
				check(!ComputedOffsets->ContainingSharedHeader.IsEmpty());
				File->SharedHeaderDependencies.Add(ComputedOffsets->ContainingSharedHeader);

				if (!ComputedOffsets->AdditionalHeader.IsEmpty())
					File->Headers.Add(ComputedOffsets->AdditionalHeader);
				OffsetExpr = PropOffset->ComputedOffsetVariable;
			}

			return OffsetExpr;
		}
	}

		FAngelscriptArtifactReference Reference;
	const bool bBuilt = TryBuildStableProviderPropertyReference(
		*JIT, InType, ASProperty, Reference);
	const TOptional<uint32> SlotIndex = bBuilt
		? JIT->FindStableProviderReferenceSlot(
			ScriptFunction, Reference, false) : TOptional<uint32>();
	if (!SlotIndex.IsSet())
	{
		SetStableProviderReferenceError(
			*JIT, ScriptFunction, TEXT("property"),
			ASProperty != nullptr
				? FStringView(UTF8_TO_TCHAR(ASProperty->name.AddressOf()))
				: FStringView(TEXT("<null>")));
		return TEXT("0u");
	}
	return FString::Printf(
		TEXT("FAngelscriptJITGeneratedReferenceAccess::GetPropertyOffset(Execution, %uu)"),
		SlotIndex.GetValue());

}
FString FStaticJITContext::VArg(int32 ArgumentIndex)
{
	if (ArgumentIndex == 0)
	{
		return VAddress(asBC_SWORDARG0(BC));
	}
	else if (ArgumentIndex == 1)
	{
		return VAddress(asBC_SWORDARG1(BC));
	}
	else if (ArgumentIndex == 2)
	{
		return VAddress(asBC_SWORDARG2(BC));
	}
	else
	{
		check(false);
		return TEXT("");
	}
}

FString FStaticJITContext::VArg_Address(int32 ArgumentIndex)
{
	if (ArgumentIndex == 0)
	{
		return VAddress(asBC_SWORDARG0(BC));
	}
	else if (ArgumentIndex == 1)
	{
		return VAddress(asBC_SWORDARG1(BC));
	}
	else if (ArgumentIndex == 2)
	{
		return VAddress(asBC_SWORDARG2(BC));
	}
	else
	{
		check(false);
		return TEXT("");
	}
}

FString FStaticJITContext::VArg_Var(int32 ArgumentIndex)
{
	if (ArgumentIndex == 0)
	{
		return VVar(asBC_SWORDARG0(BC));
	}
	else if (ArgumentIndex == 1)
	{
		return VVar(asBC_SWORDARG1(BC));
	}
	else if (ArgumentIndex == 2)
	{
		return VVar(asBC_SWORDARG2(BC));
	}
	else
	{
		check(false);
		return TEXT("");
	}
}

FString FStaticJITContext::VArg_SignedVar(int32 ArgumentIndex)
{
	return FString::Printf(TEXT("((%s&)%s)"), *VArg_SignedTypename(ArgumentIndex), *VArg_Var(ArgumentIndex));
}

int32 FStaticJITContext::VArg_Offset(int32 ArgumentIndex)
{
	if (ArgumentIndex == 0)
	{
		return asBC_SWORDARG0(BC);
	}
	else if (ArgumentIndex == 1)
	{
		return asBC_SWORDARG1(BC);
	}
	else if (ArgumentIndex == 2)
	{
		return asBC_SWORDARG2(BC);
	}
	else
	{
		check(false);
		return 0;
	}
}

bool FStaticJITContext::VArg_IsConstant(int32 ArgumentIndex)
{
	int32 VariableOffset = VArg_Offset(ArgumentIndex);
	if (ScriptFunction->objectType != nullptr)
	{
		if (VariableOffset == 0)
			return true;
	}

	for (auto& Elem : LocalVariables)
	{
		if (Elem.Key == VariableOffset)
			return Elem.Value.bIsConstant;
	}

	return false;
}

FStaticJITContext::EVariableType FStaticJITContext::VArg_Type(int32 ArgumentIndex)
{
	int32 VariableOffset = VArg_Offset(ArgumentIndex);
	if (ScriptFunction->objectType != nullptr)
	{
		if (VariableOffset == 0)
			return EVariableType::Pointer;
	}

	for (auto& Elem : LocalVariables)
	{
		if (Elem.Key == VariableOffset)
			return Elem.Value.RepresentedType;
	}

	if (VariableOffset == (ScriptFunction->objectType != nullptr ? -2 : 0)
		&& ScriptFunction->returnType.GetTokenType() != ttVoid
		&& ScriptFunction->DoesReturnOnStack())
	{
		return ReturnVariableType;
	}

	check(false);
	return EVariableType::Byte;
}

FString FStaticJITContext::VArg_LiteralTypename(int32 ArgumentIndex)
{
	int32 VariableOffset = VArg_Offset(ArgumentIndex);
	if (ScriptFunction->objectType != nullptr)
	{
		if (VariableOffset == 0)
			return LiteralObjectType;
	}

	for (auto& Elem : LocalVariables)
	{
		if (Elem.Key == VariableOffset)
			return Elem.Value.LiteralType;
	}

	if (VariableOffset == (ScriptFunction->objectType != nullptr ? -2 : 0)
		&& ScriptFunction->returnType.GetTokenType() != ttVoid
		&& ScriptFunction->DoesReturnOnStack())
	{
		return LiteralReturnType;
	}

	check(false);
	return TEXT("");
}

FString FStaticJITContext::VArg_SignedTypename(int32 ArgumentIndex)
{
	EVariableType Type = VArg_Type(ArgumentIndex);
	switch (Type)
	{
	case EVariableType::Byte:
		return TEXT("int8");
	case EVariableType::Word:
		return TEXT("int16");
	case EVariableType::DWord:
		return TEXT("int32");
	case EVariableType::QWord:
		return TEXT("int64");
	default:
		check(false);
		return TEXT("");
	}
}

bool FStaticJITContext::VArg_IsNumeric(int32 ArgumentIndex)
{
	EVariableType Type = VArg_Type(ArgumentIndex);
	switch (Type)
	{
	case EVariableType::Byte:
	case EVariableType::Word:
	case EVariableType::DWord:
	case EVariableType::QWord:
		return true;
	default:
		return false;
	}
}

bool FStaticJITContext::VArg_ContainsNonNullPtr(int32 ArgumentIndex)
{
	int32 VariableOffset = VArg_Offset(ArgumentIndex);
	if (bHasThisPointer && VariableOffset == 0)
		return true;

	for (auto& Elem : LocalVariables)
	{
		if (Elem.Key == VariableOffset)
		{
			if (Elem.Value.bIsReference)
				return true;
			return false;
		}
	}

	return false;
}

FString FStaticJITContext::VAddress(short VariableOffset)
{
	if (ScriptFunction->objectType != nullptr)
	{
		if (VariableOffset == 0)
			return TEXT("(&l_This)");
	}

	for (auto& Elem : LocalVariables)
	{
		if (Elem.Key == VariableOffset)
			return FString::Printf(TEXT("(&%s)"), *Elem.Value.LocalName);
	}

	if (VariableOffset == (ScriptFunction->objectType != nullptr ? -2 : 0)
		&& ScriptFunction->returnType.GetTokenType() != ttVoid
		&& ScriptFunction->DoesReturnOnStack())
	{
		check(bReturnOnStack);
		return TEXT("(&l_ReturnValue)");
	}

	check(false);
	return TEXT("void");
}

FString FStaticJITContext::VVar(short VariableOffset)
{
	if (ScriptFunction->objectType != nullptr)
	{
		if (VariableOffset == 0)
			return TEXT("l_This");
	}

	for (auto& Elem : LocalVariables)
	{
		if (Elem.Key == VariableOffset)
			return Elem.Value.LocalName;
	}

	if (VariableOffset == (ScriptFunction->objectType != nullptr ? -2 : 0)
		&& ScriptFunction->returnType.GetTokenType() != ttVoid
		&& ScriptFunction->DoesReturnOnStack())
	{
		check(bReturnOnStack);
		return TEXT("l_ReturnValue");
	}

	check(false);
	return TEXT("void");
}

void FStaticJITContext::Push(int32 DWords, EVariableType Type, const FString& Expression, bool bAssumeNonNull)
{
	LocalStackOffset -= DWords;
	check(LocalStackOffset >= 0);

	const ANSICHAR* StackType = (DWords == 1 ? "asDWORD" : "asQWORD");
	Line("value_assign_safe<{0}>(&l_stack[{1}], {2});", StackType, LocalStackOffset * 4, Expression);
}

void FStaticJITContext::PushVolatile(int32 DWords, EVariableType Type, const FString& Expression, bool bAssumeNonNull)
{
	LocalStackOffset -= DWords;
	check(LocalStackOffset >= 0);

	FStackExpression& Expr = FloatingStackExpressions.Emplace_GetRef();
	Expr.DWords = DWords;
	Expr.ValueType = Type;
	Expr.OffsetOnStack = LocalStackOffset;
	Expr.Expression = Expression;
	Expr.bIsVolatile = true;
}

FStaticJITContext::FStackExpression& FStaticJITContext::PushStatic(int32 DWords)
{
	LocalStackOffset -= DWords;
	check(LocalStackOffset >= 0);

	FStackExpression& Expr = FloatingStackExpressions.Emplace_GetRef();
	Expr.DWords = DWords;
	Expr.ValueType = DWords == 1 ? EVariableType::DWord : EVariableType::QWord;
	Expr.OffsetOnStack = LocalStackOffset;
	return Expr;
}

void FStaticJITContext::PushVArgValue(int32 DWords, int32 ArgumentIndex)
{
	LocalStackOffset -= DWords;
	check(LocalStackOffset >= 0);

	FStackExpression& Expr = FloatingStackExpressions.Emplace_GetRef();
	Expr.DWords = DWords;
	Expr.bIsVArgValue = true;
	Expr.VArgValueOffset = VArg_Offset(ArgumentIndex);
	Expr.OffsetOnStack = LocalStackOffset;
	Expr.ValueType = VArg_Type(ArgumentIndex);
	Expr.Expression = VArg_Var(ArgumentIndex);
	Expr.bGuaranteeNonNull = VArg_ContainsNonNullPtr(ArgumentIndex);
}

void FStaticJITContext::Pop(int32 DWords)
{
	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		if (FloatingStackExpressions[i].OffsetOnStack == LocalStackOffset)
		{
			check(FloatingStackExpressions[i].DWords == DWords);
			FloatingStackExpressions.RemoveAt(i);
			break;
		}
	}

	LocalStackOffset += DWords;
	check(LocalStackOffset <= LocalStackSize);
}

void FStaticJITContext::PopMultiple(int32 TotalDWords)
{
	LocalStackOffset += TotalDWords;
	check(LocalStackOffset <= LocalStackSize);

	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.OffsetOnStack < LocalStackOffset)
			FloatingStackExpressions.RemoveAt(i);
		else
			check(Expr.OffsetOnStack >= LocalStackOffset);
	}
}

void FStaticJITContext::MaterializeStackAtOffset(int32 Offset)
{
	int32 FindOffset = LocalStackOffset + Offset;
	check(FindOffset >= 0);
	check(FindOffset < LocalStackSize);

	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.OffsetOnStack == FindOffset)
		{
			const ANSICHAR* StackType = (Expr.DWords == 1 ? "asDWORD" : "asQWORD");

			Line("// Materialize stack {0}", Expr.OffsetOnStack);
			Line("value_assign_safe<{0}>(&l_stack[{1}], {2});", StackType, Expr.OffsetOnStack * 4, Expr.Expression);

			if (Expr.bGuaranteeNonNull)
			{
				check(Expr.DWords == 2);
				Line("SCRIPT_ASSUME_NOT_NULL(*(asPWORD*)&l_stack[{0}])", Expr.OffsetOnStack * 4);
			}

			FloatingStackExpressions.RemoveAt(i);
			break;
		}
	}
}

void FStaticJITContext::MaterializeAllPushedVariables()
{
	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.bIsVArgValue)
		{
			MaterializeStackAtOffset(Expr.OffsetOnStack - LocalStackOffset);
		}
	}
}

void FStaticJITContext::MaterializePushedVariableAtOffset(int32 VariableOffset)
{
	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.bIsVArgValue && Expr.VArgValueOffset == VariableOffset)
		{
			MaterializeStackAtOffset(Expr.OffsetOnStack - LocalStackOffset);
		}
	}
}

void FStaticJITContext::MaterializeStackVolatiles()
{
	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.bIsVolatile)
		{
			MaterializeStackAtOffset(Expr.OffsetOnStack - LocalStackOffset);
		}
	}
}

bool FStaticJITContext::IsStackOffsetMaterialized(int32 Offset)
{
	int32 FindOffset = LocalStackOffset + Offset;
	check(FindOffset >= 0);
	check(FindOffset < LocalStackSize);

	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.OffsetOnStack == FindOffset)
		{
			return false;
		}
	}

	return true;
}

FStaticJITContext::FStackExpression& FStaticJITContext::GetStaticAtStackOffset(int32 Offset)
{
	int32 FindOffset = LocalStackOffset + Offset;
	check(FindOffset >= 0);
	check(FindOffset < LocalStackSize);

	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.OffsetOnStack == FindOffset)
		{
			return Expr;
		}
	}

	check(false);
	return FloatingStackExpressions.Last();
}

void FStaticJITContext::MaterializeWholeStack()
{
	if (FloatingStackExpressions.Num() == 0)
		return;

	Line("// Materialize whole stack");
	for (int32 i = 0, Count = FloatingStackExpressions.Num(); i < Count; ++i)
	{
		auto& Expr = FloatingStackExpressions[i];
		const ANSICHAR* StackType = (Expr.DWords == 1 ? "asDWORD" : "asQWORD");
		Line("value_assign_safe<{0}>(&l_stack[{1}], {2});", StackType, Expr.OffsetOnStack * 4, Expr.Expression);

		if (Expr.bGuaranteeNonNull)
		{
			check(Expr.DWords == 2);
			Line("SCRIPT_ASSUME_NOT_NULL(*(asPWORD*)&l_stack[{0}])", Expr.OffsetOnStack * 4);
		}
	}

	FloatingStackExpressions.Empty();
}

void FStaticJITContext::MaterializeValueRegister(bool bAllowIndeterminate)
{
	switch (ValueRegisterState)
	{
	case EValueRegisterState::ByteRegister:
		Line("// Materialize Value Register");
		Line("l_valueRegister = (asQWORD)l_byteRegister;");
		ValueRegisterState = EValueRegisterState::ValueRegister;
	break;
	case EValueRegisterState::DWordRegister:
		Line("// Materialize Value Register");
		Line("l_valueRegister = (asQWORD)l_dwordRegister;");
		ValueRegisterState = EValueRegisterState::ValueRegister;
	break;
	case EValueRegisterState::FloatRegister:
		Line("// Materialize Value Register");
		Line("l_valueRegister = value_extend_safe<asQWORD>(l_floatRegister);");
		ValueRegisterState = EValueRegisterState::ValueRegister;
	break;
	case EValueRegisterState::DoubleRegister:
		Line("// Materialize Value Register");
		Line("l_valueRegister = value_as_safe<asQWORD>(l_doubleRegister);");
		ValueRegisterState = EValueRegisterState::ValueRegister;
	break;
	case EValueRegisterState::ValueRegister:
	break;
	case EValueRegisterState::Indeterminate:
		check(bAllowIndeterminate);
	break;
	default:
		check(false);
	break;
	}
}

FString FStaticJITContext::GetValueRegisterForUnsignedComparisonMaybeMaterialize()
{
	switch (ValueRegisterState)
	{
	case EValueRegisterState::ByteRegister:
		return TEXT("l_byteRegister");
	break;
	case EValueRegisterState::DWordRegister:
		return TEXT("l_dwordRegister");
	break;
	case EValueRegisterState::FloatRegister:
	case EValueRegisterState::DoubleRegister:
		MaterializeValueRegister();
		return TEXT("l_valueRegister");
	break;
	case EValueRegisterState::ValueRegister:
		return TEXT("l_valueRegister");
	break;
	default:
		check(false);
		return TEXT("");
	break;
	}
}

FString FStaticJITContext::GetValueRegisterForSignedComparisonMaybeMaterialize()
{
	switch (ValueRegisterState)
	{
	case EValueRegisterState::ByteRegister:
		return TEXT("((int8&)l_byteRegister)");
	break;
	case EValueRegisterState::DWordRegister:
		return TEXT("((int32&)l_dwordRegister)");
	break;
	case EValueRegisterState::FloatRegister:
	case EValueRegisterState::DoubleRegister:
		MaterializeValueRegister();
		return TEXT("((int64&)l_valueRegister)");
	break;
	case EValueRegisterState::ValueRegister:
		return TEXT("((int64&)l_valueRegister)");
	break;
	default:
		check(false);
		return TEXT("");
	break;
	}
}

FString FStaticJITContext::StackAddress_At(int32 DWordOffset)
{
	check(IsStackOffsetMaterialized(DWordOffset));
	return FString::Printf(TEXT("&l_stack[%d]"), (LocalStackOffset + DWordOffset) * 4);
}

FString FStaticJITContext::StackValue_At(int32 DWordOffset, int32 DWordSize)
{
	int32 FindOffset = LocalStackOffset + DWordOffset;
	check(FindOffset >= 0);
	check(FindOffset < LocalStackSize);

	for (int32 i = 0, Count = FloatingStackExpressions.Num(); i < Count; ++i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.OffsetOnStack == FindOffset)
		{
			check(DWordSize == Expr.DWords);
			return FString::Printf(TEXT("(%s)"), *Expr.Expression);
		}
	}

	if (DWordSize == 1)
		return FString::Printf(TEXT("((asDWORD&)l_stack[%d])"), FindOffset * 4);
	else
		return FString::Printf(TEXT("((asQWORD&)l_stack[%d])"), FindOffset * 4);
}

bool FStaticJITContext::IsStackValueNonNull(int32 DWordOffset)
{
	int32 FindOffset = LocalStackOffset + DWordOffset;
	check(FindOffset >= 0);
	check(FindOffset < LocalStackSize);

	for (int32 i = 0, Count = FloatingStackExpressions.Num(); i < Count; ++i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.OffsetOnStack == FindOffset)
		{
			return Expr.bGuaranteeNonNull;
		}
	}

	return false;
}

void FStaticJITContext::AddStateVar(const FString& InType, const FString& InName)
{
	for (FStateVar& Var : StateVars)
	{
		if (Var.Name == InName)
		{
			check(Var.Type == InType);
			return;
		}
	}

	StateVars.Add({InType, InName});
}

class asCScriptEngine* FStaticJITContext::GetEngine()
{
	return ScriptFunction->engine;
}

bool FAngelscriptStaticJIT::IsTypePotentiallyDifferent(asITypeInfo* TypeInfo)
{
	if ((TypeInfo->GetFlags() & (asOBJ_REF | asOBJ_VALUE)) == 0)
		return false;

	auto* ObjectType = (asCObjectType*)TypeInfo;
	asQWORD Flags = ObjectType->GetFlags();

	// Assumption that any template containers are never different
	if ((Flags & asOBJ_TEMPLATE) != 0)
	{
		if ((ObjectType->templateBaseType->flags & asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE) == 0)
			return false;
		else
			return true;
	}

	if ((Flags & asOBJ_SCRIPT_OBJECT) == 0)
	{
		if ((Flags & asOBJ_REF) != 0)
		{
			// C++ Reference types can always be different
			return true;
		}
		else
		{
			bool* ExistingCheck = CachedPotentiallyDifferent.Find(ObjectType);
			if (ExistingCheck != nullptr)
				return *ExistingCheck;

			FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(ObjectType->GetTypeId());
			UStruct* UnrealStruct = Usage.GetUnrealStruct();

			bool bPotentialDifference = false;
			if (UnrealStruct != nullptr)
			{
				// Hardcode some base struct types that we know won't change size
				static TSet<UStruct*> GuaranteedNotDifferentStructs = {
					TBaseStructure<FVector>::Get(),
					TBaseStructure<FRotator>::Get(),
					TBaseStructure<FQuat>::Get(),
					TBaseStructure<FTransform>::Get(),
					TBaseStructure<FVector2D>::Get(),
					TBaseStructure<FLinearColor>::Get(),
					TBaseStructure<FVector4>::Get(),
					TBaseStructure<FColor>::Get(),
					TBaseStructure<FFloatInterval>::Get(),
					TBaseStructure<FFloatRange>::Get(),
					TBaseStructure<FGuid>::Get(),
					TBaseStructure<FIntPoint>::Get(),
					TBaseStructure<FIntVector>::Get(),
					TBaseStructure<FIntVector4>::Get(),
					TVariantStructure<FVector2f>::Get(),
					TVariantStructure<FVector3f>::Get(),
					TVariantStructure<FVector4f>::Get(),
					TVariantStructure<FRotator3f>::Get(),
					TVariantStructure<FQuat4f>::Get(),
					TVariantStructure<FTransform3f>::Get(),
				};

				if (!GuaranteedNotDifferentStructs.Contains(UnrealStruct))
				{
					bPotentialDifference = true;
				}
			}

			CachedPotentiallyDifferent.Add(ObjectType, bPotentialDifference);
			return bPotentialDifference;
		}
	}
	else
	{
		if ((Flags & asOBJ_REF) != 0)
		{
			// Angelscript reference types can always be different
			return true;
		}
		else
		{
			// Recursively classify if the angelscript value type can be different
			bool* ExistingCheck = CachedPotentiallyDifferent.Find(ObjectType);
			if (ExistingCheck != nullptr)
				return *ExistingCheck;

			bool bPotentialDifference = false;
			for (int32 i = 0, Count = ObjectType->properties.GetLength(); i < Count; ++i)
			{
				auto& DataType = ObjectType->properties[i]->type;
				if (DataType.IsObject() && !DataType.IsReference() && !DataType.IsReferenceType())
				{
					if (IsTypePotentiallyDifferent((asCObjectType*)DataType.GetTypeInfo()))
					{
						bPotentialDifference = true;
						break;
					}
				}
			}

			CachedPotentiallyDifferent.Add(ObjectType, bPotentialDifference);
			return bPotentialDifference;
		}
	}
}


TSharedPtr<FAngelscriptStaticJIT::FSharedHeader> FAngelscriptStaticJIT::GetActiveSharedHeader()
{
	if (ActiveSharedHeader.IsValid())
		return ActiveSharedHeader;
	if (LatestUsedSharedHeaderName.IsEmpty())
		LatestUsedSharedHeaderName = TEXT("GlobalDeclarations");
	return GetSharedHeader(LatestUsedSharedHeaderName);
}

TSharedPtr<FAngelscriptStaticJIT::FSharedHeader> FAngelscriptStaticJIT::GetSharedHeader(const FString& ModuleName)
{
	LatestUsedSharedHeaderName = ModuleName;

	FString HeaderFilename = ModuleName;
	int32 DotIndex;
	if (HeaderFilename.FindLastChar((TCHAR)'.', DotIndex))
		HeaderFilename = HeaderFilename.Mid(DotIndex + 1);
	SanitizeSymbolName(HeaderFilename);
	HeaderFilename = FString::Printf(TEXT("SHARED_%s.as.jit.h"), *HeaderFilename);

	TSharedPtr<FSharedHeader>* Existing = SharedHeaders.Find(HeaderFilename);
	if (Existing != nullptr)
		return *Existing;

	TSharedPtr<FSharedHeader> NewHeader = MakeShared<FSharedHeader>();
	SharedHeaders.Add(HeaderFilename, NewHeader);
	NewHeader->Filename = HeaderFilename;
	return NewHeader;
}

TSharedPtr<FAngelscriptStaticJIT::FComputedTypeOffsets> FAngelscriptStaticJIT::GetComputedOffsets(asCObjectType* ObjectType)
{
	if (ObjectType == nullptr)
		return nullptr;

	auto* Existing = ComputedOffsets.Find(ObjectType);
	if (Existing != nullptr)
		return *Existing;

	TSharedPtr<FComputedTypeOffsets> Offsets = MakeShared<FComputedTypeOffsets>();
	ComputedOffsets.Add(ObjectType, Offsets);

	if ((ObjectType->flags & asOBJ_SCRIPT_OBJECT) == 0)
	{
		if (IsTypePotentiallyDifferent(ObjectType))
		{
			// If we have a native type available, we can still compute the size
			FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(ObjectType->GetTypeId());
			if (Usage.IsValid())
			{
				FAngelscriptType::FCppForm CppForm;
				Usage.GetCppForm(CppForm);

				FString CppType = CppForm.CppType;
				if (CppType.IsEmpty() && !CppForm.CppGenericType.IsEmpty())
					CppType = CppForm.CppGenericType;

				if (!CppType.IsEmpty())
				{
					CppType.RemoveFromEnd(TEXT("*"));

					Offsets->bHasComputedSize = true;
					Offsets->ComputedSizeVariable = FString::Printf(TEXT("sizeof(%s)"), *CppType);
					Offsets->ComputedAlignmentVariable = FString::Printf(TEXT("alignof(%s)"), *CppType);
					Offsets->AdditionalHeader = CppForm.CppHeader;
				}
				else
				{
					if (ObjectType->templateBaseType != nullptr && (ObjectType->templateBaseType->flags & asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE) != 0)
					{
						auto subType = ObjectType->templateSubTypes[0];
						if (subType.IsPrimitive() || subType.IsEnumType())
						{
							Offsets->bCanHardcodeSize = true;
							Offsets->HardcodedAlignment = FMath::Max(ObjectType->templateBaseType->alignment, subType.GetAlignment());
							Offsets->HardcodedSize = Align(ObjectType->templateBaseType->size + subType.GetSizeInMemoryBytes(), Offsets->HardcodedAlignment);
						}
						else if (subType.IsReferenceType())
						{
							Offsets->bCanHardcodeSize = true;
							Offsets->HardcodedAlignment = FMath::Max((SIZE_T)ObjectType->templateBaseType->alignment, alignof(void*));
							Offsets->HardcodedSize = Align(ObjectType->size + sizeof(void*), Offsets->HardcodedAlignment);
						}
						else if (subType.IsObject())
						{
							asCObjectType* SubObjectType = (asCObjectType*)subType.GetTypeInfo();
							if (IsTypePotentiallyDifferent(SubObjectType))
							{
								auto SubTypeOffsets = GetComputedOffsets(SubObjectType);
								if (SubTypeOffsets.IsValid())
								{
									if (SubTypeOffsets->bCanHardcodeSize)
									{
										Offsets->bCanHardcodeSize = true;
										Offsets->HardcodedAlignment = FMath::Max((SIZE_T)ObjectType->templateBaseType->alignment, SubTypeOffsets->HardcodedAlignment);
										Offsets->HardcodedSize = Align(ObjectType->templateBaseType->size + SubTypeOffsets->HardcodedSize, Offsets->HardcodedAlignment);
									}
									else if (SubTypeOffsets->bHasComputedSize)
									{
										Offsets->bHasComputedSize = true;
										Offsets->ComputedSizeVariable = FString::Printf(TEXT("Align(%s + %d, AlignmentMax(%d, %s))"),
											*SubTypeOffsets->ComputedSizeVariable,
											ObjectType->templateBaseType->size,
											ObjectType->templateBaseType->alignment,
											*SubTypeOffsets->ComputedAlignmentVariable
										);
										Offsets->ComputedAlignmentVariable = FString::Printf(TEXT("AlignmentMax(%d, %s)"),
											ObjectType->templateBaseType->alignment,
											*SubTypeOffsets->ComputedAlignmentVariable);
										Offsets->AdditionalHeader = SubTypeOffsets->AdditionalHeader;
										Offsets->ContainingSharedHeader = SubTypeOffsets->ContainingSharedHeader;
									}
								}
							}
							else
							{
								Offsets->bCanHardcodeSize = true;
								Offsets->HardcodedAlignment = FMath::Max(ObjectType->templateBaseType->alignment, SubObjectType->alignment);
								Offsets->HardcodedSize = Align(ObjectType->templateBaseType->size + SubObjectType->size, Offsets->HardcodedAlignment);
							}
						}
					}
				}
			}
		}
		else
		{
			// We can hardcode the size and alignment of this C++ type
			Offsets->bCanHardcodeSize = true;
			Offsets->HardcodedSize = ObjectType->size;
			Offsets->HardcodedAlignment = ObjectType->alignment;
		}
	}
	else
	{
		FString TypeName = ANSI_TO_TCHAR(ObjectType->name.AddressOf());

		TSharedPtr<FSharedHeader> Header = ActiveSharedHeader;
		if (!Header.IsValid())
			Header = GetSharedHeader(TypeName);

		TGuardValue ScopeActivateHeader(ActiveSharedHeader, Header);
		int PropertyBaseOffset = 0;

		Offsets->ContainingSharedHeader = Header->Filename;

		// Script objects should recursively classify their properties
		TSharedPtr<FComputedTypeOffsets> ParentOffsets;
		if (ObjectType->derivedFrom != nullptr)
		{
			ParentOffsets = GetComputedOffsets(ObjectType->derivedFrom);
			PropertyBaseOffset = ObjectType->derivedFrom->size;
			if (!ParentOffsets.IsValid())
				return Offsets;
		}
		else if (ObjectType->shadowType != nullptr)
		{
			ParentOffsets = GetComputedOffsets((asCObjectType*)ObjectType->shadowType);
			PropertyBaseOffset = ObjectType->basePropertyOffset;
			if (!ParentOffsets.IsValid())
				return Offsets;
		}

		// Go through all properties in this class to compute the offsets
		bool bValidToHardcode = true;
		SIZE_T HardcodedSize = 0;
		SIZE_T HardcodedAlignment = 0;
		FString PreviousSizeVariable;
		FString PreviousAlignmentVariable;

		// Script structs are always at least alignment 8
		if ((ObjectType->flags & asOBJ_VALUE) != 0 || (ObjectType->flags & asOBJ_SCRIPT_OBJECT) != 0)
		{
			HardcodedAlignment = 8;
		}

		bool bPreviousPropertyAllowsHardcode = true;
		SIZE_T PreviousPropertyHardcodedSize = 0;
		FString PreviousPropertyAlignmentExpression;
		FString PreviousPropertySizeExpression;

		if (ParentOffsets.IsValid())
		{
			if (ParentOffsets->bCanHardcodeSize)
			{
				bValidToHardcode = true;
				HardcodedSize = ParentOffsets->HardcodedSize;
				HardcodedAlignment = ParentOffsets->HardcodedAlignment;
			}
			else if (ParentOffsets->bHasComputedSize)
			{
				bValidToHardcode = false;
				PreviousSizeVariable = ParentOffsets->ComputedSizeVariable;
				PreviousAlignmentVariable = ParentOffsets->ComputedAlignmentVariable;

				if (!ParentOffsets->ContainingSharedHeader.IsEmpty() && ParentOffsets->ContainingSharedHeader != Header->Filename)
				{
					Header->SharedHeaderDependencies.Add(ParentOffsets->ContainingSharedHeader);

					auto OtherHeader = SharedHeaders[ParentOffsets->ContainingSharedHeader];
					OtherHeader->bHasDependentSharedHeaders = true;
				}

				if (!ParentOffsets->AdditionalHeader.IsEmpty())
				{
					Header->Includes.Add(ParentOffsets->AdditionalHeader);
				}
			}
			else
			{
				return Offsets;
			}
		}

		bool bHadUnknownProperty = false;
		for (int i = 0, Count = ObjectType->properties.GetLength(); i < Count; ++i)
		{
			asCObjectProperty* Property = ObjectType->properties[i];
			if (Property == nullptr)
				continue;

			// Skip inherited properties
			if (Property->byteOffset < PropertyBaseOffset)
				continue;

			// Classify what information we have about this property
			bool bCanHardcodeProperty = false;
			SIZE_T PropertyHardcodedSize = 0;
			SIZE_T PropertyHardcodedAlignment = 0;

			bool bCanComputeProperty = false;
			FString PropertyComputedAlignment;
			FString PropertyComputedSize;

			if (Property->type.IsPrimitive() || Property->type.IsEnumType())
			{
				bCanHardcodeProperty = true;
				PropertyHardcodedSize = Property->type.GetSizeInMemoryBytes();
				PropertyHardcodedAlignment = Property->type.GetAlignment();
			}
			else if (Property->type.IsReferenceType())
			{
				bCanHardcodeProperty = true;
				PropertyHardcodedSize = sizeof(void*);
				PropertyHardcodedAlignment = alignof(void*);
			}
			else if (auto* PropertyObjectType = CastToObjectType(Property->type.GetTypeInfo()))
			{
				if (IsTypePotentiallyDifferent(PropertyObjectType))
				{
					// If we have a native type available, we can still compute the size
					FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(PropertyObjectType->GetTypeId());
					if (Usage.IsValid())
					{
						FAngelscriptType::FCppForm CppForm;
						Usage.GetCppForm(CppForm);

						FString CppType = CppForm.CppType;
						if (CppType.IsEmpty() && !CppForm.CppGenericType.IsEmpty())
							CppType = CppForm.CppGenericType;

						if (!CppType.IsEmpty())
						{
							CppType.RemoveFromEnd(TEXT("*"));
							if (!CppForm.CppHeader.IsEmpty())
								Header->Includes.Add(CppForm.CppHeader);

							bCanComputeProperty = true;
							PropertyComputedSize = FString::Printf(TEXT("sizeof(%s)"), *CppType);
							PropertyComputedAlignment = FString::Printf(TEXT("alignof(%s)"), *CppType);
						}
					}

					if (!bCanComputeProperty)
					{
						// Try to recursively compute the size of script structs if we can
						TSharedPtr<FComputedTypeOffsets> MemberOffsets = GetComputedOffsets(PropertyObjectType);
						if (MemberOffsets.IsValid())
						{
							if (MemberOffsets->bCanHardcodeSize)
							{
								bCanHardcodeProperty = true;
								PropertyHardcodedSize = MemberOffsets->HardcodedSize;
								PropertyHardcodedAlignment = MemberOffsets->HardcodedAlignment;
							}
							else if (MemberOffsets->bHasComputedSize)
							{
								if (!MemberOffsets->ContainingSharedHeader.IsEmpty() && MemberOffsets->ContainingSharedHeader != Header->Filename)
								{
									Header->SharedHeaderDependencies.Add(MemberOffsets->ContainingSharedHeader);

									auto OtherHeader = SharedHeaders[MemberOffsets->ContainingSharedHeader];
									OtherHeader->bHasDependentSharedHeaders = true;
								}

								if (!MemberOffsets->AdditionalHeader.IsEmpty())
									Header->Includes.Add(MemberOffsets->AdditionalHeader);

								bCanComputeProperty = true;
								PropertyComputedSize = MemberOffsets->ComputedSizeVariable;
								PropertyComputedAlignment = MemberOffsets->ComputedAlignmentVariable;
							}
						}
					}
				}
				else
				{
					bCanHardcodeProperty = true;
					PropertyHardcodedSize = PropertyObjectType->size;
					PropertyHardcodedAlignment = PropertyObjectType->alignment;
				}
			}

			if (!bCanHardcodeProperty && !bCanComputeProperty)
			{
				bHadUnknownProperty = true;
				break;
			}

			FString PropertyName = ANSI_TO_TCHAR(Property->name.AddressOf());
			FComputedPropertyOffset& PropertyOffset = Offsets->Properties.Emplace(PropertyName);
			if (bValidToHardcode && bCanHardcodeProperty)
			{
				HardcodedSize = Align(HardcodedSize + PreviousPropertyHardcodedSize, PropertyHardcodedAlignment);
				HardcodedAlignment = FMath::Max(HardcodedAlignment, PropertyHardcodedAlignment);

				check(HardcodedSize == Property->byteOffset);

				PropertyOffset.bCanHardcodeOffset = true;
				PropertyOffset.HardcodedOffset = HardcodedSize;
				PropertyOffset.HardcodedAlignment = HardcodedAlignment;

				bPreviousPropertyAllowsHardcode = true;
				PreviousPropertyHardcodedSize = PropertyHardcodedSize;
			}
			else
			{
				FString OffsetName = GetUniqueSymbolName(FString::Printf(TEXT("POFFSET_%s_%s"), *TypeName, *PropertyName));
				FString AlignmentName = GetUniqueSymbolName(FString::Printf(TEXT("PALIGN_%s_%s"), *TypeName, *PropertyName));

				if (bValidToHardcode)
				{
					check(!PropertyComputedSize.IsEmpty() && !PropertyComputedAlignment.IsEmpty());
					check(bPreviousPropertyAllowsHardcode);

					Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%d + %d, %s);\n"), *OffsetName, HardcodedSize, PreviousPropertyHardcodedSize, *PropertyComputedAlignment));
					Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = AlignmentMax(%d, %s);\n"), *AlignmentName, HardcodedAlignment, *PropertyComputedAlignment));
				}
				else if (bCanHardcodeProperty)
				{
					if (bPreviousPropertyAllowsHardcode)
						Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%s + %d, %d);\n"), *OffsetName, *PreviousSizeVariable, PreviousPropertyHardcodedSize, PropertyHardcodedAlignment));
					else
						Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%s + %s, %d);\n"), *OffsetName, *PreviousSizeVariable, *PreviousPropertySizeExpression, PropertyHardcodedAlignment));

					Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = AlignmentMax(%s, %d);\n"), *AlignmentName, *PreviousAlignmentVariable, PropertyHardcodedAlignment));
				}
				else
				{
					if (bPreviousPropertyAllowsHardcode)
						Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%s + %d, %s);\n"), *OffsetName, *PreviousSizeVariable, PreviousPropertyHardcodedSize, *PropertyComputedAlignment));
					else
						Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%s + %s, %s);\n"), *OffsetName, *PreviousSizeVariable, *PreviousPropertySizeExpression, *PropertyComputedAlignment));

					Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = AlignmentMax(%s, %s);\n"), *AlignmentName, *PreviousAlignmentVariable, *PropertyComputedAlignment));
				}

				PropertyOffset.bCanHardcodeOffset = false;
				PropertyOffset.ComputedOffsetVariable = OffsetName;
				PropertyOffset.ComputedAlignmentVariable = AlignmentName;

				PreviousSizeVariable = OffsetName;
				PreviousAlignmentVariable = AlignmentName;

				bPreviousPropertyAllowsHardcode = bCanHardcodeProperty;
				PreviousPropertyAlignmentExpression = PropertyComputedAlignment;
				PreviousPropertySizeExpression = PropertyComputedSize;
				PreviousPropertyHardcodedSize = PropertyHardcodedSize;

				bValidToHardcode = false;
			}
		}

		if (!bHadUnknownProperty)
		{
			if (bValidToHardcode)
			{
				Offsets->bCanHardcodeSize = true;
				Offsets->HardcodedSize = Align(HardcodedSize, HardcodedAlignment);
				Offsets->HardcodedAlignment = HardcodedAlignment;

				check(Offsets->HardcodedSize == ObjectType->size);
				check(Offsets->HardcodedAlignment == ObjectType->alignment);
			}
			else
			{
				FString SizeName = GetUniqueSymbolName(FString::Printf(TEXT("TSIZE_%s"), *TypeName));
				FString AlignmentName = GetUniqueSymbolName(FString::Printf(TEXT("TALIGN_%s"), *TypeName));

				Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = %s;\n"), *AlignmentName, *PreviousAlignmentVariable));

				if (bPreviousPropertyAllowsHardcode)
					Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%s + %d, %s);\n"), *SizeName, *PreviousSizeVariable, PreviousPropertyHardcodedSize, *AlignmentName));
				else
					Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%s + %s, %s);\n"), *SizeName, *PreviousSizeVariable, *PreviousPropertySizeExpression, *AlignmentName));

				Offsets->bHasComputedSize = true;
				Offsets->ComputedSizeVariable = SizeName;
				Offsets->ComputedAlignmentVariable = AlignmentName;
			}
		}
	}

	return Offsets;
}

void FAngelscriptStaticJIT::GenerateCppCode(asIScriptFunction* ScriptFunction, FGenerateFunction& Generate)
{
	if (PrecompiledData == nullptr)
	{
		ensure(false);
		return;
	}

	uint32 FunctionId = PrecompiledData->CreateFunctionId(ScriptFunction);
	FJITFile& File = GetFile(ScriptFunction->GetModule());
	const int32 FirstFunctionContentIndex = File.Content.Num();

	FString Code;

	asUINT BytecodeLength;

	FStaticJITContext Context;
	Context.FunctionId = FunctionId;
	Context.ScriptFunction = (asCScriptFunction*)ScriptFunction;
	Context.File = &File;
	Context.FunctionName = Generate.FunctionSymbolName;
	Context.JIT = this;
	Context.BC = ScriptFunction->GetByteCode(&BytecodeLength);
	Context.BC_FunctionStart = Context.BC;
	Context.BC_End = Context.BC + BytecodeLength;

	// Lookup all instructions in the bytecode
	while (Context.BC < Context.BC_End)
	{
		asEBCInstr Instr = Context.GetInstr();
		FAngelscriptBytecode& Bytecode = FAngelscriptBytecode::GetBytecode(Instr);

		FStaticJITContext::FInstruction& NewInstr = Context.Instructions.Emplace_GetRef();
		NewInstr.BC = Context.BC;
		NewInstr.Bytecode = &Bytecode;

		Context.AdvanceBC();
	}

	// Run the pre-pass to add labels where needed
	for (int32 i = 0, Count = Context.Instructions.Num(); i < Count; ++i)
	{
		Context.CurrentInstructionIndex = i;
		Context.BC = Context.Instructions[i].BC;
		Context.Instructions[i].Bytecode->PrePass(Context);
	}

	// Handle stack offsets for all labels
	Context.LocalStackSize = (Context.ScriptFunction->scriptData->stackNeeded - Context.ScriptFunction->scriptData->variableSpace);
	Context.LocalStackOffset = Context.LocalStackSize;

	if (Context.Instructions.Num() != 0)
	{
		TArray<int32> PathsToCheck;
		PathsToCheck.Add(0);
		Context.Instructions[0].StackOffsetBeforeInstr = Context.LocalStackSize;
		Context.Instructions[0].bMarked = true;

		while (PathsToCheck.Num() > 0)
		{
			int32 CurPath = PathsToCheck[0];
			PathsToCheck.RemoveAt(0);

			FStaticJITContext::FInstruction& StartInstr = Context.Instructions[CurPath];
			check(StartInstr.bMarked);

			int32 StackOffset = StartInstr.StackOffsetBeforeInstr;
			StartInstr.bMarked = false;

			while (CurPath < Context.Instructions.Num())
			{
				FStaticJITContext::FInstruction& Instr = Context.Instructions[CurPath];
				if (Instr.bMarked)
				{
					check(Instr.StackOffsetBeforeInstr == StackOffset);
					break;
				}

				Instr.StackOffsetBeforeInstr = StackOffset;
				Instr.bMarked = true;

				Context.CurrentInstructionIndex = CurPath;
				Context.BC = Instr.BC;

				StackOffset -= Instr.Bytecode->GetStackSizeChange(Context);
				check(StackOffset >= 0);
				check(StackOffset <= Context.LocalStackSize);

				if (Instr.JumpTargets.Num() != 0)
				{
					for (int32 JumpIndex : Instr.JumpTargets)
					{
						auto& JumpInstr = Context.Instructions[JumpIndex];
						if (JumpInstr.bMarked)
						{
							check(JumpInstr.StackOffsetBeforeInstr == StackOffset);
							continue;
						}

						JumpInstr.bMarked = true;
						JumpInstr.StackOffsetBeforeInstr = StackOffset;
						PathsToCheck.Add(JumpIndex);
					}

					if (!Instr.bIsConditionalJump)
						break;
				}

				CurPath += 1;
			}
		}
	}

	// Reset the context after the pre-pass
	Context.BC = Context.BC_FunctionStart;

	// Start a new function 
	Context.GenerateNewFunction(ScriptFunction);
	Generate.FunctionDeclaration = Context.FunctionDecl;

	for (int32 i = 0, Count = Context.Instructions.Num(); i < Count; ++i)
	{
		Context.CurrentInstructionIndex = i;
		Context.BC = Context.Instructions[i].BC;

		asEBCInstr Instr = Context.GetInstr();

		FAngelscriptBytecode& Bytecode = FAngelscriptBytecode::GetBytecode(Instr);
		Context.NewInstruction();
		Context.UsageFlags |= Bytecode.GetUsageFlags(Context);

		Bytecode.WriteDebugComment(Context);
		bool bImplemented = Bytecode.Implement(Context);
		check(bImplemented);
	}

	// Add blocks for any exception cleanup we need to do
	if (Context.ExceptionCleanupLabels.Num() != 0)
	{
		Context.Line("SCRIPT_ASSUME(false);\n");

		for (FStaticJITContext::FExceptionCleanup& Cleanup : Context.ExceptionCleanupLabels)
		{
			FString Variables;
			for (int Pos : Cleanup.Positions)
			{
				if (!Variables.IsEmpty())
					Variables += TEXT(", ");
				Variables += FString::FromInt(Pos);
			}

			Context.Line("{0}:", Cleanup.Label);
			Context.Line("// Exception cleanup for variables {0}", Variables);

			if (Cleanup.bCalledAddNullptrException)
			{
				if (Cleanup.bCalledWithoutAddNullptrException)
				{
					// Can be called both with or without an existing exception
					Context.Line("if (!Execution.bExceptionThrown) { SCRIPT_NULL_POINTER_EXCEPTION(); }");
				}
				else
				{
					// Always adds a nullpointer exception
					Context.Line("SCRIPT_NULL_POINTER_EXCEPTION();");
				}
			}

			for (int i = 0, Count = Cleanup.Positions.Num(); i < Count; ++i)
			{
				int Pos = Cleanup.Positions[i];
				asCObjectType* ObjectType = Cleanup.Types[i];

				FDestructorCall DestructorCall(ObjectType);
				DestructorCall.CallDestroy(Context, Context.VAddress(Pos));
			}

			Context.ReturnEmptyValue();
		}
	}

	Context.WriteOutFunction();
	const bool bEmittedParmsEntry = File.Content.ContainsByPredicate(
		[&Generate](const FString& Content)
		{
			return Content.Contains(
				Generate.FunctionSymbolName + TEXT("_ParmsEntry"));
		});
	const bool bExpectedParmsEntry =
		FAngelscriptStaticJIT::CanGenerateParmsEntry(ScriptFunction);
	ensureMsgf(bEmittedParmsEntry == bExpectedParmsEntry,
		TEXT("StaticJIT Parms-entry capability drifted between emission and Runtime ABI derivation for %s."),
		UTF8_TO_TCHAR(ScriptFunction->GetDeclaration()));
	Generate.bHasParmsEntry = bEmittedParmsEntry;
	Generate.EmittedFunctionContent.Reset();
	for (int32 ContentIndex = FirstFunctionContentIndex;
		ContentIndex < File.Content.Num();
		++ContentIndex)
	{
		Generate.EmittedFunctionContent += File.Content[ContentIndex];
	}
}

void FAngelscriptStaticJIT::DetectScriptType(asCObjectType* ObjectType)
{
	// For each virtual method we override from the base type, mark the base type's functions
	// as not being final anymore.
	for (int32 i = 0, Count = ObjectType->virtualFunctionTable.GetLength(); i < Count; ++i)
	{
		auto* Function = ObjectType->virtualFunctionTable[i];
		if (Function->vfTableIdx == -1)
			continue;

		// We inherited this function from a base class and didn't override it, we don't need to do anything
		if (Function->objectType != ObjectType)
			continue;
		
		auto* BaseType = ObjectType->derivedFrom;
		while (BaseType != nullptr)
		{
			if (Function->vfTableIdx < (int)BaseType->virtualFunctionTable.GetLength())
			{
				asCScriptFunction* BaseFunction = BaseType->virtualFunctionTable[Function->vfTableIdx];
				FunctionsWithVirtualOverrides.Add(BaseFunction);

				BaseType = BaseFunction->objectType->derivedFrom;
			}
			else
			{
				break;
			}
		}
	}
}

asCScriptFunction* FAngelscriptStaticJIT::DevirtualizeFunction(asCScriptFunction* VirtualFunction)
{
	if (VirtualFunction->vfTableIdx != -1)
	{
		if (bAllowDevirtualize && !FunctionsWithVirtualOverrides.Contains(VirtualFunction))
		{
			return VirtualFunction;
		}
	}
	else if (VirtualFunction->funcType == asFUNC_IMPORTED)
	{
		if (bAllowDevirtualize)
		{
			// Devirtualize to the function that is bound currently
			int BoundId = Engine->importedFunctions[(int)VirtualFunction->GetId() & ~FUNC_IMPORTED]->boundFunctionId;
			if (BoundId != -1)
				return Engine->scriptFunctions[BoundId];
		}
	}
	else if (VirtualFunction->funcType == asFUNC_SCRIPT)
	{
		return VirtualFunction;
	}

	return nullptr;
}

bool FAngelscriptStaticJIT::IsFunctionAlwaysJIT(asCScriptFunction* VirtualFunction)
{
	if (VirtualFunction->vfTableIdx == -1)
		return FunctionsToGenerate.Contains(VirtualFunction);

	if (bAllowComprehensiveJIT)
		return true;

	return false;
}

void FAngelscriptStaticJIT::AnalyzeScriptFunction(asCScriptFunction* ScriptFunction, FGenerateFunction& Generate)
{
	FJITFile& File = GetFile(ScriptFunction->GetModule());
	if (Generate.FunctionSymbolName.IsEmpty())
	{
		Generate.FunctionSymbolName = GetUniqueFunctionName(
			File, ScriptFunction, 0);
	}

	// If nothing overrides this method, we mark it as final, this allows some runtime
	// devirtualization optimizations to happen when loading from precompiled scripts.
	if (!FunctionsWithVirtualOverrides.Contains(ScriptFunction))
	{
		ScriptFunction->traits.SetTrait(asTRAIT_FINAL, true);
	}
	else
	{
		check(!ScriptFunction->traits.GetTrait(asTRAIT_FINAL));
	}
}

namespace
{
	static void CollectStableProviderStringLiteralReferences(
		const asCScriptFunction& Function,
		const asCScriptEngine& Engine,
		TArray<FAngelscriptArtifactReference>& InOutReferences)
	{
		if (Function.scriptData == nullptr)
		{
			return;
		}
		asDWORD* Bytecode = Function.scriptData->byteCode.AddressOf();
		asDWORD* const BytecodeEnd = Bytecode
			+ Function.scriptData->byteCode.GetLength();
		while (Bytecode < BytecodeEnd)
		{
			const asEBCInstr Instruction =
				static_cast<asEBCInstr>(*reinterpret_cast<asBYTE*>(Bytecode));
			if (Instruction == asBC_PGA || Instruction == asBC_PshGPtr)
			{
				void* const Storage = reinterpret_cast<void*>(asBC_PTRARG(Bytecode));
				if (Storage != nullptr && Engine.varAddressMap.Find(Storage) == nullptr)
				{
					const FString& Literal = *static_cast<const FString*>(Storage);
					InOutReferences.AddUnique(
						FAngelscriptJITReferenceDescriptorBuilder::ForStringLiteral(
							Literal));
				}
			}
			Bytecode += asBCTypeSize[asBCInfo[Instruction].type];
		}
	}
}

bool GenerateStaticJITProviderArtifacts(
	asIScriptModule* Module,
	const FAngelscriptJITProviderGenerationSettings& Settings,
	FAngelscriptJITGenerationOutput& OutOutput,
	FString* OutError)

{
	asIScriptModule* Modules[] = {Module};
	return GenerateStaticJITProviderArtifacts(
		MakeArrayView(Modules), Settings, OutOutput, OutError);
}

static bool GenerateStaticJITProviderArtifactsImpl(
	TConstArrayView<asIScriptModule*> Modules,
	const FAngelscriptJITProviderGenerationSettings& Settings,
	FAngelscriptJITGenerationOutput& OutOutput,
	FString* OutError,
	const FAngelscriptFunctionRouteSnapshot* OverrideRouteSnapshot,
	const FAngelscriptCacheSuccessfulPublicationDto* OverridePublication);

bool GenerateStaticJITProviderArtifacts(
	const TConstArrayView<asIScriptModule*> Modules,
	const FAngelscriptJITProviderGenerationSettings& Settings,
	FAngelscriptJITGenerationOutput& OutOutput,
	FString* OutError)
{
	return GenerateStaticJITProviderArtifactsImpl(
		Modules, Settings, OutOutput, OutError, nullptr, nullptr);
}

static bool GenerateStaticJITProviderArtifactsImpl(
	const TConstArrayView<asIScriptModule*> Modules,
	const FAngelscriptJITProviderGenerationSettings& Settings,
	FAngelscriptJITGenerationOutput& OutOutput,
	FString* OutError,
	const FAngelscriptFunctionRouteSnapshot* OverrideRouteSnapshot,
	const FAngelscriptCacheSuccessfulPublicationDto* OverridePublication)
{
	OutOutput = {};
	auto Fail = [OutError](const FString& Message)
	{
		if (OutError != nullptr)
		{
			*OutError = Message;
		}
		return false;
	};

	if (Modules.IsEmpty())
	{
		return Fail(TEXT("StaticJIT Provider generation failed: no modules were supplied."));
	}
	asCModule* const ScriptModule =
		reinterpret_cast<asCModule*>(Modules[0]);
	if (ScriptModule == nullptr)
	{
		return Fail(TEXT("StaticJIT Provider generation failed: module was null."));
	}
	asCScriptEngine* const ScriptEngine =
		reinterpret_cast<asCScriptEngine*>(ScriptModule->GetEngine());
	FAngelscriptEngine* const RuntimeEngine =
		FAngelscriptEngine::TryGetByScriptEngine(ScriptEngine);
	if (ScriptEngine == nullptr || RuntimeEngine == nullptr)
	{
		return Fail(TEXT("StaticJIT Provider generation failed: module has no Runtime Engine owner."));
	}
	if (!Settings.ProviderId.IsValid()
		|| !FAngelscriptJITTargetProfiles::GetTraits(
			Settings.TargetProfile).IsSet()
		|| Settings.ProviderName.IsEmpty()
		|| Settings.OwnerModuleName.IsEmpty()
		|| Settings.ArtifactProfile.Hash.IsZero()
		|| Settings.NativeEnvironmentFingerprint.IsZero())
	{
		return Fail(TEXT("StaticJIT Provider generation failed: Provider settings are incomplete."));
	}

	const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
		ESPMode::ThreadSafe> RouteSnapshot =
		RuntimeEngine->GetFunctionRouteSnapshot();
	const FAngelscriptFunctionRouteSnapshot* const ActiveRouteSnapshot =
		OverrideRouteSnapshot != nullptr
		? OverrideRouteSnapshot : RouteSnapshot.Get();
	if (ActiveRouteSnapshot == nullptr)
	{
		return Fail(TEXT("StaticJIT Provider generation failed: current function routes are unavailable."));
	}

	FAngelscriptStaticJIT JIT;
	FAngelscriptPrecompiledData PrecompiledData(ScriptEngine);
	JIT.Engine = ScriptEngine;
	JIT.PrecompiledData = &PrecompiledData;
	JIT.bGenerateOutputCode = true;
	JIT.bEmitDebugMetadataInOutput = Settings.bEmitDebugMetadata;
	JIT.bEmitProviderEntryLinkage = true;
	JIT.bUseImmutableDirectScriptCalls = false;
	TSet<asCModule*> SeenModules;
	for (asIScriptModule* Module : Modules)
	{
		asCModule* const CurrentModule = reinterpret_cast<asCModule*>(Module);
		if (CurrentModule == nullptr
			|| CurrentModule->GetEngine() != ScriptEngine)
		{
			return Fail(
				TEXT("StaticJIT Provider generation failed: every module must belong to one Runtime Engine."));
		}
		if (!SeenModules.Contains(CurrentModule))
		{
			SeenModules.Add(CurrentModule);
			JIT.CollectModuleFunctionsForGeneration(CurrentModule);
		}
	}
	PrecompiledData.InitFromActiveScript();

	struct FFunctionAndRoute
	{
		asCScriptFunction* Function = nullptr;
		const FAngelscriptFunctionRoute* Route = nullptr;
	};
	TArray<FFunctionAndRoute> OrderedFunctions;
	OrderedFunctions.Reserve(JIT.FunctionsToGenerate.Num());
	for (const TPair<asCScriptFunction*,
		FAngelscriptStaticJIT::FGenerateFunction>& FunctionPair :
		JIT.FunctionsToGenerate)
	{
		const FAngelscriptFunctionRoute* const Route =
			ActiveRouteSnapshot->FunctionRoutes.FindByPredicate(
				[Function = FunctionPair.Key](
					const FAngelscriptFunctionRoute& Candidate)
				{
					return Candidate.Function == Function;
				});
		// CollectModuleFunctionsForGeneration also sees VM-generated helpers
		// such as a script class' synthetic StaticClass function. A Provider is
		// defined by the current verified artifact publication, so candidates
		// with no verified route are intentionally outside this generation.
		if (Route == nullptr || !Route->bHasVerifiedArtifactIdentity)
		{
			continue;
		}
		if (Route->ModuleKey.Hash.IsZero()
			|| Route->Identity.FunctionKey.Hash.IsZero()
			|| Route->Identity.Content.Execution.IsZero()
			|| Route->Identity.Content.Debug.IsZero())
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Provider generation failed: function '%s' has no complete current stable artifact identity."),
				ANSI_TO_TCHAR(FunctionPair.Key->GetDeclaration())));
		}
		if (!(Route->Identity.Profile.Hash
			== Settings.ArtifactProfile.Hash))
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Provider generation failed: function %s uses profile %s, expected %s."),
				*Route->Identity.FunctionKey.Hash.ToHexString(),
				*Route->Identity.Profile.Hash.ToHexString(),
				*Settings.ArtifactProfile.Hash.ToHexString()));
		}
		OrderedFunctions.Add({FunctionPair.Key, Route});
	}
	if (OrderedFunctions.IsEmpty())
	{
		return Fail(TEXT("StaticJIT Provider generation failed: module has no eligible script functions."));
	}
	OrderedFunctions.Sort([](
		const FFunctionAndRoute& Left,
		const FFunctionAndRoute& Right)
	{
		if (!(Left.Route->ModuleKey == Right.Route->ModuleKey))
		{
			return Left.Route->ModuleKey.Hash < Right.Route->ModuleKey.Hash;
		}
		return Left.Route->Identity.FunctionKey.Hash
			< Right.Route->Identity.FunctionKey.Hash;
	});

	FAngelscriptCacheService* const CacheService =
		RuntimeEngine->GetCacheService();
	const FAngelscriptCacheLifecyclePublications Publications =
		CacheService != nullptr
			? CacheService->GetLifecyclePublications()
			: FAngelscriptCacheLifecyclePublications{};
	const FAngelscriptCacheSuccessfulPublicationDto* const CurrentPublication =
		OverridePublication != nullptr
		? OverridePublication : Publications.Current.Get();
	if (CurrentPublication == nullptr)
	{
		return Fail(TEXT("StaticJIT Provider generation failed: the current verified artifact publication is unavailable."));
	}
	if (!(CurrentPublication->Profile.Hash
		== Settings.ArtifactProfile.Hash))
	{
		return Fail(TEXT("StaticJIT Provider generation failed: the current artifact publication uses a different profile."));
	}
	for (const FFunctionAndRoute& FunctionAndRoute : OrderedFunctions)
	{
		const FAngelscriptCacheCleanModuleArtifacts* const PublishedModule =
			CurrentPublication->Modules.FindByPredicate(
				[ModuleKey = FunctionAndRoute.Route->ModuleKey](
					const FAngelscriptCacheCleanModuleArtifacts& Candidate)
				{
					return Candidate.ModuleKey == ModuleKey;
				});
		const FAngelscriptFunctionArtifactReferenceSet* const ReferenceSet =
			PublishedModule != nullptr
				? PublishedModule->ValidatedFunctionArtifactReferenceSets.
					FindByPredicate(
						[FunctionKey = FunctionAndRoute.Route->Identity.FunctionKey](
							const FAngelscriptFunctionArtifactReferenceSet& Candidate)
						{
							return Candidate.FunctionKey == FunctionKey;
						})
				: nullptr;
		if (ReferenceSet == nullptr)
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Provider generation failed: function %s has no verified stable reference set."),
				*FunctionAndRoute.Route->Identity.FunctionKey.Hash.ToHexString()));
		}
		if (PublishedModule == nullptr
			|| FunctionAndRoute.Function == nullptr
			|| FunctionAndRoute.Function->module == nullptr)
		{
			return Fail(TEXT("StaticJIT Provider generation failed: verified function has no published module owner."));
		}
		JIT.StableProviderModuleKeys.FindOrAdd(
			FunctionAndRoute.Function->module) = PublishedModule->ModuleKey;
		TArray<FAngelscriptArtifactReference>& EffectiveReferences =
			JIT.StableProviderReferences.FindOrAdd(FunctionAndRoute.Function);
		EffectiveReferences = ReferenceSet->RequiredReferences;
		CollectStableProviderStringLiteralReferences(
			*FunctionAndRoute.Function, *ScriptEngine, EffectiveReferences);
		EffectiveReferences.Sort();
		for (int32 ReferenceIndex = 0;
			ReferenceIndex < EffectiveReferences.Num();
			++ReferenceIndex)
		{
			const FAngelscriptArtifactReference& Reference =
				EffectiveReferences[ReferenceIndex];
			if (Reference.Kind
				!= EAngelscriptArtifactReferenceKind::ScriptFunction)
			{
				continue;
			}
			const FAngelscriptFunctionRoute* const ReferencedRoute =
				ActiveRouteSnapshot->FunctionRoutes.FindByPredicate(
					[StableKey = Reference.StableKey](
						const FAngelscriptFunctionRoute& Candidate)
					{
						return Candidate.Function != nullptr
							&& Candidate.Identity.FunctionKey.Hash == StableKey;
					});
			if (ReferencedRoute != nullptr)
			{
				JIT.StableProviderFunctionReferenceSlots.
					FindOrAdd(FunctionAndRoute.Function).Add(
						static_cast<asCScriptFunction*>(ReferencedRoute->Function),
						static_cast<uint32>(ReferenceIndex));
			}
		}
	}
	for (asUINT ModuleIndex = 0;
		ModuleIndex < ScriptEngine->scriptModules.GetLength();
		++ModuleIndex)
	{
		asCModule* const CurrentModule =
			ScriptEngine->scriptModules[ModuleIndex];
		if (CurrentModule == nullptr)
		{
			continue;
		}
		for (asUINT TypeIndex = 0;
			TypeIndex < CurrentModule->classTypes.GetLength();
			++TypeIndex)
		{
			if (asCObjectType* const ObjectType =
				CurrentModule->classTypes[TypeIndex])
			{
				JIT.DetectScriptType(ObjectType);
			}
		}
	}

	for (const FFunctionAndRoute& FunctionAndRoute : OrderedFunctions)
	{
		FAngelscriptStaticJIT::FGenerateFunction& Generate =
			JIT.FunctionsToGenerate.FindChecked(FunctionAndRoute.Function);
		Generate.FunctionSymbolName =
			FAngelscriptJITGeneration::BuildSymbolPrefix(
				FunctionAndRoute.Route->Identity.FunctionKey,
				FunctionAndRoute.Route->Identity.Content.Execution);
		JIT.UsedUniqueNames.Add(Generate.FunctionSymbolName);
		JIT.AnalyzeScriptFunction(FunctionAndRoute.Function, Generate);
	}
	for (const FFunctionAndRoute& FunctionAndRoute : OrderedFunctions)
	{
		FAngelscriptStaticJIT::FGenerateFunction& Generate =
			JIT.FunctionsToGenerate.FindChecked(FunctionAndRoute.Function);
		JIT.GenerateCppCode(FunctionAndRoute.Function, Generate);
	}
	if (!JIT.StableProviderReferenceError.IsEmpty())
	{
		return Fail(JIT.StableProviderReferenceError);
	}

	FAngelscriptJITGenerationRequest Request;
	Request.TargetProfile = Settings.TargetProfile;
	Request.ProviderId = Settings.ProviderId;
	Request.ProviderName = Settings.ProviderName;
	Request.OwnerModuleName = Settings.OwnerModuleName;
	Request.ArtifactProfile = Settings.ArtifactProfile;
	Request.NativeEnvironmentFingerprint =
		Settings.NativeEnvironmentFingerprint;
	TMap<asIScriptModule*, TSharedPtr<FAngelscriptModuleDesc>> ModuleDescs;
	for (const FFunctionAndRoute& FunctionAndRoute : OrderedFunctions)
	{
		const FAngelscriptCacheCleanModuleArtifacts* const PublishedModule =
			CurrentPublication->Modules.FindByPredicate(
				[ModuleKey = FunctionAndRoute.Route->ModuleKey](
					const FAngelscriptCacheCleanModuleArtifacts& Candidate)
				{
					return Candidate.ModuleKey == ModuleKey;
				});
		if (PublishedModule == nullptr
			|| PublishedModule->CanonicalModuleName.IsEmpty())
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Provider generation failed: current publication does not name module %s."),
				*FunctionAndRoute.Route->ModuleKey.Hash.ToHexString()));
		}
		asIScriptModule* const ScriptFunctionModule =
			FunctionAndRoute.Function->GetModule();
		TSharedPtr<FAngelscriptModuleDesc>& ModuleDesc =
			ModuleDescs.FindOrAdd(ScriptFunctionModule);
		if (!ModuleDesc.IsValid())
		{
			ModuleDesc = RuntimeEngine->GetModule(ScriptFunctionModule);
		}
		if (!ModuleDesc.IsValid() || ModuleDesc->Code.Num() != 1
			|| ModuleDesc->Code[0].VirtualPath.IsEmpty())
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Provider generation requires module %s to resolve to exactly one virtual AngelScript source."),
				*PublishedModule->CanonicalModuleName));
		}
		if (!Request.Modules.ContainsByPredicate(
			[ModuleKey = PublishedModule->ModuleKey](
				const FAngelscriptJITGenerationModule& Candidate)
			{
				return Candidate.ModuleKey == ModuleKey;
			}))
		{
			Request.Modules.Add({PublishedModule->ModuleKey,
				PublishedModule->CanonicalModuleName,
				ModuleDesc->Code[0].VirtualPath});
		}
	}

	TSet<FString> HeaderSet;
	for (const TPair<asIScriptModule*, TSharedPtr<FJITFile>>& FilePair :
		JIT.JITFiles)
	{
		if (!FilePair.Value.IsValid())
		{
			continue;
		}
		for (const FString& Header : FilePair.Value->Headers)
		{
			HeaderSet.Add(Header);
		}
	}
	for (const TPair<FString, TSharedPtr<
		FAngelscriptStaticJIT::FSharedHeader>>& HeaderPair : JIT.SharedHeaders)
	{
		if (!HeaderPair.Value.IsValid())
		{
			continue;
		}
		for (const FString& Include : HeaderPair.Value->Includes)
		{
			HeaderSet.Add(Include);
		}
	}

	TArray<FString> SortedHeaders = HeaderSet.Array();
	auto StableStringLess = [](const FString& Left, const FString& Right)
	{
		return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(
			Left, Right) < 0;
	};
	SortedHeaders.Sort(StableStringLess);

	Request.ModuleSourcePreamble = PROVIDER_STANDARD_HEADER;
	for (const FString& Header : SortedHeaders)
	{
		Request.ModuleSourcePreamble += Header + TEXT("\n");
	}
	Request.ModuleSourcePreamble += TEXT("\n") + STANDARD_INCLUDES + TEXT("\n");
	Request.ModuleSourcePreamble += TEXT("\n");

	TArray<TPair<FString, TSharedPtr<
		FAngelscriptStaticJIT::FSharedHeader>>> SortedSharedHeaders;
	for (const TPair<FString, TSharedPtr<
		FAngelscriptStaticJIT::FSharedHeader>>& HeaderPair : JIT.SharedHeaders)
	{
		SortedSharedHeaders.Add(HeaderPair);
	}
	SortedSharedHeaders.Sort([&StableStringLess](const auto& Left, const auto& Right)
	{
		return StableStringLess(Left.Key, Right.Key);
	});
	for (const auto& HeaderPair : SortedSharedHeaders)
	{
		if (!HeaderPair.Value.IsValid())
		{
			continue;
		}
		for (const FString& Content : HeaderPair.Value->Content)
		{
			Request.ModuleSourcePreamble += Content;
			if (!Content.EndsWith(TEXT("\n")))
			{
				Request.ModuleSourcePreamble += TEXT("\n");
			}
		}
	}
	Request.ModuleSourceFooter = PROVIDER_STANDARD_FOOTER;
	Request.ProviderSourcePreamble =
		TEXT("#include \"StaticJIT/StaticJITHeader.h\"\n\n");

	for (const FFunctionAndRoute& FunctionAndRoute : OrderedFunctions)
	{
		const FAngelscriptStaticJIT::FGenerateFunction& GeneratedCode =
			JIT.FunctionsToGenerate.FindChecked(FunctionAndRoute.Function);
		FAngelscriptJITGenerationFunction& Function =
			Request.Functions.Emplace_GetRef();
		Function.ModuleKey = FunctionAndRoute.Route->ModuleKey;
		Function.FunctionKey =
			FunctionAndRoute.Route->Identity.FunctionKey;
		Function.ExecutionHash =
			FunctionAndRoute.Route->Identity.Content.Execution;
		Function.DebugHash =
			FunctionAndRoute.Route->Identity.Content.Debug;
		Function.CanonicalDeclaration =
			FunctionAndRoute.Route->CanonicalDeclaration;
		const TSharedPtr<FAngelscriptModuleDesc>* const ModuleDescPtr =
			ModuleDescs.Find(FunctionAndRoute.Function->GetModule());
		const char* const ScriptSectionAnsi =
			FunctionAndRoute.Function->GetScriptSectionName();
		if (ModuleDescPtr == nullptr || !ModuleDescPtr->IsValid()
			|| ScriptSectionAnsi == nullptr)
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Provider generation cannot resolve source metadata for function %s."),
				*Function.FunctionKey.Hash.ToHexString()));
		}
		const FString ScriptSection = ANSI_TO_TCHAR(ScriptSectionAnsi);
		const FAngelscriptModuleDesc::FCodeSection* const CodeSection =
			(*ModuleDescPtr)->Code.FindByPredicate(
				[&ScriptSection](
					const FAngelscriptModuleDesc::FCodeSection& Candidate)
				{
					return ScriptSection.Equals(Candidate.AbsoluteFilename,
							ESearchCase::CaseSensitive)
						|| ScriptSection.Equals(Candidate.VirtualPath,
							ESearchCase::CaseSensitive)
						|| ScriptSection.Equals(Candidate.RelativeFilename,
							ESearchCase::CaseSensitive)
						|| (!Candidate.AbsoluteFilename.IsEmpty()
							&& ScriptSection.Equals(Candidate.AbsoluteFilename,
								ESearchCase::IgnoreCase));
				});
		if (CodeSection == nullptr || CodeSection->VirtualPath.IsEmpty())
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Provider generation cannot map section %s to a virtual source for function %s."),
				*ScriptSection,
				*Function.FunctionKey.Hash.ToHexString()));
		}
		Function.SourceSection = CodeSection->VirtualPath;
		// Prefer the declaration itself over the first bytecode line. Compiler-
		// generated class functions (default constructors, factories,
		// __InitDefaults, and destructors) intentionally carry declaredAt=0 in
		// this AngelScript fork, so map those back to their owning/returned type's
		// declaration before falling back to the preprocessor's class descriptor
		// and finally bytecode debug information.
		asCTypeInfo* DeclaringType =
			FunctionAndRoute.Function->objectType;
		if (DeclaringType == nullptr)
		{
			DeclaringType =
				FunctionAndRoute.Function->returnType.GetTypeInfo();
		}
		int32 PackedSourceLocation =
			FunctionAndRoute.Function->scriptData != nullptr
				? FunctionAndRoute.Function->scriptData->declaredAt : 0;
		if ((PackedSourceLocation & 0xFFFFF) == 0)
		{
			if (DeclaringType != nullptr
				&& (DeclaringType->declaredAt & 0xFFFFF) != 0)
			{
				PackedSourceLocation = DeclaringType->declaredAt;
			}
		}
		if ((PackedSourceLocation & 0xFFFFF) == 0
			&& DeclaringType != nullptr)
		{
			const TSharedRef<FAngelscriptClassDesc>* const ClassDesc =
				(*ModuleDescPtr)->Classes.FindByPredicate(
					[DeclaringType](
						const TSharedRef<FAngelscriptClassDesc>& Candidate)
					{
						return Candidate->ScriptType == DeclaringType;
					});
			if (ClassDesc != nullptr && (*ClassDesc)->LineNumber > 0)
			{
				PackedSourceLocation = (*ClassDesc)->LineNumber & 0xFFFFF;
			}
		}
		if ((PackedSourceLocation & 0xFFFFF) == 0)
		{
			PackedSourceLocation =
				FunctionAndRoute.Function->GetLineNumber(0, nullptr);
		}
		Function.SourceLine = static_cast<uint32>(
			FMath::Max(0, PackedSourceLocation & 0xFFFFF));
		Function.SourceColumn = static_cast<uint32>(
			FMath::Max(0, PackedSourceLocation >> 20));
		Function.bHasVMEntry = true;
		Function.bHasRawEntry = true;
		Function.bHasParmsEntry = GeneratedCode.bHasParmsEntry;
		Function.EntryAbiHash =
			FAngelscriptJITGeneration::BuildEntryAbiHash(
				FunctionAndRoute.Route->CanonicalDeclaration,
				Function.bHasVMEntry,
				Function.bHasRawEntry,
				Function.bHasParmsEntry);
		Function.VMEntrySymbol =
			GeneratedCode.FunctionSymbolName + TEXT("_VMEntry");
		Function.RawEntrySymbol = GeneratedCode.FunctionSymbolName;
		Function.ParmsEntrySymbol = GeneratedCode.bHasParmsEntry
			? GeneratedCode.FunctionSymbolName + TEXT("_ParmsEntry") : FString();
		Function.RawEntryDeclaration = GeneratedCode.FunctionDeclaration;
		const FAngelscriptCacheCleanModuleArtifacts* const PublishedModule =
			CurrentPublication->Modules.FindByPredicate(
				[ModuleKey = Function.ModuleKey](
					const FAngelscriptCacheCleanModuleArtifacts& Candidate)
				{
					return Candidate.ModuleKey == ModuleKey;
				});
		if (PublishedModule == nullptr)
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Provider generation failed: current publication does not contain module %s."),
				*Function.ModuleKey.Hash.ToHexString()));
		}
		const TArray<FAngelscriptArtifactReference>* const EffectiveReferences =
			JIT.StableProviderReferences.Find(FunctionAndRoute.Function);
		if (EffectiveReferences == nullptr)
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Provider generation failed: function %s has no verified stable reference set."),
				*Function.FunctionKey.Hash.ToHexString()));
		}
		for (const FAngelscriptArtifactReference& Reference
			: *EffectiveReferences)
		{
			if (!Reference.IsValid() || Reference.ExpectedAbi.IsZero())
			{
				return Fail(FString::Printf(
					TEXT("StaticJIT Provider generation failed: function %s has an incomplete stable reference."),
					*Function.FunctionKey.Hash.ToHexString()));
			}
			Function.References.Add({
				EAngelscriptJITReferenceSlotFlags::None,
				Reference});
		}
		const FString DebugFilenameSymbol =
			TEXT("MODULENAME_") + GeneratedCode.FunctionSymbolName;
		Function.ImplementationTemplate = FString::Printf(
			TEXT("#if AS_JIT_DEBUG_CALLSTACKS\n")
			TEXT("#undef SCRIPT_DEBUG_FILENAME\n")
			TEXT("static const char* %s = \"%s\";\n")
			TEXT("#define SCRIPT_DEBUG_FILENAME %s\n")
			TEXT("#endif\n\n"),
			*DebugFilenameSymbol,
			ANSI_TO_TCHAR(FunctionAndRoute.Function->GetModuleName()),
			*DebugFilenameSymbol);
		Function.ImplementationTemplate += GeneratedCode.EmittedFunctionContent;
	}

	return FAngelscriptJITGeneration::Generate(
		Request, OutOutput, OutError);
}

#if AS_WITH_STATIC_JIT_DIAGNOSTICS && AS_CAN_GENERATE_JIT
bool GenerateStaticJITSourceTextForDiagnostics(
	asIScriptModule* Module,
	FString& OutSourceText,
	const bool bEmitDebugMetadata,
	FString* OutError)
{
	OutSourceText.Reset();
	auto Fail = [OutError](const FString& Message)
	{
		if (OutError != nullptr)
		{
			*OutError = Message;
		}
		return false;
	};

	asCModule* const ScriptModule = reinterpret_cast<asCModule*>(Module);
	if (ScriptModule == nullptr)
	{
		return Fail(TEXT(
			"GenerateStaticJITSourceTextForDiagnostics failed: module was null."));
	}

	asCScriptEngine* const ScriptEngine =
		reinterpret_cast<asCScriptEngine*>(ScriptModule->GetEngine());
	FAngelscriptEngine* const RuntimeEngine =
		FAngelscriptEngine::TryGetByScriptEngine(ScriptEngine);
	if (ScriptEngine == nullptr || RuntimeEngine == nullptr)
	{
		return Fail(TEXT(
			"GenerateStaticJITSourceTextForDiagnostics failed: module has no Runtime Engine owner."));
	}

	const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
		ESPMode::ThreadSafe> Routes =
		RuntimeEngine->GetFunctionRouteSnapshot();
	if (!Routes.IsValid())
	{
		return Fail(TEXT(
			"GenerateStaticJITSourceTextForDiagnostics failed: current function routes are unavailable."));
	}

	const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc =
		RuntimeEngine->GetModuleByModuleName(
			ANSI_TO_TCHAR(ScriptModule->GetName()));
	if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule != ScriptModule)
	{
		return Fail(TEXT(
			"GenerateStaticJITSourceTextForDiagnostics failed: module has no current Runtime descriptor."));
	}

	FAngelscriptCacheService* const CacheService =
		RuntimeEngine->GetCacheService();
	const FAngelscriptCacheLifecyclePublications Publications =
		CacheService != nullptr
		? CacheService->GetLifecyclePublications()
		: FAngelscriptCacheLifecyclePublications{};
	FAngelscriptCacheCleanCaptureOptions CaptureOptions;
	if (Publications.Current.IsValid())
	{
		CaptureOptions.Compatibility = Publications.Current->Compatibility;
		CaptureOptions.Context = Publications.Current->Context;
		CaptureOptions.Profile = Publications.Current->Profile;
	}
	else
	{
		FAngelscriptCacheEnvironmentProfile DiagnosticProfile;
		const FAngelscriptPreprocessorContext PreprocessorContext =
			FAngelscriptPreprocessorContext::CreateFromCurrentEngineContext();
		const TArray<FAngelscriptSourceRoot> DiagnosticRoots{
			FAngelscriptSourceRoot::FromGameRoot(
				FPaths::Combine(FPaths::ProjectSavedDir(),
					TEXT("StaticJITDiagnostics")))
		};
		const FAngelscriptCacheEnvironmentProfileResult ProfileResult =
			BuildAngelscriptCacheEnvironmentProfile(
				*RuntimeEngine, PreprocessorContext, DiagnosticRoots,
				DiagnosticProfile);
		if (!ProfileResult.IsSuccess())
		{
			return Fail(FString::Printf(TEXT(
				"GenerateStaticJITSourceTextForDiagnostics failed to build the current Engine profile: %s"),
				*ProfileResult.Detail));
		}
		CaptureOptions = MoveTemp(DiagnosticProfile.CaptureOptions);
	}
	FAngelscriptCacheCleanModuleArtifacts DiagnosticArtifacts;
	const FAngelscriptCacheCleanCaptureResult CaptureResult =
		CaptureAngelscriptCurrentModuleFunctionArtifactsForDiagnostics(
			ModuleDesc.ToSharedRef(), CaptureOptions, DiagnosticArtifacts);
	if (!CaptureResult.IsSuccess())
	{
		return Fail(FString::Printf(TEXT(
			"GenerateStaticJITSourceTextForDiagnostics failed to validate current module artifacts: %s"),
			*CaptureResult.Detail));
	}

	FAngelscriptFunctionRouteSnapshot DiagnosticRoutes;
	DiagnosticRoutes.Generation = Routes->Generation;
	for (const FAngelscriptFunctionRoute& CurrentRoute
		: Routes->FunctionRoutes)
	{
		if (CurrentRoute.Function == nullptr
			|| CurrentRoute.Function->GetModule() != Module)
		{
			continue;
		}
		const FAngelscriptFunctionArtifactIdentity* const Identity =
			DiagnosticArtifacts.ValidatedFunctionArtifactIdentities.
				FindByPredicate(
					[&CurrentRoute](
						const FAngelscriptFunctionArtifactIdentity& Candidate)
					{
						return Candidate.FunctionKey
							== CurrentRoute.Identity.FunctionKey;
					});
		if (Identity == nullptr)
		{
			continue;
		}
		FAngelscriptFunctionRoute Route = CurrentRoute;
		Route.ModuleKey = DiagnosticArtifacts.ModuleKey;
		Route.Identity = *Identity;
		Route.bHasVerifiedArtifactIdentity = true;
		DiagnosticRoutes.FunctionRoutes.Add(MoveTemp(Route));
	}
	const FAngelscriptFunctionRoute* const IdentityRoute =
		DiagnosticRoutes.FunctionRoutes.IsEmpty()
		? nullptr : &DiagnosticRoutes.FunctionRoutes[0];
	if (IdentityRoute == nullptr)
	{
		return Fail(TEXT(
			"GenerateStaticJITSourceTextForDiagnostics failed: validated artifacts did not match a current module function."));
	}

	FAngelscriptCacheSuccessfulPublicationDto DiagnosticPublication;
	DiagnosticPublication.Compatibility = CaptureOptions.Compatibility;
	DiagnosticPublication.Context = CaptureOptions.Context;
	DiagnosticPublication.Profile = CaptureOptions.Profile;
	DiagnosticPublication.Modules.Add(MoveTemp(DiagnosticArtifacts));

	FAngelscriptJITProviderGenerationSettings Settings;
	Settings.TargetProfile =
		FAngelscriptJITProviderRouter::ResolveTargetProfile(
			RuntimeEngine->GetRuntimeConfig());
	Settings.ProviderId.Hash = IdentityRoute->ModuleKey.Hash;
	Settings.ProviderName = TEXT("StaticJIT Diagnostics");
	Settings.OwnerModuleName = TEXT("AngelscriptRuntime");
	Settings.ArtifactProfile = IdentityRoute->Identity.Profile;
	Settings.NativeEnvironmentFingerprint =
		FAngelscriptJITTargetProfiles::
			BuildCurrentNativeEnvironmentFingerprint(Settings.TargetProfile);
	Settings.bEmitDebugMetadata = bEmitDebugMetadata;

	FAngelscriptJITGenerationOutput Output;
	if (!GenerateStaticJITProviderArtifactsImpl(
		MakeArrayView(&Module, 1), Settings, Output, OutError,
		&DiagnosticRoutes, &DiagnosticPublication))
	{
		return false;
	}

	const FAngelscriptJITGeneratedModule* const GeneratedModule =
		Output.Modules.FindByPredicate(
			[&IdentityRoute](
				const FAngelscriptJITGeneratedModule& Candidate)
			{
				return Candidate.ModuleKey == IdentityRoute->ModuleKey;
			});
	if (GeneratedModule == nullptr)
	{
		return Fail(TEXT(
			"GenerateStaticJITSourceTextForDiagnostics failed: current Provider output omitted the requested AS module."));
	}

	const FAngelscriptJITGeneratedFile* const GeneratedSource =
		Output.Files.FindByPredicate(
			[GeneratedModule](
				const FAngelscriptJITGeneratedFile& Candidate)
			{
				return Candidate.Kind
						== EAngelscriptJITGeneratedFileKind::ModuleSource
					&& Candidate.RelativePath
						== GeneratedModule->SourceRelativePath;
			});
	if (GeneratedSource == nullptr)
	{
		return Fail(TEXT(
			"GenerateStaticJITSourceTextForDiagnostics failed: current Provider module source was missing."));
	}

	OutSourceText = GeneratedSource->Content;
	return true;
}
#endif

void FAngelscriptStaticJIT::SanitizeSymbolName(FString& InOutSymbol)
{
	for (int32 i = 0, Count = InOutSymbol.Len(); i < Count; ++i)
	{
		auto Chr = InOutSymbol[i];
		if (Chr == '_')
			continue;
		if (Chr >= '0' && Chr <= '9')
			continue;
		if (Chr >= 'a' && Chr <= 'z')
			continue;
		if (Chr >= 'A' && Chr <= 'Z')
			continue;

		// Tilde characters indicate destructors, so make that more clear
		if (Chr == '~')
		{
			InOutSymbol.RemoveAt(i, 1);
			InOutSymbol.InsertAt(i, TEXT("Destruct__"));
			Count = InOutSymbol.Len();
			continue;
		}

		// Replace anything that isn't alphanumeric or underscore with underscore
		InOutSymbol[i] = '_';
	}
}

#endif // AS_CAN_GENERATE_JIT
