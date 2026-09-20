#include "Framework/Generate/AngelscriptTestRefSourceOpGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace RefSourceOpGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs host FRefRoot fixtures and SameReference.");
constexpr const TCHAR* NullPointerException = TEXT("Null pointer access");

struct FRefSourceOpCell
{
	ERefSourceOpOperation Operation = ERefSourceOpOperation::Initialize;
	ERefSourceOpQualifier Qualifier = ERefSourceOpQualifier::Mutable;
	ERefSourceOpSource Source = ERefSourceOpSource::NewLocal;
};

const TCHAR* OperationToken(const ERefSourceOpOperation Operation)
{
	switch (Operation)
	{
	case ERefSourceOpOperation::Initialize:
		return TEXT("INITIALIZE");
	case ERefSourceOpOperation::Assign:
		return TEXT("ASSIGN");
	case ERefSourceOpOperation::Pass:
		return TEXT("PASS");
	case ERefSourceOpOperation::Return:
		return TEXT("RETURN");
	case ERefSourceOpOperation::Identity:
		return TEXT("IDENTITY");
	case ERefSourceOpOperation::NullCompare:
		return TEXT("NULL_COMPARE");
	case ERefSourceOpOperation::Cast:
		return TEXT("CAST");
	case ERefSourceOpOperation::MemberAccess:
		return TEXT("MEMBER_ACCESS");
	case ERefSourceOpOperation::AliasMutation:
		return TEXT("ALIAS_MUTATION");
	default:
		return nullptr;
	}
}

const TCHAR* QualifierToken(const ERefSourceOpQualifier Qualifier)
{
	switch (Qualifier)
	{
	case ERefSourceOpQualifier::Mutable:
		return TEXT("MUTABLE");
	case ERefSourceOpQualifier::ConstObject:
		return TEXT("CONST_OBJECT");
	case ERefSourceOpQualifier::ConstInput:
		return TEXT("CONST_INPUT");
	case ERefSourceOpQualifier::ConstRemovalInvalid:
		return TEXT("CONST_REMOVAL_INVALID");
	default:
		return nullptr;
	}
}

const TCHAR* SourceToken(const ERefSourceOpSource Source)
{
	switch (Source)
	{
	case ERefSourceOpSource::NewLocal:
		return TEXT("NEW_LOCAL");
	case ERefSourceOpSource::Field:
		return TEXT("FIELD");
	case ERefSourceOpSource::Parameter:
		return TEXT("PARAMETER");
	case ERefSourceOpSource::Return:
		return TEXT("RETURN");
	case ERefSourceOpSource::BaseView:
		return TEXT("BASE_VIEW");
	case ERefSourceOpSource::DerivedView:
		return TEXT("DERIVED_VIEW");
	case ERefSourceOpSource::NativeObject:
		return TEXT("NATIVE_OBJECT");
	case ERefSourceOpSource::Null:
		return TEXT("NULL");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FRefSourceOpParams& Params)
{
	return OperationToken(Params.Operation) != nullptr
		&& QualifierToken(Params.Qualifier) != nullptr
		&& SourceToken(Params.Source) != nullptr;
}

int32 SourceValue(const ERefSourceOpSource Source)
{
	switch (Source)
	{
	case ERefSourceOpSource::NewLocal:
		return 11;
	case ERefSourceOpSource::Field:
		return 12;
	case ERefSourceOpSource::Parameter:
		return 13;
	case ERefSourceOpSource::Return:
		return 14;
	case ERefSourceOpSource::BaseView:
		return 15;
	case ERefSourceOpSource::DerivedView:
		return 16;
	case ERefSourceOpSource::NativeObject:
		return 17;
	default:
		return 0;
	}
}

bool IsDerivedStaticView(const ERefSourceOpSource Source)
{
	return Source == ERefSourceOpSource::DerivedView;
}

bool IsDynamicallyDerived(const ERefSourceOpSource Source)
{
	return Source == ERefSourceOpSource::BaseView || Source == ERefSourceOpSource::DerivedView;
}

bool IsNullSource(const ERefSourceOpSource Source)
{
	return Source == ERefSourceOpSource::Null;
}

bool IsReject(const ERefSourceOpOperation Operation, const ERefSourceOpQualifier Qualifier)
{
	if (Qualifier == ERefSourceOpQualifier::ConstRemovalInvalid)
	{
		return true;
	}
	return Operation == ERefSourceOpOperation::AliasMutation && Qualifier != ERefSourceOpQualifier::Mutable;
}

bool IsFault(
	const ERefSourceOpSource Source,
	const ERefSourceOpOperation Operation,
	const ERefSourceOpQualifier Qualifier)
{
	if (IsReject(Operation, Qualifier))
	{
		return false;
	}
	return IsNullSource(Source)
		&& (Operation == ERefSourceOpOperation::MemberAccess
			|| Operation == ERefSourceOpOperation::Cast
			|| Operation == ERefSourceOpOperation::AliasMutation);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FRefSourceOpCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-REF-SOURCE-OP-%s-%s-%s"),
		OperationToken(Cell.Operation),
		QualifierToken(Cell.Qualifier),
		SourceToken(Cell.Source));
}

FString MakeEntryName(const FRefSourceOpCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FRefSourceOpParams ParamsFromCell(const FRefSourceOpCell& Cell)
{
	FRefSourceOpParams Params;
	Params.Operation = Cell.Operation;
	Params.Qualifier = Cell.Qualifier;
	Params.Source = Cell.Source;
	return Params;
}

TArray<FRefSourceOpCell> AllCells()
{
	TArray<FRefSourceOpCell> Cells;
	Cells.SetNum(288);
	for (int32 Index = 0; Index < 288; ++Index)
	{
		FRefSourceOpCell& Cell = Cells[Index];
		Cell.Operation = static_cast<ERefSourceOpOperation>(Index / 32);
		Cell.Qualifier = static_cast<ERefSourceOpQualifier>((Index / 8) % 4);
		Cell.Source = static_cast<ERefSourceOpSource>(Index % 8);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FRefSourceOpCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FRefSourceOpCell> Cells = AllCells();
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

FString StaticType(const ERefSourceOpSource Source)
{
	return IsDerivedStaticView(Source) ? FString(TEXT("FRefDerived")) : FString(TEXT("FRefRoot"));
}

FString LocalViewType(const ERefSourceOpSource Source, const ERefSourceOpQualifier Qualifier)
{
	return Qualifier == ERefSourceOpQualifier::Mutable
		? StaticType(Source)
		: TEXT("const ") + StaticType(Source);
}

FString ParameterDeclaration(const ERefSourceOpSource Source, const ERefSourceOpQualifier Qualifier)
{
	const FString Type = StaticType(Source);
	switch (Qualifier)
	{
	case ERefSourceOpQualifier::ConstObject:
		return TEXT("const ") + Type + TEXT(" Source");
	case ERefSourceOpQualifier::ConstInput:
	case ERefSourceOpQualifier::ConstRemovalInvalid:
		return TEXT("const ") + Type + TEXT("& in Source");
	default:
		return Type + TEXT(" Source");
	}
}

FString SourceExpression(const ERefSourceOpSource Source)
{
	const int32 Value = SourceValue(Source);
	switch (Source)
	{
	case ERefSourceOpSource::NewLocal:
		return FString::Printf(TEXT("MakeRefRoot(%d)"), Value);
	case ERefSourceOpSource::Field:
		return TEXT("Owner.Field");
	case ERefSourceOpSource::Parameter:
		return TEXT("ParameterSource");
	case ERefSourceOpSource::Return:
		return TEXT("ProvideReference()");
	case ERefSourceOpSource::BaseView:
		return FString::Printf(TEXT("MakeRefDerivedAsRoot(%d)"), Value);
	case ERefSourceOpSource::DerivedView:
		return FString::Printf(TEXT("MakeRefDerived(%d)"), Value);
	case ERefSourceOpSource::NativeObject:
		return FString::Printf(TEXT("GetNativeRef(%d)"), Value);
	case ERefSourceOpSource::Null:
		return TEXT("nullptr");
	default:
		return TEXT("nullptr");
	}
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int ObserveReference(const FRefRoot Object)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Object == nullptr"));
	AppendLine(Source, TEXT("\t\t? -1"));
	AppendLine(Source, TEXT("\t\t: Object.GetValue();"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FRefRoot ReturnReference(FRefRoot Object)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Object;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("const FRefRoot ReturnConstReference(const FRefRoot Object)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Object;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void RequireMutable(FRefRoot& inout Object)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tif (Object == nullptr)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tObject = MakeRefRoot(81);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\telse"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tObject.SetValue(82);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
}

void AppendFieldOwner(FString& Source)
{
	AppendLine(Source, TEXT("struct FReferenceFieldOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFRefRoot Field;"));
	AppendLine(Source, TEXT("}"));
}

void AppendProvideReference(FString& Source)
{
	AppendLine(Source, TEXT("FRefRoot ProvideReference()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn MakeRefRoot(14);"));
	AppendLine(Source, TEXT("}"));
}

void AppendOperation(FString& Source, const FRefSourceOpParams& Params)
{
	const FString Type = LocalViewType(Params.Source, Params.Qualifier);
	switch (Params.Operation)
	{
	case ERefSourceOpOperation::Initialize:
		AppendLine(Source, TEXT("\t") + Type + TEXT(" Alias = Source;"));
		AppendLine(Source, TEXT("\treturn SameReference(Source, Alias)"));
		AppendLine(Source, TEXT("\t\t? 1"));
		AppendLine(Source, TEXT("\t\t: 0;"));
		break;
	case ERefSourceOpOperation::Assign:
		AppendLine(
			Source,
			TEXT("\t") + Type
				+ (IsDerivedStaticView(Params.Source) ? TEXT(" Alias = MakeRefDerived(91);") : TEXT(" Alias = MakeRefRoot(91);")));
		AppendLine(Source, TEXT("\tAlias = Source;"));
		AppendLine(Source, TEXT("\treturn SameReference(Source, Alias)"));
		AppendLine(Source, TEXT("\t\t? 1"));
		AppendLine(Source, TEXT("\t\t: 0;"));
		break;
	case ERefSourceOpOperation::Pass:
		if (IsNullSource(Params.Source))
		{
			AppendLine(Source, TEXT("\treturn ObserveReference(Source) == -1 ? 1 : 0;"));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\treturn ObserveReference(Source) == %d ? 1 : 0;"), SourceValue(Params.Source)));
		}
		break;
	case ERefSourceOpOperation::Return:
		AppendLine(
			Source,
			Params.Qualifier == ERefSourceOpQualifier::Mutable
				? TEXT("\tFRefRoot Returned = ReturnReference(Source);")
				: TEXT("\tconst FRefRoot Returned = ReturnConstReference(Source);"));
		AppendLine(Source, TEXT("\treturn SameReference(Source, Returned)"));
		AppendLine(Source, TEXT("\t\t? 1"));
		AppendLine(Source, TEXT("\t\t: 0;"));
		break;
	case ERefSourceOpOperation::Identity:
		AppendLine(Source, TEXT("\t") + Type + TEXT(" Alias = Source;"));
		AppendLine(Source, TEXT("\treturn SameReference(Source, Alias)"));
		AppendLine(Source, TEXT("\t\t&& Source == Alias"));
		AppendLine(Source, TEXT("\t\t? 1"));
		AppendLine(Source, TEXT("\t\t: 0;"));
		break;
	case ERefSourceOpOperation::NullCompare:
		AppendLine(
			Source,
			IsNullSource(Params.Source)
				? TEXT("\treturn Source == nullptr ? 1 : 0;")
				: TEXT("\treturn Source != nullptr ? 1 : 0;"));
		break;
	case ERefSourceOpOperation::Cast:
		if (IsDerivedStaticView(Params.Source))
		{
			AppendLine(
				Source,
				Params.Qualifier == ERefSourceOpQualifier::Mutable
					? TEXT("\tFRefRoot Casted = Source.opImplCast();")
					: TEXT("\tconst FRefRoot Casted = Source.opImplCast();"));
			AppendLine(Source, TEXT("\treturn Casted != nullptr"));
			AppendLine(Source, TEXT("\t\t&& Casted.GetKind() == 202"));
			AppendLine(Source, TEXT("\t\t&& SameReference(Source, Casted)"));
			AppendLine(Source, TEXT("\t\t? 1"));
			AppendLine(Source, TEXT("\t\t: 0;"));
		}
		else
		{
			AppendLine(
				Source,
				Params.Qualifier == ERefSourceOpQualifier::Mutable
					? TEXT("\tFRefDerived Casted = Source.opCast();")
					: TEXT("\tconst FRefDerived Casted = Source.opCast();"));
			if (IsDynamicallyDerived(Params.Source))
			{
				AppendLine(Source, TEXT("\treturn Casted != nullptr"));
				AppendLine(Source, TEXT("\t\t&& Casted.GetKind() == 202"));
				AppendLine(Source, TEXT("\t\t&& SameReference(Source, Casted)"));
				AppendLine(Source, TEXT("\t\t? 1"));
				AppendLine(Source, TEXT("\t\t: 0;"));
			}
			else
			{
				AppendLine(Source, TEXT("\treturn Casted == nullptr ? 1 : 0;"));
			}
		}
		break;
	case ERefSourceOpOperation::MemberAccess:
		AppendLine(Source, FString::Printf(TEXT("\treturn Source.GetValue() == %d ? 1 : 0;"), SourceValue(Params.Source)));
		break;
	case ERefSourceOpOperation::AliasMutation:
		AppendLine(Source, TEXT("\t") + Type + TEXT(" Alias = Source;"));
		AppendLine(Source, TEXT("\tAlias.SetValue(77);"));
		AppendLine(Source, TEXT("\treturn Source.GetValue() == 77"));
		AppendLine(Source, TEXT("\t\t&& SameReference(Source, Alias)"));
		AppendLine(Source, TEXT("\t\t? 1"));
		AppendLine(Source, TEXT("\t\t: 0;"));
		break;
	default:
		AppendLine(Source, TEXT("\treturn 0;"));
		break;
	}
}

FString ExerciseName(const FString& FunctionName)
{
	return TEXT("Exercise") + FunctionName;
}

void AppendExercise(FString& Source, const FRefSourceOpParams& Params, const FString& FunctionName)
{
	AppendLine(
		Source,
		FString::Printf(
			TEXT("int %s(%s)"),
			*ExerciseName(FunctionName),
			*ParameterDeclaration(Params.Source, Params.Qualifier)));
	AppendLine(Source, TEXT("{"));
	AppendOperation(Source, Params);
	if (Params.Qualifier == ERefSourceOpQualifier::ConstRemovalInvalid)
	{
		AppendLine(Source, TEXT("\tRequireMutable(Source);"));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FRefSourceOpParams& Params, const FString& FunctionName)
{
	AppendExercise(Source, Params, FunctionName);
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Source == ERefSourceOpSource::Field)
	{
		AppendLine(Source, TEXT("\tFReferenceFieldOwner Owner;"));
		AppendLine(Source, FString::Printf(TEXT("\tOwner.Field = MakeRefRoot(%d);"), SourceValue(Params.Source)));
	}
	else if (Params.Source == ERefSourceOpSource::Parameter)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFRefRoot ParameterSource = MakeRefRoot(%d);"), SourceValue(Params.Source)));
	}
	AppendLine(
		Source,
		FString::Printf(TEXT("\treturn %s(%s);"), *ExerciseName(FunctionName), *SourceExpression(Params.Source)));
	AppendLine(Source, TEXT("}"));
}

void AppendIsolatedProviders(FString& Source, const ERefSourceOpSource SourceKind)
{
	if (SourceKind == ERefSourceOpSource::Field)
	{
		AppendFieldOwner(Source);
		AppendLine(Source);
	}
	if (SourceKind == ERefSourceOpSource::Return)
	{
		AppendProvideReference(Source);
		AppendLine(Source);
	}
}

FString BuildOwnedSource(const FRefSourceOpParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendIsolatedProviders(Source, Params.Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FRefSourceOpCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Operation, Cell.Qualifier))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	if (IsFault(Cell.Source, Cell.Operation, Cell.Qualifier))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = NullPointerException;
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = 1;
	}
	return Info;
}
}

FStringView FRefSourceOpGenerator::GetProductId() const
{
	return TEXT("LANG-REF-SOURCE-OP");
}

TArray<FGeneratedCaseInfo> FRefSourceOpGenerator::ListCases() const
{
	using namespace RefSourceOpGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FRefSourceOpCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("RefSourceOp PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FRefSourceOpGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace RefSourceOpGeneratorDetail;
	FRefSourceOpCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	if (IsReject(Cell.Operation, Cell.Qualifier))
	{
		return BuildRejectSource(CaseId);
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FRefSourceOpGenerator::BuildDumpSource() const
{
	using namespace RefSourceOpGeneratorDetail;
	const TArray<FRefSourceOpCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-REF-SOURCE-OP"));
	AppendLine(Source, TEXT("// Cases: 288 total, 193 normal, 88 reject, 7 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendFieldOwner(Source);
	AppendLine(Source);
	AppendProvideReference(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Operation, Cells[Index].Qualifier))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cells[Index].Source, Cells[Index].Operation, Cells[Index].Qualifier))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedException: %s"), NullPointerException));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, TEXT("// ExpectedReturn: 1"));
		}
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), HostNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsReject(Cells[Index].Operation, Cells[Index].Qualifier))
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
		AppendSharedHelpers(Source);
		AppendLine(Source);
		AppendIsolatedProviders(Source, Cells[Index].Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FRefSourceOpGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace RefSourceOpGeneratorDetail;
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendFieldOwner(Source);
	AppendLine(Source);
	AppendProvideReference(Source);
	int32 Emitted = 0;
	const TArray<FRefSourceOpCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Operation, Cells[Index].Qualifier))
		{
			continue;
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FRefSourceOpGenerator::BuildReferenceIdentitySource(const FRefSourceOpParams& Params) const
{
	using namespace RefSourceOpGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Operation, Params.Qualifier))
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

int32 FRefSourceOpGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace RefSourceOpGeneratorDetail;
	FRefSourceOpCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Operation, Cell.Qualifier) || IsFault(Cell.Source, Cell.Operation, Cell.Qualifier))
	{
		return 0;
	}
	return 1;
}

FString FRefSourceOpGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace RefSourceOpGeneratorDetail;
	FRefSourceOpCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Operation, Cell.Qualifier))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FRefSourceOpGenerator::ListRejectCaseIds() const
{
	using namespace RefSourceOpGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FRefSourceOpCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Operation, Cells[Index].Qualifier))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
