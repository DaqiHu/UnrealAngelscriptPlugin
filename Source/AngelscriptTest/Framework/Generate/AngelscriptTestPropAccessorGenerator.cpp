#include "Framework/Generate/AngelscriptTestPropAccessorGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace PropAccessorGeneratorDetail
{
constexpr int32 ScenarioCount = 18;
constexpr int32 ShapeCount = 4;
constexpr int32 TotalCells = ScenarioCount * ShapeCount;
constexpr const TCHAR* HostNotes = TEXT("Needs native CreateRegisteredProperty and ObserveRegisteredProperty.");

struct FAccessorCell
{
	EPropAccessorScenario Scenario = EPropAccessorScenario::GetterReadMutable;
	EPropAccessorSourceShape SourceShape = EPropAccessorSourceShape::SingleLine;
};

const TCHAR* ScenarioToken(const EPropAccessorScenario Scenario)
{
	switch (Scenario)
	{
	case EPropAccessorScenario::GetterReadMutable:
		return TEXT("GETTER_READ_MUTABLE");
	case EPropAccessorScenario::GetterReadConst:
		return TEXT("GETTER_READ_CONST");
	case EPropAccessorScenario::GetterNonconstReadMutable:
		return TEXT("GETTER_NONCONST_READ_MUTABLE");
	case EPropAccessorScenario::GetterNonconstReadConstRejected:
		return TEXT("GETTER_NONCONST_READ_CONST_REJECTED");
	case EPropAccessorScenario::GetterWriteMissing:
		return TEXT("GETTER_WRITE_MISSING");
	case EPropAccessorScenario::GetterCompoundMissingSet:
		return TEXT("GETTER_COMPOUND_MISSING_SET");
	case EPropAccessorScenario::SetterWriteMutable:
		return TEXT("SETTER_WRITE_MUTABLE");
	case EPropAccessorScenario::SetterWriteConstRejected:
		return TEXT("SETTER_WRITE_CONST_REJECTED");
	case EPropAccessorScenario::SetterReadMissing:
		return TEXT("SETTER_READ_MISSING");
	case EPropAccessorScenario::SetterCompoundMissingGet:
		return TEXT("SETTER_COMPOUND_MISSING_GET");
	case EPropAccessorScenario::BothReadMutable:
		return TEXT("BOTH_READ_MUTABLE");
	case EPropAccessorScenario::BothWriteMutable:
		return TEXT("BOTH_WRITE_MUTABLE");
	case EPropAccessorScenario::BothCompoundMutable:
		return TEXT("BOTH_COMPOUND_MUTABLE");
	case EPropAccessorScenario::BothCompoundConstRejected:
		return TEXT("BOTH_COMPOUND_CONST_REJECTED");
	case EPropAccessorScenario::RecursiveGetter:
		return TEXT("RECURSIVE_GETTER");
	case EPropAccessorScenario::RecursiveSetter:
		return TEXT("RECURSIVE_SETTER");
	case EPropAccessorScenario::ThrowingGetter:
		return TEXT("THROWING_GETTER");
	case EPropAccessorScenario::ThrowingSetter:
		return TEXT("THROWING_SETTER");
	default:
		return nullptr;
	}
}

const TCHAR* ShapeToken(const EPropAccessorSourceShape SourceShape)
{
	switch (SourceShape)
	{
	case EPropAccessorSourceShape::SingleLine:
		return TEXT("SINGLE_LINE");
	case EPropAccessorSourceShape::Multiline:
		return TEXT("MULTILINE");
	case EPropAccessorSourceShape::Parenthesized:
		return TEXT("PARENTHESIZED");
	case EPropAccessorSourceShape::HelperCall:
		return TEXT("HELPER_CALL");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FPropAccessorParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr && ShapeToken(Params.SourceShape) != nullptr;
}

bool IsReject(const EPropAccessorScenario Scenario)
{
	return Scenario == EPropAccessorScenario::GetterNonconstReadConstRejected
		|| Scenario == EPropAccessorScenario::GetterWriteMissing
		|| Scenario == EPropAccessorScenario::GetterCompoundMissingSet
		|| Scenario == EPropAccessorScenario::SetterWriteConstRejected
		|| Scenario == EPropAccessorScenario::SetterReadMissing
		|| Scenario == EPropAccessorScenario::SetterCompoundMissingGet
		|| Scenario == EPropAccessorScenario::BothCompoundConstRejected;
}

bool IsFault(const EPropAccessorScenario Scenario)
{
	return Scenario == EPropAccessorScenario::ThrowingGetter
		|| Scenario == EPropAccessorScenario::ThrowingSetter;
}

bool IsConstReceiver(const EPropAccessorScenario Scenario)
{
	return Scenario == EPropAccessorScenario::GetterReadConst
		|| Scenario == EPropAccessorScenario::GetterNonconstReadConstRejected
		|| Scenario == EPropAccessorScenario::SetterWriteConstRejected
		|| Scenario == EPropAccessorScenario::BothCompoundConstRejected;
}

bool IsRead(const EPropAccessorScenario Scenario)
{
	return Scenario == EPropAccessorScenario::GetterReadMutable
		|| Scenario == EPropAccessorScenario::GetterReadConst
		|| Scenario == EPropAccessorScenario::GetterNonconstReadMutable
		|| Scenario == EPropAccessorScenario::GetterNonconstReadConstRejected
		|| Scenario == EPropAccessorScenario::SetterReadMissing
		|| Scenario == EPropAccessorScenario::BothReadMutable
		|| Scenario == EPropAccessorScenario::RecursiveGetter
		|| Scenario == EPropAccessorScenario::ThrowingGetter;
}

bool IsWrite(const EPropAccessorScenario Scenario)
{
	return Scenario == EPropAccessorScenario::GetterWriteMissing
		|| Scenario == EPropAccessorScenario::SetterWriteMutable
		|| Scenario == EPropAccessorScenario::SetterWriteConstRejected
		|| Scenario == EPropAccessorScenario::BothWriteMutable
		|| Scenario == EPropAccessorScenario::RecursiveSetter
		|| Scenario == EPropAccessorScenario::ThrowingSetter;
}

int32 ExpectedFromScenario(const EPropAccessorScenario Scenario)
{
	if (IsReject(Scenario) || IsFault(Scenario))
	{
		return 0;
	}
	if (Scenario == EPropAccessorScenario::BothCompoundMutable)
	{
		return 36;
	}
	if (IsWrite(Scenario))
	{
		return 73;
	}
	return 31;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FAccessorCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-PROP-ACCESSOR-%s-%s"),
		ScenarioToken(Cell.Scenario),
		ShapeToken(Cell.SourceShape));
}

FString MakeEntryName(const FAccessorCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FPropAccessorParams ParamsFromCell(const FAccessorCell& Cell)
{
	FPropAccessorParams Params;
	Params.Scenario = Cell.Scenario;
	Params.SourceShape = Cell.SourceShape;
	return Params;
}

TArray<FAccessorCell> AllCells()
{
	TArray<FAccessorCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FAccessorCell& Cell = Cells[Index];
		Cell.Scenario = static_cast<EPropAccessorScenario>(Index / ShapeCount);
		Cell.SourceShape = static_cast<EPropAccessorSourceShape>(Index % ShapeCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FAccessorCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FAccessorCell& Cell : AllCells())
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
	AppendLine(Source, TEXT("class FRegisteredProperty"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FRegisteredProperty CreateRegisteredProperty(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFRegisteredProperty Receiver;"));
	AppendLine(Source, TEXT("\tReceiver.Value = Value;"));
	AppendLine(Source, TEXT("\treturn Receiver;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObserveRegisteredProperty(const FRegisteredProperty& in Receiver)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Receiver.Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendOperationBody(
	FString& Source,
	const FPropAccessorParams& Params,
	const FString& Indent)
{
	const FString Continuation = Indent + TEXT("\t");
	if (IsRead(Params.Scenario))
	{
		if (Params.SourceShape == EPropAccessorSourceShape::Multiline)
		{
			AppendLine(Source, Indent + TEXT("int Observed ="));
			AppendLine(Source, Continuation + TEXT("Receiver.Value;"));
			if (IsFault(Params.Scenario))
			{
				AppendLine(Source, Indent + TEXT("int Zero = 0;"));
				AppendLine(Source, Indent + TEXT("return 1 / Zero;"));
			}
			else
			{
				AppendLine(Source, Indent + TEXT("return Observed;"));
			}
		}
		else if (Params.SourceShape == EPropAccessorSourceShape::Parenthesized)
		{
			if (IsFault(Params.Scenario))
			{
				AppendLine(Source, Indent + TEXT("int Observed = (((Receiver.Value)));"));
				AppendLine(Source, Indent + TEXT("int Zero = 0;"));
				AppendLine(Source, Indent + TEXT("return 1 / Zero;"));
			}
			else
			{
				AppendLine(Source, Indent + TEXT("return (((Receiver.Value)));"));
			}
		}
		else
		{
			if (IsFault(Params.Scenario))
			{
				AppendLine(Source, Indent + TEXT("int Observed = Receiver.Value;"));
				AppendLine(Source, Indent + TEXT("int Zero = 0;"));
				AppendLine(Source, Indent + TEXT("return 1 / Zero;"));
			}
			else
			{
				AppendLine(Source, Indent + TEXT("return Receiver.Value;"));
			}
		}
		return;
	}

	if (IsWrite(Params.Scenario))
	{
		if (Params.SourceShape == EPropAccessorSourceShape::Multiline)
		{
			AppendLine(Source, Indent + TEXT("Receiver.Value ="));
			AppendLine(Source, Continuation + TEXT("73;"));
		}
		else if (Params.SourceShape == EPropAccessorSourceShape::Parenthesized)
		{
			AppendLine(Source, Indent + TEXT("((Receiver.Value)) = 73;"));
		}
		else
		{
			AppendLine(Source, Indent + TEXT("Receiver.Value = 73;"));
		}
	}
	else
	{
		if (Params.SourceShape == EPropAccessorSourceShape::Multiline)
		{
			AppendLine(Source, Indent + TEXT("Receiver.Value +="));
			AppendLine(Source, Continuation + TEXT("5;"));
		}
		else if (Params.SourceShape == EPropAccessorSourceShape::Parenthesized)
		{
			AppendLine(Source, Indent + TEXT("((Receiver.Value)) += 5;"));
		}
		else
		{
			AppendLine(Source, Indent + TEXT("Receiver.Value += 5;"));
		}
	}
	if (IsFault(Params.Scenario))
	{
		AppendLine(Source, Indent + TEXT("int Zero = 0;"));
		AppendLine(Source, Indent + TEXT("return 1 / Zero;"));
	}
	else
	{
		AppendLine(Source, Indent + TEXT("return ObserveRegisteredProperty(Receiver);"));
	}
}

void AppendCaseHelpers(FString& Source, const FPropAccessorParams& Params, const FString& Qualifier)
{
	if (Params.Scenario == EPropAccessorScenario::RecursiveGetter)
	{
		AppendLine(Source, FString::Printf(
			TEXT("int ReenterRegisteredPropertyGet_%s(FRegisteredProperty& inout Receiver)"),
			*Qualifier));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Receiver.Value;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Scenario == EPropAccessorScenario::RecursiveSetter)
	{
		AppendLine(Source, FString::Printf(
			TEXT("void ReenterRegisteredPropertySet_%s(FRegisteredProperty& inout Receiver, int Value)"),
			*Qualifier));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tReceiver.Value = Value;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	if (Params.SourceShape != EPropAccessorSourceShape::HelperCall)
	{
		return;
	}
	AppendLine(Source, IsConstReceiver(Params.Scenario)
		? FString::Printf(TEXT("int InvokeRegisteredProperty_%s(const FRegisteredProperty& in Receiver)"), *Qualifier)
		: FString::Printf(TEXT("int InvokeRegisteredProperty_%s(FRegisteredProperty& inout Receiver)"), *Qualifier));
	AppendLine(Source, TEXT("{"));
	AppendOperationBody(Source, Params, TEXT("\t"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FPropAccessorParams& Params, const FString& FunctionName)
{
	const FString Qualifier = FunctionName;
	AppendCaseHelpers(Source, Params, Qualifier);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, IsConstReceiver(Params.Scenario)
		? TEXT("\tconst FRegisteredProperty Receiver = CreateRegisteredProperty(31);")
		: TEXT("\tFRegisteredProperty Receiver = CreateRegisteredProperty(31);"));
	if (Params.SourceShape == EPropAccessorSourceShape::HelperCall)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn InvokeRegisteredProperty_%s(Receiver);"), *Qualifier));
	}
	else
	{
		AppendOperationBody(Source, Params, TEXT("\t"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FPropAccessorParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FAccessorCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Scenario))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	if (IsFault(Cell.Scenario))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Divide by zero");
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = ExpectedFromScenario(Cell.Scenario);
	}
	return Info;
}
}

FStringView FPropAccessorGenerator::GetProductId() const
{
	return TEXT("LANG-PROP-ACCESSOR");
}

TArray<FGeneratedCaseInfo> FPropAccessorGenerator::ListCases() const
{
	using namespace PropAccessorGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FAccessorCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("PropAccessor PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FPropAccessorGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace PropAccessorGeneratorDetail;
	FAccessorCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FPropAccessorGenerator::BuildDumpSource() const
{
	using namespace PropAccessorGeneratorDetail;
	const TArray<FAccessorCell> Cells = AllCells();
	int32 NormalCount = 0;
	int32 RejectCount = 0;
	int32 FaultCount = 0;
	for (const FAccessorCell& Cell : Cells)
	{
		if (IsReject(Cell.Scenario))
		{
			++RejectCount;
		}
		else if (IsFault(Cell.Scenario))
		{
			++FaultCount;
		}
		else
		{
			++NormalCount;
		}
	}

	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-PROP-ACCESSOR"));
	AppendLine(Source, FString::Printf(
		TEXT("// Cases: %d total, %d normal, %d reject, %d fault"),
		Cells.Num(),
		NormalCount,
		RejectCount,
		FaultCount));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);

	bool bNeedBlank = true;
	for (const FAccessorCell& Cell : Cells)
	{
		if (IsReject(Cell.Scenario))
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
		if (IsFault(Cell.Scenario))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}

	for (const FAccessorCell& Cell : Cells)
	{
		if (!IsReject(Cell.Scenario))
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
		AppendSharedHelpers(Source);
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FPropAccessorGenerator::BuildRegisteredPropertySource(const FPropAccessorParams& Params) const
{
	using namespace PropAccessorGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Scenario))
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

FString FPropAccessorGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace PropAccessorGeneratorDetail;
	const TArray<FAccessorCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (const FAccessorCell& Cell : Cells)
	{
		if (IsReject(Cell.Scenario))
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

int32 FPropAccessorGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace PropAccessorGeneratorDetail;
	FAccessorCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Scenario) || IsFault(Cell.Scenario))
	{
		return 0;
	}
	return ExpectedFromScenario(Cell.Scenario);
}

FString FPropAccessorGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace PropAccessorGeneratorDetail;
	FAccessorCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Scenario))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FPropAccessorGenerator::ListRejectCaseIds() const
{
	using namespace PropAccessorGeneratorDetail;
	TArray<FString> Ids;
	for (const FAccessorCell& Cell : AllCells())
	{
		if (IsReject(Cell.Scenario))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
