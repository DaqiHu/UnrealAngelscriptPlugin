/* Maintained Unreal AngelScript fork — engine-independent definition ownership. */
#pragma once

#include "as_config.h"
#include "as_datatype.h"
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

struct asSMetadataOptions
{
	bool FloatIsFloat64 = true;
	bool ExpandDefaultArrayToTemplate = true;
	asUINT PointerByteSize = sizeof(void*);
	asCString TargetProfile = "native";
};

enum class asEDefinitionSetState : asBYTE
{
	Building, Frozen, Attaching, Attached, Retired
};

enum class asEMetadataResult : asBYTE
{
	Succeeded, InvalidArgument, InvalidState, ForeignOwner, DuplicateIdentity,
	DuplicateName, UnfrozenDependency, IncompleteDefinition, InvalidLayout,
	RecursiveValueType, InvalidSignature
};

enum class asEMetadataRegistrationResult : asBYTE
{
	Succeeded, InvalidArgument, InvalidState, InvalidIdentity, InvalidLayout,
	IdentityConflict, NameConflict, AlreadyRegistered, ForeignEngine, Retired
};

struct asSMetadataParameter
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

// One compile unit's unique owner of TypeInfo/Function/Global until batch
// Engine registration. Not asCModule.
class ANGELSCRIPTRUNTIME_API asCModuleDefinitionSet
{
public:
	asCModuleDefinitionSet();
	explicit asCModuleDefinitionSet(const asSMetadataOptions& Options);
	static TUniquePtr<asCModuleDefinitionSet> Create(const asSMetadataOptions& Options = {});
	~asCModuleDefinitionSet();
	asCModuleDefinitionSet(const asCModuleDefinitionSet&) = delete;
	asCModuleDefinitionSet& operator=(const asCModuleDefinitionSet&) = delete;

	const asSMetadataOptions& GetOptions() const { return Options; }
	asEDefinitionSetState GetState() const { return State.load(std::memory_order_acquire); }
	bool IsFrozen() const { return GetState() != asEDefinitionSetState::Building; }
	bool IsImmutable() const { return IsFrozen(); }
	bool IsDestroying() const { return bDestroying; }
	TConstArrayView<asCTypeInfo*> GetTypes() const { return MakeArrayView(Types); }
	TConstArrayView<asCScriptFunction*> GetFunctions() const { return MakeArrayView(Functions); }
	TConstArrayView<asCGlobalProperty*> GetGlobalProperties() const { return MakeArrayView(Globals); }
	TConstArrayView<asCModuleDefinitionSet*> GetDependencies() const { return MakeArrayView(Dependencies); }
	bool Owns(const asCTypeInfo* Type) const;
	bool Owns(const asCScriptFunction* Function) const;
	bool CanReference(const asCTypeInfo* Type) const;
	bool CanReference(const asCScriptFunction* Function) const;
	asCTypeInfo* FindType(const char* Name) const;
	asCTypeInfo* FindType(const asSStableKey& Key) const;
	asCScriptFunction* FindFunction(const asSStableKey& Key) const;
	asCGlobalProperty* FindGlobalProperty(const asSStableKey& Key) const;
	asEMetadataResult CreateGlobalProperty(const asSStableKey& Key, const asSStableKey& TypeUseKey,
		const char* Name, const char* Namespace, const asCDataType& Type, asCGlobalProperty*& Out);
	asEMetadataResult CreateNativeGlobalProperty(const asSStableKey& Key, const asSStableKey& TypeUseKey,
		const char* Name, const char* Namespace, const asCDataType& Type, asCGlobalProperty*& Out);
	asCDataType Primitive(eTokenType Token, bool IsConst = false) const;
	asEMetadataResult SetIdentityContext(TSharedPtr<const asCTypeContext, ESPMode::ThreadSafe> Context);
	TSharedPtr<const asCTypeContext, ESPMode::ThreadSafe> GetIdentityContext() const;
	asCScriptEngine* GetBoundEngine() const;
	int GetBoundTypeId(const asCTypeInfo* Type) const;
	int GetBoundFunctionId(const asCScriptFunction* Function) const;
	int GetBoundDataTypeId(const asCDataType& Type) const;
	asEMetadataResult AddDependency(asCModuleDefinitionSet* Dependency);
	asEMetadataResult CreateObjectType(const asSStableKey& Key,
		const char* Name, const char* Namespace, asQWORD Flags, asCObjectType*& Out);
	asEMetadataResult CreateEnumType(const asSStableKey& Key,
		const char* Name, const char* Namespace, asCEnumType*& Out);
	asEMetadataResult CreateTypedefType(const asSStableKey& Key,
		const char* Name, const char* Namespace, asCTypedefType*& Out);
	asEMetadataResult CreateCallableType(const asSStableKey& Key,
		const char* Name, const char* Namespace, asCScriptFunction& Signature, asCCallableType*& Out);
	asEMetadataResult CreateFunction(const asSStableKey& Key,
		const char* Name, const char* Namespace, asEFuncType Kind, asCScriptFunction*& Out);
	// A structural Callable TypeUse is a signature, not a Function entity.
	// Multiple nominal delegate/event types may share this image-owned object.
	asEMetadataResult CreateCallableSignature(const asSStableKey& SignatureKey, asCScriptFunction*& Out);
	asEMetadataResult DefineFunction(asCScriptFunction& Function, const asCDataType& ReturnType,
		TConstArrayView<asSMetadataParameter> Parameters, asCObjectType* Owner = nullptr);
	asEMetadataResult SetTypeDeclarationFacts(asCTypeInfo& Type, asETypeDeclKind Kind,
		asUINT GenericArity = 0, asCTypeInfo* Owner = nullptr);
	asEMetadataResult SetFunctionDeclarationFacts(asCScriptFunction& Function, asEFunctionIdentityKind Kind,
		asCTypeInfo* Owner = nullptr, asUINT GenericArity = 0, bool IsStatic = false);
	asEMetadataResult SetFunctionLocal(asCScriptFunction& Function, bool IsLocal);
	asEMetadataResult SetNativeFunctionFacts(asCScriptFunction& Function, const asSNativeFunctionFacts& Facts);
	asEMetadataResult SetNativePropertyFacts(asCObjectType& Owner, asCObjectProperty& Property, const asSNativePropertyFacts& Facts);
	asEMetadataResult SetNativeGlobalFacts(asCGlobalProperty& Global, const asSNativeGlobalFacts& Facts);
	asEMetadataResult DefineAccessSpecifier(asCObjectType& Owner, const asSAccessSpecifier& Definition, asSAccessSpecifier*& Out);
	asEMetadataResult SetPropertyAccess(asCObjectType& Owner, asCObjectProperty& Property, asSAccessSpecifier& Policy);
	asEMetadataResult SetFunctionAccess(asCScriptFunction& Function, asSAccessSpecifier& Policy);
	asEMetadataResult SetListInitializer(asCObjectType& Type, asCScriptFunction& Factory, const asSMetadataListPattern& Pattern);
	const asSMetadataListInitializer* GetListInitializer(const asCObjectType& Type) const;
	asEMetadataResult AddProperty(asCObjectType& Owner, const char* Name,
		const asCDataType& Type, asCObjectProperty*& Out, bool IsPrivate = false, bool IsProtected = false);
	asEMetadataResult DefineNativeProperty(asCObjectType& Owner, const char* Name,
		const asCDataType& Type, const asSNativePropertyAddress& Address, asCObjectProperty*& Out);
	asEMetadataResult SetBaseType(asCObjectType& Type, asCObjectType& Base);
	asEMetadataResult SetInterfaceDeclaration(asCObjectType& Type);
	asEMetadataResult AddInterface(asCObjectType& Type, asCObjectType& Interface);
	asEMetadataResult AddBehaviour(asCObjectType& Type, asEBehaviours Behaviour, asCScriptFunction& Function);
	asEMetadataResult AddTemplateArgument(asCObjectType& Type, const asCDataType& Argument);
	asEMetadataResult SetDefaultArrayType(asCObjectType& Type);
	bool IsDefaultArrayType(const asCTypeInfo* Type) const;
	asEMetadataResult AddEnumValue(asCEnumType& Type, const char* Name, int Value);
	asEMetadataResult SetAlias(asCTypedefType& Type, const asCDataType& Alias);
	asEMetadataResult AddChildCallableType(asCObjectType& Type, asCCallableType& CallableType);
	asEMetadataResult SetNativeLayout(asCObjectType& Type, asUINT Size, asUINT Alignment);
	asEMetadataResult SetFixedTemplateLayout(asCObjectType& Type, asUINT Size, asUINT Alignment);
	asEMetadataResult CreateTemplateInstance(asCObjectType& Template, const asSStableKey& TypeUseKey,
		TConstArrayView<asCDataType> Arguments, asCObjectType*& Out);
	bool ValidateFrozenDefinitions() const;
	asEMetadataResult FinalizeLayouts();
	asEMetadataResult Freeze();
	asEFingerprintStatus TryGetSchemaHash(const asCTypeInfo& Type, asSSchemaHash& OutHash,
		TConstArrayView<uint8>* OutWitness = nullptr) const;
	asEFingerprintStatus TryGetLayoutHash(const asCTypeInfo& Type, asSLayoutHash& OutHash,
		TConstArrayView<uint8>* OutWitness = nullptr) const;
	void SetFingerprintDigestProvider(const asIFingerprintDigestProvider* Provider);
	void AttachDependencies(TConstArrayView<asCModuleDefinitionSet*> InDependencies);
	void AdoptEmittedByteCode(TUniquePtr<asCByteCodeImage> InImage);
	asCByteCodeImage* PeekEmittedByteCode() const;
	TUniquePtr<asCByteCodeImage> TakeEmittedByteCode();

private:
	friend class asCScriptEngine;
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
		const asCModuleDefinitionSet*,
		const asSByteCodeEmissionOptions&,
		TUniquePtr<asCByteCodeImage>&);
	asEMetadataResult ValidateNew(const asSStableKey& Key, const char* Name,
		const char* Namespace, bool IsType) const;
	asSNameSpace* InternNamespace(const char* Name);
	asEMetadataResult ValidateDataType(const asCDataType& Type, bool AllowVoid, bool AllowWildcard = false) const;
	asCDataType NormalizeOptions(const asCDataType& Type) const;
	bool ReferencesSet(const asCModuleDefinitionSet* Set) const;
	bool ValidateFrozenIdentity() const;
	bool ValidateListInitializers() const;
	bool ValidateListPattern(const asSMetadataListPattern& Pattern, asUINT GenericArity, int Depth, int& NodeBudget) const;
	bool ValidateGlobalIdentity(const asCGlobalProperty& Global) const;
	asEMetadataResult CreateGlobalPropertyInternal(const asSStableKey& Key, const asSStableKey& TypeUseKey,
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

	const asSMetadataOptions Options;
	std::atomic<asEDefinitionSetState> State{asEDefinitionSetState::Building};
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
	TArray<asCModuleDefinitionSet*> Dependencies;
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
