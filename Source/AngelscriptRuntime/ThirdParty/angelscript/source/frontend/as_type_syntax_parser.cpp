#include "frontend/as_type_syntax_parser.h"
#include "frontend/as_identifier_table.h"
#include "Containers/StringConv.h"

BEGIN_AS_NAMESPACE
FString asCTypeSyntaxParser::Text(int32 Index) const
{
    const asCToken& Token = Tokens[Index];
    if (!Token.Identifier) return asGetTokenSpelling(Token.Kind);
    const auto Bytes = Token.Identifier->GetSpelling();
    FUTF8ToTCHAR Value(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
    return FString(Value.Length(), Value.Get());
}
asCSourceRange asCTypeSyntaxParser::Span(int32 Begin, int32 End) const
{
    return {Tokens[Begin].Range.Begin, Tokens[End].Range.End};
}
	bool asCTypeSyntaxParser::ParseTypeSyntax(int32& Index, int32 End, asSTypeSyntax& OutType) const
	{
		int32 PendingClosers = 0;
		return ParseTypeSyntaxInternal(Index, End, OutType, PendingClosers, 0) && PendingClosers == 0;
	}

	bool asCTypeSyntaxParser::ParseTypeSyntaxInternal(int32& Index, int32 End, asSTypeSyntax& OutType, int32& PendingClosers, int32 Depth) const
	{
		if (Depth > 64 || PendingClosers != 0) return false;
		const int32 Begin = Index;
		if (Index < End && Tokens[Index].Kind == asETokenKind::KwConst) { OutType.bConst = true; ++Index; }
		if (Index < End && Tokens[Index].Kind == asETokenKind::Scope) { OutType.bGlobal = true; ++Index; }
		auto IsName = [](asETokenKind Kind)
		{
			switch (Kind)
			{
			case asETokenKind::Identifier: case asETokenKind::KwVoid: case asETokenKind::KwBool: case asETokenKind::KwAuto:
			case asETokenKind::KwInt: case asETokenKind::KwInt8: case asETokenKind::KwInt16: case asETokenKind::KwInt64:
			case asETokenKind::KwUInt: case asETokenKind::KwUInt8: case asETokenKind::KwUInt16: case asETokenKind::KwUInt64:
			case asETokenKind::KwFloat: case asETokenKind::KwFloat32: case asETokenKind::KwDouble: case asETokenKind::KwFloat64:
				return true;
			default: return false;
			}
		};
		if (Index < End && Tokens[Index].Kind == asETokenKind::Question)
		{
			OutType.bWildcard = true;
			OutType.ComponentRanges.Add(Tokens[Index++].Range);
		}
		else
		{
			if (Index >= End || !IsName(Tokens[Index].Kind)) return false;
			OutType.NameParts.Add(Text(Index)); OutType.ComponentRanges.Add(Tokens[Index++].Range);
		}
		while (!OutType.bWildcard && Index < End && Tokens[Index].Kind == asETokenKind::Scope)
		{
			++Index;
			if (Index >= End || Tokens[Index].Kind != asETokenKind::Identifier) return false;
			OutType.NameParts.Add(Text(Index)); OutType.ComponentRanges.Add(Tokens[Index++].Range);
		}

		if (!OutType.bWildcard && Index < End && Tokens[Index].Kind == asETokenKind::Less)
		{
			++Index;
			for (;;)
			{
				asSTypeSyntax Argument;
				if (!ParseTypeSyntaxInternal(Index, End, Argument, PendingClosers, Depth + 1)) return false;
				OutType.GenericArguments.Add(MoveTemp(Argument));
				if (PendingClosers > 0) { --PendingClosers; break; }
				if (Index >= End) return false;
				if (Tokens[Index].Kind == asETokenKind::Greater) { ++Index; break; }
				if (Tokens[Index].Kind == asETokenKind::ShiftRight || Tokens[Index].Kind == asETokenKind::ShiftRightArithmetic)
				{
					PendingClosers = Tokens[Index++].Kind == asETokenKind::ShiftRight ? 1 : 2;
					break;
				}
				if (Tokens[Index++].Kind != asETokenKind::Comma) return false;
			}
		}
		while (!OutType.bWildcard && PendingClosers == 0 && Index + 1 < End && Tokens[Index].Kind == asETokenKind::LeftBracket && Tokens[Index + 1].Kind == asETokenKind::RightBracket)
		{
			OutType.ArrayDimensions.Add(Span(Index, Index + 1));
			Index += 2;
		}
		if (!OutType.bWildcard && PendingClosers == 0 && Index < End && Tokens[Index].Kind == asETokenKind::At) { OutType.bHandle = true; ++Index; }
		if (PendingClosers == 0 && Index < End && Tokens[Index].Kind == asETokenKind::Amp)
		{
			++Index;
			OutType.Reference = asETypeReferenceDirection::InOut;
			if (Index < End)
			{
				if (Tokens[Index].Kind == asETokenKind::KwIn) { OutType.Reference = asETypeReferenceDirection::In; ++Index; }
				else if (Tokens[Index].Kind == asETokenKind::KwOut) { OutType.Reference = asETypeReferenceDirection::Out; ++Index; }
				else if (Tokens[Index].Kind == asETokenKind::KwInOut) ++Index;
			}
		}
		OutType.Range = Span(Begin, Index - 1);
		if (PendingClosers > 0) OutType.Range.End.ByteOffset -= static_cast<asUINT>(PendingClosers);
		return true;
	}

END_AS_NAMESPACE
