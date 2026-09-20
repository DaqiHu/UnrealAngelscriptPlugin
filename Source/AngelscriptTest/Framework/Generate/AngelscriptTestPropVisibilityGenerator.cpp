#include "Framework/Generate/AngelscriptTestPropVisibilityGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace PropVisibilityGeneratorDetail
{
constexpr int32 PathCount = 10;
constexpr int32 OperationCount = 2;
constexpr int32 VisibilityCount = 3;
constexpr int32 TotalCells = PathCount * OperationCount * VisibilityCount;

struct FVisibilityCell
{
	EPropVisibilityAccessPath AccessPath = EPropVisibilityAccessPath::OwnerMethod;
	EPropVisibilityOperation Operation = EPropVisibilityOperation::Read;
	EPropVisibilityVisibility Visibility = EPropVisibilityVisibility::Default;
};

const TCHAR* PathToken(const EPropVisibilityAccessPath AccessPath)
{
	switch (AccessPath)
	{
	case EPropVisibilityAccessPath::OwnerMethod:
		return TEXT("OWNER_METHOD");
	case EPropVisibilityAccessPath::OwnerConstructor:
		return TEXT("OWNER_CONSTRUCTOR");
	case EPropVisibilityAccessPath::OwnerDestructor:
		return TEXT("OWNER_DESTRUCTOR");
	case EPropVisibilityAccessPath::AccessorBody:
		return TEXT("ACCESSOR_BODY");
	case EPropVisibilityAccessPath::DirectDerived:
		return TEXT("DIRECT_DERIVED");
	case EPropVisibilityAccessPath::DeepDerived:
		return TEXT("DEEP_DERIVED");
	case EPropVisibilityAccessPath::UnrelatedType:
		return TEXT("UNRELATED_TYPE");
	case EPropVisibilityAccessPath::GlobalSameModule:
		return TEXT("GLOBAL_SAME_MODULE");
	case EPropVisibilityAccessPath::BaseViewFromDerived:
		return TEXT("BASE_VIEW_FROM_DERIVED");
	case EPropVisibilityAccessPath::DerivedViewFromGlobal:
		return TEXT("DERIVED_VIEW_FROM_GLOBAL");
	default:
		return nullptr;
	}
}

const TCHAR* OperationToken(const EPropVisibilityOperation Operation)
{
	switch (Operation)
	{
	case EPropVisibilityOperation::Read:
		return TEXT("READ");
	case EPropVisibilityOperation::Write:
		return TEXT("WRITE");
	default:
		return nullptr;
	}
}

const TCHAR* VisibilityToken(const EPropVisibilityVisibility Visibility)
{
	switch (Visibility)
	{
	case EPropVisibilityVisibility::Default:
		return TEXT("DEFAULT");
	case EPropVisibilityVisibility::Private:
		return TEXT("PRIVATE");
	case EPropVisibilityVisibility::Protected:
		return TEXT("PROTECTED");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FPropVisibilityParams& Params)
{
	return PathToken(Params.AccessPath) != nullptr
		&& OperationToken(Params.Operation) != nullptr
		&& VisibilityToken(Params.Visibility) != nullptr;
}

bool IsOwnerPath(const EPropVisibilityAccessPath AccessPath)
{
	return AccessPath == EPropVisibilityAccessPath::OwnerMethod
		|| AccessPath == EPropVisibilityAccessPath::OwnerConstructor
		|| AccessPath == EPropVisibilityAccessPath::OwnerDestructor
		|| AccessPath == EPropVisibilityAccessPath::AccessorBody;
}

bool IsDerivedPath(const EPropVisibilityAccessPath AccessPath)
{
	return AccessPath == EPropVisibilityAccessPath::DirectDerived
		|| AccessPath == EPropVisibilityAccessPath::DeepDerived
		|| AccessPath == EPropVisibilityAccessPath::BaseViewFromDerived;
}

bool IsReject(const FPropVisibilityParams& Params)
{
	if (IsOwnerPath(Params.AccessPath))
	{
		return false;
	}
	if (IsDerivedPath(Params.AccessPath))
	{
		return Params.Visibility == EPropVisibilityVisibility::Private;
	}
	return Params.Visibility != EPropVisibilityVisibility::Default;
}

int32 ExpectedFromOperation(const EPropVisibilityOperation Operation)
{
	return Operation == EPropVisibilityOperation::Read ? 41 : 67;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FVisibilityCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-PROP-VISIBILITY-%s-%s-%s"),
		PathToken(Cell.AccessPath),
		OperationToken(Cell.Operation),
		VisibilityToken(Cell.Visibility));
}

FString MakeEntryName(const FVisibilityCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FPropVisibilityParams ParamsFromCell(const FVisibilityCell& Cell)
{
	FPropVisibilityParams Params;
	Params.AccessPath = Cell.AccessPath;
	Params.Operation = Cell.Operation;
	Params.Visibility = Cell.Visibility;
	return Params;
}

TArray<FVisibilityCell> AllCells()
{
	TArray<FVisibilityCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FVisibilityCell& Cell = Cells[Index];
		Cell.AccessPath = static_cast<EPropVisibilityAccessPath>(Index / (OperationCount * VisibilityCount));
		Cell.Operation = static_cast<EPropVisibilityOperation>((Index / VisibilityCount) % OperationCount);
		Cell.Visibility = static_cast<EPropVisibilityVisibility>(Index % VisibilityCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FVisibilityCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FVisibilityCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

const TCHAR* VisibilityPrefix(const EPropVisibilityVisibility Visibility)
{
	if (Visibility == EPropVisibilityVisibility::Private)
	{
		return TEXT("private ");
	}
	if (Visibility == EPropVisibilityVisibility::Protected)
	{
		return TEXT("protected ");
	}
	return TEXT("");
}

void AppendReadOrWrite(
	FString& Source,
	const FPropVisibilityParams& Params,
	const TCHAR* Receiver,
	const TCHAR* Indent,
	const bool bReturn)
{
	if (Params.Operation == EPropVisibilityOperation::Read)
	{
		AppendLine(Source, FString::Printf(TEXT("%sint Observed = %sVisibleValue;"), Indent, Receiver));
		if (bReturn)
		{
			AppendLine(Source, FString::Printf(TEXT("%sreturn Observed;"), Indent));
		}
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("%s%sVisibleValue = 67;"), Indent, Receiver));
		if (bReturn)
		{
			AppendLine(Source, FString::Printf(TEXT("%sreturn ReadVisibleValue();"), Indent));
		}
	}
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int VisibilityDestructorObservation = -1;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void RecordPropertyVisibilityDestructor(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tVisibilityDestructorObservation = Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ReadPropertyVisibilityDestructor()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn VisibilityDestructorObservation;"));
	AppendLine(Source, TEXT("}"));
}

void AppendOwnerType(FString& Source, const FPropVisibilityParams& Params, const FString& Qualifier)
{
	AppendLine(Source, FString::Printf(TEXT("class FVisibilityOwner_%s"), *Qualifier));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%sint VisibleValue = 41;"), VisibilityPrefix(Params.Visibility)));
	AppendLine(Source, TEXT("\tint ConstructorObservation = -1;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint ReadVisibleValue() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn VisibleValue;"));
	AppendLine(Source, TEXT("\t}"));
	if (Params.AccessPath == EPropVisibilityAccessPath::OwnerMethod)
	{
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint ProbeOwnerMethod()"));
		AppendLine(Source, TEXT("\t{"));
		AppendReadOrWrite(Source, Params, TEXT(""), TEXT("\t\t"), true);
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.AccessPath == EPropVisibilityAccessPath::OwnerConstructor)
	{
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityOwner_%s()"), *Qualifier));
		AppendLine(Source, TEXT("\t{"));
		AppendReadOrWrite(Source, Params, TEXT(""), TEXT("\t\t"), false);
		AppendLine(Source, Params.Operation == EPropVisibilityOperation::Read
			? TEXT("\t\tConstructorObservation = Observed;")
			: TEXT("\t\tConstructorObservation = ReadVisibleValue();"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.AccessPath == EPropVisibilityAccessPath::OwnerDestructor)
	{
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\t~FVisibilityOwner_%s()"), *Qualifier));
		AppendLine(Source, TEXT("\t{"));
		AppendReadOrWrite(Source, Params, TEXT(""), TEXT("\t\t"), false);
		AppendLine(Source, Params.Operation == EPropVisibilityOperation::Read
			? TEXT("\t\tRecordPropertyVisibilityDestructor(Observed);")
			: TEXT("\t\tRecordPropertyVisibilityDestructor(ReadVisibleValue());"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.AccessPath == EPropVisibilityAccessPath::AccessorBody)
	{
		AppendLine(Source);
		if (Params.Operation == EPropVisibilityOperation::Read)
		{
			AppendLine(Source, TEXT("\tint ExplicitReadAccessor() const"));
			AppendLine(Source, TEXT("\t{"));
			AppendReadOrWrite(Source, Params, TEXT(""), TEXT("\t\t"), true);
			AppendLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendLine(Source, TEXT("\tvoid ExplicitWriteAccessor()"));
			AppendLine(Source, TEXT("\t{"));
			AppendReadOrWrite(Source, Params, TEXT(""), TEXT("\t\t"), false);
			AppendLine(Source, TEXT("\t}"));
		}
	}
	if (Params.AccessPath != EPropVisibilityAccessPath::OwnerConstructor)
	{
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityOwner_%s()"), *Qualifier));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendDerivedTypes(FString& Source, const FPropVisibilityParams& Params, const FString& Qualifier)
{
	AppendLine(Source, FString::Printf(
		TEXT("class FVisibilityDerived_%s : FVisibilityOwner_%s"),
		*Qualifier,
		*Qualifier));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tFVisibilityDerived_%s()"), *Qualifier));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tsuper();"));
	AppendLine(Source, TEXT("\t}"));
	if (Params.AccessPath == EPropVisibilityAccessPath::DirectDerived)
	{
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint ProbeDirectDerived()"));
		AppendLine(Source, TEXT("\t{"));
		AppendReadOrWrite(Source, Params, TEXT(""), TEXT("\t\t"), true);
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.AccessPath == EPropVisibilityAccessPath::BaseViewFromDerived)
	{
		AppendLine(Source);
		AppendLine(Source, FString::Printf(
			TEXT("\tint ProbeBaseView(FVisibilityOwner_%s& inout BaseView)"),
			*Qualifier));
		AppendLine(Source, TEXT("\t{"));
		AppendReadOrWrite(Source, Params, TEXT("BaseView."), TEXT("\t\t"), false);
		AppendLine(Source, Params.Operation == EPropVisibilityOperation::Read
			? TEXT("\t\treturn Observed;")
			: TEXT("\t\treturn BaseView.ReadVisibleValue();"));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(
		TEXT("class FVisibilityDeepDerived_%s : FVisibilityDerived_%s"),
		*Qualifier,
		*Qualifier));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tFVisibilityDeepDerived_%s()"), *Qualifier));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tsuper();"));
	AppendLine(Source, TEXT("\t}"));
	if (Params.AccessPath == EPropVisibilityAccessPath::DeepDerived)
	{
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint ProbeDeepDerived()"));
		AppendLine(Source, TEXT("\t{"));
		AppendReadOrWrite(Source, Params, TEXT(""), TEXT("\t\t"), true);
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	if (Params.AccessPath == EPropVisibilityAccessPath::UnrelatedType)
	{
		AppendLine(Source, FString::Printf(TEXT("class FVisibilityUnrelated_%s"), *Qualifier));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityUnrelated_%s()"), *Qualifier));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(
			TEXT("\tint ProbeUnrelated(FVisibilityOwner_%s& inout Receiver)"),
			*Qualifier));
		AppendLine(Source, TEXT("\t{"));
		AppendReadOrWrite(Source, Params, TEXT("Receiver."), TEXT("\t\t"), false);
		AppendLine(Source, Params.Operation == EPropVisibilityOperation::Read
			? TEXT("\t\treturn Observed;")
			: TEXT("\t\treturn Receiver.ReadVisibleValue();"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendEntry(FString& Source, const FPropVisibilityParams& Params, const FString& FunctionName)
{
	const FString Qualifier = FunctionName;
	AppendOwnerType(Source, Params, Qualifier);
	AppendDerivedTypes(Source, Params, Qualifier);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.AccessPath == EPropVisibilityAccessPath::OwnerMethod)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityOwner_%s Receiver = FVisibilityOwner_%s();"), *Qualifier, *Qualifier));
		AppendLine(Source, TEXT("\treturn Receiver.ProbeOwnerMethod();"));
	}
	else if (Params.AccessPath == EPropVisibilityAccessPath::OwnerConstructor)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityOwner_%s Receiver = FVisibilityOwner_%s();"), *Qualifier, *Qualifier));
		AppendLine(Source, TEXT("\treturn Receiver.ConstructorObservation;"));
	}
	else if (Params.AccessPath == EPropVisibilityAccessPath::OwnerDestructor)
	{
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\tFVisibilityOwner_%s Receiver = FVisibilityOwner_%s();"), *Qualifier, *Qualifier));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn ReadPropertyVisibilityDestructor();"));
	}
	else if (Params.AccessPath == EPropVisibilityAccessPath::AccessorBody)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityOwner_%s Receiver = FVisibilityOwner_%s();"), *Qualifier, *Qualifier));
		if (Params.Operation == EPropVisibilityOperation::Read)
		{
			AppendLine(Source, TEXT("\treturn Receiver.ExplicitReadAccessor();"));
		}
		else
		{
			AppendLine(Source, TEXT("\tReceiver.ExplicitWriteAccessor();"));
			AppendLine(Source, TEXT("\treturn Receiver.ReadVisibleValue();"));
		}
	}
	else if (Params.AccessPath == EPropVisibilityAccessPath::DirectDerived)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityDerived_%s Receiver = FVisibilityDerived_%s();"), *Qualifier, *Qualifier));
		AppendLine(Source, TEXT("\treturn Receiver.ProbeDirectDerived();"));
	}
	else if (Params.AccessPath == EPropVisibilityAccessPath::DeepDerived)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityDeepDerived_%s Receiver = FVisibilityDeepDerived_%s();"), *Qualifier, *Qualifier));
		AppendLine(Source, TEXT("\treturn Receiver.ProbeDeepDerived();"));
	}
	else if (Params.AccessPath == EPropVisibilityAccessPath::UnrelatedType)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityOwner_%s Receiver = FVisibilityOwner_%s();"), *Qualifier, *Qualifier));
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityUnrelated_%s Observer = FVisibilityUnrelated_%s();"), *Qualifier, *Qualifier));
		AppendLine(Source, TEXT("\treturn Observer.ProbeUnrelated(Receiver);"));
	}
	else if (Params.AccessPath == EPropVisibilityAccessPath::BaseViewFromDerived)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityDerived_%s Receiver = FVisibilityDerived_%s();"), *Qualifier, *Qualifier));
		AppendLine(Source, FString::Printf(TEXT("\tFVisibilityOwner_%s BaseView = Receiver;"), *Qualifier));
		AppendLine(Source, TEXT("\treturn Receiver.ProbeBaseView(BaseView);"));
	}
	else
	{
		if (Params.AccessPath == EPropVisibilityAccessPath::DerivedViewFromGlobal)
		{
			AppendLine(Source, FString::Printf(TEXT("\tFVisibilityDerived_%s Receiver = FVisibilityDerived_%s();"), *Qualifier, *Qualifier));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\tFVisibilityOwner_%s Receiver = FVisibilityOwner_%s();"), *Qualifier, *Qualifier));
		}
		AppendReadOrWrite(Source, Params, TEXT("Receiver."), TEXT("\t"), false);
		AppendLine(Source, Params.Operation == EPropVisibilityOperation::Read
			? TEXT("\treturn Observed;")
			: TEXT("\treturn Receiver.ReadVisibleValue();"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FPropVisibilityParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FVisibilityCell& Cell)
{
	const FPropVisibilityParams Params = ParamsFromCell(Cell);
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Params))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = ExpectedFromOperation(Cell.Operation);
	if (Cell.AccessPath == EPropVisibilityAccessPath::OwnerDestructor)
	{
		Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
		Info.ExecutionNotes = TEXT("Needs native RecordPropertyVisibilityDestructor.");
	}
	else
	{
		Info.Execution = EGeneratedCaseExecution::Standalone;
	}
	return Info;
}
}

FStringView FPropVisibilityGenerator::GetProductId() const
{
	return TEXT("LANG-PROP-VISIBILITY");
}

TArray<FGeneratedCaseInfo> FPropVisibilityGenerator::ListCases() const
{
	using namespace PropVisibilityGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FVisibilityCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("PropVisibility PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FPropVisibilityGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace PropVisibilityGeneratorDetail;
	FVisibilityCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FPropVisibilityGenerator::BuildDumpSource() const
{
	using namespace PropVisibilityGeneratorDetail;
	const TArray<FVisibilityCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-PROP-VISIBILITY"));
	AppendLine(Source, TEXT("// Cases: 60 total, 42 normal, 18 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	bool bNeedBlank = true;
	for (const FVisibilityCell& Cell : Cells)
	{
		if (IsReject(ParamsFromCell(Cell)))
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
		AppendLine(Source, FString::Printf(TEXT("// Execution: %s"),
			Info.Execution == EGeneratedCaseExecution::RequiresHostSetup
				? TEXT("RequiresHostSetup")
				: TEXT("Standalone")));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	for (const FVisibilityCell& Cell : Cells)
	{
		if (!IsReject(ParamsFromCell(Cell)))
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
		AppendSharedHelpers(Source);
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FPropVisibilityGenerator::BuildPropertyVisibilitySource(const FPropVisibilityParams& Params) const
{
	using namespace PropVisibilityGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params))
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

FString FPropVisibilityGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace PropVisibilityGeneratorDetail;
	const TArray<FVisibilityCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (const FVisibilityCell& Cell : Cells)
	{
		if (IsReject(ParamsFromCell(Cell)))
		{
			continue;
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FPropVisibilityGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace PropVisibilityGeneratorDetail;
	FVisibilityCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(ParamsFromCell(Cell)))
	{
		return 0;
	}
	return ExpectedFromOperation(Cell.Operation);
}

FString FPropVisibilityGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace PropVisibilityGeneratorDetail;
	FVisibilityCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FPropVisibilityGenerator::ListRejectCaseIds() const
{
	using namespace PropVisibilityGeneratorDetail;
	TArray<FString> Ids;
	for (const FVisibilityCell& Cell : AllCells())
	{
		if (IsReject(ParamsFromCell(Cell)))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
