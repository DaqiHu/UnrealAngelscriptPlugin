/*
   Maintained Unreal AngelScript fork — revision-bound in-memory source edits.
*/

#include "frontend/Basic/as_source_edit.h"

#include "Containers/Map.h"
#include "Containers/Utf8String.h"
#include "unreal/AngelscriptSource.h"

BEGIN_AS_NAMESPACE

asCSourceSnapshot::asCSourceSnapshot(asCSourceManager InManager)
	: Manager(MoveTemp(InManager))
{
}

namespace
	{
		bool RangesOverlap(const asCSourceRange& Left, const asCSourceRange& Right)
		{
			return Left.Begin.FileID == Right.Begin.FileID
				&& Left.Begin.ByteOffset < Right.End.ByteOffset
				&& Right.Begin.ByteOffset < Left.End.ByteOffset;
		}

		bool SamePointConflict(const asSSourceEdit& Left, const asSSourceEdit& Right)
		{
			return Left.Range.Begin.FileID == Right.Range.Begin.FileID
				&& Left.Range.Begin.ByteOffset == Right.Range.End.ByteOffset
				&& Right.Range.Begin.ByteOffset == Left.Range.End.ByteOffset
				&& Left.Range.Length() == 0
				&& Right.Range.Length() == 0
				&& (Left.ReplacementUtf8 != Right.ReplacementUtf8);
		}
	}

bool asCSourceEditApplier::Apply(
	const asCDiagnosticResult& Owner,
	const asSDiagnosticFixAlternative& Fix,
	TSharedPtr<const asCSourceSnapshot, ESPMode::ThreadSafe>& OutSnapshot) const
{
	OutSnapshot.Reset();
	if (Fix.SourceEdits.IsEmpty())
	{
		return false;
	}

	const asCSourceManager& Sources = Owner.GetSources();
	if (!Sources.IsReady())
	{
		return false;
	}

	TArray<asSSourceEdit> Edits = Fix.SourceEdits;
	for (int32 Index = 0; Index < Edits.Num(); ++Index)
	{
		const asSSourceEdit& Edit = Edits[Index];
		if (Edit.Range.Begin.SnapshotID != Sources.GetSnapshotID()
			|| !Sources.IsValidRange(Edit.Range))
		{
			return false;
		}
		FStringView Key;
		uint64 Revision = 0;
		if (!Sources.TryGetLogicalSourceKey(Edit.Range.Begin.FileID, Key)
			|| Key != Edit.LogicalSourceKey
			|| !Sources.TryGetContentRevision(Edit.Range.Begin.FileID, Revision)
			|| Revision != Edit.ExpectedRevision)
		{
			return false;
		}
		TConstArrayView<uint8> Original;
		if (!Sources.TryGetBytes(Edit.Range, Original)
			|| Original.Num() != Edit.ExpectedOriginalUtf8.Num()
			|| FMemory::Memcmp(
				Original.GetData(),
				Edit.ExpectedOriginalUtf8.GetData(),
				Original.Num()) != 0)
		{
			return false;
		}
		for (int32 Other = Index + 1; Other < Edits.Num(); ++Other)
		{
			if (RangesOverlap(Edit.Range, Edits[Other].Range)
				|| SamePointConflict(Edit, Edits[Other]))
			{
				return false;
			}
		}
	}

	TMap<asUINT, TArray<asSSourceEdit>> ByFile;
	for (const asSSourceEdit& Edit : Edits)
	{
		ByFile.FindOrAdd(Edit.Range.Begin.FileID.Value).Add(Edit);
	}

	TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> NextSources;
	for (asUINT FileIndex = 1; FileIndex <= Sources.GetFileCount(); ++FileIndex)
	{
		const asCSourceFileID File(FileIndex);
		FStringView Key;
		TConstArrayView<uint8> Bytes;
		if (!Sources.TryGetLogicalSourceKey(File, Key) || !Sources.TryGetBytes(File, Bytes))
		{
			return false;
		}
		TArray<uint8> Written;
		Written.Append(Bytes.GetData(), Bytes.Num());
		if (TArray<asSSourceEdit>* FileEdits = ByFile.Find(File.Value))
		{
			FileEdits->Sort([](const asSSourceEdit& Left, const asSSourceEdit& Right)
			{
				return Left.Range.Begin.ByteOffset > Right.Range.Begin.ByteOffset;
			});
			for (const asSSourceEdit& Edit : *FileEdits)
			{
				Written.RemoveAt(
					static_cast<int32>(Edit.Range.Begin.ByteOffset),
					static_cast<int32>(Edit.Range.Length()));
				Written.Insert(
					Edit.ReplacementUtf8.GetData(),
					Edit.ReplacementUtf8.Num(),
					static_cast<int32>(Edit.Range.Begin.ByteOffset));
			}
		}
		FUtf8String Body;
		if (Written.Num() > 0)
		{
			Body.AppendChars(reinterpret_cast<const UTF8CHAR*>(Written.GetData()), Written.Num());
		}
		NextSources.Add(MakeShared<FAngelscriptSource, ESPMode::ThreadSafe>(
			FString(Key),
			MoveTemp(Body)));
	}

	asCSourceManager Next(NextSources);
	if (!Next.IsReady())
	{
		return false;
	}
	OutSnapshot = MakeShared<asCSourceSnapshot, ESPMode::ThreadSafe>(MoveTemp(Next));
	return true;
}

END_AS_NAMESPACE
