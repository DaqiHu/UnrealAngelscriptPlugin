#pragma once

#include "Framework/Source/AngelscriptTestSource.h"
#include "Framework/Source/AngelscriptTestSourceTypes.h"

#include "Templates/SharedPointer.h"

namespace AngelscriptTest
{
class FAngelscriptTestCodeBuilder;
class FAngelscriptTestCode;
struct FAngelscriptTestCodeBuildResultTestAccess;

class ANGELSCRIPTTEST_API FAngelscriptTestSourceCase
{
public:
	FAngelscriptTestSourceCase(const FAngelscriptTestSourceCase&) = default;
	FAngelscriptTestSourceCase(FAngelscriptTestSourceCase&&) = default;
	FAngelscriptTestSourceCase& operator=(const FAngelscriptTestSourceCase&) = default;
	FAngelscriptTestSourceCase& operator=(FAngelscriptTestSourceCase&&) = default;

	const FAngelscriptTestFileMeta& GetFileMeta() const &;
	const FAngelscriptTestFileMeta& GetFileMeta() const && = delete;
	const FAngelscriptTestVersionMeta& GetVersionMeta() const &;
	const FAngelscriptTestVersionMeta& GetVersionMeta() const && = delete;
	FAngelscriptTestSource GetSource() const;

private:
	struct FData;
	FAngelscriptTestSourceCase(
		TSharedRef<const FAngelscriptTestFileMeta, ESPMode::ThreadSafe> FileMeta,
		FAngelscriptTestVersionMeta VersionMeta,
		FAngelscriptTestSource Source);

	TSharedRef<const FData, ESPMode::ThreadSafe> Data;

	friend class FAngelscriptTestCodeBuilder;
	friend class FAngelscriptTestCode;
	friend struct FAngelscriptTestCodeBuildResultTestAccess;
};
}
