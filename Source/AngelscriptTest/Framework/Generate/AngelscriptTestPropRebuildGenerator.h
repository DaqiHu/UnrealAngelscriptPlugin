#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EPropRebuildObservation : uint8
{
	Metadata,
	Runtime,
	OldHandleCleanup,
};

enum class EPropRebuildPath : uint8
{
	Rebuild,
	SaveLoad,
};

enum class EPropRebuildScenario : uint8
{
	StoredSameSource,
	StoredValue,
	StoredFieldType,
	StoredFieldOrder,
	StoredInheritance,
	RegisteredSameSource,
	RegisteredReadValue,
	RegisteredGetterPresence,
	RegisteredSetterPresence,
	RegisteredConstness,
	RegisteredIndexedSameSource,
	RegisteredIndexedValue,
	RegisteredIndexedConstness,
	RegisteredIndexedIndexType,
	RegisteredIndexedOverloadSet,
};

struct FPropRebuildParams
{
	EPropRebuildObservation Observation = EPropRebuildObservation::Metadata;
	EPropRebuildPath Path = EPropRebuildPath::Rebuild;
	EPropRebuildScenario Scenario = EPropRebuildScenario::StoredSameSource;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FPropRebuildGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildScenarioSource(const FPropRebuildParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
