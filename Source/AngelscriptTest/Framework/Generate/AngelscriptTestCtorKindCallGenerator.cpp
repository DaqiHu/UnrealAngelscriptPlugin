#include "Framework/Generate/AngelscriptTestCtorKindCallGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace CtorKindCallGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native constructor-selection host markers.");

struct FCtorKindCallCell
{
	ECtorKindCallCall Call = ECtorKindCallCall::Local;
	ECtorKindCallKind Kind = ECtorKindCallKind::ImplicitDefault;
	ECtorKindCallObject Object = ECtorKindCallObject::ScriptValue;
};

const TCHAR* CallToken(const ECtorKindCallCall Call)
{
	switch (Call)
	{
	case ECtorKindCallCall::Local:
		return TEXT("LOCAL");
	case ECtorKindCallCall::Temporary:
		return TEXT("TEMPORARY");
	case ECtorKindCallCall::Field:
		return TEXT("FIELD");
	case ECtorKindCallCall::Return:
		return TEXT("RETURN");
	case ECtorKindCallCall::Argument:
		return TEXT("ARGUMENT");
	case ECtorKindCallCall::BaseCall:
		return TEXT("BASE_CALL");
	case ECtorKindCallCall::CopyDeclaration:
		return TEXT("COPY_DECLARATION");
	case ECtorKindCallCall::Assignment:
		return TEXT("ASSIGNMENT");
	default:
		return nullptr;
	}
}

const TCHAR* KindToken(const ECtorKindCallKind Kind)
{
	switch (Kind)
	{
	case ECtorKindCallKind::ImplicitDefault:
		return TEXT("IMPLICIT_DEFAULT");
	case ECtorKindCallKind::DeclaredDefault:
		return TEXT("DECLARED_DEFAULT");
	case ECtorKindCallKind::Parameterized:
		return TEXT("PARAMETERIZED");
	case ECtorKindCallKind::Overloaded:
		return TEXT("OVERLOADED");
	case ECtorKindCallKind::Copy:
		return TEXT("COPY");
	case ECtorKindCallKind::Conversion:
		return TEXT("CONVERSION");
	default:
		return nullptr;
	}
}

const TCHAR* ObjectToken(const ECtorKindCallObject Object)
{
	switch (Object)
	{
	case ECtorKindCallObject::ScriptValue:
		return TEXT("SCRIPT_VALUE");
	case ECtorKindCallObject::ScriptReference:
		return TEXT("SCRIPT_REFERENCE");
	case ECtorKindCallObject::Base:
		return TEXT("BASE");
	case ECtorKindCallObject::Derived:
		return TEXT("DERIVED");
	case ECtorKindCallObject::NativeValue:
		return TEXT("NATIVE_VALUE");
	case ECtorKindCallObject::NativeReference:
		return TEXT("NATIVE_REFERENCE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FCtorKindCallParams& Params)
{
	return CallToken(Params.Call) != nullptr
		&& KindToken(Params.Kind) != nullptr
		&& ObjectToken(Params.Object) != nullptr;
}

bool IsReject(const ECtorKindCallCall Call, const ECtorKindCallKind Kind, const ECtorKindCallObject Object)
{
	if (Call != ECtorKindCallCall::BaseCall)
	{
		return false;
	}
	return Object != ECtorKindCallObject::Derived || Kind == ECtorKindCallKind::ImplicitDefault;
}

bool IsRejectCell(const FCtorKindCallCell& Cell)
{
	return IsReject(Cell.Call, Cell.Kind, Cell.Object);
}

bool IsReferenceObject(const ECtorKindCallObject Object)
{
	return Object == ECtorKindCallObject::ScriptReference
		|| Object == ECtorKindCallObject::Base
		|| Object == ECtorKindCallObject::Derived
		|| Object == ECtorKindCallObject::NativeReference;
}

bool IsValueObject(const ECtorKindCallObject Object)
{
	return Object == ECtorKindCallObject::ScriptValue || Object == ECtorKindCallObject::NativeValue;
}

int32 KindMarker(const ECtorKindCallKind Kind)
{
	switch (Kind)
	{
	case ECtorKindCallKind::ImplicitDefault:
		return 101;
	case ECtorKindCallKind::DeclaredDefault:
		return 201;
	case ECtorKindCallKind::Parameterized:
		return 301;
	case ECtorKindCallKind::Overloaded:
		return 401;
	case ECtorKindCallKind::Copy:
		return 501;
	default:
		return 601;
	}
}

int32 CallMarker(const ECtorKindCallCall Call)
{
	switch (Call)
	{
	case ECtorKindCallCall::Local:
		return 11;
	case ECtorKindCallCall::Temporary:
		return 12;
	case ECtorKindCallCall::Field:
		return 13;
	case ECtorKindCallCall::Return:
		return 14;
	case ECtorKindCallCall::Argument:
		return 15;
	case ECtorKindCallCall::BaseCall:
		return 16;
	case ECtorKindCallCall::CopyDeclaration:
		return 17;
	default:
		return 18;
	}
}

int32 ExpectedFromCell(const FCtorKindCallCell& Cell)
{
	const int32 ExpectedValue =
		Cell.Kind == ECtorKindCallKind::ImplicitDefault
			|| (Cell.Kind == ECtorKindCallKind::DeclaredDefault
				&& Cell.Object == ECtorKindCallObject::NativeValue)
			? 0
			: 7;
	const bool bHasTransferMutation = Cell.Kind == ECtorKindCallKind::Copy
		|| Cell.Call == ECtorKindCallCall::CopyDeclaration
		|| Cell.Call == ECtorKindCallCall::Assignment;
	const bool bAliases = IsReferenceObject(Cell.Object) && Cell.Call != ECtorKindCallCall::BaseCall;
	return bAliases && bHasTransferMutation ? 9 : ExpectedValue;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FCtorKindCallCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CTOR-KIND-CALL-%s-%s-%s"),
		CallToken(Cell.Call),
		KindToken(Cell.Kind),
		ObjectToken(Cell.Object));
}

FString MakeEntryName(const FCtorKindCallCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString TypeTag(const ECtorKindCallKind Kind, const ECtorKindCallObject Object)
{
	FString Tag = FString(KindToken(Kind)) + ObjectToken(Object);
	Tag.ReplaceInline(TEXT("_"), TEXT(""));
	return Tag;
}

FString ObjectTypeName(const ECtorKindCallKind Kind, const ECtorKindCallObject Object)
{
	if (Object == ECtorKindCallObject::NativeValue)
	{
		return TEXT("FNativeCaseValue");
	}
	if (Object == ECtorKindCallObject::NativeReference)
	{
		return TEXT("FNativeCaseReference");
	}
	return FString::Printf(TEXT("FKindObject%s"), *TypeTag(Kind, Object));
}

FCtorKindCallParams ParamsFromCell(const FCtorKindCallCell& Cell)
{
	FCtorKindCallParams Params;
	Params.Call = Cell.Call;
	Params.Kind = Cell.Kind;
	Params.Object = Cell.Object;
	return Params;
}

TArray<FCtorKindCallCell> AllCells()
{
	TArray<FCtorKindCallCell> Cells;
	Cells.SetNum(288);
	for (int32 Index = 0; Index < 288; ++Index)
	{
		FCtorKindCallCell& Cell = Cells[Index];
		Cell.Call = static_cast<ECtorKindCallCall>(Index / 36);
		Cell.Kind = static_cast<ECtorKindCallKind>((Index / 6) % 6);
		Cell.Object = static_cast<ECtorKindCallObject>(Index % 6);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FCtorKindCallCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FCtorKindCallCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString SelectedExpression(const ECtorKindCallKind Kind, const ECtorKindCallObject Object)
{
	const FString TypeName = ObjectTypeName(Kind, Object);
	if (Object == ECtorKindCallObject::NativeReference)
	{
		if (Kind == ECtorKindCallKind::ImplicitDefault)
		{
			return TEXT("MakeNativeReferenceImplicit()");
		}
		if (Kind == ECtorKindCallKind::DeclaredDefault)
		{
			return TEXT("MakeNativeReferenceDeclared()");
		}
		if (Kind == ECtorKindCallKind::Parameterized)
		{
			return TEXT("MakeNativeReferenceParameterized(7)");
		}
		if (Kind == ECtorKindCallKind::Overloaded)
		{
			return TEXT("MakeNativeReferenceOverloaded(3, 4)");
		}
		if (Kind == ECtorKindCallKind::Copy)
		{
			return TEXT("MakeNativeReferenceImplicit()");
		}
		return TEXT("MakeNativeReferenceConversion(int64(int8(7)))");
	}
	if (Kind == ECtorKindCallKind::Parameterized)
	{
		return TypeName + TEXT("(7)");
	}
	if (Kind == ECtorKindCallKind::Overloaded)
	{
		return TypeName + TEXT("(3, 4)");
	}
	if (Kind == ECtorKindCallKind::Conversion)
	{
		return TypeName + TEXT("(int64(int8(7)))");
	}
	return TypeName + TEXT("()");
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("void RecordConstructorKindSelected(int Marker)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void RecordConstructorCallRoute(int Marker)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void RecordConstructorTransferObservation(int SourceValue, int TargetValue)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void RecordConstructorScriptCopy()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FNativeCaseValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FNativeCaseReference"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseReference MakeNativeReferenceImplicit()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Value;"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseReference MakeNativeReferenceDeclared()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Value;"));
	AppendLine(Source, TEXT("\tValue.Value = 7;"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseReference MakeNativeReferenceParameterized(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Result;"));
	AppendLine(Source, TEXT("\tResult.Value = Value;"));
	AppendLine(Source, TEXT("\treturn Result;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseReference MakeNativeReferenceOverloaded(int Left, int Right)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Result;"));
	AppendLine(Source, TEXT("\tResult.Value = Left + Right;"));
	AppendLine(Source, TEXT("\treturn Result;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseReference MakeNativeReferenceConversion(int64 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Result;"));
	AppendLine(Source, TEXT("\tResult.Value = int(Value);"));
	AppendLine(Source, TEXT("\treturn Result;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendScriptConstructor(FString& Source, const ECtorKindCallKind Kind, const FString& TypeName)
{
	if (Kind == ECtorKindCallKind::DeclaredDefault)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s()"), *TypeName));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = 7;"));
		AppendLine(Source, FString::Printf(TEXT("\t\tRecordConstructorKindSelected(%d);"), KindMarker(Kind)));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
	}
	else if (Kind == ECtorKindCallKind::Parameterized)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s(int InValue)"), *TypeName));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = InValue;"));
		AppendLine(Source, FString::Printf(TEXT("\t\tRecordConstructorKindSelected(%d);"), KindMarker(Kind)));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
	}
	else if (Kind == ECtorKindCallKind::Overloaded)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s(int Left, int Right)"), *TypeName));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = Left + Right;"));
		AppendLine(Source, FString::Printf(TEXT("\t\tRecordConstructorKindSelected(%d);"), KindMarker(Kind)));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
	}
	else if (Kind == ECtorKindCallKind::Conversion)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s(int64 InValue)"), *TypeName));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = int(InValue);"));
		AppendLine(Source, FString::Printf(TEXT("\t\tRecordConstructorKindSelected(%d);"), KindMarker(Kind)));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
	}
}

void AppendObjectTypes(FString& Source, const ECtorKindCallKind Kind, const ECtorKindCallObject Object)
{
	if (Object == ECtorKindCallObject::NativeValue || Object == ECtorKindCallObject::NativeReference)
	{
		return;
	}
	const FString Tag = TypeTag(Kind, Object);
	const FString TypeName = ObjectTypeName(Kind, Object);
	const bool bValue = Object == ECtorKindCallObject::ScriptValue;
	if (Object == ECtorKindCallObject::Derived)
	{
		AppendLine(Source, FString::Printf(TEXT("class FKindBase%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint BaseValue = 2;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(
		TEXT("%s %s%s"),
		bValue ? TEXT("struct") : TEXT("class"),
		*TypeName,
		Object == ECtorKindCallObject::Derived ? *FString::Printf(TEXT(" : FKindBase%s"), *Tag) : TEXT("")));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source);
	AppendScriptConstructor(Source, Kind, TypeName);
	if (bValue)
	{
		if (Kind == ECtorKindCallKind::Copy)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s()"), *TypeName));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, TEXT("\t}"));
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("\t%s(const %s& Other)"), *TypeName, *TypeName));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = Other.Value;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorScriptCopy();"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	if (Object == ECtorKindCallObject::Base)
	{
		AppendLine(Source, FString::Printf(TEXT("class FKindDerivedWitness%s : %s"), *Tag, *TypeName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendConstruction(FString& Source, const FCtorKindCallParams& Params, const TCHAR* VariableName)
{
	const FString TypeName = ObjectTypeName(Params.Kind, Params.Object);
	if (Params.Kind == ECtorKindCallKind::Copy)
	{
		if (IsReferenceObject(Params.Object))
		{
			AppendLine(Source, FString::Printf(
				TEXT("\t%s CopySource = %s;"),
				*TypeName,
				*SelectedExpression(ECtorKindCallKind::ImplicitDefault, Params.Object)));
			AppendLine(Source, TEXT("\tCopySource.Value = 7;"));
			AppendLine(Source, FString::Printf(TEXT("\t%s %s = CopySource;"), *TypeName, VariableName));
		}
		else
		{
			if (Params.Object == ECtorKindCallObject::NativeValue)
			{
				AppendLine(Source, FString::Printf(TEXT("\t%s CopySource(7);"), *TypeName));
			}
			else
			{
				AppendLine(Source, FString::Printf(TEXT("\t%s CopySource;"), *TypeName));
				AppendLine(Source, TEXT("\tCopySource.Value = 7;"));
			}
			AppendLine(Source, FString::Printf(TEXT("\t%s %s(CopySource);"), *TypeName, VariableName));
		}
		AppendLine(Source, TEXT("\tCopySource.Value = 9;"));
		AppendLine(Source, FString::Printf(
			TEXT("\tRecordConstructorTransferObservation(CopySource.Value, %s.Value);"),
			VariableName));
		return;
	}
	if (Params.Kind == ECtorKindCallKind::ImplicitDefault && IsValueObject(Params.Object))
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s %s;"), *TypeName, VariableName));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s %s = %s;"),
			*TypeName,
			VariableName,
			*SelectedExpression(Params.Kind, Params.Object)));
	}
}

void AppendEntry(FString& Source, const FCtorKindCallParams& Params, const FString& FunctionName)
{
	const FString TypeName = ObjectTypeName(Params.Kind, Params.Object);
	const int32 Route = CallMarker(Params.Call);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Call == ECtorKindCallCall::Temporary)
	{
		AppendLine(Source, FString::Printf(TEXT("\tRecordConstructorCallRoute(%d);"), Route));
		AppendLine(Source, FString::Printf(TEXT("\treturn (%s).Value;"), *SelectedExpression(Params.Kind, Params.Object)));
	}
	else if (Params.Call == ECtorKindCallCall::Field)
	{
		AppendConstruction(Source, Params, TEXT("Selected"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Stored = Selected;"), *TypeName));
		AppendLine(Source, FString::Printf(TEXT("\tRecordConstructorCallRoute(%d);"), Route));
		AppendLine(Source, TEXT("\treturn Stored.Value;"));
	}
	else if (Params.Call == ECtorKindCallCall::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("\tRecordConstructorCallRoute(%d);"), Route));
		AppendLine(Source, FString::Printf(TEXT("\treturn (%s).Value;"), *SelectedExpression(Params.Kind, Params.Object)));
	}
	else if (Params.Call == ECtorKindCallCall::Argument)
	{
		AppendConstruction(Source, Params, TEXT("Selected"));
		AppendLine(Source, FString::Printf(TEXT("\tRecordConstructorCallRoute(%d);"), Route));
		AppendLine(Source, TEXT("\treturn Selected.Value;"));
	}
	else if (Params.Call == ECtorKindCallCall::BaseCall)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Selected = %s;"), *TypeName, *SelectedExpression(Params.Kind, Params.Object)));
		AppendLine(Source, TEXT("\tsuper();"));
		AppendLine(Source, FString::Printf(TEXT("\tRecordConstructorCallRoute(%d);"), Route));
		AppendLine(Source, TEXT("\treturn Selected.Value;"));
	}
	else if (Params.Call == ECtorKindCallCall::CopyDeclaration)
	{
		AppendConstruction(Source, Params, TEXT("Selected"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Copied = Selected;"), *TypeName));
		AppendLine(Source, TEXT("\tSelected.Value = 9;"));
		AppendLine(Source, TEXT("\tRecordConstructorTransferObservation(Selected.Value, Copied.Value);"));
		AppendLine(Source, FString::Printf(TEXT("\tRecordConstructorCallRoute(%d);"), Route));
		AppendLine(Source, TEXT("\treturn Copied.Value;"));
	}
	else if (Params.Call == ECtorKindCallCall::Assignment)
	{
		AppendConstruction(Source, Params, TEXT("Selected"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Assigned;"), *TypeName));
		AppendLine(Source, TEXT("\tAssigned = Selected;"));
		AppendLine(Source, TEXT("\tSelected.Value = 9;"));
		AppendLine(Source, TEXT("\tRecordConstructorTransferObservation(Selected.Value, Assigned.Value);"));
		AppendLine(Source, FString::Printf(TEXT("\tRecordConstructorCallRoute(%d);"), Route));
		AppendLine(Source, TEXT("\treturn Assigned.Value;"));
	}
	else
	{
		AppendConstruction(Source, Params, TEXT("Selected"));
		AppendLine(Source, FString::Printf(TEXT("\tRecordConstructorCallRoute(%d);"), Route));
		AppendLine(Source, TEXT("\treturn Selected.Value;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FCtorKindCallParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendObjectTypes(Source, Params.Kind, Params.Object);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FCtorKindCallCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsRejectCell(Cell))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FCtorKindCallGenerator::GetProductId() const
{
	return TEXT("LANG-CTOR-KIND-CALL");
}

TArray<FGeneratedCaseInfo> FCtorKindCallGenerator::ListCases() const
{
	using namespace CtorKindCallGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FCtorKindCallCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("CtorKindCall PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FCtorKindCallGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace CtorKindCallGeneratorDetail;
	FCtorKindCallCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FCtorKindCallGenerator::BuildDumpSource() const
{
	using namespace CtorKindCallGeneratorDetail;
	const TArray<FCtorKindCallCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CTOR-KIND-CALL"));
	AppendLine(Source, TEXT("// Cases: 288 total, 257 normal, 31 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);
	TSet<FString> EmittedTypes;
	for (const FCtorKindCallCell& Cell : Cells)
	{
		if (IsRejectCell(Cell))
		{
			continue;
		}
		const FString Key = TypeTag(Cell.Kind, Cell.Object);
		if (!EmittedTypes.Contains(Key))
		{
			AppendObjectTypes(Source, Cell.Kind, Cell.Object);
			EmittedTypes.Add(Key);
		}
	}
	bool bNeedBlank = false;
	for (const FCtorKindCallCell& Cell : Cells)
	{
		if (IsRejectCell(Cell))
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
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	for (const FCtorKindCallCell& Cell : Cells)
	{
		if (!IsRejectCell(Cell))
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
		AppendHelpers(Source);
		AppendObjectTypes(Source, Cell.Kind, Cell.Object);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FCtorKindCallGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace CtorKindCallGeneratorDetail;
	const TArray<FCtorKindCallCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	TSet<FString> EmittedTypes;
	for (const FCtorKindCallCell& Cell : Cells)
	{
		if (IsRejectCell(Cell))
		{
			continue;
		}
		const FString Key = TypeTag(Cell.Kind, Cell.Object);
		if (!EmittedTypes.Contains(Key))
		{
			AppendObjectTypes(Source, Cell.Kind, Cell.Object);
			EmittedTypes.Add(Key);
		}
	}
	int32 Emitted = 0;
	for (const FCtorKindCallCell& Cell : Cells)
	{
		if (IsRejectCell(Cell))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FCtorKindCallGenerator::BuildConstructorSelectionSource(const FCtorKindCallParams& Params) const
{
	using namespace CtorKindCallGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Call, Params.Kind, Params.Object))
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

int32 FCtorKindCallGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace CtorKindCallGeneratorDetail;
	FCtorKindCallCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsRejectCell(Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FCtorKindCallGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace CtorKindCallGeneratorDetail;
	FCtorKindCallCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsRejectCell(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FCtorKindCallGenerator::ListRejectCaseIds() const
{
	using namespace CtorKindCallGeneratorDetail;
	TArray<FString> Ids;
	for (const FCtorKindCallCell& Cell : AllCells())
	{
		if (IsRejectCell(Cell))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
