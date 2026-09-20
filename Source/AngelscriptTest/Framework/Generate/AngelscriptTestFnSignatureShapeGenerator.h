#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnSignatureShapeTarget : uint8
{
	Global,
	NamespaceGlobal,
	InstanceMethod,
};

enum class EFnSignatureShapeDirectionPattern : uint8
{
	AllValue,
	AllIn,
	AllOut,
	AlternatingInoutOut,
};

enum class EFnSignatureShapeTypePattern : uint8
{
	HomogeneousInt,
	HomogeneousFloat,
	AlternatingIntFloat,
	AlternatingFloatInt,
};

enum class EFnSignatureShapeArity : uint8
{
	One,
	Two,
	Three,
	Four,
};

struct FFnSignatureShapeParams
{
	EFnSignatureShapeTarget Target = EFnSignatureShapeTarget::Global;
	EFnSignatureShapeDirectionPattern Direction = EFnSignatureShapeDirectionPattern::AllValue;
	EFnSignatureShapeTypePattern Type = EFnSignatureShapeTypePattern::HomogeneousInt;
	EFnSignatureShapeArity Arity = EFnSignatureShapeArity::One;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnSignatureShapeGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildSource(const FFnSignatureShapeParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
