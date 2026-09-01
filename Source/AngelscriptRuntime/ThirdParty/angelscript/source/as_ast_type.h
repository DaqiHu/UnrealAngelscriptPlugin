#ifndef AS_AST_TYPE_H
#define AS_AST_TYPE_H

#include "as_array.h"
#include "as_ast_fwd.h"
#include "as_ast_kind.h"
#include "as_string.h"
#include "as_tokendef.h"

BEGIN_AS_NAMESPACE

// Clang TemplateSpecializationType stores template arguments as QualTypes.
// Canonical intern identity remains kind + stableKey; these arguments are the
// AST-local subtype facts so backends do not parse the key string.
struct asSAstTemplateArgument
{
	asASTTypeRef type;
	asDWORD quals;

	asSAstTemplateArgument()
		: quals(0)
	{
	}
};

class ANGELSCRIPTRUNTIME_API asCType
{
public:
	asASTTypeRef id;
	asEASTTypeKind kind;
	eTokenType primitiveToken;
	asCString stableKey;
	// Sema-authored install gate. Identity remains kind + stableKey; this flag
	// tells CodeGen/binding to run the application template callback before
	// publishing an executable generation. It is not a Runtime pointer or TypeId.
	bool requiresRuntimeValidation;
	asCArray<asSAstTemplateArgument> templateArguments;

	asCType()
		: kind(asAST_TYPE_INVALID)
		, primitiveToken(ttUnrecognizedToken)
		, requiresRuntimeValidation(false)
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
