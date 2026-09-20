#pragma once

#include "Framework/Source/AngelscriptTestSource.h"
#include "Framework/Source/AngelscriptTestSourceTypes.h"

#include "Misc/Optional.h"

namespace AngelscriptTest
{
class FAngelscriptTestAnnotations;

class ANGELSCRIPTTEST_API FAngelscriptTestSourceParseResult
{
public:
	[[nodiscard]] bool IsSuccess() const;
	const FAngelscriptTestSource* GetSource() const &;
	const FAngelscriptTestSource* GetSource() const && = delete;
	const TArray<FAngelscriptTestError>& GetErrors() const &;
	const TArray<FAngelscriptTestError>& GetErrors() const && = delete;

private:
	explicit FAngelscriptTestSourceParseResult(FAngelscriptTestSource Source);
	explicit FAngelscriptTestSourceParseResult(TArray<FAngelscriptTestError> Errors);

	TOptional<FAngelscriptTestSource> Source;
	TArray<FAngelscriptTestError> Errors;

	friend class FAngelscriptTestAnnotations;
};
}
