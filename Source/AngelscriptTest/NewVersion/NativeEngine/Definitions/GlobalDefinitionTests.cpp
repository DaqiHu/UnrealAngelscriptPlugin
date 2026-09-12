#include "CQTest.h"
#include "as_module_definition_set.h"
#include "as_property.h"
#include "as_scriptengine.h"
#include "frontend/as_type_context.h"
#include <thread>
#include <vector>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(GlobalDefinitions, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FFixture
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCModuleDefinitionSet> Set = asCModuleDefinitionSet::Create();
		AS_NAMESPACE_QUALIFIER asSStableKey Key, TypeKey;
		asCGlobalProperty* Global = nullptr;
		asEMetadataResult Create(const TCHAR* Name = TEXT("Answer"), const TCHAR* Scope = TEXT("N"))
		{
Set->SetIdentityContext(Context);
			auto& Registry = Context->GetIdentityRegistry();
			asSDeclarationIdentityDescriptor Namespace;
			Namespace.StableScope = TEXT("global-tests"); Namespace.Kind = asEDeclarationIdentityKind::Namespace; Namespace.Name = Scope;
			asSStableKey Owner;
			if (Registry.InternDeclaration(Namespace, Owner) != asETypeIdentityStatus::Succeeded) return asEMetadataResult::InvalidArgument;
			asSDeclarationIdentityDescriptor Declaration;
			Declaration.StableScope = Namespace.StableScope; Declaration.Kind = asEDeclarationIdentityKind::GlobalVariable;
			Declaration.Name = Name; Declaration.Owner = Owner;
			if (Registry.InternDeclaration(Declaration, Key) != asETypeIdentityStatus::Succeeded) return asEMetadataResult::InvalidArgument;
			asSStableTypeUseDescriptor Type;
			Type.Kind = asETypeUseKind::Primitive; Type.Primitive = asEPrimitiveType::Int32; Type.Qualifiers = asETypeUseQualifiers::Const;
			if (Registry.InternTypeUse(Type, TypeKey) != asETypeIdentityStatus::Succeeded) return asEMetadataResult::InvalidArgument;
			return Set->CreateGlobalProperty(Key, TypeKey, TCHAR_TO_UTF8(Name), TCHAR_TO_UTF8(Scope), Set->Primitive(ttInt, true), Global);
		}
		bool Freeze() { return Set->FinalizeLayouts() == asEMetadataResult::Succeeded && Set->Freeze() == asEMetadataResult::Succeeded; }
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
		ASSERT_THAT(IsTrue(Fixture.Create() == asEMetadataResult::Succeeded));
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
		ASSERT_THAT(IsTrue(Fixture.Create() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		asCGlobalProperty* Global = Fixture.Global;
		{
			FEngine Engine;
			ASSERT_THAT(IsTrue(Engine.Ptr->InstallDefinitionSet(MoveTemp(Fixture.Set)) == asEMetadataRegistrationResult::Succeeded));
			ASSERT_THAT(IsTrue(Global == Engine.Ptr->FindMetadataGlobalProperty("N", "Answer")));
			ASSERT_THAT(IsTrue(Global->name == "Answer"));
			ASSERT_THAT(IsNull(Fixture.Set.Get()));
		}
	}
	TEST_METHOD(MutableGlobalAndVoidTypeAreRejectedWithoutPublication)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEMetadataResult::Succeeded));
		auto Set = asCModuleDefinitionSet::Create(); Set->SetIdentityContext(Fixture.Context);
		asCGlobalProperty* Rejected = nullptr;
		ASSERT_THAT(IsTrue(Set->CreateGlobalProperty(Fixture.Key, Fixture.TypeKey, "Answer", "N", Set->Primitive(ttInt), Rejected) == asEMetadataResult::InvalidSignature));
		ASSERT_THAT(IsNull(Rejected));
		ASSERT_THAT(IsTrue(Set->CreateGlobalProperty(Fixture.Key, Fixture.TypeKey, "Answer", "N", Set->Primitive(ttVoid, true), Rejected) == asEMetadataResult::InvalidSignature));
		ASSERT_THAT(AreEqual(0, Set->GetGlobalProperties().Num()));
	}
	TEST_METHOD(WrongKeyRoleNameNamespaceAndTypeAreRejected)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEMetadataResult::Succeeded));
		for (int Case = 0; Case < 4; ++Case)
		{
			auto Set = asCModuleDefinitionSet::Create(); Set->SetIdentityContext(Fixture.Context);
			asCGlobalProperty* Rejected = nullptr;
			const auto Status = Set->CreateGlobalProperty(Case == 0 ? Fixture.TypeKey : Fixture.Key, Fixture.TypeKey,
				Case == 1 ? "Other" : "Answer", Case == 2 ? "Other" : "N", Set->Primitive(Case == 3 ? ttFloat32 : ttInt, true), Rejected);
			ASSERT_THAT(IsTrue(Status == asEMetadataResult::InvalidSignature));
			ASSERT_THAT(IsNull(Rejected));
			ASSERT_THAT(AreEqual(0, Set->GetGlobalProperties().Num()));
		}
	}
	TEST_METHOD(FrozenGlobalMutationIsRejectedByRegistration)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		Fixture.Global->type = Fixture.Set->Primitive(ttFloat32, true);
		FEngine Engine;
		ASSERT_THAT(IsTrue(Engine.Ptr->InstallDefinitionSet(MoveTemp(Fixture.Set)) == asEMetadataRegistrationResult::InvalidIdentity));
		ASSERT_THAT(IsNull(Engine.Ptr->FindMetadataGlobalProperty("N", "Answer")));
		ASSERT_THAT(IsNull(Fixture.Set.Get()));
	}
	TEST_METHOD(DuplicateGlobalRegistrationDoesNotPublishNewImage)
	{
		FFixture First, Second;
		ASSERT_THAT(IsTrue(First.Create() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Second.Create() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(First.Freeze() && Second.Freeze()));
		FEngine Engine;
		ASSERT_THAT(IsTrue(Engine.Ptr->InstallDefinitionSet(MoveTemp(First.Set)) == asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(Engine.Ptr->InstallDefinitionSet(MoveTemp(Second.Set)) == asEMetadataRegistrationResult::IdentityConflict));
		ASSERT_THAT(IsTrue(First.Global == Engine.Ptr->FindMetadataGlobalProperty("N", "Answer")));
		ASSERT_THAT(IsNull(Second.Set.Get()));
	}
	TEST_METHOD(GlobalExternalReferencesAreConcurrentWithoutGraphCycles)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEMetadataResult::Succeeded));
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
