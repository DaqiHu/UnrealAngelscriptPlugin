#pragma once

#include "AngelscriptTypeBindInfoStore.h"

namespace UE::Angelscript::Private { class FAngelscriptBindCollection; }

enum class EAngelscriptBindingInspectionScope : uint8
{
	CollectionOnly,
	SelectedFamily,
	FullRuntime,
};

struct FAngelscriptBindingInspectionSource
{
	FString OwnerModule;
	FString Provider;
	FString File;
	int32 Line = 0;
	FString Phase;
};

struct FAngelscriptBindingInspectionMember
{
	FString Owner;
	FString Identity;
	FString Kind;
	FString Declaration;
	FString TypeReference;
	FString TypeCategory;
	uint64 PropertyOffset = 0;
	FString EnumValue;
	FString ConstantValue;
	FString Storage;
	FString NativeTarget;
	FString FunctionLifetime;
	FString AuxiliaryLifetime;
	uint64 Traits = 0;
	FString FirstParameter;
	FString CompileOut;
	bool bCanRead = true;
	bool bCanWrite = true;
	bool bCanEdit = true;
	bool bProtected = false;
	FString DeprecationMessage;
	FString Documentation;
	FString Category;
	TArray<FString> Recipes;
	FAngelscriptBindingInspectionSource Source;
};

struct FAngelscriptBindingInspectionType
{
	FString Identity;
	FString Name;
	FString Namespace;
	FString Kind;
	FString BaseType;
	TArray<FString> Interfaces;
	FString AliasTarget;
	FString TemplateType;
	FString TemplateDeclaration;
	uint64 NativeSize = 0;
	uint64 NativeAlignment = 0;
	uint64 ObjectFlags = 0;
	FString NativeOwner;
	FString AdapterName;
	TArray<FString> LifecycleRecipes;
	TArray<FAngelscriptBindingInspectionSource> Sources;
	TArray<FAngelscriptBindingInspectionMember> Members;
};

struct FAngelscriptBindingInspectionReflectedType
{
	FString NativePath;
	FString Kind;
	FString IntendedIdentity;
	bool bEligible = false;
	FString ExclusionReason;
	FString ParentPath;
	TArray<FString> Interfaces;
	TArray<FString> Properties;
	TArray<TPair<FString, FString>> EnumValues;
	bool bHasGCReferences = false;
	bool bNeedsDestruction = false;
	TArray<FString> LifetimeRecipes;
};

struct FAngelscriptBindingInspectionProvider
{
	FAngelscriptBindingInspectionSource Source;
	bool bEligible = true;
	FString Disposition;
};

/** Pointer-free, deterministic projection of a collection or detached Store. */
struct FAngelscriptBindingInspection
{
	static constexpr int32 SchemaVersion = 1;
	EAngelscriptBindingInspectionScope Scope = EAngelscriptBindingInspectionScope::SelectedFamily;
	FString Stage;
	bool bCollectionOnly = false;
	bool bSealed = false;
	bool bRecordingFailed = false;
	bool bValidationAttempted = false;
	bool bValid = false;
	FString Diagnostic;
	FAngelscriptBindRecordingPolicy Policy;
	TArray<FAngelscriptBindingInspectionProvider> Providers;
	TArray<FAngelscriptBindingInspectionType> Types;
	TArray<FAngelscriptBindingInspectionReflectedType> ReflectedTypes;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptTypeBindInfoInspection final
{
public:
	static bool InspectCollection(
		const UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
		FAngelscriptBindingInspection& Out, FString& Diagnostic);
	static bool InspectStore(TSharedRef<const FAngelscriptTypeBindInfoStore> Store,
		EAngelscriptBindingInspectionScope Scope,
		FAngelscriptBindingInspection& Out, FString& Diagnostic);
};
