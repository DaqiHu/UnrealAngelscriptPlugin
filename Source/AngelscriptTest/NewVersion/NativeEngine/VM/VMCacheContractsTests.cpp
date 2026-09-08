#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_bytecode_image_codec.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_metadata_fingerprint.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"
#include "frontend/as_type_context.h"
#include "frontend/as_type_identity.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace VMCacheContractsPrivate
{
	int32 GNativeCalls = 0;
	int32 GNativeLastA = 0;
	int32 GNativeLastB = 0;

	void ResetNative()
	{
		GNativeCalls = 0;
		GNativeLastA = 0;
		GNativeLastB = 0;
	}

	void GenericAdd(asIScriptGeneric* Generic)
	{
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		++GNativeCalls;
		GNativeLastA = A;
		GNativeLastB = B;
		Generic->SetReturnDWord(static_cast<asDWORD>(A + B));
	}

	struct FZeroDigest final : AS_NAMESPACE_QUALIFIER asIFingerprintDigestProvider
	{
		AS_NAMESPACE_QUALIFIER asSFingerprintDigest ComputeDigest(TConstArrayView<uint8>) const override
		{
			return {};
		}
	};

	asSMetadataParameter MakeIntParam(const char* Name, asDWORD Flags = asTM_NONE)
	{
		asSMetadataParameter Parameter;
		Parameter.Name = Name;
		Parameter.Flags = Flags;
		return Parameter;
	}

	asSStableKey MissingKey(uint8 Tag)
	{
		uint8 Bytes[32] = {};
		Bytes[0] = Tag;
		asSStableKey Key;
		asSStableKey::TryCreate(MakeArrayView(Bytes, 32), Key);
		return Key;
	}

	asCGlobalProperty* DefineGlobal(FDetachedDefinitionFixture& Fixture, const TCHAR* Name)
	{
		AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Declaration;
		Declaration.StableScope = Fixture.Scope;
		Declaration.Name = Name;
		Declaration.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::GlobalVariable;
		asSStableKey Key;
		if (Fixture.Context->GetIdentityRegistry().InternDeclaration(Declaration, Key)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor TypeUse;
		TypeUse.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
		TypeUse.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32;
		TypeUse.Qualifiers = AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const;
		asSStableKey TypeKey;
		if (Fixture.Context->GetIdentityRegistry().InternTypeUse(TypeUse, TypeKey)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		asCGlobalProperty* Global = nullptr;
		if (Fixture.Image->CreateGlobalProperty(
			Key, TypeKey, TCHAR_TO_UTF8(Name), "", Fixture.Image->Primitive(ttInt, true), Global)
			!= asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		return Global;
	}

	asEByteCodeImageStatus AddGlobalRequirement(asCByteCodeImageBuilder& Builder, asCGlobalProperty& Global, asUINT& OutSlot)
	{
		asSByteCodeRequirement Requirement;
		Requirement.Role = asEByteCodeRequirementRole::Global;
		Requirement.Key = Global.GetStableKey();
		Requirement.Name = UTF8_TO_TCHAR(Global.name.AddressOf());
		return Builder.AddRequirement(Requirement, OutSlot);
	}

	asSByteCodeRequirement* FindRequirement(asCByteCodeImage& Image, asEByteCodeRequirementRole Role, const asSStableKey& Key)
	{
		for (asSByteCodeRequirement& Requirement : Image.Requirements)
		{
			if (Requirement.Role == Role && Requirement.Key == Key)
			{
				return &Requirement;
			}
		}
		return nullptr;
	}

	TUniquePtr<asCByteCodeImage> MakeReturn(
		asCScriptFunction* Function,
		int32 Value,
		asCTypeInfo* ExtraType = nullptr,
		asCScriptFunction* ExtraFunction = nullptr,
		const TCHAR* StringLiteral = nullptr)
	{
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		if (ExtraType && Builder.AddTypeRequirement(*ExtraType, Slot) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (ExtraFunction && Builder.AddFunctionRequirement(*ExtraFunction, Slot) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (StringLiteral && Builder.AddString(StringLiteral, Slot) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.AddFunctionRequirement(*Function, Slot) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, ByteArgumentDwords(*Function)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Push = ByteImm32(Value);
		if (Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Function));
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		return Image;
	}

	TUniquePtr<asCByteCodeImage> MakeNativeAddImage(asCScriptFunction* Run, asCScriptFunction* Add)
	{
		asCByteCodeImageBuilder Builder;
		asUINT AddSlot = 0;
		asUINT RunSlot = 0;
		if (Builder.AddFunctionRequirement(*Add, AddSlot) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.AddFunctionRequirement(*Run, RunSlot) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Right = ByteImm32(22);
		if (Builder.Emit(asBC_PshC4, MakeArrayView(&Right, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Left = ByteImm32(20);
		if (Builder.Emit(asBC_PshC4, MakeArrayView(&Left, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, AddSlot);
		if (Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		return Image;
	}

	TUniquePtr<asCByteCodeImage> MakeGlobalSetImage(asCScriptFunction* Run, asCGlobalProperty* Global)
	{
		asCByteCodeImageBuilder Builder;
		asUINT RunSlot = 0;
		asUINT GlobalSlot = 0;
		if (Builder.AddFunctionRequirement(*Run, RunSlot) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (AddGlobalRequirement(Builder, *Global, GlobalSlot) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand GlobalOp = ByteSlot(asEByteCodeOperandKind::GlobalSlot, GlobalSlot);
		asSByteCodeOperand SetG[2] = {GlobalOp, ByteImm32(42)};
		if (Builder.Emit(asBC_SetG4, MakeArrayView(SetG, 2)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Load[2] = {ByteLocal(0), GlobalOp};
		if (Builder.Emit(asBC_LdGRdR4, MakeArrayView(Load, 2)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		return Image;
	}

	TUniquePtr<asCByteCodeImage> MakeTwoBodiesLateMissing(asCScriptFunction* First)
	{
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		if (Builder.AddFunctionRequirement(*First, Slot) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.BeginFunction(First->GetStableKey(), First->GetStableKey(), 4, ByteArgumentDwords(*First)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Push = ByteImm32(7);
		if (Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*First));
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		const asSStableKey Missing = MissingKey(0xA1);
		if (Builder.BeginFunction(Missing, Missing, 4, 0) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand PushMissing = ByteImm32(1);
		if (Builder.Emit(asBC_PshC4, MakeArrayView(&PushMissing, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		return Image;
	}

	bool ExecuteInt(asCScriptEngine* Engine, asCScriptFunction* Function, int32 Expected)
	{
		asIScriptContext* Context = Engine->CreateContext();
		if (!Context)
		{
			return false;
		}
		const bool bOk = Context->Prepare(Function) == 0
			&& Context->Execute() == asEXECUTION_FINISHED
			&& static_cast<int32>(Context->GetReturnDWord()) == Expected;
		Context->Release();
		return bOk;
	}

	struct alignas(8) FCacheNativeRecord
	{
		uint8 Tag;
		int32 Value;
	};

	asCObjectType* DefineNativeRecord(FDetachedDefinitionFixture& Fixture, int Offset)
	{
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = Fixture.Scope;
		Descriptor.Name = TEXT("CacheNativeRecord");
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct;
		asSStableKey Key;
		if (Fixture.Context->GetIdentityRegistry().InternNominalType(Descriptor, Key)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		asCObjectType* Type = nullptr;
		if (Fixture.Image->CreateObjectType(Key, "CacheNativeRecord", "", asOBJ_VALUE | asOBJ_APP_CLASS, Type)
			!= asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		if (Fixture.Image->SetNativeLayout(*Type, sizeof(FCacheNativeRecord), alignof(FCacheNativeRecord))
			!= asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		asSNativePropertyAddress Address;
		Address.Mode = asENativeAddressMode::Direct;
		Address.Offset = Offset;
		asCObjectProperty* Property = nullptr;
		if (Fixture.Image->DefineNativeProperty(*Type, "Value", Fixture.Image->Primitive(ttInt), Address, Property)
			!= asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		return Type;
	}

	FString GGenericError;

	asCObjectType* DefineGenericIntBox(FDetachedDefinitionFixture& Fixture, std::shared_ptr<asCMetadataImage>& OutHost)
	{
		GGenericError.Reset();
		if (!Fixture.BindContext())
		{
			GGenericError = TEXT("bind-context");
			return nullptr;
		}
		OutHost = asCMetadataImage::Create();
		if (OutHost->SetIdentityContext(Fixture.Context) != asEMetadataResult::Succeeded)
		{
			GGenericError = TEXT("host-context");
			return nullptr;
		}
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = Fixture.Scope;
		Descriptor.Name = TEXT("CacheBox");
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct;
		Descriptor.GenericArity = 1;
		asSStableKey TemplateKey;
		if (Fixture.Context->GetIdentityRegistry().InternNominalType(Descriptor, TemplateKey)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			GGenericError = TEXT("intern-template");
			return nullptr;
		}
		asCObjectType* Template = nullptr;
		if (OutHost->CreateObjectType(TemplateKey, "CacheBox", "", asOBJ_VALUE | asOBJ_TEMPLATE, Template)
			!= asEMetadataResult::Succeeded)
		{
			GGenericError = TEXT("create-template");
			return nullptr;
		}
		if (OutHost->SetTypeDeclarationFacts(*Template, AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct, 1)
			!= asEMetadataResult::Succeeded)
		{
			GGenericError = TEXT("template-facts");
			return nullptr;
		}
		if (OutHost->SetFixedTemplateLayout(*Template, 16, 8) != asEMetadataResult::Succeeded)
		{
			GGenericError = TEXT("template-layout");
			return nullptr;
		}
		if (OutHost->FinalizeLayouts() != asEMetadataResult::Succeeded)
		{
			GGenericError = TEXT("host-finalize");
			return nullptr;
		}
		if (OutHost->Freeze() != asEMetadataResult::Succeeded)
		{
			GGenericError = TEXT("host-freeze");
			return nullptr;
		}
		if (Fixture.Image->AddDependency(OutHost) != asEMetadataResult::Succeeded)
		{
			GGenericError = TEXT("add-dependency");
			return nullptr;
		}
		AS_NAMESPACE_QUALIFIER asSStableKey Argument;
		if (Fixture.Context->InternPrimitiveTypeUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, Argument)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			GGenericError = TEXT("intern-arg");
			return nullptr;
		}
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Use;
		Use.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
		Use.Declaration = TemplateKey;
		Use.Arguments.Add(Argument);
		asSStableKey InstanceUse;
		if (Fixture.Context->GetIdentityRegistry().InternTypeUse(Use, InstanceUse)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			GGenericError = TEXT("intern-use");
			return nullptr;
		}
		asCObjectType* Instance = nullptr;
		asCDataType ArgumentType = Fixture.Image->Primitive(ttInt);
		const asEMetadataResult InstanceStatus = Fixture.Image->CreateTemplateInstance(
			*Template, InstanceUse, MakeArrayView(&ArgumentType, 1), Instance);
		if (InstanceStatus != asEMetadataResult::Succeeded)
		{
			GGenericError = FString::Printf(TEXT("instance-%d"), int32(InstanceStatus));
			return nullptr;
		}
		return Instance;
	}
}

TEST_CLASS_WITH_FLAGS(VMCacheContracts, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(NamespacedStringAndGenericExecuteOnFreshEngineAfterProducerRelease)
	{
		TArray<uint8> Bytes;
		asSStableKey RunKey;
		asSStableKey TypeKey;
		{
			FDetachedDefinitionFixture Fixture;
			Fixture.Scope = TEXT("vm-cache-contracts");
			std::shared_ptr<asCMetadataImage> Host;
			asCObjectType* Box = VMCacheContractsPrivate::DefineGenericIntBox(Fixture, Host);
			ASSERT_THAT(IsTrue(Box != nullptr, *VMCacheContractsPrivate::GGenericError));
			asCScriptFunction* Run = Fixture.DefineFunction(
				TEXT("Run"), TEXT("Game.Cache"), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			RunKey = Run->GetStableKey();
			TypeKey = Box->GetStableKey();
			FNativeEngineOwner EngineA;
			ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineA.Ptr->RegisterMetadataImage(Fixture.Image)));
			TUniquePtr<asCByteCodeImage> Image = VMCacheContractsPrivate::MakeReturn(Run, 42, Box, nullptr, TEXT("cache-token"));
			ASSERT_THAT(IsNotNull(Image.Get()));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineA.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			ASSERT_THAT(IsTrue(VMCacheContractsPrivate::ExecuteInt(EngineA.Ptr, Run, 42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
			if (Snapshot) Snapshot->Release();
		}
		FDetachedDefinitionFixture FixtureB;
		FixtureB.Scope = TEXT("vm-cache-contracts");
		std::shared_ptr<asCMetadataImage> HostB;
		asCObjectType* BoxB = VMCacheContractsPrivate::DefineGenericIntBox(FixtureB, HostB);
		ASSERT_THAT(IsTrue(BoxB != nullptr, *VMCacheContractsPrivate::GGenericError));
		asCScriptFunction* RunB = FixtureB.DefineFunction(
			TEXT("Run"), TEXT("Game.Cache"), asFUNC_SCRIPT, FixtureB.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureB.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(RunB->GetStableKey() == RunKey));
		ASSERT_THAT(IsTrue(BoxB->GetStableKey() == TypeKey));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->RegisterMetadataImage(FixtureB.Image)));
		ASSERT_THAT(IsTrue(EngineB.Ptr->FindMetadataFunction(RunKey) == RunB));
		ASSERT_THAT(IsTrue(EngineB.Ptr->FindMetadataType(TypeKey) == BoxB));
		ASSERT_THAT(IsTrue(RunB->GetId() != 0));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		ASSERT_THAT(IsTrue(Decoded->Strings.Contains(TEXT("cache-token"))));
		asCExecutableSnapshot* SnapshotB = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineB.Ptr->LinkByteCodeImage(*Decoded, SnapshotB)));
		ASSERT_THAT(IsTrue(SnapshotB->FindFunction(RunKey) == RunB));
		ASSERT_THAT(IsTrue(SnapshotB->Types.FindRef(TypeKey) == BoxB));
		ASSERT_THAT(IsTrue(VMCacheContractsPrivate::ExecuteInt(EngineB.Ptr, RunB, 42)));
		if (SnapshotB) SnapshotB->Release();
	}

	TEST_METHOD(NativeCallbackAndGlobalStorageUseBPointers)
	{
		VMCacheContractsPrivate::ResetNative();
		TArray<uint8> Bytes;
		asSStableKey RunKey;
		asSStableKey AddKey;
		asSStableKey GlobalKey;
		{
			FDetachedDefinitionFixture Fixture;
			Fixture.Scope = TEXT("vm-cache-native");
			asSMetadataParameter Params[2] = {
				VMCacheContractsPrivate::MakeIntParam("a"),
				VMCacheContractsPrivate::MakeIntParam("b")
			};
			Params[0].Type = Fixture.Image->Primitive(ttInt);
			Params[1].Type = Fixture.Image->Primitive(ttInt);
			asCScriptFunction* Add = Fixture.DefineFunction(
				TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(Params, 2));
			asCScriptFunction* Run = Fixture.DefineFunction(
				TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
			asCGlobalProperty* Global = VMCacheContractsPrivate::DefineGlobal(Fixture, TEXT("Cell"));
			ASSERT_THAT(IsNotNull(Global));
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			RunKey = Run->GetStableKey();
			AddKey = Add->GetStableKey();
			GlobalKey = Global->GetStableKey();
			FNativeEngineOwner EngineA;
			ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineA.Ptr->RegisterMetadataImage(Fixture.Image)));
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
				EngineA.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(VMCacheContractsPrivate::GenericAdd), asCALL_GENERIC)));
			int32 StorageA = 7;
			int32 UnrelatedA = 0x11111111;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineA.Ptr->BindGlobalStorage(Global->GetStableKey(), &StorageA)));
			TUniquePtr<asCByteCodeImage> Image = VMCacheContractsPrivate::MakeNativeAddImage(Run, Add);
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineA.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			ASSERT_THAT(IsTrue(VMCacheContractsPrivate::ExecuteInt(EngineA.Ptr, Run, 42)));
			ASSERT_THAT(AreEqual(1, VMCacheContractsPrivate::GNativeCalls));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
			if (Snapshot) Snapshot->Release();
			(void)UnrelatedA;
		}
		VMCacheContractsPrivate::ResetNative();
		FDetachedDefinitionFixture FixtureB;
		FixtureB.Scope = TEXT("vm-cache-native");
		asSMetadataParameter ParamsB[2] = {
			VMCacheContractsPrivate::MakeIntParam("a"),
			VMCacheContractsPrivate::MakeIntParam("b")
		};
		ParamsB[0].Type = FixtureB.Image->Primitive(ttInt);
		ParamsB[1].Type = FixtureB.Image->Primitive(ttInt);
		asCScriptFunction* AddB = FixtureB.DefineFunction(
			TEXT("Add"), TEXT(""), asFUNC_SYSTEM, FixtureB.Image->Primitive(ttInt), MakeArrayView(ParamsB, 2));
		asCScriptFunction* RunB = FixtureB.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureB.Image->Primitive(ttInt), {});
		asCGlobalProperty* GlobalB = VMCacheContractsPrivate::DefineGlobal(FixtureB, TEXT("Cell"));
		asCGlobalProperty* Unrelated = VMCacheContractsPrivate::DefineGlobal(FixtureB, TEXT("Keep"));
		ASSERT_THAT(IsTrue(FixtureB.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(RunB->GetStableKey() == RunKey));
		ASSERT_THAT(IsTrue(AddB->GetStableKey() == AddKey));
		ASSERT_THAT(IsTrue(GlobalB->GetStableKey() == GlobalKey));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->RegisterMetadataImage(FixtureB.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			EngineB.Ptr->BindNativeFunction(AddB->GetStableKey(), asFUNCTION(VMCacheContractsPrivate::GenericAdd), asCALL_GENERIC)));
		int32 StorageB = 9;
		int32 UnrelatedB = 0x22222222;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineB.Ptr->BindGlobalStorage(GlobalB->GetStableKey(), &StorageB)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineB.Ptr->BindGlobalStorage(Unrelated->GetStableKey(), &UnrelatedB)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* SnapshotB = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineB.Ptr->LinkByteCodeImage(*Decoded, SnapshotB)));
		ASSERT_THAT(IsTrue(SnapshotB->FindFunction(RunKey) == RunB));
		ASSERT_THAT(IsTrue(SnapshotB->FindFunction(AddKey) == AddB));
		ASSERT_THAT(IsTrue(VMCacheContractsPrivate::ExecuteInt(EngineB.Ptr, RunB, 42)));
		ASSERT_THAT(AreEqual(1, VMCacheContractsPrivate::GNativeCalls));
		ASSERT_THAT(AreEqual(20, VMCacheContractsPrivate::GNativeLastA));
		ASSERT_THAT(AreEqual(22, VMCacheContractsPrivate::GNativeLastB));
		ASSERT_THAT(AreEqual(0x22222222, UnrelatedB));
		if (SnapshotB) SnapshotB->Release();
	}

	TEST_METHOD(FuncdefAndMethodSignatureResolveOnB)
	{
		TArray<uint8> Bytes;
		asSStableKey RunKey;
		asSStableKey MethodKey;
		{
			FDetachedDefinitionFixture Fixture;
			Fixture.Scope = TEXT("vm-cache-funcdef");
			asCObjectType* Point = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
			Fixture.DefineProperty(*Point, "x", Fixture.Image->Primitive(ttInt));
			asCScriptFunction* Method = Fixture.DefineFunction(
				TEXT("Read"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {}, Point);
			asCScriptFunction* Signature = Fixture.DefineFunction(
				TEXT("Callback"), TEXT(""), asFUNC_CALLABLE_SIGNATURE, Fixture.Image->Primitive(ttInt), {});
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor TypeDescriptor;
			TypeDescriptor.StableScope = Fixture.Scope;
			TypeDescriptor.Name = TEXT("CallbackType");
			TypeDescriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::CallableType;
			asSStableKey TypeKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Fixture.Context->GetIdentityRegistry().InternNominalType(TypeDescriptor, TypeKey)));
			asCCallableType* CallableType = nullptr;
			ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
				Fixture.Image->CreateCallableType(TypeKey, "CallbackType", "", *Signature, CallableType)));
			asCScriptFunction* Run = Fixture.DefineFunction(
				TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			RunKey = Run->GetStableKey();
			MethodKey = Method->GetStableKey();
			FNativeEngineOwner EngineA;
			ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineA.Ptr->RegisterMetadataImage(Fixture.Image)));
			TUniquePtr<asCByteCodeImage> Image = VMCacheContractsPrivate::MakeReturn(Run, 42, Point, Method);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		}
		FDetachedDefinitionFixture FixtureB;
		FixtureB.Scope = TEXT("vm-cache-funcdef");
		asCObjectType* PointB = FixtureB.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		FixtureB.DefineProperty(*PointB, "x", FixtureB.Image->Primitive(ttInt));
		asCScriptFunction* MethodB = FixtureB.DefineFunction(
			TEXT("Read"), TEXT(""), asFUNC_SCRIPT, FixtureB.Image->Primitive(ttInt), {}, PointB);
		asCScriptFunction* SignatureB = FixtureB.DefineFunction(
			TEXT("Callback"), TEXT(""), asFUNC_CALLABLE_SIGNATURE, FixtureB.Image->Primitive(ttInt), {});
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor TypeDescriptorB;
		TypeDescriptorB.StableScope = FixtureB.Scope;
		TypeDescriptorB.Name = TEXT("CallbackType");
		TypeDescriptorB.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::CallableType;
		asSStableKey TypeKeyB;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
			FixtureB.Context->GetIdentityRegistry().InternNominalType(TypeDescriptorB, TypeKeyB)));
		asCCallableType* CallableTypeB = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			FixtureB.Image->CreateCallableType(TypeKeyB, "CallbackType", "", *SignatureB, CallableTypeB)));
		asCScriptFunction* RunB = FixtureB.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureB.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureB.FinalizeAndFreeze()));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->RegisterMetadataImage(FixtureB.Image)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* SnapshotB = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineB.Ptr->LinkByteCodeImage(*Decoded, SnapshotB)));
		ASSERT_THAT(IsTrue(SnapshotB->FindFunction(RunKey) == RunB));
		ASSERT_THAT(IsTrue(SnapshotB->FindFunction(MethodKey) == MethodB));
		ASSERT_THAT(IsTrue(VMCacheContractsPrivate::ExecuteInt(EngineB.Ptr, RunB, 42)));
		if (SnapshotB) SnapshotB->Release();
	}

	TEST_METHOD(MissingTypeRejectsWithoutInstalling)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Point = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		Fixture.DefineProperty(*Point, "x", Fixture.Image->Primitive(ttInt));
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = VMCacheContractsPrivate::MakeReturn(Run, 42, Point);
		FNativeEngineOwner Engine;
		asCScriptFunction* OnlyRun = Engine.Ptr->FindMetadataFunction(Run->GetStableKey());
		ASSERT_THAT(IsNull(OnlyRun));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingDeclaration, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		ASSERT_THAT(IsNull(Snapshot));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Run->GetStableKey())));
		ASSERT_THAT(IsNull(Engine.Ptr->FindMetadataType(Point->GetStableKey())));
	}

	TEST_METHOD(ChangedReturnSameFunctionKeyRejects)
	{
		FDetachedDefinitionFixture IntReturn;
		IntReturn.Scope = TEXT("vm-cache-return");
		asCScriptFunction* Left = IntReturn.DefineFunction(
			TEXT("Read"), TEXT(""), asFUNC_SCRIPT, IntReturn.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(IntReturn.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = VMCacheContractsPrivate::MakeReturn(Left, 42);
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		FDetachedDefinitionFixture DoubleReturn;
		DoubleReturn.Scope = TEXT("vm-cache-return");
		asCScriptFunction* Right = DoubleReturn.DefineFunction(
			TEXT("Read"), TEXT(""), asFUNC_SCRIPT, DoubleReturn.Image->Primitive(ttFloat64), {});
		ASSERT_THAT(IsTrue(DoubleReturn.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(Left->GetStableKey() == Right->GetStableKey()));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->RegisterMetadataImage(DoubleReturn.Image)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, EngineB.Ptr->LinkByteCodeImage(*Decoded, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(EngineB.Ptr->FindPublishedExecutable(Right->GetStableKey())));
	}

	TEST_METHOD(ChangedParameterModeRejects)
	{
		FDetachedDefinitionFixture InRef;
		InRef.Scope = TEXT("vm-cache-mode");
		asSMetadataParameter InParam = VMCacheContractsPrivate::MakeIntParam("value", asTM_INREF);
		InParam.Type = InRef.Image->Primitive(ttInt);
		InParam.Type.MakeReference(true);
		asCScriptFunction* Left = InRef.DefineFunction(
			TEXT("Write"), TEXT(""), asFUNC_SCRIPT, InRef.Image->Primitive(ttInt), MakeArrayView(&InParam, 1));
		ASSERT_THAT(IsTrue(InRef.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = VMCacheContractsPrivate::MakeReturn(Left, 42);
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		FDetachedDefinitionFixture OutRef;
		OutRef.Scope = TEXT("vm-cache-mode");
		asSMetadataParameter OutParam = VMCacheContractsPrivate::MakeIntParam("value", asTM_OUTREF);
		OutParam.Type = OutRef.Image->Primitive(ttInt);
		OutParam.Type.MakeReference(true);
		asCScriptFunction* Right = OutRef.DefineFunction(
			TEXT("Write"), TEXT(""), asFUNC_SCRIPT, OutRef.Image->Primitive(ttInt), MakeArrayView(&OutParam, 1));
		ASSERT_THAT(IsTrue(OutRef.FinalizeAndFreeze()));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->RegisterMetadataImage(OutRef.Image)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Failed = nullptr;
		const asEByteCodeLinkStatus Status = EngineB.Ptr->LinkByteCodeImage(*Decoded, Failed);
		ASSERT_THAT(IsTrue(Status == asEByteCodeLinkStatus::SchemaMismatch || Status == asEByteCodeLinkStatus::MissingDeclaration));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(EngineB.Ptr->FindPublishedExecutable(Right->GetStableKey())));
		ASSERT_THAT(IsNull(EngineB.Ptr->FindPublishedExecutable(Left->GetStableKey())));
	}

	TEST_METHOD(OffsetOnlyNativeLayoutRejects)
	{
		FDetachedDefinitionFixture FixtureA;
		FixtureA.Scope = TEXT("vm-cache-offset");
		asCObjectType* TypeA = VMCacheContractsPrivate::DefineNativeRecord(
			FixtureA, static_cast<int>(offsetof(VMCacheContractsPrivate::FCacheNativeRecord, Value)));
		asCScriptFunction* RunA = FixtureA.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureA.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureA.FinalizeAndFreeze()));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		asUINT PropSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*TypeA, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunA, RunSlot)));
		asCObjectProperty* PropertyA = TypeA->localProperties.GetLength() > 0
			? TypeA->localProperties[0]
			: (TypeA->properties.GetLength() > 0 ? TypeA->properties[0] : nullptr);
		ASSERT_THAT(IsNotNull(PropertyA));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.AddPropertyRequirement(*TypeA, *PropertyA, PropSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(RunA->GetStableKey(), RunA->GetStableKey(), 4, ByteArgumentDwords(*RunA))));
		asSByteCodeOperand Push = ByteImm32(42);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*RunA));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		FDetachedDefinitionFixture FixtureB;
		FixtureB.Scope = TEXT("vm-cache-offset");
		asCObjectType* TypeB = VMCacheContractsPrivate::DefineNativeRecord(FixtureB, 0);
		asCScriptFunction* RunB = FixtureB.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureB.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureB.FinalizeAndFreeze()));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->RegisterMetadataImage(FixtureB.Image)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, EngineB.Ptr->LinkByteCodeImage(*Decoded, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(EngineB.Ptr->FindPublishedExecutable(RunB->GetStableKey())));
	}

	TEST_METHOD(InjectedEqualDigestDifferentWitnessRejects)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Point = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		Fixture.DefineProperty(*Point, "x", Fixture.Image->Primitive(ttInt));
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		TUniquePtr<asCByteCodeImage> Image = VMCacheContractsPrivate::MakeReturn(Run, 42, Point);
		asSByteCodeRequirement* TypeReq = VMCacheContractsPrivate::FindRequirement(
			*Image, asEByteCodeRequirementRole::Type, Point->GetStableKey());
		ASSERT_THAT(IsNotNull(TypeReq));
		ASSERT_THAT(IsTrue(TypeReq->SchemaWitness.Num() > 0));
		TypeReq->SchemaWitness[0] ^= 0x5A;
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, Engine.Ptr->LinkByteCodeImage(*Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Run->GetStableKey())));
	}

	TEST_METHOD(FailedFingerprintQueryRejects)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Point = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		Fixture.DefineProperty(*Point, "x", Fixture.Image->Primitive(ttInt));
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		VMCacheContractsPrivate::FZeroDigest Provider;
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		TUniquePtr<asCByteCodeImage> Image = VMCacheContractsPrivate::MakeReturn(Run, 42, Point);
		Fixture.Image->SetFingerprintDigestProvider(&Provider);
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, Engine.Ptr->LinkByteCodeImage(*Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Run->GetStableKey())));
		Fixture.Image->SetFingerprintDigestProvider(nullptr);
	}

	TEST_METHOD(ForeignAttachedImageRejectsOnSecondEngine)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner EngineA;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineA.Ptr->RegisterMetadataImage(Fixture.Image)));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::ForeignEngine, EngineB.Ptr->RegisterMetadataImage(Fixture.Image)));
		TUniquePtr<asCByteCodeImage> Image = VMCacheContractsPrivate::MakeReturn(Run, 42);
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingDeclaration, EngineB.Ptr->LinkByteCodeImage(*Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
	}

	TEST_METHOD(MalformedPointerProfileRejects)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		TUniquePtr<asCByteCodeImage> Image = VMCacheContractsPrivate::MakeReturn(Run, 42);
		Image->PointerByteSize = 4;
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::InvalidArgument, Engine.Ptr->LinkByteCodeImage(*Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Run->GetStableKey())));
	}

	TEST_METHOD(LateMissingFunctionLeavesInstalledBodyAndSentinel)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCGlobalProperty* Global = VMCacheContractsPrivate::DefineGlobal(Fixture, TEXT("Cell"));
		ASSERT_THAT(IsNotNull(Global));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		int32 Sentinel = 0x11223344;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindGlobalStorage(Global->GetStableKey(), &Sentinel)));
		TUniquePtr<asCByteCodeImage> Good = VMCacheContractsPrivate::MakeReturn(Run, 42);
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Good, Snapshot)));
		ASSERT_THAT(IsTrue(VMCacheContractsPrivate::ExecuteInt(Engine.Ptr, Run, 42)));
		const auto Counts = Engine.Ptr->GetMetadataRegistrationCounts();
		TUniquePtr<asCByteCodeImage> Bad = VMCacheContractsPrivate::MakeTwoBodiesLateMissing(Run);
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreNotEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Bad, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetMetadataRegistrationCounts() == Counts));
		ASSERT_THAT(AreEqual(0x11223344, Sentinel));
		ASSERT_THAT(IsTrue(Engine.Ptr->FindBoundGlobalStorage(Global->GetStableKey()) == &Sentinel));
		ASSERT_THAT(IsNotNull(Engine.Ptr->FindPublishedExecutable(Run->GetStableKey())));
		ASSERT_THAT(IsTrue(VMCacheContractsPrivate::ExecuteInt(Engine.Ptr, Run, 42)));
		if (Snapshot) Snapshot->Release();
	}

	TEST_METHOD(DecodeDoesNotCreateDefinitionsOnB)
	{
		TArray<uint8> Bytes;
		asSStableKey RunKey;
		{
			FDetachedDefinitionFixture Fixture;
			asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			RunKey = Run->GetStableKey();
			TUniquePtr<asCByteCodeImage> Image = VMCacheContractsPrivate::MakeReturn(Run, 42);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		}
		FNativeEngineOwner EngineB;
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		ASSERT_THAT(IsNull(EngineB.Ptr->FindMetadataFunction(RunKey)));
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingDeclaration, EngineB.Ptr->LinkByteCodeImage(*Decoded, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(EngineB.Ptr->FindMetadataFunction(RunKey)));
		ASSERT_THAT(IsNull(EngineB.Ptr->FindPublishedExecutable(RunKey)));
	}

	TEST_METHOD(VersionOneBytesRejectDecode)
	{
		asCByteCodeImage Empty;
		Empty.PointerByteSize = 8;
		Empty.bFloatIsFloat64 = true;
		TArray<uint8> Encoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(Empty, Encoded)));
		ASSERT_THAT(IsTrue(Encoded.Num() > 5));
		Encoded[4] = 1;
		Encoded[5] = 0;
		TUniquePtr<asCByteCodeImage> Rejected;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::UnknownVersion, asCByteCodeImageCodec::Decode(Encoded, Rejected)));
		ASSERT_THAT(IsNull(Rejected.Get()));
	}
};

#endif
