#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/Optional.h"

namespace AngelscriptTest
{
struct ANGELSCRIPTTEST_API FAngelscriptTestFileMeta
{
	FString Version;
	FString Tag;
	FString Summary;
	TArray<FString> Topics;
};

struct ANGELSCRIPTTEST_API FAngelscriptTestVersionMeta
{
	FString Tag;
	TOptional<FString> Parent;
	FString Summary;
	TArray<FString> Topics;
};

struct ANGELSCRIPTTEST_API FAngelscriptTestError
{
	FString Code;
	FString Message;
	FString OwnerModule;
	FString FileTag;
	FString VersionTag;
	FString SourcePath;
	int32 SourceLine = 0;
	int32 ByteOffset = INDEX_NONE;
};

class ANGELSCRIPTTEST_API FAngelscriptTestStatus
{
public:
	static FAngelscriptTestStatus Success();
	static FAngelscriptTestStatus Failure(FAngelscriptTestError Error);
	static FAngelscriptTestStatus Failure(TArray<FAngelscriptTestError> Errors);

	[[nodiscard]] bool IsSuccess() const;
	const TArray<FAngelscriptTestError>& GetErrors() const &;
	const TArray<FAngelscriptTestError>& GetErrors() const && = delete;

private:
	explicit FAngelscriptTestStatus(TArray<FAngelscriptTestError> Errors);

	TArray<FAngelscriptTestError> Errors;
};
}
