#include "Framework/Catalog/AngelscriptTestCodeRegistration.h"

#include "Framework/Catalog/AngelscriptTestCode.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

namespace AngelscriptTest
{
struct FAngelscriptTestCodeRegistry::FData
{
	FCriticalSection Mutex;
	TArray<FRecord> Records;
	TArray<FBatchRecord> BatchRecords;
	bool bSnapshotTaken = false;
	FAngelscriptTestCode* BoundCode = nullptr;
};

FAngelscriptTestCodeRegistry::FAngelscriptTestCodeRegistry()
	: Data(MakeUnique<FData>())
{
}

FAngelscriptTestCodeRegistry::~FAngelscriptTestCodeRegistry() = default;

void FAngelscriptTestCodeRegistry::Add(FRecord Record)
{
	FAngelscriptTestCode* BoundCode = nullptr;
	FAngelscriptTestError LateError;
	{
		FScopeLock Lock(&Data->Mutex);
		if (!Data->bSnapshotTaken)
		{
			Data->Records.Add(MoveTemp(Record));
			return;
		}

		BoundCode = Data->BoundCode;
		LateError = {
			.Code = TEXT("LateRegistration"),
			.Message = TEXT("Test code registrations are closed after the activation snapshot."),
			.OwnerModule = Record.OwnerModule,
			.FileTag = Record.FileMeta.Tag,
			.SourcePath = Record.SourceFile,
			.SourceLine = Record.SourceLine,
		};
	}

	if (BoundCode != nullptr)
	{
		BoundCode->RetainRegistrationError(MoveTemp(LateError));
	}
}

void FAngelscriptTestCodeRegistry::AddBatch(FBatchRecord Record)
{
	FAngelscriptTestCode* BoundCode = nullptr;
	FAngelscriptTestError LateError;
	{
		FScopeLock Lock(&Data->Mutex);
		if (!Data->bSnapshotTaken)
		{
			Data->BatchRecords.Add(MoveTemp(Record));
			return;
		}

		BoundCode = Data->BoundCode;
		LateError = {
			.Code = TEXT("LateRegistration"),
			.Message = TEXT("Test code registrations are closed after the activation snapshot."),
			.OwnerModule = Record.OwnerModule,
			.SourcePath = Record.SourceFile,
			.SourceLine = Record.SourceLine,
		};
	}

	if (BoundCode != nullptr)
	{
		BoundCode->RetainRegistrationError(MoveTemp(LateError));
	}
}

FAngelscriptTestCodeRegistry::FSnapshot FAngelscriptTestCodeRegistry::Snapshot(
	FAngelscriptTestCode& Code)
{
	FScopeLock Lock(&Data->Mutex);
	if (Data->bSnapshotTaken)
	{
		return {};
	}
	Data->bSnapshotTaken = true;
	Data->BoundCode = &Code;
	FSnapshot Result;
	Result.Records = Data->Records;
	Result.BatchRecords = Data->BatchRecords;
	return Result;
}

void FAngelscriptTestCodeRegistry::Unbind(const FAngelscriptTestCode& Code)
{
	FScopeLock Lock(&Data->Mutex);
	if (Data->BoundCode == &Code)
	{
		Data->BoundCode = nullptr;
	}
}

FAngelscriptTestCodeRegistry& FAngelscriptTestCodeRegistration::GetGlobalRegistry()
{
	static FAngelscriptTestCodeRegistry Registry;
	return Registry;
}

FAngelscriptTestCodeRegistration::FAngelscriptTestCodeRegistration(
	FAngelscriptTestFileMeta FileMeta,
	FCodeFactory Factory,
	const ANSICHAR* OwnerModule,
	const ANSICHAR* SourceFile,
	int32 SourceLine)
	: FAngelscriptTestCodeRegistration(
		GetGlobalRegistry(),
		MoveTemp(FileMeta),
		Factory,
		OwnerModule,
		SourceFile,
		SourceLine)
{
}

FAngelscriptTestCodeRegistration::FAngelscriptTestCodeRegistration(
	FAngelscriptTestCodeRegistry& Registry,
	FAngelscriptTestFileMeta FileMeta,
	FCodeFactory Factory,
	const ANSICHAR* OwnerModule,
	const ANSICHAR* SourceFile,
	int32 SourceLine)
{
	Registry.Add({
		.FileMeta = MoveTemp(FileMeta),
		.Factory = Factory,
		.OwnerModule = OwnerModule != nullptr ? UTF8_TO_TCHAR(OwnerModule) : TEXT(""),
		.SourceFile = SourceFile != nullptr ? UTF8_TO_TCHAR(SourceFile) : TEXT(""),
		.SourceLine = SourceLine,
	});
}

FAngelscriptTestCodeActivationBarrier::FAngelscriptTestCodeActivationBarrier(
	TFunction<void()> InActivation)
	: Activation(MoveTemp(InActivation))
{
}

FAngelscriptTestCodeActivationBarrier::~FAngelscriptTestCodeActivationBarrier()
{
	Shutdown();
}

void FAngelscriptTestCodeActivationBarrier::Install(
	bool bStartupLoadingComplete,
	FSimpleMulticastDelegate& InCompletionDelegate)
{
	Shutdown();
	if (bStartupLoadingComplete)
	{
		Activation();
		return;
	}

	CompletionDelegate = &InCompletionDelegate;
	CompletionHandle = CompletionDelegate->AddRaw(
		this, &FAngelscriptTestCodeActivationBarrier::HandleCompletion);
}

void FAngelscriptTestCodeActivationBarrier::Shutdown()
{
	if (CompletionDelegate != nullptr && CompletionHandle.IsValid())
	{
		CompletionDelegate->Remove(CompletionHandle);
	}
	CompletionDelegate = nullptr;
	CompletionHandle.Reset();
}

void FAngelscriptTestCodeActivationBarrier::HandleCompletion()
{
	Shutdown();
	Activation();
}
}
