#pragma once

#include "NewVersion/Framework/Source/AngelscriptTestSourceCase.h"

#include "Misc/Optional.h"

namespace AngelscriptTest
{
class FAngelscriptTestCode;
struct FAngelscriptTestCodeBuildResultTestAccess;

class ANGELSCRIPTTEST_API FAngelscriptTestSourceResult
{
public:
	[[nodiscard]] bool IsSuccess() const;
	const FAngelscriptTestSourceCase* GetCase() const &;
	const FAngelscriptTestSourceCase* GetCase() const && = delete;
	const FAngelscriptTestError* GetError() const &;
	const FAngelscriptTestError* GetError() const && = delete;

private:
	explicit FAngelscriptTestSourceResult(FAngelscriptTestSourceCase SourceCase);
	explicit FAngelscriptTestSourceResult(FAngelscriptTestError Error);

	TOptional<FAngelscriptTestSourceCase> SourceCase;
	TOptional<FAngelscriptTestError> Error;

	friend class FAngelscriptTestCode;
	friend struct FAngelscriptTestCodeBuildResultTestAccess;
};
}
