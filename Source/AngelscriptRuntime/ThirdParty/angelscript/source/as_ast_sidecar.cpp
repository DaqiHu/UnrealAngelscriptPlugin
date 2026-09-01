#include "as_ast_sidecar.h"
#include "as_ast_verifier.h"
#include <cstring>

BEGIN_AS_NAMESPACE

static const asBYTE kMagic[4] = { 'C', 'A', 'S', 'T' };
static const asUINT kVersion = asAST_SIDECAR_SCHEMA_VERSION;
static const asUINT kMaxBytes = 1024 * 1024;
static const asUINT kMaxNodes = 65536;

static void AppendU32(asCArray<asBYTE>& out, asUINT value)
{
	out.PushLast((asBYTE)(value & 0xff));
	out.PushLast((asBYTE)((value >> 8) & 0xff));
	out.PushLast((asBYTE)((value >> 16) & 0xff));
	out.PushLast((asBYTE)((value >> 24) & 0xff));
}

static void AppendBytes(asCArray<asBYTE>& out, const char* text)
{
	const asUINT n = text ? (asUINT)strlen(text) : 0;
	AppendU32(out, n);
	for( asUINT i = 0; i < n; ++i )
	{
		out.PushLast((asBYTE)text[i]);
	}
}

static void AppendRawBytes(asCArray<asBYTE>& out, const char* bytes, asUINT length)
{
	AppendU32(out, length);
	for( asUINT i = 0; i < length; ++i )
	{
		out.PushLast((asBYTE)bytes[i]);
	}
}

static void AppendU64(asCArray<asBYTE>& out, asQWORD value)
{
	for( asUINT i = 0; i < 8; ++i )
	{
		out.PushLast((asBYTE)((value >> (i * 8)) & 0xff));
	}
}

static bool RemainingAtLeast(asUINT offset, asUINT length, asUINT n)
{
	return offset <= length && n <= length - offset;
}

static bool ReadU32(const asBYTE* bytes, asUINT length, asUINT& offset, asUINT& value)
{
	if( !RemainingAtLeast(offset, length, 4) )
	{
		return false;
	}
	value = (asUINT)bytes[offset]
		| ((asUINT)bytes[offset + 1] << 8)
		| ((asUINT)bytes[offset + 2] << 16)
		| ((asUINT)bytes[offset + 3] << 24);
	offset += 4;
	return true;
}

static bool ReadBytes(const asBYTE* bytes, asUINT length, asUINT& offset, asCString& text)
{
	asUINT n = 0;
	if( !ReadU32(bytes, length, offset, n) || !RemainingAtLeast(offset, length, n) )
	{
		return false;
	}
	if( n )
	{
		text.Assign(reinterpret_cast<const char*>(bytes + offset), n);
	}
	else
	{
		text = "";
	}
	offset += n;
	return true;
}

static bool ReadU64(const asBYTE* bytes, asUINT length, asUINT& offset, asQWORD& value)
{
	if( !RemainingAtLeast(offset, length, 8) )
	{
		return false;
	}
	value = 0;
	for( asUINT i = 0; i < 8; ++i )
	{
		value |= ((asQWORD)bytes[offset + i]) << (i * 8);
	}
	offset += 8;
	return true;
}

static bool AppendLifetimeNodeId(
	asCArray<asBYTE>& out,
	const asSASTLifetimeNodeId& id)
{
	// Public snapshot tokens and pointers are never durable protocol identity.
	// The value is only an ordinal into the DTO graph serialized below.
	if( id.snapshotOwner != 0 )
	{
		return false;
	}
	AppendU32(out, static_cast<asUINT>(id.nodeClass));
	AppendU32(out, id.value);
	return true;
}

static bool ReadLifetimeNodeId(
	const asBYTE* bytes,
	asUINT length,
	asUINT& offset,
	asSASTLifetimeNodeId& id)
{
	asUINT nodeClass = 0;
	asUINT value = 0;
	if( !ReadU32(bytes, length, offset, nodeClass)
		|| !ReadU32(bytes, length, offset, value)
		|| nodeClass > static_cast<asUINT>(asAST_LIFETIME_NODE_EXPR) )
	{
		return false;
	}
	id = asSASTLifetimeNodeId(
		static_cast<asEASTLifetimeNodeClass>(nodeClass), value, 0);
	return true;
}

static bool AppendLifetimeRecord(
	asCArray<asBYTE>& out,
	const asSASTLifetimeRecord& record)
{
	AppendU32(out, static_cast<asUINT>(record.subjectKind));
	if( !AppendLifetimeNodeId(out, record.subject) )
		return false;
	AppendU32(out, static_cast<asUINT>(record.actionKind));
	if( !AppendLifetimeNodeId(out, record.actionTarget)
		|| !AppendLifetimeNodeId(out, record.activationPoint)
		|| !AppendLifetimeNodeId(out, record.semanticRegion) )
	{
		return false;
	}
	AppendU32(out, static_cast<asUINT>(record.phase));
	AppendU32(out, record.supportedExitMask);
	AppendU32(out, record.constructionStep);
	AppendU32(out, record.order);
	AppendU32(out, record.completeObjectCommit ? 1u : 0u);
	return true;
}

static bool ReadLifetimeRecord(
	const asBYTE* bytes,
	asUINT length,
	asUINT& offset,
	asSASTLifetimeRecord& record)
{
	asUINT subjectKind = 0;
	asUINT actionKind = 0;
	asUINT phase = 0;
	asUINT supportedExitMask = 0;
	asUINT completeObjectCommit = 0;
	if( !ReadU32(bytes, length, offset, subjectKind)
		|| subjectKind <= static_cast<asUINT>(asAST_LIFETIME_SUBJECT_INVALID)
		|| subjectKind > static_cast<asUINT>(asAST_LIFETIME_SUBJECT_DEFERRED_OUT)
		|| !ReadLifetimeNodeId(bytes, length, offset, record.subject)
		|| !ReadU32(bytes, length, offset, actionKind)
		|| actionKind <= static_cast<asUINT>(asAST_LIFETIME_ACTION_INVALID)
		|| actionKind > static_cast<asUINT>(asAST_LIFETIME_ACTION_DESTROY_ELEMENT)
		|| !ReadLifetimeNodeId(bytes, length, offset, record.actionTarget)
		|| !ReadLifetimeNodeId(bytes, length, offset, record.activationPoint)
		|| !ReadLifetimeNodeId(bytes, length, offset, record.semanticRegion)
		|| !ReadU32(bytes, length, offset, phase)
		|| phase <= static_cast<asUINT>(asAST_LIFETIME_PHASE_INVALID)
		|| phase > static_cast<asUINT>(asAST_LIFETIME_PHASE_AGGREGATE_ELEMENT)
		|| !ReadU32(bytes, length, offset, supportedExitMask)
		|| !ReadU32(bytes, length, offset, record.constructionStep)
		|| !ReadU32(bytes, length, offset, record.order)
		|| !ReadU32(bytes, length, offset, completeObjectCommit)
		|| completeObjectCommit > 1 )
	{
		return false;
	}
	record.subjectKind = static_cast<asEASTLifetimeSubjectKind>(subjectKind);
	record.actionKind = static_cast<asEASTLifetimeActionKind>(actionKind);
	record.phase = static_cast<asEASTLifetimePhase>(phase);
	record.supportedExitMask = static_cast<asDWORD>(supportedExitMask);
	record.completeObjectCommit = completeObjectCommit != 0;
	return true;
}

static void AppendRange(asCArray<asBYTE>& out, const asCSourceRange& range)
{
	AppendU32(out, range.begin.fileID);
	AppendU32(out, range.begin.offset);
	AppendU32(out, range.end.fileID);
	AppendU32(out, range.end.offset);
}

static bool ReadRange(const asBYTE* bytes, asUINT length, asUINT& offset, asCSourceRange& range)
{
	asUINT beginFile = 0;
	asUINT beginOffset = 0;
	asUINT endFile = 0;
	asUINT endOffset = 0;
	if( !ReadU32(bytes, length, offset, beginFile)
		|| !ReadU32(bytes, length, offset, beginOffset)
		|| !ReadU32(bytes, length, offset, endFile)
		|| !ReadU32(bytes, length, offset, endOffset) )
	{
		return false;
	}
	range = asCSourceRange(asCSourceLocation(beginFile, beginOffset), asCSourceLocation(endFile, endOffset));
	return true;
}

static void AppendSourceAnchor(
	asCArray<asBYTE>& out,
	const asSSourceAnchor& anchor)
{
	AppendBytes(out, anchor.section.AddressOf());
	AppendU32(out, anchor.sourceOffset);
	AppendU32(out, anchor.sourceLength);
	AppendU32(out, anchor.row);
	AppendU32(out, anchor.column);
}

static bool ReadSourceAnchor(
	const asBYTE* bytes,
	asUINT length,
	asUINT& offset,
	asSSourceAnchor& anchor)
{
	return ReadBytes(bytes, length, offset, anchor.section)
		&& ReadU32(bytes, length, offset, anchor.sourceOffset)
		&& ReadU32(bytes, length, offset, anchor.sourceLength)
		&& ReadU32(bytes, length, offset, anchor.row)
		&& ReadU32(bytes, length, offset, anchor.column);
}

static void AppendSourceProvenanceRange(
	asCArray<asBYTE>& out,
	const asSSourceProvenanceRange& range)
{
	AppendU32(out, range.processedOffset);
	AppendU32(out, range.processedLength);
	AppendU32(out, range.hasAuthoredOrigin ? 1 : 0);
	if( range.hasAuthoredOrigin )
	{
		AppendSourceAnchor(out, range.authoredOrigin);
	}
	AppendU32(out, range.hasGeneratedOrigin ? 1 : 0);
	if( range.hasGeneratedOrigin )
	{
		AppendU32(out, (asUINT)range.generatedOrigin.kind);
		AppendBytes(out, range.generatedOrigin.generatorName.AddressOf());
		AppendU32(out, range.generatedOrigin.hasAuthoredAnchor ? 1 : 0);
		if( range.generatedOrigin.hasAuthoredAnchor )
		{
			AppendSourceAnchor(out, range.generatedOrigin.authoredAnchor);
		}
	}
}

static bool ReadSourceProvenanceRange(
	const asBYTE* bytes,
	asUINT length,
	asUINT& offset,
	asSSourceProvenanceRange& range)
{
	asUINT hasAuthoredOrigin = 0;
	asUINT hasGeneratedOrigin = 0;
	if( !ReadU32(bytes, length, offset, range.processedOffset)
		|| !ReadU32(bytes, length, offset, range.processedLength)
		|| !ReadU32(bytes, length, offset, hasAuthoredOrigin)
		|| hasAuthoredOrigin > 1 )
	{
		return false;
	}
	range.hasAuthoredOrigin = hasAuthoredOrigin == 1;
	if( range.hasAuthoredOrigin
		&& !ReadSourceAnchor(bytes, length, offset, range.authoredOrigin) )
	{
		return false;
	}
	if( !ReadU32(bytes, length, offset, hasGeneratedOrigin)
		|| hasGeneratedOrigin > 1 )
	{
		return false;
	}
	range.hasGeneratedOrigin = hasGeneratedOrigin == 1;
	if( range.hasGeneratedOrigin )
	{
		asUINT kind = 0;
		asUINT hasAuthoredAnchor = 0;
		if( !ReadU32(bytes, length, offset, kind)
			|| kind > (asUINT)asEGeneratedSourceKind::RangeBasedFor
			|| !ReadBytes(
				bytes,
				length,
				offset,
				range.generatedOrigin.generatorName)
			|| !ReadU32(bytes, length, offset, hasAuthoredAnchor)
			|| hasAuthoredAnchor > 1 )
		{
			return false;
		}
		range.generatedOrigin.kind = (asEGeneratedSourceKind)kind;
		range.generatedOrigin.hasAuthoredAnchor = hasAuthoredAnchor == 1;
		if( range.generatedOrigin.hasAuthoredAnchor
			&& !ReadSourceAnchor(
				bytes,
				length,
				offset,
				range.generatedOrigin.authoredAnchor) )
		{
			return false;
		}
	}
	return true;
}

static void AppendIdArray(asCArray<asBYTE>& out, const asCArray<asASTDeclId>& values)
{
	AppendU32(out, values.GetLength());
	for( asUINT i = 0; i < values.GetLength(); ++i )
	{
		AppendU32(out, values[i].value);
	}
}

static void AppendStmtIdArray(asCArray<asBYTE>& out, const asCArray<asASTStmtId>& values)
{
	AppendU32(out, values.GetLength());
	for( asUINT i = 0; i < values.GetLength(); ++i )
	{
		AppendU32(out, values[i].value);
	}
}

static void AppendExprIdArray(asCArray<asBYTE>& out, const asCArray<asASTExprId>& values)
{
	AppendU32(out, values.GetLength());
	for( asUINT i = 0; i < values.GetLength(); ++i )
	{
		AppendU32(out, values[i].value);
	}
}

static void AppendStringArray(asCArray<asBYTE>& out, const asCArray<asCString>& values)
{
	AppendU32(out, values.GetLength());
	for( asUINT i = 0; i < values.GetLength(); ++i )
	{
		AppendBytes(out, values[i].AddressOf());
	}
}

static void AppendMethodRelationArray(
	asCArray<asBYTE>& out,
	const asCArray<asSASTMethodRelation>& relations)
{
	AppendU32(out, relations.GetLength());
	for( asUINT index = 0; index < relations.GetLength(); ++index )
	{
		AppendU32(out, static_cast<asUINT>(relations[index].kind));
		AppendU32(out, relations[index].implementation.value);
		AppendU32(out, relations[index].requirement.value);
	}
}

struct asSSidecarMethodRelationDTO
{
	asUINT kind;
	asUINT implementation;
	asUINT requirement;
};

struct asSSidecarDeclDTO
{
	asEASTDeclKind kind;
	asUINT parent;
	asCSourceRange range;
	asUINT type;
	asDWORD quals;
	asCString name;
	asCString stableKey;
	asCArray<asUINT> children;
	asCArray<asUINT> captures;
	asUINT body;
	asDWORD traits;
	asEASTAccessorKind accessorKind;
	asUINT accessorField;
	asUINT accessSpecifier;
	asCString origin;
	asCString defaultArg;
	asCArray<asCString> dependencies;
	asCArray<asUINT> bases;
	asCArray<asSSidecarMethodRelationDTO> methodRelations;
	asCArray<asUINT> inits;
	asUINT formalIndex;
	int byteOffset;
	int byteSize;
	asUINT byteAlignment;
	bool hasConstantValue;
	asQWORD constantValue;
};

struct asSSidecarStmtDTO
{
	asEASTStmtKind kind;
	asUINT owner;
	asCSourceRange range;
	asUINT target;
	asUINT expr;
	asUINT decl;
	asEASTSafePointRole safePointRole;
	asCArray<asUINT> children;
};

struct asSSidecarCallArgumentDTO
{
	asUINT expression;
	asUINT formal;
	asUINT formalType;
	asDWORD formalQuals;
	asCSourceRange range;
	asCString authoredName;
	asUINT formalIndex;
	asUINT sourceOrdinal;
	asEASTCallArgumentOrigin origin;

	asSSidecarCallArgumentDTO()
		: expression(0)
		, formal(0)
		, formalType(0)
		, formalQuals(0)
		, formalIndex(asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL)
		, sourceOrdinal(asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL)
		, origin(asAST_CALL_ARGUMENT_INVALID)
	{
	}
};

struct asSSidecarExprDTO
{
	asEASTExprKind kind;
	asCSourceRange range;
	asUINT type;
	asDWORD quals;
	asEASTValueCategory valueCategory;
	asUINT resolvedDecl;
	asUINT receiver;
	asCString literal;
	asQWORD literalBits;
	asEASTSafePointRole safePointRole;
	asEASTCallDispatch callDispatch;
	asCArray<asUINT> children;
	asCArray<asSSidecarCallArgumentDTO> callArguments;
};

static bool IsAbsentOrOwnedRange(
	const asCSourceManager& sources,
	const asCSourceRange& range)
{
	return (!range.begin.IsValid() && !range.end.IsValid())
		|| sources.IsValidRange(range);
}

static bool SupportsCallArgumentRelation(
	asEASTExprKind kind,
	const asCString& literal)
{
	return kind == asAST_EXPR_CALL
		|| (kind == asAST_EXPR_CONSTRUCT
			&& !literal.Equals("list-pattern"));
}

static int AppendCallArgumentArray(
	const asCASTContext& context,
	const asCExpr& expr,
	asCArray<asBYTE>& out,
	asUINT& totalCallArgumentCount)
{
	const asUINT count = expr.callArguments.GetLength();
	if( count > kMaxNodes || totalCallArgumentCount > kMaxNodes - count )
	{
		return asAST_SIDECAR_BUDGET;
	}
	if( !SupportsCallArgumentRelation(expr.kind, expr.literal) && count != 0 )
	{
		return asAST_SIDECAR_MALFORMED;
	}
	totalCallArgumentCount += count;
	AppendU32(out, count);
	for( asUINT index = 0; index < count; ++index )
	{
		const asSASTCallArgument& argument = expr.callArguments[index];
		if( argument.expression.snapshotOwner != 0
			|| argument.formal.snapshotOwner != 0
			|| argument.formalType.type.snapshotOwner != 0 )
		{
			return asAST_SIDECAR_POINTER;
		}
		if( !argument.expression.IsValid()
			|| context.GetExpr(argument.expression) == 0
			|| (argument.formal.IsValid()
				&& context.GetDecl(argument.formal) == 0)
			|| !argument.formalType.IsValid()
			|| context.GetType(argument.formalType.type) == 0
			|| !asASTQualifiersAreValid(argument.formalType.quals)
			|| !IsAbsentOrOwnedRange(
				context.GetSourceManager(), argument.range)
			|| argument.origin <= asAST_CALL_ARGUMENT_INVALID
			|| argument.origin > asAST_CALL_ARGUMENT_GENERATED )
		{
			return asAST_SIDECAR_MALFORMED;
		}

		AppendU32(out, argument.expression.value);
		AppendU32(out, argument.formal.value);
		AppendU32(out, argument.formalType.type.value);
		AppendU32(out, argument.formalType.quals);
		AppendRange(out, argument.range);
		AppendBytes(out, argument.authoredName.AddressOf());
		AppendU32(out, argument.formalIndex);
		AppendU32(out, argument.sourceOrdinal);
		AppendU32(out, static_cast<asUINT>(argument.origin));
	}
	return asAST_SIDECAR_OK;
}

static bool ReadIdArray(const asBYTE* bytes, asUINT length, asUINT& offset, asCArray<asUINT>& values)
{
	asUINT count = 0;
	if( !ReadU32(bytes, length, offset, count) || count > kMaxNodes )
	{
		return false;
	}
	values.SetLength(0);
	for( asUINT i = 0; i < count; ++i )
	{
		asUINT value = 0;
		if( !ReadU32(bytes, length, offset, value) )
		{
			return false;
		}
		values.PushLast(value);
	}
	return true;
}

static int ReadMethodRelationArray(
	const asBYTE* bytes,
	asUINT length,
	asUINT& offset,
	asCArray<asSSidecarMethodRelationDTO>& relations)
{
	asUINT count = 0;
	if( !ReadU32(bytes, length, offset, count) )
	{
		return asAST_SIDECAR_MALFORMED;
	}
	if( count > kMaxNodes )
	{
		return asAST_SIDECAR_BUDGET;
	}
	relations.SetLength(0);
	for( asUINT index = 0; index < count; ++index )
	{
		asSSidecarMethodRelationDTO relation;
		if( !ReadU32(bytes, length, offset, relation.kind)
			|| !ReadU32(bytes, length, offset, relation.implementation)
			|| !ReadU32(bytes, length, offset, relation.requirement)
			|| (relation.kind != asAST_METHOD_RELATION_BASE_OVERRIDE
				&& relation.kind != asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION) )
		{
			return asAST_SIDECAR_MALFORMED;
		}
		relations.PushLast(relation);
	}
	return asAST_SIDECAR_OK;
}

static bool ReadStringArray(const asBYTE* bytes, asUINT length, asUINT& offset, asCArray<asCString>& values)
{
	asUINT count = 0;
	if( !ReadU32(bytes, length, offset, count) || count > kMaxNodes )
	{
		return false;
	}
	values.SetLength(0);
	for( asUINT i = 0; i < count; ++i )
	{
		asCString value;
		if( !ReadBytes(bytes, length, offset, value) )
		{
			return false;
		}
		values.PushLast(value);
	}
	return true;
}

int asCASTEncodeSidecar(const asCASTContext& context, const asSASTSidecarHeader& header, asCArray<asBYTE>& out)
{
	out.SetLength(0);
	if( header.functionKey.GetLength() == 0 )
	{
		return asAST_SIDECAR_FUNCTION_KEY_MISMATCH;
	}
	if( header.version != kVersion )
	{
		return asAST_SIDECAR_UNKNOWN_KIND;
	}
	if( !context.IsPublishable() || !context.GetTranslationUnit().IsValid()
		|| context.GetDeclCount() == 0 || context.GetDeclCount() > kMaxNodes
		|| context.GetStmtCount() > kMaxNodes || context.GetExprCount() > kMaxNodes
		|| context.GetTypeCount() > kMaxNodes )
	{
		return asAST_SIDECAR_MALFORMED;
	}
	asSAstVerifyResult publication;
	if( asCASTVerifyPublication(context, publication) != asAST_VERIFY_OK )
	{
		return asAST_SIDECAR_MALFORMED;
	}

	for( asUINT i = 0; i < 4; ++i )
	{
		out.PushLast(kMagic[i]);
	}
	AppendU32(out, kVersion);
	AppendBytes(out, header.functionKey.AddressOf());
	AppendBytes(out, header.profile.AddressOf());

	const asCSourceManager& sources = context.GetSourceManager();
	AppendU32(out, sources.GetSectionCount());
	for( asUINT i = 0; i < sources.GetSectionCount(); ++i )
	{
		const asASTFileID file = sources.GetFileIDByIndex(i);
		const char* buffer = sources.GetBuffer(file);
		const asUINT bufferLength = sources.GetBufferLength(file);
		AppendBytes(out, sources.GetLogicalKey(file));
		AppendU32(out, (asUINT)sources.GetOrigin(file));
		AppendU32(out, (asUINT)sources.GetLineOffset(file));
		AppendRawBytes(out, buffer ? buffer : "", bufferLength);
		const asUINT provenanceRangeCount =
			sources.GetSourceProvenanceRangeCount(file);
		if( provenanceRangeCount > kMaxNodes )
		{
			out.SetLength(0);
			return asAST_SIDECAR_BUDGET;
		}
		AppendU32(out, provenanceRangeCount);
		for( asUINT rangeIndex = 0;
			rangeIndex < provenanceRangeCount;
			++rangeIndex )
		{
			asSSourceProvenanceRange range;
			if( !sources.GetSourceProvenanceRange(
				file,
				rangeIndex,
				range) )
			{
				out.SetLength(0);
				return asAST_SIDECAR_MALFORMED;
			}
			AppendSourceProvenanceRange(out, range);
		}
	}

	AppendU32(out, context.GetTypeCount());
	for( asUINT i = 1; i <= context.GetTypeCount(); ++i )
	{
		const asCType* type = context.GetType(asASTTypeRef(i));
		if( type == 0 )
		{
			out.SetLength(0);
			return asAST_SIDECAR_MALFORMED;
		}
		AppendU32(out, (asUINT)type->kind);
		AppendU32(out, (asUINT)type->primitiveToken);
		AppendBytes(out, type->stableKey.AddressOf());
	}

	AppendU32(out, context.GetDeclCount());
	asUINT totalMethodRelationCount = 0;
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( decl == 0 || decl->id.value != i )
		{
			out.SetLength(0);
			return asAST_SIDECAR_MALFORMED;
		}
		if( decl->methodRelations.GetLength() > kMaxNodes
			|| totalMethodRelationCount
				> kMaxNodes - decl->methodRelations.GetLength() )
		{
			out.SetLength(0);
			return asAST_SIDECAR_BUDGET;
		}
		totalMethodRelationCount += decl->methodRelations.GetLength();
		for( asUINT relationIndex = 0;
			relationIndex < decl->methodRelations.GetLength();
			++relationIndex )
		{
			const asSASTMethodRelation& relation =
				decl->methodRelations[relationIndex];
			if( relation.implementation.snapshotOwner != 0
				|| relation.requirement.snapshotOwner != 0 )
			{
				out.SetLength(0);
				return asAST_SIDECAR_POINTER;
			}
			if( (relation.kind != asAST_METHOD_RELATION_BASE_OVERRIDE
					&& relation.kind != asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION)
				|| relation.implementation == relation.requirement
				|| context.GetDecl(relation.implementation) == 0
				|| context.GetDecl(relation.requirement) == 0 )
			{
				out.SetLength(0);
				return asAST_SIDECAR_MALFORMED;
			}
		}
		AppendU32(out, (asUINT)decl->kind);
		AppendU32(out, decl->parent.value);
		AppendRange(out, decl->range);
		AppendU32(out, decl->type.type.value);
		AppendU32(out, decl->type.quals);
		AppendBytes(out, decl->name.AddressOf());
		AppendBytes(out, decl->stableKey.AddressOf());
		AppendIdArray(out, decl->children);
		AppendIdArray(out, decl->captures);
		AppendU32(out, decl->body.value);
		AppendU32(out, decl->traits);
		AppendU32(out, (asUINT)decl->accessorKind);
		AppendU32(out, decl->accessorField.value);
		AppendU32(out, decl->accessSpecifier.value);
		AppendBytes(out, decl->origin.AddressOf());
		AppendBytes(out, decl->defaultArg.AddressOf());
		AppendStringArray(out, decl->dependencies);
		AppendIdArray(out, decl->bases);
		AppendExprIdArray(out, decl->inits);
		AppendU32(out, (asUINT)decl->byteOffset);
		AppendU32(out, (asUINT)decl->byteSize);
		AppendU32(out, decl->byteAlignment);
		AppendU32(out, decl->hasConstantValue ? 1 : 0);
		AppendU64(out, decl->constantValue);
		AppendMethodRelationArray(out, decl->methodRelations);
		AppendU32(out, decl->formalIndex);
	}

	AppendU32(out, context.GetStmtCount());
	for( asUINT i = 1; i <= context.GetStmtCount(); ++i )
	{
		const asCStmt* stmt = context.GetStmt(asASTStmtId(i));
		if( stmt == 0 || stmt->id.value != i )
		{
			out.SetLength(0);
			return asAST_SIDECAR_MALFORMED;
		}
		AppendU32(out, (asUINT)stmt->kind);
		AppendU32(out, stmt->owner.value);
		AppendRange(out, stmt->range);
		AppendU32(out, stmt->target.value);
		AppendU32(out, stmt->expr.value);
		AppendU32(out, stmt->decl.value);
		AppendU32(out, (asUINT)stmt->safePointRole);
		AppendStmtIdArray(out, stmt->children);
	}

	AppendU32(out, context.GetExprCount());
	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asCExpr* expr = context.GetExpr(asASTExprId(i));
		if( expr == 0 || expr->id.value != i )
		{
			out.SetLength(0);
			return asAST_SIDECAR_MALFORMED;
		}
		AppendU32(out, (asUINT)expr->kind);
		AppendRange(out, expr->range);
		AppendU32(out, expr->type.type.value);
		AppendU32(out, expr->type.quals);
		AppendU32(out, (asUINT)expr->valueCategory);
		AppendU32(out, expr->resolvedDecl.value);
		AppendU32(out, expr->receiver.value);
		AppendRawBytes(out, expr->literal.AddressOf(),
			static_cast<asUINT>(expr->literal.GetLength()));
		AppendU64(out, expr->literalBits);
		AppendU32(out, (asUINT)expr->safePointRole);
		AppendU32(out, (asUINT)expr->callDispatch);
		AppendExprIdArray(out, expr->children);
	}

	if( context.GetLifetimeRecordCount() > kMaxNodes )
	{
		out.SetLength(0);
		return asAST_SIDECAR_BUDGET;
	}
	AppendU32(out, context.GetLifetimeProtocolRevision());
	AppendU32(out, context.GetLifetimeRecordCount());
	for( asUINT i = 0; i < context.GetLifetimeRecordCount(); ++i )
	{
		const asSASTLifetimeRecord* const record =
			context.GetLifetimeRecord(i);
		if( record == 0 || !AppendLifetimeRecord(out, *record) )
		{
			out.SetLength(0);
			return asAST_SIDECAR_POINTER;
		}
	}

	// V9 is append-only relative to V8. CallArgument records follow the complete
	// lifetime protocol so a previous schema is an ordinary version miss and no
	// decoder can mistake argument authority for a legacy expression field.
	asUINT totalCallArgumentCount = 0;
	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asCExpr* const expr = context.GetExpr(asASTExprId(i));
		if( expr == 0 )
		{
			out.SetLength(0);
			return asAST_SIDECAR_MALFORMED;
		}
		const int callArgumentStatus = AppendCallArgumentArray(
			context, *expr, out, totalCallArgumentCount);
		if( callArgumentStatus != asAST_SIDECAR_OK )
		{
			out.SetLength(0);
			return callArgumentStatus;
		}
	}

	if( out.GetLength() > kMaxBytes )
	{
		out.SetLength(0);
		return asAST_SIDECAR_BUDGET;
	}
	return asAST_SIDECAR_OK;
}

static int DecodeFail(asCASTContext& context, int status)
{
	context.ResetForSidecarDecodeFailure();
	return status;
}

static bool ReadDeclDTO(
	const asBYTE* bytes,
	asUINT length,
	asUINT& offset,
	asSSidecarDeclDTO& out,
	int& outStatus)
{
	outStatus = asAST_SIDECAR_MALFORMED;
	asUINT kind = 0;
	asUINT quals = 0;
	asUINT traits = 0;
	asUINT accessorKind = 0;
	asUINT byteOffset = 0;
	asUINT byteSize = 0;
	asUINT byteAlignment = 0;
	asUINT hasConstantValue = 0;
	if( !ReadU32(bytes, length, offset, kind)
		|| !ReadU32(bytes, length, offset, out.parent)
		|| !ReadRange(bytes, length, offset, out.range)
		|| !ReadU32(bytes, length, offset, out.type)
		|| !ReadU32(bytes, length, offset, quals)
		|| !ReadBytes(bytes, length, offset, out.name)
		|| !ReadBytes(bytes, length, offset, out.stableKey)
		|| !ReadIdArray(bytes, length, offset, out.children)
		|| !ReadIdArray(bytes, length, offset, out.captures)
		|| !ReadU32(bytes, length, offset, out.body)
		|| !ReadU32(bytes, length, offset, traits)
		|| !ReadU32(bytes, length, offset, accessorKind)
		|| !ReadU32(bytes, length, offset, out.accessorField)
		|| !ReadU32(bytes, length, offset, out.accessSpecifier)
		|| !ReadBytes(bytes, length, offset, out.origin)
		|| !ReadBytes(bytes, length, offset, out.defaultArg)
		|| !ReadStringArray(bytes, length, offset, out.dependencies)
		|| !ReadIdArray(bytes, length, offset, out.bases)
		|| !ReadIdArray(bytes, length, offset, out.inits)
		|| !ReadU32(bytes, length, offset, byteOffset)
		|| !ReadU32(bytes, length, offset, byteSize)
		|| !ReadU32(bytes, length, offset, byteAlignment)
		|| !ReadU32(bytes, length, offset, hasConstantValue)
		|| !ReadU64(bytes, length, offset, out.constantValue) )
	{
		return false;
	}
	outStatus = ReadMethodRelationArray(
		bytes, length, offset, out.methodRelations);
	if( outStatus != asAST_SIDECAR_OK )
	{
		return false;
	}
	if( !ReadU32(bytes, length, offset, out.formalIndex) )
	{
		outStatus = asAST_SIDECAR_MALFORMED;
		return false;
	}
	out.kind = (asEASTDeclKind)kind;
	out.quals = quals;
	out.traits = traits;
	out.accessorKind = (asEASTAccessorKind)accessorKind;
	out.byteOffset = (int)byteOffset;
	out.byteSize = (int)byteSize;
	out.byteAlignment = byteAlignment;
	out.hasConstantValue = hasConstantValue == 1;
	if( hasConstantValue > 1 )
	{
		outStatus = asAST_SIDECAR_MALFORMED;
		return false;
	}
	outStatus = asAST_SIDECAR_OK;
	return true;
}

static bool ReadStmtDTO(const asBYTE* bytes, asUINT length, asUINT& offset, asSSidecarStmtDTO& out)
{
	asUINT kind = 0;
	asUINT safePointRole = 0;
	if( !ReadU32(bytes, length, offset, kind)
		|| !ReadU32(bytes, length, offset, out.owner)
		|| !ReadRange(bytes, length, offset, out.range)
		|| !ReadU32(bytes, length, offset, out.target)
		|| !ReadU32(bytes, length, offset, out.expr)
		|| !ReadU32(bytes, length, offset, out.decl)
		|| !ReadU32(bytes, length, offset, safePointRole)
		|| !ReadIdArray(bytes, length, offset, out.children) )
	{
		return false;
	}
	out.kind = (asEASTStmtKind)kind;
	out.safePointRole = (asEASTSafePointRole)safePointRole;
	return true;
}

static bool ReadExprDTO(const asBYTE* bytes, asUINT length, asUINT& offset, asSSidecarExprDTO& out)
{
	asUINT kind = 0;
	asUINT quals = 0;
	asUINT valueCategory = 0;
	asUINT safePointRole = 0;
	asUINT callDispatch = 0;
	if( !ReadU32(bytes, length, offset, kind)
		|| !ReadRange(bytes, length, offset, out.range)
		|| !ReadU32(bytes, length, offset, out.type)
		|| !ReadU32(bytes, length, offset, quals)
		|| !ReadU32(bytes, length, offset, valueCategory)
		|| !ReadU32(bytes, length, offset, out.resolvedDecl)
		|| !ReadU32(bytes, length, offset, out.receiver)
		|| !ReadBytes(bytes, length, offset, out.literal)
		|| !ReadU64(bytes, length, offset, out.literalBits)
		|| !ReadU32(bytes, length, offset, safePointRole)
		|| !ReadU32(bytes, length, offset, callDispatch)
		|| !ReadIdArray(bytes, length, offset, out.children) )
	{
		return false;
	}
	out.kind = (asEASTExprKind)kind;
	out.quals = quals;
	out.valueCategory = (asEASTValueCategory)valueCategory;
	out.safePointRole = (asEASTSafePointRole)safePointRole;
	out.callDispatch = (asEASTCallDispatch)callDispatch;
	return callDispatch <= asAST_CALL_DISPATCH_INDIRECT;
}

static bool IsValidDeclRef(asUINT id, asUINT count)
{
	return id == 0 || id <= count;
}

static bool IsValidStmtRef(asUINT id, asUINT count)
{
	return id == 0 || id <= count;
}

static bool IsValidExprRef(asUINT id, asUINT count)
{
	return id == 0 || id <= count;
}

static int ReadCallArgumentArray(
	const asBYTE* bytes,
	asUINT length,
	asUINT& offset,
	const asCSourceManager& sources,
	asUINT declCount,
	asUINT exprCount,
	asUINT typeCount,
	asSSidecarExprDTO& expr,
	asUINT& totalCallArgumentCount)
{
	asUINT count = 0;
	if( !ReadU32(bytes, length, offset, count) )
	{
		return asAST_SIDECAR_MALFORMED;
	}
	if( count > kMaxNodes || totalCallArgumentCount > kMaxNodes - count )
	{
		return asAST_SIDECAR_BUDGET;
	}
	if( !SupportsCallArgumentRelation(expr.kind, expr.literal) && count != 0 )
	{
		return asAST_SIDECAR_MALFORMED;
	}
	totalCallArgumentCount += count;
	expr.callArguments.SetLength(0);
	for( asUINT index = 0; index < count; ++index )
	{
		asSSidecarCallArgumentDTO argument;
		asUINT formalQuals = 0;
		asUINT origin = 0;
		if( !ReadU32(bytes, length, offset, argument.expression)
			|| !ReadU32(bytes, length, offset, argument.formal)
			|| !ReadU32(bytes, length, offset, argument.formalType)
			|| !ReadU32(bytes, length, offset, formalQuals)
			|| !ReadRange(bytes, length, offset, argument.range)
			|| !ReadBytes(bytes, length, offset, argument.authoredName)
			|| !ReadU32(bytes, length, offset, argument.formalIndex)
			|| !ReadU32(bytes, length, offset, argument.sourceOrdinal)
			|| !ReadU32(bytes, length, offset, origin) )
		{
			return asAST_SIDECAR_MALFORMED;
		}
		argument.formalQuals = formalQuals;
		argument.origin = static_cast<asEASTCallArgumentOrigin>(origin);
		if( argument.expression == 0
			|| !IsValidExprRef(argument.expression, exprCount)
			|| !IsValidDeclRef(argument.formal, declCount)
			|| argument.formalType == 0
			|| argument.formalType > typeCount
			|| !asASTQualifiersAreValid(argument.formalQuals)
			|| !IsAbsentOrOwnedRange(sources, argument.range)
			|| origin <= asAST_CALL_ARGUMENT_INVALID
			|| origin > asAST_CALL_ARGUMENT_GENERATED )
		{
			return asAST_SIDECAR_MALFORMED;
		}
		expr.callArguments.PushLast(argument);
	}
	return asAST_SIDECAR_OK;
}

int asCASTReadSidecarHeader(const asBYTE* bytes, asUINT length, asSASTSidecarHeader& outHeader)
{
	outHeader = asSASTSidecarHeader();
	if( bytes == 0 )
	{
		return asAST_SIDECAR_MALFORMED;
	}
	if( length == 0 )
	{
		return asAST_SIDECAR_EMPTY_NOT_ABSENCE;
	}
	if( length > kMaxBytes )
	{
		return asAST_SIDECAR_BUDGET;
	}
	if( length < 4 || bytes[0] != kMagic[0] || bytes[1] != kMagic[1]
		|| bytes[2] != kMagic[2] || bytes[3] != kMagic[3] )
	{
		return asAST_SIDECAR_MALFORMED;
	}

	asUINT offset = 4;
	asUINT version = 0;
	if( !ReadU32(bytes, length, offset, version) || version != kVersion )
	{
		return asAST_SIDECAR_UNKNOWN_KIND;
	}
	if( !ReadBytes(bytes, length, offset, outHeader.functionKey)
		|| !ReadBytes(bytes, length, offset, outHeader.profile)
		|| outHeader.functionKey.GetLength() == 0 )
	{
		return asAST_SIDECAR_MALFORMED;
	}
	outHeader.version = version;
	return asAST_SIDECAR_OK;
}

int asCASTDecodeSidecar(asCASTContext& context, const asSASTSidecarHeader& expected, const asBYTE* bytes, asUINT length)
{
	if( bytes == 0 )
	{
		return asAST_SIDECAR_MALFORMED;
	}
	if( length == 0 )
	{
		return asAST_SIDECAR_EMPTY_NOT_ABSENCE;
	}
	if( length > kMaxBytes )
	{
		return asAST_SIDECAR_BUDGET;
	}
	if( expected.version != kVersion || !context.IsBuilding() || context.GetSourceManager().GetSectionCount() != 0
		|| context.GetDeclCount() != 0 || context.GetStmtCount() != 0 || context.GetExprCount() != 0 || context.GetTypeCount() != 0 )
	{
		return asAST_SIDECAR_MALFORMED;
	}
	if( length < 4 || bytes[0] != kMagic[0] || bytes[1] != kMagic[1] || bytes[2] != kMagic[2] || bytes[3] != kMagic[3] )
	{
		return asAST_SIDECAR_MALFORMED;
	}

	asUINT offset = 4;
	asUINT version = 0;
	if( !ReadU32(bytes, length, offset, version) || version != kVersion )
	{
		return asAST_SIDECAR_UNKNOWN_KIND;
	}

	asCString functionKey;
	asCString profile;
	if( !ReadBytes(bytes, length, offset, functionKey) || !ReadBytes(bytes, length, offset, profile) )
	{
		return asAST_SIDECAR_MALFORMED;
	}
	if( !functionKey.Equals(expected.functionKey) )
	{
		return asAST_SIDECAR_FUNCTION_KEY_MISMATCH;
	}
	if( expected.profile.GetLength() == 0 || !profile.Equals(expected.profile) )
	{
		return asAST_SIDECAR_PROFILE_MISMATCH;
	}

	asUINT sourceCount = 0;
	if( !ReadU32(bytes, length, offset, sourceCount) || sourceCount > kMaxNodes )
	{
		return DecodeFail(context, asAST_SIDECAR_BUDGET);
	}
	for( asUINT i = 0; i < sourceCount; ++i )
	{
		asCString logicalKey;
		asCString sourceBytes;
		asUINT origin = 0;
		asUINT lineOffset = 0;
		asUINT provenanceRangeCount = 0;
		if( !ReadBytes(bytes, length, offset, logicalKey)
			|| !ReadU32(bytes, length, offset, origin)
			|| !ReadU32(bytes, length, offset, lineOffset)
			|| !ReadBytes(bytes, length, offset, sourceBytes)
			|| !ReadU32(bytes, length, offset, provenanceRangeCount) )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		if( provenanceRangeCount > kMaxNodes )
		{
			return DecodeFail(context, asAST_SIDECAR_BUDGET);
		}
		asCArray<asSSourceProvenanceRange> provenanceRanges;
		provenanceRanges.SetLength(provenanceRangeCount);
		for( asUINT rangeIndex = 0;
			rangeIndex < provenanceRangeCount;
			++rangeIndex )
		{
			if( !ReadSourceProvenanceRange(
				bytes,
				length,
				offset,
				provenanceRanges[rangeIndex]) )
			{
				return DecodeFail(context, asAST_SIDECAR_MALFORMED);
			}
		}
		const asASTFileID file = context.AddSourceSection(
			logicalKey.AddressOf(),
			(asEASTSourceOrigin)origin,
			sourceBytes.GetLength() ? sourceBytes.AddressOf() : "",
			sourceBytes.GetLength(),
			(int)lineOffset,
			provenanceRangeCount != 0
				? provenanceRanges.AddressOf()
				: 0,
			provenanceRangeCount);
		if( file != i + 1 )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
	}

	asUINT typeCount = 0;
	if( !ReadU32(bytes, length, offset, typeCount) || typeCount > kMaxNodes )
	{
		return DecodeFail(context, asAST_SIDECAR_BUDGET);
	}
	for( asUINT i = 1; i <= typeCount; ++i )
	{
		asUINT kind = 0;
		asUINT primitiveToken = 0;
		asCString stableKey;
		if( !ReadU32(bytes, length, offset, kind)
			|| !ReadU32(bytes, length, offset, primitiveToken)
			|| !ReadBytes(bytes, length, offset, stableKey) )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		asCQualType restored;
		if( (asEASTTypeKind)kind == asAST_TYPE_VOID || (asEASTTypeKind)kind == asAST_TYPE_PRIMITIVE )
		{
			restored = context.InternPrimitive((eTokenType)primitiveToken, 0);
		}
		else
		{
			restored = context.InternNamedType((asEASTTypeKind)kind, stableKey.AddressOf(), 0);
		}
		if( !restored.IsValid() || restored.type.value != i )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
	}

	asUINT declCount = 0;
	if( !ReadU32(bytes, length, offset, declCount) || declCount == 0 || declCount > kMaxNodes )
	{
		return DecodeFail(context, asAST_SIDECAR_BUDGET);
	}
	asCArray<asSSidecarDeclDTO> decls;
	for( asUINT i = 0; i < declCount; ++i )
	{
		asSSidecarDeclDTO dto;
		int declReadStatus = asAST_SIDECAR_MALFORMED;
		if( !ReadDeclDTO(bytes, length, offset, dto, declReadStatus)
			|| (dto.type != 0 && dto.type > typeCount) )
		{
			return DecodeFail(context,
				declReadStatus == asAST_SIDECAR_OK
					? asAST_SIDECAR_MALFORMED : declReadStatus);
		}
		const asCQualType type(asASTTypeRef(dto.type), dto.quals);
		asASTDeclId id;
		if( i == 0 )
		{
			if( dto.kind != asAST_DECL_TRANSLATION_UNIT || dto.parent != 0 )
			{
				return DecodeFail(context, asAST_SIDECAR_MALFORMED);
			}
			id = context.CreateTranslationUnit(dto.name.AddressOf());
		}
		else
		{
			id = context.CreateDecl(dto.kind, asASTDeclId(), dto.range, dto.name.AddressOf());
		}
		if( !id.IsValid() || id.value != i + 1
			|| context.SetDeclRange(id, dto.range) != asAST_VERIFY_OK
			|| context.SetDeclType(id, type) != asAST_VERIFY_OK
			|| context.SetStableKey(id, dto.stableKey.AddressOf()) != asAST_VERIFY_OK
			|| context.SetDeclTraits(id, dto.traits) != asAST_VERIFY_OK
			|| context.SetOrigin(id, dto.origin.AddressOf()) != asAST_VERIFY_OK
			|| context.SetDefaultArg(id, dto.defaultArg.AddressOf()) != asAST_VERIFY_OK
			|| context.SetDeclByteOffset(id, dto.byteOffset) != asAST_VERIFY_OK
			|| context.SetDeclLayout(id, dto.byteSize, dto.byteAlignment) != asAST_VERIFY_OK )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		if( dto.hasConstantValue && context.SetDeclConstantValue(id, dto.constantValue) != asAST_VERIFY_OK )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		if( (dto.kind == asAST_DECL_PARAM
				&& context.SetDeclFormalIndex(id, dto.formalIndex) != asAST_VERIFY_OK)
			|| (dto.kind != asAST_DECL_PARAM
				&& dto.formalIndex != asAST_FORMAL_INDEX_NONE) )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		decls.PushLast(dto);
	}

	for( asUINT i = 0; i < declCount; ++i )
	{
		const asSSidecarDeclDTO& dto = decls[i];
		const asASTDeclId id(i + 1);
		if( (i == 0 && (dto.parent != 0 || dto.children.GetLength() && dto.children[0] == 0))
			|| (i != 0 && (!IsValidDeclRef(dto.parent, declCount) || dto.parent == i + 1))
			|| !IsValidStmtRef(dto.body, kMaxNodes)
			|| !IsValidDeclRef(dto.accessorField, declCount)
			|| !IsValidDeclRef(dto.accessSpecifier, declCount) )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		if( i != 0 && context.SetDeclParent(id, asASTDeclId(dto.parent)) != asAST_VERIFY_OK )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		if( context.ClearDeclChildren(id) != asAST_VERIFY_OK )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		for( asUINT c = 0; c < dto.children.GetLength(); ++c )
		{
			if( !IsValidDeclRef(dto.children[c], declCount) || dto.children[c] == 0
				|| context.AddDeclChild(id, asASTDeclId(dto.children[c])) != asAST_VERIFY_OK )
			{
				return DecodeFail(context, asAST_SIDECAR_MALFORMED);
			}
		}
		for( asUINT d = 0; d < dto.dependencies.GetLength(); ++d )
		{
			if( context.AddDependency(id, dto.dependencies[d].AddressOf()) != asAST_VERIFY_OK )
			{
				return DecodeFail(context, asAST_SIDECAR_MALFORMED);
			}
		}
		for( asUINT b = 0; b < dto.bases.GetLength(); ++b )
		{
			if( !IsValidDeclRef(dto.bases[b], declCount) || dto.bases[b] == 0
				|| context.AddBase(id, asASTDeclId(dto.bases[b])) != asAST_VERIFY_OK )
			{
				return DecodeFail(context, asAST_SIDECAR_MALFORMED);
			}
		}
		for( asUINT c = 0; c < dto.captures.GetLength(); ++c )
		{
			if( !IsValidDeclRef(dto.captures[c], declCount) || dto.captures[c] == 0
				|| context.AddCapture(id, asASTDeclId(dto.captures[c])) != asAST_VERIFY_OK )
			{
				return DecodeFail(context, asAST_SIDECAR_MALFORMED);
			}
		}
		if( dto.accessorKind != asAST_ACCESSOR_NONE )
		{
			if( dto.accessorField == 0 || context.SetGeneratedAccessor(id, dto.accessorKind, asASTDeclId(dto.accessorField)) != asAST_VERIFY_OK )
			{
				return DecodeFail(context, asAST_SIDECAR_MALFORMED);
			}
		}
		if( dto.accessSpecifier != 0
			&& context.SetDeclAccessSpecifier(
				id, asASTDeclId(dto.accessSpecifier)) != asAST_VERIFY_OK )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
	}

	// Restore method relations only after every declaration and declaration
	// link exists. Final Seal independently authenticates their semantics.
	for( asUINT i = 0; i < declCount; ++i )
	{
		const asSSidecarDeclDTO& dto = decls[i];
		const asASTDeclId id(i + 1);
		for( asUINT relationIndex = 0;
			relationIndex < dto.methodRelations.GetLength();
			++relationIndex )
		{
			const asSSidecarMethodRelationDTO& relation =
				dto.methodRelations[relationIndex];
			if( !IsValidDeclRef(relation.implementation, declCount)
				|| relation.implementation == 0
				|| !IsValidDeclRef(relation.requirement, declCount)
				|| relation.requirement == 0
				|| context.AddMethodRelation(
					id,
					asSASTMethodRelation(
						static_cast<asEASTMethodRelationKind>(relation.kind),
						asASTDeclId(relation.implementation),
						asASTDeclId(relation.requirement))) != asAST_VERIFY_OK )
			{
				return DecodeFail(context, asAST_SIDECAR_MALFORMED);
			}
		}
	}

	asUINT stmtCount = 0;
	if( !ReadU32(bytes, length, offset, stmtCount) || stmtCount > kMaxNodes )
	{
		return DecodeFail(context, asAST_SIDECAR_BUDGET);
	}
	asCArray<asSSidecarStmtDTO> stmts;
	for( asUINT i = 0; i < stmtCount; ++i )
	{
		asSSidecarStmtDTO dto;
		if( !ReadStmtDTO(bytes, length, offset, dto) || !IsValidDeclRef(dto.owner, declCount) )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		const asASTStmtId id = context.CreateStmt(dto.kind, asASTDeclId(dto.owner), dto.range);
		if( !id.IsValid() || id.value != i + 1 || context.SetStmtSafePointRole(id, dto.safePointRole) != asAST_VERIFY_OK )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		stmts.PushLast(dto);
	}

	asUINT exprCount = 0;
	if( !ReadU32(bytes, length, offset, exprCount) || exprCount > kMaxNodes )
	{
		return DecodeFail(context, asAST_SIDECAR_BUDGET);
	}
	asCArray<asSSidecarExprDTO> exprs;
	for( asUINT i = 0; i < exprCount; ++i )
	{
		asSSidecarExprDTO dto;
		if( !ReadExprDTO(bytes, length, offset, dto) || (dto.type != 0 && dto.type > typeCount) )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		const asASTExprId id = context.CreateExpr(dto.kind, asCQualType(asASTTypeRef(dto.type), dto.quals), dto.valueCategory, dto.range);
		if( !id.IsValid() || id.value != i + 1 )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		exprs.PushLast(dto);
	}

	for( asUINT i = 0; i < declCount; ++i )
	{
		const asSSidecarDeclDTO& dto = decls[i];
		const asASTDeclId id(i + 1);
		if( !IsValidStmtRef(dto.body, stmtCount)
			|| context.SetBody(id, asASTStmtId(dto.body)) != asAST_VERIFY_OK )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		for( asUINT n = 0; n < dto.inits.GetLength(); ++n )
		{
			if( !IsValidExprRef(dto.inits[n], exprCount) || dto.inits[n] == 0
				|| context.AddDeclInit(id, asASTExprId(dto.inits[n])) != asAST_VERIFY_OK )
			{
				return DecodeFail(context, asAST_SIDECAR_MALFORMED);
			}
		}
	}

	for( asUINT i = 0; i < stmtCount; ++i )
	{
		const asSSidecarStmtDTO& dto = stmts[i];
		const asASTStmtId id(i + 1);
		if( !IsValidStmtRef(dto.target, stmtCount) || !IsValidExprRef(dto.expr, exprCount) || !IsValidDeclRef(dto.decl, declCount)
			|| context.SetTarget(id, asASTStmtId(dto.target)) != asAST_VERIFY_OK
			|| context.SetStmtExpr(id, asASTExprId(dto.expr)) != asAST_VERIFY_OK
			|| context.SetStmtDecl(id, asASTDeclId(dto.decl)) != asAST_VERIFY_OK )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		for( asUINT c = 0; c < dto.children.GetLength(); ++c )
		{
			if( !IsValidStmtRef(dto.children[c], stmtCount) || dto.children[c] == 0
				|| context.AddStmtChild(id, asASTStmtId(dto.children[c])) != asAST_VERIFY_OK )
			{
				return DecodeFail(context, asAST_SIDECAR_MALFORMED);
			}
		}
	}

	for( asUINT i = 0; i < exprCount; ++i )
	{
		const asSSidecarExprDTO& dto = exprs[i];
		const asASTExprId id(i + 1);
		if( !IsValidDeclRef(dto.resolvedDecl, declCount) || !IsValidExprRef(dto.receiver, exprCount)
			|| context.SetResolvedDecl(id, asASTDeclId(dto.resolvedDecl)) != asAST_VERIFY_OK
			|| context.SetLiteralBytes(id, dto.literal.AddressOf(),
				static_cast<asUINT>(dto.literal.GetLength())) != asAST_VERIFY_OK
			|| context.SetLiteralBits(id, dto.literalBits) != asAST_VERIFY_OK
			|| context.SetExprSafePointRole(id, dto.safePointRole) != asAST_VERIFY_OK
			|| (dto.callDispatch != asAST_CALL_DISPATCH_NONE
				&& context.SetExprCallDispatch(id, dto.callDispatch) != asAST_VERIFY_OK) )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		if( dto.receiver != 0 && context.SetExprReceiver(id, asASTExprId(dto.receiver)) != asAST_VERIFY_OK )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
		for( asUINT c = 0; c < dto.children.GetLength(); ++c )
		{
			if( !IsValidExprRef(dto.children[c], exprCount) || dto.children[c] == 0
				|| context.AddExprChild(id, asASTExprId(dto.children[c])) != asAST_VERIFY_OK )
			{
				return DecodeFail(context, asAST_SIDECAR_MALFORMED);
			}
		}
	}

	asUINT lifetimeProtocolRevision = 0;
	asUINT lifetimeRecordCount = 0;
	if( !ReadU32(bytes, length, offset, lifetimeProtocolRevision)
		|| !ReadU32(bytes, length, offset, lifetimeRecordCount)
		|| lifetimeRecordCount > kMaxNodes
		|| context.SetLifetimeProtocolRevision(lifetimeProtocolRevision)
			!= asAST_VERIFY_OK )
	{
		return DecodeFail(context, asAST_SIDECAR_MALFORMED);
	}
	for( asUINT i = 0; i < lifetimeRecordCount; ++i )
	{
		asSASTLifetimeRecord record;
		if( !ReadLifetimeRecord(bytes, length, offset, record)
			|| context.AddLifetimeRecord(record) != asAST_VERIFY_OK )
		{
			return DecodeFail(context, asAST_SIDECAR_MALFORMED);
		}
	}

	// Read and authenticate the entire append-only V9 trailer before exposing
	// even one CallArgument record through the candidate Context.
	asUINT totalCallArgumentCount = 0;
	for( asUINT i = 0; i < exprCount; ++i )
	{
		const int callArgumentStatus = ReadCallArgumentArray(
			bytes,
			length,
			offset,
			context.GetSourceManager(),
			declCount,
			exprCount,
			typeCount,
			exprs[i],
			totalCallArgumentCount);
		if( callArgumentStatus != asAST_SIDECAR_OK )
		{
			return DecodeFail(context, callArgumentStatus);
		}
	}
	if( offset != length )
	{
		return DecodeFail(context, asAST_SIDECAR_MALFORMED);
	}

	for( asUINT i = 0; i < exprCount; ++i )
	{
		const asASTExprId call(i + 1);
		for( asUINT argumentIndex = 0;
			argumentIndex < exprs[i].callArguments.GetLength();
			++argumentIndex )
		{
			const asSSidecarCallArgumentDTO& dto =
				exprs[i].callArguments[argumentIndex];
			asSASTCallArgument argument;
			argument.expression = asASTExprId(dto.expression);
			argument.formal = asASTDeclId(dto.formal);
			argument.formalType = asCQualType(
				asASTTypeRef(dto.formalType), dto.formalQuals);
			argument.range = dto.range;
			argument.authoredName = dto.authoredName;
			argument.formalIndex = dto.formalIndex;
			argument.sourceOrdinal = dto.sourceOrdinal;
			argument.origin = dto.origin;
			if( context.AddCallArgument(call, argument) != asAST_VERIFY_OK )
			{
				return DecodeFail(context, asAST_SIDECAR_MALFORMED);
			}
		}
	}

	if( context.Seal() != asAST_VERIFY_OK )
	{
		return DecodeFail(context, asAST_SIDECAR_MALFORMED);
	}
	asSAstVerifyResult publication;
	if( asCASTVerifyPublication(context, publication) != asAST_VERIFY_OK )
	{
		return DecodeFail(context, asAST_SIDECAR_MALFORMED);
	}
	return asAST_SIDECAR_OK;
}

static asQWORD Fnv1a64(const asBYTE* bytes, asUINT length)
{
	asQWORD hash = 14695981039346656037ULL;
	for( asUINT i = 0; i < length; ++i )
	{
		hash ^= bytes[i];
		hash *= 1099511628211ULL;
	}
	return hash;
}

static void Hex64(asQWORD value, asCString& out)
{
	const char* digits = "0123456789abcdef";
	char buf[17];
	for( int i = 15; i >= 0; --i )
	{
		buf[i] = digits[value & 0xf];
		value >>= 4;
	}
	buf[16] = 0;
	out = buf;
}

int asCASTContentHash(const asBYTE* bytes, asUINT length, asCString& outHex)
{
	if( bytes == 0 || length == 0 )
	{
		outHex = "";
		return asAST_SIDECAR_EMPTY_NOT_ABSENCE;
	}
	Hex64(Fnv1a64(bytes, length), outHex);
	return asAST_SIDECAR_OK;
}

static bool IsFunctionLike(asEASTDeclKind kind)
{
	return kind == asAST_DECL_FUNCTION
		|| kind == asAST_DECL_METHOD
		|| kind == asAST_DECL_CONSTRUCTOR
		|| kind == asAST_DECL_DESTRUCTOR
		|| kind == asAST_DECL_FUNCDEF;
}

// Function records are incremental-cache identities, not context dumps. They
// deliberately walk only the function's semantic closure and use stable keys
// and source slices rather than context-local node indices or addresses. A
// sibling edit must not make this record stale merely because it shifted an
// arena index in the enclosing snapshot.
static void AppendRangeIdentity(const asCASTContext& context, const asCSourceRange& range, asCArray<asBYTE>& material)
{
	if( !range.IsValid() )
	{
		AppendU32(material, 0);
		return;
	}

	const asCSourceManager& sources = context.GetSourceManager();
	if( !sources.IsValidRange(range) )
	{
		AppendU32(material, 1);
		return;
	}

	AppendU32(material, 2);
	AppendBytes(material, sources.GetLogicalKey(range.begin.fileID));
	AppendU32(material, (asUINT)sources.GetOrigin(range.begin.fileID));
	AppendU32(material, range.begin.offset);
	AppendU32(material, range.end.offset);
	const char* buffer = sources.GetBuffer(range.begin.fileID);
	const asUINT bufferLength = sources.GetBufferLength(range.begin.fileID);
	const asUINT begin = range.begin.offset;
	const asUINT length = range.end.offset - range.begin.offset;
	if( buffer == 0 || begin > bufferLength || length > bufferLength - begin )
	{
		AppendU32(material, 0);
		return;
	}
	AppendRawBytes(material, buffer + begin, length);
}

static void AppendTypeIdentity(const asCASTContext& context, const asCQualType& type, asCArray<asBYTE>& material)
{
	AppendU32(material, type.quals);
	if( !type.IsValid() )
	{
		AppendU32(material, 0);
		return;
	}

	const asCType* value = context.GetType(type.type);
	if( value == 0 )
	{
		AppendU32(material, 1);
		return;
	}

	AppendU32(material, 2);
	AppendU32(material, (asUINT)value->kind);
	AppendU32(material, (asUINT)value->primitiveToken);
	AppendBytes(material, value->stableKey.AddressOf());
}

static void AppendDeclReferenceIdentity(const asCASTContext& context, asASTDeclId id, asCArray<asBYTE>& material)
{
	if( !id.IsValid() )
	{
		AppendU32(material, 0);
		return;
	}

	const asCDecl* decl = context.GetDecl(id);
	if( decl == 0 )
	{
		AppendU32(material, 1);
		return;
	}

	AppendU32(material, 2);
	AppendU32(material, (asUINT)decl->kind);
	AppendBytes(material, decl->stableKey.AddressOf());
	AppendBytes(material, decl->name.AddressOf());
	AppendTypeIdentity(context, decl->type, material);
	AppendRangeIdentity(context, decl->range, material);
	AppendU32(material, decl->formalIndex);
}

static void AppendExprIdentity(const asCASTContext& context, asASTExprId id, asCArray<asBYTE>& material);

static void AppendDeclIdentity(const asCASTContext& context, const asCDecl& decl, asCArray<asBYTE>& material)
{
	AppendU32(material, (asUINT)decl.kind);
	AppendBytes(material, decl.stableKey.AddressOf());
	AppendBytes(material, decl.name.AddressOf());
	AppendTypeIdentity(context, decl.type, material);
	AppendRangeIdentity(context, decl.range, material);
	AppendU32(material, decl.traits);
	AppendU32(material, (asUINT)decl.accessorKind);
	AppendDeclReferenceIdentity(context, decl.accessorField, material);
	AppendDeclReferenceIdentity(context, decl.accessSpecifier, material);
	AppendBytes(material, decl.origin.AddressOf());
	AppendBytes(material, decl.defaultArg.AddressOf());
	AppendU32(material, decl.hasConstantValue ? 1 : 0);
	AppendU64(material, decl.constantValue);
	AppendU32(material, (asUINT)decl.byteOffset);
	AppendU32(material, (asUINT)decl.byteSize);
	AppendU32(material, decl.byteAlignment);
	AppendU32(material, decl.formalIndex);

	AppendU32(material, decl.dependencies.GetLength());
	for( asUINT i = 0; i < decl.dependencies.GetLength(); ++i )
	{
		AppendBytes(material, decl.dependencies[i].AddressOf());
	}
	AppendU32(material, decl.bases.GetLength());
	for( asUINT i = 0; i < decl.bases.GetLength(); ++i )
	{
		AppendDeclReferenceIdentity(context, decl.bases[i], material);
	}
	AppendU32(material, decl.methodRelations.GetLength());
	for( asUINT i = 0; i < decl.methodRelations.GetLength(); ++i )
	{
		AppendU32(material, static_cast<asUINT>(decl.methodRelations[i].kind));
		AppendDeclReferenceIdentity(
			context, decl.methodRelations[i].implementation, material);
		AppendDeclReferenceIdentity(
			context, decl.methodRelations[i].requirement, material);
	}
	AppendU32(material, decl.captures.GetLength());
	for( asUINT i = 0; i < decl.captures.GetLength(); ++i )
	{
		AppendDeclReferenceIdentity(context, decl.captures[i], material);
	}
	AppendU32(material, decl.inits.GetLength());
	for( asUINT i = 0; i < decl.inits.GetLength(); ++i )
	{
		AppendExprIdentity(context, decl.inits[i], material);
	}
}

static void AppendStmtIdentity(const asCASTContext& context, asASTStmtId id, asCArray<asBYTE>& material)
{
	if( !id.IsValid() )
	{
		AppendU32(material, 0);
		return;
	}

	const asCStmt* stmt = context.GetStmt(id);
	if( stmt == 0 )
	{
		AppendU32(material, 1);
		return;
	}

	AppendU32(material, 2);
	AppendU32(material, (asUINT)stmt->kind);
	AppendRangeIdentity(context, stmt->range, material);
	AppendU32(material, (asUINT)stmt->safePointRole);
	AppendDeclReferenceIdentity(context, stmt->owner, material);
	AppendDeclReferenceIdentity(context, stmt->decl, material);
	AppendExprIdentity(context, stmt->expr, material);
	if( stmt->target.IsValid() )
	{
		const asCStmt* target = context.GetStmt(stmt->target);
		AppendU32(material, target ? 2 : 1);
		if( target )
		{
			AppendU32(material, (asUINT)target->kind);
			AppendRangeIdentity(context, target->range, material);
		}
	}
	else
	{
		AppendU32(material, 0);
	}

	AppendU32(material, stmt->children.GetLength());
	for( asUINT i = 0; i < stmt->children.GetLength(); ++i )
	{
		AppendStmtIdentity(context, stmt->children[i], material);
	}
}

static void AppendExprIdentity(const asCASTContext& context, asASTExprId id, asCArray<asBYTE>& material)
{
	if( !id.IsValid() )
	{
		AppendU32(material, 0);
		return;
	}

	const asCExpr* expr = context.GetExpr(id);
	if( expr == 0 )
	{
		AppendU32(material, 1);
		return;
	}

	AppendU32(material, 2);
	AppendU32(material, (asUINT)expr->kind);
	AppendRangeIdentity(context, expr->range, material);
	AppendTypeIdentity(context, expr->type, material);
	AppendU32(material, (asUINT)expr->valueCategory);
	AppendDeclReferenceIdentity(context, expr->resolvedDecl, material);
	AppendExprIdentity(context, expr->receiver, material);
	AppendRawBytes(material, expr->literal.AddressOf(),
		static_cast<asUINT>(expr->literal.GetLength()));
	AppendU64(material, expr->literalBits);
	AppendU32(material, (asUINT)expr->safePointRole);
	AppendU32(material, (asUINT)expr->callDispatch);
	AppendU32(material, expr->children.GetLength());
	for( asUINT i = 0; i < expr->children.GetLength(); ++i )
	{
		AppendExprIdentity(context, expr->children[i], material);
	}

	// The verifier authenticates each record expression against the child at
	// the same stored evaluation record. Hash the formal ordinal plus the non-derivable
	// binding facts here; recursively hashing the expression again would only
	// duplicate the child closure above.
	AppendU32(material, expr->callArguments.GetLength());
	for( asUINT i = 0; i < expr->callArguments.GetLength(); ++i )
	{
		const asSASTCallArgument& argument = expr->callArguments[i];
		AppendU32(material, i);
		AppendDeclReferenceIdentity(context, argument.formal, material);
		AppendTypeIdentity(context, argument.formalType, material);
		AppendRangeIdentity(context, argument.range, material);
		AppendBytes(material, argument.authoredName.AddressOf());
		AppendU32(material, argument.formalIndex);
		AppendU32(material, argument.sourceOrdinal);
		AppendU32(material, static_cast<asUINT>(argument.origin));
	}
}

static void AppendFunctionRecordMaterial(const asCASTContext& context, const asCDecl& function, const char* profile, asCArray<asBYTE>& material)
{
	AppendBytes(material, "canonical-ast-function-record-v3");
	AppendBytes(material, profile ? profile : "");
	AppendDeclIdentity(context, function, material);

	// Parameters and compiler-created child declarations alter the callable
	// body contract even when the function's return type is unchanged.
	AppendU32(material, function.children.GetLength());
	for( asUINT i = 0; i < function.children.GetLength(); ++i )
	{
		const asCDecl* child = context.GetDecl(function.children[i]);
		if( child )
		{
			AppendU32(material, 2);
			AppendDeclIdentity(context, *child, material);
		}
		else
		{
			AppendU32(material, 1);
		}
	}

	AppendStmtIdentity(context, function.body, material);
}

int asCASTCollectFunctionRecords(const asCASTContext& context, const char* profile, asCArray<asSASTSidecarRecordId>& records)
{
	records.SetLength(0);
	if( !context.IsPublishable() )
	{
		return asAST_SIDECAR_MALFORMED;
	}
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( decl == 0 || !IsFunctionLike(decl->kind) )
		{
			continue;
		}
		asSASTSidecarRecordId record;
		record.functionKey = decl->stableKey.GetLength() ? decl->stableKey : decl->name;
		asCArray<asBYTE> material;
		AppendFunctionRecordMaterial(context, *decl, profile, material);
		for( asUINT d = 0; d < decl->dependencies.GetLength(); ++d )
		{
			record.dependencies.PushLast(decl->dependencies[d]);
		}
		asCASTContentHash(material.AddressOf(), material.GetLength(), record.contentHash);
		records.PushLast(record);
	}
	return asAST_SIDECAR_OK;
}

int asCASTPlanIncrementalSidecars(
	const asCArray<asSASTSidecarRecordId>& previous,
	const asCArray<asSASTSidecarRecordId>& current,
	asCArray<asCString>& reusedKeys,
	asCArray<asCString>& rebuiltKeys,
	asCArray<asCString>& retiredKeys)
{
	reusedKeys.SetLength(0);
	rebuiltKeys.SetLength(0);
	retiredKeys.SetLength(0);
	for( asUINT p = 0; p < previous.GetLength(); ++p )
	{
		int found = -1;
		for( asUINT c = 0; c < current.GetLength(); ++c )
		{
			if( current[c].functionKey.Equals(previous[p].functionKey) )
			{
				found = (int)c;
				break;
			}
		}
		if( found < 0 )
		{
			retiredKeys.PushLast(previous[p].functionKey);
			continue;
		}
		if( current[(asUINT)found].contentHash.Equals(previous[p].contentHash) )
		{
			reusedKeys.PushLast(previous[p].functionKey);
		}
		else
		{
			rebuiltKeys.PushLast(previous[p].functionKey);
		}
	}
	for( asUINT c = 0; c < current.GetLength(); ++c )
	{
		bool existed = false;
		for( asUINT p = 0; p < previous.GetLength(); ++p )
		{
			if( previous[p].functionKey.Equals(current[c].functionKey) )
			{
				existed = true;
				break;
			}
		}
		if( !existed )
		{
			rebuiltKeys.PushLast(current[c].functionKey);
		}
	}
	return asAST_SIDECAR_OK;
}

int asCASTCommitSidecarSet(asCArray<asSASTSidecarRecordId>& live, const asCArray<asSASTSidecarRecordId>& next)
{
	for( asUINT i = 0; i < next.GetLength(); ++i )
	{
		if( next[i].contentHash.GetLength() == 0 || next[i].functionKey.GetLength() == 0 )
		{
			return asAST_SIDECAR_MALFORMED;
		}
	}
	live = next;
	return asAST_SIDECAR_OK;
}

int asCASTRejectNonSidecarInput(const asBYTE* bytes, asUINT length)
{
	if( bytes == 0 || length == 0 )
	{
		return asAST_SIDECAR_EMPTY_NOT_ABSENCE;
	}
	if( length >= 4 && bytes[0] == 'A' && bytes[1] == 'S' && bytes[2] == 'T' && bytes[3] == '\n' )
	{
		return asAST_SIDECAR_UNSUPPORTED_INPUT;
	}
	if( length >= 4 && bytes[0] == 'C' && bytes[1] == 'A' && bytes[2] == 'S' && bytes[3] == 'T' )
	{
		asUINT offset = 4;
		asUINT version = 0;
		if( !ReadU32(bytes, length, offset, version) || version != kVersion )
		{
			return asAST_SIDECAR_UNKNOWN_KIND;
		}
		return asAST_SIDECAR_OK;
	}
	for( asUINT i = 0; i + 4 < length && i < 64; ++i )
	{
		if( (bytes[i] == '.' && bytes[i + 1] == 'h' && bytes[i + 2] == 'i' && bytes[i + 3] == 'r')
			|| (bytes[i] == 'H' && bytes[i + 1] == 'I' && bytes[i + 2] == 'R') )
		{
			return asAST_SIDECAR_UNSUPPORTED_INPUT;
		}
	}
	return asAST_SIDECAR_MALFORMED;
}

END_AS_NAMESPACE
