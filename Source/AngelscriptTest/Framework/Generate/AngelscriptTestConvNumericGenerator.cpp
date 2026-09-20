#include "Framework/Generate/AngelscriptTestConvNumericGenerator.h"

#include "Containers/Set.h"

#include <cstdlib>

namespace AngelscriptTest::Generate
{
namespace ConvNumericGeneratorDetail
{
constexpr int32 TypeCount = 10;
constexpr int32 FormCount = 6;
constexpr int32 ValueCount = 7;
constexpr int32 TotalCells = TypeCount * TypeCount * FormCount * ValueCount;

struct FNumericTypeInfo
{
	EConvNumericType Type = EConvNumericType::Int8;
	const TCHAR* Token = nullptr;
	const TCHAR* ScriptType = nullptr;
	const TCHAR* HelperSuffix = nullptr;
	const TCHAR* ZeroLiteral = nullptr;
	const TCHAR* OneLiteral = nullptr;
	const TCHAR* MinimumLiteral = nullptr;
	const TCHAR* MaximumLiteral = nullptr;
	const TCHAR* NearBoundaryLiteral = nullptr;
	uint8 WidthInBytes = 1;
	bool bSigned = true;
	bool bFloating = false;
};

const FNumericTypeInfo TypeInfos[TypeCount] = {
	{EConvNumericType::Int8, TEXT("INT8"), TEXT("int8"), TEXT("Int8"), TEXT("0"), TEXT("1"), TEXT("-128"), TEXT("127"), TEXT("126"), 1, true, false},
	{EConvNumericType::Int16, TEXT("INT16"), TEXT("int16"), TEXT("Int16"), TEXT("0"), TEXT("1"), TEXT("-32768"), TEXT("32767"), TEXT("32766"), 2, true, false},
	{EConvNumericType::Int, TEXT("INT"), TEXT("int"), TEXT("Int"), TEXT("0"), TEXT("1"), TEXT("-2147483647 - 1"), TEXT("2147483647"), TEXT("2147483646"), 4, true, false},
	{EConvNumericType::Int64, TEXT("INT64"), TEXT("int64"), TEXT("Int64"), TEXT("0"), TEXT("1"), TEXT("-9223372036854775807 - 1"), TEXT("9223372036854775807"), TEXT("9223372036854775806"), 8, true, false},
	{EConvNumericType::Uint8, TEXT("UINT8"), TEXT("uint8"), TEXT("Uint8"), TEXT("0"), TEXT("1"), TEXT("0"), TEXT("255"), TEXT("254"), 1, false, false},
	{EConvNumericType::Uint16, TEXT("UINT16"), TEXT("uint16"), TEXT("Uint16"), TEXT("0"), TEXT("1"), TEXT("0"), TEXT("65535"), TEXT("65534"), 2, false, false},
	{EConvNumericType::Uint, TEXT("UINT"), TEXT("uint"), TEXT("Uint"), TEXT("0"), TEXT("1"), TEXT("0"), TEXT("4294967295"), TEXT("4294967294"), 4, false, false},
	{EConvNumericType::Uint64, TEXT("UINT64"), TEXT("uint64"), TEXT("Uint64"), TEXT("0"), TEXT("1"), TEXT("0"), TEXT("18446744073709551615"), TEXT("18446744073709551614"), 8, false, false},
	{EConvNumericType::Float32, TEXT("FLOAT32"), TEXT("float"), TEXT("Float"), TEXT("0.0f"), TEXT("1.0f"), TEXT("-3.402823466e+38f"), TEXT("3.402823466e+38f"), TEXT("3.402823263e+38f"), 4, true, true},
	{EConvNumericType::Float64, TEXT("FLOAT64"), TEXT("double"), TEXT("Double"), TEXT("0.0"), TEXT("1.0"), TEXT("-1.7976931348623157e+308"), TEXT("1.7976931348623157e+308"), TEXT("1.7976931348623155e+308"), 8, true, true},
};

struct FNumericCell
{
	EConvNumericType Source = EConvNumericType::Int8;
	EConvNumericType Target = EConvNumericType::Int8;
	EConvNumericForm Form = EConvNumericForm::Assignment;
	EConvNumericValue Value = EConvNumericValue::Zero;
};

const TCHAR* TypeToken(const EConvNumericType Type)
{
	const int32 Index = static_cast<int32>(Type);
	return (Index >= 0 && Index < TypeCount) ? TypeInfos[Index].Token : nullptr;
}

const TCHAR* FormToken(const EConvNumericForm Form)
{
	switch (Form)
	{
	case EConvNumericForm::Assignment:
		return TEXT("ASSIGNMENT");
	case EConvNumericForm::Initializer:
		return TEXT("INITIALIZER");
	case EConvNumericForm::Argument:
		return TEXT("ARGUMENT");
	case EConvNumericForm::Return:
		return TEXT("RETURN");
	case EConvNumericForm::Promotion:
		return TEXT("PROMOTION");
	case EConvNumericForm::ExplicitCast:
		return TEXT("EXPLICIT_CAST");
	default:
		return nullptr;
	}
}

const TCHAR* ValueToken(const EConvNumericValue Value)
{
	switch (Value)
	{
	case EConvNumericValue::Zero:
		return TEXT("ZERO");
	case EConvNumericValue::One:
		return TEXT("ONE");
	case EConvNumericValue::Negative:
		return TEXT("NEGATIVE");
	case EConvNumericValue::Min:
		return TEXT("MIN");
	case EConvNumericValue::Max:
		return TEXT("MAX");
	case EConvNumericValue::NearBoundary:
		return TEXT("NEAR_BOUNDARY");
	case EConvNumericValue::Fractional:
		return TEXT("FRACTIONAL");
	default:
		return nullptr;
	}
}

const FNumericTypeInfo* FindType(const EConvNumericType Type)
{
	const int32 Index = static_cast<int32>(Type);
	return (Index >= 0 && Index < TypeCount) ? &TypeInfos[Index] : nullptr;
}

bool IsValidParams(const FConvNumericParams& Params)
{
	return FindType(Params.Source) != nullptr
		&& FindType(Params.Target) != nullptr
		&& FormToken(Params.Form) != nullptr
		&& ValueToken(Params.Value) != nullptr;
}

const TCHAR* SourceLiteral(const FNumericTypeInfo& Type, const EConvNumericValue Value)
{
	switch (Value)
	{
	case EConvNumericValue::Zero:
		return Type.ZeroLiteral;
	case EConvNumericValue::One:
		return Type.OneLiteral;
	case EConvNumericValue::Negative:
		return Type.bFloating || Type.bSigned ? TEXT("-1") : Type.MaximumLiteral;
	case EConvNumericValue::Min:
		return Type.MinimumLiteral;
	case EConvNumericValue::Max:
		return Type.MaximumLiteral;
	case EConvNumericValue::NearBoundary:
		return Type.NearBoundaryLiteral;
	case EConvNumericValue::Fractional:
		return Type.bFloating ? (Type.Type == EConvNumericType::Float32 ? TEXT("1.5f") : TEXT("1.5")) : TEXT("1");
	default:
		return nullptr;
	}
}

const ANSICHAR* SourceLiteralAnsi(const FNumericTypeInfo& Type, const EConvNumericValue Value)
{
	switch (Value)
	{
	case EConvNumericValue::Zero:
		return Type.Type == EConvNumericType::Float32 ? "0.0f" : Type.Type == EConvNumericType::Float64 ? "0.0" : "0";
	case EConvNumericValue::One:
		return Type.Type == EConvNumericType::Float32 ? "1.0f" : Type.Type == EConvNumericType::Float64 ? "1.0" : "1";
	case EConvNumericValue::Negative:
		if (Type.bFloating || Type.bSigned)
		{
			return "-1";
		}
		if (Type.Type == EConvNumericType::Uint8)
		{
			return "255";
		}
		if (Type.Type == EConvNumericType::Uint16)
		{
			return "65535";
		}
		if (Type.Type == EConvNumericType::Uint)
		{
			return "4294967295";
		}
		return "18446744073709551615";
	case EConvNumericValue::Min:
		if (Type.Type == EConvNumericType::Int8)
		{
			return "-128";
		}
		if (Type.Type == EConvNumericType::Int16)
		{
			return "-32768";
		}
		if (Type.Type == EConvNumericType::Int)
		{
			return "-2147483647 - 1";
		}
		if (Type.Type == EConvNumericType::Int64)
		{
			return "-9223372036854775807 - 1";
		}
		if (Type.Type == EConvNumericType::Float32)
		{
			return "-3.402823466e+38f";
		}
		if (Type.Type == EConvNumericType::Float64)
		{
			return "-1.7976931348623157e+308";
		}
		return "0";
	case EConvNumericValue::Max:
		if (Type.Type == EConvNumericType::Int8)
		{
			return "127";
		}
		if (Type.Type == EConvNumericType::Int16)
		{
			return "32767";
		}
		if (Type.Type == EConvNumericType::Int)
		{
			return "2147483647";
		}
		if (Type.Type == EConvNumericType::Int64)
		{
			return "9223372036854775807";
		}
		if (Type.Type == EConvNumericType::Uint8)
		{
			return "255";
		}
		if (Type.Type == EConvNumericType::Uint16)
		{
			return "65535";
		}
		if (Type.Type == EConvNumericType::Uint)
		{
			return "4294967295";
		}
		if (Type.Type == EConvNumericType::Uint64)
		{
			return "18446744073709551615";
		}
		if (Type.Type == EConvNumericType::Float32)
		{
			return "3.402823466e+38f";
		}
		return "1.7976931348623157e+308";
	case EConvNumericValue::NearBoundary:
		if (Type.Type == EConvNumericType::Int8)
		{
			return "126";
		}
		if (Type.Type == EConvNumericType::Int16)
		{
			return "32766";
		}
		if (Type.Type == EConvNumericType::Int)
		{
			return "2147483646";
		}
		if (Type.Type == EConvNumericType::Int64)
		{
			return "9223372036854775806";
		}
		if (Type.Type == EConvNumericType::Uint8)
		{
			return "254";
		}
		if (Type.Type == EConvNumericType::Uint16)
		{
			return "65534";
		}
		if (Type.Type == EConvNumericType::Uint)
		{
			return "4294967294";
		}
		if (Type.Type == EConvNumericType::Uint64)
		{
			return "18446744073709551614";
		}
		if (Type.Type == EConvNumericType::Float32)
		{
			return "3.402823263e+38f";
		}
		return "1.7976931348623155e+308";
	case EConvNumericValue::Fractional:
		return Type.Type == EConvNumericType::Float32 ? "1.5f" : Type.bFloating ? "1.5" : "1";
	default:
		return "0";
	}
}

int64 SignedValueForCase(const FNumericTypeInfo& Type, const EConvNumericValue Value)
{
	const uint32 BitCount = static_cast<uint32>(Type.WidthInBytes) * 8u;
	if (Value == EConvNumericValue::Zero)
	{
		return 0;
	}
	if (Value == EConvNumericValue::One || Value == EConvNumericValue::Fractional)
	{
		return 1;
	}
	if (Value == EConvNumericValue::Negative)
	{
		return -1;
	}
	if (Value == EConvNumericValue::Min)
	{
		return BitCount == 64u ? MIN_int64 : -(int64(1) << (BitCount - 1u));
	}
	const int64 Maximum = BitCount == 64u ? MAX_int64 : ((int64(1) << (BitCount - 1u)) - 1);
	return Value == EConvNumericValue::NearBoundary ? Maximum - 1 : Maximum;
}

uint64 UnsignedValueForCase(const FNumericTypeInfo& Type, const EConvNumericValue Value)
{
	if (Value == EConvNumericValue::Zero || Value == EConvNumericValue::Min)
	{
		return 0;
	}
	if (Value == EConvNumericValue::One || Value == EConvNumericValue::Fractional)
	{
		return 1;
	}
	const uint32 BitCount = static_cast<uint32>(Type.WidthInBytes) * 8u;
	const uint64 Maximum = BitCount == 64u ? MAX_uint64 : ((uint64(1) << BitCount) - 1u);
	return Value == EConvNumericValue::NearBoundary ? Maximum - 1 : Maximum;
}

double FloatingValueForCase(const FNumericTypeInfo& Type, const EConvNumericValue Value)
{
	const double Parsed = std::strtod(SourceLiteralAnsi(Type, Value), nullptr);
	return Type.Type == EConvNumericType::Float32 ? static_cast<double>(static_cast<float>(Parsed)) : Parsed;
}

bool HasPortableExactValue(
	const FNumericTypeInfo& Source,
	const FNumericTypeInfo& Target,
	const EConvNumericValue Value)
{
	if (!Source.bFloating || Target.bFloating)
	{
		return true;
	}
	if (Value == EConvNumericValue::Zero
		|| Value == EConvNumericValue::One
		|| Value == EConvNumericValue::Fractional)
	{
		return true;
	}
	return Value == EConvNumericValue::Negative && Target.bSigned;
}

FString BuildIndependentExpectedLiteral(
	const FNumericTypeInfo& Source,
	const FNumericTypeInfo& Target,
	const EConvNumericValue Value)
{
	if (Target.bFloating)
	{
		if (Target.Type == EConvNumericType::Float32)
		{
			const float Expected = static_cast<float>(Source.bFloating
				? FloatingValueForCase(Source, Value)
				: Source.bSigned
					? static_cast<double>(SignedValueForCase(Source, Value))
					: static_cast<double>(UnsignedValueForCase(Source, Value)));
			return FString::Printf(TEXT("%.9gf"), static_cast<double>(Expected));
		}
		const double Expected = Source.bFloating
			? FloatingValueForCase(Source, Value)
			: Source.bSigned
				? static_cast<double>(SignedValueForCase(Source, Value))
				: static_cast<double>(UnsignedValueForCase(Source, Value));
		return FString::Printf(TEXT("%.17g"), Expected);
	}
	if (!HasPortableExactValue(Source, Target, Value))
	{
		return FString();
	}

	uint64 ConvertedBits = 0;
	if (Source.bFloating)
	{
		const double SourceValue = FloatingValueForCase(Source, Value);
		ConvertedBits = Target.bSigned
			? static_cast<uint64>(static_cast<int64>(SourceValue))
			: static_cast<uint64>(SourceValue);
	}
	else if (Source.bSigned)
	{
		ConvertedBits = static_cast<uint64>(SignedValueForCase(Source, Value));
	}
	else
	{
		ConvertedBits = UnsignedValueForCase(Source, Value);
	}

	const uint32 BitCount = static_cast<uint32>(Target.WidthInBytes) * 8u;
	const uint64 Mask = BitCount == 64u ? MAX_uint64 : ((uint64(1) << BitCount) - 1u);
	ConvertedBits &= Mask;
	if (Target.bSigned)
	{
		const uint64 SignBit = BitCount == 64u ? (uint64(1) << 63u) : (uint64(1) << (BitCount - 1u));
		const int64 Expected = (ConvertedBits & SignBit) != 0
			? static_cast<int64>(ConvertedBits | ~Mask)
			: static_cast<int64>(ConvertedBits);
		return FString::Printf(TEXT("%lld"), static_cast<long long>(Expected));
	}
	return FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(ConvertedBits));
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FNumericCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CONV-NUMERIC-%s-%s-%s-%s"),
		TypeToken(Cell.Source),
		TypeToken(Cell.Target),
		FormToken(Cell.Form),
		ValueToken(Cell.Value));
}

FString MakeEntryName(const FNumericCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FConvNumericParams ParamsFromCell(const FNumericCell& Cell)
{
	FConvNumericParams Params;
	Params.Source = Cell.Source;
	Params.Target = Cell.Target;
	Params.Form = Cell.Form;
	Params.Value = Cell.Value;
	return Params;
}

FNumericCell CellFromIndex(const int32 Index)
{
	FNumericCell Cell;
	Cell.Source = static_cast<EConvNumericType>(Index / (TypeCount * FormCount * ValueCount));
	const int32 AfterSource = Index % (TypeCount * FormCount * ValueCount);
	Cell.Target = static_cast<EConvNumericType>(AfterSource / (FormCount * ValueCount));
	const int32 AfterTarget = AfterSource % (FormCount * ValueCount);
	Cell.Form = static_cast<EConvNumericForm>(AfterTarget / ValueCount);
	Cell.Value = static_cast<EConvNumericValue>(AfterTarget % ValueCount);
	return Cell;
}

bool TryFindCell(const FStringView CaseId, FNumericCell& OutCell)
{
	const FString Needle(CaseId);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FNumericCell Cell = CellFromIndex(Index);
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendPassHelper(FString& Source, const FNumericTypeInfo& Target)
{
	AppendLine(Source, FString::Printf(
		TEXT("%s PassNumeric%s(%s Value)"),
		Target.ScriptType,
		Target.HelperSuffix,
		Target.ScriptType));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendReturnHelper(FString& Source, const FNumericTypeInfo& SourceType, const FNumericTypeInfo& Target)
{
	AppendLine(Source, FString::Printf(
		TEXT("%s ReturnNumeric%s(%s Value)"),
		Target.ScriptType,
		Target.HelperSuffix,
		SourceType.ScriptType));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendSharedHelpers(FString& Source)
{
	for (int32 TargetIndex = 0; TargetIndex < TypeCount; ++TargetIndex)
	{
		if (TargetIndex > 0)
		{
			AppendLine(Source);
		}
		AppendPassHelper(Source, TypeInfos[TargetIndex]);
	}
	for (int32 TargetIndex = 0; TargetIndex < TypeCount; ++TargetIndex)
	{
		for (int32 SourceIndex = 0; SourceIndex < TypeCount; ++SourceIndex)
		{
			AppendLine(Source);
			AppendReturnHelper(Source, TypeInfos[SourceIndex], TypeInfos[TargetIndex]);
		}
	}
}

void AppendOwnedHelpers(FString& Source, const FConvNumericParams& Params)
{
	const FNumericTypeInfo* const SourceType = FindType(Params.Source);
	const FNumericTypeInfo* const TargetType = FindType(Params.Target);
	if (SourceType == nullptr || TargetType == nullptr)
	{
		return;
	}
	if (Params.Form == EConvNumericForm::Argument)
	{
		AppendPassHelper(Source, *TargetType);
		AppendLine(Source);
	}
	else if (Params.Form == EConvNumericForm::Return)
	{
		AppendReturnHelper(Source, *SourceType, *TargetType);
		AppendLine(Source);
	}
}

void AppendEntry(FString& Source, const FConvNumericParams& Params, const FString& FunctionName)
{
	const FNumericTypeInfo* const SourceType = FindType(Params.Source);
	const FNumericTypeInfo* const TargetType = FindType(Params.Target);
	if (SourceType == nullptr || TargetType == nullptr)
	{
		return;
	}
	const TCHAR* const Literal = SourceLiteral(*SourceType, Params.Value);
	const bool bPortable = HasPortableExactValue(*SourceType, *TargetType, Params.Value);
	const FString ExpectedLiteral = bPortable
		? BuildIndependentExpectedLiteral(*SourceType, *TargetType, Params.Value)
		: FString();

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s SourceValue = %s;"), SourceType->ScriptType, Literal));
	if (Params.Form == EConvNumericForm::Assignment)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Converted;"), TargetType->ScriptType));
		AppendLine(Source, TEXT("\tConverted = SourceValue;"));
	}
	else if (Params.Form == EConvNumericForm::Initializer)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Converted = SourceValue;"), TargetType->ScriptType));
	}
	else if (Params.Form == EConvNumericForm::Argument)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Converted = PassNumeric%s(SourceValue);"),
			TargetType->ScriptType,
			TargetType->HelperSuffix));
	}
	else if (Params.Form == EConvNumericForm::Return)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Converted = ReturnNumeric%s(SourceValue);"),
			TargetType->ScriptType,
			TargetType->HelperSuffix));
	}
	else if (Params.Form == EConvNumericForm::Promotion)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Converted = SourceValue + %s(0);"),
			TargetType->ScriptType,
			TargetType->ScriptType));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Converted = %s(SourceValue);"),
			TargetType->ScriptType,
			TargetType->ScriptType));
	}
	if (bPortable)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s ExpectedValue = %s(%s);"),
			TargetType->ScriptType,
			TargetType->ScriptType,
			*ExpectedLiteral));
		AppendLine(Source, TEXT("\treturn Converted == ExpectedValue ? 1 : 0;"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn 1;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FConvNumericParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FNumericCell& Cell)
{
	const FNumericTypeInfo* const SourceType = FindType(Cell.Source);
	const FNumericTypeInfo* const TargetType = FindType(Cell.Target);
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = 1;
	if (SourceType != nullptr && TargetType != nullptr
		&& !HasPortableExactValue(*SourceType, *TargetType, Cell.Value))
	{
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = TEXT("exact floating-to-integer result is not portable for this source value");
	}
	return Info;
}
}

FStringView FConvNumericGenerator::GetProductId() const
{
	return TEXT("LANG-CONV-NUMERIC");
}

TArray<FGeneratedCaseInfo> FConvNumericGenerator::ListCases() const
{
	using namespace ConvNumericGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	Cases.Reserve(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FNumericCell Cell = CellFromIndex(Index);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("ConvNumeric PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FConvNumericGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ConvNumericGeneratorDetail;
	FNumericCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FConvNumericGenerator::BuildDumpSource() const
{
	using namespace ConvNumericGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CONV-NUMERIC"));
	AppendLine(Source, TEXT("// Cases: 4200 total, 4200 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FNumericCell Cell = CellFromIndex(Index);
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, TEXT("// ExpectedReturn: 1"));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		if (Info.bLimitedObservation)
		{
			AppendLine(Source, TEXT("// LimitedObservation: true"));
			AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FConvNumericGenerator::BuildNumericConversionSource(const FConvNumericParams& Params) const
{
	using namespace ConvNumericGeneratorDetail;
	if (!IsValidParams(Params))
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

FString FConvNumericGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ConvNumericGeneratorDetail;
	FString Source;
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(CellFromIndex(Index)), MakeEntryName(CellFromIndex(Index)));
	}
	OutCaseCount = TotalCells;
	return Source;
}

int32 FConvNumericGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ConvNumericGeneratorDetail;
	FNumericCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return 1;
}
}
