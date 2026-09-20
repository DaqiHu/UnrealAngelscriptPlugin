#include "Framework/Generate/AngelscriptTestInhCastGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace InhCastGeneratorDetail
{
struct FCastCell
{
	EInhCastConstness Constness = EInhCastConstness::Mutable;
	EInhCastRelation Relation = EInhCastRelation::Exact;
	EInhCastUse Use = EInhCastUse::Assign;
};

const TCHAR* ConstnessToken(const EInhCastConstness Constness)
{
	switch (Constness)
	{
	case EInhCastConstness::Mutable:
		return TEXT("MUTABLE");
	case EInhCastConstness::Const:
		return TEXT("CONST");
	default:
		return nullptr;
	}
}

const TCHAR* RelationToken(const EInhCastRelation Relation)
{
	switch (Relation)
	{
	case EInhCastRelation::Exact:
		return TEXT("EXACT");
	case EInhCastRelation::Upcast:
		return TEXT("UPCAST");
	case EInhCastRelation::DowncastSuccess:
		return TEXT("DOWNCAST_SUCCESS");
	case EInhCastRelation::DowncastFailure:
		return TEXT("DOWNCAST_FAILURE");
	case EInhCastRelation::Sibling:
		return TEXT("SIBLING");
	case EInhCastRelation::Null:
		return TEXT("NULL");
	default:
		return nullptr;
	}
}

const TCHAR* UseToken(const EInhCastUse Use)
{
	switch (Use)
	{
	case EInhCastUse::Assign:
		return TEXT("ASSIGN");
	case EInhCastUse::Argument:
		return TEXT("ARGUMENT");
	case EInhCastUse::Return:
		return TEXT("RETURN");
	case EInhCastUse::IdentityCompare:
		return TEXT("IDENTITY_COMPARE");
	case EInhCastUse::MemberCall:
		return TEXT("MEMBER_CALL");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FInhCastParams& Params)
{
	return ConstnessToken(Params.Constness) != nullptr
		&& RelationToken(Params.Relation) != nullptr
		&& UseToken(Params.Use) != nullptr;
}

bool IsSuccessfulRelation(const EInhCastRelation Relation)
{
	return Relation == EInhCastRelation::Exact
		|| Relation == EInhCastRelation::Upcast
		|| Relation == EInhCastRelation::DowncastSuccess;
}

int32 ExpectedFromCell(const FCastCell& Cell)
{
	if (Cell.Use == EInhCastUse::IdentityCompare)
	{
		if (IsSuccessfulRelation(Cell.Relation) || Cell.Relation == EInhCastRelation::Null)
		{
			return 1;
		}
		return 0;
	}
	return IsSuccessfulRelation(Cell.Relation) ? 2 : -1;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FCastCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-INH-CAST-%s-%s-%s"),
		ConstnessToken(Cell.Constness),
		RelationToken(Cell.Relation),
		UseToken(Cell.Use));
}

FString MakeEntryName(const FCastCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FInhCastParams ParamsFromCell(const FCastCell& Cell)
{
	FInhCastParams Params;
	Params.Constness = Cell.Constness;
	Params.Relation = Cell.Relation;
	Params.Use = Cell.Use;
	return Params;
}

TArray<FCastCell> AllCells()
{
	TArray<FCastCell> Cells;
	Cells.SetNum(60);
	for (int32 Index = 0; Index < 60; ++Index)
	{
		FCastCell& Cell = Cells[Index];
		Cell.Constness = static_cast<EInhCastConstness>(Index / 30);
		Cell.Relation = static_cast<EInhCastRelation>((Index / 5) % 6);
		Cell.Use = static_cast<EInhCastUse>(Index % 5);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FCastCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FCastCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString SourceType(const EInhCastRelation Relation)
{
	if (Relation == EInhCastRelation::Exact || Relation == EInhCastRelation::Upcast)
	{
		return TEXT("FCastDerived");
	}
	return TEXT("FCastRoot");
}

FString TargetType(const EInhCastRelation Relation)
{
	return Relation == EInhCastRelation::Upcast ? FString(TEXT("FCastRoot")) : FString(TEXT("FCastDerived"));
}

FString SourceExpression(const EInhCastRelation Relation)
{
	switch (Relation)
	{
	case EInhCastRelation::Exact:
	case EInhCastRelation::Upcast:
		return TEXT("MakeCastDerived()");
	case EInhCastRelation::DowncastSuccess:
		return TEXT("MakeCastDerivedAsRoot()");
	case EInhCastRelation::DowncastFailure:
		return TEXT("MakeCastRoot()");
	case EInhCastRelation::Sibling:
		return TEXT("MakeCastSiblingAsRoot()");
	case EInhCastRelation::Null:
	default:
		return TEXT("nullptr");
	}
}

FString QualifiedType(const FString& Type, const EInhCastConstness Constness)
{
	return Constness == EInhCastConstness::Const ? TEXT("const ") + Type + TEXT("@") : Type + TEXT("@");
}

FString CastExpression(const FString& Target, const TCHAR* VariableName)
{
	return FString::Printf(TEXT("cast<%s>(%s)"), *Target, VariableName);
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("class FCastRoot"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Kind = 1;"));
	AppendLine(Source, TEXT("\tint Identity = 1;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint GetKind() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Kind;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint GetIdentity() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Identity;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FCastDerived : FCastRoot"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFCastDerived()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tKind = 2;"));
	AppendLine(Source, TEXT("\t\tIdentity = 1;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FCastSibling : FCastRoot"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFCastSibling()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tKind = 3;"));
	AppendLine(Source, TEXT("\t\tIdentity = 1;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FCastRoot@ MakeCastRoot()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FCastRoot();"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FCastDerived@ MakeCastDerived()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FCastDerived();"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FCastRoot@ MakeCastDerivedAsRoot()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFCastDerived@ Object = FCastDerived();"));
	AppendLine(Source, TEXT("\treturn Object;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FCastRoot@ MakeCastSiblingAsRoot()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFCastSibling@ Object = FCastSibling();"));
	AppendLine(Source, TEXT("\treturn Object;"));
	AppendLine(Source, TEXT("}"));
}

FString HelperSuffix(const FInhCastParams& Params)
{
	return MakeCanonicalEntryName(
		FString::Printf(
			TEXT("%s-%s-%s"),
			ConstnessToken(Params.Constness),
			RelationToken(Params.Relation),
			UseToken(Params.Use)))
		.Mid(5);
}

void AppendObservationHelper(
	FString& Source,
	const FString& Target,
	const EInhCastConstness Constness,
	const FString& Suffix)
{
	AppendLine(
		Source,
		FString::Printf(TEXT("int ObserveCast%s(%s Value)"), *Suffix, *QualifiedType(Target, Constness)));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value is null ? -1 : Value.GetKind();"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendReturnHelper(
	FString& Source,
	const FString& SourceTypeName,
	const FString& Target,
	const EInhCastConstness Constness,
	const FString& Suffix)
{
	AppendLine(
		Source,
		FString::Printf(
			TEXT("%s ReturnCast%s(%s Value)"),
			*QualifiedType(Target, Constness),
			*Suffix,
			*QualifiedType(SourceTypeName, Constness)));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *CastExpression(Target, TEXT("Value"))));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FInhCastParams& Params, const FString& FunctionName)
{
	const FString SourceTypeName = SourceType(Params.Relation);
	const FString Target = TargetType(Params.Relation);
	const FString Cast = CastExpression(Target, TEXT("Source"));
	const FString Suffix = HelperSuffix(Params);
	AppendObservationHelper(Source, Target, Params.Constness, Suffix);
	AppendReturnHelper(Source, SourceTypeName, Target, Params.Constness, Suffix);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(
		Source,
		FString::Printf(
			TEXT("\t%s Source = %s;"),
			*QualifiedType(SourceTypeName, Params.Constness),
			*SourceExpression(Params.Relation)));
	if (Params.Use == EInhCastUse::Assign || Params.Use == EInhCastUse::MemberCall)
	{
		AppendLine(
			Source,
			FString::Printf(TEXT("\t%s Result = %s;"), *QualifiedType(Target, Params.Constness), *Cast));
		AppendLine(Source, TEXT("\treturn Result is null ? -1 : Result.GetKind();"));
	}
	else if (Params.Use == EInhCastUse::Argument)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveCast%s(%s);"), *Suffix, *Cast));
	}
	else if (Params.Use == EInhCastUse::Return)
	{
		AppendLine(
			Source,
			FString::Printf(
				TEXT("\t%s Result = ReturnCast%s(Source);"),
				*QualifiedType(Target, Params.Constness),
				*Suffix));
		AppendLine(Source, TEXT("\treturn Result is null ? -1 : Result.GetKind();"));
	}
	else
	{
		AppendLine(
			Source,
			FString::Printf(TEXT("\t%s Result = %s;"), *QualifiedType(Target, Params.Constness), *Cast));
		AppendLine(Source, TEXT("\tif (Result is null)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn Source is null ? 1 : 0;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn Result.GetIdentity() == Source.GetIdentity() ? 1 : 0;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FInhCastParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FCastCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	return Info;
}
}

FStringView FInhCastGenerator::GetProductId() const
{
	return TEXT("LANG-INH-CAST");
}

TArray<FGeneratedCaseInfo> FInhCastGenerator::ListCases() const
{
	using namespace InhCastGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FCastCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("InhCast PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FInhCastGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace InhCastGeneratorDetail;
	FCastCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FInhCastGenerator::BuildDumpSource() const
{
	using namespace InhCastGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-INH-CAST"));
	AppendLine(Source, TEXT("// Cases: 60 total, 60 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (const FCastCell& Cell : AllCells())
	{
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

FString FInhCastGenerator::BuildInheritanceCastSource(const FInhCastParams& Params) const
{
	using namespace InhCastGeneratorDetail;
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

FString FInhCastGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace InhCastGeneratorDetail;
	const TArray<FCastCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

int32 FInhCastGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace InhCastGeneratorDetail;
	FCastCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
