#include "AngelscriptTypeBindInfoCatalog.h"

#include "AngelscriptBinds.h"
#include "AngelscriptBindsInternal.h"

namespace
{
	bool IsInstallableTemplate(const FAngelscriptTypeBindDeclaration& Declaration)
	{
		return Declaration.Name == TEXT("TArray") || Declaration.Name == TEXT("TMap")
			|| Declaration.Name == TEXT("TSet") || Declaration.Name == TEXT("TOptional")
			|| Declaration.Name == TEXT("TObjectPtr") || Declaration.Name == TEXT("TSubclassOf")
			|| Declaration.Name == TEXT("TWeakObjectPtr") || Declaration.Name == TEXT("TSoftObjectPtr")
			|| Declaration.Name == TEXT("TSoftClassPtr")
			|| Declaration.Name == TEXT("TArrayIterator")
			|| Declaration.Name == TEXT("TArrayConstIterator")
			|| Declaration.Name == TEXT("TMapIterator")
			|| Declaration.Name == TEXT("TMapConstIterator")
			|| Declaration.Name == TEXT("TSetIterator")
			|| Declaration.Name == TEXT("TSetConstIterator");
	}
	bool RecordPrimitive(FAngelscriptTypeBindInfoStore& Store, const TCHAR* Name,
		const TCHAR* Owner, uint32 Size, uint32 Alignment, std::initializer_list<const TCHAR*> Aliases,
		FString& Diagnostic)
	{
		FAngelscriptPrimitiveBindDefinition Definition;
		Definition.Name = Name; Definition.NativeOwner = Owner; Definition.AdapterName = FString(Name) + TEXT("Type");
		Definition.NativeSize = Size; Definition.NativeAlignment = Alignment;
		for (const TCHAR* Alias : Aliases) Definition.Aliases.Add(Alias);
		Definition.Source = {TEXT("AngelscriptRuntime"), TEXT("PrimitiveTypes.TypeInfrastructure"), TEXT("Binds/Bind_Primitives.cpp"), 1, EAngelscriptBindPhase::TypeInfrastructure};
		return Store.RecordPrimitiveDefinition(MoveTemp(Definition), Diagnostic);
	}
}

bool FAngelscriptTypeBindInfoCatalog::RecordRuntimeTypeDeclarations(
	const UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
	FAngelscriptTypeBindInfoStore& Store, FString& Diagnostic)
{
	Diagnostic.Reset();
	if (!IsInGameThread()) { Diagnostic = TEXT("Runtime type declarations must be recorded on the GameThread."); return false; }
	if (!Collection.IsSealed() || !Collection.GetCollectionFailureDiagnostic().IsEmpty())
	{
		Diagnostic = Collection.GetCollectionFailureDiagnostic().IsEmpty()
			? TEXT("Runtime type declarations require a sealed provider collection.") : Collection.GetCollectionFailureDiagnostic();
		return false;
	}
	if (Store.IsSealed() || Store.HasFailed()) { Diagnostic = TEXT("Runtime type declarations require a writable Store."); return false; }
	if (!RecordPrimitive(Store, TEXT("bool"), TEXT("bool"), sizeof(bool), alignof(bool), {}, Diagnostic)
		|| !RecordPrimitive(Store, TEXT("int8"), TEXT("int8"), sizeof(int8), alignof(int8), {}, Diagnostic)
		|| !RecordPrimitive(Store, TEXT("int16"), TEXT("int16"), sizeof(int16), alignof(int16), {}, Diagnostic)
		|| !RecordPrimitive(Store, TEXT("int"), TEXT("int32"), sizeof(int32), alignof(int32), {TEXT("int32")}, Diagnostic)
		|| !RecordPrimitive(Store, TEXT("uint"), TEXT("uint32"), sizeof(uint32), alignof(uint32), {TEXT("uint32")}, Diagnostic)
		|| !RecordPrimitive(Store, TEXT("int64"), TEXT("int64"), sizeof(int64), alignof(int64), {}, Diagnostic)
		|| !RecordPrimitive(Store, TEXT("uint64"), TEXT("uint64"), sizeof(uint64), alignof(uint64), {}, Diagnostic)
		|| !RecordPrimitive(Store, Store.GetPolicy().bScriptFloatIsFloat64 ? TEXT("float32") : TEXT("float"),
			TEXT("float"), sizeof(float), alignof(float), {}, Diagnostic)
		|| !RecordPrimitive(Store, Store.GetPolicy().bScriptFloatIsFloat64 ? TEXT("float") : TEXT("float64"),
			TEXT("double"), sizeof(double), alignof(double), {TEXT("double")}, Diagnostic)) return false;

	FAngelscriptBinds Binds(Store, true);
	for (const FAngelscriptBindRecord& Record : Collection.GetRecords())
	{
		if (Record.Phase != EAngelscriptBindPhase::TypeDeclarations && !Record.bContainsTypeDeclarations) continue;
		FAngelscriptRecordedProvider Provider;
		Provider.Source = {Record.OwnerModule, Record.BindName,
			Record.SourceFile != nullptr ? ANSI_TO_TCHAR(Record.SourceFile) : FString(), Record.SourceLine, Record.Phase};
		Provider.bEligible = (!Store.GetPolicy().bRuntimeModuleOnly
				|| Record.OwnerModule == TEXT("AngelscriptRuntime"))
			&& (Record.Condition == nullptr || Record.Condition(Store.GetPolicy()));
		if (!Store.RecordProvider(Provider, Diagnostic)) return false;
		if (!Provider.bEligible) continue;
		Store.SetSource(Provider.Source); Store.SetNamespace({}); Record.Callback(Binds);
		if (Store.HasFailed()) { Diagnostic = Store.GetFailureDiagnostic(); return false; }
	}
	Store.SetSource({}); Store.SetNamespace({}); return true;
}

bool FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(
	const UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
	FAngelscriptTypeBindInfoStore& Store, FString& Diagnostic)
{
	FAngelscriptTypeBindInfoStore Catalog(Store.GetPolicy());
	if (!RecordRuntimeTypeDeclarations(Collection, Catalog, Diagnostic)) return false;
	for (const auto& Primitive : Catalog.GetPrimitiveDefinitions())
		if (!Store.RecordPrimitiveDefinition(Primitive, Diagnostic)) return false;
	for (const auto& Provider : Catalog.GetProviders())
		if (!Store.RecordProvider(Provider, Diagnostic)) return false;
	if (Catalog.GetReflectedSnapshot().IsValid()
		&& !Store.SetReflectedSnapshot(Catalog.GetReflectedSnapshot().ToSharedRef(), Diagnostic)) return false;
	for (const auto& Type : Catalog.GetTypes())
	{
		if (Type.Declaration.Kind == EAngelscriptTypeBindKind::Template && !IsInstallableTemplate(Type.Declaration)) continue;
		FAngelscriptTypeBindHandle Handle;
		if (!Store.RecordType(Type.Declaration, Handle, Diagnostic)) return false;
		for (const auto& Source : Type.Sources)
		{
			auto Additional = Type.Declaration; Additional.Source = Source;
			if (!Store.RecordType(Additional, Handle, Diagnostic)) return false;
		}
		for (const auto& Member : Type.Members)
		{
			if (Member.Kind != EAngelscriptTypeBindMemberKind::EnumValue
				|| Member.Source.Provider != TEXT("ConfigEnums")) continue;
			FAngelscriptTypeBindMemberHandle MemberHandle;
			if (!Store.RecordMember(Handle, Member, MemberHandle, Diagnostic)) return false;
		}
	}
	for (const auto& Effect : Catalog.GetEffects())
		if (!Store.RecordEffect(Effect, Diagnostic)) return false;
	return true;
}

bool FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(
	const UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
	TConstArrayView<FName> ProviderNames,
	FAngelscriptTypeBindInfoStore& Store, FString& Diagnostic)
{
	Diagnostic.Reset();
	if (!IsInGameThread()) { Diagnostic = TEXT("Selected providers must be recorded on the GameThread."); return false; }
	if (!Collection.IsSealed() || !Collection.GetCollectionFailureDiagnostic().IsEmpty())
	{
		Diagnostic = Collection.GetCollectionFailureDiagnostic().IsEmpty()
			? TEXT("Selected provider recording requires a sealed provider collection.") : Collection.GetCollectionFailureDiagnostic();
		return false;
	}
	if (Store.IsSealed() || Store.HasFailed()) { Diagnostic = TEXT("Selected provider recording requires a writable Store."); return false; }

	TSet<FName> Requested;
	for (const FName Name : ProviderNames)
	{
		if (Name.IsNone()) { Diagnostic = TEXT("Selected provider names must be non-empty."); return false; }
		Requested.Add(Name);
	}
	TSet<FName> Found;
	FAngelscriptBinds Binds(Store);
	for (const FAngelscriptBindRecord& Record : Collection.GetRecords())
	{
		if (!Requested.Contains(Record.BindName)) continue;
		Found.Add(Record.BindName);
		FAngelscriptRecordedProvider Provider;
		Provider.Source = {Record.OwnerModule, Record.BindName,
			Record.SourceFile != nullptr ? ANSI_TO_TCHAR(Record.SourceFile) : FString(), Record.SourceLine, Record.Phase};
		Provider.bEligible = (!Store.GetPolicy().bRuntimeModuleOnly
				|| Record.OwnerModule == TEXT("AngelscriptRuntime"))
			&& (Record.Condition == nullptr || Record.Condition(Store.GetPolicy()));
		if (!Store.RecordProvider(Provider, Diagnostic)) return false;
		if (!Provider.bEligible) continue;
		Store.SetSource(Provider.Source); Store.SetNamespace({}); Record.Callback(Binds);
		if (Store.HasFailed()) { Diagnostic = Store.GetFailureDiagnostic(); return false; }
	}
	Store.SetSource({}); Store.SetNamespace({});
	for (const FName Name : Requested)
	{
		if (!Found.Contains(Name)) { Diagnostic = FString::Printf(TEXT("Selected provider '%s' was not registered."), *Name.ToString()); return false; }
	}
	return true;
}
