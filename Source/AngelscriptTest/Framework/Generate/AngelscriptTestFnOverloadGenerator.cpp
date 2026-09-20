#include "Framework/Generate/AngelscriptTestFnOverloadGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnOverloadGeneratorDetail
{
struct FOverloadCell
{
	EFnOverloadDiscriminator Discriminator = EFnOverloadDiscriminator::Type;
	EFnOverloadOutcome Outcome = EFnOverloadOutcome::Exact;
};

const TCHAR* DiscriminatorToken(const EFnOverloadDiscriminator Discriminator)
{
	switch (Discriminator)
	{
	case EFnOverloadDiscriminator::Type:
		return TEXT("TYPE");
	case EFnOverloadDiscriminator::Arity:
		return TEXT("ARITY");
	case EFnOverloadDiscriminator::Const:
		return TEXT("CONST");
	case EFnOverloadDiscriminator::Direction:
		return TEXT("DIRECTION");
	case EFnOverloadDiscriminator::Namespace:
		return TEXT("NAMESPACE");
	case EFnOverloadDiscriminator::Default:
		return TEXT("DEFAULT");
	case EFnOverloadDiscriminator::Conversion:
		return TEXT("CONVERSION");
	default:
		return nullptr;
	}
}

const TCHAR* OutcomeToken(const EFnOverloadOutcome Outcome)
{
	switch (Outcome)
	{
	case EFnOverloadOutcome::Exact:
		return TEXT("EXACT");
	case EFnOverloadOutcome::Promotion:
		return TEXT("PROMOTION");
	case EFnOverloadOutcome::Ambiguous:
		return TEXT("AMBIGUOUS");
	case EFnOverloadOutcome::Missing:
		return TEXT("MISSING");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FFnOverloadParams& Params)
{
	return DiscriminatorToken(Params.Discriminator) != nullptr && OutcomeToken(Params.Outcome) != nullptr;
}

bool IsReject(const EFnOverloadOutcome Outcome)
{
	return Outcome == EFnOverloadOutcome::Ambiguous || Outcome == EFnOverloadOutcome::Missing;
}

int32 MarkerBase(const EFnOverloadDiscriminator Discriminator)
{
	switch (Discriminator)
	{
	case EFnOverloadDiscriminator::Type:
		return 100;
	case EFnOverloadDiscriminator::Arity:
		return 200;
	case EFnOverloadDiscriminator::Const:
		return 300;
	case EFnOverloadDiscriminator::Direction:
		return 400;
	case EFnOverloadDiscriminator::Namespace:
		return 500;
	case EFnOverloadDiscriminator::Default:
		return 600;
	default:
		return 700;
	}
}

int32 ExpectedMarker(const EFnOverloadDiscriminator Discriminator, const EFnOverloadOutcome Outcome)
{
	return MarkerBase(Discriminator) + (Outcome == EFnOverloadOutcome::Exact ? 1 : 2);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FOverloadCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-OVERLOAD-%s-%s"),
		DiscriminatorToken(Cell.Discriminator),
		OutcomeToken(Cell.Outcome));
}

FString MakeEntryName(const FOverloadCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString HelperStem(const FString& FunctionName)
{
	if (FunctionName == TEXT("Entry") || FunctionName == TEXT("ProbeEntry"))
	{
		return FString();
	}
	if (FunctionName.StartsWith(TEXT("Entry")))
	{
		return FunctionName.Mid(5);
	}
	return FunctionName;
}

FString ProbeName(const FString& FunctionName)
{
	const FString Stem = HelperStem(FunctionName);
	return Stem.IsEmpty() ? FString(TEXT("Probe")) : TEXT("Probe") + Stem;
}

FString OwnerName(const FString& FunctionName)
{
	const FString Stem = HelperStem(FunctionName);
	return Stem.IsEmpty() ? FString(TEXT("FOwner")) : TEXT("FOwner") + Stem;
}

FFnOverloadParams ParamsFromCell(const FOverloadCell& Cell)
{
	FFnOverloadParams Params;
	Params.Discriminator = Cell.Discriminator;
	Params.Outcome = Cell.Outcome;
	return Params;
}

TArray<FOverloadCell> AllCells()
{
	TArray<FOverloadCell> Cells;
	Cells.SetNum(28);
	for (int32 Index = 0; Index < 28; ++Index)
	{
		FOverloadCell& Cell = Cells[Index];
		Cell.Discriminator = static_cast<EFnOverloadDiscriminator>(Index / 4);
		Cell.Outcome = static_cast<EFnOverloadOutcome>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FOverloadCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FOverloadCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendSimpleFunction(
	FString& Source,
	const FString& Name,
	const TCHAR* Parameters,
	const int32 Marker,
	const TCHAR* Indentation = TEXT(""),
	const TCHAR* Qualifier = TEXT(""))
{
	AppendLine(Source, FString::Printf(TEXT("%sint %s(%s)%s"), Indentation, *Name, Parameters, Qualifier));
	AppendLine(Source, FString::Printf(TEXT("%s{"), Indentation));
	AppendLine(Source, FString::Printf(TEXT("%s\treturn %d;"), Indentation, Marker));
	AppendLine(Source, FString::Printf(TEXT("%s}"), Indentation));
}

void AppendTypeOrConversion(
	FString& Source,
	const FFnOverloadParams& Params,
	const FString& FunctionName)
{
	const FString Probe = ProbeName(FunctionName);
	const int32 Marker = ExpectedMarker(Params.Discriminator, Params.Outcome);
	if (Params.Outcome == EFnOverloadOutcome::Exact)
	{
		AppendSimpleFunction(Source, Probe, TEXT("int Value"), Marker);
		AppendLine(Source);
		AppendSimpleFunction(Source, Probe, TEXT("double Value"), Marker + 10);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		if (Params.Discriminator == EFnOverloadDiscriminator::Conversion)
		{
			AppendLine(Source, FString::Printf(TEXT("\treturn %s(int(7));"), *Probe));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\treturn %s(7);"), *Probe));
		}
		AppendLine(Source, TEXT("}"));
	}
	else if (Params.Outcome == EFnOverloadOutcome::Promotion)
	{
		AppendSimpleFunction(Source, Probe, TEXT("double Value"), Marker);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(7);"), *Probe));
		AppendLine(Source, TEXT("}"));
	}
	else if (Params.Outcome == EFnOverloadOutcome::Ambiguous)
	{
		AppendSimpleFunction(Source, Probe, TEXT("float Value"), 1);
		AppendLine(Source);
		AppendSimpleFunction(Source, Probe, TEXT("double Value"), 2);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(7);"), *Probe));
		AppendLine(Source, TEXT("}"));
	}
	else
	{
		AppendLine(Source, TEXT("struct FValue"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendSimpleFunction(Source, Probe, TEXT("FValue Value"), 1);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(true);"), *Probe));
		AppendLine(Source, TEXT("}"));
	}
}

void AppendArityOrDefault(
	FString& Source,
	const FFnOverloadParams& Params,
	const FString& FunctionName)
{
	const FString Probe = ProbeName(FunctionName);
	const int32 Marker = ExpectedMarker(Params.Discriminator, Params.Outcome);
	if (Params.Outcome == EFnOverloadOutcome::Exact)
	{
		AppendSimpleFunction(Source, Probe, TEXT("int Value"), Marker + 10);
		AppendLine(Source);
		AppendSimpleFunction(Source, Probe, TEXT("int Left, int Right"), Marker);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(3, 4);"), *Probe));
		AppendLine(Source, TEXT("}"));
	}
	else if (Params.Outcome == EFnOverloadOutcome::Promotion)
	{
		AppendSimpleFunction(Source, Probe, TEXT("double Left, double Right"), Marker);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(3, 4);"), *Probe));
		AppendLine(Source, TEXT("}"));
	}
	else if (Params.Outcome == EFnOverloadOutcome::Ambiguous)
	{
		AppendSimpleFunction(Source, Probe, TEXT("int Value"), 1);
		AppendLine(Source);
		AppendSimpleFunction(Source, Probe, TEXT("int Value, int Extra = 2"), 2);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(3);"), *Probe));
		AppendLine(Source, TEXT("}"));
	}
	else
	{
		AppendSimpleFunction(Source, Probe, TEXT("int Left, int Right"), 1);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		if (Params.Discriminator == EFnOverloadDiscriminator::Default)
		{
			AppendLine(Source, FString::Printf(TEXT("\treturn %s();"), *Probe));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\treturn %s(1, 2, 3);"), *Probe));
		}
		AppendLine(Source, TEXT("}"));
	}
}

void AppendConst(
	FString& Source,
	const FFnOverloadParams& Params,
	const FString& FunctionName)
{
	const FString Probe = ProbeName(FunctionName);
	const FString Owner = OwnerName(FunctionName);
	AppendLine(Source, FString::Printf(TEXT("struct %s"), *Owner));
	AppendLine(Source, TEXT("{"));
	if (Params.Outcome == EFnOverloadOutcome::Exact)
	{
		AppendSimpleFunction(Source, Probe, TEXT(""), 301, TEXT("\t"));
		AppendLine(Source);
		AppendSimpleFunction(Source, Probe, TEXT(""), 311, TEXT("\t"), TEXT(" const"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint Invoke()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %s();"), *Probe));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Outcome == EFnOverloadOutcome::Promotion)
	{
		AppendSimpleFunction(Source, Probe, TEXT(""), 302, TEXT("\t"), TEXT(" const"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint Invoke()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %s();"), *Probe));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Outcome == EFnOverloadOutcome::Ambiguous)
	{
		AppendSimpleFunction(Source, Probe, TEXT("float Value"), 1, TEXT("\t"), TEXT(" const"));
		AppendLine(Source);
		AppendSimpleFunction(Source, Probe, TEXT("double Value"), 2, TEXT("\t"), TEXT(" const"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint Invoke() const"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %s(7);"), *Probe));
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendSimpleFunction(Source, Probe, TEXT(""), 1, TEXT("\t"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint Invoke() const"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %s();"), *Probe));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Owner;"), *Owner));
	AppendLine(Source, TEXT("\treturn Owner.Invoke();"));
	AppendLine(Source, TEXT("}"));
}

void AppendDirection(
	FString& Source,
	const FFnOverloadParams& Params,
	const FString& FunctionName)
{
	const FString Probe = ProbeName(FunctionName);
	if (Params.Outcome == EFnOverloadOutcome::Exact)
	{
		AppendSimpleFunction(Source, Probe, TEXT("int& in Value"), 401);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 7;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(Value);"), *Probe));
		AppendLine(Source, TEXT("}"));
	}
	else if (Params.Outcome == EFnOverloadOutcome::Promotion)
	{
		AppendSimpleFunction(Source, Probe, TEXT("double& in Value"), 402);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tdouble Value = 7.0;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(Value);"), *Probe));
		AppendLine(Source, TEXT("}"));
	}
	else if (Params.Outcome == EFnOverloadOutcome::Ambiguous)
	{
		AppendSimpleFunction(Source, Probe, TEXT("float& in Value"), 1);
		AppendLine(Source);
		AppendSimpleFunction(Source, Probe, TEXT("double& in Value"), 2);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 7;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(Value);"), *Probe));
		AppendLine(Source, TEXT("}"));
	}
	else
	{
		AppendSimpleFunction(Source, Probe, TEXT("int& out Value"), 1);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tconst int Value = 7;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(Value);"), *Probe));
		AppendLine(Source, TEXT("}"));
	}
}

void AppendNamespace(
	FString& Source,
	const FFnOverloadParams& Params,
	const FString& FunctionName)
{
	const FString Probe = ProbeName(FunctionName);
	if (Params.Outcome == EFnOverloadOutcome::Missing)
	{
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Missing::Probe(7);"));
		AppendLine(Source, TEXT("}"));
		return;
	}

	AppendLine(Source, TEXT("namespace A"));
	AppendLine(Source, TEXT("{"));
	if (Params.Outcome == EFnOverloadOutcome::Exact)
	{
		AppendSimpleFunction(Source, Probe, TEXT("int Value"), 501, TEXT("\t"));
	}
	else if (Params.Outcome == EFnOverloadOutcome::Promotion)
	{
		AppendSimpleFunction(Source, Probe, TEXT("double Value"), 502, TEXT("\t"));
	}
	else
	{
		AppendSimpleFunction(Source, Probe, TEXT("float Value"), 1, TEXT("\t"));
		AppendLine(Source);
		AppendSimpleFunction(Source, Probe, TEXT("double Value"), 2, TEXT("\t"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("namespace B"));
	AppendLine(Source, TEXT("{"));
	AppendSimpleFunction(Source, Probe, TEXT("int Value"), 599, TEXT("\t"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\treturn A::%s(7);"), *Probe));
	AppendLine(Source, TEXT("}"));
}

void AppendOverload(FString& Source, const FFnOverloadParams& Params, const FString& FunctionName)
{
	if (Params.Discriminator == EFnOverloadDiscriminator::Type
		|| Params.Discriminator == EFnOverloadDiscriminator::Conversion)
	{
		AppendTypeOrConversion(Source, Params, FunctionName);
	}
	else if (Params.Discriminator == EFnOverloadDiscriminator::Arity
		|| Params.Discriminator == EFnOverloadDiscriminator::Default)
	{
		AppendArityOrDefault(Source, Params, FunctionName);
	}
	else if (Params.Discriminator == EFnOverloadDiscriminator::Const)
	{
		AppendConst(Source, Params, FunctionName);
	}
	else if (Params.Discriminator == EFnOverloadDiscriminator::Direction)
	{
		AppendDirection(Source, Params, FunctionName);
	}
	else
	{
		AppendNamespace(Source, Params, FunctionName);
	}
}

FString BuildOwnedSource(const FFnOverloadParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOverload(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FOverloadCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Outcome))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedMarker(Cell.Discriminator, Cell.Outcome);
	return Info;
}
}

FStringView FFnOverloadGenerator::GetProductId() const
{
	return TEXT("LANG-FN-OVERLOAD");
}

TArray<FGeneratedCaseInfo> FFnOverloadGenerator::ListCases() const
{
	using namespace FnOverloadGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FOverloadCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("FnOverload PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnOverloadGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnOverloadGeneratorDetail;
	FOverloadCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnOverloadGenerator::BuildDumpSource() const
{
	using namespace FnOverloadGeneratorDetail;
	const TArray<FOverloadCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-OVERLOAD"));
	AppendLine(Source, TEXT("// Cases: 28 total, 14 normal, 14 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));

	bool bNeedBlank = true;
	for (const FOverloadCell& Cell : Cells)
	{
		if (IsReject(Cell.Outcome))
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
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendOverload(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}

	for (const FOverloadCell& Cell : Cells)
	{
		if (!IsReject(Cell.Outcome))
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
		AppendOverload(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnOverloadGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnOverloadGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FOverloadCell& Cell : AllCells())
	{
		if (IsReject(Cell.Outcome))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendOverload(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FFnOverloadGenerator::BuildOverloadSource(const FFnOverloadParams& Params) const
{
	using namespace FnOverloadGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Outcome))
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

int32 FFnOverloadGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnOverloadGeneratorDetail;
	FOverloadCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Outcome))
	{
		return 0;
	}
	return ExpectedMarker(Cell.Discriminator, Cell.Outcome);
}

FString FFnOverloadGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace FnOverloadGeneratorDetail;
	FOverloadCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Outcome))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FFnOverloadGenerator::ListRejectCaseIds() const
{
	using namespace FnOverloadGeneratorDetail;
	TArray<FString> Ids;
	for (const FOverloadCell& Cell : AllCells())
	{
		if (IsReject(Cell.Outcome))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
