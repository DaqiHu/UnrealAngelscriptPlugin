#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "as_datatype.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "Async/ParallelFor.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(AccessDefinitions, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FFixture
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		std::shared_ptr<asCMetadataImage> Image = asCMetadataImage::Create();
		asCObjectType* Owner = nullptr;
		asCScriptFunction* Method = nullptr;
		asCObjectProperty* Property = nullptr;
		bool Initialize()
		{
			if (Image->SetIdentityContext(Context) != asEMetadataResult::Succeeded || !AddType("Owner", "", Owner) || !AddMethod("Read", "", Owner, Method)) return false;
			return Image->AddProperty(*Owner, "Value", Image->Primitive(ttInt), Property) == asEMetadataResult::Succeeded;
		}
		bool AddType(const char* Name, const char* Namespace, asCObjectType*& Out)
		{
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = TEXT("access-definitions"); Descriptor.Name = UTF8_TO_TCHAR(Name); Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
			if (*Namespace) Descriptor.NamespacePath.Add(UTF8_TO_TCHAR(Namespace));
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			return Context->GetIdentityRegistry().InternNominalType(Descriptor, Key) == AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded &&
				Image->CreateObjectType(Key, Name, Namespace, asOBJ_REF | asOBJ_SCRIPT_OBJECT, Out) == asEMetadataResult::Succeeded;
		}
		bool AddMethod(const char* Name, const char* Namespace, asCObjectType* Type, asCScriptFunction*& Out)
		{
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Descriptor;
			Descriptor.StableScope = TEXT("access-definitions"); Descriptor.Name = UTF8_TO_TCHAR(Name);
			Descriptor.Kind = Type ? AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Method : AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global;
			if (Type) Descriptor.Owner = Type->GetStableKey();
			if (*Namespace) Descriptor.NamespacePath.Add(UTF8_TO_TCHAR(Namespace));
			Context->InternPrimitiveTypeUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Void, Descriptor.ReturnType);
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			return Context->GetIdentityRegistry().InternFunction(Descriptor, Key) == AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded &&
				Image->CreateFunction(Key, Name, Namespace, asFUNC_SCRIPT, Out) == asEMetadataResult::Succeeded &&
				Image->DefineFunction(*Out, Image->Primitive(ttVoid), {}, Type) == asEMetadataResult::Succeeded;
		}
		bool Freeze() { return Image->FinalizeLayouts() == asEMetadataResult::Succeeded && Image->Freeze() == asEMetadataResult::Succeeded && Image->ValidateFrozenDefinitions(); }
	};
	static asSAccessSpecifier Policy(const char* Name = "Trusted")
	{
		asSAccessSpecifier Result; Result.name = Name; Result.bIsPrivate = true;
		asSAccessPermission Permission; Permission.accessName = "Friend"; Permission.bReadOnly = true;
		Result.permissions.PushLast(Permission); return Result;
	}
public:
	TEST_METHOD(ActualOwnedPolicyBindsPropertiesAndFunctionsWithoutEngineIds)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Authored = Policy(); asSAccessSpecifier* Actual = nullptr;
		ASSERT_THAT(IsTrue(Fixture.Image->DefineAccessSpecifier(*Fixture.Owner, Authored, Actual) == asEMetadataResult::Succeeded));
		Authored.permissions[0].accessName = "ChangedCallerInput";
		ASSERT_THAT(IsTrue(Actual->permissions[0].accessName == "Friend"));
		ASSERT_THAT(IsTrue(Fixture.Owner->GetAccessSpecifier("Trusted") == Actual));
		ASSERT_THAT(IsTrue(Fixture.Image->SetPropertyAccess(*Fixture.Owner, *Fixture.Property, *Actual) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->SetFunctionAccess(*Fixture.Method, *Actual) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Property->accessSpecifier == Actual && Fixture.Method->accessSpecifier == Actual));
		ASSERT_THAT(IsTrue(Fixture.Freeze())); ASSERT_THAT(IsNull(Fixture.Method->GetEngine()));
		ASSERT_THAT(IsTrue(Fixture.Image->SetFunctionAccess(*Fixture.Method, *Actual) == asEMetadataResult::InvalidState));
		ASSERT_THAT(IsTrue(Fixture.Image->SetPropertyAccess(*Fixture.Owner, *Fixture.Property, *Actual) == asEMetadataResult::InvalidState));
	}
	TEST_METHOD(InvalidAndDuplicatePoliciesCannotPartiallyPublish)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize())); asSAccessSpecifier* Actual = nullptr;
		auto Invalid = Policy(""); ASSERT_THAT(IsTrue(Fixture.Image->DefineAccessSpecifier(*Fixture.Owner, Invalid, Actual) == asEMetadataResult::InvalidArgument));
		ASSERT_THAT(IsNull(Actual)); ASSERT_THAT(AreEqual(asUINT(0), Fixture.Owner->accessSpecifiers.GetLength()));
		Invalid = Policy(); Invalid.bIsProtected = true;
		ASSERT_THAT(IsTrue(Fixture.Image->DefineAccessSpecifier(*Fixture.Owner, Invalid, Actual) == asEMetadataResult::InvalidArgument));
		ASSERT_THAT(IsTrue(Fixture.Image->DefineAccessSpecifier(*Fixture.Owner, Policy(), Actual) == asEMetadataResult::Succeeded));
		asSAccessSpecifier* Rejected = Actual;
		ASSERT_THAT(IsTrue(Fixture.Image->DefineAccessSpecifier(*Fixture.Owner, Policy(), Rejected) == asEMetadataResult::DuplicateName));
		ASSERT_THAT(IsNull(Rejected)); ASSERT_THAT(AreEqual(asUINT(1), Fixture.Owner->accessSpecifiers.GetLength()));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
	}
	TEST_METHOD(UnownedAndUnrelatedPoliciesCannotBindActualMembers)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Unowned = Policy(); ASSERT_THAT(IsTrue(Fixture.Image->SetFunctionAccess(*Fixture.Method, Unowned) == asEMetadataResult::ForeignOwner));
		asCObjectType* Other = nullptr; ASSERT_THAT(IsTrue(Fixture.AddType("Other", "", Other)));
		asSAccessSpecifier* Unrelated = nullptr;
		ASSERT_THAT(IsTrue(Fixture.Image->DefineAccessSpecifier(*Other, Policy(), Unrelated) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->SetPropertyAccess(*Fixture.Owner, *Fixture.Property, *Unrelated) == asEMetadataResult::ForeignOwner));
		ASSERT_THAT(IsTrue(Fixture.Image->SetPropertyAccess(*Other, *Fixture.Property, *Unrelated) == asEMetadataResult::ForeignOwner));
		ASSERT_THAT(IsTrue(Fixture.Image->SetFunctionAccess(*Fixture.Method, *Unrelated) == asEMetadataResult::ForeignOwner));
		ASSERT_THAT(IsNull(Fixture.Method->accessSpecifier)); ASSERT_THAT(IsNull(Fixture.Property->accessSpecifier));
	}
	TEST_METHOD(FrozenPolicyContentsAndMemberRebindingAreAuthenticated)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize())); asSAccessSpecifier* First = nullptr; asSAccessSpecifier* Second = nullptr;
		ASSERT_THAT(IsTrue(Fixture.Image->DefineAccessSpecifier(*Fixture.Owner, Policy(), First) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->DefineAccessSpecifier(*Fixture.Owner, Policy("Other"), Second) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->SetFunctionAccess(*Fixture.Method, *First) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		First->permissions[0].bReadOnly = false; ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
		First->permissions[0].bReadOnly = true; ASSERT_THAT(IsTrue(Fixture.Image->ValidateFrozenDefinitions()));
		Fixture.Method->accessSpecifier = Second; ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
		Fixture.Method->accessSpecifier = First; ASSERT_THAT(IsTrue(Fixture.Image->ValidateFrozenDefinitions()));
		Fixture.Property->isPrivate = true; ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
	}
	TEST_METHOD(ActualDetachedCallerNamesAndNamespacesDrivePermissionUnion)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		asCObjectType* FriendType = nullptr; asCScriptFunction* Friend = nullptr; asCScriptFunction* NamespaceFriend = nullptr; asCScriptFunction* Stranger = nullptr;
		ASSERT_THAT(IsTrue(Fixture.AddType("Friend", "", FriendType) && Fixture.AddMethod("Read", "", FriendType, Friend)));
		ASSERT_THAT(IsTrue(Fixture.AddMethod("GlobalRead", "TrustedNamespace", nullptr, NamespaceFriend) && Fixture.AddMethod("Stranger", "", nullptr, Stranger)));
		auto Authored = Policy(); asSAccessPermission Edit; Edit.accessName = "TrustedNamespace"; Edit.bEditDefaults = true; Authored.permissions.PushLast(Edit);
		asSAccessSpecifier* Actual = nullptr; ASSERT_THAT(IsTrue(Fixture.Image->DefineAccessSpecifier(*Fixture.Owner, Authored, Actual) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		struct FCase { asCScriptFunction* Caller; bool Read; bool Write; bool Edit; };
		for (const auto& Case : {FCase{Friend, true, false, false}, FCase{NamespaceFriend, false, false, true}, FCase{Stranger, false, false, false}})
		{
			bool Read = false, Write = false, CanEdit = false; Actual->GetAllowedAccess(false, Case.Caller, Read, Write, CanEdit);
			ASSERT_THAT(AreEqual(Case.Read, Read)); ASSERT_THAT(AreEqual(Case.Write, Write)); ASSERT_THAT(AreEqual(Case.Edit, CanEdit));
		}
	}
	TEST_METHOD(TamperedFrozenPolicyCannotRegisterWithEngine)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize())); asSAccessSpecifier* Actual = nullptr;
		ASSERT_THAT(IsTrue(Fixture.Image->DefineAccessSpecifier(*Fixture.Owner, Policy(), Actual) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->SetPropertyAccess(*Fixture.Owner, *Fixture.Property, *Actual) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		Actual->permissions[0].bReadOnly = false;
		ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Fixture.Image) != asEMetadataRegistrationResult::Succeeded));
		Actual->permissions[0].bReadOnly = true;
		ASSERT_THAT(IsTrue(Fixture.Image->ValidateFrozenDefinitions()));
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Fixture.Image) == asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(Engine->FindMetadataType(Fixture.Owner->GetStableKey()) == Fixture.Owner));
	}

	TEST_METHOD(ConcurrentOwnedPolicyCreationHasNoEngineOrSharedNameRace)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		TArray<int32> Results; Results.SetNumZeroed(16);
		ParallelFor(16, [&](int32 Index)
		{
			const FString Name = FString::Printf(TEXT("Policy%d"), Index); auto Value = Policy(TCHAR_TO_UTF8(*Name));
			asSAccessSpecifier* Actual = nullptr;
			Results[Index] = Fixture.Image->DefineAccessSpecifier(*Fixture.Owner, Value, Actual) == asEMetadataResult::Succeeded && Actual ? 1 : 0;
		});
		for (const int32 Result : Results) ASSERT_THAT(AreEqual(1, Result));
		ASSERT_THAT(AreEqual(asUINT(16), Fixture.Owner->accessSpecifiers.GetLength())); ASSERT_THAT(IsTrue(Fixture.Freeze()));
	}

	TEST_METHOD(FrozenFieldTypeMutationsCannotValidateOrRegister)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize())); ASSERT_THAT(IsTrue(Fixture.Freeze()));
		const asCDataType Original = Fixture.Property->type;
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		const auto Before = Engine->GetMetadataRegistrationCounts();
		Fixture.Property->type = Fixture.Image->Primitive(ttInt64);
		ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Fixture.Image) != asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(Engine->GetMetadataRegistrationCounts() == Before));
		Fixture.Property->type = Fixture.Image->Primitive(ttFloat32);
		ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
		Fixture.Property->type = Original;
		ASSERT_THAT(IsTrue(Fixture.Image->ValidateFrozenDefinitions()));
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Fixture.Image) == asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(Engine->FindMetadataType(Fixture.Owner->GetStableKey()) == Fixture.Owner));
	}

	TEST_METHOD(FrozenFieldCannotGainAForeignTypeEdge)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize())); ASSERT_THAT(IsTrue(Fixture.Freeze()));
		auto ForeignContext = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		auto Foreign = asCMetadataImage::Create();
		ASSERT_THAT(IsTrue(Foreign->SetIdentityContext(ForeignContext) == asEMetadataResult::Succeeded));
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = TEXT("foreign-field"); Descriptor.Name = TEXT("Other"); Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
		AS_NAMESPACE_QUALIFIER asSStableKey Key; asCObjectType* Other = nullptr;
		ASSERT_THAT(IsTrue(ForeignContext->GetIdentityRegistry().InternNominalType(Descriptor, Key) == AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded));
		ASSERT_THAT(IsTrue(Foreign->CreateObjectType(Key, "Other", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Other) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Foreign->FinalizeLayouts() == asEMetadataResult::Succeeded && Foreign->Freeze() == asEMetadataResult::Succeeded));
		const asCDataType Original = Fixture.Property->type;
		Fixture.Property->type = asCDataType::CreateObjectHandle(Other, false);
		ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
		Fixture.Property->type = Original;
		ASSERT_THAT(IsTrue(Fixture.Image->ValidateFrozenDefinitions()));
	}

	TEST_METHOD(ExposedMethodObjectTypeMustMatchDeclarationOwner)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		asCObjectType* Other = nullptr; ASSERT_THAT(IsTrue(Fixture.AddType("Other", "", Other)));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		asCObjectType* Original = Fixture.Method->objectType;
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		const auto Before = Engine->GetMetadataRegistrationCounts();
		Fixture.Method->objectType = nullptr;
		ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Fixture.Image) != asEMetadataRegistrationResult::Succeeded));
		Fixture.Method->objectType = Other;
		ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
		auto ForeignContext = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		auto Foreign = asCMetadataImage::Create();
		ASSERT_THAT(IsTrue(Foreign->SetIdentityContext(ForeignContext) == asEMetadataResult::Succeeded));
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = TEXT("foreign-owner"); Descriptor.Name = TEXT("Foreign"); Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
		AS_NAMESPACE_QUALIFIER asSStableKey Key; asCObjectType* ForeignType = nullptr;
		ASSERT_THAT(IsTrue(ForeignContext->GetIdentityRegistry().InternNominalType(Descriptor, Key) == AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded));
		ASSERT_THAT(IsTrue(Foreign->CreateObjectType(Key, "Foreign", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, ForeignType) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Foreign->FinalizeLayouts() == asEMetadataResult::Succeeded && Foreign->Freeze() == asEMetadataResult::Succeeded));
		Fixture.Method->objectType = ForeignType;
		ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
		Fixture.Method->objectType = Original;
		ASSERT_THAT(IsTrue(Fixture.Image->ValidateFrozenDefinitions()));
		ASSERT_THAT(IsTrue(Engine->GetMetadataRegistrationCounts() == Before));
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Fixture.Image) == asEMetadataRegistrationResult::Succeeded));
	}
};
#endif
