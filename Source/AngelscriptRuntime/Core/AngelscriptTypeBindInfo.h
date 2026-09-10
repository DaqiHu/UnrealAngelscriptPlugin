#pragma once

#include "CoreMinimal.h"
#include "AngelscriptInclude.h"
#include "FunctionCallers.h"
#include "FunctionBinding/NativeModuleFunctionBindingBridge.h"

class FAngelscriptTypeBindInfoStore;

enum class EAngelscriptBindingStage : uint8 { None, Recording, Engine, Types, Members, Native };
struct FAngelscriptBindingDiagnostic
{
	EAngelscriptBindingStage Stage = EAngelscriptBindingStage::None;
	FString Message;
};
class UStruct;
class UFunction;
struct FAngelscriptStaticJITNativeCallDescriptor;

enum class EAngelscriptBindPhase : uint8
{
	TypeDeclarations,
	TypeInfrastructure,
	ExplicitBindings,
	GeneratedBindings,
	ReflectionBindings,
	PostReflectionBindings,
	Finalization,
};

/** Effective host policy captured with the snapshot, without consulting an Engine. */
struct FAngelscriptBindRecordingPolicy
{
	bool bEditor = WITH_EDITOR;
	bool bTest = UE_BUILD_TEST;
	bool bLogging = !NO_LOGGING;
	bool bShipping = UE_BUILD_SHIPPING;
	bool bSimulateCooked = false;
	bool bForceConstWithinDevelopmentOnlyFunctions = false;
	bool bScriptFloatIsFloat64 = true;
	bool bDeprecateDoubleType = false;
	/** Full Runtime snapshots retain other-module rows as policy exclusions. */
	bool bRuntimeModuleOnly = false;
	FName Platform;

	bool operator==(const FAngelscriptBindRecordingPolicy& Other) const
	{
		return bEditor == Other.bEditor
			&& bTest == Other.bTest
			&& bLogging == Other.bLogging
			&& bShipping == Other.bShipping
			&& bSimulateCooked == Other.bSimulateCooked
			&& bForceConstWithinDevelopmentOnlyFunctions == Other.bForceConstWithinDevelopmentOnlyFunctions
			&& bScriptFloatIsFloat64 == Other.bScriptFloatIsFloat64
			&& bDeprecateDoubleType == Other.bDeprecateDoubleType
			&& bRuntimeModuleOnly == Other.bRuntimeModuleOnly
			&& Platform == Other.Platform;
	}
	bool operator!=(const FAngelscriptBindRecordingPolicy& Other) const { return !(*this == Other); }
};

enum class EAngelscriptTypeBindKind : uint8
{
	Value,
	ObjectHandle,
	Enum,
	Interface,
	Template,
	Namespace,
	Alias,
};

enum class EAngelscriptTemplateContainerKind : uint8
{
	Generic,
	Array,
	Map,
	Set,
	Optional,
	ObjectPtr,
	SubclassOf,
	WeakObjectPtr,
	SoftObjectPtr,
	SoftClassPtr,
};

enum class EAngelscriptTypeBindMemberKind : uint8
{
	Constructor,
	Destructor,
	Method,
	Property,
	Behaviour,
	GlobalFunction,
	GlobalProperty,
	EnumValue,
};

struct FAngelscriptTypeBindSource
{
	FName OwnerModule;
	FName Provider;
	FString File;
	int32 Line = 0;
	EAngelscriptBindPhase Phase = EAngelscriptBindPhase::TypeDeclarations;
};

struct FAngelscriptRecordedProvider
{
	FAngelscriptTypeBindSource Source;
	bool bEligible = true;
};

enum class EAngelscriptBindEffectKind : uint8
{
	ToString,
	SkipFunction,
	SkipClass,
	DeprecateFunction,
	DirectBindArchitectureProbe,
};

/** A typed, ordered non-declaration contribution applied to one fresh Engine. */
struct FAngelscriptBindEffect
{
	EAngelscriptBindEffectKind Kind = EAngelscriptBindEffectKind::ToString;
	FString Name;
	FString SecondaryName;
	void (*ToString)(void*, FString&) = nullptr;
	bool bImplicitConversion = false;
	bool bHandleType = false;
	FAngelscriptTypeBindSource Source;
};

/** One detached manual/generated native-module function-address candidate. */
struct FAngelscriptNativeFunctionMapRecord
{
	FName ModuleName;
	FString OwnerType;
	FString FunctionName;
	FString Signature;
	void (*Thunk)(UObject*, FAngelscriptNativeModuleFunctionBindingCallFrame*) = nullptr;
	uint16 ArgCount = 0;
	uint16 ReturnSize = 0;
	uint32 Flags = 0;
	uint32 LayoutVersion = FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected;
	EAngelscriptFunctionBindingOrigin Origin = EAngelscriptFunctionBindingOrigin::Unknown;
	bool bPlaceholder = false;
	FAngelscriptTypeBindSource Source;
};

struct FAngelscriptTypeBindDeclaration
{
	FString Name;
	FString Namespace;
	EAngelscriptTypeBindKind Kind = EAngelscriptTypeBindKind::Value;
	uint32 NativeSize = 0;
	uint32 NativeAlignment = 0;
	uint64 ObjectFlags = 0;
	FString BaseType;
	TArray<FString> Interfaces;
	FString AliasTarget;
	FString TemplateType;
	FString TemplateDeclaration;
	UStruct* UnrealType = nullptr;
	FString NativeOwner;
	FString AdapterName;
	void (*DefaultConstruct)(void*) = nullptr;
	void (*CopyConstruct)(void*, const void*) = nullptr;
	void (*Destroy)(void*) = nullptr;
	bool (*Equal)(const void*, const void*) = nullptr;
	uint32 (*Hash)(const void*) = nullptr;
	bool bTriviallyCopyable = false;
	bool bTriviallyDestructible = false;
	EAngelscriptTemplateContainerKind TemplateContainer = EAngelscriptTemplateContainerKind::Generic;
	FAngelscriptTypeBindSource Source;
};

/** Adapter facts for builtins which are owned by the VM rather than declared as object types. */
struct FAngelscriptPrimitiveBindDefinition
{
	FString Name;
	FString NativeOwner;
	FString AdapterName;
	uint32 NativeSize = 0;
	uint32 NativeAlignment = 0;
	TArray<FString> Aliases;
	FAngelscriptTypeBindSource Source;
};

enum class EAngelscriptBindFunctionTrait : uint32
{
	None = 0,
	EditorOnly = 1 << 0,
	Deprecated = 1 << 1,
	PropertyAccessor = 1 << 2,
	GeneratedAccessor = 1 << 3,
	NoDiscard = 1 << 4,
	WorldContext = 1 << 5,
	NotCallable = 1 << 6,
	ImplicitConstructor = 1 << 7,
	ForceConstArguments = 1 << 8,
};
ENUM_CLASS_FLAGS(EAngelscriptBindFunctionTrait);

enum class EAngelscriptBindFirstParameter : uint8 { None, ScriptFunction, ScriptObjectType };
enum class EAngelscriptBindCompileOut : uint8
{
	None, Entirely, MethodChain, InTest, IfNoLog, Ensure, Check, ReplaceFirstArgumentInTest,
};

enum class EAngelscriptNativeBindRecipeKind : uint8
{
	Constructor, Destructor, Assignment, UObjectCast, Method, Function, FunctionHeader,
	ExternalCall, UFunction, ArrayIndex, ArrayIteratorCreate, ArrayIteratorProceed,
	TemplateCall, DelegateExecute, MulticastExecute, EventExecute, PushArgument, PushArgumentRef,
};

/** Lifetime category for data referenced by a detached binding record. */
enum class EAngelscriptBindHostLifetime : uint8
{
	None,
	CopiedRecord,
	BorrowedHost,
	RetainedHost,
};

struct FAngelscriptNativeBindRecipe
{
	EAngelscriptNativeBindRecipeKind Kind = EAngelscriptNativeBindRecipeKind::Function;
	FString Name;
	FString Header;
	FString CustomForm;
	UFunction* UnrealFunction = nullptr;
	bool bTrivial = false;
	bool bGuaranteed = false;
	bool bNeedsCompare = false;
	bool bNeedsCopy = false;
	TSharedPtr<const FAngelscriptStaticJITNativeCallDescriptor> ExternalCall;
};

struct FAngelscriptRecordedNativeCallable
{
	asSFuncPtr Function;
	ASAutoCaller::FunctionCaller Caller = ASAutoCaller::FunctionCaller::Make();
	asECallConvTypes CallConv = asCALL_CDECL;
	void* Auxiliary = nullptr;
	asEBehaviours Behaviour = asBEHAVE_MAX;
	EAngelscriptBindHostLifetime FunctionLifetime = EAngelscriptBindHostLifetime::None;
	EAngelscriptBindHostLifetime AuxiliaryLifetime = EAngelscriptBindHostLifetime::None;
};

struct FAngelscriptTypeBindInfoMember
{
	EAngelscriptTypeBindMemberKind Kind = EAngelscriptTypeBindMemberKind::Method;
	int PublishedFunctionId = -1;
	FString Declaration;
	SIZE_T PropertyOffset = 0;
	int32 EnumValue = 0;
	const void* GlobalAddress = nullptr;
	EAngelscriptBindHostLifetime GlobalStorageLifetime = EAngelscriptBindHostLifetime::None;
	bool bPureConstant = false;
	uint64 ConstantValue = 0;
	bool bCanRead = true;
	bool bCanWrite = true;
	bool bCanEdit = true;
	bool bProtected = false;
	FAngelscriptRecordedNativeCallable Native;
	EAngelscriptBindFunctionTrait Traits = EAngelscriptBindFunctionTrait::None;
	EAngelscriptBindFirstParameter FirstParameter = EAngelscriptBindFirstParameter::None;
	EAngelscriptBindCompileOut CompileOut = EAngelscriptBindCompileOut::None;
	int32 DeterminesOutputTypeArgument = INDEX_NONE;
	FString DeprecationMessage;
	FString Documentation;
	FString Category;
	UFunction* DocumentationFunction = nullptr;
	TArray<FAngelscriptNativeBindRecipe> NativeRecipes;
	FAngelscriptTypeBindSource Source;
};

struct FAngelscriptTypeBindInfo
{
	FAngelscriptTypeBindDeclaration Declaration;
	int PublishedTypeId = -1;
	TArray<FAngelscriptTypeBindSource> Sources;
	TArray<FAngelscriptTypeBindInfoMember> Members;
};

struct FAngelscriptPublishedMemberInfo
{
	FString Name;
	FString Declaration;
	FString TypeName;
	EAngelscriptTypeBindMemberKind Kind = EAngelscriptTypeBindMemberKind::Method;
	SIZE_T PropertyOffset = 0;
	int PublishedFunctionId = -1;
};

struct FAngelscriptPublishedClassInfo
{
	FString Name;
	FString Namespace;
	int TypeId = -1;
	uint32 NativeSize = 0;
	uint32 NativeAlignment = 0;
	TArray<FAngelscriptPublishedMemberInfo> Members;
};

/** An index remains stable while recording grows; the owning store is never moved. */
struct FAngelscriptTypeBindHandle
{
	const FAngelscriptTypeBindInfoStore* Store = nullptr;
	int32 Index = INDEX_NONE;

	bool operator==(const FAngelscriptTypeBindHandle& Other) const
	{
		return Store == Other.Store && Index == Other.Index;
	}
};

struct FAngelscriptTypeBindMemberHandle
{
	FAngelscriptTypeBindHandle Type;
	int32 Index = INDEX_NONE;
};
