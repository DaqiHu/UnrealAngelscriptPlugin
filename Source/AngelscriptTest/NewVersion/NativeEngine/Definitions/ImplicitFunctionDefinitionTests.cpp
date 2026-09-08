#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "../NativeEngineTestSupport.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_scriptengine.h"
#include "frontend/as_type_context.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(ImplicitFunctionDefinitions, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FFixture
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		std::shared_ptr<asCMetadataImage> Image = asCMetadataImage::Create();
		AS_NAMESPACE_QUALIFIER asSStableKey Void, Int, EnclosingKey;
		asCScriptFunction* Enclosing = nullptr;
		bool Initialize()
		{
if (Image->SetIdentityContext(Context) != asEMetadataResult::Succeeded) return false;
			Context->InternPrimitiveTypeUse(asEPrimitiveType::Void, Void);
			Context->InternPrimitiveTypeUse(asEPrimitiveType::Int32, Int);
			asSFunctionIdentityDescriptor Identity;
			Identity.StableScope = TEXT("implicit-definition-test"); Identity.Name = TEXT("Enclosing");
			Identity.Kind = asEFunctionIdentityKind::Global; Identity.ReturnType = Void;
			return Context->GetIdentityRegistry().InternFunction(Identity, EnclosingKey) == asETypeIdentityStatus::Succeeded &&
				Image->CreateFunction(EnclosingKey, "Enclosing", "", asFUNC_SCRIPT, Enclosing) == asEMetadataResult::Succeeded &&
				Image->DefineFunction(*Enclosing, Image->Primitive(ttVoid), {}) == asEMetadataResult::Succeeded;
		}
	};
public:
	TEST_METHOD(NamedFunctionIdentityRemainsStableAcrossContexts)
	{
		FFixture First, Second;
		ASSERT_THAT(IsTrue(First.Initialize() && Second.Initialize()));
		ASSERT_THAT(IsTrue(First.EnclosingKey == Second.EnclosingKey));
		asSFunctionIdentityDescriptor Observed;
		ASSERT_THAT(IsTrue(First.Context->GetIdentityRegistry().ResolveFunction(First.EnclosingKey, Observed) == asETypeIdentityStatus::Succeeded));
		ASSERT_THAT(IsTrue(Observed.Name == TEXT("Enclosing") && Observed.Owner.IsZero()));
	}
	TEST_METHOD(RetiredAnonymousKindCannotAcquireAFunctionIdentity)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		asSFunctionIdentityDescriptor Identity;
		Identity.StableScope = TEXT("implicit-definition-test");
		Identity.Kind = static_cast<asEFunctionIdentityKind>(7); Identity.ReturnType = Fixture.Int;
		asSStableKey Rejected;
		ASSERT_THAT(IsTrue(Fixture.Context->GetIdentityRegistry().InternFunction(Identity, Rejected) == asETypeIdentityStatus::InvalidDescriptor));
		Identity.Name = TEXT("Disguised");
		ASSERT_THAT(IsTrue(Fixture.Context->GetIdentityRegistry().InternFunction(Identity, Rejected) == asETypeIdentityStatus::InvalidDescriptor));
		ASSERT_THAT(IsTrue(Rejected.IsZero()));
	}
	TEST_METHOD(NamedFunctionRegistersWithSamePointerAndRetainsItsDefinition)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		ASSERT_THAT(IsTrue(Fixture.Image->SetFunctionDeclarationFacts(*Fixture.Enclosing, static_cast<asEFunctionIdentityKind>(7)) == asEMetadataResult::InvalidArgument));
		ASSERT_THAT(IsTrue(Fixture.Image->FinalizeLayouts() == asEMetadataResult::Succeeded && Fixture.Image->Freeze() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->ValidateFrozenDefinitions()));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Fixture.Image) == asEMetadataRegistrationResult::Succeeded));
		asCScriptFunction* Retained = Fixture.Enclosing;
		ASSERT_THAT(IsTrue(Engine->FindMetadataFunction(Fixture.EnclosingKey) == Retained));
		Fixture.Image.reset();
		ASSERT_THAT(IsTrue(Retained->GetStableKey() == Fixture.EnclosingKey && Retained->name == "Enclosing"));
	}
	TEST_METHOD(ClassDefaultHasAnOwnerButIsNotAnOrdinaryMethod)
	{
FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		asSNominalTypeDescriptor Record; Record.StableScope = TEXT("implicit-definition-test"); Record.Name = TEXT("Record"); Record.Kind = asETypeDeclKind::Class;
		asSStableKey RecordKey, FunctionKey; asCObjectType* Type = nullptr; asCScriptFunction* Function = nullptr;
		ASSERT_THAT(IsTrue(Fixture.Context->GetIdentityRegistry().InternNominalType(Record, RecordKey) == asETypeIdentityStatus::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->CreateObjectType(RecordKey, "Record", "", asOBJ_SCRIPT_OBJECT | asOBJ_REF, Type) == asEMetadataResult::Succeeded));
		asSFunctionIdentityDescriptor Identity; Identity.StableScope = Record.StableScope; Identity.Owner = RecordKey;
		Identity.Kind = asEFunctionIdentityKind::ClassDefault; Identity.ReturnType = Fixture.Void;
		ASSERT_THAT(IsTrue(Fixture.Context->GetIdentityRegistry().InternFunction(Identity, FunctionKey) == asETypeIdentityStatus::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->CreateFunction(FunctionKey, "", "", asFUNC_SCRIPT, Function) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->DefineFunction(*Function, Fixture.Image->Primitive(ttVoid), {}, Type) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->SetFunctionDeclarationFacts(*Function, asEFunctionIdentityKind::ClassDefault, Type) == asEMetadataResult::Succeeded));
		ASSERT_THAT(AreEqual(asUINT(0), Type->GetMethodCount()));
		ASSERT_THAT(IsTrue(Fixture.Image->FinalizeLayouts() == asEMetadataResult::Succeeded && Fixture.Image->Freeze() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->ValidateFrozenDefinitions()));
		Identity.ReturnType = Fixture.Int; asSStableKey Rejected;
		ASSERT_THAT(IsTrue(Fixture.Context->GetIdentityRegistry().InternFunction(Identity, Rejected) != asETypeIdentityStatus::Succeeded));
	}
};
#endif
