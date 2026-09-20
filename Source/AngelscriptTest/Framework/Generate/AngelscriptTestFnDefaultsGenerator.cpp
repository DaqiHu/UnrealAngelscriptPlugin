#include "Framework/Generate/AngelscriptTestFnDefaultsGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnDefaultsGeneratorDetail
{
struct FDefaultsCell
{
	EFnDefaultsOmission Omission = EFnDefaultsOmission::None;
	EFnDefaultsPattern Pattern = EFnDefaultsPattern::FinalOne;
	EFnDefaultsTarget Target = EFnDefaultsTarget::Global;
};

const TCHAR* OmissionToken(const EFnDefaultsOmission Omission)
{
	switch (Omission)
	{
	case EFnDefaultsOmission::None:
		return TEXT("NONE");
	case EFnDefaultsOmission::One:
		return TEXT("ONE");
	case EFnDefaultsOmission::Many:
		return TEXT("MANY");
	default:
		return nullptr;
	}
}

const TCHAR* PatternToken(const EFnDefaultsPattern Pattern)
{
	switch (Pattern)
	{
	case EFnDefaultsPattern::FinalOne:
		return TEXT("FINAL_ONE");
	case EFnDefaultsPattern::FinalMany:
		return TEXT("FINAL_MANY");
	case EFnDefaultsPattern::AllOptional:
		return TEXT("ALL_OPTIONAL");
	case EFnDefaultsPattern::ExplicitOverride:
		return TEXT("EXPLICIT_OVERRIDE");
	case EFnDefaultsPattern::MixedOmittedProvided:
		return TEXT("MIXED_OMITTED_PROVIDED");
	case EFnDefaultsPattern::NonTrailingInvalid:
		return TEXT("NON_TRAILING_INVALID");
	case EFnDefaultsPattern::TypeInvalid:
		return TEXT("TYPE_INVALID");
	case EFnDefaultsPattern::EarlierParameterReferenceInvalid:
		return TEXT("EARLIER_PARAMETER_REFERENCE_INVALID");
	default:
		return nullptr;
	}
}

const TCHAR* TargetToken(const EFnDefaultsTarget Target)
{
	switch (Target)
	{
	case EFnDefaultsTarget::Global:
		return TEXT("GLOBAL");
	case EFnDefaultsTarget::NamespaceGlobal:
		return TEXT("NAMESPACE_GLOBAL");
	case EFnDefaultsTarget::InstanceMethod:
		return TEXT("INSTANCE_METHOD");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FFnDefaultsParams& Params)
{
	return OmissionToken(Params.Omission) != nullptr
		&& PatternToken(Params.Pattern) != nullptr
		&& TargetToken(Params.Target) != nullptr;
}

const TCHAR* PatternParameters(const EFnDefaultsPattern Pattern)
{
	switch (Pattern)
	{
	case EFnDefaultsPattern::FinalOne:
		return TEXT("int A, int B, int C = 3");
	case EFnDefaultsPattern::FinalMany:
		return TEXT("int A, int B = 2, int C = 3");
	case EFnDefaultsPattern::AllOptional:
	case EFnDefaultsPattern::ExplicitOverride:
	case EFnDefaultsPattern::MixedOmittedProvided:
		return TEXT("int A = 1, int B = 2, int C = 3");
	case EFnDefaultsPattern::NonTrailingInvalid:
		return TEXT("int A = 1, int B, int C = 3");
	case EFnDefaultsPattern::TypeInvalid:
		return TEXT("int A = UnknownDefault, int B = 2, int C = 3");
	case EFnDefaultsPattern::EarlierParameterReferenceInvalid:
		return TEXT("int A = 1, int B = A, int C = 3");
	default:
		return nullptr;
	}
}

int32 OptionalCount(const EFnDefaultsPattern Pattern)
{
	switch (Pattern)
	{
	case EFnDefaultsPattern::FinalOne:
		return 1;
	case EFnDefaultsPattern::FinalMany:
		return 2;
	case EFnDefaultsPattern::AllOptional:
	case EFnDefaultsPattern::ExplicitOverride:
	case EFnDefaultsPattern::MixedOmittedProvided:
		return 3;
	default:
		return 0;
	}
}

int32 OmittedCount(const EFnDefaultsOmission Omission)
{
	switch (Omission)
	{
	case EFnDefaultsOmission::None:
		return 0;
	case EFnDefaultsOmission::One:
		return 1;
	case EFnDefaultsOmission::Many:
		return 2;
	default:
		return -1;
	}
}

bool IsValidDeclaration(const EFnDefaultsPattern Pattern)
{
	return Pattern != EFnDefaultsPattern::NonTrailingInvalid
		&& Pattern != EFnDefaultsPattern::TypeInvalid
		&& Pattern != EFnDefaultsPattern::EarlierParameterReferenceInvalid;
}

bool UsesNamedArguments(const EFnDefaultsPattern Pattern)
{
	return Pattern == EFnDefaultsPattern::MixedOmittedProvided;
}

bool IsReject(const EFnDefaultsOmission Omission, const EFnDefaultsPattern Pattern)
{
	const int32 Omitted = OmittedCount(Omission);
	if (!IsValidDeclaration(Pattern))
	{
		if (Pattern == EFnDefaultsPattern::TypeInvalid)
		{
			return false;
		}
		if (Pattern == EFnDefaultsPattern::EarlierParameterReferenceInvalid)
		{
			return Omitted > 1;
		}
		return true;
	}
	return Omitted > OptionalCount(Pattern);
}

int32 ExpectedResult(const EFnDefaultsOmission Omission, const EFnDefaultsPattern Pattern)
{
	if (IsReject(Omission, Pattern))
	{
		return 0;
	}
	if (UsesNamedArguments(Pattern))
	{
		if (Omission == EFnDefaultsOmission::None)
		{
			return 456;
		}
		if (Omission == EFnDefaultsOmission::One)
		{
			return 426;
		}
		return 126;
	}
	if (Omission == EFnDefaultsOmission::None)
	{
		return 456;
	}
	if (Omission == EFnDefaultsOmission::One)
	{
		return 453;
	}
	return 423;
}

FString CallArguments(const EFnDefaultsOmission Omission, const EFnDefaultsPattern Pattern)
{
	if (UsesNamedArguments(Pattern))
	{
		if (Omission == EFnDefaultsOmission::None)
		{
			return TEXT("A: 4, B: 5, C: 6");
		}
		if (Omission == EFnDefaultsOmission::One)
		{
			return TEXT("A: 4, C: 6");
		}
		return TEXT("C: 6");
	}
	if (Omission == EFnDefaultsOmission::None)
	{
		return TEXT("4, 5, 6");
	}
	if (Omission == EFnDefaultsOmission::One)
	{
		return TEXT("4, 5");
	}
	return TEXT("4");
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FDefaultsCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-DEFAULTS-%s-%s-%s"),
		OmissionToken(Cell.Omission),
		PatternToken(Cell.Pattern),
		TargetToken(Cell.Target));
}

FString MakeEntryName(const FDefaultsCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FFnDefaultsParams ParamsFromCell(const FDefaultsCell& Cell)
{
	FFnDefaultsParams Params;
	Params.Omission = Cell.Omission;
	Params.Pattern = Cell.Pattern;
	Params.Target = Cell.Target;
	return Params;
}

TArray<FDefaultsCell> AllCells()
{
	TArray<FDefaultsCell> Cells;
	Cells.SetNum(72);
	for (int32 Index = 0; Index < 72; ++Index)
	{
		FDefaultsCell& Cell = Cells[Index];
		Cell.Omission = static_cast<EFnDefaultsOmission>(Index / 24);
		Cell.Pattern = static_cast<EFnDefaultsPattern>((Index / 3) % 8);
		Cell.Target = static_cast<EFnDefaultsTarget>(Index % 3);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FDefaultsCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FDefaultsCell& Cell : AllCells())
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

void AppendEntry(FString& Source, const FFnDefaultsParams& Params, const FString& FunctionName)
{
	const FString Suffix = NameSuffix(FunctionName);
	const FString ProbeName = TEXT("Probe") + Suffix;
	const FString Arguments = CallArguments(Params.Omission, Params.Pattern);
	const TCHAR* const Parameters = PatternParameters(Params.Pattern);
	if (Params.Target == EFnDefaultsTarget::Global)
	{
		AppendLine(Source, FString::Printf(TEXT("int %s(%s)"), *ProbeName, Parameters));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn A * 100 + B * 10 + C;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(%s);"), *ProbeName, *Arguments));
		AppendLine(Source, TEXT("}"));
	}
	else if (Params.Target == EFnDefaultsTarget::NamespaceGlobal)
	{
		const FString Namespace = TEXT("N") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("namespace %s"), *Namespace));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *ProbeName, Parameters));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn A * 100 + B * 10 + C;"));
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
		AppendLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *ProbeName, Parameters));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn A * 100 + B * 10 + C;"));
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

FString BuildOwnedSource(const FFnDefaultsParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FDefaultsCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Omission, Cell.Pattern))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedResult(Cell.Omission, Cell.Pattern);
	return Info;
}
}

FStringView FFnDefaultsGenerator::GetProductId() const
{
	return TEXT("LANG-FN-DEFAULTS");
}

TArray<FGeneratedCaseInfo> FFnDefaultsGenerator::ListCases() const
{
	using namespace FnDefaultsGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FDefaultsCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("FnDefaults PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnDefaultsGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnDefaultsGeneratorDetail;
	FDefaultsCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnDefaultsGenerator::BuildDumpSource() const
{
	using namespace FnDefaultsGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-DEFAULTS"));
	AppendLine(Source, TEXT("// Cases: 72 total, 57 normal, 15 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	bool bNeedBlank = false;
	for (const FDefaultsCell& Cell : AllCells())
	{
		if (IsReject(Cell.Omission, Cell.Pattern))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	(void)bNeedBlank;
	for (const FDefaultsCell& Cell : AllCells())
	{
		if (!IsReject(Cell.Omission, Cell.Pattern))
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
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnDefaultsGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnDefaultsGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FDefaultsCell& Cell : AllCells())
	{
		if (IsReject(Cell.Omission, Cell.Pattern))
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

FString FFnDefaultsGenerator::BuildDefaultArgumentSource(const FFnDefaultsParams& Params) const
{
	using namespace FnDefaultsGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Omission, Params.Pattern))
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

int32 FFnDefaultsGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnDefaultsGeneratorDetail;
	FDefaultsCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Omission, Cell.Pattern))
	{
		return 0;
	}
	return ExpectedResult(Cell.Omission, Cell.Pattern);
}

FString FFnDefaultsGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace FnDefaultsGeneratorDetail;
	FDefaultsCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Omission, Cell.Pattern))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FFnDefaultsGenerator::ListRejectCaseIds() const
{
	using namespace FnDefaultsGeneratorDetail;
	TArray<FString> Ids;
	for (const FDefaultsCell& Cell : AllCells())
	{
		if (IsReject(Cell.Omission, Cell.Pattern))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
