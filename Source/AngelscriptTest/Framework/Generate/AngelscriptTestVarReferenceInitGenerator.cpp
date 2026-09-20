#include "Framework/Generate/AngelscriptTestVarReferenceInitGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace VarReferenceInitGeneratorDetail
{
struct FReferenceCell
{
	EVarRefDecl Declaration = EVarRefDecl::ExplicitType;
	EVarRefSource Source = EVarRefSource::ConstructedLocal;
	EVarRefType Type = EVarRefType::ScriptReference;
	EVarRefUse Use = EVarRefUse::Identity;
};

const TCHAR* DeclarationToken(const EVarRefDecl Declaration)
{
	switch (Declaration)
	{
	case EVarRefDecl::ExplicitType:
		return TEXT("EXPLICIT_TYPE");
	case EVarRefDecl::Auto:
		return TEXT("AUTO");
	case EVarRefDecl::ConstView:
		return TEXT("CONST_VIEW");
	default:
		return nullptr;
	}
}

const TCHAR* SourceToken(const EVarRefSource Source)
{
	switch (Source)
	{
	case EVarRefSource::ConstructedLocal:
		return TEXT("CONSTRUCTED_LOCAL");
	case EVarRefSource::Parameter:
		return TEXT("PARAMETER");
	case EVarRefSource::FunctionReturn:
		return TEXT("FUNCTION_RETURN");
	case EVarRefSource::Field:
		return TEXT("FIELD");
	case EVarRefSource::Null:
		return TEXT("NULL");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EVarRefType Type)
{
	switch (Type)
	{
	case EVarRefType::ScriptReference:
		return TEXT("SCRIPT_REFERENCE");
	case EVarRefType::NativeReference:
		return TEXT("NATIVE_REFERENCE");
	default:
		return nullptr;
	}
}

const TCHAR* UseToken(const EVarRefUse Use)
{
	switch (Use)
	{
	case EVarRefUse::Identity:
		return TEXT("IDENTITY");
	case EVarRefUse::Mutation:
		return TEXT("MUTATION");
	case EVarRefUse::Argument:
		return TEXT("ARGUMENT");
	case EVarRefUse::Return:
		return TEXT("RETURN");
	case EVarRefUse::NullCompare:
		return TEXT("NULL_COMPARE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FVarReferenceInitParams& Params)
{
	return DeclarationToken(Params.Declaration) != nullptr
		&& SourceToken(Params.Source) != nullptr
		&& TypeToken(Params.Type) != nullptr
		&& UseToken(Params.Use) != nullptr;
}

bool IsReject(const EVarRefSource Source, const EVarRefDecl Declaration, const EVarRefUse Use)
{
	if (Source == EVarRefSource::Null && Declaration == EVarRefDecl::Auto)
	{
		return true;
	}
	return Declaration == EVarRefDecl::ConstView && Use == EVarRefUse::Mutation;
}

bool IsNullFault(const EVarRefSource Source, const EVarRefDecl Declaration, const EVarRefUse Use)
{
	return Source == EVarRefSource::Null
		&& Declaration == EVarRefDecl::ExplicitType
		&& Use == EVarRefUse::Mutation;
}

const TCHAR* ScriptTypeName(const EVarRefType Type)
{
	return Type == EVarRefType::ScriptReference ? TEXT("FScriptCaseReference") : TEXT("FNativeCaseReference");
}

const TCHAR* HolderName(const EVarRefType Type)
{
	return Type == EVarRefType::ScriptReference
		? TEXT("FVariableReferenceHolderScript")
		: TEXT("FVariableReferenceHolderNative");
}

FString FreshExpression(const EVarRefType Type)
{
	return Type == EVarRefType::NativeReference
		? FString(TEXT("CreateNativeCaseReference(11)"))
		: FString(TEXT("FScriptCaseReference(11)"));
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FReferenceCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-VAR-REFERENCE-INIT-%s-%s-%s-%s"),
		DeclarationToken(Cell.Declaration),
		SourceToken(Cell.Source),
		TypeToken(Cell.Type),
		UseToken(Cell.Use));
}

FString MakeEntryName(const FReferenceCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FVarReferenceInitParams ParamsFromCell(const FReferenceCell& Cell)
{
	FVarReferenceInitParams Params;
	Params.Declaration = Cell.Declaration;
	Params.Source = Cell.Source;
	Params.Type = Cell.Type;
	Params.Use = Cell.Use;
	return Params;
}

TArray<FReferenceCell> AllCells()
{
	TArray<FReferenceCell> Cells;
	Cells.SetNum(150);
	for (int32 Index = 0; Index < 150; ++Index)
	{
		FReferenceCell& Cell = Cells[Index];
		Cell.Declaration = static_cast<EVarRefDecl>(Index / 50);
		Cell.Source = static_cast<EVarRefSource>((Index / 10) % 5);
		Cell.Type = static_cast<EVarRefType>((Index / 5) % 2);
		Cell.Use = static_cast<EVarRefUse>(Index % 5);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FReferenceCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FReferenceCell> Cells = AllCells();
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

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int BeginScriptCaseReference(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void EndScriptCaseReference(int ObjectId, int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FScriptCaseReference"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("\tint ObjectId = -1;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFScriptCaseReference()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tObjectId = BeginScriptCaseReference(Value);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFScriptCaseReference(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t\tObjectId = BeginScriptCaseReference(Value);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\t~FScriptCaseReference()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tEndScriptCaseReference(ObjectId, Value);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FNativeCaseReference"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
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
	AppendLine(Source, TEXT("class FVariableReferenceHolderScript"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFScriptCaseReference Stored;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFVariableReferenceHolderScript()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tStored = FScriptCaseReference(11);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FVariableReferenceHolderNative"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Stored;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFVariableReferenceHolderNative()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tStored = CreateNativeCaseReference(11);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FScriptCaseReference PassVariableReference(FScriptCaseReference Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("const FScriptCaseReference PassVariableReference(const FScriptCaseReference Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseReference PassVariableReference(FNativeCaseReference Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("const FNativeCaseReference PassVariableReference(const FNativeCaseReference Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObserveVariableReference(const FScriptCaseReference Candidate, const FScriptCaseReference Expected)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tif (Candidate != Expected)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn -1;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\tif (Candidate == nullptr)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn 1;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Candidate.Value == 11 ? 1 : -2;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObserveVariableReference(const FNativeCaseReference Candidate, const FNativeCaseReference Expected)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tif (Candidate != Expected)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn -1;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\tif (Candidate == nullptr)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn 1;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Candidate.Value == 11 ? 1 : -2;"));
	AppendLine(Source, TEXT("}"));
}

FString MakeVariableDeclaration(
	const EVarRefType Type,
	const EVarRefDecl Declaration,
	const FString& Initializer,
	const TCHAR* Prefix)
{
	if (Declaration == EVarRefDecl::Auto)
	{
		return FString::Printf(TEXT("%sauto Variable = %s;"), Prefix, *Initializer);
	}
	return FString::Printf(
		TEXT("%s%s%s Variable = %s;"),
		Prefix,
		Declaration == EVarRefDecl::ConstView ? TEXT("const ") : TEXT(""),
		ScriptTypeName(Type),
		*Initializer);
}

void AppendVariableUse(
	FString& Source,
	const FVarReferenceInitParams& Params,
	const TCHAR* Prefix)
{
	if (Params.Use == EVarRefUse::Identity)
	{
		AppendLine(Source, FString::Printf(TEXT("%sreturn Variable == Original ? 1 : -1;"), Prefix));
	}
	else if (Params.Use == EVarRefUse::Mutation)
	{
		AppendLine(Source, FString::Printf(TEXT("%sVariable.Value += 5;"), Prefix));
		AppendLine(Source, FString::Printf(
			TEXT("%sreturn Original.Value == 16 && Variable.Value == 16 ? 1 : -1;"),
			Prefix));
	}
	else if (Params.Use == EVarRefUse::Argument)
	{
		AppendLine(Source, FString::Printf(TEXT("%sreturn ObserveVariableReference(Variable, Original);"), Prefix));
	}
	else if (Params.Use == EVarRefUse::Return)
	{
		AppendLine(Source, FString::Printf(
			TEXT("%s%s%s Returned = PassVariableReference(Variable);"),
			Prefix,
			Params.Declaration == EVarRefDecl::ConstView ? TEXT("const ") : TEXT(""),
			ScriptTypeName(Params.Type)));
		AppendLine(Source, FString::Printf(TEXT("%sreturn Returned == Original ? 1 : -1;"), Prefix));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("%sreturn (Variable == nullptr) == %s ? 1 : -1;"),
			Prefix,
			Params.Source == EVarRefSource::Null ? TEXT("true") : TEXT("false")));
	}
}

void AppendEntry(FString& Source, const FVarReferenceInitParams& Params, const FString& FunctionName)
{
	const TCHAR* TypeName = ScriptTypeName(Params.Type);
	if (Params.Source == EVarRefSource::Parameter)
	{
		AppendLine(Source, FString::Printf(TEXT("int Exercise%s(%s Original)"), *FunctionName, TypeName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, MakeVariableDeclaration(Params.Type, Params.Declaration, TEXT("Original"), TEXT("\t")));
		AppendVariableUse(Source, Params, TEXT("\t"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Original = %s;"), TypeName, *FreshExpression(Params.Type)));
		AppendLine(Source, FString::Printf(TEXT("\treturn Exercise%s(Original);"), *FunctionName));
		AppendLine(Source, TEXT("}"));
		return;
	}

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	FString Initializer;
	if (Params.Source == EVarRefSource::Field)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Holder = %s();"), HolderName(Params.Type), HolderName(Params.Type)));
		AppendLine(Source, FString::Printf(TEXT("\t%s Original = Holder.Stored;"), TypeName));
		Initializer = TEXT("Holder.Stored");
	}
	else if (Params.Source == EVarRefSource::Null)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Original = nullptr;"), TypeName));
		Initializer = TEXT("nullptr");
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Original = %s;"), TypeName, *FreshExpression(Params.Type)));
		Initializer = Params.Source == EVarRefSource::FunctionReturn
			? FString(TEXT("PassVariableReference(Original)"))
			: FString(TEXT("Original"));
	}
	AppendLine(Source, MakeVariableDeclaration(Params.Type, Params.Declaration, Initializer, TEXT("\t")));
	AppendVariableUse(Source, Params, TEXT("\t"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FVarReferenceInitParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FReferenceCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Source, Cell.Declaration, Cell.Use))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	if (IsNullFault(Cell.Source, Cell.Declaration, Cell.Use))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.Execution = EGeneratedCaseExecution::Standalone;
		Info.ExpectedException = TEXT("Null pointer access");
		return Info;
	}
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = 1;
	return Info;
}
}

FStringView FVarReferenceInitGenerator::GetProductId() const
{
	return TEXT("LANG-VAR-REFERENCE-INIT");
}

TArray<FGeneratedCaseInfo> FVarReferenceInitGenerator::ListCases() const
{
	using namespace VarReferenceInitGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FReferenceCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("VarReferenceInit PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FVarReferenceInitGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace VarReferenceInitGeneratorDetail;
	FReferenceCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FVarReferenceInitGenerator::BuildDumpSource() const
{
	using namespace VarReferenceInitGeneratorDetail;
	const TArray<FReferenceCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-VAR-REFERENCE-INIT"));
	AppendLine(Source, TEXT("// Cases: 150 total, 128 normal, 20 reject, 2 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	bool bNeedBlank = true;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FReferenceCell& Cell = Cells[Index];
		if (IsReject(Cell.Source, Cell.Declaration, Cell.Use))
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
		if (IsNullFault(Cell.Source, Cell.Declaration, Cell.Use))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Null pointer access"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, TEXT("// ExpectedReturn: 1"));
		}
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FReferenceCell& Cell = Cells[Index];
		if (!IsReject(Cell.Source, Cell.Declaration, Cell.Use))
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
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FVarReferenceInitGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace VarReferenceInitGeneratorDetail;
	const TArray<FReferenceCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Source, Cells[Index].Declaration, Cells[Index].Use))
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

FString FVarReferenceInitGenerator::BuildReferenceInitializationSource(const FVarReferenceInitParams& Params) const
{
	using namespace VarReferenceInitGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Source, Params.Declaration, Params.Use))
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

int32 FVarReferenceInitGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace VarReferenceInitGeneratorDetail;
	FReferenceCell Cell;
	if (!TryFindCell(CaseId, Cell)
		|| IsReject(Cell.Source, Cell.Declaration, Cell.Use)
		|| IsNullFault(Cell.Source, Cell.Declaration, Cell.Use))
	{
		return 0;
	}
	return 1;
}

FString FVarReferenceInitGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace VarReferenceInitGeneratorDetail;
	FReferenceCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Source, Cell.Declaration, Cell.Use))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FVarReferenceInitGenerator::ListRejectCaseIds() const
{
	using namespace VarReferenceInitGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FReferenceCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Source, Cells[Index].Declaration, Cells[Index].Use))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
