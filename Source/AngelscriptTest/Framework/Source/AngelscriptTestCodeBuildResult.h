#pragma once

#include "Framework/Source/AngelscriptTestSourceCase.h"

#include "Containers/Array.h"
#include "Templates/UniquePtr.h"

namespace AngelscriptTest
{
class FAngelscriptTestCodeBuilder;
class FAngelscriptTestCode;
class FAngelscriptTestSourceParser;
struct FAngelscriptTestCodeBuildResultTestAccess;
struct FAngelscriptTestCodeBuildResultParserTestAccess;

class ANGELSCRIPTTEST_API FAngelscriptTestCodeBuildResult
{
public:
	~FAngelscriptTestCodeBuildResult();
	FAngelscriptTestCodeBuildResult(FAngelscriptTestCodeBuildResult&& Other) noexcept;
	FAngelscriptTestCodeBuildResult& operator=(FAngelscriptTestCodeBuildResult&& Other) noexcept;
	FAngelscriptTestCodeBuildResult(const FAngelscriptTestCodeBuildResult&) = delete;
	FAngelscriptTestCodeBuildResult& operator=(const FAngelscriptTestCodeBuildResult&) = delete;

	[[nodiscard]] bool IsSuccess() const;
	const TArray<FAngelscriptTestError>& GetErrors() const &;
	const TArray<FAngelscriptTestError>& GetErrors() const && = delete;

private:
	struct FPayload;
	explicit FAngelscriptTestCodeBuildResult(TArray<FAngelscriptTestSourceCase> SourceCases);
	explicit FAngelscriptTestCodeBuildResult(TArray<FAngelscriptTestError> Errors);

	const TArray<FAngelscriptTestSourceCase>& GetCasesForAdmission() const &;
	TArray<FAngelscriptTestSourceCase> TakeCasesForAdmission();

	TUniquePtr<FPayload> Payload;
	TArray<FAngelscriptTestError> Errors;

	friend class FAngelscriptTestCodeBuilder;
	friend class FAngelscriptTestCode;
	friend class FAngelscriptTestSourceParser;
	friend struct FAngelscriptTestCodeBuildResultTestAccess;
	friend struct FAngelscriptTestCodeBuildResultParserTestAccess;
};
}
