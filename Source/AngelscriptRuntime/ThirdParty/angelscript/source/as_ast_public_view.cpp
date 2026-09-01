#include "as_ast_public_view.h"
#include "as_memory.h"
#include "as_module.h"
#include "HAL/PlatformAtomics.h"

BEGIN_AS_NAMESPACE

namespace
{
	asCAtomic gNextSnapshotOwner;

	bool HasCompleteV1View(const asUINT suppliedSize, const size_t requiredSize)
	{
		return suppliedSize >= static_cast<asUINT>(requiredSize);
	}

	int ValidateV1ViewVersion(const asDWORD requestedVersion)
	{
		// Zero is the V1 convenience form for aggregate-initialized views. Once a
		// caller names a version, never silently reinterpret that request as V1.
		return requestedVersion == 0 || requestedVersion == asAST_API_VERSION_1
			? asSUCCESS
			: asNOT_SUPPORTED;
	}

	asDWORD AllocateSnapshotOwner()
	{
		asDWORD owner = gNextSnapshotOwner.atomicInc();
		if( owner == 0 )
			owner = gNextSnapshotOwner.atomicInc();
		return owner;
	}
}

asCASTSnapshot::asCASTSnapshot(
	asCASTContext* ownedContext,
	const char* inModuleName,
	const char* inGenerationKey,
	const char* inProvenance,
	asCRuntimeTypeGeneration* inRuntimeGeneration)
	: context(ownedContext)
	, runtimeGeneration(inRuntimeGeneration)
	, snapshotOwner(AllocateSnapshotOwner())
	, currentGeneration(1)
{
	refCount.set(1);
	if( inModuleName ) moduleName = inModuleName;
	if( inGenerationKey ) generationKey = inGenerationKey;
	provenance = inProvenance && inProvenance[0] ? inProvenance : "unknown";
	if( runtimeGeneration )
		runtimeGeneration->AddRef();
}

asCASTSnapshot::~asCASTSnapshot()
{
	if( context )
	{
		asDELETE(context, asCASTContext);
		context = 0;
	}
	if( runtimeGeneration )
	{
		runtimeGeneration->Release();
		runtimeGeneration = 0;
	}
}

int asCASTSnapshot::AddRef() const
{
	return refCount.atomicInc();
}

int asCASTSnapshot::Release() const
{
	const int r = refCount.atomicDec();
	if( r == 0 )
	{
		asDELETE(const_cast<asCASTSnapshot*>(this), asCASTSnapshot);
	}
	return r;
}

asDWORD asCASTSnapshot::GetAPIVersion() const
{
	return asAST_API_VERSION_1;
}

const char* asCASTSnapshot::GetModuleName() const
{
	return moduleName.AddressOf();
}

const char* asCASTSnapshot::GetGenerationKey() const
{
	return generationKey.AddressOf();
}

bool asCASTSnapshot::IsCurrentGeneration() const
{
	return FPlatformAtomics::InterlockedCompareExchange(&currentGeneration, 0, 0) != 0;
}

void asCASTSnapshot::SetCurrentGeneration(bool current)
{
	FPlatformAtomics::InterlockedExchange(&currentGeneration, current ? 1 : 0);
}

asUINT asCASTSnapshot::GetRuntimeTypeBindingCountForDiagnostics() const
{
	return runtimeGeneration
		? runtimeGeneration->GetBindings().GetBindingCount() : 0;
}

const asSResolvedRuntimeTypeBinding*
asCASTSnapshot::GetRuntimeTypeBindingForDiagnostics(asUINT index) const
{
	return runtimeGeneration
		? runtimeGeneration->GetBindings().GetBinding(index) : 0;
}

bool asCASTSnapshot::IsRuntimeTypeGenerationRetiredForDiagnostics() const
{
	return runtimeGeneration && runtimeGeneration->IsRetired();
}

asCScriptFunction*
asCASTSnapshot::GetRetiredRuntimeFunctionByDeclForDiagnostics(
	const char* declaration) const
{
	return runtimeGeneration
		? static_cast<asCScriptFunction*>(
			runtimeGeneration->GetRetiredFunctionByDecl(declaration))
		: 0;
}

asASTDeclId asCASTSnapshot::MakePublicDeclId(asASTDeclId internalId) const
{
	return internalId.IsValid() ? asASTDeclId(internalId.value, snapshotOwner) : asASTDeclId();
}

asASTStmtId asCASTSnapshot::MakePublicStmtId(asASTStmtId internalId) const
{
	return internalId.IsValid() ? asASTStmtId(internalId.value, snapshotOwner) : asASTStmtId();
}

asASTExprId asCASTSnapshot::MakePublicExprId(asASTExprId internalId) const
{
	return internalId.IsValid() ? asASTExprId(internalId.value, snapshotOwner) : asASTExprId();
}

asASTTypeRef asCASTSnapshot::MakePublicTypeRef(asASTTypeRef internalId) const
{
	return internalId.IsValid() ? asASTTypeRef(internalId.value, snapshotOwner) : asASTTypeRef();
}

asASTSourceFileId asCASTSnapshot::MakePublicSourceFileId(asASTFileID internalId) const
{
	return internalId != 0 ? asASTSourceFileId(internalId, snapshotOwner) : asASTSourceFileId();
}

asSASTSourceLocation asCASTSnapshot::MakePublicSourceLocation(const asCSourceLocation& internalLocation) const
{
	asSASTSourceLocation location = {};
	location.file = MakePublicSourceFileId(internalLocation.fileID);
	location.offset = internalLocation.offset;
	return location;
}

asSASTSourceRange asCASTSnapshot::MakePublicSourceRange(const asCSourceRange& internalRange) const
{
	asSASTSourceRange range = {};
	range.begin = MakePublicSourceLocation(internalRange.begin);
	range.end = MakePublicSourceLocation(internalRange.end);
	return range;
}

bool asCASTSnapshot::OwnsPublicId(asDWORD owner) const
{
	return owner != 0 && owner == snapshotOwner;
}

asASTDeclId asCASTSnapshot::GetTranslationUnitDecl() const
{
	return context ? MakePublicDeclId(context->GetTranslationUnit()) : asASTDeclId();
}

int asCASTSnapshot::GetDecl(asASTDeclId id, asSASTDeclView* outView) const
{
	if( context == 0 || outView == 0 )
	{
		return asINVALID_ARG;
	}
	// V1 declaration views are append-only. The original published layout ended
	// immediately before accessSpecifier; accept that capacity and write each
	// later semantic fact only when the caller explicitly supplies room for it.
	const asUINT suppliedSize = outView->structSize;
	const size_t originalV1Size = offsetof(asSASTDeclView, accessSpecifier);
	if( !HasCompleteV1View(suppliedSize, originalV1Size) )
	{
		return asINVALID_ARG;
	}
	if( !OwnsPublicId(id.snapshotOwner) )
	{
		return asINVALID_ARG;
	}
	const int viewRequest = ValidateV1ViewVersion(outView->apiVersion);
	if( viewRequest != asSUCCESS )
	{
		return viewRequest;
	}
	const asCDecl* decl = static_cast<const asCASTContext*>(context)->GetDecl(asASTDeclId(id.value));
	if( decl == 0 )
	{
		return asNO_FUNCTION;
	}
	outView->structSize = (asUINT)sizeof(asSASTDeclView);
	outView->apiVersion = asAST_API_VERSION_1;
	outView->kind = (asBYTE)decl->kind;
	outView->id = MakePublicDeclId(decl->id);
	outView->parent = MakePublicDeclId(decl->parent);
	outView->type = MakePublicTypeRef(decl->type.type);
	outView->name = decl->name.AddressOf();
	outView->stableKey = decl->stableKey.AddressOf();
	outView->sourceRange = MakePublicSourceRange(decl->range);
	outView->typeQuals = decl->type.quals;
	outView->body = MakePublicStmtId(decl->body);
	outView->traits = decl->traits;
	outView->accessorKind = decl->accessorKind;
	outView->accessorField = MakePublicDeclId(decl->accessorField);
	outView->childCount = decl->children.GetLength();
	if( HasCompleteV1View(
		suppliedSize, offsetof(asSASTDeclView, formalIndex)) )
	{
		outView->accessSpecifier = MakePublicDeclId(decl->accessSpecifier);
	}
	if( HasCompleteV1View(suppliedSize, sizeof(*outView)) )
	{
		outView->formalIndex = decl->formalIndex;
	}
	return asSUCCESS;
}

int asCASTSnapshot::GetStmt(asASTStmtId id, asSASTStmtView* outView) const
{
	if( context == 0 || outView == 0 )
	{
		return asINVALID_ARG;
	}
	if( !HasCompleteV1View(outView->structSize, sizeof(*outView)) )
	{
		return asINVALID_ARG;
	}
	if( !OwnsPublicId(id.snapshotOwner) )
	{
		return asINVALID_ARG;
	}
	const int viewRequest = ValidateV1ViewVersion(outView->apiVersion);
	if( viewRequest != asSUCCESS )
	{
		return viewRequest;
	}
	const asCStmt* stmt = static_cast<const asCASTContext*>(context)->GetStmt(asASTStmtId(id.value));
	if( stmt == 0 )
	{
		return asNO_FUNCTION;
	}
	outView->structSize = (asUINT)sizeof(asSASTStmtView);
	outView->apiVersion = asAST_API_VERSION_1;
	outView->kind = (asBYTE)stmt->kind;
	outView->id = MakePublicStmtId(stmt->id);
	outView->expr = MakePublicExprId(stmt->expr);
	outView->sourceRange = MakePublicSourceRange(stmt->range);
	outView->owner = MakePublicDeclId(stmt->owner);
	outView->target = MakePublicStmtId(stmt->target);
	outView->decl = MakePublicDeclId(stmt->decl);
	outView->safePointRole = stmt->safePointRole;
	outView->childCount = stmt->children.GetLength();
	return asSUCCESS;
}

int asCASTSnapshot::GetExpr(asASTExprId id, asSASTExprView* outView) const
{
	if( context == 0 || outView == 0 )
	{
		return asINVALID_ARG;
	}
	if( !HasCompleteV1View(outView->structSize, sizeof(*outView)) )
	{
		return asINVALID_ARG;
	}
	if( !OwnsPublicId(id.snapshotOwner) )
	{
		return asINVALID_ARG;
	}
	const int viewRequest = ValidateV1ViewVersion(outView->apiVersion);
	if( viewRequest != asSUCCESS )
	{
		return viewRequest;
	}
	const asCExpr* expr = static_cast<const asCASTContext*>(context)->GetExpr(asASTExprId(id.value));
	if( expr == 0 )
	{
		return asNO_FUNCTION;
	}
	outView->structSize = (asUINT)sizeof(asSASTExprView);
	outView->apiVersion = asAST_API_VERSION_1;
	outView->kind = (asBYTE)expr->kind;
	outView->id = MakePublicExprId(expr->id);
	outView->type = MakePublicTypeRef(expr->type.type);
	outView->quals = expr->type.quals;
	outView->literal = expr->literal.AddressOf();
	outView->sourceRange = MakePublicSourceRange(expr->range);
	outView->valueCategory = expr->valueCategory;
	outView->resolvedDecl = MakePublicDeclId(expr->resolvedDecl);
	outView->receiver = MakePublicExprId(expr->receiver);
	outView->literalBits = expr->literalBits;
	outView->safePointRole = expr->safePointRole;
	outView->childCount = expr->children.GetLength();
	return asSUCCESS;
}

int asCASTSnapshot::GetType(asASTTypeRef id, asSASTTypeView* outView) const
{
	if( context == 0 || outView == 0 )
	{
		return asINVALID_ARG;
	}
	if( !HasCompleteV1View(outView->structSize, sizeof(*outView)) )
	{
		return asINVALID_ARG;
	}
	if( !OwnsPublicId(id.snapshotOwner) )
	{
		return asINVALID_ARG;
	}
	const int viewRequest = ValidateV1ViewVersion(outView->apiVersion);
	if( viewRequest != asSUCCESS )
	{
		return viewRequest;
	}
	const asCType* type = context->GetType(asASTTypeRef(id.value));
	if( type == 0 )
	{
		return asNO_FUNCTION;
	}
	outView->structSize = (asUINT)sizeof(asSASTTypeView);
	outView->apiVersion = asAST_API_VERSION_1;
	outView->kind = (asBYTE)type->kind;
	outView->id = MakePublicTypeRef(type->id);
	outView->stableKey = type->stableKey.AddressOf();
	return asSUCCESS;
}

int asCASTSnapshot::GetDeclChild(asASTDeclId parent, asUINT childIndex, asASTDeclId* outChild) const
{
	if( context == 0 || outChild == 0 || !OwnsPublicId(parent.snapshotOwner) )
	{
		return asINVALID_ARG;
	}
	const asCDecl* decl = context->GetDecl(asASTDeclId(parent.value));
	if( decl == 0 )
	{
		return asNO_FUNCTION;
	}
	if( childIndex >= decl->children.GetLength() )
	{
		return asINVALID_ARG;
	}
	*outChild = MakePublicDeclId(decl->children[childIndex]);
	return asSUCCESS;
}

int asCASTSnapshot::GetStmtChild(asASTStmtId parent, asUINT childIndex, asASTStmtId* outChild) const
{
	if( context == 0 || outChild == 0 || !OwnsPublicId(parent.snapshotOwner) )
	{
		return asINVALID_ARG;
	}
	const asCStmt* stmt = context->GetStmt(asASTStmtId(parent.value));
	if( stmt == 0 )
	{
		return asNO_FUNCTION;
	}
	if( childIndex >= stmt->children.GetLength() )
	{
		return asINVALID_ARG;
	}
	*outChild = MakePublicStmtId(stmt->children[childIndex]);
	return asSUCCESS;
}

int asCASTSnapshot::GetExprChild(asASTExprId parent, asUINT childIndex, asASTExprId* outChild) const
{
	if( context == 0 || outChild == 0 || !OwnsPublicId(parent.snapshotOwner) )
	{
		return asINVALID_ARG;
	}
	const asCExpr* expr = context->GetExpr(asASTExprId(parent.value));
	if( expr == 0 )
	{
		return asNO_FUNCTION;
	}
	if( childIndex >= expr->children.GetLength() )
	{
		return asINVALID_ARG;
	}
	*outChild = MakePublicExprId(expr->children[childIndex]);
	return asSUCCESS;
}

int asCASTSnapshot::GetSourceFile(asASTSourceFileId id, asSASTSourceFileView* outView) const
{
	if( context == 0 || outView == 0 )
	{
		return asINVALID_ARG;
	}
	if( !HasCompleteV1View(outView->structSize, sizeof(*outView)) )
	{
		return asINVALID_ARG;
	}
	if( !OwnsPublicId(id.snapshotOwner) )
	{
		return asINVALID_ARG;
	}
	const int viewRequest = ValidateV1ViewVersion(outView->apiVersion);
	if( viewRequest != asSUCCESS )
	{
		return viewRequest;
	}
	const asCSourceManager& sourceManager = context->GetSourceManager();
	const asASTFileID internalId = id.value;
	const char* logicalKey = sourceManager.GetLogicalKey(internalId);
	if( logicalKey == 0 )
	{
		return asNO_FUNCTION;
	}
	outView->structSize = (asUINT)sizeof(asSASTSourceFileView);
	outView->apiVersion = asAST_API_VERSION_1;
	outView->id = MakePublicSourceFileId(internalId);
	outView->origin = sourceManager.GetOrigin(internalId);
	outView->lineOffset = sourceManager.GetLineOffset(internalId);
	outView->byteLength = sourceManager.GetBufferLength(internalId);
	outView->logicalKey = logicalKey;
	return asSUCCESS;
}

int asCASTSnapshot::GetSourceLineColumn(asSASTSourceLocation location, asUINT* outLine, asUINT* outColumn) const
{
	if( context == 0 || outLine == 0 || outColumn == 0 || !OwnsPublicId(location.file.snapshotOwner) )
	{
		return asINVALID_ARG;
	}
	asSSourceLineColumn lineColumn = {};
	if( !context->GetSourceManager().GetLineColumn(
		asCSourceLocation(location.file.value, location.offset), lineColumn) )
	{
		return asINVALID_ARG;
	}
	*outLine = (asUINT)lineColumn.row;
	*outColumn = (asUINT)lineColumn.column;
	return asSUCCESS;
}

END_AS_NAMESPACE
