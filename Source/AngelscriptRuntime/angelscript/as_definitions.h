/* Maintained Unreal AngelScript fork — engine-independent definition ownership. */
#pragma once

#include "as_config.h"
#include "as_datatype.h"
#include "as_typeinfo.h"
#include "as_metadata_list_pattern.h"
#include "as_property.h"
#include "frontend/Basic/as_stable_key.h"
#include "as_metadata_fingerprint.h"
#include "Templates/UniquePtr.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Misc/Crc.h"

#include <atomic>
#include <memory>
#include <mutex>

BEGIN_AS_NAMESPACE

class asCTypeInfo;
class asCObjectType;
class asCEnumType;
class asCTypedefType;
class asCCallableType;
class asCScriptFunction;
class asCObjectProperty;
class asCGlobalProperty;
struct asSNameSpace;
class asCScriptEngine;
class asCTypeContext;
class asCByteCodeImage;
class asCByteCodeEmitter;
class asCBuilder;
class asCBuilderState;
class asCCompilationSession;
class asCDefinitionConsumer;
class asCEngineCompileRegistration;
struct asSByteCodeEmissionOptions;
struct asSByteCodeEmissionResult;
	enum class asETypeDeclKind : uint8;
	enum class asEFunctionIdentityKind : uint8;
	class asCTypeIdAllocator;

struct asSDefinitionDestroyCounters
{
	std::atomic<int32> Types{0};
	std::atomic<int32> Functions{0};
	std::atomic<int32> Namespaces{0};
};

struct asSDefinitionOptions
{
	bool FloatIsFloat64 = true;
	bool ExpandDefaultArrayToTemplate = true;
	asUINT PointerByteSize = sizeof(void*);
	asCString TargetProfile = "native";
	asETypeInfoKind Kind = asETypeInfoKind::ScriptEngine;
	asCTypeIdAllocator* TypeIdAllocator = nullptr;
};

enum class asEDefinitionState : asBYTE
{
	Building, Frozen, Attaching, Attached, Retired
};

enum class asEDefinitionResult : asBYTE
{
	Succeeded, InvalidArgument, InvalidState, ForeignOwner, DuplicateIdentity,
	DuplicateName, UnfrozenDependency, IncompleteDefinition, InvalidLayout,
	RecursiveValueType, InvalidSignature
};

enum class asERegistrationResult : asBYTE
{
	Succeeded, InvalidArgument, InvalidState, InvalidIdentity, InvalidLayout,
	IdentityConflict, NameConflict, AlreadyRegistered, ForeignEngine, Retired
};

struct asSDefinitionParameter
{
	asCString Name;
	asCDataType Type;
	asDWORD Flags = asTM_NONE;
	asCString DefaultArgument;
};

struct asSNativeFunctionFacts
{
	asDWORD Traits = 0;
	asECompileOutType CompileOut = asECompileOutType::CompileCalls;
	int DeterminesOutputTypeArgument = -1;
	asCString DeprecationMessage;
};

struct asSNativePropertyFacts
{
	bool Readable = true;
	bool Writeable = true;
	bool Editable = true;
	bool Protected = false;
	bool EditorOnly = false;
	bool Deprecated = false;
	asCString DeprecationMessage;
};

struct asSNativeGlobalFacts
{
	bool PureConstant = false;
	asQWORD ConstantValue = 0;
	bool EditorOnly = false;
};

class asCDefinitions;

class ANGELSCRIPTRUNTIME_API asCDefinitionGraphLease
{
public:
	asCDefinitionGraphLease() = default;
	explicit asCDefinitionGraphLease(TUniquePtr<asCDefinitions>&& Producer);
	asCDefinitions* Get() const { return Graph.Get(); }
	asCDefinitions& operator*() const { return *Graph; }
	asCDefinitions* operator->() const { return Graph.Get(); }
	explicit operator bool() const { return Graph.IsValid(); }

private:
	friend class asCDefinitions;
	explicit asCDefinitionGraphLease(TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> InGraph)
		: Graph(MoveTemp(InGraph))
	{
	}
	TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> Graph;
};

// One compile unit's unique owner of TypeInfo/Function/Global until batch
// Engine registration. Not asCModule.
class ANGELSCRIPTRUNTIME_API asCDefinitions
{
public:
	asCDefinitions();
	explicit asCDefinitions(const asSDefinitionOptions& Options);
	static TUniquePtr<asCDefinitions> Create(const asSDefinitionOptions& Options = {});
	static asCDefinitionGraphLease AdoptGraph(TUniquePtr<asCDefinitions>&& Producer);
	asCDefinitionGraphLease RetainGraph();
	void SetDestroyCounters(asSDefinitionDestroyCounters* Counters);
	~asCDefinitions();
	asCDefinitions(const asCDefinitions&) = delete;
	asCDefinitions& operator=(const asCDefinitions&) = delete;

	const asSDefinitionOptions& GetOptions() const { return Options; }
	asEDefinitionState GetState() const { return State.load(std::memory_order_acquire); }
	bool IsFrozen() const { return GetState() != asEDefinitionState::Building; }
	bool IsImmutable() const { return IsFrozen(); }
	bool IsDestroying() const { return bDestroying; }
	TConstArrayView<asCTypeInfo*> GetTypes() const { return MakeArrayView(Types); }
	TConstArrayView<asCScriptFunction*> GetFunctions() const { return MakeArrayView(Functions); }
	TConstArrayView<asCGlobalProperty*> GetGlobals() const { return MakeArrayView(Globals); }
	TConstArrayView<asCGlobalProperty*> GetGlobalProperties() const { return MakeArrayView(Globals); }
	TConstArrayView<asCDefinitions*> GetDependencies() const { return MakeArrayView(Dependencies); }
	bool Owns(const asCTypeInfo* Type) const;
	bool Owns(const asCScriptFunction* Function) const;
	bool CanReference(const asCTypeInfo* Type) const;
	bool CanReference(const asCScriptFunction* Function) const;
	asCTypeInfo* FindType(const char* Name) const;
	asCTypeInfo* FindType(const asSStableKey& Key) const;
	asCObjectType* FindTemplateInstance(const asCObjectType* Template, TConstArrayView<asCDataType> Arguments) const;
	asCScriptFunction* FindFunction(const asSStableKey& Key) const;
	asCGlobalProperty* FindGlobalProperty(const asSStableKey& Key) const;
	asEDefinitionResult CreateGlobalProperty(const asSStableKey& Key, const asSStableKey& TypeUseKey,
		const char* Name, const char* Namespace, const asCDataType& Type, asCGlobalProperty*& Out);
	asEDefinitionResult CreateNativeGlobalProperty(const asSStableKey& Key, const asSStableKey& TypeUseKey,
		const char* Name, const char* Namespace, const asCDataType& Type, asCGlobalProperty*& Out);
	asCDataType Primitive(eTokenType Token, bool IsConst = false) const;
	asEDefinitionResult SetIdentityContext(TSharedPtr<const asCTypeContext, ESPMode::ThreadSafe> Context);
	TSharedPtr<const asCTypeContext, ESPMode::ThreadSafe> GetIdentityContext() const;
	asCScriptEngine* GetBoundEngine() const;
	int GetBoundTypeId(const asCTypeInfo* Type) const;
	int GetBoundFunctionId(const asCScriptFunction* Function) const;
	int GetBoundDataTypeId(const asCDataType& Type) const;
	asEDefinitionResult AddDependency(asCDefinitions* Dependency);
	asEDefinitionResult CreateObjectType(const asSStableKey& Key,
		const char* Name, const char* Namespace, asQWORD Flags, asCObjectType*& Out);
	asEDefinitionResult CreateEnumType(const asSStableKey& Key,
		const char* Name, const char* Namespace, asCEnumType*& Out);
	asEDefinitionResult CreateTypedefType(const asSStableKey& Key,
		const char* Name, const char* Namespace, asCTypedefType*& Out);
	asEDefinitionResult CreateCallableType(const asSStableKey& Key,
		const char* Name, const char* Namespace, asCScriptFunction& Signature, asCCallableType*& Out);
	asEDefinitionResult CreateFunction(const asSStableKey& Key,
		const char* Name, const char* Namespace, asEFuncType Kind, asCScriptFunction*& Out);
	// A structural Callable TypeUse is a signature, not a Function entity.
	// Multiple nominal delegate/event types may share this image-owned object.
	asEDefinitionResult CreateCallableSignature(const asSStableKey& SignatureKey, asCScriptFunction*& Out);
	asEDefinitionResult DefineFunction(asCScriptFunction& Function, const asCDataType& ReturnType,
		TConstArrayView<asSDefinitionParameter> Parameters, asCObjectType* Owner = nullptr);
	asEDefinitionResult SetTypeDeclarationFacts(asCTypeInfo& Type, asETypeDeclKind Kind,
		asUINT GenericArity = 0, asCTypeInfo* Owner = nullptr);
	asEDefinitionResult SetFunctionDeclarationFacts(asCScriptFunction& Function, asEFunctionIdentityKind Kind,
		asCTypeInfo* Owner = nullptr, asUINT GenericArity = 0, bool IsStatic = false);
	asEDefinitionResult SetFunctionLocal(asCScriptFunction& Function, bool IsLocal);
	asEDefinitionResult SetNativeFunctionFacts(asCScriptFunction& Function, const asSNativeFunctionFacts& Facts);
	asEDefinitionResult BindNativeFunction(asCScriptFunction& Function, const asSFuncPtr& FunctionPtr,
		asDWORD CallConv, asFunctionCaller Caller = nullptr, void* Auxiliary = nullptr);
	asEDefinitionResult SetNativePropertyFacts(asCObjectType& Owner, asCObjectProperty& Property, const asSNativePropertyFacts& Facts);
	asEDefinitionResult SetNativeGlobalFacts(asCGlobalProperty& Global, const asSNativeGlobalFacts& Facts);
	asEDefinitionResult DefineAccessSpecifier(asCObjectType& Owner, const asSAccessSpecifier& Definition, asSAccessSpecifier*& Out);
	asEDefinitionResult SetPropertyAccess(asCObjectType& Owner, asCObjectProperty& Property, asSAccessSpecifier& Policy);
	asEDefinitionResult SetFunctionAccess(asCScriptFunction& Function, asSAccessSpecifier& Policy);
	asEDefinitionResult SetListInitializer(asCObjectType& Type, asCScriptFunction& Factory, const asSMetadataListPattern& Pattern);
	const asSMetadataListInitializer* GetListInitializer(const asCObjectType& Type) const;
	asEDefinitionResult AddProperty(asCObjectType& Owner, const char* Name,
		const asCDataType& Type, asCObjectProperty*& Out, bool IsPrivate = false, bool IsProtected = false);
	asEDefinitionResult DefineNativeProperty(asCObjectType& Owner, const char* Name,
		const asCDataType& Type, const asSNativePropertyAddress& Address, asCObjectProperty*& Out);
	asEDefinitionResult SetBaseType(asCObjectType& Type, asCObjectType& Base);
	asEDefinitionResult SetInterfaceDeclaration(asCObjectType& Type);
	asEDefinitionResult AddInterface(asCObjectType& Type, asCObjectType& Interface);
	asEDefinitionResult AddBehaviour(asCObjectType& Type, asEBehaviours Behaviour, asCScriptFunction& Function);
	asEDefinitionResult AddTemplateArgument(asCObjectType& Type, const asCDataType& Argument);
	asEDefinitionResult SetDefaultArrayType(asCObjectType& Type);
	bool IsDefaultArrayType(const asCTypeInfo* Type) const;
	asEDefinitionResult AddEnumValue(asCEnumType& Type, const char* Name, int Value);
	asEDefinitionResult SetAlias(asCTypedefType& Type, const asCDataType& Alias);
	asEDefinitionResult AddChildCallableType(asCObjectType& Type, asCCallableType& CallableType);
	asEDefinitionResult SetNativeLayout(asCObjectType& Type, asUINT Size, asUINT Alignment);
	asEDefinitionResult SetFixedTemplateLayout(asCObjectType& Type, asUINT Size, asUINT Alignment);
	asEDefinitionResult CreateTemplateInstance(asCObjectType& Template, const asSStableKey& TypeUseKey,
		TConstArrayView<asCDataType> Arguments, asCObjectType*& Out);
	asEDefinitionResult CreateHostTemplateInstance(asCObjectType& Template, const asSStableKey& Key,
		TConstArrayView<asCDataType> Arguments, asCObjectType*& Out);
	bool ValidateFrozenDefinitions() const;
	asEDefinitionResult FinalizeLayouts();
	asEDefinitionResult Freeze();
	asEFingerprintStatus TryGetSchemaHash(const asCTypeInfo& Type, asSSchemaHash& OutHash,
		TConstArrayView<uint8>* OutWitness = nullptr) const;
	asEFingerprintStatus TryGetLayoutHash(const asCTypeInfo& Type, asSLayoutHash& OutHash,
		TConstArrayView<uint8>* OutWitness = nullptr) const;
	void SetFingerprintDigestProvider(const asIFingerprintDigestProvider* Provider);
	void AttachDependencies(TConstArrayView<asCDefinitions*> InDependencies);
	void AdoptEmittedByteCode(TUniquePtr<asCByteCodeImage> InImage);
	asCByteCodeImage* PeekEmittedByteCode() const;
	TUniquePtr<asCByteCodeImage> TakeEmittedByteCode();

private:
	friend class asCScriptEngine;
	friend class asCDefinitionGraphLease;
	friend class asCTypeInfo;
	friend class asCObjectType;
	friend class asCEnumType;
	friend class asCTypedefType;
	friend class asCCallableType;
	friend class asCScriptFunction;
	friend class asCGlobalProperty;
	friend class asCEngineCompileRegistration;
	friend class asCDefinitionConsumer;
	friend class asCCompilationSession;
	friend class asCBuilder;
	friend class asCBuilderState;
	friend class asCByteCodeEmitter;
	friend class FAngelscriptTypeBindInfoDraft;
	friend class FAngelscriptTypeBindInfoApply;
	friend class FAngelscriptTypeBindInfoInstallation;
	friend struct FDetachedDefinitionFixture;
	friend struct asSMetadataFingerprintEncoder;
	friend ANGELSCRIPTRUNTIME_API asSByteCodeEmissionResult asEmitByteCodeImage(
		const asCCompilationSession*,
		const asCDefinitions*,
		const asSByteCodeEmissionOptions&,
		TUniquePtr<asCByteCodeImage>&);
	asEDefinitionResult ValidateNew(const asSStableKey& Key, const char* Name,
		const char* Namespace, bool IsType) const;
	asSNameSpace* InternNamespace(const char* Name);
	asEDefinitionResult ValidateDataType(const asCDataType& Type, bool AllowVoid, bool AllowWildcard = false) const;
	asCDataType NormalizeOptions(const asCDataType& Type) const;
	bool ReferencesSet(const asCDefinitions* Set) const;
	bool ValidateFrozenIdentity() const;
	bool ValidateListInitializers() const;
	bool ValidateListPattern(const asSMetadataListPattern& Pattern, asUINT GenericArity, int Depth, int& NodeBudget) const;
	bool ValidateGlobalIdentity(const asCGlobalProperty& Global) const;
	asEDefinitionResult CreateGlobalPropertyInternal(const asSStableKey& Key, const asSStableKey& TypeUseKey,
		const char* Name, const char* Namespace, const asCDataType& Type, asCGlobalProperty*& Out, bool Native);
	bool ValidateTypeUse(const asSStableKey& Key, const asCDataType& Actual, int Depth = 0) const;
	bool ValidateFrozenLayout() const;
	bool CanUseAccessSpecifier(const asCObjectType& Owner, const asSAccessSpecifier* Policy) const;
	struct asSAccessPolicySnapshot
	{
		const asCObjectType* Owner = nullptr;
		TArray<uint8> Encoding;
	};
	struct asSMemberAccessSnapshot
	{
		const asSAccessSpecifier* Policy = nullptr;
		uint8 StandardAccess = 0;
		bool operator==(const asSMemberAccessSnapshot& Other) const { return Policy == Other.Policy && StandardAccess == Other.StandardAccess; }
	};
	bool CollectAccessSnapshot(TMap<const asSAccessSpecifier*, asSAccessPolicySnapshot>& Policies,
		TMap<const asCObjectProperty*, asSMemberAccessSnapshot>& Properties,
		TMap<const asCScriptFunction*, asSMemberAccessSnapshot>& Methods) const;
	bool CaptureAccessSnapshot();
	bool ValidateAccessSnapshot() const;
	void InvalidateLayouts() { bLayoutsFinalized = false; }

	const asSDefinitionOptions Options;
	std::atomic<asEDefinitionState> State{asEDefinitionState::Building};
	mutable std::mutex MutationMutex;
	TArray<asCTypeInfo*> Types;
	TArray<asCScriptFunction*> Functions;
	TArray<asCGlobalProperty*> Globals;
	template<typename ValueType>
	struct asTNameKeyFuncs : TDefaultMapHashableKeyFuncs<FString, ValueType, false>
	{
		static bool Matches(const FString& A, const FString& B) { return A.Equals(B, ESearchCase::CaseSensitive); }
		static uint32 GetKeyHash(const FString& Key) { return FCrc::StrCrc32(*Key); }
	};
	TMap<asSStableKey, asCGlobalProperty*> GlobalsByKey;
	TMap<FString, asCGlobalProperty*, FDefaultSetAllocator, asTNameKeyFuncs<asCGlobalProperty*>> GlobalsByName;
	TArray<asSNameSpace*> Namespaces;
	TArray<asCDefinitions*> Dependencies;
	TMap<asSStableKey, asCTypeInfo*> TypesByKey;
	TMap<asSStableKey, asCScriptFunction*> FunctionsByKey;
	TMap<FString, asCTypeInfo*, FDefaultSetAllocator, asTNameKeyFuncs<asCTypeInfo*>> TypesByName;
	TMap<asCObjectType*, TPair<asUINT, asUINT>> NativeLayouts;
	TMap<const asCObjectType*, asSMetadataListInitializer> ListInitializers;
	asCObjectType* DefaultArrayType = nullptr;
	bool bLayoutsFinalized = false;
	bool bDestroying = false;
	TSharedPtr<const asCTypeContext, ESPMode::ThreadSafe> IdentityContext;
	TMap<asCTypeInfo*, TPair<int, int>> FrozenLayouts;
	TMap<asCTypeInfo*, asQWORD> FrozenTypeFlags;
	TMap<asCObjectProperty*, int> FrozenPropertyOffsets;
	TMap<asCObjectProperty*, asCDataType> FrozenPropertyTypes;
	TMap<asCObjectType*, bool> FrozenTemplateLayoutPolicies;
	// Exact, private freeze witnesses, not semantic proxies or persistent keys.
	TMap<const asSAccessSpecifier*, asSAccessPolicySnapshot> FrozenAccessPolicies;
	TMap<const asCObjectProperty*, asSMemberAccessSnapshot> FrozenPropertyAccess;
	TMap<const asCScriptFunction*, asSMemberAccessSnapshot> FrozenFunctionAccess;
	// Guarded by MutationMutex. Neither pointer nor IDs live in semantic fields.
	asCScriptEngine* BoundEngine = nullptr;
	TMap<const asCTypeInfo*, int> BoundTypeIds;
	TMap<const asCScriptFunction*, int> BoundFunctionIds;
	asSDefinitionDestroyCounters* DestroyCounters = nullptr;
	TWeakPtr<asCDefinitions, ESPMode::ThreadSafe> Self;

	const asIFingerprintDigestProvider* FingerprintDigestProvider = nullptr;
	struct asSCachedFingerprint
	{
		asSSchemaHash Schema;
		asSLayoutHash Layout;
		TArray<uint8> SchemaWitness;
		TArray<uint8> LayoutWitness;
	};
	mutable TMap<const asCTypeInfo*, asSCachedFingerprint> Fingerprints;
	TUniquePtr<asCByteCodeImage> EmittedByteCode;
	asEFingerprintStatus EnsureFingerprintsLocked(const asCTypeInfo& Type, const asSCachedFingerprint*& Out) const;
};

END_AS_NAMESPACE
