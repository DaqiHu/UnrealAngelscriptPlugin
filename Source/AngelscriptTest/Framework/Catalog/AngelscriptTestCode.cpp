#include "Framework/Catalog/AngelscriptTestCode.h"
#include "Framework/Catalog/AngelscriptTestCodeRegistration.h"

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Templates/UnrealTemplate.h"

namespace AngelscriptTest
{
namespace
{
FAngelscriptTestError MakeCodeError(
	FStringView Code,
	FStringView Message,
	FStringView FileTag = FStringView(),
	FStringView VersionTag = FStringView(),
	FStringView OwnerModule = FStringView())
{
	return {
		.Code = FString(Code),
		.Message = FString(Message),
		.OwnerModule = FString(OwnerModule),
		.FileTag = FString(FileTag),
		.VersionTag = FString(VersionTag),
	};
}

bool HasAllTopics(TConstArrayView<FString> Topics, TConstArrayView<FString> RequiredTopics)
{
	for (const FString& RequiredTopic : RequiredTopics)
	{
		if (!Topics.ContainsByPredicate([&RequiredTopic](const FString& Topic)
		{
			return Topic.Equals(RequiredTopic, ESearchCase::CaseSensitive);
		}))
		{
			return false;
		}
	}
	return true;
}
}

struct FAngelscriptTestCode::FData
{
	struct FFileEntry
	{
		TMap<FString, FAngelscriptTestSourceCase> CasesByVersion;
	};

	bool bActivated = false;
	bool bActivationInProgress = false;
	TArray<FAngelscriptTestError> RegistrationErrors;
	TMap<FString, FFileEntry> FilesByTag;
	FAngelscriptTestCodeRegistry* ActiveRegistry = nullptr;
};

FAngelscriptTestCode::FAngelscriptTestCode()
	: Data(MakeUnique<FData>())
{
}

FAngelscriptTestCode::~FAngelscriptTestCode()
{
	if (Data->ActiveRegistry != nullptr)
	{
		Data->ActiveRegistry->Unbind(*this);
	}
}

FAngelscriptTestCode& FAngelscriptTestCode::GetInstance()
{
	static FAngelscriptTestCode Instance;
	return Instance;
}

FAngelscriptTestStatus FAngelscriptTestCode::ActivateRegistrations()
{
	return ActivateRegistrations(FAngelscriptTestCodeRegistration::GetGlobalRegistry());
}

FAngelscriptTestStatus FAngelscriptTestCode::ActivateRegistrations(
	FAngelscriptTestCodeRegistry& Registry)
{
	if (Data->bActivated)
	{
		return Data->RegistrationErrors.IsEmpty()
			? FAngelscriptTestStatus::Success()
			: FAngelscriptTestStatus::Failure(Data->RegistrationErrors);
	}
	if (Data->bActivationInProgress)
	{
		return FAngelscriptTestStatus::Failure(MakeCodeError(
			TEXT("ReentrantActivation"),
			TEXT("Test code activation cannot be entered from a registration factory.")));
	}

	TGuardValue<bool> ActivationGuard(Data->bActivationInProgress, true);
	Data->ActiveRegistry = &Registry;
	FAngelscriptTestCodeRegistry::FSnapshot Snapshot = Registry.Snapshot(*this);
	TArray<FAdmissionBatch> Batches;
	Batches.Reserve(Snapshot.Records.Num() + Snapshot.BatchRecords.Num());
	for (FAngelscriptTestCodeRegistry::FRecord& Record : Snapshot.Records)
	{
		FAdmissionBatch& Batch = Batches.AddDefaulted_GetRef();
		Batch.OwnerModule = Record.OwnerModule;
		Batch.SourceFile = Record.SourceFile;
		Batch.SourceLine = Record.SourceLine;
		if (Record.Factory == nullptr)
		{
			Batch.Errors.Add(MakeCodeError(
				TEXT("NullRegistrationFactory"),
				TEXT("A test code registration requires a factory."),
				Record.FileMeta.Tag,
				FStringView(),
				Record.OwnerModule));
			Batch.Errors.Last().SourcePath = Record.SourceFile;
			Batch.Errors.Last().SourceLine = Record.SourceLine;
			continue;
		}
		Batch.Files.Add(Record.Factory(Record.FileMeta));
	}
	for (FAngelscriptTestCodeRegistry::FBatchRecord& Record : Snapshot.BatchRecords)
	{
		FAdmissionBatch& Batch = Batches.AddDefaulted_GetRef();
		Batch.OwnerModule = Record.OwnerModule;
		Batch.SourceFile = Record.SourceFile;
		Batch.SourceLine = Record.SourceLine;
		if (Record.Factory == nullptr)
		{
			Batch.Errors.Add(MakeCodeError(
				TEXT("NullRegistrationFactory"),
				TEXT("A test code registration requires a factory."),
				FStringView(),
				FStringView(),
				Record.OwnerModule));
			Batch.Errors.Last().SourcePath = Record.SourceFile;
			Batch.Errors.Last().SourceLine = Record.SourceLine;
			continue;
		}
		Record.Factory(Batch.Files, Batch.Errors);
	}
	return AdmitDetachedBatches(MoveTemp(Batches));
}

void FAngelscriptTestCode::RetainRegistrationError(FAngelscriptTestError Error)
{
	Data->RegistrationErrors.Add(MoveTemp(Error));
}

FAngelscriptTestStatus FAngelscriptTestCode::AdmitDetachedBatches(TArray<FAdmissionBatch> Batches)
{
	if (Data->bActivated)
	{
		return FAngelscriptTestStatus::Failure(MakeCodeError(
			TEXT("AlreadyActivated"),
			TEXT("This code database has already completed admission.")));
	}

	Data->bActivated = true;
	TArray<bool> bRejected;
	bRejected.Init(false, Batches.Num());
	TArray<TSet<FString>> Claims;
	Claims.SetNum(Batches.Num());
	auto MakeAdmissionError = [](const FAdmissionBatch& Batch, FStringView Code, FStringView Message, FStringView FileTag)
	{
		FAngelscriptTestError Error = MakeCodeError(
			Code, Message, FileTag, FStringView(), Batch.OwnerModule);
		Error.SourcePath = Batch.SourceFile;
		Error.SourceLine = Batch.SourceLine;
		return Error;
	};

	for (int32 BatchIndex = 0; BatchIndex < Batches.Num(); ++BatchIndex)
	{
		FAdmissionBatch& Batch = Batches[BatchIndex];
		for (FAngelscriptTestError& Error : Batch.Errors)
		{
			if (Error.OwnerModule.IsEmpty())
			{
				Error.OwnerModule = Batch.OwnerModule;
			}
			if (Error.SourcePath.IsEmpty())
			{
				Error.SourcePath = Batch.SourceFile;
				Error.SourceLine = Batch.SourceLine;
			}
		}
		bRejected[BatchIndex] = !Batch.Errors.IsEmpty();

		for (FAngelscriptTestCodeBuildResult& File : Batch.Files)
		{
			if (!File.IsSuccess())
			{
				bRejected[BatchIndex] = true;
				for (FAngelscriptTestError Error : File.GetErrors())
				{
					if (Error.OwnerModule.IsEmpty())
					{
						Error.OwnerModule = Batch.OwnerModule;
					}
					if (Error.SourcePath.IsEmpty())
					{
						Error.SourcePath = Batch.SourceFile;
						Error.SourceLine = Batch.SourceLine;
					}
					Batch.Errors.Add(MoveTemp(Error));
				}
				continue;
			}

			const TArray<FAngelscriptTestSourceCase>& Cases = File.GetCasesForAdmission();
			if (Cases.IsEmpty())
			{
				bRejected[BatchIndex] = true;
				Batch.Errors.Add(MakeAdmissionError(
					Batch,
					TEXT("EmptyAdmissionFile"),
					TEXT("A successful built file must contain at least one version."),
					FStringView()));
				continue;
			}

			const FString FileTag = Cases[0].GetFileMeta().Tag;
			if (Claims[BatchIndex].Contains(FileTag))
			{
				bRejected[BatchIndex] = true;
				Batch.Errors.Add(MakeAdmissionError(
					Batch,
					TEXT("DuplicateFileTagInBatch"),
					TEXT("A batch may provide each file Tag only once."),
					FileTag));
			}
			Claims[BatchIndex].Add(FileTag);

			if (Cases.ContainsByPredicate([&FileTag](const FAngelscriptTestSourceCase& SourceCase)
			{
				return SourceCase.GetFileMeta().Tag != FileTag;
			}))
			{
				bRejected[BatchIndex] = true;
				Batch.Errors.Add(MakeAdmissionError(
					Batch,
					TEXT("MixedFileBuildResult"),
					TEXT("One built file payload may contain only one file Tag."),
					FileTag));
			}
		}
	}

	TMap<FString, TArray<int32>> ClaimersByFile;
	for (int32 BatchIndex = 0; BatchIndex < Batches.Num(); ++BatchIndex)
	{
		if (bRejected[BatchIndex])
		{
			continue;
		}
		for (const FString& FileTag : Claims[BatchIndex])
		{
			ClaimersByFile.FindOrAdd(FileTag).Add(BatchIndex);
		}
	}

	for (const TPair<FString, TArray<int32>>& Claim : ClaimersByFile)
	{
		if (Claim.Value.Num() > 1)
		{
			for (const int32 BatchIndex : Claim.Value)
			{
				bRejected[BatchIndex] = true;
				Batches[BatchIndex].Errors.Add(MakeAdmissionError(
					Batches[BatchIndex],
					TEXT("FileTagConflict"),
					TEXT("Multiple admission batches claim the same file Tag."),
					Claim.Key));
			}
		}
		else if (Data->FilesByTag.Contains(Claim.Key))
		{
			const int32 BatchIndex = Claim.Value[0];
			bRejected[BatchIndex] = true;
			Batches[BatchIndex].Errors.Add(MakeAdmissionError(
				Batches[BatchIndex],
				TEXT("FileTagConflict"),
				TEXT("The file Tag is already present in this database."),
				Claim.Key));
		}
	}

	for (int32 BatchIndex = 0; BatchIndex < Batches.Num(); ++BatchIndex)
	{
		FAdmissionBatch& Batch = Batches[BatchIndex];
		if (bRejected[BatchIndex])
		{
			Data->RegistrationErrors.Append(MoveTemp(Batch.Errors));
			continue;
		}

		for (FAngelscriptTestCodeBuildResult& File : Batch.Files)
		{
			TArray<FAngelscriptTestSourceCase> Cases = File.TakeCasesForAdmission();
			const FString FileTag = Cases[0].GetFileMeta().Tag;
			FData::FFileEntry Entry;
			for (FAngelscriptTestSourceCase& SourceCase : Cases)
			{
				const FString VersionTag = SourceCase.GetVersionMeta().Tag;
				Entry.CasesByVersion.Add(VersionTag, MoveTemp(SourceCase));
			}
			Data->FilesByTag.Add(FileTag, MoveTemp(Entry));
		}
	}

	return Data->RegistrationErrors.IsEmpty()
		? FAngelscriptTestStatus::Success()
		: FAngelscriptTestStatus::Failure(Data->RegistrationErrors);
}

FAngelscriptTestSourceResult FAngelscriptTestCode::Get(
	FStringView FileTag,
	FStringView VersionTag) const
{
	if (!Data->bActivated)
	{
		return FAngelscriptTestSourceResult(MakeCodeError(
			TEXT("NotActivated"),
			TEXT("The code database has not completed activation."),
			FileTag,
			VersionTag));
	}

	const FData::FFileEntry* File = Data->FilesByTag.Find(FString(FileTag));
	if (File == nullptr)
	{
		return FAngelscriptTestSourceResult(MakeCodeError(
			TEXT("UnknownFileTag"),
			TEXT("The requested file Tag is not present in the code database."),
			FileTag,
			VersionTag));
	}
	const FAngelscriptTestSourceCase* SourceCase = File->CasesByVersion.Find(FString(VersionTag));
	if (SourceCase == nullptr)
	{
		return FAngelscriptTestSourceResult(MakeCodeError(
			TEXT("UnknownVersionTag"),
			TEXT("The requested version Tag is not present in the file."),
			FileTag,
			VersionTag));
	}
	return FAngelscriptTestSourceResult(*SourceCase);
}

FAngelscriptTestStatus FAngelscriptTestCode::FindCases(
	FStringView FileTag,
	TConstArrayView<FString> RequiredVersionTopics,
	TArray<FAngelscriptTestSourceCase>& OutCases) const
{
	if (!Data->bActivated)
	{
		return FAngelscriptTestStatus::Failure(MakeCodeError(
			TEXT("NotActivated"),
			TEXT("The code database has not completed activation."),
			FileTag));
	}
	const FData::FFileEntry* File = Data->FilesByTag.Find(FString(FileTag));
	if (File == nullptr)
	{
		return FAngelscriptTestStatus::Failure(MakeCodeError(
			TEXT("UnknownFileTag"),
			TEXT("The requested file Tag is not present in the code database."),
			FileTag));
	}

	TArray<FAngelscriptTestSourceCase> Matches;
	for (const TPair<FString, FAngelscriptTestSourceCase>& Entry : File->CasesByVersion)
	{
		if (HasAllTopics(Entry.Value.GetVersionMeta().Topics, RequiredVersionTopics))
		{
			Matches.Add(Entry.Value);
		}
	}
	Matches.Sort([](const FAngelscriptTestSourceCase& Left, const FAngelscriptTestSourceCase& Right)
	{
		return Left.GetVersionMeta().Tag.Compare(
			Right.GetVersionMeta().Tag,
			ESearchCase::CaseSensitive) < 0;
	});
	OutCases = MoveTemp(Matches);
	return FAngelscriptTestStatus::Success();
}

FAngelscriptTestStatus FAngelscriptTestCode::FindFiles(
	TConstArrayView<FString> RequiredFileTopics,
	TArray<FAngelscriptTestFileMeta>& OutFiles) const
{
	if (!Data->bActivated)
	{
		return FAngelscriptTestStatus::Failure(MakeCodeError(
			TEXT("NotActivated"),
			TEXT("The code database has not completed activation.")));
	}
	if (!Data->RegistrationErrors.IsEmpty())
	{
		return FAngelscriptTestStatus::Failure(Data->RegistrationErrors);
	}

	TArray<FAngelscriptTestFileMeta> Matches;
	for (const TPair<FString, FData::FFileEntry>& Entry : Data->FilesByTag)
	{
		if (Entry.Value.CasesByVersion.IsEmpty())
		{
			continue;
		}
		const FAngelscriptTestFileMeta& FileMeta =
			Entry.Value.CasesByVersion.CreateConstIterator().Value().GetFileMeta();
		if (HasAllTopics(FileMeta.Topics, RequiredFileTopics))
		{
			Matches.Add(FileMeta);
		}
	}
	Matches.Sort([](const FAngelscriptTestFileMeta& Left, const FAngelscriptTestFileMeta& Right)
	{
		return Left.Tag.Compare(Right.Tag, ESearchCase::CaseSensitive) < 0;
	});
	OutFiles = MoveTemp(Matches);
	return FAngelscriptTestStatus::Success();
}

bool FAngelscriptTestCode::IsActivated() const
{
	return Data->bActivated;
}

TArray<FAngelscriptTestError> FAngelscriptTestCode::GetRegistrationErrors() const
{
	return Data->RegistrationErrors;
}
}
