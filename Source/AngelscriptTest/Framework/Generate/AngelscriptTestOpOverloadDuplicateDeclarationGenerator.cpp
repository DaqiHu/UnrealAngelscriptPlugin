#include "Framework/Generate/AngelscriptTestOpOverloadDuplicateDeclarationGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpOverloadDuplicateDeclarationGeneratorDetail
{
constexpr int32 CellCount = 7;

struct FDuplicateCell
{
	EOpOverloadDuplicateDeclarationKind Kind = EOpOverloadDuplicateDeclarationKind::Duplicate;
	EOpOverloadDuplicateDeclarationFamily Family = EOpOverloadDuplicateDeclarationFamily::Unary;
};

const TCHAR* KindToken(const EOpOverloadDuplicateDeclarationKind Kind)
{
	switch (Kind)
	{
	case EOpOverloadDuplicateDeclarationKind::Duplicate:
		return TEXT("DUPLICATE");
	default:
		return nullptr;
	}
}

const TCHAR* FamilyToken(const EOpOverloadDuplicateDeclarationFamily Family)
{
	switch (Family)
	{
	case EOpOverloadDuplicateDeclarationFamily::Unary:
		return TEXT("UNARY");
	case EOpOverloadDuplicateDeclarationFamily::Binary:
		return TEXT("BINARY");
	case EOpOverloadDuplicateDeclarationFamily::Comparison:
		return TEXT("COMPARISON");
	case EOpOverloadDuplicateDeclarationFamily::Index:
		return TEXT("INDEX");
	case EOpOverloadDuplicateDeclarationFamily::Call:
		return TEXT("CALL");
	case EOpOverloadDuplicateDeclarationFamily::Conversion:
		return TEXT("CONVERSION");
	case EOpOverloadDuplicateDeclarationFamily::Assignment:
		return TEXT("ASSIGNMENT");
	default:
		return nullptr;
	}
}

const TCHAR* MethodName(const EOpOverloadDuplicateDeclarationFamily Family)
{
	switch (Family)
	{
	case EOpOverloadDuplicateDeclarationFamily::Unary:
		return TEXT("opNeg");
	case EOpOverloadDuplicateDeclarationFamily::Binary:
		return TEXT("opAdd");
	case EOpOverloadDuplicateDeclarationFamily::Comparison:
		return TEXT("opEquals");
	case EOpOverloadDuplicateDeclarationFamily::Index:
		return TEXT("opIndex");
	case EOpOverloadDuplicateDeclarationFamily::Call:
		return TEXT("opCall");
	case EOpOverloadDuplicateDeclarationFamily::Conversion:
		return TEXT("opImplConv");
	case EOpOverloadDuplicateDeclarationFamily::Assignment:
		return TEXT("opAssign");
	default:
		return nullptr;
	}
}

const TCHAR* ReturnType(const EOpOverloadDuplicateDeclarationFamily Family)
{
	if (Family == EOpOverloadDuplicateDeclarationFamily::Comparison)
	{
		return TEXT("bool");
	}
	if (Family == EOpOverloadDuplicateDeclarationFamily::Assignment)
	{
		return TEXT("FOverloadedOperatorValue&");
	}
	return TEXT("int");
}

FString ParameterDecl(const EOpOverloadDuplicateDeclarationFamily Family)
{
	switch (Family)
	{
	case EOpOverloadDuplicateDeclarationFamily::Binary:
	case EOpOverloadDuplicateDeclarationFamily::Comparison:
	case EOpOverloadDuplicateDeclarationFamily::Assignment:
		return TEXT("const FOverloadedOperatorValue& Other");
	case EOpOverloadDuplicateDeclarationFamily::Index:
	case EOpOverloadDuplicateDeclarationFamily::Call:
		return TEXT("int Other");
	default:
		return FString();
	}
}

bool IsConstMethod(const EOpOverloadDuplicateDeclarationFamily Family)
{
	return Family != EOpOverloadDuplicateDeclarationFamily::Assignment;
}

bool IsValidParams(const FOpOverloadDuplicateDeclarationParams& Params)
{
	return KindToken(Params.Kind) != nullptr && FamilyToken(Params.Family) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FDuplicateCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-OVERLOAD-DUPLICATE-DECLARATION-%s-%s"),
		KindToken(Cell.Kind),
		FamilyToken(Cell.Family));
}

FString MakeEntryName(const FDuplicateCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpOverloadDuplicateDeclarationParams ParamsFromCell(const FDuplicateCell& Cell)
{
	FOpOverloadDuplicateDeclarationParams Params;
	Params.Kind = Cell.Kind;
	Params.Family = Cell.Family;
	return Params;
}

FDuplicateCell CellFromIndex(const int32 Index)
{
	FDuplicateCell Cell;
	Cell.Family = static_cast<EOpOverloadDuplicateDeclarationFamily>(Index);
	return Cell;
}

bool TryFindCell(const FStringView CaseId, FDuplicateCell& OutCell)
{
	const FString Needle(CaseId);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FDuplicateCell Cell = CellFromIndex(Index);
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendOperatorMethod(
	FString& Source,
	const EOpOverloadDuplicateDeclarationFamily Family,
	const int32 Marker,
	const TCHAR* CauseSuffix)
{
	const FString Parameter = ParameterDecl(Family);
	const TCHAR* const ConstSuffix = IsConstMethod(Family) ? TEXT(" const") : TEXT("");
	const FString Signature = Parameter.IsEmpty()
		? FString::Printf(TEXT("\t%s %s()%s%s"), ReturnType(Family), MethodName(Family), ConstSuffix, CauseSuffix)
		: FString::Printf(
			TEXT("\t%s %s(%s)%s%s"),
			ReturnType(Family),
			MethodName(Family),
			*Parameter,
			ConstSuffix,
			CauseSuffix);
	AppendLine(Source, Signature);
	AppendLine(Source, TEXT("\t{"));
	if (Family == EOpOverloadDuplicateDeclarationFamily::Assignment)
	{
		AppendLine(Source, FString::Printf(TEXT("\t\tValue = %d;"), Marker));
		AppendLine(Source, TEXT("\t\treturn this;"));
	}
	else if (Family == EOpOverloadDuplicateDeclarationFamily::Comparison)
	{
		AppendLine(Source, TEXT("\t\treturn true;"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), Marker));
	}
	AppendLine(Source, TEXT("\t}"));
}

void AppendOwnedModule(FString& Source, const FOpOverloadDuplicateDeclarationParams& Params, const FString& FunctionName)
{
	AppendLine(Source, TEXT("struct FOverloadedOperatorValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFOverloadedOperatorValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendOperatorMethod(Source, Params.Family, 401, TEXT(""));
	AppendLine(Source);
	AppendOperatorMethod(Source, Params.Family, 402, TEXT(" // OP_OVERLOAD_DUPLICATE_CAUSE"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFOverloadedOperatorValue Value(5);"));
	AppendLine(Source, TEXT("\treturn Value.Value;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpOverloadDuplicateDeclarationParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedModule(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FDuplicateCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	return Info;
}
}

FStringView FOpOverloadDuplicateDeclarationGenerator::GetProductId() const
{
	return TEXT("LANG-OP-OVERLOAD-DUPLICATE-DECLARATION");
}

TArray<FGeneratedCaseInfo> FOpOverloadDuplicateDeclarationGenerator::ListCases() const
{
	using namespace OpOverloadDuplicateDeclarationGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FDuplicateCell Cell = CellFromIndex(Index);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpOverloadDuplicateDeclaration PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FOpOverloadDuplicateDeclarationGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FOpOverloadDuplicateDeclarationGenerator::BuildDumpSource() const
{
	using namespace OpOverloadDuplicateDeclarationGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-OVERLOAD-DUPLICATE-DECLARATION"));
	AppendLine(Source, TEXT("// Cases: 7 total, 0 normal, 7 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FDuplicateCell Cell = CellFromIndex(Index);
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		AppendOwnedModule(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FOpOverloadDuplicateDeclarationGenerator::BuildDuplicateSource(const FOpOverloadDuplicateDeclarationParams& Params) const
{
	using namespace OpOverloadDuplicateDeclarationGeneratorDetail;
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

FString FOpOverloadDuplicateDeclarationGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

int32 FOpOverloadDuplicateDeclarationGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpOverloadDuplicateDeclarationGeneratorDetail;
	FDuplicateCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return 0;
}

FString FOpOverloadDuplicateDeclarationGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpOverloadDuplicateDeclarationGeneratorDetail;
	FDuplicateCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpOverloadDuplicateDeclarationGenerator::ListRejectCaseIds() const
{
	using namespace OpOverloadDuplicateDeclarationGeneratorDetail;
	TArray<FString> Ids;
	Ids.Reserve(CellCount);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		Ids.Add(MakeCaseId(CellFromIndex(Index)));
	}
	return Ids;
}
}
