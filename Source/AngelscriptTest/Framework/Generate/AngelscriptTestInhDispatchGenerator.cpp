#include "Framework/Generate/AngelscriptTestInhDispatchGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace InhDispatchGeneratorDetail
{
struct FDispatchCell
{
	EInhDispatchDepth Depth = EInhDispatchDepth::Base;
	EInhDispatchMember Member = EInhDispatchMember::Field;
	EInhDispatchView View = EInhDispatchView::DerivedObject;
	EInhDispatchInvocation Invocation = EInhDispatchInvocation::Direct;
};

const TCHAR* DepthToken(const EInhDispatchDepth Depth)
{
	switch (Depth)
	{
	case EInhDispatchDepth::Base:
		return TEXT("BASE");
	case EInhDispatchDepth::TwoLevels:
		return TEXT("TWO_LEVELS");
	case EInhDispatchDepth::ThreeLevels:
		return TEXT("THREE_LEVELS");
	case EInhDispatchDepth::Deep:
		return TEXT("DEEP");
	default:
		return nullptr;
	}
}

const TCHAR* MemberToken(const EInhDispatchMember Member)
{
	switch (Member)
	{
	case EInhDispatchMember::Field:
		return TEXT("FIELD");
	case EInhDispatchMember::NonvirtualMethod:
		return TEXT("NONVIRTUAL_METHOD");
	case EInhDispatchMember::VirtualMethod:
		return TEXT("VIRTUAL_METHOD");
	case EInhDispatchMember::Override:
		return TEXT("OVERRIDE");
	case EInhDispatchMember::GetterMethod:
		return TEXT("GETTER_METHOD");
	case EInhDispatchMember::SetterMethod:
		return TEXT("SETTER_METHOD");
	default:
		return nullptr;
	}
}

const TCHAR* ViewToken(const EInhDispatchView View)
{
	switch (View)
	{
	case EInhDispatchView::DerivedObject:
		return TEXT("DERIVED_OBJECT");
	case EInhDispatchView::BaseView:
		return TEXT("BASE_VIEW");
	case EInhDispatchView::ExplicitBase:
		return TEXT("EXPLICIT_BASE");
	case EInhDispatchView::Owner:
		return TEXT("OWNER");
	case EInhDispatchView::DerivedImpl:
		return TEXT("DERIVED_IMPL");
	default:
		return nullptr;
	}
}

const TCHAR* InvocationToken(const EInhDispatchInvocation Invocation)
{
	switch (Invocation)
	{
	case EInhDispatchInvocation::Direct:
		return TEXT("DIRECT");
	case EInhDispatchInvocation::VirtualRoute:
		return TEXT("VIRTUAL_ROUTE");
	case EInhDispatchInvocation::ExplicitBase:
		return TEXT("EXPLICIT_BASE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FInhDispatchParams& Params)
{
	return DepthToken(Params.Depth) != nullptr
		&& MemberToken(Params.Member) != nullptr
		&& ViewToken(Params.View) != nullptr
		&& InvocationToken(Params.Invocation) != nullptr;
}

int32 BaseEdges(const EInhDispatchDepth Depth)
{
	switch (Depth)
	{
	case EInhDispatchDepth::TwoLevels:
		return 2;
	case EInhDispatchDepth::ThreeLevels:
		return 3;
	case EInhDispatchDepth::Deep:
		return 8;
	case EInhDispatchDepth::Base:
	default:
		return 1;
	}
}

bool UsesExplicitBase(const FDispatchCell& Cell)
{
	return Cell.View == EInhDispatchView::ExplicitBase
		|| Cell.Invocation == EInhDispatchInvocation::ExplicitBase;
}

bool UsesBaseView(const FDispatchCell& Cell)
{
	return !UsesExplicitBase(Cell)
		&& (Cell.View == EInhDispatchView::BaseView
			|| Cell.Invocation == EInhDispatchInvocation::VirtualRoute);
}

int32 ExpectedFromCell(const FDispatchCell& Cell)
{
	const int32 Level = BaseEdges(Cell.Depth);
	if (Cell.Member == EInhDispatchMember::NonvirtualMethod)
	{
		return 100;
	}
	if (UsesExplicitBase(Cell))
	{
		if (Cell.Member == EInhDispatchMember::Field)
		{
			return 701;
		}
		if (Cell.Member == EInhDispatchMember::SetterMethod)
		{
			return 107;
		}
		return 100;
	}
	if (Cell.Member == EInhDispatchMember::Field)
	{
		if (Cell.Invocation == EInhDispatchInvocation::VirtualRoute)
		{
			return 701 + Level * 10;
		}
		return 701;
	}
	if (Cell.Member == EInhDispatchMember::SetterMethod)
	{
		return 100 + Level * 100 + 7;
	}
	return 100 + Level * 100;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FDispatchCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-INH-DISPATCH-%s-%s-%s-%s"),
		DepthToken(Cell.Depth),
		MemberToken(Cell.Member),
		ViewToken(Cell.View),
		InvocationToken(Cell.Invocation));
}

FString MakeEntryName(const FDispatchCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString TypeSuffix(const FDispatchCell& Cell)
{
	return MakeCanonicalEntryName(
		FString::Printf(
			TEXT("%s-%s-%s-%s"),
			DepthToken(Cell.Depth),
			MemberToken(Cell.Member),
			ViewToken(Cell.View),
			InvocationToken(Cell.Invocation)))
		.Mid(5);
}

FInhDispatchParams ParamsFromCell(const FDispatchCell& Cell)
{
	FInhDispatchParams Params;
	Params.Depth = Cell.Depth;
	Params.Member = Cell.Member;
	Params.View = Cell.View;
	Params.Invocation = Cell.Invocation;
	return Params;
}

TArray<FDispatchCell> AllCells()
{
	TArray<FDispatchCell> Cells;
	Cells.SetNum(360);
	for (int32 Index = 0; Index < 360; ++Index)
	{
		FDispatchCell& Cell = Cells[Index];
		Cell.Depth = static_cast<EInhDispatchDepth>(Index / 90);
		Cell.Member = static_cast<EInhDispatchMember>((Index / 15) % 6);
		Cell.View = static_cast<EInhDispatchView>((Index / 3) % 5);
		Cell.Invocation = static_cast<EInhDispatchInvocation>(Index % 3);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FDispatchCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FDispatchCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

bool IsOverriddenMember(const EInhDispatchMember Member)
{
	return Member != EInhDispatchMember::NonvirtualMethod;
}

FString TypeNameForLevel(const int32 Level, const int32 Edges, const FString& Suffix)
{
	if (Level == 0)
	{
		return TEXT("FDispatchRoot") + Suffix;
	}
	if (Level == Edges)
	{
		return TEXT("FDispatchPrimary") + Suffix;
	}
	return FString::Printf(TEXT("FDispatchLevel%d%s"), Level, *Suffix);
}

FString MethodName(const EInhDispatchMember Member)
{
	switch (Member)
	{
	case EInhDispatchMember::Field:
		return TEXT("ReadDispatchField");
	case EInhDispatchMember::NonvirtualMethod:
		return TEXT("StableValue");
	case EInhDispatchMember::VirtualMethod:
		return TEXT("VirtualValue");
	case EInhDispatchMember::Override:
		return TEXT("OverrideValue");
	case EInhDispatchMember::GetterMethod:
		return TEXT("GetValue");
	case EInhDispatchMember::SetterMethod:
		return TEXT("SetValue");
	default:
		return FString();
	}
}

FString MethodCall(const EInhDispatchMember Member, const FString& Prefix)
{
	if (Member == EInhDispatchMember::SetterMethod)
	{
		return Prefix + MethodName(Member) + TEXT("(7)");
	}
	return Prefix + MethodName(Member) + TEXT("()");
}

FString DirectMemberExpression(const EInhDispatchMember Member, const FString& Prefix)
{
	if (Member == EInhDispatchMember::Field)
	{
		return Prefix + TEXT("DispatchField");
	}
	return MethodCall(Member, Prefix);
}

FString ProbeExpression(const FDispatchCell& Cell, const FString& RootName, const bool bInsideMember)
{
	if (UsesExplicitBase(Cell))
	{
		return MethodCall(Cell.Member, RootName + TEXT("::"));
	}
	if (UsesBaseView(Cell))
	{
		return Cell.Invocation == EInhDispatchInvocation::VirtualRoute
			? MethodCall(Cell.Member, TEXT("BaseView."))
			: DirectMemberExpression(Cell.Member, TEXT("BaseView."));
	}
	if (bInsideMember)
	{
		return DirectMemberExpression(Cell.Member, FString());
	}
	return DirectMemberExpression(Cell.Member, TEXT("Object."));
}

void AppendRootTarget(FString& Source, const EInhDispatchMember Member)
{
	if (Member == EInhDispatchMember::Field)
	{
		AppendLine(Source, TEXT("\tint DispatchField = 701;"));
		AppendLine(Source);
	}
	if (Member == EInhDispatchMember::SetterMethod)
	{
		AppendLine(Source, TEXT("\tint SetValue(int InValue)"));
	}
	else
	{
		AppendLine(
			Source,
			FString::Printf(
				TEXT("\tint %s()%s"),
				*MethodName(Member),
				Member == EInhDispatchMember::NonvirtualMethod ? TEXT(" final") : TEXT("")));
	}
	AppendLine(Source, TEXT("\t{"));
	if (Member == EInhDispatchMember::Field)
	{
		AppendLine(Source, TEXT("\t\treturn DispatchField;"));
	}
	else if (Member == EInhDispatchMember::SetterMethod)
	{
		AppendLine(Source, TEXT("\t\treturn 100 + InValue;"));
	}
	else
	{
		AppendLine(Source, TEXT("\t\treturn 100;"));
	}
	AppendLine(Source, TEXT("\t}"));
}

void AppendDerivedTarget(FString& Source, const EInhDispatchMember Member, const int32 Level)
{
	if (!IsOverriddenMember(Member))
	{
		return;
	}
	const bool bExplicitOverride = Member != EInhDispatchMember::VirtualMethod;
	if (Member == EInhDispatchMember::SetterMethod)
	{
		AppendLine(
			Source,
			FString::Printf(TEXT("\tint SetValue(int InValue)%s"), bExplicitOverride ? TEXT(" override") : TEXT("")));
	}
	else
	{
		AppendLine(
			Source,
			FString::Printf(
				TEXT("\tint %s()%s"),
				*MethodName(Member),
				bExplicitOverride ? TEXT(" override") : TEXT("")));
	}
	AppendLine(Source, TEXT("\t{"));
	if (Member == EInhDispatchMember::Field)
	{
		AppendLine(Source, FString::Printf(TEXT("\t\treturn DispatchField + %d;"), Level * 10));
	}
	else if (Member == EInhDispatchMember::SetterMethod)
	{
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %d + InValue;"), 100 + Level * 100));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), 100 + Level * 100));
	}
	AppendLine(Source, TEXT("\t}"));
}

void AppendMemberProbe(FString& Source, const FDispatchCell& Cell, const FString& RootName)
{
	AppendLine(Source, TEXT("\tint ProbeMember()"));
	AppendLine(Source, TEXT("\t{"));
	if (UsesBaseView(Cell))
	{
		AppendLine(Source, FString::Printf(TEXT("\t\t%s BaseView = this;"), *RootName));
	}
	AppendLine(Source, TEXT("\t\treturn ") + ProbeExpression(Cell, RootName, true) + TEXT(";"));
	AppendLine(Source, TEXT("\t}"));
}

void AppendTypesAndEntry(FString& Source, const FDispatchCell& Cell, const FString& FunctionName)
{
	const FString Suffix = TypeSuffix(Cell);
	const int32 Edges = BaseEdges(Cell.Depth);
	const FString RootName = TypeNameForLevel(0, Edges, Suffix);
	const FString PrimaryName = TypeNameForLevel(Edges, Edges, Suffix);
	for (int32 Level = 0; Level <= Edges; ++Level)
	{
		const FString TypeName = TypeNameForLevel(Level, Edges, Suffix);
		if (Level == 0)
		{
			AppendLine(Source, FString::Printf(TEXT("class %s"), *TypeName));
		}
		else
		{
			AppendLine(
				Source,
				FString::Printf(TEXT("class %s : %s"), *TypeName, *TypeNameForLevel(Level - 1, Edges, Suffix)));
		}
		AppendLine(Source, TEXT("{"));
		if (Level == 0)
		{
			AppendRootTarget(Source, Cell.Member);
		}
		else
		{
			AppendDerivedTarget(Source, Cell.Member, Level);
		}
		const bool bAddOwnerProbe = Level == 0 && Cell.View == EInhDispatchView::Owner;
		const bool bAddDerivedProbe =
			Level == Edges && (UsesExplicitBase(Cell) || Cell.View == EInhDispatchView::DerivedImpl);
		if (bAddOwnerProbe || bAddDerivedProbe)
		{
			AppendLine(Source);
			AppendMemberProbe(Source, Cell, RootName);
		}
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}

	const bool bOwner = Cell.View == EInhDispatchView::Owner;
	const bool bBaseParam = Cell.View == EInhDispatchView::BaseView
		&& Cell.Invocation != EInhDispatchInvocation::ExplicitBase;
	const FString ParamType = (bOwner || bBaseParam) ? RootName : PrimaryName;
	AppendLine(Source, FString::Printf(TEXT("int ProbeDispatch%s(%s Object)"), *Suffix, *ParamType));
	AppendLine(Source, TEXT("{"));
	if (bOwner || UsesExplicitBase(Cell) || Cell.View == EInhDispatchView::DerivedImpl)
	{
		AppendLine(Source, TEXT("\treturn Object.ProbeMember();"));
	}
	else
	{
		if (UsesBaseView(Cell))
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s BaseView = Object;"), *RootName));
		}
		AppendLine(Source, TEXT("\treturn ") + ProbeExpression(Cell, RootName, false) + TEXT(";"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Object;"), *PrimaryName));
	AppendLine(Source, FString::Printf(TEXT("\treturn ProbeDispatch%s(Object);"), *Suffix));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FDispatchCell& Cell, const FString& FunctionName)
{
	FString Source;
	AppendTypesAndEntry(Source, Cell, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FDispatchCell& Cell)
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

FStringView FInhDispatchGenerator::GetProductId() const
{
	return TEXT("LANG-INH-DISPATCH");
}

TArray<FGeneratedCaseInfo> FInhDispatchGenerator::ListCases() const
{
	using namespace InhDispatchGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FDispatchCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("InhDispatch PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FInhDispatchGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace InhDispatchGeneratorDetail;
	FDispatchCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(Cell, MakeEntryName(Cell));
}

FString FInhDispatchGenerator::BuildDumpSource() const
{
	using namespace InhDispatchGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-INH-DISPATCH"));
	AppendLine(Source, TEXT("// Cases: 360 total, 360 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (const FDispatchCell& Cell : AllCells())
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

FString FInhDispatchGenerator::BuildInheritanceDispatchSource(const FInhDispatchParams& Params) const
{
	using namespace InhDispatchGeneratorDetail;
	if (!IsValidParams(Params))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	FDispatchCell Cell;
	Cell.Depth = Params.Depth;
	Cell.Member = Params.Member;
	Cell.View = Params.View;
	Cell.Invocation = Params.Invocation;
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	return BuildOwnedSource(Cell, FunctionName);
}

FString FInhDispatchGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace InhDispatchGeneratorDetail;
	const TArray<FDispatchCell> Cells = AllCells();
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

int32 FInhDispatchGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace InhDispatchGeneratorDetail;
	FDispatchCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
