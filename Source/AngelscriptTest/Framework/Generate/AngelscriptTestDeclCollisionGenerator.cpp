#include "Framework/Generate/AngelscriptTestDeclCollisionGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace DeclCollisionGeneratorDetail
{
enum class ECollisionShape : uint8
{
	GlobalFunctionNoArgs,
	GlobalFunctionOneArg,
	ObjectType,
	EnumType,
	MethodNoArgs,
	MethodOneArg,
	Field,
	PropertyGet,
	PropertySet,
};

struct FCollisionCell
{
	EDeclCollisionOrder InsertionOrder = EDeclCollisionOrder::LeftThenRight;
	EDeclCollisionRelation NamespaceRelation = EDeclCollisionRelation::Same;
	EDeclCollisionPair Pair = EDeclCollisionPair::FunctionOverload;
};

const TCHAR* OrderToken(const EDeclCollisionOrder Order)
{
	switch (Order)
	{
	case EDeclCollisionOrder::LeftThenRight:
		return TEXT("LEFT_THEN_RIGHT");
	case EDeclCollisionOrder::RightThenLeft:
		return TEXT("RIGHT_THEN_LEFT");
	default:
		return nullptr;
	}
}

const TCHAR* RelationToken(const EDeclCollisionRelation Relation)
{
	switch (Relation)
	{
	case EDeclCollisionRelation::Same:
		return TEXT("SAME");
	case EDeclCollisionRelation::Different:
		return TEXT("DIFFERENT");
	case EDeclCollisionRelation::Nested:
		return TEXT("NESTED");
	default:
		return nullptr;
	}
}

const TCHAR* PairToken(const EDeclCollisionPair Pair)
{
	switch (Pair)
	{
	case EDeclCollisionPair::FunctionOverload:
		return TEXT("FUNCTION_OVERLOAD");
	case EDeclCollisionPair::FunctionDuplicate:
		return TEXT("FUNCTION_DUPLICATE");
	case EDeclCollisionPair::FunctionType:
		return TEXT("FUNCTION_TYPE");
	case EDeclCollisionPair::FunctionEnum:
		return TEXT("FUNCTION_ENUM");
	case EDeclCollisionPair::TypeDuplicate:
		return TEXT("TYPE_DUPLICATE");
	case EDeclCollisionPair::TypeEnum:
		return TEXT("TYPE_ENUM");
	case EDeclCollisionPair::EnumDuplicate:
		return TEXT("ENUM_DUPLICATE");
	case EDeclCollisionPair::MethodOverload:
		return TEXT("METHOD_OVERLOAD");
	case EDeclCollisionPair::MethodDuplicate:
		return TEXT("METHOD_DUPLICATE");
	case EDeclCollisionPair::MethodField:
		return TEXT("METHOD_FIELD");
	case EDeclCollisionPair::MethodProperty:
		return TEXT("METHOD_PROPERTY");
	case EDeclCollisionPair::FieldDuplicate:
		return TEXT("FIELD_DUPLICATE");
	case EDeclCollisionPair::FieldProperty:
		return TEXT("FIELD_PROPERTY");
	case EDeclCollisionPair::PropertyGetSet:
		return TEXT("PROPERTY_GET_SET");
	case EDeclCollisionPair::PropertyDuplicateGet:
		return TEXT("PROPERTY_DUPLICATE_GET");
	case EDeclCollisionPair::PropertyDuplicateSet:
		return TEXT("PROPERTY_DUPLICATE_SET");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FDeclCollisionParams& Params)
{
	return OrderToken(Params.InsertionOrder) != nullptr
		&& RelationToken(Params.NamespaceRelation) != nullptr
		&& PairToken(Params.Pair) != nullptr;
}

void PairShapes(const EDeclCollisionPair Pair, ECollisionShape& OutLeft, ECollisionShape& OutRight, bool& bOutMemberOwner)
{
	bOutMemberOwner = false;
	switch (Pair)
	{
	case EDeclCollisionPair::FunctionOverload:
		OutLeft = ECollisionShape::GlobalFunctionNoArgs;
		OutRight = ECollisionShape::GlobalFunctionOneArg;
		break;
	case EDeclCollisionPair::FunctionDuplicate:
		OutLeft = ECollisionShape::GlobalFunctionNoArgs;
		OutRight = ECollisionShape::GlobalFunctionNoArgs;
		break;
	case EDeclCollisionPair::FunctionType:
		OutLeft = ECollisionShape::GlobalFunctionNoArgs;
		OutRight = ECollisionShape::ObjectType;
		break;
	case EDeclCollisionPair::FunctionEnum:
		OutLeft = ECollisionShape::GlobalFunctionNoArgs;
		OutRight = ECollisionShape::EnumType;
		break;
	case EDeclCollisionPair::TypeDuplicate:
		OutLeft = ECollisionShape::ObjectType;
		OutRight = ECollisionShape::ObjectType;
		break;
	case EDeclCollisionPair::TypeEnum:
		OutLeft = ECollisionShape::ObjectType;
		OutRight = ECollisionShape::EnumType;
		break;
	case EDeclCollisionPair::EnumDuplicate:
		OutLeft = ECollisionShape::EnumType;
		OutRight = ECollisionShape::EnumType;
		break;
	case EDeclCollisionPair::MethodOverload:
		OutLeft = ECollisionShape::MethodNoArgs;
		OutRight = ECollisionShape::MethodOneArg;
		bOutMemberOwner = true;
		break;
	case EDeclCollisionPair::MethodDuplicate:
		OutLeft = ECollisionShape::MethodNoArgs;
		OutRight = ECollisionShape::MethodNoArgs;
		bOutMemberOwner = true;
		break;
	case EDeclCollisionPair::MethodField:
		OutLeft = ECollisionShape::MethodNoArgs;
		OutRight = ECollisionShape::Field;
		bOutMemberOwner = true;
		break;
	case EDeclCollisionPair::MethodProperty:
		OutLeft = ECollisionShape::MethodNoArgs;
		OutRight = ECollisionShape::PropertyGet;
		bOutMemberOwner = true;
		break;
	case EDeclCollisionPair::FieldDuplicate:
		OutLeft = ECollisionShape::Field;
		OutRight = ECollisionShape::Field;
		bOutMemberOwner = true;
		break;
	case EDeclCollisionPair::FieldProperty:
		OutLeft = ECollisionShape::Field;
		OutRight = ECollisionShape::PropertyGet;
		bOutMemberOwner = true;
		break;
	case EDeclCollisionPair::PropertyGetSet:
		OutLeft = ECollisionShape::PropertyGet;
		OutRight = ECollisionShape::PropertySet;
		bOutMemberOwner = true;
		break;
	case EDeclCollisionPair::PropertyDuplicateGet:
		OutLeft = ECollisionShape::PropertyGet;
		OutRight = ECollisionShape::PropertyGet;
		bOutMemberOwner = true;
		break;
	case EDeclCollisionPair::PropertyDuplicateSet:
	default:
		OutLeft = ECollisionShape::PropertySet;
		OutRight = ECollisionShape::PropertySet;
		bOutMemberOwner = true;
		break;
	}
}

bool UsesRemovedPropertySyntax(const EDeclCollisionPair Pair)
{
	ECollisionShape Left = ECollisionShape::GlobalFunctionNoArgs;
	ECollisionShape Right = ECollisionShape::GlobalFunctionNoArgs;
	bool bMemberOwner = false;
	PairShapes(Pair, Left, Right, bMemberOwner);
	return Left == ECollisionShape::PropertyGet
		|| Left == ECollisionShape::PropertySet
		|| Right == ECollisionShape::PropertyGet
		|| Right == ECollisionShape::PropertySet;
}

bool LegalInSameOwner(const EDeclCollisionPair Pair)
{
	return Pair == EDeclCollisionPair::FunctionOverload || Pair == EDeclCollisionPair::MethodOverload;
}

bool SameOwner(const EDeclCollisionRelation Relation)
{
	return Relation == EDeclCollisionRelation::Same;
}

bool ShouldCompile(const FCollisionCell& Cell)
{
	return !UsesRemovedPropertySyntax(Cell.Pair)
		&& (!SameOwner(Cell.NamespaceRelation) || LegalInSameOwner(Cell.Pair));
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FCollisionCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-DECL-COLLISION-%s-%s-%s"),
		OrderToken(Cell.InsertionOrder),
		RelationToken(Cell.NamespaceRelation),
		PairToken(Cell.Pair));
}

FString MakeEntryName(const FCollisionCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString TypeSuffix(const FCollisionCell& Cell)
{
	return MakeCanonicalEntryName(
		FString::Printf(
			TEXT("%s-%s-%s"),
			OrderToken(Cell.InsertionOrder),
			RelationToken(Cell.NamespaceRelation),
			PairToken(Cell.Pair)))
		.Mid(5);
}

FString Indent(const int32 Depth)
{
	FString Result;
	for (int32 Index = 0; Index < Depth; ++Index)
	{
		Result += TEXT("\t");
	}
	return Result;
}

int32 AppendNamespaceOpen(FString& Source, const FString& Namespace)
{
	TArray<FString> Parts;
	Namespace.ParseIntoArray(Parts, TEXT("::"), true);
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		const FString Prefix = Indent(Index);
		AppendLine(Source, Prefix + TEXT("namespace ") + Parts[Index]);
		AppendLine(Source, Prefix + TEXT("{"));
	}
	return Parts.Num();
}

void AppendNamespaceClose(FString& Source, const int32 Depth)
{
	for (int32 Index = Depth - 1; Index >= 0; --Index)
	{
		AppendLine(Source, Indent(Index) + TEXT("}"));
	}
	AppendLine(Source);
}

void AppendGlobalDeclaration(FString& Source, const ECollisionShape Shape, const int32 Depth)
{
	const FString Prefix = Indent(Depth);
	switch (Shape)
	{
	case ECollisionShape::GlobalFunctionNoArgs:
		AppendLine(Source, Prefix + TEXT("int Clash()"));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendLine(Source, Prefix + TEXT("\treturn 11;"));
		AppendLine(Source, Prefix + TEXT("}"));
		break;
	case ECollisionShape::GlobalFunctionOneArg:
		AppendLine(Source, Prefix + TEXT("int Clash(int Value)"));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendLine(Source, Prefix + TEXT("\treturn Value + 12;"));
		AppendLine(Source, Prefix + TEXT("}"));
		break;
	case ECollisionShape::ObjectType:
		AppendLine(Source, Prefix + TEXT("class Clash"));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendLine(Source, Prefix + TEXT("\tint Value = 13;"));
		AppendLine(Source, Prefix + TEXT("}"));
		break;
	case ECollisionShape::EnumType:
		AppendLine(Source, Prefix + TEXT("enum Clash"));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendLine(Source, Prefix + TEXT("\tClashValue = 14"));
		AppendLine(Source, Prefix + TEXT("}"));
		break;
	default:
		break;
	}
	AppendLine(Source);
}

void AppendMemberDeclaration(FString& Source, const ECollisionShape Shape, const int32 Depth)
{
	const FString Prefix = Indent(Depth);
	switch (Shape)
	{
	case ECollisionShape::MethodNoArgs:
		AppendLine(Source, Prefix + TEXT("int Clash()"));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendLine(Source, Prefix + TEXT("\treturn 21;"));
		AppendLine(Source, Prefix + TEXT("}"));
		break;
	case ECollisionShape::MethodOneArg:
		AppendLine(Source, Prefix + TEXT("int Clash(int Value)"));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendLine(Source, Prefix + TEXT("\treturn Value + 22;"));
		AppendLine(Source, Prefix + TEXT("}"));
		break;
	case ECollisionShape::Field:
		AppendLine(Source, Prefix + TEXT("int Clash = 23;"));
		break;
	case ECollisionShape::PropertyGet:
		AppendLine(Source, Prefix + TEXT("int Clash"));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendLine(Source, Prefix + TEXT("\tget"));
		AppendLine(Source, Prefix + TEXT("\t{"));
		AppendLine(Source, Prefix + TEXT("\t\treturn 24;"));
		AppendLine(Source, Prefix + TEXT("\t}"));
		AppendLine(Source, Prefix + TEXT("}"));
		break;
	case ECollisionShape::PropertySet:
		AppendLine(Source, Prefix + TEXT("int Clash"));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendLine(Source, Prefix + TEXT("\tset"));
		AppendLine(Source, Prefix + TEXT("\t{"));
		AppendLine(Source, Prefix + TEXT("\t}"));
		AppendLine(Source, Prefix + TEXT("}"));
		break;
	default:
		break;
	}
	AppendLine(Source);
}

void AppendMemberOwner(
	FString& Source,
	const FString& Namespace,
	const FString& OwnerName,
	const ECollisionShape First,
	const ECollisionShape* Second)
{
	const int32 NamespaceDepth = AppendNamespaceOpen(Source, Namespace);
	const FString Prefix = Indent(NamespaceDepth);
	AppendLine(Source, Prefix + TEXT("class ") + OwnerName);
	AppendLine(Source, Prefix + TEXT("{"));
	AppendMemberDeclaration(Source, First, NamespaceDepth + 1);
	if (Second != nullptr)
	{
		AppendMemberDeclaration(Source, *Second, NamespaceDepth + 1);
	}
	AppendLine(Source, Prefix + TEXT("}"));
	AppendNamespaceClose(Source, NamespaceDepth);
}

void AppendGlobalOwner(
	FString& Source,
	const FString& Namespace,
	const ECollisionShape First,
	const ECollisionShape* Second)
{
	const int32 NamespaceDepth = AppendNamespaceOpen(Source, Namespace);
	AppendGlobalDeclaration(Source, First, NamespaceDepth);
	if (Second != nullptr)
	{
		AppendGlobalDeclaration(Source, *Second, NamespaceDepth);
	}
	AppendNamespaceClose(Source, NamespaceDepth);
}

void LeftRightNamespaces(const FCollisionCell& Cell, const FString& Suffix, FString& OutLeft, FString& OutRight)
{
	if (Cell.NamespaceRelation == EDeclCollisionRelation::Same)
	{
		OutLeft = TEXT("Collision") + Suffix;
		OutRight = OutLeft;
	}
	else if (Cell.NamespaceRelation == EDeclCollisionRelation::Different)
	{
		OutLeft = TEXT("CollisionLeft") + Suffix;
		OutRight = TEXT("CollisionRight") + Suffix;
	}
	else
	{
		OutLeft = TEXT("CollisionOuter") + Suffix;
		OutRight = OutLeft + TEXT("::CollisionInner");
	}
}

void AppendCollisionBody(FString& Source, const FCollisionCell& Cell, const FString& FunctionName)
{
	ECollisionShape Left = ECollisionShape::GlobalFunctionNoArgs;
	ECollisionShape Right = ECollisionShape::GlobalFunctionNoArgs;
	bool bMemberOwner = false;
	PairShapes(Cell.Pair, Left, Right, bMemberOwner);
	const bool bLeftFirst = Cell.InsertionOrder == EDeclCollisionOrder::LeftThenRight;
	const ECollisionShape First = bLeftFirst ? Left : Right;
	const ECollisionShape Second = bLeftFirst ? Right : Left;
	const FString Suffix = TypeSuffix(Cell);
	FString LeftNamespace;
	FString RightNamespace;
	LeftRightNamespaces(Cell, Suffix, LeftNamespace, RightNamespace);
	const FString OwnerName = TEXT("FOwner") + Suffix;
	if (SameOwner(Cell.NamespaceRelation))
	{
		if (bMemberOwner)
		{
			AppendMemberOwner(Source, LeftNamespace, OwnerName, First, &Second);
		}
		else
		{
			AppendGlobalOwner(Source, LeftNamespace, First, &Second);
		}
	}
	else if (bMemberOwner)
	{
		AppendMemberOwner(Source, LeftNamespace, OwnerName, First, nullptr);
		AppendMemberOwner(Source, RightNamespace, OwnerName, Second, nullptr);
	}
	else
	{
		AppendGlobalOwner(Source, LeftNamespace, First, nullptr);
		AppendGlobalOwner(Source, RightNamespace, Second, nullptr);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 71;"));
	AppendLine(Source, TEXT("}"));
}

TArray<FCollisionCell> AllCells()
{
	TArray<FCollisionCell> Cells;
	Cells.SetNum(96);
	for (int32 Index = 0; Index < 96; ++Index)
	{
		FCollisionCell& Cell = Cells[Index];
		Cell.InsertionOrder = static_cast<EDeclCollisionOrder>(Index / 48);
		Cell.NamespaceRelation = static_cast<EDeclCollisionRelation>((Index / 16) % 3);
		Cell.Pair = static_cast<EDeclCollisionPair>(Index % 16);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FCollisionCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FCollisionCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString BuildOwnedSource(const FCollisionCell& Cell, const FString& FunctionName)
{
	FString Source;
	AppendCollisionBody(Source, Cell, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FCollisionCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (ShouldCompile(Cell))
	{
		Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.Execution = EGeneratedCaseExecution::Standalone;
		Info.ExpectedReturn = 71;
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
	}
	return Info;
}
}

FStringView FDeclCollisionGenerator::GetProductId() const
{
	return TEXT("LANG-DECL-COLLISION");
}

TArray<FGeneratedCaseInfo> FDeclCollisionGenerator::ListCases() const
{
	using namespace DeclCollisionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FCollisionCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("DeclCollision PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FDeclCollisionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace DeclCollisionGeneratorDetail;
	FCollisionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(Cell, MakeEntryName(Cell));
}

FString FDeclCollisionGenerator::BuildDumpSource() const
{
	using namespace DeclCollisionGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-DECL-COLLISION"));
	AppendLine(Source, TEXT("// Cases: 96 total, 48 normal, 48 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));

	bool bNeedBlank = true;
	for (const FCollisionCell& Cell : AllCells())
	{
		if (!ShouldCompile(Cell))
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
		AppendLine(Source, TEXT("// ExpectedReturn: 71"));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendCollisionBody(Source, Cell, MakeEntryName(Cell));
		bNeedBlank = true;
	}

	for (const FCollisionCell& Cell : AllCells())
	{
		if (ShouldCompile(Cell))
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
		AppendCollisionBody(Source, Cell, MakeEntryName(Cell));
	}
	return Source;
}

FString FDeclCollisionGenerator::BuildCollisionSource(const FDeclCollisionParams& Params) const
{
	using namespace DeclCollisionGeneratorDetail;
	if (!IsValidParams(Params))
	{
		return FString();
	}
	FCollisionCell Cell;
	Cell.InsertionOrder = Params.InsertionOrder;
	Cell.NamespaceRelation = Params.NamespaceRelation;
	Cell.Pair = Params.Pair;
	if (!ShouldCompile(Cell))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	return BuildOwnedSource(Cell, FunctionName);
}

FString FDeclCollisionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace DeclCollisionGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FCollisionCell& Cell : AllCells())
	{
		if (!ShouldCompile(Cell))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendCollisionBody(Source, Cell, MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FDeclCollisionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace DeclCollisionGeneratorDetail;
	FCollisionCell Cell;
	if (!TryFindCell(CaseId, Cell) || !ShouldCompile(Cell))
	{
		return 0;
	}
	return 71;
}

FString FDeclCollisionGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace DeclCollisionGeneratorDetail;
	FCollisionCell Cell;
	if (!TryFindCell(CaseId, Cell) || ShouldCompile(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(Cell, MakeEntryName(Cell));
}

TArray<FString> FDeclCollisionGenerator::ListRejectCaseIds() const
{
	using namespace DeclCollisionGeneratorDetail;
	TArray<FString> Ids;
	for (const FCollisionCell& Cell : AllCells())
	{
		if (!ShouldCompile(Cell))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
