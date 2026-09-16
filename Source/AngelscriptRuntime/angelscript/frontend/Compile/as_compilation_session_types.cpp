#include "frontend/Compile/as_compilation_session.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_type.h"
#include "as_definitions.h"
#include "as_scriptfunction.h"
#include "as_objecttype.h"
#include "as_typeinfo.h"
#include "as_namespace.h"
#include "Containers/Set.h"

BEGIN_AS_NAMESPACE
void asCCompilationSession::BindFrozenDefinitions(const asCDefinitions* Set)
	{
		FrozenDefinitions = Set;
	}

bool asCCompilationSession::AddExternalDefinitions(asCDefinitions* Set)
	{
		if (Phase != asECompilationPhase::Collecting || !Set) return false;
		if (ExternalDefinitions.Contains(Set)) return true;
		TArray<asCDefinitions*> Closure;
		TSet<const asCDefinitions*> Visited;
		TFunction<bool(asCDefinitions*)> Visit = [&](asCDefinitions* Item)
		{
			if (!Item || !Item->IsFrozen() || !Item->GetIdentityContext() ||
				Item->GetIdentityContext()->GetIdentityRegistryOwner() != Context.GetTypeContext()->GetIdentityRegistryOwner()) return false;
			if (Item->GetOptions().Kind != asETypeInfoKind::HostProcess && !Item->ValidateFrozenDefinitions()) return false;
			if (Visited.Contains(Item) || ExternalDefinitions.Contains(Item)) return true;
			Visited.Add(Item);
			for (asCDefinitions* Dependency : Item->GetDependencies()) if (!Visit(Dependency)) return false;
			Closure.Add(Item);
			return true;
		};
		if (!Visit(Set)) return false;
		TMap<FString, asCType*> Added;
		auto& Registry = Context.GetTypeContext()->GetIdentityRegistry();
		for (const auto& Item : Closure)
		for (const asCTypeInfo* Type : Item->GetTypes())
		{
			asSNominalTypeDescriptor Decl;
			if (Registry.ResolveNominalType(Type->GetStableKey(), Decl) != asETypeIdentityStatus::Succeeded) continue;
			TArray<FString> Parts = Decl.NamespacePath;
			TArray<FString> Owners;
			asSStableKey Owner = Decl.Owner;
			for (int32 Depth = 0; !Owner.IsZero(); ++Depth)
			{
				asSNominalTypeDescriptor Parent;
				if (Depth >= 64 || Registry.ResolveNominalType(Owner, Parent) != asETypeIdentityStatus::Succeeded) return false;
				Owners.Insert(Parent.Name, 0); Owner = Parent.Owner;
			}
			Parts.Append(Owners); Parts.Add(Decl.Name);
			const FString Name = FString::Join(Parts, TEXT("::"));
			if (Added.Contains(Name) || ExternalTypes.Contains(Name)) return false;
			asSStableTypeUseDescriptor Use;
			Use.Kind = asETypeUseKind::Nominal; Use.Declaration = Type->GetStableKey();
			asSStableKey Key;
			if (Registry.InternTypeUse(Use, Key) != asETypeIdentityStatus::Succeeded) return false;
			asCType* Resolved = Context.GetNominalType(Use.Declaration, Key);
			if (!Resolved) return false;
			Added.Add(Name, Resolved);
		}
		TMap<FString, TArray<asSExternalCallable>> Callables;
		for (const auto& Item : Closure)
		{
			for (asCScriptFunction* Function : Item->GetFunctions())
			{
				if (!Function || Function->IsMetadataCallableSignature() || Function->name.GetLength() == 0) continue;
				asSFunctionIdentityDescriptor Descriptor;
				if (Registry.ResolveFunction(Function->GetStableKey(), Descriptor) != asETypeIdentityStatus::Succeeded)
				{
					// HostProcess still writes CRC keys for most binds. Compile
					// against that freeze must publish interned types without
					// requiring every function to be interned first.
					if (Item->GetOptions().Kind == asETypeInfoKind::HostProcess) continue;
					return false;
				}
				asSExternalCallable Callable;
				Callable.Key = Function->GetStableKey();
				Callable.Function = Function;
				Callable.OwnerKey = Descriptor.Owner;
				Callable.Set = Item;
				Callable.bConstMethod = Function->IsReadOnly();
				FString Name = UTF8_TO_TCHAR(Function->name.AddressOf());
				FString Qualified = Name;
				if (Function->nameSpace && Function->nameSpace->name.GetLength() > 0)
					Qualified = FString(UTF8_TO_TCHAR(Function->nameSpace->name.AddressOf())) + TEXT("::") + Name;
				if (Function->objectType)
					Qualified = FString(UTF8_TO_TCHAR(Function->objectType->name.AddressOf())) + TEXT("::") + Name;
				Callable.QualifiedName = Qualified;
				Callables.FindOrAdd(Qualified).Add(Callable);
			}
		}
		ExternalTypes.Append(Added);
		ExternalDefinitions.Append(Closure);
		for (auto& Pair : Callables)
			ExternalCallables.FindOrAdd(Pair.Key).Append(MoveTemp(Pair.Value));
		return true;
	}

	TConstArrayView<asSExternalCallable> asCCompilationSession::FindExternalCallables(const FString& Name) const
	{
		if (const TArray<asSExternalCallable>* Found = ExternalCallables.Find(Name)) return MakeArrayView(*Found);
		return {};
	}

	asCRecordDecl* asCCompilationSession::FindRecordDeclaration(const asSStableKey& Identity) const
	{
		return RecordDeclarations.FindRef(Identity);
	}
	asCNamedDecl* asCCompilationSession::FindNominalDeclaration(const asSStableKey& Identity) const
	{
		return NominalDeclarations.FindRef(Identity);
	}

	asCQualType asCCompilationSession::ResolveStructuredType(const asSTypeSyntax& Syntax,
		const TFunction<asCType*(const FString&, bool)>& LookupType, int32 Depth, FString* OutReason)
	{
		auto Fail = [&](const TCHAR* Reason) -> asCQualType
		{
			if (OutReason && OutReason->IsEmpty()) *OutReason = Reason;
			return {};
		};
		if (Depth > 64 || !Syntax.IsValid()) return Fail(TEXT("unresolved-type"));
		asCType* Type = LookupType(Syntax.GetQualifiedName(), Syntax.bGlobal);
		if (!Type) return Fail(TEXT("unresolved-type"));
		auto& Registry = Context.GetTypeContext()->GetIdentityRegistry();
		TArray<asCQualType> Arguments;
		for (const auto& Argument : Syntax.GenericArguments)
		{
			auto Resolved = ResolveStructuredType(Argument, LookupType, Depth + 1, OutReason);
			if (!Resolved.IsValid()) return Fail(TEXT("unresolved-type"));
			if (const auto* Builtin = dyn_cast<asCBuiltinType>(Resolved.GetType()); Builtin && Builtin->GetPrimitive() == asEPrimitiveType::Void)
				return Fail(TEXT("invalid-qualifier"));
			Arguments.Add(Resolved);
		}
		auto Instantiate = [&](asCType* Template, TArray<asCQualType> ActualArguments) -> asCType*
		{
			const auto* Nominal = dyn_cast<asCNominalType>(Template);
			asSNominalTypeDescriptor Decl;
			if (!Nominal || Registry.ResolveNominalType(Nominal->GetDeclarationIdentity(), Decl) != asETypeIdentityStatus::Succeeded)
			{
				if (OutReason && OutReason->IsEmpty()) *OutReason = TEXT("unresolved-type");
				return nullptr;
			}
			if (Decl.GenericArity != asUINT(ActualArguments.Num()))
			{
				if (OutReason && OutReason->IsEmpty()) *OutReason = TEXT("generic-arity");
				return nullptr;
			}
			asSStableTypeUseDescriptor Use;
			Use.Kind = asETypeUseKind::Nominal; Use.Declaration = Nominal->GetDeclarationIdentity();
			for (const auto& Argument : ActualArguments)
			{
				asSStableKey Key;
				if (Argument.GetStableIdentity(Registry, Key) != asETypeIdentityStatus::Succeeded)
				{
					if (OutReason && OutReason->IsEmpty()) *OutReason = TEXT("unresolved-type");
					return nullptr;
				}
				Use.Arguments.Add(Key);
			}
			asSStableKey Key;
			if (Registry.InternTypeUse(Use, Key) != asETypeIdentityStatus::Succeeded)
			{
				if (OutReason && OutReason->IsEmpty()) *OutReason = TEXT("unresolved-type");
				return nullptr;
			}
			return Context.GetGenericType(Template, MoveTemp(ActualArguments), Key);
		};
		if (!Arguments.IsEmpty()) Type = Instantiate(Type, MoveTemp(Arguments));
		else if (const auto* Nominal = dyn_cast<asCNominalType>(Type))
		{
			asSNominalTypeDescriptor Decl;
			if (Registry.ResolveNominalType(Nominal->GetDeclarationIdentity(), Decl) != asETypeIdentityStatus::Succeeded)
				return Fail(TEXT("unresolved-type"));
			if (Decl.GenericArity) return Fail(TEXT("generic-arity"));
		}
		if (!Type) return {};
		for (const auto& Dimension : Syntax.ArrayDimensions)
		{
			if (!Dimension.IsOrdered()) return Fail(TEXT("unresolved-type"));
			asCQualType Element;
			if (!asCQualType::TryCreate(Type, asETypeQualifiers::None, Element)) return Fail(TEXT("invalid-qualifier"));
			asCType* Template = LookupType(Context.GetTypeContext()->GetOptions().ArrayTemplateTypeName, false);
			Type = Instantiate(Template, {Element});
			if (!Type) return {};
		}
		asETypeQualifiers Qualifiers = asETypeQualifiers::None;
		if (Syntax.bConst) Qualifiers = Qualifiers | asETypeQualifiers::Const;
		if (Syntax.bHandle) Qualifiers = Qualifiers | asETypeQualifiers::Handle;
		if (Syntax.Reference != asETypeReferenceDirection::None) Qualifiers = Qualifiers | asETypeQualifiers::Reference;
		if (const auto* Builtin = dyn_cast<asCBuiltinType>(Type))
			if (Syntax.bHandle || (Builtin->GetPrimitive() == asEPrimitiveType::Void && Qualifiers != asETypeQualifiers::None))
				return Fail(TEXT("invalid-qualifier"));
		asCQualType Result;
		if (!asCQualType::TryCreate(Type, Qualifiers, Result)) return Fail(TEXT("invalid-qualifier"));
		return Result;
	}

	asCQualType asCCompilationSession::ResolveTypeSyntax(const asSTypeSyntax& Syntax, const asCDecl& Scope)
	{
		return ResolveStructuredType(Syntax, [&](const FString& Name, bool Global) -> asCType*
		{
			const auto& Options = Context.GetTypeContext()->GetOptions();
			asEPrimitiveType Primitive = asEPrimitiveType::Invalid;
			if (Name == TEXT("void")) Primitive = asEPrimitiveType::Void;
			else if (Name == TEXT("bool")) Primitive = asEPrimitiveType::Bool;
			else if (Name == TEXT("int8")) Primitive = asEPrimitiveType::Int8;
			else if (Name == TEXT("int16")) Primitive = asEPrimitiveType::Int16;
			else if (Name == TEXT("int") || Name == TEXT("int32")) Primitive = asEPrimitiveType::Int32;
			else if (Name == TEXT("int64")) Primitive = asEPrimitiveType::Int64;
			else if (Name == TEXT("uint8")) Primitive = asEPrimitiveType::UInt8;
			else if (Name == TEXT("uint16")) Primitive = asEPrimitiveType::UInt16;
			else if (Name == TEXT("uint") || Name == TEXT("uint32")) Primitive = asEPrimitiveType::UInt32;
			else if (Name == TEXT("uint64")) Primitive = asEPrimitiveType::UInt64;
			else if (Name == TEXT("float")) Primitive = Options.FloatIsFloat64 ? asEPrimitiveType::Float64 : asEPrimitiveType::Float32;
			else if (Name == TEXT("float32")) Primitive = asEPrimitiveType::Float32;
			else if (Name == TEXT("double") || Name == TEXT("float64")) Primitive = asEPrimitiveType::Float64;
			if (Primitive != asEPrimitiveType::Invalid) return Context.GetBuiltinType(Primitive);
			const auto Found = LookupInBodyScope(Global ? TEXT("::") + Name : Name, Scope);
			asCType* Result = nullptr;
			for (asCDecl* Decl : Found)
				if (const auto* Named = dyn_cast<asCNamedDecl>(Decl))
					if (asCType* Declared = GetDeclaredType(Named->GetStableIdentity()))
					{
						if (Result) return nullptr;
						Result = Declared;
					}
			if (Result) return Result;
			// Search explicit environment entries with the same enclosing-scope
			// order as declarations; never scan an Engine or the global registry.
			if (!Global)
			{
				TArray<FString> Parts;
				for (auto* Parent = Scope.GetDeclContext(); Parent; )
				{
					auto* Owner = Parent->GetOwnerDecl();
					if (!Owner) break;
					if (const auto* Named = dyn_cast<asCNamedDecl>(Owner); Named &&
						(isa<asCNamespaceDecl>(Named) || isa<asCRecordDecl>(Named))) Parts.Insert(Named->GetName(), 0);
					Parent = Owner->GetDeclContext();
				}
				while (!Parts.IsEmpty())
				{
					if (auto* External = ExternalTypes.FindRef(FString::Join(Parts, TEXT("::")) + TEXT("::") + Name)) return External;
					Parts.Pop();
				}
			}
			return ExternalTypes.FindRef(Name);
		});
	}
END_AS_NAMESPACE
