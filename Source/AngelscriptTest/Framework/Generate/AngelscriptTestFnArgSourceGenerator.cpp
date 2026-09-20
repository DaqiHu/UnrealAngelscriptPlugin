#include "Framework/Generate/AngelscriptTestFnArgSourceGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnArgSourceGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native FNativeCaseReference.");
constexpr const TCHAR* FaultException = TEXT("Null pointer access");

struct FArgSourceCell
{
	EFnArgSourceDirection Direction = EFnArgSourceDirection::Value;
	EFnArgSourceSource Source = EFnArgSourceSource::Literal;
};

const TCHAR* DirectionToken(const EFnArgSourceDirection Direction)
{
	switch (Direction)
	{
	case EFnArgSourceDirection::Value:
		return TEXT("VALUE");
	case EFnArgSourceDirection::In:
		return TEXT("IN");
	case EFnArgSourceDirection::Out:
		return TEXT("OUT");
	case EFnArgSourceDirection::Inout:
		return TEXT("INOUT");
	default:
		return nullptr;
	}
}

const TCHAR* SourceToken(const EFnArgSourceSource Source)
{
	switch (Source)
	{
	case EFnArgSourceSource::Literal:
		return TEXT("LITERAL");
	case EFnArgSourceSource::LocalLvalue:
		return TEXT("LOCAL_LVALUE");
	case EFnArgSourceSource::ConstLocal:
		return TEXT("CONST_LOCAL");
	case EFnArgSourceSource::GlobalConst:
		return TEXT("GLOBAL_CONST");
	case EFnArgSourceSource::Field:
		return TEXT("FIELD");
	case EFnArgSourceSource::FunctionReturn:
		return TEXT("FUNCTION_RETURN");
	case EFnArgSourceSource::ArithmeticExpression:
		return TEXT("ARITHMETIC_EXPRESSION");
	case EFnArgSourceSource::ConditionalExpression:
		return TEXT("CONDITIONAL_EXPRESSION");
	case EFnArgSourceSource::Null:
		return TEXT("NULL");
	case EFnArgSourceSource::BaseView:
		return TEXT("BASE_VIEW");
	case EFnArgSourceSource::DerivedView:
		return TEXT("DERIVED_VIEW");
	default:
		return nullptr;
	}
}

const TCHAR* DirectionSuffix(const EFnArgSourceDirection Direction)
{
	switch (Direction)
	{
	case EFnArgSourceDirection::Value:
		return TEXT("");
	case EFnArgSourceDirection::In:
		return TEXT("& in");
	case EFnArgSourceDirection::Out:
		return TEXT("& out");
	case EFnArgSourceDirection::Inout:
		return TEXT("& inout");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FFnArgSourceParams& Params)
{
	return DirectionToken(Params.Direction) != nullptr && SourceToken(Params.Source) != nullptr;
}

bool WritesValue(const EFnArgSourceDirection Direction)
{
	return Direction == EFnArgSourceDirection::Out || Direction == EFnArgSourceDirection::Inout;
}

bool IsObjectSource(const EFnArgSourceSource Source)
{
	return Source == EFnArgSourceSource::Null
		|| Source == EFnArgSourceSource::BaseView
		|| Source == EFnArgSourceSource::DerivedView;
}

bool IsReject(const EFnArgSourceDirection Direction, const EFnArgSourceSource Source)
{
	if (Source == EFnArgSourceSource::DerivedView)
	{
		return Direction != EFnArgSourceDirection::Value;
	}
	if (Direction == EFnArgSourceDirection::Value || Direction == EFnArgSourceDirection::In)
	{
		return false;
	}
	return Source != EFnArgSourceSource::LocalLvalue
		&& Source != EFnArgSourceSource::Field
		&& Source != EFnArgSourceSource::BaseView;
}

bool IsFault(const EFnArgSourceSource Source)
{
	return Source == EFnArgSourceSource::BaseView || Source == EFnArgSourceSource::DerivedView;
}

bool NeedsHost(const EFnArgSourceSource Source)
{
	return Source == EFnArgSourceSource::Null;
}

int32 ExpectedFromCell(const EFnArgSourceDirection Direction, const EFnArgSourceSource Source)
{
	if (IsReject(Direction, Source) || IsFault(Source))
	{
		return 0;
	}
	if (WritesValue(Direction))
	{
		return 18;
	}
	return Source == EFnArgSourceSource::Null ? 1 : 7;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FArgSourceCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-ARG-SOURCE-%s-%s"),
		DirectionToken(Cell.Direction),
		SourceToken(Cell.Source));
}

FString MakeEntryName(const FArgSourceCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FFnArgSourceParams ParamsFromCell(const FArgSourceCell& Cell)
{
	FFnArgSourceParams Params;
	Params.Direction = Cell.Direction;
	Params.Source = Cell.Source;
	return Params;
}

TArray<FArgSourceCell> AllCells()
{
	TArray<FArgSourceCell> Cells;
	Cells.SetNum(44);
	for (int32 Index = 0; Index < 44; ++Index)
	{
		FArgSourceCell& Cell = Cells[Index];
		Cell.Direction = static_cast<EFnArgSourceDirection>(Index / 11);
		Cell.Source = static_cast<EFnArgSourceSource>(Index % 11);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FArgSourceCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FArgSourceCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString ProbeNameFor(const FString& FunctionName)
{
	return FunctionName == TEXT("Entry") ? FString(TEXT("Probe")) : TEXT("Probe") + FunctionName;
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("const int GlobalConstValue = 7;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ProvideValue()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 7;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FHolder"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 7;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FBase"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 7;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FDerived : FBase"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
}

FString ArgumentExpression(const EFnArgSourceSource Source)
{
	switch (Source)
	{
	case EFnArgSourceSource::Literal:
		return TEXT("7");
	case EFnArgSourceSource::LocalLvalue:
	case EFnArgSourceSource::ConstLocal:
		return TEXT("Value");
	case EFnArgSourceSource::GlobalConst:
		return TEXT("GlobalConstValue");
	case EFnArgSourceSource::Field:
		return TEXT("Holder.Value");
	case EFnArgSourceSource::FunctionReturn:
		return TEXT("ProvideValue()");
	case EFnArgSourceSource::ArithmeticExpression:
		return TEXT("3 + 4");
	case EFnArgSourceSource::ConditionalExpression:
		return TEXT("true ? 7 : 8");
	case EFnArgSourceSource::Null:
		return TEXT("NullValue");
	case EFnArgSourceSource::BaseView:
	case EFnArgSourceSource::DerivedView:
	default:
		return TEXT("Value");
	}
}

const TCHAR* ProbeType(const EFnArgSourceSource Source)
{
	if (Source == EFnArgSourceSource::Null)
	{
		return TEXT("FNativeCaseReference");
	}
	return IsObjectSource(Source) ? TEXT("FBase") : TEXT("int");
}

void AppendProbe(FString& Source, const FFnArgSourceParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(
		TEXT("int %s(%s%s Value)"),
		*ProbeNameFor(FunctionName),
		ProbeType(Params.Source),
		DirectionSuffix(Params.Direction)));
	AppendLine(Source, TEXT("{"));
	if (WritesValue(Params.Direction))
	{
		if (IsObjectSource(Params.Source))
		{
			AppendLine(Source, TEXT("\tValue = FDerived();"));
		}
		else
		{
			AppendLine(Source, TEXT("\tValue = 9;"));
		}
	}
	if (IsObjectSource(Params.Source))
	{
		AppendLine(Source, TEXT("\treturn Value == nullptr ? 1 : Value.Value;"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn Value;"));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendEntryLocals(FString& Source, const EFnArgSourceSource SourceKind)
{
	switch (SourceKind)
	{
	case EFnArgSourceSource::LocalLvalue:
		AppendLine(Source, TEXT("\tint Value = 7;"));
		break;
	case EFnArgSourceSource::ConstLocal:
		AppendLine(Source, TEXT("\tconst int Value = 7;"));
		break;
	case EFnArgSourceSource::Field:
		AppendLine(Source, TEXT("\tFHolder Holder;"));
		break;
	case EFnArgSourceSource::BaseView:
		AppendLine(Source, TEXT("\tFBase Value;"));
		AppendLine(Source, TEXT("\tValue.Value = 7;"));
		break;
	case EFnArgSourceSource::DerivedView:
		AppendLine(Source, TEXT("\tFDerived Value;"));
		AppendLine(Source, TEXT("\tValue.Value = 7;"));
		break;
	case EFnArgSourceSource::Null:
		AppendLine(Source, TEXT("\tFNativeCaseReference NullValue = nullptr;"));
		break;
	default:
		break;
	}
}

void AppendEntry(FString& Source, const FFnArgSourceParams& Params, const FString& FunctionName)
{
	const FString Call = FString::Printf(
		TEXT("%s(%s)"),
		*ProbeNameFor(FunctionName),
		*ArgumentExpression(Params.Source));
	FString ReturnExpr = Call;
	if (WritesValue(Params.Direction))
	{
		if (IsObjectSource(Params.Source))
		{
			ReturnExpr = FString::Printf(TEXT("%s + (Value == nullptr ? 100 : Value.Value)"), *Call);
		}
		else if (Params.Source == EFnArgSourceSource::Field)
		{
			ReturnExpr = FString::Printf(TEXT("%s + Holder.Value"), *Call);
		}
		else
		{
			ReturnExpr = FString::Printf(TEXT("%s + Value"), *Call);
		}
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendEntryLocals(Source, Params.Source);
	AppendLine(Source, TEXT("\treturn ") + ReturnExpr + TEXT(";"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnArgSourceParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendLine(Source);
	AppendProbe(Source, Params, FunctionName);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FArgSourceCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Direction, Cell.Source))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	if (IsFault(Cell.Source))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = FaultException;
		Info.Execution = EGeneratedCaseExecution::Standalone;
		return Info;
	}
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = ExpectedFromCell(Cell.Direction, Cell.Source);
	if (NeedsHost(Cell.Source))
	{
		Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
		Info.ExecutionNotes = HostNotes;
	}
	else
	{
		Info.Execution = EGeneratedCaseExecution::Standalone;
	}
	return Info;
}
}

FStringView FFnArgSourceGenerator::GetProductId() const
{
	return TEXT("LANG-FN-ARG-SOURCE");
}

TArray<FGeneratedCaseInfo> FFnArgSourceGenerator::ListCases() const
{
	using namespace FnArgSourceGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FArgSourceCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("FnArgSource PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnArgSourceGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnArgSourceGeneratorDetail;
	FArgSourceCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnArgSourceGenerator::BuildDumpSource() const
{
	using namespace FnArgSourceGeneratorDetail;
	const TArray<FArgSourceCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-ARG-SOURCE"));
	AppendLine(Source, TEXT("// Cases: 44 total, 22 normal, 17 reject, 5 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);
	bool bNeedBlank = true;
	for (const FArgSourceCell& Cell : Cells)
	{
		if (IsReject(Cell.Direction, Cell.Source))
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
		if (IsFault(Cell.Source))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Null pointer access"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		AppendLine(Source, Info.Execution == EGeneratedCaseExecution::RequiresHostSetup
			? TEXT("// Execution: RequiresHostSetup")
			: TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendProbe(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	for (const FArgSourceCell& Cell : Cells)
	{
		if (!IsReject(Cell.Direction, Cell.Source))
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
		AppendHelpers(Source);
		AppendLine(Source);
		AppendProbe(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnArgSourceGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnArgSourceGeneratorDetail;
	FString Source;
	AppendHelpers(Source);
	int32 Emitted = 0;
	for (const FArgSourceCell& Cell : AllCells())
	{
		if (IsReject(Cell.Direction, Cell.Source))
		{
			continue;
		}
		AppendLine(Source);
		AppendProbe(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FFnArgSourceGenerator::BuildArgumentSource(const FFnArgSourceParams& Params) const
{
	using namespace FnArgSourceGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Direction, Params.Source))
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

int32 FFnArgSourceGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnArgSourceGeneratorDetail;
	FArgSourceCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Direction, Cell.Source) || IsFault(Cell.Source))
	{
		return 0;
	}
	return ExpectedFromCell(Cell.Direction, Cell.Source);
}

FString FFnArgSourceGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace FnArgSourceGeneratorDetail;
	FArgSourceCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Direction, Cell.Source))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FFnArgSourceGenerator::ListRejectCaseIds() const
{
	using namespace FnArgSourceGeneratorDetail;
	TArray<FString> Ids;
	for (const FArgSourceCell& Cell : AllCells())
	{
		if (IsReject(Cell.Direction, Cell.Source))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
