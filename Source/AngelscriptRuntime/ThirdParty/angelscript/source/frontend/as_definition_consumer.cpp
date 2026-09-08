#include "frontend/as_definition_consumer.h"

#include "frontend/as_compilation_session.h"
#include "frontend/as_ast_cast.h"
#include "frontend/as_ast_verifier.h"
#include "frontend/as_attr.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptfunction.h"
#include "Containers/Set.h"
#include "Templates/Function.h"

BEGIN_AS_NAMESPACE
namespace
	{
		FString NamespaceOf(const asCDecl& Decl)
		{
			TArray<FString> Parts;
			for (auto* Parent = Decl.GetDeclContext(); Parent; )
			{
				auto* Owner = Parent->GetOwnerDecl();
				if (!Owner) break;
				if (const auto* Namespace = dyn_cast<asCNamespaceDecl>(Owner)) Parts.Insert(Namespace->GetName(), 0);
				Parent = Owner->GetDeclContext();
			}
			return FString::Join(Parts, TEXT("::"));
		}
		const asCRecordDecl* RecordOwner(const asCDecl& Decl)
		{
			for (auto* Parent = Decl.GetDeclContext(); Parent; )
			{
				auto* Owner = Parent->GetOwnerDecl();
				if (!Owner) return nullptr;
				if (const auto* Record = dyn_cast<asCRecordDecl>(Owner)) return Record;
				Parent = Owner->GetDeclContext();
			}
			return nullptr;
		}
		::asSAccessSpecifier ConvertAccess(const asCAccessSpecifierDecl& Access)
		{
			asSAccessSpecifier Result;
			Result.name = TCHAR_TO_UTF8(*Access.GetName());
			Result.bIsPrivate = Access.GetBaseAccess() == asEAccessSpecifier::Private;
			Result.bIsProtected = Access.GetBaseAccess() == asEAccessSpecifier::Protected;
			for (const asSAuthoredAccessPermission& Permission : Access.GetPermissions())
			{
				bool ReadOnly = false;
				bool EditDefaults = false;
				bool Inherited = false;
				for (const asSAccessPermissionModifier& Modifier : Permission.Modifiers)
				{
					ReadOnly |= Modifier.Kind == asEAccessPermissionModifier::ReadOnly;
					EditDefaults |= Modifier.Kind == asEAccessPermissionModifier::EditDefaults;
					Inherited |= Modifier.Kind == asEAccessPermissionModifier::Inherited;
				}
				if (Permission.Subject == asEAccessPermissionSubject::Wildcard)
				{
					Result.bAnyReadOnly |= ReadOnly;
					Result.bAnyEditDefaults |= EditDefaults;
					continue;
				}
				asSAccessPermission Converted;
				Converted.accessName = TCHAR_TO_UTF8(*Permission.Name);
				Converted.bReadOnly = ReadOnly;
				Converted.bEditDefaults = EditDefaults;
				Converted.bInherited = Inherited;
				Result.permissions.PushLast(Converted);
			}
			return Result;
		}
		eTokenType PrimitiveToken(asEPrimitiveType Primitive)
		{
			switch (Primitive)
			{
			case asEPrimitiveType::Void: return ttVoid;
			case asEPrimitiveType::Bool: return ttBool;
			case asEPrimitiveType::Int8: return ttInt8;
			case asEPrimitiveType::Int16: return ttInt16;
			case asEPrimitiveType::Int32: return ttInt;
			case asEPrimitiveType::Int64: return ttInt64;
			case asEPrimitiveType::UInt8: return ttUInt8;
			case asEPrimitiveType::UInt16: return ttUInt16;
			case asEPrimitiveType::UInt32: return ttUInt;
			case asEPrimitiveType::UInt64: return ttUInt64;
			case asEPrimitiveType::Float32: return ttFloat32;
			case asEPrimitiveType::Float64: return ttFloat64;
			default: return ttUnrecognizedToken;
			}
		}
	}

	asSDefinitionConsumerResult asCDefinitionConsumer::Consume(const asCCompilationSession& Session,
		TConstArrayView<std::shared_ptr<asCMetadataImage>> Dependencies) const
	{
		asSDefinitionConsumerResult Result;
		auto Fail = [&](asEDefinitionConsumerStatus Status, const asCASTNode* Node, const TCHAR* Message)
		{
			Result.Status = Status;
			Result.Diagnostics.Add({Node ? Node->GetSourceRange() : asCSourceRange{}, FString(Message)});
		};
		if (!Session.IsPublishable() || !Session.IsASTVerified())
		{
			Fail(asEDefinitionConsumerStatus::InvalidPhase, nullptr, TEXT("Definitions require the successful AST verification barrier."));
			return Result;
		}
		const auto& AST = Session.GetASTContext();
		const auto* TranslationUnit = Session.GetTranslationUnit();
		asSASTVerificationResult Verification;
		if (!TranslationUnit || !asCASTVerifier::Verify(AST, TranslationUnit, Verification))
		{
			Fail(asEDefinitionConsumerStatus::InvalidAST, TranslationUnit, TEXT("The verified declaration graph is no longer valid."));
			return Result;
		}
		const auto& TypeContext = AST.GetTypeContext();
		const auto& Settings = TypeContext->GetOptions();
		asSMetadataOptions Options;
		Options.FloatIsFloat64 = Settings.FloatIsFloat64;
		Options.ExpandDefaultArrayToTemplate = Settings.ExpandDefaultArrayToTemplate;
		Options.PointerByteSize = Settings.PointerByteSize;
		Options.TargetProfile = TCHAR_TO_UTF8(*Settings.TargetProfile);
		auto Image = asCMetadataImage::Create(Options);
		if (!Image || Image->SetIdentityContext(TypeContext) != asEMetadataResult::Succeeded)
		{
			Fail(asEDefinitionConsumerStatus::InvalidIdentity, TranslationUnit, TEXT("The definition image must retain the exact immutable TypeContext."));
			return Result;
		}
		for (const auto& Dependency : Dependencies)
		{
			if (Image->AddDependency(Dependency) != asEMetadataResult::Succeeded)
			{
				Fail(asEDefinitionConsumerStatus::InvalidDependency, TranslationUnit, TEXT("Definition dependencies must be compatible frozen images."));
				return Result;
			}
		}
		TArray<const asCDecl*> Declarations;
		TFunction<void(const asCDeclContext&)> Collect = [&](const asCDeclContext& Scope)
		{
			for (const asCDecl* Decl : Scope.GetDecls())
			{
				Declarations.Add(Decl);
				if (const auto* Namespace = dyn_cast<asCNamespaceDecl>(Decl)) Collect(*Namespace);
				else if (const auto* Record = dyn_cast<asCRecordDecl>(Decl)) Collect(*Record);
			}
		};
		Collect(*TranslationUnit);
		TMap<const asCDecl*, asCTypeInfo*> Types;
		TMap<const asCDecl*, asCScriptFunction*> Functions;
		TSet<asCScriptFunction*> DefinedSignatures;
		// Pass one publishes no edges: recursive names all have actual objects
		// before a field, base class or callable parameter can refer to them.
		for (const asCDecl* Decl : Declarations)
		{
			const auto* Named = dyn_cast<asCNamedDecl>(Decl);
			if (!Named || isa<asCNamespaceDecl>(Decl) || isa<asCVarDecl>(Decl) || isa<asCEnumConstantDecl>(Decl)
				|| isa<asCAccessSpecifierDecl>(Decl)) continue;
			const FString Namespace = NamespaceOf(*Decl);
			asEMetadataResult Status = asEMetadataResult::InvalidArgument;
			if (const auto* Record = dyn_cast<asCRecordDecl>(Decl))
			{
				if (Record->GetDeclaredTypeKind() == asETypeDeclKind::Enum)
				{
					asCEnumType* Type = nullptr;
					Status = Image->CreateEnumType(Named->GetStableIdentity(), TCHAR_TO_UTF8(*Named->GetName()), TCHAR_TO_UTF8(*Namespace), Type);
					if (Type) Types.Add(Decl, Type);
				}
				else
				{
					asCObjectType* Type = nullptr;
					const asQWORD Flags = asOBJ_SCRIPT_OBJECT | (Record->GetDeclaredTypeKind() == asETypeDeclKind::Struct ? asOBJ_VALUE : asOBJ_REF);
					Status = Image->CreateObjectType(Named->GetStableIdentity(), TCHAR_TO_UTF8(*Named->GetName()), TCHAR_TO_UTF8(*Namespace), Flags, Type);
					if (Type) Types.Add(Decl, Type);
					if (Status == asEMetadataResult::Succeeded && Record->GetDeclaredTypeKind() == asETypeDeclKind::Interface)
						Status = Image->SetInterfaceDeclaration(*Type);
				}
			}
			else if (const auto* Callable = dyn_cast<asCCallableTypeDecl>(Decl))
			{
				asCScriptFunction* Signature = nullptr;
				if (!Callable->GetResolvedSignature()) Status = asEMetadataResult::InvalidSignature;
				else Status = Image->CreateCallableSignature(Callable->GetResolvedSignature()->GetStableIdentity(), Signature);
				if (Status == asEMetadataResult::Succeeded)
				{
					asCCallableType* Type = nullptr;
					Status = Image->CreateCallableType(Named->GetStableIdentity(), TCHAR_TO_UTF8(*Named->GetName()), TCHAR_TO_UTF8(*Namespace), *Signature, Type);
					if (Type) Types.Add(Decl, Type);
					Functions.Add(Decl, Signature);
				}
			}
			else if (isa<asCFunctionDecl>(Decl))
			{
				asCScriptFunction* Function = nullptr;
				Status = Image->CreateFunction(Named->GetStableIdentity(), TCHAR_TO_UTF8(*Named->GetName()), TCHAR_TO_UTF8(*Namespace), asFUNC_SCRIPT, Function);
				if (Function) Functions.Add(Decl, Function);
			}
			else if (isa<asCTypeAliasDecl>(Decl))
			{
				asCTypedefType* Alias = nullptr;
				Status = Image->CreateTypedefType(Named->GetStableIdentity(), TCHAR_TO_UTF8(*Named->GetName()), TCHAR_TO_UTF8(*Namespace), Alias);
				if (Alias) Types.Add(Decl, Alias);
			}
			else
			{
				Fail(asEDefinitionConsumerStatus::UnsupportedDefinition, Decl, TEXT("This declaration has no maintained semantic definition form."));
				return Result;
			}
			if (Status != asEMetadataResult::Succeeded)
			{
				Fail(asEDefinitionConsumerStatus::InvalidDefinition, Decl, TEXT("Cannot create an identity-authenticated definition shell."));
				return Result;
			}
		}
		TFunction<asCTypeInfo*(const std::shared_ptr<asCMetadataImage>&, const asSStableKey&)> FindType =
			[&](const std::shared_ptr<asCMetadataImage>& Owner, const asSStableKey& Key) -> asCTypeInfo*
		{
			if (auto* Found = Owner->FindType(Key)) return Found;
			for (const auto& Dependency : Owner->GetDependencies())
				if (auto* Found = FindType(Dependency, Key)) return Found;
			return nullptr;
		};
		TFunction<bool(const asCQualType&, asCDataType&)> ResolveType = [&](const asCQualType& Input, asCDataType& Out)
		{
			if (!Input.IsValid() || Input.GetType()->IsInvalid() || !AST.Owns(Input.GetType())) return false;
			if (const auto* Builtin = dyn_cast<asCBuiltinType>(Input.GetType()))
			{
				const auto Token = PrimitiveToken(Builtin->GetPrimitive());
				if (Token == ttUnrecognizedToken) return false;
				Out = Image->Primitive(Token);
			}
			else if (const auto* Nominal = dyn_cast<asCNominalType>(Input.GetType()))
			{
				asCTypeInfo* Type = FindType(Image, Nominal->GetDeclarationIdentity());
				if (!Type) return false;
				Out = asCDataType::CreateType(Type, false);
			}
			else if (const auto* Generic = dyn_cast<asCGenericType>(Input.GetType()))
			{
				const auto* Template = dyn_cast<asCNominalType>(Generic->GetTemplateType());
				asCTypeInfo* Definition = Template ? FindType(Image, Template->GetDeclarationIdentity()) : nullptr;
				if (!Definition || !(Definition->flags & asOBJ_TEMPLATE) || (Definition->flags & asOBJ_CALLABLE_TYPE)) return false;
				TArray<asCDataType> Arguments;
				for (const auto& Argument : Generic->GetArguments())
				{
					asCDataType Converted;
					if (!ResolveType(Argument, Converted)) return false;
					Arguments.Add(Converted);
				}
				asCObjectType* Instance = nullptr;
				if (Image->CreateTemplateInstance(*static_cast<asCObjectType*>(Definition), Generic->GetStableIdentity(), Arguments, Instance) != asEMetadataResult::Succeeded) return false;
				Out = asCDataType::CreateType(Instance, false);
			}
			else return false; // Never reparse printed type names or infer host template layout.
			if (Input.IsHandle() && Out.MakeHandle(true, true) < 0) return false;
			if (Input.IsConst()) { if (Input.IsHandle()) Out.MakeHandleToConst(true); else Out.MakeReadOnly(true); }
			if (Input.IsReference()) Out.MakeReference(true);
			Out.SetMetadataNullable(Input.IsNullable());
			return true;
		};
		TMap<const asCAccessSpecifierDecl*, ::asSAccessSpecifier*> PublishedAccess;
		for (const asCDecl* Decl : Declarations)
		{
			const auto* Access = dyn_cast<asCAccessSpecifierDecl>(Decl);
			asCTypeInfo* OwnerType = Access ? Types.FindRef(RecordOwner(*Decl)) : nullptr;
			asCObjectType* Owner = OwnerType && (OwnerType->flags & (asOBJ_VALUE | asOBJ_REF)) && !(OwnerType->flags & asOBJ_CALLABLE_TYPE)
				? static_cast<asCObjectType*>(OwnerType) : nullptr;
			if (!Access) continue;
			::asSAccessSpecifier* Actual = nullptr;
			if (!Owner || Image->DefineAccessSpecifier(*Owner, ConvertAccess(*Access), Actual) != asEMetadataResult::Succeeded)
			{
				Fail(asEDefinitionConsumerStatus::InvalidDefinition, Decl, TEXT("A resolved access policy cannot form a valid detached definition."));
				return Result;
			}
			PublishedAccess.Add(Access, Actual);
		}
		// Pass two fills semantic relationships. All changes remain private to this
		// local image, so failure cannot leak a partial result to the caller.
		for (const asCDecl* Decl : Declarations)
		{
			asCTypeInfo* OwnerType = Types.FindRef(RecordOwner(*Decl));
			asCObjectType* Owner = OwnerType && (OwnerType->flags & (asOBJ_VALUE | asOBJ_REF)) && !(OwnerType->flags & asOBJ_CALLABLE_TYPE)
				? static_cast<asCObjectType*>(OwnerType) : nullptr;
			asEMetadataResult Status = asEMetadataResult::Succeeded;
			if (asCTypeInfo* Type = Types.FindRef(Decl))
			{
				const asETypeDeclKind Kind = isa<asCTypeAliasDecl>(Decl) ? asETypeDeclKind::Typedef :
					isa<asCRecordDecl>(Decl) ? cast<asCRecordDecl>(Decl)->GetDeclaredTypeKind() : cast<asCCallableTypeDecl>(Decl)->GetDeclaredTypeKind();
				Status = Image->SetTypeDeclarationFacts(*Type, Kind, 0, OwnerType);
				if (Status == asEMetadataResult::Succeeded && Owner && (Type->flags & asOBJ_CALLABLE_TYPE))
					Status = Image->AddChildCallableType(*Owner, *static_cast<asCCallableType*>(Type));
			}
			if (Status == asEMetadataResult::Succeeded)
				if (const auto* Record = dyn_cast<asCRecordDecl>(Decl))
				{
					auto* Type = Types.FindRef(Decl);
					for (const auto* Base : Record->GetResolvedBases())
					{
						asCQualType BaseQual;
						asCDataType BaseType;
						if (!asCQualType::TryCreate(const_cast<asCType*>(Base), asETypeQualifiers::None, BaseQual) ||
							!ResolveType(BaseQual, BaseType) || !BaseType.GetTypeInfo() ||
							!(BaseType.GetTypeInfo()->flags & (asOBJ_VALUE | asOBJ_REF)) || (BaseType.GetTypeInfo()->flags & asOBJ_CALLABLE_TYPE) ||
							!(Type->flags & (asOBJ_VALUE | asOBJ_REF)))
						{ Status = asEMetadataResult::InvalidSignature; break; }
						auto* BaseObject = static_cast<asCObjectType*>(BaseType.GetTypeInfo());
						Status = BaseObject->IsInterface() ? Image->AddInterface(*static_cast<asCObjectType*>(Type), *BaseObject) :
							Image->SetBaseType(*static_cast<asCObjectType*>(Type), *BaseObject);
						if (Status != asEMetadataResult::Succeeded) break;
					}
				}
			if (const auto* Variable = dyn_cast<asCVarDecl>(Decl))
			{
				asCDataType Type;
				asCObjectProperty* Property = nullptr;
				if (!ResolveType(Variable->GetResolvedQualType(), Type)) Status = asEMetadataResult::InvalidSignature;
				else if (Owner)
				{
					Status = Image->AddProperty(*Owner, TCHAR_TO_UTF8(*Variable->GetName()), Type, Property,
						Decl->GetAccess() == asEAccessSpecifier::Private, Decl->GetAccess() == asEAccessSpecifier::Protected);
					if (Status == asEMetadataResult::Succeeded && Variable->GetAuthoredAccessUse())
					{
						::asSAccessSpecifier* Policy = PublishedAccess.FindRef(Variable->GetAuthoredAccessUse()->Resolved);
						Status = Policy ? Image->SetPropertyAccess(*Owner, *Property, *Policy) : asEMetadataResult::InvalidSignature;
					}
				}
				else
				{
					asSStableKey TypeKey;
					asCGlobalProperty* Global = nullptr;
					if (Variable->GetResolvedQualType().GetStableIdentity(TypeContext->GetIdentityRegistry(), TypeKey) != asETypeIdentityStatus::Succeeded)
						Status = asEMetadataResult::InvalidSignature;
					else Status = Image->CreateGlobalProperty(Variable->GetStableIdentity(), TypeKey, TCHAR_TO_UTF8(*Variable->GetName()),
						TCHAR_TO_UTF8(*NamespaceOf(*Decl)), Type, Global);
				}
			}
			else if (const auto* Constant = dyn_cast<asCEnumConstantDecl>(Decl))
			{
				if (!OwnerType || !(OwnerType->flags & asOBJ_ENUM) || !Constant->HasResolvedValue() || Constant->GetValue() < MIN_int32 || Constant->GetValue() > MAX_int32)
					Status = asEMetadataResult::InvalidArgument;
				else Status = Image->AddEnumValue(*static_cast<asCEnumType*>(OwnerType), TCHAR_TO_UTF8(*Constant->GetName()), static_cast<int>(Constant->GetValue()));
			}
			else if (const auto* Alias = dyn_cast<asCTypeAliasDecl>(Decl))
			{
				asCDataType Target;
				if (!ResolveType(Alias->GetResolvedQualType(), Target)) Status = asEMetadataResult::InvalidSignature;
				else Status = Image->SetAlias(*static_cast<asCTypedefType*>(Types.FindRef(Decl)), Target);
			}
			if (asCScriptFunction* Function = Functions.FindRef(Decl))
			{
				const auto* FunctionDecl = dyn_cast<asCFunctionDecl>(Decl);
				const auto* CallableDecl = dyn_cast<asCCallableTypeDecl>(Decl);
				const auto* Signature = FunctionDecl ? FunctionDecl->GetResolvedSignature() : CallableDecl->GetResolvedSignature();
				const auto Parameters = FunctionDecl ? FunctionDecl->GetParameters() : CallableDecl->GetParameters();
				asCDataType ReturnType;
				TArray<asSMetadataParameter> Converted;
				if (!Signature || Signature->GetParameterTypes().Num() != Parameters.Num() ||
					Signature->GetParameterPassingModes().Num() != Parameters.Num() || !ResolveType(Signature->GetReturnType(), ReturnType))
					Status = asEMetadataResult::InvalidSignature;
				else for (int Index = 0; Index < Parameters.Num(); ++Index)
				{
					asSMetadataParameter Parameter;
					// Structural signatures have no authored parameter-name identity.
					// Nominal declarations retain authored names in AST/host output.
					Parameter.Name = TCHAR_TO_UTF8(*(CallableDecl ? FString::Printf(TEXT("arg%d"), Index) : Parameters[Index]->GetName()));
					if (!ResolveType(Signature->GetParameterTypes()[Index], Parameter.Type)) { Status = asEMetadataResult::InvalidSignature; break; }
					const auto Mode = Signature->GetParameterPassingModes()[Index];
					Parameter.Flags = Mode == asEParameterPassingMode::In ? asTM_INREF : Mode == asEParameterPassingMode::Out ? asTM_OUTREF :
						Mode == asEParameterPassingMode::InOut ? asTM_INOUTREF : asTM_NONE;
					if (FunctionDecl && Parameters[Index]->GetInitializer())
					{
						asCSourceManager Sources(AST.GetSourceSnapshot());
						TConstArrayView<uint8> Bytes;
						if (!Sources.TryGetBytes(Parameters[Index]->GetDeferredInitializerRange(), Bytes)) { Status = asEMetadataResult::InvalidSignature; break; }
						Parameter.DefaultArgument.Assign(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num());
					}
					Converted.Add(MoveTemp(Parameter));
				}
				if (Status == asEMetadataResult::Succeeded && !DefinedSignatures.Contains(Function))
				{
					Status = Image->DefineFunction(*Function, ReturnType, Converted, FunctionDecl ? Owner : nullptr);
					if (Status == asEMetadataResult::Succeeded && FunctionDecl)
					{
						Function->SetReadOnly(FunctionDecl->IsConstMethod());
						Function->SetFinal((uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::Final)) != 0);
						Function->SetOverride((uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::Override)) != 0);
						Function->SetProperty((uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::Property)) != 0);
						Function->SetExplicit((uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::Explicit)) != 0);
						Function->SetMixin((uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::Mixin)) != 0);
						Function->traits.SetTrait(asTRAIT_ACCEPT_TEMPORARY_OBJECT, (uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::AcceptTemporary)) != 0);
						Function->traits.SetTrait(asTRAIT_EXTERNAL_IMPLICIT_THIS, (uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::ExternalImplicitThis)) != 0);
						Function->traits.SetTrait(asTRAIT_NODISCARD, (uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::NoDiscard)) != 0);
						Function->traits.SetTrait(asTRAIT_ALLOWDISCARD, (uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::AllowDiscard)) != 0);
						Function->traits.SetTrait(asTRAIT_GENERATED_FUNCTION, (uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::Generated)) != 0);
						Function->traits.SetTrait(asTRAIT_DEPRECATED, (uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::Deprecated)) != 0);
						Function->traits.SetTrait(asTRAIT_DEFAULTS_ONLY, (uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::Defaults)) != 0);
						Function->traits.SetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION, (uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::UnsafeDuringConstruction)) != 0);
						Image->SetFunctionLocal(*Function, (uint16(FunctionDecl->GetModifiers()) & uint16(asEFunctionModifiers::Local)) != 0);
						Function->SetPrivate(Decl->GetAccess() == asEAccessSpecifier::Private);
						Function->SetProtected(Decl->GetAccess() == asEAccessSpecifier::Protected);
						if (Status == asEMetadataResult::Succeeded && FunctionDecl->GetAuthoredAccessUse())
						{
							::asSAccessSpecifier* Policy = PublishedAccess.FindRef(FunctionDecl->GetAuthoredAccessUse()->Resolved);
							Status = Policy ? Image->SetFunctionAccess(*Function, *Policy) : asEMetadataResult::InvalidSignature;
						}
						const auto Kind = FunctionDecl->GetFunctionKind() == asEFunctionKind::Constructor ? asEFunctionIdentityKind::Constructor :
							FunctionDecl->GetFunctionKind() == asEFunctionKind::Destructor ? asEFunctionIdentityKind::Destructor :
							FunctionDecl->GetFunctionKind() == asEFunctionKind::ClassDefault ? asEFunctionIdentityKind::ClassDefault :
							FunctionDecl->GetFunctionKind() == asEFunctionKind::Conversion ? asEFunctionIdentityKind::Conversion :
							Owner ? asEFunctionIdentityKind::Method : asEFunctionIdentityKind::Global;
						if (Status == asEMetadataResult::Succeeded) Status = Image->SetFunctionDeclarationFacts(*Function, Kind, Owner);
						if (Status == asEMetadataResult::Succeeded && Owner && (Kind == asEFunctionIdentityKind::Constructor || Kind == asEFunctionIdentityKind::Destructor))
							Status = Image->AddBehaviour(*Owner, Kind == asEFunctionIdentityKind::Constructor ? asBEHAVE_CONSTRUCT : asBEHAVE_DESTRUCT, *Function);
					}
					DefinedSignatures.Add(Function);
				}
			}
			if (Status != asEMetadataResult::Succeeded)
			{
				Fail(asEDefinitionConsumerStatus::InvalidDefinition, Decl, TEXT("A resolved declaration cannot form a valid detached semantic definition."));
				return Result;
			}
		}
		Result.Status = asEDefinitionConsumerStatus::Succeeded;
		Result.Image = MoveTemp(Image);
		return Result;
	}
END_AS_NAMESPACE
