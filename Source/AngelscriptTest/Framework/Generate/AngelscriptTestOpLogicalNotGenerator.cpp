#include "Framework/Generate/AngelscriptTestOpLogicalNotGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpLogicalNotGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native ObserveNotOperand.");

struct FLogicalNotCell
{
	EOpLogicalNotCategory Category = EOpLogicalNotCategory::MutableLValue;
	EOpLogicalNotValue Value = EOpLogicalNotValue::False;
};

const TCHAR* CategoryToken(const EOpLogicalNotCategory Category)
{
	switch (Category)
	{
	case EOpLogicalNotCategory::MutableLValue:
		return TEXT("MUTABLE_LVALUE");
	case EOpLogicalNotCategory::ConstLValue:
		return TEXT("CONST_LVALUE");
	case EOpLogicalNotCategory::Temporary:
		return TEXT("TEMPORARY");
	case EOpLogicalNotCategory::Field:
		return TEXT("FIELD");
	case EOpLogicalNotCategory::Alias:
		return TEXT("ALIAS");
	default:
		return nullptr;
	}
}

const TCHAR* ValueToken(const EOpLogicalNotValue Value)
{
	switch (Value)
	{
	case EOpLogicalNotValue::False:
		return TEXT("FALSE");
	case EOpLogicalNotValue::True:
		return TEXT("TRUE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FOpLogicalNotParams& Params)
{
	return CategoryToken(Params.Category) != nullptr && ValueToken(Params.Value) != nullptr;
}

bool ValueBool(const EOpLogicalNotValue Value)
{
	return Value == EOpLogicalNotValue::True;
}

int32 ExpectedBits(const EOpLogicalNotValue Value)
{
	return ValueBool(Value) ? 0 : 1;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FLogicalNotCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-LOGICAL-NOT-%s-%s"),
		CategoryToken(Cell.Category),
		ValueToken(Cell.Value));
}

FString MakeEntryName(const FLogicalNotCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpLogicalNotParams ParamsFromCell(const FLogicalNotCell& Cell)
{
	FOpLogicalNotParams Params;
	Params.Category = Cell.Category;
	Params.Value = Cell.Value;
	return Params;
}

TArray<FLogicalNotCell> AllCells()
{
	TArray<FLogicalNotCell> Cells;
	Cells.SetNum(10);
	for (int32 Index = 0; Index < 10; ++Index)
	{
		FLogicalNotCell& Cell = Cells[Index];
		Cell.Category = static_cast<EOpLogicalNotCategory>(Index / 2);
		Cell.Value = static_cast<EOpLogicalNotValue>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FLogicalNotCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FLogicalNotCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int ObserveLogicalNotType(bool Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 301;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObserveLogicalNotType(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 399;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("bool MakeLogicalNotTemporary(bool Input)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Input;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FLogicalNotOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tbool Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("bool ApplyLogicalNotAlias(bool& in Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn !ObserveNotOperand(Value);"));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FOpLogicalNotParams& Params, const FString& FunctionName)
{
	const TCHAR* const Literal = ValueBool(Params.Value) ? TEXT("true") : TEXT("false");
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tbool Input = %s;"), Literal));
	if (Params.Category == EOpLogicalNotCategory::MutableLValue)
	{
		AppendLine(Source, TEXT("\tbool Value = Input;"));
		AppendLine(Source, TEXT("\tconst bool Result = !ObserveNotOperand(Value);"));
	}
	else if (Params.Category == EOpLogicalNotCategory::ConstLValue)
	{
		AppendLine(Source, TEXT("\tconst bool Value = Input;"));
		AppendLine(Source, TEXT("\tconst bool Result = !ObserveNotOperand(Value);"));
	}
	else if (Params.Category == EOpLogicalNotCategory::Temporary)
	{
		AppendLine(Source, TEXT("\tconst bool Result = !MakeLogicalNotTemporary(ObserveNotOperand(Input));"));
	}
	else if (Params.Category == EOpLogicalNotCategory::Field)
	{
		AppendLine(Source, TEXT("\tFLogicalNotOwner Owner;"));
		AppendLine(Source, TEXT("\tOwner.Value = Input;"));
		AppendLine(Source, TEXT("\tconst bool Result = !ObserveNotOperand(Owner.Value);"));
	}
	else
	{
		AppendLine(Source, TEXT("\tbool Value = Input;"));
		AppendLine(Source, TEXT("\tconst bool Result = ApplyLogicalNotAlias(Value);"));
	}
	AppendLine(Source, TEXT("\treturn Result ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpLogicalNotParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FLogicalNotCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedBits(Cell.Value);
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FOpLogicalNotGenerator::GetProductId() const
{
	return TEXT("LANG-OP-LOGICAL-NOT");
}

TArray<FGeneratedCaseInfo> FOpLogicalNotGenerator::ListCases() const
{
	using namespace OpLogicalNotGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FLogicalNotCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpLogicalNot PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpLogicalNotGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpLogicalNotGeneratorDetail;
	FLogicalNotCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpLogicalNotGenerator::BuildDumpSource() const
{
	using namespace OpLogicalNotGeneratorDetail;
	const TArray<FLogicalNotCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-LOGICAL-NOT"));
	AppendLine(Source, TEXT("// Cases: 10 total, 10 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FLogicalNotCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FOpLogicalNotGenerator::BuildLogicalNotSource(const FOpLogicalNotParams& Params) const
{
	using namespace OpLogicalNotGeneratorDetail;
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

FString FOpLogicalNotGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpLogicalNotGeneratorDetail;
	const TArray<FLogicalNotCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

int32 FOpLogicalNotGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpLogicalNotGeneratorDetail;
	FLogicalNotCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedBits(Cell.Value);
}
}
