#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITProviderEmitter.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	using ECapability = EAngelscriptTypedASTJITDirectCapability;

	bool HasCapability(
		const FAngelscriptTypedASTJITGenerationCapabilityProfile& Profile,
		const ECapability Capability)
	{
		return EnumHasAnyFlags(Profile.AvailableCapabilities, Capability);
	}

	FString EscapeTypedASTProviderCppString(const FStringView Value)
	{
		FString Result;
		Result.Reserve(Value.Len());
		for (const TCHAR Character : Value)
		{
			switch (Character)
			{
			case TEXT('\\'):
				Result += TEXT("\\\\");
				break;
			case TEXT('"'):
				Result += TEXT("\\\"");
				break;
			case TEXT('\n'):
				Result += TEXT("\\n");
				break;
			case TEXT('\r'):
				Result += TEXT("\\r");
				break;
			case TEXT('\t'):
				Result += TEXT("\\t");
				break;
			default:
				Result.AppendChar(Character);
				break;
			}
		}
		return Result;
	}

	FString BuildRawParameterList(
		const FAngelscriptTypedASTJITProviderEntryPlan& Plan)
	{
		FString Result = Plan.bHasNativeObjectSlot
			? TEXT(", void* Object") : FString();
		for (int32 Index = 0; Index < Plan.Parameters.Num(); ++Index)
		{
			Result += FString::Printf(TEXT(", %s p_%d"),
				*Plan.Parameters[Index].EntryCppType, Index);
		}
		return Result;
	}

	FString BuildBodyArguments(
		const FAngelscriptTypedASTJITProviderEntryPlan& Plan,
		const bool bRequiresExecutionState,
		const bool bRequiresInstrumentationFrame)
	{
		FString Result = bRequiresExecutionState
			? TEXT("Execution") : FString();
		if (bRequiresInstrumentationFrame)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}
			Result += TEXT("TypedFrame");
		}
		for (int32 Index = 0; Index < Plan.Parameters.Num(); ++Index)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}
			const FAngelscriptTypedASTJITProviderParameterPlan& Parameter =
				Plan.Parameters[Index];
			if (Parameter.Marshalling
				== EAngelscriptTypedASTJITProviderParameterMarshalling::BorrowedValueAddress)
			{
				Result += FString::Printf(TEXT("*p_%d"), Index);
			}
			else
			{
				Result += FString::Printf(TEXT("static_cast<%s>(p_%d)"),
					*Parameter.BodyCppType, Index);
			}
		}
		return Result;
	}

	FString BuildVMArguments(
		const FAngelscriptTypedASTJITProviderEntryPlan& Plan)
	{
		FString Result = Plan.bHasNativeObjectSlot
			? TEXT(",\n\t\t*(void**)l_fp") : FString();
		for (int32 Index = 0; Index < Plan.Parameters.Num(); ++Index)
		{
			const FAngelscriptTypedASTJITProviderParameterPlan& Parameter =
				Plan.Parameters[Index];
			Result += FString::Printf(
				TEXT(",\n\t\t*(%s*)(l_fp + %d)"),
				*Parameter.EntryCppType,
				Parameter.VMFrameOffsetDWords);
		}
		return Result;
	}

	FString BuildParmsHeader(
		const FAngelscriptTypedASTJITProviderEntryPlan& Plan)
	{
		FString Result = TEXT("\tSIZE_T ParmsOffset = 0;\n");
		for (int32 Index = 0; Index < Plan.Parameters.Num(); ++Index)
		{
			const FString& StorageType = Plan.Parameters[Index].BodyCppType;
			if (Index != 0)
			{
				Result += FString::Printf(
					TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"),
					*StorageType);
			}
			Result += FString::Printf(
				TEXT("\tconst SIZE_T ParmOffset_%d = ParmsOffset;\n"),
				Index);
			Result += FString::Printf(
				TEXT("\tParmsOffset += sizeof(%s);\n"), *StorageType);
		}
		if (Plan.ReflectedParmsLayout
			== EAngelscriptTypedASTJITReflectedParmsLayout::
				AppendGeneratedWorldContext)
		{
			Result += TEXT(
				"\tParmsOffset = Align(ParmsOffset, alignof(void*));\n"
				"\tconst SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;\n"
				"\tParmsOffset += sizeof(void*);\n"
				"\tstatic_cast<void>(GeneratedWorldContextParmOffset);\n");
		}
		if (!Plan.Parameters.IsEmpty()
			|| Plan.ReflectedParmsLayout
				== EAngelscriptTypedASTJITReflectedParmsLayout::
					AppendGeneratedWorldContext)
		{
			Result += FString::Printf(
				TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"),
				*Plan.Return.BodyCppType);
		}
		Result += TEXT("\tconst SIZE_T ReturnParmOffset = ParmsOffset;\n");
		return Result;
	}

	FString BuildParmsArguments(
		const FAngelscriptTypedASTJITProviderEntryPlan& Plan)
	{
		FString Result = Plan.bHasNativeObjectSlot
			? TEXT(",\n\t\tObject") : FString();
		for (int32 Index = 0; Index < Plan.Parameters.Num(); ++Index)
		{
			const FAngelscriptTypedASTJITProviderParameterPlan& Parameter =
				Plan.Parameters[Index];
			if (Parameter.Marshalling
				== EAngelscriptTypedASTJITProviderParameterMarshalling::BorrowedValueAddress)
			{
				Result += FString::Printf(
					TEXT(",\n\t\t(%s*)(((SIZE_T)Parms) + ParmOffset_%d)"),
					*Parameter.BodyCppType,
					Index);
			}
			else
			{
				Result += FString::Printf(
					TEXT(",\n\t\tstatic_cast<%s>(*(%s*)(((SIZE_T)Parms) + ParmOffset_%d))"),
					*Parameter.EntryCppType,
					*Parameter.BodyCppType,
					Index);
			}
		}
		return Result;
	}
}

bool BuildAngelscriptTypedASTJITProviderEntryPlan(
	const FStaticJITEntryPlan& SharedEntryPlan,
	FAngelscriptTypedASTJITProviderEntryPlan& OutPlan,
	FString* OutError)
{
	OutPlan = {};
	auto Fail = [OutError](const FString& Detail)
	{
		if (OutError != nullptr)
		{
			*OutError = Detail;
		}
		return false;
	};

	if (!SharedEntryPlan.IsStructurallyComplete())
	{
		return Fail(TEXT(
			"TypedASTJITProviderEntryPlanMalformed: the shared Entry Plan is incomplete."));
	}
	if (!SharedEntryPlan.bTypedAdapterRepresentable)
	{
		return Fail(SharedEntryPlan.TypedAdapterFailure.IsEmpty()
			? TEXT("TypedASTJITProviderShapeUnsupported")
			: SharedEntryPlan.TypedAdapterFailure);
	}
	if (SharedEntryPlan.Return.Placement
		== EAngelscriptStaticJITEntryReturnPlacement::HiddenStack)
	{
		return Fail(TEXT(
			"TypedASTJITProviderShapeUnsupported: hidden return storage is outside the reviewed adapter set."));
	}
	if (SharedEntryPlan.Parameters.ContainsByPredicate(
		[](const FStaticJITEntryParameterPlan& Parameter)
		{
			return Parameter.Marshalling
				== EAngelscriptStaticJITEntryParameterMarshalling::
					OpaqueCompatibilityValue;
		}))
	{
		return Fail(TEXT(
			"TypedASTJITProviderParameterUnsupported: the shared plan contains an opaque compatibility parameter."));
	}
	static_cast<FStaticJITEntryPlan&>(OutPlan) = SharedEntryPlan;
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	return true;
}

bool BuildAngelscriptTypedASTJITProviderEntryPlan(
	const asCScriptFunction& ScriptFunction,
	const bool bHasVMEntry,
	const bool bHasRawEntry,
	const bool bHasParmsEntry,
	FAngelscriptTypedASTJITProviderEntryPlan& OutPlan,
	FString* OutError)
{
	FStaticJITEntryPlan SharedEntryPlan;
	if (!BuildAngelscriptStaticJITEntryPlan(
		ScriptFunction,
		bHasVMEntry,
		bHasRawEntry,
		bHasParmsEntry,
		SharedEntryPlan,
		OutError))
	{
		OutPlan = {};
		return false;
	}
	return BuildAngelscriptTypedASTJITProviderEntryPlan(
		SharedEntryPlan, OutPlan, OutError);
}

FAngelscriptTypedASTJITProviderEmission
EmitAngelscriptTypedASTJITProviderFunction(
	const FAngelscriptTypedASTJITProviderEntryPlan& Plan,
	const FAngelscriptTypedASTJITEmission& Body)
{
	FAngelscriptTypedASTJITProviderEmission Result;
	if (!Body.bSuccess || !Plan.bHasRawEntry || !Plan.bHasVMEntry
		|| Plan.Return.BodyCppType.IsEmpty()
		|| Plan.Return.EntryCppType.IsEmpty()
		|| Plan.CanonicalDeclaration.TrimStartAndEnd().IsEmpty()
		|| Plan.SourceSection.TrimStartAndEnd().IsEmpty()
		|| Plan.SourceLine == 0)
	{
		Result.FailureDetail = TEXT(
			"TypedASTJITProviderEmissionInvalidInput: an exact scalar entry plan and successful body are required.");
		return Result;
	}
	if (!Plan.InstrumentationProfile.IsValid()
		|| !HasCapability(
			Plan.InstrumentationProfile,
			ECapability::RecursionBudget))
	{
		Result.FailureDetail = TEXT(
			"TypedASTJITProviderInstrumentationProfileInvalid: every published root requires one explicit profile with RecursionBudget.");
		return Result;
	}
	const bool bEmitFramePosition = HasCapability(
		Plan.InstrumentationProfile,
		ECapability::FramePosition);
	if (!(Body.InstrumentationProfileIdentityHash
			== Plan.InstrumentationProfile.IdentityHash)
		|| Body.bRequiresInstrumentationFrame != bEmitFramePosition)
	{
		Result.FailureDetail = TEXT(
			"TypedASTJITProviderInstrumentationProfileMismatch: wrapper and body must consume the exact same immutable profile.");
		return Result;
	}

	const FString RawParameters = BuildRawParameterList(Plan);
	Result.RawEntryDeclaration = FString::Printf(
		TEXT("%s {RAW_SYMBOL}(FScriptExecution& Execution%s)"),
		*Plan.Return.EntryCppType, *RawParameters);
	Result.ImplementationTemplate = Body.Definition;
	if (!Result.ImplementationTemplate.EndsWith(TEXT("\n")))
	{
		Result.ImplementationTemplate += TEXT("\n");
	}
	Result.ImplementationTemplate += FString::Printf(TEXT(
		"\n{RAW_ENTRY_COMMENT}%s {RAW_SYMBOL}(FScriptExecution& Execution%s)\n"
		"{\n"
		"\tSCRIPT_ASSUME_NO_EXCEPTION()\n"
		"\tFScopeTypedASTJITExecutionFrame TypedFrame(\n"
		"\t\tExecution,\n"
		"\t\tExecution.jitFunction,\n"
		"\t\t\"%s\",\n"
		"\t\t\"%s\",\n"
		"\t\t%u,\n"
		"\t\t%s,\n"
		"\t\t%s);\n"
		"\tif (UNLIKELY(!TypedFrame.IsActive()))\n"
		"\t{\n"
		"\t\treturn {};\n"
		"\t}\n"
		"\treturn {SYMBOL_PREFIX}_TypedBody(%s);\n"
		"}\n"
		"\n{VM_ENTRY_COMMENT}void {VM_SYMBOL}(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)\n"
		"{\n"
		"\t*(%s*)l_outValue = {RAW_SYMBOL}(Execution%s);\n"
		"}\n"),
		*Plan.Return.EntryCppType,
		*RawParameters,
		*EscapeTypedASTProviderCppString(Plan.SourceSection),
		*EscapeTypedASTProviderCppString(Plan.CanonicalDeclaration),
		Plan.SourceLine,
		Plan.bHasNativeObjectSlot ? TEXT("Object") : TEXT("nullptr"),
		bEmitFramePosition ? TEXT("true") : TEXT("false"),
		*BuildBodyArguments(
			Plan,
			Body.bRequiresExecutionState,
			Body.bRequiresInstrumentationFrame),
		*Plan.Return.EntryCppType,
		*BuildVMArguments(Plan));

	if (Plan.bHasParmsEntry)
	{
		Result.ImplementationTemplate += FString::Printf(TEXT(
			"\n{PARMS_ENTRY_COMMENT}void {PARMS_SYMBOL}(FScriptExecution& Execution, void* Object, void* Parms)\n"
			"{\n"
			"%s"
			"%s"
			"\t*(%s*)(((SIZE_T)Parms) + ReturnParmOffset) = {RAW_SYMBOL}(Execution%s);\n"
			"}\n"),
			Plan.bHasNativeObjectSlot
				? TEXT("") : TEXT("\tstatic_cast<void>(Object);\n"),
			*BuildParmsHeader(Plan),
			*Plan.Return.BodyCppType,
			*BuildParmsArguments(Plan));
	}

	Result.RequiredIncludes = Body.RequiredIncludes;
	Result.RequiredIncludes.AddUnique(TEXT("StaticJIT/StaticJITHeader.h"));
	Result.bSuccess = true;
	return Result;
}

FAngelscriptTypedASTJITInternalHelperEmission
EmitAngelscriptTypedASTJITInternalHelper(
	const FAngelscriptTypedASTJITInternalHelperPlan& Plan,
	const FAngelscriptTypedASTJITEmission& Body)
{
	FAngelscriptTypedASTJITInternalHelperEmission Result;
	const auto Fail = [&Result](const FString& Detail)
		-> FAngelscriptTypedASTJITInternalHelperEmission
	{
		Result = {};
		Result.FailureDetail = Detail;
		return Result;
	};
	if (!Body.bSuccess)
	{
		return Fail(TEXT(
			"TypedASTJIT internal helper requires a successful body emission."));
	}
	if (!Plan.InstrumentationProfile.IsValid()
		|| !HasCapability(
			Plan.InstrumentationProfile,
			ECapability::RecursionBudget))
	{
		return Fail(TEXT(
			"TypedASTJIT internal helper requires one explicit instrumentation profile with RecursionBudget."));
	}
	const bool bEmitFramePosition = HasCapability(
		Plan.InstrumentationProfile,
		ECapability::FramePosition);
	if (!(Body.InstrumentationProfileIdentityHash
			== Plan.InstrumentationProfile.IdentityHash)
		|| Body.bRequiresInstrumentationFrame != bEmitFramePosition)
	{
		return Fail(TEXT(
			"TypedASTJIT internal helper wrapper and body must consume the exact same instrumentation profile."));
	}
	if (Plan.Symbol.IsEmpty() || Plan.BodySymbol.IsEmpty()
		|| Plan.ReturnBodyCppType.IsEmpty()
		|| Plan.CanonicalDeclaration.IsEmpty()
		|| !Plan.SourceSection.StartsWith(
			TEXT("/Angelscript/"), ESearchCase::CaseSensitive)
		|| Plan.SourceLine == 0
		|| Plan.FunctionReferenceSlotIndex < 0)
	{
		return Fail(TEXT(
			"TypedASTJIT internal helper plan lacks a symbol, reviewed scalar return, logical source metadata, or function reference slot."));
	}
	if (!Body.Declaration.Contains(
		Plan.BodySymbol, ESearchCase::CaseSensitive))
	{
		return Fail(TEXT(
			"TypedASTJIT internal helper body symbol does not match its emitted declaration."));
	}

	FString Parameters;
	FString Arguments = Body.bRequiresExecutionState
		? TEXT("Execution") : FString();
	if (Body.bRequiresInstrumentationFrame)
	{
		if (!Arguments.IsEmpty())
		{
			Arguments += TEXT(", ");
		}
		Arguments += TEXT("TypedFrame");
	}
	for (int32 ParameterIndex = 0;
		ParameterIndex < Plan.Parameters.Num(); ++ParameterIndex)
	{
		const FAngelscriptTypedASTJITProviderParameterPlan& Parameter =
			Plan.Parameters[ParameterIndex];
		if (Parameter.BodyCppType.IsEmpty()
			|| Parameter.Marshalling
				!= EAngelscriptTypedASTJITProviderParameterMarshalling::ScalarValue)
		{
			return Fail(TEXT(
				"TypedASTJIT internal helper parameters must use the reviewed scalar body ABI."));
		}
		Parameters += FString::Printf(
			TEXT(",\n\tconst %s as_helper_p%d"),
			*Parameter.BodyCppType, ParameterIndex);
		if (!Arguments.IsEmpty())
		{
			Arguments += TEXT(", ");
		}
		Arguments += FString::Printf(
			TEXT("as_helper_p%d"), ParameterIndex);
	}

	const FString Signature = FString::Printf(
		TEXT("static %s %s(\n\tFScriptExecution& Execution%s"),
		*Plan.ReturnBodyCppType, *Plan.Symbol, *Parameters);
	Result.ForwardDeclaration = Signature + TEXT(");\n");
	Result.Implementation = FString::Printf(TEXT(
		"%s)\n"
		"{\n"
		"\tasIScriptFunction* const TypedFunction =\n"
		"\t\tFAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(\n"
		"\t\t\tExecution, %uu);\n"
		"\tFScopeTypedASTJITExecutionFrame TypedFrame(\n"
		"\t\tExecution,\n"
		"\t\tTypedFunction,\n"
		"\t\t\"%s\",\n"
		"\t\t\"%s\",\n"
		"\t\t%u,\n"
		"\t\tnullptr,\n"
		"\t\t%s);\n"
		"\tif (UNLIKELY(!TypedFrame.IsActive()))\n"
		"\t{\n"
		"\t\treturn {};\n"
		"\t}\n"
		"\treturn %s(\n"
		"\t\t%s);\n"
		"}\n"),
		*Signature,
		static_cast<uint32>(Plan.FunctionReferenceSlotIndex),
		*EscapeTypedASTProviderCppString(Plan.SourceSection),
		*EscapeTypedASTProviderCppString(Plan.CanonicalDeclaration),
		Plan.SourceLine,
		bEmitFramePosition ? TEXT("true") : TEXT("false"),
		*Plan.BodySymbol,
		*Arguments);
	Result.RequiredIncludes.Add(TEXT("StaticJIT/StaticJITHeader.h"));
	Result.bSuccess = true;
	return Result;
}
