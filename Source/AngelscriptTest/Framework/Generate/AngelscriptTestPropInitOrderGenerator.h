#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EPropInitOrderObservation : uint8
{
	BaseEntry,
	BaseExit,
	DerivedEntry,
	DerivedExit,
};

enum class EPropInitOrderPosition : uint8
{
	BaseFirst,
	BaseMiddle,
	BaseLast,
	DerivedFirst,
	DerivedLast,
};

enum class EPropInitOrderSource : uint8
{
	DefaultValue,
	DeclarationInitializer,
	OwnerLiteralAssignment,
	OwnerSourceAssignment,
	DerivedReassignment,
};

enum class EPropInitOrderType : uint8
{
	Int8,
	Int16,
	Int,
	Int64,
	UInt8,
	UInt16,
	UInt,
	UInt64,
	Float32,
	Float64,
	Bool,
	Enum,
	Typedef,
	ScriptValue,
	NativeValue,
};

struct FPropInitOrderParams
{
	EPropInitOrderObservation Observation = EPropInitOrderObservation::BaseEntry;
	EPropInitOrderPosition Position = EPropInitOrderPosition::BaseFirst;
	EPropInitOrderSource Source = EPropInitOrderSource::DefaultValue;
	EPropInitOrderType Type = EPropInitOrderType::Int8;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FPropInitOrderGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildPropertyInitializationSource(const FPropInitOrderParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
