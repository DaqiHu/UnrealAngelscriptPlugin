#include "Framework/Generate/AngelscriptTestConvEnumAliasGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ConvEnumAliasGeneratorDetail
{
struct FConvEnumAliasCell
{
	EConvEnumAliasSource Source = EConvEnumAliasSource::Enum;
	EConvEnumAliasTarget Target = EConvEnumAliasTarget::Enum;
	EConvEnumAliasForm Form = EConvEnumAliasForm::Assignment;
	EConvEnumAliasValue Value = EConvEnumAliasValue::Zero;
};

const TCHAR* SourceToken(const EConvEnumAliasSource Source)
{
	switch (Source)
	{
	case EConvEnumAliasSource::Enum:
		return TEXT("ENUM");
	case EConvEnumAliasSource::AliasInt8:
		return TEXT("ALIAS_INT8");
	case EConvEnumAliasSource::AliasInt:
		return TEXT("ALIAS_INT");
	case EConvEnumAliasSource::AliasInt64:
		return TEXT("ALIAS_INT64");
	case EConvEnumAliasSource::AliasUint:
		return TEXT("ALIAS_UINT");
	case EConvEnumAliasSource::AliasUint64:
		return TEXT("ALIAS_UINT64");
	default:
		return nullptr;
	}
}

const TCHAR* TargetToken(const EConvEnumAliasTarget Target)
{
	switch (Target)
	{
	case EConvEnumAliasTarget::Enum:
		return TEXT("ENUM");
	case EConvEnumAliasTarget::Int8:
		return TEXT("INT8");
	case EConvEnumAliasTarget::Int:
		return TEXT("INT");
	case EConvEnumAliasTarget::Int64:
		return TEXT("INT64");
	case EConvEnumAliasTarget::Uint:
		return TEXT("UINT");
	case EConvEnumAliasTarget::Uint64:
		return TEXT("UINT64");
	case EConvEnumAliasTarget::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* FormToken(const EConvEnumAliasForm Form)
{
	switch (Form)
	{
	case EConvEnumAliasForm::Assignment:
		return TEXT("ASSIGNMENT");
	case EConvEnumAliasForm::Initializer:
		return TEXT("INITIALIZER");
	case EConvEnumAliasForm::Argument:
		return TEXT("ARGUMENT");
	case EConvEnumAliasForm::Return:
		return TEXT("RETURN");
	case EConvEnumAliasForm::Promotion:
		return TEXT("PROMOTION");
	case EConvEnumAliasForm::ExplicitCast:
		return TEXT("EXPLICIT_CAST");
	default:
		return nullptr;
	}
}

const TCHAR* ValueToken(const EConvEnumAliasValue Value)
{
	switch (Value)
	{
	case EConvEnumAliasValue::Zero:
		return TEXT("ZERO");
	case EConvEnumAliasValue::One:
		return TEXT("ONE");
	case EConvEnumAliasValue::Negative:
		return TEXT("NEGATIVE");
	case EConvEnumAliasValue::NearMin:
		return TEXT("NEAR_MIN");
	case EConvEnumAliasValue::NearMax:
		return TEXT("NEAR_MAX");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FConvEnumAliasParams& Params)
{
	return SourceToken(Params.Source) != nullptr
		&& TargetToken(Params.Target) != nullptr
		&& FormToken(Params.Form) != nullptr
		&& ValueToken(Params.Value) != nullptr;
}

bool IsAliasSource(const EConvEnumAliasSource Source)
{
	return Source != EConvEnumAliasSource::Enum;
}

FConvEnumAliasParams ParamsFromCell(const FConvEnumAliasCell& Cell)
{
	FConvEnumAliasParams Params;
	Params.Source = Cell.Source;
	Params.Target = Cell.Target;
	Params.Form = Cell.Form;
	Params.Value = Cell.Value;
	return Params;
}

bool IsReject(const FConvEnumAliasParams& Params)
{
	if (IsAliasSource(Params.Source) && Params.Target == EConvEnumAliasTarget::Enum
		&& Params.Form != EConvEnumAliasForm::ExplicitCast)
	{
		return true;
	}
	return Params.Source == EConvEnumAliasSource::Enum
		&& Params.Target == EConvEnumAliasTarget::Enum
		&& Params.Form == EConvEnumAliasForm::Promotion;
}

bool IsRejectCell(const FConvEnumAliasCell& Cell)
{
	return IsReject(ParamsFromCell(Cell));
}

const TCHAR* SourceScriptType(const EConvEnumAliasSource Source)
{
	switch (Source)
	{
	case EConvEnumAliasSource::Enum:
		return TEXT("EConversionEnum");
	case EConvEnumAliasSource::AliasInt8:
		return TEXT("AliasInt8");
	case EConvEnumAliasSource::AliasInt:
		return TEXT("AliasInt");
	case EConvEnumAliasSource::AliasInt64:
		return TEXT("AliasInt64");
	case EConvEnumAliasSource::AliasUint:
		return TEXT("AliasUInt");
	case EConvEnumAliasSource::AliasUint64:
		return TEXT("AliasUInt64");
	default:
		return TEXT("int");
	}
}

const TCHAR* TargetScriptType(const EConvEnumAliasTarget Target)
{
	switch (Target)
	{
	case EConvEnumAliasTarget::Enum:
		return TEXT("EConversionEnum");
	case EConvEnumAliasTarget::Int8:
		return TEXT("int8");
	case EConvEnumAliasTarget::Int:
		return TEXT("int");
	case EConvEnumAliasTarget::Int64:
		return TEXT("int64");
	case EConvEnumAliasTarget::Uint:
		return TEXT("uint");
	case EConvEnumAliasTarget::Uint64:
		return TEXT("uint64");
	case EConvEnumAliasTarget::Float64:
		return TEXT("float");
	default:
		return TEXT("int");
	}
}

const TCHAR* SourceLiteral(const EConvEnumAliasSource Source, const EConvEnumAliasValue Value)
{
	if (Source == EConvEnumAliasSource::Enum)
	{
		switch (Value)
		{
		case EConvEnumAliasValue::Zero:
			return TEXT("EConversionEnum::Zero");
		case EConvEnumAliasValue::One:
			return TEXT("EConversionEnum::One");
		case EConvEnumAliasValue::Negative:
			return TEXT("EConversionEnum::Negative");
		case EConvEnumAliasValue::NearMin:
			return TEXT("EConversionEnum::NearMinimum");
		default:
			return TEXT("EConversionEnum::NearMaximum");
		}
	}
	switch (Value)
	{
	case EConvEnumAliasValue::Zero:
		return TEXT("0");
	case EConvEnumAliasValue::One:
		return TEXT("1");
	case EConvEnumAliasValue::Negative:
		return TEXT("-1");
	case EConvEnumAliasValue::NearMin:
		return TEXT("-126");
	default:
		return TEXT("126");
	}
}

int64 DeclaredValue(const EConvEnumAliasValue Value)
{
	switch (Value)
	{
	case EConvEnumAliasValue::Zero:
		return 0;
	case EConvEnumAliasValue::One:
		return 1;
	case EConvEnumAliasValue::Negative:
		return -1;
	case EConvEnumAliasValue::NearMin:
		return -126;
	default:
		return 126;
	}
}

int64 SourceValueForCase(const EConvEnumAliasSource Source, const EConvEnumAliasValue Value)
{
	const int64 Declared = DeclaredValue(Value);
	if (Source == EConvEnumAliasSource::AliasUint)
	{
		return static_cast<int64>(static_cast<uint32>(Declared));
	}
	if (Source == EConvEnumAliasSource::AliasUint64)
	{
		return static_cast<int64>(static_cast<uint64>(Declared));
	}
	return Declared;
}

uint64 NormalizeUnsigned(const int64 Value, const uint8 WidthInBytes)
{
	const uint64 Mask = WidthInBytes == sizeof(uint64)
		? MAX_uint64
		: ((uint64(1) << (WidthInBytes * 8)) - 1);
	return static_cast<uint64>(Value) & Mask;
}

int64 NormalizeSigned(const int64 Value, const uint8 WidthInBytes)
{
	const uint64 UnsignedValue = NormalizeUnsigned(Value, WidthInBytes);
	if (WidthInBytes == sizeof(uint64))
	{
		return static_cast<int64>(UnsignedValue);
	}
	const uint64 SignBit = uint64(1) << (WidthInBytes * 8 - 1);
	if ((UnsignedValue & SignBit) == 0)
	{
		return static_cast<int64>(UnsignedValue);
	}
	const uint64 SignExtended = UnsignedValue | ~((uint64(1) << (WidthInBytes * 8)) - 1);
	return static_cast<int64>(SignExtended);
}

FString ExpectedLiteral(const EConvEnumAliasSource Source, const EConvEnumAliasTarget Target, const EConvEnumAliasValue Value)
{
	if (Target == EConvEnumAliasTarget::Enum)
	{
		switch (Value)
		{
		case EConvEnumAliasValue::Zero:
			return TEXT("EConversionEnum::Zero");
		case EConvEnumAliasValue::One:
			return TEXT("EConversionEnum::One");
		case EConvEnumAliasValue::Negative:
			return TEXT("EConversionEnum::Negative");
		case EConvEnumAliasValue::NearMin:
			return TEXT("EConversionEnum::NearMinimum");
		default:
			return TEXT("EConversionEnum::NearMaximum");
		}
	}
	const int64 SourceValue = SourceValueForCase(Source, Value);
	if (Target == EConvEnumAliasTarget::Float64)
	{
		const double FloatingValue = Source == EConvEnumAliasSource::AliasUint64
			? static_cast<double>(static_cast<uint64>(SourceValue))
			: static_cast<double>(SourceValue);
		return FString::Printf(TEXT("%.17g"), FloatingValue);
	}
	if (Target == EConvEnumAliasTarget::Uint)
	{
		return LexToString(NormalizeUnsigned(SourceValue, 4));
	}
	if (Target == EConvEnumAliasTarget::Uint64)
	{
		return LexToString(NormalizeUnsigned(SourceValue, 8));
	}
	if (Target == EConvEnumAliasTarget::Int8)
	{
		return LexToString(NormalizeSigned(SourceValue, 1));
	}
	if (Target == EConvEnumAliasTarget::Int64)
	{
		return LexToString(NormalizeSigned(SourceValue, 8));
	}
	return LexToString(NormalizeSigned(SourceValue, 4));
}

FString AxisPascal(const TCHAR* Token)
{
	return MakeCanonicalEntryName(Token).Mid(5);
}

FString PassHelperName(const EConvEnumAliasTarget Target)
{
	return FString::Printf(TEXT("PassEnumAliasTarget%s"), *AxisPascal(TargetToken(Target)));
}

FString ReturnHelperName(const EConvEnumAliasSource Source, const EConvEnumAliasTarget Target)
{
	return FString::Printf(
		TEXT("ReturnEnumAliasTarget%s%s"),
		*AxisPascal(SourceToken(Source)),
		*AxisPascal(TargetToken(Target)));
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FConvEnumAliasCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CONV-ENUM-ALIAS-%s-%s-%s-%s"),
		SourceToken(Cell.Source),
		TargetToken(Cell.Target),
		FormToken(Cell.Form),
		ValueToken(Cell.Value));
}

FString MakeEntryName(const FConvEnumAliasCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

TArray<FConvEnumAliasCell> AllCells()
{
	TArray<FConvEnumAliasCell> Cells;
	Cells.SetNum(1260);
	for (int32 Index = 0; Index < 1260; ++Index)
	{
		FConvEnumAliasCell& Cell = Cells[Index];
		Cell.Source = static_cast<EConvEnumAliasSource>(Index / 210);
		Cell.Target = static_cast<EConvEnumAliasTarget>((Index / 30) % 7);
		Cell.Form = static_cast<EConvEnumAliasForm>((Index / 5) % 6);
		Cell.Value = static_cast<EConvEnumAliasValue>(Index % 5);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FConvEnumAliasCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FConvEnumAliasCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendSharedTypes(FString& Source)
{
	AppendLine(Source, TEXT("enum EConversionEnum"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tNearMinimum = -126,"));
	AppendLine(Source, TEXT("\tNegative = -1,"));
	AppendLine(Source, TEXT("\tZero = 0,"));
	AppendLine(Source, TEXT("\tOne = 1,"));
	AppendLine(Source, TEXT("\tNearMaximum = 126"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("typedef int8 AliasInt8;"));
	AppendLine(Source, TEXT("typedef int AliasInt;"));
	AppendLine(Source, TEXT("typedef int64 AliasInt64;"));
	AppendLine(Source, TEXT("typedef uint AliasUInt;"));
	AppendLine(Source, TEXT("typedef uint64 AliasUInt64;"));
}

void AppendPassHelper(FString& Source, const EConvEnumAliasTarget Target)
{
	const TCHAR* const Type = TargetScriptType(Target);
	const FString Name = PassHelperName(Target);
	AppendLine(Source, FString::Printf(TEXT("%s %s(%s Value)"), Type, *Name, Type));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendReturnHelper(FString& Source, const EConvEnumAliasSource SourceKind, const EConvEnumAliasTarget Target)
{
	const TCHAR* const TargetType = TargetScriptType(Target);
	const TCHAR* const SourceType = SourceScriptType(SourceKind);
	const FString Name = ReturnHelperName(SourceKind, Target);
	AppendLine(Source, FString::Printf(TEXT("%s %s(%s Value)"), TargetType, *Name, SourceType));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendAggregateHelpers(FString& Source)
{
	AppendSharedTypes(Source);
	AppendLine(Source);
	for (int32 TargetIndex = 0; TargetIndex < 7; ++TargetIndex)
	{
		AppendPassHelper(Source, static_cast<EConvEnumAliasTarget>(TargetIndex));
		AppendLine(Source);
	}
	for (int32 SourceIndex = 0; SourceIndex < 6; ++SourceIndex)
	{
		for (int32 TargetIndex = 0; TargetIndex < 7; ++TargetIndex)
		{
			AppendReturnHelper(
				Source,
				static_cast<EConvEnumAliasSource>(SourceIndex),
				static_cast<EConvEnumAliasTarget>(TargetIndex));
			AppendLine(Source);
		}
	}
}

void AppendOwnedHelpers(FString& Source, const FConvEnumAliasParams& Params)
{
	AppendSharedTypes(Source);
	AppendLine(Source);
	if (Params.Form == EConvEnumAliasForm::Argument)
	{
		AppendPassHelper(Source, Params.Target);
		AppendLine(Source);
	}
	else if (Params.Form == EConvEnumAliasForm::Return)
	{
		AppendReturnHelper(Source, Params.Source, Params.Target);
		AppendLine(Source);
	}
}

void AppendEntry(FString& Source, const FConvEnumAliasParams& Params, const FString& FunctionName)
{
	const TCHAR* const TargetType = TargetScriptType(Params.Target);
	const TCHAR* const SourceType = SourceScriptType(Params.Source);
	const FString Literal = ExpectedLiteral(Params.Source, Params.Target, Params.Value);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(
		TEXT("\t%s SourceValue = %s;"),
		SourceType,
		SourceLiteral(Params.Source, Params.Value)));
	if (Params.Form == EConvEnumAliasForm::Assignment)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Converted;"), TargetType));
		AppendLine(Source, TEXT("\tConverted = SourceValue;"));
	}
	else if (Params.Form == EConvEnumAliasForm::Initializer)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Converted = SourceValue;"), TargetType));
	}
	else if (Params.Form == EConvEnumAliasForm::Argument)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Converted = %s(SourceValue);"),
			TargetType,
			*PassHelperName(Params.Target)));
	}
	else if (Params.Form == EConvEnumAliasForm::Return)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Converted = %s(SourceValue);"),
			TargetType,
			*ReturnHelperName(Params.Source, Params.Target)));
	}
	else if (Params.Form == EConvEnumAliasForm::Promotion)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Converted = SourceValue + %s(0);"),
			TargetType,
			TargetType));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Converted = %s(SourceValue);"),
			TargetType,
			TargetType));
	}
	AppendLine(Source, FString::Printf(TEXT("\t%s ActualValue = Converted;"), TargetType));
	AppendLine(Source, FString::Printf(TEXT("\t%s ExpectedValue = %s;"), TargetType, *Literal));
	AppendLine(Source, TEXT("\treturn ActualValue == ExpectedValue ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FConvEnumAliasParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FConvEnumAliasCell& Cell)
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
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = 1;
	return Info;
}
}

FStringView FConvEnumAliasGenerator::GetProductId() const
{
	return TEXT("LANG-CONV-ENUM-ALIAS");
}

TArray<FGeneratedCaseInfo> FConvEnumAliasGenerator::ListCases() const
{
	using namespace ConvEnumAliasGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FConvEnumAliasCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("ConvEnumAlias PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FConvEnumAliasGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ConvEnumAliasGeneratorDetail;
	FConvEnumAliasCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FConvEnumAliasGenerator::BuildDumpSource() const
{
	using namespace ConvEnumAliasGeneratorDetail;
	const TArray<FConvEnumAliasCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CONV-ENUM-ALIAS"));
	AppendLine(Source, TEXT("// Cases: 1260 total, 1130 normal, 130 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendAggregateHelpers(Source);
	bool bNeedBlank = true;
	for (const FConvEnumAliasCell& Cell : Cells)
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
		AppendLine(Source, TEXT("// ExpectedReturn: 1"));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	for (const FConvEnumAliasCell& Cell : Cells)
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
		AppendOwnedHelpers(Source, ParamsFromCell(Cell));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FConvEnumAliasGenerator::BuildEnumAliasConversionSource(const FConvEnumAliasParams& Params) const
{
	using namespace ConvEnumAliasGeneratorDetail;
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

FString FConvEnumAliasGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ConvEnumAliasGeneratorDetail;
	const TArray<FConvEnumAliasCell> Cells = AllCells();
	FString Source;
	AppendAggregateHelpers(Source);
	int32 Emitted = 0;
	for (const FConvEnumAliasCell& Cell : Cells)
	{
		if (IsRejectCell(Cell))
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

int32 FConvEnumAliasGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ConvEnumAliasGeneratorDetail;
	FConvEnumAliasCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsRejectCell(Cell))
	{
		return 0;
	}
	return 1;
}

FString FConvEnumAliasGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ConvEnumAliasGeneratorDetail;
	FConvEnumAliasCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsRejectCell(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FConvEnumAliasGenerator::ListRejectCaseIds() const
{
	using namespace ConvEnumAliasGeneratorDetail;
	TArray<FString> Ids;
	for (const FConvEnumAliasCell& Cell : AllCells())
	{
		if (IsRejectCell(Cell))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
