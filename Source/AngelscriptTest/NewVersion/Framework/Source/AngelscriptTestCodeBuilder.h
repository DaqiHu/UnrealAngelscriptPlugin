#pragma once

#include "NewVersion/Framework/Source/AngelscriptTestCodeBuildResult.h"
#include "NewVersion/Framework/Source/AngelscriptTestSource.h"
#include "NewVersion/Framework/Source/AngelscriptTestSourceTypes.h"

#include "Templates/UniquePtr.h"

namespace AngelscriptTest
{
class ANGELSCRIPTTEST_API FAngelscriptTestCodeBuilder
{
public:
	explicit FAngelscriptTestCodeBuilder(const FAngelscriptTestFileMeta& FileMeta);
	~FAngelscriptTestCodeBuilder();
	FAngelscriptTestCodeBuilder(const FAngelscriptTestCodeBuilder&) = delete;
	FAngelscriptTestCodeBuilder& operator=(const FAngelscriptTestCodeBuilder&) = delete;

	void AddRoot(FAngelscriptTestVersionMeta VersionMeta, FAngelscriptTestSource Source);
	void AddVersion(FAngelscriptTestVersionMeta VersionMeta, FAngelscriptTestSource Source);
	[[nodiscard]] FAngelscriptTestCodeBuildResult Build();

private:
	void AddNode(
		FAngelscriptTestVersionMeta VersionMeta,
		FAngelscriptTestSource Source,
		bool bRoot);

	struct FData;
	TUniquePtr<FData> Data;
};
}
