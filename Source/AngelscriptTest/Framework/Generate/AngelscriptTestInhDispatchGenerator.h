#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EInhDispatchDepth : uint8
{
	Base,
	TwoLevels,
	ThreeLevels,
	Deep,
};

enum class EInhDispatchMember : uint8
{
	Field,
	NonvirtualMethod,
	VirtualMethod,
	Override,
	GetterMethod,
	SetterMethod,
};

enum class EInhDispatchView : uint8
{
	DerivedObject,
	BaseView,
	ExplicitBase,
	Owner,
	DerivedImpl,
};

enum class EInhDispatchInvocation : uint8
{
	Direct,
	VirtualRoute,
	ExplicitBase,
};

struct FInhDispatchParams
{
	EInhDispatchDepth Depth = EInhDispatchDepth::Base;
	EInhDispatchMember Member = EInhDispatchMember::Field;
	EInhDispatchView View = EInhDispatchView::DerivedObject;
	EInhDispatchInvocation Invocation = EInhDispatchInvocation::Direct;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FInhDispatchGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildInheritanceDispatchSource(const FInhDispatchParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
