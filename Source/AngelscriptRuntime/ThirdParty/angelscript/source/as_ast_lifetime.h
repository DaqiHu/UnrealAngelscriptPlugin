/*
   Maintained Unreal AngelScript fork - Canonical AST lifetime protocol.
   Snapshot-owned semantic facts only; no Runtime/backend identity or state.
*/

#ifndef AS_AST_LIFETIME_H
#define AS_AST_LIFETIME_H

#include "as_array.h"
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

// One deterministic lexical-scope relation mechanically derived from exact
// semanticRegion identities. parentRegion is invalid for a root lifetime
// region. The view never stores AST or Runtime pointers.
struct asSASTLifetimeScopeEdge
{
	asSASTLifetimeNodeId region;
	asSASTLifetimeNodeId parentRegion;
	asUINT depth;
	asUINT structuralOrdinal;

	asSASTLifetimeScopeEdge()
		: depth(0)
		, structuralOrdinal(0)
	{
	}

	bool operator==(const asSASTLifetimeScopeEdge& other) const
	{
		return region == other.region
			&& parentRegion == other.parentRegion
			&& depth == other.depth
			&& structuralOrdinal == other.structuralOrdinal;
	}
	bool operator!=(const asSASTLifetimeScopeEdge& other) const
	{
		return !(*this == other);
	}
};

// A typed copy of one authenticated source fact plus only mechanical lexical
// ordinals. recordIndex remains the Context protocol index; activationOrdinal
// is the direct-child commit position inside semanticRegion; cleanupOrdinal is
// its reverse position among records in that same region.
struct asSASTLifetimeViewRecord
{
	asSASTLifetimeRecord record;
	asUINT recordIndex;
	asUINT scopeEdgeIndex;
	asUINT activationOrdinal;
	asUINT cleanupOrdinal;

	asSASTLifetimeViewRecord()
		: recordIndex(0)
		, scopeEdgeIndex(0)
		, activationOrdinal(0)
		, cleanupOrdinal(0)
	{
	}

	bool operator==(const asSASTLifetimeViewRecord& other) const
	{
		return record == other.record
			&& recordIndex == other.recordIndex
			&& scopeEdgeIndex == other.scopeEdgeIndex
			&& activationOrdinal == other.activationOrdinal
			&& cleanupOrdinal == other.cleanupOrdinal;
	}
	bool operator!=(const asSASTLifetimeViewRecord& other) const
	{
		return !(*this == other);
	}
};

// A commit point exposes the live record-index set immediately before and
// after its success boundary. abortCleanup is reverse(liveBefore), so the
// current object can never be cleaned if its initializer fails;
// reverseLiveCleanup is reverse(liveAfter).
struct asSASTLifetimeCommitPoint
{
	asSASTLifetimeNodeId activationPoint;
	asSASTLifetimeNodeId semanticRegion;
	asUINT recordIndex;
	asCArray<asUINT> liveBefore;
	asCArray<asUINT> liveAfter;
	asCArray<asUINT> abortCleanup;
	asCArray<asUINT> reverseLiveCleanup;

	asSASTLifetimeCommitPoint()
		: recordIndex(0)
	{
	}

	bool operator==(const asSASTLifetimeCommitPoint& other) const
	{
		if( activationPoint != other.activationPoint
			|| semanticRegion != other.semanticRegion
			|| recordIndex != other.recordIndex
			|| liveBefore.GetLength() != other.liveBefore.GetLength()
			|| liveAfter.GetLength() != other.liveAfter.GetLength()
			|| abortCleanup.GetLength() != other.abortCleanup.GetLength()
			|| reverseLiveCleanup.GetLength()
				!= other.reverseLiveCleanup.GetLength() )
		{
			return false;
		}
		for( asUINT i = 0; i < liveBefore.GetLength(); ++i )
			if( liveBefore[i] != other.liveBefore[i] ) return false;
		for( asUINT i = 0; i < liveAfter.GetLength(); ++i )
			if( liveAfter[i] != other.liveAfter[i] ) return false;
		for( asUINT i = 0; i < abortCleanup.GetLength(); ++i )
			if( abortCleanup[i] != other.abortCleanup[i] ) return false;
		for( asUINT i = 0; i < reverseLiveCleanup.GetLength(); ++i )
			if( reverseLiveCleanup[i]
				!= other.reverseLiveCleanup[i] ) return false;
		return true;
	}
	bool operator!=(const asSASTLifetimeCommitPoint& other) const
	{
		return !(*this == other);
	}
};

// One deterministic constructor-local committed-prefix plan. recordIndices are
// protocol record indices in execution order and include the unique complete
// commit. For a non-delegating constructor that commit is the final
// COMPLETE_OBJECT record; for a delegating constructor it is the sole
// CONSTRUCTOR_DELEGATING record. No Runtime type, pointer, slot, label or EH
// state is retained here.
struct asSASTLifetimeConstructionPlan
{
	asASTDeclId constructor;
	asASTDeclId ownerClass;
	asCArray<asUINT> recordIndices;
	asUINT completeRecordIndex;

	asSASTLifetimeConstructionPlan()
		: completeRecordIndex(static_cast<asUINT>(-1))
	{
	}

	bool operator==(const asSASTLifetimeConstructionPlan& other) const
	{
		if( constructor != other.constructor
			|| ownerClass != other.ownerClass
			|| completeRecordIndex != other.completeRecordIndex
			|| recordIndices.GetLength() != other.recordIndices.GetLength() )
		{
			return false;
		}
		for( asUINT index = 0; index < recordIndices.GetLength(); ++index )
			if( recordIndices[index] != other.recordIndices[index] ) return false;
		return true;
	}
	bool operator!=(const asSASTLifetimeConstructionPlan& other) const
	{
		return !(*this == other);
	}
};

// Compatibility syntax is retained while LEGACY and diagnostic/reference
// consumers still need the native AST shape.  Only this named, authenticated
// projection may be consumed by Canonical backends.
enum asEASTLifetimeCompatibilityRole : asBYTE
{
	asAST_LIFETIME_COMPAT_INVALID = 0,
	asAST_LIFETIME_COMPAT_NORMAL_EXIT,
	asAST_LIFETIME_COMPAT_TRANSFER_EXIT,
	asAST_LIFETIME_COMPAT_FOREACH_EXIT,
};

struct asSASTLifetimeCleanupBinding
{
	asASTStmtId statement;
	asASTExprId expression;
	asSASTLifetimeNodeId subject;
	asEASTLifetimeActionKind actionKind;
	asSASTLifetimeNodeId actionTarget;
	asEASTLifetimeCompatibilityRole role;
	asASTStmtId structuralOwner;
	asDWORD exitMask;
	asUINT recordIndex;

	asSASTLifetimeCleanupBinding()
		: actionKind(asAST_LIFETIME_ACTION_INVALID)
		, role(asAST_LIFETIME_COMPAT_INVALID)
		, exitMask(asAST_LIFETIME_EXIT_NONE)
		, recordIndex(0)
	{
	}
};

// Backend-facing semantic exit order derived from authenticated lifetime
// records. Compatibility cleanup statement/expression identities are
// deliberately absent: they remain parity evidence and cannot route CodeGen.
enum asEASTLifetimeExitPlanKind : asBYTE
{
	asAST_LIFETIME_EXIT_PLAN_INVALID = 0,
	asAST_LIFETIME_EXIT_PLAN_NORMAL,
	asAST_LIFETIME_EXIT_PLAN_TRANSFER,
	asAST_LIFETIME_EXIT_PLAN_FOREACH,
};

struct asSASTLifetimeExitPlan
{
	asASTStmtId structuralOwner;
	asEASTLifetimeExitPlanKind kind;
	asDWORD exitMask;
	asCArray<asUINT> recordIndices;

	asSASTLifetimeExitPlan()
		: kind(asAST_LIFETIME_EXIT_PLAN_INVALID)
		, exitMask(asAST_LIFETIME_EXIT_NONE)
	{
	}

	bool operator==(const asSASTLifetimeExitPlan& other) const
	{
		if( structuralOwner != other.structuralOwner
			|| kind != other.kind
			|| exitMask != other.exitMask
			|| recordIndices.GetLength() != other.recordIndices.GetLength() )
		{
			return false;
		}
		for( asUINT index = 0; index < recordIndices.GetLength(); ++index )
			if( recordIndices[index] != other.recordIndices[index] ) return false;
		return true;
	}
	bool operator!=(const asSASTLifetimeExitPlan& other) const
	{
		return !(*this == other);
	}
};

// Named foreach phases hide the retained children[0..3] compatibility shape.
// condition is the dedicated expression edge; cleanup is optional for
// primitive/enum iterators.
struct asSASTForEachPhases
{
	asASTStmtId statement;
	asASTStmtId initializer;
	asASTExprId condition;
	asASTStmtId body;
	asASTStmtId increment;
	asASTStmtId cleanup;
	asUINT lifetimeRecordIndex;

	asSASTForEachPhases()
		: lifetimeRecordIndex(static_cast<asUINT>(-1))
	{
	}
};

class asCASTLifetimeView;
ANGELSCRIPTRUNTIME_API int asCASTBuildLifetimeView(
	const asCASTContext& context,
	asCASTLifetimeView& outView);

// Transient, deterministic and pointer-free. It is intentionally not owned by
// asCASTContext and has no DTO/publication API. Consumers rebuild it while
// holding an immutable snapshot lease and copy only their own detached facts.
class ANGELSCRIPTRUNTIME_API asCASTLifetimeView
{
public:
	asCASTLifetimeView();

	asDWORD GetProtocolRevision() const { return protocolRevision; }
	asQWORD GetStructuralDigest() const { return structuralDigest; }
	asUINT GetScopeEdgeCount() const { return scopeEdges.GetLength(); }
	asUINT GetRecordCount() const { return records.GetLength(); }
	asUINT GetCommitPointCount() const { return commitPoints.GetLength(); }
	asUINT GetConstructionPlanCount() const
	{
		return constructionPlans.GetLength();
	}
	asUINT GetCleanupBindingCount() const { return cleanupBindings.GetLength(); }
	asUINT GetExitPlanCount() const { return exitPlans.GetLength(); }
	asUINT GetForEachPhaseCount() const { return forEachPhases.GetLength(); }
	const asSASTLifetimeScopeEdge* GetScopeEdge(asUINT index) const;
	const asSASTLifetimeViewRecord* GetRecord(asUINT index) const;
	const asSASTLifetimeCommitPoint* GetCommitPoint(asUINT index) const;
	const asSASTLifetimeConstructionPlan* GetConstructionPlan(
		asUINT index) const;
	const asSASTLifetimeConstructionPlan* FindConstructionPlan(
		asASTDeclId constructor) const;
	const asSASTLifetimeCleanupBinding* GetCleanupBinding(asUINT index) const;
	const asSASTLifetimeCleanupBinding* FindCleanupBinding(
		asASTStmtId statement) const;
	const asSASTLifetimeExitPlan* GetExitPlan(asUINT index) const;
	const asSASTLifetimeExitPlan* FindExitPlan(
		asASTStmtId structuralOwner,
		asEASTLifetimeExitPlanKind kind) const;
	const asSASTForEachPhases* GetForEachPhases(asUINT index) const;
	const asSASTForEachPhases* FindForEachPhases(
		asASTStmtId statement) const;

private:
	friend ANGELSCRIPTRUNTIME_API int asCASTBuildLifetimeView(
		const asCASTContext& context,
		asCASTLifetimeView& outView);

	void Reset();

	asDWORD protocolRevision;
	asQWORD structuralDigest;
	asCArray<asSASTLifetimeScopeEdge> scopeEdges;
	asCArray<asSASTLifetimeViewRecord> records;
	asCArray<asSASTLifetimeCommitPoint> commitPoints;
	asCArray<asSASTLifetimeConstructionPlan> constructionPlans;
	asCArray<asSASTLifetimeCleanupBinding> cleanupBindings;
	asCArray<asSASTLifetimeExitPlan> exitPlans;
	asCArray<asSASTForEachPhases> forEachPhases;
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
