#pragma once
#include "AngelscriptTypeBindInfoStore.h"
#include "UObject/StrongObjectPtr.h"

class FProperty;
class UScriptStruct;
class UFunction;
enum class EAngelscriptReflectedKind : uint8 { Class, Interface, Struct, Enum, Delegate };
enum class EAngelscriptReflectedDeclarationFilter : uint8 { All, Classes, Structs, Enums };
enum class EAngelscriptReflectedParameterDirection : uint8 { In, Out, InOut, Return };
enum class EAngelscriptReflectedFunctionDispatch : uint8 { ReflectiveFallback, Generated };

/** Copied property shape; referenced UE identities are retained by the snapshot. */
struct FAngelscriptReflectedTypeUse
{
	FString Kind;
	FString CppType;
	UObject* ReferencedType = nullptr;
	TArray<FAngelscriptReflectedTypeUse> Arguments;
};
struct FAngelscriptReflectedProperty
{
	FString Name;
	FProperty* NativeProperty = nullptr;
	int32 Offset = 0;
	int32 ElementSize = 0;
	int32 ArrayDim = 1;
	uint64 Flags = 0;
	FAngelscriptReflectedTypeUse Type;
};
struct FAngelscriptReflectedLifetime
{
	UScriptStruct* Struct = nullptr;
	void (*Initialize)(UScriptStruct*, void*) = nullptr;
	void (*Destroy)(UScriptStruct*, void*) = nullptr;
	void (*Copy)(UScriptStruct*, void*, const void*) = nullptr;
	bool bHasGCReferences = false;
	bool bNeedsDestruction = false;
};
struct FAngelscriptReflectedFunctionParameter
{
	FString Name;
	FProperty* NativeProperty = nullptr;
	FAngelscriptReflectedTypeUse Type;
	EAngelscriptReflectedParameterDirection Direction = EAngelscriptReflectedParameterDirection::In;
	FString DefaultValue;
};
struct FAngelscriptReflectedFunction
{
	FString Name;
	FString Declaration;
	UFunction* NativeFunction = nullptr;
	TArray<FAngelscriptReflectedFunctionParameter> Parameters;
	bool bEligible = false;
	FString ExclusionReason;
	EAngelscriptReflectedFunctionDispatch Dispatch = EAngelscriptReflectedFunctionDispatch::ReflectiveFallback;
	bool bStatic = false;
	bool bFunctionLibrary = false;
	int32 MixinReceiverParameter = INDEX_NONE;
	int32 WorldContextParameter = INDEX_NONE;
	int32 DeterminesOutputTypeParameter = INDEX_NONE;
};
struct FAngelscriptReflectedTypeRecord
{
	EAngelscriptReflectedKind Kind = EAngelscriptReflectedKind::Class;
	UObject* NativeType = nullptr;
	FString NativePath;
	UObject* Parent = nullptr;
	TArray<UClass*> Interfaces;
	FAngelscriptTypeBindDeclaration Declaration;
	bool bEligible = false;
	FString ExclusionReason;
	TArray<FAngelscriptReflectedProperty> Properties;
	TArray<FAngelscriptReflectedFunction> Functions;
	TArray<TPair<FString, int64>> EnumValues;
	FAngelscriptReflectedLifetime Lifetime;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptReflectedTypeSnapshot
{
public:
	TConstArrayView<FAngelscriptReflectedTypeRecord> GetTypes() const { return Types; }
	const FAngelscriptReflectedTypeRecord* Find(const UObject* NativeType) const;
	const FAngelscriptBindRecordingPolicy& GetPolicy() const { return Policy; }
private:
	friend class FAngelscriptTypeBindInfoReflection;
	FAngelscriptBindRecordingPolicy Policy;
	TArray<FAngelscriptReflectedTypeRecord> Types;
	TArray<TStrongObjectPtr<UObject>> RetainedObjects;
	TMap<const UObject*, int32> TypeIndices;
};

/** Serial GameThread reflection capture, with no script Engine or config service. */
class ANGELSCRIPTRUNTIME_API FAngelscriptTypeBindInfoReflection
{
public:
	static bool Capture(TConstArrayView<UObject*> Roots, const FAngelscriptBindRecordingPolicy& Policy,
		TSharedPtr<const FAngelscriptReflectedTypeSnapshot>& Out, FString& Diagnostic);
	static bool CaptureLoaded(const FAngelscriptBindRecordingPolicy& Policy,
		TSharedPtr<const FAngelscriptReflectedTypeSnapshot>& Out, FString& Diagnostic);
	static bool RecordDeclarations(FAngelscriptTypeBindInfoStore& Store,
		TSharedRef<const FAngelscriptReflectedTypeSnapshot> Snapshot, FString& Diagnostic,
		EAngelscriptReflectedDeclarationFilter Filter = EAngelscriptReflectedDeclarationFilter::All);
	static bool RecordLoadedDeclarations(FAngelscriptTypeBindInfoStore& Store, FString& Diagnostic,
		EAngelscriptReflectedDeclarationFilter Filter = EAngelscriptReflectedDeclarationFilter::All);
	/** Records captured eligible instance functions as generic callables after their nominal dependencies exist. */
	static bool RecordFunctions(FAngelscriptTypeBindInfoStore& Store,
		TSharedRef<const FAngelscriptReflectedTypeSnapshot> Snapshot, FString& Diagnostic,
		TConstArrayView<UObject*> NativeTypes = {});
};

namespace UE::Angelscript::Private
{
	// Declaration policy and native adaptation remain in their owning providers.
	void DescribeReflectedClass(UClass* Class, const FAngelscriptBindRecordingPolicy& Policy, FAngelscriptReflectedTypeRecord& Out);
	void DescribeReflectedStruct(UScriptStruct* Struct, const FAngelscriptBindRecordingPolicy& Policy, FAngelscriptReflectedTypeRecord& Out);
	void DescribeReflectedEnum(UEnum* Enum, const FAngelscriptBindRecordingPolicy& Policy, FAngelscriptReflectedTypeRecord& Out);
	void DescribeReflectedDelegate(UFunction* Signature, const FAngelscriptBindRecordingPolicy& Policy, FAngelscriptReflectedTypeRecord& Out);
}
