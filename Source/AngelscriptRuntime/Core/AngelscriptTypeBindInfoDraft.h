#pragma once
#include "AngelscriptTypeBindInfoStore.h"
#include "as_module_definition_set.h"
#include "frontend/Compile/as_binding_declaration.h"

/** Mutable, engine-independent metadata owned by one application of a snapshot. */
class ANGELSCRIPTRUNTIME_API FAngelscriptTypeBindInfoDraft final
{
public:
	static constexpr const TCHAR* StableScope = TEXT("Angelscript.RuntimeBindings");
	static bool Create(TSharedRef<const FAngelscriptTypeBindInfoStore> Store,
		TUniquePtr<FAngelscriptTypeBindInfoDraft>& Out, FString& Diagnostic);
	AS_NAMESPACE_QUALIFIER asCTypeInfo* FindType(const FString& Name, const FString& Namespace = {}) const;
	AS_NAMESPACE_QUALIFIER asSStableKey FindNominal(const FString& Name, const FString& Namespace) const;
	bool ResolveTypeUse(const AS_NAMESPACE_QUALIFIER asSStableKey& Key,
		AS_NAMESPACE_QUALIFIER asCDataType& Out, FString& Diagnostic);
	bool MaterializeTemplateDeclaration(const FString& Declaration,
		TUniquePtr<AS_NAMESPACE_QUALIFIER asCModuleDefinitionSet>& OutSet,
		AS_NAMESPACE_QUALIFIER asCObjectType*& OutType, FString& Diagnostic);
	const FAngelscriptTypeBindDeclaration* FindDeclaration(
		const AS_NAMESPACE_QUALIFIER asCTypeInfo* Type) const;
	AS_NAMESPACE_QUALIFIER asCModuleDefinitionSet* GetSet() const { return Set.Get() ? Set.Get() : SetView; }
	TUniquePtr<AS_NAMESPACE_QUALIFIER asCModuleDefinitionSet> TakeSet()
	{
		SetView = Set.Get();
		return MoveTemp(Set);
	}
	TUniquePtr<AS_NAMESPACE_QUALIFIER asCModuleDefinitionSet> TakeTemplateSet()
	{
		return MoveTemp(TemplateSet);
	}
	const TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>& GetTypeContext() const { return Context; }
	const FAngelscriptTypeBindInfoStore& GetStore() const { return Store.Get(); }
	TSharedRef<const FAngelscriptTypeBindInfoStore> GetStoreShared() const { return Store; }
	AS_NAMESPACE_QUALIFIER asCTypeInfo* GetType(int32 RecordIndex) const { return Types.IsValidIndex(RecordIndex) ? Types[RecordIndex] : nullptr; }
private:
	friend class FAngelscriptTypeBindInfoInstallation;
	explicit FAngelscriptTypeBindInfoDraft(TSharedRef<const FAngelscriptTypeBindInfoStore> InStore) : Store(MoveTemp(InStore)) {}
	bool ResolveTypeUseInImage(const AS_NAMESPACE_QUALIFIER asSStableKey& Key,
		AS_NAMESPACE_QUALIFIER asCModuleDefinitionSet* Target,
		AS_NAMESPACE_QUALIFIER asCDataType& Out, FString& Diagnostic);
	AS_NAMESPACE_QUALIFIER asCTypeInfo* FindTypeByKey(const AS_NAMESPACE_QUALIFIER asSStableKey& Key) const;
	TSharedRef<const FAngelscriptTypeBindInfoStore> Store;
	TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
	TUniquePtr<AS_NAMESPACE_QUALIFIER asCModuleDefinitionSet> Set;
	AS_NAMESPACE_QUALIFIER asCModuleDefinitionSet* SetView = nullptr;
	TUniquePtr<AS_NAMESPACE_QUALIFIER asCModuleDefinitionSet> TemplateSet;
	TArray<TUniquePtr<AS_NAMESPACE_QUALIFIER asCModuleDefinitionSet>> OwnedLateSets;
	TArray<AS_NAMESPACE_QUALIFIER asCModuleDefinitionSet*> LateSets;
	TArray<AS_NAMESPACE_QUALIFIER asCTypeInfo*> Types;
	TArray<AS_NAMESPACE_QUALIFIER asSStableKey> Nominals;
};
