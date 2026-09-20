#include "Framework/Generate/AngelscriptTestFnArityTypeStressGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnArityTypeStressGeneratorDetail
{
struct FArityTypeStressCell
{
	EFnArityTypeStressArity Arity = EFnArityTypeStressArity::Zero;
	EFnArityTypeStressPattern Pattern = EFnArityTypeStressPattern::HomogeneousInt;
	EFnArityTypeStressTarget Target = EFnArityTypeStressTarget::Global;
};

const TCHAR* ArityToken(const EFnArityTypeStressArity Arity)
{
	switch (Arity)
	{
	case EFnArityTypeStressArity::Zero:
		return TEXT("ZERO");
	case EFnArityTypeStressArity::One:
		return TEXT("ONE");
	case EFnArityTypeStressArity::Two:
		return TEXT("TWO");
	case EFnArityTypeStressArity::Four:
		return TEXT("FOUR");
	case EFnArityTypeStressArity::Eight:
		return TEXT("EIGHT");
	case EFnArityTypeStressArity::Sixteen:
		return TEXT("SIXTEEN");
	case EFnArityTypeStressArity::ThirtyTwo:
		return TEXT("THIRTY_TWO");
	case EFnArityTypeStressArity::SixtyFour:
		return TEXT("SIXTY_FOUR");
	default:
		return nullptr;
	}
}

const TCHAR* PatternToken(const EFnArityTypeStressPattern Pattern)
{
	switch (Pattern)
	{
	case EFnArityTypeStressPattern::HomogeneousInt:
		return TEXT("HOMOGENEOUS_INT");
	case EFnArityTypeStressPattern::HomogeneousBool:
		return TEXT("HOMOGENEOUS_BOOL");
	case EFnArityTypeStressPattern::AlternatingIntBool:
		return TEXT("ALTERNATING_INT_BOOL");
	case EFnArityTypeStressPattern::AlternatingBoolInt:
		return TEXT("ALTERNATING_BOOL_INT");
	default:
		return nullptr;
	}
}

const TCHAR* TargetToken(const EFnArityTypeStressTarget Target)
{
	switch (Target)
	{
	case EFnArityTypeStressTarget::Global:
		return TEXT("GLOBAL");
	case EFnArityTypeStressTarget::NamespaceGlobal:
		return TEXT("NAMESPACE_GLOBAL");
	case EFnArityTypeStressTarget::InstanceMethod:
		return TEXT("INSTANCE_METHOD");
	default:
		return nullptr;
	}
}

int32 ArityCount(const EFnArityTypeStressArity Arity)
{
	switch (Arity)
	{
	case EFnArityTypeStressArity::Zero:
		return 0;
	case EFnArityTypeStressArity::One:
		return 1;
	case EFnArityTypeStressArity::Two:
		return 2;
	case EFnArityTypeStressArity::Four:
		return 4;
	case EFnArityTypeStressArity::Eight:
		return 8;
	case EFnArityTypeStressArity::Sixteen:
		return 16;
	case EFnArityTypeStressArity::ThirtyTwo:
		return 32;
	case EFnArityTypeStressArity::SixtyFour:
		return 64;
	default:
		return -1;
	}
}

bool IsValidParams(const FFnArityTypeStressParams& Params)
{
	return ArityToken(Params.Arity) != nullptr
		&& PatternToken(Params.Pattern) != nullptr
		&& TargetToken(Params.Target) != nullptr;
}

bool IsBoolParameter(const EFnArityTypeStressPattern Pattern, const int32 Index)
{
	if (Pattern == EFnArityTypeStressPattern::HomogeneousBool)
	{
		return true;
	}
	if (Pattern == EFnArityTypeStressPattern::AlternatingIntBool)
	{
		return (Index % 2) == 1;
	}
	if (Pattern == EFnArityTypeStressPattern::AlternatingBoolInt)
	{
		return (Index % 2) == 0;
	}
	return false;
}

int32 ArgumentValueAsInt(const EFnArityTypeStressPattern Pattern, const int32 Index)
{
	if (IsBoolParameter(Pattern, Index))
	{
		return (Index % 2) == 0 ? 1 : 0;
	}
	return Index + 1;
}

int32 ExpectedResult(const EFnArityTypeStressArity Arity, const EFnArityTypeStressPattern Pattern)
{
	const int32 Count = ArityCount(Arity);
	if (Count == 0)
	{
		return 42;
	}
	int32 Result = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Result += ArgumentValueAsInt(Pattern, Index);
	}
	return Result;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FArityTypeStressCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-ARITY-TYPE-STRESS-%s-%s-%s"),
		ArityToken(Cell.Arity),
		PatternToken(Cell.Pattern),
		TargetToken(Cell.Target));
}

FString MakeEntryName(const FArityTypeStressCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FFnArityTypeStressParams ParamsFromCell(const FArityTypeStressCell& Cell)
{
	FFnArityTypeStressParams Params;
	Params.Arity = Cell.Arity;
	Params.Pattern = Cell.Pattern;
	Params.Target = Cell.Target;
	return Params;
}

TArray<FArityTypeStressCell> AllCells()
{
	TArray<FArityTypeStressCell> Cells;
	Cells.SetNum(96);
	for (int32 Index = 0; Index < 96; ++Index)
	{
		FArityTypeStressCell& Cell = Cells[Index];
		Cell.Arity = static_cast<EFnArityTypeStressArity>(Index / 12);
		Cell.Pattern = static_cast<EFnArityTypeStressPattern>((Index / 3) % 4);
		Cell.Target = static_cast<EFnArityTypeStressTarget>(Index % 3);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FArityTypeStressCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FArityTypeStressCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString NameSuffix(const FString& FunctionName)
{
	return FunctionName == TEXT("Entry") ? FString() : FunctionName;
}

FString MakeParameters(const int32 Count, const EFnArityTypeStressPattern Pattern)
{
	if (Count == 0)
	{
		return FString();
	}
	TArray<FString> Parameters;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Parameters.Add(FString::Printf(
			TEXT("%s P%d"),
			IsBoolParameter(Pattern, Index) ? TEXT("bool") : TEXT("int"),
			Index));
	}
	return FString::Join(Parameters, TEXT(", "));
}

FString MakeArguments(const int32 Count, const EFnArityTypeStressPattern Pattern)
{
	if (Count == 0)
	{
		return FString();
	}
	TArray<FString> Arguments;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (IsBoolParameter(Pattern, Index))
		{
			Arguments.Add((Index % 2) == 0 ? TEXT("true") : TEXT("false"));
		}
		else
		{
			Arguments.Add(FString::FromInt(Index + 1));
		}
	}
	return FString::Join(Arguments, TEXT(", "));
}

FString MakeResultExpression(const int32 Count, const EFnArityTypeStressPattern Pattern)
{
	if (Count == 0)
	{
		return TEXT("42");
	}
	TArray<FString> Terms;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Terms.Add(IsBoolParameter(Pattern, Index)
			? FString::Printf(TEXT("(P%d ? 1 : 0)"), Index)
			: FString::Printf(TEXT("P%d"), Index));
	}
	return FString::Join(Terms, TEXT(" + "));
}

void AppendEntry(FString& Source, const FFnArityTypeStressParams& Params, const FString& FunctionName)
{
	const int32 Count = ArityCount(Params.Arity);
	const FString Parameters = MakeParameters(Count, Params.Pattern);
	const FString Arguments = MakeArguments(Count, Params.Pattern);
	const FString Result = MakeResultExpression(Count, Params.Pattern);
	const FString Suffix = NameSuffix(FunctionName);
	const FString ProbeName = TEXT("Probe") + Suffix;
	if (Params.Target == EFnArityTypeStressTarget::Global)
	{
		AppendLine(Source, FString::Printf(TEXT("int %s(%s)"), *ProbeName, *Parameters));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *Result));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(%s);"), *ProbeName, *Arguments));
		AppendLine(Source, TEXT("}"));
	}
	else if (Params.Target == EFnArityTypeStressTarget::NamespaceGlobal)
	{
		const FString Namespace = TEXT("N") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("namespace %s"), *Namespace));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *ProbeName, *Parameters));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %s;"), *Result));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s::%s(%s);"), *Namespace, *ProbeName, *Arguments));
		AppendLine(Source, TEXT("}"));
	}
	else
	{
		const FString TypeName = TEXT("FOwner") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("struct %s"), *TypeName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *ProbeName, *Parameters));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %s;"), *Result));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Owner;"), *TypeName));
		AppendLine(Source, FString::Printf(TEXT("\treturn Owner.%s(%s);"), *ProbeName, *Arguments));
		AppendLine(Source, TEXT("}"));
	}
}

FString BuildOwnedSource(const FFnArityTypeStressParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FArityTypeStressCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedResult(Cell.Arity, Cell.Pattern);
	return Info;
}
}

FStringView FFnArityTypeStressGenerator::GetProductId() const
{
	return TEXT("LANG-FN-ARITY-TYPE-STRESS");
}

TArray<FGeneratedCaseInfo> FFnArityTypeStressGenerator::ListCases() const
{
	using namespace FnArityTypeStressGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FArityTypeStressCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("FnArityTypeStress PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnArityTypeStressGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnArityTypeStressGeneratorDetail;
	FArityTypeStressCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnArityTypeStressGenerator::BuildDumpSource() const
{
	using namespace FnArityTypeStressGeneratorDetail;
	const TArray<FArityTypeStressCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-ARITY-TYPE-STRESS"));
	AppendLine(Source, TEXT("// Cases: 96 total, 96 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FArityTypeStressCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnArityTypeStressGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnArityTypeStressGeneratorDetail;
	const TArray<FArityTypeStressCell> Cells = AllCells();
	FString Source;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (Index > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	OutCaseCount = Cells.Num();
	return Source;
}

FString FFnArityTypeStressGenerator::BuildSource(const FFnArityTypeStressParams& Params) const
{
	using namespace FnArityTypeStressGeneratorDetail;
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

int32 FFnArityTypeStressGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnArityTypeStressGeneratorDetail;
	FArityTypeStressCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedResult(Cell.Arity, Cell.Pattern);
}
}
