/* Shared token grammar for script and detached host type syntax. */
#pragma once
#include "frontend/as_type_syntax.h"
#include "frontend/as_token.h"
#include "Containers/ArrayView.h"

BEGIN_AS_NAMESPACE
class ANGELSCRIPTRUNTIME_API asCTypeSyntaxParser final
{
public:
	explicit asCTypeSyntaxParser(TConstArrayView<asCToken> InTokens) : Tokens(InTokens) {}
	bool ParseTypeSyntax(int32& Index, int32 End, asSTypeSyntax& OutType) const;
	// Cast parsing consumes one additional closing angle from a compound token.
	bool ParseTypeSyntaxInternal(int32& Index, int32 End, asSTypeSyntax& OutType, int32& PendingClosers, int32 Depth) const;
private:
	FString Text(int32 Index) const;
	asCSourceRange Span(int32 Begin, int32 End) const;
	TConstArrayView<asCToken> Tokens;
};
END_AS_NAMESPACE
