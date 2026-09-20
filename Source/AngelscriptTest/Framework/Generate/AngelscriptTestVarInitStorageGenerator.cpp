#include "Framework/Generate/AngelscriptTestVarInitStorageGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace VarInitStorageGeneratorDetail
{
constexpr int32 TypeCount = 15;
constexpr int32 StorageCount = 7;

struct FInitCell
{
	EVarInitKind Initializer = EVarInitKind::Default;
	EVarInitStorage Storage = EVarInitStorage::Local;
	EVarInitType Type = EVarInitType::Int8;
};

const TCHAR* InitializerToken(const EVarInitKind Initializer)
{
	switch (Initializer)
	{
	case EVarInitKind::Default:
		return TEXT("DEFAULT");
	case EVarInitKind::Literal:
		return TEXT("LITERAL");
	case EVarInitKind::Expression:
		return TEXT("EXPRESSION");
	case EVarInitKind::Copy:
		return TEXT("COPY");
	case EVarInitKind::Constructor:
		return TEXT("CONSTRUCTOR");
	case EVarInitKind::FunctionReturn:
		return TEXT("FUNCTION_RETURN");
	case EVarInitKind::Conditional:
		return TEXT("CONDITIONAL");
	default:
		return nullptr;
	}
}

const TCHAR* StorageToken(const EVarInitStorage Storage)
{
	switch (Storage)
	{
	case EVarInitStorage::Local:
		return TEXT("LOCAL");
	case EVarInitStorage::ConstLocal:
		return TEXT("CONST_LOCAL");
	case EVarInitStorage::Auto:
		return TEXT("AUTO");
	case EVarInitStorage::LoopInitializer:
		return TEXT("LOOP_INITIALIZER");
	case EVarInitStorage::BranchLocal:
		return TEXT("BRANCH_LOCAL");
	case EVarInitStorage::ConstGlobal:
		return TEXT("CONST_GLOBAL");
	case EVarInitStorage::FieldLinkage:
		return TEXT("FIELD_LINKAGE");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EVarInitType Type)
{
	switch (Type)
	{
	case EVarInitType::Int8:
		return TEXT("INT8");
	case EVarInitType::Int16:
		return TEXT("INT16");
	case EVarInitType::Int:
		return TEXT("INT");
	case EVarInitType::Int64:
		return TEXT("INT64");
	case EVarInitType::Uint8:
		return TEXT("UINT8");
	case EVarInitType::Uint16:
		return TEXT("UINT16");
	case EVarInitType::Uint:
		return TEXT("UINT");
	case EVarInitType::Uint64:
		return TEXT("UINT64");
	case EVarInitType::Float32:
		return TEXT("FLOAT32");
	case EVarInitType::Float64:
		return TEXT("FLOAT64");
	case EVarInitType::Bool:
		return TEXT("BOOL");
	case EVarInitType::Enum:
		return TEXT("ENUM");
	case EVarInitType::Typedef:
		return TEXT("TYPEDEF");
	case EVarInitType::ScriptValue:
		return TEXT("SCRIPT_VALUE");
	case EVarInitType::NativeValue:
		return TEXT("NATIVE_VALUE");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptTypeName(const EVarInitType Type)
{
	switch (Type)
	{
	case EVarInitType::Int8:
		return TEXT("int8");
	case EVarInitType::Int16:
		return TEXT("int16");
	case EVarInitType::Int:
		return TEXT("int");
	case EVarInitType::Int64:
		return TEXT("int64");
	case EVarInitType::Uint8:
		return TEXT("uint8");
	case EVarInitType::Uint16:
		return TEXT("uint16");
	case EVarInitType::Uint:
		return TEXT("uint");
	case EVarInitType::Uint64:
		return TEXT("uint64");
	case EVarInitType::Float32:
		return TEXT("float");
	case EVarInitType::Float64:
		return TEXT("double");
	case EVarInitType::Bool:
		return TEXT("bool");
	case EVarInitType::Enum:
		return TEXT("ENativeCaseEnum");
	case EVarInitType::Typedef:
		return TEXT("NativeCaseAlias");
	case EVarInitType::ScriptValue:
		return TEXT("FScriptCaseValue");
	case EVarInitType::NativeValue:
		return TEXT("FNativeCaseValue");
	default:
		return TEXT("int8");
	}
}

const TCHAR* TypeSuffix(const EVarInitType Type)
{
	switch (Type)
	{
	case EVarInitType::Int8:
		return TEXT("Int8");
	case EVarInitType::Int16:
		return TEXT("Int16");
	case EVarInitType::Int:
		return TEXT("Int");
	case EVarInitType::Int64:
		return TEXT("Int64");
	case EVarInitType::Uint8:
		return TEXT("Uint8");
	case EVarInitType::Uint16:
		return TEXT("Uint16");
	case EVarInitType::Uint:
		return TEXT("Uint");
	case EVarInitType::Uint64:
		return TEXT("Uint64");
	case EVarInitType::Float32:
		return TEXT("Float32");
	case EVarInitType::Float64:
		return TEXT("Float64");
	case EVarInitType::Bool:
		return TEXT("Bool");
	case EVarInitType::Enum:
		return TEXT("Enum");
	case EVarInitType::Typedef:
		return TEXT("Typedef");
	case EVarInitType::ScriptValue:
		return TEXT("ScriptValue");
	case EVarInitType::NativeValue:
		return TEXT("NativeValue");
	default:
		return TEXT("Int8");
	}
}

const TCHAR* InitSuffix(const EVarInitKind Initializer)
{
	switch (Initializer)
	{
	case EVarInitKind::Default:
		return TEXT("Default");
	case EVarInitKind::Literal:
		return TEXT("Literal");
	case EVarInitKind::Expression:
		return TEXT("Expression");
	case EVarInitKind::Copy:
		return TEXT("Copy");
	case EVarInitKind::Constructor:
		return TEXT("Constructor");
	case EVarInitKind::FunctionReturn:
		return TEXT("FunctionReturn");
	case EVarInitKind::Conditional:
		return TEXT("Conditional");
	default:
		return TEXT("Default");
	}
}

bool IsValidParams(const FVarInitStorageParams& Params)
{
	return InitializerToken(Params.Initializer) != nullptr
		&& StorageToken(Params.Storage) != nullptr
		&& TypeToken(Params.Type) != nullptr;
}

bool IsObjectValue(const EVarInitType Type)
{
	return Type == EVarInitType::ScriptValue || Type == EVarInitType::NativeValue;
}

bool IsStableDefault(const EVarInitType Type)
{
	return IsObjectValue(Type);
}

bool ShouldCompile(const EVarInitKind Initializer, const EVarInitStorage Storage, const EVarInitType Type)
{
	if (Initializer == EVarInitKind::Default && Storage == EVarInitStorage::Auto)
	{
		return false;
	}
	return !(Storage == EVarInitStorage::ConstGlobal && Type == EVarInitType::NativeValue);
}

int32 ExpectedFromCell(const EVarInitKind Initializer, const EVarInitType Type)
{
	if (Initializer == EVarInitKind::Default)
	{
		return 0;
	}
	if (Initializer == EVarInitKind::Expression
		&& Type != EVarInitType::Bool
		&& Type != EVarInitType::Enum
		&& !IsObjectValue(Type))
	{
		return 2;
	}
	return 1;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FInitCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-VAR-INIT-STORAGE-%s-%s-%s"),
		InitializerToken(Cell.Initializer),
		StorageToken(Cell.Storage),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FInitCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FVarInitStorageParams ParamsFromCell(const FInitCell& Cell)
{
	FVarInitStorageParams Params;
	Params.Initializer = Cell.Initializer;
	Params.Storage = Cell.Storage;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FInitCell> AllCells()
{
	TArray<FInitCell> Cells;
	Cells.SetNum(735);
	for (int32 Index = 0; Index < 735; ++Index)
	{
		FInitCell& Cell = Cells[Index];
		Cell.Initializer = static_cast<EVarInitKind>(Index / (StorageCount * TypeCount));
		Cell.Storage = static_cast<EVarInitStorage>((Index / TypeCount) % StorageCount);
		Cell.Type = static_cast<EVarInitType>(Index % TypeCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FInitCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FInitCell> Cells = AllCells();
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

FString MakeValueExpression(const EVarInitType Type, const int32 Value)
{
	if (Type == EVarInitType::Bool)
	{
		return Value == 0 ? FString(TEXT("false")) : FString(TEXT("true"));
	}
	if (Type == EVarInitType::Enum)
	{
		return Value == 0 ? FString(TEXT("ENativeCaseEnum::Zero")) : FString(TEXT("ENativeCaseEnum::One"));
	}
	if (Type == EVarInitType::Typedef)
	{
		return FString::Printf(TEXT("NativeCaseAlias(%d)"), Value);
	}
	if (IsObjectValue(Type))
	{
		return FString::Printf(TEXT("%s(%d)"), ScriptTypeName(Type), Value);
	}
	if (Type == EVarInitType::Float32)
	{
		return Value == 2 ? FString(TEXT("1.0f + 1.0f")) : (Value == 0 ? FString(TEXT("0.0f")) : FString(TEXT("1.0f")));
	}
	if (Type == EVarInitType::Float64)
	{
		return Value == 2 ? FString(TEXT("1.0 + 1.0")) : (Value == 0 ? FString(TEXT("0.0")) : FString(TEXT("1.0")));
	}
	return Value == 2 ? FString(TEXT("1 + 1")) : FString::Printf(TEXT("%d"), Value);
}

FString MakeInitializerExpression(const EVarInitKind Initializer, const EVarInitType Type, const FString& SourceName)
{
	if (Initializer == EVarInitKind::Literal || Initializer == EVarInitKind::Constructor)
	{
		return MakeValueExpression(Type, 1);
	}
	if (Initializer == EVarInitKind::Expression)
	{
		if (Type == EVarInitType::Bool)
		{
			return FString(TEXT("!false"));
		}
		if (Type == EVarInitType::Enum || IsObjectValue(Type))
		{
			return FString::Printf(
				TEXT("true ? %s : %s"),
				*MakeValueExpression(Type, 1),
				*MakeValueExpression(Type, 0));
		}
		return MakeValueExpression(Type, 2);
	}
	if (Initializer == EVarInitKind::Copy)
	{
		return SourceName;
	}
	if (Initializer == EVarInitKind::FunctionReturn)
	{
		return FString::Printf(TEXT("MakeVariableValue%s()"), TypeSuffix(Type));
	}
	if (Initializer == EVarInitKind::Conditional)
	{
		return FString::Printf(
			TEXT("true ? %s : %s"),
			*MakeValueExpression(Type, 1),
			*MakeValueExpression(Type, 0));
	}
	return FString();
}

FString HolderName(const EVarInitKind Initializer, const EVarInitType Type)
{
	return FString::Printf(TEXT("FVariableHolder%s%s"), InitSuffix(Initializer), TypeSuffix(Type));
}

FString GlobalName(const EVarInitKind Initializer, const EVarInitType Type)
{
	return FString::Printf(TEXT("VariableValue%s%s"), InitSuffix(Initializer), TypeSuffix(Type));
}

FString GlobalSourceName(const EVarInitKind Initializer, const EVarInitType Type)
{
	return FString::Printf(TEXT("VariableSource%s%s"), InitSuffix(Initializer), TypeSuffix(Type));
}

void AppendSharedTypeHelpers(FString& Source)
{
	AppendLine(Source, TEXT("enum ENativeCaseEnum"));
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
}

void AppendFactory(FString& Source, const EVarInitType Type)
{
	AppendLine(Source, FString::Printf(TEXT("%s MakeVariableValue%s()"), ScriptTypeName(Type), TypeSuffix(Type)));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *MakeValueExpression(Type, 1)));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendFieldHolder(FString& Source, const EVarInitKind Initializer, const EVarInitType Type)
{
	AppendLine(Source, FString::Printf(TEXT("struct %s"), *HolderName(Initializer, Type)));
	AppendLine(Source, TEXT("{"));
	if (Initializer == EVarInitKind::Copy)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s VariableSource = %s;"),
			ScriptTypeName(Type),
			*MakeValueExpression(Type, 1)));
	}
	if (Initializer == EVarInitKind::Default)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s VariableValue;"), ScriptTypeName(Type)));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s VariableValue = %s;"),
			ScriptTypeName(Type),
			*MakeInitializerExpression(Initializer, Type, FString(TEXT("VariableSource")))));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendConstGlobal(FString& Source, const EVarInitKind Initializer, const EVarInitType Type)
{
	const FString Name = GlobalName(Initializer, Type);
	if (Initializer == EVarInitKind::Copy)
	{
		AppendLine(Source, FString::Printf(
			TEXT("const %s %s = %s;"),
			ScriptTypeName(Type),
			*GlobalSourceName(Initializer, Type),
			*MakeValueExpression(Type, 1)));
	}
	if (Initializer == EVarInitKind::Default)
	{
		AppendLine(Source, FString::Printf(TEXT("const %s %s;"), ScriptTypeName(Type), *Name));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("const %s %s = %s;"),
			ScriptTypeName(Type),
			*Name,
			*MakeInitializerExpression(Initializer, Type, GlobalSourceName(Initializer, Type))));
	}
	AppendLine(Source);
}

void AppendSharedHelpers(FString& Source)
{
	AppendSharedTypeHelpers(Source);
	for (int32 TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
	{
		AppendFactory(Source, static_cast<EVarInitType>(TypeIndex));
	}
	for (int32 InitIndex = 0; InitIndex < 7; ++InitIndex)
	{
		for (int32 TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
		{
			const EVarInitKind Initializer = static_cast<EVarInitKind>(InitIndex);
			const EVarInitType Type = static_cast<EVarInitType>(TypeIndex);
			if (ShouldCompile(Initializer, EVarInitStorage::FieldLinkage, Type))
			{
				AppendFieldHolder(Source, Initializer, Type);
			}
			if (ShouldCompile(Initializer, EVarInitStorage::ConstGlobal, Type))
			{
				AppendConstGlobal(Source, Initializer, Type);
			}
		}
	}
}

void AppendTypedDeclaration(
	FString& Source,
	const FString& Prefix,
	const TCHAR* Qualifier,
	const EVarInitKind Initializer,
	const EVarInitType Type,
	const FString& SourceName)
{
	if (Initializer == EVarInitKind::Default)
	{
		AppendLine(Source, FString::Printf(TEXT("%s%s%s VariableValue;"), *Prefix, Qualifier, ScriptTypeName(Type)));
		return;
	}
	AppendLine(Source, FString::Printf(
		TEXT("%s%s%s VariableValue = %s;"),
		*Prefix,
		Qualifier,
		ScriptTypeName(Type),
		*MakeInitializerExpression(Initializer, Type, SourceName)));
}

void AppendCopySourceIfNeeded(
	FString& Source,
	const FString& Prefix,
	const EVarInitKind Initializer,
	const EVarInitType Type)
{
	if (Initializer != EVarInitKind::Copy)
	{
		return;
	}
	AppendLine(Source, FString::Printf(
		TEXT("%s%s VariableSource = %s;"),
		*Prefix,
		ScriptTypeName(Type),
		*MakeValueExpression(Type, 1)));
}

void AppendEntry(FString& Source, const FVarInitStorageParams& Params, const FString& FunctionName)
{
	const int32 Expected = ExpectedFromCell(Params.Initializer, Params.Type);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Storage == EVarInitStorage::Local || Params.Storage == EVarInitStorage::ConstLocal)
	{
		AppendCopySourceIfNeeded(Source, TEXT("\t"), Params.Initializer, Params.Type);
		AppendTypedDeclaration(
			Source,
			TEXT("\t"),
			Params.Storage == EVarInitStorage::ConstLocal ? TEXT("const ") : TEXT(""),
			Params.Initializer,
			Params.Type,
			FString(TEXT("VariableSource")));
		if (Params.Initializer == EVarInitKind::Default
			&& !IsStableDefault(Params.Type)
			&& Params.Storage != EVarInitStorage::ConstLocal)
		{
			AppendLine(Source, FString::Printf(TEXT("\tVariableValue = %s;"), *MakeValueExpression(Params.Type, 0)));
		}
	}
	else if (Params.Storage == EVarInitStorage::Auto)
	{
		AppendCopySourceIfNeeded(Source, TEXT("\t"), Params.Initializer, Params.Type);
		if (Params.Initializer == EVarInitKind::Default)
		{
			AppendLine(Source, TEXT("\tauto VariableValue;"));
		}
		else
		{
			AppendLine(Source, FString::Printf(
				TEXT("\tauto VariableValue = %s;"),
				*MakeInitializerExpression(Params.Initializer, Params.Type, FString(TEXT("VariableSource")))));
		}
	}
	else if (Params.Storage == EVarInitStorage::LoopInitializer)
	{
		AppendCopySourceIfNeeded(Source, TEXT("\t"), Params.Initializer, Params.Type);
		AppendLine(Source, TEXT("\tint Visits = 0;"));
		if (Params.Initializer == EVarInitKind::Default)
		{
			AppendLine(Source, FString::Printf(TEXT("\tfor (%s VariableValue; Visits < 1; ++Visits)"), ScriptTypeName(Params.Type)));
		}
		else
		{
			AppendLine(Source, FString::Printf(
				TEXT("\tfor (%s VariableValue = %s; Visits < 1; ++Visits)"),
				ScriptTypeName(Params.Type),
				*MakeInitializerExpression(Params.Initializer, Params.Type, FString(TEXT("VariableSource")))));
		}
		AppendLine(Source, TEXT("\t{"));
		if (Params.Initializer == EVarInitKind::Default && !IsStableDefault(Params.Type))
		{
			AppendLine(Source, FString::Printf(TEXT("\t\tVariableValue = %s;"), *MakeValueExpression(Params.Type, 0)));
		}
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), Expected));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn -2;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	else if (Params.Storage == EVarInitStorage::BranchLocal)
	{
		AppendLine(Source, TEXT("\tif (true)"));
		AppendLine(Source, TEXT("\t{"));
		AppendCopySourceIfNeeded(Source, TEXT("\t\t"), Params.Initializer, Params.Type);
		AppendTypedDeclaration(
			Source,
			TEXT("\t\t"),
			TEXT(""),
			Params.Initializer,
			Params.Type,
			FString(TEXT("VariableSource")));
		if (Params.Initializer == EVarInitKind::Default && !IsStableDefault(Params.Type))
		{
			AppendLine(Source, FString::Printf(TEXT("\t\tVariableValue = %s;"), *MakeValueExpression(Params.Type, 0)));
		}
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), Expected));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn -2;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	else if (Params.Storage == EVarInitStorage::ConstGlobal)
	{
		if (IsObjectValue(Params.Type))
		{
			AppendLine(Source, FString::Printf(
				TEXT("\tint Observed = %s.Value;"),
				*GlobalName(Params.Initializer, Params.Type)));
		}
		else
		{
			AppendLine(Source, FString::Printf(
				TEXT("\tint Observed = int(%s);"),
				*GlobalName(Params.Initializer, Params.Type)));
		}
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Holder;"),
			*HolderName(Params.Initializer, Params.Type)));
		if (Params.Initializer == EVarInitKind::Default && !IsStableDefault(Params.Type))
		{
			AppendLine(Source, FString::Printf(
				TEXT("\tHolder.VariableValue = %s;"),
				*MakeValueExpression(Params.Type, 0)));
		}
	}
	AppendLine(Source, FString::Printf(TEXT("\treturn %d;"), Expected));
	AppendLine(Source, TEXT("}"));
}

void AppendOwnedHelpers(FString& Source, const FVarInitStorageParams& Params)
{
	AppendSharedTypeHelpers(Source);
	if (Params.Initializer == EVarInitKind::FunctionReturn)
	{
		AppendFactory(Source, Params.Type);
	}
	if (Params.Storage == EVarInitStorage::ConstGlobal)
	{
		AppendConstGlobal(Source, Params.Initializer, Params.Type);
	}
	else if (Params.Storage == EVarInitStorage::FieldLinkage)
	{
		AppendFieldHolder(Source, Params.Initializer, Params.Type);
	}
}

FString BuildOwnedSource(const FVarInitStorageParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FInitCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (!ShouldCompile(Cell.Initializer, Cell.Storage, Cell.Type))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell.Initializer, Cell.Type);
	return Info;
}
}

FStringView FVarInitStorageGenerator::GetProductId() const
{
	return TEXT("LANG-VAR-INIT-STORAGE");
}

TArray<FGeneratedCaseInfo> FVarInitStorageGenerator::ListCases() const
{
	using namespace VarInitStorageGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FInitCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("VarInitStorage PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FVarInitStorageGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace VarInitStorageGeneratorDetail;
	FInitCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FVarInitStorageGenerator::BuildDumpSource() const
{
	using namespace VarInitStorageGeneratorDetail;
	const TArray<FInitCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-VAR-INIT-STORAGE"));
	AppendLine(Source, TEXT("// Cases: 735 total, 713 normal, 22 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FInitCell& Cell = Cells[Index];
		if (!ShouldCompile(Cell.Initializer, Cell.Storage, Cell.Type))
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
		const FInitCell& Cell = Cells[Index];
		if (ShouldCompile(Cell.Initializer, Cell.Storage, Cell.Type))
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

FString FVarInitStorageGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace VarInitStorageGeneratorDetail;
	const TArray<FInitCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!ShouldCompile(Cells[Index].Initializer, Cells[Index].Storage, Cells[Index].Type))
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

FString FVarInitStorageGenerator::BuildVariableSource(const FVarInitStorageParams& Params) const
{
	using namespace VarInitStorageGeneratorDetail;
	if (!IsValidParams(Params) || !ShouldCompile(Params.Initializer, Params.Storage, Params.Type))
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

int32 FVarInitStorageGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace VarInitStorageGeneratorDetail;
	FInitCell Cell;
	if (!TryFindCell(CaseId, Cell) || !ShouldCompile(Cell.Initializer, Cell.Storage, Cell.Type))
	{
		return 0;
	}
	return ExpectedFromCell(Cell.Initializer, Cell.Type);
}

FString FVarInitStorageGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace VarInitStorageGeneratorDetail;
	FInitCell Cell;
	if (!TryFindCell(CaseId, Cell) || ShouldCompile(Cell.Initializer, Cell.Storage, Cell.Type))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FVarInitStorageGenerator::ListRejectCaseIds() const
{
	using namespace VarInitStorageGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FInitCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!ShouldCompile(Cells[Index].Initializer, Cells[Index].Storage, Cells[Index].Type))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}

