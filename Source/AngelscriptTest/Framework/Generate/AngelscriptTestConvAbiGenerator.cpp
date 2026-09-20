#include "Framework/Generate/AngelscriptTestConvAbiGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ConvAbiGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT(
	"Needs CaseId-qualified float32/float64 host registration. ExpectedBits is the host memcpy oracle; int32 101/202/1 is only the direction marker.");

struct FConvAbiCell
{
	EConvAbiScriptDeclaration ScriptDeclaration = EConvAbiScriptDeclaration::Float;
	EConvAbiNativeStorage NativeStorage = EConvAbiNativeStorage::Float32;
	EConvAbiDirection Direction = EConvAbiDirection::Argument;
};

const TCHAR* ScriptToken(const EConvAbiScriptDeclaration ScriptDeclaration)
{
	switch (ScriptDeclaration)
	{
	case EConvAbiScriptDeclaration::Float:
		return TEXT("FLOAT");
	case EConvAbiScriptDeclaration::Double:
		return TEXT("DOUBLE");
	default:
		return nullptr;
	}
}

const TCHAR* StorageToken(const EConvAbiNativeStorage NativeStorage)
{
	switch (NativeStorage)
	{
	case EConvAbiNativeStorage::Float32:
		return TEXT("FLOAT32");
	case EConvAbiNativeStorage::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* DirectionToken(const EConvAbiDirection Direction)
{
	switch (Direction)
	{
	case EConvAbiDirection::Argument:
		return TEXT("ARGUMENT");
	case EConvAbiDirection::Return:
		return TEXT("RETURN");
	case EConvAbiDirection::Property:
		return TEXT("PROPERTY");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FConvAbiParams& Params)
{
	return ScriptToken(Params.ScriptDeclaration) != nullptr
		&& StorageToken(Params.NativeStorage) != nullptr
		&& DirectionToken(Params.Direction) != nullptr;
}

const TCHAR* ScriptType(const EConvAbiScriptDeclaration ScriptDeclaration)
{
	return ScriptDeclaration == EConvAbiScriptDeclaration::Double ? TEXT("double") : TEXT("float");
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FConvAbiCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CONV-ABI-%s-%s-%s"),
		ScriptToken(Cell.ScriptDeclaration),
		StorageToken(Cell.NativeStorage),
		DirectionToken(Cell.Direction));
}

FString MakeEntryName(const FConvAbiCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString HostQualifier(const FConvAbiCell& Cell)
{
	const FString EntryName = MakeEntryName(Cell);
	return EntryName.Mid(5);
}

FConvAbiParams ParamsFromCell(const FConvAbiCell& Cell)
{
	FConvAbiParams Params;
	Params.ScriptDeclaration = Cell.ScriptDeclaration;
	Params.NativeStorage = Cell.NativeStorage;
	Params.Direction = Cell.Direction;
	return Params;
}

TArray<FConvAbiCell> AllCells()
{
	TArray<FConvAbiCell> Cells;
	Cells.SetNum(12);
	for (int32 Index = 0; Index < 12; ++Index)
	{
		FConvAbiCell& Cell = Cells[Index];
		Cell.ScriptDeclaration = static_cast<EConvAbiScriptDeclaration>(Index / 6);
		Cell.NativeStorage = static_cast<EConvAbiNativeStorage>((Index / 3) % 2);
		Cell.Direction = static_cast<EConvAbiDirection>(Index % 3);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FConvAbiCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FConvAbiCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

int32 ExpectedMarker(const EConvAbiNativeStorage NativeStorage, const EConvAbiDirection Direction)
{
	if (Direction != EConvAbiDirection::Argument)
	{
		return 1;
	}
	return NativeStorage == EConvAbiNativeStorage::Float32 ? 101 : 202;
}

void AppendEntry(FString& Source, const FConvAbiParams& Params, const FString& FunctionName)
{
	FConvAbiCell Cell;
	Cell.ScriptDeclaration = Params.ScriptDeclaration;
	Cell.NativeStorage = Params.NativeStorage;
	Cell.Direction = Params.Direction;
	const TCHAR* const Type = ScriptType(Params.ScriptDeclaration);
	const FString Qualifier = HostQualifier(Cell);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Direction == EConvAbiDirection::Argument)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = %s(3.25);"), Type, Type));
		AppendLine(Source, FString::Printf(TEXT("\treturn CallAbi%s(Value);"), *Qualifier));
	}
	else if (Params.Direction == EConvAbiDirection::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Result = CallAbi%s();"), Type, *Qualifier));
		AppendLine(Source, FString::Printf(TEXT("\treturn Result == %s(3.25) ? 1 : 0;"), Type));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Input = %s(6.5);"), Type, Type));
		AppendLine(Source, FString::Printf(TEXT("\tAbiValue%s = Input;"), *Qualifier));
		AppendLine(Source, FString::Printf(TEXT("\t%s Output = AbiValue%s;"), Type, *Qualifier));
		AppendLine(Source, TEXT("\treturn Output == Input ? 1 : 0;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FConvAbiParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FConvAbiCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedMarker(Cell.NativeStorage, Cell.Direction);
	Info.bLimitedObservation = true;
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FConvAbiGenerator::GetProductId() const
{
	return TEXT("LANG-CONV-ABI");
}

TArray<FGeneratedCaseInfo> FConvAbiGenerator::ListCases() const
{
	using namespace ConvAbiGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FConvAbiCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("ConvAbi PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FConvAbiGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ConvAbiGeneratorDetail;
	FConvAbiCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FConvAbiGenerator::BuildDumpSource() const
{
	using namespace ConvAbiGeneratorDetail;
	const TArray<FConvAbiCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CONV-ABI"));
	AppendLine(Source, TEXT("// Cases: 12 total, 12 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FConvAbiCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source, TEXT("// LimitedObservation: true"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FConvAbiGenerator::BuildAbiSource(const FConvAbiParams& Params) const
{
	using namespace ConvAbiGeneratorDetail;
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

FString FConvAbiGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ConvAbiGeneratorDetail;
	const TArray<FConvAbiCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (Index > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

int32 FConvAbiGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ConvAbiGeneratorDetail;
	FConvAbiCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedMarker(Cell.NativeStorage, Cell.Direction);
}
}
