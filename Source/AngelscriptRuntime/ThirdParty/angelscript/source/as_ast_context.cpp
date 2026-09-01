#include "as_ast_context.h"
#include "as_ast_verifier.h"
#include "as_memory.h"
#include <cstring>
#include <new>

BEGIN_AS_NAMESPACE

asCASTContext::asCASTContext()
	: arenaUsedBytes(0)
	, lifecycleState(asAST_CONTEXT_BUILDING)
	, lifetimeProtocolRevision(asAST_LIFETIME_PROTOCOL_REVISION)
{
}

asCASTContext::~asCASTContext()
{
	DestroyAll();
}

void* asCASTContext::Allocate(asUINT bytes, asUINT alignment)
{
	if( bytes == 0 )
	{
		bytes = 1;
	}
	if( alignment < 8 )
	{
		alignment = 8;
	}
	asUINT pad = alignment - 1;
	asUINT need = bytes + pad;
	asSArenaBlock* block = 0;
	if( arenaBlocks.GetLength() )
	{
		block = &arenaBlocks[arenaBlocks.GetLength() - 1];
		asUINT aligned = (block->used + pad) & ~pad;
		if( aligned + bytes > block->cap )
		{
			block = 0;
		}
	}
	if( block == 0 )
	{
		asUINT cap = need > 65536 ? need : 65536;
		asSArenaBlock next;
		next.mem = asNEWARRAY(char, cap);
		if( next.mem == 0 )
		{
			return 0;
		}
		next.used = 0;
		next.cap = cap;
		arenaBlocks.PushLast(next);
		block = &arenaBlocks[arenaBlocks.GetLength() - 1];
	}
	asUINT aligned = (block->used + pad) & ~pad;
	void* ptr = block->mem + aligned;
	block->used = aligned + bytes;
	arenaUsedBytes += bytes;
	return ptr;
}

bool asCASTContext::ArenaContains(const void* ptr) const
{
	if( ptr == 0 )
	{
		return false;
	}
	const char* p = static_cast<const char*>(ptr);
	for( asUINT i = 0; i < arenaBlocks.GetLength(); ++i )
	{
		const asSArenaBlock& block = arenaBlocks[i];
		if( p >= block.mem && p < block.mem + block.used )
		{
			return true;
		}
	}
	return false;
}

asUINT asCASTContext::GetArenaUsedBytes() const
{
	return arenaUsedBytes;
}

void asCASTContext::DestroyAll()
{
	for( asUINT i = 0; i < decls.GetLength(); ++i )
	{
		decls[i]->~asCDecl();
	}
	for( asUINT i = 0; i < stmts.GetLength(); ++i )
	{
		stmts[i]->~asCStmt();
	}
	for( asUINT i = 0; i < exprs.GetLength(); ++i )
	{
		exprs[i]->~asCExpr();
	}
	for( asUINT i = 0; i < types.GetLength(); ++i )
	{
		types[i]->~asCType();
	}
	for( asUINT i = 0; i < arenaBlocks.GetLength(); ++i )
	{
		asDELETEARRAY(arenaBlocks[i].mem);
	}
	arenaBlocks.SetLength(0);
	arenaUsedBytes = 0;
	decls.SetLength(0);
	stmts.SetLength(0);
	exprs.SetLength(0);
	types.SetLength(0);
	translationUnit = asASTDeclId();
	lifecycleState = asAST_CONTEXT_BUILDING;
	lifetimeProtocolRevision = asAST_LIFETIME_PROTOCOL_REVISION;
	lifetimeRecords.SetLength(0);
}

int asCASTContext::RejectPostSealMutation() const
{
	return IsBuilding() ? asAST_VERIFY_OK : asAST_VERIFY_POST_SEAL_MUTATION;
}

int asCASTContext::SetLifetimeProtocolRevision(asDWORD revision)
{
	if( !IsBuilding() )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	if( lifetimeRecords.GetLength() != 0 )
	{
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}
	lifetimeProtocolRevision = revision;
	return asAST_VERIFY_OK;
}

int asCASTContext::AddLifetimeRecord(
	const asSASTLifetimeRecord& record)
{
	if( !IsBuilding() )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	lifetimeRecords.PushLast(record);
	return asAST_VERIFY_OK;
}

const asSASTLifetimeRecord* asCASTContext::GetLifetimeRecord(
	asUINT index) const
{
	return index < lifetimeRecords.GetLength()
		? &lifetimeRecords[index] : 0;
}

asQWORD asCASTContext::GetLifetimeProtocolHash() const
{
	return asASTHashLifetimeProtocol(
		lifetimeProtocolRevision,
		lifetimeRecords.GetLength() != 0 ? &lifetimeRecords[0] : 0,
		lifetimeRecords.GetLength());
}

void asCASTContext::ResetForSidecarDecodeFailure()
{
	DestroyAll();
}

asASTDeclId asCASTContext::CreateTranslationUnit(const char* moduleName)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asASTDeclId();
	}
	if( translationUnit.IsValid() )
	{
		return translationUnit;
	}
	translationUnit = CreateDecl(asAST_DECL_TRANSLATION_UNIT, asASTDeclId(), asCSourceRange(), moduleName ? moduleName : "");
	return translationUnit;
}

asASTDeclId asCASTContext::CreateDecl(asEASTDeclKind kind, asASTDeclId parent, const asCSourceRange& range, const char* name)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asASTDeclId();
	}
	if( parent.IsValid() && GetDecl(parent) == 0 )
	{
		return asASTDeclId();
	}

	void* mem = Allocate(sizeof(asCDecl), alignof(asCDecl));
	if( mem == 0 )
	{
		return asASTDeclId();
	}
	asCDecl* decl = new (mem) asCDecl();
	decl->kind = kind;
	decl->parent = parent;
	decl->range = range;
	if( kind == asAST_DECL_PARAM && parent.IsValid() )
	{
		// Construction order is the Sema-authored formal order. Persist that
		// relation on the ParamDecl before children can be reordered for traversal.
		decl->formalIndex = GetFormalDeclCount(parent);
	}
	if( name )
	{
		decl->name = name;
		decl->stableKey = name;
	}
	decls.PushLast(decl);
	decl->id = asASTDeclId(decls.GetLength());

	if( parent.IsValid() )
	{
		asCDecl* parentDecl = MutableDecl(parent);
		if( parentDecl )
		{
			parentDecl->children.PushLast(decl->id);
		}
	}
	return decl->id;
}

asASTStmtId asCASTContext::CreateStmt(asEASTStmtKind kind, asASTDeclId owner, const asCSourceRange& range)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asASTStmtId();
	}
	void* mem = Allocate(sizeof(asCStmt), alignof(asCStmt));
	if( mem == 0 )
	{
		return asASTStmtId();
	}
	asCStmt* stmt = new (mem) asCStmt();
	stmt->kind = kind;
	stmt->owner = owner;
	stmt->range = range;
	stmts.PushLast(stmt);
	stmt->id = asASTStmtId(stmts.GetLength());
	return stmt->id;
}

asASTExprId asCASTContext::CreateExpr(asEASTExprKind kind, const asCQualType& type, asEASTValueCategory category, const asCSourceRange& range)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asASTExprId();
	}
	void* mem = Allocate(sizeof(asCExpr), alignof(asCExpr));
	if( mem == 0 )
	{
		return asASTExprId();
	}
	asCExpr* expr = new (mem) asCExpr();
	expr->kind = kind;
	expr->type = type;
	expr->valueCategory = category;
	expr->range = range;
	exprs.PushLast(expr);
	expr->id = asASTExprId(exprs.GetLength());
	return expr->id;
}

asCQualType asCASTContext::InternType(asEASTTypeKind kind, eTokenType primitiveToken, const char* stableKey, asDWORD quals)
{
	if( !asASTQualifiersAreValid(quals) )
	{
		return asCQualType();
	}
	if( kind == asAST_TYPE_VOID )
	{
		const asDWORD allowedOnVoid = asAST_QUAL_HANDLE;
		if( (quals & ~allowedOnVoid) != 0 )
		{
			return asCQualType();
		}
	}

	const char* key = stableKey ? stableKey : "";
	for( asUINT i = 0; i < types.GetLength(); ++i )
	{
		if( types[i]->kind == kind
			&& types[i]->primitiveToken == primitiveToken
			&& types[i]->stableKey.Equals(key) )
		{
			return asCQualType(types[i]->id, quals);
		}
	}

	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asCQualType();
	}

	void* mem = Allocate(sizeof(asCType), alignof(asCType));
	if( mem == 0 )
	{
		return asCQualType();
	}
	asCType* type = new (mem) asCType();
	type->kind = kind;
	type->primitiveToken = primitiveToken;
	type->stableKey = key;
	types.PushLast(type);
	type->id = asASTTypeRef(types.GetLength());
	return asCQualType(type->id, quals);
}

asCQualType asCASTContext::InternPrimitive(eTokenType primitiveToken, asDWORD quals)
{
	asCString key;
	switch( primitiveToken )
	{
	case ttVoid: key = "void"; break;
	case ttBool: key = "bool"; break;
	case ttInt: key = "int"; break;
	case ttInt8: key = "int8"; break;
	case ttInt16: key = "int16"; break;
	case ttInt64: key = "int64"; break;
	case ttUInt: key = "uint"; break;
	case ttUInt8: key = "uint8"; break;
	case ttUInt16: key = "uint16"; break;
	case ttUInt64: key = "uint64"; break;
	case ttFloat32:
		key = "float32";
		break;
	case ttFloat:
		key = "float";
		break;
	case ttDouble:
	case ttFloat64:
		key = "double";
		break;
	default: return asCQualType();
	}
	const asEASTTypeKind kind = (primitiveToken == ttVoid) ? asAST_TYPE_VOID : asAST_TYPE_PRIMITIVE;
	return InternType(kind, primitiveToken, key.AddressOf(), quals);
}

asCQualType asCASTContext::InternNamedType(asEASTTypeKind kind, const char* stableKey, asDWORD quals)
{
	if( stableKey == 0 || *stableKey == 0 )
	{
		return asCQualType();
	}
	return InternType(kind, ttUnrecognizedToken, stableKey, quals);
}

const asCDecl* asCASTContext::GetDecl(asASTDeclId id) const
{
	if( !id.IsValid() || id.snapshotOwner != 0 || id.value > decls.GetLength() )
	{
		return 0;
	}
	return decls[id.value - 1];
}

asUINT asCASTContext::GetFormalDeclCount(asASTDeclId owner) const
{
	const asCDecl* const declaration = GetDecl(owner);
	if( declaration == 0 )
	{
		return 0;
	}
	asUINT count = 0;
	for( asUINT index = 0; index < declaration->children.GetLength(); ++index )
	{
		const asCDecl* const child = GetDecl(declaration->children[index]);
		if( child != 0 && child->kind == asAST_DECL_PARAM )
		{
			++count;
		}
	}
	return count;
}

const asCDecl* asCASTContext::GetFormalDecl(
	asASTDeclId owner,
	asUINT formalIndex) const
{
	const asCDecl* const declaration = GetDecl(owner);
	if( declaration == 0 )
	{
		return 0;
	}
	const asCDecl* match = 0;
	for( asUINT index = 0; index < declaration->children.GetLength(); ++index )
	{
		const asCDecl* const child = GetDecl(declaration->children[index]);
		if( child == 0 || child->kind != asAST_DECL_PARAM
			|| child->formalIndex != formalIndex )
		{
			continue;
		}
		if( match != 0 )
		{
			return 0;
		}
		match = child;
	}
	return match;
}

const asCStmt* asCASTContext::GetStmt(asASTStmtId id) const
{
	if( !id.IsValid() || id.snapshotOwner != 0 || id.value > stmts.GetLength() )
	{
		return 0;
	}
	return stmts[id.value - 1];
}

const asCExpr* asCASTContext::GetExpr(asASTExprId id) const
{
	if( !id.IsValid() || id.snapshotOwner != 0 || id.value > exprs.GetLength() )
	{
		return 0;
	}
	return exprs[id.value - 1];
}

const asCType* asCASTContext::GetType(asASTTypeRef id) const
{
	// Public snapshot references carry an owner token and must be returned to
	// the snapshot that issued them. Internal AST construction uses owner zero;
	// accepting any other owner here would turn a coincident numeric index into
	// false cross-snapshot type identity.
	if( !id.IsValid() || id.snapshotOwner != 0 || id.value > types.GetLength() )
	{
		return 0;
	}
	return types[id.value - 1];
}

bool asCASTContext::OwnsDecl(asASTDeclId id) const
{
	return GetDecl(id) != 0;
}

bool asCASTContext::OwnsStmt(asASTStmtId id) const
{
	return GetStmt(id) != 0;
}

bool asCASTContext::OwnsExpr(asASTExprId id) const
{
	return GetExpr(id) != 0;
}

int asCASTContext::Seal()
{
	if( !IsBuilding() )
	{
		return asAST_VERIFY_INVALID_LIFECYCLE_TRANSITION;
	}

	asSAstVerifyResult result;
	const int status = asCASTVerify(*this, result);
	if( status != asAST_VERIFY_OK )
	{
		return status;
	}
	int transition = AdvanceLifecycle(
		asAST_CONTEXT_BUILDING,
		asAST_CONTEXT_SEMA_FINALIZED);
	if( transition != asAST_VERIFY_OK )
		return transition;
	transition = AdvanceLifecycle(
		asAST_CONTEXT_SEMA_FINALIZED,
		asAST_CONTEXT_LIFETIME_PLANNED);
	if( transition != asAST_VERIFY_OK )
		return transition;
	return AdvanceLifecycle(
		asAST_CONTEXT_LIFETIME_PLANNED,
		asAST_CONTEXT_FROZEN_PUBLISHABLE);
}

int asCASTContext::AdvanceLifecycle(
	asEASTContextLifecycleState expected,
	asEASTContextLifecycleState next)
{
	if( lifecycleState != expected
		|| static_cast<int>(next) != static_cast<int>(expected) + 1
		|| next > asAST_CONTEXT_FROZEN_PUBLISHABLE )
	{
		return asAST_VERIFY_INVALID_LIFECYCLE_TRANSITION;
	}

	if( next == asAST_CONTEXT_FROZEN_PUBLISHABLE )
	{
		asSAstVerifyResult result;
		const int status = asCASTVerify(*this, result);
		if( status != asAST_VERIFY_OK )
		{
			return status;
		}
	}
	else if( next == asAST_CONTEXT_LIFETIME_PLANNED )
	{
		const int status = asCASTVerifyLifetimeProtocol(*this);
		if( status != asAST_VERIFY_OK )
		{
			return status;
		}
	}

	lifecycleState = next;
	return asAST_VERIFY_OK;
}

asCSourceManager& asCASTContext::MutableSourceManager()
{
	return sourceManager;
}

asCDecl* asCASTContext::MutableDecl(asASTDeclId id)
{
	if( !IsBuilding() || !id.IsValid() || id.snapshotOwner != 0 || id.value > decls.GetLength() )
	{
		return 0;
	}
	return decls[id.value - 1];
}

asCStmt* asCASTContext::MutableStmt(asASTStmtId id)
{
	if( !IsBuilding() || !id.IsValid() || id.snapshotOwner != 0 || id.value > stmts.GetLength() )
	{
		return 0;
	}
	return stmts[id.value - 1];
}

asCExpr* asCASTContext::MutableExpr(asASTExprId id)
{
	if( !IsBuilding() || !id.IsValid() || id.snapshotOwner != 0 || id.value > exprs.GetLength() )
	{
		return 0;
	}
	return exprs[id.value - 1];
}

asCType* asCASTContext::MutableType(asASTTypeRef id)
{
	if( !IsBuilding() || !id.IsValid() || id.snapshotOwner != 0 || id.value > types.GetLength() )
	{
		return 0;
	}
	return types[id.value - 1];
}

int asCASTContext::SetTypeRequiresRuntimeValidation(asASTTypeRef id, bool required)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCType* type = MutableType(id);
	if( type == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	type->requiresRuntimeValidation = required;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetTypeTemplateArguments(
	asASTTypeRef id,
	const asCArray<asCQualType>& arguments)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCType* type = MutableType(id);
	if( type == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( type->templateArguments.GetLength() != 0 )
	{
		if( type->templateArguments.GetLength() != arguments.GetLength() )
		{
			return asAST_VERIFY_INVALID_TYPE;
		}
		for( asUINT index = 0; index < arguments.GetLength(); ++index )
		{
			if( type->templateArguments[index].type != arguments[index].type
				|| type->templateArguments[index].quals != arguments[index].quals )
			{
				return asAST_VERIFY_INVALID_TYPE;
			}
		}
		return asAST_VERIFY_OK;
	}
	for( asUINT index = 0; index < arguments.GetLength(); ++index )
	{
		if( !arguments[index].IsValid()
			|| GetType(arguments[index].type) == 0 )
		{
			return asAST_VERIFY_DANGLING_ID;
		}
		asSAstTemplateArgument argument;
		argument.type = arguments[index].type;
		argument.quals = arguments[index].quals;
		type->templateArguments.PushLast(argument);
	}
	return asAST_VERIFY_OK;
}

asASTFileID asCASTContext::AddSourceSection(
	const char* logicalKey,
	asEASTSourceOrigin origin,
	const char* bytes,
	size_t byteCount,
	int lineOffset,
	const asSSourceProvenanceRange* provenanceRanges,
	asUINT provenanceRangeCount)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return 0;
	}
	return MutableSourceManager().AddSection(
		logicalKey,
		origin,
		bytes,
		byteCount,
		lineOffset,
		provenanceRanges,
		provenanceRangeCount);
}

asASTFileID asCASTContext::RemapSourceSection(
	const char* logicalKey,
	asEASTSourceOrigin origin,
	const char* bytes,
	size_t byteCount,
	int lineOffset,
	const asSSourceProvenanceRange* provenanceRanges,
	asUINT provenanceRangeCount)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return 0;
	}
	return MutableSourceManager().RemapLogical(
		logicalKey,
		origin,
		bytes,
		byteCount,
		lineOffset,
		provenanceRanges,
		provenanceRangeCount);
}

int asCASTContext::SetDeclType(asASTDeclId id, const asCQualType& type)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->type = type;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetBody(asASTDeclId id, asASTStmtId body)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->body = body;
	return asAST_VERIFY_OK;
}

int asCASTContext::AddDeclTrait(asASTDeclId id, asDWORD traits)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->traits |= traits;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetDeclTraits(asASTDeclId id, asDWORD traits)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->traits = traits;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetDeclName(asASTDeclId id, const char* name)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->name = name ? name : "";
	return asAST_VERIFY_OK;
}

int asCASTContext::SetGeneratedAccessor(asASTDeclId id, asEASTAccessorKind kind, asASTDeclId field)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 || !field.IsValid() || GetDecl(field) == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->accessorKind = kind;
	decl->accessorField = field;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetDeclAccessSpecifier(
	asASTDeclId member, asASTDeclId specifier)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(member);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	// Keep construction permissive for verifier/fuzz coverage. Seal is the
	// authority for dangling, wrong-kind and cross-record target rejection.
	decl->accessSpecifier = specifier;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetStableKey(asASTDeclId id, const char* key)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->stableKey = key ? key : "";
	return asAST_VERIFY_OK;
}

int asCASTContext::AddDependency(asASTDeclId id, const char* key)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->dependencies.PushLast(key ? key : "");
	return asAST_VERIFY_OK;
}

int asCASTContext::ClearDeclDependencies(asASTDeclId id)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->dependencies.SetLength(0);
	return asAST_VERIFY_OK;
}

int asCASTContext::SetOrigin(asASTDeclId id, const char* origin)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->origin = origin ? origin : "";
	return asAST_VERIFY_OK;
}

int asCASTContext::AddBase(asASTDeclId id, asASTDeclId base)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->bases.PushLast(base);
	return asAST_VERIFY_OK;
}

int asCASTContext::AddMethodRelation(
	asASTDeclId owner,
	const asSASTMethodRelation& relation)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* const declaration = MutableDecl(owner);
	if( declaration == 0
		|| GetDecl(relation.implementation) == 0
		|| GetDecl(relation.requirement) == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( relation.kind != asAST_METHOD_RELATION_BASE_OVERRIDE
		&& relation.kind != asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION )
	{
		return asAST_VERIFY_WRONG_KIND;
	}
	if( relation.implementation == relation.requirement )
	{
		return asAST_VERIFY_INVALID_CHILD;
	}
	for( asUINT index = 0;
		index < declaration->methodRelations.GetLength(); ++index )
	{
		const asSASTMethodRelation& existing =
			declaration->methodRelations[index];
		if( existing.kind == relation.kind
			&& existing.implementation == relation.implementation
			&& existing.requirement == relation.requirement )
		{
			return asAST_VERIFY_INVALID_CHILD;
		}
	}
	declaration->methodRelations.PushLast(relation);
	return asAST_VERIFY_OK;
}

int asCASTContext::SetDefaultArg(asASTDeclId id, const char* text)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->defaultArg = text ? text : "";
	return asAST_VERIFY_OK;
}

int asCASTContext::SetDeclConstantValue(asASTDeclId id, asQWORD value)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->hasConstantValue = true;
	decl->constantValue = value;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetDeclFormalIndex(asASTDeclId id, asUINT formalIndex)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( decl->kind != asAST_DECL_PARAM
		|| formalIndex == asAST_FORMAL_INDEX_NONE )
	{
		return asAST_VERIFY_WRONG_KIND;
	}
	decl->formalIndex = formalIndex;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetDeclByteOffset(asASTDeclId id, int byteOffset)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->byteOffset = byteOffset;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetDeclLayout(asASTDeclId id, int byteSize, asUINT byteAlignment)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->byteSize = byteSize;
	decl->byteAlignment = byteAlignment;
	return asAST_VERIFY_OK;
}

int asCASTContext::AddDeclInit(asASTDeclId id, asASTExprId expr)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( !expr.IsValid() || GetExpr(expr) == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->inits.PushLast(expr);
	return asAST_VERIFY_OK;
}

int asCASTContext::PrependDeclInit(asASTDeclId id, asASTExprId expr)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( !expr.IsValid() || GetExpr(expr) == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	const asUINT oldLength = decl->inits.GetLength();
	decl->inits.PushLast(expr);
	if( decl->inits.GetLength() != oldLength + 1 )
	{
		return asOUT_OF_MEMORY;
	}
	for( asUINT index = oldLength; index > 0; --index )
	{
		decl->inits[index] = decl->inits[index - 1];
	}
	decl->inits[0] = expr;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetDeclParent(asASTDeclId id, asASTDeclId parent)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->parent = parent;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetDeclRange(asASTDeclId id, const asCSourceRange& range)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->range = range;
	return asAST_VERIFY_OK;
}

int asCASTContext::AddDeclChild(asASTDeclId parent, asASTDeclId child)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(parent);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->children.PushLast(child);
	return asAST_VERIFY_OK;
}

int asCASTContext::AddCapture(asASTDeclId lambda, asASTDeclId captured)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(lambda);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( !captured.IsValid() || GetDecl(captured) == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	for( asUINT i = 0; i < decl->captures.GetLength(); ++i )
	{
		if( decl->captures[i] == captured )
		{
			return asAST_VERIFY_OK;
		}
	}
	decl->captures.PushLast(captured);
	return asAST_VERIFY_OK;
}

int asCASTContext::ClearDeclChildren(asASTDeclId id)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->children.SetLength(0);
	return asAST_VERIFY_OK;
}

int asCASTContext::SetDeclKind(asASTDeclId id, asEASTDeclKind kind)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCDecl* decl = MutableDecl(id);
	if( decl == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	decl->kind = kind;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetStmtRange(asASTStmtId id, const asCSourceRange& range)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCStmt* stmt = MutableStmt(id);
	if( stmt == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	stmt->range = range;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetStmtExpr(asASTStmtId id, asASTExprId expr)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCStmt* stmt = MutableStmt(id);
	if( stmt == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	stmt->expr = expr;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetTarget(asASTStmtId id, asASTStmtId target)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCStmt* stmt = MutableStmt(id);
	if( stmt == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	stmt->target = target;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetStmtSafePointRole(asASTStmtId id, asEASTSafePointRole role)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCStmt* stmt = MutableStmt(id);
	if( stmt == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	stmt->safePointRole = role;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetExprSafePointRole(asASTExprId id, asEASTSafePointRole role)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* expr = MutableExpr(id);
	if( expr == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	expr->safePointRole = role;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetExprCallDispatch(asASTExprId id, asEASTCallDispatch dispatch)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* expr = MutableExpr(id);
	if( expr == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( expr->kind != asAST_EXPR_CALL
		&& expr->kind != asAST_EXPR_CONSTRUCT )
	{
		return asAST_VERIFY_WRONG_KIND;
	}
	if( dispatch > asAST_CALL_DISPATCH_INDIRECT )
	{
		return asAST_VERIFY_INVALID_CHILD;
	}
	expr->callDispatch = dispatch;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetStmtDecl(asASTStmtId id, asASTDeclId decl)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCStmt* stmt = MutableStmt(id);
	if( stmt == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	stmt->decl = decl;
	return asAST_VERIFY_OK;
}

int asCASTContext::AddStmtChild(asASTStmtId parent, asASTStmtId child)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCStmt* stmt = MutableStmt(parent);
	if( stmt == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	stmt->children.PushLast(child);
	return asAST_VERIFY_OK;
}

int asCASTContext::ClearStmtChildren(asASTStmtId id)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCStmt* stmt = MutableStmt(id);
	if( stmt == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	stmt->children.SetLength(0);
	return asAST_VERIFY_OK;
}

int asCASTContext::SetStmtId(asASTStmtId id, asASTStmtId newId)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCStmt* stmt = MutableStmt(id);
	if( stmt == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	stmt->id = newId;
	return asAST_VERIFY_OK;
}

int asCASTContext::AddExprChild(asASTExprId parent, asASTExprId child)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* expr = MutableExpr(parent);
	if( expr == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	expr->children.PushLast(child);
	return asAST_VERIFY_OK;
}

int asCASTContext::ClearExprChildren(asASTExprId id)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* expr = MutableExpr(id);
	if( expr == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	expr->children.SetLength(0);
	return asAST_VERIFY_OK;
}

int asCASTContext::AddCallArgument(
	asASTExprId call,
	const asSASTCallArgument& argument)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* const expr = MutableExpr(call);
	if( expr == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( expr->kind != asAST_EXPR_CALL
		&& expr->kind != asAST_EXPR_CONSTRUCT
		&& expr->kind != asAST_EXPR_CALL_REWRITE )
	{
		return asAST_VERIFY_WRONG_KIND;
	}
	if( !argument.expression.IsValid()
		|| GetExpr(argument.expression) == 0
		|| (argument.formal.IsValid() && GetDecl(argument.formal) == 0) )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	expr->callArguments.PushLast(argument);
	return asAST_VERIFY_OK;
}

int asCASTContext::ClearCallArguments(asASTExprId call)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* const expr = MutableExpr(call);
	if( expr == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( expr->kind != asAST_EXPR_CALL
		&& expr->kind != asAST_EXPR_CALL_REWRITE )
	{
		return asAST_VERIFY_WRONG_KIND;
	}
	expr->callArguments.SetLength(0);
	return asAST_VERIFY_OK;
}

int asCASTContext::ReplaceCallArgumentExpression(
	asASTExprId call,
	asUINT argumentIndex,
	asASTExprId replacement)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* const expr = MutableExpr(call);
	if( expr == 0 || !replacement.IsValid() || GetExpr(replacement) == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( expr->kind != asAST_EXPR_CALL
		&& expr->kind != asAST_EXPR_CONSTRUCT
		&& expr->kind != asAST_EXPR_CALL_REWRITE )
	{
		return asAST_VERIFY_WRONG_KIND;
	}
	if( argumentIndex >= expr->callArguments.GetLength()
		|| argumentIndex >= expr->children.GetLength() )
	{
		return asAST_VERIFY_INVALID_CHILD;
	}
	const asASTExprId previous =
		expr->callArguments[argumentIndex].expression;
	if( !previous.IsValid() || expr->children[argumentIndex] != previous
		|| replacement == expr->id )
	{
		return asAST_VERIFY_INVALID_CHILD;
	}
	// Validate the complete dual relation before mutating either edge. The two
	// assignments below are then one semantic operation while the context is
	// exclusively owned by the building Sema.
	expr->children[argumentIndex] = replacement;
	expr->callArguments[argumentIndex].expression = replacement;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetExprReceiver(asASTExprId id, asASTExprId receiver)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* expr = MutableExpr(id);
	if( expr == 0 || !receiver.IsValid() || GetExpr(receiver) == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( expr->kind != asAST_EXPR_CALL
		&& expr->kind != asAST_EXPR_DEFERRED_OUT
		&& expr->kind != asAST_EXPR_CALL_REWRITE )
	{
		return asAST_VERIFY_WRONG_KIND;
	}
	if( receiver == id )
	{
		return asAST_VERIFY_INVALID_CHILD;
	}
	expr->receiver = receiver;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetLiteral(asASTExprId id, const char* text)
{
	return SetLiteralBytes(id, text, text ? static_cast<asUINT>(std::strlen(text)) : 0);
}

int asCASTContext::SetLiteralBytes(asASTExprId id, const char* bytes, asUINT length)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* expr = MutableExpr(id);
	if( expr == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( bytes == 0 && length != 0 )
	{
		return asAST_VERIFY_INVALID_CHILD;
	}
	if( length != 0 )
	{
		expr->literal.Assign(bytes, length);
	}
	else
	{
		expr->literal = "";
	}
	return asAST_VERIFY_OK;
}

int asCASTContext::SetLiteralBits(asASTExprId id, asQWORD bits)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* expr = MutableExpr(id);
	if( expr == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	expr->literalBits = bits;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetResolvedDecl(asASTExprId id, asASTDeclId decl)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* expr = MutableExpr(id);
	if( expr == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	expr->resolvedDecl = decl;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetExprType(asASTExprId id, const asCQualType& type)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* expr = MutableExpr(id);
	if( expr == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	expr->type = type;
	return asAST_VERIFY_OK;
}

int asCASTContext::SetExprId(asASTExprId id, asASTExprId newId)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* expr = MutableExpr(id);
	if( expr == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	expr->id = newId;
	return asAST_VERIFY_OK;
}

int asCASTContext::TombstoneDiscardedExpr(asASTExprId id)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	asCExpr* expr = MutableExpr(id);
	if( expr == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	expr->kind = asAST_EXPR_ERROR;
	expr->valueCategory = asAST_VALUE_PRVALUE;
	expr->resolvedDecl = asASTDeclId();
	expr->receiver = asASTExprId();
	expr->literal = "discarded-semantic-subtree";
	expr->literalBits = 0;
	expr->safePointRole = asAST_SAFEPOINT_NONE;
	expr->callDispatch = asAST_CALL_DISPATCH_NONE;
	expr->children.SetLength(0);
	expr->callArguments.SetLength(0);
	return asAST_VERIFY_OK;
}

int asCASTContext::ReplaceExprUses(asASTExprId oldExpr, asASTExprId replacement)
{
	if( RejectPostSealMutation() != asAST_VERIFY_OK )
	{
		return asAST_VERIFY_POST_SEAL_MUTATION;
	}
	if( !oldExpr.IsValid() || !replacement.IsValid()
		|| GetExpr(oldExpr) == 0 || GetExpr(replacement) == 0 )
	{
		return asAST_VERIFY_DANGLING_ID;
	}
	if( oldExpr == replacement )
	{
		return asAST_VERIFY_OK;
	}

	for( asUINT i = 0; i < decls.GetLength(); ++i )
	{
		asCDecl* decl = decls[i];
		if( decl == 0 )
		{
			continue;
		}
		for( asUINT init = 0; init < decl->inits.GetLength(); ++init )
		{
			if( decl->inits[init] == oldExpr )
			{
				decl->inits[init] = replacement;
			}
		}
	}

	for( asUINT i = 0; i < stmts.GetLength(); ++i )
	{
		asCStmt* stmt = stmts[i];
		if( stmt && stmt->expr == oldExpr )
		{
			stmt->expr = replacement;
		}
	}

	for( asUINT i = 0; i < exprs.GetLength(); ++i )
	{
		asCExpr* expr = exprs[i];
		if( expr == 0 )
		{
			continue;
		}
		if( expr->receiver == oldExpr )
		{
			if( expr->id == replacement )
			{
				return asAST_VERIFY_INVALID_CHILD;
			}
			expr->receiver = replacement;
		}
		for( asUINT child = 0; child < expr->children.GetLength(); ++child )
		{
			if( expr->children[child] == oldExpr )
			{
				if( expr->id == replacement )
				{
					return asAST_VERIFY_INVALID_CHILD;
				}
				expr->children[child] = replacement;
			}
		}
		for( asUINT argumentIndex = 0;
			argumentIndex < expr->callArguments.GetLength();
			++argumentIndex )
		{
			asSASTCallArgument& argument =
				expr->callArguments[argumentIndex];
			if( argument.expression == oldExpr )
			{
				if( expr->id == replacement )
				{
					return asAST_VERIFY_INVALID_CHILD;
				}
				argument.expression = replacement;
			}
		}
	}
	return asAST_VERIFY_OK;
}

END_AS_NAMESPACE
