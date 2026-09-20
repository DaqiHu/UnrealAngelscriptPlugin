#include "Framework/Generate/AngelscriptTestPropForkSemanticsGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace PropForkSemanticsGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native CreatePropertyForkCarrier, GetValue, and GetIndexedValue.");

struct FForkCell
{
	EPropForkSemanticsFamily Family = EPropForkSemanticsFamily::ScriptDecorator;
	EPropForkSemanticsVariant Variant = EPropForkSemanticsVariant::Getter;
};

const TCHAR* FamilyToken(const EPropForkSemanticsFamily Family)
{
	switch (Family)
	{
	case EPropForkSemanticsFamily::ScriptDecorator:
		return TEXT("SCRIPT_DECORATOR");
	case EPropForkSemanticsFamily::NativeRegistration:
		return TEXT("NATIVE_REGISTRATION");
	case EPropForkSemanticsFamily::AutomaticAccess:
		return TEXT("AUTOMATIC_ACCESS");
	case EPropForkSemanticsFamily::DirectMethod:
		return TEXT("DIRECT_METHOD");
	default:
		return nullptr;
	}
}

const TCHAR* VariantToken(const EPropForkSemanticsVariant Variant)
{
	switch (Variant)
	{
	case EPropForkSemanticsVariant::Getter:
		return TEXT("GETTER");
	case EPropForkSemanticsVariant::Setter:
		return TEXT("SETTER");
	case EPropForkSemanticsVariant::IndexedGetter:
		return TEXT("INDEXED_GETTER");
	case EPropForkSemanticsVariant::IndexedSetter:
		return TEXT("INDEXED_SETTER");
	case EPropForkSemanticsVariant::Read:
		return TEXT("READ");
	case EPropForkSemanticsVariant::Write:
		return TEXT("WRITE");
	case EPropForkSemanticsVariant::IndexedRead:
		return TEXT("INDEXED_READ");
	case EPropForkSemanticsVariant::IndexedWrite:
		return TEXT("INDEXED_WRITE");
	case EPropForkSemanticsVariant::ReadWriteIndexed:
		return TEXT("READ_WRITE_INDEXED");
	default:
		return nullptr;
	}
}

bool VariantMatchesFamily(const EPropForkSemanticsFamily Family, const EPropForkSemanticsVariant Variant)
{
	if (Family == EPropForkSemanticsFamily::ScriptDecorator || Family == EPropForkSemanticsFamily::NativeRegistration)
	{
		return Variant == EPropForkSemanticsVariant::Getter
			|| Variant == EPropForkSemanticsVariant::Setter
			|| Variant == EPropForkSemanticsVariant::IndexedGetter
			|| Variant == EPropForkSemanticsVariant::IndexedSetter;
	}
	if (Family == EPropForkSemanticsFamily::AutomaticAccess)
	{
		return Variant == EPropForkSemanticsVariant::Read
			|| Variant == EPropForkSemanticsVariant::Write
			|| Variant == EPropForkSemanticsVariant::IndexedRead
			|| Variant == EPropForkSemanticsVariant::IndexedWrite;
	}
	return Family == EPropForkSemanticsFamily::DirectMethod
		&& Variant == EPropForkSemanticsVariant::ReadWriteIndexed;
}

bool IsValidParams(const FPropForkSemanticsParams& Params)
{
	return FamilyToken(Params.Family) != nullptr
		&& VariantToken(Params.Variant) != nullptr
		&& VariantMatchesFamily(Params.Family, Params.Variant);
}

bool IsReject(const EPropForkSemanticsFamily Family)
{
	return Family != EPropForkSemanticsFamily::DirectMethod;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FForkCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-PROP-FORK-SEMANTICS-%s-%s"),
		FamilyToken(Cell.Family),
		VariantToken(Cell.Variant));
}

FString MakeEntryName(const FForkCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FPropForkSemanticsParams ParamsFromCell(const FForkCell& Cell)
{
	FPropForkSemanticsParams Params;
	Params.Family = Cell.Family;
	Params.Variant = Cell.Variant;
	return Params;
}

TArray<FForkCell> AllCells()
{
	TArray<FForkCell> Cells;
	Cells.Reserve(13);
	const EPropForkSemanticsVariant DecoratorVariants[] = {
		EPropForkSemanticsVariant::Getter,
		EPropForkSemanticsVariant::Setter,
		EPropForkSemanticsVariant::IndexedGetter,
		EPropForkSemanticsVariant::IndexedSetter};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FForkCell Cell;
		Cell.Family = EPropForkSemanticsFamily::ScriptDecorator;
		Cell.Variant = DecoratorVariants[Index];
		Cells.Add(Cell);
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FForkCell Cell;
		Cell.Family = EPropForkSemanticsFamily::NativeRegistration;
		Cell.Variant = DecoratorVariants[Index];
		Cells.Add(Cell);
	}
	const EPropForkSemanticsVariant AccessVariants[] = {
		EPropForkSemanticsVariant::Read,
		EPropForkSemanticsVariant::Write,
		EPropForkSemanticsVariant::IndexedRead,
		EPropForkSemanticsVariant::IndexedWrite};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FForkCell Cell;
		Cell.Family = EPropForkSemanticsFamily::AutomaticAccess;
		Cell.Variant = AccessVariants[Index];
		Cells.Add(Cell);
	}
	FForkCell Direct;
	Direct.Family = EPropForkSemanticsFamily::DirectMethod;
	Direct.Variant = EPropForkSemanticsVariant::ReadWriteIndexed;
	Cells.Add(Direct);
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FForkCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FForkCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

const TCHAR* NativeDecoratorDeclaration(const EPropForkSemanticsVariant Variant)
{
	switch (Variant)
	{
	case EPropForkSemanticsVariant::Getter:
		return TEXT("int get_Value() const property");
	case EPropForkSemanticsVariant::Setter:
		return TEXT("void set_Value(int Value) property");
	case EPropForkSemanticsVariant::IndexedGetter:
		return TEXT("int get_Value(int Index) const property");
	case EPropForkSemanticsVariant::IndexedSetter:
		return TEXT("void set_Value(int Index, int Value) property");
	default:
		return TEXT("");
	}
}

void AppendDecoratorClass(FString& Source, const EPropForkSemanticsVariant Variant)
{
	AppendLine(Source, TEXT("class FPropertyDecoratorProbe"));
	AppendLine(Source, TEXT("{"));
	if (Variant == EPropForkSemanticsVariant::Getter)
	{
		AppendLine(Source, TEXT("\tint get_Value() property"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 41;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Variant == EPropForkSemanticsVariant::Setter)
	{
		AppendLine(Source, TEXT("\tvoid set_Value(int Value) property"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Variant == EPropForkSemanticsVariant::IndexedGetter)
	{
		AppendLine(Source, TEXT("\tint get_Value(int Index) property"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn Index;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendLine(Source, TEXT("\tvoid set_Value(int Index, int Value) property"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FPropForkSemanticsParams& Params, const FString& FunctionName)
{
	if (Params.Family == EPropForkSemanticsFamily::ScriptDecorator)
	{
		AppendDecoratorClass(Source, Params.Variant);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 97;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	if (Params.Family == EPropForkSemanticsFamily::NativeRegistration)
	{
		AppendLine(Source, FString::Printf(
			TEXT("// RegisterObjectMethod(..., \"%s\")"),
			NativeDecoratorDeclaration(Params.Variant)));
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn asINVALID_DECLARATION;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	if (Params.Family == EPropForkSemanticsFamily::AutomaticAccess)
	{
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFPropertyForkCarrier Receiver = CreatePropertyForkCarrier(31);"));
		if (Params.Variant == EPropForkSemanticsVariant::Read)
		{
			AppendLine(Source, TEXT("\treturn Receiver.Value;"));
		}
		else if (Params.Variant == EPropForkSemanticsVariant::Write)
		{
			AppendLine(Source, TEXT("\tReceiver.Value = 41;"));
			AppendLine(Source, TEXT("\treturn Receiver.Value;"));
		}
		else if (Params.Variant == EPropForkSemanticsVariant::IndexedRead)
		{
			AppendLine(Source, TEXT("\treturn Receiver.Value[2];"));
		}
		else
		{
			AppendLine(Source, TEXT("\tReceiver.Value[2] = 41;"));
			AppendLine(Source, TEXT("\treturn Receiver.Value[2];"));
		}
		AppendLine(Source, TEXT("}"));
		return;
	}

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFPropertyForkCarrier Receiver = CreatePropertyForkCarrier(31);"));
	AppendLine(Source, TEXT("\tReceiver.SetValue(41);"));
	AppendLine(Source, TEXT("\tReceiver.SetIndexedValue(2, 43);"));
	AppendLine(Source, TEXT("\treturn Receiver.GetValue() + Receiver.GetIndexedValue(2);"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FPropForkSemanticsParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FForkCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Family))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = 88;
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FPropForkSemanticsGenerator::GetProductId() const
{
	return TEXT("LANG-PROP-FORK-SEMANTICS");
}

TArray<FGeneratedCaseInfo> FPropForkSemanticsGenerator::ListCases() const
{
	using namespace PropForkSemanticsGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FForkCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("PropForkSemantics PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FPropForkSemanticsGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace PropForkSemanticsGeneratorDetail;
	FForkCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FPropForkSemanticsGenerator::BuildDumpSource() const
{
	using namespace PropForkSemanticsGeneratorDetail;
	const TArray<FForkCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-PROP-FORK-SEMANTICS"));
	AppendLine(Source, TEXT("// Cases: 13 total, 1 normal, 12 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);

	bool bNeedBlank = false;
	for (const FForkCell& Cell : Cells)
	{
		if (IsReject(Cell.Family))
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
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}

	for (const FForkCell& Cell : Cells)
	{
		if (!IsReject(Cell.Family))
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
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FPropForkSemanticsGenerator::BuildDirectMethodSource(const FPropForkSemanticsParams& Params) const
{
	using namespace PropForkSemanticsGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Family))
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

FString FPropForkSemanticsGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace PropForkSemanticsGeneratorDetail;
	const TArray<FForkCell> Cells = AllCells();
	FString Source;
	int32 Emitted = 0;
	for (const FForkCell& Cell : Cells)
	{
		if (IsReject(Cell.Family))
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

int32 FPropForkSemanticsGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace PropForkSemanticsGeneratorDetail;
	FForkCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Family))
	{
		return 0;
	}
	return 88;
}

FString FPropForkSemanticsGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace PropForkSemanticsGeneratorDetail;
	FForkCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Family))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FPropForkSemanticsGenerator::ListRejectCaseIds() const
{
	using namespace PropForkSemanticsGeneratorDetail;
	TArray<FString> Ids;
	for (const FForkCell& Cell : AllCells())
	{
		if (IsReject(Cell.Family))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
