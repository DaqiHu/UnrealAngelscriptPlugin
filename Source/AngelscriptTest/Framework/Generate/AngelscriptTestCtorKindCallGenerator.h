#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ECtorKindCallCall : uint8
{
	Local,
	Temporary,
	Field,
	Return,
	Argument,
	BaseCall,
	CopyDeclaration,
	Assignment,
};

enum class ECtorKindCallKind : uint8
{
	ImplicitDefault,
	DeclaredDefault,
	Parameterized,
	Overloaded,
	Copy,
	Conversion,
};

enum class ECtorKindCallObject : uint8
{
	ScriptValue,
	ScriptReference,
	Base,
	Derived,
	NativeValue,
	NativeReference,
};

struct FCtorKindCallParams
{
	ECtorKindCallCall Call = ECtorKindCallCall::Local;
	ECtorKindCallKind Kind = ECtorKindCallKind::ImplicitDefault;
	ECtorKindCallObject Object = ECtorKindCallObject::ScriptValue;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FCtorKindCallGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildConstructorSelectionSource(const FCtorKindCallParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
