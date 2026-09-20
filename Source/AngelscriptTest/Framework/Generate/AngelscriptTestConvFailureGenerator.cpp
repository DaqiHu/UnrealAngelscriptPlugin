#include "Framework/Generate/AngelscriptTestConvFailureGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ConvFailureGeneratorDetail
{
struct FConvFailureCell
{
	EConvFailureKind Failure = EConvFailureKind::ImplicitNarrowing;
	EConvFailureRecovery Recovery = EConvFailureRecovery::FreshModule;
};

const TCHAR* FailureToken(const EConvFailureKind Failure)
{
	switch (Failure)
	{
	case EConvFailureKind::ImplicitNarrowing:
		return TEXT("IMPLICIT_NARROWING");
	case EConvFailureKind::NumericToEnum:
		return TEXT("NUMERIC_TO_ENUM");
	case EConvFailureKind::UnrelatedReference:
		return TEXT("UNRELATED_REFERENCE");
	case EConvFailureKind::BadDowncast:
		return TEXT("BAD_DOWNCAST");
	case EConvFailureKind::NullValueTarget:
		return TEXT("NULL_VALUE_TARGET");
	case EConvFailureKind::AmbiguousConstructor:
		return TEXT("AMBIGUOUS_CONSTRUCTOR");
	case EConvFailureKind::AmbiguousOperator:
		return TEXT("AMBIGUOUS_OPERATOR");
	case EConvFailureKind::ExplicitOnlyImplicitUse:
		return TEXT("EXPLICIT_ONLY_IMPLICIT_USE");
	case EConvFailureKind::ConversionException:
		return TEXT("CONVERSION_EXCEPTION");
	case EConvFailureKind::ConstructorException:
		return TEXT("CONSTRUCTOR_EXCEPTION");
	case EConvFailureKind::AbiMismatch:
		return TEXT("ABI_MISMATCH");
	case EConvFailureKind::ConditionalNoCommonType:
		return TEXT("CONDITIONAL_NO_COMMON_TYPE");
	default:
		return nullptr;
	}
}

const TCHAR* RecoveryToken(const EConvFailureRecovery Recovery)
{
	switch (Recovery)
	{
	case EConvFailureRecovery::FreshModule:
		return TEXT("FRESH_MODULE");
	case EConvFailureRecovery::SameModuleOrContext:
		return TEXT("SAME_MODULE_OR_CONTEXT");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FConvFailureParams& Params)
{
	return FailureToken(Params.Failure) != nullptr && RecoveryToken(Params.Recovery) != nullptr;
}

bool IsReject(const EConvFailureKind Failure)
{
	switch (Failure)
	{
	case EConvFailureKind::AmbiguousConstructor:
	case EConvFailureKind::AmbiguousOperator:
	case EConvFailureKind::NullValueTarget:
	case EConvFailureKind::ConversionException:
	case EConvFailureKind::ConstructorException:
		return false;
	default:
		return true;
	}
}

bool IsFault(const EConvFailureKind Failure)
{
	return Failure == EConvFailureKind::NullValueTarget
		|| Failure == EConvFailureKind::ConversionException
		|| Failure == EConvFailureKind::ConstructorException;
}

const TCHAR* FaultException(const EConvFailureKind Failure)
{
	if (Failure == EConvFailureKind::NullValueTarget)
	{
		return TEXT("Null pointer access");
	}
	if (Failure == EConvFailureKind::ConversionException || Failure == EConvFailureKind::ConstructorException)
	{
		return TEXT("Divide by zero");
	}
	return nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FConvFailureCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CONV-FAILURE-%s-%s"),
		FailureToken(Cell.Failure),
		RecoveryToken(Cell.Recovery));
}

FString MakeEntryName(const FConvFailureCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FConvFailureParams ParamsFromCell(const FConvFailureCell& Cell)
{
	FConvFailureParams Params;
	Params.Failure = Cell.Failure;
	Params.Recovery = Cell.Recovery;
	return Params;
}

TArray<FConvFailureCell> AllCells()
{
	TArray<FConvFailureCell> Cells;
	Cells.SetNum(24);
	for (int32 Index = 0; Index < 24; ++Index)
	{
		FConvFailureCell& Cell = Cells[Index];
		Cell.Failure = static_cast<EConvFailureKind>(Index / 2);
		Cell.Recovery = static_cast<EConvFailureRecovery>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FConvFailureCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FConvFailureCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendFailureHelpers(FString& Source, const EConvFailureKind Failure)
{
	if (Failure == EConvFailureKind::ImplicitNarrowing || Failure == EConvFailureKind::NumericToEnum)
	{
		AppendLine(Source, TEXT("enum EConversionFailure"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tZero = 0,"));
		AppendLine(Source, TEXT("\tOne = 1"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("int RequireFailureEnum(EConversionFailure Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn int(Value);"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Failure == EConvFailureKind::UnrelatedReference)
	{
		AppendLine(Source, TEXT("class FUnrelatedConversionLeft"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("class FUnrelatedConversionRight"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("int RequireUnrelatedReference(FUnrelatedConversionRight Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 1;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Failure == EConvFailureKind::BadDowncast)
	{
		AppendLine(Source, TEXT("class FConversionBase"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("class FConversionDerived : FConversionBase"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("int RequireDerivedReference(FConversionDerived Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 1;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Failure == EConvFailureKind::AmbiguousConstructor)
	{
		AppendLine(Source, TEXT("int SelectAmbiguousConstructor(int64 Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 1;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("int SelectAmbiguousConstructor(uint64 Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 2;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Failure == EConvFailureKind::AmbiguousOperator)
	{
		AppendLine(Source, TEXT("struct FAmbiguousConversionOperator"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint opAdd(int64 Value) const"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint opAdd(uint64 Value) const"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 2;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Failure == EConvFailureKind::ExplicitOnlyImplicitUse)
	{
		AppendLine(Source, TEXT("struct FExplicitOnlyConversion"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tFExplicitOnlyConversion(int InValue)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = InValue;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint opConv() const"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn Value;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Failure == EConvFailureKind::ConversionException)
	{
		AppendLine(Source, TEXT("struct FThrowingConversion"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tFThrowingConversion(int InValue)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = InValue;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint opImplConv() const"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tint Zero = 0;"));
		AppendLine(Source, TEXT("\t\treturn Value / Zero;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Failure == EConvFailureKind::ConstructorException)
	{
		AppendLine(Source, TEXT("struct FThrowingConversionConstructor"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tFThrowingConversionConstructor(int InValue)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tint Zero = 0;"));
		AppendLine(Source, TEXT("\t\tValue = InValue / Zero;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Failure == EConvFailureKind::NullValueTarget)
	{
		AppendLine(Source, TEXT("class FNullConversionTarget"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else
	{
		AppendLine(Source, TEXT("struct FConditionalConversionValue"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendRecoveryHelper(FString& Source)
{
	AppendLine(Source, TEXT("int RunConversionRecovery()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 77;"));
	AppendLine(Source, TEXT("}"));
}

void AppendAggregateHelpers(FString& Source)
{
	AppendFailureHelpers(Source, EConvFailureKind::AmbiguousConstructor);
	AppendFailureHelpers(Source, EConvFailureKind::AmbiguousOperator);
	AppendFailureHelpers(Source, EConvFailureKind::NullValueTarget);
	AppendFailureHelpers(Source, EConvFailureKind::ConversionException);
	AppendFailureHelpers(Source, EConvFailureKind::ConstructorException);
	AppendRecoveryHelper(Source);
}

void AppendEntry(FString& Source, const FConvFailureParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t// Recovery: %s"), RecoveryToken(Params.Recovery)));
	if (Params.Failure == EConvFailureKind::ImplicitNarrowing)
	{
		AppendLine(Source, TEXT("\tfloat SourceValue = 1.5;"));
		AppendLine(Source, TEXT("\treturn RequireFailureEnum(SourceValue); // CONVERSION_CAUSE"));
	}
	else if (Params.Failure == EConvFailureKind::NumericToEnum)
	{
		AppendLine(Source, TEXT("\tint SourceValue = 1;"));
		AppendLine(Source, TEXT("\treturn RequireFailureEnum(SourceValue); // CONVERSION_CAUSE"));
	}
	else if (Params.Failure == EConvFailureKind::UnrelatedReference)
	{
		AppendLine(Source, TEXT("\tFUnrelatedConversionLeft SourceValue = nullptr;"));
		AppendLine(Source, TEXT("\treturn RequireUnrelatedReference(SourceValue); // CONVERSION_CAUSE"));
	}
	else if (Params.Failure == EConvFailureKind::BadDowncast)
	{
		AppendLine(Source, TEXT("\tFConversionBase SourceValue = nullptr;"));
		AppendLine(Source, TEXT("\treturn RequireDerivedReference(SourceValue); // CONVERSION_CAUSE"));
	}
	else if (Params.Failure == EConvFailureKind::NullValueTarget)
	{
		AppendLine(Source, TEXT("\tFNullConversionTarget Target = nullptr;"));
		AppendLine(Source, TEXT("\treturn Target.Value; // CONVERSION_CAUSE"));
	}
	else if (Params.Failure == EConvFailureKind::AmbiguousConstructor)
	{
		AppendLine(Source, TEXT("\treturn SelectAmbiguousConstructor(int8(1)); // CONVERSION_CAUSE"));
	}
	else if (Params.Failure == EConvFailureKind::AmbiguousOperator)
	{
		AppendLine(Source, TEXT("\tFAmbiguousConversionOperator SourceValue;"));
		AppendLine(Source, TEXT("\treturn SourceValue + int8(1); // CONVERSION_CAUSE"));
	}
	else if (Params.Failure == EConvFailureKind::ExplicitOnlyImplicitUse)
	{
		AppendLine(Source, TEXT("\tFExplicitOnlyConversion SourceValue(9);"));
		AppendLine(Source, TEXT("\tint ConvertedValue = SourceValue; // CONVERSION_CAUSE"));
		AppendLine(Source, TEXT("\treturn ConvertedValue;"));
	}
	else if (Params.Failure == EConvFailureKind::ConversionException)
	{
		AppendLine(Source, TEXT("\tFThrowingConversion SourceValue(9);"));
		AppendLine(Source, TEXT("\tint ConvertedValue = SourceValue; // CONVERSION_CAUSE"));
		AppendLine(Source, TEXT("\treturn ConvertedValue;"));
	}
	else if (Params.Failure == EConvFailureKind::ConstructorException)
	{
		AppendLine(Source, TEXT("\tFThrowingConversionConstructor SourceValue(9); // CONVERSION_CAUSE"));
		AppendLine(Source, TEXT("\treturn SourceValue.Value;"));
	}
	else if (Params.Failure == EConvFailureKind::AbiMismatch)
	{
		AppendLine(Source, TEXT("\tdouble SourceValue = 1.5;"));
		AppendLine(Source, TEXT("\tFConditionalConversionValue ConvertedValue = SourceValue; // CONVERSION_CAUSE"));
		AppendLine(Source, TEXT("\treturn ConvertedValue.Value;"));
	}
	else
	{
		AppendLine(Source, TEXT("\tbool bUseNumber = true;"));
		AppendLine(Source, TEXT("\treturn bUseNumber ? 1 : FConditionalConversionValue(); // CONVERSION_CAUSE"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FConvFailureParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendFailureHelpers(Source, Params.Failure);
	AppendRecoveryHelper(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FConvFailureCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Failure))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::Standalone;
	if (IsFault(Cell.Failure))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = FaultException(Cell.Failure);
		return Info;
	}
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = 1;
	return Info;
}
}

FStringView FConvFailureGenerator::GetProductId() const
{
	return TEXT("LANG-CONV-FAILURE");
}

TArray<FGeneratedCaseInfo> FConvFailureGenerator::ListCases() const
{
	using namespace ConvFailureGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FConvFailureCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("ConvFailure PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FConvFailureGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ConvFailureGeneratorDetail;
	FConvFailureCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FConvFailureGenerator::BuildDumpSource() const
{
	using namespace ConvFailureGeneratorDetail;
	const TArray<FConvFailureCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CONV-FAILURE"));
	AppendLine(Source, TEXT("// Cases: 24 total, 4 normal, 14 reject, 6 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendAggregateHelpers(Source);
	bool bNeedBlank = true;
	for (const FConvFailureCell& Cell : Cells)
	{
		if (IsReject(Cell.Failure))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (bNeedBlank)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cell.Failure))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedException: %s"), *Info.ExpectedException));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, TEXT("// ExpectedReturn: 1"));
		}
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	for (const FConvFailureCell& Cell : Cells)
	{
		if (!IsReject(Cell.Failure))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		AppendFailureHelpers(Source, Cell.Failure);
		AppendRecoveryHelper(Source);
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FConvFailureGenerator::BuildFailureSource(const FConvFailureParams& Params) const
{
	using namespace ConvFailureGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Failure))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	return BuildOwnedSource(Params, FunctionName);
}

FString FConvFailureGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ConvFailureGeneratorDetail;
	const TArray<FConvFailureCell> Cells = AllCells();
	FString Source;
	AppendAggregateHelpers(Source);
	int32 Emitted = 0;
	for (const FConvFailureCell& Cell : Cells)
	{
		if (IsReject(Cell.Failure))
		{
			continue;
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FConvFailureGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ConvFailureGeneratorDetail;
	FConvFailureCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Failure) || IsFault(Cell.Failure))
	{
		return 0;
	}
	return 1;
}

FString FConvFailureGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ConvFailureGeneratorDetail;
	FConvFailureCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Failure))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FConvFailureGenerator::ListRejectCaseIds() const
{
	using namespace ConvFailureGeneratorDetail;
	TArray<FString> Ids;
	for (const FConvFailureCell& Cell : AllCells())
	{
		if (IsReject(Cell.Failure))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
