#include "Framework/Generate/AngelscriptTestSwitchGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace SwitchGeneratorDetail
{
struct FSwitchCell
{
	ESwitchSelector Selector = ESwitchSelector::Int8;
	ESwitchCase Case = ESwitchCase::First;
	ESwitchExit Exit = ESwitchExit::Break;
};

const TCHAR* SelectorToken(const ESwitchSelector Selector)
{
	switch (Selector)
	{
	case ESwitchSelector::Int8:
		return TEXT("INT8");
	case ESwitchSelector::Int16:
		return TEXT("INT16");
	case ESwitchSelector::Int:
		return TEXT("INT");
	case ESwitchSelector::Int64:
		return TEXT("INT64");
	case ESwitchSelector::Uint8:
		return TEXT("UINT8");
	case ESwitchSelector::Uint16:
		return TEXT("UINT16");
	case ESwitchSelector::Uint:
		return TEXT("UINT");
	case ESwitchSelector::Uint64:
		return TEXT("UINT64");
	case ESwitchSelector::Enum:
		return TEXT("ENUM");
	case ESwitchSelector::Typedef:
		return TEXT("TYPEDEF");
	case ESwitchSelector::Boundary:
		return TEXT("BOUNDARY");
	case ESwitchSelector::Unsupported:
		return TEXT("UNSUPPORTED");
	default:
		return nullptr;
	}
}

const TCHAR* CaseToken(const ESwitchCase Case)
{
	switch (Case)
	{
	case ESwitchCase::First:
		return TEXT("FIRST");
	case ESwitchCase::Middle:
		return TEXT("MIDDLE");
	case ESwitchCase::Last:
		return TEXT("LAST");
	case ESwitchCase::Default:
		return TEXT("DEFAULT");
	case ESwitchCase::NoMatch:
		return TEXT("NO_MATCH");
	case ESwitchCase::Fallthrough:
		return TEXT("FALLTHROUGH");
	case ESwitchCase::Grouped:
		return TEXT("GROUPED");
	case ESwitchCase::Duplicate:
		return TEXT("DUPLICATE");
	case ESwitchCase::NonConstant:
		return TEXT("NON_CONSTANT");
	default:
		return nullptr;
	}
}

const TCHAR* ExitToken(const ESwitchExit Exit)
{
	switch (Exit)
	{
	case ESwitchExit::Break:
		return TEXT("BREAK");
	case ESwitchExit::Fallthrough:
		return TEXT("FALLTHROUGH");
	case ESwitchExit::Return:
		return TEXT("RETURN");
	case ESwitchExit::Exception:
		return TEXT("EXCEPTION");
	default:
		return nullptr;
	}
}

const TCHAR* SelectorTypeName(const ESwitchSelector Selector)
{
	switch (Selector)
	{
	case ESwitchSelector::Int8:
		return TEXT("int8");
	case ESwitchSelector::Int16:
		return TEXT("int16");
	case ESwitchSelector::Int:
		return TEXT("int");
	case ESwitchSelector::Int64:
		return TEXT("int64");
	case ESwitchSelector::Uint8:
		return TEXT("uint8");
	case ESwitchSelector::Uint16:
		return TEXT("uint16");
	case ESwitchSelector::Uint:
		return TEXT("uint");
	case ESwitchSelector::Uint64:
		return TEXT("uint64");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FSwitchParams& Params)
{
	return SelectorToken(Params.Selector) != nullptr
		&& CaseToken(Params.Case) != nullptr
		&& ExitToken(Params.Exit) != nullptr;
}

bool IsReject(const ESwitchSelector Selector, const ESwitchCase Case, const ESwitchExit Exit)
{
	if (Selector == ESwitchSelector::Unsupported)
	{
		return true;
	}
	if (Case == ESwitchCase::Duplicate || Case == ESwitchCase::NonConstant)
	{
		return true;
	}
	if ((Case == ESwitchCase::Default || Case == ESwitchCase::NoMatch) && Exit == ESwitchExit::Fallthrough)
	{
		return true;
	}
	return false;
}

bool IsRejectCell(const FSwitchCell& Cell)
{
	return IsReject(Cell.Selector, Cell.Case, Cell.Exit);
}

bool IsFaultCell(const FSwitchCell& Cell)
{
	return !IsRejectCell(Cell) && Cell.Exit == ESwitchExit::Exception;
}

int32 SelectorValueFor(const ESwitchCase Case)
{
	switch (Case)
	{
	case ESwitchCase::First:
	case ESwitchCase::Fallthrough:
		return 1;
	case ESwitchCase::Middle:
	case ESwitchCase::Grouped:
		return 2;
	case ESwitchCase::Last:
		return 3;
	case ESwitchCase::Default:
		return 4;
	default:
		return 5;
	}
}

int32 TraceSeed(const ESwitchCase Case)
{
	switch (Case)
	{
	case ESwitchCase::First:
		return 11;
	case ESwitchCase::Middle:
		return 12;
	case ESwitchCase::Last:
		return 13;
	case ESwitchCase::Default:
		return 14;
	case ESwitchCase::NoMatch:
		return 15;
	case ESwitchCase::Fallthrough:
		return 16;
	default:
		return 17;
	}
}

int32 ExpectedFromCell(const FSwitchCell& Cell)
{
	int32 Value = TraceSeed(Cell.Case) + (Cell.Case == ESwitchCase::Fallthrough ? 1 : 0);
	if (Cell.Selector == ESwitchSelector::Boundary)
	{
		Value += 6;
	}
	if (Cell.Exit == ESwitchExit::Fallthrough)
	{
		Value += 100;
	}
	else if (Cell.Exit == ESwitchExit::Return)
	{
		Value += 200;
	}
	return Value;
}

FString TypedLiteral(const ESwitchSelector Selector, const int32 Value)
{
	if (Selector == ESwitchSelector::Enum)
	{
		switch (Value)
		{
		case 0:
			return TEXT("ESelector::Zero");
		case 1:
			return TEXT("ESelector::One");
		case 2:
			return TEXT("ESelector::Two");
		case 3:
			return TEXT("ESelector::Three");
		case 4:
			return TEXT("ESelector::Four");
		case 5:
			return TEXT("ESelector::Five");
		default:
			return TEXT("ESelector::Six");
		}
	}
	if (Selector == ESwitchSelector::Boundary)
	{
		return FString::Printf(TEXT("%d"), 2147483641 + Value);
	}
	if (Selector == ESwitchSelector::Typedef)
	{
		return FString::Printf(TEXT("SelectorAlias(%d)"), Value);
	}
	if (Selector == ESwitchSelector::Unsupported)
	{
		return FString::FromInt(Value);
	}
	return FString::Printf(TEXT("%s(%d)"), SelectorTypeName(Selector), Value);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FSwitchCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CF-SWITCH-%s-%s-%s"),
		SelectorToken(Cell.Selector),
		CaseToken(Cell.Case),
		ExitToken(Cell.Exit));
}

FString MakeEntryName(const FSwitchCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FSwitchParams ParamsFromCell(const FSwitchCell& Cell)
{
	FSwitchParams Params;
	Params.Selector = Cell.Selector;
	Params.Case = Cell.Case;
	Params.Exit = Cell.Exit;
	return Params;
}

void AppendSelectorDeclaration(FString& Source, const ESwitchSelector Selector, const ESwitchCase Case)
{
	if (Selector == ESwitchSelector::Enum)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tESelector Value = %s;"),
			*TypedLiteral(ESwitchSelector::Enum, SelectorValueFor(Case))));
	}
	else if (Selector == ESwitchSelector::Typedef)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tSelectorAlias Value = SelectorAlias(%d);"),
			SelectorValueFor(Case)));
	}
	else if (Selector == ESwitchSelector::Boundary)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tint Value = %s;"),
			*TypedLiteral(ESwitchSelector::Boundary, SelectorValueFor(Case))));
	}
	else if (Selector == ESwitchSelector::Unsupported)
	{
		AppendLine(Source, TEXT("\tFUnsupported Value;"));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Value = %s(%d);"),
			SelectorTypeName(Selector),
			SelectorTypeName(Selector),
			SelectorValueFor(Case)));
	}
}

FString CaseLabel(const ESwitchSelector Selector, const int32 Value)
{
	return FString::Printf(TEXT("\tcase %s:"), *TypedLiteral(Selector, Value));
}

void AppendSelectedExit(FString& Source, const ESwitchSelector Selector, const ESwitchExit Exit, const FString& Indent)
{
	if (Exit == ESwitchExit::Break)
	{
		AppendLine(Source, Indent + TEXT("break;"));
	}
	else if (Exit == ESwitchExit::Fallthrough)
	{
		AppendLine(Source, Indent.LeftChop(1) + FString::Printf(TEXT("case %s:"), *TypedLiteral(Selector, 6)));
		AppendLine(Source, Indent + TEXT("Trace += 100;"));
		AppendLine(Source, Indent + TEXT("break;"));
	}
	else if (Exit == ESwitchExit::Return)
	{
		AppendLine(Source, Indent + TEXT("return Trace + 200;"));
	}
	else
	{
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + TEXT("\tint ExceptionDivisor = 0;"));
		AppendLine(Source, Indent + TEXT("\tTrace += 1 / ExceptionDivisor;"));
		AppendLine(Source, Indent + TEXT("}"));
		AppendLine(Source, Indent + TEXT("break;"));
	}
}

void AppendSelectedCase(FString& Source, const ESwitchSelector Selector, const ESwitchCase Case, const ESwitchExit Exit)
{
	const int32 Seed = TraceSeed(Case);
	if (Case == ESwitchCase::Duplicate)
	{
		AppendLine(Source, CaseLabel(Selector, 1));
		AppendLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
		AppendSelectedExit(Source, Selector, Exit, TEXT("\t\t"));
		AppendLine(Source, CaseLabel(Selector, 1));
		AppendLine(Source, TEXT("\t\tTrace += 1000;"));
		AppendLine(Source, TEXT("\t\tbreak;"));
		return;
	}
	if (Case == ESwitchCase::NonConstant)
	{
		AppendLine(Source, TEXT("\tcase DynamicCase:"));
		AppendLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
		AppendSelectedExit(Source, Selector, Exit, TEXT("\t\t"));
		return;
	}
	if (Case == ESwitchCase::Default || Case == ESwitchCase::NoMatch)
	{
		AppendLine(Source, CaseLabel(Selector, 0));
		AppendLine(Source, TEXT("\t\tTrace += 1000;"));
		AppendLine(Source, TEXT("\t\tbreak;"));
		AppendLine(Source, TEXT("\tdefault:"));
		AppendLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
		AppendSelectedExit(Source, Selector, Exit, TEXT("\t\t"));
		return;
	}
	if (Case == ESwitchCase::Grouped)
	{
		AppendLine(Source, CaseLabel(Selector, 1));
		AppendLine(Source, CaseLabel(Selector, 2));
		AppendLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
		AppendSelectedExit(Source, Selector, Exit, TEXT("\t\t"));
		return;
	}
	if (Case == ESwitchCase::Fallthrough)
	{
		AppendLine(Source, CaseLabel(Selector, 1));
		AppendLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
		AppendLine(Source, CaseLabel(Selector, 2));
		AppendLine(Source, TEXT("\t\tTrace += 1;"));
		AppendSelectedExit(Source, Selector, Exit, TEXT("\t\t"));
		return;
	}
	AppendLine(Source, CaseLabel(Selector, SelectorValueFor(Case)));
	AppendLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
	AppendSelectedExit(Source, Selector, Exit, TEXT("\t\t"));
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("enum ESelector"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tZero = 0,"));
	AppendLine(Source, TEXT("\tOne = 1,"));
	AppendLine(Source, TEXT("\tTwo = 2,"));
	AppendLine(Source, TEXT("\tThree = 3,"));
	AppendLine(Source, TEXT("\tFour = 4,"));
	AppendLine(Source, TEXT("\tFive = 5,"));
	AppendLine(Source, TEXT("\tSix = 6"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("typedef int SelectorAlias;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FUnsupported"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int BoundaryControl()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Trace = 0;"));
	AppendLine(Source, TEXT("\tint Lower = 2147483642;"));
	AppendLine(Source, TEXT("\tswitch (Lower)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\tcase 2147483642:"));
	AppendLine(Source, TEXT("\t\tTrace += 1;"));
	AppendLine(Source, TEXT("\t\tbreak;"));
	AppendLine(Source, TEXT("\tdefault:"));
	AppendLine(Source, TEXT("\t\tTrace += 100;"));
	AppendLine(Source, TEXT("\t\tbreak;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\tint Trigger = 2147483643;"));
	AppendLine(Source, TEXT("\tswitch (Trigger)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\tcase 2147483643:"));
	AppendLine(Source, TEXT("\t\tTrace += 2;"));
	AppendLine(Source, TEXT("\t\tbreak;"));
	AppendLine(Source, TEXT("\tdefault:"));
	AppendLine(Source, TEXT("\t\tTrace += 200;"));
	AppendLine(Source, TEXT("\t\tbreak;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\tint Dense = 2147483647;"));
	AppendLine(Source, TEXT("\tswitch (Dense)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\tcase 2147483645:"));
	AppendLine(Source, TEXT("\t\tTrace += 400;"));
	AppendLine(Source, TEXT("\t\tbreak;"));
	AppendLine(Source, TEXT("\tcase 2147483646:"));
	AppendLine(Source, TEXT("\t\tTrace += 500;"));
	AppendLine(Source, TEXT("\t\tbreak;"));
	AppendLine(Source, TEXT("\tcase 2147483647:"));
	AppendLine(Source, TEXT("\t\tTrace += 3;"));
	AppendLine(Source, TEXT("\t\tbreak;"));
	AppendLine(Source, TEXT("\tdefault:"));
	AppendLine(Source, TEXT("\t\tTrace += 800;"));
	AppendLine(Source, TEXT("\t\tbreak;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Trace;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FSwitchParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, Params.Selector == ESwitchSelector::Boundary
		? TEXT("\tint Trace = BoundaryControl();")
		: TEXT("\tint Trace = 0;"));
	AppendSelectorDeclaration(Source, Params.Selector, Params.Case);
	if (Params.Case == ESwitchCase::NonConstant)
	{
		AppendLine(Source, Params.Selector == ESwitchSelector::Enum
			? TEXT("\tESelector DynamicCase = ESelector::One;")
			: TEXT("\tint DynamicCase = 1;"));
	}
	AppendLine(Source, TEXT("\tswitch (Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendSelectedCase(Source, Params.Selector, Params.Case, Params.Exit);
	if (Params.Case != ESwitchCase::Default && Params.Case != ESwitchCase::NoMatch)
	{
		AppendLine(Source, TEXT("\tdefault:"));
		AppendLine(Source, TEXT("\t\tTrace += 5000;"));
		AppendLine(Source, TEXT("\t\tbreak;"));
	}
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Trace;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FSwitchParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

TArray<FSwitchCell> AllCells()
{
	TArray<FSwitchCell> Cells;
	Cells.Reserve(432);
	for (int32 SelectorIndex = 0; SelectorIndex < 12; ++SelectorIndex)
	{
		for (int32 CaseIndex = 0; CaseIndex < 9; ++CaseIndex)
		{
			for (int32 ExitIndex = 0; ExitIndex < 4; ++ExitIndex)
			{
				FSwitchCell Cell;
				Cell.Selector = static_cast<ESwitchSelector>(SelectorIndex);
				Cell.Case = static_cast<ESwitchCase>(CaseIndex);
				Cell.Exit = static_cast<ESwitchExit>(ExitIndex);
				Cells.Add(Cell);
			}
		}
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FSwitchCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FSwitchCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FGeneratedCaseInfo MakeCaseInfo(const FSwitchCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsRejectCell(Cell))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::Standalone;
	if (IsFaultCell(Cell))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Divide by zero");
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = ExpectedFromCell(Cell);
	}
	return Info;
}
}

FStringView FSwitchGenerator::GetProductId() const
{
	return TEXT("LANG-CF-SWITCH");
}

TArray<FGeneratedCaseInfo> FSwitchGenerator::ListCases() const
{
	using namespace SwitchGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FSwitchCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("Switch PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FSwitchGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace SwitchGeneratorDetail;
	FSwitchCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FSwitchGenerator::BuildDumpSource() const
{
	using namespace SwitchGeneratorDetail;
	const TArray<FSwitchCell> Cells = AllCells();
	int32 NormalCount = 0;
	int32 RejectCount = 0;
	int32 FaultCount = 0;
	for (const FSwitchCell& Cell : Cells)
	{
		if (IsRejectCell(Cell))
		{
			++RejectCount;
		}
		else if (IsFaultCell(Cell))
		{
			++FaultCount;
		}
		else
		{
			++NormalCount;
		}
	}

	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CF-SWITCH"));
	AppendLine(Source, FString::Printf(
		TEXT("// Cases: %d total, %d normal, %d reject, %d fault"),
		Cells.Num(),
		NormalCount,
		RejectCount,
		FaultCount));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);

	bool bNeedBlank = false;
	for (const FSwitchCell& Cell : Cells)
	{
		if (IsRejectCell(Cell))
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
		if (IsFaultCell(Cell))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}

	for (const FSwitchCell& Cell : Cells)
	{
		if (!IsRejectCell(Cell))
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
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FSwitchGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace SwitchGeneratorDetail;
	const TArray<FSwitchCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	int32 Emitted = 0;
	for (const FSwitchCell& Cell : Cells)
	{
		if (IsRejectCell(Cell))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FSwitchGenerator::BuildSwitchSource(const FSwitchParams& Params) const
{
	using namespace SwitchGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Selector, Params.Case, Params.Exit))
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

int32 FSwitchGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace SwitchGeneratorDetail;
	FSwitchCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsRejectCell(Cell) || IsFaultCell(Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FSwitchGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace SwitchGeneratorDetail;
	FSwitchCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsRejectCell(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FSwitchGenerator::ListRejectCaseIds() const
{
	using namespace SwitchGeneratorDetail;
	TArray<FString> Ids;
	for (const FSwitchCell& Cell : AllCells())
	{
		if (IsRejectCell(Cell))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
