#include "Framework/Generate/AngelscriptTestExprSourceBoundaryGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ExprSourceBoundaryGeneratorDetail
{
struct FSourceBoundaryCell
{
	EExprSourceBoundaryBuild Build = EExprSourceBoundaryBuild::First;
	EExprSourceBoundaryContext Context = EExprSourceBoundaryContext::Initializer;
	EExprSourceBoundaryScenario Scenario = EExprSourceBoundaryScenario::ParenthesesOne;
};

const TCHAR* BuildToken(const EExprSourceBoundaryBuild Build)
{
	switch (Build)
	{
	case EExprSourceBoundaryBuild::First:
		return TEXT("FIRST");
	case EExprSourceBoundaryBuild::SameRebuild:
		return TEXT("SAME_REBUILD");
	case EExprSourceBoundaryBuild::ChangedRebuild:
		return TEXT("CHANGED_REBUILD");
	default:
		return nullptr;
	}
}

const TCHAR* ContextToken(const EExprSourceBoundaryContext Context)
{
	switch (Context)
	{
	case EExprSourceBoundaryContext::Initializer:
		return TEXT("INITIALIZER");
	case EExprSourceBoundaryContext::Argument:
		return TEXT("ARGUMENT");
	case EExprSourceBoundaryContext::Return:
		return TEXT("RETURN");
	case EExprSourceBoundaryContext::Condition:
		return TEXT("CONDITION");
	case EExprSourceBoundaryContext::Index:
		return TEXT("INDEX");
	default:
		return nullptr;
	}
}

const TCHAR* ScenarioToken(const EExprSourceBoundaryScenario Scenario)
{
	switch (Scenario)
	{
	case EExprSourceBoundaryScenario::ParenthesesOne:
		return TEXT("PARENTHESES_ONE");
	case EExprSourceBoundaryScenario::ParenthesesEight:
		return TEXT("PARENTHESES_EIGHT");
	case EExprSourceBoundaryScenario::ParenthesesSixtyFour:
		return TEXT("PARENTHESES_SIXTY_FOUR");
	case EExprSourceBoundaryScenario::ChainOne:
		return TEXT("CHAIN_ONE");
	case EExprSourceBoundaryScenario::ChainEight:
		return TEXT("CHAIN_EIGHT");
	case EExprSourceBoundaryScenario::ChainThirtyTwo:
		return TEXT("CHAIN_THIRTY_TWO");
	case EExprSourceBoundaryScenario::ArgumentsZero:
		return TEXT("ARGUMENTS_ZERO");
	case EExprSourceBoundaryScenario::ArgumentsOne:
		return TEXT("ARGUMENTS_ONE");
	case EExprSourceBoundaryScenario::ArgumentsMany:
		return TEXT("ARGUMENTS_MANY");
	case EExprSourceBoundaryScenario::NumericMinimum:
		return TEXT("NUMERIC_MINIMUM");
	case EExprSourceBoundaryScenario::NumericMaximum:
		return TEXT("NUMERIC_MAXIMUM");
	case EExprSourceBoundaryScenario::Whitespace:
		return TEXT("WHITESPACE");
	case EExprSourceBoundaryScenario::Comments:
		return TEXT("COMMENTS");
	case EExprSourceBoundaryScenario::Multiline:
		return TEXT("MULTILINE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FExprSourceBoundaryParams& Params)
{
	return BuildToken(Params.Build) != nullptr
		&& ContextToken(Params.Context) != nullptr
		&& ScenarioToken(Params.Scenario) != nullptr;
}

int32 VersionSeed(const EExprSourceBoundaryBuild Build)
{
	return Build == EExprSourceBoundaryBuild::ChangedRebuild ? 58 : 41;
}

int32 ParenthesesDepth(const EExprSourceBoundaryScenario Scenario)
{
	switch (Scenario)
	{
	case EExprSourceBoundaryScenario::ParenthesesOne:
		return 1;
	case EExprSourceBoundaryScenario::ParenthesesEight:
		return 8;
	case EExprSourceBoundaryScenario::ParenthesesSixtyFour:
		return 64;
	default:
		return 0;
	}
}

int32 ChainDepth(const EExprSourceBoundaryScenario Scenario)
{
	switch (Scenario)
	{
	case EExprSourceBoundaryScenario::ChainOne:
		return 1;
	case EExprSourceBoundaryScenario::ChainEight:
		return 8;
	case EExprSourceBoundaryScenario::ChainThirtyTwo:
		return 32;
	default:
		return 0;
	}
}

int32 ExpectedFromCell(const FSourceBoundaryCell& Cell)
{
	const int32 Seed = VersionSeed(Cell.Build);
	switch (Cell.Scenario)
	{
	case EExprSourceBoundaryScenario::ChainOne:
	case EExprSourceBoundaryScenario::ChainEight:
	case EExprSourceBoundaryScenario::ChainThirtyTwo:
		return Seed + ChainDepth(Cell.Scenario);
	case EExprSourceBoundaryScenario::ArgumentsMany:
		return Seed + 120;
	case EExprSourceBoundaryScenario::NumericMinimum:
		return Cell.Build == EExprSourceBoundaryBuild::ChangedRebuild ? MIN_int32 + 17 : MIN_int32;
	case EExprSourceBoundaryScenario::NumericMaximum:
		return Cell.Build == EExprSourceBoundaryBuild::ChangedRebuild ? MAX_int32 - 17 : MAX_int32;
	case EExprSourceBoundaryScenario::Whitespace:
	case EExprSourceBoundaryScenario::Comments:
	case EExprSourceBoundaryScenario::Multiline:
		return Seed + 3;
	default:
		return Seed;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FSourceBoundaryCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-EXPR-SOURCE-BOUNDARY-%s-%s-%s"),
		BuildToken(Cell.Build),
		ContextToken(Cell.Context),
		ScenarioToken(Cell.Scenario));
}

FString MakeEntryName(const FSourceBoundaryCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FExprSourceBoundaryParams ParamsFromCell(const FSourceBoundaryCell& Cell)
{
	FExprSourceBoundaryParams Params;
	Params.Build = Cell.Build;
	Params.Context = Cell.Context;
	Params.Scenario = Cell.Scenario;
	return Params;
}

TArray<FSourceBoundaryCell> AllCells()
{
	TArray<FSourceBoundaryCell> Cells;
	Cells.SetNum(210);
	for (int32 Index = 0; Index < 210; ++Index)
	{
		FSourceBoundaryCell& Cell = Cells[Index];
		Cell.Build = static_cast<EExprSourceBoundaryBuild>(Index / 70);
		Cell.Context = static_cast<EExprSourceBoundaryContext>((Index / 14) % 5);
		Cell.Scenario = static_cast<EExprSourceBoundaryScenario>(Index % 14);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FSourceBoundaryCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FSourceBoundaryCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (MakeCaseId(Cells[Index]) == Needle)
		{
			OutCell = Cells[Index];
			return true;
		}
	}
	return false;
}

FString ParenthesizedExpression(const int32 Depth, const int32 Seed)
{
	FString Expression = FString::Printf(TEXT("int64(%d)"), Seed);
	for (int32 Index = 0; Index < Depth; ++Index)
	{
		Expression = TEXT("(") + Expression + TEXT(")");
	}
	return Expression;
}

FString ChainExpression(const int32 Depth, const int32 Seed)
{
	FString Expression = FString::Printf(TEXT("MakeBoundaryChain(%d)"), Seed);
	for (int32 Index = 0; Index < Depth; ++Index)
	{
		Expression += TEXT(".Step()");
	}
	Expression += TEXT(".Value");
	return Expression;
}

FString ManyArgumentExpression(const int32 Seed)
{
	FString Expression = FString::Printf(TEXT("BoundaryArgumentsMany(%d"), Seed);
	for (int32 Value = 1; Value < 16; ++Value)
	{
		Expression += FString::Printf(TEXT(", %d"), Value);
	}
	Expression += TEXT(")");
	return Expression;
}

FString BoundaryExpression(const FExprSourceBoundaryParams& Params)
{
	const int32 Seed = VersionSeed(Params.Build);
	const int32 Parentheses = ParenthesesDepth(Params.Scenario);
	if (Parentheses > 0)
	{
		return ParenthesizedExpression(Parentheses, Seed);
	}
	const int32 Chain = ChainDepth(Params.Scenario);
	if (Chain > 0)
	{
		return ChainExpression(Chain, Seed);
	}
	switch (Params.Scenario)
	{
	case EExprSourceBoundaryScenario::ArgumentsZero:
		return Params.Build == EExprSourceBoundaryBuild::ChangedRebuild
			? TEXT("BoundaryArgumentsZeroChanged()")
			: TEXT("BoundaryArgumentsZero()");
	case EExprSourceBoundaryScenario::ArgumentsOne:
		return FString::Printf(TEXT("BoundaryArgumentsOne(%d)"), Seed);
	case EExprSourceBoundaryScenario::ArgumentsMany:
		return ManyArgumentExpression(Seed);
	case EExprSourceBoundaryScenario::NumericMinimum:
		return Params.Build == EExprSourceBoundaryBuild::ChangedRebuild
			? TEXT("int64(-2147483647 - 1) + 17")
			: TEXT("int64(-2147483647 - 1)");
	case EExprSourceBoundaryScenario::NumericMaximum:
		return Params.Build == EExprSourceBoundaryBuild::ChangedRebuild
			? TEXT("int64(2147483647) - 17")
			: TEXT("int64(2147483647)");
	case EExprSourceBoundaryScenario::Whitespace:
		return FString::Printf(TEXT("int64(  %d\t+\t3  )"), Seed);
	case EExprSourceBoundaryScenario::Comments:
		return FString::Printf(TEXT("int64(%d /* seed */ + /* delta */ 3)"), Seed);
	case EExprSourceBoundaryScenario::Multiline:
		return FString::Printf(TEXT("int64(\n\t\t%d\n\t\t+\n\t\t3\n\t)"), Seed);
	default:
		return FString::Printf(TEXT("int64(%d)"), Seed);
	}
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("struct FBoundaryChain"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint64 Value = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFBoundaryChain()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFBoundaryChain(int64 InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFBoundaryChain Step() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FBoundaryChain(Value + 1);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FBoundaryChain MakeBoundaryChain(int64 Seed)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FBoundaryChain(Seed);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int64 BoundaryArgumentsZero()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 41;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int64 BoundaryArgumentsZeroChanged()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 58;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int64 BoundaryArgumentsOne(int64 A0)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn A0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int64 BoundaryArgumentsMany(int64 A0, int64 A1, int64 A2, int64 A3, int64 A4, int64 A5, int64 A6, int64 A7, int64 A8, int64 A9, int64 A10, int64 A11, int64 A12, int64 A13, int64 A14, int64 A15)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn A0 + A1 + A2 + A3 + A4 + A5 + A6 + A7"));
	AppendLine(Source, TEXT("\t\t+ A8 + A9 + A10 + A11 + A12 + A13 + A14 + A15;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int64 ObserveBoundary(int64 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FBoundaryIndex"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint64 opIndex(int64 Index) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Index;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FExprSourceBoundaryParams& Params, const FString& FunctionName)
{
	const FString Expression = BoundaryExpression(Params);
	const int32 Expected = ExpectedFromCell(FSourceBoundaryCell{Params.Build, Params.Context, Params.Scenario});
	if (Params.Context == EExprSourceBoundaryContext::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("int64 ProduceBoundary_%s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	switch (Params.Context)
	{
	case EExprSourceBoundaryContext::Initializer:
		AppendLine(Source, FString::Printf(TEXT("\tint64 Result = %s;"), *Expression));
		AppendLine(Source, TEXT("\treturn int(Result);"));
		break;
	case EExprSourceBoundaryContext::Argument:
		AppendLine(Source, FString::Printf(TEXT("\treturn int(ObserveBoundary(%s));"), *Expression));
		break;
	case EExprSourceBoundaryContext::Return:
		AppendLine(Source, FString::Printf(TEXT("\treturn int(ProduceBoundary_%s());"), *FunctionName));
		break;
	case EExprSourceBoundaryContext::Condition:
		AppendLine(Source, FString::Printf(TEXT("\tif (%s == %d)"), *Expression, Expected));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), Expected));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn -1;"));
		break;
	case EExprSourceBoundaryContext::Index:
		AppendLine(Source, TEXT("\tFBoundaryIndex Probe;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn int(Probe[%s]);"), *Expression));
		break;
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FExprSourceBoundaryParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FSourceBoundaryCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.bLimitedObservation = true;
	Info.ExecutionNotes = TEXT("int32 narrowing of the documented int64 boundary result.");
	if (Cell.Build == EExprSourceBoundaryBuild::First)
	{
		Info.Execution = EGeneratedCaseExecution::Standalone;
	}
	else
	{
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		Info.ExecutionNotes += Cell.Build == EExprSourceBoundaryBuild::ChangedRebuild
			? TEXT(" Changed-rebuild source variant; no rebuild protocol is executed.")
			: TEXT(" Same-rebuild source variant; no rebuild protocol is executed.");
	}
	return Info;
}

void AppendCaseComments(FString& Source, const FGeneratedCaseInfo& Info)
{
	AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
	AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
	AppendLine(Source, TEXT("// Outcome: ReturnValue"));
	AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
	AppendLine(Source, Info.Execution == EGeneratedCaseExecution::Standalone
		? TEXT("// Execution: Standalone")
		: TEXT("// Execution: SourceOnly"));
	AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
	AppendLine(Source, TEXT("// LimitedObservation: true"));
	AppendLine(Source);
}
}

FStringView FExprSourceBoundaryGenerator::GetProductId() const
{
	return TEXT("LANG-EXPR-SOURCE-BOUNDARY");
}

TArray<FGeneratedCaseInfo> FExprSourceBoundaryGenerator::ListCases() const
{
	using namespace ExprSourceBoundaryGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FSourceBoundaryCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("ExprSourceBoundary PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FExprSourceBoundaryGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ExprSourceBoundaryGeneratorDetail;
	FSourceBoundaryCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FExprSourceBoundaryGenerator::BuildDumpSource() const
{
	using namespace ExprSourceBoundaryGeneratorDetail;
	const TArray<FSourceBoundaryCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-EXPR-SOURCE-BOUNDARY"));
	AppendLine(Source, TEXT("// Cases: 210 total, 210 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendCaseComments(Source, Info);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FExprSourceBoundaryGenerator::BuildExpressionBoundarySource(const FExprSourceBoundaryParams& Params) const
{
	using namespace ExprSourceBoundaryGeneratorDetail;
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

FString FExprSourceBoundaryGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ExprSourceBoundaryGeneratorDetail;
	const TArray<FSourceBoundaryCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	OutCaseCount = Cells.Num();
	return Source;
}

int32 FExprSourceBoundaryGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ExprSourceBoundaryGeneratorDetail;
	FSourceBoundaryCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
