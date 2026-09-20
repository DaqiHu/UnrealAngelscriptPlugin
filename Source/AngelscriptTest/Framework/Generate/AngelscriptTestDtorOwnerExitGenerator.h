#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EDtorOwnerExitScenario : uint8
{
	LocalBlockEnd,
	LocalReturn,
	LocalEarlyReturn,
	LocalBreak,
	LocalContinue,
	LocalSwitchExit,
	LocalException,
	LocalAbort,
	LocalUnprepare,
	NestedLocalBlockEnd,
	NestedLocalReturn,
	NestedLocalException,
	FieldBlockEnd,
	FieldReturn,
	FieldException,
	FieldAbort,
	BaseDerivedBlockEnd,
	BaseDerivedReturn,
	BaseDerivedException,
	BaseDerivedAbort,
	TemporaryStatementEnd,
	TemporaryEarlyReturn,
	TemporaryException,
	ReturnedValueConsume,
	ReturnedValueDiscard,
	ReturnedValueException,
	ArgumentCopyReturn,
	ArgumentCopyException,
	ArgumentCopyAbort,
	ReferenceScopeEnd,
	ReferenceAliasScopeEnd,
	ReferenceReturn,
	ReferenceException,
	ReferenceAbort,
	ReferenceUnprepare,
	ModuleDiscardGlobal,
	EngineShutdownGlobal,
};

enum class EDtorOwnerExitNesting : uint8
{
	One,
	Sequential,
	NestedScopes,
	NestedCalls,
	Loop,
	Recursion,
};

enum class EDtorOwnerExitObservation : uint8
{
	EventOrder,
	OwnershipOnce,
	TerminalStateRecovery,
};

struct FDtorOwnerExitParams
{
	EDtorOwnerExitScenario Scenario = EDtorOwnerExitScenario::LocalBlockEnd;
	EDtorOwnerExitNesting Nesting = EDtorOwnerExitNesting::One;
	EDtorOwnerExitObservation Observation = EDtorOwnerExitObservation::EventOrder;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FDtorOwnerExitGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildDestructorExitSource(const FDtorOwnerExitParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
