/*
   Maintained Unreal AngelScript fork - read-only canonical AST diagnostics.
   Internal host-neutral API; not part of the public AngelScript SDK ABI.
*/

#ifndef AS_AST_DIAGNOSTICS_H
#define AS_AST_DIAGNOSTICS_H

#include "as_ast_dump.h"

BEGIN_AS_NAMESPACE

enum asEASTDiagnosticOperation : asBYTE
{
	asAST_DIAGNOSTIC_LIST = 0,
	asAST_DIAGNOSTIC_DUMP,
	asAST_DIAGNOSTIC_VERIFY,
	asAST_DIAGNOSTIC_QUERY,
	asAST_DIAGNOSTIC_DIFF,
};

enum asEASTDiagnosticFormat : asBYTE
{
	asAST_DIAGNOSTIC_TEXT = 0,
	asAST_DIAGNOSTIC_JSON,
};

struct asSASTDiagnosticRequest
{
	asEASTDiagnosticOperation operation;
	asEASTDiagnosticFormat format;
	asCString moduleName;
	asCString compareModuleName;
	asSASTStructuredDumpFilter filter;
	bool includeSourceSnippets;
	asUINT maxSnippetBytes;

	asSASTDiagnosticRequest()
		: operation(asAST_DIAGNOSTIC_LIST)
		, format(asAST_DIAGNOSTIC_TEXT)
		, includeSourceSnippets(false)
		, maxSnippetBytes(160)
	{
	}
};

// Every operation acquires its own public V1 lease from the selected module(s)
// and releases it before returning. Output is explicitly diagnostic-only and
// must never be accepted as compiler, Cache V2, or semantic identity input.
ANGELSCRIPTRUNTIME_API int asCASTRunDiagnostics(
	asIScriptEngine* engine,
	const asSASTDiagnosticRequest& request,
	asCString& out);

END_AS_NAMESPACE

#endif
