#include "as_ast_type.h"

BEGIN_AS_NAMESPACE

bool asASTQualifiersAreValid(asDWORD quals)
{
	const asDWORD known = asAST_QUAL_CONST
		| asAST_QUAL_HANDLE
		| asAST_QUAL_AUTO_HANDLE
		| asAST_QUAL_REFERENCE
		| asAST_QUAL_PARAM_DIR_MASK;
	if( (quals & ~known) != 0 )
	{
		return false;
	}
	const asDWORD dir = quals & asAST_QUAL_PARAM_DIR_MASK;
	if( dir != 0 && (quals & asAST_QUAL_REFERENCE) == 0 )
	{
		return false;
	}
	if( (quals & asAST_QUAL_AUTO_HANDLE) != 0 && (quals & asAST_QUAL_HANDLE) == 0 )
	{
		return false;
	}
	return true;
}

END_AS_NAMESPACE
