#include "CQTest.h"
#include "as_definitions.h"
#include "as_property.h"
#include "as_scriptengine.h"
#include "frontend/AST/as_type_context.h"
#include <thread>
#include <vector>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(GlobalDefinitions, "Angelscript.UnitTest.NativeEngine.Definitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FFixture
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCDefinitions> Set = asCDefinitions::Create();
		AS_NAMESPACE_QUALIFIER asSStableKey Key, TypeKey;
		asCGlobalProperty* Global = nullptr;
		asEDefinitionResult Create(const TCHAR* Name = TEXT("Answer"), const TCHAR* Scope = TEXT("N"))
		{
Set->SetIdentityContext(Context);
			auto& Registry = Context->GetIdentityRegistry();
			asSDeclarationIdentityDescriptor Namespace;
			Namespace.StableScope = TEXT("global-tests"); Namespace.Kind = asEDeclarationIdentityKind::Namespace; Namespace.Name = Scope;
			asSStableKey Owner;
			if (Registry.InternDeclaration(Namespace, Owner) != asETypeIdentityStatus::Succeeded) return asEDefinitionResult::InvalidArgument;
			asSDeclarationIdentityDescriptor Declaration;
			Declaration.StableScope = Namespace.StableScope; Declaration.Kind = asEDeclarationIdentityKind::GlobalVariable;
			Declaration.Name = Name; Declaration.Owner = Owner;
			if (Registry.InternDeclaration(Declaration, Key) != asETypeIdentityStatus::Succeeded) return asEDefinitionResult::InvalidArgument;
			asSStableTypeUseDescriptor Type;
			Type.Kind = asETypeUseKind::Primitive; Type.Primitive = asEPrimitiveType::Int32; Type.Qualifiers = asETypeUseQualifiers::Const;
			if (Registry.InternTypeUse(Type, TypeKey) != asETypeIdentityStatus::Succeeded) return asEDefinitionResult::InvalidArgument;
			return Set->CreateGlobalProperty(Key, TypeKey, TCHAR_TO_UTF8(Name), TCHAR_TO_UTF8(Scope), Set->Primitive(ttInt, true), Global);
		}
		bool Freeze() { return Set->FinalizeLayouts() == asEDefinitionResult::Succeeded && Set->Freeze() == asEDefinitionResult::Succeeded; }
	};
	struct FEngine
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FEngine() { if (Ptr) Ptr->ShutDownAndRelease(); }
	};
public:
	TEST_METHOD(DetachedGlobalMetadataDoesNotAllocateExecutableStorage)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		ASSERT_THAT(AreEqual(1, Fixture.Set->GetGlobalProperties().Num()));
		ASSERT_THAT(IsTrue(Fixture.Global == Fixture.Set->FindGlobalProperty(Fixture.Key)));
		ASSERT_THAT(IsTrue(Fixture.Global->GetStableKey() == Fixture.Key));
		ASSERT_THAT(IsTrue(Fixture.Global->type.IsReadOnly()));
		ASSERT_THAT(IsTrue(Fixture.Global->type.GetTokenType() == ttInt));
		ASSERT_THAT(IsNull(Fixture.Global->GetAddressOfValue()));
		ASSERT_THAT(IsNull(Fixture.Global->GetRegisteredAddress()));
		ASSERT_THAT(IsNull(Fixture.Global->GetInitFunc()));
	}
	TEST_METHOD(GlobalOnlyLeaseRetainsImageAfterProducerAndEngineDestruction)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		asCGlobalProperty* Global = Fixture.Global;
		{
			FEngine Engine;
			ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Fixture.Set)) == asERegistrationResult::Succeeded));
			ASSERT_THAT(IsTrue(Global == Engine.Ptr->FindGlobalProperty("N", "Answer")));
			ASSERT_THAT(IsTrue(Global->name == "Answer"));
			ASSERT_THAT(IsNull(Fixture.Set.Get()));
		}
	}
	TEST_METHOD(MutableGlobalAndVoidTypeAreRejectedWithoutPublication)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEDefinitionResult::Succeeded));
		auto Set = asCDefinitions::Create(); Set->SetIdentityContext(Fixture.Context);
		asCGlobalProperty* Rejected = nullptr;
		ASSERT_THAT(IsTrue(Set->CreateGlobalProperty(Fixture.Key, Fixture.TypeKey, "Answer", "N", Set->Primitive(ttInt), Rejected) == asEDefinitionResult::InvalidSignature));
		ASSERT_THAT(IsNull(Rejected));
		ASSERT_THAT(IsTrue(Set->CreateGlobalProperty(Fixture.Key, Fixture.TypeKey, "Answer", "N", Set->Primitive(ttVoid, true), Rejected) == asEDefinitionResult::InvalidSignature));
		ASSERT_THAT(AreEqual(0, Set->GetGlobalProperties().Num()));
	}
	TEST_METHOD(WrongKeyRoleNameNamespaceAndTypeAreRejected)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEDefinitionResult::Succeeded));
		for (int Case = 0; Case < 4; ++Case)
		{
			auto Set = asCDefinitions::Create(); Set->SetIdentityContext(Fixture.Context);
			asCGlobalProperty* Rejected = nullptr;
			const auto Status = Set->CreateGlobalProperty(Case == 0 ? Fixture.TypeKey : Fixture.Key, Fixture.TypeKey,
				Case == 1 ? "Other" : "Answer", Case == 2 ? "Other" : "N", Set->Primitive(Case == 3 ? ttFloat32 : ttInt, true), Rejected);
			ASSERT_THAT(IsTrue(Status == asEDefinitionResult::InvalidSignature));
			ASSERT_THAT(IsNull(Rejected));
			ASSERT_THAT(AreEqual(0, Set->GetGlobalProperties().Num()));
		}
	}
	TEST_METHOD(FrozenGlobalMutationIsRejectedByRegistration)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		Fixture.Global->type = Fixture.Set->Primitive(ttFloat32, true);
		FEngine Engine;
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Fixture.Set)) == asERegistrationResult::InvalidIdentity));
		ASSERT_THAT(IsNull(Engine.Ptr->FindGlobalProperty("N", "Answer")));
		ASSERT_THAT(IsNull(Fixture.Set.Get()));
	}
	TEST_METHOD(DuplicateGlobalRegistrationDoesNotPublishNewImage)
	{
		FFixture First, Second;
		ASSERT_THAT(IsTrue(First.Create() == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Second.Create() == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(First.Freeze() && Second.Freeze()));
		FEngine Engine;
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(First.Set)) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Second.Set)) == asERegistrationResult::IdentityConflict));
		ASSERT_THAT(IsTrue(First.Global == Engine.Ptr->FindGlobalProperty("N", "Answer")));
		ASSERT_THAT(IsNull(Second.Set.Get()));
	}
	TEST_METHOD(GlobalExternalReferencesAreConcurrentWithoutGraphCycles)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		std::vector<std::thread> Workers;
		for (int Index = 0; Index < 8; ++Index) Workers.emplace_back([&Fixture]() {
			for (int Iteration = 0; Iteration < 1000; ++Iteration) { Fixture.Global->AddRef(); Fixture.Global->Release(); }
		});
		for (auto& Worker : Workers) Worker.join();
		ASSERT_THAT(IsNotNull(Fixture.Set.Get()));
		ASSERT_THAT(IsTrue(Fixture.Global->name == "Answer"));
	}
};
#endif
