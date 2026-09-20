#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EVarRefDecl : uint8
{
	ExplicitType,
	Auto,
	ConstView,
};

enum class EVarRefSource : uint8
{
	ConstructedLocal,
	Parameter,
	FunctionReturn,
	Field,
	Null,
};

enum class EVarRefType : uint8
{
	ScriptReference,
	NativeReference,
};

enum class EVarRefUse : uint8
{
	Identity,
	Mutation,
	Argument,
	Return,
	NullCompare,
};

struct FVarReferenceInitParams
{
	EVarRefDecl Declaration = EVarRefDecl::ExplicitType;
	EVarRefSource Source = EVarRefSource::ConstructedLocal;
	EVarRefType Type = EVarRefType::ScriptReference;
	EVarRefUse Use = EVarRefUse::Identity;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FVarReferenceInitGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildReferenceInitializationSource(const FVarReferenceInitParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
