#include "Framework/Generate/AngelscriptTestDtorDeclarationGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace DtorDeclarationGeneratorDetail
{
constexpr const TCHAR* HostNotes =
	TEXT("Needs native FNativeCaseValue, FNativeCaseReference, and RecordDestructorDeclarationMarker.");
constexpr int32 ScenarioAxisCount = 13;
constexpr int32 ObservationAxisCount = 4;
constexpr int32 TotalCells = ScenarioAxisCount * ObservationAxisCount;

struct FDeclarationCell
{
	EDtorDeclarationScenario Scenario = EDtorDeclarationScenario::ImplicitScriptValue;
	EDtorDeclarationObservation Observation = EDtorDeclarationObservation::Compile;
};

const TCHAR* ScenarioToken(const EDtorDeclarationScenario Scenario)
{
	switch (Scenario)
	{
	case EDtorDeclarationScenario::ImplicitScriptValue:
		return TEXT("IMPLICIT_SCRIPT_VALUE");
	case EDtorDeclarationScenario::DeclaredScriptValue:
		return TEXT("DECLARED_SCRIPT_VALUE");
	case EDtorDeclarationScenario::ImplicitScriptReference:
		return TEXT("IMPLICIT_SCRIPT_REFERENCE");
	case EDtorDeclarationScenario::DeclaredScriptReference:
		return TEXT("DECLARED_SCRIPT_REFERENCE");
	case EDtorDeclarationScenario::NativeValue:
		return TEXT("NATIVE_VALUE");
	case EDtorDeclarationScenario::NativeReference:
		return TEXT("NATIVE_REFERENCE");
	case EDtorDeclarationScenario::EmptyDestructor:
		return TEXT("EMPTY_DESTRUCTOR");
	case EDtorDeclarationScenario::FieldDestructor:
		return TEXT("FIELD_DESTRUCTOR");
	case EDtorDeclarationScenario::BaseDerivedDestructor:
		return TEXT("BASE_DERIVED_DESTRUCTOR");
	case EDtorDeclarationScenario::PrivateDestructor:
		return TEXT("PRIVATE_DESTRUCTOR");
	case EDtorDeclarationScenario::ThrowingDestructor:
		return TEXT("THROWING_DESTRUCTOR");
	case EDtorDeclarationScenario::ThrowingDerivedMembersBase:
		return TEXT("THROWING_DERIVED_MEMBERS_BASE");
	case EDtorDeclarationScenario::MalformedDestructor:
		return TEXT("MALFORMED_DESTRUCTOR");
	default:
		return nullptr;
	}
}

const TCHAR* ObservationToken(const EDtorDeclarationObservation Observation)
{
	switch (Observation)
	{
	case EDtorDeclarationObservation::Compile:
		return TEXT("COMPILE");
	case EDtorDeclarationObservation::Metadata:
		return TEXT("METADATA");
	case EDtorDeclarationObservation::Runtime:
		return TEXT("RUNTIME");
	case EDtorDeclarationObservation::Cleanup:
		return TEXT("CLEANUP");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FDtorDeclarationParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr && ObservationToken(Params.Observation) != nullptr;
}

bool IsReject(const EDtorDeclarationScenario Scenario)
{
	return Scenario == EDtorDeclarationScenario::MalformedDestructor;
}

bool IsFault(const EDtorDeclarationScenario Scenario)
{
	return Scenario == EDtorDeclarationScenario::ThrowingDestructor
		|| Scenario == EDtorDeclarationScenario::ThrowingDerivedMembersBase;
}

int32 ExpectedValue(const EDtorDeclarationScenario Scenario)
{
	switch (Scenario)
	{
	case EDtorDeclarationScenario::ImplicitScriptValue:
		return 41;
	case EDtorDeclarationScenario::DeclaredScriptValue:
		return 42;
	case EDtorDeclarationScenario::ImplicitScriptReference:
		return 43;
	case EDtorDeclarationScenario::DeclaredScriptReference:
		return 44;
	case EDtorDeclarationScenario::NativeValue:
		return 45;
	case EDtorDeclarationScenario::NativeReference:
		return 46;
	case EDtorDeclarationScenario::EmptyDestructor:
		return 47;
	case EDtorDeclarationScenario::FieldDestructor:
		return 48;
	case EDtorDeclarationScenario::BaseDerivedDestructor:
		return 49;
	case EDtorDeclarationScenario::PrivateDestructor:
		return 50;
	default:
		return 0;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FDeclarationCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-DTOR-DECLARATION-%s-%s"),
		ScenarioToken(Cell.Scenario),
		ObservationToken(Cell.Observation));
}

FString MakeEntryName(const FDeclarationCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FDtorDeclarationParams ParamsFromCell(const FDeclarationCell& Cell)
{
	FDtorDeclarationParams Params;
	Params.Scenario = Cell.Scenario;
	Params.Observation = Cell.Observation;
	return Params;
}

void AppendScenarioTypes(FString& Source, const EDtorDeclarationScenario Scenario)
{
	if (Scenario == EDtorDeclarationScenario::ImplicitScriptValue)
	{
		AppendLine(Source, TEXT("struct FImplicitValue"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendLine(Source, TEXT("\tint Value = 41;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Scenario == EDtorDeclarationScenario::DeclaredScriptValue)
	{
		AppendLine(Source, TEXT("struct FDeclaredValue"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendLine(Source, TEXT("\tint Value = 42;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\t~FDeclaredValue()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordDestructorDeclarationMarker(102);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Scenario == EDtorDeclarationScenario::ImplicitScriptReference)
	{
		AppendLine(Source, TEXT("class FImplicitReference"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendLine(Source, TEXT("\tint Value = 43;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Scenario == EDtorDeclarationScenario::DeclaredScriptReference)
	{
		AppendLine(Source, TEXT("class FDeclaredReference"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendLine(Source, TEXT("\tint Value = 44;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\t~FDeclaredReference()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordDestructorDeclarationMarker(104);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Scenario == EDtorDeclarationScenario::EmptyDestructor)
	{
		AppendLine(Source, TEXT("struct FEmptyDestructor"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendLine(Source, TEXT("\tint Value = 47;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\t~FEmptyDestructor()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Scenario == EDtorDeclarationScenario::FieldDestructor)
	{
		for (int32 Index = 1; Index <= 3; ++Index)
		{
			AppendLine(Source, FString::Printf(TEXT("struct FDeclarationField%d"), Index));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
			AppendLine(Source);
			AppendLine(Source, FString::Printf(TEXT("\t~FDeclarationField%d()"), Index));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, FString::Printf(TEXT("\t\tRecordDestructorDeclarationMarker(%d);"), 300 + Index));
			AppendLine(Source, TEXT("\t}"));
			AppendLine(Source, TEXT("}"));
			AppendLine(Source);
		}
		AppendLine(Source, TEXT("struct FFieldOwner"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFDeclarationField1 First;"));
		AppendLine(Source, TEXT("\tFDeclarationField2 Middle;"));
		AppendLine(Source, TEXT("\tFDeclarationField3 Last;"));
		AppendLine(Source, TEXT("\tint Value = 48;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\t~FFieldOwner()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordDestructorDeclarationMarker(399);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Scenario == EDtorDeclarationScenario::BaseDerivedDestructor)
	{
		AppendLine(Source, TEXT("class FDestructorBase"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue BaseTracked;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\t~FDestructorBase()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordDestructorDeclarationMarker(502);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("class FDestructorDerived : FDestructorBase"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue DerivedTracked;"));
		AppendLine(Source, TEXT("\tint Value = 49;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\t~FDestructorDerived()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordDestructorDeclarationMarker(501);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Scenario == EDtorDeclarationScenario::PrivateDestructor)
	{
		AppendLine(Source, TEXT("class FPrivateDestructor"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendLine(Source, TEXT("\tint Value = 50;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tprivate ~FPrivateDestructor()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordDestructorDeclarationMarker(601);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Scenario == EDtorDeclarationScenario::ThrowingDestructor)
	{
		AppendLine(Source, TEXT("class FThrowingDestructor"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendLine(Source, TEXT("\tint Value = 51;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\t~FThrowingDestructor()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordDestructorDeclarationMarker(701);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Scenario == EDtorDeclarationScenario::ThrowingDerivedMembersBase)
	{
		AppendLine(Source, TEXT("struct FThrowingFirstMember"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\t~FThrowingFirstMember()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordDestructorDeclarationMarker(811);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("struct FThrowingSecondMember"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\t~FThrowingSecondMember()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordDestructorDeclarationMarker(812);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("class FThrowingBase"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue BaseTracked;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\t~FThrowingBase()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordDestructorDeclarationMarker(802);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("class FThrowingDerived : FThrowingBase"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFThrowingFirstMember First;"));
		AppendLine(Source, TEXT("\tFThrowingSecondMember Second;"));
		AppendLine(Source, TEXT("\tint Value = 52;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\t~FThrowingDerived()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordDestructorDeclarationMarker(801);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Scenario == EDtorDeclarationScenario::MalformedDestructor)
	{
		AppendLine(Source, TEXT("class FMalformedDestructor"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\t~FMalformedDestructor(int InvalidParameter)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendAllSharedTypes(FString& Source)
{
	for (int32 Index = 0; Index < ScenarioAxisCount; ++Index)
	{
		const EDtorDeclarationScenario Scenario = static_cast<EDtorDeclarationScenario>(Index);
		if (!IsReject(Scenario))
		{
			AppendScenarioTypes(Source, Scenario);
		}
	}
}

void AppendRecovery(FString& Source)
{
	AppendLine(Source, TEXT("int RunDestructorDeclarationRecovery()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseValue Recovery(97);"));
	AppendLine(Source, TEXT("\treturn Recovery.Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FDtorDeclarationParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Scenario == EDtorDeclarationScenario::ImplicitScriptValue)
	{
		AppendLine(Source, TEXT("\tFImplicitValue Object;"));
		AppendLine(Source, TEXT("\treturn Object.Value;"));
	}
	else if (Params.Scenario == EDtorDeclarationScenario::DeclaredScriptValue)
	{
		AppendLine(Source, TEXT("\tFDeclaredValue Object;"));
		AppendLine(Source, TEXT("\treturn Object.Value;"));
	}
	else if (Params.Scenario == EDtorDeclarationScenario::ImplicitScriptReference)
	{
		AppendLine(Source, TEXT("\tFImplicitReference Object = FImplicitReference();"));
		AppendLine(Source, TEXT("\treturn Object.Value;"));
	}
	else if (Params.Scenario == EDtorDeclarationScenario::DeclaredScriptReference)
	{
		AppendLine(Source, TEXT("\tFDeclaredReference Object = FDeclaredReference();"));
		AppendLine(Source, TEXT("\treturn Object.Value;"));
	}
	else if (Params.Scenario == EDtorDeclarationScenario::NativeValue)
	{
		AppendLine(Source, TEXT("\tFNativeCaseValue Object(45);"));
		AppendLine(Source, TEXT("\treturn Object.Value;"));
	}
	else if (Params.Scenario == EDtorDeclarationScenario::NativeReference)
	{
		AppendLine(Source, TEXT("\tFNativeCaseReference Object = CreateNativeCaseReference(46);"));
		AppendLine(Source, TEXT("\treturn Object.Value;"));
	}
	else if (Params.Scenario == EDtorDeclarationScenario::EmptyDestructor)
	{
		AppendLine(Source, TEXT("\tFEmptyDestructor Object;"));
		AppendLine(Source, TEXT("\treturn Object.Value;"));
	}
	else if (Params.Scenario == EDtorDeclarationScenario::FieldDestructor)
	{
		AppendLine(Source, TEXT("\tFFieldOwner Object;"));
		AppendLine(Source, TEXT("\treturn Object.Value;"));
	}
	else if (Params.Scenario == EDtorDeclarationScenario::BaseDerivedDestructor)
	{
		AppendLine(Source, TEXT("\tFDestructorDerived Object = FDestructorDerived();"));
		AppendLine(Source, TEXT("\treturn Object.Value;"));
	}
	else if (Params.Scenario == EDtorDeclarationScenario::PrivateDestructor)
	{
		AppendLine(Source, TEXT("\tFPrivateDestructor Object = FPrivateDestructor();"));
		AppendLine(Source, TEXT("\treturn Object.Value;"));
	}
	else if (Params.Scenario == EDtorDeclarationScenario::ThrowingDestructor)
	{
		AppendLine(Source, TEXT("\tFThrowingDestructor Object = FThrowingDestructor();"));
		AppendLine(Source, TEXT("\tint Zero = 0;"));
		AppendLine(Source, TEXT("\tint Fault = 1 / Zero;"));
		AppendLine(Source, TEXT("\treturn Object.Value + Fault;"));
	}
	else if (Params.Scenario == EDtorDeclarationScenario::ThrowingDerivedMembersBase)
	{
		AppendLine(Source, TEXT("\tFThrowingDerived Object = FThrowingDerived();"));
		AppendLine(Source, TEXT("\tint Zero = 0;"));
		AppendLine(Source, TEXT("\tint Fault = 1 / Zero;"));
		AppendLine(Source, TEXT("\treturn Object.Value + Fault;"));
	}
	else
	{
		AppendLine(Source, TEXT("\tFMalformedDestructor Object = FMalformedDestructor();"));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FDtorDeclarationParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendScenarioTypes(Source, Params.Scenario);
	AppendEntry(Source, Params, FunctionName);
	if (!IsReject(Params.Scenario))
	{
		AppendLine(Source);
		AppendRecovery(Source);
	}
	return Source;
}

TArray<FDeclarationCell> AllCells()
{
	TArray<FDeclarationCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FDeclarationCell& Cell = Cells[Index];
		Cell.Scenario = static_cast<EDtorDeclarationScenario>(Index / ObservationAxisCount);
		Cell.Observation = static_cast<EDtorDeclarationObservation>(Index % ObservationAxisCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FDeclarationCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FDeclarationCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FGeneratedCaseInfo MakeCaseInfo(const FDeclarationCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Scenario))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	if (IsFault(Cell.Scenario))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Divide by zero");
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = ExpectedValue(Cell.Scenario);
	}
	return Info;
}
}

FStringView FDtorDeclarationGenerator::GetProductId() const
{
	return TEXT("LANG-DTOR-DECLARATION");
}

TArray<FGeneratedCaseInfo> FDtorDeclarationGenerator::ListCases() const
{
	using namespace DtorDeclarationGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FDeclarationCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("DtorDeclaration PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FDtorDeclarationGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace DtorDeclarationGeneratorDetail;
	FDeclarationCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FDtorDeclarationGenerator::BuildDumpSource() const
{
	using namespace DtorDeclarationGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-DTOR-DECLARATION"));
	AppendLine(Source, TEXT("// Cases: 52 total, 40 normal, 4 reject, 8 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendAllSharedTypes(Source);
	AppendRecovery(Source);

	bool bNeedBlank = true;
	for (const FDeclarationCell& Cell : AllCells())
	{
		if (IsReject(Cell.Scenario))
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
		if (IsFault(Cell.Scenario))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), HostNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}

	for (const FDeclarationCell& Cell : AllCells())
	{
		if (!IsReject(Cell.Scenario))
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
		AppendScenarioTypes(Source, Cell.Scenario);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FDtorDeclarationGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace DtorDeclarationGeneratorDetail;
	FString Source;
	AppendAllSharedTypes(Source);
	AppendRecovery(Source);
	int32 Emitted = 0;
	for (const FDeclarationCell& Cell : AllCells())
	{
		if (IsReject(Cell.Scenario))
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

FString FDtorDeclarationGenerator::BuildDestructorDeclarationSource(const FDtorDeclarationParams& Params) const
{
	using namespace DtorDeclarationGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Scenario))
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

int32 FDtorDeclarationGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace DtorDeclarationGeneratorDetail;
	FDeclarationCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Scenario) || IsFault(Cell.Scenario))
	{
		return 0;
	}
	return ExpectedValue(Cell.Scenario);
}

FString FDtorDeclarationGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace DtorDeclarationGeneratorDetail;
	FDeclarationCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Scenario))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FDtorDeclarationGenerator::ListRejectCaseIds() const
{
	using namespace DtorDeclarationGeneratorDetail;
	TArray<FString> Ids;
	for (const FDeclarationCell& Cell : AllCells())
	{
		if (IsReject(Cell.Scenario))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
