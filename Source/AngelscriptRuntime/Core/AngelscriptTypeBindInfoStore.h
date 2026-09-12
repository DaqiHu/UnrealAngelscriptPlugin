#pragma once

#include "AngelscriptTypeBindInfo.h"
#include "as_module_definition_set.h"
#include "as_type_id_registry.h"

class FAngelscriptReflectedTypeSnapshot;

/** Detached input to per-engine application. All exposed record views are const. */
class ANGELSCRIPTRUNTIME_API FAngelscriptTypeBindInfoStore
{
public:
	explicit FAngelscriptTypeBindInfoStore(FAngelscriptBindRecordingPolicy InPolicy = {})
		: Policy(MoveTemp(InPolicy)) {}
	FAngelscriptTypeBindInfoStore(const FAngelscriptTypeBindInfoStore&) = delete;
	FAngelscriptTypeBindInfoStore& operator=(const FAngelscriptTypeBindInfoStore&) = delete;
	FAngelscriptTypeBindInfoStore(FAngelscriptTypeBindInfoStore&&) = delete;
	FAngelscriptTypeBindInfoStore& operator=(FAngelscriptTypeBindInfoStore&&) = delete;

	bool RecordType(const FAngelscriptTypeBindDeclaration& Declaration,
		FAngelscriptTypeBindHandle& OutHandle, FString& OutDiagnostic);
	bool RecordMember(FAngelscriptTypeBindHandle Type, const FAngelscriptTypeBindInfoMember& Member,
		FAngelscriptTypeBindMemberHandle& OutHandle, FString& OutDiagnostic);
	bool UpdateMember(FAngelscriptTypeBindMemberHandle Handle,
		const FAngelscriptTypeBindInfoMember& Member, FString& OutDiagnostic);
	bool Seal(FString& OutDiagnostic);
	bool IsSealed() const { return bSealed; }
	bool HasFailed() const { return !FailureDiagnostic.IsEmpty(); }
	const FString& GetFailureDiagnostic() const { return FailureDiagnostic; }
	bool ReportFailure(FString Diagnostic);
	const FString& GetNamespace() const { return ActiveNamespace; }
	bool SetNamespace(FString Namespace);
	const FAngelscriptTypeBindSource& GetSource() const { return ActiveSource; }
	bool SetSource(FAngelscriptTypeBindSource Source);
	const FAngelscriptBindRecordingPolicy& GetPolicy() const { return Policy; }
	TConstArrayView<FAngelscriptRecordedProvider> GetProviders() const { return Providers; }
	bool RecordProvider(FAngelscriptRecordedProvider Provider, FString& OutDiagnostic);
	bool RecordEffect(FAngelscriptBindEffect Effect, FString& OutDiagnostic);
	TConstArrayView<FAngelscriptBindEffect> GetEffects() const { return Effects; }
	bool UpdateTypeDeclaration(FAngelscriptTypeBindHandle Type,
		const FAngelscriptTypeBindDeclaration& Declaration, FString& OutDiagnostic);
	bool RecordPrimitiveDefinition(FAngelscriptPrimitiveBindDefinition Definition, FString& OutDiagnostic);
	TConstArrayView<FAngelscriptPrimitiveBindDefinition> GetPrimitiveDefinitions() const { return PrimitiveDefinitions; }
	bool RecordNativeFunctionMap(FAngelscriptNativeFunctionMapRecord Record, FString& OutDiagnostic);
	bool RecordNativeFunctionMapView(const FAngelscriptNativeModuleFunctionBindingView& View,
		const FAngelscriptTypeBindSource& Source, FString& OutDiagnostic);
	TConstArrayView<FAngelscriptNativeFunctionMapRecord> GetNativeFunctionMaps() const { return NativeFunctionMaps; }
	bool SetReflectedSnapshot(TSharedRef<const FAngelscriptReflectedTypeSnapshot> Snapshot, FString& OutDiagnostic);
	const TSharedPtr<const FAngelscriptReflectedTypeSnapshot>& GetReflectedSnapshot() const { return ReflectedSnapshot; }

	TConstArrayView<FAngelscriptTypeBindInfo> GetTypes() const { return Types; }
	FAngelscriptTypeBindHandle FindType(const FString& Name, const FString& Namespace = FString()) const;
	FAngelscriptTypeBindHandle FindScope(const FString& Namespace = FString()) const;
	const FAngelscriptTypeBindInfo* GetType(FAngelscriptTypeBindHandle Handle) const;
	const FAngelscriptTypeBindInfoMember* GetMember(FAngelscriptTypeBindMemberHandle Handle) const;
	AS_NAMESPACE_QUALIFIER asEMetadataRegistrationResult PublishProcessIds() const;
	int GetPublishedTypeId(FAngelscriptTypeBindHandle Handle) const;
	int GetPublishedFunctionId(FAngelscriptTypeBindMemberHandle Handle) const;
	AS_NAMESPACE_QUALIFIER asETypeQueryResult GetPublishedClassInfo(
		int TypeId, FAngelscriptPublishedClassInfo& Out) const;
	bool IsPublished() const { return bPublished; }

private:
	struct FTypeKeyFuncs : TDefaultMapHashableKeyFuncs<FString, int32, false>
	{
		static bool Matches(const FString& A, const FString& B)
		{
			return A.Equals(B, ESearchCase::CaseSensitive);
		}
		static uint32 GetKeyHash(const FString& Key) { return FCrc::StrCrc32(*Key); }
	};

	static FString TypeKey(const FString& Name, const FString& Namespace, bool bNamespace = false);
	bool CanWrite(FString& OutDiagnostic) const;
	bool Fail(FString Diagnostic, FString& OutDiagnostic);

	TArray<FAngelscriptTypeBindInfo> Types;
	TMap<FString, int32, FDefaultSetAllocator, FTypeKeyFuncs> TypeIndices;
	FString FailureDiagnostic;
	FString ActiveNamespace;
	FAngelscriptTypeBindSource ActiveSource;
	FAngelscriptBindRecordingPolicy Policy;
	TArray<FAngelscriptRecordedProvider> Providers;
	TArray<FAngelscriptBindEffect> Effects;
	TArray<FAngelscriptPrimitiveBindDefinition> PrimitiveDefinitions;
	TArray<FAngelscriptNativeFunctionMapRecord> NativeFunctionMaps;
	TSharedPtr<const FAngelscriptReflectedTypeSnapshot> ReflectedSnapshot;
	bool bSealed = false;
	bool bPublished = false;
};
