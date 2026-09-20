#include "Framework/Generate/AngelscriptTestDtorOwnerExitGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace DtorOwnerExitGeneratorDetail
{
constexpr const TCHAR* HostNotes =
	TEXT("Needs native lifecycle helpers and ReachDestructorExitTerminal.");
constexpr int32 ScenarioAxisCount = 37;
constexpr int32 NestingAxisCount = 6;
constexpr int32 ObservationAxisCount = 3;
constexpr int32 TotalCells = ScenarioAxisCount * NestingAxisCount * ObservationAxisCount;

struct FOwnerExitCell
{
	EDtorOwnerExitScenario Scenario = EDtorOwnerExitScenario::LocalBlockEnd;
	EDtorOwnerExitNesting Nesting = EDtorOwnerExitNesting::One;
	EDtorOwnerExitObservation Observation = EDtorOwnerExitObservation::EventOrder;
};

const TCHAR* ScenarioToken(const EDtorOwnerExitScenario Scenario)
{
	switch (Scenario)
	{
	case EDtorOwnerExitScenario::LocalBlockEnd:
		return TEXT("LOCAL_BLOCK_END");
	case EDtorOwnerExitScenario::LocalReturn:
		return TEXT("LOCAL_RETURN");
	case EDtorOwnerExitScenario::LocalEarlyReturn:
		return TEXT("LOCAL_EARLY_RETURN");
	case EDtorOwnerExitScenario::LocalBreak:
		return TEXT("LOCAL_BREAK");
	case EDtorOwnerExitScenario::LocalContinue:
		return TEXT("LOCAL_CONTINUE");
	case EDtorOwnerExitScenario::LocalSwitchExit:
		return TEXT("LOCAL_SWITCH_EXIT");
	case EDtorOwnerExitScenario::LocalException:
		return TEXT("LOCAL_EXCEPTION");
	case EDtorOwnerExitScenario::LocalAbort:
		return TEXT("LOCAL_ABORT");
	case EDtorOwnerExitScenario::LocalUnprepare:
		return TEXT("LOCAL_UNPREPARE");
	case EDtorOwnerExitScenario::NestedLocalBlockEnd:
		return TEXT("NESTED_LOCAL_BLOCK_END");
	case EDtorOwnerExitScenario::NestedLocalReturn:
		return TEXT("NESTED_LOCAL_RETURN");
	case EDtorOwnerExitScenario::NestedLocalException:
		return TEXT("NESTED_LOCAL_EXCEPTION");
	case EDtorOwnerExitScenario::FieldBlockEnd:
		return TEXT("FIELD_BLOCK_END");
	case EDtorOwnerExitScenario::FieldReturn:
		return TEXT("FIELD_RETURN");
	case EDtorOwnerExitScenario::FieldException:
		return TEXT("FIELD_EXCEPTION");
	case EDtorOwnerExitScenario::FieldAbort:
		return TEXT("FIELD_ABORT");
	case EDtorOwnerExitScenario::BaseDerivedBlockEnd:
		return TEXT("BASE_DERIVED_BLOCK_END");
	case EDtorOwnerExitScenario::BaseDerivedReturn:
		return TEXT("BASE_DERIVED_RETURN");
	case EDtorOwnerExitScenario::BaseDerivedException:
		return TEXT("BASE_DERIVED_EXCEPTION");
	case EDtorOwnerExitScenario::BaseDerivedAbort:
		return TEXT("BASE_DERIVED_ABORT");
	case EDtorOwnerExitScenario::TemporaryStatementEnd:
		return TEXT("TEMPORARY_STATEMENT_END");
	case EDtorOwnerExitScenario::TemporaryEarlyReturn:
		return TEXT("TEMPORARY_EARLY_RETURN");
	case EDtorOwnerExitScenario::TemporaryException:
		return TEXT("TEMPORARY_EXCEPTION");
	case EDtorOwnerExitScenario::ReturnedValueConsume:
		return TEXT("RETURNED_VALUE_CONSUME");
	case EDtorOwnerExitScenario::ReturnedValueDiscard:
		return TEXT("RETURNED_VALUE_DISCARD");
	case EDtorOwnerExitScenario::ReturnedValueException:
		return TEXT("RETURNED_VALUE_EXCEPTION");
	case EDtorOwnerExitScenario::ArgumentCopyReturn:
		return TEXT("ARGUMENT_COPY_RETURN");
	case EDtorOwnerExitScenario::ArgumentCopyException:
		return TEXT("ARGUMENT_COPY_EXCEPTION");
	case EDtorOwnerExitScenario::ArgumentCopyAbort:
		return TEXT("ARGUMENT_COPY_ABORT");
	case EDtorOwnerExitScenario::ReferenceScopeEnd:
		return TEXT("REFERENCE_SCOPE_END");
	case EDtorOwnerExitScenario::ReferenceAliasScopeEnd:
		return TEXT("REFERENCE_ALIAS_SCOPE_END");
	case EDtorOwnerExitScenario::ReferenceReturn:
		return TEXT("REFERENCE_RETURN");
	case EDtorOwnerExitScenario::ReferenceException:
		return TEXT("REFERENCE_EXCEPTION");
	case EDtorOwnerExitScenario::ReferenceAbort:
		return TEXT("REFERENCE_ABORT");
	case EDtorOwnerExitScenario::ReferenceUnprepare:
		return TEXT("REFERENCE_UNPREPARE");
	case EDtorOwnerExitScenario::ModuleDiscardGlobal:
		return TEXT("MODULE_DISCARD_GLOBAL");
	case EDtorOwnerExitScenario::EngineShutdownGlobal:
		return TEXT("ENGINE_SHUTDOWN_GLOBAL");
	default:
		return nullptr;
	}
}

const TCHAR* NestingToken(const EDtorOwnerExitNesting Nesting)
{
	switch (Nesting)
	{
	case EDtorOwnerExitNesting::One:
		return TEXT("ONE");
	case EDtorOwnerExitNesting::Sequential:
		return TEXT("SEQUENTIAL");
	case EDtorOwnerExitNesting::NestedScopes:
		return TEXT("NESTED_SCOPES");
	case EDtorOwnerExitNesting::NestedCalls:
		return TEXT("NESTED_CALLS");
	case EDtorOwnerExitNesting::Loop:
		return TEXT("LOOP");
	case EDtorOwnerExitNesting::Recursion:
		return TEXT("RECURSION");
	default:
		return nullptr;
	}
}

const TCHAR* ObservationToken(const EDtorOwnerExitObservation Observation)
{
	switch (Observation)
	{
	case EDtorOwnerExitObservation::EventOrder:
		return TEXT("EVENT_ORDER");
	case EDtorOwnerExitObservation::OwnershipOnce:
		return TEXT("OWNERSHIP_ONCE");
	case EDtorOwnerExitObservation::TerminalStateRecovery:
		return TEXT("TERMINAL_STATE_RECOVERY");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FDtorOwnerExitParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr
		&& NestingToken(Params.Nesting) != nullptr
		&& ObservationToken(Params.Observation) != nullptr;
}

int32 RouteId(const EDtorOwnerExitScenario Scenario)
{
	return static_cast<int32>(Scenario) + 1;
}

int32 TokenBase(const EDtorOwnerExitNesting Nesting)
{
	return (static_cast<int32>(Nesting) + 1) * 100;
}

int32 UnitCount(const EDtorOwnerExitNesting Nesting)
{
	return Nesting == EDtorOwnerExitNesting::One ? 1 : 3;
}

bool IsGlobal(const EDtorOwnerExitScenario Scenario)
{
	return Scenario == EDtorOwnerExitScenario::ModuleDiscardGlobal
		|| Scenario == EDtorOwnerExitScenario::EngineShutdownGlobal;
}

bool IsFault(const EDtorOwnerExitScenario Scenario)
{
	return Scenario == EDtorOwnerExitScenario::LocalException
		|| Scenario == EDtorOwnerExitScenario::NestedLocalException
		|| Scenario == EDtorOwnerExitScenario::FieldException
		|| Scenario == EDtorOwnerExitScenario::BaseDerivedException
		|| Scenario == EDtorOwnerExitScenario::TemporaryException
		|| Scenario == EDtorOwnerExitScenario::ReturnedValueException
		|| Scenario == EDtorOwnerExitScenario::ArgumentCopyException
		|| Scenario == EDtorOwnerExitScenario::ReferenceException;
}

bool IsNestedLocal(const EDtorOwnerExitScenario Scenario)
{
	return Scenario == EDtorOwnerExitScenario::NestedLocalBlockEnd
		|| Scenario == EDtorOwnerExitScenario::NestedLocalReturn
		|| Scenario == EDtorOwnerExitScenario::NestedLocalException;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString Indent(const int32 Level)
{
	return FString::ChrN(Level, TEXT('\t'));
}

FString MakeCaseId(const FOwnerExitCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-DTOR-OWNER-EXIT-%s-%s-%s"),
		ScenarioToken(Cell.Scenario),
		NestingToken(Cell.Nesting),
		ObservationToken(Cell.Observation));
}

FString MakeEntryName(const FOwnerExitCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FDtorOwnerExitParams ParamsFromCell(const FOwnerExitCell& Cell)
{
	FDtorOwnerExitParams Params;
	Params.Scenario = Cell.Scenario;
	Params.Nesting = Cell.Nesting;
	Params.Observation = Cell.Observation;
	return Params;
}

void AppendSharedTypes(FString& Source)
{
	AppendLine(Source, TEXT("struct FExitValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint ObjectId = -1;"));
	AppendLine(Source, TEXT("\tint Token = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFExitValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(0);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFExitValue(int InToken)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tToken = InToken;"));
	AppendLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Token);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFExitValue(const FExitValue& Other)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tToken = Other.Token;"));
	AppendLine(Source, TEXT("\t\tObjectId = CopyNativeScriptLifecycle(Other.ObjectId, Token);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFExitValue& opAssign(const FExitValue& Other)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tAssignNativeScriptLifecycle(ObjectId, Other.ObjectId, Other.Token);"));
	AppendLine(Source, TEXT("\t\tToken = Other.Token;"));
	AppendLine(Source, TEXT("\t\treturn this;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\t~FExitValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Token);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FExitReference"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint ObjectId = -1;"));
	AppendLine(Source, TEXT("\tint Token = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFExitReference()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(0);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFExitReference(int InToken)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tToken = InToken;"));
	AppendLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Token);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\t~FExitReference()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Token);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint GetToken() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Token;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FExitFieldOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFExitReference First;"));
	AppendLine(Source, TEXT("\tFExitReference Middle;"));
	AppendLine(Source, TEXT("\tFExitReference Last;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFExitFieldOwner(int Token)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tFirst = FExitReference(Token * 10 + 1);"));
	AppendLine(Source, TEXT("\t\tMiddle = FExitReference(Token * 10 + 2);"));
	AppendLine(Source, TEXT("\t\tLast = FExitReference(Token * 10 + 3);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FExitBaseOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFExitReference BaseField;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFExitBaseOwner(int Token)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tBaseField = FExitReference(Token * 10 + 1);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FExitDerivedOwner : FExitBaseOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFExitReference DerivedMiddle;"));
	AppendLine(Source, TEXT("\tFExitReference DerivedLast;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFExitDerivedOwner(int Token)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tsuper(Token);"));
	AppendLine(Source, TEXT("\t\tDerivedMiddle = FExitReference(Token * 10 + 2);"));
	AppendLine(Source, TEXT("\t\tDerivedLast = FExitReference(Token * 10 + 3);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObserveExitValue(const FExitValue& Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.Token;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FExitValue MakeExitValue(int Token)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FExitValue(Token);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void ConsumeExitTemporaryAndTerminate(const FExitValue& Value, int RouteId)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tif (Value.Token >= 0)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tReachDestructorExitTerminal(RouteId);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void ConsumeExitValueByValue(FExitValue Value, int RouteId, bool Terminal)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tif (Terminal)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tReachDestructorExitTerminal(RouteId);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FExitReference BuildExitGlobalCall3(int TokenBase)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FExitReference(TokenBase + 3);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FExitReference BuildExitGlobalCall2(int TokenBase)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFExitReference Local = FExitReference(TokenBase + 2);"));
	AppendLine(Source, TEXT("\treturn BuildExitGlobalCall3(TokenBase);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FExitReference BuildExitGlobalCall1(int TokenBase)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFExitReference Local = FExitReference(TokenBase + 1);"));
	AppendLine(Source, TEXT("\treturn BuildExitGlobalCall2(TokenBase);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FExitReference BuildExitGlobalNestedCall(int Depth, int TokenBase)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tif (Depth <= 1)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FExitReference(TokenBase + Depth);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\tFExitReference Local = FExitReference(TokenBase + Depth);"));
	AppendLine(Source, TEXT("\treturn BuildExitGlobalNestedCall(Depth - 1, TokenBase);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FExitReference BuildExitGlobalLoop(int TokenBase)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFExitReference Result;"));
	AppendLine(Source, TEXT("\tfor (int Index = 1; Index <= 3; ++Index)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tResult = FExitReference(TokenBase + Index);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Result;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int RunDestructorExitRecovery()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFExitReference Recovery = FExitReference(909);"));
	AppendLine(Source, TEXT("\treturn Recovery.GetToken();"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendFaultOrReturn(FString& Source, const FString& Prefix, const EDtorOwnerExitScenario Scenario)
{
	if (IsFault(Scenario))
	{
		AppendLine(Source, Prefix + TEXT("int Zero = 0;"));
		AppendLine(Source, Prefix + TEXT("return 1 / Zero;"));
		return;
	}
	AppendLine(Source, Prefix + FString::Printf(TEXT("return %d;"), RouteId(Scenario)));
}

void AppendPassiveOwner(
	FString& Source,
	const EDtorOwnerExitScenario Scenario,
	const FString& TokenExpression,
	const FString& Suffix,
	const int32 IndentLevel)
{
	const FString Prefix = Indent(IndentLevel);
	if (Scenario == EDtorOwnerExitScenario::NestedLocalBlockEnd
		|| Scenario == EDtorOwnerExitScenario::NestedLocalReturn
		|| Scenario == EDtorOwnerExitScenario::NestedLocalException)
	{
		AppendLine(Source, FString::Printf(TEXT("%sFExitValue Outer%s(%s);"), *Prefix, *Suffix, *TokenExpression));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("%s\tFExitValue Inner%s(%s + 1);"), *Prefix, *Suffix, *TokenExpression));
		AppendLine(Source, Prefix + TEXT("}"));
		return;
	}
	if (Scenario == EDtorOwnerExitScenario::FieldBlockEnd
		|| Scenario == EDtorOwnerExitScenario::FieldReturn
		|| Scenario == EDtorOwnerExitScenario::FieldException
		|| Scenario == EDtorOwnerExitScenario::FieldAbort)
	{
		AppendLine(Source, FString::Printf(
			TEXT("%sFExitFieldOwner Owner%s = FExitFieldOwner(%s);"),
			*Prefix,
			*Suffix,
			*TokenExpression));
		return;
	}
	if (Scenario == EDtorOwnerExitScenario::BaseDerivedBlockEnd
		|| Scenario == EDtorOwnerExitScenario::BaseDerivedReturn
		|| Scenario == EDtorOwnerExitScenario::BaseDerivedException
		|| Scenario == EDtorOwnerExitScenario::BaseDerivedAbort)
	{
		AppendLine(Source, FString::Printf(
			TEXT("%sFExitDerivedOwner Owner%s = FExitDerivedOwner(%s);"),
			*Prefix,
			*Suffix,
			*TokenExpression));
		return;
	}
	if (Scenario == EDtorOwnerExitScenario::TemporaryStatementEnd
		|| Scenario == EDtorOwnerExitScenario::TemporaryEarlyReturn
		|| Scenario == EDtorOwnerExitScenario::TemporaryException)
	{
		AppendLine(Source, FString::Printf(TEXT("%sObserveExitValue(FExitValue(%s));"), *Prefix, *TokenExpression));
		return;
	}
	if (Scenario == EDtorOwnerExitScenario::ReturnedValueDiscard)
	{
		AppendLine(Source, FString::Printf(TEXT("%sMakeExitValue(%s);"), *Prefix, *TokenExpression));
		return;
	}
	if (Scenario == EDtorOwnerExitScenario::ReturnedValueConsume
		|| Scenario == EDtorOwnerExitScenario::ReturnedValueException)
	{
		AppendLine(Source, FString::Printf(
			TEXT("%sFExitValue Returned%s = MakeExitValue(%s);"),
			*Prefix,
			*Suffix,
			*TokenExpression));
		return;
	}
	if (Scenario == EDtorOwnerExitScenario::ArgumentCopyReturn
		|| Scenario == EDtorOwnerExitScenario::ArgumentCopyException
		|| Scenario == EDtorOwnerExitScenario::ArgumentCopyAbort)
	{
		AppendLine(Source, FString::Printf(TEXT("%sFExitValue Source%s(%s);"), *Prefix, *Suffix, *TokenExpression));
		AppendLine(Source, FString::Printf(
			TEXT("%sConsumeExitValueByValue(Source%s, %d, false);"),
			*Prefix,
			*Suffix,
			RouteId(Scenario)));
		return;
	}
	if (Scenario == EDtorOwnerExitScenario::ReferenceAliasScopeEnd)
	{
		AppendLine(Source, FString::Printf(
			TEXT("%sFExitReference Owner%s = FExitReference(%s);"),
			*Prefix,
			*Suffix,
			*TokenExpression));
		AppendLine(Source, FString::Printf(
			TEXT("%sFExitReference Alias%s = Owner%s;"),
			*Prefix,
			*Suffix,
			*Suffix));
		return;
	}
	if (Scenario == EDtorOwnerExitScenario::ReferenceScopeEnd
		|| Scenario == EDtorOwnerExitScenario::ReferenceReturn
		|| Scenario == EDtorOwnerExitScenario::ReferenceException
		|| Scenario == EDtorOwnerExitScenario::ReferenceAbort
		|| Scenario == EDtorOwnerExitScenario::ReferenceUnprepare)
	{
		AppendLine(Source, FString::Printf(
			TEXT("%sFExitReference Owner%s = FExitReference(%s);"),
			*Prefix,
			*Suffix,
			*TokenExpression));
		return;
	}
	AppendLine(Source, FString::Printf(TEXT("%sFExitValue Owner%s(%s);"), *Prefix, *Suffix, *TokenExpression));
}

void AppendSimpleTerminalRoute(
	FString& Source,
	const EDtorOwnerExitScenario Scenario,
	const FString& TokenExpression,
	const int32 IndentLevel)
{
	const FString Prefix = Indent(IndentLevel);
	if (Scenario == EDtorOwnerExitScenario::LocalEarlyReturn)
	{
		AppendLine(Source, FString::Printf(TEXT("%sif (%s > 0)"), *Prefix, *TokenExpression));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendLine(Source, Prefix + FString::Printf(TEXT("\tReachDestructorExitTerminal(%d);"), RouteId(Scenario)));
		AppendFaultOrReturn(Source, Prefix + TEXT("\t"), Scenario);
		AppendLine(Source, Prefix + TEXT("}"));
		return;
	}
	AppendLine(Source, Prefix + FString::Printf(TEXT("ReachDestructorExitTerminal(%d);"), RouteId(Scenario)));
	if (Scenario == EDtorOwnerExitScenario::LocalReturn
		|| Scenario == EDtorOwnerExitScenario::NestedLocalReturn
		|| Scenario == EDtorOwnerExitScenario::FieldReturn
		|| Scenario == EDtorOwnerExitScenario::BaseDerivedReturn
		|| Scenario == EDtorOwnerExitScenario::ReferenceReturn)
	{
		AppendFaultOrReturn(Source, Prefix, Scenario);
	}
}

void AppendLocalControlTerminal(
	FString& Source,
	const EDtorOwnerExitScenario Scenario,
	const FString& TokenExpression,
	const FString& Suffix,
	const int32 IndentLevel)
{
	const FString Prefix = Indent(IndentLevel);
	if (Scenario == EDtorOwnerExitScenario::LocalBreak)
	{
		AppendLine(Source, Prefix + TEXT("while (true)"));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendPassiveOwner(Source, Scenario, TokenExpression, Suffix, IndentLevel + 1);
		AppendLine(Source, Prefix + FString::Printf(TEXT("\tReachDestructorExitTerminal(%d);"), RouteId(Scenario)));
		AppendLine(Source, Prefix + TEXT("\tbreak;"));
		AppendLine(Source, Prefix + TEXT("}"));
		return;
	}
	if (Scenario == EDtorOwnerExitScenario::LocalContinue)
	{
		AppendLine(Source, Prefix + TEXT("for (int ExitIndex = 0; ExitIndex < 1; ++ExitIndex)"));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendPassiveOwner(Source, Scenario, TokenExpression, Suffix, IndentLevel + 1);
		AppendLine(Source, Prefix + FString::Printf(TEXT("\tReachDestructorExitTerminal(%d);"), RouteId(Scenario)));
		AppendLine(Source, Prefix + TEXT("\tcontinue;"));
		AppendLine(Source, Prefix + TEXT("}"));
		return;
	}
	AppendLine(Source, Prefix + TEXT("switch (1)"));
	AppendLine(Source, Prefix + TEXT("{"));
	AppendLine(Source, Prefix + TEXT("case 1:"));
	AppendLine(Source, Prefix + TEXT("{"));
	AppendPassiveOwner(Source, Scenario, TokenExpression, Suffix, IndentLevel + 1);
	AppendLine(Source, Prefix + FString::Printf(TEXT("\tReachDestructorExitTerminal(%d);"), RouteId(Scenario)));
	AppendLine(Source, Prefix + TEXT("\tbreak;"));
	AppendLine(Source, Prefix + TEXT("}"));
	AppendLine(Source, Prefix + TEXT("default:"));
	AppendLine(Source, Prefix + TEXT("{"));
	AppendLine(Source, Prefix + TEXT("\tbreak;"));
	AppendLine(Source, Prefix + TEXT("}"));
	AppendLine(Source, Prefix + TEXT("}"));
}

void AppendNestedLocalTerminal(
	FString& Source,
	const EDtorOwnerExitScenario Scenario,
	const FString& TokenExpression,
	const FString& Suffix,
	const int32 IndentLevel)
{
	const FString Prefix = Indent(IndentLevel);
	AppendLine(Source, FString::Printf(TEXT("%sFExitValue Outer%s(%s);"), *Prefix, *Suffix, *TokenExpression));
	AppendLine(Source, Prefix + TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("%s\tFExitValue Inner%s(%s + 1);"), *Prefix, *Suffix, *TokenExpression));
	AppendSimpleTerminalRoute(Source, Scenario, TokenExpression, IndentLevel + 1);
	AppendLine(Source, Prefix + TEXT("}"));
}

void AppendTerminalOwner(
	FString& Source,
	const EDtorOwnerExitScenario Scenario,
	const FString& TokenExpression,
	const FString& Suffix,
	const int32 IndentLevel)
{
	const FString Prefix = Indent(IndentLevel);
	if (Scenario == EDtorOwnerExitScenario::LocalBreak
		|| Scenario == EDtorOwnerExitScenario::LocalContinue
		|| Scenario == EDtorOwnerExitScenario::LocalSwitchExit)
	{
		AppendLocalControlTerminal(Source, Scenario, TokenExpression, Suffix, IndentLevel);
		return;
	}
	if (IsNestedLocal(Scenario))
	{
		AppendNestedLocalTerminal(Source, Scenario, TokenExpression, Suffix, IndentLevel);
		return;
	}
	if (Scenario == EDtorOwnerExitScenario::TemporaryEarlyReturn)
	{
		AppendLine(Source, FString::Printf(
			TEXT("%sif (ObserveExitValue(FExitValue(%s)) == %s)"),
			*Prefix,
			*TokenExpression,
			*TokenExpression));
		AppendLine(Source, Prefix + TEXT("{"));
		AppendLine(Source, Prefix + FString::Printf(TEXT("\tReachDestructorExitTerminal(%d);"), RouteId(Scenario)));
		AppendFaultOrReturn(Source, Prefix + TEXT("\t"), Scenario);
		AppendLine(Source, Prefix + TEXT("}"));
		return;
	}
	if (Scenario == EDtorOwnerExitScenario::TemporaryException)
	{
		AppendLine(Source, FString::Printf(
			TEXT("%sConsumeExitTemporaryAndTerminate(FExitValue(%s), %d);"),
			*Prefix,
			*TokenExpression,
			RouteId(Scenario)));
		AppendFaultOrReturn(Source, Prefix, Scenario);
		return;
	}
	if (Scenario == EDtorOwnerExitScenario::ArgumentCopyReturn
		|| Scenario == EDtorOwnerExitScenario::ArgumentCopyException
		|| Scenario == EDtorOwnerExitScenario::ArgumentCopyAbort)
	{
		AppendLine(Source, FString::Printf(TEXT("%sFExitValue Source%s(%s);"), *Prefix, *Suffix, *TokenExpression));
		AppendLine(Source, FString::Printf(
			TEXT("%sConsumeExitValueByValue(Source%s, %d, true);"),
			*Prefix,
			*Suffix,
			RouteId(Scenario)));
		if (IsFault(Scenario))
		{
			AppendFaultOrReturn(Source, Prefix, Scenario);
		}
		return;
	}
	AppendPassiveOwner(Source, Scenario, TokenExpression, Suffix, IndentLevel);
	AppendSimpleTerminalRoute(Source, Scenario, TokenExpression, IndentLevel);
}

void AppendEntryTail(FString& Source, const EDtorOwnerExitScenario Scenario)
{
	if (IsFault(Scenario))
	{
		AppendLine(Source, TEXT("\tint Zero = 0;"));
		AppendLine(Source, TEXT("\treturn 1 / Zero;"));
		return;
	}
	AppendLine(Source, FString::Printf(TEXT("\treturn %d;"), RouteId(Scenario)));
}

void AppendOneShape(FString& Source, const FDtorOwnerExitParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendTerminalOwner(
		Source,
		Params.Scenario,
		FString::FromInt(TokenBase(Params.Nesting) + 1),
		TEXT("One"),
		1);
	AppendEntryTail(Source, Params.Scenario);
	AppendLine(Source, TEXT("}"));
}

void AppendSequentialShape(FString& Source, const FDtorOwnerExitParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	const int32 Count = UnitCount(Params.Nesting);
	for (int32 Index = 1; Index <= Count; ++Index)
	{
		AppendLine(Source, TEXT("\t{"));
		const FString Token = FString::FromInt(TokenBase(Params.Nesting) + Index);
		const FString Suffix = FString::Printf(TEXT("Sequential%d"), Index);
		if (Index == Count)
		{
			AppendTerminalOwner(Source, Params.Scenario, Token, Suffix, 2);
		}
		else
		{
			AppendPassiveOwner(Source, Params.Scenario, Token, Suffix, 2);
		}
		AppendLine(Source, TEXT("\t}"));
	}
	AppendEntryTail(Source, Params.Scenario);
	AppendLine(Source, TEXT("}"));
}

void AppendNestedScopeLevel(
	FString& Source,
	const FDtorOwnerExitParams& Params,
	const int32 Level)
{
	const FString Prefix = Indent(Level);
	AppendLine(Source, Prefix + TEXT("{"));
	const FString Token = FString::FromInt(TokenBase(Params.Nesting) + Level);
	const FString Suffix = FString::Printf(TEXT("Scope%d"), Level);
	if (Level == UnitCount(Params.Nesting))
	{
		AppendTerminalOwner(Source, Params.Scenario, Token, Suffix, Level + 1);
	}
	else
	{
		AppendPassiveOwner(Source, Params.Scenario, Token, Suffix, Level + 1);
		AppendNestedScopeLevel(Source, Params, Level + 1);
	}
	AppendLine(Source, Prefix + TEXT("}"));
}

void AppendNestedScopesShape(FString& Source, const FDtorOwnerExitParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendNestedScopeLevel(Source, Params, 1);
	AppendEntryTail(Source, Params.Scenario);
	AppendLine(Source, TEXT("}"));
}

void AppendNestedCallsShape(FString& Source, const FDtorOwnerExitParams& Params, const FString& FunctionName)
{
	const int32 Count = UnitCount(Params.Nesting);
	for (int32 Level = Count; Level >= 1; --Level)
	{
		AppendLine(Source, FString::Printf(TEXT("int %sCall%d()"), *FunctionName, Level));
		AppendLine(Source, TEXT("{"));
		const FString Token = FString::FromInt(TokenBase(Params.Nesting) + Level);
		const FString Suffix = FString::Printf(TEXT("Call%d"), Level);
		if (Level == Count)
		{
			AppendTerminalOwner(Source, Params.Scenario, Token, Suffix, 1);
			AppendEntryTail(Source, Params.Scenario);
		}
		else
		{
			AppendPassiveOwner(Source, Params.Scenario, Token, Suffix, 1);
			AppendLine(Source, FString::Printf(TEXT("\treturn %sCall%d();"), *FunctionName, Level + 1));
		}
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (IsFault(Params.Scenario))
	{
		AppendLine(Source, FString::Printf(TEXT("\tint Result = %sCall1();"), *FunctionName));
		AppendLine(Source, TEXT("\tint Zero = 0;"));
		AppendLine(Source, TEXT("\treturn Result + 1 / Zero;"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %sCall1();"), *FunctionName));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendLoopShape(FString& Source, const FDtorOwnerExitParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(
		TEXT("\tfor (int ShapeIndex = 1; ShapeIndex <= %d; ++ShapeIndex)"),
		UnitCount(Params.Nesting)));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, FString::Printf(TEXT("\t\tif (ShapeIndex < %d)"), UnitCount(Params.Nesting)));
	AppendLine(Source, TEXT("\t\t{"));
	AppendPassiveOwner(
		Source,
		Params.Scenario,
		FString::Printf(TEXT("%d + ShapeIndex"), TokenBase(Params.Nesting)),
		TEXT("LoopPassive"),
		3);
	AppendLine(Source, TEXT("\t\t}"));
	AppendLine(Source, TEXT("\t\telse"));
	AppendLine(Source, TEXT("\t\t{"));
	AppendTerminalOwner(
		Source,
		Params.Scenario,
		FString::Printf(TEXT("%d + ShapeIndex"), TokenBase(Params.Nesting)),
		TEXT("LoopTerminal"),
		3);
	AppendLine(Source, TEXT("\t\t}"));
	AppendLine(Source, TEXT("\t}"));
	AppendEntryTail(Source, Params.Scenario);
	AppendLine(Source, TEXT("}"));
}

void AppendRecursionShape(FString& Source, const FDtorOwnerExitParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %sRecursive(int Depth)"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tif (Depth <= 1)"));
	AppendLine(Source, TEXT("\t{"));
	AppendTerminalOwner(
		Source,
		Params.Scenario,
		FString::Printf(TEXT("%d + Depth"), TokenBase(Params.Nesting)),
		TEXT("RecursiveTerminal"),
		2);
	AppendFaultOrReturn(Source, TEXT("\t\t"), Params.Scenario);
	AppendLine(Source, TEXT("\t}"));
	AppendPassiveOwner(
		Source,
		Params.Scenario,
		FString::Printf(TEXT("%d + Depth"), TokenBase(Params.Nesting)),
		TEXT("RecursivePassive"),
		1);
	AppendLine(Source, FString::Printf(TEXT("\treturn %sRecursive(Depth - 1);"), *FunctionName));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (IsFault(Params.Scenario))
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tint Result = %sRecursive(%d);"),
			*FunctionName,
			UnitCount(Params.Nesting)));
		AppendLine(Source, TEXT("\tint Zero = 0;"));
		AppendLine(Source, TEXT("\treturn Result + 1 / Zero;"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %sRecursive(%d);"), *FunctionName, UnitCount(Params.Nesting)));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendGlobalShape(FString& Source, const FDtorOwnerExitParams& Params, const FString& FunctionName)
{
	const int32 Base = TokenBase(Params.Nesting);
	if (Params.Nesting == EDtorOwnerExitNesting::One)
	{
		AppendLine(Source, FString::Printf(
			TEXT("FExitReference %sExitGlobal = FExitReference(%d);"),
			*FunctionName,
			Base + 1));
	}
	else if (Params.Nesting == EDtorOwnerExitNesting::Sequential)
	{
		for (int32 Index = 1; Index <= UnitCount(Params.Nesting); ++Index)
		{
			AppendLine(Source, FString::Printf(
				TEXT("FExitReference %sExitGlobal%d = FExitReference(%d);"),
				*FunctionName,
				Index,
				Base + Index));
		}
	}
	else if (Params.Nesting == EDtorOwnerExitNesting::NestedScopes)
	{
		AppendLine(Source, FString::Printf(
			TEXT("FExitFieldOwner %sExitGlobal = FExitFieldOwner(%d);"),
			*FunctionName,
			Base + 1));
	}
	else if (Params.Nesting == EDtorOwnerExitNesting::Loop)
	{
		AppendLine(Source, FString::Printf(
			TEXT("FExitReference %sExitGlobal = BuildExitGlobalLoop(%d);"),
			*FunctionName,
			Base));
	}
	else if (Params.Nesting == EDtorOwnerExitNesting::NestedCalls)
	{
		AppendLine(Source, FString::Printf(
			TEXT("FExitReference %sExitGlobal = BuildExitGlobalCall1(%d);"),
			*FunctionName,
			Base));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("FExitReference %sExitGlobal = BuildExitGlobalNestedCall(%d, %d);"),
			*FunctionName,
			UnitCount(Params.Nesting),
			Base));
	}
	AppendLine(Source, FString::Printf(
		TEXT("int %sExitGlobalReady = RecordDestructorExitGlobalReady(%d);"),
		*FunctionName,
		RouteId(Params.Scenario)));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tif (%sExitGlobalReady <= 0)"), *FunctionName));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tReachDestructorExitTerminal(-1);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %d;"), RouteId(Params.Scenario)));
	AppendLine(Source, TEXT("}"));
}

void AppendNonGlobalShape(FString& Source, const FDtorOwnerExitParams& Params, const FString& FunctionName)
{
	if (Params.Nesting == EDtorOwnerExitNesting::One)
	{
		AppendOneShape(Source, Params, FunctionName);
	}
	else if (Params.Nesting == EDtorOwnerExitNesting::Sequential)
	{
		AppendSequentialShape(Source, Params, FunctionName);
	}
	else if (Params.Nesting == EDtorOwnerExitNesting::NestedScopes)
	{
		AppendNestedScopesShape(Source, Params, FunctionName);
	}
	else if (Params.Nesting == EDtorOwnerExitNesting::NestedCalls)
	{
		AppendNestedCallsShape(Source, Params, FunctionName);
	}
	else if (Params.Nesting == EDtorOwnerExitNesting::Loop)
	{
		AppendLoopShape(Source, Params, FunctionName);
	}
	else
	{
		AppendRecursionShape(Source, Params, FunctionName);
	}
}

void AppendCaseSource(FString& Source, const FDtorOwnerExitParams& Params, const FString& FunctionName)
{
	if (IsGlobal(Params.Scenario))
	{
		AppendGlobalShape(Source, Params, FunctionName);
	}
	else
	{
		AppendNonGlobalShape(Source, Params, FunctionName);
	}
}

FString BuildOwnedSource(const FDtorOwnerExitParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedTypes(Source);
	AppendCaseSource(Source, Params, FunctionName);
	return Source;
}

TArray<FOwnerExitCell> AllCells()
{
	TArray<FOwnerExitCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FOwnerExitCell& Cell = Cells[Index];
		Cell.Scenario = static_cast<EDtorOwnerExitScenario>(Index / (NestingAxisCount * ObservationAxisCount));
		const int32 Remainder = Index % (NestingAxisCount * ObservationAxisCount);
		Cell.Nesting = static_cast<EDtorOwnerExitNesting>(Remainder / ObservationAxisCount);
		Cell.Observation = static_cast<EDtorOwnerExitObservation>(Remainder % ObservationAxisCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FOwnerExitCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FOwnerExitCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FGeneratedCaseInfo MakeCaseInfo(const FOwnerExitCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
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
		Info.ExpectedReturn = RouteId(Cell.Scenario);
	}
	return Info;
}
}

FStringView FDtorOwnerExitGenerator::GetProductId() const
{
	return TEXT("LANG-DTOR-OWNER-EXIT");
}

TArray<FGeneratedCaseInfo> FDtorOwnerExitGenerator::ListCases() const
{
	using namespace DtorOwnerExitGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FOwnerExitCell& Cell : AllCells())
	{
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("DtorOwnerExit PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FDtorOwnerExitGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace DtorOwnerExitGeneratorDetail;
	FOwnerExitCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FDtorOwnerExitGenerator::BuildDumpSource() const
{
	using namespace DtorOwnerExitGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-DTOR-OWNER-EXIT"));
	AppendLine(Source, TEXT("// Cases: 666 total, 522 normal, 0 reject, 144 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedTypes(Source);
	for (const FOwnerExitCell& Cell : AllCells())
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
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
		AppendCaseSource(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FDtorOwnerExitGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace DtorOwnerExitGeneratorDetail;
	FString Source;
	AppendSharedTypes(Source);
	int32 Emitted = 0;
	for (const FOwnerExitCell& Cell : AllCells())
	{
		AppendLine(Source);
		AppendCaseSource(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FDtorOwnerExitGenerator::BuildDestructorExitSource(const FDtorOwnerExitParams& Params) const
{
	using namespace DtorOwnerExitGeneratorDetail;
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

int32 FDtorOwnerExitGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace DtorOwnerExitGeneratorDetail;
	FOwnerExitCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsFault(Cell.Scenario))
	{
		return 0;
	}
	return RouteId(Cell.Scenario);
}
}
