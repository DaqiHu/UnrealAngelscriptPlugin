#pragma once
#include "AngelscriptTypeBindInfoStore.h"
#include "as_metadata_image.h"
#include "frontend/as_binding_declaration.h"

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
		std::shared_ptr<AS_NAMESPACE_QUALIFIER asCMetadataImage>& OutImage,
		AS_NAMESPACE_QUALIFIER asCObjectType*& OutType, FString& Diagnostic);
	const FAngelscriptTypeBindDeclaration* FindDeclaration(
		const AS_NAMESPACE_QUALIFIER asCTypeInfo* Type) const;
	const std::shared_ptr<AS_NAMESPACE_QUALIFIER asCMetadataImage>& GetImage() const { return Image; }
	const TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>& GetTypeContext() const { return Context; }
	const FAngelscriptTypeBindInfoStore& GetStore() const { return Store.Get(); }
	TSharedRef<const FAngelscriptTypeBindInfoStore> GetStoreShared() const { return Store; }
	AS_NAMESPACE_QUALIFIER asCTypeInfo* GetType(int32 RecordIndex) const { return Types.IsValidIndex(RecordIndex) ? Types[RecordIndex] : nullptr; }
private:
	friend class FAngelscriptTypeBindInfoInstallation;
	explicit FAngelscriptTypeBindInfoDraft(TSharedRef<const FAngelscriptTypeBindInfoStore> InStore) : Store(MoveTemp(InStore)) {}
	bool ResolveTypeUseInImage(const AS_NAMESPACE_QUALIFIER asSStableKey& Key,
		const std::shared_ptr<AS_NAMESPACE_QUALIFIER asCMetadataImage>& Target,
		AS_NAMESPACE_QUALIFIER asCDataType& Out, FString& Diagnostic);
	AS_NAMESPACE_QUALIFIER asCTypeInfo* FindTypeByKey(const AS_NAMESPACE_QUALIFIER asSStableKey& Key) const;
	TSharedRef<const FAngelscriptTypeBindInfoStore> Store;
	TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
	std::shared_ptr<AS_NAMESPACE_QUALIFIER asCMetadataImage> Image;
	std::shared_ptr<AS_NAMESPACE_QUALIFIER asCMetadataImage> TemplateImage;
	TArray<std::shared_ptr<AS_NAMESPACE_QUALIFIER asCMetadataImage>> LateImages;
	TArray<AS_NAMESPACE_QUALIFIER asCTypeInfo*> Types;
	TArray<AS_NAMESPACE_QUALIFIER asSStableKey> Nominals;
};
