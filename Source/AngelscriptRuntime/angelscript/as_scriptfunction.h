/*
   AngelCode Scripting Library
   Copyright (c) 2003-2017 Andreas Jonsson

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any
   purpose, including commercial applications, and to alter it and
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you
      must not claim that you wrote the original software. If you use
      this software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and
      must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
      distribution.

   The original version of this library can be located at:
   http://www.angelcode.com/angelscript/

   Andreas Jonsson
   andreas@angelcode.com
*/


//
// as_scriptfunction.h
//
// A container for a compiled script function
//



#ifndef AS_SCRIPTFUNCTION_H
#define AS_SCRIPTFUNCTION_H

#include "as_config.h"
#include "as_string.h"
#include "as_array.h"
#include "as_datatype.h"
#include "as_typeinfo.h"
#include "as_atomic.h"
#include "as_buildartifact.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"

#include <mutex>
#include <memory>
#include "frontend/Basic/as_stable_key.h"

BEGIN_AS_NAMESPACE

class asCDefinitions;
class asCDefinitions;
 enum class asEFunctionIdentityKind : uint8; 

class asCScriptEngine;
class asCModule;
class asCRuntimeTypeGeneration;
class asCConfigGroup;
class asCGlobalProperty;
class asCCallableType;
struct asSNameSpace;

struct asSScriptVariable
{
	asCString   name;
	asCDataType type;
	int         stackOffset;
	//[UE++]: Carry in the stock restore-local variable storage bit without disturbing APV2 debug data ownership.
	bool        onHeap = false;
	//[UE--]
	asUINT      declaredAtProgramPos;
};

//[UE++]: Carry in the stock restore try/catch metadata container surface for later 3-way merge work.
struct asSTryCatchInfo
{
	asUINT tryPos = 0;
	asUINT catchPos = 0;
	int stackOffset = 0;
};
//[UE--]

enum asEListPatternNodeType
{
	asLPT_REPEAT,
	asLPT_REPEAT_SAME,
	asLPT_START,
	asLPT_END,
	asLPT_TYPE
};

struct asSListPatternNode
{
	asSListPatternNode(asEListPatternNodeType t) : type(t), next(0) {}
	virtual ~asSListPatternNode() {};
	virtual asSListPatternNode *Duplicate() { return asNEW(asSListPatternNode)(type); }
	asEListPatternNodeType  type;
	asSListPatternNode     *next;
};

struct asSListPatternDataTypeNode : public asSListPatternNode
{
	asSListPatternDataTypeNode(const asCDataType &dt) : asSListPatternNode(asLPT_TYPE), dataType(dt) {}
	asSListPatternNode *Duplicate() { return asNEW(asSListPatternDataTypeNode)(dataType); }
	asCDataType dataType;
};

enum asEObjVarInfoOption
{
	asOBJ_UNINIT,
	asOBJ_INIT,
	asBLOCK_BEGIN,
	asBLOCK_END,
	asOBJ_VARDECL
};

// Generation-local physical cleanup derived from the verifier-authenticated
// Canonical constructor lifetime protocol. The semantic protocol remains
// snapshot-owned and pointer-free; these values exist only in an installed VM
// generation and are rebuilt/resolved when bytecode is restored.
enum asEConstructorCleanupAction
{
	asCONSTRUCTOR_CLEANUP_INVALID = 0,
	asCONSTRUCTOR_CLEANUP_DESTROY_VALUE,
	asCONSTRUCTOR_CLEANUP_RELEASE_REFERENCE,
};

struct asSConstructorCleanupInfo
{
	asCTypeInfo* objectType = 0;
	int actionFunctionId = 0;
	int objectOffset = 0;
	asEConstructorCleanupAction action = asCONSTRUCTOR_CLEANUP_INVALID;
	asBYTE phase = 0;
	bool completeObject = false;
};

// Generation-local physical cleanup for one homogeneous Canonical
// initializer-list/array aggregate. The sealed AST owns the exact semantic
// action and ordered element commits; the VM owns only this resolved buffer
// layout and reads the committed count stored in the list buffer itself.
// None of these Runtime pointers/offsets are persisted as Canonical identity.
struct asSAggregateCleanupInfo
{
	int bufferOffset = 0;
	asCTypeInfo* elementType = 0;
	int actionFunctionId = 0;
	asUINT committedCountOffset = 0;
	asUINT firstElementOffset = 0;
	asUINT elementStride = 0;
	asUINT maxElementCount = 0;
};

enum asEFuncTrait
{
	asTRAIT_CONSTRUCTOR = 0x1,
	asTRAIT_DESTRUCTOR  = 0x2,
	asTRAIT_CONST       = 0x4,
	asTRAIT_PRIVATE     = 0x8,
	asTRAIT_PROTECTED   = 0x10,
	asTRAIT_FINAL       = 0x20,
	asTRAIT_OVERRIDE    = 0x40,
	asTRAIT_PROPERTY    = 0x200,
	asTRAIT_IMPLICITCONSTRUCTOR = 0x400,
	asTRAIT_MIXIN = 0x800,
	asTRAIT_LOCAL = 0x1000,
	asTRAIT_NODISCARD = 0x2000,
	asTRAIT_DEPRECATED = 0x4000,
	asTRAIT_GENERIC_TEMPLATE_FUNCTION = 0x8000,
	asTRAIT_USES_WORLDCONTEXT = 0x10000,
	asTRAIT_ACCEPT_TEMPORARY_OBJECT = 0x20000,
	asTRAIT_GENERATED_FUNCTION = 0x40000,
	asTRAIT_NOT_CALLABLE = 0x80000,
	asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS = 0x100000,
	asTRAIT_EXTERNAL_IMPLICIT_THIS = 0x200000,
	asTRAIT_ALLOWDISCARD = 0x400000,
	asTRAIT_EDITOR_ONLY = 0x800000,
	//[UE++]: Reintroduce the frozen explicit-trait bit needed by the restore prerequisite surface.
	asTRAIT_EXPLICIT = 0x1000000,
	asTRAIT_UNSAFE_DURING_CONSTRUCTION = 0x2000000,
	asTRAIT_DEFAULTS_ONLY = 0x4000000,
	//[UE--]
};

enum class asECompileOutType
{
	CompileCalls,
	CompileOutEntirely,
	ReplaceWithFirstParam,
	CompileOutAsMethodChain,
};

struct asSFunctionTraits
{
	asSFunctionTraits() : traits(0) {}
	void SetTrait(asEFuncTrait trait, bool set) { if (set) traits |= trait; else traits &= ~trait; }
	bool GetTrait(asEFuncTrait trait) const { return (traits & trait) ? true : false; }
//protected:
	asDWORD traits;
};

struct asSObjectVariableInfo
{
	asUINT              programPos;
	int                 variableOffset;
	asEObjVarInfoOption option;
};

struct asSSystemFunctionInterface;

// TODO: Might be interesting to allow enumeration of accessed global variables, and
//       also functions/methods that are being called. This could be used to build a
//       code database with call graphs, etc.

void RegisterScriptFunction(asCScriptEngine *engine);

//[UE++]: Rename export macro to match the runtime module rename.
class ANGELSCRIPTRUNTIME_API asCScriptFunction final : public asIScriptFunction
//[UE--]
{
public:
	// From asIScriptFunction
	asIScriptEngine     *GetEngine() const;

	// Memory management
	int AddRef() const;
	int Release() const;

	// Miscellaneous
	int                  GetId() const;
	asEFuncType          GetFuncType() const;
	const char          *GetScriptSectionName() const;
	const char          *GetConfigGroup() const;
	asDWORD              GetAccessMask() const;
	void                *GetAuxiliary() const;
	bool IsNoOp() const;

	static asUINT GenerateExposedType(bool editable, bool readable, bool writeable);

	// Function signature
	asITypeInfo         *GetObjectType() const;
	const char          *GetObjectName() const;
	const char          *GetName() const;
	const char          *GetNamespace() const;
	const char          *GetDeclaration(bool includeObjectName = true, bool includeNamespace = false, bool includeParamNames = false, bool forDisplay = false) const;
	bool                 IsReadOnly() const;
	bool                 IsSetterAndOnlyValidDuringInitDefaults() const;
	bool                 IsPrivate() const;
	bool                 IsProtected() const;
	bool                 IsFinal() const;
	bool                 IsOverride() const;
	bool                 IsProperty() const;
	bool                 IsMixin() const;
	asUINT               GetParamCount() const;
	int                  GetParam(asUINT index, int *typeId, asDWORD *flags = 0, const char **name = 0, const char **defaultArg = 0) const;
	int                  GetReturnTypeId(asDWORD *flags = 0) const;

	// Type id for function pointers
	int                  GetTypeId() const;
	bool                 IsCompatibleWithTypeId(int typeId) const;

	// Delegates
	void                *GetDelegateObject() const;
	asITypeInfo         *GetDelegateObjectType() const;
	asIScriptFunction   *GetDelegateFunction() const;

	// Debug information
	asUINT               GetVarCount() const;
	int                  GetVar(asUINT index, const char **name, int *typeId = 0) const;
	const char *         GetVarDecl(asUINT index, bool includeNamespace = false) const;
	int                  FindNextLineWithCode(int line) const;

	// For JIT compilation
	asDWORD             *GetByteCode(asUINT *length = 0);
	TConstArrayView<asDWORD> GetStableByteCode() const;
	void SetStableByteCode(TArray<asDWORD> Bytes);

	// User data
	void                *SetUserData(void *userData, asPWORD type);
	void                *GetUserData() const { return userData; }

	void* DebugPrototypePtr = nullptr;

public:
	//-----------------------------------
	// Internal methods

	void SetReadOnly(bool set) { traits.SetTrait(asTRAIT_CONST, set); }
	void SetFinal(bool set) { traits.SetTrait(asTRAIT_FINAL, set); }
	void SetOverride(bool set) { traits.SetTrait(asTRAIT_OVERRIDE, set); }
	void SetProtected(bool set) { traits.SetTrait(asTRAIT_PROTECTED, set); }
	void SetPrivate(bool set) { traits.SetTrait(asTRAIT_PRIVATE, set); }
	void SetProperty(bool set) { traits.SetTrait(asTRAIT_PROPERTY, set); }
	void SetMixin(bool set) { traits.SetTrait(asTRAIT_MIXIN, set); }
	void SetExplicit(bool set) { traits.SetTrait(asTRAIT_EXPLICIT, set); }

	asCScriptFunction(asCScriptEngine *engine, asCModule *mod, asEFuncType funcType);
	~asCScriptFunction();
	const asSStableKey& GetStableKey() const { return stableKey; }
	asETypeInfoKind GetTypeInfoKind() const { return typeInfoKind; }
	asEFunctionIdentityKind GetDeclarationKind() const { return static_cast<asEFunctionIdentityKind>(declarationKind); }
	std::shared_ptr<asSSystemFunctionInterface> GetProcessInterface() const { return processInterface; }
	bool IsMetadataStatic() const { return metadataStaticDeclaration; }
	bool IsMetadataLocal() const { return traits.GetTrait(asTRAIT_LOCAL); }
	bool IsMetadataCallableSignature() const { return metadataCallableSignature; }
	asCDefinitions* GetDefinitions() const { return definitions; }
	const asCDataType& GetReturnDataType() const { return returnType; }
	const asCDataType* GetParameterDataType(asUINT index) const
	{
		return index < parameterTypes.GetLength() ? &parameterTypes[index] : nullptr;
	}

	// Keep an internal reference counter to separate references coming from
	// application or script objects and references coming from the script code
	int AddRefInternal();
	int ReleaseInternal();
	// Canonical functions borrow their owning generation while module-owned.
	// A 0->1 external reference transition acquires a generation execution
	// lease; the matching 1->0 transition releases it. This keeps the complete
	// retired type/global/TypeId environment alive for prepared contexts without
	// creating a function-generation ownership cycle.
	void SetRuntimeTypeGeneration(asCRuntimeTypeGeneration* generation);

	void     DestroyHalfCreated();

	// TODO: operator==
	// TODO: The asIScriptFunction should provide operator== and operator!= that should do a
	//       a value comparison. Two delegate objects that point to the same object and class method should compare as equal
	// TODO: The operator== should also be provided in script as opEquals to allow the same comparison in script
	//       To do this we'll need some way to adapt the argtype for opEquals for each funcdef, preferrably without instantiating lots of different methods
	//       Perhaps reusing 'auto' to mean the same type as the object
	//bool      operator==(const asCScriptFunction &other) const;

	void      DestroyInternal();

	void      AddVariable(asCString &name, asCDataType &type, int stackOffset);
	void      AddVariable(const asCString &name, const asCDataType &type, int stackOffset, bool onHeap);

	int       GetSpaceNeededForArguments();
	int       GetSpaceNeededForReturnValue();
	asCString GetDeclarationStr(bool includeObjectName = true, bool includeNamespace = false, bool includeParamNames = false, bool forDisplay = false, bool includeDefaultValues = true) const;
	int       GetLineNumber(int programPosition, int *sectionIdx);
	void      ComputeSignatureId();
	bool      IsSignatureEqual(const asCScriptFunction *func) const;
	bool      IsSignatureExceptNameEqual(const asCScriptFunction *func) const;
	bool      IsSignatureExceptNameEqual(const asCDataType &retType, const asCArray<asCDataType> &paramTypes, const asCArray<asETypeModifiers> &inOutFlags, const asCObjectType *type, bool isReadOnly) const;
	bool      IsSignatureExceptNameAndReturnTypeEqual(const asCScriptFunction *fun) const;
	bool      IsSignatureExceptNameAndReturnTypeEqual(const asCArray<asCDataType> &paramTypes, const asCArray<asETypeModifiers> &inOutFlags, const asCObjectType *type, bool isReadOnly) const;
	bool      IsSignatureExceptNameAndObjectTypeEqual(const asCScriptFunction *func) const;

	asCTypeInfo *GetTypeInfoOfLocalVar(short varOffset);

	void      MakeDelegate(asCScriptFunction *func, void *obj);

	bool      DoesReturnOnStack() const;

	void      JITCompile();
	virtual int SetJITBinding(const asSJITFunctionBinding &binding);
	virtual asSJITFunctionBinding GetJITBinding() const;
	// Internal execution lease. Retired bindings are released only after the
	// last active reader exits, so UserData and provider code stay reachable.
	asSJITFunctionBinding AcquireJITBindingForExecution() const;
	void ReleaseJITBindingAfterExecution() const;

	void      AddReferences(
		const asSBuildArtifactRestoreContext *restoreContext = 0);
	void      ReleaseReferences(
		const asSBuildArtifactRestoreContext *restoreContext = 0);

	void      AllocateScriptFunctionData();
	void      DeallocateScriptFunctionData();

	asCGlobalProperty *GetPropertyByGlobalVarPtr(void *gvarPtr);

	// GC methods (for delegates)
	int  GetRefCount();
	void SetFlag();
	bool GetFlag();
	void EnumReferences(asIScriptEngine *engine);
	void ReleaseAllHandles(asIScriptEngine *engine);

	void CalculateParameterOffsets();

public:
	//-----------------------------------
	// Properties

	mutable asCAtomic            externalRefCount; // Used for external referneces
	        asCAtomic            internalRefCount; // Used for internal references
	asCScriptEngine             *engine;
	asCModule                   *module;
	asCRuntimeTypeGeneration    *runtimeTypeGeneration;

	// Function signature
	asCString                      name;
	asCDataType                  returnType;
	asCArray<asCDataType>        parameterTypes;
	asCArray<asCDataType>        templateSubTypes;
	asCArray<asCString>          parameterNames;
	asCArray<asETypeModifiers>   inOutFlags;
	asCArray<asCString *>        defaultArgs;
	asCObjectType               *objectType;
	void* userData;
	asUINT                       exposedType;
	asDWORD                      accessMask = 0xFFFFFFFF;
	int                          signatureId = 0;
	asSAccessSpecifier*          accessSpecifier;

	asCArray<int>				 parameterOffsets;
	asCString                    hiddenArgumentDefault;

	// Namespace will be null for funcdefs that are declared as child funcdefs
	// of a class. In this case the namespace shall be taken from the parentClass
	// in the funcdefType
	asSNameSpace                *nameSpace;

	// Used by asFUNC_DELEGATE
	static asCCallableType              *callableType; // Doesn't increase refCount
	void              *objForDelegate;
	asCScriptFunction *funcForDelegate;

	// Used by list factory behaviour
	asSListPatternNode *listPattern;

	// The complete JIT Binding is retired and published only through the
	// lifecycle methods. Keeping it private prevents entry generations from
	// being mutated independently by Runtime consumers.
private:
	asSJITFunctionBinding           jitBinding;
	asIJITCompiler                 *jitBindingOwner = 0;
	struct SRetiredJITBinding
	{
		asSJITFunctionBinding Binding;
		asIJITCompiler *Owner = 0;
	};
	mutable std::mutex              jitBindingMutex;
	mutable asUINT                  activeJITBindingReaders = 0;
	mutable asCArray<SRetiredJITBinding> retiredJITBindings;
public:

	// Used by asFUNC_SCRIPT
	struct ScriptFunctionData
	{
		// A bytecode global-pointer instruction normally resolves its property
		// through the engine's address map. A different module can be discarded
		// and swept before this function is retired, however, so keep the exact
		// property references acquired while AddReferences() still has that map.
		// The bytecode offset distinguishes a property pointer from a string
		// constant at ReleaseReferences() time after the provider has gone away.
		struct ScriptGlobalPropertyReference
		{
			asUINT byteCodeOffset = 0;
			asCGlobalProperty* property = 0;
		};

		// Bytecode for the script function
		asCArray<asDWORD>               byteCode;
		asCArray<ScriptGlobalPropertyReference> globalPropertyReferences;

		// The stack space needed for the local variables
		asDWORD                         variableSpace;

		// These hold information on objects and function pointers, including temporary
		// variables used by exception handler and when saving bytecode
		asCArray<asCTypeInfo*>          objVariableTypes;
		asCArray<int>                   objVariablePos;

		// The first variables in above array are allocated on the heap, the rest on the stack.
		// This variable shows how many are on the heap.
		asUINT                          objVariablesOnHeap;

		// Holds information on scope for object variables on the stack
		asCArray<asSObjectVariableInfo> objVariableInfo;

		// Canonical constructors publish one linear physical cleanup directory in
		// semantic construction order. constructorCommittedCountOffset names a
		// private VM-frame dword that is written only after an individual step
		// succeeds. Exception cleanup consumes exactly that committed prefix in
		// reverse. A zero offset means this is not a Canonical constructor frame.
		asCArray<asSConstructorCleanupInfo> constructorCleanupInfo;
		int constructorCommittedCountOffset = 0;

		// Canonical aggregate buffers participate in the ordinary heap-object
		// directory so their cleanup remains globally reverse-ordered with every
		// other local. Entries here replace the list-pattern byte heuristic with
		// one exact destructor and a buffer-owned committed-prefix cursor.
		asCArray<asSAggregateCleanupInfo> aggregateCleanupInfo;

		// Canonical constructor factories keep their raw allocation out of the
		// ordinary object-variable directory: a partially constructed object must
		// never be retired through its complete-object destructor. The constructor
		// frame has already executed the committed-prefix cleanup before this
		// factory frame is unwound.
		asCTypeInfo* partialConstructionObjectType = 0;
		int partialConstructionObjectOffset = 0;
		int partialConstructionConstructorId = 0;

		asCArray<asSTryCatchInfo>       tryCatchInfo;

		// The stack needed to execute the function
		int                             stackNeeded;

		// Hold debug information about temporary variables and their types
		struct ScriptTempVariable
		{
			int Offset;
			eTokenType Token;
		};
		asCArray<ScriptTempVariable>    temporaryVariables;

		// Ownership-neutral pointer slots used by compiler-generated VM helpers.
		// These offsets are intentionally separate from objVariablePos: entries in
		// that table participate in exception cleanup, while these slots may only
		// borrow storage that CALLSYS or another callee owns. JIT backends still
		// need their pointer representation to interpret ClrVPtr/PshVPtr/PSF.
		asCArray<int>                   pointerVariablePos;

		// Holds debug information on explicitly declared variables
		asCArray<asSScriptVariable*>    variables;
		// Store position, line number pairs for debug information
		asCArray<int>                   lineNumbers;
		// Store the script section where the code was declared
		int                             scriptSectionIdx;
		// Store the location where the function was declared  (row in the lower 20 bits, and column in the upper 12)
		int                             declaredAt;
		// Store position/index pairs if the bytecode is compiled from multiple script sections
		asCArray<int>                   sectionIdxs;

		// Canonical token slice retained for stable external artifact identity. This
		// contains no comments or whitespace and is independent of source line moves.
		asCString                       artifactCanonicalSource;

		// Raw engine-local dependencies observed by the authoritative compiler for
		// this successful invocation. Failed and NotCacheable invocations leave this
		// empty. The Runtime bridge maps these pointers to stable semantic keys before
		// anything is persisted.
		asCArray<asSBuildArtifactDependency> artifactDependencies;
	};
	ScriptFunctionData          *scriptData;
	TArray<asDWORD>              stableByteCode;

	// Used by asFUNC_SYSTEM
	asSSystemFunctionInterface  *sysFuncIntf;

	// Used by asFUNC_VIRTUAL
	int                          vfTableIdx;

	int spaceNeededForArguments = 0;
	int totalSpaceBeforeFunction = 0;
	int8 hiddenArgumentIndex = -1;
	int8 determinesOutputTypeArgumentIndex = -1;

	int                          id;
	asEFuncType                  funcType;
	asSFunctionTraits            traits;
	//[UE++]: Current-Engine declaration metadata for Cache V2 stable identity.
	// The owner pointer is transient and is never serialized; the Runtime maps it
	// to a stable TypeKey before any artifact is persisted.
	asEBuildArtifactInvocationKind artifactInvocationKind =
		asBUILD_ARTIFACT_INVOCATION_INVALID;
	asCTypeInfo* artifactOwnerType = nullptr;
	// Exact address-free identity of the sealed canonical declaration related to
	// this function. Source CodeGen assigns script shells directly; Canonical
	// Sema assigns registered system functions when it projects their exact
	// external declaration; Cache V2 may assign script shells only after
	// validating the restored AST against the live VM skeleton. It is
	// deliberately not serialized and never stores a DeclId or AST pointer whose
	// meaning is local to one context.
	asCString canonicalASTStableDeclKey;
	// Exact source-facing qualifier bits for each authored formal, indexed by
	// source formal ordinal. Script Runtime ABI normalization is intentionally
	// non-invertible (`T` and explicit `const T&inout` may share one shell
	// datatype), so Canonical producers must carry this structured distinction
	// instead of asking a later consumer to infer it from the live ABI. Hidden
	// lambda/call-adapter parameters are not source formals and are excluded.
	// The array is generation-local, contains no AST ids or Engine numeric type
	// ids, and is rebuilt from a sealed AST after Cache restore rather than
	// serialized as an independent authority.
	asCArray<asDWORD> canonicalASTSourceParameterQualifiers;
	//[UE--]
	asECompileOutType            compileOutType = asECompileOutType::CompileCalls;

	// Stub functions and delegates don't own the object and parameters
	mutable bool                 gcFlag;
	bool                         dontCleanUpOnException : 1;
	bool                         isInUse : 1;

#if WITH_EDITOR
	asCString deprecationMessage;
#endif

private:
	friend class asCDefinitions;
	asCScriptFunction(asCDefinitions& owner, const asSStableKey& key, asEFuncType funcType);
	asCDefinitions* definitions = nullptr;
	asETypeInfoKind typeInfoKind = asETypeInfoKind::ScriptEngine;
	std::shared_ptr<asSSystemFunctionInterface> processInterface;
	asSStableKey stableKey;
	mutable std::mutex metadataReferenceMutex;

	bool metadataSignatureDefined = false;
	bool metadataCallableSignature = false;
	asCCallableType* metadataNominalCallableType = nullptr;
	asBYTE declarationKind = 0;
	asUINT genericArity = 0;
	asCTypeInfo* declarationOwner = nullptr;
	bool metadataStaticDeclaration = false;

};

const char * const DELEGATE_FACTORY = "$dlgte";
AS_API asCScriptFunction *CreateDelegate(asCScriptFunction *func, void *obj);

END_AS_NAMESPACE

#endif
