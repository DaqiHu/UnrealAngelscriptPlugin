#include "Framework/Generate/AngelscriptTestExprChainGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ExprChainGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native MakeChain and FChainNode.");

enum class EChainOp : uint8
{
	Call,
	Member,
	Index,
	Cast,
};

struct FChainCell
{
	EExprChainContext Context = EExprChainContext::Initializer;
	EExprChainDepth Depth = EExprChainDepth::Two;
	EExprChainShape Shape = EExprChainShape::CallMember;
	EExprChainState State = EExprChainState::Valid;
};

const TCHAR* ContextToken(const EExprChainContext Context)
{
	switch (Context)
	{
	case EExprChainContext::Initializer:
		return TEXT("INITIALIZER");
	case EExprChainContext::Argument:
		return TEXT("ARGUMENT");
	case EExprChainContext::Return:
		return TEXT("RETURN");
	case EExprChainContext::Condition:
		return TEXT("CONDITION");
	case EExprChainContext::Assignment:
		return TEXT("ASSIGNMENT");
	default:
		return nullptr;
	}
}

const TCHAR* DepthToken(const EExprChainDepth Depth)
{
	switch (Depth)
	{
	case EExprChainDepth::Two:
		return TEXT("TWO");
	case EExprChainDepth::Three:
		return TEXT("THREE");
	case EExprChainDepth::Eight:
		return TEXT("EIGHT");
	case EExprChainDepth::DeepBoundary:
		return TEXT("DEEP_BOUNDARY");
	default:
		return nullptr;
	}
}

const TCHAR* ShapeToken(const EExprChainShape Shape)
{
	switch (Shape)
	{
	case EExprChainShape::CallMember:
		return TEXT("CALL_MEMBER");
	case EExprChainShape::MemberCall:
		return TEXT("MEMBER_CALL");
	case EExprChainShape::IndexMember:
		return TEXT("INDEX_MEMBER");
	case EExprChainShape::MemberIndex:
		return TEXT("MEMBER_INDEX");
	case EExprChainShape::CastMember:
		return TEXT("CAST_MEMBER");
	case EExprChainShape::CallIndexCast:
		return TEXT("CALL_INDEX_CAST");
	case EExprChainShape::MemberCallIndex:
		return TEXT("MEMBER_CALL_INDEX");
	case EExprChainShape::CastCallMemberIndex:
		return TEXT("CAST_CALL_MEMBER_INDEX");
	default:
		return nullptr;
	}
}

const TCHAR* StateToken(const EExprChainState State)
{
	switch (State)
	{
	case EExprChainState::Valid:
		return TEXT("VALID");
	case EExprChainState::NullReceiver:
		return TEXT("NULL_RECEIVER");
	case EExprChainState::InvalidIntermediate:
		return TEXT("INVALID_INTERMEDIATE");
	case EExprChainState::ExceptionIntermediate:
		return TEXT("EXCEPTION_INTERMEDIATE");
	case EExprChainState::MissingTerminal:
		return TEXT("MISSING_TERMINAL");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FExprChainParams& Params)
{
	return ContextToken(Params.Context) != nullptr
		&& DepthToken(Params.Depth) != nullptr
		&& ShapeToken(Params.Shape) != nullptr
		&& StateToken(Params.State) != nullptr;
}

bool IsReject(const EExprChainState State)
{
	return State == EExprChainState::InvalidIntermediate || State == EExprChainState::MissingTerminal;
}

bool IsFault(const EExprChainState State)
{
	return State == EExprChainState::NullReceiver || State == EExprChainState::ExceptionIntermediate;
}

int32 DepthValue(const EExprChainDepth Depth)
{
	switch (Depth)
	{
	case EExprChainDepth::Two:
		return 2;
	case EExprChainDepth::Three:
		return 3;
	case EExprChainDepth::Eight:
		return 8;
	case EExprChainDepth::DeepBoundary:
		return 32;
	default:
		return 0;
	}
}

int32 PatternCount(const EExprChainShape Shape)
{
	switch (Shape)
	{
	case EExprChainShape::CallIndexCast:
	case EExprChainShape::MemberCallIndex:
		return 3;
	case EExprChainShape::CastCallMemberIndex:
		return 4;
	default:
		return 2;
	}
}

EChainOp OperationAt(const EExprChainShape Shape, const int32 StageIndex)
{
	const int32 PatternIndex = StageIndex % PatternCount(Shape);
	switch (Shape)
	{
	case EExprChainShape::CallMember:
		return PatternIndex == 0 ? EChainOp::Call : EChainOp::Member;
	case EExprChainShape::MemberCall:
		return PatternIndex == 0 ? EChainOp::Member : EChainOp::Call;
	case EExprChainShape::IndexMember:
		return PatternIndex == 0 ? EChainOp::Index : EChainOp::Member;
	case EExprChainShape::MemberIndex:
		return PatternIndex == 0 ? EChainOp::Member : EChainOp::Index;
	case EExprChainShape::CastMember:
		return PatternIndex == 0 ? EChainOp::Cast : EChainOp::Member;
	case EExprChainShape::CallIndexCast:
		return PatternIndex == 0 ? EChainOp::Call : (PatternIndex == 1 ? EChainOp::Index : EChainOp::Cast);
	case EExprChainShape::MemberCallIndex:
		return PatternIndex == 0 ? EChainOp::Member : (PatternIndex == 1 ? EChainOp::Call : EChainOp::Index);
	case EExprChainShape::CastCallMemberIndex:
		if (PatternIndex == 0)
		{
			return EChainOp::Cast;
		}
		if (PatternIndex == 1)
		{
			return EChainOp::Call;
		}
		return PatternIndex == 2 ? EChainOp::Member : EChainOp::Index;
	default:
		return EChainOp::Call;
	}
}

int32 OperationValue(const EChainOp Operation)
{
	switch (Operation)
	{
	case EChainOp::Call:
		return 1;
	case EChainOp::Member:
		return 3;
	case EChainOp::Index:
		return 5;
	case EChainOp::Cast:
		return 7;
	default:
		return 0;
	}
}

int32 FailureStage(const EExprChainDepth Depth)
{
	return (DepthValue(Depth) + 1) / 2;
}

int32 ExpectedValue(const EExprChainShape Shape, const EExprChainDepth Depth)
{
	int32 Value = 100;
	const int32 Count = DepthValue(Depth);
	for (int32 Stage = 0; Stage < Count; ++Stage)
	{
		Value += OperationValue(OperationAt(Shape, Stage));
	}
	return Value;
}

int32 ExpectedFromCell(const FChainCell& Cell)
{
	if (IsReject(Cell.State) || IsFault(Cell.State))
	{
		return 0;
	}
	return Cell.Context == EExprChainContext::Condition ? 1 : ExpectedValue(Cell.Shape, Cell.Depth);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FChainCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-EXPR-CHAIN-%s-%s-%s-%s"),
		ContextToken(Cell.Context),
		DepthToken(Cell.Depth),
		ShapeToken(Cell.Shape),
		StateToken(Cell.State));
}

FString MakeEntryName(const FChainCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FExprChainParams ParamsFromCell(const FChainCell& Cell)
{
	FExprChainParams Params;
	Params.Context = Cell.Context;
	Params.Depth = Cell.Depth;
	Params.Shape = Cell.Shape;
	Params.State = Cell.State;
	return Params;
}

FString AppendChainOperation(const FString& Receiver, const EChainOp Operation, const int32 Stage)
{
	switch (Operation)
	{
	case EChainOp::Call:
		return Receiver + TEXT(".StepCall()");
	case EChainOp::Member:
		return Receiver + TEXT(".GetMember()");
	case EChainOp::Index:
		return Receiver + FString::Printf(TEXT("[%d]"), Stage);
	case EChainOp::Cast:
		return Receiver + TEXT(".opCast()");
	default:
		return Receiver;
	}
}

FString BuildChainExpression(const FExprChainParams& Params)
{
	FString Expression = TEXT("MakeChain()");
	const int32 Depth = DepthValue(Params.Depth);
	const int32 InjectedStage = FailureStage(Params.Depth);
	for (int32 Stage = 1; Stage <= Depth; ++Stage)
	{
		if (Params.State == EExprChainState::InvalidIntermediate && Stage == InjectedStage)
		{
			Expression += TEXT(".BreakToInt()");
		}
		else
		{
			Expression = AppendChainOperation(Expression, OperationAt(Params.Shape, Stage - 1), Stage);
		}
	}
	if (Params.State == EExprChainState::MissingTerminal)
	{
		Expression += TEXT(".MissingTerminal");
	}
	else
	{
		Expression += TEXT(".Value");
	}
	return Expression;
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int ObserveChain(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int RecoverExpressionChain()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 149;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FExprChainParams& Params, const FString& FunctionName)
{
	const FString Expression = BuildChainExpression(Params);
	const int32 Expected = ExpectedValue(Params.Shape, Params.Depth);
	if (Params.Context == EExprChainContext::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("int ProduceChainValue_%s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Context == EExprChainContext::Initializer)
	{
		AppendLine(Source, FString::Printf(TEXT("\tint Result = %s;"), *Expression));
		AppendLine(Source, TEXT("\treturn Result;"));
	}
	else if (Params.Context == EExprChainContext::Argument)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveChain(%s);"), *Expression));
	}
	else if (Params.Context == EExprChainContext::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ProduceChainValue_%s();"), *FunctionName));
	}
	else if (Params.Context == EExprChainContext::Condition)
	{
		AppendLine(Source, FString::Printf(TEXT("\tif (%s == %d)"), *Expression, Expected));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else
	{
		AppendLine(Source, TEXT("\tint Result = 0;"));
		AppendLine(Source, FString::Printf(TEXT("\tResult = %s;"), *Expression));
		AppendLine(Source, TEXT("\treturn Result;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FExprChainParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

TArray<FChainCell> AllCells()
{
	TArray<FChainCell> Cells;
	Cells.Reserve(800);
	for (int32 ContextIndex = 0; ContextIndex < 5; ++ContextIndex)
	{
		for (int32 DepthIndex = 0; DepthIndex < 4; ++DepthIndex)
		{
			for (int32 ShapeIndex = 0; ShapeIndex < 8; ++ShapeIndex)
			{
				for (int32 StateIndex = 0; StateIndex < 5; ++StateIndex)
				{
					FChainCell Cell;
					Cell.Context = static_cast<EExprChainContext>(ContextIndex);
					Cell.Depth = static_cast<EExprChainDepth>(DepthIndex);
					Cell.Shape = static_cast<EExprChainShape>(ShapeIndex);
					Cell.State = static_cast<EExprChainState>(StateIndex);
					Cells.Add(Cell);
				}
			}
		}
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FChainCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FChainCell> Cells = AllCells();
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

FGeneratedCaseInfo MakeCaseInfo(const FChainCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.State))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	if (IsFault(Cell.State))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = Cell.State == EExprChainState::NullReceiver
			? TEXT("Null pointer access")
			: TEXT("Expression chain intermediate exception");
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = ExpectedFromCell(Cell);
	}
	return Info;
}
}

FStringView FExprChainGenerator::GetProductId() const
{
	return TEXT("LANG-EXPR-CHAIN");
}

TArray<FGeneratedCaseInfo> FExprChainGenerator::ListCases() const
{
	using namespace ExprChainGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FChainCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("ExprChain PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FExprChainGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ExprChainGeneratorDetail;
	FChainCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FExprChainGenerator::BuildDumpSource() const
{
	using namespace ExprChainGeneratorDetail;
	const TArray<FChainCell> Cells = AllCells();
	int32 NormalCount = 0;
	int32 RejectCount = 0;
	int32 FaultCount = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].State))
		{
			++RejectCount;
		}
		else if (IsFault(Cells[Index].State))
		{
			++FaultCount;
		}
		else
		{
			++NormalCount;
		}
	}

	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-EXPR-CHAIN"));
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
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].State))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (bNeedBlank)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cells[Index].State))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedException: %s"), *Info.ExpectedException));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		bNeedBlank = true;
	}

	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsReject(Cells[Index].State))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		AppendHelpers(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FExprChainGenerator::BuildExpressionChainSource(const FExprChainParams& Params) const
{
	using namespace ExprChainGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.State))
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

FString FExprChainGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ExprChainGeneratorDetail;
	const TArray<FChainCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].State))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FExprChainGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ExprChainGeneratorDetail;
	FChainCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.State) || IsFault(Cell.State))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FExprChainGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ExprChainGeneratorDetail;
	FChainCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.State))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FExprChainGenerator::ListRejectCaseIds() const
{
	using namespace ExprChainGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FChainCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].State))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
