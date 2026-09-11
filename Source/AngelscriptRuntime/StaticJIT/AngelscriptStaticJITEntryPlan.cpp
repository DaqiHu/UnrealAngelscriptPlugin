#include "StaticJIT/AngelscriptStaticJITEntryPlan.h"

#if AS_CAN_GENERATE_JIT

#include "AngelscriptType.h"

#include "StartAngelscriptHeaders.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptStaticJITEntryPlan_Private
{
	bool TrySpellBorrowedValueType(
		const asCDataType& Type,
		FString& OutType)
	{
		if (!Type.IsValid() || !Type.IsReference() || !Type.IsReadOnly()
			|| Type.IsObjectHandle() || Type.GetTypeInfo() == nullptr)
		{
			return false;
		}
		const asCString& Name = Type.GetTypeInfo()->name;
		if (Name != "FString" && Name != "FLinearColor")
		{
			return false;
		}
		OutType = UTF8_TO_TCHAR(Name.AddressOf());
		return true;
	}

	FString TryBuildCompatibilityCppType(
		const asCScriptFunction& ScriptFunction,
		const int32 ParameterIndex,
		const asCDataType& Type)
	{
		FAngelscriptTypeUsage Usage = ParameterIndex == INDEX_NONE
			? FAngelscriptTypeUsage::FromReturn(
				const_cast<asCScriptFunction*>(&ScriptFunction))
			: FAngelscriptTypeUsage::FromParam(
				const_cast<asCScriptFunction*>(&ScriptFunction), ParameterIndex);
		if (Usage.IsValid())
		{
			FAngelscriptType::FCppForm CppForm;
			Usage.GetCppForm(CppForm);
			if (!CppForm.CppType.IsEmpty())
			{
				return CppForm.CppType;
			}
			if (!CppForm.CppGenericType.IsEmpty())
			{
				return CppForm.CppGenericType;
			}
		}
		if (Type.IsReferenceType() || Type.IsObjectHandle())
		{
			return TEXT("UObject*");
		}
		if (Type.IsObject() || Type.IsReference())
		{
			return TEXT("FUnknownValueType");
		}
		return FString();
	}

	void AddRequiredInclude(
		const asCScriptFunction& ScriptFunction,
		const int32 ParameterIndex,
		TArray<FString>& OutIncludes)
	{
		FAngelscriptTypeUsage Usage = ParameterIndex == INDEX_NONE
			? FAngelscriptTypeUsage::FromReturn(
				const_cast<asCScriptFunction*>(&ScriptFunction))
			: FAngelscriptTypeUsage::FromParam(
				const_cast<asCScriptFunction*>(&ScriptFunction), ParameterIndex);
		if (!Usage.IsValid())
		{
			return;
		}
		FAngelscriptType::FCppForm CppForm;
		Usage.GetCppForm(CppForm);
		if (!CppForm.CppHeader.IsEmpty())
		{
			OutIncludes.AddUnique(CppForm.CppHeader);
		}
	}

	void MarkTypedFailure(FStaticJITEntryPlan& Plan, const FString& Detail)
	{
		Plan.bTypedAdapterRepresentable = false;
		if (Plan.TypedAdapterFailure.IsEmpty())
		{
			Plan.TypedAdapterFailure = Detail;
		}
	}

	FAngelscriptHash256 BuildEntryPlanHash(const FStaticJITEntryPlan& Plan)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("static-jit-entry-plan-v2"));
		Writer.WriteString(Plan.CanonicalDeclaration);
		Writer.WriteString(Plan.SourceSection);
		Writer.WriteUInt32(Plan.SourceLine);
		Writer.WriteBool(Plan.bHasVMEntry);
		Writer.WriteBool(Plan.bHasRawEntry);
		Writer.WriteBool(Plan.bHasParmsEntry);
		Writer.WriteBool(Plan.bHasNativeObjectSlot);
		Writer.WriteBool(Plan.bTypedAdapterRepresentable);
		Writer.WriteString(Plan.TypedAdapterFailure);
		Writer.WriteUInt32(static_cast<uint32>(Plan.ReceiverKind));
		Writer.WriteUInt32(static_cast<uint32>(Plan.ReceiverParameterIndex));
		Writer.WriteUInt32(static_cast<uint32>(Plan.ReflectedParmsLayout));
		Writer.WriteUInt32(static_cast<uint32>(Plan.Parameters.Num()));
		for (const FStaticJITEntryParameterPlan& Parameter : Plan.Parameters)
		{
			Writer.WriteString(Parameter.CanonicalName);
			Writer.WriteString(Parameter.BodyCppType);
			Writer.WriteString(Parameter.EntryCppType);
			Writer.WriteString(Parameter.ReflectedStorageCppType);
			Writer.WriteUInt32(static_cast<uint32>(
				Parameter.FormalParameterIndex));
			Writer.WriteUInt32(static_cast<uint32>(
				Parameter.VMFrameOffsetDWords));
			Writer.WriteUInt32(static_cast<uint32>(
				Parameter.ReflectedParameterIndex));
			Writer.WriteUInt32(static_cast<uint32>(Parameter.Role));
			Writer.WriteUInt32(static_cast<uint32>(Parameter.Marshalling));
		}
		Writer.WriteString(Plan.Return.BodyCppType);
		Writer.WriteString(Plan.Return.EntryCppType);
		Writer.WriteString(Plan.Return.ReflectedStorageCppType);
		Writer.WriteUInt32(static_cast<uint32>(
			Plan.Return.VMResultOffsetDWords));
		Writer.WriteUInt32(static_cast<uint32>(Plan.Return.Placement));
		Writer.WriteUInt32(static_cast<uint32>(Plan.RequiredIncludes.Num()));
		for (const FString& Include : Plan.RequiredIncludes)
		{
			Writer.WriteString(Include);
		}
		return Writer.FinalizeHash();
	}
}

bool FStaticJITEntryPlan::IsStructurallyComplete() const
{
	if (EntryPlanHash.IsZero() || CanonicalDeclaration.IsEmpty()
		|| !bHasVMEntry || !bHasRawEntry)
	{
		return false;
	}
	if (bHasNativeObjectSlot
		!= (ReceiverKind
			== EAngelscriptStaticJITEntryReceiverKind::NativeObjectSlot))
	{
		return false;
	}
	if (ReceiverKind
		== EAngelscriptStaticJITEntryReceiverKind::DeclaredParameterAlias)
	{
		if (!Parameters.IsValidIndex(ReceiverParameterIndex)
			|| Parameters[ReceiverParameterIndex].FormalParameterIndex
				!= ReceiverParameterIndex
			|| Parameters[ReceiverParameterIndex].Role
				!= EAngelscriptStaticJITEntryParameterRole::ReceiverAlias)
		{
			return false;
		}
	}
	for (int32 Index = 0; Index < Parameters.Num(); ++Index)
	{
		const FStaticJITEntryParameterPlan& Parameter = Parameters[Index];
		if (Parameter.FormalParameterIndex != Index
			|| Parameter.ReflectedParameterIndex != Index
			|| Parameter.EntryCppType.IsEmpty()
			|| Parameter.BodyCppType.IsEmpty())
		{
			return false;
		}
	}
	const bool bReturnComplete = Return.Placement
		== EAngelscriptStaticJITEntryReturnPlacement::None
		|| (!Return.BodyCppType.IsEmpty() && !Return.EntryCppType.IsEmpty());
	return bReturnComplete
		&& EntryPlanHash
			== AngelscriptStaticJITEntryPlan_Private::BuildEntryPlanHash(*this);
}

bool TrySpellAngelscriptStaticJITBodyCppType(
	const asCDataType& Type,
	FString& OutType)
{
	OutType.Reset();
	if (!Type.IsValid() || Type.IsObjectHandle())
	{
		return false;
	}
	if (Type.IsEnumType())
	{
		switch (Type.GetSizeInMemoryBytes())
		{
		case 1: OutType = TEXT("int8"); return true;
		case 2: OutType = TEXT("int16"); return true;
		case 4: OutType = TEXT("int32"); return true;
		case 8: OutType = TEXT("int64"); return true;
		default: return false;
		}
	}
	if (Type.GetTypeInfo() != nullptr)
	{
		FString BorrowedType;
		if (!AngelscriptStaticJITEntryPlan_Private::TrySpellBorrowedValueType(
			Type, BorrowedType))
		{
			return false;
		}
		OutType = BorrowedType + TEXT("&");
		return true;
	}
	if (Type.IsReference())
	{
		return false;
	}
	switch (Type.GetTokenType())
	{
	case ttVoid: OutType = TEXT("void"); return true;
	case ttBool: OutType = TEXT("bool"); return true;
	case ttInt8: OutType = TEXT("int8"); return true;
	case ttUInt8: OutType = TEXT("uint8"); return true;
	case ttInt16: OutType = TEXT("int16"); return true;
	case ttUInt16: OutType = TEXT("uint16"); return true;
	case ttInt: OutType = TEXT("int32"); return true;
	case ttUInt: OutType = TEXT("uint32"); return true;
	case ttInt64: OutType = TEXT("int64"); return true;
	case ttUInt64: OutType = TEXT("uint64"); return true;
	case ttFloat32: OutType = TEXT("float"); return true;
	case ttFloat64: OutType = TEXT("double"); return true;
	default: return false;
	}
}

bool TrySpellAngelscriptStaticJITEntryCppType(
	const asCDataType& Type,
	FString& OutType)
{
	if (!TrySpellAngelscriptStaticJITBodyCppType(Type, OutType)
		|| Type.IsFloatExtendedToDouble()
		|| Type.GetTokenType() == ttVoid
		|| Type.GetTypeInfo() != nullptr)
	{
		return false;
	}
	switch (Type.GetSizeInMemoryBytes())
	{
	case 1: OutType = TEXT("asBYTE"); return true;
	case 2: OutType = TEXT("asWORD"); return true;
	case 4:
		OutType = Type.GetTokenType() == ttFloat32
			? TEXT("float") : TEXT("asDWORD");
		return true;
	case 8:
		OutType = Type.GetTokenType() == ttFloat64
			? TEXT("double") : TEXT("asQWORD");
		return true;
	default: return false;
	}
}

void FinalizeAngelscriptStaticJITEntryPlan(FStaticJITEntryPlan& Plan)
{
	Plan.EntryPlanHash =
		AngelscriptStaticJITEntryPlan_Private::BuildEntryPlanHash(Plan);
}

bool BuildAngelscriptStaticJITEntryPlan(
	const asCScriptFunction& ScriptFunction,
	const bool bHasVMEntry,
	const bool bHasRawEntry,
	const bool bHasParmsEntry,
	FStaticJITEntryPlan& OutPlan,
	FString* OutError)
{
	using namespace AngelscriptStaticJITEntryPlan_Private;
	OutPlan = {};
	auto Fail = [&OutPlan, OutError](const FString& Detail)
	{
		OutPlan = {};
		if (OutError != nullptr)
		{
			*OutError = Detail;
		}
		return false;
	};

	if (ScriptFunction.scriptData == nullptr)
	{
		return Fail(TEXT(
			"StaticJITEntryPlanFunctionUnsupported: an entry plan requires a script function."));
	}
	if (!bHasVMEntry || !bHasRawEntry)
	{
		return Fail(TEXT(
			"StaticJITEntryPlanEntrySetMalformed: every captured script function requires VM and raw entries."));
	}
	if (ScriptFunction.artifactInvocationKind == asBUILD_ARTIFACT_INVOCATION_RESERVED_ANONYMOUS_FUNCTION)
		return Fail(TEXT("Anonymous-function invocation artifacts are not supported."));
	if (ScriptFunction.parameterOffsets.GetLength()
			!= ScriptFunction.parameterTypes.GetLength()
		|| ScriptFunction.parameterNames.GetLength()
			!= ScriptFunction.parameterTypes.GetLength())
	{
		return Fail(TEXT(
			"StaticJITEntryPlanParameterLayoutMalformed: parameter metadata arrays disagree."));
	}

	OutPlan.bHasVMEntry = bHasVMEntry;
	OutPlan.bHasRawEntry = bHasRawEntry;
	OutPlan.bHasParmsEntry = bHasParmsEntry;
	OutPlan.CanonicalDeclaration = ANSI_TO_TCHAR(
		ScriptFunction.GetDeclarationStr(
			true, true, false, true, false).AddressOf());
	if (const char* const SourceSection = ScriptFunction.GetScriptSectionName())
	{
		OutPlan.SourceSection = ANSI_TO_TCHAR(SourceSection);
	}
	OutPlan.SourceLine = static_cast<uint32>(FMath::Max(
		const_cast<asCScriptFunction&>(ScriptFunction).
			GetLineNumber(0, nullptr) & 0xFFFFF,
		0));

	if (ScriptFunction.objectType != nullptr)
	{
		OutPlan.bHasNativeObjectSlot = true;
		OutPlan.ReceiverKind =
			EAngelscriptStaticJITEntryReceiverKind::NativeObjectSlot;
	}
	else if (ScriptFunction.traits.GetTrait(asTRAIT_EXTERNAL_IMPLICIT_THIS)
		|| ScriptFunction.IsMixin())
	{
		OutPlan.ReceiverKind =
			EAngelscriptStaticJITEntryReceiverKind::DeclaredParameterAlias;
		OutPlan.ReceiverParameterIndex = 0;
	}

	OutPlan.bTypedAdapterRepresentable = true;
	if (ScriptFunction.DoesReturnOnStack())
	{
		MarkTypedFailure(OutPlan,
			TEXT("StaticJITEntryPlanTypedReturnOnStackUnsupported"));
	}
	if (OutPlan.bHasNativeObjectSlot
		&& (ScriptFunction.funcType == asFUNC_VIRTUAL
			|| !ScriptFunction.traits.GetTrait(asTRAIT_FINAL)))
	{
		MarkTypedFailure(OutPlan,
			TEXT("StaticJITEntryPlanTypedReceiverUnsupported"));
	}

	for (asUINT Index = 0;
		Index < ScriptFunction.parameterTypes.GetLength();
		++Index)
	{
		const asCDataType& Type = ScriptFunction.parameterTypes[Index];
		FStaticJITEntryParameterPlan& Parameter =
			OutPlan.Parameters.Emplace_GetRef();
		if (Index < ScriptFunction.parameterNames.GetLength()
			&& ScriptFunction.parameterNames[Index].GetLength() > 0)
		{
			Parameter.CanonicalName = UTF8_TO_TCHAR(
				ScriptFunction.parameterNames[Index].AddressOf());
		}
		else
		{
			Parameter.CanonicalName = FString::Printf(TEXT("_p%u"), Index);
		}
		Parameter.FormalParameterIndex = static_cast<int32>(Index);
		Parameter.VMFrameOffsetDWords =
			ScriptFunction.parameterOffsets[Index];
		Parameter.ReflectedParameterIndex = static_cast<int32>(Index);
		if (OutPlan.ReceiverKind
				== EAngelscriptStaticJITEntryReceiverKind::DeclaredParameterAlias
			&& OutPlan.ReceiverParameterIndex == static_cast<int32>(Index))
		{
			Parameter.Role =
				EAngelscriptStaticJITEntryParameterRole::ReceiverAlias;
		}

		if (TrySpellAngelscriptStaticJITEntryCppType(
				Type, Parameter.EntryCppType)
			&& TrySpellAngelscriptStaticJITBodyCppType(
				Type, Parameter.BodyCppType))
		{
			Parameter.ReflectedStorageCppType = Parameter.BodyCppType;
		}
		else
		{
			FString BorrowedType;
			if (Index < ScriptFunction.inOutFlags.GetLength()
				&& ScriptFunction.inOutFlags[Index] == asTM_INREF
				&& TrySpellBorrowedValueType(Type, BorrowedType))
			{
				Parameter.BodyCppType = BorrowedType;
				Parameter.EntryCppType = BorrowedType + TEXT("*");
				Parameter.ReflectedStorageCppType = BorrowedType;
				Parameter.Marshalling =
					EAngelscriptStaticJITEntryParameterMarshalling::
						BorrowedValueAddress;
			}
			else
			{
				FString CompatibilityType = TryBuildCompatibilityCppType(
					ScriptFunction, static_cast<int32>(Index), Type);
				if (CompatibilityType.IsEmpty())
				{
					CompatibilityType = TEXT("FUnknownValueType");
				}
				Parameter.BodyCppType = CompatibilityType;
				Parameter.EntryCppType = CompatibilityType;
				Parameter.ReflectedStorageCppType = CompatibilityType;
				Parameter.Marshalling =
					EAngelscriptStaticJITEntryParameterMarshalling::
						OpaqueCompatibilityValue;
				MarkTypedFailure(OutPlan, FString::Printf(TEXT(
					"StaticJITEntryPlanTypedParameterUnsupported:%u"), Index));
			}
		}
		AddRequiredInclude(
			ScriptFunction, static_cast<int32>(Index),
			OutPlan.RequiredIncludes);
	}

	if (OutPlan.ReceiverKind
			== EAngelscriptStaticJITEntryReceiverKind::DeclaredParameterAlias
		&& !OutPlan.Parameters.IsValidIndex(OutPlan.ReceiverParameterIndex))
	{
		return Fail(TEXT(
			"StaticJITEntryPlanReceiverAliasMalformed: receiver parameter zero is absent."));
	}

	if (ScriptFunction.returnType.GetTokenType() == ttVoid)
	{
		OutPlan.Return.BodyCppType = TEXT("void");
		OutPlan.Return.EntryCppType = TEXT("void");
		OutPlan.Return.ReflectedStorageCppType = TEXT("void");
		OutPlan.Return.Placement =
			EAngelscriptStaticJITEntryReturnPlacement::None;
	}
	else
	{
		OutPlan.Return.VMResultOffsetDWords =
			OutPlan.bHasNativeObjectSlot ? 2 : 0;
		if (TrySpellAngelscriptStaticJITEntryCppType(
				ScriptFunction.returnType, OutPlan.Return.EntryCppType)
			&& TrySpellAngelscriptStaticJITBodyCppType(
				ScriptFunction.returnType, OutPlan.Return.BodyCppType))
		{
			// The maintained raw-entry spelling uses the literal C++ return
			// type rather than the VM carrier spelling used for parameters.
			OutPlan.Return.EntryCppType = OutPlan.Return.BodyCppType;
			OutPlan.Return.ReflectedStorageCppType =
				OutPlan.Return.BodyCppType;
			OutPlan.Return.Placement =
				EAngelscriptStaticJITEntryReturnPlacement::Direct;
		}
		else
		{
			FString CompatibilityType = TryBuildCompatibilityCppType(
				ScriptFunction, INDEX_NONE, ScriptFunction.returnType);
			if (CompatibilityType.IsEmpty())
			{
				CompatibilityType = TEXT("FUnknownValueType");
			}
			OutPlan.Return.BodyCppType = CompatibilityType;
			OutPlan.Return.EntryCppType = CompatibilityType;
			OutPlan.Return.ReflectedStorageCppType = CompatibilityType;
			OutPlan.Return.Placement = ScriptFunction.DoesReturnOnStack()
				? EAngelscriptStaticJITEntryReturnPlacement::HiddenStack
				: EAngelscriptStaticJITEntryReturnPlacement::Direct;
			MarkTypedFailure(OutPlan,
				TEXT("StaticJITEntryPlanTypedReturnUnsupported"));
		}
		AddRequiredInclude(
			ScriptFunction, INDEX_NONE, OutPlan.RequiredIncludes);
	}

	OutPlan.RequiredIncludes.Sort();
	FinalizeAngelscriptStaticJITEntryPlan(OutPlan);
	if (!OutPlan.IsStructurallyComplete())
	{
		return Fail(TEXT(
			"StaticJITEntryPlanIncomplete: the frozen entry plan failed structural validation."));
	}
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	return true;
}

bool CanGenerateAngelscriptStaticJITParmsEntry(
	const asCScriptFunction* ScriptFunction)
{
	if (ScriptFunction == nullptr
		|| ScriptFunction->GetFuncType() != asFUNC_SCRIPT)
	{
		return false;
	}
	for (asUINT ParameterIndex = 0;
		ParameterIndex < ScriptFunction->parameterTypes.GetLength();
		++ParameterIndex)
	{
		const asCDataType& DataType =
			ScriptFunction->parameterTypes[ParameterIndex];
		if ((DataType.IsObject() || DataType.IsReference())
			&& DataType.GetSizeInMemoryBytes() < 4
			&& !DataType.IsObject())
		{
			return false;
		}
	}
	return true;
}

#endif // AS_CAN_GENERATE_JIT
