#include "Framework/Generate/AngelscriptTestOpUnaryRejectionGenerator.h"

#include "Containers/Map.h"
#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpUnaryRejectionGeneratorDetail
{
struct FRejectionCell
{
	EOpUnaryRejectionCategory Category = EOpUnaryRejectionCategory::MutableLvalue;
	EOpUnaryRejectionFailure Failure = EOpUnaryRejectionFailure::PositiveBool;
};

const TCHAR* CategoryToken(const EOpUnaryRejectionCategory Category)
{
	switch (Category)
	{
	case EOpUnaryRejectionCategory::MutableLvalue:
		return TEXT("MUTABLE_LVALUE");
	case EOpUnaryRejectionCategory::ConstLvalue:
		return TEXT("CONST_LVALUE");
	case EOpUnaryRejectionCategory::Temporary:
		return TEXT("TEMPORARY");
	case EOpUnaryRejectionCategory::Field:
		return TEXT("FIELD");
	case EOpUnaryRejectionCategory::Alias:
		return TEXT("ALIAS");
	default:
		return nullptr;
	}
}

const TCHAR* FailureToken(const EOpUnaryRejectionFailure Failure)
{
	switch (Failure)
	{
	case EOpUnaryRejectionFailure::PositiveBool:
		return TEXT("POSITIVE_BOOL");
	case EOpUnaryRejectionFailure::NegativeBool:
		return TEXT("NEGATIVE_BOOL");
	case EOpUnaryRejectionFailure::BitNotBool:
		return TEXT("BIT_NOT_BOOL");
	case EOpUnaryRejectionFailure::LogicalNotSigned:
		return TEXT("LOGICAL_NOT_SIGNED");
	case EOpUnaryRejectionFailure::LogicalNotUnsigned:
		return TEXT("LOGICAL_NOT_UNSIGNED");
	case EOpUnaryRejectionFailure::LogicalNotFloat:
		return TEXT("LOGICAL_NOT_FLOAT");
	case EOpUnaryRejectionFailure::BitNotFloat32:
		return TEXT("BIT_NOT_FLOAT32");
	case EOpUnaryRejectionFailure::BitNotFloat64:
		return TEXT("BIT_NOT_FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* FailureType(const EOpUnaryRejectionFailure Failure)
{
	switch (Failure)
	{
	case EOpUnaryRejectionFailure::PositiveBool:
	case EOpUnaryRejectionFailure::NegativeBool:
	case EOpUnaryRejectionFailure::BitNotBool:
		return TEXT("bool");
	case EOpUnaryRejectionFailure::LogicalNotSigned:
		return TEXT("int");
	case EOpUnaryRejectionFailure::LogicalNotUnsigned:
		return TEXT("uint");
	case EOpUnaryRejectionFailure::LogicalNotFloat:
	case EOpUnaryRejectionFailure::BitNotFloat64:
		return TEXT("float64");
	case EOpUnaryRejectionFailure::BitNotFloat32:
		return TEXT("float32");
	default:
		return nullptr;
	}
}

const TCHAR* FailureOperator(const EOpUnaryRejectionFailure Failure)
{
	switch (Failure)
	{
	case EOpUnaryRejectionFailure::PositiveBool:
		return TEXT("+");
	case EOpUnaryRejectionFailure::NegativeBool:
		return TEXT("-");
	case EOpUnaryRejectionFailure::BitNotBool:
	case EOpUnaryRejectionFailure::BitNotFloat32:
	case EOpUnaryRejectionFailure::BitNotFloat64:
		return TEXT("~");
	default:
		return TEXT("!");
	}
}

const TCHAR* FailureLiteral(const EOpUnaryRejectionFailure Failure)
{
	switch (Failure)
	{
	case EOpUnaryRejectionFailure::PositiveBool:
	case EOpUnaryRejectionFailure::NegativeBool:
	case EOpUnaryRejectionFailure::BitNotBool:
		return TEXT("false");
	case EOpUnaryRejectionFailure::LogicalNotFloat:
	case EOpUnaryRejectionFailure::BitNotFloat32:
	case EOpUnaryRejectionFailure::BitNotFloat64:
		return TEXT("0.0");
	default:
		return TEXT("0");
	}
}

bool IsValidParams(const FOpUnaryRejectionParams& Params)
{
	return CategoryToken(Params.Category) != nullptr && FailureToken(Params.Failure) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FRejectionCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-UNARY-REJECTION-%s-%s"),
		CategoryToken(Cell.Category),
		FailureToken(Cell.Failure));
}

FString MakeEntryName(const FRejectionCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpUnaryRejectionParams ParamsFromCell(const FRejectionCell& Cell)
{
	FOpUnaryRejectionParams Params;
	Params.Category = Cell.Category;
	Params.Failure = Cell.Failure;
	return Params;
}

FRejectionCell CellFromParams(const FOpUnaryRejectionParams& Params)
{
	FRejectionCell Cell;
	Cell.Category = Params.Category;
	Cell.Failure = Params.Failure;
	return Cell;
}

TArray<FRejectionCell> AllCells()
{
	TArray<FRejectionCell> Cells;
	Cells.SetNum(40);
	for (int32 Index = 0; Index < 40; ++Index)
	{
		FRejectionCell& Cell = Cells[Index];
		Cell.Category = static_cast<EOpUnaryRejectionCategory>(Index / 8);
		Cell.Failure = static_cast<EOpUnaryRejectionFailure>(Index % 8);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FRejectionCell& OutCell)
{
	static const TArray<FRejectionCell> Cells = AllCells();
	static TMap<FString, int32> IndexById;
	if (IndexById.Num() == 0)
	{
		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			IndexById.Add(MakeCaseId(Cells[Index]), Index);
		}
	}
	if (const int32* const Found = IndexById.Find(FString(CaseId)))
	{
		OutCell = Cells[*Found];
		return true;
	}
	return false;
}

void AppendCategoryHelpers(FString& Source, const FOpUnaryRejectionParams& Params)
{
	const TCHAR* const Type = FailureType(Params.Failure);
	if (Params.Category == EOpUnaryRejectionCategory::Temporary)
	{
		AppendLine(Source, FString::Printf(TEXT("%s MakeInvalidUnaryTemporary(%s Input)"), Type, Type));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Input;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		return;
	}
	if (Params.Category == EOpUnaryRejectionCategory::Field)
	{
		AppendLine(Source, TEXT("struct FInvalidUnaryOwner"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Value;"), Type));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendEntry(FString& Source, const FOpUnaryRejectionParams& Params, const FString& FunctionName)
{
	const TCHAR* const Type = FailureType(Params.Failure);
	const TCHAR* const Token = FailureOperator(Params.Failure);
	const TCHAR* const Literal = FailureLiteral(Params.Failure);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Input = %s;"), Type, Literal));
	if (Params.Category == EOpUnaryRejectionCategory::MutableLvalue)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), Type));
		AppendLine(Source, FString::Printf(TEXT("\treturn int(%sValue); // UNARY_CAUSE"), Token));
	}
	else if (Params.Category == EOpUnaryRejectionCategory::ConstLvalue)
	{
		AppendLine(Source, FString::Printf(TEXT("\tconst %s Value = Input;"), Type));
		AppendLine(Source, FString::Printf(TEXT("\treturn int(%sValue); // UNARY_CAUSE"), Token));
	}
	else if (Params.Category == EOpUnaryRejectionCategory::Temporary)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn int(%sMakeInvalidUnaryTemporary(Input)); // UNARY_CAUSE"), Token));
	}
	else if (Params.Category == EOpUnaryRejectionCategory::Field)
	{
		AppendLine(Source, TEXT("\tFInvalidUnaryOwner Owner;"));
		AppendLine(Source, TEXT("\tOwner.Value = Input;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn int(%sOwner.Value); // UNARY_CAUSE"), Token));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn int(%sInput); // UNARY_CAUSE"), Token));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpUnaryRejectionParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendCategoryHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FRejectionCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	return Info;
}
}

FStringView FOpUnaryRejectionGenerator::GetProductId() const
{
	return TEXT("LANG-OP-UNARY-REJECTION");
}

TArray<FGeneratedCaseInfo> FOpUnaryRejectionGenerator::ListCases() const
{
	using namespace OpUnaryRejectionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FRejectionCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpUnaryRejection PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpUnaryRejectionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpUnaryRejectionGeneratorDetail;
	FRejectionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpUnaryRejectionGenerator::BuildDumpSource() const
{
	using namespace OpUnaryRejectionGeneratorDetail;
	const TArray<FRejectionCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-UNARY-REJECTION"));
	AppendLine(Source, TEXT("// Cases: 40 total, 0 normal, 40 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		AppendCategoryHelpers(Source, ParamsFromCell(Cells[Index]));
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FOpUnaryRejectionGenerator::BuildUnaryFailureSource(const FOpUnaryRejectionParams& Params) const
{
	using namespace OpUnaryRejectionGeneratorDetail;
	if (!IsValidParams(Params))
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

FString FOpUnaryRejectionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

int32 FOpUnaryRejectionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpUnaryRejectionGeneratorDetail;
	FRejectionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return 0;
}

FString FOpUnaryRejectionGenerator::BuildRejectSource(const FStringView CaseId) const
{
	return BuildCaseSource(CaseId);
}

TArray<FString> FOpUnaryRejectionGenerator::ListRejectCaseIds() const
{
	using namespace OpUnaryRejectionGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FRejectionCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Ids.Add(MakeCaseId(Cells[Index]));
	}
	return Ids;
}
}
