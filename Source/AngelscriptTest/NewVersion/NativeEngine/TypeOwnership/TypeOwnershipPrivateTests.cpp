#include "CQTest.h"
#include "TypeOwnershipTestSupport.h"
#include "as_builder.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "frontend/as_type_context.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(Private, "Angelscript.UnitTest.NativeEngine.TypeOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FArrayHost
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCMetadataImage> Image = asCMetadataImage::Create();
		asCObjectType* Template = nullptr;

		bool Initialize()
		{
			if (Image->SetIdentityContext(Context) != asEMetadataResult::Succeeded) return false;
			asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = TEXT("type-ownership-private-host");
			Descriptor.Kind = asETypeDeclKind::Struct;
			Descriptor.Name = TEXT("array");
			Descriptor.GenericArity = 1;
			asSStableKey Key;
			if (Context->GetIdentityRegistry().InternNominalType(Descriptor, Key) != asETypeIdentityStatus::Succeeded)
			{
				return false;
			}
			if (Image->CreateObjectType(Key, "array", "", asOBJ_VALUE | asOBJ_TEMPLATE, Template) != asEMetadataResult::Succeeded)
			{
				return false;
			}
			if (Image->SetTypeDeclarationFacts(*Template, asETypeDeclKind::Struct, 1) != asEMetadataResult::Succeeded)
			{
				return false;
			}
			return Image->SetFixedTemplateLayout(*Template, 16, 8) == asEMetadataResult::Succeeded
				&& Image->FinalizeLayouts() == asEMetadataResult::Succeeded
				&& Image->Freeze() == asEMetadataResult::Succeeded;
		}
	};

	static asCObjectType* FindType(asCMetadataImage& Image, const char* Name)
	{
		return TypeOwnershipTest::FindImageType(Image, Name);
	}

public:
	TEST_METHOD(ScriptThingIsolation)
	{
		using namespace TypeOwnershipTest;
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> ContextA =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> ContextB =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		FSourceExecutionInput InputA(TEXT("class ScriptThing { int Value; }"), ContextA, nullptr);
		FSourceExecutionInput InputB(TEXT("class ScriptThing { int Value; }"), ContextB, nullptr);
		TUniquePtr<asCBuilder> BuilderA;
		TUniquePtr<asCBuilder> BuilderB;
		asCObjectType* TypeA = nullptr;
		asCObjectType* TypeB = nullptr;
		ASSERT_THAT(IsTrue(CompileScriptThing(TEXT("type-ownership-private-a"), InputA, BuilderA, TypeA)));
		ASSERT_THAT(IsTrue(CompileScriptThing(TEXT("type-ownership-private-b"), InputB, BuilderB, TypeB)));
		ASSERT_THAT(IsTrue(TypeA != TypeB));
		ASSERT_THAT(IsFalse(TypeA->GetStableKey() == TypeB->GetStableKey()));
		FNativeEngineOwner EngineA;
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			EngineA.Ptr->RegisterMetadataImage(BuilderA->TakeDefinitions())));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			EngineB.Ptr->RegisterMetadataImage(BuilderB->TakeDefinitions())));
		ASSERT_THAT(IsTrue(TypeA->GetEngine() == EngineA.Ptr));
		ASSERT_THAT(IsTrue(TypeB->GetEngine() == EngineB.Ptr));
		ASSERT_THAT(IsTrue(TypeA->GetTypeId() != TypeB->GetTypeId()));
		ASSERT_THAT(IsTrue(EngineA.Ptr->FindMetadataType("", "ScriptThing") == TypeA));
		ASSERT_THAT(IsTrue(EngineB.Ptr->FindMetadataType("", "ScriptThing") == TypeB));
		ASSERT_THAT(IsTrue(EngineA.Ptr->GetTypeInfoById(TypeA->GetTypeId()) == TypeA));
		ASSERT_THAT(IsNull(EngineB.Ptr->GetTypeInfoById(TypeA->GetTypeId())));
		ASSERT_THAT(IsNull(EngineA.Ptr->GetTypeInfoById(TypeB->GetTypeId())));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::InvalidArgument,
			EngineB.Ptr->RegisterMetadataImage(BuilderA->TakeDefinitions())));
	}

	TEST_METHOD(SpecializationStaysWithEngine)
	{
		FArrayHost HostA;
		FArrayHost HostB;
		ASSERT_THAT(IsTrue(HostA.Initialize()));
		ASSERT_THAT(IsTrue(HostB.Initialize()));
		FSourceExecutionInput InputA(TEXT("class ScriptThing { int[] Values; }"), HostA.Context, HostA.Image.Get());
		FSourceExecutionInput InputB(TEXT("class ScriptThing { int[] Values; }"), HostB.Context, HostB.Image.Get());
		InputA.Options.StableScope = TEXT("type-ownership-private-spec-a");
		InputB.Options.StableScope = TEXT("type-ownership-private-spec-b");
		TUniquePtr<asCBuilder> BuilderA = MakeUnique<asCBuilder>(InputA.Snapshot, InputA.Diagnostics, InputA.Options);
		TUniquePtr<asCBuilder> BuilderB = MakeUnique<asCBuilder>(InputB.Snapshot, InputB.Diagnostics, InputB.Options);
		ASSERT_THAT(IsTrue(BuilderA->RunThrough(), DescribeBuilderFailure(*BuilderA)));
		ASSERT_THAT(IsTrue(BuilderB->RunThrough(), DescribeBuilderFailure(*BuilderB)));
		asCObjectType* ThingA = FindType(*BuilderA->GetDefinitions(), "ScriptThing");
		asCObjectType* ThingB = FindType(*BuilderB->GetDefinitions(), "ScriptThing");
		ASSERT_THAT(IsNotNull(ThingA));
		ASSERT_THAT(IsNotNull(ThingB));
		asCObjectType* SpecA = static_cast<asCObjectType*>(ThingA->properties[0]->type.GetTypeInfo());
		asCObjectType* SpecB = static_cast<asCObjectType*>(ThingB->properties[0]->type.GetTypeInfo());
		ASSERT_THAT(IsNotNull(SpecA));
		ASSERT_THAT(IsNotNull(SpecB));
		ASSERT_THAT(IsNotNull(SpecA->GetMetadataTemplateDefinition()));
		ASSERT_THAT(IsTrue(SpecA != SpecB));
		FNativeEngineOwner EngineA;
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			EngineA.Ptr->RegisterMetadataImage(MoveTemp(HostA.Image))));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			EngineA.Ptr->RegisterMetadataImage(BuilderA->TakeDefinitions())));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			EngineB.Ptr->RegisterMetadataImage(MoveTemp(HostB.Image))));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			EngineB.Ptr->RegisterMetadataImage(BuilderB->TakeDefinitions())));
		ASSERT_THAT(IsTrue(SpecA->GetEngine() == EngineA.Ptr));
		ASSERT_THAT(IsTrue(SpecB->GetEngine() == EngineB.Ptr));
		ASSERT_THAT(IsTrue(EngineA.Ptr->FindMetadataType(SpecA->GetStableKey()) == SpecA));
		ASSERT_THAT(IsTrue(EngineB.Ptr->FindMetadataType(SpecB->GetStableKey()) == SpecB));
		ASSERT_THAT(IsTrue(EngineA.Ptr->FindMetadataType(SpecB->GetStableKey()) != SpecB));
		ASSERT_THAT(IsTrue(EngineB.Ptr->FindMetadataType(SpecA->GetStableKey()) != SpecA));
		ASSERT_THAT(IsNull(EngineB.Ptr->GetTypeInfoById(SpecA->GetTypeId())));
		ASSERT_THAT(IsNull(EngineA.Ptr->GetTypeInfoById(SpecB->GetTypeId())));
	}
};

#endif
