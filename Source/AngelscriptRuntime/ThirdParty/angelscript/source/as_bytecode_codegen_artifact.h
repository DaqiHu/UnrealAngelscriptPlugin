#ifndef AS_BYTECODE_CODEGEN_ARTIFACT_H
#define AS_BYTECODE_CODEGEN_ARTIFACT_H

#include "as_array.h"
#include "as_config.h"
#include "as_runtime_type_binding.h"

BEGIN_AS_NAMESPACE

class asCScriptEngine;
class asCScriptFunction;
class asCGlobalProperty;
class asCFuncdefType;
class asCTypeInfo;
class asCObjectType;
class asCModule;
struct sBindInfo;

enum asEBytecodeCodeGenSyntheticFunctionKind
{
	asBYTECODE_CODEGEN_GLOBAL_INITIALIZER = -1,
	asBYTECODE_CODEGEN_CONSTRUCTOR_FACTORY = -2,
};

struct asSCanonicalConstructorFactoryPair
{
	asCScriptFunction* constructor = 0;
	asCScriptFunction* factory = 0;
	asCObjectType* objectType = 0;
};

// Snapshot-local declaration identity bound to one detached Runtime object
// shell. The numeric declaration id is meaningful only while Generate retains
// the owning sealed Context; neither it nor the Runtime pointer is serialized.
struct asSCanonicalObjectTypeBinding
{
	asUINT declarationId = 0;
	asCObjectType* objectType = 0;
};

struct asSCanonicalFunctionVTableIndex
{
	asCScriptFunction* function = 0;
	int index = -1;
};

// Fully prepared generation-local projection of one Canonical record. Every
// array is non-owning until Commit swaps it into the object shell and acquires
// the normal method/vtable references. Preparation therefore cannot leak a
// partial Runtime graph, while Abandon remains symmetric after installation.
struct asSCanonicalObjectDispatchPlan
{
	asCObjectType* objectType = 0;
	bool isInterface = false;
	asCArray<int> methodIds;
	asCArray<asCScriptFunction*> methodFunctions;
	asCArray<asCObjectType*> interfaces;
	asCArray<asUINT> interfaceVFTOffsets;
	asCArray<asCScriptFunction*> virtualFunctionTable;
	asUINT ordinaryVTableSize = 0;
	asCArray<asSCanonicalFunctionVTableIndex> functionVTableIndices;
};

// Detached CodeGen install record. Included only by as_bytecode_codegen.cpp.
// Tests observe Generate through module/engine tables, not this type.
struct asSBytecodeCodeGenArtifact
{
	asCArray<asCScriptFunction*> functions;
	// Parallel to functions: raw asEASTDeclKind values, kept header-light.
	// Negative values are the explicit synthetic kinds above. Initializers and
	// constructor factories both need engine function ids, but neither is a
	// source-callable global declaration.
	asCArray<int> functionDeclKinds;
	asCArray<asSCanonicalConstructorFactoryPair> constructorFactories;
	asCArray<asCGlobalProperty*> globals;
	asCArray<sBindInfo*> imports;
	asCArray<asCFuncdefType*> funcdefs;
	asCArray<asCTypeInfo*> types;
	// Exact detached record bindings and their complete pre-publication dispatch
	// plans. Both collections are generation-local and deliberately pointerful;
	// stable Canonical identity remains in the sealed snapshot.
	asCArray<asSCanonicalObjectTypeBinding> objectTypeBindings;
	asCArray<asSCanonicalObjectDispatchPlan> objectDispatchPlans;
	bool objectDispatchPlansInstalled = false;
	// Pointer-free relocation inventory produced before installation. The
	// generation-local table is frozen only after every record resolves. The
	// function body array is deliberately non-owning: prepared-module bodies are
	// Stage-2 shells, while fully detached generation owns the same pointers in
	// `functions`. Relocation records address this array only by ordinal.
	asCArray<asCScriptFunction*> relocationFunctions;
	asCArray<asSTypeRelocation> typeRelocations;
	asCRuntimeTypeBindingTable runtimeTypeBindings;
	// Non-owning late-projection plan. This is intentionally distinct from
	// `types`: prepared-module TypeInfos are Stage-2-owned, yet bytecode property
	// operands can still require a numeric public TypeId. Canonical stable keys
	// remain the identity; these pointers exist only inside one build-lock-held
	// publication transaction.
	asCArray<asCTypeInfo*> pendingTypeIdTypes;
	int pendingTypeIdSequenceStart = -1;
	asUINT pendingTypeIdCount = 0;
	bool pendingTypeIdsPublished = false;
	// GetListPatternType materializes an anonymous Engine-owned cleanup helper.
	// Track only helpers created by this candidate so Abandon can restore the
	// cache exactly; pre-existing helpers remain shared Engine state.
	asCArray<asCObjectType*> createdListPatternTypes;
	// Set only after Commit() has called AddReferences() for every ordinary and
	// factory function. Abandon() uses it to reverse those edges before removing
	// engine slots, preserving transaction rollback for cross-function bytecode.
	bool functionReferencesAdded = false;

	// Reserve an id in the candidate's local ordering without changing the
	// engine's scriptFunctions/freeScriptFunctionIds tables. The build lock
	// guarantees that no other compilation can consume the same id before this
	// artifact either commits or abandons.
	int GetNextPendingScriptFunctionId(const asCScriptEngine* engine) const;
	// Imported functions use the engine's separate LIFO slot allocator. They
	// are also candidate-only during emission: CALLBND carries the resulting
	// flagged id without requiring an engine importedFunctions insertion.
	int GetNextPendingImportedFunctionId(const asCScriptEngine* engine) const;
	// Candidate global properties own real storage for emitted LDG pointers,
	// but their globalProperties slot, module lists, and varAddressMap entry are
	// deferred until Commit().
	int GetNextPendingGlobalPropertyId(const asCScriptEngine* engine) const;
	// Assign candidate-local numeric projections without mutating the Engine's
	// sequence/map. The plan can include both artifact-owned and prepared
	// non-owning types; CommitPendingTypeIds publishes the complete projection.
	int PreparePendingTypeIds(asCScriptEngine* engine);
	int CommitPendingTypeIds(asCScriptEngine* engine);
	// Return the stable candidate-local ordinal used by pointer-free relocation
	// records. This does not acquire a function reference or installation owner.
	asUINT GetOrAddRelocationFunction(asCScriptFunction* function);
	// Resolve all stable identities and patch only detached candidate bytecode.
	// Any failure occurs before Commit mutates an Engine/module table.
	int ResolveAndPatchTypeRelocations(
		asCScriptEngine* engine,
		const asCASTContext& context,
		const asSRuntimeTypeBindingEnvironment& environment,
		asCString* detail = 0
#if WITH_ANGELSCRIPT_UNITTESTS
		, asUINT testFailureAtRelocationUse = asTYPE_RELOC_USE_COUNT,
		int testFailureCode = asERROR,
		bool* outTestFailureInjected = 0
#endif
		);
	void Abandon(asCScriptEngine* engine);
	// Publish an already-emitted single-function closure into Engine function-id
	// slots without adding it to the owner module. The primary function retains
	// its construction reference for the CompileFunction caller; nested emitted
	// functions keep only real bytecode/signature references.
	int CommitDetachedSingleFunction(
		asCModule* diagnosticOwner,
		asCScriptFunction* primaryFunction);
	int  Commit(asCModule* module, bool publishAggregateRuntimeBindings
#if WITH_ANGELSCRIPT_UNITTESTS
		, asUINT testFailureAfterCommitStage, int testFailureCode,
		bool* outTestFailureInjected
#endif
	);
};

END_AS_NAMESPACE

#endif
