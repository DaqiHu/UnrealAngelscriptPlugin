#include "as_source_manager.h"
#include "as_memory.h"
#include <cstring>

BEGIN_AS_NAMESPACE

namespace
{
	bool SourceAnchorEquals(
		const asSSourceAnchor& left,
		const asSSourceAnchor& right)
	{
		return left.section.Equals(right.section.AddressOf())
			&& left.sourceOffset == right.sourceOffset
			&& left.sourceLength == right.sourceLength
			&& left.row == right.row
			&& left.column == right.column;
	}

	bool GeneratedSourceOriginEquals(
		const asSGeneratedSourceOrigin& left,
		const asSGeneratedSourceOrigin& right)
	{
		return left.kind == right.kind
			&& left.generatorName.Equals(right.generatorName.AddressOf())
			&& left.hasAuthoredAnchor == right.hasAuthoredAnchor
			&& (!left.hasAuthoredAnchor
				|| SourceAnchorEquals(
					left.authoredAnchor,
					right.authoredAnchor));
	}

	bool SourceProvenanceRangeEquals(
		const asSSourceProvenanceRange& left,
		const asSSourceProvenanceRange& right)
	{
		return left.processedOffset == right.processedOffset
			&& left.processedLength == right.processedLength
			&& left.hasAuthoredOrigin == right.hasAuthoredOrigin
			&& (!left.hasAuthoredOrigin
				|| SourceAnchorEquals(left.authoredOrigin, right.authoredOrigin))
			&& left.hasGeneratedOrigin == right.hasGeneratedOrigin
			&& (!left.hasGeneratedOrigin
				|| GeneratedSourceOriginEquals(
					left.generatedOrigin,
					right.generatedOrigin));
	}
}

asCSourceManager::asCSourceManager()
{
}

asCSourceManager::~asCSourceManager()
{
	for( asUINT i = 0; i < sections.GetLength(); ++i )
	{
		asDELETE(sections[i], asSSection);
	}
	sections.SetLength(0);
}

void asCSourceManager::BuildLineStarts(asSSection& section)
{
	section.lineStarts.SetLength(0);
	section.lineStarts.PushLast(0);
	const char* bytes = section.buffer.AddressOf();
	const asUINT length = (asUINT)section.buffer.GetLength();
	for( asUINT i = 0; i < length; ++i )
	{
		if( bytes[i] == '\n' )
		{
			section.lineStarts.PushLast(i + 1);
		}
	}
}

asASTFileID asCSourceManager::AddSection(
	const char* logicalKey,
	asEASTSourceOrigin origin,
	const char* bytes,
	size_t byteCount,
	int lineOffset,
	const asSSourceProvenanceRange* provenanceRanges,
	asUINT provenanceRangeCount)
{
	if( logicalKey == 0 || origin == asAST_SOURCE_INVALID )
	{
		return 0;
	}
	if( !ValidateProvenanceRanges(
		provenanceRanges,
		provenanceRangeCount,
		byteCount) )
	{
		return 0;
	}

	asSSection* section = asNEW(asSSection);
	if( section == 0 )
	{
		return 0;
	}

	section->logicalKey = logicalKey;
	section->origin = origin;
	if( bytes != 0 && byteCount > 0 )
	{
		section->buffer.Assign(bytes, (asUINT)byteCount);
	}
	section->lineOffset = lineOffset;
	section->provenanceRanges.SetLength(provenanceRangeCount);
	for( asUINT index = 0; index < provenanceRangeCount; ++index )
	{
		section->provenanceRanges[index] = provenanceRanges[index];
	}
	BuildLineStarts(*section);
	sections.PushLast(section);
	return (asASTFileID)sections.GetLength();
}

asCSourceManager::asSSection* asCSourceManager::GetSection(asASTFileID fileID)
{
	if( fileID == 0 || fileID > sections.GetLength() )
	{
		return 0;
	}
	return sections[fileID - 1];
}

const asCSourceManager::asSSection* asCSourceManager::GetSection(asASTFileID fileID) const
{
	if( fileID == 0 || fileID > sections.GetLength() )
	{
		return 0;
	}
	return sections[fileID - 1];
}

const char* asCSourceManager::GetLogicalKey(asASTFileID fileID) const
{
	const asSSection* section = GetSection(fileID);
	return section ? section->logicalKey.AddressOf() : 0;
}

asEASTSourceOrigin asCSourceManager::GetOrigin(asASTFileID fileID) const
{
	const asSSection* section = GetSection(fileID);
	return section ? section->origin : asAST_SOURCE_INVALID;
}

const char* asCSourceManager::GetBuffer(asASTFileID fileID) const
{
	const asSSection* section = GetSection(fileID);
	return section ? section->buffer.AddressOf() : 0;
}

asUINT asCSourceManager::GetBufferLength(asASTFileID fileID) const
{
	const asSSection* section = GetSection(fileID);
	return section ? (asUINT)section->buffer.GetLength() : 0;
}

int asCSourceManager::GetLineOffset(asASTFileID fileID) const
{
	const asSSection* section = GetSection(fileID);
	return section ? section->lineOffset : 0;
}

asUINT asCSourceManager::GetSourceProvenanceRangeCount(asASTFileID fileID) const
{
	const asSSection* section = GetSection(fileID);
	return section ? section->provenanceRanges.GetLength() : 0;
}

bool asCSourceManager::GetSourceProvenanceRange(
	asASTFileID fileID,
	asUINT index,
	asSSourceProvenanceRange& outRange) const
{
	const asSSection* section = GetSection(fileID);
	if( section == 0 || index >= section->provenanceRanges.GetLength() )
	{
		return false;
	}
	outRange = section->provenanceRanges[index];
	return true;
}

asCSourceLocation asCSourceManager::MakeLocation(asASTFileID fileID, asUINT offset) const
{
	const asSSection* section = GetSection(fileID);
	if( section == 0 || offset > (asUINT)section->buffer.GetLength() )
	{
		return asCSourceLocation();
	}
	return asCSourceLocation(fileID, offset);
}

asCSourceRange asCSourceManager::MakeRange(asASTFileID fileID, asUINT beginOffset, asUINT endOffset) const
{
	asCSourceRange range(MakeLocation(fileID, beginOffset), MakeLocation(fileID, endOffset));
	if( !IsValidRange(range) )
	{
		return asCSourceRange();
	}
	return range;
}

bool asCSourceManager::IsValidRange(const asCSourceRange& range) const
{
	if( !range.IsValid() )
	{
		return false;
	}
	const asSSection* section = GetSection(range.begin.fileID);
	if( section == 0 )
	{
		return false;
	}
	const asUINT length = (asUINT)section->buffer.GetLength();
	return range.end.offset <= length;
}

bool asCSourceManager::ResolveSourceSpan(
	const asCSourceRange& range,
	asSSourceSpan& outSpan) const
{
	outSpan = asSSourceSpan();
	if( !IsValidRange(range) )
	{
		return false;
	}

	const asSSection* section = GetSection(range.begin.fileID);
	if( section == 0 )
	{
		return false;
	}
	asSSourceLineColumn lineColumn = {};
	if( !GetLineColumn(range.begin, lineColumn) )
	{
		return false;
	}

	outSpan.section = section->logicalKey;
	outSpan.sourceOffset = range.begin.offset;
	outSpan.sourceLength = range.end.offset - range.begin.offset;
	outSpan.row = (asUINT)lineColumn.row;
	outSpan.column = (asUINT)lineColumn.column;
	ApplySourceProvenance(*section, outSpan);
	return true;
}

bool asCSourceManager::GetLineColumn(const asCSourceLocation& loc, asSSourceLineColumn& out) const
{
	out.row = 0;
	out.column = 0;
	const asSSection* section = GetSection(loc.fileID);
	if( section == 0 || loc.offset > (asUINT)section->buffer.GetLength() )
	{
		return false;
	}

	asUINT lineIndex = 0;
	for( asUINT i = 0; i < section->lineStarts.GetLength(); ++i )
	{
		if( section->lineStarts[i] <= loc.offset )
		{
			lineIndex = i;
		}
		else
		{
			break;
		}
	}

	out.row = section->lineOffset + (int)lineIndex + 1;
	out.column = (int)(loc.offset - section->lineStarts[lineIndex]) + 1;
	return true;
}

asASTFileID asCSourceManager::FindFile(const char* logicalKey, asEASTSourceOrigin origin) const
{
	if( logicalKey == 0 )
	{
		return 0;
	}
	for( asUINT i = 0; i < sections.GetLength(); ++i )
	{
		if( sections[i]->origin == origin && sections[i]->logicalKey.Equals(logicalKey) )
		{
			return i + 1;
		}
	}
	return 0;
}

asASTFileID asCSourceManager::RemapLogical(
	const char* logicalKey,
	asEASTSourceOrigin origin,
	const char* bytes,
	size_t byteCount,
	int lineOffset,
	const asSSourceProvenanceRange* provenanceRanges,
	asUINT provenanceRangeCount)
{
	if( !ValidateProvenanceRanges(
		provenanceRanges,
		provenanceRangeCount,
		byteCount) )
	{
		return 0;
	}
	const asASTFileID existing = FindFile(logicalKey, origin);
	if( existing != 0 )
	{
		const asSSection* section = GetSection(existing);
		const bool hasSameLineOffset = section != 0 && section->lineOffset == lineOffset;
		const bool hasSameByteCount = section != 0 && (size_t)section->buffer.GetLength() == byteCount;
		const bool hasSameBytes =
			byteCount == 0 ||
			(bytes != 0 && section != 0 && std::memcmp(section->buffer.AddressOf(), bytes, byteCount) == 0);
		const bool hasSameProvenance = section != 0
			&& HasSameProvenanceRanges(
				*section,
				provenanceRanges,
				provenanceRangeCount);
		return hasSameLineOffset && hasSameByteCount && hasSameBytes
			&& hasSameProvenance ? existing : 0;
	}
	return AddSection(
		logicalKey,
		origin,
		bytes,
		byteCount,
		lineOffset,
		provenanceRanges,
		provenanceRangeCount);
}

bool asCSourceManager::ValidateProvenanceRanges(
	const asSSourceProvenanceRange* ranges,
	asUINT rangeCount,
	size_t byteCount) const
{
	if( rangeCount != 0 && ranges == 0 )
	{
		return false;
	}
	asUINT previousEnd = 0;
	for( asUINT index = 0; index < rangeCount; ++index )
	{
		const asSSourceProvenanceRange& range = ranges[index];
		if( range.processedLength == 0
			|| (size_t)range.processedOffset > byteCount
			|| (size_t)range.processedLength
				> byteCount - (size_t)range.processedOffset
			|| (index != 0 && range.processedOffset < previousEnd) )
		{
			return false;
		}
		previousEnd = range.processedOffset + range.processedLength;
	}
	return true;
}

bool asCSourceManager::HasSameProvenanceRanges(
	const asSSection& section,
	const asSSourceProvenanceRange* ranges,
	asUINT rangeCount) const
{
	if( section.provenanceRanges.GetLength() != rangeCount )
	{
		return false;
	}
	for( asUINT index = 0; index < rangeCount; ++index )
	{
		if( !SourceProvenanceRangeEquals(
			section.provenanceRanges[index],
			ranges[index]) )
		{
			return false;
		}
	}
	return true;
}

void asCSourceManager::ApplySourceProvenance(
	const asSSection& section,
	asSSourceSpan& span) const
{
	const asUINT spanEnd = span.sourceOffset + span.sourceLength;
	if( spanEnd < span.sourceOffset )
	{
		return;
	}
	for( asUINT index = 0;
		index < section.provenanceRanges.GetLength();
		++index )
	{
		const asSSourceProvenanceRange& range =
			section.provenanceRanges[index];
		const asUINT rangeEnd = range.processedOffset + range.processedLength;
		if( span.sourceOffset < range.processedOffset )
		{
			break;
		}
		if( span.sourceOffset >= range.processedOffset
			&& spanEnd <= rangeEnd )
		{
			span.hasAuthoredOrigin = range.hasAuthoredOrigin;
			span.authoredOrigin = range.authoredOrigin;
			span.hasGeneratedOrigin = range.hasGeneratedOrigin;
			span.generatedOrigin = range.generatedOrigin;
			return;
		}
	}
}

END_AS_NAMESPACE
