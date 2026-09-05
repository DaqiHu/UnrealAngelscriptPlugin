/*
   Maintained Unreal AngelScript fork — structured frontend diagnostics.
*/

#pragma once

#include "frontend/as_source_manager.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
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

	class ANGELSCRIPTRUNTIME_API asCDiagnosticFragment
	{
	public:
		asCDiagnosticFragment(const asCDiagnosticFragment&) = delete;
		asCDiagnosticFragment& operator=(const asCDiagnosticFragment&) = delete;
		asCDiagnosticFragment(asCDiagnosticFragment&&) = default;
		asCDiagnosticFragment& operator=(asCDiagnosticFragment&&) = default;

		bool Report(asSDiagnosticRecord Record);
		uint64 GetSnapshotID() const { return Snapshot->GetSnapshotID(); }

	private:
		friend class asCDiagnosticsEngine;
		asCDiagnosticFragment(
			TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> InSnapshot,
			FStringView InStableFragmentKey);

		TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot;
		FString StableFragmentKey;
		asUINT NextLocalSequence = 0;
		TArray<asSDiagnosticRecord> Records;
	};

	class ANGELSCRIPTRUNTIME_API asCDiagnosticsEngine
	{
	public:
		explicit asCDiagnosticsEngine(const asCSourceManager& SourceManager);

		asCDiagnosticsEngine(const asCDiagnosticsEngine&) = delete;
		asCDiagnosticsEngine& operator=(const asCDiagnosticsEngine&) = delete;

		asCDiagnosticFragment CreateFragment(FStringView StableFragmentKey) const;
		bool Submit(asCDiagnosticFragment&& Fragment);
		void EmitTo(asIDiagnosticConsumer& Consumer) const;
		asUINT GetPendingCount() const;

	private:
		TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot;
		mutable FCriticalSection RecordsLock;
		TArray<asSDiagnosticRecord> Records;
	};

	class ANGELSCRIPTRUNTIME_API asCDiagnosticRenderer
	{
	public:
		static FString Render(FStringView Format, const asSDiagnosticRecord& Record);
		static FString SerializeStable(TConstArrayView<asSDiagnosticRecord> Records);
	};

END_AS_NAMESPACE

