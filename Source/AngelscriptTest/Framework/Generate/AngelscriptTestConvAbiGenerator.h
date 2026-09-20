#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EConvAbiScriptDeclaration : uint8
{
	Float,
	Double,
};

enum class EConvAbiNativeStorage : uint8
{
	Float32,
	Float64,
};

enum class EConvAbiDirection : uint8
{
	Argument,
	Return,
	Property,
};

struct FConvAbiParams
{
	EConvAbiScriptDeclaration ScriptDeclaration = EConvAbiScriptDeclaration::Float;
	EConvAbiNativeStorage NativeStorage = EConvAbiNativeStorage::Float32;
	EConvAbiDirection Direction = EConvAbiDirection::Argument;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FConvAbiGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildAbiSource(const FConvAbiParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
