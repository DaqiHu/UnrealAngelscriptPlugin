#ifndef AS_AST_PUBLIC_VIEW_H
#define AS_AST_PUBLIC_VIEW_H

#include "as_ast_context.h"
#include "as_atomic.h"

BEGIN_AS_NAMESPACE

class asCRuntimeTypeGeneration;
class asCScriptFunction;
struct asSResolvedRuntimeTypeBinding;

class ANGELSCRIPTRUNTIME_API asCASTSnapshot : public asIASTSnapshot
{
public:
	explicit asCASTSnapshot(
		asCASTContext* ownedContext,
		const char* moduleName,
		const char* generationKey,
		const char* provenance = "source-build",
		asCRuntimeTypeGeneration* runtimeGeneration = 0);
	virtual ~asCASTSnapshot();

	virtual int AddRef() const;
	virtual int Release() const;
	virtual asDWORD GetAPIVersion() const;
	virtual const char* GetModuleName() const;
	virtual const char* GetGenerationKey() const;
	virtual bool IsCurrentGeneration() const;
	virtual asASTDeclId GetTranslationUnitDecl() const;
	virtual int GetDecl(asASTDeclId id, asSASTDeclView* outView) const;
	virtual int GetStmt(asASTStmtId id, asSASTStmtView* outView) const;
	virtual int GetExpr(asASTExprId id, asSASTExprView* outView) const;
	virtual int GetType(asASTTypeRef id, asSASTTypeView* outView) const;
	virtual int GetDeclChild(asASTDeclId parent, asUINT childIndex, asASTDeclId* outChild) const;
	virtual int GetStmtChild(asASTStmtId parent, asUINT childIndex, asASTStmtId* outChild) const;
	virtual int GetExprChild(asASTExprId parent, asUINT childIndex, asASTExprId* outChild) const;
	virtual int GetSourceFile(asASTSourceFileId id, asSASTSourceFileView* outView) const;
	virtual int GetSourceLineColumn(asSASTSourceLocation location, asUINT* outLine, asUINT* outColumn) const;

	void SetCurrentGeneration(bool current);
	const asCASTContext* GetContext() const { return context; }
	const char* GetProvenance() const { return provenance.AddressOf(); }
	asDWORD GetReferenceCountForDiagnostics() const { return refCount.get(); }
	asUINT GetRuntimeTypeBindingCountForDiagnostics() const;
	const asSResolvedRuntimeTypeBinding*
		GetRuntimeTypeBindingForDiagnostics(asUINT index) const;
	bool IsRuntimeTypeGenerationRetiredForDiagnostics() const;
	asCScriptFunction* GetRetiredRuntimeFunctionByDeclForDiagnostics(
		const char* declaration) const;

private:
	asASTDeclId MakePublicDeclId(asASTDeclId internalId) const;
	asASTStmtId MakePublicStmtId(asASTStmtId internalId) const;
	asASTExprId MakePublicExprId(asASTExprId internalId) const;
	asASTTypeRef MakePublicTypeRef(asASTTypeRef internalId) const;
	asASTSourceFileId MakePublicSourceFileId(asASTFileID internalId) const;
	asSASTSourceLocation MakePublicSourceLocation(const asCSourceLocation& internalLocation) const;
	asSASTSourceRange MakePublicSourceRange(const asCSourceRange& internalRange) const;
	bool OwnsPublicId(asDWORD owner) const;

	mutable asCAtomic refCount;
	asCASTContext* context;
	asCString moduleName;
	asCString generationKey;
	asCString provenance;
	asCRuntimeTypeGeneration* runtimeGeneration;
	asDWORD snapshotOwner;
	mutable volatile int32 currentGeneration;
};

END_AS_NAMESPACE

#endif
