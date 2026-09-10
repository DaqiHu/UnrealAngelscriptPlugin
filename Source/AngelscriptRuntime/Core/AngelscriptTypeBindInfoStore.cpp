#include "AngelscriptTypeBindInfoStore.h"
#include "AngelscriptTypeBindInfoReflection.h"
#include "as_type_id_registry.h"

bool FAngelscriptTypeBindInfoStore::SetReflectedSnapshot(TSharedRef<const FAngelscriptReflectedTypeSnapshot> Snapshot, FString& OutDiagnostic)
{
	if (!CanWrite(OutDiagnostic)) return false;
	if (Policy != Snapshot->GetPolicy())
		return Fail(TEXT("A reflection snapshot policy must match the recording Store policy."), OutDiagnostic);
	if (ReflectedSnapshot.IsValid() && ReflectedSnapshot.Get() != &Snapshot.Get())
		return Fail(TEXT("A recording Store cannot mix distinct reflection snapshots."), OutDiagnostic);
	ReflectedSnapshot = MoveTemp(Snapshot);
	OutDiagnostic.Reset(); return true;
}

FString FAngelscriptTypeBindInfoStore::TypeKey(const FString& Name, const FString& Namespace,
	const bool bNamespace)
{
	// Namespace scopes and nominal types may have the same spelling. FString keys
	// use explicit case-sensitive key functions for AngelScript nominal identity.
	return FString::Printf(TEXT("%c:%s::%s"), bNamespace ? TEXT('N') : TEXT('T'), *Namespace, *Name);
}

bool FAngelscriptTypeBindInfoStore::CanWrite(FString& OutDiagnostic) const
{
	if (bSealed)
	{
		OutDiagnostic = TEXT("The binding store is sealed; its records cannot be changed.");
		return false;
	}
	if (!FailureDiagnostic.IsEmpty())
	{
		OutDiagnostic = FailureDiagnostic;
		return false;
	}
	OutDiagnostic.Reset();
	return true;
}

bool FAngelscriptTypeBindInfoStore::Fail(FString Diagnostic, FString& OutDiagnostic)
{
	if (FailureDiagnostic.IsEmpty())
	{
		FailureDiagnostic = MoveTemp(Diagnostic);
	}
	OutDiagnostic = FailureDiagnostic;
	return false;
}

bool FAngelscriptTypeBindInfoStore::RecordType(const FAngelscriptTypeBindDeclaration& Declaration,
	FAngelscriptTypeBindHandle& OutHandle, FString& OutDiagnostic)
{
	OutHandle = {};
	if (!CanWrite(OutDiagnostic)) return false;
	if (Declaration.Name.IsEmpty() && Declaration.Kind != EAngelscriptTypeBindKind::Namespace)
	{
		return Fail(FString::Printf(TEXT("Binding provider '%s' declared an empty type name (%s:%d)."),
			*Declaration.Source.Provider.ToString(), *Declaration.Source.File, Declaration.Source.Line), OutDiagnostic);
	}

	const FString Key = TypeKey(Declaration.Name, Declaration.Namespace,
		Declaration.Kind == EAngelscriptTypeBindKind::Namespace);
	if (const int32* ExistingIndex = TypeIndices.Find(Key))
	{
		auto& Existing = Types[*ExistingIndex];
		const auto& Previous = Existing.Declaration;
		bool bSameInterfaces = Previous.Interfaces.Num() == Declaration.Interfaces.Num();
		for (int32 Index = 0; bSameInterfaces && Index < Previous.Interfaces.Num(); ++Index)
			bSameInterfaces = Previous.Interfaces[Index].Equals(Declaration.Interfaces[Index], ESearchCase::CaseSensitive);
		if (Previous.Kind != Declaration.Kind || Previous.NativeSize != Declaration.NativeSize
			|| Previous.NativeAlignment != Declaration.NativeAlignment
			|| Previous.ObjectFlags != Declaration.ObjectFlags
			|| !Previous.BaseType.Equals(Declaration.BaseType, ESearchCase::CaseSensitive)
			|| !bSameInterfaces || !Previous.AliasTarget.Equals(Declaration.AliasTarget, ESearchCase::CaseSensitive)
			|| !Previous.TemplateType.Equals(Declaration.TemplateType, ESearchCase::CaseSensitive)
			|| !Previous.TemplateDeclaration.Equals(Declaration.TemplateDeclaration, ESearchCase::CaseSensitive)
			|| Previous.UnrealType != Declaration.UnrealType
			|| (!Previous.NativeOwner.IsEmpty() && !Declaration.NativeOwner.IsEmpty() && Previous.NativeOwner != Declaration.NativeOwner)
			|| (!Previous.AdapterName.IsEmpty() && !Declaration.AdapterName.IsEmpty() && Previous.AdapterName != Declaration.AdapterName)
			|| (Previous.DefaultConstruct != nullptr && Declaration.DefaultConstruct != nullptr && Previous.DefaultConstruct != Declaration.DefaultConstruct)
			|| (Previous.CopyConstruct != nullptr && Declaration.CopyConstruct != nullptr && Previous.CopyConstruct != Declaration.CopyConstruct)
			|| (Previous.Destroy != nullptr && Declaration.Destroy != nullptr && Previous.Destroy != Declaration.Destroy)
			|| (Previous.Equal != nullptr && Declaration.Equal != nullptr && Previous.Equal != Declaration.Equal)
			|| (Previous.Hash != nullptr && Declaration.Hash != nullptr && Previous.Hash != Declaration.Hash)
			|| Previous.TemplateContainer != Declaration.TemplateContainer)
		{
			return Fail(FString::Printf(
				TEXT("Incompatible binding definition for '%s::%s': provider '%s' (%s:%d) conflicts with provider '%s' (%s:%d) in kind, native layout, flags or base type."),
				*Declaration.Namespace, *Declaration.Name,
				*Previous.Source.Provider.ToString(), *Previous.Source.File, Previous.Source.Line,
				*Declaration.Source.Provider.ToString(), *Declaration.Source.File, Declaration.Source.Line), OutDiagnostic);
		}
		const auto& Source = Declaration.Source;
		if (!Existing.Sources.ContainsByPredicate([&Source](const FAngelscriptTypeBindSource& Entry)
			{
				return Entry.OwnerModule == Source.OwnerModule && Entry.Provider == Source.Provider
					&& Entry.File == Source.File && Entry.Line == Source.Line && Entry.Phase == Source.Phase;
			}))
		{
			Existing.Sources.Add(Source);
		}
		OutHandle = {this, *ExistingIndex};
		return true;
	}

	FAngelscriptTypeBindInfo Entry;
	Entry.Declaration = Declaration;
	Entry.Sources.Add(Declaration.Source);
	const int32 Index = Types.Add(MoveTemp(Entry));
	TypeIndices.Add(Key, Index);
	OutHandle = {this, Index};
	return true;
}

bool FAngelscriptTypeBindInfoStore::UpdateTypeDeclaration(FAngelscriptTypeBindHandle Type,
	const FAngelscriptTypeBindDeclaration& Declaration, FString& OutDiagnostic)
{
	if (!CanWrite(OutDiagnostic)) return false;
	if (GetType(Type) == nullptr) return Fail(TEXT("Cannot update a declaration through an invalid or foreign type handle."), OutDiagnostic);
	const FAngelscriptTypeBindDeclaration& Existing = Types[Type.Index].Declaration;
	if (Existing.Name != Declaration.Name || Existing.Namespace != Declaration.Namespace || Existing.Kind != Declaration.Kind)
		return Fail(TEXT("A declaration update cannot change nominal identity or kind."), OutDiagnostic);
	Types[Type.Index].Declaration = Declaration;
	if (!ActiveSource.OwnerModule.IsNone() && !ActiveSource.Provider.IsNone()
		&& !(ActiveSource.OwnerModule == Declaration.Source.OwnerModule
			&& ActiveSource.Provider == Declaration.Source.Provider
			&& ActiveSource.Phase == Declaration.Source.Phase)
		&& !Types[Type.Index].Sources.ContainsByPredicate([this](const FAngelscriptTypeBindSource& Source)
		{
			return Source.OwnerModule == ActiveSource.OwnerModule
				&& Source.Provider == ActiveSource.Provider
				&& Source.Phase == ActiveSource.Phase;
		}))
	{
		Types[Type.Index].Sources.Add(ActiveSource);
	}
	return true;
}

bool FAngelscriptTypeBindInfoStore::RecordPrimitiveDefinition(
	FAngelscriptPrimitiveBindDefinition Definition, FString& OutDiagnostic)
{
	if (!CanWrite(OutDiagnostic)) return false;
	if (Definition.Name.IsEmpty() || Definition.NativeOwner.IsEmpty() || Definition.AdapterName.IsEmpty()
		|| Definition.NativeSize == 0 || Definition.NativeAlignment == 0
		|| (Definition.NativeAlignment & (Definition.NativeAlignment - 1)) != 0)
		return Fail(TEXT("Primitive binding definitions require name, owner, adapter and valid native layout."), OutDiagnostic);
	if (PrimitiveDefinitions.ContainsByPredicate([&](const auto& Existing) { return Existing.Name == Definition.Name; }))
		return Fail(FString::Printf(TEXT("Duplicate primitive binding definition '%s'."), *Definition.Name), OutDiagnostic);
	PrimitiveDefinitions.Add(MoveTemp(Definition));
	return true;
}

bool FAngelscriptTypeBindInfoStore::RecordNativeFunctionMap(
	FAngelscriptNativeFunctionMapRecord Record, FString& OutDiagnostic)
{
	if (!CanWrite(OutDiagnostic)) return false;
	if (Record.ModuleName.IsNone() || Record.OwnerType.IsEmpty() || Record.FunctionName.IsEmpty()
		|| Record.Signature.IsEmpty() || Record.LayoutVersion == 0
		|| Record.Origin == EAngelscriptFunctionBindingOrigin::Unknown
		|| (!Record.bPlaceholder && Record.Thunk == nullptr)
		|| Record.Source.OwnerModule.IsNone() || Record.Source.Provider.IsNone()
		|| Record.Source.File.IsEmpty() || Record.Source.Line <= 0)
	{
		return Fail(TEXT("Native function-map records require module, symbol, signature, source, origin and a real thunk or explicit placeholder."), OutDiagnostic);
	}
	auto SameSymbol = [&Record](const FAngelscriptNativeFunctionMapRecord& Existing)
	{
		return Existing.OwnerType.Equals(Record.OwnerType, ESearchCase::CaseSensitive)
			&& Existing.FunctionName.Equals(Record.FunctionName, ESearchCase::CaseSensitive);
	};
	if (FAngelscriptNativeFunctionMapRecord* Existing = NativeFunctionMaps.FindByPredicate(SameSymbol))
	{
		const bool bCompatible = Existing->ModuleName == Record.ModuleName
			&& Existing->Signature.Equals(Record.Signature, ESearchCase::CaseSensitive)
			&& Existing->ArgCount == Record.ArgCount && Existing->ReturnSize == Record.ReturnSize
			&& Existing->Flags == Record.Flags && Existing->LayoutVersion == Record.LayoutVersion;
		if (!bCompatible)
		{
			return Fail(FString::Printf(TEXT("Native function-map symbol '%s::%s' from provider '%s' conflicts with provider '%s' in module, signature, ABI facts or layout version."),
				*Record.OwnerType, *Record.FunctionName, *Existing->Source.Provider.ToString(), *Record.Source.Provider.ToString()), OutDiagnostic);
		}
		const bool bExistingManual = Existing->Origin == EAngelscriptFunctionBindingOrigin::Manual;
		const bool bNewManual = Record.Origin == EAngelscriptFunctionBindingOrigin::Manual;
		if ((Existing->bPlaceholder && !Record.bPlaceholder) || (!bExistingManual && bNewManual))
		{
			*Existing = MoveTemp(Record);
		}
		OutDiagnostic.Reset();
		return true;
	}
	NativeFunctionMaps.Add(MoveTemp(Record));
	OutDiagnostic.Reset();
	return true;
}

bool FAngelscriptTypeBindInfoStore::RecordNativeFunctionMapView(
	const FAngelscriptNativeModuleFunctionBindingView& View,
	const FAngelscriptTypeBindSource& Source, FString& OutDiagnostic)
{
	if (!CanWrite(OutDiagnostic)) return false;
	if (View.LayoutVersion != FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected)
		return Fail(FString::Printf(TEXT("Native function-map layout version 0x%08x does not match 0x%08x for provider '%s'."),
			View.LayoutVersion, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected, *Source.Provider.ToString()), OutDiagnostic);
	if (View.ModuleName == nullptr || View.Count < 0 || (View.Count > 0 && View.Table == nullptr))
		return Fail(FString::Printf(TEXT("Native function-map view for provider '%s' is malformed."), *Source.Provider.ToString()), OutDiagnostic);
	for (int32 Index = 0; Index < View.Count; ++Index)
	{
		const FAngelscriptNativeModuleFunctionBinding& Binding = View.Table[Index];
		if (Binding.ClassName == nullptr || Binding.FunctionName == nullptr || Binding.Thunk == nullptr)
			return Fail(FString::Printf(TEXT("Native function-map module '%s' has a missing class, function or thunk at table index %d (provider '%s')."),
				View.ModuleName, Index, *Source.Provider.ToString()), OutDiagnostic);
		FAngelscriptNativeFunctionMapRecord Record;
		Record.ModuleName = View.ModuleName;
		Record.OwnerType = Binding.ClassName;
		Record.FunctionName = Binding.FunctionName;
		Record.Signature = FString::Printf(TEXT("args=%u;return=%u;flags=%u"), Binding.ArgCount, Binding.RetSize, Binding.Flags);
		Record.Thunk = Binding.Thunk;
		Record.ArgCount = Binding.ArgCount;
		Record.ReturnSize = Binding.RetSize;
		Record.Flags = Binding.Flags;
		Record.LayoutVersion = View.LayoutVersion;
		Record.Origin = EAngelscriptFunctionBindingOrigin::NativeModule;
		Record.Source = Source;
		if (!RecordNativeFunctionMap(MoveTemp(Record), OutDiagnostic)) return false;
	}
	OutDiagnostic.Reset();
	return true;
}

bool FAngelscriptTypeBindInfoStore::RecordMember(FAngelscriptTypeBindHandle Type,
	const FAngelscriptTypeBindInfoMember& Member, FAngelscriptTypeBindMemberHandle& OutHandle,
	FString& OutDiagnostic)
{
	OutHandle = {};
	if (!CanWrite(OutDiagnostic)) return false;
	if (GetType(Type) == nullptr)
	{
		return Fail(TEXT("Cannot record a member through an invalid or foreign binding type handle."), OutDiagnostic);
	}
	// Copy first: the input may itself be a view of an earlier recorded member.
	FAngelscriptTypeBindInfoMember Copy = Member;
	const int32 Index = Types[Type.Index].Members.Add(MoveTemp(Copy));
	OutHandle = {Type, Index};
	return true;
}

bool FAngelscriptTypeBindInfoStore::UpdateMember(FAngelscriptTypeBindMemberHandle Handle,
	const FAngelscriptTypeBindInfoMember& Member, FString& OutDiagnostic)
{
	if (!CanWrite(OutDiagnostic)) return false;
	if (GetMember(Handle) == nullptr)
	{
		return Fail(TEXT("Cannot update a member through an invalid or foreign binding member handle."), OutDiagnostic);
	}
	Types[Handle.Type.Index].Members[Handle.Index] = Member;
	return true;
}

bool FAngelscriptTypeBindInfoStore::Seal(FString& OutDiagnostic)
{
	if (!FailureDiagnostic.IsEmpty())
	{
		OutDiagnostic = FailureDiagnostic;
		return false;
	}
	bSealed = true;
	OutDiagnostic.Reset();
	return true;
}

FAngelscriptTypeBindHandle FAngelscriptTypeBindInfoStore::FindType(
	const FString& Name, const FString& Namespace) const
{
	if (const int32* Index = TypeIndices.Find(TypeKey(Name, Namespace)))
	{
		return {this, *Index};
	}
	return {};
}

const FAngelscriptTypeBindInfo* FAngelscriptTypeBindInfoStore::GetType(FAngelscriptTypeBindHandle Handle) const
{
	return Handle.Store == this && Types.IsValidIndex(Handle.Index) ? &Types[Handle.Index] : nullptr;
}

FAngelscriptTypeBindHandle FAngelscriptTypeBindInfoStore::FindScope(const FString& Namespace) const
{
	if (const int32* Index = TypeIndices.Find(TypeKey(FString(), Namespace, true)))
	{
		return {this, *Index};
	}
	return {};
}

bool FAngelscriptTypeBindInfoStore::ReportFailure(FString Diagnostic)
{
	FString Ignored;
	if (!CanWrite(Ignored)) return false;
	return Fail(MoveTemp(Diagnostic), Ignored);
}

bool FAngelscriptTypeBindInfoStore::RecordProvider(FAngelscriptRecordedProvider Provider, FString& OutDiagnostic)
{
	if (!CanWrite(OutDiagnostic)) return false;
	const auto& Source = Provider.Source;
	if (Source.OwnerModule.IsNone() || Source.Provider.IsNone() || Source.File.IsEmpty()
		|| Source.Line <= 0 || Source.Phase > EAngelscriptBindPhase::Finalization)
	{
		return Fail(TEXT("Recorded provider identity and source provenance are required."), OutDiagnostic);
	}
	for (const auto& Existing : Providers)
	{
		const auto& Previous = Existing.Source;
		if (Previous.OwnerModule == Source.OwnerModule && Previous.Provider == Source.Provider
			&& Previous.Phase == Source.Phase)
		{
			return Fail(FString::Printf(TEXT("Duplicate recorded provider '%s': '%s:%d' and '%s:%d'."),
				*Source.Provider.ToString(), *Previous.File, Previous.Line, *Source.File, Source.Line), OutDiagnostic);
		}
	}
	Providers.Add(MoveTemp(Provider));
	return true;
}

bool FAngelscriptTypeBindInfoStore::RecordEffect(FAngelscriptBindEffect Effect, FString& OutDiagnostic)
{
	if (!CanWrite(OutDiagnostic)) return false;
	if (Effect.Source.OwnerModule.IsNone() || Effect.Source.Provider.IsNone()
		|| Effect.Source.File.IsEmpty() || Effect.Source.Line <= 0)
		return Fail(TEXT("A binding effect requires complete source provenance."), OutDiagnostic);
	if (Effect.Name.IsEmpty() && Effect.Kind != EAngelscriptBindEffectKind::DirectBindArchitectureProbe)
		return Fail(TEXT("A binding effect requires a primary identity."), OutDiagnostic);
	if (Effect.Kind == EAngelscriptBindEffectKind::ToString && Effect.ToString == nullptr)
		return Fail(TEXT("A ToString effect requires a formatter function."), OutDiagnostic);
	Effects.Add(MoveTemp(Effect));
	OutDiagnostic.Reset();
	return true;
}

bool FAngelscriptTypeBindInfoStore::SetNamespace(FString Namespace)
{
	FString Diagnostic;
	if (!CanWrite(Diagnostic)) return false;
	ActiveNamespace = MoveTemp(Namespace);
	return true;
}

bool FAngelscriptTypeBindInfoStore::SetSource(FAngelscriptTypeBindSource Source)
{
	FString Diagnostic;
	if (!CanWrite(Diagnostic)) return false;
	ActiveSource = MoveTemp(Source);
	return true;
}

const FAngelscriptTypeBindInfoMember* FAngelscriptTypeBindInfoStore::GetMember(
	FAngelscriptTypeBindMemberHandle Handle) const
{
	const auto* Type = GetType(Handle.Type);
	return Type != nullptr && Type->Members.IsValidIndex(Handle.Index) ? &Type->Members[Handle.Index] : nullptr;
}

namespace
{
	bool IsPublishedCallable(EAngelscriptTypeBindMemberKind Kind)
	{
		return Kind == EAngelscriptTypeBindMemberKind::Constructor
			|| Kind == EAngelscriptTypeBindMemberKind::Destructor
			|| Kind == EAngelscriptTypeBindMemberKind::Method
			|| Kind == EAngelscriptTypeBindMemberKind::Behaviour
			|| Kind == EAngelscriptTypeBindMemberKind::GlobalFunction;
	}

	bool HasPublishableLayout(const FAngelscriptTypeBindDeclaration& Declaration)
	{
		if (Declaration.Kind == EAngelscriptTypeBindKind::Namespace) return true;
		if (Declaration.NativeSize == 0 || Declaration.NativeAlignment == 0) return false;
		return (Declaration.NativeAlignment & (Declaration.NativeAlignment - 1)) == 0;
	}
}

AS_NAMESPACE_QUALIFIER asEMetadataRegistrationResult FAngelscriptTypeBindInfoStore::PublishProcessIds() const
{
	auto& Mutable = const_cast<FAngelscriptTypeBindInfoStore&>(*this);
	if (!Mutable.FailureDiagnostic.IsEmpty() || !Mutable.bSealed) return AS_NAMESPACE_QUALIFIER asEMetadataRegistrationResult::InvalidState;
	if (Mutable.bPublished) return AS_NAMESPACE_QUALIFIER asEMetadataRegistrationResult::AlreadyRegistered;
	asUINT TypeCount = 0;
	asUINT FunctionCount = 0;
	for (const FAngelscriptTypeBindInfo& Type : Mutable.Types)
	{
		if (!HasPublishableLayout(Type.Declaration))
		{
			return AS_NAMESPACE_QUALIFIER asEMetadataRegistrationResult::InvalidLayout;
		}
		if (Type.Declaration.Kind != EAngelscriptTypeBindKind::Namespace) ++TypeCount;
		for (const FAngelscriptTypeBindInfoMember& Member : Type.Members)
		{
			if (IsPublishedCallable(Member.Kind)) ++FunctionCount;
		}
	}
	if (TypeCount == 0 && FunctionCount == 0)
	{
		Mutable.bPublished = true;
		return AS_NAMESPACE_QUALIFIER asEMetadataRegistrationResult::Succeeded;
	}
	AS_NAMESPACE_QUALIFIER asSTypeIdReservationRequest Request;
	Request.TypeSequenceCount = TypeCount;
	Request.FunctionIdCount = FunctionCount;
	AS_NAMESPACE_QUALIFIER asSTypeIdReservation Reservation;
	if (AS_NAMESPACE_QUALIFIER asCTypeIdRegistry::Get().Reserve(Request, Reservation)
		!= AS_NAMESPACE_QUALIFIER asTYPE_ID_RESERVE_SUCCEEDED)
	{
		return AS_NAMESPACE_QUALIFIER asEMetadataRegistrationResult::InvalidState;
	}
	int NextType = Reservation.FirstTypeSequence;
	int NextFunction = Reservation.FirstFunctionId;
	for (FAngelscriptTypeBindInfo& Type : Mutable.Types)
	{
		if (Type.Declaration.Kind != EAngelscriptTypeBindKind::Namespace)
		{
			Type.PublishedTypeId = AS_NAMESPACE_QUALIFIER asCTypeIdAllocator::EncodeObjectTypeId(
				NextType++, Type.Declaration.ObjectFlags);
		}
		for (FAngelscriptTypeBindInfoMember& Member : Type.Members)
		{
			if (IsPublishedCallable(Member.Kind)) Member.PublishedFunctionId = NextFunction++;
		}
	}
	Mutable.bPublished = true;
	return AS_NAMESPACE_QUALIFIER asEMetadataRegistrationResult::Succeeded;
}

int FAngelscriptTypeBindInfoStore::GetPublishedTypeId(FAngelscriptTypeBindHandle Handle) const
{
	const FAngelscriptTypeBindInfo* Type = GetType(Handle);
	return Type ? Type->PublishedTypeId : -1;
}

int FAngelscriptTypeBindInfoStore::GetPublishedFunctionId(FAngelscriptTypeBindMemberHandle Handle) const
{
	const FAngelscriptTypeBindInfoMember* Member = GetMember(Handle);
	return Member ? Member->PublishedFunctionId : -1;
}

namespace
{
	void SplitPublishedMember(const FString& Declaration, FString& OutTypeName, FString& OutName)
	{
		FString Head = Declaration;
		const int32 Paren = Head.Find(TEXT("("));
		if (Paren != INDEX_NONE) Head.LeftInline(Paren);
		Head.TrimStartAndEndInline();
		int32 Space = INDEX_NONE;
		if (!Head.FindLastChar(TEXT(' '), Space))
		{
			OutName = Head;
			return;
		}
		OutName = Head.Mid(Space + 1).TrimStartAndEnd();
		OutTypeName = Head.Left(Space).TrimStartAndEnd();
	}
}

AS_NAMESPACE_QUALIFIER asETypeQueryResult FAngelscriptTypeBindInfoStore::GetPublishedClassInfo(
	int TypeId, FAngelscriptPublishedClassInfo& Out) const
{
	Out = {};
	if (TypeId <= AS_NAMESPACE_QUALIFIER asTYPEID_LAST_PRIMITIVE)
	{
		return AS_NAMESPACE_QUALIFIER asTYPE_QUERY_INVALID_ARGUMENT;
	}
	if (!bPublished)
	{
		return AS_NAMESPACE_QUALIFIER asTYPE_QUERY_NOT_FOUND;
	}
	for (const FAngelscriptTypeBindInfo& Type : Types)
	{
		if (Type.PublishedTypeId != TypeId) continue;
		Out.Name = Type.Declaration.Name;
		Out.Namespace = Type.Declaration.Namespace;
		Out.TypeId = Type.PublishedTypeId;
		Out.NativeSize = Type.Declaration.NativeSize;
		Out.NativeAlignment = Type.Declaration.NativeAlignment;
		Out.Members.Reserve(Type.Members.Num());
		for (const FAngelscriptTypeBindInfoMember& Member : Type.Members)
		{
			FAngelscriptPublishedMemberInfo Published;
			Published.Declaration = Member.Declaration;
			Published.Kind = Member.Kind;
			Published.PropertyOffset = Member.PropertyOffset;
			Published.PublishedFunctionId = Member.PublishedFunctionId;
			SplitPublishedMember(Member.Declaration, Published.TypeName, Published.Name);
			Out.Members.Add(MoveTemp(Published));
		}
		return AS_NAMESPACE_QUALIFIER asTYPE_QUERY_SUCCEEDED;
	}
	return AS_NAMESPACE_QUALIFIER asTYPE_QUERY_NOT_FOUND;
}
