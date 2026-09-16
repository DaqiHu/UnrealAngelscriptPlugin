/* Maintained Unreal AngelScript fork — engine-independent definition ownership. */
#include "as_definitions.h"
#include "as_bytecode_image.h"
#include "as_callfunc.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_property.h"
#include "as_namespace.h"
#include "as_memory.h"
#include "as_type_id_registry.h"
#include "Containers/StringConv.h"
#include "Containers/Set.h"
#include "frontend/Basic/as_canonical_encoding.h"
#include "frontend/AST/as_type_context.h"
#include "Hash/Blake3.h"

#include <algorithm>
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

TUniquePtr<asCDefinitions> asCDefinitions::Create(const asSDefinitionOptions& Options)
{
	if ((Options.PointerByteSize != 4 && Options.PointerByteSize != 8) || !Options.TargetProfile.GetLength())
		return nullptr;
	return TUniquePtr<asCDefinitions>(new asCDefinitions(Options));
}

asCDefinitions::asCDefinitions() : asCDefinitions(asSDefinitionOptions{}) {}

asCDefinitions::asCDefinitions(const asSDefinitionOptions& InOptions) : Options(InOptions) {}

asCDefinitionGraphLease::asCDefinitionGraphLease(TUniquePtr<asCDefinitions>&& Producer)
{
	asCDefinitions* Raw = Producer.Release();
	if (!Raw)
	{
		return;
	}
	Graph = TSharedPtr<asCDefinitions, ESPMode::ThreadSafe>(Raw);
	Raw->Self = Graph;
}

asCDefinitionGraphLease asCDefinitions::AdoptGraph(TUniquePtr<asCDefinitions>&& Producer)
{
	return asCDefinitionGraphLease(MoveTemp(Producer));
}

asCDefinitionGraphLease asCDefinitions::RetainGraph()
{
	return asCDefinitionGraphLease(Self.Pin());
}

void asCDefinitions::SetDestroyCounters(asSDefinitionDestroyCounters* Counters)
{
	DestroyCounters = Counters;
}

asEDefinitionResult asCDefinitions::BindNativeFunction(asCScriptFunction& Function, const asSFuncPtr& FunctionPtr,
	asDWORD CallConv, asFunctionCaller Caller, void* Auxiliary)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Function)) return asEDefinitionResult::ForeignOwner;
	if (Function.funcType != asFUNC_SYSTEM || !Function.metadataSignatureDefined)
		return asEDefinitionResult::InvalidArgument;
	auto Interface = std::make_shared<asSSystemFunctionInterface>();
	if (DetectCallingConvention(Function.objectType != nullptr, FunctionPtr, static_cast<int>(CallConv),
		Auxiliary, Caller, Interface.get()) < 0)
	{
		return asEDefinitionResult::InvalidSignature;
	}
	if (!Interface->CallFrame.Prepare(&Function)) return asEDefinitionResult::InvalidSignature;
	Interface->paramSize = Interface->CallFrame.ArgumentDwords;
	if ((Interface->callConv == ICC_GENERIC_FUNC || Interface->callConv == ICC_GENERIC_METHOD)
		&& PrepareSystemFunctionGeneric(&Function, Interface.get(), nullptr) != 0)
	{
		return asEDefinitionResult::InvalidSignature;
	}
	Function.processInterface = MoveTemp(Interface);
	return asEDefinitionResult::Succeeded;
}

asCDefinitions::~asCDefinitions()
{
	bDestroying = true;
	if (DestroyCounters)
	{
		DestroyCounters->Namespaces.fetch_add(Namespaces.Num());
	}
	for (asCGlobalProperty* Global : Globals) asDELETE(Global, asCGlobalProperty);
	for (asCScriptFunction* Function : Functions) asDELETE(Function, asCScriptFunction);
	for (asCTypeInfo* Type : Types) asDELETE(Type, asCTypeInfo);
	for (asSNameSpace* Namespace : Namespaces) asDELETE(Namespace, asSNameSpace);
}

bool asCDefinitions::Owns(const asCTypeInfo* Type) const { return Type && Type->GetDefinitions() == this; }
bool asCDefinitions::Owns(const asCScriptFunction* Function) const { return Function && Function->GetDefinitions() == this; }

bool asCDefinitions::IsDefaultArrayType(const asCTypeInfo* Type) const
{
	return DefaultArrayType == Type;
}

bool asCDefinitions::ReferencesSet(const asCDefinitions* Set) const
{
	if (this == Set) return true;
	for (asCDefinitions* Dependency : Dependencies)
		if (Dependency && Dependency->ReferencesSet(Set)) return true;
	return false;
}

bool asCDefinitions::CanReference(const asCTypeInfo* Type) const
{
	return Type && Type->GetDefinitions() && ReferencesSet(Type->GetDefinitions());
}
bool asCDefinitions::CanReference(const asCScriptFunction* Function) const
{
	return Function && Function->GetDefinitions() && ReferencesSet(Function->GetDefinitions());
}

asCTypeInfo* asCDefinitions::FindType(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Found = TypesByKey.Find(Key);
	return Found ? *Found : nullptr;
}
asCScriptFunction* asCDefinitions::FindFunction(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Found = FunctionsByKey.Find(Key);
	return Found ? *Found : nullptr;
}

asCGlobalProperty* asCDefinitions::FindGlobalProperty(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	return GlobalsByKey.FindRef(Key);
}

asEDefinitionResult asCDefinitions::CreateGlobalProperty(const asSStableKey& Key,
	const asSStableKey& TypeUseKey, const char* Name, const char* Namespace,
	const asCDataType& Type, asCGlobalProperty*& Out)
{
	return CreateGlobalPropertyInternal(Key, TypeUseKey, Name, Namespace, Type, Out, false);
}

asEDefinitionResult asCDefinitions::CreateNativeGlobalProperty(const asSStableKey& Key,
	const asSStableKey& TypeUseKey, const char* Name, const char* Namespace,
	const asCDataType& Type, asCGlobalProperty*& Out)
{
	return CreateGlobalPropertyInternal(Key, TypeUseKey, Name, Namespace, Type, Out, true);
}

asEDefinitionResult asCDefinitions::CreateGlobalPropertyInternal(const asSStableKey& Key,
	const asSStableKey& TypeUseKey, const char* Name, const char* Namespace,
	const asCDataType& Type, asCGlobalProperty*& Out, bool Native)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	Out = nullptr;
	const auto Status = ValidateNew(Key, Name, Namespace, false);
	if (Status != asEDefinitionResult::Succeeded) return Status;
	if (GlobalsByName.Contains(QualifiedName(Namespace, Name))) return asEDefinitionResult::DuplicateName;
	if ((!Native && !Type.IsReadOnly() && !Type.IsHandleToConst()) || Type.IsReference() ||
		ValidateDataType(Type, false) != asEDefinitionResult::Succeeded) return asEDefinitionResult::InvalidSignature;
	asCGlobalProperty Candidate;
	asSNameSpace TemporaryNamespace;
	TemporaryNamespace.name = Namespace;
	Candidate.name = Name; Candidate.nameSpace = &TemporaryNamespace; Candidate.type = NormalizeOptions(Type);
	Candidate.definitions = this; Candidate.memory = nullptr; Candidate.id = asUINT(-1);
	Candidate.stableKey = Key; Candidate.metadataTypeUseKey = TypeUseKey;
	Candidate.metadataNativeDeclaration = Native;
	if (!ValidateGlobalIdentity(Candidate)) return asEDefinitionResult::InvalidSignature;
	asCGlobalProperty* Global = asNEW(asCGlobalProperty);
	Global->name = Name; Global->nameSpace = InternNamespace(Namespace); Global->type = NormalizeOptions(Type);
	Global->definitions = this; Global->stableKey = Key; Global->metadataTypeUseKey = TypeUseKey;
	Global->metadataNativeDeclaration = Native;
	Global->id = asUINT(-1); Global->memory = nullptr; Global->refCount.set(0);
	Globals.Add(Global); GlobalsByKey.Add(Key, Global); GlobalsByName.Add(QualifiedName(Namespace, Name), Global);
	InvalidateLayouts();
	Out = Global;
	return asEDefinitionResult::Succeeded;
}

bool asCDefinitions::ValidateGlobalIdentity(const asCGlobalProperty& Global) const
{
	if (Global.metadataNativeDeclaration
		&& (!IdentityContext || Options.Kind == asETypeInfoKind::HostProcess))
	{
		return Global.definitions == this && Global.nameSpace
			&& !Global.module && !Global.initFunc && !Global.type.IsReference();
	}
if (!IdentityContext || Global.definitions != this || !Global.nameSpace || Global.module || Global.initFunc || Global.realAddress || Global.memory || Global.memoryAllocated ||
		(!Global.metadataNativeDeclaration && !Global.type.IsReadOnly() && !Global.type.IsHandleToConst()) || Global.type.IsReference()) return false;
	const auto& Registry = IdentityContext->GetIdentityRegistry();
	asSDeclarationIdentityDescriptor Decl;
	if (Registry.ResolveDeclaration(Global.stableKey, Decl) != asETypeIdentityStatus::Succeeded ||
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

asCDataType asCDefinitions::Primitive(eTokenType Token, bool IsConst) const
{
	asCDataType Type = asCDataType::CreatePrimitive(Token, IsConst, Options.FloatIsFloat64);
	Type.SetMetadataOptions(Options.FloatIsFloat64, Options.PointerByteSize);
	return Type;
}
asCDataType asCDefinitions::NormalizeOptions(const asCDataType& Type) const
{
	asCDataType Copy(Type);
	Copy.SetMetadataOptions(Options.FloatIsFloat64, Options.PointerByteSize);
	return Copy;
}

asEDefinitionResult asCDefinitions::SetIdentityContext(TSharedPtr<const asCTypeContext, ESPMode::ThreadSafe> Context)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Context.IsValid()) return asEDefinitionResult::InvalidArgument;
	const auto& Settings = Context->GetOptions();
	if (Settings.PointerByteSize != Options.PointerByteSize || Settings.FloatIsFloat64 != Options.FloatIsFloat64 ||
		Settings.ExpandDefaultArrayToTemplate != Options.ExpandDefaultArrayToTemplate || Settings.TargetProfile != UTF8_TO_TCHAR(Options.TargetProfile.AddressOf()))
		return asEDefinitionResult::InvalidArgument;
	decltype(TypesByName) ReboundNames;
	for (asCTypeInfo* Type : Types)
	{
		if (const auto* Object = CastToObjectType(Type); Object && Object->GetTemplateDefinition()) continue;
		const FString NameKey = TypeNameKey(Context.Get(), Type->GetStableKey(), Type->GetNamespace(), Type->GetName());
		if (ReboundNames.Contains(NameKey)) return asEDefinitionResult::DuplicateName;
		ReboundNames.Add(NameKey, Type);
	}
	IdentityContext = MoveTemp(Context);
	TypesByName = MoveTemp(ReboundNames);
	return asEDefinitionResult::Succeeded;
}

TSharedPtr<const asCTypeContext, ESPMode::ThreadSafe> asCDefinitions::GetIdentityContext() const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	return IdentityContext;
}

asCScriptEngine* asCDefinitions::GetBoundEngine() const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	return BoundEngine;
}

int asCDefinitions::GetBoundTypeId(const asCTypeInfo* Type) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	const int* Id = BoundTypeIds.Find(Type);
	return Owns(Type) && Id ? *Id : -1;
}

int asCDefinitions::GetBoundFunctionId(const asCScriptFunction* Function) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	const int* Id = BoundFunctionIds.Find(Function);
	return Owns(Function) && Id ? *Id : -1;
}

int asCDefinitions::GetBoundDataTypeId(const asCDataType& Type) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
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

bool asCDefinitions::ValidateFrozenLayout() const
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
					|| ValidateDataType(Property->type, false) != asEDefinitionResult::Succeeded) return false;
				++PropertyCount;
			}
		}
	}
	return PropertyCount == FrozenPropertyOffsets.Num() && PropertyCount == FrozenPropertyTypes.Num();
}

bool asCDefinitions::ValidateTypeUse(const asSStableKey& Key, const asCDataType& Actual, int Depth) const
{
if (!IdentityContext.IsValid()) return false;
	const auto& Registry = IdentityContext->GetIdentityRegistry();
	if (Depth > 64 || ValidateDataType(Actual, true, true) != asEDefinitionResult::Succeeded) return false;
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
		Observed.Declaration = Object && Object->GetTemplateDefinition() ? Object->GetTemplateDefinition()->GetStableKey() : Type->GetStableKey();
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
		case ttQuestion:
			Observed.Kind = asETypeUseKind::Wildcard;
			Observed.Primitive = asEPrimitiveType::Invalid;
			break;
		default: return false;
		}
	}
	return Registry.ValidateTypeUse(Key, Observed) == asETypeIdentityStatus::Succeeded;
}

bool asCDefinitions::ValidateFrozenIdentity() const
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
		if (asCObjectType* Instance = CastToObjectType(Type); Instance && Instance->GetTemplateDefinition())
		{
			const auto* Template = Instance->GetTemplateDefinition();
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
		Descriptor.Owner = Type->declarationOwner ? Type->declarationOwner->GetStableKey() : asSStableKey{};
		Descriptor.Kind = static_cast<asETypeDeclKind>(Type->declarationKind);
		Descriptor.GenericArity = Type->genericArity;
		if (Registry.ValidateNominalType(Type->GetStableKey(), Descriptor) != asETypeIdentityStatus::Succeeded) return false;
	}
	for (asCScriptFunction* Function : Functions)
	{
		if (!Function->metadataSignatureDefined || Function->inOutFlags.GetLength() != Function->parameterTypes.GetLength() ||
			Function->parameterNames.GetLength() != Function->parameterTypes.GetLength() || Function->defaultArgs.GetLength() != Function->parameterTypes.GetLength()) return false;
		if (Function->metadataCallableSignature)
		{
			asSStableTypeUseDescriptor Signature;
			if (Function->funcType != asFUNC_CALLABLE_SIGNATURE || Function->objectType || Function->declarationOwner ||
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
		Descriptor.Owner = Function->declarationOwner ? Function->declarationOwner->GetStableKey() : asSStableKey{};
		Descriptor.Kind = static_cast<asEFunctionIdentityKind>(Function->declarationKind);
		if (Function->traits.GetTrait(asTRAIT_CONSTRUCTOR) != (Descriptor.Kind == asEFunctionIdentityKind::Constructor) ||
			Function->traits.GetTrait(asTRAIT_DESTRUCTOR) != (Descriptor.Kind == asEFunctionIdentityKind::Destructor)) return false;
		Descriptor.GenericArity = Function->genericArity;
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
			&& (static_cast<asCTypeInfo*>(Function->objectType) != Function->declarationOwner
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

bool asCDefinitions::ValidateFrozenDefinitions() const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	return IsFrozen() && ValidateFrozenLayout() && ValidateFrozenIdentity() && ValidateAccessSnapshot();
}

asEDefinitionResult asCDefinitions::ValidateDataType(const asCDataType& Type, bool AllowVoid, bool AllowWildcard) const
{
	if (!Type.IsValid() || Type.IsAuto() || Type.IsUnresolvedObject() || Type.IsErrorDataType() || Type.IsNullHandle())
		return asEDefinitionResult::InvalidSignature;
	if (Type.GetTypeInfo())
		return CanReference(Type.GetTypeInfo()) ? asEDefinitionResult::Succeeded : asEDefinitionResult::ForeignOwner;
	if (Type.GetTokenType() == ttVoid)
		return AllowVoid && !Type.IsReference() ? asEDefinitionResult::Succeeded : asEDefinitionResult::InvalidSignature;
	if (Type.GetTokenType() == ttQuestion)
		return AllowWildcard && Type.IsReference() ? asEDefinitionResult::Succeeded : asEDefinitionResult::InvalidSignature;
	switch (Type.GetTokenType())
	{
	case ttBool: case ttInt8: case ttUInt8: case ttInt16: case ttUInt16:
	case ttInt: case ttUInt: case ttInt64: case ttUInt64:
	case ttFloat32: case ttFloat64: case ttFloat: case ttDouble:
		return asEDefinitionResult::Succeeded;
	default: return asEDefinitionResult::InvalidSignature;
	}
}

asEDefinitionResult asCDefinitions::ValidateNew(const asSStableKey& Key,
	const char* Name, const char* Namespace, bool IsType) const
{
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (Key.IsZero() || !Name || !Namespace) return asEDefinitionResult::InvalidArgument;
	if (!*Name)
	{
		asSFunctionIdentityDescriptor Identity;
		if (IsType || !IdentityContext.IsValid() ||
			IdentityContext->GetIdentityRegistry().ResolveFunction(Key, Identity) != asETypeIdentityStatus::Succeeded ||
			(Identity.Kind != asEFunctionIdentityKind::ClassDefault))
			return asEDefinitionResult::InvalidArgument;
	}
	if (TypesByKey.Contains(Key) || FunctionsByKey.Contains(Key) || GlobalsByKey.Contains(Key)) return asEDefinitionResult::DuplicateIdentity;
	if (IsType && TypesByName.Contains(TypeNameKey(IdentityContext.Get(), Key, Namespace, Name))) return asEDefinitionResult::DuplicateName;
	// The producer must retain its canonical identity registry/context. Nonzero
	// is a shell precondition, not authentication of an arbitrary digest; Engine
	// registration must validate the image against that producer-owned context.
	return asEDefinitionResult::Succeeded;
}

asSNameSpace* asCDefinitions::InternNamespace(const char* Name)
{
	for (asSNameSpace* Namespace : Namespaces)
		if (Namespace->name == Name) return Namespace;
	asSNameSpace* Namespace = asNEW(asSNameSpace);
	Namespace->name = Name;
	Namespaces.Add(Namespace);
	return Namespace;
}

asEDefinitionResult asCDefinitions::AddDependency(asCDefinitions* Dependency)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Dependency || Dependency == this) return asEDefinitionResult::InvalidArgument;
	if (!Dependency->IsFrozen()) return asEDefinitionResult::UnfrozenDependency;
	if (Dependency->Options.PointerByteSize != Options.PointerByteSize || Dependency->Options.TargetProfile != Options.TargetProfile)
		return asEDefinitionResult::InvalidLayout;
	if (Dependency->ReferencesSet(this)) return asEDefinitionResult::InvalidArgument;
	for (asCDefinitions* Existing : Dependencies) if (Existing == Dependency) return asEDefinitionResult::Succeeded;
	Dependencies.Add(Dependency);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::CreateObjectType(const asSStableKey& Key,
	const char* Name, const char* Namespace, asQWORD Flags, asCObjectType*& Out)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Result = ValidateNew(Key, Name, Namespace, true);
	if (Result != asEDefinitionResult::Succeeded) return Result;
	if ((Flags & (asOBJ_ENUM | asOBJ_TYPEDEF | asOBJ_CALLABLE_TYPE | asOBJ_LIST_PATTERN)) ||
		!(Flags & (asOBJ_VALUE | asOBJ_REF)) || ((Flags & asOBJ_VALUE) && (Flags & asOBJ_REF)))
		return asEDefinitionResult::InvalidArgument;
	Out = asNEW(asCObjectType)(*this, Key);
	Out->name = Name; Out->nameSpace = InternNamespace(Namespace); Out->flags = Flags;
	Out->declarationKind = static_cast<asBYTE>((Flags & asOBJ_VALUE) ? asETypeDeclKind::Struct : asETypeDeclKind::Class);
	Out->size = -1; Out->alignment = 1;
	Types.Add(Out); TypesByKey.Add(Key, Out); TypesByName.Add(TypeNameKey(IdentityContext.Get(), Key, Namespace, Name), Out);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::CreateEnumType(const asSStableKey& Key,
	const char* Name, const char* Namespace, asCEnumType*& Out)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Result = ValidateNew(Key, Name, Namespace, true);
	if (Result != asEDefinitionResult::Succeeded) return Result;
	Out = asNEW(asCEnumType)(*this, Key);
	Out->name = Name; Out->nameSpace = InternNamespace(Namespace); Out->flags = asOBJ_ENUM;
	Out->declarationKind = static_cast<asBYTE>(asETypeDeclKind::Enum);
	Out->size = 4; Out->alignment = 4;
	Types.Add(Out); TypesByKey.Add(Key, Out); TypesByName.Add(TypeNameKey(IdentityContext.Get(), Key, Namespace, Name), Out);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::CreateTypedefType(const asSStableKey& Key,
	const char* Name, const char* Namespace, asCTypedefType*& Out)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Result = ValidateNew(Key, Name, Namespace, true);
	if (Result != asEDefinitionResult::Succeeded) return Result;
	Out = asNEW(asCTypedefType)(*this, Key);
	Out->name = Name; Out->nameSpace = InternNamespace(Namespace); Out->flags = asOBJ_TYPEDEF;
	Out->declarationKind = static_cast<asBYTE>(asETypeDeclKind::Typedef);
	Out->size = -1; Out->alignment = 1;
	Types.Add(Out); TypesByKey.Add(Key, Out); TypesByName.Add(TypeNameKey(IdentityContext.Get(), Key, Namespace, Name), Out);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::CreateCallableType(const asSStableKey& Key,
	const char* Name, const char* Namespace, asCScriptFunction& Signature, asCCallableType*& Out)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Result = ValidateNew(Key, Name, Namespace, true);
	if (Result != asEDefinitionResult::Succeeded) return Result;
	if (!Owns(&Signature)) return asEDefinitionResult::ForeignOwner;
	if (Signature.funcType != asFUNC_CALLABLE_SIGNATURE || (!Signature.metadataCallableSignature && Signature.metadataNominalCallableType))
		return asEDefinitionResult::InvalidSignature;
	Out = asNEW(asCCallableType)(*this, Key, &Signature);
	if (!Signature.metadataCallableSignature) Signature.metadataNominalCallableType = Out;
	Out->declarationKind = static_cast<asBYTE>(asETypeDeclKind::CallableType);
	Out->name = Name; Out->nameSpace = InternNamespace(Namespace);
	Out->size = Options.PointerByteSize; Out->alignment = Options.PointerByteSize;
	Types.Add(Out); TypesByKey.Add(Key, Out); TypesByName.Add(TypeNameKey(IdentityContext.Get(), Key, Namespace, Name), Out);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::CreateFunction(const asSStableKey& Key,
	const char* Name, const char* Namespace, asEFuncType Kind, asCScriptFunction*& Out)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	auto Result = ValidateNew(Key, Name, Namespace, false);
	if (Result != asEDefinitionResult::Succeeded) return Result;
	// A delegate instance is a runtime allocation with GC/callable state, not a
	// function declaration. Funcdef signatures describe delegate/event types.
	if (Kind != asFUNC_SCRIPT && Kind != asFUNC_SYSTEM && Kind != asFUNC_CALLABLE_SIGNATURE && Kind != asFUNC_INTERFACE)
		return asEDefinitionResult::InvalidArgument;
	Out = asNEW(asCScriptFunction)(*this, Key, Kind);
	Out->name = Name; Out->nameSpace = InternNamespace(Namespace);
	Out->returnType = Primitive(ttVoid);
	Functions.Add(Out); FunctionsByKey.Add(Key, Out);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::CreateCallableSignature(const asSStableKey& SignatureKey, asCScriptFunction*& Out)
{
Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!IdentityContext.IsValid()) return asEDefinitionResult::InvalidArgument;
	asSStableTypeUseDescriptor Descriptor;
	if (IdentityContext->GetIdentityRegistry().ResolveTypeUse(SignatureKey, Descriptor) != asETypeIdentityStatus::Succeeded ||
		Descriptor.Kind != asETypeUseKind::Callable || Descriptor.Qualifiers != asETypeUseQualifiers::None)
		return asEDefinitionResult::InvalidSignature;
	if (asCScriptFunction* Existing = FunctionsByKey.FindRef(SignatureKey))
	{
		if (!Existing->metadataCallableSignature) return asEDefinitionResult::DuplicateIdentity;
		Out = Existing;
		return asEDefinitionResult::Succeeded;
	}
	const auto Result = ValidateNew(SignatureKey, "$signature", "", false);
	if (Result != asEDefinitionResult::Succeeded) return Result;
	Out = asNEW(asCScriptFunction)(*this, SignatureKey, asFUNC_CALLABLE_SIGNATURE);
	Out->metadataCallableSignature = true;
	Out->name = "$signature";
	Out->nameSpace = InternNamespace("");
	Out->returnType = Primitive(ttVoid);
	Functions.Add(Out);
	FunctionsByKey.Add(SignatureKey, Out);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::DefineFunction(asCScriptFunction& Function,
	const asCDataType& ReturnType, TConstArrayView<asSDefinitionParameter> Parameters, asCObjectType* Owner)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Function) || (Owner && !Owns(Owner))) return asEDefinitionResult::ForeignOwner;
	if (Function.funcType == asFUNC_INTERFACE && (!Owner || !Owner->IsInterface()))
	{
		if (!Function.metadataSignatureDefined)
		{
			Functions.Remove(&Function);
			FunctionsByKey.Remove(Function.GetStableKey());
			asDELETE(&Function, asCScriptFunction);
		}
		return asEDefinitionResult::InvalidArgument;
	}
	if (Function.metadataSignatureDefined) return asEDefinitionResult::InvalidState;
	auto Result = ValidateDataType(ReturnType, true);
	if (Result != asEDefinitionResult::Succeeded) return Result;
	bool HasDefault = false;
	for (const auto& Parameter : Parameters)
	{
		Result = ValidateDataType(Parameter.Type, false, true);
		if (Result != asEDefinitionResult::Succeeded) return Result;
		if (Parameter.Flags > asTM_INOUTREF || (Parameter.Flags != asTM_NONE && !Parameter.Type.IsReference()))
			return asEDefinitionResult::InvalidSignature;
		if (HasDefault && !Parameter.DefaultArgument.GetLength()) return asEDefinitionResult::InvalidSignature;
		HasDefault |= Parameter.DefaultArgument.GetLength() != 0;
	}
	// Validation above is transactional: a rejected parameter must not attach a
	// partial method to its owner or change the previous uncompleted signature.
	Function.returnType = NormalizeOptions(ReturnType);
	Function.objectType = Owner;
	Function.declarationOwner = Owner;
	Function.declarationKind = static_cast<asBYTE>(Owner ? asEFunctionIdentityKind::Method : asEFunctionIdentityKind::Global);
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
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetNativeGlobalFacts(asCGlobalProperty& Global, const asSNativeGlobalFacts& Facts)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (GlobalsByKey.FindRef(Global.GetStableKey()) != &Global || Global.definitions != this)
		return asEDefinitionResult::ForeignOwner;
	if (!Global.metadataNativeDeclaration || (Facts.PureConstant && (!Global.type.IsReadOnly() ||
		(!Global.type.IsPrimitive() && !Global.type.IsEnumType())))) return asEDefinitionResult::InvalidArgument;
	Global.isPureConstant = Facts.PureConstant;
	Global.storage = Facts.PureConstant ? Facts.ConstantValue : 0;
#if WITH_EDITOR
	Global.isEditorOnly = Facts.EditorOnly;
#endif
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetNativeFunctionFacts(asCScriptFunction& Function, const asSNativeFunctionFacts& Facts)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Function)) return asEDefinitionResult::ForeignOwner;
	if (!Function.metadataSignatureDefined || Facts.DeterminesOutputTypeArgument < -1 ||
		Facts.DeterminesOutputTypeArgument > 127 || Facts.DeterminesOutputTypeArgument >= int(Function.parameterTypes.GetLength()) ||
		Facts.CompileOut < asECompileOutType::CompileCalls || Facts.CompileOut > asECompileOutType::CompileOutAsMethodChain)
		return asEDefinitionResult::InvalidArgument;
	if ((Facts.Traits & asTRAIT_PRIVATE) && (Facts.Traits & asTRAIT_PROTECTED)) return asEDefinitionResult::InvalidArgument;
	Function.traits.traits = Facts.Traits;
	Function.compileOutType = Facts.CompileOut;
	Function.determinesOutputTypeArgumentIndex = int8(Facts.DeterminesOutputTypeArgument);
#if WITH_EDITOR
	Function.deprecationMessage = Facts.DeprecationMessage;
#endif
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetNativePropertyFacts(asCObjectType& Owner, asCObjectProperty& Property, const asSNativePropertyFacts& Facts)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Owner) || !Owner.localProperties.Exists(&Property)) return asEDefinitionResult::ForeignOwner;
	if (!Property.isNativeAddress) return asEDefinitionResult::InvalidArgument;
	Property.exposedType = asCObjectProperty::GenerateExposedType(Facts.Editable, Facts.Readable, Facts.Writeable);
	Property.isProtected = Facts.Protected; Property.isPrivate = false; Property.isAppBindProperty = true;
#if WITH_EDITOR
	Property.isEditorOnly = Facts.EditorOnly; Property.isDeprecated = Facts.Deprecated;
	Property.DeprecationMessage = Facts.DeprecationMessage;
#endif
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetTypeDeclarationFacts(asCTypeInfo& Type, asETypeDeclKind Kind,
	asUINT GenericArity, asCTypeInfo* Owner)
{
std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Type) || (Owner && !CanReference(Owner))) return asEDefinitionResult::ForeignOwner;
	if (GenericArity > 1024 || Owner == &Type) return asEDefinitionResult::InvalidArgument;
	bool ValidKind = false;
	if (CastToEnumType(&Type)) ValidKind = Kind == asETypeDeclKind::Enum;
	else if (CastToTypedefType(&Type)) ValidKind = Kind == asETypeDeclKind::Typedef;
	else if (CastToCallableType(&Type)) ValidKind = Kind == asETypeDeclKind::CallableType || Kind == asETypeDeclKind::Delegate || Kind == asETypeDeclKind::Event;
	else if (auto* Object = CastToObjectType(&Type))
		ValidKind = Kind == (Object->IsInterface() ? asETypeDeclKind::Interface : (Object->flags & asOBJ_VALUE) ? asETypeDeclKind::Struct : asETypeDeclKind::Class);
	if (!ValidKind) return asEDefinitionResult::InvalidArgument;
	Type.declarationKind = static_cast<asBYTE>(Kind);
	Type.genericArity = GenericArity;
	Type.declarationOwner = Owner;
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetFunctionDeclarationFacts(asCScriptFunction& Function, asEFunctionIdentityKind Kind,
	asCTypeInfo* Owner, asUINT GenericArity, bool IsStatic)
{
std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Function) || (Owner && !CanReference(Owner))) return asEDefinitionResult::ForeignOwner;
	if (!Function.metadataSignatureDefined || GenericArity > 1024 || Kind <= asEFunctionIdentityKind::Invalid || Kind > asEFunctionIdentityKind::Conversion || Kind == asEFunctionIdentityKind::ReservedAnonymousFunction)
		return asEDefinitionResult::InvalidArgument;
	if ((Kind == asEFunctionIdentityKind::Global) != (Owner == nullptr)) return asEDefinitionResult::InvalidArgument;
	if (Kind == asEFunctionIdentityKind::ClassDefault &&
		(Function.returnType.GetTokenType() != ttVoid || Function.returnType.GetTypeInfo() || Function.returnType.IsReference() ||
		 Function.parameterTypes.GetLength() != 0 || GenericArity || IsStatic)) return asEDefinitionResult::InvalidSignature;
	if (Kind == asEFunctionIdentityKind::Conversion &&
		(Function.parameterTypes.GetLength() != 0 || Function.returnType.GetTokenType() == ttVoid
			|| Function.returnType.IsReference() || GenericArity || IsStatic))
		return asEDefinitionResult::InvalidSignature;
	if (Kind == asEFunctionIdentityKind::Factory)
	{
		if (Function.objectType || Function.returnType.GetTypeInfo() != Owner || !Function.returnType.IsObjectHandle()) return asEDefinitionResult::InvalidSignature;
	}
	else if (Owner != Function.objectType) return asEDefinitionResult::InvalidSignature;
	if ((Kind == asEFunctionIdentityKind::Constructor || Kind == asEFunctionIdentityKind::Destructor) &&
		(Function.returnType.GetTokenType() != ttVoid || Function.returnType.IsReference() || Function.returnType.GetTypeInfo() ||
		 (Kind == asEFunctionIdentityKind::Destructor && Function.parameterTypes.GetLength() != 0)))
		return asEDefinitionResult::InvalidSignature;
	// Special declarations are represented by the behavior table, never disguised as ordinary methods.
	// Conversion methods remain in the method table; only their identity kind is Conversion.
	const bool bMethodLike = Kind == asEFunctionIdentityKind::Method || Kind == asEFunctionIdentityKind::Conversion;
	const bool bWasMethodLike = Function.declarationKind == static_cast<asBYTE>(asEFunctionIdentityKind::Method)
		|| Function.declarationKind == static_cast<asBYTE>(asEFunctionIdentityKind::Conversion);
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
	Function.declarationKind = static_cast<asBYTE>(Kind);
	Function.traits.SetTrait(asTRAIT_CONSTRUCTOR, Kind == asEFunctionIdentityKind::Constructor);
	Function.traits.SetTrait(asTRAIT_DESTRUCTOR, Kind == asEFunctionIdentityKind::Destructor);
	Function.genericArity = GenericArity;
	Function.declarationOwner = Owner;
	Function.metadataStaticDeclaration = IsStatic;
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetFunctionLocal(asCScriptFunction& Function, bool IsLocal)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Function)) return asEDefinitionResult::ForeignOwner;
	Function.traits.SetTrait(asTRAIT_LOCAL, IsLocal);
	return asEDefinitionResult::Succeeded;
}


asEDefinitionResult asCDefinitions::AddProperty(asCObjectType& Owner, const char* Name,
	const asCDataType& Type, asCObjectProperty*& Out, bool IsPrivate, bool IsProtected)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Owner)) return asEDefinitionResult::ForeignOwner;
	if (!Name || !*Name || Owner.IsInterface()) return asEDefinitionResult::InvalidArgument;
	auto Result = ValidateDataType(Type, false);
	if (Result != asEDefinitionResult::Succeeded) return Result;
	for (asUINT Index = 0; Index < Owner.localProperties.GetLength(); ++Index)
		if (Owner.localProperties[Index]->name == Name) return asEDefinitionResult::DuplicateName;
	Out = asNEW(asCObjectProperty);
	Out->name = Name; Out->type = NormalizeOptions(Type); Out->isPrivate = IsPrivate; Out->isProtected = IsProtected;
	Owner.localProperties.PushLast(Out); Owner.properties.PushLast(Out); Owner.propertyTable.Add(Out);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

namespace
{
	bool NativeMemberLayout(const asCDefinitions& Set, const asCDataType& Type, asUINT& Size, asUINT& Alignment)
	{
		if (Type.IsObjectHandle() || Type.IsReference())
		{
			Size = Set.GetOptions().PointerByteSize;
			Alignment = Set.GetOptions().PointerByteSize;
			return true;
		}
		if (asCTypeInfo* Info = Type.GetTypeInfo())
		{
			if (Info->size < 0 || Info->alignment <= 0) return false;
			Size = static_cast<asUINT>(Info->size);
			Alignment = static_cast<asUINT>(Info->alignment);
			return true;
		}
		switch (Type.GetTokenType())
		{
		case ttBool: case ttInt8: case ttUInt8: Size = 1; Alignment = 1; return true;
		case ttInt16: case ttUInt16: Size = 2; Alignment = 2; return true;
		case ttInt: case ttUInt: case ttFloat32: Size = 4; Alignment = 4; return true;
		case ttInt64: case ttUInt64: case ttFloat64: case ttDouble: Size = 8; Alignment = 8; return true;
		case ttFloat:
			Size = Set.GetOptions().FloatIsFloat64 ? 8 : 4;
			Alignment = Size;
			return true;
		default:
			return false;
		}
	}
}

asEDefinitionResult asCDefinitions::DefineNativeProperty(asCObjectType& Owner, const char* Name,
	const asCDataType& Type, const asSNativePropertyAddress& Address, asCObjectProperty*& Out)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Owner)) return asEDefinitionResult::ForeignOwner;
	if (!Name || !*Name || Owner.IsInterface() || (Owner.flags & asOBJ_SCRIPT_OBJECT)) return asEDefinitionResult::InvalidArgument;
	const auto* Native = NativeLayouts.Find(&Owner);
	if (!Native) return asEDefinitionResult::IncompleteDefinition;
	auto Result = ValidateDataType(Type, false);
	if (Result != asEDefinitionResult::Succeeded) return Result;
	for (asUINT Index = 0; Index < Owner.localProperties.GetLength(); ++Index)
		if (Owner.localProperties[Index]->name == Name) return asEDefinitionResult::DuplicateName;
	asUINT MemberSize = 0;
	asUINT MemberAlign = 0;
	if (!NativeMemberLayout(*this, NormalizeOptions(Type), MemberSize, MemberAlign) || MemberSize == 0) return asEDefinitionResult::InvalidLayout;
	int ResolvedOffset = Address.Offset;
	int ResolvedComposite = 0;
	bool bIndirect = false;
	if (Address.Mode == asENativeAddressMode::Direct)
	{
		if (Address.Offset < 0 || Address.MemberOffset != 0 || Address.CompositeStorageType) return asEDefinitionResult::InvalidLayout;
		if ((static_cast<uint64>(Address.Offset) + MemberSize) > Native->Key) return asEDefinitionResult::InvalidLayout;
		if ((MemberAlign > 1) && (Address.Offset % static_cast<int>(MemberAlign)) != 0) return asEDefinitionResult::InvalidLayout;
		ResolvedOffset = Address.Offset;
	}
	else if (Address.Mode == asENativeAddressMode::CompositeInline)
	{
		if (Address.Offset < 0 || Address.MemberOffset < 0 || !Address.CompositeStorageType) return asEDefinitionResult::InvalidLayout;
		if (!CanReference(Address.CompositeStorageType)) return asEDefinitionResult::InvalidLayout;
		asUINT CompositeSize = 0;
		if (const auto* CompositeNative = NativeLayouts.Find(CastToObjectType(Address.CompositeStorageType)))
		{
			CompositeSize = CompositeNative->Key;
		}
		else if (Address.CompositeStorageType->size > 0)
		{
			CompositeSize = static_cast<asUINT>(Address.CompositeStorageType->size);
		}
		else
		{
			return asEDefinitionResult::IncompleteDefinition;
		}
		if ((static_cast<uint64>(Address.Offset) + CompositeSize) > Native->Key) return asEDefinitionResult::InvalidLayout;
		if ((static_cast<uint64>(Address.MemberOffset) + MemberSize) > CompositeSize) return asEDefinitionResult::InvalidLayout;
		ResolvedOffset = Address.MemberOffset;
		ResolvedComposite = Address.Offset;
	}
	else if (Address.Mode == asENativeAddressMode::CompositeIndirect)
	{
		if (Address.Offset < 0 || Address.MemberOffset < 0 || !Address.CompositeStorageType) return asEDefinitionResult::InvalidLayout;
		if ((static_cast<uint64>(Address.Offset) + Options.PointerByteSize) > Native->Key) return asEDefinitionResult::InvalidLayout;
		asUINT PointeeSize = 0;
		if (const auto* CompositeNative = NativeLayouts.Find(CastToObjectType(Address.CompositeStorageType)))
		{
			PointeeSize = CompositeNative->Key;
		}
		else if (Address.CompositeStorageType->size > 0)
		{
			PointeeSize = static_cast<asUINT>(Address.CompositeStorageType->size);
		}
		else
		{
			return asEDefinitionResult::IncompleteDefinition;
		}
		if ((static_cast<uint64>(Address.MemberOffset) + MemberSize) > PointeeSize) return asEDefinitionResult::InvalidLayout;
		ResolvedOffset = Address.MemberOffset;
		ResolvedComposite = Address.Offset;
		bIndirect = true;
	}
	else
	{
		return asEDefinitionResult::InvalidArgument;
	}
	for (asUINT Index = 0; Index < Owner.localProperties.GetLength(); ++Index)
	{
		const asCObjectProperty* Existing = Owner.localProperties[Index];
		if (!Existing->isNativeAddress || Existing->nativeAddressMode != asENativeAddressMode::Direct || Address.Mode != asENativeAddressMode::Direct) continue;
		asUINT ExistingSize = 0;
		asUINT ExistingAlign = 0;
		if (!NativeMemberLayout(*this, Existing->type, ExistingSize, ExistingAlign)) return asEDefinitionResult::InvalidLayout;
		const int ExistingEnd = Existing->byteOffset + static_cast<int>(ExistingSize);
		const int NewEnd = ResolvedOffset + static_cast<int>(MemberSize);
		if (ResolvedOffset < ExistingEnd && Existing->byteOffset < NewEnd) return asEDefinitionResult::InvalidLayout;
	}
	Out = asNEW(asCObjectProperty);
	Out->name = Name;
	Out->type = NormalizeOptions(Type);
	Out->byteOffset = ResolvedOffset;
	Out->compositeOffset = ResolvedComposite;
	Out->isCompositeIndirect = bIndirect;
	Out->isNativeAddress = true;
	Out->nativeAddressMode = Address.Mode;
	Owner.localProperties.PushLast(Out); Owner.properties.PushLast(Out); Owner.propertyTable.Add(Out);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetBaseType(asCObjectType& Type, asCObjectType& Base)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Type) || !CanReference(&Base)) return asEDefinitionResult::ForeignOwner;
	if (&Type == &Base || Type.derivedFrom || Base.IsInterface()) return asEDefinitionResult::InvalidArgument;
	TSet<asCObjectType*> Visited;
	for (asCObjectType* Ancestor = &Base; Ancestor; Ancestor = Ancestor->derivedFrom)
	{
		if (Ancestor == &Type || Visited.Contains(Ancestor)) return asEDefinitionResult::RecursiveValueType;
		Visited.Add(Ancestor);
	}
	Type.derivedFrom = &Base;
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetInterfaceDeclaration(asCObjectType& Type)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Type)) return asEDefinitionResult::ForeignOwner;
	if (Type.localProperties.GetLength() || Type.derivedFrom) return asEDefinitionResult::InvalidArgument;
	Type.isInterfaceDeclaration = true;
	Type.declarationKind = static_cast<asBYTE>(asETypeDeclKind::Interface);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::AddInterface(asCObjectType& Type, asCObjectType& Interface)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Type) || !CanReference(&Interface)) return asEDefinitionResult::ForeignOwner;
	if (!Interface.IsInterface() || &Type == &Interface) return asEDefinitionResult::InvalidArgument;
	for (asUINT Index = 0; Index < Type.interfaces.GetLength(); ++Index)
		if (Type.interfaces[Index] == &Interface) return asEDefinitionResult::Succeeded;
	Type.interfaces.PushLast(&Interface);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::AddBehaviour(asCObjectType& Type, asEBehaviours Behaviour, asCScriptFunction& Function)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Type) || !CanReference(&Function)) return asEDefinitionResult::ForeignOwner;
	if (!Function.metadataSignatureDefined) return asEDefinitionResult::InvalidSignature;
	for (asUINT Index = 0; Index < Type.metadataBehaviours.GetLength(); ++Index)
		if (Type.metadataBehaviours[Index].kind == Behaviour && Type.metadataBehaviours[Index].function == &Function)
			return asEDefinitionResult::Succeeded;
	Type.metadataBehaviours.PushLast({Behaviour, &Function});
	if (Behaviour == asBEHAVE_FACTORY) Type.metadataFactories.PushLast(&Function);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::AddTemplateArgument(asCObjectType& Type, const asCDataType& Argument)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Type)) return asEDefinitionResult::ForeignOwner;
	if (Type.GetTemplateDefinition()) return asEDefinitionResult::InvalidState;
	auto Result = ValidateDataType(Argument, false);
	if (Result != asEDefinitionResult::Succeeded) return Result;
	Type.templateSubTypes.PushLast(NormalizeOptions(Argument));
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetDefaultArrayType(asCObjectType& Type)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Type)) return asEDefinitionResult::ForeignOwner;
	DefaultArrayType = &Type;
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::AddEnumValue(asCEnumType& Type, const char* Name, int Value)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Type)) return asEDefinitionResult::ForeignOwner;
	if (!Name || !*Name) return asEDefinitionResult::InvalidArgument;
	for (asUINT Index = 0; Index < Type.enumValues.GetLength(); ++Index)
		if (Type.enumValues[Index]->name == Name) return asEDefinitionResult::DuplicateName;
	asSEnumValue* Entry = asNEW(asSEnumValue);
	Entry->name = Name; Entry->value = Value; Type.enumValues.PushLast(Entry);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetAlias(asCTypedefType& Type, const asCDataType& Alias)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Type)) return asEDefinitionResult::ForeignOwner;
	if (Type.aliasForType.IsValid()) return asEDefinitionResult::InvalidState;
	auto Result = ValidateDataType(Alias, false);
	if (Result != asEDefinitionResult::Succeeded) return Result;
	Type.aliasForType = NormalizeOptions(Alias);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::AddChildCallableType(asCObjectType& Type, asCCallableType& CallableType)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Type) || !Owns(&CallableType)) return asEDefinitionResult::ForeignOwner;
	if (CallableType.parentClass) return asEDefinitionResult::InvalidState;
	CallableType.parentClass = &Type;
	CallableType.declarationOwner = &Type;
	Type.childCallableTypes.PushLast(&CallableType);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetNativeLayout(asCObjectType& Type, asUINT Size, asUINT Alignment)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Type)) return asEDefinitionResult::ForeignOwner;
	if (!ValidAlignment(Alignment) || Size > MAX_int32 || Size % Alignment || Type.IsInterface())
		return asEDefinitionResult::InvalidLayout;
	NativeLayouts.Add(&Type, {Size, Alignment});
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetFixedTemplateLayout(asCObjectType& Type, asUINT Size, asUINT Alignment)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Type)) return asEDefinitionResult::ForeignOwner;
	if (!(Type.flags & asOBJ_TEMPLATE) || !Type.genericArity || !ValidAlignment(Alignment) ||
		Size > MAX_int32 || Size % Alignment || Type.IsInterface()) return asEDefinitionResult::InvalidLayout;
	NativeLayouts.Add(&Type, {Size, Alignment});
	Type.metadataFixedTemplateLayout = true;
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::CreateTemplateInstance(asCObjectType& Template, const asSStableKey& TypeUseKey,
	TConstArrayView<asCDataType> Arguments, asCObjectType*& Out)
{
std::lock_guard<std::mutex> Lock(MutationMutex);
	Out = nullptr;
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!CanReference(&Template) || Owns(&Template) || !Template.GetDefinitions()->IsFrozen()) return asEDefinitionResult::ForeignOwner;
	if (!Template.HasFixedTemplateLayout() || Template.size < 0 || !ValidAlignment(Template.alignment)) return asEDefinitionResult::IncompleteDefinition;
	if (!IdentityContext || !(Template.flags & asOBJ_TEMPLATE) || Template.GetTemplateDefinition() || Arguments.Num() != int(Template.genericArity)) return asEDefinitionResult::InvalidSignature;
	const auto& Registry = IdentityContext->GetIdentityRegistry();
	asSStableTypeUseDescriptor Use;
	if (Registry.ResolveTypeUse(TypeUseKey, Use) != asETypeIdentityStatus::Succeeded || Use.Kind != asETypeUseKind::Nominal ||
		Use.Declaration != Template.GetStableKey() || Use.Qualifiers != asETypeUseQualifiers::None || Use.Arguments.Num() != Arguments.Num()) return asEDefinitionResult::InvalidSignature;
	for (int Index = 0; Index < Arguments.Num(); ++Index)
		if (ValidateDataType(Arguments[Index], false) != asEDefinitionResult::Succeeded || !ValidateTypeUse(Use.Arguments[Index], Arguments[Index])) return asEDefinitionResult::InvalidSignature;
	if (auto* Existing = TypesByKey.FindRef(TypeUseKey))
	{
		Out = CastToObjectType(Existing);
		return Out && Out->GetTemplateDefinition() == &Template ? asEDefinitionResult::Succeeded : asEDefinitionResult::DuplicateIdentity;
	}
	if (FunctionsByKey.Contains(TypeUseKey) || GlobalsByKey.Contains(TypeUseKey)) return asEDefinitionResult::DuplicateIdentity;
	asCObjectType* Instance = asNEW(asCObjectType)(*this, TypeUseKey);
	Instance->name = Template.name; Instance->nameSpace = InternNamespace(Template.nameSpace->name.AddressOf());
	Instance->flags = Template.flags; Instance->declarationKind = Template.declarationKind;
	Instance->declarationOwner = Template.declarationOwner;
	Instance->templateDefinition = &Template;
	for (const auto& Argument : Arguments) Instance->templateSubTypes.PushLast(NormalizeOptions(Argument));
	Instance->size = -1; Instance->alignment = 1;
	NativeLayouts.Add(Instance, {asUINT(Template.size), asUINT(Template.alignment)});
	Types.Add(Instance); TypesByKey.Add(TypeUseKey, Instance);
	InvalidateLayouts();
	Out = Instance;
	return asEDefinitionResult::Succeeded;
}

namespace
{
	bool SameTemplateArgument(const asCDataType& Left, const asCDataType& Right)
	{
		return Left.GetTokenType() == Right.GetTokenType()
			&& Left.GetTypeInfo() == Right.GetTypeInfo()
			&& Left.IsObjectHandle() == Right.IsObjectHandle()
			&& Left.IsReference() == Right.IsReference();
	}

	bool TemplateArgumentsMatch(const asCObjectType& Instance, TConstArrayView<asCDataType> Arguments)
	{
		if (Instance.templateSubTypes.GetLength() != static_cast<asUINT>(Arguments.Num())) return false;
		for (int32 Index = 0; Index < Arguments.Num(); ++Index)
		{
			if (!SameTemplateArgument(Instance.templateSubTypes[Index], Arguments[Index])) return false;
		}
		return true;
	}

}

asCObjectType* asCDefinitions::FindTemplateInstance(const asCObjectType* Template, TConstArrayView<asCDataType> Arguments) const
{
	if (!Template) return nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	for (asCTypeInfo* Type : Types)
	{
		asCObjectType* Object = CastToObjectType(Type);
		if (!Object || Object->GetTemplateDefinition() != Template) continue;
		if (TemplateArgumentsMatch(*Object, Arguments)) return Object;
	}
	return nullptr;
}

asEDefinitionResult asCDefinitions::CreateHostTemplateInstance(asCObjectType& Template, const asSStableKey& Key,
	TConstArrayView<asCDataType> Arguments, asCObjectType*& Out)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	Out = nullptr;
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!CanReference(&Template)) return asEDefinitionResult::ForeignOwner;
	if (!(Template.flags & asOBJ_TEMPLATE) || Template.GetTemplateDefinition()
		|| Arguments.Num() != static_cast<int32>(Template.genericArity) || !Template.genericArity)
	{
		return asEDefinitionResult::InvalidSignature;
	}
	if (!Template.HasFixedTemplateLayout()) return asEDefinitionResult::IncompleteDefinition;
	asUINT Size = 0;
	asUINT Alignment = 8;
	if (const auto* Native = NativeLayouts.Find(&Template))
	{
		Size = Native->Key;
		Alignment = Native->Value;
	}
	else if (Template.size >= 0 && ValidAlignment(Template.alignment))
	{
		Size = static_cast<asUINT>(Template.size);
		Alignment = static_cast<asUINT>(Template.alignment);
	}
	else
	{
		return asEDefinitionResult::IncompleteDefinition;
	}
	bool bPrivate = false;
	for (const asCDataType& Argument : Arguments)
	{
		if (ValidateDataType(Argument, false) != asEDefinitionResult::Succeeded) return asEDefinitionResult::InvalidSignature;
		if (asCTypeInfo* Type = Argument.GetTypeInfo())
		{
			if (!CanReference(Type)) return asEDefinitionResult::ForeignOwner;
			if (Type->GetTypeInfoKind() != asETypeInfoKind::HostProcess) bPrivate = true;
		}
	}
	if (Options.Kind == asETypeInfoKind::HostProcess && bPrivate) return asEDefinitionResult::InvalidArgument;
	if (Options.Kind == asETypeInfoKind::LiveRegister && !bPrivate) return asEDefinitionResult::InvalidArgument;
	for (asCTypeInfo* Type : Types)
	{
		asCObjectType* Object = CastToObjectType(Type);
		if (!Object || Object->GetTemplateDefinition() != &Template) continue;
		if (!TemplateArgumentsMatch(*Object, Arguments)) continue;
		Out = Object;
		return asEDefinitionResult::Succeeded;
	}
	const char* Namespace = Template.nameSpace ? Template.nameSpace->name.AddressOf() : "";
	if (Key.IsZero() || !Template.name.GetLength()) return asEDefinitionResult::InvalidArgument;
	if (TypesByKey.Contains(Key) || FunctionsByKey.Contains(Key) || GlobalsByKey.Contains(Key))
	{
		return asEDefinitionResult::DuplicateIdentity;
	}
	asCObjectType* Instance = asNEW(asCObjectType)(*this, Key);
	Instance->name = Template.name;
	Instance->nameSpace = InternNamespace(Namespace);
	Instance->flags = Template.flags;
	Instance->declarationKind = Template.declarationKind;
	Instance->declarationOwner = Template.declarationOwner;
	Instance->templateDefinition = &Template;
	Instance->genericArity = Template.genericArity;
	for (const asCDataType& Argument : Arguments)
	{
		Instance->templateSubTypes.PushLast(NormalizeOptions(Argument));
	}
	Instance->size = -1;
	Instance->alignment = 1;
	NativeLayouts.Add(Instance, {Size, Alignment});
	Types.Add(Instance);
	TypesByKey.Add(Key, Instance);
	InvalidateLayouts();
	Out = Instance;
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::FinalizeLayouts()
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	struct FLayout { asUINT Size = 0; asUINT Alignment = 1; };
	TMap<asCTypeInfo*, FLayout> Layouts;
	TMap<asCTypeInfo*, uint8> Visiting;
	TMap<asCObjectProperty*, int> Offsets;
	TArray<asCTypeInfo*> Order;
	asEDefinitionResult Result = asEDefinitionResult::Succeeded;
	std::function<bool(asCTypeInfo*, FLayout&)> TypeLayout;
	std::function<bool(const asCDataType&, FLayout&)> DataLayout;
	DataLayout = [&](const asCDataType& Type, FLayout& Out)
	{
		Result = ValidateDataType(Type, true);
		if (Result != asEDefinitionResult::Succeeded) return false;
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
		default: Result = asEDefinitionResult::InvalidLayout; return false;
		}
		return true;
	};
	TypeLayout = [&](asCTypeInfo* Type, FLayout& Out)
	{
		if (!CanReference(Type)) { Result = asEDefinitionResult::ForeignOwner; return false; }
		if (!Owns(Type))
		{
			if (Type->size < 0 || !ValidAlignment(Type->alignment)) { Result = asEDefinitionResult::InvalidLayout; return false; }
			Out = {static_cast<asUINT>(Type->size), static_cast<asUINT>(Type->alignment)}; return true;
		}
		if (const FLayout* Existing = Layouts.Find(Type)) { Out = *Existing; return true; }
		if (Visiting.FindRef(Type) == 1) { Result = asEDefinitionResult::RecursiveValueType; return false; }
		Visiting.Add(Type, 1);
		if (asCTypedefType* Alias = CastToTypedefType(Type))
		{
			if (!Alias->aliasForType.IsValid()) { Result = asEDefinitionResult::IncompleteDefinition; return false; }
			if (!DataLayout(Alias->aliasForType, Out)) return false;
		}
		else if (CastToEnumType(Type)) Out = {4, 4};
		else if (CastToCallableType(Type)) Out = {Options.PointerByteSize, Options.PointerByteSize};
		else if (asCObjectType* Object = CastToObjectType(Type))
		{
			Out = {};
			if (Object->derivedFrom && !TypeLayout(Object->derivedFrom, Out)) return false;
			if (Object->IsInterface() && (Object->localProperties.GetLength() || Object->derivedFrom))
			{ Result = asEDefinitionResult::InvalidLayout; return false; }
			const auto* Native = NativeLayouts.Find(Object);
			if (!(Object->flags & asOBJ_SCRIPT_OBJECT) && !Object->IsInterface() && !Native)
			{ Result = asEDefinitionResult::IncompleteDefinition; return false; }
			if (Native)
			{
				Out = {Native->Key, Native->Value};
				for (asUINT Index = 0; Index < Object->localProperties.GetLength(); ++Index)
				{
					asCObjectProperty* Property = Object->localProperties[Index];
					if (!Property->isNativeAddress) { Result = asEDefinitionResult::IncompleteDefinition; return false; }
					asUINT MemberSize = 0;
					asUINT MemberAlign = 0;
					if (!NativeMemberLayout(*this, Property->type, MemberSize, MemberAlign)) { Result = asEDefinitionResult::InvalidLayout; return false; }
					const uint64 Extent = Property->nativeAddressMode == asENativeAddressMode::Direct
						? static_cast<uint64>(Property->byteOffset) + MemberSize
						: (Property->isCompositeIndirect
							? static_cast<uint64>(Property->compositeOffset) + Options.PointerByteSize
							: static_cast<uint64>(Property->compositeOffset) + MemberSize);
					if (Extent > Native->Key) { Result = asEDefinitionResult::InvalidLayout; return false; }
					Offsets.Add(Property, Property->byteOffset);
				}
			}
			else
			{
				uint64 Position = Out.Size;
				for (asUINT Index = 0; Index < Object->localProperties.GetLength(); ++Index)
				{
					asCObjectProperty* Property = Object->localProperties[Index];
					FLayout Member;
					if (!DataLayout(Property->type, Member)) return false;
					Position = (Position + Member.Alignment - 1) & ~(uint64(Member.Alignment) - 1);
					if (Position + Member.Size > MAX_int32) { Result = asEDefinitionResult::InvalidLayout; return false; }
					Offsets.Add(Property, static_cast<int>(Position));
					Position += Member.Size;
					Out.Alignment = FMath::Max(Out.Alignment, Member.Alignment);
				}
				Position = (Position + Out.Alignment - 1) & ~(uint64(Out.Alignment) - 1);
				if (Position > MAX_int32) { Result = asEDefinitionResult::InvalidLayout; return false; }
				Out.Size = static_cast<asUINT>(Position);
			}
		}
		else { Result = asEDefinitionResult::InvalidLayout; return false; }
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
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::Freeze()
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!bLayoutsFinalized) return asEDefinitionResult::IncompleteDefinition;
	for (asCScriptFunction* Function : Functions)
		if (!Function->metadataSignatureDefined) return asEDefinitionResult::IncompleteDefinition;
	if (Options.Kind == asETypeInfoKind::HostProcess)
	{
		for (asCScriptFunction* Function : Functions)
		{
			if (Function->funcType == asFUNC_SYSTEM && !Function->processInterface)
				return asEDefinitionResult::IncompleteDefinition;
		}
	}
	for (const asCGlobalProperty* Global : Globals)
		if (!ValidateGlobalIdentity(*Global)) return asEDefinitionResult::InvalidSignature;
	if (DefaultArrayType && DefaultArrayType->templateSubTypes.GetLength() != 1)
		return asEDefinitionResult::IncompleteDefinition;
	if (!CaptureAccessSnapshot()) return asEDefinitionResult::InvalidSignature;
	if (Options.Kind == asETypeInfoKind::HostProcess)
	{
		const asUINT TypeCount = static_cast<asUINT>(Types.Num());
		const asUINT FunctionCount = static_cast<asUINT>(Functions.Num());
		if (TypeCount != 0 || FunctionCount != 0)
		{
			asCTypeIdAllocator& Allocator = Options.TypeIdAllocator
				? *Options.TypeIdAllocator
				: asCTypeIdRegistry::Get().Allocator();
			asSTypeIdReservationRequest Request;
			Request.TypeSequenceCount = TypeCount;
			Request.FunctionIdCount = FunctionCount;
			asSTypeIdReservation Reservation;
			if (Allocator.Reserve(Request, Reservation) != asTYPE_ID_RESERVE_SUCCEEDED)
			{
				return asEDefinitionResult::InvalidLayout;
			}
			for (int32 Index = 0; Index < Types.Num(); ++Index)
			{
				asCTypeInfo* Type = Types[Index];
				BoundTypeIds.Add(Type, asCTypeIdAllocator::EncodeObjectTypeId(
					Reservation.FirstTypeSequence + Index, Type->flags));
			}
			for (int32 Index = 0; Index < Functions.Num(); ++Index)
			{
				BoundFunctionIds.Add(Functions[Index], Reservation.FirstFunctionId + Index);
			}
		}
	}
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
	State.store(asEDefinitionState::Frozen, std::memory_order_release);
	return asEDefinitionResult::Succeeded;
}

namespace
{
	FString AsString(const asCString& Value)
	{
		return FString(UTF8_TO_TCHAR(Value.AddressOf()));
	}

	int32 CompareKeys(const asSStableKey& Left, const asSStableKey& Right)
	{
		return FMemory::Memcmp(Left.GetBytes().GetData(), Right.GetBytes().GetData(), asSStableKey::ByteCount);
	}

	void WriteKey(asCCanonicalEncoding& Encoding, const asSStableKey& Key)
	{
		Encoding.WriteBytes(Key.GetBytes());
	}

	asDWORD SemanticFlags(const asCTypeInfo& Type)
	{
		asDWORD Flags = 0;
		if (const asCObjectType* Object = CastToObjectType(const_cast<asCTypeInfo*>(&Type)))
		{
			if (Object->flags & asOBJ_VALUE) Flags |= 1u;
			if (Object->flags & asOBJ_REF) Flags |= 2u;
			if (Object->flags & asOBJ_SCRIPT_OBJECT) Flags |= 4u;
			if (Object->flags & asOBJ_GC) Flags |= 8u;
			if (Object->flags & asOBJ_TEMPLATE) Flags |= 16u;
			if (Object->flags & asOBJ_POD) Flags |= 32u;
			if (Object->IsInterface()) Flags |= 64u;
		}
		return Flags;
	}

	uint8 StorageCategory(const asCTypeInfo& Type)
	{
		if (CastToEnumType(const_cast<asCTypeInfo*>(&Type))) return 0;
		if (CastToTypedefType(const_cast<asCTypeInfo*>(&Type))) return 4;
		if (CastToCallableType(const_cast<asCTypeInfo*>(&Type))) return 3;
		if (const asCObjectType* Object = CastToObjectType(const_cast<asCTypeInfo*>(&Type)))
		{
			return (Object->flags & asOBJ_REF) ? 2 : 1;
		}
		return 0;
	}

	asSFingerprintDigest HashWitness(TConstArrayView<uint8> Witness, const asIFingerprintDigestProvider* Provider)
	{
		if (Provider)
		{
			return Provider->ComputeDigest(Witness);
		}
		const FBlake3Hash Hash = FBlake3::HashBuffer(Witness.GetData(), static_cast<uint64>(Witness.Num()));
		asSFingerprintDigest Result;
		asSFingerprintDigest::TryCreate(MakeArrayView(Hash.GetBytes(), asSFingerprintDigest::ByteCount), Result);
		return Result;
	}

	asDWORD PassingMode(asDWORD InOutFlag)
	{
		if (InOutFlag == asTM_INREF) return 1;
		if (InOutFlag == asTM_OUTREF) return 2;
		if (InOutFlag == asTM_INOUTREF) return 3;
		return 0;
	}
}

struct asSMetadataFingerprintEncoder
{
	explicit asSMetadataFingerprintEncoder(const asCDefinitions& InSet)
		: Set(InSet)
	{
	}

	const asCDefinitions& Set;

	bool InternDataTypeUse(const asCDataType& Type, asSStableKey& OutKey) const
	{
		OutKey = {};
		if (!Set.IdentityContext.IsValid())
		{
			return false;
		}
		asCTypeContext* Context = const_cast<asCTypeContext*>(Set.IdentityContext.Get());
		asCStableIdentityRegistry& Registry = Context->GetIdentityRegistry();
		asSStableTypeUseDescriptor Descriptor;
		if (Type.IsReadOnly() || Type.IsHandleToConst())
		{
			Descriptor.Qualifiers = Descriptor.Qualifiers | asETypeUseQualifiers::Const;
		}
		if (Type.IsReference())
		{
			Descriptor.Qualifiers = Descriptor.Qualifiers | asETypeUseQualifiers::Reference;
		}
		if (Type.IsObjectHandle())
		{
			Descriptor.Qualifiers = Descriptor.Qualifiers | asETypeUseQualifiers::Handle;
		}
		if (Type.IsMetadataNullable())
		{
			Descriptor.Qualifiers = Descriptor.Qualifiers | asETypeUseQualifiers::Nullable;
		}
		if (asCTypeInfo* Info = Type.GetTypeInfo())
		{
			Descriptor.Kind = asETypeUseKind::Nominal;
			asCObjectType* Object = CastToObjectType(Info);
			if (Object && Object->GetTemplateDefinition())
			{
				Descriptor.Declaration = Object->GetTemplateDefinition()->GetStableKey();
				for (asUINT Index = 0; Index < Object->templateSubTypes.GetLength(); ++Index)
				{
					asSStableKey Argument;
					if (!InternDataTypeUse(Object->templateSubTypes[Index], Argument))
					{
						return false;
					}
					Descriptor.Arguments.Add(Argument);
				}
			}
			else
			{
				Descriptor.Declaration = Info->GetStableKey();
			}
		}
		else
		{
			Descriptor.Kind = asETypeUseKind::Primitive;
			switch (Type.GetTokenType())
			{
			case ttVoid: Descriptor.Primitive = asEPrimitiveType::Void; break;
			case ttBool: Descriptor.Primitive = asEPrimitiveType::Bool; break;
			case ttInt8: Descriptor.Primitive = asEPrimitiveType::Int8; break;
			case ttInt16: Descriptor.Primitive = asEPrimitiveType::Int16; break;
			case ttInt: Descriptor.Primitive = asEPrimitiveType::Int32; break;
			case ttInt64: Descriptor.Primitive = asEPrimitiveType::Int64; break;
			case ttUInt8: Descriptor.Primitive = asEPrimitiveType::UInt8; break;
			case ttUInt16: Descriptor.Primitive = asEPrimitiveType::UInt16; break;
			case ttUInt: Descriptor.Primitive = asEPrimitiveType::UInt32; break;
			case ttUInt64: Descriptor.Primitive = asEPrimitiveType::UInt64; break;
			case ttFloat32: Descriptor.Primitive = asEPrimitiveType::Float32; break;
			case ttFloat64: Descriptor.Primitive = asEPrimitiveType::Float64; break;
			case ttDouble: Descriptor.Primitive = asEPrimitiveType::Float64; break;
			case ttFloat:
				Descriptor.Primitive = Set.Options.FloatIsFloat64
					? asEPrimitiveType::Float64
					: asEPrimitiveType::Float32;
				break;
			default:
				return false;
			}
		}
		return Registry.InternTypeUse(Descriptor, OutKey) == asETypeIdentityStatus::Succeeded;
	}

	void WriteFunctionContract(asCCanonicalEncoding& Encoding, const asCScriptFunction& Function) const
	{
		WriteKey(Encoding, Function.GetStableKey());
		asSStableKey ReturnKey;
		if (!InternDataTypeUse(Function.returnType, ReturnKey))
		{
			WriteKey(Encoding, {});
		}
		else
		{
			WriteKey(Encoding, ReturnKey);
		}
		Encoding.WriteUInt32(Function.parameterTypes.GetLength());
		for (asUINT Index = 0; Index < Function.parameterTypes.GetLength(); ++Index)
		{
			asSStableKey ParameterKey;
			InternDataTypeUse(Function.parameterTypes[Index], ParameterKey);
			WriteKey(Encoding, ParameterKey);
			Encoding.WriteUInt8(static_cast<uint8>(PassingMode(Function.inOutFlags[Index])));
		}
		Encoding.WriteUInt32(static_cast<uint32>(Function.IsMetadataStatic() ? 1 : 0)
			| (Function.IsFinal() ? 2u : 0u)
			| (Function.IsOverride() ? 4u : 0u)
			| (Function.IsProperty() ? 8u : 0u)
			| (Function.IsReadOnly() ? 16u : 0u));
		Encoding.WriteUInt16(static_cast<uint16>(Function.IsReadOnly() ? asEFunctionIdentityModifiers::ConstMethod : asEFunctionIdentityModifiers::None));
		Encoding.WriteUInt8(static_cast<uint8>(Function.GetDeclarationKind()));
	}

	void WriteListPattern(asCCanonicalEncoding& Encoding, const asSMetadataListPattern& Pattern) const
	{
		Encoding.WriteUInt8(static_cast<uint8>(Pattern.Kind));
		Encoding.WriteUInt32(Pattern.TemplateParameterIndex);
		asSStableKey ValueKey;
		if (Pattern.ValueType.IsValid())
		{
			InternDataTypeUse(Pattern.ValueType, ValueKey);
		}
		WriteKey(Encoding, ValueKey);
		Encoding.WriteUInt32(static_cast<uint32>(Pattern.Children.Num()));
		for (const asSMetadataListPattern& Child : Pattern.Children)
		{
			WriteListPattern(Encoding, Child);
		}
	}

	bool EncodeSchema(const asCTypeInfo& Type, TArray<uint8>& Out) const
	{
		asCCanonicalEncoding Encoding(asECanonicalIdentityDomain::Schema);
		Encoding.WriteUInt8(static_cast<uint8>(Type.GetDeclarationKind()));
		Encoding.WriteUInt32(SemanticFlags(Type));
		WriteKey(Encoding, Type.GetDeclarationOwner() ? Type.GetDeclarationOwner()->GetStableKey() : asSStableKey{});
		const asCObjectType* Object = CastToObjectType(const_cast<asCTypeInfo*>(&Type));
		WriteKey(Encoding, Object && Object->derivedFrom ? Object->derivedFrom->GetStableKey() : asSStableKey{});
		TArray<asSStableKey> Interfaces;
		if (Object)
		{
			for (asUINT Index = 0; Index < Object->interfaces.GetLength(); ++Index)
			{
				if (Object->interfaces[Index])
				{
					Interfaces.Add(Object->interfaces[Index]->GetStableKey());
				}
			}
			Interfaces.Sort([](const asSStableKey& Left, const asSStableKey& Right)
			{
				return CompareKeys(Left, Right) < 0;
			});
		}
		Encoding.WriteUInt32(static_cast<uint32>(Interfaces.Num()));
		for (const asSStableKey& Key : Interfaces)
		{
			WriteKey(Encoding, Key);
		}
		Encoding.WriteUInt32(Type.GetGenericArity());
		TArray<asSStableKey> Arguments;
		if (Object)
		{
			for (asUINT Index = 0; Index < Object->templateSubTypes.GetLength(); ++Index)
			{
				asSStableKey Argument;
				if (!InternDataTypeUse(Object->templateSubTypes[Index], Argument))
				{
					return false;
				}
				Arguments.Add(Argument);
			}
		}
		Encoding.WriteUInt32(static_cast<uint32>(Arguments.Num()));
		for (const asSStableKey& Key : Arguments)
		{
			WriteKey(Encoding, Key);
		}
		const asUINT FieldCount = Object ? Object->localProperties.GetLength() : 0;
		Encoding.WriteUInt32(FieldCount);
		for (asUINT Index = 0; Index < FieldCount; ++Index)
		{
			const asCObjectProperty* Property = Object->localProperties[Index];
			Encoding.WriteString(AsString(Property->name));
			asSStableKey TypeUse;
			if (!InternDataTypeUse(Property->type, TypeUse))
			{
				return false;
			}
			WriteKey(Encoding, TypeUse);
			Encoding.WriteUInt8(Property->isPrivate ? 1 : 0);
			Encoding.WriteUInt8(Property->isProtected ? 1 : 0);
			Encoding.WriteString(Property->accessSpecifier ? AsString(Property->accessSpecifier->name) : FString());
		}
		const asCEnumType* EnumType = CastToEnumType(const_cast<asCTypeInfo*>(&Type));
		const asUINT EnumCount = EnumType ? EnumType->enumValues.GetLength() : 0;
		Encoding.WriteUInt32(EnumCount);
		for (asUINT Index = 0; Index < EnumCount; ++Index)
		{
			const asSEnumValue* Value = EnumType->enumValues[Index];
			Encoding.WriteString(AsString(Value->name));
			Encoding.WriteUInt32(static_cast<uint32>(Value->value));
		}
		asSStableKey AliasKey;
		if (const asCTypedefType* Alias = CastToTypedefType(const_cast<asCTypeInfo*>(&Type)))
		{
			if (Alias->aliasForType.IsValid() && !InternDataTypeUse(Alias->aliasForType, AliasKey))
			{
				return false;
			}
		}
		WriteKey(Encoding, AliasKey);
		TArray<const asCScriptFunction*> Methods;
		if (Object)
		{
			for (asUINT Index = 0; Index < Object->metadataMethods.GetLength(); ++Index)
			{
				if (Object->metadataMethods[Index])
				{
					Methods.Add(Object->metadataMethods[Index]);
				}
			}
		}
		Methods.Sort([](const asCScriptFunction& Left, const asCScriptFunction& Right)
		{
			return CompareKeys(Left.GetStableKey(), Right.GetStableKey()) < 0;
		});
		Encoding.WriteUInt32(static_cast<uint32>(Methods.Num()));
		for (const asCScriptFunction* Method : Methods)
		{
			WriteFunctionContract(Encoding, *Method);
		}
		TArray<asCObjectType::asSMetadataBehaviour> Behaviours;
		if (Object)
		{
			for (asUINT Index = 0; Index < Object->metadataBehaviours.GetLength(); ++Index)
			{
				Behaviours.Add(Object->metadataBehaviours[Index]);
			}
		}
		Behaviours.Sort([](const asCObjectType::asSMetadataBehaviour& Left, const asCObjectType::asSMetadataBehaviour& Right)
		{
			if (Left.kind != Right.kind)
			{
				return static_cast<uint8>(Left.kind) < static_cast<uint8>(Right.kind);
			}
			const asSStableKey LeftKey = Left.function ? Left.function->GetStableKey() : asSStableKey{};
			const asSStableKey RightKey = Right.function ? Right.function->GetStableKey() : asSStableKey{};
			return CompareKeys(LeftKey, RightKey) < 0;
		});
		Encoding.WriteUInt32(static_cast<uint32>(Behaviours.Num()));
		for (const asCObjectType::asSMetadataBehaviour& Behaviour : Behaviours)
		{
			Encoding.WriteUInt8(static_cast<uint8>(Behaviour.kind));
			if (Behaviour.function)
			{
				WriteFunctionContract(Encoding, *Behaviour.function);
			}
			else
			{
				WriteKey(Encoding, {});
			}
		}
		const asSMetadataListInitializer* List = Object ? Set.ListInitializers.Find(Object) : nullptr;
		Encoding.WriteUInt8(List ? 1 : 0);
		if (List)
		{
			WriteKey(Encoding, List->Factory ? List->Factory->GetStableKey() : asSStableKey{});
			WriteListPattern(Encoding, List->Pattern);
		}
		TArray<const asSAccessSpecifier*> Policies;
		if (Object)
		{
			for (asUINT Index = 0; Index < Object->accessSpecifiers.GetLength(); ++Index)
			{
				if (Object->accessSpecifiers[Index])
				{
					Policies.Add(Object->accessSpecifiers[Index]);
				}
			}
		}
		Policies.Sort([](const asSAccessSpecifier& Left, const asSAccessSpecifier& Right)
		{
			return AsString(Left.name) < AsString(Right.name);
		});
		Encoding.WriteUInt32(static_cast<uint32>(Policies.Num()));
		for (const asSAccessSpecifier* Policy : Policies)
		{
			Encoding.WriteString(AsString(Policy->name));
			Encoding.WriteUInt8(Policy->bIsPrivate ? 1 : 0);
			Encoding.WriteUInt8(Policy->bIsProtected ? 1 : 0);
			Encoding.WriteUInt8(Policy->bAnyReadOnly ? 1 : 0);
			Encoding.WriteUInt8(Policy->bAnyEditDefaults ? 1 : 0);
			Encoding.WriteUInt32(Policy->permissions.GetLength());
			for (asUINT Index = 0; Index < Policy->permissions.GetLength(); ++Index)
			{
				const asSAccessPermission& Permission = Policy->permissions[Index];
				Encoding.WriteString(AsString(Permission.accessName));
				Encoding.WriteUInt8(Permission.bInherited ? 1 : 0);
				Encoding.WriteUInt8(Permission.bReadOnly ? 1 : 0);
				Encoding.WriteUInt8(Permission.bEditDefaults ? 1 : 0);
			}
		}
		Out = TArray<uint8>(Encoding.GetBytes().GetData(), Encoding.GetBytes().Num());
		return true;
	}

	bool MemberLayout(const asCDataType& Type, asUINT& Size, asUINT& Alignment, bool& bPointer) const
	{
		bPointer = Type.IsObjectHandle() || Type.IsReference();
		if (bPointer)
		{
			Size = Set.Options.PointerByteSize;
			Alignment = Set.Options.PointerByteSize;
			return true;
		}
		if (asCTypeInfo* Info = Type.GetTypeInfo())
		{
			if (Info->size < 0 || Info->alignment <= 0)
			{
				return false;
			}
			Size = static_cast<asUINT>(Info->size);
			Alignment = static_cast<asUINT>(Info->alignment);
			return true;
		}
		switch (Type.GetTokenType())
		{
		case ttBool: case ttInt8: case ttUInt8: Size = 1; Alignment = 1; return true;
		case ttInt16: case ttUInt16: Size = 2; Alignment = 2; return true;
		case ttInt: case ttUInt: case ttFloat32: Size = 4; Alignment = 4; return true;
		case ttInt64: case ttUInt64: case ttFloat64: case ttDouble: Size = 8; Alignment = 8; return true;
		case ttFloat:
			Size = Set.Options.FloatIsFloat64 ? 8 : 4;
			Alignment = Size;
			return true;
		default:
			return false;
		}
	}

	bool EncodeLayout(const asCTypeInfo& Type, TArray<uint8>& Out, TSet<const asCTypeInfo*>& Visiting) const
	{
		if (Visiting.Contains(&Type))
		{
			return false;
		}
		const asCObjectType* Object = CastToObjectType(const_cast<asCTypeInfo*>(&Type));
		const bool bValueObject = Object && (Object->flags & asOBJ_VALUE) && !(Object->flags & asOBJ_REF);
		if (bValueObject)
		{
			Visiting.Add(&Type);
		}
		asCCanonicalEncoding Encoding(asECanonicalIdentityDomain::Layout);
		Encoding.WriteUInt16(1);
		Encoding.WriteUInt8(static_cast<uint8>(Set.Options.PointerByteSize));
		Encoding.WriteUInt8(1);
		Encoding.WriteUInt8(Set.Options.FloatIsFloat64 ? 1 : 0);
		Encoding.WriteUInt8(StorageCategory(Type));
		if (Type.size < 0 || Type.alignment <= 0)
		{
			if (bValueObject)
			{
				Visiting.Remove(&Type);
			}
			return false;
		}
		Encoding.WriteUInt32(static_cast<uint32>(Type.size));
		Encoding.WriteUInt32(static_cast<uint32>(Type.alignment));
		WriteKey(Encoding, Object && Object->derivedFrom ? Object->derivedFrom->GetStableKey() : asSStableKey{});
		if (Object && Object->derivedFrom && (Object->derivedFrom->flags & asOBJ_VALUE))
		{
			TArray<uint8> BaseLayout;
			if (!EncodeLayout(*Object->derivedFrom, BaseLayout, Visiting))
			{
				if (bValueObject)
				{
					Visiting.Remove(&Type);
				}
				return false;
			}
			Encoding.WriteBytes(MakeArrayView(BaseLayout));
		}
		else
		{
			Encoding.WriteUInt32(0);
		}
		const asUINT FieldCount = Object ? Object->localProperties.GetLength() : 0;
		Encoding.WriteUInt32(FieldCount);
		for (asUINT Index = 0; Index < FieldCount; ++Index)
		{
			const asCObjectProperty* Property = Object->localProperties[Index];
			asUINT Size = 0;
			asUINT Alignment = 0;
			bool bPointer = false;
			if (!MemberLayout(Property->type, Size, Alignment, bPointer))
			{
				if (bValueObject)
				{
					Visiting.Remove(&Type);
				}
				return false;
			}
			Encoding.WriteString(AsString(Property->name));
			Encoding.WriteUInt32(static_cast<uint32>(Property->byteOffset));
			Encoding.WriteUInt32(Size);
			Encoding.WriteUInt32(Alignment);
			Encoding.WriteUInt8(static_cast<uint8>(Property->nativeAddressMode));
			Encoding.WriteUInt8((bPointer || Property->isCompositeIndirect) ? 1 : 0);
			Encoding.WriteUInt32(static_cast<uint32>(Property->compositeOffset));
			if (!bPointer && Property->type.GetTypeInfo() && CastToObjectType(Property->type.GetTypeInfo())
				&& (Property->type.GetTypeInfo()->flags & asOBJ_VALUE))
			{
				TArray<uint8> Child;
				if (!EncodeLayout(*Property->type.GetTypeInfo(), Child, Visiting))
				{
					if (bValueObject)
					{
						Visiting.Remove(&Type);
					}
					return false;
				}
				Encoding.WriteBytes(MakeArrayView(Child));
			}
		}
		if (bValueObject)
		{
			Visiting.Remove(&Type);
		}
		Out = TArray<uint8>(Encoding.GetBytes().GetData(), Encoding.GetBytes().Num());
		return true;
	}
};

void asCDefinitions::SetFingerprintDigestProvider(const asIFingerprintDigestProvider* Provider)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	FingerprintDigestProvider = Provider;
}

asEFingerprintStatus asCDefinitions::EnsureFingerprintsLocked(const asCTypeInfo& Type, const asSCachedFingerprint*& Out) const
{
	Out = nullptr;
	if (!Owns(&Type) && !CanReference(&Type))
	{
		return asEFingerprintStatus::InvalidDefinition;
	}
	const asEDefinitionState SetState = GetState();
	if (SetState == asEDefinitionState::Building)
	{
		return asEFingerprintStatus::NotReady;
	}
	if (SetState == asEDefinitionState::Attaching)
	{
		return asEFingerprintStatus::NotReady;
	}
	if (!bLayoutsFinalized)
	{
		return asEFingerprintStatus::NotReady;
	}
	if (Options.PointerByteSize != 4 && Options.PointerByteSize != 8)
	{
		return asEFingerprintStatus::UnsupportedTarget;
	}
	if (!IdentityContext.IsValid())
	{
		return asEFingerprintStatus::UnauthenticatedDefinition;
	}
	// HostProcess still uses CRC keys for most symbols. Schema/layout
	// witnesses can be encoded from the frozen graph without requiring
	// every function to be interned first.
	if (Options.Kind != asETypeInfoKind::HostProcess && !ValidateFrozenIdentity())
	{
		return asEFingerprintStatus::UnauthenticatedDefinition;
	}
	if (!ValidateFrozenLayout() || !ValidateAccessSnapshot())
	{
		return asEFingerprintStatus::InvalidDefinition;
	}
	asSMetadataFingerprintEncoder Encoder(*this);
	TArray<uint8> SchemaWitness;
	TArray<uint8> LayoutWitness;
	TSet<const asCTypeInfo*> Visiting;
	if (!Encoder.EncodeSchema(Type, SchemaWitness))
	{
		return asEFingerprintStatus::InvalidDefinition;
	}
	if (!Encoder.EncodeLayout(Type, LayoutWitness, Visiting))
	{
		return asEFingerprintStatus::InvalidDefinition;
	}
	if (SchemaWitness.Num() == 0 || LayoutWitness.Num() == 0)
	{
		return asEFingerprintStatus::InvalidDefinition;
	}
	asCDefinitions::asSCachedFingerprint Computed;
	Computed.SchemaWitness = MoveTemp(SchemaWitness);
	Computed.LayoutWitness = MoveTemp(LayoutWitness);
	Computed.Schema.Digest = HashWitness(MakeArrayView(Computed.SchemaWitness), FingerprintDigestProvider);
	Computed.Layout.Digest = HashWitness(MakeArrayView(Computed.LayoutWitness), FingerprintDigestProvider);
	if (Computed.Schema.Digest.IsZero() || Computed.Layout.Digest.IsZero())
	{
		return asEFingerprintStatus::InvalidDefinition;
	}
	if (const asSCachedFingerprint* Existing = Fingerprints.Find(&Type))
	{
		if (Existing->SchemaWitness != Computed.SchemaWitness || Existing->LayoutWitness != Computed.LayoutWitness)
		{
			return asEFingerprintStatus::InvalidDefinition;
		}
		if (Existing->Schema.Digest != Computed.Schema.Digest || Existing->Layout.Digest != Computed.Layout.Digest)
		{
			return asEFingerprintStatus::InvalidDefinition;
		}
		Out = Existing;
		return asEFingerprintStatus::Succeeded;
	}
	const asSCachedFingerprint& Stored = Fingerprints.Add(&Type, MoveTemp(Computed));
	Out = &Stored;
	return asEFingerprintStatus::Succeeded;
}

asEFingerprintStatus asCDefinitions::TryGetSchemaHash(const asCTypeInfo& Type, asSSchemaHash& OutHash,
	TConstArrayView<uint8>* OutWitness) const
{
	OutHash = {};
	if (OutWitness)
	{
		*OutWitness = {};
	}
	std::lock_guard<std::mutex> Lock(MutationMutex);
	const asSCachedFingerprint* Cached = nullptr;
	const asEFingerprintStatus Status = EnsureFingerprintsLocked(Type, Cached);
	if (Status != asEFingerprintStatus::Succeeded || !Cached)
	{
		return Status;
	}
	OutHash = Cached->Schema;
	if (OutWitness)
	{
		*OutWitness = MakeArrayView(Cached->SchemaWitness);
	}
	return asEFingerprintStatus::Succeeded;
}

asEFingerprintStatus asCDefinitions::TryGetLayoutHash(const asCTypeInfo& Type, asSLayoutHash& OutHash,
	TConstArrayView<uint8>* OutWitness) const
{
	OutHash = {};
	if (OutWitness)
	{
		*OutWitness = {};
	}
	std::lock_guard<std::mutex> Lock(MutationMutex);
	const asSCachedFingerprint* Cached = nullptr;
	const asEFingerprintStatus Status = EnsureFingerprintsLocked(Type, Cached);
	if (Status != asEFingerprintStatus::Succeeded || !Cached)
	{
		return Status;
	}
	OutHash = Cached->Layout;
	if (OutWitness)
	{
		*OutWitness = MakeArrayView(Cached->LayoutWitness);
	}
	return asEFingerprintStatus::Succeeded;
}


asCTypeInfo* asCDefinitions::FindType(const char* Name) const
{
	if (!Name) return nullptr;
	for (asCTypeInfo* Type : Types)
	{
		if (!Type || Type->name != Name) continue;
		if (Type->nameSpace && Type->nameSpace->name.GetLength() != 0) continue;
		return Type;
	}
	return nullptr;
}

void asCDefinitions::AttachDependencies(TConstArrayView<asCDefinitions*> InDependencies)
{
	if (IsImmutable()) return;
	Dependencies.Reset();
	for (asCDefinitions* Dependency : InDependencies) Dependencies.Add(Dependency);
}

void asCDefinitions::AdoptEmittedByteCode(TUniquePtr<asCByteCodeImage> InImage)
{
	EmittedByteCode = MoveTemp(InImage);
}

asCByteCodeImage* asCDefinitions::PeekEmittedByteCode() const
{
	return EmittedByteCode.Get();
}

TUniquePtr<asCByteCodeImage> asCDefinitions::TakeEmittedByteCode()
{
	return MoveTemp(EmittedByteCode);
}

END_AS_NAMESPACE
