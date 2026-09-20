/*
   Maintained Unreal AngelScript fork — immutable frontend source queries.
*/

#pragma once

#include "frontend/Basic/as_source_location.h"
#include "frontend/Basic/as_source_provenance.h"
#include "unreal/AngelscriptSource.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/StringView.h"
#include "Templates/SharedPointer.h"

BEGIN_AS_NAMESPACE

struct asSSourceLineColumn
{
	asUINT Line = 0;
	asUINT Column = 0;
};

ANGELSCRIPTRUNTIME_API bool asTryConvertSourceByteLength(uint64 ByteCount, asUINT& OutLength);

class asSSourceIndex;

class ANGELSCRIPTRUNTIME_API asCSourceManager
{
public:
	asCSourceManager();
	explicit asCSourceManager(
		TConstArrayView<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources);
	asCSourceManager(const asCSourceManager& Other);
	asCSourceManager& operator=(const asCSourceManager& Other);
	asCSourceManager(asCSourceManager&& Other) noexcept;
	asCSourceManager& operator=(asCSourceManager&& Other) noexcept;
	~asCSourceManager();

	uint64 GetSnapshotID() const;
	bool IsReady() const;
	asUINT GetFileCount() const;
	bool IsValidFileID(asCSourceFileID FileID) const;
	bool TryGetBytes(asCSourceFileID FileID, TConstArrayView<uint8>& OutBytes) const;
	bool TryGetLogicalSourceKey(asCSourceFileID FileID, FStringView& OutLogicalSourceKey) const;
	bool TryGetContentRevision(asCSourceFileID FileID, uint64& OutContentRevision) const;
	asCSourceFileID FindFile(FStringView LogicalSourceKey, uint64 ContentRevision) const;

	asCSourceLocation MakeLocation(asCSourceFileID FileID, asUINT ByteOffset) const;
	asCSourceRange MakeRange(asCSourceFileID FileID, asUINT BeginByteOffset, asUINT EndByteOffset) const;
	bool IsValidLocation(const asCSourceLocation& Location) const;
	bool IsValidRange(const asCSourceRange& Range) const;
	bool TryGetBytes(const asCSourceRange& Range, TConstArrayView<uint8>& OutBytes) const;
	bool HasMaterializedLineMap(asCSourceFileID FileID) const;
	bool TryGetLineColumn(const asCSourceLocation& Location, asSSourceLineColumn& OutLineColumn) const;
	asCSourceOriginID AddOrigin(const asSSourceOriginRecord& Record);
	bool FindOrigins(const asCSourceRange& Range, TArray<asSSourceOriginRecord>& OutRecords) const;
	bool TryResolveOriginChain(
		asCSourceOriginID OriginID,
		TArray<asSSourceOriginRecord>& OutChain) const;

	bool TryCreateStableAnchor(const asCSourceRange& Range, asSStableSourceAnchor& OutAnchor) const;
	bool TryRelocateStableAnchor(const asSStableSourceAnchor& Anchor, asCSourceRange& OutRange) const;

private:
	TSharedPtr<asSSourceIndex, ESPMode::ThreadSafe> Index;
};

END_AS_NAMESPACE
