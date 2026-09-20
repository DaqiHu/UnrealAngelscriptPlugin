#include "Framework/Generate/AngelscriptTestExprFailureGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ExprFailureGeneratorDetail
{
constexpr const TCHAR* HostNotes =
	TEXT("Needs native ThrowFailureStage, RaiseFailureIndex, and reference fixtures.");

struct FFailureCell
{
	EExprFailureContext Context = EExprFailureContext::Initializer;
	EExprFailureKind Failure = EExprFailureKind::InvalidLValue;
	EExprFailureRecovery Recovery = EExprFailureRecovery::FreshModule;
};

const TCHAR* ContextToken(const EExprFailureContext Context)
{
	switch (Context)
	{
	case EExprFailureContext::Initializer:
		return TEXT("INITIALIZER");
	case EExprFailureContext::Assignment:
		return TEXT("ASSIGNMENT");
	case EExprFailureContext::Argument:
		return TEXT("ARGUMENT");
	case EExprFailureContext::Return:
		return TEXT("RETURN");
	case EExprFailureContext::Condition:
		return TEXT("CONDITION");
	case EExprFailureContext::LoopClause:
		return TEXT("LOOP_CLAUSE");
	case EExprFailureContext::SwitchSelector:
		return TEXT("SWITCH_SELECTOR");
	case EExprFailureContext::Index:
		return TEXT("INDEX");
	default:
		return nullptr;
	}
}

const TCHAR* FailureToken(const EExprFailureKind Failure)
{
	switch (Failure)
	{
	case EExprFailureKind::InvalidLValue:
		return TEXT("INVALID_LVALUE");
	case EExprFailureKind::MissingDelimiter:
		return TEXT("MISSING_DELIMITER");
	case EExprFailureKind::MalformedTernary:
		return TEXT("MALFORMED_TERNARY");
	case EExprFailureKind::MissingSymbol:
		return TEXT("MISSING_SYMBOL");
	case EExprFailureKind::AmbiguousSymbol:
		return TEXT("AMBIGUOUS_SYMBOL");
	case EExprFailureKind::InaccessibleMember:
		return TEXT("INACCESSIBLE_MEMBER");
	case EExprFailureKind::MissingMember:
		return TEXT("MISSING_MEMBER");
	case EExprFailureKind::DivideZero:
		return TEXT("DIVIDE_ZERO");
	case EExprFailureKind::IndexOutOfRange:
		return TEXT("INDEX_OUT_OF_RANGE");
	case EExprFailureKind::NullAccess:
		return TEXT("NULL_ACCESS");
	case EExprFailureKind::ExceptionLeft:
		return TEXT("EXCEPTION_LEFT");
	case EExprFailureKind::ExceptionRight:
		return TEXT("EXCEPTION_RIGHT");
	default:
		return nullptr;
	}
}

const TCHAR* RecoveryToken(const EExprFailureRecovery Recovery)
{
	switch (Recovery)
	{
	case EExprFailureRecovery::FreshModule:
		return TEXT("FRESH_MODULE");
	case EExprFailureRecovery::RebuildOrContextReuse:
		return TEXT("REBUILD_OR_CONTEXT_REUSE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FExprFailureParams& Params)
{
	return ContextToken(Params.Context) != nullptr
		&& FailureToken(Params.Failure) != nullptr
		&& RecoveryToken(Params.Recovery) != nullptr;
}

bool IsReject(const EExprFailureKind Failure)
{
	return Failure == EExprFailureKind::InvalidLValue
		|| Failure == EExprFailureKind::MissingDelimiter
		|| Failure == EExprFailureKind::MalformedTernary
		|| Failure == EExprFailureKind::MissingSymbol
		|| Failure == EExprFailureKind::AmbiguousSymbol
		|| Failure == EExprFailureKind::InaccessibleMember
		|| Failure == EExprFailureKind::MissingMember;
}

FString ExpectedExceptionText(const EExprFailureKind Failure)
{
	switch (Failure)
	{
	case EExprFailureKind::DivideZero:
		return TEXT("Divide by zero");
	case EExprFailureKind::IndexOutOfRange:
		return TEXT("Expression index out of range");
	case EExprFailureKind::NullAccess:
		return TEXT("Null pointer access");
	case EExprFailureKind::ExceptionLeft:
		return TEXT("Expression left operand exception");
	case EExprFailureKind::ExceptionRight:
		return TEXT("Expression right operand exception");
	default:
		return FString();
	}
}

FString FailureExpression(const EExprFailureKind Failure)
{
	switch (Failure)
	{
	case EExprFailureKind::InvalidLValue:
		return TEXT("((1 + 2) = 3)");
	case EExprFailureKind::MissingDelimiter:
		return TEXT("RecordFailureStage(1]");
	case EExprFailureKind::MalformedTernary:
		return TEXT("(true ? RecordFailureStage(1))");
	case EExprFailureKind::MissingSymbol:
		return TEXT("MissingFailureSymbol");
	case EExprFailureKind::AmbiguousSymbol:
		return TEXT("ResolveAmbiguousFailure(nullptr)");
	case EExprFailureKind::InaccessibleMember:
		return TEXT("HiddenOwner.HiddenFailureMember");
	case EExprFailureKind::MissingMember:
		return TEXT("MissingOwner.MissingFailureMember");
	case EExprFailureKind::DivideZero:
		return TEXT("RecordFailureStage(1) / RecordFailureStage(0)");
	case EExprFailureKind::IndexOutOfRange:
		return TEXT("FailureIndex[RecordFailureStage(9)]");
	case EExprFailureKind::NullAccess:
		return TEXT("RecordFailureStage(1) + NullFailure.GetValue()");
	case EExprFailureKind::ExceptionLeft:
		return TEXT("ThrowFailureStage(1) + RecordFailureStage(2)");
	case EExprFailureKind::ExceptionRight:
		return TEXT("RecordFailureStage(1) + ThrowFailureStage(2)");
	default:
		return TEXT("0");
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FFailureCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-EXPR-FAILURE-%s-%s-%s"),
		ContextToken(Cell.Context),
		FailureToken(Cell.Failure),
		RecoveryToken(Cell.Recovery));
}

FString MakeEntryName(const FFailureCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FExprFailureParams ParamsFromCell(const FFailureCell& Cell)
{
	FExprFailureParams Params;
	Params.Context = Cell.Context;
	Params.Failure = Cell.Failure;
	Params.Recovery = Cell.Recovery;
	return Params;
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int RecordFailureStage(int Marker)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Marker;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ResolveAmbiguousFailure(FRefRoot Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 1;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ResolveAmbiguousFailure(FRefUnrelated Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 2;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FFailureRestricted"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tprivate int HiddenFailureMember;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FFailureIndex"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint opIndex(int Index) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn RaiseFailureIndex();"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FFailureContextIndex"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint opIndex(int Index) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Index;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObserveFailure(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int RecoverExpressionFailure()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 191;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendFailureSetup(FString& Source, const EExprFailureKind Failure)
{
	if (Failure == EExprFailureKind::InaccessibleMember)
	{
		AppendLine(Source, TEXT("\tFFailureRestricted HiddenOwner;"));
	}
	else if (Failure == EExprFailureKind::MissingMember)
	{
		AppendLine(Source, TEXT("\tFNativeCaseValue MissingOwner(37);"));
	}
	else if (Failure == EExprFailureKind::IndexOutOfRange)
	{
		AppendLine(Source, TEXT("\tFFailureIndex FailureIndex;"));
	}
	else if (Failure == EExprFailureKind::NullAccess)
	{
		AppendLine(Source, TEXT("\tFRefRoot NullFailure = nullptr;"));
	}
}

void AppendFailureUse(FString& Source, const EExprFailureContext Context, const FString& Expression)
{
	if (Context == EExprFailureContext::Initializer)
	{
		AppendLine(Source, FString::Printf(TEXT("\tint Result = %s;"), *Expression));
		AppendLine(Source, TEXT("\treturn Result;"));
	}
	else if (Context == EExprFailureContext::Assignment)
	{
		AppendLine(Source, TEXT("\tint Result = 0;"));
		AppendLine(Source, FString::Printf(TEXT("\tResult = %s;"), *Expression));
		AppendLine(Source, TEXT("\treturn Result;"));
	}
	else if (Context == EExprFailureContext::Argument)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveFailure(%s);"), *Expression));
	}
	else if (Context == EExprFailureContext::Condition)
	{
		AppendLine(Source, FString::Printf(TEXT("\tif (%s != 0)"), *Expression));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else if (Context == EExprFailureContext::LoopClause)
	{
		AppendLine(Source, TEXT("\tint Count = 0;"));
		AppendLine(Source, FString::Printf(TEXT("\tfor (; %s != 0; ++Count)"), *Expression));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn Count;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn Count;"));
	}
	else if (Context == EExprFailureContext::SwitchSelector)
	{
		AppendLine(Source, FString::Printf(TEXT("\tswitch (%s)"), *Expression));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\tcase 0:"));
		AppendLine(Source, TEXT("\t\treturn 0;"));
		AppendLine(Source, TEXT("\tdefault:"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendLine(Source, TEXT("\tFFailureContextIndex ContextIndex;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn ContextIndex[%s];"), *Expression));
	}
}

void AppendEntry(FString& Source, const FExprFailureParams& Params, const FString& FunctionName)
{
	const FString Expression = FailureExpression(Params.Failure);
	if (Params.Context == EExprFailureContext::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("int ProduceFailure_%s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendFailureSetup(Source, Params.Failure);
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Context == EExprFailureContext::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ProduceFailure_%s();"), *FunctionName));
	}
	else
	{
		AppendFailureSetup(Source, Params.Failure);
		AppendFailureUse(Source, Params.Context, Expression);
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FExprFailureParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

TArray<FFailureCell> AllCells()
{
	TArray<FFailureCell> Cells;
	Cells.Reserve(192);
	for (int32 ContextIndex = 0; ContextIndex < 8; ++ContextIndex)
	{
		for (int32 FailureIndex = 0; FailureIndex < 12; ++FailureIndex)
		{
			for (int32 RecoveryIndex = 0; RecoveryIndex < 2; ++RecoveryIndex)
			{
				FFailureCell Cell;
				Cell.Context = static_cast<EExprFailureContext>(ContextIndex);
				Cell.Failure = static_cast<EExprFailureKind>(FailureIndex);
				Cell.Recovery = static_cast<EExprFailureRecovery>(RecoveryIndex);
				Cells.Add(Cell);
			}
		}
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FFailureCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FFailureCell> Cells = AllCells();
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

FGeneratedCaseInfo MakeCaseInfo(const FFailureCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Failure))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		if (Cell.Recovery == EExprFailureRecovery::RebuildOrContextReuse)
		{
			Info.ExecutionNotes = TEXT("Lifecycle rebuild/reuse is a source variant, not executed.");
		}
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
	Info.ExpectedException = ExpectedExceptionText(Cell.Failure);
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	if (Cell.Recovery == EExprFailureRecovery::RebuildOrContextReuse)
	{
		Info.bLimitedObservation = true;
		Info.ExecutionNotes =
			TEXT("Needs native ThrowFailureStage, RaiseFailureIndex, and reference fixtures. Lifecycle rebuild/reuse is not executed.");
	}
	return Info;
}
}

FStringView FExprFailureGenerator::GetProductId() const
{
	return TEXT("LANG-EXPR-FAILURE");
}

TArray<FGeneratedCaseInfo> FExprFailureGenerator::ListCases() const
{
	using namespace ExprFailureGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FFailureCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("ExprFailure PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FExprFailureGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ExprFailureGeneratorDetail;
	FFailureCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FExprFailureGenerator::BuildDumpSource() const
{
	using namespace ExprFailureGeneratorDetail;
	const TArray<FFailureCell> Cells = AllCells();
	int32 RejectCount = 0;
	int32 FaultCount = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Failure))
		{
			++RejectCount;
		}
		else
		{
			++FaultCount;
		}
	}

	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-EXPR-FAILURE"));
	AppendLine(Source, FString::Printf(
		TEXT("// Cases: %d total, 0 normal, %d reject, %d fault"),
		Cells.Num(),
		RejectCount,
		FaultCount));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);

	bool bNeedBlank = false;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Failure))
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
		AppendLine(Source, TEXT("// Outcome: RuntimeException"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedException: %s"), *Info.ExpectedException));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		if (Info.bLimitedObservation)
		{
			AppendLine(Source, TEXT("// LimitedObservation: true"));
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		bNeedBlank = true;
	}

	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsReject(Cells[Index].Failure))
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

FString FExprFailureGenerator::BuildExpressionFailureSource(const FExprFailureParams& Params) const
{
	using namespace ExprFailureGeneratorDetail;
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

FString FExprFailureGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ExprFailureGeneratorDetail;
	const TArray<FFailureCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Failure))
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

int32 FExprFailureGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ExprFailureGeneratorDetail;
	FFailureCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return 0;
}

FString FExprFailureGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ExprFailureGeneratorDetail;
	FFailureCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Failure))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FExprFailureGenerator::ListRejectCaseIds() const
{
	using namespace ExprFailureGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FFailureCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Failure))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
