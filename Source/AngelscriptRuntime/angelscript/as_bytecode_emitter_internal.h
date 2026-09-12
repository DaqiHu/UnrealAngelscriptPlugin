#pragma once
#include "as_bytecode_image_builder.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "frontend/AST/as_decl.h"
#include "frontend/AST/as_expr.h"
#include "frontend/AST/as_stmt.h"
#include "frontend/AST/as_ast_cast.h"

#include "Containers/Map.h"

BEGIN_AS_NAMESPACE
class asCCompilationSession;
struct asSByteCodeEmitFrame
{
	asCByteCodeImageBuilder* Builder = nullptr;
	const asCCompilationSession* Session = nullptr;
	asCScriptFunction* Function = nullptr;
	asUINT NextLocal = 4;
	asUINT MaxFrameDWords = 64;
	asUINT ParameterDwords = 0;
	TMap<asCDecl*, int32> Locals;
	TMap<asSStableKey, asCScriptFunction*> Functions;
	TMap<asSStableKey, asCObjectType*> Types;
	TArray<TPair<int32, asCObjectType*>> LiveObjects;
	TArray<int32> ConstructedSlots;
	TArray<asUINT> BreakLabels;
	TArray<asUINT> ContinueLabels;
	TArray<int32> BreakLiveStarts;
	TArray<int32> ContinueLiveStarts;
	asUINT AllocLocal(asUINT Count = 1)
	{
		const asUINT Result = NextLocal;
		NextLocal += Count;
		return Result;
	}
};
bool asEmitExpr(asSByteCodeEmitFrame& Frame, asCExpr* Expr, int32& OutLocal);
bool asEmitStmt(asSByteCodeEmitFrame& Frame, asCStmt* Stmt);
bool asEmitReturn(asSByteCodeEmitFrame& Frame, asCReturnStmt* Stmt);
bool asEmitCall(asSByteCodeEmitFrame& Frame, asCCallExpr* Call, int32& OutLocal);
bool asEmitMemberAddress(asSByteCodeEmitFrame& Frame, asCMemberExpr* Member);
bool asEmitConstructInto(asSByteCodeEmitFrame& Frame, asCConstructExpr* Construct, int32 DestLocal);
bool asEmitDestroyLive(asSByteCodeEmitFrame& Frame);
bool asEmitDestroyFrom(asSByteCodeEmitFrame& Frame, int32 FirstLive, bool bPopLive = true, bool bRecordCleanup = true);
bool asEmitNoteLiveObject(asSByteCodeEmitFrame& Frame, int32 Local, asCObjectType* Type);
bool asEmitCallDefaultConstructor(asSByteCodeEmitFrame& Frame, asCObjectType* Type, int32 DestLocal);
bool asEmitNoteSource(asSByteCodeEmitFrame& Frame, const asCStmt* Stmt);
asCObjectType* asEmitFindObjectType(asSByteCodeEmitFrame& Frame, asCType* Type);
END_AS_NAMESPACE
