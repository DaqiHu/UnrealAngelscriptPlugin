/*
   Maintained Unreal AngelScript fork — bounded UTF-8 byte stream.
*/

#pragma once

#include "frontend/Basic/as_source_manager.h"

BEGIN_AS_NAMESPACE

class asCTokenizer;

	class asCCharacterStream
	{
	public:
		asCCharacterStream() = default;

		static bool TryCreate(
			const asCSourceManager& SourceManager,
			const asCSourceRange& Range,
			asCCharacterStream& OutStream)
		{
			OutStream = asCCharacterStream();
			TConstArrayView<uint8> Bytes;
			if (!SourceManager.TryGetBytes(Range, Bytes))
			{
				return false;
			}
			OutStream.BoundRange = Range;
			OutStream.Bytes = Bytes;
			OutStream.bValid = true;
			return true;
		}

		bool IsValid() const { return bValid; }
		bool IsAtEnd() const { return !bValid || Cursor >= static_cast<asUINT>(Bytes.Num()); }
		asUINT GetByteOffset() const { return BoundRange.Begin.ByteOffset + Cursor; }
		asUINT GetRemainingByteCount() const
		{
			return bValid ? static_cast<asUINT>(Bytes.Num()) - Cursor : 0;
		}

		uint8 Peek(asUINT LookAhead = 0) const
		{
			const asUINT Index = Cursor + LookAhead;
			return bValid && Index < static_cast<asUINT>(Bytes.Num()) ? Bytes[Index] : 0;
		}

		bool Advance(asUINT ByteCount = 1)
		{
			if (!bValid || ByteCount > GetRemainingByteCount())
			{
				return false;
			}
			Cursor += ByteCount;
			return true;
		}

	private:
		friend class asCTokenizer;

		const uint8* GetCurrentPointer() const { return Bytes.GetData() + Cursor; }
		const uint8* GetEndPointer() const { return Bytes.GetData() + Bytes.Num(); }
		const asCSourceRange& GetBoundRange() const { return BoundRange; }

		asCSourceRange BoundRange;
		TConstArrayView<uint8> Bytes;
		asUINT Cursor = 0;
		bool bValid = false;
	};

END_AS_NAMESPACE

