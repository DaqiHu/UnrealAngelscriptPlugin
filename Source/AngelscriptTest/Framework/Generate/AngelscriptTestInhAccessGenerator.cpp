#include "Framework/Generate/AngelscriptTestInhAccessGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace InhAccessGeneratorDetail
{
struct FAccessCell
{
	EInhAccessKind Access = EInhAccessKind::Default;
	EInhAccessMember Member = EInhAccessMember::Field;
	EInhAccessSite Site = EInhAccessSite::Owner;
};

const TCHAR* AccessToken(const EInhAccessKind Access)
{
	switch (Access)
	{
	case EInhAccessKind::Default:
		return TEXT("DEFAULT");
	case EInhAccessKind::Protected:
		return TEXT("PROTECTED");
	case EInhAccessKind::Private:
		return TEXT("PRIVATE");
	default:
		return nullptr;
	}
}

const TCHAR* MemberToken(const EInhAccessMember Member)
{
	switch (Member)
	{
	case EInhAccessMember::Field:
		return TEXT("FIELD");
	case EInhAccessMember::Method:
		return TEXT("METHOD");
	case EInhAccessMember::GetterSetterMethod:
		return TEXT("GETTER_SETTER_METHOD");
	case EInhAccessMember::Constructor:
		return TEXT("CONSTRUCTOR");
	default:
		return nullptr;
	}
}

const TCHAR* SiteToken(const EInhAccessSite Site)
{
	switch (Site)
	{
	case EInhAccessSite::Owner:
		return TEXT("OWNER");
	case EInhAccessSite::DirectDerived:
		return TEXT("DIRECT_DERIVED");
	case EInhAccessSite::DeepDerived:
		return TEXT("DEEP_DERIVED");
	case EInhAccessSite::Unrelated:
		return TEXT("UNRELATED");
	case EInhAccessSite::Global:
		return TEXT("GLOBAL");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FInhAccessParams& Params)
{
	return AccessToken(Params.Access) != nullptr
		&& MemberToken(Params.Member) != nullptr
		&& SiteToken(Params.Site) != nullptr;
}

bool IsPrivate(const EInhAccessKind Access)
{
	return Access == EInhAccessKind::Private;
}

bool IsProtected(const EInhAccessKind Access)
{
	return Access == EInhAccessKind::Protected;
}

bool IsConstructor(const EInhAccessMember Member)
{
	return Member == EInhAccessMember::Constructor;
}

bool ShouldCompile(const FAccessCell& Cell)
{
	if (IsConstructor(Cell.Member) && IsPrivate(Cell.Access) && Cell.Site == EInhAccessSite::Owner)
	{
		return false;
	}
	if (Cell.Site == EInhAccessSite::Owner)
	{
		return true;
	}
	if (IsConstructor(Cell.Member)
		&& IsProtected(Cell.Access)
		&& (Cell.Site == EInhAccessSite::Unrelated || Cell.Site == EInhAccessSite::Global))
	{
		return true;
	}
	if (Cell.Site == EInhAccessSite::DirectDerived || Cell.Site == EInhAccessSite::DeepDerived)
	{
		return !IsPrivate(Cell.Access);
	}
	return !IsPrivate(Cell.Access) && !IsProtected(Cell.Access);
}

int32 ExpectedFromCell(const FAccessCell& Cell)
{
	if (!ShouldCompile(Cell))
	{
		return 0;
	}
	if (Cell.Member == EInhAccessMember::Field)
	{
		return 41;
	}
	if (Cell.Member == EInhAccessMember::Method)
	{
		return 42;
	}
	if (Cell.Member == EInhAccessMember::GetterSetterMethod)
	{
		return 43;
	}
	if (Cell.Site == EInhAccessSite::Unrelated)
	{
		return 46;
	}
	if (Cell.Site == EInhAccessSite::DirectDerived || Cell.Site == EInhAccessSite::DeepDerived)
	{
		return 48;
	}
	return 47;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FAccessCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-INH-ACCESS-%s-%s-%s"),
		AccessToken(Cell.Access),
		MemberToken(Cell.Member),
		SiteToken(Cell.Site));
}

FString MakeEntryName(const FAccessCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString TypeSuffix(const FAccessCell& Cell)
{
	return MakeCanonicalEntryName(
		FString::Printf(
			TEXT("%s-%s-%s"),
			AccessToken(Cell.Access),
			MemberToken(Cell.Member),
			SiteToken(Cell.Site)))
		.Mid(5);
}

const TCHAR* AccessPrefix(const EInhAccessKind Access)
{
	if (Access == EInhAccessKind::Protected)
	{
		return TEXT("protected ");
	}
	if (Access == EInhAccessKind::Private)
	{
		return TEXT("private ");
	}
	return TEXT("");
}

FInhAccessParams ParamsFromCell(const FAccessCell& Cell)
{
	FInhAccessParams Params;
	Params.Access = Cell.Access;
	Params.Member = Cell.Member;
	Params.Site = Cell.Site;
	return Params;
}

TArray<FAccessCell> AllCells()
{
	TArray<FAccessCell> Cells;
	Cells.SetNum(60);
	for (int32 Index = 0; Index < 60; ++Index)
	{
		FAccessCell& Cell = Cells[Index];
		Cell.Access = static_cast<EInhAccessKind>(Index / 20);
		Cell.Member = static_cast<EInhAccessMember>((Index / 5) % 4);
		Cell.Site = static_cast<EInhAccessSite>(Index % 5);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FAccessCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FAccessCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendTargetMembers(FString& Source, const FAccessCell& Cell, const FString& BaseName)
{
	const TCHAR* const Prefix = AccessPrefix(Cell.Access);
	if (Cell.Member == EInhAccessMember::Field)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%sint Value = 41;"), Prefix));
		return;
	}
	if (Cell.Member == EInhAccessMember::Method)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%sint ReadValue()"), Prefix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 42;"));
		AppendLine(Source, TEXT("\t}"));
		return;
	}
	if (Cell.Member == EInhAccessMember::GetterSetterMethod)
	{
		AppendLine(Source, TEXT("\tint StoredValue = 43;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\t%sint GetValue()"), Prefix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn StoredValue;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\t%svoid SetValue(int InValue)"), Prefix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tStoredValue = InValue;"));
		AppendLine(Source, TEXT("\t}"));
		return;
	}
	AppendLine(Source, TEXT("\tint StoredValue = 44;"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("\t%s%s()"), Prefix, *BaseName));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tStoredValue = 45;"));
	AppendLine(Source, TEXT("\t}"));
}

void AppendAccessOperation(FString& Source, const EInhAccessMember Member, const FString& Receiver, const TCHAR* Indent)
{
	if (Member == EInhAccessMember::Field)
	{
		AppendLine(Source, FString::Printf(TEXT("%sreturn %sValue;"), Indent, *Receiver));
		return;
	}
	if (Member == EInhAccessMember::Method)
	{
		AppendLine(Source, FString::Printf(TEXT("%sreturn %sReadValue();"), Indent, *Receiver));
		return;
	}
	if (Member == EInhAccessMember::GetterSetterMethod)
	{
		AppendLine(Source, FString::Printf(TEXT("%s%sSetValue(73);"), Indent, *Receiver));
		AppendLine(Source, FString::Printf(TEXT("%sreturn %sGetValue();"), Indent, *Receiver));
	}
}

void AppendTypesAndEntry(FString& Source, const FAccessCell& Cell, const FString& FunctionName)
{
	const FString Suffix = TypeSuffix(Cell);
	const FString BaseName = TEXT("FAccessBase") + Suffix;
	const FString DerivedName = TEXT("FAccessDerived") + Suffix;
	const FString DeepName = TEXT("FAccessDeep") + Suffix;
	const FString UnrelatedName = TEXT("FAccessUnrelated") + Suffix;

	AppendLine(Source, FString::Printf(TEXT("class %s"), *BaseName));
	AppendLine(Source, TEXT("{"));
	AppendTargetMembers(Source, Cell, BaseName);
	if (Cell.Site == EInhAccessSite::Owner)
	{
		AppendLine(Source);
		if (IsConstructor(Cell.Member))
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s MakeOwner()"), *BaseName));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, FString::Printf(TEXT("\t\treturn %s();"), *BaseName));
			AppendLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendLine(Source, TEXT("\tint ProbeAccess()"));
			AppendLine(Source, TEXT("\t{"));
			AppendAccessOperation(Source, Cell.Member, FString(), TEXT("\t\t"));
			AppendLine(Source, TEXT("\t}"));
		}
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);

	AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *DerivedName, *BaseName));
	AppendLine(Source, TEXT("{"));
	if (IsConstructor(Cell.Member))
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s()"), *DerivedName));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tsuper();"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Cell.Site == EInhAccessSite::DirectDerived)
	{
		AppendLine(Source, TEXT("\tint ProbeAccess()"));
		AppendLine(Source, TEXT("\t{"));
		AppendAccessOperation(Source, Cell.Member, FString(), TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);

	AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *DeepName, *DerivedName));
	AppendLine(Source, TEXT("{"));
	if (IsConstructor(Cell.Member))
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s()"), *DeepName));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tsuper();"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Cell.Site == EInhAccessSite::DeepDerived)
	{
		AppendLine(Source, TEXT("\tint ProbeAccess()"));
		AppendLine(Source, TEXT("\t{"));
		AppendAccessOperation(Source, Cell.Member, FString(), TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);

	if (Cell.Site == EInhAccessSite::Unrelated)
	{
		AppendLine(Source, FString::Printf(TEXT("class %s"), *UnrelatedName));
		AppendLine(Source, TEXT("{"));
		if (IsConstructor(Cell.Member))
		{
			AppendLine(Source, TEXT("\tint ProbeAccess()"));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, FString::Printf(TEXT("\t\t%s Object;"), *BaseName));
			AppendLine(Source, TEXT("\t\treturn 46;"));
			AppendLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\tint ProbeAccess(%s Object)"), *BaseName));
			AppendLine(Source, TEXT("\t{"));
			AppendAccessOperation(Source, Cell.Member, TEXT("Object."), TEXT("\t\t"));
			AppendLine(Source, TEXT("\t}"));
		}
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}

	if (Cell.Site == EInhAccessSite::Global)
	{
		if (IsConstructor(Cell.Member))
		{
			AppendLine(Source, FString::Printf(TEXT("int ProbeGlobalAccess%s()"), *Suffix));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, FString::Printf(TEXT("\t%s Object;"), *BaseName));
			AppendLine(Source, TEXT("\treturn 47;"));
			AppendLine(Source, TEXT("}"));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("int ProbeGlobalAccess%s(%s Object)"), *Suffix, *BaseName));
			AppendLine(Source, TEXT("{"));
			AppendAccessOperation(Source, Cell.Member, TEXT("Object."), TEXT("\t"));
			AppendLine(Source, TEXT("}"));
		}
		AppendLine(Source);
	}

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (IsConstructor(Cell.Member))
	{
		if (Cell.Site == EInhAccessSite::Owner)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s Owner;"), *BaseName));
			AppendLine(Source, FString::Printf(TEXT("\t%s Created = Owner.MakeOwner();"), *BaseName));
			AppendLine(Source, TEXT("\treturn 47;"));
		}
		else if (Cell.Site == EInhAccessSite::DirectDerived)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s Created;"), *DerivedName));
			AppendLine(Source, TEXT("\treturn 48;"));
		}
		else if (Cell.Site == EInhAccessSite::DeepDerived)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s Created;"), *DeepName));
			AppendLine(Source, TEXT("\treturn 48;"));
		}
		else if (Cell.Site == EInhAccessSite::Unrelated)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s Observer;"), *UnrelatedName));
			AppendLine(Source, TEXT("\treturn Observer.ProbeAccess();"));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\treturn ProbeGlobalAccess%s();"), *Suffix));
		}
	}
	else if (Cell.Site == EInhAccessSite::Owner)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Object;"), *BaseName));
		AppendLine(Source, TEXT("\treturn Object.ProbeAccess();"));
	}
	else if (Cell.Site == EInhAccessSite::DirectDerived)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Object;"), *DerivedName));
		AppendLine(Source, TEXT("\treturn Object.ProbeAccess();"));
	}
	else if (Cell.Site == EInhAccessSite::DeepDerived)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Object;"), *DeepName));
		AppendLine(Source, TEXT("\treturn Object.ProbeAccess();"));
	}
	else if (Cell.Site == EInhAccessSite::Unrelated)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Object;"), *BaseName));
		AppendLine(Source, FString::Printf(TEXT("\t%s Observer;"), *UnrelatedName));
		AppendLine(Source, TEXT("\treturn Observer.ProbeAccess(Object);"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Object;"), *BaseName));
		AppendLine(Source, FString::Printf(TEXT("\treturn ProbeGlobalAccess%s(Object);"), *Suffix));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FAccessCell& Cell, const FString& FunctionName)
{
	FString Source;
	AppendTypesAndEntry(Source, Cell, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FAccessCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (ShouldCompile(Cell))
	{
		Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.Execution = EGeneratedCaseExecution::Standalone;
		Info.ExpectedReturn = ExpectedFromCell(Cell);
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
	}
	return Info;
}
}

FStringView FInhAccessGenerator::GetProductId() const
{
	return TEXT("LANG-INH-ACCESS");
}

TArray<FGeneratedCaseInfo> FInhAccessGenerator::ListCases() const
{
	using namespace InhAccessGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FAccessCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("InhAccess PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FInhAccessGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace InhAccessGeneratorDetail;
	FAccessCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(Cell, MakeEntryName(Cell));
}

FString FInhAccessGenerator::BuildDumpSource() const
{
	using namespace InhAccessGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-INH-ACCESS"));
	AppendLine(Source, TEXT("// Cases: 60 total, 37 normal, 23 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));

	bool bNeedBlank = true;
	for (const FAccessCell& Cell : AllCells())
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
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendTypesAndEntry(Source, Cell, MakeEntryName(Cell));
		bNeedBlank = true;
	}

	for (const FAccessCell& Cell : AllCells())
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
		AppendTypesAndEntry(Source, Cell, MakeEntryName(Cell));
	}
	return Source;
}

FString FInhAccessGenerator::BuildInheritanceAccessSource(const FInhAccessParams& Params) const
{
	using namespace InhAccessGeneratorDetail;
	if (!IsValidParams(Params))
	{
		return FString();
	}
	FAccessCell Cell;
	Cell.Access = Params.Access;
	Cell.Member = Params.Member;
	Cell.Site = Params.Site;
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

FString FInhAccessGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace InhAccessGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FAccessCell& Cell : AllCells())
	{
		if (!ShouldCompile(Cell))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendTypesAndEntry(Source, Cell, MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FInhAccessGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace InhAccessGeneratorDetail;
	FAccessCell Cell;
	if (!TryFindCell(CaseId, Cell) || !ShouldCompile(Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FInhAccessGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace InhAccessGeneratorDetail;
	FAccessCell Cell;
	if (!TryFindCell(CaseId, Cell) || ShouldCompile(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(Cell, MakeEntryName(Cell));
}

TArray<FString> FInhAccessGenerator::ListRejectCaseIds() const
{
	using namespace InhAccessGeneratorDetail;
	TArray<FString> Ids;
	for (const FAccessCell& Cell : AllCells())
	{
		if (!ShouldCompile(Cell))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
