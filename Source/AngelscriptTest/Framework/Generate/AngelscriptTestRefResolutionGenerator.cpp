#include "Framework/Generate/AngelscriptTestRefResolutionGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace RefResolutionGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs host SelectReference candidate registration.");

struct FRefResolutionCell
{
	ERefResolutionCandidate Candidate = ERefResolutionCandidate::MutableExact;
	ERefResolutionSite Site = ERefResolutionSite::Direct;
	ERefResolutionSource Source = ERefResolutionSource::MutableLValue;
};

const TCHAR* CandidateToken(const ERefResolutionCandidate Candidate)
{
	switch (Candidate)
	{
	case ERefResolutionCandidate::MutableExact:
		return TEXT("MUTABLE_EXACT");
	case ERefResolutionCandidate::ConstPair:
		return TEXT("CONST_PAIR");
	case ERefResolutionCandidate::ValueVsIn:
		return TEXT("VALUE_VS_IN");
	case ERefResolutionCandidate::BaseVsDerived:
		return TEXT("BASE_VS_DERIVED");
	case ERefResolutionCandidate::ReturnCovariance:
		return TEXT("RETURN_COVARIANCE");
	case ERefResolutionCandidate::NullPair:
		return TEXT("NULL_PAIR");
	case ERefResolutionCandidate::NumericConversion:
		return TEXT("NUMERIC_CONVERSION");
	case ERefResolutionCandidate::CompetingConversions:
		return TEXT("COMPETING_CONVERSIONS");
	case ERefResolutionCandidate::MissingCandidate:
		return TEXT("MISSING_CANDIDATE");
	case ERefResolutionCandidate::IncompatibleCandidate:
		return TEXT("INCOMPATIBLE_CANDIDATE");
	default:
		return nullptr;
	}
}

const TCHAR* SiteToken(const ERefResolutionSite Site)
{
	switch (Site)
	{
	case ERefResolutionSite::Direct:
		return TEXT("DIRECT");
	case ERefResolutionSite::Helper:
		return TEXT("HELPER");
	default:
		return nullptr;
	}
}

const TCHAR* SourceToken(const ERefResolutionSource Source)
{
	switch (Source)
	{
	case ERefResolutionSource::MutableLValue:
		return TEXT("MUTABLE_LVALUE");
	case ERefResolutionSource::ConstLValue:
		return TEXT("CONST_LVALUE");
	case ERefResolutionSource::Temporary:
		return TEXT("TEMPORARY");
	case ERefResolutionSource::Field:
		return TEXT("FIELD");
	case ERefResolutionSource::Parameter:
		return TEXT("PARAMETER");
	case ERefResolutionSource::BaseView:
		return TEXT("BASE_VIEW");
	case ERefResolutionSource::DerivedView:
		return TEXT("DERIVED_VIEW");
	case ERefResolutionSource::Null:
		return TEXT("NULL");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FRefResolutionParams& Params)
{
	return CandidateToken(Params.Candidate) != nullptr
		&& SiteToken(Params.Site) != nullptr
		&& SourceToken(Params.Source) != nullptr;
}

bool IsCovariant(const ERefResolutionCandidate Candidate)
{
	return Candidate == ERefResolutionCandidate::ReturnCovariance;
}

bool IsNumeric(const ERefResolutionCandidate Candidate)
{
	return Candidate == ERefResolutionCandidate::NumericConversion
		|| Candidate == ERefResolutionCandidate::CompetingConversions;
}

bool IsReject(
	const ERefResolutionCandidate Candidate,
	const ERefResolutionSource Source,
	const ERefResolutionSite Site)
{
	if (Candidate == ERefResolutionCandidate::MissingCandidate
		|| Candidate == ERefResolutionCandidate::IncompatibleCandidate)
	{
		return true;
	}
	if (Candidate == ERefResolutionCandidate::MutableExact && Source == ERefResolutionSource::ConstLValue)
	{
		return true;
	}
	if (Candidate == ERefResolutionCandidate::ConstPair && Source != ERefResolutionSource::ConstLValue)
	{
		return true;
	}
	return Candidate == ERefResolutionCandidate::NullPair
		&& Source == ERefResolutionSource::Null
		&& Site == ERefResolutionSite::Direct;
}

int32 ExpectedMarker(const ERefResolutionCandidate Candidate, const ERefResolutionSource Source)
{
	switch (Candidate)
	{
	case ERefResolutionCandidate::MutableExact:
		return 101;
	case ERefResolutionCandidate::ConstPair:
		return Source == ERefResolutionSource::ConstLValue ? 202 : 201;
	case ERefResolutionCandidate::ValueVsIn:
		return Source == ERefResolutionSource::ConstLValue ? 302 : 301;
	case ERefResolutionCandidate::BaseVsDerived:
		return Source == ERefResolutionSource::DerivedView ? 402 : 401;
	case ERefResolutionCandidate::ReturnCovariance:
		return Source == ERefResolutionSource::DerivedView ? 502 : 501;
	case ERefResolutionCandidate::NullPair:
		return 601;
	case ERefResolutionCandidate::NumericConversion:
		return 701;
	case ERefResolutionCandidate::CompetingConversions:
		return 802;
	default:
		return 0;
	}
}

int32 ExpectedFromParams(const FRefResolutionParams& Params)
{
	if (IsReject(Params.Candidate, Params.Source, Params.Site))
	{
		return 0;
	}
	if (IsCovariant(Params.Candidate))
	{
		return 1;
	}
	return ExpectedMarker(Params.Candidate, Params.Source);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FRefResolutionCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-REF-RESOLUTION-%s-%s-%s"),
		CandidateToken(Cell.Candidate),
		SiteToken(Cell.Site),
		SourceToken(Cell.Source));
}

FString MakeEntryName(const FRefResolutionCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FRefResolutionParams ParamsFromCell(const FRefResolutionCell& Cell)
{
	FRefResolutionParams Params;
	Params.Candidate = Cell.Candidate;
	Params.Site = Cell.Site;
	Params.Source = Cell.Source;
	return Params;
}

TArray<FRefResolutionCell> AllCells()
{
	TArray<FRefResolutionCell> Cells;
	Cells.SetNum(160);
	for (int32 Index = 0; Index < 160; ++Index)
	{
		FRefResolutionCell& Cell = Cells[Index];
		Cell.Candidate = static_cast<ERefResolutionCandidate>(Index / 16);
		Cell.Site = static_cast<ERefResolutionSite>((Index / 8) % 2);
		Cell.Source = static_cast<ERefResolutionSource>(Index % 8);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FRefResolutionCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FRefResolutionCell> Cells = AllCells();
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

FString StaticSourceType(const ERefResolutionSource Source)
{
	return Source == ERefResolutionSource::DerivedView ? FString(TEXT("FRefDerived")) : FString(TEXT("FRefRoot"));
}

FString SourceParameter(const ERefResolutionSource Source)
{
	if (Source == ERefResolutionSource::ConstLValue)
	{
		return TEXT("const FRefRoot& in Source");
	}
	return StaticSourceType(Source) + TEXT(" Source");
}

FString SourceExpression(const ERefResolutionSource Source)
{
	switch (Source)
	{
	case ERefResolutionSource::Temporary:
		return TEXT("MakeRefRoot(33)");
	case ERefResolutionSource::Field:
		return TEXT("Owner.Field");
	case ERefResolutionSource::Parameter:
		return TEXT("MakeRefRoot(35)");
	default:
		return TEXT("Source");
	}
}

FString CallArgument(
	const ERefResolutionCandidate Candidate,
	const ERefResolutionSource Source,
	const ERefResolutionSite Site,
	const FString& Expression)
{
	if (IsNumeric(Candidate))
	{
		if (Source == ERefResolutionSource::Null)
		{
			return TEXT("0");
		}
		return TEXT("(") + Expression + TEXT(" == nullptr ? 0 : ") + Expression + TEXT(".GetValue())");
	}
	if (Candidate == ERefResolutionCandidate::NullPair
		&& Source == ERefResolutionSource::Null
		&& Site == ERefResolutionSite::Direct)
	{
		return TEXT("nullptr");
	}
	return Expression;
}

FString ResultType(const ERefResolutionSource Source)
{
	return Source == ERefResolutionSource::DerivedView
		? FString(TEXT("const FRefDerived"))
		: FString(TEXT("const FRefRoot"));
}

void AppendSelectionBody(
	FString& Script,
	const TCHAR* Indent,
	const FRefResolutionParams& Params,
	const FString& Expression)
{
	const FString Argument = CallArgument(Params.Candidate, Params.Source, Params.Site, Expression);
	if (!IsCovariant(Params.Candidate))
	{
		AppendLine(Script, FString::Printf(TEXT("%sreturn SelectReference(%s);"), Indent, *Argument));
		return;
	}
	AppendLine(Script, FString::Printf(TEXT("%s%s Result = SelectReference(%s);"), Indent, *ResultType(Params.Source), *Argument));
	if (Params.Source == ERefResolutionSource::Null)
	{
		AppendLine(Script, FString::Printf(TEXT("%sreturn Result == nullptr ? 1 : 0;"), Indent));
		return;
	}
	const int32 ExpectedKind =
		(Params.Source == ERefResolutionSource::DerivedView || Params.Source == ERefResolutionSource::BaseView)
			? 202
			: 101;
	AppendLine(Script, FString::Printf(TEXT("%sreturn Result != nullptr && Result.GetKind() == %d"), Indent, ExpectedKind));
	if (Params.Source != ERefResolutionSource::Temporary)
	{
		AppendLine(Script, FString::Printf(TEXT("%s\t&& SameReference(Result, %s)"), Indent, *Expression));
	}
	AppendLine(Script, FString::Printf(TEXT("%s\t? 1"), Indent));
	AppendLine(Script, FString::Printf(TEXT("%s\t: 0;"), Indent));
}

void AppendFieldType(FString& Script, const ERefResolutionSource Source)
{
	if (Source != ERefResolutionSource::Field)
	{
		return;
	}
	AppendLine(Script, TEXT("struct FResolutionOwner"));
	AppendLine(Script, TEXT("{"));
	AppendLine(Script, TEXT("\tFRefRoot Field;"));
	AppendLine(Script, TEXT("}"));
	AppendLine(Script);
}

void AppendEntrySourceSetup(FString& Script, const ERefResolutionSource Source)
{
	switch (Source)
	{
	case ERefResolutionSource::MutableLValue:
		AppendLine(Script, TEXT("\tFRefRoot Source = MakeRefRoot(31);"));
		break;
	case ERefResolutionSource::ConstLValue:
		AppendLine(Script, TEXT("\tconst FRefRoot Source = MakeRefRoot(32);"));
		break;
	case ERefResolutionSource::Field:
		AppendLine(Script, TEXT("\tFResolutionOwner Owner;"));
		AppendLine(Script, TEXT("\tOwner.Field = MakeRefRoot(34);"));
		break;
	case ERefResolutionSource::BaseView:
		AppendLine(Script, TEXT("\tFRefRoot Source = MakeRefDerivedAsRoot(36);"));
		break;
	case ERefResolutionSource::DerivedView:
		AppendLine(Script, TEXT("\tFRefDerived Source = MakeRefDerived(37);"));
		break;
	case ERefResolutionSource::Null:
		AppendLine(Script, TEXT("\tFRefRoot Source = nullptr;"));
		break;
	default:
		break;
	}
}

FString HelperName(const FString& FunctionName)
{
	return TEXT("InvokeThroughHelper") + FunctionName;
}

FString ParameterName(const FString& FunctionName)
{
	return TEXT("InvokeParameter") + FunctionName;
}

void AppendHelperSite(FString& Script, const FRefResolutionParams& Params, const FString& FunctionName)
{
	if (Params.Site != ERefResolutionSite::Helper)
	{
		return;
	}
	AppendLine(Script, FString::Printf(TEXT("int %s(%s)"), *HelperName(FunctionName), *SourceParameter(Params.Source)));
	AppendLine(Script, TEXT("{"));
	AppendSelectionBody(Script, TEXT("\t"), Params, TEXT("Source"));
	AppendLine(Script, TEXT("}"));
	AppendLine(Script);
}

void AppendParameterSite(FString& Script, const FRefResolutionParams& Params, const FString& FunctionName)
{
	if (Params.Source != ERefResolutionSource::Parameter)
	{
		return;
	}
	AppendLine(Script, FString::Printf(TEXT("int %s(FRefRoot Source)"), *ParameterName(FunctionName)));
	AppendLine(Script, TEXT("{"));
	if (Params.Site == ERefResolutionSite::Helper)
	{
		AppendLine(Script, FString::Printf(TEXT("\treturn %s(Source);"), *HelperName(FunctionName)));
	}
	else
	{
		AppendSelectionBody(Script, TEXT("\t"), Params, TEXT("Source"));
	}
	AppendLine(Script, TEXT("}"));
	AppendLine(Script);
}

void AppendEntry(FString& Script, const FRefResolutionParams& Params, const FString& FunctionName)
{
	AppendLine(Script, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Script, TEXT("{"));
	AppendEntrySourceSetup(Script, Params.Source);
	const FString Expression = SourceExpression(Params.Source);
	if (Params.Source == ERefResolutionSource::Parameter)
	{
		AppendLine(Script, FString::Printf(TEXT("\treturn %s(%s);"), *ParameterName(FunctionName), *Expression));
	}
	else if (Params.Site == ERefResolutionSite::Helper)
	{
		AppendLine(Script, FString::Printf(TEXT("\treturn %s(%s);"), *HelperName(FunctionName), *Expression));
	}
	else
	{
		AppendSelectionBody(Script, TEXT("\t"), Params, Expression);
	}
	AppendLine(Script, TEXT("}"));
}

void AppendCaseFunctions(
	FString& Script,
	const FRefResolutionParams& Params,
	const FString& FunctionName,
	const bool bEmitFieldType)
{
	if (bEmitFieldType)
	{
		AppendFieldType(Script, Params.Source);
	}
	AppendHelperSite(Script, Params, FunctionName);
	AppendParameterSite(Script, Params, FunctionName);
	AppendEntry(Script, Params, FunctionName);
}

FString BuildOwnedSource(const FRefResolutionParams& Params, const FString& FunctionName)
{
	FString Script;
	AppendCaseFunctions(Script, Params, FunctionName, true);
	return Script;
}

FGeneratedCaseInfo MakeCaseInfo(const FRefResolutionCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Candidate, Cell.Source, Cell.Site))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedFromParams(ParamsFromCell(Cell));
	Info.bLimitedObservation = !IsCovariant(Cell.Candidate);
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FRefResolutionGenerator::GetProductId() const
{
	return TEXT("LANG-REF-RESOLUTION");
}

TArray<FGeneratedCaseInfo> FRefResolutionGenerator::ListCases() const
{
	using namespace RefResolutionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FRefResolutionCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("RefResolution PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FRefResolutionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace RefResolutionGeneratorDetail;
	FRefResolutionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	if (IsReject(Cell.Candidate, Cell.Source, Cell.Site))
	{
		return BuildRejectSource(CaseId);
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FRefResolutionGenerator::BuildDumpSource() const
{
	using namespace RefResolutionGeneratorDetail;
	const TArray<FRefResolutionCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-REF-RESOLUTION"));
	AppendLine(Source, TEXT("// Cases: 160 total, 111 normal, 49 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendFieldType(Source, ERefResolutionSource::Field);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Candidate, Cells[Index].Source, Cells[Index].Site))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), HostNotes));
		AppendLine(Source);
		AppendCaseFunctions(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]), false);
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsReject(Cells[Index].Candidate, Cells[Index].Source, Cells[Index].Site))
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
		AppendCaseFunctions(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]), true);
	}
	return Source;
}

FString FRefResolutionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace RefResolutionGeneratorDetail;
	FString Source;
	AppendFieldType(Source, ERefResolutionSource::Field);
	int32 Emitted = 0;
	const TArray<FRefResolutionCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Candidate, Cells[Index].Source, Cells[Index].Site))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendCaseFunctions(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]), false);
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FRefResolutionGenerator::BuildReferenceResolutionSource(const FRefResolutionParams& Params) const
{
	using namespace RefResolutionGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Candidate, Params.Source, Params.Site))
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

int32 FRefResolutionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace RefResolutionGeneratorDetail;
	FRefResolutionCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Candidate, Cell.Source, Cell.Site))
	{
		return 0;
	}
	return ExpectedFromParams(ParamsFromCell(Cell));
}

FString FRefResolutionGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace RefResolutionGeneratorDetail;
	FRefResolutionCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Candidate, Cell.Source, Cell.Site))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FRefResolutionGenerator::ListRejectCaseIds() const
{
	using namespace RefResolutionGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FRefResolutionCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Candidate, Cells[Index].Source, Cells[Index].Site))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
