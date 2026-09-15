/*
   Maintained Unreal AngelScript fork — snapshot-bound UTF-8/UTF-16 positions.
*/

#include "frontend/Basic/as_source_position_codec.h"

BEGIN_AS_NAMESPACE

namespace
	{
		struct asSUtf8DecodeResult
		{
			uint32 CodePoint = 0;
			asUINT ByteCount = 1;
			bool bValid = false;
		};

		asSUtf8DecodeResult DecodeUtf8(const uint8* Bytes, asUINT Remaining)
		{
			asSUtf8DecodeResult Result;
			if (Remaining == 0)
			{
				return Result;
			}
			const uint8 First = Bytes[0];
			if (First < 0x80)
			{
				Result.CodePoint = First;
				Result.ByteCount = 1;
				Result.bValid = true;
				return Result;
			}

			asUINT Expected = 0;
			uint32 CodePoint = 0;
			uint32 Minimum = 0;
			if ((First & 0xE0) == 0xC0)
			{
				Expected = 2;
				CodePoint = First & 0x1F;
				Minimum = 0x80;
			}
			else if ((First & 0xF0) == 0xE0)
			{
				Expected = 3;
				CodePoint = First & 0x0F;
				Minimum = 0x800;
			}
			else if ((First & 0xF8) == 0xF0)
			{
				Expected = 4;
				CodePoint = First & 0x07;
				Minimum = 0x10000;
			}
			else
			{
				return Result;
			}
			if (Remaining < Expected)
			{
				return Result;
			}
			for (asUINT Index = 1; Index < Expected; ++Index)
			{
				if ((Bytes[Index] & 0xC0) != 0x80)
				{
					return Result;
				}
				CodePoint = (CodePoint << 6) | (Bytes[Index] & 0x3F);
			}
			if (CodePoint < Minimum
				|| CodePoint > 0x10FFFF
				|| (CodePoint >= 0xD800 && CodePoint <= 0xDFFF))
			{
				return Result;
			}
			Result.CodePoint = CodePoint;
			Result.ByteCount = Expected;
			Result.bValid = true;
			return Result;
		}

		int32 ColumnWidth(uint32 CodePoint, bool bUtf16, asUINT ByteCount)
		{
			if (bUtf16)
			{
				return CodePoint > 0xFFFF ? 2 : 1;
			}
			return static_cast<int32>(ByteCount);
		}

		struct asSPositionStep
		{
			asUINT ByteCount = 0;
			int32 Width = 0;
			bool bNewline = false;
			bool bValid = false;
		};

		asSPositionStep ClassifyStep(TConstArrayView<uint8> Bytes, asUINT Offset, bool bUtf16)
		{
			asSPositionStep Step;
			if (Offset >= static_cast<asUINT>(Bytes.Num()))
			{
				return Step;
			}
			const uint8 Byte = Bytes[Offset];
			if (Byte == '\r' && Offset + 1 < static_cast<asUINT>(Bytes.Num()) && Bytes[Offset + 1] == '\n')
			{
				Step.ByteCount = 2;
				Step.bNewline = true;
				Step.bValid = true;
				return Step;
			}
			if (Byte == '\n' || Byte == '\r')
			{
				Step.ByteCount = 1;
				Step.bNewline = true;
				Step.bValid = true;
				return Step;
			}
			const asSUtf8DecodeResult Decoded = DecodeUtf8(Bytes.GetData() + Offset, Bytes.Num() - Offset);
			if (!Decoded.bValid)
			{
				return Step;
			}
			Step.ByteCount = Decoded.ByteCount;
			Step.Width = ColumnWidth(Decoded.CodePoint, bUtf16, Decoded.ByteCount);
			Step.bValid = true;
			return Step;
		}
	}

asCSourcePositionCodec::asCSourcePositionCodec(const asCSourceManager& InSnapshot)
	: Snapshot(&InSnapshot)
{
}

bool asCSourcePositionCodec::ByteToPosition(
	asCSourceLocation Byte,
	bool bUtf16,
	int32& Line,
	int32& Column) const
{
	if (!Snapshot || !Snapshot->IsReady() || !Snapshot->IsValidLocation(Byte))
	{
		return false;
	}

	TConstArrayView<uint8> Bytes;
	if (!Snapshot->TryGetBytes(Byte.FileID, Bytes))
	{
		return false;
	}

	int32 WalkLine = 0;
	int32 WalkColumn = 0;
	asUINT Offset = 0;
	const asUINT Target = Byte.ByteOffset;
	const asUINT Length = static_cast<asUINT>(Bytes.Num());
	while (Offset <= Length)
	{
		if (Offset == Length)
		{
			if (Target == Length)
			{
				Line = WalkLine;
				Column = WalkColumn;
				return true;
			}
			return false;
		}

		const asSPositionStep Step = ClassifyStep(Bytes, Offset, bUtf16);
		if (!Step.bValid)
		{
			return false;
		}
		if (Target == Offset)
		{
			Line = WalkLine;
			Column = WalkColumn;
			return true;
		}
		if (Target > Offset && Target < Offset + Step.ByteCount)
		{
			return false;
		}
		if (Step.bNewline)
		{
			++WalkLine;
			WalkColumn = 0;
		}
		else
		{
			WalkColumn += Step.Width;
		}
		Offset += Step.ByteCount;
	}
	return false;
}

bool asCSourcePositionCodec::PositionToByte(
	asCSourceFileID File,
	int32 Line,
	int32 Column,
	bool bUtf16,
	asCSourceLocation& Out) const
{
	if (!Snapshot || !Snapshot->IsReady() || !Snapshot->IsValidFileID(File) || Line < 0 || Column < 0)
	{
		return false;
	}

	TConstArrayView<uint8> Bytes;
	if (!Snapshot->TryGetBytes(File, Bytes))
	{
		return false;
	}

	int32 WalkLine = 0;
	int32 WalkColumn = 0;
	asUINT Offset = 0;
	const asUINT Length = static_cast<asUINT>(Bytes.Num());
	while (Offset <= Length)
	{
		if (Offset == Length)
		{
			if (WalkLine == Line && WalkColumn == Column)
			{
				Out = Snapshot->MakeLocation(File, Offset);
				return true;
			}
			return false;
		}

		const asSPositionStep Step = ClassifyStep(Bytes, Offset, bUtf16);
		if (!Step.bValid)
		{
			return false;
		}
		if (WalkLine == Line && WalkColumn == Column)
		{
			Out = Snapshot->MakeLocation(File, Offset);
			return true;
		}
		if (WalkLine == Line && !Step.bNewline
			&& Column > WalkColumn
			&& Column < WalkColumn + Step.Width)
		{
			return false;
		}
		if (Step.bNewline)
		{
			++WalkLine;
			WalkColumn = 0;
		}
		else
		{
			WalkColumn += Step.Width;
		}
		Offset += Step.ByteCount;
	}
	return false;
}

END_AS_NAMESPACE
