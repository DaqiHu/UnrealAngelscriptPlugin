#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreNetTypes.h"

#include "Core/AngelscriptType.h"
#include "Core/Artifacts/AngelscriptArtifactIdentity.h"
#include "frontend/as_stable_key.h"

class UClass;
class UDelegateFunction;
class UEnum;
class UFunction;
class UStruct;
class asCModule;
class asCString;
class asIScriptFunction;
class asITypeInfo;

enum class EAngelscriptDescriptorLifecycle : uint8
{
	Parsed,
	Resolved,
	Materialized,
};

struct FAngelscriptResolvedSourceAnchor
{
	FString LogicalSourceKey;
	uint64 ContentRevision = 0;
	uint32 BeginByteOffset = 0;
	uint32 EndByteOffset = 0;

	bool IsValid() const
	{
		return !LogicalSourceKey.IsEmpty()
			&& ContentRevision != 0
			&& BeginByteOffset <= EndByteOffset;
	}
};

struct FAngelscriptDescriptorSemanticInfo
{
	EAngelscriptDescriptorLifecycle Lifecycle = EAngelscriptDescriptorLifecycle::Parsed;
	AS_NAMESPACE_QUALIFIER asSStableKey StableDeclarationKey;
	AS_NAMESPACE_QUALIFIER asSStableKey StableTypeUseKey;
	AS_NAMESPACE_QUALIFIER asSStableKey StableOwnerKey;
	FAngelscriptResolvedSourceAnchor SourceAnchor;

	bool IsResolved() const
	{
		return Lifecycle == EAngelscriptDescriptorLifecycle::Resolved
			&& !StableDeclarationKey.IsZero()
			&& SourceAnchor.IsValid();
	}
};

/** Description of a script property that has been added as an unreal property. */
struct FAngelscriptPropertyDesc
{
	FAngelscriptDescriptorSemanticInfo Semantic;
	FString PropertyName;
	FString LiteralType;
	FAngelscriptTypeUsage PropertyType;
	TMap<FName, FString> Meta;
	bool bBlueprintReadable = false;
	bool bBlueprintWritable = false;
	bool bEditableOnDefaults = false;
	bool bEditableOnInstance = false;
	bool bEditConst = false;
	bool bInstancedReference = false;
	bool bPersistentInstance = false;
	bool bAdvancedDisplay = false;
	bool bTransient = false;
	bool bHasUnrealProperty = false;
	bool bReplicated = false;
	bool bSkipReplication = false;
	bool bSkipSerialization = false;
	bool bSaveGame = false;
	TEnumAsByte<ELifetimeCondition> ReplicationCondition = COND_None;
	bool bRepNotify = false;
	bool bConfig = false;
	bool bInterp = false;
	bool bAssetRegistrySearchable = false;
	bool bNoClear = false;
	bool bIsPrivate = false;
	bool bIsProtected = false;
	int32 ScriptPropertyIndex = -1;
	SIZE_T ScriptPropertyOffset = 0;
	int32 LineNumber = 1;

	bool IsDefinitionEquivalent(const FAngelscriptPropertyDesc& Other) const
	{
		return Other.bBlueprintReadable == bBlueprintReadable
			&& Other.bBlueprintWritable == bBlueprintWritable
			&& Other.bEditableOnDefaults == bEditableOnDefaults
			&& Other.bEditableOnInstance == bEditableOnInstance
			&& Other.bAdvancedDisplay == bAdvancedDisplay
			&& Other.bEditConst == bEditConst
			&& Other.bInstancedReference == bInstancedReference
			&& Other.bPersistentInstance == bPersistentInstance
			&& Other.bTransient == bTransient
			&& Other.bConfig == bConfig
			&& Other.bInterp == bInterp
			&& Other.bAssetRegistrySearchable == bAssetRegistrySearchable
			&& Other.bNoClear == bNoClear
			&& Other.bReplicated == bReplicated
			&& Other.ReplicationCondition == ReplicationCondition
			&& Other.bSkipReplication == bSkipReplication
			&& Other.bSkipSerialization == bSkipSerialization
			&& Other.bSaveGame == bSaveGame
			&& Other.bRepNotify == bRepNotify
			&& Other.bIsPrivate == bIsPrivate
			&& Other.bIsProtected == bIsProtected;
	}
};

/** Description of an argument to an angelscript function. */
struct FAngelscriptArgumentDesc
{
	FAngelscriptDescriptorSemanticInfo Semantic;
	FString ArgumentName;
	FString DefaultValue;
	FAngelscriptTypeUsage Type;
	bool bBlueprintByValue = false;
	bool bBlueprintOutRef = false;
	bool bBlueprintInRef = false;
	bool bInRefForceCopyOut = false;

	bool IsDefinitionEquivalent(const FAngelscriptArgumentDesc& Other) const
	{
		return Other.bBlueprintByValue == bBlueprintByValue
			&& Other.bBlueprintOutRef == bBlueprintOutRef
			&& Other.bBlueprintInRef == bBlueprintInRef
			&& Other.bInRefForceCopyOut == bInRefForceCopyOut
			&& Other.Type == Type;
	}
};

/** Description of a script function that should be bound as an unreal function. */
struct FAngelscriptFunctionDesc
{
	FAngelscriptDescriptorSemanticInfo Semantic;
	FString FunctionName;
	FString OriginalFunctionName;
	FString ScriptFunctionName;
	TMap<FName, FString> Meta;
	FAngelscriptTypeUsage ReturnType;
	TArray<FAngelscriptArgumentDesc> Arguments;
	bool bBlueprintCallable = false;
	bool bBlueprintOverride = false;
	bool bBlueprintEvent = false;
	bool bBlueprintPure = false;
	bool bNetMulticast = false;
	bool bNetClient = false;
	bool bNetServer = false;
	bool bNetValidate = false;
	bool bUnreliable = false;
	bool bBlueprintAuthorityOnly = false;
	bool bExec = false;
	bool bCanOverrideEvent = true;
	bool bIsStatic = false;
	bool bIsConstMethod = false;
	asIScriptFunction* ScriptFunction = nullptr;
	bool bThreadSafe = false;
	UFunction* Function = nullptr;
	bool bIsNoOp = false;
	bool bIsPrivate = false;
	bool bIsProtected = false;
	int32 LineNumber = 1;

	bool SignatureMatches(TSharedPtr<FAngelscriptFunctionDesc> OtherFunction, bool bCheckNames = false) const;
	bool ParametersMatches(TSharedPtr<FAngelscriptFunctionDesc> OtherFunction, bool bCheckNames = false) const;

	bool IsDefinitionEquivalent(const FAngelscriptFunctionDesc& Other) const
	{
		return Other.bBlueprintCallable == bBlueprintCallable
			&& Other.bBlueprintOverride == bBlueprintOverride
			&& Other.bBlueprintEvent == bBlueprintEvent
			&& Other.bBlueprintPure == bBlueprintPure
			&& Other.bUnreliable == bUnreliable
			&& Other.bNetMulticast == bNetMulticast
			&& Other.bNetClient == bNetClient
			&& Other.bNetServer == bNetServer
			&& Other.bBlueprintAuthorityOnly == bBlueprintAuthorityOnly
			&& Other.bExec == bExec
			&& Other.bCanOverrideEvent == bCanOverrideEvent
			&& Other.bIsStatic == bIsStatic
			&& Other.bIsConstMethod == bIsConstMethod
			&& Other.bThreadSafe == bThreadSafe
			&& Other.bIsPrivate == bIsPrivate
			&& Other.bIsProtected == bIsProtected;
	}
};

/** Description of a script class during preprocessing. */
struct FAngelscriptClassDesc
{
	FAngelscriptDescriptorSemanticInfo Semantic;
	FString ClassName;
	FString SuperClass;
	UClass* CodeSuperClass = nullptr;
	bool bSuperIsCodeClass = false;
	bool bIsStaticsClass = false;
	bool bAbstract = false;
	bool bTransient = false;
	bool bHideDropdown = false;
	bool bDefaultToInstanced = false;
	bool bEditInlineNew = false;
	bool bIsDeprecatedClass = false;
	bool bPlaceable = true;
	bool bIsStruct = false;
	TArray<FString> ImplementedInterfaces;
	FString ConfigName;
	asITypeInfo* ScriptType = nullptr;
	UClass* Class = nullptr;
	UStruct* Struct = nullptr;
	TArray<TSharedRef<FAngelscriptPropertyDesc>> Properties;
	TArray<TSharedRef<FAngelscriptFunctionDesc>> Methods;
	FString StaticClassGlobalVariableName;
	FString DefaultsCode;
	int32 LineNumber = 1;
	TMap<FName, FString> Meta;
	FString ComposeOntoClass;
	TOptional<FString> Namespace;

	TSharedPtr<FAngelscriptPropertyDesc> GetProperty(const FString& PropName)
	{
		for (const TSharedRef<FAngelscriptPropertyDesc>& PropDesc : Properties)
		{
			if (PropName.Equals(PropDesc->PropertyName)) return PropDesc;
		}
		return nullptr;
	}
	TSharedPtr<FAngelscriptPropertyDesc> GetProperty(asCString& PropName);

	TSharedPtr<FAngelscriptFunctionDesc> GetMethod(const FString& FuncName)
	{
		for (const TSharedRef<FAngelscriptFunctionDesc>& FuncDesc : Methods)
		{
			if (FuncName.Equals(FuncDesc->FunctionName)) return FuncDesc;
		}
		return nullptr;
	}
	TSharedPtr<FAngelscriptFunctionDesc> GetMethodByScriptName(const FString& FuncName)
	{
		for (const TSharedRef<FAngelscriptFunctionDesc>& FuncDesc : Methods)
		{
			if (FuncName.Equals(FuncDesc->ScriptFunctionName)) return FuncDesc;
		}
		return nullptr;
	}

	bool AreFlagsEqual(const FAngelscriptClassDesc& Other) const
	{
		return bAbstract == Other.bAbstract
			&& bTransient == Other.bTransient
			&& bHideDropdown == Other.bHideDropdown
			&& bDefaultToInstanced == Other.bDefaultToInstanced
			&& bEditInlineNew == Other.bEditInlineNew
			&& bIsDeprecatedClass == Other.bIsDeprecatedClass
			&& bPlaceable == Other.bPlaceable
			&& ImplementedInterfaces == Other.ImplementedInterfaces;
	}
};

/** Description of a script enum during preprocessing. */
struct FAngelscriptEnumDesc
{
	FAngelscriptDescriptorSemanticInfo Semantic;
	FString EnumName;
	FString Documentation;
	TArray<FName> ValueNames;
	TArray<int32> EnumValues;
	UEnum* Enum = nullptr;
	asITypeInfo* ScriptType = nullptr;
	int32 LineNumber = 1;
	TMap<TPair<FName, int32>, FString> Meta;
};

/** Description of a delegate signature declared in angelscript. */
struct FAngelscriptDelegateDesc
{
	FAngelscriptDescriptorSemanticInfo Semantic;
	FString DelegateName;
	bool bIsMulticast = false;
	TSharedPtr<FAngelscriptFunctionDesc> Signature;
	UDelegateFunction* Function = nullptr;
	asITypeInfo* ScriptType = nullptr;
	int32 LineNumber = 1;
};

struct FAngelscriptLiteralAssetFunctionDesc
{
	FString AssetName;
	FString DeclaredTypeName;
	FString GetterFunctionName;
	FString InitializerFunctionName;
	int32 PostInitFunctionIndex = INDEX_NONE;
};

enum class EAngelscriptGeneratedSourceKind : uint8
{
	None = 0,
	LiteralAsset,
	SubsystemAccessor,
	ClassHelper,
	StaticClassHelper,
	DelegateHelper,
	RangeBasedFor,
};

struct FAngelscriptSourceProvenanceAnchor
{
	FString Section;
	uint32 SourceOffset = 0;
	uint32 SourceLength = 0;
	uint32 Row = 0;
	uint32 Column = 0;
};

struct FAngelscriptProcessedSourceProvenanceRange
{
	int32 ProcessedCodeUnitOffset = 0;
	int32 ProcessedCodeUnitLength = 0;
	bool bHasAuthoredOrigin = false;
	FAngelscriptSourceProvenanceAnchor AuthoredOrigin;
	bool bHasGeneratedOrigin = false;
	EAngelscriptGeneratedSourceKind GeneratedKind = EAngelscriptGeneratedSourceKind::None;
	FString GeneratorName;
	bool bHasGeneratedAuthoredAnchor = false;
	FAngelscriptSourceProvenanceAnchor GeneratedAuthoredAnchor;
};

/** Description of a script module during preprocessing. */
struct FAngelscriptModuleDesc
{
	FAngelscriptDescriptorSemanticInfo Semantic;
	FString ModuleName;
	struct FCodeSection
	{
		FString VirtualPath;
		FString RelativeFilename;
		FString AbsoluteFilename;
		FString Code;
		int64 CodeHash = 0;
		FAngelscriptHash256 SourceTextHash;
		TArray<FAngelscriptProcessedSourceProvenanceRange> SourceProvenance;
	};
	TArray<FCodeSection> Code;
	int64 CodeHash = 0;
	int64 CombinedDependencyHash = 0;
	TArray<TSharedRef<FAngelscriptClassDesc>> Classes;
	TArray<TSharedRef<FAngelscriptEnumDesc>> Enums;
	TArray<TSharedRef<FAngelscriptDelegateDesc>> Delegates;
	TArray<FString> ImportedModules;
	TArray<FString> PostInitFunctions;
	TArray<FAngelscriptLiteralAssetFunctionDesc> LiteralAssetFunctions;
#if WITH_EDITOR
	struct FUsageRestriction
	{
		bool bIsAllow;
		FString Pattern;
	};
	TArray<FUsageRestriction> UsageRestrictions;
	TArray<TPair<int, int>> EditorOnlyBlockLines;
#endif
	asCModule* ScriptModule = nullptr;
	bool bCompileError = false;
	bool bLoadedPrecompiledCode = false;
	bool bLoadedIncrementalCache = false;
	bool bModuleSwapInError = false;

	TSharedPtr<FAngelscriptClassDesc> GetClass(const FString& ClassName)
	{
		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : Classes)
		{
			if (ClassName == ClassDesc->ClassName) return ClassDesc;
		}
		return nullptr;
	}
	TSharedPtr<FAngelscriptClassDesc> GetClass(asITypeInfo* Type)
	{
		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : Classes)
		{
			if (ClassDesc->ScriptType == Type) return ClassDesc;
		}
		return nullptr;
	}
	TSharedPtr<FAngelscriptEnumDesc> GetEnum(const FString& EnumName)
	{
		for (const TSharedRef<FAngelscriptEnumDesc>& EnumDesc : Enums)
		{
			if (EnumName == EnumDesc->EnumName) return EnumDesc;
		}
		return nullptr;
	}
};
