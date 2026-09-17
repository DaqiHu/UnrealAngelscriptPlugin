#pragma once

#include "Framework/Source/AngelscriptTestCodeBuildResult.h"
#include "Framework/Source/AngelscriptTestSource.h"
#include "Framework/Source/AngelscriptTestSourceDescriptor.h"
#include "Framework/Source/AngelscriptTestSourceTypes.h"

#include "Misc/Optional.h"
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
	void AddRoot(
		FAngelscriptTestVersionMeta VersionMeta,
		FAngelscriptTestSource Source,
		FAngelscriptTestSourceDescriptor Descriptor);
	void AddVersion(FAngelscriptTestVersionMeta VersionMeta, FAngelscriptTestSource Source);
	void AddVersion(
		FAngelscriptTestVersionMeta VersionMeta,
		FAngelscriptTestSource Source,
		FAngelscriptTestSourceDescriptor Descriptor);
	[[nodiscard]] FAngelscriptTestCodeBuildResult Build();

private:
	void AddNode(
		FAngelscriptTestVersionMeta VersionMeta,
		FAngelscriptTestSource Source,
		bool bRoot,
		TOptional<FAngelscriptTestSourceDescriptor> Descriptor = {});
	static FAngelscriptTestSource ApplySourceDescriptor(
		FAngelscriptTestSource Source,
		const FAngelscriptTestSourceDescriptor& Descriptor);

	struct FData;
	TUniquePtr<FData> Data;
};
}
