#pragma once

#include "NewVersion/Framework/Source/AngelscriptTestCodeBuildResult.h"
#include "NewVersion/Framework/Source/AngelscriptTestSourceResult.h"
#include "NewVersion/Framework/Source/AngelscriptTestSourceTypes.h"

#include "Templates/UniquePtr.h"

namespace AngelscriptTest
{
class FAngelscriptTestCodeRegistration;
class FAngelscriptTestCodeRegistry;
struct FAngelscriptTestCodeDatabaseTestAccess;
struct FAngelscriptTestCodeRegistrationTestAccess;

class ANGELSCRIPTTEST_API FAngelscriptTestCode
{
public:
	~FAngelscriptTestCode();
	FAngelscriptTestCode(const FAngelscriptTestCode&) = delete;
	FAngelscriptTestCode& operator=(const FAngelscriptTestCode&) = delete;

	static FAngelscriptTestCode& GetInstance();
	FAngelscriptTestStatus ActivateRegistrations();

	FAngelscriptTestSourceResult Get(FStringView FileTag, FStringView VersionTag) const;
	FAngelscriptTestStatus FindCases(
		FStringView FileTag,
		TConstArrayView<FString> RequiredVersionTopics,
		TArray<FAngelscriptTestSourceCase>& OutCases) const;
	FAngelscriptTestStatus FindFiles(
		TConstArrayView<FString> RequiredFileTopics,
		TArray<FAngelscriptTestFileMeta>& OutFiles) const;

	[[nodiscard]] bool IsActivated() const;
	TArray<FAngelscriptTestError> GetRegistrationErrors() const;

private:
	struct FAdmissionBatch
	{
		FString OwnerModule;
		FString SourceFile;
		int32 SourceLine = 0;
		TArray<FAngelscriptTestCodeBuildResult> Files;
		TArray<FAngelscriptTestError> Errors;
	};

	FAngelscriptTestCode();
	FAngelscriptTestStatus ActivateRegistrations(FAngelscriptTestCodeRegistry& Registry);
	FAngelscriptTestStatus AdmitDetachedBatches(TArray<FAdmissionBatch> Batches);
	void RetainRegistrationError(FAngelscriptTestError Error);

	struct FData;
	TUniquePtr<FData> Data;

	friend class FAngelscriptTestCodeRegistration;
	friend class FAngelscriptTestCodeRegistry;
	friend struct FAngelscriptTestCodeDatabaseTestAccess;
	friend struct FAngelscriptTestCodeRegistrationTestAccess;
};
}
