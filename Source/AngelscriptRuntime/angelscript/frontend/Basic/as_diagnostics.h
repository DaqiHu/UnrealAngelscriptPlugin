/*
   Maintained Unreal AngelScript fork — structured frontend diagnostics.
*/

#pragma once

#include "frontend/Basic/as_source_manager.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"
#include "Templates/SharedPointer.h"

BEGIN_AS_NAMESPACE

enum class asEDiagnosticSeverity : uint8
	{
		Error,
		Warning,
		Note,
		Remark,
	};

	enum class asEDiagnosticArgumentKind : uint8
	{
		Text,
		Signed,
		Unsigned,
		Real,
		Boolean,
	};

	enum class asEDiagnosticStage : uint8
	{
		Lexer,
		Preprocessor,
		Parser,
		Sema,
		Builder,
		Driver,
	};

	enum class asEDiagnosticCategory : uint8
	{
		Lexical,
		Syntax,
		Semantic,
		Driver,
		Internal,
	};

	enum class asEDiagnosticWarningGroup : uint8
	{
		None,
		Unused,
		Deprecated,
	};

	enum class asEDiagnosticCatalogID : asUINT
	{
#define AS_DIAGNOSTIC(Name, ID, Stage, Category, Severity, WarningGroup, Format) Name = ID,
#include "frontend/Basic/as_diagnostic_catalog.def"
#undef AS_DIAGNOSTIC
	};

	struct ANGELSCRIPTRUNTIME_API asSDiagnosticCatalogDescriptor
	{
		asUINT ID = 0;
		const TCHAR* Name = TEXT("");
		asEDiagnosticStage Stage = asEDiagnosticStage::Driver;
		asEDiagnosticCategory Category = asEDiagnosticCategory::Driver;
		asEDiagnosticSeverity DefaultSeverity = asEDiagnosticSeverity::Error;
		asEDiagnosticWarningGroup WarningGroup = asEDiagnosticWarningGroup::None;
		const TCHAR* Format = TEXT("");
	};

	ANGELSCRIPTRUNTIME_API const asSDiagnosticCatalogDescriptor* asFindDiagnosticCatalog(asUINT ID);
	ANGELSCRIPTRUNTIME_API asUINT asDiagnosticIdFromSyntaxReason(FStringView Reason);
	ANGELSCRIPTRUNTIME_API asUINT asDiagnosticIdFromSemanticReason(FStringView Reason);

	struct ANGELSCRIPTRUNTIME_API asSDiagnosticArgument
	{
		asEDiagnosticArgumentKind Kind = asEDiagnosticArgumentKind::Text;
		FString TextValue;
		int64 SignedValue = 0;
		uint64 UnsignedValue = 0;
		double RealValue = 0.0;
		bool BooleanValue = false;

		static asSDiagnosticArgument FromText(FStringView Value);
		static asSDiagnosticArgument FromSigned(int64 Value);
		static asSDiagnosticArgument FromUnsigned(uint64 Value);
		static asSDiagnosticArgument FromReal(double Value);
		static asSDiagnosticArgument FromBoolean(bool Value);
		FString ToDisplayString() const;
	};

	struct ANGELSCRIPTRUNTIME_API asSDiagnosticFixIt
	{
		asCSourceRange Range;
		TArray<uint8> ReplacementUtf8;

		static asSDiagnosticFixIt FromText(const asCSourceRange& InRange, FStringView ReplacementText);
	};

	struct asSDiagnosticRecord
	{
		asUINT DiagnosticID = 0;
		asEDiagnosticSeverity Severity = asEDiagnosticSeverity::Error;
		asCSourceRange PrimaryRange;
		TArray<asCSourceRange> RelatedRanges;
		TArray<asSDiagnosticArgument> Arguments;
		TArray<asSDiagnosticFixIt> FixIts;

		// Filled by the reporting fragment. These are deterministic ordering data,
		// not rendered message text or process-global identities.
		FString StableSourceKey;
		FString StableFragmentKey;
		asUINT LocalSequence = 0;
		bool bHasLocation = true;
	};

	struct ANGELSCRIPTRUNTIME_API asSSourceEdit
	{
		FString LogicalSourceKey;
		uint64 ExpectedRevision = 0;
		asCSourceRange Range;
		TArray<uint8> ExpectedOriginalUtf8;
		TArray<uint8> ReplacementUtf8;
	};

	struct ANGELSCRIPTRUNTIME_API asSDiagnosticFixAlternative
	{
		FString Title;
		TArray<asSDiagnosticFixIt> Edits;
		TArray<asSSourceEdit> SourceEdits;
	};

	struct ANGELSCRIPTRUNTIME_API asSDiagnosticGroup
	{
		asSDiagnosticRecord Primary;
		TArray<asSDiagnosticRecord> Notes;
		TArray<asSDiagnosticFixAlternative> Fixes;
	};

	struct ANGELSCRIPTRUNTIME_API asSDiagnosticOptions
	{
		bool bEnableWarnings = true;
		bool bWarningsAsErrors = false;
		int32 ErrorReportLimit = 0;
		bool bSuppressDisplay = false;
		TMap<asUINT, bool> PerDiagnosticEnabled;
		TMap<asEDiagnosticWarningGroup, bool> PerWarningGroupEnabled;
	};

	class ANGELSCRIPTRUNTIME_API asCDiagnosticResult
	{
	public:
		TConstArrayView<asSDiagnosticGroup> GetGroups() const { return MakeArrayView(Groups); }
		TConstArrayView<asSDiagnosticGroup> GetVisibleGroups() const { return MakeArrayView(VisibleGroups); }
		const asSDiagnosticOptions& GetOptions() const { return Options; }
		const asCSourceManager& GetSources() const { return Sources; }
		bool HasLanguageFailure() const { return bLanguageFailure; }
		bool IsTruncated() const { return bTruncated; }
		bool HasWarningPolicyFailure() const { return bWarningPolicyFailure; }

	private:
		friend class asCDiagnosticsEngine;
		asCSourceManager Sources;
		asSDiagnosticOptions Options;
		TArray<asSDiagnosticGroup> Groups;
		TArray<asSDiagnosticGroup> VisibleGroups;
		bool bLanguageFailure = false;
		bool bTruncated = false;
		bool bWarningPolicyFailure = false;
	};

	class ANGELSCRIPTRUNTIME_API asIDiagnosticConsumer
	{
	public:
		virtual ~asIDiagnosticConsumer() = default;
		virtual void HandleDiagnostics(TConstArrayView<asSDiagnosticRecord> OrderedRecords) = 0;
	};

	class ANGELSCRIPTRUNTIME_API asCCollectingDiagnosticConsumer final : public asIDiagnosticConsumer
	{
	public:
		void HandleDiagnostics(TConstArrayView<asSDiagnosticRecord> OrderedRecords) override;
		TConstArrayView<asSDiagnosticRecord> GetRecords() const { return MakeArrayView(Records); }
		void Reset() { Records.Reset(); }

	private:
		TArray<asSDiagnosticRecord> Records;
	};

	class asCDiagnosticsEngine;
	class asCDiagnosticFragment;

	class ANGELSCRIPTRUNTIME_API asCDiagnostic
	{
	public:
		asCDiagnostic() = default;
		asCDiagnostic(asCDiagnostic&& Other) noexcept;
		~asCDiagnostic();
		asCDiagnostic(const asCDiagnostic&) = delete;
		asCDiagnostic& operator=(const asCDiagnostic&) = delete;
		asCDiagnostic& operator=(asCDiagnostic&&) = delete;

		asCDiagnostic& operator<<(FStringView Value);
		asCDiagnostic& operator<<(int64 Value);
		asCDiagnostic& operator<<(bool Value);
		asCDiagnostic& operator<<(const asSDiagnosticArgument& Value);
		asCDiagnostic& operator<<(const asSDiagnosticFixIt& Value);
		asCDiagnostic& operator<<(const asCSourceRange& RelatedRange);
		asCDiagnostic& operator<<(const asSDiagnosticRecord& Note);
		asCDiagnostic& operator<<(const asSDiagnosticFixAlternative& Fix);

	private:
		friend class asCDiagnosticFragment;
		struct FTarget
		{
			asCDiagnosticFragment* Fragment = nullptr;
		};

		asCDiagnostic(TSharedPtr<FTarget, ESPMode::ThreadSafe> InTarget, asSDiagnosticRecord InPending);

		TSharedPtr<FTarget, ESPMode::ThreadSafe> Target;
		asSDiagnosticRecord Pending;
		TArray<asSDiagnosticRecord> Notes;
		TArray<asSDiagnosticFixAlternative> Fixes;
		bool bCommit = false;
	};

	class ANGELSCRIPTRUNTIME_API asCDiagnosticFragment
	{
	public:
		asCDiagnosticFragment(const asCDiagnosticFragment&) = delete;
		asCDiagnosticFragment& operator=(const asCDiagnosticFragment&) = delete;
		asCDiagnosticFragment(asCDiagnosticFragment&& Other) noexcept;
		asCDiagnosticFragment& operator=(asCDiagnosticFragment&& Other) noexcept;
		~asCDiagnosticFragment();

		asCDiagnostic Diag(const asCSourceRange& Range, asUINT ID);
		asCDiagnostic DiagUnlocated(asUINT ID);
		bool Report(asSDiagnosticRecord Record);
		bool ReportGroup(asSDiagnosticGroup Group);
		uint64 GetSnapshotID() const { return Sources.GetSnapshotID(); }

	private:
		friend class asCDiagnosticsEngine;
		friend class asCDiagnostic;
		asCDiagnosticFragment(
			asCSourceManager InSources,
			FStringView InStableFragmentKey);
		void DetachTarget();

		asCSourceManager Sources;
		FString StableFragmentKey;
		asUINT NextLocalSequence = 0;
		TArray<asSDiagnosticGroup> Groups;
		TSharedPtr<asCDiagnostic::FTarget, ESPMode::ThreadSafe> Target;
	};

	class ANGELSCRIPTRUNTIME_API asCDiagnosticsEngine
	{
	public:
		explicit asCDiagnosticsEngine(
			const asCSourceManager& SourceManager,
			asSDiagnosticOptions InOptions = {});

		asCDiagnosticsEngine(const asCDiagnosticsEngine&) = delete;
		asCDiagnosticsEngine& operator=(const asCDiagnosticsEngine&) = delete;

		asCDiagnosticFragment CreateFragment(FStringView StableFragmentKey) const;
		bool Submit(asCDiagnosticFragment&& Fragment);
		void EmitTo(asIDiagnosticConsumer& Consumer) const;
		asUINT GetPendingCount() const;
		const asCSourceManager& GetSources() const { return Sources; }
		const asSDiagnosticOptions& GetOptions() const { return Options; }
		bool HasLanguageFailure() const;
		TSharedRef<const asCDiagnosticResult, ESPMode::ThreadSafe> CaptureResult() const;

	private:
		asCSourceManager Sources;
		asSDiagnosticOptions Options;
		mutable FCriticalSection RecordsLock;
		TArray<asSDiagnosticGroup> Groups;
	};

END_AS_NAMESPACE

#include "frontend/Basic/as_diagnostic_renderer.h"

