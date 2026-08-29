#ifndef AS_AST_CONTEXT_H
#define AS_AST_CONTEXT_H

#include "as_array.h"
#include "as_decl.h"
#include "as_expr.h"
#include "as_ast_lifetime.h"
#include "as_source_manager.h"
#include "as_stmt.h"

BEGIN_AS_NAMESPACE

enum asEASTContextLifecycleState : asBYTE
{
	asAST_CONTEXT_BUILDING = 0,
	asAST_CONTEXT_SEMA_FINALIZED,
	asAST_CONTEXT_LIFETIME_PLANNED,
	asAST_CONTEXT_FROZEN_PUBLISHABLE,
};

class ANGELSCRIPTRUNTIME_API asCASTContext
{
public:
	asCASTContext();
	~asCASTContext();

	const asCSourceManager& GetSourceManager() const { return sourceManager; }

	asASTDeclId CreateTranslationUnit(const char* moduleName);
	asASTDeclId CreateDecl(asEASTDeclKind kind, asASTDeclId parent, const asCSourceRange& range, const char* name);
	asASTStmtId CreateStmt(asEASTStmtKind kind, asASTDeclId owner, const asCSourceRange& range);
	asASTExprId CreateExpr(asEASTExprKind kind, const asCQualType& type, asEASTValueCategory category, const asCSourceRange& range);

	asCQualType InternPrimitive(eTokenType primitiveToken, asDWORD quals = 0);
	asCQualType InternNamedType(asEASTTypeKind kind, const char* stableKey, asDWORD quals = 0);

	asASTFileID AddSourceSection(
		const char* logicalKey,
		asEASTSourceOrigin origin,
		const char* bytes,
		size_t byteCount,
		int lineOffset = 0,
		const asSSourceProvenanceRange* provenanceRanges = 0,
		asUINT provenanceRangeCount = 0);
	asASTFileID RemapSourceSection(
		const char* logicalKey,
		asEASTSourceOrigin origin,
		const char* bytes,
		size_t byteCount,
		int lineOffset = 0,
		const asSSourceProvenanceRange* provenanceRanges = 0,
		asUINT provenanceRangeCount = 0);
	int SetDeclType(asASTDeclId id, const asCQualType& type);
	int SetBody(asASTDeclId id, asASTStmtId body);
	int AddDeclTrait(asASTDeclId id, asDWORD traits);
	int SetDeclTraits(asASTDeclId id, asDWORD traits);
	int SetDeclName(asASTDeclId id, const char* name);
	int SetGeneratedAccessor(asASTDeclId id, asEASTAccessorKind kind, asASTDeclId field);
	int SetDeclAccessSpecifier(asASTDeclId member, asASTDeclId specifier);
	int SetStableKey(asASTDeclId id, const char* key);
	int AddDependency(asASTDeclId id, const char* key);
	int ClearDeclDependencies(asASTDeclId id);
	int SetOrigin(asASTDeclId id, const char* origin);
	int AddBase(asASTDeclId id, asASTDeclId base);
	int AddMethodRelation(
		asASTDeclId owner,
		const asSASTMethodRelation& relation);
	int SetDefaultArg(asASTDeclId id, const char* text);
	int SetDeclConstantValue(asASTDeclId id, asQWORD value);
	int SetDeclByteOffset(asASTDeclId id, int byteOffset);
	int SetDeclLayout(asASTDeclId id, int byteSize, asUINT byteAlignment);
	int AddDeclInit(asASTDeclId id, asASTExprId expr);
	int PrependDeclInit(asASTDeclId id, asASTExprId expr);
	int SetDeclParent(asASTDeclId id, asASTDeclId parent);
	int SetDeclRange(asASTDeclId id, const asCSourceRange& range);
	int AddDeclChild(asASTDeclId parent, asASTDeclId child);
	int AddCapture(asASTDeclId lambda, asASTDeclId captured);
	int ClearDeclChildren(asASTDeclId id);
	int SetDeclKind(asASTDeclId id, asEASTDeclKind kind);
	int SetStmtRange(asASTStmtId id, const asCSourceRange& range);
	int SetStmtExpr(asASTStmtId id, asASTExprId expr);
	int SetTarget(asASTStmtId id, asASTStmtId target);
	int SetStmtSafePointRole(asASTStmtId id, asEASTSafePointRole role);
	int SetExprSafePointRole(asASTExprId id, asEASTSafePointRole role);
	int SetExprCallDispatch(asASTExprId id, asEASTCallDispatch dispatch);
	int SetStmtDecl(asASTStmtId id, asASTDeclId decl);
	int AddStmtChild(asASTStmtId parent, asASTStmtId child);
	int ClearStmtChildren(asASTStmtId id);
	int SetStmtId(asASTStmtId id, asASTStmtId newId);
	int AddExprChild(asASTExprId parent, asASTExprId child);
	int ClearExprChildren(asASTExprId id);
	int SetExprReceiver(asASTExprId id, asASTExprId receiver);
	int SetLiteral(asASTExprId id, const char* text);
	// Semantic string payloads are byte sequences, not C strings. This setter
	// preserves embedded NUL bytes and is also used by sidecar restoration.
	int SetLiteralBytes(asASTExprId id, const char* bytes, asUINT length);
	int SetLiteralBits(asASTExprId id, asQWORD bits);
	int SetResolvedDecl(asASTExprId id, asASTDeclId decl);
	int SetExprType(asASTExprId id, const asCQualType& type);
	int SetExprId(asASTExprId id, asASTExprId newId);
	// Pre-seal Sema recovery may replace a provisional expression after a later
	// declaration makes its exact type and callee available. Rewrite every
	// owning edge, while leaving the arena-allocated old node as an unreachable
	// recovery artifact.
	int ReplaceExprUses(asASTExprId oldExpr, asASTExprId replacement);

	const asCDecl* GetDecl(asASTDeclId id) const;
	const asCStmt* GetStmt(asASTStmtId id) const;
	const asCExpr* GetExpr(asASTExprId id) const;
	const asCType* GetType(asASTTypeRef id) const;

	bool ArenaContains(const void* ptr) const;
	asUINT GetArenaUsedBytes() const;

	asASTDeclId GetTranslationUnit() const { return translationUnit; }

	bool OwnsDecl(asASTDeclId id) const;
	bool OwnsStmt(asASTStmtId id) const;
	bool OwnsExpr(asASTExprId id) const;

	// Compatibility completion for callers that have already authored all
	// current Sema/lifetime facts. It still executes the three adjacent
	// transitions and is deliberately one-shot rather than idempotent.
	int Seal();
	int AdvanceLifecycle(
		asEASTContextLifecycleState expected,
		asEASTContextLifecycleState next);
	asEASTContextLifecycleState GetLifecycleState() const { return lifecycleState; }
	bool IsBuilding() const { return lifecycleState == asAST_CONTEXT_BUILDING; }
	bool IsPublishable() const { return lifecycleState == asAST_CONTEXT_FROZEN_PUBLISHABLE; }
	int SetLifetimeProtocolRevision(asDWORD revision);
	int AddLifetimeRecord(const asSASTLifetimeRecord& record);
	asDWORD GetLifetimeProtocolRevision() const { return lifetimeProtocolRevision; }
	asUINT GetLifetimeRecordCount() const { return lifetimeRecords.GetLength(); }
	const asSASTLifetimeRecord* GetLifetimeRecord(asUINT index) const;
	asQWORD GetLifetimeProtocolHash() const;
	int RejectPostSealMutation() const;
	// Cache DTO decoding starts from a fresh context. This internal recovery
	// helper makes a rejected DTO leave that context empty rather than exposing
	// a partially reconstructed graph to a later consumer.
	void ResetUnsealedForSidecarDecode();

	asUINT GetDeclCount() const { return decls.GetLength(); }
	asUINT GetStmtCount() const { return stmts.GetLength(); }
	asUINT GetExprCount() const { return exprs.GetLength(); }
	asUINT GetTypeCount() const { return types.GetLength(); }

private:
	asCASTContext(const asCASTContext&) = delete;
	asCASTContext& operator=(const asCASTContext&) = delete;
	asCASTContext(asCASTContext&&) = delete;
	asCASTContext& operator=(asCASTContext&&) = delete;

	void DestroyAll();
	void* Allocate(asUINT bytes, asUINT alignment);
	asCQualType InternType(asEASTTypeKind kind, eTokenType primitiveToken, const char* stableKey, asDWORD quals);
	asCSourceManager& MutableSourceManager();
	asCDecl* MutableDecl(asASTDeclId id);
	asCStmt* MutableStmt(asASTStmtId id);
	asCExpr* MutableExpr(asASTExprId id);

	struct asSArenaBlock
	{
		char* mem;
		asUINT used;
		asUINT cap;
	};

	asCSourceManager sourceManager;
	asCArray<asCDecl*> decls;
	asCArray<asCStmt*> stmts;
	asCArray<asCExpr*> exprs;
	asCArray<asCType*> types;
	asCArray<asSArenaBlock> arenaBlocks;
	asUINT arenaUsedBytes;
	asASTDeclId translationUnit;
	asEASTContextLifecycleState lifecycleState;
	asDWORD lifetimeProtocolRevision;
	asCArray<asSASTLifetimeRecord> lifetimeRecords;
};

END_AS_NAMESPACE

#endif
