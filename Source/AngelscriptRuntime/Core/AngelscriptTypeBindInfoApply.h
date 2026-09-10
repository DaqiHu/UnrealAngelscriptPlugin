#pragma once
#include "AngelscriptTypeBindInfoDraft.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "UObject/StrongObjectPtr.h"

struct FAngelscriptReflectedFunction;
enum class EAngelscriptReflectedParameterDirection : uint8;
enum class EAngelscriptReflectedFunctionDispatch : uint8;

struct FAngelscriptInstalledBindMember
{
	FAngelscriptTypeBindMemberHandle Record;
	AS_NAMESPACE_QUALIFIER asCScriptFunction* Function = nullptr;
	AS_NAMESPACE_QUALIFIER asCGlobalProperty* Global = nullptr;
	AS_NAMESPACE_QUALIFIER asCObjectProperty* Property = nullptr;
};

struct FAngelscriptTemplateElementOperations
{
	AS_NAMESPACE_QUALIFIER asCDataType Type;
	const FAngelscriptTypeBindDeclaration* Declaration = nullptr;
	const struct FAngelscriptReflectedTypeRecord* Reflected = nullptr;
	const struct FAngelscriptTemplateInstanceOperations* Nested = nullptr;
	FString DisplayName;
	uint32 Size = 0;
	uint32 Alignment = 1;
	bool bPrimitive = false;
	bool bEnumScalar = false;
	bool bObjectHandle = false;
	bool bCanConstruct = false;
	bool bCanCopy = false;
	bool bCanDestroy = false;
	bool bCanEqual = false;
	bool bCanHash = false;
};

/** Per-owner operations for one concrete template specialization. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptTemplateInstanceOperations
{
	AS_NAMESPACE_QUALIFIER asCObjectType* Type = nullptr;
	EAngelscriptTemplateContainerKind ContainerKind = EAngelscriptTemplateContainerKind::Generic;
	uint32 ValueSize = 0;
	uint32 ValueAlignment = 1;
	UClass* ObjectConstraint = nullptr;
	TArray<FAngelscriptTemplateElementOperations> Arguments;

	bool ConstructValue(void* Address, FString& Diagnostic) const;
	bool CopyConstructValue(void* Address, const void* Source, FString& Diagnostic) const;
	bool DestroyValue(void* Address, FString& Diagnostic) const;
	void EnumerateReferences(const void* Address, TFunctionRef<void(UObject*)> Visitor) const;
	bool ArrayAddCopy(FScriptArray& Array, const void* Value, FString& Diagnostic) const;
	bool ArrayAppendCopy(FScriptArray& Array, const FScriptArray& Other, FString& Diagnostic) const;
	bool ArrayRemoveAt(FScriptArray& Array, int32 Index, FString& Diagnostic) const;
	bool ArrayEmpty(FScriptArray& Array, FString& Diagnostic) const;
	void* ArrayElement(FScriptArray& Array, int32 Index, FString& Diagnostic) const;
	const void* ArrayElement(const FScriptArray& Array, int32 Index, FString& Diagnostic) const;
	int32 ArrayFindIndex(const FScriptArray& Array, const void* Value, FString& Diagnostic) const;
	bool ArrayEquals(const FScriptArray& A, const FScriptArray& B, FString& Diagnostic) const;
	bool MapSetCopy(FScriptMap& Map, const void* Key, const void* Value, FString& Diagnostic) const;
	bool MapFindCopy(const FScriptMap& Map, const void* Key, void* OutValue, FString& Diagnostic) const;
	bool MapRemove(FScriptMap& Map, const void* Key, FString& Diagnostic) const;
	bool MapEmpty(FScriptMap& Map, FString& Diagnostic) const;
	int32 MapFirstIndex(const FScriptMap& Map, FString& Diagnostic) const;
	int32 MapNextIndex(const FScriptMap& Map, int32 Index, FString& Diagnostic) const;
	const void* MapKey(const FScriptMap& Map, int32 Index, FString& Diagnostic) const;
	void* MapValue(FScriptMap& Map, int32 Index, FString& Diagnostic) const;
	const void* MapValue(const FScriptMap& Map, int32 Index, FString& Diagnostic) const;
	bool SetAddCopy(FScriptSet& Set, const void* Value, FString& Diagnostic) const;
	bool SetContains(const FScriptSet& Set, const void* Value, FString& Diagnostic) const;
	bool SetRemove(FScriptSet& Set, const void* Value, FString& Diagnostic) const;
	bool SetEmpty(FScriptSet& Set, FString& Diagnostic) const;
	int32 SetFirstIndex(const FScriptSet& Set, FString& Diagnostic) const;
	int32 SetNextIndex(const FScriptSet& Set, int32 Index, FString& Diagnostic) const;
	void* SetElement(FScriptSet& Set, int32 Index, FString& Diagnostic) const;
	const void* SetElement(const FScriptSet& Set, int32 Index, FString& Diagnostic) const;
	bool OptionalIsSet(const void* Optional, FString& Diagnostic) const;
	bool OptionalSetCopy(void* Optional, const void* Value, FString& Diagnostic) const;
	bool OptionalReset(void* Optional, FString& Diagnostic) const;
	void* OptionalValue(void* Optional, FString& Diagnostic) const;
	const void* OptionalValue(const void* Optional, FString& Diagnostic) const;
	UClass* GetObjectConstraint(FString& Diagnostic) const;
	bool AssignSubclass(TSubclassOf<UObject>& Value, UClass* Class, FString& Diagnostic) const;
	bool AssignWeakObject(TWeakObjectPtr<UObject>& Value, UObject* Object, FString& Diagnostic) const;
	bool AssignSoftObjectPath(FSoftObjectPtr& Value, const FSoftObjectPath& Path, FString& Diagnostic) const;
	int32 GetArgumentCount() const;
};

/** Metadata and record correspondence owned by one application of a Store. */
class ANGELSCRIPTRUNTIME_API FAngelscriptTypeBindInfoInstallation final
{
public:
	~FAngelscriptTypeBindInfoInstallation();
	AS_NAMESPACE_QUALIFIER asCMetadataImage* GetImage() const { return Draft->GetImage(); }
	TUniquePtr<AS_NAMESPACE_QUALIFIER asCMetadataImage> TakeImage() { return Draft->TakeImage(); }
	TUniquePtr<AS_NAMESPACE_QUALIFIER asCMetadataImage> TakeTemplateImage() { return Draft->TakeTemplateImage(); }
	AS_NAMESPACE_QUALIFIER asCTypeInfo* FindType(const FString& Name, const FString& Namespace = {}) const { return Draft->FindType(Name, Namespace); }
	AS_NAMESPACE_QUALIFIER asCScriptFunction* FindFunction(FAngelscriptTypeBindMemberHandle Record) const;
	TConstArrayView<FAngelscriptInstalledBindMember> GetMembers() const { return Members; }
	const FAngelscriptTypeBindInfoStore& GetStore() const { return Draft->GetStore(); }
	TSharedRef<const FAngelscriptTypeBindInfoStore> GetStoreShared() const { return Draft->GetStoreShared(); }
	bool MaterializeTemplateInstance(const FString& Declaration,
		AS_NAMESPACE_QUALIFIER asCScriptEngine& Engine,
		AS_NAMESPACE_QUALIFIER asCObjectType*& OutType, FString& Diagnostic) const;
	const FAngelscriptTemplateInstanceOperations* FindTemplateOperations(
		const AS_NAMESPACE_QUALIFIER asCObjectType* Type) const;
	bool ReadReflectedProperty(const AS_NAMESPACE_QUALIFIER asCTypeInfo* OwnerType, UObject* NativeType,
		const void* Container, FName PropertyName, void* OutValue, FString& Diagnostic) const;
	bool WriteReflectedProperty(const AS_NAMESPACE_QUALIFIER asCTypeInfo* OwnerType, UObject* NativeType,
		void* Container, FName PropertyName, const void* Value, FString& Diagnostic) const;
	bool ReflectedTypeImplements(const AS_NAMESPACE_QUALIFIER asCTypeInfo* OwnerType, UClass* NativeClass,
		UClass* InterfaceClass, FString& Diagnostic) const;
	bool InitializeReflectedStruct(const AS_NAMESPACE_QUALIFIER asCTypeInfo* OwnerType, UScriptStruct* Struct,
		void* Address, FString& Diagnostic) const;
	bool CopyReflectedStruct(const AS_NAMESPACE_QUALIFIER asCTypeInfo* OwnerType, UScriptStruct* Struct,
		void* Destination, const void* Source, FString& Diagnostic) const;
	bool DestroyReflectedStruct(const AS_NAMESPACE_QUALIFIER asCTypeInfo* OwnerType, UScriptStruct* Struct,
		void* Address, FString& Diagnostic) const;
	bool RetainReflectedObject(UObject* Object, FString& Diagnostic) const;
	bool AssignStrongObject(const FAngelscriptTemplateInstanceOperations& Operations,
		TObjectPtr<UObject>& Value, UObject* Object, FString& Diagnostic) const;
	void ReleaseReflectedObjects() const;
	const FAngelscriptNativeFunctionMapRecord* FindNativeFunctionMap(
		const FString& OwnerType, const FString& FunctionName) const;
	bool InstallAlternateNativeFunctionMap(TConstArrayView<FAngelscriptNativeFunctionMapRecord> Records,
		FString& Diagnostic) const;
	bool InvokeNativeFunctionMap(const FString& OwnerType, const FString& FunctionName, UObject* Self,
		TConstArrayView<void*> Arguments, void* ReturnAddress, FString& Diagnostic) const;
	const FAngelscriptReflectedFunction* FindReflectedFunction(const AS_NAMESPACE_QUALIFIER asCTypeInfo* OwnerType,
		UClass* NativeClass, FName FunctionName, FString& Diagnostic) const;
	int32 GetInstalledReflectedFunctionCount() const;
	bool InvokeReflectedFunction(const AS_NAMESPACE_QUALIFIER asCTypeInfo* OwnerType, UClass* NativeClass,
		UObject* Receiver, FName FunctionName, TConstArrayView<void*> Arguments,
		TConstArrayView<EAngelscriptReflectedParameterDirection> Directions, void* ReturnAddress,
		EAngelscriptReflectedFunctionDispatch* OutDispatch, FString& Diagnostic) const;
	const FAngelscriptReflectedFunction* FindReflectedLibraryFunction(
		UClass* LibraryClass, FName FunctionName, FString& Diagnostic) const;
	bool InvokeReflectedLibraryFunction(UClass* LibraryClass, UObject* MixinReceiver, UObject* WorldContext,
		FName FunctionName, TConstArrayView<void*> Arguments,
		TConstArrayView<EAngelscriptReflectedParameterDirection> Directions, void* ReturnAddress,
		UClass** OutDeterminedType, FString& Diagnostic) const;
	bool BindDelegate(FScriptDelegate& Delegate, UObject* Object, FName FunctionName,
		UFunction* Signature, FString& Diagnostic) const;
	bool AddMulticastDelegate(FMulticastScriptDelegate& Delegate, UObject* Object, FName FunctionName,
		UFunction* Signature, FString& Diagnostic) const;
	bool RemoveMulticastDelegate(FMulticastScriptDelegate& Delegate, UObject* Object, FName FunctionName,
		FString& Diagnostic) const;
	bool ExecuteDelegate(const FScriptDelegate& Delegate, UFunction* Signature, void* Parameters,
		FString& Diagnostic) const;
	bool ExecuteMulticastDelegate(const FMulticastScriptDelegate& Delegate, UFunction* Signature,
		void* Parameters, FString& Diagnostic) const;
	bool BindDelegatePayload(UObject* Object, FName FunctionName, UScriptStruct* PayloadType,
		const void* Payload, int32& OutHandle, FString& Diagnostic) const;
	bool ExecuteDelegatePayload(int32 Handle, FString& Diagnostic) const;
	bool UnbindDelegatePayload(int32 Handle, FString& Diagnostic) const;
	const FAngelscriptReflectedFunction* RegisterReflectedEvent(UClass* Class, FName FunctionName,
		FString& Diagnostic) const;
	int32 GetDelegateRegistryEntryCount() const;
	bool InvokeCollisionLineTraceTestByChannel(UWorld* World, const FVector& Start, const FVector& End,
		ECollisionChannel Channel, const FCollisionQueryParams& QueryParams,
		const FCollisionResponseParams& ResponseParams, bool& OutHit, FString& Diagnostic) const;
private:
	friend class FAngelscriptTypeBindInfoApply;
	bool BuildTemplateOperations(AS_NAMESPACE_QUALIFIER asCObjectType* Type, FString& Diagnostic) const;
	TUniquePtr<FAngelscriptTypeBindInfoDraft> Draft;
	TArray<FAngelscriptInstalledBindMember> Members;
	mutable TArray<TUniquePtr<FAngelscriptTemplateInstanceOperations>> TemplateOperations;
	mutable TArray<TStrongObjectPtr<UObject>> ReflectedObjectReferences;
	mutable TArray<FAngelscriptNativeFunctionMapRecord> NativeFunctionMaps;
	struct FDelegateSubscription;
	struct FDelegatePayload;
	mutable TArray<TSharedPtr<FDelegateSubscription>> DelegateSubscriptions;
	mutable TArray<TSharedPtr<FDelegatePayload>> DelegatePayloads;
	mutable TArray<const FAngelscriptReflectedFunction*> ReflectedEventRegistry;
	mutable int32 NextDelegatePayloadHandle = 1;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptTypeBindInfoApply final
{
public:
	/** Build and validate a complete frozen image without registering it into an Engine. */
	static bool Prepare(TSharedRef<const FAngelscriptTypeBindInfoStore> Store,
		TUniquePtr<FAngelscriptTypeBindInfoInstallation>& Out, FString& Diagnostic,
		EAngelscriptBindingStage* Stage = nullptr);
	static bool Install(TSharedRef<const FAngelscriptTypeBindInfoStore> Store,
		AS_NAMESPACE_QUALIFIER asCScriptEngine& Engine,
		TUniquePtr<FAngelscriptTypeBindInfoInstallation>& Out, FString& Diagnostic,
		EAngelscriptBindingStage* Stage = nullptr);
	static bool ConnectNative(const FAngelscriptTypeBindInfoInstallation& Installation,
		AS_NAMESPACE_QUALIFIER asCScriptEngine& Engine, FString& Diagnostic);
	static AS_NAMESPACE_QUALIFIER asEMetadataRegistrationResult MaterializePublishedTypes(
		TSharedRef<const FAngelscriptTypeBindInfoStore> Store,
		AS_NAMESPACE_QUALIFIER asCScriptEngine& Engine,
		TUniquePtr<AS_NAMESPACE_QUALIFIER asCMetadataImage> Image = {});
};
