#include "Framework/Generate/AngelscriptTestCtorVisibilityGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace CtorVisibilityGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native constructor-visibility host markers.");

struct FCtorVisibilityCell
{
	ECtorVisibilitySelection Selection = ECtorVisibilitySelection::Exact;
	ECtorVisibilitySite Site = ECtorVisibilitySite::Owner;
	ECtorVisibilityVisibility Visibility = ECtorVisibilityVisibility::Default;
};

const TCHAR* SelectionToken(const ECtorVisibilitySelection Selection)
{
	switch (Selection)
	{
	case ECtorVisibilitySelection::Exact:
		return TEXT("EXACT");
	case ECtorVisibilitySelection::Promotion:
		return TEXT("PROMOTION");
	case ECtorVisibilitySelection::ExplicitCast:
		return TEXT("EXPLICIT_CAST");
	case ECtorVisibilitySelection::ImplicitRejected:
		return TEXT("IMPLICIT_REJECTED");
	case ECtorVisibilitySelection::Ambiguous:
		return TEXT("AMBIGUOUS");
	case ECtorVisibilitySelection::Missing:
		return TEXT("MISSING");
	default:
		return nullptr;
	}
}

const TCHAR* SiteToken(const ECtorVisibilitySite Site)
{
	switch (Site)
	{
	case ECtorVisibilitySite::Owner:
		return TEXT("OWNER");
	case ECtorVisibilitySite::Derived:
		return TEXT("DERIVED");
	case ECtorVisibilitySite::Unrelated:
		return TEXT("UNRELATED");
	case ECtorVisibilitySite::Global:
		return TEXT("GLOBAL");
	default:
		return nullptr;
	}
}

const TCHAR* VisibilityToken(const ECtorVisibilityVisibility Visibility)
{
	switch (Visibility)
	{
	case ECtorVisibilityVisibility::Default:
		return TEXT("DEFAULT");
	case ECtorVisibilityVisibility::Protected:
		return TEXT("PROTECTED");
	case ECtorVisibilityVisibility::Private:
		return TEXT("PRIVATE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FCtorVisibilityParams& Params)
{
	return SelectionToken(Params.Selection) != nullptr
		&& SiteToken(Params.Site) != nullptr
		&& VisibilityToken(Params.Visibility) != nullptr;
}

bool IsDirectSelection(const ECtorVisibilitySelection Selection)
{
	return Selection == ECtorVisibilitySelection::Exact
		|| Selection == ECtorVisibilitySelection::Promotion
		|| Selection == ECtorVisibilitySelection::ExplicitCast;
}

bool IsAccessible(const ECtorVisibilityVisibility Visibility, const ECtorVisibilitySite Site)
{
	if (Visibility == ECtorVisibilityVisibility::Default)
	{
		return true;
	}
	if (Visibility == ECtorVisibilityVisibility::Protected)
	{
		return Site == ECtorVisibilitySite::Owner || Site == ECtorVisibilitySite::Derived;
	}
	return Site == ECtorVisibilitySite::Owner;
}

bool IsReject(const ECtorVisibilitySelection Selection, const ECtorVisibilitySite Site, const ECtorVisibilityVisibility Visibility)
{
	return !IsDirectSelection(Selection) || !IsAccessible(Visibility, Site);
}

const TCHAR* VisibilityPrefix(const ECtorVisibilityVisibility Visibility)
{
	if (Visibility == ECtorVisibilityVisibility::Protected)
	{
		return TEXT("protected ");
	}
	if (Visibility == ECtorVisibilityVisibility::Private)
	{
		return TEXT("private ");
	}
	return TEXT("");
}

FString ConstructorArgument(const ECtorVisibilitySelection Selection)
{
	if (Selection == ECtorVisibilitySelection::Promotion)
	{
		return TEXT("int8(7)");
	}
	if (Selection == ECtorVisibilitySelection::ExplicitCast)
	{
		return TEXT("int64(int8(7))");
	}
	return TEXT("int(7)");
}

FString CandidateType(const ECtorVisibilitySelection Selection, const FString& Tag)
{
	if (Selection == ECtorVisibilitySelection::Promotion || Selection == ECtorVisibilitySelection::ExplicitCast)
	{
		return TEXT("int64");
	}
	if (Selection == ECtorVisibilitySelection::Missing)
	{
		return FString::Printf(TEXT("FVisibilityNoMatch%s"), *Tag);
	}
	return TEXT("int");
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FCtorVisibilityCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CTOR-VISIBILITY-%s-%s-%s"),
		SelectionToken(Cell.Selection),
		SiteToken(Cell.Site),
		VisibilityToken(Cell.Visibility));
}

FString MakeEntryName(const FCtorVisibilityCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString Compact(const TCHAR* Token)
{
	FString Text(Token);
	Text.ReplaceInline(TEXT("_"), TEXT(""));
	return Text;
}

FString TypeTag(const FCtorVisibilityCell& Cell)
{
	return Compact(SelectionToken(Cell.Selection)) + Compact(SiteToken(Cell.Site)) + Compact(VisibilityToken(Cell.Visibility));
}

FCtorVisibilityParams ParamsFromCell(const FCtorVisibilityCell& Cell)
{
	FCtorVisibilityParams Params;
	Params.Selection = Cell.Selection;
	Params.Site = Cell.Site;
	Params.Visibility = Cell.Visibility;
	return Params;
}

TArray<FCtorVisibilityCell> AllCells()
{
	TArray<FCtorVisibilityCell> Cells;
	Cells.SetNum(72);
	for (int32 Index = 0; Index < 72; ++Index)
	{
		FCtorVisibilityCell& Cell = Cells[Index];
		Cell.Selection = static_cast<ECtorVisibilitySelection>(Index / 12);
		Cell.Site = static_cast<ECtorVisibilitySite>((Index / 3) % 4);
		Cell.Visibility = static_cast<ECtorVisibilityVisibility>(Index % 3);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FCtorVisibilityCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FCtorVisibilityCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("void RecordConstructorVisibilitySelection(int Marker)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendTypes(FString& Source, const FCtorVisibilityParams& Params, const FString& Tag)
{
	const FString Target = FString::Printf(TEXT("FVisibilityTarget%s"), *Tag);
	AppendLine(Source, FString::Printf(TEXT("struct FVisibilityNoMatch%s"), *Tag));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("class %s"), *Target));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("\t%s()"), *Target));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = 1;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	if (Params.Selection == ECtorVisibilitySelection::Ambiguous)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s%s(int InValue, int Extra = 1)"), VisibilityPrefix(Params.Visibility), *Target));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = InValue + Extra;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorVisibilitySelection(501);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\t%s%s(int InValue, float Extra = 1.0f)"), VisibilityPrefix(Params.Visibility), *Target));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = InValue + int(Extra);"));
		AppendLine(Source, TEXT("\t\tRecordConstructorVisibilitySelection(502);"));
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s%s(%s InValue)"),
			VisibilityPrefix(Params.Visibility),
			*Target,
			*CandidateType(Params.Selection, Tag)));
		AppendLine(Source, TEXT("\t{"));
		if (Params.Selection == ECtorVisibilitySelection::Missing)
		{
			AppendLine(Source, TEXT("\t\tValue = InValue.Value;"));
		}
		else
		{
			AppendLine(Source, TEXT("\t\tValue = int(InValue);"));
		}
		AppendLine(Source, FString::Printf(
			TEXT("\t\tRecordConstructorVisibilitySelection(%d);"),
			Params.Selection == ECtorVisibilitySelection::Exact
				? 101
				: (Params.Selection == ECtorVisibilitySelection::Promotion ? 201 : 301)));
		AppendLine(Source, TEXT("\t}"));
	}
	if (Params.Site == ECtorVisibilitySite::Owner)
	{
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint ExerciseVisibility()"));
		AppendLine(Source, TEXT("\t{"));
		if (Params.Selection == ECtorVisibilitySelection::ImplicitRejected)
		{
			AppendLine(Source, FString::Printf(TEXT("\t\tConsumeVisibilityTarget%s(int(7));"), *Tag));
			AppendLine(Source, TEXT("\t\treturn 0;"));
		}
		else
		{
			AppendLine(Source, FString::Printf(
				TEXT("\t\t%s Selected = %s(%s);"),
				*Target,
				*Target,
				*ConstructorArgument(Params.Selection)));
			AppendLine(Source, TEXT("\t\treturn Selected.Value;"));
		}
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("void ConsumeVisibilityTarget%s(%s Value)"), *Tag, *Target));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	if (Params.Site == ECtorVisibilitySite::Derived)
	{
		AppendLine(Source, FString::Printf(TEXT("class FVisibilityDerived%s : %s"), *Tag, *Target));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityDerived%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		if (Params.Selection == ECtorVisibilitySelection::ImplicitRejected)
		{
			AppendLine(Source, TEXT("\t\tsuper();"));
			AppendLine(Source, FString::Printf(TEXT("\t\tConsumeVisibilityTarget%s(int(7));"), *Tag));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\t\tsuper(%s);"), *ConstructorArgument(Params.Selection)));
		}
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Site == ECtorVisibilitySite::Unrelated)
	{
		AppendLine(Source, FString::Printf(TEXT("class FVisibilityUnrelated%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint ExerciseVisibility()"));
		AppendLine(Source, TEXT("\t{"));
		if (Params.Selection == ECtorVisibilitySelection::ImplicitRejected)
		{
			AppendLine(Source, FString::Printf(TEXT("\t\tConsumeVisibilityTarget%s(int(7));"), *Tag));
			AppendLine(Source, TEXT("\t\treturn 0;"));
		}
		else
		{
			AppendLine(Source, FString::Printf(
				TEXT("\t\t%s Selected = %s(%s);"),
				*Target,
				*Target,
				*ConstructorArgument(Params.Selection)));
			AppendLine(Source, TEXT("\t\treturn Selected.Value;"));
		}
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendEntry(FString& Source, const FCtorVisibilityParams& Params, const FString& FunctionName, const FString& Tag)
{
	const FString Target = FString::Printf(TEXT("FVisibilityTarget%s"), *Tag);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Site == ECtorVisibilitySite::Owner)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Owner = %s();"), *Target, *Target));
		AppendLine(Source, TEXT("\treturn Owner.ExerciseVisibility();"));
	}
	else if (Params.Site == ECtorVisibilitySite::Derived)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityDerived%s Selected = FVisibilityDerived%s();"), *Tag, *Tag));
		AppendLine(Source, Params.Selection == ECtorVisibilitySelection::ImplicitRejected
			? TEXT("\treturn 0;")
			: TEXT("\treturn Selected.Value;"));
	}
	else if (Params.Site == ECtorVisibilitySite::Unrelated)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityUnrelated%s Caller = FVisibilityUnrelated%s();"), *Tag, *Tag));
		AppendLine(Source, TEXT("\treturn Caller.ExerciseVisibility();"));
	}
	else if (Params.Selection == ECtorVisibilitySelection::ImplicitRejected)
	{
		AppendLine(Source, FString::Printf(TEXT("\tConsumeVisibilityTarget%s(int(7));"), *Tag));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Selected = %s(%s);"),
			*Target,
			*Target,
			*ConstructorArgument(Params.Selection)));
		AppendLine(Source, TEXT("\treturn Selected.Value;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString CellTag(const FCtorVisibilityParams& Params)
{
	FCtorVisibilityCell Cell;
	Cell.Selection = Params.Selection;
	Cell.Site = Params.Site;
	Cell.Visibility = Params.Visibility;
	return TypeTag(Cell);
}

FString BuildOwnedSource(const FCtorVisibilityParams& Params, const FString& FunctionName)
{
	const FString Tag = CellTag(Params);
	FString Source;
	AppendHelpers(Source);
	AppendTypes(Source, Params, Tag);
	AppendEntry(Source, Params, FunctionName, Tag);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FCtorVisibilityCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Selection, Cell.Site, Cell.Visibility))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = 7;
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FCtorVisibilityGenerator::GetProductId() const
{
	return TEXT("LANG-CTOR-VISIBILITY");
}

TArray<FGeneratedCaseInfo> FCtorVisibilityGenerator::ListCases() const
{
	using namespace CtorVisibilityGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FCtorVisibilityCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("CtorVisibility PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FCtorVisibilityGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace CtorVisibilityGeneratorDetail;
	FCtorVisibilityCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FCtorVisibilityGenerator::BuildDumpSource() const
{
	using namespace CtorVisibilityGeneratorDetail;
	const TArray<FCtorVisibilityCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CTOR-VISIBILITY"));
	AppendLine(Source, TEXT("// Cases: 72 total, 21 normal, 51 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);
	bool bNeedBlank = false;
	for (const FCtorVisibilityCell& Cell : Cells)
	{
		if (IsReject(Cell.Selection, Cell.Site, Cell.Visibility))
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
		AppendLine(Source, TEXT("// ExpectedReturn: 7"));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source);
		AppendTypes(Source, ParamsFromCell(Cell), TypeTag(Cell));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell), TypeTag(Cell));
		bNeedBlank = true;
	}
	for (const FCtorVisibilityCell& Cell : Cells)
	{
		if (!IsReject(Cell.Selection, Cell.Site, Cell.Visibility))
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
		AppendTypes(Source, ParamsFromCell(Cell), TypeTag(Cell));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell), TypeTag(Cell));
	}
	return Source;
}

FString FCtorVisibilityGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace CtorVisibilityGeneratorDetail;
	const TArray<FCtorVisibilityCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	int32 Emitted = 0;
	for (const FCtorVisibilityCell& Cell : Cells)
	{
		if (IsReject(Cell.Selection, Cell.Site, Cell.Visibility))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendTypes(Source, ParamsFromCell(Cell), TypeTag(Cell));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell), TypeTag(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FCtorVisibilityGenerator::BuildConstructorVisibilitySource(const FCtorVisibilityParams& Params) const
{
	using namespace CtorVisibilityGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Selection, Params.Site, Params.Visibility))
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

int32 FCtorVisibilityGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace CtorVisibilityGeneratorDetail;
	FCtorVisibilityCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Selection, Cell.Site, Cell.Visibility))
	{
		return 0;
	}
	return 7;
}

FString FCtorVisibilityGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace CtorVisibilityGeneratorDetail;
	FCtorVisibilityCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Selection, Cell.Site, Cell.Visibility))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FCtorVisibilityGenerator::ListRejectCaseIds() const
{
	using namespace CtorVisibilityGeneratorDetail;
	TArray<FString> Ids;
	for (const FCtorVisibilityCell& Cell : AllCells())
	{
		if (IsReject(Cell.Selection, Cell.Site, Cell.Visibility))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
