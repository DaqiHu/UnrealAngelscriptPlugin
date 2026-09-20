#include "Framework/Generate/AngelscriptTestInhOverrideSignatureGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace InhOverrideSignatureGeneratorDetail
{
struct FOverrideCell
{
	EInhOverrideSignatureDimension Dimension = EInhOverrideSignatureDimension::ParameterType;
	EInhOverrideSignatureVariant Variant = EInhOverrideSignatureVariant::Exact;
	EInhOverrideSignatureView View = EInhOverrideSignatureView::Base;
};

struct FSignatureShape
{
	FString BaseAccess;
	FString BaseDeclaration = TEXT("int Resolve(int Value)");
	FString BaseArgument = TEXT("1");
	FString DerivedAccess;
	FString DerivedDeclaration = TEXT("int Resolve(int Value) override");
	FString DerivedArgument = TEXT("1");
	FString DerivedResultExpression = TEXT("Resolve(1)");
	bool bBuildAccepted = true;
};

const TCHAR* DimensionToken(const EInhOverrideSignatureDimension Dimension)
{
	switch (Dimension)
	{
	case EInhOverrideSignatureDimension::ParameterType:
		return TEXT("PARAMETER_TYPE");
	case EInhOverrideSignatureDimension::ParameterCount:
		return TEXT("PARAMETER_COUNT");
	case EInhOverrideSignatureDimension::ReturnType:
		return TEXT("RETURN_TYPE");
	case EInhOverrideSignatureDimension::Constness:
		return TEXT("CONSTNESS");
	case EInhOverrideSignatureDimension::Visibility:
		return TEXT("VISIBILITY");
	case EInhOverrideSignatureDimension::NameHiding:
		return TEXT("NAME_HIDING");
	default:
		return nullptr;
	}
}

const TCHAR* VariantToken(const EInhOverrideSignatureVariant Variant)
{
	switch (Variant)
	{
	case EInhOverrideSignatureVariant::Exact:
		return TEXT("EXACT");
	case EInhOverrideSignatureVariant::CompatibleOverload:
		return TEXT("COMPATIBLE_OVERLOAD");
	case EInhOverrideSignatureVariant::Incompatible:
		return TEXT("INCOMPATIBLE");
	default:
		return nullptr;
	}
}

const TCHAR* ViewToken(const EInhOverrideSignatureView View)
{
	switch (View)
	{
	case EInhOverrideSignatureView::Base:
		return TEXT("BASE");
	case EInhOverrideSignatureView::Derived:
		return TEXT("DERIVED");
	case EInhOverrideSignatureView::ExplicitBase:
		return TEXT("EXPLICIT_BASE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FInhOverrideSignatureParams& Params)
{
	return DimensionToken(Params.Dimension) != nullptr
		&& VariantToken(Params.Variant) != nullptr
		&& ViewToken(Params.View) != nullptr;
}

FSignatureShape MakeSignatureShape(
	const EInhOverrideSignatureDimension Dimension,
	const EInhOverrideSignatureVariant Variant)
{
	FSignatureShape Shape;
	if (Dimension == EInhOverrideSignatureDimension::Constness)
	{
		Shape.BaseDeclaration = TEXT("int Resolve(int Value) const");
		Shape.DerivedDeclaration = TEXT("int Resolve(int Value) const override");
	}
	else if (Dimension == EInhOverrideSignatureDimension::Visibility)
	{
		Shape.BaseAccess = TEXT("protected ");
		Shape.DerivedAccess = TEXT("protected ");
	}

	if (Variant == EInhOverrideSignatureVariant::Exact)
	{
		return Shape;
	}

	if (Dimension == EInhOverrideSignatureDimension::ParameterType)
	{
		Shape.DerivedDeclaration = Variant == EInhOverrideSignatureVariant::CompatibleOverload
			? FString(TEXT("int Resolve(float Value)"))
			: FString(TEXT("int Resolve(float Value) override"));
		Shape.DerivedArgument = TEXT("1.5f");
		Shape.DerivedResultExpression = TEXT("Resolve(1.5f)");
	}
	else if (Dimension == EInhOverrideSignatureDimension::ParameterCount)
	{
		Shape.DerivedDeclaration = Variant == EInhOverrideSignatureVariant::CompatibleOverload
			? FString(TEXT("int Resolve(int Left, int Right)"))
			: FString(TEXT("int Resolve(int Left, int Right) override"));
		Shape.DerivedArgument = TEXT("1, 2");
		Shape.DerivedResultExpression = TEXT("Resolve(1, 2)");
	}
	else if (Dimension == EInhOverrideSignatureDimension::ReturnType)
	{
		Shape.DerivedDeclaration = Variant == EInhOverrideSignatureVariant::CompatibleOverload
			? FString(TEXT("float Resolve(float Value)"))
			: FString(TEXT("float Resolve(int Value) override"));
		if (Variant == EInhOverrideSignatureVariant::CompatibleOverload)
		{
			Shape.DerivedArgument = TEXT("1.5f");
			Shape.DerivedResultExpression = TEXT("int(Resolve(1.5f))");
		}
		else
		{
			Shape.DerivedResultExpression = TEXT("int(Resolve(1))");
		}
	}
	else if (Dimension == EInhOverrideSignatureDimension::Constness)
	{
		Shape.DerivedDeclaration = Variant == EInhOverrideSignatureVariant::CompatibleOverload
			? FString(TEXT("int Resolve(int Value)"))
			: FString(TEXT("int Resolve(int Value) override"));
	}
	else if (Dimension == EInhOverrideSignatureDimension::Visibility)
	{
		if (Variant == EInhOverrideSignatureVariant::CompatibleOverload)
		{
			Shape.DerivedAccess.Reset();
			Shape.DerivedDeclaration = TEXT("int Resolve(float Value)");
			Shape.DerivedArgument = TEXT("1.5f");
			Shape.DerivedResultExpression = TEXT("Resolve(1.5f)");
		}
		else
		{
			Shape.BaseAccess = TEXT("private ");
			Shape.DerivedAccess.Reset();
			Shape.DerivedDeclaration = TEXT("int Resolve(int Value)");
		}
	}
	else
	{
		if (Variant == EInhOverrideSignatureVariant::CompatibleOverload)
		{
			Shape.DerivedDeclaration = TEXT("int Resolve(float Value)");
			Shape.DerivedArgument = TEXT("2.5f");
			Shape.DerivedResultExpression = TEXT("Resolve(2.5f)");
		}
		else
		{
			Shape.DerivedDeclaration = TEXT("int ResolveRenamed(int Value) override");
			Shape.DerivedResultExpression = TEXT("ResolveRenamed(1)");
		}
	}

	if (Variant == EInhOverrideSignatureVariant::Incompatible)
	{
		Shape.bBuildAccepted = false;
	}
	return Shape;
}

bool IsReject(const FOverrideCell& Cell)
{
	return !MakeSignatureShape(Cell.Dimension, Cell.Variant).bBuildAccepted;
}

int32 ExpectedFromCell(const FOverrideCell& Cell)
{
	if (IsReject(Cell))
	{
		return 0;
	}
	if (Cell.View == EInhOverrideSignatureView::ExplicitBase)
	{
		return 102;
	}
	if (Cell.View == EInhOverrideSignatureView::Base)
	{
		return Cell.Variant == EInhOverrideSignatureVariant::Exact ? 203 : 102;
	}
	if (Cell.Variant == EInhOverrideSignatureVariant::CompatibleOverload)
	{
		if (Cell.Dimension == EInhOverrideSignatureDimension::ParameterCount)
		{
			return 205;
		}
		if (Cell.Dimension == EInhOverrideSignatureDimension::NameHiding)
		{
			return 204;
		}
	}
	return 203;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FOverrideCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-INH-OVERRIDE-SIGNATURE-%s-%s-%s"),
		DimensionToken(Cell.Dimension),
		VariantToken(Cell.Variant),
		ViewToken(Cell.View));
}

FString MakeEntryName(const FOverrideCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString TypeSuffix(const FOverrideCell& Cell)
{
	return MakeCanonicalEntryName(
		FString::Printf(
			TEXT("%s-%s-%s"),
			DimensionToken(Cell.Dimension),
			VariantToken(Cell.Variant),
			ViewToken(Cell.View)))
		.Mid(5);
}

FString DerivedReturnExpression(const FSignatureShape& Shape)
{
	if (Shape.DerivedDeclaration.StartsWith(TEXT("float ")))
	{
		return TEXT("202.0f + float(Value)");
	}
	if (Shape.DerivedDeclaration.Contains(TEXT("Left")))
	{
		return TEXT("202 + Left + Right");
	}
	return TEXT("202 + int(Value)");
}

TArray<FOverrideCell> AllCells()
{
	TArray<FOverrideCell> Cells;
	Cells.SetNum(54);
	for (int32 Index = 0; Index < 54; ++Index)
	{
		FOverrideCell& Cell = Cells[Index];
		Cell.Dimension = static_cast<EInhOverrideSignatureDimension>(Index / 9);
		Cell.Variant = static_cast<EInhOverrideSignatureVariant>((Index / 3) % 3);
		Cell.View = static_cast<EInhOverrideSignatureView>(Index % 3);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FOverrideCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FOverrideCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendMethod(
	FString& Source,
	const FString& Access,
	const FString& Declaration,
	const FString& ReturnExpression)
{
	AppendLine(Source, TEXT("\t") + Access + Declaration);
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn ") + ReturnExpression + TEXT(";"));
	AppendLine(Source, TEXT("\t}"));
}

void AppendTypesAndEntry(FString& Source, const FOverrideCell& Cell, const FString& FunctionName)
{
	const FSignatureShape Shape = MakeSignatureShape(Cell.Dimension, Cell.Variant);
	const FString Suffix = TypeSuffix(Cell);
	const FString BaseName = TEXT("FSignatureBase") + Suffix;
	const FString PrimaryName = TEXT("FSignaturePrimary") + Suffix;

	AppendLine(Source, FString::Printf(TEXT("class %s"), *BaseName));
	AppendLine(Source, TEXT("{"));
	AppendMethod(Source, Shape.BaseAccess, Shape.BaseDeclaration, TEXT("101 + Value"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint ProbeBase()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Resolve(") + Shape.BaseArgument + TEXT(");"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *PrimaryName, *BaseName));
	AppendLine(Source, TEXT("{"));
	AppendMethod(Source, Shape.DerivedAccess, Shape.DerivedDeclaration, DerivedReturnExpression(Shape));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint ProbeDerived()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn ") + Shape.DerivedResultExpression + TEXT(";"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint ProbeExplicitBase()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(
		Source,
		FString::Printf(TEXT("\t\treturn %s::Resolve(%s);"), *BaseName, *Shape.BaseArgument));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Object;"), *PrimaryName));
	if (Cell.View == EInhOverrideSignatureView::Base)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s BaseView = Object;"), *BaseName));
		AppendLine(Source, TEXT("\treturn BaseView.ProbeBase();"));
	}
	else if (Cell.View == EInhOverrideSignatureView::Derived)
	{
		AppendLine(Source, TEXT("\treturn Object.ProbeDerived();"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn Object.ProbeExplicitBase();"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOverrideCell& Cell, const FString& FunctionName)
{
	FString Source;
	AppendTypesAndEntry(Source, Cell, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FOverrideCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
	}
	else
	{
		Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.Execution = EGeneratedCaseExecution::Standalone;
		Info.ExpectedReturn = ExpectedFromCell(Cell);
	}
	return Info;
}
}

FStringView FInhOverrideSignatureGenerator::GetProductId() const
{
	return TEXT("LANG-INH-OVERRIDE-SIGNATURE");
}

TArray<FGeneratedCaseInfo> FInhOverrideSignatureGenerator::ListCases() const
{
	using namespace InhOverrideSignatureGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FOverrideCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("InhOverrideSignature PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FInhOverrideSignatureGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace InhOverrideSignatureGeneratorDetail;
	FOverrideCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(Cell, MakeEntryName(Cell));
}

FString FInhOverrideSignatureGenerator::BuildDumpSource() const
{
	using namespace InhOverrideSignatureGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-INH-OVERRIDE-SIGNATURE"));
	AppendLine(Source, TEXT("// Cases: 54 total, 36 normal, 18 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));

	bool bNeedBlank = true;
	for (const FOverrideCell& Cell : AllCells())
	{
		if (IsReject(Cell))
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

	for (const FOverrideCell& Cell : AllCells())
	{
		if (!IsReject(Cell))
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

FString FInhOverrideSignatureGenerator::BuildOverrideSignatureSource(const FInhOverrideSignatureParams& Params) const
{
	using namespace InhOverrideSignatureGeneratorDetail;
	if (!IsValidParams(Params))
	{
		return FString();
	}
	FOverrideCell Cell;
	Cell.Dimension = Params.Dimension;
	Cell.Variant = Params.Variant;
	Cell.View = Params.View;
	if (IsReject(Cell))
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

FString FInhOverrideSignatureGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace InhOverrideSignatureGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FOverrideCell& Cell : AllCells())
	{
		if (IsReject(Cell))
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

int32 FInhOverrideSignatureGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace InhOverrideSignatureGeneratorDetail;
	FOverrideCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FInhOverrideSignatureGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace InhOverrideSignatureGeneratorDetail;
	FOverrideCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(Cell, MakeEntryName(Cell));
}

TArray<FString> FInhOverrideSignatureGenerator::ListRejectCaseIds() const
{
	using namespace InhOverrideSignatureGeneratorDetail;
	TArray<FString> Ids;
	for (const FOverrideCell& Cell : AllCells())
	{
		if (IsReject(Cell))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
