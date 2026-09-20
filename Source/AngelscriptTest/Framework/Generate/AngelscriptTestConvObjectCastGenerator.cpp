#include "Framework/Generate/AngelscriptTestConvObjectCastGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ConvObjectCastGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native MakeObjectCastBase, MakeObjectCastDerived, MakeObjectCastUnrelated, MakeObjectCastNull.");
constexpr int32 KindCount = 4;
constexpr int32 ViewCount = 3;
constexpr int32 FormCount = 5;
constexpr int32 TotalCells = KindCount * ViewCount * ViewCount * FormCount;

struct FObjectCastCell
{
	EConvObjectCastKind Kind = EConvObjectCastKind::Base;
	EConvObjectCastView Source = EConvObjectCastView::Base;
	EConvObjectCastView Target = EConvObjectCastView::Base;
	EConvObjectCastForm Form = EConvObjectCastForm::Assignment;
};

const TCHAR* KindToken(const EConvObjectCastKind Kind)
{
	switch (Kind)
	{
	case EConvObjectCastKind::Base:
		return TEXT("BASE");
	case EConvObjectCastKind::Derived:
		return TEXT("DERIVED");
	case EConvObjectCastKind::Unrelated:
		return TEXT("UNRELATED");
	case EConvObjectCastKind::Null:
		return TEXT("NULL");
	default:
		return nullptr;
	}
}

const TCHAR* ViewToken(const EConvObjectCastView View)
{
	switch (View)
	{
	case EConvObjectCastView::Base:
		return TEXT("BASE");
	case EConvObjectCastView::Derived:
		return TEXT("DERIVED");
	case EConvObjectCastView::Unrelated:
		return TEXT("UNRELATED");
	default:
		return nullptr;
	}
}

const TCHAR* FormToken(const EConvObjectCastForm Form)
{
	switch (Form)
	{
	case EConvObjectCastForm::Assignment:
		return TEXT("ASSIGNMENT");
	case EConvObjectCastForm::Initializer:
		return TEXT("INITIALIZER");
	case EConvObjectCastForm::Argument:
		return TEXT("ARGUMENT");
	case EConvObjectCastForm::Return:
		return TEXT("RETURN");
	case EConvObjectCastForm::ExplicitCast:
		return TEXT("EXPLICIT_CAST");
	default:
		return nullptr;
	}
}

const TCHAR* ViewScriptType(const EConvObjectCastView View)
{
	switch (View)
	{
	case EConvObjectCastView::Base:
		return TEXT("FObjectCastBase");
	case EConvObjectCastView::Derived:
		return TEXT("FObjectCastDerived");
	case EConvObjectCastView::Unrelated:
		return TEXT("FObjectCastUnrelated");
	default:
		return nullptr;
	}
}

const TCHAR* ViewSuffix(const EConvObjectCastView View)
{
	switch (View)
	{
	case EConvObjectCastView::Base:
		return TEXT("Base");
	case EConvObjectCastView::Derived:
		return TEXT("Derived");
	case EConvObjectCastView::Unrelated:
		return TEXT("Unrelated");
	default:
		return nullptr;
	}
}

const TCHAR* FactoryCall(const EConvObjectCastKind Kind)
{
	switch (Kind)
	{
	case EConvObjectCastKind::Base:
		return TEXT("MakeObjectCastBase()");
	case EConvObjectCastKind::Derived:
		return TEXT("MakeObjectCastDerived()");
	case EConvObjectCastKind::Unrelated:
		return TEXT("MakeObjectCastUnrelated()");
	case EConvObjectCastKind::Null:
	default:
		return TEXT("MakeObjectCastNull()");
	}
}

bool IsValidParams(const FConvObjectCastParams& Params)
{
	return KindToken(Params.Kind) != nullptr
		&& ViewToken(Params.Source) != nullptr
		&& ViewToken(Params.Target) != nullptr
		&& FormToken(Params.Form) != nullptr;
}

bool IsImplicitlyConvertible(const EConvObjectCastView Source, const EConvObjectCastView Target)
{
	return Source == Target || Target == EConvObjectCastView::Base;
}

bool ShouldCompile(const FConvObjectCastParams& Params)
{
	if (Params.Form == EConvObjectCastForm::ExplicitCast)
	{
		return IsImplicitlyConvertible(Params.Source, Params.Target)
			|| (Params.Source == EConvObjectCastView::Base && Params.Target != EConvObjectCastView::Base);
	}
	return IsImplicitlyConvertible(Params.Source, Params.Target);
}

bool IsVisibleThroughView(const EConvObjectCastKind Kind, const EConvObjectCastView View)
{
	if (Kind == EConvObjectCastKind::Null)
	{
		return false;
	}
	if (View == EConvObjectCastView::Base)
	{
		return true;
	}
	return (Kind == EConvObjectCastKind::Derived && View == EConvObjectCastView::Derived)
		|| (Kind == EConvObjectCastKind::Unrelated && View == EConvObjectCastView::Unrelated);
}

int32 ExpectedRuntimeResult(const FObjectCastCell& Cell)
{
	return IsVisibleThroughView(Cell.Kind, Cell.Source) && IsVisibleThroughView(Cell.Kind, Cell.Target)
		? (static_cast<int32>(Cell.Kind) + 1) * 1000 + 1
		: -1;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FObjectCastCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CONV-OBJECT-CAST-%s-%s-%s-%s"),
		KindToken(Cell.Kind),
		ViewToken(Cell.Source),
		ViewToken(Cell.Target),
		FormToken(Cell.Form));
}

FString MakeEntryName(const FObjectCastCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FConvObjectCastParams ParamsFromCell(const FObjectCastCell& Cell)
{
	FConvObjectCastParams Params;
	Params.Kind = Cell.Kind;
	Params.Source = Cell.Source;
	Params.Target = Cell.Target;
	Params.Form = Cell.Form;
	return Params;
}

FObjectCastCell CellFromIndex(const int32 Index)
{
	FObjectCastCell Cell;
	Cell.Kind = static_cast<EConvObjectCastKind>(Index / (ViewCount * ViewCount * FormCount));
	const int32 AfterKind = Index % (ViewCount * ViewCount * FormCount);
	Cell.Source = static_cast<EConvObjectCastView>(AfterKind / (ViewCount * FormCount));
	const int32 AfterSource = AfterKind % (ViewCount * FormCount);
	Cell.Target = static_cast<EConvObjectCastView>(AfterSource / FormCount);
	Cell.Form = static_cast<EConvObjectCastForm>(AfterSource % FormCount);
	return Cell;
}

bool TryFindCell(const FStringView CaseId, FObjectCastCell& OutCell)
{
	const FString Needle(CaseId);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FObjectCastCell Cell = CellFromIndex(Index);
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString SourceExpression(const EConvObjectCastView Source)
{
	return Source == EConvObjectCastView::Base
		? FString(TEXT("RuntimeValue"))
		: FString::Printf(TEXT("cast<%s>(RuntimeValue)"), ViewScriptType(Source));
}

FString ConvertedSource(const FConvObjectCastParams& Params)
{
	return Params.Form == EConvObjectCastForm::ExplicitCast
		? FString::Printf(TEXT("cast<%s>(SourceValue)"), ViewScriptType(Params.Target))
		: FString(TEXT("SourceValue"));
}

void AppendObserveHelper(FString& Source, const EConvObjectCastView Target)
{
	const TCHAR* const Type = ViewScriptType(Target);
	AppendLine(Source, FString::Printf(TEXT("int ObserveObjectCastTarget(%s Value)"), Type));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value == nullptr ? -1 : Value.GetField();"));
	AppendLine(Source, TEXT("}"));
}

void AppendReturnHelper(FString& Source, const EConvObjectCastView SourceView, const EConvObjectCastView TargetView)
{
	const TCHAR* const TargetType = ViewScriptType(TargetView);
	const TCHAR* const SourceType = ViewScriptType(SourceView);
	const TCHAR* const Suffix = ViewSuffix(TargetView);
	AppendLine(Source, FString::Printf(TEXT("%s ReturnObjectCast%s(%s Value)"), TargetType, Suffix, SourceType));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendSharedHelpers(FString& Source)
{
	for (int32 TargetIndex = 0; TargetIndex < ViewCount; ++TargetIndex)
	{
		if (TargetIndex > 0)
		{
			AppendLine(Source);
		}
		AppendObserveHelper(Source, static_cast<EConvObjectCastView>(TargetIndex));
	}
	for (int32 SourceIndex = 0; SourceIndex < ViewCount; ++SourceIndex)
	{
		for (int32 TargetIndex = 0; TargetIndex < ViewCount; ++TargetIndex)
		{
			const EConvObjectCastView SourceView = static_cast<EConvObjectCastView>(SourceIndex);
			const EConvObjectCastView TargetView = static_cast<EConvObjectCastView>(TargetIndex);
			if (!IsImplicitlyConvertible(SourceView, TargetView))
			{
				continue;
			}
			AppendLine(Source);
			AppendReturnHelper(Source, SourceView, TargetView);
		}
	}
}

void AppendOwnedHelpers(FString& Source, const FConvObjectCastParams& Params)
{
	AppendObserveHelper(Source, Params.Target);
	AppendLine(Source);
	if (Params.Form == EConvObjectCastForm::Return)
	{
		AppendReturnHelper(Source, Params.Source, Params.Target);
		AppendLine(Source);
	}
}

void AppendEntry(FString& Source, const FConvObjectCastParams& Params, const FString& FunctionName)
{
	const TCHAR* const SourceType = ViewScriptType(Params.Source);
	const TCHAR* const TargetType = ViewScriptType(Params.Target);
	const TCHAR* const TargetSuffix = ViewSuffix(Params.Target);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tFObjectCastBase RuntimeValue = %s;"), FactoryCall(Params.Kind)));
	AppendLine(Source, FString::Printf(TEXT("\t%s SourceValue = %s;"), SourceType, *SourceExpression(Params.Source)));
	if (Params.Form == EConvObjectCastForm::Assignment)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s TargetValue;"), TargetType));
		AppendLine(Source, TEXT("\tTargetValue = SourceValue;"));
	}
	else if (Params.Form == EConvObjectCastForm::Initializer)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s TargetValue = SourceValue;"), TargetType));
	}
	else if (Params.Form == EConvObjectCastForm::Argument)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveObjectCastTarget(%s);"), *ConvertedSource(Params)));
	}
	else if (Params.Form == EConvObjectCastForm::Return)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s TargetValue = ReturnObjectCast%s(SourceValue);"),
			TargetType,
			TargetSuffix));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s TargetValue = %s;"), TargetType, *ConvertedSource(Params)));
	}
	if (Params.Form != EConvObjectCastForm::Argument)
	{
		AppendLine(Source, TEXT("\tif (TargetValue == nullptr)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn -1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn TargetValue.GetIdentity() == SourceValue.GetIdentity() ? TargetValue.GetField() : -2;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FConvObjectCastParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FObjectCastCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (!ShouldCompile(ParamsFromCell(Cell)))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedRuntimeResult(Cell);
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FConvObjectCastGenerator::GetProductId() const
{
	return TEXT("LANG-CONV-OBJECT-CAST");
}

TArray<FGeneratedCaseInfo> FConvObjectCastGenerator::ListCases() const
{
	using namespace ConvObjectCastGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	Cases.Reserve(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FObjectCastCell Cell = CellFromIndex(Index);
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("ConvObjectCast PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FConvObjectCastGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ConvObjectCastGeneratorDetail;
	FObjectCastCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FConvObjectCastGenerator::BuildDumpSource() const
{
	using namespace ConvObjectCastGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CONV-OBJECT-CAST"));
	AppendLine(Source, TEXT("// Cases: 180 total, 108 normal, 72 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	bool bNeedBlank = false;
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FObjectCastCell Cell = CellFromIndex(Index);
		const FConvObjectCastParams Params = ParamsFromCell(Cell);
		if (!ShouldCompile(Params))
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
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, Params, MakeEntryName(Cell));
		bNeedBlank = true;
	}
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FObjectCastCell Cell = CellFromIndex(Index);
		if (ShouldCompile(ParamsFromCell(Cell)))
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
		AppendOwnedHelpers(Source, ParamsFromCell(Cell));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FConvObjectCastGenerator::BuildObjectCastSource(const FConvObjectCastParams& Params) const
{
	using namespace ConvObjectCastGeneratorDetail;
	if (!IsValidParams(Params) || !ShouldCompile(Params))
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

FString FConvObjectCastGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ConvObjectCastGeneratorDetail;
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FObjectCastCell Cell = CellFromIndex(Index);
		const FConvObjectCastParams Params = ParamsFromCell(Cell);
		if (!ShouldCompile(Params))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, Params, MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FConvObjectCastGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ConvObjectCastGeneratorDetail;
	FObjectCastCell Cell;
	if (!TryFindCell(CaseId, Cell) || !ShouldCompile(ParamsFromCell(Cell)))
	{
		return 0;
	}
	return ExpectedRuntimeResult(Cell);
}

FString FConvObjectCastGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ConvObjectCastGeneratorDetail;
	FObjectCastCell Cell;
	if (!TryFindCell(CaseId, Cell) || ShouldCompile(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FConvObjectCastGenerator::ListRejectCaseIds() const
{
	using namespace ConvObjectCastGeneratorDetail;
	TArray<FString> Ids;
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FObjectCastCell Cell = CellFromIndex(Index);
		if (!ShouldCompile(ParamsFromCell(Cell)))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
