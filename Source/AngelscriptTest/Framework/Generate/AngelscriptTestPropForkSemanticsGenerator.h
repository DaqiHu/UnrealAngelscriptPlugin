#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EPropForkSemanticsFamily : uint8
{
	ScriptDecorator,
	NativeRegistration,
	AutomaticAccess,
	DirectMethod,
};

enum class EPropForkSemanticsVariant : uint8
{
	Getter,
	Setter,
	IndexedGetter,
	IndexedSetter,
	Read,
	Write,
	IndexedRead,
	IndexedWrite,
	ReadWriteIndexed,
};

struct FPropForkSemanticsParams
{
	EPropForkSemanticsFamily Family = EPropForkSemanticsFamily::ScriptDecorator;
	EPropForkSemanticsVariant Variant = EPropForkSemanticsVariant::Getter;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FPropForkSemanticsGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildDirectMethodSource(const FPropForkSemanticsParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
