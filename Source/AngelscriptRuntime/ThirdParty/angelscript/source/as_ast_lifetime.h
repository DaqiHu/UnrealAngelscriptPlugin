/*
   Maintained Unreal AngelScript fork - Canonical AST lifetime protocol.
   Snapshot-owned semantic facts only; no Runtime/backend identity or state.
*/

#ifndef AS_AST_LIFETIME_H
#define AS_AST_LIFETIME_H

#include "as_ast_fwd.h"

BEGIN_AS_NAMESPACE

class asCASTContext;

static const asDWORD asAST_LIFETIME_PROTOCOL_REVISION = 1;

enum asEASTLifetimeNodeClass : asBYTE
{
	asAST_LIFETIME_NODE_INVALID = 0,
	asAST_LIFETIME_NODE_DECL,
	asAST_LIFETIME_NODE_STMT,
	asAST_LIFETIME_NODE_EXPR,
};

// A protocol reference is meaningful only inside the Context that owns the
// record. snapshotOwner must therefore remain zero; public snapshot tokens are
// deliberately rejected by protocol authentication.
struct asSASTLifetimeNodeId
{
	asEASTLifetimeNodeClass nodeClass;
	asUINT value;
	asDWORD snapshotOwner;

	asSASTLifetimeNodeId()
		: nodeClass(asAST_LIFETIME_NODE_INVALID)
		, value(0)
		, snapshotOwner(0)
	{
	}

	asSASTLifetimeNodeId(
		asEASTLifetimeNodeClass inNodeClass,
		asUINT inValue,
		asDWORD inSnapshotOwner = 0)
		: nodeClass(inNodeClass)
		, value(inValue)
		, snapshotOwner(inSnapshotOwner)
	{
	}

	static asSASTLifetimeNodeId FromDecl(asASTDeclId id)
	{
		return asSASTLifetimeNodeId(
			asAST_LIFETIME_NODE_DECL, id.value, id.snapshotOwner);
	}
	static asSASTLifetimeNodeId FromStmt(asASTStmtId id)
	{
		return asSASTLifetimeNodeId(
			asAST_LIFETIME_NODE_STMT, id.value, id.snapshotOwner);
	}
	static asSASTLifetimeNodeId FromExpr(asASTExprId id)
	{
		return asSASTLifetimeNodeId(
			asAST_LIFETIME_NODE_EXPR, id.value, id.snapshotOwner);
	}

	bool IsValid() const
	{
		return nodeClass != asAST_LIFETIME_NODE_INVALID && value != 0;
	}
	bool operator==(const asSASTLifetimeNodeId& other) const
	{
		return nodeClass == other.nodeClass
			&& value == other.value
			&& snapshotOwner == other.snapshotOwner;
	}
	bool operator!=(const asSASTLifetimeNodeId& other) const
	{
		return !(*this == other);
	}
};

enum asEASTLifetimeSubjectKind : asBYTE
{
	asAST_LIFETIME_SUBJECT_INVALID = 0,
	asAST_LIFETIME_SUBJECT_LOCAL,
	asAST_LIFETIME_SUBJECT_TEMPORARY,
	asAST_LIFETIME_SUBJECT_FOREACH_ITERATOR,
	asAST_LIFETIME_SUBJECT_CONSTRUCTION_SUBOBJECT,
	asAST_LIFETIME_SUBJECT_AGGREGATE_ELEMENT,
	asAST_LIFETIME_SUBJECT_DEFERRED_OUT,
};

enum asEASTLifetimeActionKind : asBYTE
{
	asAST_LIFETIME_ACTION_INVALID = 0,
	asAST_LIFETIME_ACTION_DESTROY_VALUE,
	asAST_LIFETIME_ACTION_RELEASE_REFERENCE,
	asAST_LIFETIME_ACTION_COMMIT_DEFERRED_OUT,
	asAST_LIFETIME_ACTION_DESTROY_ELEMENT,
};

enum asEASTLifetimePhase : asBYTE
{
	asAST_LIFETIME_PHASE_INVALID = 0,
	asAST_LIFETIME_PHASE_FULL_EXPRESSION,
	asAST_LIFETIME_PHASE_LEXICAL_SCOPE,
	asAST_LIFETIME_PHASE_LOOP_ITERATION,
	asAST_LIFETIME_PHASE_CONSTRUCTOR_BASE,
	asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER,
	asAST_LIFETIME_PHASE_CONSTRUCTOR_DELEGATING,
	asAST_LIFETIME_PHASE_COMPLETE_OBJECT,
	asAST_LIFETIME_PHASE_AGGREGATE_ELEMENT,
};

enum asEASTLifetimeExitMask : asDWORD
{
	asAST_LIFETIME_EXIT_NONE = 0,
	asAST_LIFETIME_EXIT_NORMAL = 1u << 0,
	asAST_LIFETIME_EXIT_RETURN = 1u << 1,
	asAST_LIFETIME_EXIT_BREAK = 1u << 2,
	asAST_LIFETIME_EXIT_CONTINUE = 1u << 3,
	asAST_LIFETIME_EXIT_FALLTHROUGH = 1u << 4,
	asAST_LIFETIME_EXIT_EXCEPTION = 1u << 5,
	asAST_LIFETIME_EXIT_ALL =
		asAST_LIFETIME_EXIT_NORMAL
		| asAST_LIFETIME_EXIT_RETURN
		| asAST_LIFETIME_EXIT_BREAK
		| asAST_LIFETIME_EXIT_CONTINUE
		| asAST_LIFETIME_EXIT_FALLTHROUGH
		| asAST_LIFETIME_EXIT_EXCEPTION,
};

// One record is a Sema-authored semantic lifetime fact. Physical cleanup
// labels, slots, stacks, frame layout and rendered dump identities belong to
// backend-local state and are intentionally absent.
struct asSASTLifetimeRecord
{
	asEASTLifetimeSubjectKind subjectKind;
	asSASTLifetimeNodeId subject;
	asEASTLifetimeActionKind actionKind;
	asSASTLifetimeNodeId actionTarget;
	asSASTLifetimeNodeId activationPoint;
	asSASTLifetimeNodeId semanticRegion;
	asEASTLifetimePhase phase;
	asDWORD supportedExitMask;
	asUINT constructionStep;
	asUINT order;
	bool completeObjectCommit;

	asSASTLifetimeRecord()
		: subjectKind(asAST_LIFETIME_SUBJECT_INVALID)
		, actionKind(asAST_LIFETIME_ACTION_INVALID)
		, phase(asAST_LIFETIME_PHASE_INVALID)
		, supportedExitMask(asAST_LIFETIME_EXIT_NONE)
		, constructionStep(0)
		, order(0)
		, completeObjectCommit(false)
	{
	}

	bool operator==(const asSASTLifetimeRecord& other) const
	{
		return subjectKind == other.subjectKind
			&& subject == other.subject
			&& actionKind == other.actionKind
			&& actionTarget == other.actionTarget
			&& activationPoint == other.activationPoint
			&& semanticRegion == other.semanticRegion
			&& phase == other.phase
			&& supportedExitMask == other.supportedExitMask
			&& constructionStep == other.constructionStep
			&& order == other.order
			&& completeObjectCommit == other.completeObjectCommit;
	}
	bool operator!=(const asSASTLifetimeRecord& other) const
	{
		return !(*this == other);
	}
};

ANGELSCRIPTRUNTIME_API asQWORD asASTHashLifetimeNodeId(
	const asSASTLifetimeNodeId& id);
ANGELSCRIPTRUNTIME_API asQWORD asASTHashLifetimeRecord(
	const asSASTLifetimeRecord& record);
ANGELSCRIPTRUNTIME_API asQWORD asASTHashLifetimeProtocol(
	asDWORD revision,
	const asSASTLifetimeRecord* records,
	asUINT recordCount);
ANGELSCRIPTRUNTIME_API int asCASTVerifyLifetimeProtocol(
	const asCASTContext& context);

END_AS_NAMESPACE

#endif
