#include "CQTest.h"
#include "as_metadata_image.h"
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
		std::shared_ptr<asCMetadataImage> Image = asCMetadataImage::Create();
		AS_NAMESPACE_QUALIFIER asSStableKey Key, TypeKey;
		asCGlobalProperty* Global = nullptr;
		asEMetadataResult Create(const TCHAR* Name = TEXT("Answer"), const TCHAR* Scope = TEXT("N"))
		{
Image->SetIdentityContext(Context);
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
			return Image->CreateGlobalProperty(Key, TypeKey, TCHAR_TO_UTF8(Name), TCHAR_TO_UTF8(Scope), Image->Primitive(ttInt, true), Global);
		}
		bool Freeze() { return Image->FinalizeLayouts() == asEMetadataResult::Succeeded && Image->Freeze() == asEMetadataResult::Succeeded; }
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
		ASSERT_THAT(AreEqual(1, Fixture.Image->GetGlobalProperties().Num()));
		ASSERT_THAT(IsTrue(Fixture.Global == Fixture.Image->FindGlobalProperty(Fixture.Key)));
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
		std::weak_ptr<asCMetadataImage> Weak = Fixture.Image;
		Fixture.Global->AddRef();
		{
			FEngine Engine;
			ASSERT_THAT(IsTrue(Engine.Ptr->RegisterMetadataImage(Fixture.Image) == asEMetadataRegistrationResult::Succeeded));
			ASSERT_THAT(IsTrue(Fixture.Global == Engine.Ptr->FindMetadataGlobalProperty("N", "Answer")));
			Fixture.Image.reset();
		}
		ASSERT_THAT(IsFalse(Weak.expired()));
		ASSERT_THAT(IsTrue(Fixture.Global->name == "Answer"));
		ASSERT_THAT(IsTrue(Fixture.Global->GetMetadataImage()->GetState() == asEMetadataImageState::Retired));
		Fixture.Global->Release();
		ASSERT_THAT(IsTrue(Weak.expired()));
	}
	TEST_METHOD(MutableGlobalAndVoidTypeAreRejectedWithoutPublication)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEMetadataResult::Succeeded));
		auto Image = asCMetadataImage::Create(); Image->SetIdentityContext(Fixture.Context);
		asCGlobalProperty* Rejected = nullptr;
		ASSERT_THAT(IsTrue(Image->CreateGlobalProperty(Fixture.Key, Fixture.TypeKey, "Answer", "N", Image->Primitive(ttInt), Rejected) == asEMetadataResult::InvalidSignature));
		ASSERT_THAT(IsNull(Rejected));
		ASSERT_THAT(IsTrue(Image->CreateGlobalProperty(Fixture.Key, Fixture.TypeKey, "Answer", "N", Image->Primitive(ttVoid, true), Rejected) == asEMetadataResult::InvalidSignature));
		ASSERT_THAT(AreEqual(0, Image->GetGlobalProperties().Num()));
	}
	TEST_METHOD(WrongKeyRoleNameNamespaceAndTypeAreRejected)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEMetadataResult::Succeeded));
		for (int Case = 0; Case < 4; ++Case)
		{
			auto Image = asCMetadataImage::Create(); Image->SetIdentityContext(Fixture.Context);
			asCGlobalProperty* Rejected = nullptr;
			const auto Status = Image->CreateGlobalProperty(Case == 0 ? Fixture.TypeKey : Fixture.Key, Fixture.TypeKey,
				Case == 1 ? "Other" : "Answer", Case == 2 ? "Other" : "N", Image->Primitive(Case == 3 ? ttFloat32 : ttInt, true), Rejected);
			ASSERT_THAT(IsTrue(Status == asEMetadataResult::InvalidSignature));
			ASSERT_THAT(IsNull(Rejected));
			ASSERT_THAT(AreEqual(0, Image->GetGlobalProperties().Num()));
		}
	}
	TEST_METHOD(FrozenGlobalMutationIsRejectedByRegistration)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		Fixture.Global->type = Fixture.Image->Primitive(ttFloat32, true);
		FEngine Engine;
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterMetadataImage(Fixture.Image) == asEMetadataRegistrationResult::InvalidIdentity));
		ASSERT_THAT(IsNull(Engine.Ptr->FindMetadataGlobalProperty("N", "Answer")));
		ASSERT_THAT(IsTrue(Fixture.Image->GetState() == asEMetadataImageState::Frozen));
	}
	TEST_METHOD(DuplicateGlobalRegistrationDoesNotPublishNewImage)
	{
		FFixture First, Second;
		ASSERT_THAT(IsTrue(First.Create() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Second.Create() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(First.Freeze() && Second.Freeze()));
		FEngine Engine;
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterMetadataImage(First.Image) == asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterMetadataImage(Second.Image) == asEMetadataRegistrationResult::IdentityConflict));
		ASSERT_THAT(IsTrue(First.Global == Engine.Ptr->FindMetadataGlobalProperty("N", "Answer")));
		ASSERT_THAT(IsTrue(Second.Image->GetState() == asEMetadataImageState::Frozen));
	}
	TEST_METHOD(GlobalExternalReferencesAreConcurrentWithoutGraphCycles)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Create() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		std::weak_ptr<asCMetadataImage> Weak = Fixture.Image;
		std::vector<std::thread> Workers;
		for (int Index = 0; Index < 8; ++Index) Workers.emplace_back([&Fixture]() {
			for (int Iteration = 0; Iteration < 1000; ++Iteration) { Fixture.Global->AddRef(); Fixture.Global->Release(); }
		});
		for (auto& Worker : Workers) Worker.join();
		Fixture.Image.reset();
		ASSERT_THAT(IsTrue(Weak.expired()));
	}
};
#endif
