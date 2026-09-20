#pragma once

#include "Framework/Source/AngelscriptTestCodeBuildResult.h"
#include "Framework/Source/AngelscriptTestSourceTypes.h"

#include "Delegates/Delegate.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"

namespace AngelscriptTest
{
class FAngelscriptTestCode;
struct FAngelscriptTestCodeRegistrationTestAccess;

class FAngelscriptTestCodeRegistry
{
public:
	~FAngelscriptTestCodeRegistry();
	FAngelscriptTestCodeRegistry(const FAngelscriptTestCodeRegistry&) = delete;
	FAngelscriptTestCodeRegistry& operator=(const FAngelscriptTestCodeRegistry&) = delete;

private:
	struct FRecord
	{
		FAngelscriptTestFileMeta FileMeta;
		FAngelscriptTestCodeBuildResult (*Factory)(const FAngelscriptTestFileMeta&) = nullptr;
		FString OwnerModule;
		FString SourceFile;
		int32 SourceLine = 0;
	};

	struct FBatchRecord
	{
		using FFactory = void (*)(
			TArray<FAngelscriptTestCodeBuildResult>& OutFiles,
			TArray<FAngelscriptTestError>& OutErrors);

		FFactory Factory = nullptr;
		FString OwnerModule;
		FString SourceFile;
		int32 SourceLine = 0;
	};

	struct FSnapshot
	{
		TArray<FRecord> Records;
		TArray<FBatchRecord> BatchRecords;
	};

	FAngelscriptTestCodeRegistry();
	void Add(FRecord Record);
	void AddBatch(FBatchRecord Record);
	FSnapshot Snapshot(FAngelscriptTestCode& Code);
	void Unbind(const FAngelscriptTestCode& Code);

	struct FData;
	TUniquePtr<FData> Data;

	friend class FAngelscriptTestCode;
	friend class FAngelscriptTestCodeRegistration;
	friend struct FAngelscriptTestCodeRegistrationTestAccess;
};

class ANGELSCRIPTTEST_API FAngelscriptTestCodeRegistration
{
public:
	using FCodeFactory = FAngelscriptTestCodeBuildResult (*)(const FAngelscriptTestFileMeta&);

	FAngelscriptTestCodeRegistration(
		FAngelscriptTestFileMeta FileMeta,
		FCodeFactory Factory,
		const ANSICHAR* OwnerModule = UE_MODULE_NAME,
		const ANSICHAR* SourceFile = __builtin_FILE(),
		int32 SourceLine = __builtin_LINE());

private:
	FAngelscriptTestCodeRegistration(
		FAngelscriptTestCodeRegistry& Registry,
		FAngelscriptTestFileMeta FileMeta,
		FCodeFactory Factory,
		const ANSICHAR* OwnerModule,
		const ANSICHAR* SourceFile,
		int32 SourceLine);

	static FAngelscriptTestCodeRegistry& GetGlobalRegistry();

	friend class FAngelscriptTestCode;
	friend struct FAngelscriptTestCodeRegistrationTestAccess;
};

class ANGELSCRIPTTEST_API FAngelscriptTestCodeActivationBarrier
{
public:
	explicit FAngelscriptTestCodeActivationBarrier(TFunction<void()> Activation);
	~FAngelscriptTestCodeActivationBarrier();

	void Install(bool bStartupLoadingComplete, FSimpleMulticastDelegate& CompletionDelegate);
	void Shutdown();

private:
	void HandleCompletion();

	TFunction<void()> Activation;
	FSimpleMulticastDelegate* CompletionDelegate = nullptr;
	FDelegateHandle CompletionHandle;
};
}
