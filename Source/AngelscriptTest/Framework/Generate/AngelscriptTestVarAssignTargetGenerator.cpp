#include "Framework/Generate/AngelscriptTestVarAssignTargetGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace VarAssignTargetGeneratorDetail
{
constexpr int32 TypeCount = 17;
constexpr int32 TargetCount = 7;

struct FAssignCell
{
	EVarAssignKind Assignment = EVarAssignKind::Simple;
	EVarAssignTarget Target = EVarAssignTarget::MutableLocal;
	EVarAssignType Type = EVarAssignType::Int8;
};

const TCHAR* AssignmentToken(const EVarAssignKind Assignment)
{
	switch (Assignment)
	{
	case EVarAssignKind::Simple:
		return TEXT("SIMPLE");
	case EVarAssignKind::CopySource:
		return TEXT("COPY_SOURCE");
	case EVarAssignKind::SelfAssignment:
		return TEXT("SELF_ASSIGNMENT");
	case EVarAssignKind::Compound:
		return TEXT("COMPOUND");
	case EVarAssignKind::ReferenceRebind:
		return TEXT("REFERENCE_REBIND");
	default:
		return nullptr;
	}
}

const TCHAR* TargetToken(const EVarAssignTarget Target)
{
	switch (Target)
	{
	case EVarAssignTarget::MutableLocal:
		return TEXT("MUTABLE_LOCAL");
	case EVarAssignTarget::ConstLocal:
		return TEXT("CONST_LOCAL");
	case EVarAssignTarget::MutableField:
		return TEXT("MUTABLE_FIELD");
	case EVarAssignTarget::ConstField:
		return TEXT("CONST_FIELD");
	case EVarAssignTarget::ReferenceAlias:
		return TEXT("REFERENCE_ALIAS");
	case EVarAssignTarget::Temporary:
		return TEXT("TEMPORARY");
	case EVarAssignTarget::ExpressionResult:
		return TEXT("EXPRESSION_RESULT");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EVarAssignType Type)
{
	switch (Type)
	{
	case EVarAssignType::Int8:
		return TEXT("INT8");
	case EVarAssignType::Int16:
		return TEXT("INT16");
	case EVarAssignType::Int:
		return TEXT("INT");
	case EVarAssignType::Int64:
		return TEXT("INT64");
	case EVarAssignType::Uint8:
		return TEXT("UINT8");
	case EVarAssignType::Uint16:
		return TEXT("UINT16");
	case EVarAssignType::Uint:
		return TEXT("UINT");
	case EVarAssignType::Uint64:
		return TEXT("UINT64");
	case EVarAssignType::Float32:
		return TEXT("FLOAT32");
	case EVarAssignType::Float64:
		return TEXT("FLOAT64");
	case EVarAssignType::Bool:
		return TEXT("BOOL");
	case EVarAssignType::Enum:
		return TEXT("ENUM");
	case EVarAssignType::Typedef:
		return TEXT("TYPEDEF");
	case EVarAssignType::ScriptValue:
		return TEXT("SCRIPT_VALUE");
	case EVarAssignType::NativeValue:
		return TEXT("NATIVE_VALUE");
	case EVarAssignType::ScriptReference:
		return TEXT("SCRIPT_REFERENCE");
	case EVarAssignType::NativeReference:
		return TEXT("NATIVE_REFERENCE");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptTypeName(const EVarAssignType Type)
{
	switch (Type)
	{
	case EVarAssignType::Int8:
		return TEXT("int8");
	case EVarAssignType::Int16:
		return TEXT("int16");
	case EVarAssignType::Int:
		return TEXT("int");
	case EVarAssignType::Int64:
		return TEXT("int64");
	case EVarAssignType::Uint8:
		return TEXT("uint8");
	case EVarAssignType::Uint16:
		return TEXT("uint16");
	case EVarAssignType::Uint:
		return TEXT("uint");
	case EVarAssignType::Uint64:
		return TEXT("uint64");
	case EVarAssignType::Float32:
		return TEXT("float");
	case EVarAssignType::Float64:
		return TEXT("double");
	case EVarAssignType::Bool:
		return TEXT("bool");
	case EVarAssignType::Enum:
		return TEXT("ENativeAssignmentEnum");
	case EVarAssignType::Typedef:
		return TEXT("NativeCaseAlias");
	case EVarAssignType::ScriptValue:
		return TEXT("FScriptCaseValue");
	case EVarAssignType::NativeValue:
		return TEXT("FNativeCaseValue");
	case EVarAssignType::ScriptReference:
		return TEXT("FScriptCaseReference");
	case EVarAssignType::NativeReference:
		return TEXT("FNativeCaseReference");
	default:
		return TEXT("int8");
	}
}

const TCHAR* TypeSuffix(const EVarAssignType Type)
{
	switch (Type)
	{
	case EVarAssignType::Int8:
		return TEXT("Int8");
	case EVarAssignType::Int16:
		return TEXT("Int16");
	case EVarAssignType::Int:
		return TEXT("Int");
	case EVarAssignType::Int64:
		return TEXT("Int64");
	case EVarAssignType::Uint8:
		return TEXT("Uint8");
	case EVarAssignType::Uint16:
		return TEXT("Uint16");
	case EVarAssignType::Uint:
		return TEXT("Uint");
	case EVarAssignType::Uint64:
		return TEXT("Uint64");
	case EVarAssignType::Float32:
		return TEXT("Float32");
	case EVarAssignType::Float64:
		return TEXT("Float64");
	case EVarAssignType::Bool:
		return TEXT("Bool");
	case EVarAssignType::Enum:
		return TEXT("Enum");
	case EVarAssignType::Typedef:
		return TEXT("Typedef");
	case EVarAssignType::ScriptValue:
		return TEXT("ScriptValue");
	case EVarAssignType::NativeValue:
		return TEXT("NativeValue");
	case EVarAssignType::ScriptReference:
		return TEXT("ScriptReference");
	case EVarAssignType::NativeReference:
		return TEXT("NativeReference");
	default:
		return TEXT("Int8");
	}
}

const TCHAR* KindSuffix(const EVarAssignKind Assignment)
{
	switch (Assignment)
	{
	case EVarAssignKind::Simple:
		return TEXT("Simple");
	case EVarAssignKind::CopySource:
		return TEXT("CopySource");
	case EVarAssignKind::SelfAssignment:
		return TEXT("SelfAssignment");
	case EVarAssignKind::Compound:
		return TEXT("Compound");
	case EVarAssignKind::ReferenceRebind:
		return TEXT("ReferenceRebind");
	default:
		return TEXT("Simple");
	}
}

bool IsValidParams(const FVarAssignTargetParams& Params)
{
	return AssignmentToken(Params.Assignment) != nullptr
		&& TargetToken(Params.Target) != nullptr
		&& TypeToken(Params.Type) != nullptr;
}

bool IsReferenceType(const EVarAssignType Type)
{
	return Type == EVarAssignType::ScriptReference || Type == EVarAssignType::NativeReference;
}

bool IsValueObjectType(const EVarAssignType Type)
{
	return Type == EVarAssignType::ScriptValue || Type == EVarAssignType::NativeValue;
}

bool IsObjectType(const EVarAssignType Type)
{
	return IsValueObjectType(Type) || IsReferenceType(Type);
}

bool SupportsCompound(const EVarAssignType Type)
{
	return Type <= EVarAssignType::Float64 || Type == EVarAssignType::Typedef;
}

bool IsWritable(const EVarAssignTarget Target)
{
	return Target == EVarAssignTarget::MutableLocal
		|| Target == EVarAssignTarget::MutableField
		|| Target == EVarAssignTarget::ReferenceAlias;
}

bool IsForkConstLocalRef(
	const EVarAssignKind Assignment,
	const EVarAssignTarget Target,
	const EVarAssignType Type)
{
	return IsReferenceType(Type)
		&& Target == EVarAssignTarget::ConstLocal
		&& Assignment != EVarAssignKind::Compound;
}

bool IsForkDiscardedValue(
	const EVarAssignKind Assignment,
	const EVarAssignTarget Target,
	const EVarAssignType Type)
{
	return IsValueObjectType(Type)
		&& (Assignment == EVarAssignKind::Simple
			|| Assignment == EVarAssignKind::CopySource
			|| Assignment == EVarAssignKind::SelfAssignment)
		&& (Target == EVarAssignTarget::Temporary || Target == EVarAssignTarget::ExpressionResult);
}

bool ShouldCompile(
	const EVarAssignKind Assignment,
	const EVarAssignTarget Target,
	const EVarAssignType Type)
{
	if (IsForkConstLocalRef(Assignment, Target, Type) || IsForkDiscardedValue(Assignment, Target, Type))
	{
		return true;
	}
	if (!IsWritable(Target))
	{
		return false;
	}
	if (Assignment == EVarAssignKind::Compound)
	{
		return SupportsCompound(Type);
	}
	if (Assignment == EVarAssignKind::ReferenceRebind)
	{
		return IsReferenceType(Type);
	}
	return true;
}

int32 InitialValue(const EVarAssignType Type)
{
	return Type == EVarAssignType::Bool || Type == EVarAssignType::Enum ? 0 : 11;
}

int32 SourceValue(const EVarAssignType Type)
{
	return Type == EVarAssignType::Bool || Type == EVarAssignType::Enum ? 1 : 29;
}

int32 AlternateValue(const EVarAssignType Type)
{
	return Type == EVarAssignType::Bool || Type == EVarAssignType::Enum ? 0 : 37;
}

int32 ExpectedFromCell(const FAssignCell& Cell)
{
	if (IsForkDiscardedValue(Cell.Assignment, Cell.Target, Cell.Type))
	{
		return 0;
	}
	if (Cell.Assignment == EVarAssignKind::SelfAssignment)
	{
		return InitialValue(Cell.Type);
	}
	if (Cell.Assignment == EVarAssignKind::Compound)
	{
		return InitialValue(Cell.Type) + SourceValue(Cell.Type);
	}
	if (Cell.Assignment == EVarAssignKind::CopySource)
	{
		const int32 SourceAfter = Cell.Target == EVarAssignTarget::ReferenceAlias
			? SourceValue(Cell.Type)
			: (IsObjectType(Cell.Type) ? AlternateValue(Cell.Type) : SourceValue(Cell.Type));
		const int32 TargetAfter = IsReferenceType(Cell.Type) ? SourceAfter : SourceValue(Cell.Type);
		return TargetAfter * 100 + SourceAfter;
	}
	if (Cell.Assignment == EVarAssignKind::ReferenceRebind)
	{
		const int32 SharedAfter = AlternateValue(Cell.Type);
		return SharedAfter * 100 + SharedAfter;
	}
	return SourceValue(Cell.Type);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FAssignCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-VAR-ASSIGN-TARGET-%s-%s-%s"),
		AssignmentToken(Cell.Assignment),
		TargetToken(Cell.Target),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FAssignCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FVarAssignTargetParams ParamsFromCell(const FAssignCell& Cell)
{
	FVarAssignTargetParams Params;
	Params.Assignment = Cell.Assignment;
	Params.Target = Cell.Target;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FAssignCell> AllCells()
{
	TArray<FAssignCell> Cells;
	Cells.SetNum(595);
	for (int32 Index = 0; Index < 595; ++Index)
	{
		FAssignCell& Cell = Cells[Index];
		Cell.Assignment = static_cast<EVarAssignKind>(Index / (TargetCount * TypeCount));
		Cell.Target = static_cast<EVarAssignTarget>((Index / TypeCount) % TargetCount);
		Cell.Type = static_cast<EVarAssignType>(Index % TypeCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FAssignCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FAssignCell> Cells = AllCells();
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

FString MakeTypedValue(const EVarAssignType Type, const int32 Value)
{
	if (Type == EVarAssignType::Bool)
	{
		return Value == 0 ? FString(TEXT("false")) : FString(TEXT("true"));
	}
	if (Type == EVarAssignType::Enum)
	{
		return Value == 0 ? FString(TEXT("ENativeAssignmentEnum::Zero")) : FString(TEXT("ENativeAssignmentEnum::One"));
	}
	if (Type == EVarAssignType::ScriptReference)
	{
		return FString::Printf(TEXT("FScriptCaseReference(%d)"), Value);
	}
	if (Type == EVarAssignType::NativeReference)
	{
		return FString::Printf(TEXT("CreateNativeCaseReference(%d)"), Value);
	}
	return FString::Printf(TEXT("%s(%d)"), ScriptTypeName(Type), Value);
}

FString HolderName(const EVarAssignType Type)
{
	return FString::Printf(TEXT("FAssignmentHolder%s"), TypeSuffix(Type));
}

FString AliasName(const EVarAssignKind Assignment, const EVarAssignType Type)
{
	return FString::Printf(TEXT("ApplyAssignmentAlias%s%s"), KindSuffix(Assignment), TypeSuffix(Type));
}

void AppendSharedTypeHelpers(FString& Source)
{
	AppendLine(Source, TEXT("enum ENativeAssignmentEnum"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tZero = 0,"));
	AppendLine(Source, TEXT("\tOne = 1"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("typedef int NativeCaseAlias;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FScriptCaseValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFScriptCaseValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = 0;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFScriptCaseValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FNativeCaseValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = 0;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FScriptCaseReference"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFScriptCaseReference()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = 0;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFScriptCaseReference(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FNativeCaseReference"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseReference()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = 0;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseReference(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseReference CreateNativeCaseReference(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FNativeCaseReference(Value);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendObserveOverload(FString& Source, const EVarAssignType Type)
{
	const TCHAR* const Script = ScriptTypeName(Type);
	if (IsReferenceType(Type))
	{
		AppendLine(Source, FString::Printf(TEXT("int ObserveAssignmentValue(const %s Value)"), Script));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Value == nullptr ? -1 : Value.Value;"));
	}
	else if (IsValueObjectType(Type))
	{
		AppendLine(Source, FString::Printf(TEXT("int ObserveAssignmentValue(const %s& in Value)"), Script));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Value.Value;"));
	}
	else if (Type == EVarAssignType::Bool)
	{
		AppendLine(Source, TEXT("int ObserveAssignmentValue(bool Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Value ? 1 : 0;"));
	}
	else if (Type == EVarAssignType::Typedef)
	{
		AppendLine(Source, TEXT("int ObserveAssignmentValue(NativeCaseAlias Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Value;"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("int ObserveAssignmentValue(%s Value)"), Script));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn int(Value);"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendHolder(FString& Source, const EVarAssignType Type)
{
	AppendLine(Source, FString::Printf(TEXT("class %s"), *HolderName(Type)));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(
		TEXT("\t%s Target = %s;"),
		ScriptTypeName(Type),
		*MakeTypedValue(Type, InitialValue(Type))));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendAliasFunction(FString& Source, const EVarAssignKind Assignment, const EVarAssignType Type)
{
	const TCHAR* const Script = ScriptTypeName(Type);
	AppendLine(Source, FString::Printf(
		TEXT("int %s(%s& inout Alias, %s Source)"),
		*AliasName(Assignment, Type),
		Script,
		Script));
	AppendLine(Source, TEXT("{"));
	if (Assignment == EVarAssignKind::SelfAssignment)
	{
		AppendLine(Source, TEXT("\tAlias = Alias;"));
	}
	else if (Assignment == EVarAssignKind::Compound)
	{
		AppendLine(Source, TEXT("\tAlias += Source;"));
	}
	else if (Assignment == EVarAssignKind::ReferenceRebind && !IsReferenceType(Type))
	{
		AppendLine(Source, TEXT("\tAlias = nullptr;"));
	}
	else
	{
		AppendLine(Source, TEXT("\tAlias = Source;"));
	}
	if (Assignment == EVarAssignKind::CopySource || Assignment == EVarAssignKind::ReferenceRebind)
	{
		if (IsObjectType(Type) && Assignment == EVarAssignKind::ReferenceRebind)
		{
			AppendLine(Source, FString::Printf(TEXT("\tSource.Value = %d;"), AlternateValue(Type)));
		}
		AppendLine(Source, TEXT("\treturn ObserveAssignmentValue(Alias) * 100 + ObserveAssignmentValue(Source);"));
	}
	else
	{
		AppendLine(Source, TEXT("\tint SourceTrace = ObserveAssignmentValue(Source);"));
		AppendLine(Source, TEXT("\treturn ObserveAssignmentValue(Alias) + SourceTrace - SourceTrace;"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendSharedHelpers(FString& Source)
{
	AppendSharedTypeHelpers(Source);
	for (int32 TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
	{
		const EVarAssignType Type = static_cast<EVarAssignType>(TypeIndex);
		AppendObserveOverload(Source, Type);
	}
	for (int32 TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
	{
		AppendHolder(Source, static_cast<EVarAssignType>(TypeIndex));
	}
	for (int32 AssignmentIndex = 0; AssignmentIndex < 5; ++AssignmentIndex)
	{
		for (int32 TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
		{
			AppendAliasFunction(
				Source,
				static_cast<EVarAssignKind>(AssignmentIndex),
				static_cast<EVarAssignType>(TypeIndex));
		}
	}
}

FString MakeTargetExpression(const EVarAssignType Type, const EVarAssignTarget Target)
{
	if (Target == EVarAssignTarget::MutableLocal || Target == EVarAssignTarget::ConstLocal)
	{
		return FString(TEXT("Target"));
	}
	if (Target == EVarAssignTarget::MutableField || Target == EVarAssignTarget::ConstField)
	{
		return FString(TEXT("Holder.Target"));
	}
	if (Target == EVarAssignTarget::ReferenceAlias)
	{
		return FString(TEXT("Alias"));
	}
	const FString Initial = MakeTypedValue(Type, InitialValue(Type));
	if (Target == EVarAssignTarget::Temporary)
	{
		return Type == EVarAssignType::Enum ? FString(TEXT("ENativeAssignmentEnum(0)")) : Initial;
	}
	return FString::Printf(TEXT("(true ? %s : %s)"), *Initial, *MakeTypedValue(Type, SourceValue(Type)));
}

void AppendEntry(FString& Source, const FVarAssignTargetParams& Params, const FString& FunctionName)
{
	const TCHAR* const Script = ScriptTypeName(Params.Type);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Target == EVarAssignTarget::MutableLocal
		|| Params.Target == EVarAssignTarget::ConstLocal
		|| Params.Target == EVarAssignTarget::ReferenceAlias)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s%s Target = %s;"),
			Params.Target == EVarAssignTarget::ConstLocal ? TEXT("const ") : TEXT(""),
			Script,
			*MakeTypedValue(Params.Type, InitialValue(Params.Type))));
	}
	else if (Params.Target == EVarAssignTarget::MutableField || Params.Target == EVarAssignTarget::ConstField)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s%s Holder = %s();"),
			Params.Target == EVarAssignTarget::ConstField ? TEXT("const ") : TEXT(""),
			*HolderName(Params.Type),
			*HolderName(Params.Type)));
	}
	if (Params.Assignment == EVarAssignKind::CopySource || Params.Assignment == EVarAssignKind::ReferenceRebind)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Source = %s;"),
			Script,
			*MakeTypedValue(Params.Type, SourceValue(Params.Type))));
	}
	if (Params.Target == EVarAssignTarget::ReferenceAlias)
	{
		const FString AliasSource = Params.Assignment == EVarAssignKind::CopySource
			|| Params.Assignment == EVarAssignKind::ReferenceRebind
			? FString(TEXT("Source"))
			: MakeTypedValue(Params.Type, SourceValue(Params.Type));
		AppendLine(Source, FString::Printf(
			TEXT("\treturn %s(Target, %s);"),
			*AliasName(Params.Assignment, Params.Type),
			*AliasSource));
		AppendLine(Source, TEXT("}"));
		return;
	}
	const FString TargetExpression = MakeTargetExpression(Params.Type, Params.Target);
	if (Params.Assignment == EVarAssignKind::SelfAssignment)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s = %s;"), *TargetExpression, *TargetExpression));
	}
	else if (Params.Assignment == EVarAssignKind::Compound)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s += %s;"),
			*TargetExpression,
			*MakeTypedValue(Params.Type, SourceValue(Params.Type))));
	}
	else if (Params.Assignment == EVarAssignKind::ReferenceRebind && !IsReferenceType(Params.Type))
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s = nullptr;"), *TargetExpression));
	}
	else
	{
		const FString SourceExpression = Params.Assignment == EVarAssignKind::CopySource
			|| Params.Assignment == EVarAssignKind::ReferenceRebind
			? FString(TEXT("Source"))
			: MakeTypedValue(Params.Type, SourceValue(Params.Type));
		AppendLine(Source, FString::Printf(TEXT("\t%s = %s;"), *TargetExpression, *SourceExpression));
	}
	if (IsForkDiscardedValue(Params.Assignment, Params.Target, Params.Type)
		|| (!IsWritable(Params.Target) && !IsForkConstLocalRef(Params.Assignment, Params.Target, Params.Type)))
	{
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else if (IsObjectType(Params.Type)
		&& (Params.Assignment == EVarAssignKind::CopySource || Params.Assignment == EVarAssignKind::ReferenceRebind))
	{
		AppendLine(Source, FString::Printf(TEXT("\tSource.Value = %d;"), AlternateValue(Params.Type)));
		AppendLine(Source, FString::Printf(
			TEXT("\treturn ObserveAssignmentValue(%s) * 100 + ObserveAssignmentValue(Source);"),
			*TargetExpression));
	}
	else if (Params.Assignment == EVarAssignKind::CopySource || Params.Assignment == EVarAssignKind::ReferenceRebind)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\treturn ObserveAssignmentValue(%s) * 100 + ObserveAssignmentValue(Source);"),
			*TargetExpression));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveAssignmentValue(%s);"), *TargetExpression));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendOwnedHelpers(FString& Source, const FVarAssignTargetParams& Params)
{
	AppendSharedTypeHelpers(Source);
	AppendObserveOverload(Source, Params.Type);
	if (Params.Target == EVarAssignTarget::MutableField || Params.Target == EVarAssignTarget::ConstField)
	{
		AppendHolder(Source, Params.Type);
	}
	if (Params.Target == EVarAssignTarget::ReferenceAlias)
	{
		AppendAliasFunction(Source, Params.Assignment, Params.Type);
	}
}

FString BuildOwnedSource(const FVarAssignTargetParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FAssignCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (!ShouldCompile(Cell.Assignment, Cell.Target, Cell.Type))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	return Info;
}
}

FStringView FVarAssignTargetGenerator::GetProductId() const
{
	return TEXT("LANG-VAR-ASSIGN-TARGET");
}

TArray<FGeneratedCaseInfo> FVarAssignTargetGenerator::ListCases() const
{
	using namespace VarAssignTargetGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FAssignCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("VarAssignTarget PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FVarAssignTargetGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace VarAssignTargetGeneratorDetail;
	FAssignCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FVarAssignTargetGenerator::BuildDumpSource() const
{
	using namespace VarAssignTargetGeneratorDetail;
	const TArray<FAssignCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-VAR-ASSIGN-TARGET"));
	AppendLine(Source, TEXT("// Cases: 595 total, 212 normal, 383 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FAssignCell& Cell = Cells[Index];
		if (!ShouldCompile(Cell.Assignment, Cell.Target, Cell.Type))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FAssignCell& Cell = Cells[Index];
		if (ShouldCompile(Cell.Assignment, Cell.Target, Cell.Type))
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

FString FVarAssignTargetGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace VarAssignTargetGeneratorDetail;
	const TArray<FAssignCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!ShouldCompile(Cells[Index].Assignment, Cells[Index].Target, Cells[Index].Type))
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

FString FVarAssignTargetGenerator::BuildAssignmentSource(const FVarAssignTargetParams& Params) const
{
	using namespace VarAssignTargetGeneratorDetail;
	if (!IsValidParams(Params) || !ShouldCompile(Params.Assignment, Params.Target, Params.Type))
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

int32 FVarAssignTargetGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace VarAssignTargetGeneratorDetail;
	FAssignCell Cell;
	if (!TryFindCell(CaseId, Cell) || !ShouldCompile(Cell.Assignment, Cell.Target, Cell.Type))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FVarAssignTargetGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace VarAssignTargetGeneratorDetail;
	FAssignCell Cell;
	if (!TryFindCell(CaseId, Cell) || ShouldCompile(Cell.Assignment, Cell.Target, Cell.Type))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FVarAssignTargetGenerator::ListRejectCaseIds() const
{
	using namespace VarAssignTargetGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FAssignCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!ShouldCompile(Cells[Index].Assignment, Cells[Index].Target, Cells[Index].Type))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}

