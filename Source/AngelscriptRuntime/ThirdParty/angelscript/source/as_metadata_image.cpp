/* Maintained Unreal AngelScript fork — engine-independent definition ownership. */
#include "as_metadata_image.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_property.h"
#include "as_namespace.h"
#include "as_memory.h"
#include "Containers/StringConv.h"
#include "Containers/Set.h"
#include "frontend/as_type_context.h"

#include <functional>

BEGIN_AS_NAMESPACE

namespace
{
	FString QualifiedName(const char* Namespace, const char* Name)
	{
		return FString(UTF8_TO_TCHAR(Namespace)) + TEXT("::") + UTF8_TO_TCHAR(Name);
	}
	FString TypeNameKey(const asCTypeContext* Context, const asSStableKey& Key,
		const char* Namespace, const char* Name)
	{
		asSNominalTypeDescriptor Descriptor;
		if (Context) Context->GetIdentityRegistry().ResolveNominalType(Key, Descriptor);
		// Owner identity is distinct from the namespace; never flatten it into
		// the actual TypeInfo namespace merely to appease a legacy name index.
		return Descriptor.Owner.ToHexString() + TEXT("|") + QualifiedName(Namespace, Name);
	}
	bool ValidAlignment(asUINT Alignment)
	{
		return Alignment && !(Alignment & (Alignment - 1)) && Alignment <= MAX_int32;
	}
}

std::shared_ptr<asCMetadataImage> asCMetadataImage::Create(const asSMetadataOptions& Options)
{
	if ((Options.PointerByteSize != 4 && Options.PointerByteSize != 8) || !Options.TargetProfile.GetLength())
		return {};
	return std::shared_ptr<asCMetadataImage>(new asCMetadataImage(Options));
}

asCMetadataImage::asCMetadataImage(const asSMetadataOptions& InOptions) : Options(InOptions) {}

asCMetadataImage::~asCMetadataImage()
{
	// All semantic edges are borrowed from this graph or retained dependencies.
	// There are no external object leases once the shared control block reaches 0.
	bDestroying = true;
	for (asCGlobalProperty* Global : Globals) asDELETE(Global, asCGlobalProperty);
	for (asCScriptFunction* Function : Functions) asDELETE(Function, asCScriptFunction);
	for (asCTypeInfo* Type : Types) asDELETE(Type, asCTypeInfo);
	for (asSNameSpace* Namespace : Namespaces) asDELETE(Namespace, asSNameSpace);
}

bool asCMetadataImage::Owns(const asCTypeInfo* Type) const { return Type && Type->GetMetadataImage() == this; }
bool asCMetadataImage::Owns(const asCScriptFunction* Function) const { return Function && Function->GetMetadataImage() == this; }

bool asCMetadataImage::IsDefaultArrayType(const asCTypeInfo* Type) const
{
	return DefaultArrayType == Type;
}

bool asCMetadataImage::ReferencesImage(const asCMetadataImage* Image) const
{
	if (this == Image) return true;
	for (const auto& Dependency : Dependencies)
		if (Dependency->ReferencesImage(Image)) return true;
	return false;
}

bool asCMetadataImage::CanReference(const asCTypeInfo* Type) const
{
	return Type && Type->GetMetadataImage() && ReferencesImage(Type->GetMetadataImage());
}
bool asCMetadataImage::CanReference(const asCScriptFunction* Function) const
{
	return Function && Function->GetMetadataImage() && ReferencesImage(Function->GetMetadataImage());
}

asCTypeInfo* asCMetadataImage::FindType(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Found = TypesByKey.Find(Key);
	return Found ? *Found : nullptr;
}
asCScriptFunction* asCMetadataImage::FindFunction(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Found = FunctionsByKey.Find(Key);
	return Found ? *Found : nullptr;
}

asCGlobalProperty* asCMetadataImage::FindGlobalProperty(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	return GlobalsByKey.FindRef(Key);
}

asEMetadataResult asCMetadataImage::CreateGlobalProperty(const asSStableKey& Key,
	const asSStableKey& TypeUseKey, const char* Name, const char* Namespace,
	const asCDataType& Type, asCGlobalProperty*& Out)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	Out = nullptr;
	const auto Status = ValidateNew(Key, Name, Namespace, false);
	if (Status != asEMetadataResult::Succeeded) return Status;
	if (GlobalsByName.Contains(QualifiedName(Namespace, Name))) return asEMetadataResult::DuplicateName;
	if ((!Type.IsReadOnly() && !Type.IsHandleToConst()) || Type.IsReference() ||
		ValidateDataType(Type, false) != asEMetadataResult::Succeeded) return asEMetadataResult::InvalidSignature;
	asCGlobalProperty Candidate;
	asSNameSpace TemporaryNamespace;
	TemporaryNamespace.name = Namespace;
	Candidate.name = Name; Candidate.nameSpace = &TemporaryNamespace; Candidate.type = NormalizeOptions(Type);
	Candidate.metadataOwner = this; Candidate.memory = nullptr; Candidate.id = asUINT(-1);
	Candidate.metadataStableKey = Key; Candidate.metadataTypeUseKey = TypeUseKey;
	if (!ValidateGlobalIdentity(Candidate)) return asEMetadataResult::InvalidSignature;
	asCGlobalProperty* Global = asNEW(asCGlobalProperty);
	Global->name = Name; Global->nameSpace = InternNamespace(Namespace); Global->type = NormalizeOptions(Type);
	Global->metadataOwner = this; Global->metadataStableKey = Key; Global->metadataTypeUseKey = TypeUseKey;
	Global->id = asUINT(-1); Global->memory = nullptr; Global->refCount.set(0);
	Globals.Add(Global); GlobalsByKey.Add(Key, Global); GlobalsByName.Add(QualifiedName(Namespace, Name), Global);
	InvalidateLayouts();
	Out = Global;
	return asEMetadataResult::Succeeded;
}

bool asCMetadataImage::ValidateGlobalIdentity(const asCGlobalProperty& Global) const
{
if (!IdentityContext || Global.metadataOwner != this || !Global.nameSpace || Global.module || Global.initFunc || Global.realAddress || Global.memory || Global.memoryAllocated ||
		(!Global.type.IsReadOnly() && !Global.type.IsHandleToConst()) || Global.type.IsReference()) return false;
	const auto& Registry = IdentityContext->GetIdentityRegistry();
	asSDeclarationIdentityDescriptor Decl;
	if (Registry.ResolveDeclaration(Global.metadataStableKey, Decl) != asETypeIdentityStatus::Succeeded ||
		Decl.Kind != asEDeclarationIdentityKind::GlobalVariable || Decl.Name != UTF8_TO_TCHAR(Global.name.AddressOf())) return false;
	TArray<FString> Names;
	asSStableKey Owner = Decl.Owner;
	for (int Depth = 0; !Owner.IsZero(); ++Depth)
	{
		if (Depth >= 64) return false;
		asSDeclarationIdentityDescriptor Parent;
		if (Registry.ResolveDeclaration(Owner, Parent) != asETypeIdentityStatus::Succeeded ||
			Parent.Kind != asEDeclarationIdentityKind::Namespace || Parent.StableScope != Decl.StableScope) return false;
		Names.Insert(Parent.Name, 0); Owner = Parent.Owner;
	}
	return FString::Join(Names, TEXT("::")) == UTF8_TO_TCHAR(Global.nameSpace->name.AddressOf()) &&
		ValidateTypeUse(Global.metadataTypeUseKey, Global.type);
}

asCDataType asCMetadataImage::Primitive(eTokenType Token, bool IsConst) const
{
	asCDataType Type = asCDataType::CreatePrimitive(Token, IsConst, Options.FloatIsFloat64);
	Type.SetMetadataOptions(Options.FloatIsFloat64, Options.PointerByteSize);
	return Type;
}
asCDataType asCMetadataImage::NormalizeOptions(const asCDataType& Type) const
{
	asCDataType Copy(Type);
	Copy.SetMetadataOptions(Options.FloatIsFloat64, Options.PointerByteSize);
	return Copy;
}

asEMetadataResult asCMetadataImage::SetIdentityContext(TSharedPtr<const asCTypeContext, ESPMode::ThreadSafe> Context)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Context.IsValid()) return asEMetadataResult::InvalidArgument;
	const auto& Settings = Context->GetOptions();
	if (Settings.PointerByteSize != Options.PointerByteSize || Settings.FloatIsFloat64 != Options.FloatIsFloat64 ||
		Settings.ExpandDefaultArrayToTemplate != Options.ExpandDefaultArrayToTemplate || Settings.TargetProfile != UTF8_TO_TCHAR(Options.TargetProfile.AddressOf()))
		return asEMetadataResult::InvalidArgument;
	TMap<FString, asCTypeInfo*> ReboundNames;
	for (asCTypeInfo* Type : Types)
	{
		if (const auto* Object = CastToObjectType(Type); Object && Object->GetMetadataTemplateDefinition()) continue;
		const FString NameKey = TypeNameKey(Context.Get(), Type->GetStableKey(), Type->GetNamespace(), Type->GetName());
		if (ReboundNames.Contains(NameKey)) return asEMetadataResult::DuplicateName;
		ReboundNames.Add(NameKey, Type);
	}
	IdentityContext = MoveTemp(Context);
	TypesByName = MoveTemp(ReboundNames);
	return asEMetadataResult::Succeeded;
}

TSharedPtr<const asCTypeContext, ESPMode::ThreadSafe> asCMetadataImage::GetIdentityContext() const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	return IdentityContext;
}

asCScriptEngine* asCMetadataImage::GetBoundEngine() const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	return BoundEngine;
}

int asCMetadataImage::GetBoundTypeId(const asCTypeInfo* Type) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	const int* Id = BoundTypeIds.Find(Type);
	return BoundEngine && Owns(Type) && Id ? *Id : -1;
}

int asCMetadataImage::GetBoundFunctionId(const asCScriptFunction* Function) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	const int* Id = BoundFunctionIds.Find(Function);
	return BoundEngine && Owns(Function) && Id ? *Id : -1;
}

int asCMetadataImage::GetBoundDataTypeId(const asCDataType& Type) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (!BoundEngine) return -1;
	int Id = -1;
	if (Type.GetTypeInfo())
	{
		const int* Found = BoundTypeIds.Find(Type.GetTypeInfo());
		if (!Found) return -1;
		Id = *Found;
	}
	else
	{
		switch (Type.GetTokenType())
		{
		case ttVoid: Id = asTYPEID_VOID; break;
		case ttBool: Id = asTYPEID_BOOL; break;
		case ttInt8: Id = asTYPEID_INT8; break;
		case ttInt16: Id = asTYPEID_INT16; break;
		case ttInt: Id = asTYPEID_INT32; break;
		case ttInt64: Id = asTYPEID_INT64; break;
		case ttUInt8: Id = asTYPEID_UINT8; break;
		case ttUInt16: Id = asTYPEID_UINT16; break;
		case ttUInt: Id = asTYPEID_UINT32; break;
		case ttUInt64: Id = asTYPEID_UINT64; break;
		case ttFloat32: Id = asTYPEID_FLOAT32; break;
		case ttFloat64: Id = asTYPEID_FLOAT64; break;
		default: return -1;
		}
	}
	if (Type.IsObjectHandle()) Id |= asTYPEID_OBJHANDLE;
	if (Type.IsHandleToConst()) Id |= asTYPEID_HANDLETOCONST;
	return Id;
}

bool asCMetadataImage::ValidateFrozenLayout() const
{
	if (!bLayoutsFinalized || FrozenLayouts.Num() != Types.Num()) return false;
	int PropertyCount = 0;
	for (asCTypeInfo* Type : Types)
	{
		const auto* Layout = FrozenLayouts.Find(Type);
		const auto* Flags = FrozenTypeFlags.Find(Type);
		if (!Layout || !Flags || *Flags != Type->flags || Layout->Key != Type->size || Layout->Value != Type->alignment) return false;
		if (asCObjectType* Object = CastToObjectType(Type))
		{
			const auto* Policy = FrozenTemplateLayoutPolicies.Find(Object);
			if (!Policy || *Policy != Object->HasFixedTemplateLayout()) return false;
			for (asUINT Index = 0; Index < Object->localProperties.GetLength(); ++Index)
			{
				auto* Property = Object->localProperties[Index];
				const int* Offset = FrozenPropertyOffsets.Find(Property);
				const asCDataType* FrozenType = FrozenPropertyTypes.Find(Property);
				if (!Offset || *Offset != Property->byteOffset || !FrozenType || !(*FrozenType == Property->type)
					|| ValidateDataType(Property->type, false) != asEMetadataResult::Succeeded) return false;
				++PropertyCount;
			}
		}
	}
	return PropertyCount == FrozenPropertyOffsets.Num() && PropertyCount == FrozenPropertyTypes.Num();
}

bool asCMetadataImage::ValidateTypeUse(const asSStableKey& Key, const asCDataType& Actual, int Depth) const
{
if (!IdentityContext.IsValid()) return false;
	const auto& Registry = IdentityContext->GetIdentityRegistry();
	if (Depth > 64 || ValidateDataType(Actual, true) != asEMetadataResult::Succeeded) return false;
	asSStableTypeUseDescriptor Expected;
	if (Registry.ResolveTypeUse(Key, Expected) != asETypeIdentityStatus::Succeeded) return false;
	asSStableTypeUseDescriptor Observed = Expected;
	Observed.Qualifiers = asETypeUseQualifiers::None;
	if (Actual.IsReadOnly() || Actual.IsHandleToConst()) Observed.Qualifiers = Observed.Qualifiers | asETypeUseQualifiers::Const;
	if (Actual.IsReference()) Observed.Qualifiers = Observed.Qualifiers | asETypeUseQualifiers::Reference;
	if (Actual.IsObjectHandle()) Observed.Qualifiers = Observed.Qualifiers | asETypeUseQualifiers::Handle;
	if (Actual.IsMetadataNullable()) Observed.Qualifiers = Observed.Qualifiers | asETypeUseQualifiers::Nullable;
	if (asCTypeInfo* Type = Actual.GetTypeInfo())
	{
		Observed.Kind = asETypeUseKind::Nominal;
		asCObjectType* Object = CastToObjectType(Type);
		Observed.Declaration = Object && Object->GetMetadataTemplateDefinition() ? Object->GetMetadataTemplateDefinition()->GetStableKey() : Type->GetStableKey();
		const int Count = Object ? Object->templateSubTypes.GetLength() : 0;
		if (Expected.Arguments.Num() != Count) return false;
		for (int Index = 0; Index < Count; ++Index)
			if (!ValidateTypeUse(Expected.Arguments[Index], Object->templateSubTypes[Index], Depth + 1)) return false;
	}
	else
	{
		Observed.Kind = asETypeUseKind::Primitive;
		switch (Actual.GetTokenType())
		{
		case ttVoid: Observed.Primitive = asEPrimitiveType::Void; break;
		case ttBool: Observed.Primitive = asEPrimitiveType::Bool; break;
		case ttInt8: Observed.Primitive = asEPrimitiveType::Int8; break;
		case ttInt16: Observed.Primitive = asEPrimitiveType::Int16; break;
		case ttInt: Observed.Primitive = asEPrimitiveType::Int32; break;
		case ttInt64: Observed.Primitive = asEPrimitiveType::Int64; break;
		case ttUInt8: Observed.Primitive = asEPrimitiveType::UInt8; break;
		case ttUInt16: Observed.Primitive = asEPrimitiveType::UInt16; break;
		case ttUInt: Observed.Primitive = asEPrimitiveType::UInt32; break;
		case ttUInt64: Observed.Primitive = asEPrimitiveType::UInt64; break;
		case ttFloat32: Observed.Primitive = asEPrimitiveType::Float32; break;
		case ttFloat64: Observed.Primitive = asEPrimitiveType::Float64; break;
		default: return false;
		}
	}
	return Registry.ValidateTypeUse(Key, Observed) == asETypeIdentityStatus::Succeeded;
}

bool asCMetadataImage::ValidateFrozenIdentity() const
{
if (!IdentityContext.IsValid()) return false;
	if (!ValidateListInitializers()) return false;
	const auto& Registry = IdentityContext->GetIdentityRegistry();
	auto NamespacePath = [](const asSNameSpace* Namespace)
	{
		TArray<FString> Path;
		if (Namespace) FString(UTF8_TO_TCHAR(Namespace->name.AddressOf())).ParseIntoArray(Path, TEXT("::"), true);
		return Path;
	};
	for (const asCGlobalProperty* Global : Globals)
		if (!ValidateGlobalIdentity(*Global)) return false;
	for (asCTypeInfo* Type : Types)
	{
		if (asCObjectType* Instance = CastToObjectType(Type); Instance && Instance->GetMetadataTemplateDefinition())
		{
			const auto* Template = Instance->GetMetadataTemplateDefinition();
			asSStableTypeUseDescriptor Use;
			if (!CanReference(Template) || !Template->HasFixedTemplateLayout() ||
				Registry.ResolveTypeUse(Instance->GetStableKey(), Use) != asETypeIdentityStatus::Succeeded ||
				Use.Qualifiers != asETypeUseQualifiers::None || Use.Declaration != Template->GetStableKey() ||
				Instance->name != Template->name || Instance->nameSpace->name != Template->nameSpace->name ||
				!ValidateTypeUse(Instance->GetStableKey(), asCDataType::CreateType(Instance, false))) return false;
			continue;
		}
		asSNominalTypeDescriptor Descriptor;
		if (Registry.ResolveNominalType(Type->GetStableKey(), Descriptor) != asETypeIdentityStatus::Succeeded) return false;
		Descriptor.Name = UTF8_TO_TCHAR(Type->name.AddressOf());
		Descriptor.NamespacePath = NamespacePath(Type->nameSpace);
		Descriptor.Owner = Type->metadataDeclarationOwner ? Type->metadataDeclarationOwner->GetStableKey() : asSStableKey{};
		Descriptor.Kind = static_cast<asETypeDeclKind>(Type->metadataDeclarationKind);
		Descriptor.GenericArity = Type->metadataGenericArity;
		if (Registry.ValidateNominalType(Type->GetStableKey(), Descriptor) != asETypeIdentityStatus::Succeeded) return false;
	}
	for (asCScriptFunction* Function : Functions)
	{
		if (!Function->metadataSignatureDefined || Function->inOutFlags.GetLength() != Function->parameterTypes.GetLength() ||
			Function->parameterNames.GetLength() != Function->parameterTypes.GetLength() || Function->defaultArgs.GetLength() != Function->parameterTypes.GetLength()) return false;
		if (Function->metadataCallableSignature)
		{
			asSStableTypeUseDescriptor Signature;
			if (Function->funcType != asFUNC_FUNCDEF || Function->objectType || Function->metadataDeclarationOwner ||
				Registry.ResolveTypeUse(Function->GetStableKey(), Signature) != asETypeIdentityStatus::Succeeded ||
				Signature.Kind != asETypeUseKind::Callable || Signature.Qualifiers != asETypeUseQualifiers::None ||
				Signature.Parameters.Num() != int(Function->parameterTypes.GetLength()) ||
				!ValidateTypeUse(Signature.ReturnType, Function->returnType, 0)) return false;
			for (int Index = 0; Index < Signature.Parameters.Num(); ++Index)
			{
				const asDWORD Mode = static_cast<asDWORD>(Function->inOutFlags[Index]);
				if (Mode > asTM_INOUTREF || (Mode != asTM_NONE && !Function->parameterTypes[Index].IsReference()) ||
					!ValidateTypeUse(Signature.Parameters[Index].Type, Function->parameterTypes[Index], 0)) return false;
				Signature.Parameters[Index].PassingMode = Mode == asTM_INREF ? asEParameterPassingMode::In :
					Mode == asTM_OUTREF ? asEParameterPassingMode::Out : Mode == asTM_INOUTREF ? asEParameterPassingMode::InOut : asEParameterPassingMode::Value;
			}
			if (Registry.ValidateTypeUse(Function->GetStableKey(), Signature) != asETypeIdentityStatus::Succeeded) return false;
			continue;
		}
		asSFunctionIdentityDescriptor Descriptor;
		if (Registry.ResolveFunction(Function->GetStableKey(), Descriptor) != asETypeIdentityStatus::Succeeded) return false;
		Descriptor.Name = UTF8_TO_TCHAR(Function->name.AddressOf());
		Descriptor.NamespacePath = NamespacePath(Function->nameSpace);
		Descriptor.Owner = Function->metadataDeclarationOwner ? Function->metadataDeclarationOwner->GetStableKey() : asSStableKey{};
		Descriptor.Kind = static_cast<asEFunctionIdentityKind>(Function->metadataDeclarationKind);
		if (Function->traits.GetTrait(asTRAIT_CONSTRUCTOR) != (Descriptor.Kind == asEFunctionIdentityKind::Constructor) ||
			Function->traits.GetTrait(asTRAIT_DESTRUCTOR) != (Descriptor.Kind == asEFunctionIdentityKind::Destructor)) return false;
		Descriptor.EnclosingFunction = Function->metadataEnclosingFunction ? Function->metadataEnclosingFunction->GetStableKey() : asSStableKey{};
		Descriptor.LambdaOrdinal = Function->metadataLambdaOrdinal;
		if (Descriptor.Kind == asEFunctionIdentityKind::Lambda &&
			(!Owns(Function->metadataEnclosingFunction) || Function->metadataEnclosingFunction == Function)) return false;
		Descriptor.GenericArity = Function->metadataGenericArity;
		Descriptor.DefinitionModifiers = Function->metadataStaticDeclaration ? asEFunctionDefinitionModifiers::Static : asEFunctionDefinitionModifiers::None;
		if (Function->IsFinal()) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::Final;
		if (Function->IsOverride()) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::Override;
		if (Function->IsProperty()) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::Property;
		if (Function->traits.GetTrait(asTRAIT_EXPLICIT)) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::Explicit;
		if (Function->IsMixin()) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::Mixin;
		if (Function->IsMetadataLocal()) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::Local;
		if (Function->traits.GetTrait(asTRAIT_ACCEPT_TEMPORARY_OBJECT)) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::AcceptTemporary;
		if (Function->traits.GetTrait(asTRAIT_EXTERNAL_IMPLICIT_THIS)) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::ExternalImplicitThis;
		if (Function->traits.GetTrait(asTRAIT_NODISCARD)) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::NoDiscard;
		if (Function->traits.GetTrait(asTRAIT_ALLOWDISCARD)) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::AllowDiscard;
		if (Function->traits.GetTrait(asTRAIT_GENERATED_FUNCTION)) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::Generated;
		if (Function->traits.GetTrait(asTRAIT_DEPRECATED)) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::Deprecated;
		if (Function->traits.GetTrait(asTRAIT_DEFAULTS_ONLY)) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::Defaults;
		if (Function->traits.GetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION)) Descriptor.DefinitionModifiers = Descriptor.DefinitionModifiers | asEFunctionDefinitionModifiers::UnsafeDuringConstruction;
		if ((Descriptor.Kind == asEFunctionIdentityKind::Method || Descriptor.Kind == asEFunctionIdentityKind::Conversion)
			&& (static_cast<asCTypeInfo*>(Function->objectType) != Function->metadataDeclarationOwner
				|| !Function->objectType || !CanReference(Function->objectType))) return false;
		Descriptor.Modifiers = Function->IsReadOnly() ? asEFunctionIdentityModifiers::ConstMethod : asEFunctionIdentityModifiers::None;
		if (Descriptor.Parameters.Num() != int(Function->parameterTypes.GetLength()) || !ValidateTypeUse(Descriptor.ReturnType, Function->returnType, 0)) return false;
		for (int Index = 0; Index < Descriptor.Parameters.Num(); ++Index)
		{
			if (static_cast<asDWORD>(Function->inOutFlags[Index]) > asTM_INOUTREF ||
				(Function->inOutFlags[Index] != asTM_NONE && !Function->parameterTypes[Index].IsReference())) return false;
			if (!ValidateTypeUse(Descriptor.Parameters[Index].Type, Function->parameterTypes[Index], 0)) return false;
			const auto Mode = Function->inOutFlags[Index] == asTM_INREF ? asEParameterPassingMode::In :
				Function->inOutFlags[Index] == asTM_OUTREF ? asEParameterPassingMode::Out :
				Function->inOutFlags[Index] == asTM_INOUTREF ? asEParameterPassingMode::InOut : asEParameterPassingMode::Value;
			Descriptor.Parameters[Index].PassingMode = Mode;
		}
		if (Registry.ValidateFunction(Function->GetStableKey(), Descriptor) != asETypeIdentityStatus::Succeeded) return false;
	}
	return true;
}

bool asCMetadataImage::ValidateFrozenDefinitions() const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	return IsFrozen() && ValidateFrozenLayout() && ValidateFrozenIdentity() && ValidateAccessSnapshot();
}

asEMetadataResult asCMetadataImage::ValidateDataType(const asCDataType& Type, bool AllowVoid) const
{
	if (!Type.IsValid() || Type.IsAuto() || Type.IsUnresolvedObject() || Type.IsErrorDataType() || Type.IsNullHandle())
		return asEMetadataResult::InvalidSignature;
	if (Type.GetTypeInfo())
		return CanReference(Type.GetTypeInfo()) ? asEMetadataResult::Succeeded : asEMetadataResult::ForeignOwner;
	if (Type.GetTokenType() == ttVoid)
		return AllowVoid && !Type.IsReference() ? asEMetadataResult::Succeeded : asEMetadataResult::InvalidSignature;
	switch (Type.GetTokenType())
	{
	case ttBool: case ttInt8: case ttUInt8: case ttInt16: case ttUInt16:
	case ttInt: case ttUInt: case ttInt64: case ttUInt64:
	case ttFloat32: case ttFloat64: case ttFloat: case ttDouble:
		return asEMetadataResult::Succeeded;
	default: return asEMetadataResult::InvalidSignature;
	}
}

asEMetadataResult asCMetadataImage::ValidateNew(const asSStableKey& Key,
	const char* Name, const char* Namespace, bool IsType) const
{
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (Key.IsZero() || !Name || !Namespace) return asEMetadataResult::InvalidArgument;
	if (!*Name)
	{
		asSFunctionIdentityDescriptor Identity;
		if (IsType || !IdentityContext.IsValid() ||
			IdentityContext->GetIdentityRegistry().ResolveFunction(Key, Identity) != asETypeIdentityStatus::Succeeded ||
			(Identity.Kind != asEFunctionIdentityKind::Lambda && Identity.Kind != asEFunctionIdentityKind::ClassDefault))
			return asEMetadataResult::InvalidArgument;
	}
	if (TypesByKey.Contains(Key) || FunctionsByKey.Contains(Key) || GlobalsByKey.Contains(Key)) return asEMetadataResult::DuplicateIdentity;
	if (IsType && TypesByName.Contains(TypeNameKey(IdentityContext.Get(), Key, Namespace, Name))) return asEMetadataResult::DuplicateName;
	// The producer must retain its canonical identity registry/context. Nonzero
	// is a shell precondition, not authentication of an arbitrary digest; Engine
	// registration must validate the image against that producer-owned context.
	return asEMetadataResult::Succeeded;
}

asSNameSpace* asCMetadataImage::InternNamespace(const char* Name)
{
	for (asSNameSpace* Namespace : Namespaces)
		if (Namespace->name == Name) return Namespace;
	asSNameSpace* Namespace = asNEW(asSNameSpace);
	Namespace->name = Name;
	Namespaces.Add(Namespace);
	return Namespace;
}

asEMetadataResult asCMetadataImage::AddDependency(const std::shared_ptr<asCMetadataImage>& Dependency)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Dependency || Dependency.get() == this) return asEMetadataResult::InvalidArgument;
	if (!Dependency->IsFrozen()) return asEMetadataResult::UnfrozenDependency;
	if (Dependency->Options.PointerByteSize != Options.PointerByteSize || Dependency->Options.TargetProfile != Options.TargetProfile)
		return asEMetadataResult::InvalidLayout;
	if (Dependency->ReferencesImage(this)) return asEMetadataResult::InvalidArgument;
	for (const auto& Existing : Dependencies) if (Existing == Dependency) return asEMetadataResult::Succeeded;
	Dependencies.Add(Dependency);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::CreateObjectType(const asSStableKey& Key,
	const char* Name, const char* Namespace, asQWORD Flags, asCObjectType*& Out)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Result = ValidateNew(Key, Name, Namespace, true);
	if (Result != asEMetadataResult::Succeeded) return Result;
	if ((Flags & (asOBJ_ENUM | asOBJ_TYPEDEF | asOBJ_FUNCDEF | asOBJ_LIST_PATTERN)) ||
		!(Flags & (asOBJ_VALUE | asOBJ_REF)) || ((Flags & asOBJ_VALUE) && (Flags & asOBJ_REF)))
		return asEMetadataResult::InvalidArgument;
	Out = asNEW(asCObjectType)(*this, Key);
	Out->name = Name; Out->nameSpace = InternNamespace(Namespace); Out->flags = Flags;
	Out->metadataDeclarationKind = static_cast<asBYTE>((Flags & asOBJ_VALUE) ? asETypeDeclKind::Struct : asETypeDeclKind::Class);
	Out->size = -1; Out->alignment = 1;
	Types.Add(Out); TypesByKey.Add(Key, Out); TypesByName.Add(TypeNameKey(IdentityContext.Get(), Key, Namespace, Name), Out);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::CreateEnumType(const asSStableKey& Key,
	const char* Name, const char* Namespace, asCEnumType*& Out)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Result = ValidateNew(Key, Name, Namespace, true);
	if (Result != asEMetadataResult::Succeeded) return Result;
	Out = asNEW(asCEnumType)(*this, Key);
	Out->name = Name; Out->nameSpace = InternNamespace(Namespace); Out->flags = asOBJ_ENUM;
	Out->metadataDeclarationKind = static_cast<asBYTE>(asETypeDeclKind::Enum);
	Out->size = 4; Out->alignment = 4;
	Types.Add(Out); TypesByKey.Add(Key, Out); TypesByName.Add(TypeNameKey(IdentityContext.Get(), Key, Namespace, Name), Out);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::CreateTypedefType(const asSStableKey& Key,
	const char* Name, const char* Namespace, asCTypedefType*& Out)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Result = ValidateNew(Key, Name, Namespace, true);
	if (Result != asEMetadataResult::Succeeded) return Result;
	Out = asNEW(asCTypedefType)(*this, Key);
	Out->name = Name; Out->nameSpace = InternNamespace(Namespace); Out->flags = asOBJ_TYPEDEF;
	Out->metadataDeclarationKind = static_cast<asBYTE>(asETypeDeclKind::Typedef);
	Out->size = -1; Out->alignment = 1;
	Types.Add(Out); TypesByKey.Add(Key, Out); TypesByName.Add(TypeNameKey(IdentityContext.Get(), Key, Namespace, Name), Out);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::CreateFuncdefType(const asSStableKey& Key,
	const char* Name, const char* Namespace, asCScriptFunction& Signature, asCFuncdefType*& Out)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Result = ValidateNew(Key, Name, Namespace, true);
	if (Result != asEMetadataResult::Succeeded) return Result;
	if (!Owns(&Signature)) return asEMetadataResult::ForeignOwner;
	if (Signature.funcType != asFUNC_FUNCDEF || (!Signature.metadataCallableSignature && Signature.metadataNominalFuncdefType))
		return asEMetadataResult::InvalidSignature;
	Out = asNEW(asCFuncdefType)(*this, Key, &Signature);
	if (!Signature.metadataCallableSignature) Signature.metadataNominalFuncdefType = Out;
	Out->metadataDeclarationKind = static_cast<asBYTE>(asETypeDeclKind::Funcdef);
	Out->name = Name; Out->nameSpace = InternNamespace(Namespace);
	Out->size = Options.PointerByteSize; Out->alignment = Options.PointerByteSize;
	Types.Add(Out); TypesByKey.Add(Key, Out); TypesByName.Add(TypeNameKey(IdentityContext.Get(), Key, Namespace, Name), Out);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::CreateFunction(const asSStableKey& Key,
	const char* Name, const char* Namespace, asEFuncType Kind, asCScriptFunction*& Out)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Result = ValidateNew(Key, Name, Namespace, false);
	if (Result != asEMetadataResult::Succeeded) return Result;
	// A delegate instance is a runtime allocation with GC/callable state, not a
	// function declaration. Funcdef signatures describe delegate/event types.
	if (Kind != asFUNC_SCRIPT && Kind != asFUNC_SYSTEM && Kind != asFUNC_FUNCDEF)
		return asEMetadataResult::InvalidArgument;
	Out = asNEW(asCScriptFunction)(*this, Key, Kind);
	Out->name = Name; Out->nameSpace = InternNamespace(Namespace);
	Out->returnType = Primitive(ttVoid);
	Functions.Add(Out); FunctionsByKey.Add(Key, Out);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::CreateCallableSignature(const asSStableKey& SignatureKey, asCScriptFunction*& Out)
{
Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!IdentityContext.IsValid()) return asEMetadataResult::InvalidArgument;
	asSStableTypeUseDescriptor Descriptor;
	if (IdentityContext->GetIdentityRegistry().ResolveTypeUse(SignatureKey, Descriptor) != asETypeIdentityStatus::Succeeded ||
		Descriptor.Kind != asETypeUseKind::Callable || Descriptor.Qualifiers != asETypeUseQualifiers::None)
		return asEMetadataResult::InvalidSignature;
	if (asCScriptFunction* Existing = FunctionsByKey.FindRef(SignatureKey))
	{
		if (!Existing->metadataCallableSignature) return asEMetadataResult::DuplicateIdentity;
		Out = Existing;
		return asEMetadataResult::Succeeded;
	}
	const auto Result = ValidateNew(SignatureKey, "$signature", "", false);
	if (Result != asEMetadataResult::Succeeded) return Result;
	Out = asNEW(asCScriptFunction)(*this, SignatureKey, asFUNC_FUNCDEF);
	Out->metadataCallableSignature = true;
	Out->name = "$signature";
	Out->nameSpace = InternNamespace("");
	Out->returnType = Primitive(ttVoid);
	Functions.Add(Out);
	FunctionsByKey.Add(SignatureKey, Out);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::DefineFunction(asCScriptFunction& Function,
	const asCDataType& ReturnType, TConstArrayView<asSMetadataParameter> Parameters, asCObjectType* Owner)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Function) || (Owner && !Owns(Owner))) return asEMetadataResult::ForeignOwner;
	if (Function.metadataSignatureDefined) return asEMetadataResult::InvalidState;
	auto Result = ValidateDataType(ReturnType, true);
	if (Result != asEMetadataResult::Succeeded) return Result;
	bool HasDefault = false;
	for (const auto& Parameter : Parameters)
	{
		Result = ValidateDataType(Parameter.Type, false);
		if (Result != asEMetadataResult::Succeeded) return Result;
		if (Parameter.Flags > asTM_INOUTREF || (Parameter.Flags != asTM_NONE && !Parameter.Type.IsReference()))
			return asEMetadataResult::InvalidSignature;
		if (HasDefault && !Parameter.DefaultArgument.GetLength()) return asEMetadataResult::InvalidSignature;
		HasDefault |= Parameter.DefaultArgument.GetLength() != 0;
	}
	// Validation above is transactional: a rejected parameter must not attach a
	// partial method to its owner or change the previous uncompleted signature.
	Function.returnType = NormalizeOptions(ReturnType);
	Function.objectType = Owner;
	Function.metadataDeclarationOwner = Owner;
	Function.metadataDeclarationKind = static_cast<asBYTE>(Owner ? asEFunctionIdentityKind::Method : asEFunctionIdentityKind::Global);
	for (const auto& Parameter : Parameters)
	{
		Function.parameterTypes.PushLast(NormalizeOptions(Parameter.Type));
		Function.parameterNames.PushLast(Parameter.Name);
		Function.inOutFlags.PushLast(static_cast<asETypeModifiers>(Parameter.Flags));
		Function.defaultArgs.PushLast(Parameter.DefaultArgument.GetLength() ? asNEW(asCString)(Parameter.DefaultArgument) : nullptr);
	}
	if (Owner)
	{
		Owner->metadataMethods.PushLast(&Function);
		Owner->methodTable.Add(&Function);
	}
	Function.metadataSignatureDefined = true;
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::SetTypeDeclarationFacts(asCTypeInfo& Type, asETypeDeclKind Kind,
	asUINT GenericArity, asCTypeInfo* Owner)
{
std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type) || (Owner && !CanReference(Owner))) return asEMetadataResult::ForeignOwner;
	if (GenericArity > 1024 || Owner == &Type) return asEMetadataResult::InvalidArgument;
	bool ValidKind = false;
	if (CastToEnumType(&Type)) ValidKind = Kind == asETypeDeclKind::Enum;
	else if (CastToTypedefType(&Type)) ValidKind = Kind == asETypeDeclKind::Typedef;
	else if (CastToFuncdefType(&Type)) ValidKind = Kind == asETypeDeclKind::Funcdef || Kind == asETypeDeclKind::Delegate || Kind == asETypeDeclKind::Event;
	else if (auto* Object = CastToObjectType(&Type))
		ValidKind = Kind == (Object->IsInterface() ? asETypeDeclKind::Interface : (Object->flags & asOBJ_VALUE) ? asETypeDeclKind::Struct : asETypeDeclKind::Class);
	if (!ValidKind) return asEMetadataResult::InvalidArgument;
	Type.metadataDeclarationKind = static_cast<asBYTE>(Kind);
	Type.metadataGenericArity = GenericArity;
	Type.metadataDeclarationOwner = Owner;
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::SetFunctionDeclarationFacts(asCScriptFunction& Function, asEFunctionIdentityKind Kind,
	asCTypeInfo* Owner, asUINT GenericArity, bool IsStatic)
{
std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Function) || (Owner && !CanReference(Owner))) return asEMetadataResult::ForeignOwner;
	if (!Function.metadataSignatureDefined || GenericArity > 1024 || Kind <= asEFunctionIdentityKind::Invalid || Kind > asEFunctionIdentityKind::Conversion)
		return asEMetadataResult::InvalidArgument;
	if ((Kind == asEFunctionIdentityKind::Global || Kind == asEFunctionIdentityKind::Lambda) != (Owner == nullptr)) return asEMetadataResult::InvalidArgument;
	if (Kind == asEFunctionIdentityKind::Lambda && !Function.metadataEnclosingFunction) return asEMetadataResult::InvalidArgument;
	if (Kind == asEFunctionIdentityKind::ClassDefault &&
		(Function.returnType.GetTokenType() != ttVoid || Function.returnType.GetTypeInfo() || Function.returnType.IsReference() ||
		 Function.parameterTypes.GetLength() != 0 || GenericArity || IsStatic)) return asEMetadataResult::InvalidSignature;
	if (Kind == asEFunctionIdentityKind::Conversion &&
		(Function.parameterTypes.GetLength() != 0 || Function.returnType.GetTokenType() == ttVoid
			|| Function.returnType.IsReference() || GenericArity || IsStatic))
		return asEMetadataResult::InvalidSignature;
	if (Kind == asEFunctionIdentityKind::Factory)
	{
		if (Function.objectType || Function.returnType.GetTypeInfo() != Owner || !Function.returnType.IsObjectHandle()) return asEMetadataResult::InvalidSignature;
	}
	else if (Owner != Function.objectType) return asEMetadataResult::InvalidSignature;
	if ((Kind == asEFunctionIdentityKind::Constructor || Kind == asEFunctionIdentityKind::Destructor) &&
		(Function.returnType.GetTokenType() != ttVoid || Function.returnType.IsReference() || Function.returnType.GetTypeInfo() ||
		 (Kind == asEFunctionIdentityKind::Destructor && Function.parameterTypes.GetLength() != 0)))
		return asEMetadataResult::InvalidSignature;
	// Special declarations are represented by the behavior table, never disguised as ordinary methods.
	// Conversion methods remain in the method table; only their identity kind is Conversion.
	const bool bMethodLike = Kind == asEFunctionIdentityKind::Method || Kind == asEFunctionIdentityKind::Conversion;
	const bool bWasMethodLike = Function.metadataDeclarationKind == static_cast<asBYTE>(asEFunctionIdentityKind::Method)
		|| Function.metadataDeclarationKind == static_cast<asBYTE>(asEFunctionIdentityKind::Conversion);
	if (Function.objectType && !bMethodLike)
	{
		Function.objectType->metadataMethods.RemoveValue(&Function);
		Function.objectType->methodTable.Remove(&Function);
	}
	else if (Function.objectType && !bWasMethodLike)
	{
		Function.objectType->metadataMethods.PushLast(&Function);
		Function.objectType->methodTable.Add(&Function);
	}
	Function.metadataDeclarationKind = static_cast<asBYTE>(Kind);
	Function.traits.SetTrait(asTRAIT_CONSTRUCTOR, Kind == asEFunctionIdentityKind::Constructor);
	Function.traits.SetTrait(asTRAIT_DESTRUCTOR, Kind == asEFunctionIdentityKind::Destructor);
	Function.metadataGenericArity = GenericArity;
	Function.metadataDeclarationOwner = Owner;
	Function.metadataStaticDeclaration = IsStatic;
	if (Kind != asEFunctionIdentityKind::Lambda) { Function.metadataEnclosingFunction = nullptr; Function.metadataLambdaOrdinal = 0; }
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::SetFunctionLocal(asCScriptFunction& Function, bool IsLocal)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Function)) return asEMetadataResult::ForeignOwner;
	Function.traits.SetTrait(asTRAIT_LOCAL, IsLocal);
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::SetLambdaOrigin(asCScriptFunction& Function, asCScriptFunction& EnclosingFunction, asUINT Ordinal)
{
std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Function) || !Owns(&EnclosingFunction)) return asEMetadataResult::ForeignOwner;
	if (&Function == &EnclosingFunction || !Function.metadataSignatureDefined || Function.objectType || !IdentityContext.IsValid())
		return asEMetadataResult::InvalidArgument;
	asSFunctionIdentityDescriptor Descriptor;
	if (IdentityContext->GetIdentityRegistry().ResolveFunction(Function.GetStableKey(), Descriptor) != asETypeIdentityStatus::Succeeded ||
		Descriptor.Kind != asEFunctionIdentityKind::Lambda || Descriptor.EnclosingFunction != EnclosingFunction.GetStableKey() ||
		Descriptor.LambdaOrdinal != Ordinal || Function.name.GetLength() != 0)
		return asEMetadataResult::InvalidSignature;
	Function.metadataDeclarationKind = static_cast<asBYTE>(asEFunctionIdentityKind::Lambda);
	Function.metadataDeclarationOwner = nullptr;
	Function.metadataGenericArity = 0;
	Function.metadataStaticDeclaration = false;
	Function.metadataEnclosingFunction = &EnclosingFunction;
	Function.metadataLambdaOrdinal = Ordinal;
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::AddProperty(asCObjectType& Owner, const char* Name,
	const asCDataType& Type, asCObjectProperty*& Out, bool IsPrivate, bool IsProtected)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Owner)) return asEMetadataResult::ForeignOwner;
	if (!Name || !*Name || Owner.IsInterface()) return asEMetadataResult::InvalidArgument;
	auto Result = ValidateDataType(Type, false);
	if (Result != asEMetadataResult::Succeeded) return Result;
	for (asUINT Index = 0; Index < Owner.localProperties.GetLength(); ++Index)
		if (Owner.localProperties[Index]->name == Name) return asEMetadataResult::DuplicateName;
	Out = asNEW(asCObjectProperty);
	Out->name = Name; Out->type = NormalizeOptions(Type); Out->isPrivate = IsPrivate; Out->isProtected = IsProtected;
	Owner.localProperties.PushLast(Out); Owner.properties.PushLast(Out); Owner.propertyTable.Add(Out);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::SetBaseType(asCObjectType& Type, asCObjectType& Base)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type) || !CanReference(&Base)) return asEMetadataResult::ForeignOwner;
	if (&Type == &Base || Type.derivedFrom || Base.IsInterface()) return asEMetadataResult::InvalidArgument;
	TSet<asCObjectType*> Visited;
	for (asCObjectType* Ancestor = &Base; Ancestor; Ancestor = Ancestor->derivedFrom)
	{
		if (Ancestor == &Type || Visited.Contains(Ancestor)) return asEMetadataResult::RecursiveValueType;
		Visited.Add(Ancestor);
	}
	Type.derivedFrom = &Base;
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::SetInterfaceDeclaration(asCObjectType& Type)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type)) return asEMetadataResult::ForeignOwner;
	if (Type.localProperties.GetLength() || Type.derivedFrom) return asEMetadataResult::InvalidArgument;
	Type.isInterfaceDeclaration = true;
	Type.metadataDeclarationKind = static_cast<asBYTE>(asETypeDeclKind::Interface);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::AddInterface(asCObjectType& Type, asCObjectType& Interface)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type) || !CanReference(&Interface)) return asEMetadataResult::ForeignOwner;
	if (!Interface.IsInterface() || &Type == &Interface) return asEMetadataResult::InvalidArgument;
	for (asUINT Index = 0; Index < Type.interfaces.GetLength(); ++Index)
		if (Type.interfaces[Index] == &Interface) return asEMetadataResult::Succeeded;
	Type.interfaces.PushLast(&Interface);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::AddBehaviour(asCObjectType& Type, asEBehaviours Behaviour, asCScriptFunction& Function)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type) || !CanReference(&Function)) return asEMetadataResult::ForeignOwner;
	if (!Function.metadataSignatureDefined) return asEMetadataResult::InvalidSignature;
	for (asUINT Index = 0; Index < Type.metadataBehaviours.GetLength(); ++Index)
		if (Type.metadataBehaviours[Index].kind == Behaviour && Type.metadataBehaviours[Index].function == &Function)
			return asEMetadataResult::Succeeded;
	Type.metadataBehaviours.PushLast({Behaviour, &Function});
	if (Behaviour == asBEHAVE_FACTORY) Type.metadataFactories.PushLast(&Function);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::AddTemplateArgument(asCObjectType& Type, const asCDataType& Argument)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type)) return asEMetadataResult::ForeignOwner;
	if (Type.GetMetadataTemplateDefinition()) return asEMetadataResult::InvalidState;
	auto Result = ValidateDataType(Argument, false);
	if (Result != asEMetadataResult::Succeeded) return Result;
	Type.templateSubTypes.PushLast(NormalizeOptions(Argument));
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::SetDefaultArrayType(asCObjectType& Type)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type)) return asEMetadataResult::ForeignOwner;
	DefaultArrayType = &Type;
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::AddEnumValue(asCEnumType& Type, const char* Name, int Value)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type)) return asEMetadataResult::ForeignOwner;
	if (!Name || !*Name) return asEMetadataResult::InvalidArgument;
	for (asUINT Index = 0; Index < Type.enumValues.GetLength(); ++Index)
		if (Type.enumValues[Index]->name == Name) return asEMetadataResult::DuplicateName;
	asSEnumValue* Entry = asNEW(asSEnumValue);
	Entry->name = Name; Entry->value = Value; Type.enumValues.PushLast(Entry);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::SetAlias(asCTypedefType& Type, const asCDataType& Alias)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type)) return asEMetadataResult::ForeignOwner;
	if (Type.aliasForType.IsValid()) return asEMetadataResult::InvalidState;
	auto Result = ValidateDataType(Alias, false);
	if (Result != asEMetadataResult::Succeeded) return Result;
	Type.aliasForType = NormalizeOptions(Alias);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::AddChildFuncdef(asCObjectType& Type, asCFuncdefType& Funcdef)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type) || !Owns(&Funcdef)) return asEMetadataResult::ForeignOwner;
	if (Funcdef.parentClass) return asEMetadataResult::InvalidState;
	Funcdef.parentClass = &Type;
	Funcdef.metadataDeclarationOwner = &Type;
	Type.childFuncDefs.PushLast(&Funcdef);
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::SetNativeLayout(asCObjectType& Type, asUINT Size, asUINT Alignment)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type)) return asEMetadataResult::ForeignOwner;
	if (!ValidAlignment(Alignment) || Size > MAX_int32 || Size % Alignment || Type.IsInterface())
		return asEMetadataResult::InvalidLayout;
	NativeLayouts.Add(&Type, {Size, Alignment});
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::SetFixedTemplateLayout(asCObjectType& Type, asUINT Size, asUINT Alignment)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type)) return asEMetadataResult::ForeignOwner;
	if (!(Type.flags & asOBJ_TEMPLATE) || !Type.metadataGenericArity || !ValidAlignment(Alignment) ||
		Size > MAX_int32 || Size % Alignment || Type.IsInterface()) return asEMetadataResult::InvalidLayout;
	NativeLayouts.Add(&Type, {Size, Alignment});
	Type.metadataFixedTemplateLayout = true;
	InvalidateLayouts();
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::CreateTemplateInstance(asCObjectType& Template, const asSStableKey& TypeUseKey,
	TConstArrayView<asCDataType> Arguments, asCObjectType*& Out)
{
std::lock_guard<std::mutex> Lock(MutationMutex);
	Out = nullptr;
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!CanReference(&Template) || Owns(&Template) || !Template.GetMetadataImage()->IsFrozen()) return asEMetadataResult::ForeignOwner;
	if (!Template.HasFixedTemplateLayout() || Template.size < 0 || !ValidAlignment(Template.alignment)) return asEMetadataResult::IncompleteDefinition;
	if (!IdentityContext || !(Template.flags & asOBJ_TEMPLATE) || Template.GetMetadataTemplateDefinition() || Arguments.Num() != int(Template.metadataGenericArity)) return asEMetadataResult::InvalidSignature;
	const auto& Registry = IdentityContext->GetIdentityRegistry();
	asSStableTypeUseDescriptor Use;
	if (Registry.ResolveTypeUse(TypeUseKey, Use) != asETypeIdentityStatus::Succeeded || Use.Kind != asETypeUseKind::Nominal ||
		Use.Declaration != Template.GetStableKey() || Use.Qualifiers != asETypeUseQualifiers::None || Use.Arguments.Num() != Arguments.Num()) return asEMetadataResult::InvalidSignature;
	for (int Index = 0; Index < Arguments.Num(); ++Index)
		if (ValidateDataType(Arguments[Index], false) != asEMetadataResult::Succeeded || !ValidateTypeUse(Use.Arguments[Index], Arguments[Index])) return asEMetadataResult::InvalidSignature;
	if (auto* Existing = TypesByKey.FindRef(TypeUseKey))
	{
		Out = CastToObjectType(Existing);
		return Out && Out->GetMetadataTemplateDefinition() == &Template ? asEMetadataResult::Succeeded : asEMetadataResult::DuplicateIdentity;
	}
	if (FunctionsByKey.Contains(TypeUseKey) || GlobalsByKey.Contains(TypeUseKey)) return asEMetadataResult::DuplicateIdentity;
	asCObjectType* Instance = asNEW(asCObjectType)(*this, TypeUseKey);
	Instance->name = Template.name; Instance->nameSpace = InternNamespace(Template.nameSpace->name.AddressOf());
	Instance->flags = Template.flags; Instance->metadataDeclarationKind = Template.metadataDeclarationKind;
	Instance->metadataDeclarationOwner = Template.metadataDeclarationOwner;
	Instance->metadataTemplateDefinition = &Template;
	for (const auto& Argument : Arguments) Instance->templateSubTypes.PushLast(NormalizeOptions(Argument));
	Instance->size = -1; Instance->alignment = 1;
	NativeLayouts.Add(Instance, {asUINT(Template.size), asUINT(Template.alignment)});
	Types.Add(Instance); TypesByKey.Add(TypeUseKey, Instance);
	InvalidateLayouts();
	Out = Instance;
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::FinalizeLayouts()
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	struct FLayout { asUINT Size = 0; asUINT Alignment = 1; };
	TMap<asCTypeInfo*, FLayout> Layouts;
	TMap<asCTypeInfo*, uint8> Visiting;
	TMap<asCObjectProperty*, int> Offsets;
	TArray<asCTypeInfo*> Order;
	asEMetadataResult Result = asEMetadataResult::Succeeded;
	std::function<bool(asCTypeInfo*, FLayout&)> TypeLayout;
	std::function<bool(const asCDataType&, FLayout&)> DataLayout;
	DataLayout = [&](const asCDataType& Type, FLayout& Out)
	{
		Result = ValidateDataType(Type, true);
		if (Result != asEMetadataResult::Succeeded) return false;
		if (Type.IsReference() || Type.IsObjectHandle() || (Type.GetTypeInfo() && (Type.GetTypeInfo()->flags & asOBJ_REF)))
		{
			Out = {Options.PointerByteSize, Options.PointerByteSize}; return true;
		}
		if (Type.GetTypeInfo()) return TypeLayout(Type.GetTypeInfo(), Out);
		switch (Type.GetTokenType())
		{
		case ttVoid: Out = {0, 1}; break;
		case ttBool: case ttInt8: case ttUInt8: Out = {1, 1}; break;
		case ttInt16: case ttUInt16: Out = {2, 2}; break;
		case ttInt: case ttUInt: case ttFloat32: Out = {4, 4}; break;
		case ttInt64: case ttUInt64: case ttFloat64: case ttDouble: Out = {8, 8}; break;
		case ttFloat: Out = Options.FloatIsFloat64 ? FLayout{8, 8} : FLayout{4, 4}; break;
		default: Result = asEMetadataResult::InvalidLayout; return false;
		}
		return true;
	};
	TypeLayout = [&](asCTypeInfo* Type, FLayout& Out)
	{
		if (!CanReference(Type)) { Result = asEMetadataResult::ForeignOwner; return false; }
		if (!Owns(Type))
		{
			if (Type->size < 0 || !ValidAlignment(Type->alignment)) { Result = asEMetadataResult::InvalidLayout; return false; }
			Out = {static_cast<asUINT>(Type->size), static_cast<asUINT>(Type->alignment)}; return true;
		}
		if (const FLayout* Existing = Layouts.Find(Type)) { Out = *Existing; return true; }
		if (Visiting.FindRef(Type) == 1) { Result = asEMetadataResult::RecursiveValueType; return false; }
		Visiting.Add(Type, 1);
		if (asCTypedefType* Alias = CastToTypedefType(Type))
		{
			if (!Alias->aliasForType.IsValid()) { Result = asEMetadataResult::IncompleteDefinition; return false; }
			if (!DataLayout(Alias->aliasForType, Out)) return false;
		}
		else if (CastToEnumType(Type)) Out = {4, 4};
		else if (CastToFuncdefType(Type)) Out = {Options.PointerByteSize, Options.PointerByteSize};
		else if (asCObjectType* Object = CastToObjectType(Type))
		{
			Out = {};
			if (Object->derivedFrom && !TypeLayout(Object->derivedFrom, Out)) return false;
			if (Object->IsInterface() && (Object->localProperties.GetLength() || Object->derivedFrom))
			{ Result = asEMetadataResult::InvalidLayout; return false; }
			const auto* Native = NativeLayouts.Find(Object);
			if (!(Object->flags & asOBJ_SCRIPT_OBJECT) && !Object->IsInterface() && !Native)
			{ Result = asEMetadataResult::IncompleteDefinition; return false; }
			uint64 Position = Out.Size;
			for (asUINT Index = 0; Index < Object->localProperties.GetLength(); ++Index)
			{
				asCObjectProperty* Property = Object->localProperties[Index];
				FLayout Member;
				if (!DataLayout(Property->type, Member)) return false;
				Position = (Position + Member.Alignment - 1) & ~(uint64(Member.Alignment) - 1);
				if (Position + Member.Size > MAX_int32) { Result = asEMetadataResult::InvalidLayout; return false; }
				Offsets.Add(Property, static_cast<int>(Position));
				Position += Member.Size;
				Out.Alignment = FMath::Max(Out.Alignment, Member.Alignment);
			}
			Position = (Position + Out.Alignment - 1) & ~(uint64(Out.Alignment) - 1);
			if (Position > MAX_int32) { Result = asEMetadataResult::InvalidLayout; return false; }
			Out.Size = static_cast<asUINT>(Position);
			if (Native)
			{
				if (Native->Key < Out.Size || Native->Value < Out.Alignment) { Result = asEMetadataResult::InvalidLayout; return false; }
				Out = {Native->Key, Native->Value};
			}
		}
		else { Result = asEMetadataResult::InvalidLayout; return false; }
		Visiting.Add(Type, 2); Layouts.Add(Type, Out); Order.Add(Type); return true;
	};
	for (asCTypeInfo* Type : Types)
	{
		FLayout Layout;
		if (!TypeLayout(Type, Layout)) return Result;
	}
	// Commit only after the complete graph has passed. In particular, cycles or
	// invalid native layouts cannot publish offsets from a successful prefix.
	for (asCTypeInfo* Type : Order)
	{
		const FLayout& Layout = Layouts.FindChecked(Type);
		Type->size = Layout.Size; Type->alignment = Layout.Alignment;
		if (asCObjectType* Object = CastToObjectType(Type))
		{
			Object->properties.SetLength(0); Object->propertyTable.EraseAll();
			if (Object->derivedFrom)
				for (asUINT Index = 0; Index < Object->derivedFrom->properties.GetLength(); ++Index)
				{
					auto* Property = Object->derivedFrom->properties[Index];
					Object->properties.PushLast(Property); Object->propertyTable.Add(Property);
				}
			for (asUINT Index = 0; Index < Object->localProperties.GetLength(); ++Index)
			{
				auto* Property = Object->localProperties[Index];
				Property->byteOffset = Offsets.FindChecked(Property);
				Object->properties.PushLast(Property); Object->propertyTable.Add(Property);
			}
		}
	}
	// Semantic layouts are not a VM call frame. Parameter offsets, bytecode and
	// executable callables require a separate, explicit backend preparation.
	bLayoutsFinalized = true;
	return asEMetadataResult::Succeeded;
}

asEMetadataResult asCMetadataImage::Freeze()
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!bLayoutsFinalized) return asEMetadataResult::IncompleteDefinition;
	for (asCScriptFunction* Function : Functions)
		if (!Function->metadataSignatureDefined) return asEMetadataResult::IncompleteDefinition;
	for (const asCGlobalProperty* Global : Globals)
		if (!ValidateGlobalIdentity(*Global)) return asEMetadataResult::InvalidSignature;
	if (DefaultArrayType && DefaultArrayType->templateSubTypes.GetLength() != 1)
		return asEMetadataResult::IncompleteDefinition;
	if (!CaptureAccessSnapshot()) return asEMetadataResult::InvalidSignature;
	for (asCTypeInfo* Type : Types)
	{
		FrozenLayouts.Add(Type, {Type->size, Type->alignment});
		FrozenTypeFlags.Add(Type, Type->flags);
		if (asCObjectType* Object = CastToObjectType(Type))
		{
			FrozenTemplateLayoutPolicies.Add(Object, Object->HasFixedTemplateLayout());
			for (asUINT Index = 0; Index < Object->localProperties.GetLength(); ++Index)
			{
				asCObjectProperty* Property = Object->localProperties[Index];
				FrozenPropertyOffsets.Add(Property, Property->byteOffset);
				FrozenPropertyTypes.Add(Property, Property->type);
			}
		}
	}
	State.store(asEMetadataImageState::Frozen, std::memory_order_release);
	return asEMetadataResult::Succeeded;
}

END_AS_NAMESPACE
