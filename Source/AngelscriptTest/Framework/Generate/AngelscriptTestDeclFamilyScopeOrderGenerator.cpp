#include "Framework/Generate/AngelscriptTestDeclFamilyScopeOrderGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace DeclFamilyScopeOrderGeneratorDetail
{
struct FFamilyCell
{
	EDeclFamilyScopeOrderFamily Family = EDeclFamilyScopeOrderFamily::Function;
	EDeclFamilyScopeOrderScope Scope = EDeclFamilyScopeOrderScope::Global;
	EDeclFamilyScopeOrderOrdering Ordering = EDeclFamilyScopeOrderOrdering::BeforeUse;
	int32 FamilyIndex = 0;
	int32 ScopeIndex = 0;
	int32 OrderingIndex = 0;
};

const TCHAR* FamilyToken(const EDeclFamilyScopeOrderFamily Family)
{
	switch (Family)
	{
	case EDeclFamilyScopeOrderFamily::Function:
		return TEXT("FUNCTION");
	case EDeclFamilyScopeOrderFamily::Method:
		return TEXT("METHOD");
	case EDeclFamilyScopeOrderFamily::Class:
		return TEXT("CLASS");
	case EDeclFamilyScopeOrderFamily::Struct:
		return TEXT("STRUCT");
	case EDeclFamilyScopeOrderFamily::Field:
		return TEXT("FIELD");
	case EDeclFamilyScopeOrderFamily::Constructor:
		return TEXT("CONSTRUCTOR");
	case EDeclFamilyScopeOrderFamily::Destructor:
		return TEXT("DESTRUCTOR");
	case EDeclFamilyScopeOrderFamily::Namespace:
		return TEXT("NAMESPACE");
	case EDeclFamilyScopeOrderFamily::Enum:
		return TEXT("ENUM");
	case EDeclFamilyScopeOrderFamily::Typedef:
		return TEXT("TYPEDEF");
	case EDeclFamilyScopeOrderFamily::Funcdef:
		return TEXT("FUNCDEF");
	case EDeclFamilyScopeOrderFamily::Import:
		return TEXT("IMPORT");
	case EDeclFamilyScopeOrderFamily::VirtualProperty:
		return TEXT("VIRTUAL_PROPERTY");
	case EDeclFamilyScopeOrderFamily::IndexedProperty:
		return TEXT("INDEXED_PROPERTY");
	case EDeclFamilyScopeOrderFamily::MixinGlobal:
		return TEXT("MIXIN_GLOBAL");
	default:
		return nullptr;
	}
}

const TCHAR* ScopeToken(const EDeclFamilyScopeOrderScope Scope)
{
	switch (Scope)
	{
	case EDeclFamilyScopeOrderScope::Global:
		return TEXT("GLOBAL");
	case EDeclFamilyScopeOrderScope::Namespace:
		return TEXT("NAMESPACE");
	case EDeclFamilyScopeOrderScope::NestedNamespace:
		return TEXT("NESTED_NAMESPACE");
	case EDeclFamilyScopeOrderScope::Member:
		return TEXT("MEMBER");
	case EDeclFamilyScopeOrderScope::MultipleSections:
		return TEXT("MULTIPLE_SECTIONS");
	case EDeclFamilyScopeOrderScope::ImportedModule:
		return TEXT("IMPORTED_MODULE");
	default:
		return nullptr;
	}
}

const TCHAR* OrderingToken(const EDeclFamilyScopeOrderOrdering Ordering)
{
	switch (Ordering)
	{
	case EDeclFamilyScopeOrderOrdering::BeforeUse:
		return TEXT("BEFORE_USE");
	case EDeclFamilyScopeOrderOrdering::ForwardUse:
		return TEXT("FORWARD_USE");
	case EDeclFamilyScopeOrderOrdering::SameSection:
		return TEXT("SAME_SECTION");
	case EDeclFamilyScopeOrderOrdering::LaterSection:
		return TEXT("LATER_SECTION");
	case EDeclFamilyScopeOrderOrdering::ReversedSections:
		return TEXT("REVERSED_SECTIONS");
	case EDeclFamilyScopeOrderOrdering::Rebuild:
		return TEXT("REBUILD");
	default:
		return nullptr;
	}
}

bool IsIncludedFamily(const EDeclFamilyScopeOrderFamily Family)
{
	const int32 Index = static_cast<int32>(Family);
	return Index >= 0 && Index <= static_cast<int32>(EDeclFamilyScopeOrderFamily::Typedef);
}

bool IsIncludedScope(const EDeclFamilyScopeOrderScope Scope)
{
	const int32 Index = static_cast<int32>(Scope);
	return Index >= 0 && Index <= static_cast<int32>(EDeclFamilyScopeOrderScope::MultipleSections);
}

bool IsValidParams(const FDeclFamilyScopeOrderParams& Params)
{
	return FamilyToken(Params.Family) != nullptr
		&& ScopeToken(Params.Scope) != nullptr
		&& OrderingToken(Params.Ordering) != nullptr
		&& IsIncludedFamily(Params.Family)
		&& IsIncludedScope(Params.Scope);
}

int32 ExpectedFromCell(const FFamilyCell& Cell)
{
	return (100 + Cell.FamilyIndex) + (200 + Cell.ScopeIndex) + (300 + Cell.OrderingIndex);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FFamilyCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-DECL-FAMILY-SCOPE-ORDER-%s-%s-%s"),
		FamilyToken(Cell.Family),
		ScopeToken(Cell.Scope),
		OrderingToken(Cell.Ordering));
}

FString MakeEntryName(const FFamilyCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString TypeSuffix(const FFamilyCell& Cell)
{
	return MakeCanonicalEntryName(
		FString::Printf(
			TEXT("%s-%s-%s"),
			FamilyToken(Cell.Family),
			ScopeToken(Cell.Scope),
			OrderingToken(Cell.Ordering)))
		.Mid(5);
}

void AppendFunction(FString& Source, const FString& Declaration, const FString& ReturnExpression)
{
	int32 IndentationLength = 0;
	while (IndentationLength < Declaration.Len() && Declaration[IndentationLength] == TCHAR('\t'))
	{
		++IndentationLength;
	}
	const FString Indentation = Declaration.Left(IndentationLength);
	AppendLine(Source, Declaration);
	AppendLine(Source, Indentation + TEXT("{"));
	AppendLine(Source, Indentation + TEXT("\treturn ") + ReturnExpression + TEXT(";"));
	AppendLine(Source, Indentation + TEXT("}"));
}

void AppendFamilySource(FString& Source, const FFamilyCell& Cell, const FString& Suffix)
{
	const FString Value = FString::FromInt(100 + Cell.FamilyIndex);
	const FString Witness = TEXT("FamilyWitness") + Suffix;
	if (Cell.Family == EDeclFamilyScopeOrderFamily::Function)
	{
		const FString Published = TEXT("FamilyPublishedFunction") + Suffix;
		AppendFunction(Source, FString::Printf(TEXT("int %s()"), *Published), Value);
		AppendLine(Source);
		AppendFunction(Source, FString::Printf(TEXT("int %s()"), *Witness), Published + TEXT("()"));
	}
	else if (Cell.Family == EDeclFamilyScopeOrderFamily::Method)
	{
		const FString TypeName = TEXT("FFamilyMethod") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("class %s"), *TypeName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Read()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn ") + Value + TEXT(";"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *Witness));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = %s();"), *TypeName, *TypeName));
		AppendLine(Source, TEXT("\treturn Value.Read();"));
		AppendLine(Source, TEXT("}"));
	}
	else if (Cell.Family == EDeclFamilyScopeOrderFamily::Class)
	{
		const FString TypeName = TEXT("FFamilyClass") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("class %s"), *TypeName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = ") + Value + TEXT(";"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint Read()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn Value;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *Witness));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = %s();"), *TypeName, *TypeName));
		AppendLine(Source, TEXT("\treturn Value.Read();"));
		AppendLine(Source, TEXT("}"));
	}
	else if (Cell.Family == EDeclFamilyScopeOrderFamily::Struct)
	{
		const FString TypeName = TEXT("FFamilyStruct") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("struct %s"), *TypeName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = ") + Value + TEXT(";"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint Read()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn Value;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *Witness));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Value;"), *TypeName));
		AppendLine(Source, TEXT("\treturn Value.Read();"));
		AppendLine(Source, TEXT("}"));
	}
	else if (Cell.Family == EDeclFamilyScopeOrderFamily::Field)
	{
		const FString TypeName = TEXT("FFamilyField") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("class %s"), *TypeName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Published = ") + Value + TEXT(";"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *Witness));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = %s();"), *TypeName, *TypeName));
		AppendLine(Source, TEXT("\treturn Value.Published;"));
		AppendLine(Source, TEXT("}"));
	}
	else if (Cell.Family == EDeclFamilyScopeOrderFamily::Constructor)
	{
		const FString TypeName = TEXT("FFamilyConstructor") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("class %s"), *TypeName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Published;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\t%s(int InValue)"), *TypeName));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tPublished = InValue;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *Witness));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = %s(%s);"), *TypeName, *TypeName, *Value));
		AppendLine(Source, TEXT("\treturn Value.Published;"));
		AppendLine(Source, TEXT("}"));
	}
	else if (Cell.Family == EDeclFamilyScopeOrderFamily::Destructor)
	{
		const FString TypeName = TEXT("FFamilyDestructor") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("class %s"), *TypeName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Published = ") + Value + TEXT(";"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\t~%s()"), *TypeName));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *Witness));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Value;"), *TypeName));
		AppendLine(Source, TEXT("\treturn ") + Value + TEXT(";"));
		AppendLine(Source, TEXT("}"));
	}
	else if (Cell.Family == EDeclFamilyScopeOrderFamily::Namespace)
	{
		const FString NamespaceName = TEXT("FamilyNamespace") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("namespace %s"), *NamespaceName));
		AppendLine(Source, TEXT("{"));
		AppendFunction(Source, TEXT("\tint Read()"), Value);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendFunction(Source, FString::Printf(TEXT("int %s()"), *Witness), NamespaceName + TEXT("::Read()"));
	}
	else if (Cell.Family == EDeclFamilyScopeOrderFamily::Enum)
	{
		const FString EnumName = TEXT("EFamilyPublished") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("enum %s"), *EnumName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tPublished = ") + Value);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendFunction(
			Source,
			FString::Printf(TEXT("int %s()"), *Witness),
			FString::Printf(TEXT("int(%s::Published)"), *EnumName));
	}
	else
	{
		const FString AliasName = TEXT("FamilyPublishedAlias") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("typedef int %s;"), *AliasName));
		AppendLine(Source);
		AppendFunction(
			Source,
			FString::Printf(TEXT("int %s()"), *Witness),
			FString::Printf(TEXT("%s(%s)"), *AliasName, *Value));
	}
	AppendLine(Source);
}

void AppendScopeSource(FString& Source, const FFamilyCell& Cell, const FString& Suffix)
{
	const FString Value = FString::FromInt(200 + Cell.ScopeIndex);
	const FString Witness = TEXT("ScopeWitness") + Suffix;
	if (Cell.Scope == EDeclFamilyScopeOrderScope::Global)
	{
		AppendFunction(Source, FString::Printf(TEXT("int %s()"), *Witness), Value);
	}
	else if (Cell.Scope == EDeclFamilyScopeOrderScope::Namespace)
	{
		const FString NamespaceName = TEXT("ScopeNamespace") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("namespace %s"), *NamespaceName));
		AppendLine(Source, TEXT("{"));
		AppendFunction(Source, TEXT("\tint Read()"), Value);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendFunction(Source, FString::Printf(TEXT("int %s()"), *Witness), NamespaceName + TEXT("::Read()"));
	}
	else if (Cell.Scope == EDeclFamilyScopeOrderScope::NestedNamespace)
	{
		const FString OuterName = TEXT("ScopeOuter") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("namespace %s"), *OuterName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tnamespace ScopeInner"));
		AppendLine(Source, TEXT("\t{"));
		AppendFunction(Source, TEXT("\t\tint Read()"), Value);
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendFunction(
			Source,
			FString::Printf(TEXT("int %s()"), *Witness),
			OuterName + TEXT("::ScopeInner::Read()"));
	}
	else if (Cell.Scope == EDeclFamilyScopeOrderScope::Member)
	{
		const FString TypeName = TEXT("FScopeMember") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("class %s"), *TypeName));
		AppendLine(Source, TEXT("{"));
		AppendFunction(Source, TEXT("\tint Read()"), Value);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *Witness));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = %s();"), *TypeName, *TypeName));
		AppendLine(Source, TEXT("\treturn Value.Read();"));
		AppendLine(Source, TEXT("}"));
	}
	else
	{
		const FString Provider = TEXT("ScopeSectionProvider") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("namespace %s"), *Provider));
		AppendLine(Source, TEXT("{"));
		AppendFunction(Source, TEXT("\tint Read()"), Value);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendFunction(Source, FString::Printf(TEXT("int %s()"), *Witness), Provider + TEXT("::Read()"));
	}
	AppendLine(Source);
}

void AppendOrderingSource(FString& Source, const FFamilyCell& Cell, const FString& Suffix)
{
	const FString Value = FString::FromInt(300 + Cell.OrderingIndex);
	const FString Target = TEXT("OrderingTarget") + Suffix;
	const FString Witness = TEXT("OrderingWitness") + Suffix;
	FString TargetSource;
	FString ConsumerSource;
	AppendFunction(TargetSource, FString::Printf(TEXT("int %s()"), *Target), Value);
	AppendFunction(ConsumerSource, FString::Printf(TEXT("int %s()"), *Witness), Target + TEXT("()"));
	const bool bTargetFirst = Cell.Ordering == EDeclFamilyScopeOrderOrdering::BeforeUse
		|| Cell.Ordering == EDeclFamilyScopeOrderOrdering::Rebuild
		|| Cell.Ordering == EDeclFamilyScopeOrderOrdering::SameSection;
	if (bTargetFirst)
	{
		Source += TargetSource;
		AppendLine(Source);
		Source += ConsumerSource;
	}
	else
	{
		Source += ConsumerSource;
		AppendLine(Source);
		Source += TargetSource;
	}
	AppendLine(Source);
}

void AppendTypesAndEntry(FString& Source, const FFamilyCell& Cell, const FString& FunctionName)
{
	const FString Suffix = TypeSuffix(Cell);
	AppendFamilySource(Source, Cell, Suffix);
	AppendScopeSource(Source, Cell, Suffix);
	AppendOrderingSource(Source, Cell, Suffix);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(
		Source,
		FString::Printf(
			TEXT("\treturn FamilyWitness%s() + ScopeWitness%s() + OrderingWitness%s();"),
			*Suffix,
			*Suffix,
			*Suffix));
	AppendLine(Source, TEXT("}"));
}

TArray<FFamilyCell> AllCells()
{
	TArray<FFamilyCell> Cells;
	Cells.SetNum(300);
	for (int32 Index = 0; Index < 300; ++Index)
	{
		FFamilyCell& Cell = Cells[Index];
		Cell.FamilyIndex = Index / 30;
		Cell.ScopeIndex = (Index / 6) % 5;
		Cell.OrderingIndex = Index % 6;
		Cell.Family = static_cast<EDeclFamilyScopeOrderFamily>(Cell.FamilyIndex);
		Cell.Scope = static_cast<EDeclFamilyScopeOrderScope>(Cell.ScopeIndex);
		Cell.Ordering = static_cast<EDeclFamilyScopeOrderOrdering>(Cell.OrderingIndex);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FFamilyCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FFamilyCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString BuildOwnedSource(const FFamilyCell& Cell, const FString& FunctionName)
{
	FString Source;
	AppendTypesAndEntry(Source, Cell, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FFamilyCell& Cell)
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

FStringView FDeclFamilyScopeOrderGenerator::GetProductId() const
{
	return TEXT("LANG-DECL-FAMILY-SCOPE-ORDER");
}

TArray<FGeneratedCaseInfo> FDeclFamilyScopeOrderGenerator::ListCases() const
{
	using namespace DeclFamilyScopeOrderGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FFamilyCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("DeclFamilyScopeOrder PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FDeclFamilyScopeOrderGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace DeclFamilyScopeOrderGeneratorDetail;
	FFamilyCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(Cell, MakeEntryName(Cell));
}

FString FDeclFamilyScopeOrderGenerator::BuildDumpSource() const
{
	using namespace DeclFamilyScopeOrderGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-DECL-FAMILY-SCOPE-ORDER"));
	AppendLine(Source, TEXT("// Cases: 300 total, 300 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (const FFamilyCell& Cell : AllCells())
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendTypesAndEntry(Source, Cell, MakeEntryName(Cell));
	}
	return Source;
}

FString FDeclFamilyScopeOrderGenerator::BuildDeclarationSource(const FDeclFamilyScopeOrderParams& Params) const
{
	using namespace DeclFamilyScopeOrderGeneratorDetail;
	if (!IsValidParams(Params))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	FFamilyCell Cell;
	Cell.Family = Params.Family;
	Cell.Scope = Params.Scope;
	Cell.Ordering = Params.Ordering;
	Cell.FamilyIndex = static_cast<int32>(Params.Family);
	Cell.ScopeIndex = static_cast<int32>(Params.Scope);
	Cell.OrderingIndex = static_cast<int32>(Params.Ordering);
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	return BuildOwnedSource(Cell, FunctionName);
}

FString FDeclFamilyScopeOrderGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace DeclFamilyScopeOrderGeneratorDetail;
	const TArray<FFamilyCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (Index > 0)
		{
			AppendLine(Source);
		}
		AppendTypesAndEntry(Source, Cells[Index], MakeEntryName(Cells[Index]));
	}
	return Source;
}

int32 FDeclFamilyScopeOrderGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace DeclFamilyScopeOrderGeneratorDetail;
	FFamilyCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
