#include "CQTest.h"
#include "TypeOwnershipTestSupport.h"
#include "as_builder.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "frontend/AST/as_type_context.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(Private, "Angelscript.UnitTest.NativeEngine.TypeOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FArrayHost
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCDefinitions> Set = MakeUnique<asCDefinitions>();
		asCObjectType* Template = nullptr;

		bool Initialize()
		{
			if (Set->SetIdentityContext(Context) != asEDefinitionResult::Succeeded) return false;
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
			if (Set->CreateObjectType(Key, "array", "", asOBJ_VALUE | asOBJ_TEMPLATE, Template) != asEDefinitionResult::Succeeded)
			{
				return false;
			}
			if (Set->SetTypeDeclarationFacts(*Template, asETypeDeclKind::Struct, 1) != asEDefinitionResult::Succeeded)
			{
				return false;
			}
			return Set->SetFixedTemplateLayout(*Template, 16, 8) == asEDefinitionResult::Succeeded
				&& Set->FinalizeLayouts() == asEDefinitionResult::Succeeded
				&& Set->Freeze() == asEDefinitionResult::Succeeded;
		}
	};

	static asCObjectType* FindType(asCDefinitions& Definitions, const char* Name)
	{
		return TypeOwnershipTest::FindSetType(Definitions, Name);
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
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded,
			RegisterCompiledDefinitions(*EngineA.Ptr, *BuilderA)));
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded,
			RegisterCompiledDefinitions(*EngineB.Ptr, *BuilderB)));
		ASSERT_THAT(IsTrue(TypeA->GetEngine() == EngineA.Ptr));
		ASSERT_THAT(IsTrue(TypeB->GetEngine() == EngineB.Ptr));
		ASSERT_THAT(IsTrue(TypeA->GetTypeId() != TypeB->GetTypeId()));
		ASSERT_THAT(IsTrue(EngineA.Ptr->FindType("", "ScriptThing") == TypeA));
		ASSERT_THAT(IsTrue(EngineB.Ptr->FindType("", "ScriptThing") == TypeB));
		ASSERT_THAT(IsTrue(EngineA.Ptr->GetTypeInfoById(TypeA->GetTypeId()) == TypeA));
		ASSERT_THAT(IsNull(EngineB.Ptr->GetTypeInfoById(TypeA->GetTypeId())));
		ASSERT_THAT(IsNull(EngineA.Ptr->GetTypeInfoById(TypeB->GetTypeId())));
		ASSERT_THAT(AreEqual(asERegistrationResult::InvalidArgument,
			RegisterCompiledDefinitions(*EngineB.Ptr, *BuilderA)));
	}

	TEST_METHOD(SpecializationStaysWithEngine)
	{
		FArrayHost HostA;
		FArrayHost HostB;
		ASSERT_THAT(IsTrue(HostA.Initialize()));
		ASSERT_THAT(IsTrue(HostB.Initialize()));
		FSourceExecutionInput InputA(TEXT("class ScriptThing { int[] Values; }"), HostA.Context, HostA.Set.Get());
		FSourceExecutionInput InputB(TEXT("class ScriptThing { int[] Values; }"), HostB.Context, HostB.Set.Get());
		InputA.Options.StableScope = TEXT("type-ownership-private-spec-a");
		InputB.Options.StableScope = TEXT("type-ownership-private-spec-b");
		TUniquePtr<asCBuilder> BuilderA = MakeUnique<asCBuilder>(InputA.Sources, InputA.Options);
		TUniquePtr<asCBuilder> BuilderB = MakeUnique<asCBuilder>(InputB.Sources, InputB.Options);
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
		ASSERT_THAT(IsNotNull(SpecA->GetTemplateDefinition()));
		ASSERT_THAT(IsTrue(SpecA != SpecB));
		FNativeEngineOwner EngineA;
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded,
			EngineA.Ptr->RegisterExternalDefinitions(MoveTemp(HostA.Set))));
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded,
			RegisterCompiledDefinitions(*EngineA.Ptr, *BuilderA)));
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded,
			EngineB.Ptr->RegisterExternalDefinitions(MoveTemp(HostB.Set))));
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded,
			RegisterCompiledDefinitions(*EngineB.Ptr, *BuilderB)));
		ASSERT_THAT(IsTrue(SpecA->GetEngine() == EngineA.Ptr));
		ASSERT_THAT(IsTrue(SpecB->GetEngine() == EngineB.Ptr));
		ASSERT_THAT(IsTrue(EngineA.Ptr->FindType(SpecA->GetStableKey()) == SpecA));
		ASSERT_THAT(IsTrue(EngineB.Ptr->FindType(SpecB->GetStableKey()) == SpecB));
		ASSERT_THAT(IsTrue(EngineA.Ptr->FindType(SpecB->GetStableKey()) != SpecB));
		ASSERT_THAT(IsTrue(EngineB.Ptr->FindType(SpecA->GetStableKey()) != SpecA));
		ASSERT_THAT(IsNull(EngineB.Ptr->GetTypeInfoById(SpecA->GetTypeId())));
		ASSERT_THAT(IsNull(EngineA.Ptr->GetTypeInfoById(SpecB->GetTypeId())));
	}
};

#endif
