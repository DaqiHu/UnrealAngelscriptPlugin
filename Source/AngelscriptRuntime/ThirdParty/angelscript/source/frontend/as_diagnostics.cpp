/*
   Maintained Unreal AngelScript fork — structured frontend diagnostics.
*/

#include "frontend/as_diagnostics.h"

#include "Containers/StringConv.h"
#include "Misc/ScopeLock.h"

BEGIN_AS_NAMESPACE

asSDiagnosticArgument asSDiagnosticArgument::FromText(FStringView Value)
	{
		asSDiagnosticArgument Result;
		Result.Kind = asEDiagnosticArgumentKind::Text;
		Result.TextValue = FString(Value);
		return Result;
	}

	asSDiagnosticArgument asSDiagnosticArgument::FromSigned(int64 Value)
	{
		asSDiagnosticArgument Result;
		Result.Kind = asEDiagnosticArgumentKind::Signed;
		Result.SignedValue = Value;
		return Result;
	}

	asSDiagnosticArgument asSDiagnosticArgument::FromUnsigned(uint64 Value)
	{
		asSDiagnosticArgument Result;
		Result.Kind = asEDiagnosticArgumentKind::Unsigned;
		Result.UnsignedValue = Value;
		return Result;
	}

	asSDiagnosticArgument asSDiagnosticArgument::FromReal(double Value)
	{
		asSDiagnosticArgument Result;
		Result.Kind = asEDiagnosticArgumentKind::Real;
		Result.RealValue = Value;
		return Result;
	}

	asSDiagnosticArgument asSDiagnosticArgument::FromBoolean(bool Value)
	{
		asSDiagnosticArgument Result;
		Result.Kind = asEDiagnosticArgumentKind::Boolean;
		Result.BooleanValue = Value;
		return Result;
	}

	FString asSDiagnosticArgument::ToDisplayString() const
	{
		switch (Kind)
		{
		case asEDiagnosticArgumentKind::Text:
			return TextValue;
		case asEDiagnosticArgumentKind::Signed:
			return FString::Printf(TEXT("%lld"), static_cast<long long>(SignedValue));
		case asEDiagnosticArgumentKind::Unsigned:
			return FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(UnsignedValue));
		case asEDiagnosticArgumentKind::Real:
			return FString::SanitizeFloat(RealValue);
		case asEDiagnosticArgumentKind::Boolean:
			return BooleanValue ? TEXT("true") : TEXT("false");
		default:
			return FString();
		}
	}

	asSDiagnosticFixIt asSDiagnosticFixIt::FromText(
		const asCSourceRange& InRange,
		FStringView ReplacementText)
	{
		asSDiagnosticFixIt Result;
		Result.Range = InRange;
		const FTCHARToUTF8 Converted(ReplacementText.GetData(), ReplacementText.Len());
		Result.ReplacementUtf8.Append(
			reinterpret_cast<const uint8*>(Converted.Get()),
			Converted.Length());
		return Result;
	}

	void asCCollectingDiagnosticConsumer::HandleDiagnostics(
		TConstArrayView<asSDiagnosticRecord> OrderedRecords)
	{
		Records.Reset();
		Records.Append(OrderedRecords.GetData(), OrderedRecords.Num());
	}

	asCDiagnosticFragment::asCDiagnosticFragment(
		TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> InSnapshot,
		FStringView InStableFragmentKey)
		: Snapshot(MoveTemp(InSnapshot))
		, StableFragmentKey(InStableFragmentKey)
	{
	}

	bool asCDiagnosticFragment::Report(asSDiagnosticRecord Record)
	{
		if (StableFragmentKey.IsEmpty() || Record.DiagnosticID == 0)
		{
			return false;
		}

		const asCSourceManager SourceManager(Snapshot);
		if (!SourceManager.IsValidRange(Record.PrimaryRange))
		{
			return false;
		}
		for (const asCSourceRange& RelatedRange : Record.RelatedRanges)
		{
			if (!SourceManager.IsValidRange(RelatedRange))
			{
				return false;
			}
		}
		for (const asSDiagnosticFixIt& FixIt : Record.FixIts)
		{
			if (!SourceManager.IsValidRange(FixIt.Range))
			{
				return false;
			}
		}

		FStringView LogicalSourceKey;
		if (!Snapshot->TryGetLogicalSourceKey(Record.PrimaryRange.Begin.FileID, LogicalSourceKey))
		{
			return false;
		}

		Record.StableSourceKey = FString(LogicalSourceKey);
		Record.StableFragmentKey = StableFragmentKey;
		Record.LocalSequence = NextLocalSequence++;
		Records.Add(MoveTemp(Record));
		return true;
	}

	asCDiagnosticsEngine::asCDiagnosticsEngine(const asCSourceManager& SourceManager)
		: Snapshot(SourceManager.GetSnapshot())
	{
	}

	asCDiagnosticFragment asCDiagnosticsEngine::CreateFragment(FStringView StableFragmentKey) const
	{
		return asCDiagnosticFragment(Snapshot, StableFragmentKey);
	}

	bool asCDiagnosticsEngine::Submit(asCDiagnosticFragment&& Fragment)
	{
		if (Fragment.GetSnapshotID() != Snapshot->GetSnapshotID())
		{
			return false;
		}

		FScopeLock Lock(&RecordsLock);
		for (asSDiagnosticRecord& Record : Fragment.Records)
		{
			Records.Add(MoveTemp(Record));
		}
		Fragment.Records.Reset();
		return true;
	}

	void asCDiagnosticsEngine::EmitTo(asIDiagnosticConsumer& Consumer) const
	{
		TArray<asSDiagnosticRecord> OrderedRecords;
		{
			FScopeLock Lock(&RecordsLock);
			OrderedRecords = Records;
		}

		OrderedRecords.Sort([](const asSDiagnosticRecord& Left, const asSDiagnosticRecord& Right)
		{
			const int32 SourceComparison = Left.StableSourceKey.Compare(
				Right.StableSourceKey,
				ESearchCase::CaseSensitive);
			if (SourceComparison != 0)
			{
				return SourceComparison < 0;
			}
			if (Left.PrimaryRange.Begin.ByteOffset != Right.PrimaryRange.Begin.ByteOffset)
			{
				return Left.PrimaryRange.Begin.ByteOffset < Right.PrimaryRange.Begin.ByteOffset;
			}
			if (Left.PrimaryRange.End.ByteOffset != Right.PrimaryRange.End.ByteOffset)
			{
				return Left.PrimaryRange.End.ByteOffset < Right.PrimaryRange.End.ByteOffset;
			}
			if (Left.Severity != Right.Severity)
			{
				return static_cast<uint8>(Left.Severity) < static_cast<uint8>(Right.Severity);
			}
			if (Left.DiagnosticID != Right.DiagnosticID)
			{
				return Left.DiagnosticID < Right.DiagnosticID;
			}
			const int32 FragmentComparison = Left.StableFragmentKey.Compare(
				Right.StableFragmentKey,
				ESearchCase::CaseSensitive);
			if (FragmentComparison != 0)
			{
				return FragmentComparison < 0;
			}
			return Left.LocalSequence < Right.LocalSequence;
		});
		Consumer.HandleDiagnostics(OrderedRecords);
	}

	asUINT asCDiagnosticsEngine::GetPendingCount() const
	{
		FScopeLock Lock(&RecordsLock);
		return static_cast<asUINT>(Records.Num());
	}

	FString asCDiagnosticRenderer::Render(
		FStringView Format,
		const asSDiagnosticRecord& Record)
	{
		FString Result(Format);
		for (int32 Index = 0; Index < Record.Arguments.Num(); ++Index)
		{
			const FString Placeholder = FString::Printf(TEXT("{%d}"), Index);
			const FString Value = Record.Arguments[Index].ToDisplayString();
			Result.ReplaceInline(*Placeholder, *Value, ESearchCase::CaseSensitive);
		}
		return Result;
	}

	FString asCDiagnosticRenderer::SerializeStable(
		TConstArrayView<asSDiagnosticRecord> Records)
	{
		FString Result;
		for (const asSDiagnosticRecord& Record : Records)
		{
			Result += FString::Printf(
				TEXT("%s|%u|%u|%u|%u|%s|%u"),
				*Record.StableSourceKey,
				Record.PrimaryRange.Begin.ByteOffset,
				Record.PrimaryRange.End.ByteOffset,
				static_cast<asUINT>(Record.Severity),
				Record.DiagnosticID,
				*Record.StableFragmentKey,
				Record.LocalSequence);
			for (const asSDiagnosticArgument& Argument : Record.Arguments)
			{
				Result += TEXT("|");
				Result += FString::Printf(TEXT("%u:"), static_cast<asUINT>(Argument.Kind));
				Result += Argument.ToDisplayString();
			}
			Result += TEXT("\n");
		}
		return Result;
	}

END_AS_NAMESPACE

