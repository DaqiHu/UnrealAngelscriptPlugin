/*
   Maintained Unreal AngelScript fork — canonical typed AST.
   Clang-shaped IDs and pipeline selection. Not public SDK ABI.
*/

#ifndef AS_AST_FWD_H
#define AS_AST_FWD_H

#include "as_config.h"

BEGIN_AS_NAMESPACE

enum asECompilerPipeline
{
	asCOMPILER_PIPELINE_LEGACY = 0,
	asCOMPILER_PIPELINE_CANONICAL = 1,
};

// Observable Bytecode origin. CANONICAL pipeline selection is not proof of this value.
enum asEBytecodePublisher
{
	asBYTECODE_PUBLISHER_NONE = 0,
	asBYTECODE_PUBLISHER_COMPILER = 1,
	asBYTECODE_PUBLISHER_CANONICAL_CODEGEN = 2,
};

typedef asUINT asASTFileID;

class asCSourceManager;
class asCASTContext;
class asCType;
class asCQualType;
class asCDecl;
class asCStmt;
class asCExpr;
class asCRuntimeTypeBridge;

END_AS_NAMESPACE

#endif
