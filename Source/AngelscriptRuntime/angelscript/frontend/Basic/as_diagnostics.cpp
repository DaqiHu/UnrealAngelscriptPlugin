/*
   Maintained Unreal AngelScript fork — structured frontend diagnostics.
*/

#include "frontend/Basic/as_diagnostics.h"

#include "Containers/StringConv.h"
#include "Misc/ScopeLock.h"

BEGIN_AS_NAMESPACE

namespace
	{
		const asSDiagnosticCatalogDescriptor Catalog[] = {
#define AS_DIAGNOSTIC(Name, ID, InStage, InCategory, InSeverity, InWarningGroup, InFormat) \
	{ ID, TEXT(#Name), asEDiagnosticStage::InStage, asEDiagnosticCategory::InCategory, \
		asEDiagnosticSeverity::InSeverity, asEDiagnosticWarningGroup::InWarningGroup, TEXT(InFormat) },
#include "frontend/Basic/as_diagnostic_catalog.def"
#undef AS_DIAGNOSTIC
		};

		bool IsRangeOwned(const asCSourceManager& Sources, const asCSourceRange& Range)
		{
			return Sources.IsValidRange(Range);
		}

		bool PrepareRecord(asCSourceManager& Sources, FStringView FragmentKey, asUINT& NextLocalSequence, asSDiagnosticRecord& Record)
		{
			if (Record.DiagnosticID == 0)
			{
				return false;
			}
			if (Record.bHasLocation)
			{
				if (!IsRangeOwned(Sources, Record.PrimaryRange))
				{
					return false;
				}
				for (const asCSourceRange& RelatedRange : Record.RelatedRanges)
				{
					if (!IsRangeOwned(Sources, RelatedRange))
					{
						return false;
					}
				}
				for (const asSDiagnosticFixIt& FixIt : Record.FixIts)
				{
					if (!IsRangeOwned(Sources, FixIt.Range))
					{
						return false;
					}
				}
				FStringView LogicalSourceKey;
				if (!Sources.TryGetLogicalSourceKey(Record.PrimaryRange.Begin.FileID, LogicalSourceKey))
				{
					return false;
				}
				Record.StableSourceKey = FString(LogicalSourceKey);
			}
			else if (Record.PrimaryRange.Begin.IsValid() || Record.PrimaryRange.End.IsValid())
			{
				if (!IsRangeOwned(Sources, Record.PrimaryRange))
				{
					return false;
				}
			}
			Record.StableFragmentKey = FString(FragmentKey);
			Record.LocalSequence = NextLocalSequence++;
			return true;
		}

		asEDiagnosticSeverity CatalogSeverity(asUINT ID, asEDiagnosticSeverity Fallback)
		{
			if (const asSDiagnosticCatalogDescriptor* Descriptor = asFindDiagnosticCatalog(ID))
			{
				return Descriptor->DefaultSeverity;
			}
			return Fallback;
		}

		asEDiagnosticWarningGroup CatalogWarningGroup(asUINT ID)
		{
			if (const asSDiagnosticCatalogDescriptor* Descriptor = asFindDiagnosticCatalog(ID))
			{
				return Descriptor->WarningGroup;
			}
			return asEDiagnosticWarningGroup::None;
		}

		bool IsWarningEnabled(const asSDiagnosticOptions& Options, asUINT ID, asEDiagnosticWarningGroup Group)
		{
			if (const bool* PerId = Options.PerDiagnosticEnabled.Find(ID))
			{
				return *PerId;
			}
			if (Group != asEDiagnosticWarningGroup::None)
			{
				if (const bool* PerGroup = Options.PerWarningGroupEnabled.Find(Group))
				{
					return *PerGroup;
				}
			}
			return Options.bEnableWarnings;
		}

		int32 CompareGroups(const asSDiagnosticGroup& Left, const asSDiagnosticGroup& Right)
		{
			const bool bLeftLocated = Left.Primary.bHasLocation;
			const bool bRightLocated = Right.Primary.bHasLocation;
			if (bLeftLocated != bRightLocated)
			{
				return bLeftLocated ? -1 : 1;
			}
			if (bLeftLocated)
			{
				const int32 SourceComparison = Left.Primary.StableSourceKey.Compare(
					Right.Primary.StableSourceKey,
					ESearchCase::CaseSensitive);
				if (SourceComparison != 0)
				{
					return SourceComparison;
				}
				if (Left.Primary.PrimaryRange.Begin.ByteOffset != Right.Primary.PrimaryRange.Begin.ByteOffset)
				{
					return Left.Primary.PrimaryRange.Begin.ByteOffset < Right.Primary.PrimaryRange.Begin.ByteOffset ? -1 : 1;
				}
				if (Left.Primary.PrimaryRange.End.ByteOffset != Right.Primary.PrimaryRange.End.ByteOffset)
				{
					return Left.Primary.PrimaryRange.End.ByteOffset < Right.Primary.PrimaryRange.End.ByteOffset ? -1 : 1;
				}
			}
			else
			{
				if (Left.Primary.DiagnosticID != Right.Primary.DiagnosticID)
				{
					return Left.Primary.DiagnosticID < Right.Primary.DiagnosticID ? -1 : 1;
				}
			}
			if (Left.Primary.Severity != Right.Primary.Severity)
			{
				return static_cast<uint8>(Left.Primary.Severity) < static_cast<uint8>(Right.Primary.Severity) ? -1 : 1;
			}
			if (Left.Primary.DiagnosticID != Right.Primary.DiagnosticID)
			{
				return Left.Primary.DiagnosticID < Right.Primary.DiagnosticID ? -1 : 1;
			}
			const int32 FragmentComparison = Left.Primary.StableFragmentKey.Compare(
				Right.Primary.StableFragmentKey,
				ESearchCase::CaseSensitive);
			if (FragmentComparison != 0)
			{
				return FragmentComparison;
			}
			if (Left.Primary.LocalSequence != Right.Primary.LocalSequence)
			{
				return Left.Primary.LocalSequence < Right.Primary.LocalSequence ? -1 : 1;
			}
			return 0;
		}
	}

	const asSDiagnosticCatalogDescriptor* asFindDiagnosticCatalog(asUINT ID)
	{
		for (const asSDiagnosticCatalogDescriptor& Entry : Catalog)
		{
			if (Entry.ID == ID)
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	asUINT asDiagnosticIdFromSyntaxReason(FStringView Reason)
	{
#define AS_SYNTAX_DIAGNOSTIC(Name, ID, Key, Format) \
		if (Reason.Equals(TEXT(Key), ESearchCase::CaseSensitive)) { return ID; }
#include "frontend/Basic/as_diagnostic_syntax.def"
#undef AS_SYNTAX_DIAGNOSTIC
		return 0;
	}

	asUINT asDiagnosticIdFromSemanticReason(FStringView Reason)
	{
		if (Reason.StartsWith(TEXT("unresolved-reference")))
		{
			return static_cast<asUINT>(asEDiagnosticCatalogID::UnresolvedReference);
		}
		if (Reason.StartsWith(TEXT("unresolved-type"))
			|| Reason.StartsWith(TEXT("unresolved-return"))
			|| Reason.StartsWith(TEXT("unresolved-callable-return"))
			|| Reason.StartsWith(TEXT("unresolved-base")))
		{
			return static_cast<asUINT>(asEDiagnosticCatalogID::UnknownType);
		}
		if (Reason.StartsWith(TEXT("duplicate:")))
		{
			return static_cast<asUINT>(asEDiagnosticCatalogID::DuplicateDeclaration);
		}
		if (Reason.StartsWith(TEXT("generic-arity")))
		{
			return static_cast<asUINT>(asEDiagnosticCatalogID::GenericArity);
		}
		if (Reason.StartsWith(TEXT("invalid-qualifier")))
		{
			return static_cast<asUINT>(asEDiagnosticCatalogID::InvalidQualifier);
		}
		if (Reason.Equals(TEXT("implicit-case-fallthrough"), ESearchCase::CaseSensitive))
		{
			return static_cast<asUINT>(asEDiagnosticCatalogID::ImplicitCaseFallthrough);
		}
		if (Reason.Equals(TEXT("deprecated-function-call"), ESearchCase::CaseSensitive))
		{
			return static_cast<asUINT>(asEDiagnosticCatalogID::DeprecatedFunctionCall);
		}
		if (Reason.Equals(TEXT("nodiscard-call-result-unused"), ESearchCase::CaseSensitive))
		{
			return static_cast<asUINT>(asEDiagnosticCatalogID::NodiscardCallResultUnused);
		}
#define AS_SEMANTIC_DIAGNOSTIC(Name, ID, Key, Format) \
		if (Reason.Equals(TEXT(Key), ESearchCase::CaseSensitive) \
			|| Reason.StartsWith(TEXT(Key) TEXT(":"))) \
		{ \
			return ID; \
		}
#include "frontend/Basic/as_diagnostic_semantic.def"
#undef AS_SEMANTIC_DIAGNOSTIC
		return 0;
	}

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

	asCDiagnostic::asCDiagnostic(
		TSharedPtr<FTarget, ESPMode::ThreadSafe> InTarget,
		asSDiagnosticRecord InPending)
		: Target(MoveTemp(InTarget))
		, Pending(MoveTemp(InPending))
		, bCommit(Target.IsValid() && Target->Fragment != nullptr)
	{
	}

	asCDiagnostic::asCDiagnostic(asCDiagnostic&& Other) noexcept
		: Target(MoveTemp(Other.Target))
		, Pending(MoveTemp(Other.Pending))
		, Notes(MoveTemp(Other.Notes))
		, Fixes(MoveTemp(Other.Fixes))
		, bCommit(Other.bCommit)
	{
		Other.bCommit = false;
	}

	asCDiagnostic::~asCDiagnostic()
	{
		if (!bCommit || !Target.IsValid() || Target->Fragment == nullptr)
		{
			return;
		}
		asSDiagnosticGroup Group;
		Group.Primary = MoveTemp(Pending);
		Group.Notes = MoveTemp(Notes);
		Group.Fixes = MoveTemp(Fixes);
		Target->Fragment->ReportGroup(MoveTemp(Group));
	}

	asCDiagnostic& asCDiagnostic::operator<<(FStringView Value)
	{
		if (bCommit)
		{
			Pending.Arguments.Add(asSDiagnosticArgument::FromText(Value));
		}
		return *this;
	}

	asCDiagnostic& asCDiagnostic::operator<<(int64 Value)
	{
		if (bCommit)
		{
			Pending.Arguments.Add(asSDiagnosticArgument::FromSigned(Value));
		}
		return *this;
	}

	asCDiagnostic& asCDiagnostic::operator<<(bool Value)
	{
		if (bCommit)
		{
			Pending.Arguments.Add(asSDiagnosticArgument::FromBoolean(Value));
		}
		return *this;
	}

	asCDiagnostic& asCDiagnostic::operator<<(const asSDiagnosticArgument& Value)
	{
		if (bCommit)
		{
			Pending.Arguments.Add(Value);
		}
		return *this;
	}

	asCDiagnostic& asCDiagnostic::operator<<(const asSDiagnosticFixIt& Value)
	{
		if (bCommit)
		{
			Pending.FixIts.Add(Value);
		}
		return *this;
	}

	asCDiagnostic& asCDiagnostic::operator<<(const asCSourceRange& RelatedRange)
	{
		if (bCommit)
		{
			Pending.RelatedRanges.Add(RelatedRange);
		}
		return *this;
	}

	asCDiagnostic& asCDiagnostic::operator<<(const asSDiagnosticRecord& Note)
	{
		if (bCommit)
		{
			Notes.Add(Note);
		}
		return *this;
	}

	asCDiagnostic& asCDiagnostic::operator<<(const asSDiagnosticFixAlternative& Fix)
	{
		if (bCommit)
		{
			Fixes.Add(Fix);
		}
		return *this;
	}

	asCDiagnosticFragment::asCDiagnosticFragment(
		asCSourceManager InSources,
		FStringView InStableFragmentKey)
		: Sources(MoveTemp(InSources))
		, StableFragmentKey(InStableFragmentKey)
		, Target(MakeShared<asCDiagnostic::FTarget, ESPMode::ThreadSafe>())
	{
		Target->Fragment = this;
	}

	asCDiagnosticFragment::asCDiagnosticFragment(asCDiagnosticFragment&& Other) noexcept
		: Sources(MoveTemp(Other.Sources))
		, StableFragmentKey(MoveTemp(Other.StableFragmentKey))
		, NextLocalSequence(Other.NextLocalSequence)
		, Groups(MoveTemp(Other.Groups))
		, Target(MoveTemp(Other.Target))
	{
		if (Target.IsValid())
		{
			Target->Fragment = this;
		}
		Other.NextLocalSequence = 0;
		Other.DetachTarget();
	}

	asCDiagnosticFragment& asCDiagnosticFragment::operator=(asCDiagnosticFragment&& Other) noexcept
	{
		if (this != &Other)
		{
			DetachTarget();
			Sources = MoveTemp(Other.Sources);
			StableFragmentKey = MoveTemp(Other.StableFragmentKey);
			NextLocalSequence = Other.NextLocalSequence;
			Groups = MoveTemp(Other.Groups);
			Target = MoveTemp(Other.Target);
			if (Target.IsValid())
			{
				Target->Fragment = this;
			}
			Other.NextLocalSequence = 0;
			Other.DetachTarget();
		}
		return *this;
	}

	asCDiagnosticFragment::~asCDiagnosticFragment()
	{
		DetachTarget();
	}

	void asCDiagnosticFragment::DetachTarget()
	{
		if (Target.IsValid())
		{
			Target->Fragment = nullptr;
		}
	}

	asCDiagnostic asCDiagnosticFragment::Diag(const asCSourceRange& Range, asUINT ID)
	{
		if (!Target.IsValid() || Target->Fragment == nullptr || StableFragmentKey.IsEmpty() || ID == 0)
		{
			return asCDiagnostic();
		}

		asSDiagnosticRecord Record;
		Record.DiagnosticID = ID;
		Record.Severity = CatalogSeverity(ID, asEDiagnosticSeverity::Error);
		Record.PrimaryRange = Range;
		Record.bHasLocation = true;
		return asCDiagnostic(Target, MoveTemp(Record));
	}

	asCDiagnostic asCDiagnosticFragment::DiagUnlocated(asUINT ID)
	{
		if (!Target.IsValid() || Target->Fragment == nullptr || StableFragmentKey.IsEmpty() || ID == 0)
		{
			return asCDiagnostic();
		}

		asSDiagnosticRecord Record;
		Record.DiagnosticID = ID;
		Record.Severity = CatalogSeverity(ID, asEDiagnosticSeverity::Error);
		Record.bHasLocation = false;
		return asCDiagnostic(Target, MoveTemp(Record));
	}

	bool asCDiagnosticFragment::Report(asSDiagnosticRecord Record)
	{
		asSDiagnosticGroup Group;
		Group.Primary = MoveTemp(Record);
		return ReportGroup(MoveTemp(Group));
	}

	bool asCDiagnosticFragment::ReportGroup(asSDiagnosticGroup Group)
	{
		if (StableFragmentKey.IsEmpty())
		{
			return false;
		}
		if (!PrepareRecord(Sources, StableFragmentKey, NextLocalSequence, Group.Primary))
		{
			return false;
		}
		for (asSDiagnosticRecord& Note : Group.Notes)
		{
			Note.Severity = asEDiagnosticSeverity::Note;
			if (!PrepareRecord(Sources, StableFragmentKey, NextLocalSequence, Note))
			{
				return false;
			}
		}
		for (asSDiagnosticFixAlternative& Fix : Group.Fixes)
		{
			for (const asSDiagnosticFixIt& Edit : Fix.Edits)
			{
				if (Group.Primary.bHasLocation && !IsRangeOwned(Sources, Edit.Range))
				{
					return false;
				}
			}
		}
		Groups.Add(MoveTemp(Group));
		return true;
	}

	asCDiagnosticsEngine::asCDiagnosticsEngine(
		const asCSourceManager& SourceManager,
		asSDiagnosticOptions InOptions)
		: Sources(SourceManager)
		, Options(MoveTemp(InOptions))
	{
	}

	asCDiagnosticFragment asCDiagnosticsEngine::CreateFragment(FStringView StableFragmentKey) const
	{
		return asCDiagnosticFragment(Sources, StableFragmentKey);
	}

	bool asCDiagnosticsEngine::Submit(asCDiagnosticFragment&& Fragment)
	{
		if (Fragment.GetSnapshotID() != Sources.GetSnapshotID())
		{
			return false;
		}

		FScopeLock Lock(&RecordsLock);
		for (asSDiagnosticGroup& Group : Fragment.Groups)
		{
			Groups.Add(MoveTemp(Group));
		}
		Fragment.Groups.Reset();
		Fragment.DetachTarget();
		return true;
	}

	void asCDiagnosticsEngine::EmitTo(asIDiagnosticConsumer& Consumer) const
	{
		const TSharedRef<const asCDiagnosticResult, ESPMode::ThreadSafe> Result = CaptureResult();
		TArray<asSDiagnosticRecord> OrderedRecords;
		for (const asSDiagnosticGroup& Group : Result->GetVisibleGroups())
		{
			OrderedRecords.Add(Group.Primary);
		}
		Consumer.HandleDiagnostics(OrderedRecords);
	}

	asUINT asCDiagnosticsEngine::GetPendingCount() const
	{
		FScopeLock Lock(&RecordsLock);
		return static_cast<asUINT>(Groups.Num());
	}

	bool asCDiagnosticsEngine::HasLanguageFailure() const
	{
		return CaptureResult()->HasLanguageFailure();
	}

	TSharedRef<const asCDiagnosticResult, ESPMode::ThreadSafe> asCDiagnosticsEngine::CaptureResult() const
	{
		TArray<asSDiagnosticGroup> Snapshot;
		{
			FScopeLock Lock(&RecordsLock);
			Snapshot = Groups;
		}
		Snapshot.Sort([](const asSDiagnosticGroup& Left, const asSDiagnosticGroup& Right)
		{
			return CompareGroups(Left, Right) < 0;
		});

		TSharedRef<asCDiagnosticResult, ESPMode::ThreadSafe> Result =
			MakeShared<asCDiagnosticResult, ESPMode::ThreadSafe>();
		Result->Sources = Sources;
		Result->Options = Options;

		int32 RetainedErrors = 0;
		for (asSDiagnosticGroup& Group : Snapshot)
		{
			const asEDiagnosticSeverity DefaultSeverity = CatalogSeverity(
				Group.Primary.DiagnosticID,
				Group.Primary.Severity);
			const asEDiagnosticWarningGroup WarningGroup = CatalogWarningGroup(Group.Primary.DiagnosticID);
			const bool bIsWarning = DefaultSeverity == asEDiagnosticSeverity::Warning
				|| Group.Primary.Severity == asEDiagnosticSeverity::Warning;
			const bool bEnabledWarning = bIsWarning && IsWarningEnabled(Options, Group.Primary.DiagnosticID, WarningGroup);
			const bool bLanguageError = !bIsWarning && Group.Primary.Severity == asEDiagnosticSeverity::Error;

			if (bIsWarning && !bEnabledWarning)
			{
				continue;
			}
			if (bIsWarning && Options.bWarningsAsErrors)
			{
				Result->bWarningPolicyFailure = true;
				Group.Primary.Severity = asEDiagnosticSeverity::Error;
			}
			if (bLanguageError)
			{
				Result->bLanguageFailure = true;
			}

			const bool bEffectiveError = Group.Primary.Severity == asEDiagnosticSeverity::Error;
			if (bEffectiveError && Options.ErrorReportLimit > 0 && RetainedErrors >= Options.ErrorReportLimit)
			{
				Result->bTruncated = true;
				continue;
			}
			if (bEffectiveError)
			{
				++RetainedErrors;
			}
			Result->Groups.Add(MoveTemp(Group));
		}

		if (!Options.bSuppressDisplay)
		{
			Result->VisibleGroups = Result->Groups;
		}
		return Result;
	}

END_AS_NAMESPACE

