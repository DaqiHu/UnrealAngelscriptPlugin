#ifndef AS_AST_TYPE_H
#define AS_AST_TYPE_H

#include "as_ast_fwd.h"
#include "as_ast_kind.h"
#include "as_string.h"
#include "as_tokendef.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCType
{
public:
	asASTTypeRef id;
	asEASTTypeKind kind;
	eTokenType primitiveToken;
	asCString stableKey;

	asCType()
		: kind(asAST_TYPE_INVALID)
		, primitiveToken(ttUnrecognizedToken)
	{
	}
};

class ANGELSCRIPTRUNTIME_API asCQualType
{
public:
	asASTTypeRef type;
	asDWORD quals;

	asCQualType() : quals(0) {}
	asCQualType(asASTTypeRef InType, asDWORD InQuals) : type(InType), quals(InQuals) {}

	bool IsValid() const { return type.IsValid(); }
	bool IsConst() const { return (quals & asAST_QUAL_CONST) != 0; }
	bool IsHandle() const { return (quals & asAST_QUAL_HANDLE) != 0; }
	bool IsAutoHandle() const { return (quals & asAST_QUAL_AUTO_HANDLE) != 0; }
	bool IsReference() const { return (quals & asAST_QUAL_REFERENCE) != 0; }

	bool operator==(const asCQualType& Other) const
	{
		return type == Other.type && quals == Other.quals;
	}

	bool operator!=(const asCQualType& Other) const
	{
		return !(*this == Other);
	}
};

ANGELSCRIPTRUNTIME_API bool asASTQualifiersAreValid(asDWORD quals);

END_AS_NAMESPACE

#endif
