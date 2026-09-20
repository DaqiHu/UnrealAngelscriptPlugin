#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ERefSourceOpOperation : uint8
{
	Initialize,
	Assign,
	Pass,
	Return,
	Identity,
	NullCompare,
	Cast,
	MemberAccess,
	AliasMutation,
};

enum class ERefSourceOpQualifier : uint8
{
	Mutable,
	ConstObject,
	ConstInput,
	ConstRemovalInvalid,
};

enum class ERefSourceOpSource : uint8
{
	NewLocal,
	Field,
	Parameter,
	Return,
	BaseView,
	DerivedView,
	NativeObject,
	Null,
};

struct FRefSourceOpParams
{
	ERefSourceOpOperation Operation = ERefSourceOpOperation::Initialize;
	ERefSourceOpQualifier Qualifier = ERefSourceOpQualifier::Mutable;
	ERefSourceOpSource Source = ERefSourceOpSource::NewLocal;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FRefSourceOpGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildReferenceIdentitySource(const FRefSourceOpParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
