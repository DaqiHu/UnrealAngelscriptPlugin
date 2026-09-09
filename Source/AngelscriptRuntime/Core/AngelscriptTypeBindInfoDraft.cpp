#include "AngelscriptTypeBindInfoDraft.h"
#include "as_objecttype.h"
#include "as_typeinfo.h"

#ifdef AS_USE_NAMESPACE
using namespace AngelScript;
#endif

namespace
{
	bool DraftFailure(const FAngelscriptTypeBindSource& Source, const FString& Declaration,
		const FString& Reason, FString& Diagnostic)
	{
		Diagnostic = FString::Printf(TEXT("Layouts: %s / %s (%s:%d), '%s': %s"),
			*Source.OwnerModule.ToString(), *Source.Provider.ToString(), *Source.File, Source.Line, *Declaration, *Reason);
		return false;
	}
	asETypeDeclKind DeclarationKind(EAngelscriptTypeBindKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptTypeBindKind::Value: return asETypeDeclKind::Struct;
		case EAngelscriptTypeBindKind::ObjectHandle: return asETypeDeclKind::Class;
		case EAngelscriptTypeBindKind::Enum: return asETypeDeclKind::Enum;
		case EAngelscriptTypeBindKind::Interface: return asETypeDeclKind::Interface;
		case EAngelscriptTypeBindKind::Alias: return asETypeDeclKind::Typedef;
		case EAngelscriptTypeBindKind::Template: return asETypeDeclKind::Struct;
		default: return asETypeDeclKind::Invalid;
		}
	}
	int32 TemplateArity(const FString& Spelling)
	{
		FString Parameters = Spelling;
		if (!Parameters.RemoveFromStart(TEXT("<")) || !Parameters.RemoveFromEnd(TEXT(">"))) return 0;
		TArray<FString> Parts; Parameters.ParseIntoArray(Parts, TEXT(","), true);
		return Parts.Num();
	}
}

bool FAngelscriptTypeBindInfoDraft::Create(TSharedRef<const FAngelscriptTypeBindInfoStore> Store,
	TUniquePtr<FAngelscriptTypeBindInfoDraft>& Out, FString& Diagnostic)
{
	Out.Reset(); Diagnostic.Reset();
	if (!Store->IsSealed() || Store->HasFailed())
	{
		Diagnostic = TEXT("Layouts require a valid sealed binding store."); return false;
	}
	TUniquePtr<FAngelscriptTypeBindInfoDraft> Draft(new FAngelscriptTypeBindInfoDraft(Store));
	Draft->Image = asCMetadataImage::Create();
	Draft->TemplateImage = asCMetadataImage::Create();
	if (Draft->Image->SetIdentityContext(Draft->Context) != asEMetadataResult::Succeeded
		|| Draft->TemplateImage->SetIdentityContext(Draft->Context) != asEMetadataResult::Succeeded)
	{
		Diagnostic = TEXT("Layouts could not attach the canonical type context."); return false;
	}
	const auto Records = Store->GetTypes();
	Draft->Types.SetNumZeroed(Records.Num()); Draft->Nominals.SetNum(Records.Num());
	// Every nominal shell exists before a base, alias or property is resolved.
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		const auto& D = Records[Index].Declaration;
		if (D.Kind == EAngelscriptTypeBindKind::Namespace) continue;
		asSNominalTypeDescriptor Nominal;
		Nominal.StableScope = StableScope; D.Namespace.ParseIntoArray(Nominal.NamespacePath, TEXT("::"), true);
		Nominal.Name = D.Name; Nominal.Kind = DeclarationKind(D.Kind);
		Nominal.GenericArity = D.Kind == EAngelscriptTypeBindKind::Template ? TemplateArity(D.TemplateType) : 0;
		if (Nominal.Kind == asETypeDeclKind::Invalid)
			return DraftFailure(D.Source, D.Name, TEXT("type kind requires its dedicated installer"), Diagnostic);
		if (D.Kind == EAngelscriptTypeBindKind::Template && Nominal.GenericArity <= 0)
			return DraftFailure(D.Source, D.TemplateDeclaration, TEXT("template declaration requires canonical generic arguments"), Diagnostic);
		if (Draft->Context->GetIdentityRegistry().InternNominalType(Nominal, Draft->Nominals[Index]) != asETypeIdentityStatus::Succeeded)
			return DraftFailure(D.Source, D.Name, TEXT("invalid nominal identity"), Diagnostic);
		asEMetadataResult Status;
		if (D.Kind == EAngelscriptTypeBindKind::Enum)
		{
			asCEnumType* Type = nullptr;
			Status = Draft->Image->CreateEnumType(Draft->Nominals[Index], TCHAR_TO_UTF8(*D.Name), TCHAR_TO_UTF8(*D.Namespace), Type);
			Draft->Types[Index] = Type;
		}
		else if (D.Kind == EAngelscriptTypeBindKind::Alias)
		{
			asCTypedefType* Type = nullptr;
			Status = Draft->Image->CreateTypedefType(Draft->Nominals[Index], TCHAR_TO_UTF8(*D.Name), TCHAR_TO_UTF8(*D.Namespace), Type);
			Draft->Types[Index] = Type;
		}
		else
		{
			asCObjectType* Type = nullptr;
			auto TargetImage = D.Kind == EAngelscriptTypeBindKind::Template ? Draft->TemplateImage : Draft->Image;
			Status = TargetImage->CreateObjectType(Draft->Nominals[Index], TCHAR_TO_UTF8(*D.Name), TCHAR_TO_UTF8(*D.Namespace), D.ObjectFlags, Type);
			Draft->Types[Index] = Type;
			if (Status == asEMetadataResult::Succeeded)
			{
				if (D.Kind == EAngelscriptTypeBindKind::Template)
					Status = TargetImage->SetTypeDeclarationFacts(*Type, Nominal.Kind, Nominal.GenericArity);
				if (Status == asEMetadataResult::Succeeded) Status = D.Kind == EAngelscriptTypeBindKind::Interface
					? TargetImage->SetInterfaceDeclaration(*Type)
					: D.Kind == EAngelscriptTypeBindKind::Template
						? TargetImage->SetFixedTemplateLayout(*Type, D.NativeSize, D.NativeAlignment)
						: TargetImage->SetNativeLayout(*Type, D.NativeSize, D.NativeAlignment);
			}
		}
		if (Status != asEMetadataResult::Succeeded)
			return DraftFailure(D.Source, D.Name, TEXT("invalid native type definition or alignment"), Diagnostic);
		auto FactsImage = D.Kind == EAngelscriptTypeBindKind::Template ? Draft->TemplateImage : Draft->Image;
		if (D.Kind != EAngelscriptTypeBindKind::Template
			&& FactsImage->SetTypeDeclarationFacts(*Draft->Types[Index], Nominal.Kind, Nominal.GenericArity) != asEMetadataResult::Succeeded)
			return DraftFailure(D.Source, D.Name, TEXT("invalid declaration facts"), Diagnostic);
	}
	if (Draft->TemplateImage->FinalizeLayouts() != asEMetadataResult::Succeeded
		|| Draft->TemplateImage->Freeze() != asEMetadataResult::Succeeded
		|| !Draft->TemplateImage->ValidateFrozenDefinitions()
		|| Draft->Image->AddDependency(Draft->TemplateImage) != asEMetadataResult::Succeeded)
	{
		Diagnostic = TEXT("Layouts: template declarations could not be frozen as a dependency."); return false;
	}
	struct FEdge { int32 Target; const FAngelscriptTypeBindSource* Source; FString Declaration; };
	TArray<TArray<FEdge>> Dependencies; Dependencies.SetNum(Records.Num());
	auto AddEdge = [&](int32 Owner, asCTypeInfo* Target, const FAngelscriptTypeBindSource& Source, FString Text)
	{
		const int32 TargetIndex = Draft->Types.IndexOfByKey(Target);
		if (Target && TargetIndex != INDEX_NONE) Dependencies[Owner].Add({TargetIndex, &Source, MoveTemp(Text)});
	};
	auto Parse = [&](const FString& Text, const FAngelscriptTypeBindDeclaration& Owner,
		const FAngelscriptTypeBindSource& Source, asSBindingDeclaration& Parsed, asCDataType& Type)
	{
		asSBindingDeclarationContext Environment; Environment.StableScope = StableScope; Environment.Namespace = Owner.Namespace;
		asSBindingDeclarationDiagnostic Error;
		if (!asCBindingDeclarationParser::Parse(Text, Environment, Draft->Context.Get(),
			[&](const FString& Name, const FString& Namespace) { return Draft->FindNominal(Name, Namespace); }, Parsed, Error))
			return DraftFailure(Source, Text, FString::Printf(TEXT("%s (bytes %u..%u)"), *Error.Message,
				Error.Range.Begin.ByteOffset, Error.Range.End.ByteOffset), Diagnostic);
		FString Reason;
		if (!Draft->ResolveTypeUse(Parsed.Type, Type, Reason)) return DraftFailure(Source, Text, Reason, Diagnostic);
		return true;
	};
	struct FProperty { int32 Owner; const FAngelscriptTypeBindInfoMember* Record; FString Name; asCDataType Type; };
	TArray<FProperty> Properties;
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		const auto& Record = Records[Index]; const auto& D = Record.Declaration;
		if (!Draft->Types[Index]) continue;
		if (D.Kind == EAngelscriptTypeBindKind::Template) continue;
		auto* Object = CastToObjectType(Draft->Types[Index]);
		if (!D.BaseType.IsEmpty())
		{
			auto* Base = CastToObjectType(Draft->FindType(D.BaseType, D.Namespace));
			if (!Object || !Base || Draft->Image->SetBaseType(*Object, *Base) != asEMetadataResult::Succeeded)
				return DraftFailure(D.Source, D.Name + TEXT(" : ") + D.BaseType, TEXT("missing, invalid or cyclic base type"), Diagnostic);
			AddEdge(Index, Base, D.Source, D.Name + TEXT(" : ") + D.BaseType);
		}
		for (const auto& Name : D.Interfaces)
		{
			auto* Interface = CastToObjectType(Draft->FindType(Name, D.Namespace));
			if (!Object || !Interface || Draft->Image->AddInterface(*Object, *Interface) != asEMetadataResult::Succeeded)
				return DraftFailure(D.Source, D.Name + TEXT(" : ") + Name, TEXT("missing or invalid interface"), Diagnostic);
			AddEdge(Index, Interface, D.Source, D.Name + TEXT(" : ") + Name);
		}
		if (D.Kind == EAngelscriptTypeBindKind::Alias)
		{
			asSBindingDeclaration Parsed; asCDataType Type;
			if (!Parse(D.AliasTarget, D, D.Source, Parsed, Type)) return false;
			if (!Parsed.Name.IsEmpty() || !Parsed.Function.IsZero() ||
				Draft->Image->SetAlias(*CastToTypedefType(Draft->Types[Index]), Type) != asEMetadataResult::Succeeded)
				return DraftFailure(D.Source, D.Name, TEXT("invalid alias target"), Diagnostic);
			if (!Type.IsReference() && !Type.IsObjectHandle()) AddEdge(Index, Type.GetTypeInfo(), D.Source, D.Name);
		}
		for (const auto& Member : Record.Members)
		{
			const auto& Source = Member.Source.File.IsEmpty() ? D.Source : Member.Source;
			if (Member.Kind == EAngelscriptTypeBindMemberKind::EnumValue)
			{
				auto* Enum = CastToEnumType(Draft->Types[Index]);
				if (!Enum || Draft->Image->AddEnumValue(*Enum, TCHAR_TO_UTF8(*Member.Declaration), Member.EnumValue) != asEMetadataResult::Succeeded)
					return DraftFailure(Source, Member.Declaration, TEXT("invalid enum value"), Diagnostic);
			}
			else if (Member.Kind == EAngelscriptTypeBindMemberKind::Property)
			{
				asSBindingDeclaration Parsed; asCDataType Type;
				if (!Parse(Member.Declaration, D, Source, Parsed, Type)) return false;
				if (!Object || Parsed.Name.IsEmpty() || !Parsed.Function.IsZero())
					return DraftFailure(Source, Member.Declaration, TEXT("native property requires an object owner and field declaration"), Diagnostic);
				Properties.Add({Index, &Member, Parsed.Name, Type});
				if (!Type.IsReference() && !Type.IsObjectHandle() && !(Type.GetTypeInfo() && (Type.GetTypeInfo()->flags & asOBJ_REF)))
					AddEdge(Index, Type.GetTypeInfo(), Source, Member.Declaration);
			}
		}
	}
	TArray<uint8> Visiting; Visiting.SetNumZeroed(Records.Num()); TArray<int32> Path;
	TFunction<bool(int32)> Visit = [&](int32 Index)
	{
		if (Visiting[Index] == 2) return true;
		Visiting[Index] = 1; Path.Add(Index);
		for (const auto& Edge : Dependencies[Index])
		{
			if (Visiting[Edge.Target] == 1)
			{
				FString Cycle;
				for (int32 Node : Path) Cycle += Records[Node].Declaration.Name + TEXT(" -> ");
				Cycle += Records[Edge.Target].Declaration.Name;
				return DraftFailure(*Edge.Source, Edge.Declaration, TEXT("by-value/base/interface dependency cycle: ") + Cycle, Diagnostic);
			}
			if (!Visit(Edge.Target)) return false;
		}
		Path.Pop(EAllowShrinking::No); Visiting[Index] = 2; return true;
	};
	for (int32 Index = 0; Index < Records.Num(); ++Index) if (!Visit(Index)) return false;
	// Publish native type sizes only within this private draft so the checked
	// property API can validate forward member extents. The image stays mutable.
	if (Draft->Image->FinalizeLayouts() != asEMetadataResult::Succeeded)
	{
		Diagnostic = TEXT("Layouts: incomplete or invalid native type/alias layout."); return false;
	}
	for (const auto& Property : Properties)
	{
		const auto& Member = *Property.Record;
		asSNativePropertyAddress Address; Address.Offset = Member.PropertyOffset;
		asCObjectProperty* Installed = nullptr;
		if (Draft->Image->DefineNativeProperty(*CastToObjectType(Draft->Types[Property.Owner]), TCHAR_TO_UTF8(*Property.Name),
			Property.Type, Address, Installed) != asEMetadataResult::Succeeded)
			return DraftFailure(Member.Source.File.IsEmpty() ? Records[Property.Owner].Declaration.Source : Member.Source,
				Member.Declaration, TEXT("invalid native property offset, extent or alignment"), Diagnostic);
	}
	if (Draft->Image->FinalizeLayouts() != asEMetadataResult::Succeeded)
	{
		Diagnostic = TEXT("Layouts: native member layout finalization failed."); return false;
	}
	Out = MoveTemp(Draft); return true;
}

asSStableKey FAngelscriptTypeBindInfoDraft::FindNominal(const FString& Name, const FString& Namespace) const
{
	FString Spelling = Name;
	const bool Global = Spelling.RemoveFromStart(TEXT("::"), ESearchCase::CaseSensitive);
	FString Scope = Global ? FString() : Namespace;
	for (;;)
	{
		FString Qualified = Scope.IsEmpty() ? Spelling : Scope + TEXT("::") + Spelling;
		const int32 Split = Qualified.Find(TEXT("::"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		const auto Handle = Store->FindType(Split == INDEX_NONE ? Qualified : Qualified.Mid(Split + 2), Split == INDEX_NONE ? FString() : Qualified.Left(Split));
		if (Nominals.IsValidIndex(Handle.Index)) return Nominals[Handle.Index];
		if (Scope.IsEmpty()) return {};
		const int32 Parent = Scope.Find(TEXT("::"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		Scope = Parent == INDEX_NONE ? FString() : Scope.Left(Parent);
	}
}
asCTypeInfo* FAngelscriptTypeBindInfoDraft::FindType(const FString& Name, const FString& Namespace) const
{
	return FindTypeByKey(FindNominal(Name, Namespace));
}

asCTypeInfo* FAngelscriptTypeBindInfoDraft::FindTypeByKey(const asSStableKey& Key) const
{
	if (Key.IsZero()) return nullptr;
	if (Image) if (auto* Type = Image->FindType(Key)) return Type;
	if (TemplateImage) if (auto* Type = TemplateImage->FindType(Key)) return Type;
	for (const auto& Late : LateImages) if (auto* Type = Late->FindType(Key)) return Type;
	return nullptr;
}

const FAngelscriptTypeBindDeclaration* FAngelscriptTypeBindInfoDraft::FindDeclaration(const asCTypeInfo* Type) const
{
	if (!Type) return nullptr;
	if (const asCObjectType* Object = CastToObjectType(const_cast<asCTypeInfo*>(Type)))
		if (Object->GetMetadataTemplateDefinition()) Type = Object->GetMetadataTemplateDefinition();
	const int32 Index = Types.IndexOfByKey(const_cast<asCTypeInfo*>(Type));
	const auto Records = Store->GetTypes();
	return Records.IsValidIndex(Index) ? &Records[Index].Declaration : nullptr;
}

bool FAngelscriptTypeBindInfoDraft::ResolveTypeUse(const asSStableKey& Key, asCDataType& Out, FString& Diagnostic)
{
	return ResolveTypeUseInImage(Key, Image, Out, Diagnostic);
}

bool FAngelscriptTypeBindInfoDraft::ResolveTypeUseInImage(const asSStableKey& Key,
	const std::shared_ptr<asCMetadataImage>& Target, asCDataType& Out, FString& Diagnostic)
{
	Out = {}; Diagnostic.Reset();
	if (!Target || Target->GetState() != asEMetadataImageState::Building)
	{
		Diagnostic = TEXT("template type use requires a mutable dependent image"); return false;
	}
	asSStableTypeUseDescriptor Use;
	if (Context->GetIdentityRegistry().ResolveTypeUse(Key, Use) != asETypeIdentityStatus::Succeeded)
	{
		Diagnostic = TEXT("unknown canonical type use"); return false;
	}
	if (Use.Kind == asETypeUseKind::Primitive)
	{
		eTokenType Token = ttUnrecognizedToken;
		switch (Use.Primitive)
		{
		case asEPrimitiveType::Void: Token = ttVoid; break;
		case asEPrimitiveType::Bool: Token = ttBool; break;
		case asEPrimitiveType::Int8: Token = ttInt8; break;
		case asEPrimitiveType::Int16: Token = ttInt16; break;
		case asEPrimitiveType::Int32: Token = ttInt; break;
		case asEPrimitiveType::Int64: Token = ttInt64; break;
		case asEPrimitiveType::UInt8: Token = ttUInt8; break;
		case asEPrimitiveType::UInt16: Token = ttUInt16; break;
		case asEPrimitiveType::UInt32: Token = ttUInt; break;
		case asEPrimitiveType::UInt64: Token = ttUInt64; break;
		case asEPrimitiveType::Float32: Token = ttFloat32; break;
		case asEPrimitiveType::Float64: Token = ttFloat64; break;
		default: Diagnostic = TEXT("unsupported primitive type"); return false;
		}
		Out = Target->Primitive(Token);
	}
	else if (Use.Kind == asETypeUseKind::Wildcard)
	{
		Out = Target->Primitive(ttQuestion);
	}
	else if (Use.Kind == asETypeUseKind::Nominal && Use.Arguments.IsEmpty())
	{
		auto* Type = FindTypeByKey(Use.Declaration);
		if (!Type) { Diagnostic = TEXT("nominal type is absent from this draft"); return false; }
		Out = asCDataType::CreateType(Type, false);
	}
	else if (Use.Kind == asETypeUseKind::Nominal && !Use.Arguments.IsEmpty())
	{
		asSStableKey InstanceKey = Key;
		if (Use.Qualifiers != asETypeUseQualifiers::None)
		{
			asSStableTypeUseDescriptor InstanceUse = Use;
			InstanceUse.Qualifiers = asETypeUseQualifiers::None;
			if (Context->GetIdentityRegistry().InternTypeUse(InstanceUse, InstanceKey) != asETypeIdentityStatus::Succeeded)
			{
				Diagnostic = TEXT("could not canonicalize the unqualified template instance"); return false;
			}
		}
		if (auto* Existing = CastToObjectType(FindTypeByKey(InstanceKey)); Existing && Target->CanReference(Existing))
		{
			Out = asCDataType::CreateType(Existing, false);
		}
		else
		{
			auto* Template = CastToObjectType(FindTypeByKey(Use.Declaration));
			const auto* Declaration = FindDeclaration(Template);
			if (!Template || !Declaration || Declaration->Kind != EAngelscriptTypeBindKind::Template)
			{
				Diagnostic = TEXT("template declaration is absent from this draft"); return false;
			}
			TArray<asCDataType> Arguments;
			for (const asSStableKey& ArgumentKey : Use.Arguments)
			{
				asCDataType Argument;
				if (!ResolveTypeUseInImage(ArgumentKey, Target, Argument, Diagnostic)) return false;
				Arguments.Add(Argument);
			}
			asCObjectType* Instance = nullptr;
			const asEMetadataResult InstanceResult = Target->CreateTemplateInstance(*Template, InstanceKey, Arguments, Instance);
			if (InstanceResult != asEMetadataResult::Succeeded || !Instance)
			{
				Diagnostic = FString::Printf(TEXT("could not materialize template instance '%s' (metadata status %d)"),
					*Declaration->Name, static_cast<int32>(InstanceResult)); return false;
			}
			if (Declaration->TemplateContainer == EAngelscriptTemplateContainerKind::Optional)
			{
				const uint32 Alignment = FMath::Max<uint32>(1, Arguments[0].GetAlignment());
				const uint32 Payload = Align(static_cast<uint32>(Arguments[0].GetSizeInMemoryBytes()), Alignment);
				const uint32 Size = Align(Payload + static_cast<uint32>(sizeof(bool)), Alignment);
				if (Target->SetNativeLayout(*Instance, Size, Alignment) != asEMetadataResult::Succeeded)
				{
					Diagnostic = TEXT("optional specialization has an invalid element-dependent layout"); return false;
				}
			}
			Out = asCDataType::CreateType(Instance, false);
		}
	}
	else { Diagnostic = TEXT("type use requires callable installation"); return false; }
	const auto Has = [&](asETypeUseQualifiers Flag) { return (static_cast<uint16>(Use.Qualifiers) & static_cast<uint16>(Flag)) != 0; };
	if (Has(asETypeUseQualifiers::Handle) && Out.MakeHandle(true) < 0)
	{
		Out = {}; Diagnostic = TEXT("type does not support object handles"); return false;
	}
	if (Has(asETypeUseQualifiers::Const)) Out.MakeConstValueOrConstObject(true);
	if (Has(asETypeUseQualifiers::Reference)) Out.MakeReference(true);
	Out.SetMetadataNullable(Has(asETypeUseQualifiers::Nullable));
	return true;
}

bool FAngelscriptTypeBindInfoDraft::MaterializeTemplateDeclaration(const FString& Declaration,
	std::shared_ptr<asCMetadataImage>& OutImage, asCObjectType*& OutType, FString& Diagnostic)
{
	OutImage.reset(); OutType = nullptr; Diagnostic.Reset();
	asSBindingDeclarationContext Environment; Environment.StableScope = StableScope;
	asSBindingDeclaration Parsed; asSBindingDeclarationDiagnostic Error;
	if (!asCBindingDeclarationParser::Parse(Declaration, Environment, Context.Get(),
		[&](const FString& Name, const FString& Namespace) { return FindNominal(Name, Namespace); }, Parsed, Error)
		|| !Parsed.Name.IsEmpty() || !Parsed.Function.IsZero())
	{
		Diagnostic = Error.Message.IsEmpty() ? TEXT("template instance requires a type-only declaration")
			: FString::Printf(TEXT("%s (bytes %u..%u)"), *Error.Message, Error.Range.Begin.ByteOffset, Error.Range.End.ByteOffset);
		return false;
	}
	if (auto* Existing = CastToObjectType(FindTypeByKey(Parsed.Type)))
	{
		if (!Existing->GetMetadataTemplateDefinition()) { Diagnostic = TEXT("declaration is not a concrete template instance"); return false; }
		OutType = Existing; return true;
	}
	auto Late = asCMetadataImage::Create(Image->GetOptions());
	if (Late->SetIdentityContext(Context) != asEMetadataResult::Succeeded
		|| Late->AddDependency(Image) != asEMetadataResult::Succeeded)
	{
		Diagnostic = TEXT("could not create a dependent template image"); return false;
	}
	for (const auto& Prior : LateImages)
	{
		if (Late->AddDependency(Prior) != asEMetadataResult::Succeeded)
		{
			Diagnostic = TEXT("could not retain a prior template specialization dependency"); return false;
		}
	}
	asCDataType Type;
	if (!ResolveTypeUseInImage(Parsed.Type, Late, Type, Diagnostic)) return false;
	OutType = CastToObjectType(Type.GetTypeInfo());
	if (!OutType || !OutType->GetMetadataTemplateDefinition()
		|| Late->FinalizeLayouts() != asEMetadataResult::Succeeded
		|| Late->Freeze() != asEMetadataResult::Succeeded || !Late->ValidateFrozenDefinitions())
	{
		OutType = nullptr; Diagnostic = TEXT("dependent template image failed complete definition validation"); return false;
	}
	LateImages.Add(Late); OutImage = MoveTemp(Late); return true;
}
