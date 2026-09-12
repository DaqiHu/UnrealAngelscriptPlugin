/*
   AngelCode Scripting Library
   Copyright (c) 2003-2018 Andreas Jonsson

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
// as_scriptengine.h
//
// The implementation of the script engine interface
//



#ifndef AS_SCRIPTENGINE_H
#define AS_SCRIPTENGINE_H

#include "as_config.h"
#include "as_atomic.h"
#include "as_scriptfunction.h"
#include "as_array.h"
#include "as_datatype.h"
#include "as_objecttype.h"
#include "as_module.h"
#include "as_callfunc.h"
#include "as_configgroup.h"
#include "as_memory.h"
#include "as_gc.h"
#include "as_tokendef.h"
#include "as_module_definition_set.h"
#include "Templates/UniquePtr.h"

BEGIN_AS_NAMESPACE

class asCByteCodeImage;
class asCExecutableSnapshot;
class asCExecutableFunction;

enum class asEByteCodeLinkStatus : asBYTE
{
	Succeeded,
	InvalidArgument,
	Unverified,
	MissingDeclaration,
	SchemaMismatch,
	BodyConflict,
	ResourceFailure,
	MissingBinding,
	Retired
};

enum asECompilerPipeline
{
	asCOMPILER_PIPELINE_LEGACY = 0,
	asCOMPILER_PIPELINE_CANONICAL = 1,
};

class asCBuilder;
class asCContext;

// A batch requires the exact installed declaration as well as its stable key.
struct asSNativeFunctionBinding
{
	asCScriptFunction* Declaration = nullptr;
	asSFuncPtr Function;
	asDWORD CallConv = asCALL_CDECL;
	asFunctionCaller Caller;
	void* Auxiliary = nullptr;
	int BaseOffset = 0;
	asEFirstParamMetaData FirstParameter = asEFirstParamMetaData::None;
	int NativeParameterCount = -1;
};
struct asSNativeGlobalBinding
{
	asCGlobalProperty* Declaration = nullptr;
	void* Storage = nullptr;
};

struct asSCanonicalTemplateDeclarationFact
{
	asCString stableKey;
	asQWORD flags;
	asUINT subtypeCount;
	bool acceptValueSubtype;
	bool acceptReferenceSubtype;
	bool requiresRuntimeValidation;
	asCArray<asCString> childCallableTypes;

	asSCanonicalTemplateDeclarationFact()
		: flags(0)
		, subtypeCount(0)
		, acceptValueSubtype(true)
		, acceptReferenceSubtype(true)
		, requiresRuntimeValidation(false)
	{
	}
};

struct asSCanonicalRegisteredEnumeratorFact
{
	asCString name;
	int value;

	asSCanonicalRegisteredEnumeratorFact()
		: value(0)
	{
	}
};

struct asSCanonicalRegisteredTypeRefFact
{
	asEASTTypeKind kind;
	eTokenType primitiveToken;
	asCString stableKey;
	asDWORD quals;

	asSCanonicalRegisteredTypeRefFact()
		: kind(asAST_TYPE_INVALID)
		, primitiveToken(ttUnrecognizedToken)
		, quals(0)
	{
	}

	bool Equals(const asSCanonicalRegisteredTypeRefFact& other) const
	{
		return kind == other.kind
			&& primitiveToken == other.primitiveToken
			&& quals == other.quals
			&& stableKey.Equals(other.stableKey);
	}
};

struct asSCanonicalRegisteredTypeDeclarationFact
{
	asCString stableKey;
	asEASTTypeKind kind;
	asQWORD flags;
	bool isInterface;
	bool ambiguous;
	asCArray<asCString> baseStableKeys;
	asCArray<asSCanonicalRegisteredEnumeratorFact> enumerators;
	asSCanonicalRegisteredTypeRefFact callableReturn;
	asCArray<asSCanonicalRegisteredTypeRefFact> callableFormals;

	asSCanonicalRegisteredTypeDeclarationFact()
		: kind(asAST_TYPE_INVALID)
		, flags(0)
		, isInterface(false)
		, ambiguous(false)
	{
	}
};

// TODO: import: Remove this when import is removed
struct sBindInfo;

//[UE++]: Rename export macro to match the runtime module rename.
class ANGELSCRIPTRUNTIME_API asCScriptEngine final : public asIScriptEngine
//[UE--]
{
//=============================================================
// From asIScriptEngine
//=============================================================
public:
	// Memory management
	virtual int AddRef() const;
	virtual int Release() const;
	virtual int ShutDownAndRelease();

	// Engine properties
	virtual int     SetEngineProperty(asEEngineProp property, asPWORD value);
	virtual asPWORD GetEngineProperty(asEEngineProp property) const;

	// Compiler messages
	virtual int SetMessageCallback(const asSFuncPtr &callback, void *obj, asDWORD callConv);
	virtual int GetMessageCallback(asSFuncPtr *callback, void **obj, asDWORD *callConv);
	virtual int ClearMessageCallback();
	virtual int WriteMessage(const char *section, int row, int col, asEMsgType type, const char *message);

	// JIT Compiler
	virtual int SetJITCompiler(asIJITCompiler *compiler);
	virtual asIJITCompiler *GetJITCompiler() const;

	// Global functions
	virtual int                RegisterGlobalFunction(const char *declaration, const asSFuncPtr &funcPointer, asDWORD callConv, asFunctionCaller caller = nullptr, void *auxiliary = 0);
	virtual asUINT             GetGlobalFunctionCount() const;
	virtual asIScriptFunction *GetGlobalFunctionByIndex(asUINT index) const;
	virtual asIScriptFunction *GetGlobalFunctionByDecl(const char *declaration) const;
	void DeprecateGlobalFunctionsByName(const char *name, const char* deprecationMessage);

	// Global properties
	virtual int    RegisterGlobalProperty(const char *declaration, void *pointer);
	virtual asUINT GetGlobalPropertyCount() const;
	virtual int    GetGlobalPropertyByIndex(asUINT index, const char **name, const char **nameSpace = 0, int *typeId = 0, bool *isConst = 0, const char **configGroup = 0, void **pointer = 0, asDWORD *accessMask = 0) const;

	// Type registration
	//[UE++]: Widen object flags to preserve APV2 private high bits on the stock 2.38 layout.
	virtual int            RegisterObjectType(const char *obj, int byteSize, asQWORD flags);
	//[UE--]
	virtual int            RegisterObjectProperty(const char *obj, const char *declaration, int byteOffset, int compositeOffset = 0, bool isCompositeIndirect = false, const asUINT accessorType = 255, bool isProtected = false);
	virtual int            RegisterObjectMethod(const char *obj, const char *declaration, const asSFuncPtr &funcPointer, asDWORD callConv, asFunctionCaller caller = nullptr, void *auxiliary = 0, int compositeOffset = 0, bool isCompositeIndirect = false, const asUINT accessorType = 255);
	virtual int            RegisterObjectBehaviour(const char *obj, asEBehaviours behaviour, const char *declaration, const asSFuncPtr &funcPointer, asDWORD callConv, asFunctionCaller caller = nullptr, void *auxiliary = 0, int compositeOffset = 0, bool isCompositeIndirect = false);
	virtual int            RegisterInterface(const char *name);
	virtual int            RegisterInterfaceMethod(const char *intf, const char *declaration);
	virtual asUINT         GetObjectTypeCount() const;
	virtual asITypeInfo   *GetObjectTypeByIndex(asUINT index) const;

	// String factory
	virtual int RegisterStringFactory(const char *datatype, asIStringFactory *factory);
	virtual int GetStringFactoryReturnTypeId(asDWORD *flags) const;

	// Default array type
	virtual int RegisterDefaultArrayType(const char *type);
	virtual int GetDefaultArrayTypeId() const;

	// Enums
	virtual int          RegisterEnum(const char *type);
	virtual int          RegisterEnumValue(const char *type, const char *name, int value);
	virtual asUINT       GetEnumCount() const;
	virtual asITypeInfo *GetEnumByIndex(asUINT index) const;

	// Funcdefs

	// Typedefs
	// TODO: interface: Should perhaps rename this to Alias, since it doesn't really create a new type
	virtual int          RegisterTypedef(const char *type, const char *decl);
	virtual asUINT       GetTypedefCount() const;
	virtual asITypeInfo *GetTypedefByIndex(asUINT index) const;

	// Configuration groups
	virtual int         BeginConfigGroup(const char *groupName);
	virtual int         EndConfigGroup();
	virtual int         RemoveConfigGroup(const char *groupName);
	virtual asDWORD     SetDefaultAccessMask(asDWORD defaultMask);
	virtual int         SetDefaultNamespace(const char *nameSpace);
	virtual const char *GetDefaultNamespace() const;

	// Script modules

	// Script functions
	virtual asIScriptFunction *GetFunctionById(int funcId) const;

	// Type identification
	virtual int            GetTypeIdByDecl(const char *decl) const;
	virtual const char    *GetTypeDeclaration(int typeId, bool includeNamespace = false) const;
	virtual int            GetSizeOfPrimitiveType(int typeId) const;
	virtual asITypeInfo   *GetTypeInfoById(int typeId) const;
	virtual asITypeInfo   *GetTypeInfoByName(const char *name) const;
	virtual asITypeInfo   *GetTypeInfoByDecl(const char *decl) const;

	asITypeInfo   *GetTypeInfo(const char *name, asSNameSpace* ns) const;

	// Script execution
	virtual asIScriptContext      *CreateContext();
	virtual void                  *CreateScriptObject(const asITypeInfo *type);
	virtual void                  *CreateScriptObjectCopy(void *obj, const asITypeInfo *type);
	virtual void                  *CreateUninitializedScriptObject(const asITypeInfo *type);
	virtual asIScriptFunction     *CreateDelegate(asIScriptFunction *func, void *obj);
	virtual int                    AssignScriptObject(void *dstObj, void *srcObj, const asITypeInfo *type);
	virtual void                   ReleaseScriptObject(void *obj, const asITypeInfo *type);
	virtual void                   AddRefScriptObject(void *obj, const asITypeInfo *type);
	virtual int                    RefCastObject(void *obj, asITypeInfo *fromType, asITypeInfo *toType, void **newPtr, bool useOnlyImplicitCast = false);
	virtual asILockableSharedBool *GetWeakRefFlagOfScriptObject(void *obj, const asITypeInfo *type) const;

	// Context pooling
	virtual asIScriptContext *RequestContext();
	virtual void              ReturnContext(asIScriptContext *ctx);
	virtual int               SetContextCallbacks(asREQUESTCONTEXTFUNC_t requestCtx, asRETURNCONTEXTFUNC_t returnCtx, void *param = 0);

	// String interpretation
	virtual asETokenClass ParseToken(const char *string, size_t stringLength = 0, asUINT *tokenLength = 0) const;

	// Garbage collection
	virtual int  GarbageCollect(asDWORD flags = asGC_FULL_CYCLE, asUINT numIterations = 1);
	virtual void GetGCStatistics(asUINT *currentSize, asUINT *totalDestroyed, asUINT *totalDetected, asUINT *newObjects, asUINT *totalNewDestroyed) const;
	virtual int  NotifyGarbageCollectorOfNewObject(void *obj, asITypeInfo *type);
	virtual int  GetObjectInGC(asUINT idx, asUINT *seqNbr, void **obj = 0, asITypeInfo **type = 0);
	virtual void GCEnumCallback(void *reference);
	virtual void ForwardGCEnumReferences(void *ref, asITypeInfo *type);
	virtual void ForwardGCReleaseReferences(void *ref, asITypeInfo *type);

	// User data
	virtual void *SetUserData(void *data, asPWORD type);
	virtual void *GetUserData(asPWORD type) const;
	virtual void  SetEngineUserDataCleanupCallback(asCLEANENGINEFUNC_t callback, asPWORD type);
	virtual void  SetModuleUserDataCleanupCallback(asCLEANMODULEFUNC_t callback, asPWORD type);
	virtual void  SetContextUserDataCleanupCallback(asCLEANCONTEXTFUNC_t callback, asPWORD type);
	virtual void  SetFunctionUserDataCleanupCallback(asCLEANFUNCTIONFUNC_t callback, asPWORD type);
	virtual void  SetTypeInfoUserDataCleanupCallback(asCLEANTYPEINFOFUNC_t callback, asPWORD type);
	virtual void  SetScriptObjectUserDataCleanupCallback(asCLEANSCRIPTOBJECTFUNC_t callback, asPWORD type);

	// Exception handling
	virtual int SetTranslateAppExceptionCallback(asSFuncPtr callback, void *param, int callConv);

//===========================================================
// internal methods
//===========================================================
public:
	asCScriptEngine();
	virtual ~asCScriptEngine();
	// Semantic registration only: does not prepare VM bytecode or callables.
	asEMetadataRegistrationResult InstallDefinitionSet(TUniquePtr<asCModuleDefinitionSet> Set);
	asEMetadataRegistrationResult InstallDefinitionSet(
		TUniquePtr<asCModuleDefinitionSet> Set,
		const TMap<const asCTypeInfo*, int>& PublishedTypeIds,
		const TMap<const asCScriptFunction*, int>& PublishedFunctionIds);
	void RetireDefinitionSets();
	bool AcquireVmObjectLease();
	void ReleaseVmObjectLease();
	bool BeginVmOperation(bool Cleanup = false);
	void EndVmOperation();
	void DrainVmRetirement();
	bool IsVmRetirementRequested() const { return vmRetirementRequested.load(); }
	asEByteCodeLinkStatus BindNativeFunction(const asSStableKey& FunctionKey, void* Native);
	asEByteCodeLinkStatus BindNativeFunction(TConstArrayView<asSNativeFunctionBinding> Functions, TConstArrayView<asSNativeGlobalBinding> Globals, int& FailedIndex);
	asEByteCodeLinkStatus BindNativeFunction(const asSStableKey& FunctionKey, const asSFuncPtr& Function, asDWORD CallConv, asFunctionCaller Caller = nullptr, void* Auxiliary = nullptr, int BaseOffset = 0);
	asEByteCodeLinkStatus BindGlobalStorage(const asSStableKey& DeclarationKey, void* Storage);
	void* FindBoundGlobalStorage(const asSStableKey& DeclarationKey) const;
	class asCExecutableFunction* FindPublishedExecutable(const asSStableKey& Key) const;
	class asCExecutableFunction* AcquirePublishedExecutable(const asSStableKey& Key, class asCExecutableSnapshot*& OutSnapshot);
	void DetachPublishedSnapshot(class asCExecutableSnapshot* Snapshot);
	std::shared_ptr<asSSystemFunctionInterface> AcquireSystemInterface(asCScriptFunction* Function) const;
	asEByteCodeLinkStatus BeginExecutablePublication(asQWORD& OutGeneration);
	asEByteCodeLinkStatus CommitExecutablePublication(class asCExecutableSnapshot& Snapshot, asQWORD Generation);
	void PublishExecutable(const asSStableKey& Key, class asCExecutableFunction* Executable);
	void UnpublishExecutable(const asSStableKey& Key, class asCExecutableFunction* Executable);
	asCTypeInfo* FindMetadataType(const char* Namespace, const char* Name) const;
	asCTypeInfo* FindMetadataType(const asSStableKey& Key) const;
	asCScriptFunction* FindMetadataFunction(const asSStableKey& Key) const;
	asCGlobalProperty* FindMetadataGlobalProperty(const char* Namespace, const char* Name) const;
	asCGlobalProperty* FindMetadataGlobalProperty(const asSStableKey& Key) const;
	TPair<asUINT, asUINT> GetMetadataRegistrationCounts() const;
	TArray<const asCModuleDefinitionSet*> GetRegisteredDefinitionSets() const;

	// LEGACY remains the source-module Build default until the final cutover gate.
	// Explicit CANONICAL selection publishes Bytecode from sealed-AST CodeGen.
	// Dual is not registered.
	bool SetCompilerPipeline(asECompilerPipeline pipeline)
	{
		if( pipeline != asCOMPILER_PIPELINE_LEGACY && pipeline != asCOMPILER_PIPELINE_CANONICAL )
		{
			return false;
		}
		ep.canonicalCompilerPipeline = (pipeline == asCOMPILER_PIPELINE_CANONICAL);
		return true;
	}

	asECompilerPipeline GetCompilerPipeline() const
	{
		return ep.canonicalCompilerPipeline ? asCOMPILER_PIPELINE_CANONICAL : asCOMPILER_PIPELINE_LEGACY;
	}

	bool IsCanonicalBytecodeCodeGenReady() const
	{
		// This is a selection/readiness query, not a claim that every source form
		// or every entry point has completed cutover. A selected CANONICAL module
		// Build still verifies the sealed graph and can reject an unsupported form
		// before publishing any bytecode; CompileFunction is audited separately.
		return ep.canonicalCompilerPipeline;
	}

//protected:
	friend class asCBuilder;
	friend class asCCompiler;
	friend class asCContext;
	friend class asCDataType;
	friend class asCModule;
	friend class asCRestore;
	friend class asCByteCode;
	friend int PrepareSystemFunction(asCScriptFunction *func, asSSystemFunctionInterface *internal, asCScriptEngine *engine);

	int RegisterMethodToObjectType(asCObjectType *objectType, const char *declaration, const asSFuncPtr &funcPointer, asDWORD callConv, asFunctionCaller caller, void *auxiliary = 0, int compositeOffset = 0, bool isCompositeIndirect = false, const asUINT accessorType = 255);
	int RegisterBehaviourToObjectType(asCObjectType *objectType, asEBehaviours behaviour, const char *decl, const asSFuncPtr &funcPointer, asDWORD callConv, asFunctionCaller caller, void *auxiliary = 0, int compositeOffset = 0, bool isCompositeIndirect = false);

	int VerifyVarTypeNotInFunction(asCScriptFunction *func);

	void *CallAlloc(const asCObjectType *objType) const;
	void  CallFree(void *obj) const;
	bool  AddRawScriptObjectReference(
		const void *obj,
		asCObjectType *objType) const;
	bool  ReleaseRawScriptObject(
		void *obj,
		asCObjectType *objType) const;
	int   GcGetRefCount(void *obj, asCObjectType *type) const;
	void  GcAddRef(void *obj, asCObjectType *type) const;
	void  GcRelease(void *obj, asCObjectType *type) const;
	void  GcSetFlag(void *obj, asCObjectType *type) const;
	bool  GcGetFlag(void *obj, asCObjectType *type) const;
	void  GcEnumReferences(void *obj, asCObjectType *type);
	void  GcReleaseAllReferences(void *obj, asCObjectType *type);

	void *AllocScriptObject(const asCObjectType *objType) const;
	void FinishConstructObject(asIScriptObject* obj, asITypeInfo* type) const;
	bool AbortConstructObject(asIScriptObject* obj, asITypeInfo* type) const;

	void *CallGlobalFunctionRetPtr(int func) const;
	void *CallGlobalFunctionRetPtr(int func, void *param1) const;
	void *CallGlobalFunctionRetPtr(asSSystemFunctionInterface *func, asCScriptFunction *desc) const;
	void *CallGlobalFunctionRetPtr(asSSystemFunctionInterface *i, asCScriptFunction *s, void *param1) const;
	void  CallObjectMethod(void *obj, int func) const;
	void  CallObjectMethod(void *obj, void *param, int func) const;
	void  CallObjectMethod(void *obj, asSSystemFunctionInterface *func, asCScriptFunction *desc) const;
	void  CallObjectMethod(void *obj, void *param, asSSystemFunctionInterface *func, asCScriptFunction *desc) const;
	bool  CallObjectMethodRetBool(void *obj, int func) const;
	int   CallObjectMethodRetInt(void *obj, int func) const;
	void *CallObjectMethodRetPtr(void *obj, int func) const;
	void *CallObjectMethodRetPtr(void *obj, int param1, asCScriptFunction *func) const;
	void  CallGlobalFunction(void *param1, void *param2, asSSystemFunctionInterface *func, asCScriptFunction *desc) const;
	bool  CallGlobalFunctionRetBool(void *param1, void *param2, asSSystemFunctionInterface *func, asCScriptFunction *desc) const;
	int   CallScriptObjectMethod(void *obj, int func);

	void ConstructScriptObjectCopy(void *mem, void *obj, asCObjectType *type);

	void DeleteDiscardedModules();

	void RemoveTemplateInstanceType(asCObjectType *t);

	asCConfigGroup *FindConfigGroupForFunction(int funcId) const;
	asCConfigGroup *FindConfigGroupForGlobalVar(int gvarId) const;
	asCConfigGroup *FindConfigGroupForTypeInfo(const asCTypeInfo *type) const;
	asCConfigGroup *FindConfigGroupForCallableType(const asCCallableType *callableType) const;

	int  RequestBuild();
	void BuildCompleted();

	void PrepareEngine();
	bool isPrepared;

	int CreateContext(asIScriptContext **context, bool isInternal);

	asCTypeInfo *GetRegisteredType(const asCString &name, asSNameSpace *ns) const;

	asCObjectType *GetListPatternType(int listPatternFuncId);
	asCObjectType *GetListPatternTypeForObjectType(
		asCObjectType *objectType, bool *outCreated = 0);
	void DestroyList(asBYTE *buffer, const asCObjectType *listPatternType);
	void DestroySubList(asBYTE *&buffer, asSListPatternNode *&patternNode);

	int AddBehaviourFunction(asCScriptFunction &func, asSSystemFunctionInterface &internal);

	int ParseNamespace(const char *ns, asCArray<asCString> &nsStrings) const;
	asCString GetFunctionDeclaration(int funcId);

	asCScriptFunction *GetScriptFunction(int funcId) const;


	int  GetMethodIdByDecl(const asCObjectType *ot, const char *decl, asCModule *mod);
	int  GetFactoryIdByDecl(const asCObjectType *ot, const char *decl);

	int  GetNextScriptFunctionId();
	void AddScriptFunction(asCScriptFunction *func);
	void RemoveScriptFunction(asCScriptFunction *func);
	void RemoveCallableType(asCCallableType *func);

	int ConfigError(int err, const char *funcName, const char *arg1, const char *arg2);

	int                GetTypeIdFromDataType(const asCDataType &dt) const;
	asCDataType        GetDataTypeFromTypeId(int typeId) const;
	asCObjectType     *GetObjectTypeFromTypeId(int typeId) const;
	void               RemoveFromTypeIdMap(asCTypeInfo *type);

	bool               IsTemplateType(const char *name) const;
	void               CopyCanonicalTemplateDeclarations(
		asCArray<asSCanonicalTemplateDeclarationFact>& outDeclarations) const;
	// Embeddings whose native type system does not use asCObjectType::derivedFrom
	// can publish pointer-free nominal ancestry for Canonical Sema. Both inputs
	// are exact, namespace-qualified stable type spellings; no TypeId is retained.
	int                RegisterCanonicalTypeBaseRelation(
		const char* derivedStableKey, const char* baseStableKey);
	void               CopyCanonicalRegisteredTypeDeclarations(
		asCArray<asSCanonicalRegisteredTypeDeclarationFact>& outDeclarations,
		const asCModule* importedTypeAuthority = 0) const;
	int                SetTemplateRestrictions(asCObjectType *templateType, asCScriptFunction *func, const char *caller, const char *decl);
	asCObjectType     *GetTemplateInstanceType(asCObjectType *templateType, asCArray<asCDataType> &subTypes, asCModule *requestingModule, asCString* outErrorMessage = nullptr);
	void               BeginTemplateInstanceBuildTracking();
	void               TrackTemplateInstanceBuildRequest(asCObjectType *templateInstance, asCModule *requestingModule, bool createdDuringBuild = false);
	void               PartitionDeferredTemplateInstances(const TSet<asCModule *> &failedModules);
	void               EndTemplateInstanceBuildTracking();
	asCScriptFunction *GenerateTemplateFactoryStub(asCObjectType *templateType, asCObjectType *templateInstanceType, int origFactoryId);
	asCCallableType    *GenerateNewTemplateCallableType(asCObjectType *templateType, asCObjectType *templateInstanceType, asCCallableType *templateCallableType);
	asCDataType        DetermineTypeForTemplate(const asCDataType &orig, asCObjectType *tmpl, asCObjectType *ot);
	bool               RequireTypeReplacement(asCDataType &type, asCObjectType *templateType);
	void               DiscardTemplateInstance(asCObjectType* TemplateType);
	asCScriptFunction* GenerateTemplateFunction(asCObjectType* objectType, asCScriptFunction* templateFunc);


	asCCallableType    *FindMatchingCallableType(asCScriptFunction *func);

	// Global property management
	asCGlobalProperty *AllocateGlobalProperty();
	void RemoveGlobalProperty(asCGlobalProperty *prop);
	void RemoveGlobalProperty(int PropertyIndex);

	int GetScriptSectionNameIndex(const char *name);

	// Namespace management
	asSNameSpace *AddNameSpace(const char *name);
	asSNameSpace *FindNameSpace(const char *name) const;
	asSNameSpace *GetParentNameSpace(asSNameSpace *ns) const;

//===========================================================
// internal properties
//===========================================================
	asCMemoryMgr memoryMgr;
	asUINT initialContextStackSize;

	asCObjectType   *defaultArrayObjectType;
	asCObjectType    scriptTypeBehaviours;
	asCObjectType    functionBehaviours;

	// Registered interface
	asCArray<asCObjectType *>         registeredObjTypes;      // doesn't increase ref count
	asCArray<asCTypedefType *>        registeredTypeDefs;      // doesn't increase ref count
	asCArray<asCEnumType *>           registeredEnums;         // doesn't increase ref count
	// TODO: memory savings: Since there can be only one property with the same name a simpler symbol table should be used for global props
	asCArray<asCGlobalProperty*> registeredGlobalProps;   // increases ref count
	asCSymbolMap<asCGlobalProperty*> registeredGlobalPropTable;   // increases ref count
	asCArray<asCScriptFunction*> registeredGlobalFuncs;
	asCSymbolMap<asCScriptFunction*> registeredGlobalFuncTable;
	asCArray<asCCallableType *>        registeredCallableTypes;      // doesn't increase ref count
	asCArray<asCObjectType *>         registeredTemplateTypes; // doesn't increase ref count
	// Host-declared native ancestry that cannot be represented by the maintained
	// AngelScript Runtime object model. Values are stable names only and are
	// copied into each ephemeral Canonical Sema fact snapshot.
	asCMap<asCString, asCArray<asCString> > canonicalRegisteredTypeBaseRelations;
	asIStringFactory                 *stringFactory;
	asCDataType                       stringType;
	bool configFailed;

	// Stores all registered types
	asCSymbolMap<asCTypeInfo*> allRegisteredTypes; // increases ref count
	asCMapByName<asCTypeInfo*> allRegisteredTypesByName;

	// Dummy types used to name the subtypes in the template objects
	asCArray<asCTypeInfo *>        templateSubTypes;

	// Store information about template types
	// This list will contain all instances of templates, both registered specialized
	// types and those automacially instantiated from scripts
	TArray<asCObjectType *>      unvalidatedTemplateInstances;
	TMap<uint64, TArray<asCObjectType*, TInlineAllocator<2>>> templateInstanceBuckets;
	struct asSTemplateInstanceBuildProvenance
	{
		TSet<asCModule *> requestingModules;
		bool createdDuringBuild = false;
		bool hasUnknownRequestor = false;
		bool rejectedFromDeferredQueue = false;
	};
	bool trackTemplateInstanceBuildRequests = false;
	TMap<asCObjectType *, asSTemplateInstanceBuildProvenance> templateInstanceBuildProvenance;

	// Store information about list patterns
	asCArray<asCObjectType *>      listPatternTypes; // increases ref count

	// Stores all global properties, both those registered by application, and those declared by scripts.
	// The id of a global property is the index in this array.
	asCArray<asCGlobalProperty *> globalProperties; // increases ref count
	asCArray<int>                 freeGlobalPropertyIds;

	// This map is used to quickly find a property by its memory address
	// It is used principally during building, cleanup, and garbage detection for script functions
	TMap<void*, asCGlobalProperty*> varAddressMap; // doesn't increase ref count

	// Stores all functions, i.e. registered functions, script functions, class methods, behaviours, etc.
	asCArray<asCScriptFunction *> scriptFunctions;       // doesn't increase ref count
	asCArray<int>                 freeScriptFunctionIds;

	asCSymbolMap<asCScriptFunction*> allScriptGlobalFunctions;
	asCMapByName<asCTypeInfo*> allScriptDeclaredTypes;
	asCSymbolMap<asCGlobalProperty*> allScriptGlobalVariables;

	// An array with all module imported functions
	asCArray<sBindInfo *>  importedFunctions; // doesn't increase ref count
	asCArray<int>          freeImportedFunctionIdxs;

	// Synchronized
	mutable asCAtomic      refCount;
	// Synchronized with engineRWLock
	// This array holds all live script modules
	asCArray<asCModule *>  scriptModules;
	asCMapByName<asCModule*> scriptModulesByName;
	// Synchronized with engineRWLock
	// This flag is true if a script is currently being compiled. It is used to prevent multiple
	// threads from requesting builds at the same time (without blocking)
	bool                   isBuilding;
	// Synchronized with engineRWLock
	// This array holds modules that have been discard (thus are no longer visible to the application)
	// but cannot yet be deleted due to having external references to some of the entities in them
	// This flag is set to true during compilations of scripts (or loading pre-compiled scripts)
	// to delay the validation of template types until the subtypes have been fully declared
	bool                   deferValidationOfTemplateTypes;
	bool                   deferCalculatingTemplateSize = false;

	// Stores shared script declared types (classes, interfaces, enums)
	// Stores the funcdefs
	// TODO: redesign: Only shared funcdefs should be stored here
	//                 a funcdef becomes shared if all arguments and the return type are shared (or application registered)
	asCArray<asCCallableType *> callableTypes; // doesn't increases ref count

	// Stores the names of the script sections for debugging purposes
	asCArray<asCString *> scriptSectionNames;

	struct ScriptSectionName
	{
		int index;
		asCString name;
		const char* GetName()
		{
			return name.AddressOf();
		}
	};

	asCMapByName<ScriptSectionName*> scriptSectionIndexMap;

	// Type identifiers
	mutable int                             typeIdSeqNbr;
	mutable TMap<int, asCTypeInfo*>       mapTypeIdToTypeInfo;

	// Garbage collector
	asCGarbageCollector gc;

	// Dynamic groups
	asDWORD                    defaultAccessMask;
	asSNameSpace              *defaultNamespace;

	// Message callback
	bool                        msgCallback;
	asSSystemFunctionInterface  msgCallbackFunc;
	void                       *msgCallbackObj;
	asSFuncPtr                  msgCallbackOriginalFuncPtr;
	asDWORD                     msgCallbackOriginalCallConv = 0;
	struct preMessage_t
	{
		preMessage_t() { isSet = false; }
		bool      isSet;
		asCString message;
		asCString scriptname;
		int       r;
		int       c;
	} preMessage;

	// JIt compilation
	asIJITCompiler             *jitCompiler;

	// Namespaces
	// These are shared between all entities and are
	// only deleted once the engine is destroyed
	asCArray<asSNameSpace*> nameSpaces;
	asCMapByName<asSNameSpace*> nameSpacesByName;

	// Callbacks for context pooling
	asREQUESTCONTEXTFUNC_t  requestCtxFunc;
	asRETURNCONTEXTFUNC_t   returnCtxFunc;
	void                   *ctxCallbackParam;

	// User data
	asCArray<asPWORD>       userData;

	struct SEngineClean    { asPWORD type; asCLEANENGINEFUNC_t       cleanFunc; };
	asCArray<SEngineClean>    cleanEngineFuncs;
	struct SModuleClean    { asPWORD type; asCLEANMODULEFUNC_t       cleanFunc; };
	asCArray<SModuleClean>    cleanModuleFuncs;
	struct SContextClean   { asPWORD type; asCLEANCONTEXTFUNC_t      cleanFunc; };
	asCArray<SContextClean>   cleanContextFuncs;
	struct SFunctionClean  { asPWORD type; asCLEANFUNCTIONFUNC_t     cleanFunc; };
	asCArray<SFunctionClean>  cleanFunctionFuncs;
	struct STypeInfoClean  { asPWORD type; asCLEANTYPEINFOFUNC_t     cleanFunc; };
	asCArray<STypeInfoClean>  cleanTypeInfoFuncs;
	struct SScriptObjClean { asPWORD type; asCLEANSCRIPTOBJECTFUNC_t cleanFunc; };
	asCArray<SScriptObjClean> cleanScriptObjectFuncs;

	// Synchronization for threads
	DECLAREREADWRITELOCK(mutable engineRWLock)

private:
	void InitializeMetadataBuiltins();
	asUINT CountMetadataTypes(asQWORD Include, asQWORD Exclude = 0) const;
	asCTypeInfo* MetadataTypeByIndex(asUINT Index, asQWORD Include, asQWORD Exclude = 0) const;
	mutable std::mutex metadataRegistrationMutex;
	mutable std::mutex executablePublicationMutex;
	asQWORD executablePublicationGeneration = 0;
	std::atomic<bool> vmRetirementRequested{false};
	asUINT vmObjectLeases = 0;
	asUINT vmOperations = 0;
	bool vmDrainActive = false;
	TArray<TUniquePtr<asCModuleDefinitionSet>> definitionSets;
	TMap<asSStableKey, asCTypeInfo*> metadataTypesByKey;
	TMap<FString, asCTypeInfo*, FDefaultSetAllocator, asCModuleDefinitionSet::asTNameKeyFuncs<asCTypeInfo*>> metadataTypesByName;
	TMap<int, asCTypeInfo*> metadataTypesById;
	TMap<asSStableKey, asCScriptFunction*> metadataFunctionsByKey;
	TMap<asSStableKey, asCGlobalProperty*> metadataGlobalsByKey;
	TMap<FString, asCGlobalProperty*, FDefaultSetAllocator, asCModuleDefinitionSet::asTNameKeyFuncs<asCGlobalProperty*>> metadataGlobalsByName;
	TMap<asSStableKey, void*> metadataGlobalBindings;
	TMap<asSStableKey, class asCExecutableFunction*> publishedExecutables;
	TArray<class asCExecutableSnapshot*> publishedSnapshotLeases;
	TMap<asSStableKey, std::shared_ptr<asSSystemFunctionInterface>> nativeInterfaces;
	TMap<int, asCScriptFunction*> metadataFunctionsById;
	int nextMetadataTypeId = asTYPEID_FLOAT64 + 1;
	int nextMetadataFunctionId = 1;
	bool metadataRegistrationRetired = false;

public:

	// Engine properties
	struct
	{
		bool   allowUnsafeReferences;
		bool   optimizeByteCode;
		bool   copyScriptSections;
		asUINT maximumContextStackSize;
		asUINT initContextStackSize;
		bool   useCharacterLiterals;
		bool   allowMultilineStrings;
		bool   allowImplicitHandleTypes;
		bool   buildWithoutLineCues;
		bool   initGlobalVarsAfterBuild;
		bool   requireEnumScope;
		int    scanner;
		bool   includeJitInstructions;
		int    stringEncoding;
		bool   expandDefaultArrayToTemplate;
		bool   autoGarbageCollect;
		bool   disallowGlobalVars;
		asUINT alwaysImplDefaultConstruct;
		int    compilerWarnings;
		bool   disallowValueAssignForRefType;
		// TODO: 3.0.0: Remove the alterSyntaxNamedArgs
		int    alterSyntaxNamedArgs;
		bool   disableIntegerDivision;
		bool   disallowEmptyListElements;
		// TODO: 3.0.0: Remove the privatePropAsProtected
		bool   privatePropAsProtected;
		bool   allowUnicodeIdentifiers;
		int    heredocTrimMode;
		bool automaticImports;
		asUINT maxNestedCalls;
		asUINT genericCallMode;
		asUINT initCallStackSize;
		asUINT maxCallStackSize;
		bool   ignoreDuplicateSharedIntf;
		bool   noDebugOutput;
		bool   disableScriptClassGC;
		asUINT alwaysImplDefaultCopy;
		asUINT alwaysImplDefaultCopyConstruct;
		asUINT memberInitMode;
		asUINT boolConversionMode;
		bool   foreachSupport;
		bool   typeCheckSwitchEnums;
		bool   allowDoubleType;
		bool   floatIsFloat64;
		bool   warnOnFloatConstantsForDoubles;
		bool   warnIntegerDivision;
		bool   canonicalCompilerPipeline;
	} ep;

	// Callbacks
#ifndef AS_NO_EXCEPTIONS
	bool                       translateExceptionCallback;
	asSSystemFunctionInterface translateExceptionCallbackFunc;
	void *                     translateExceptionCallbackObj;
#endif

	// This flag is to allow a quicker shutdown when releasing the engine
	std::atomic<bool> shuttingDown{false};

	// This flag is set when the engine's destructor is called, this is to
	// avoid recursive calls if an object happens to increment/decrement
	// the ref counter during shutdown
	bool inDestructor;

	void* Manager;
};

END_AS_NAMESPACE

#endif
