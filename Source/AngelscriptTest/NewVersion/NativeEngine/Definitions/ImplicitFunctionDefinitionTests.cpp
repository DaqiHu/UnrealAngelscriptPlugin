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
		AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Lambda(asUINT Ordinal = 0) const
		{
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Identity;
			Identity.StableScope = TEXT("implicit-definition-test"); Identity.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Lambda;
			Identity.ReturnType = Int; Identity.EnclosingFunction = EnclosingKey; Identity.LambdaOrdinal = Ordinal; return Identity;
		}
	};
public:
	TEST_METHOD(LexicalOriginAndOrdinalAreStableWithoutFabricatedNames)
	{
FFixture First, Second; ASSERT_THAT(IsTrue(First.Initialize() && Second.Initialize()));
		asSStableKey A, B, Repeated;
		ASSERT_THAT(IsTrue(First.Context->GetIdentityRegistry().InternFunction(First.Lambda(0), A) == asETypeIdentityStatus::Succeeded));
		ASSERT_THAT(IsTrue(First.Context->GetIdentityRegistry().InternFunction(First.Lambda(1), B) == asETypeIdentityStatus::Succeeded));
		ASSERT_THAT(IsTrue(Second.Context->GetIdentityRegistry().InternFunction(Second.Lambda(0), Repeated) == asETypeIdentityStatus::Succeeded));
		ASSERT_THAT(IsTrue(A == Repeated && A != B));
		asSFunctionIdentityDescriptor Observed;
		ASSERT_THAT(IsTrue(First.Context->GetIdentityRegistry().ResolveFunction(A, Observed) == asETypeIdentityStatus::Succeeded));
		ASSERT_THAT(IsTrue(Observed.Name.IsEmpty() && Observed.Owner.IsZero() && Observed.EnclosingFunction == First.EnclosingKey));
	}
	TEST_METHOD(MissingForeignOrWrongRoleOriginCannotAcquireAFunctionIdentity)
	{
FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Identity = Fixture.Lambda(); asSStableKey Rejected;
		Identity.EnclosingFunction = {};
		ASSERT_THAT(IsTrue(Fixture.Context->GetIdentityRegistry().InternFunction(Identity, Rejected) == asETypeIdentityStatus::NonCacheable));
		Identity = Fixture.Lambda(); Identity.EnclosingFunction = Fixture.Int;
		ASSERT_THAT(IsTrue(Fixture.Context->GetIdentityRegistry().InternFunction(Identity, Rejected) != asETypeIdentityStatus::Succeeded));
		Identity = Fixture.Lambda(); Identity.StableScope = TEXT("foreign-scope");
		ASSERT_THAT(IsTrue(Fixture.Context->GetIdentityRegistry().InternFunction(Identity, Rejected) != asETypeIdentityStatus::Succeeded));
		Identity = Fixture.Lambda(); Identity.Name = TEXT("$lambda");
		ASSERT_THAT(IsTrue(Fixture.Context->GetIdentityRegistry().InternFunction(Identity, Rejected) != asETypeIdentityStatus::Succeeded));
		ASSERT_THAT(IsTrue(Rejected.IsZero()));
	}
	TEST_METHOD(ActualLambdaFunctionRegistersWithSamePointerAndRetainsItsOrigin)
	{
FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		asSStableKey Key; asCScriptFunction* Lambda = nullptr;
		ASSERT_THAT(IsTrue(Fixture.Context->GetIdentityRegistry().InternFunction(Fixture.Lambda(2), Key) == asETypeIdentityStatus::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->CreateFunction(Key, "", "", asFUNC_SCRIPT, Lambda) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->DefineFunction(*Lambda, Fixture.Image->Primitive(ttInt), {}) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->SetLambdaOrigin(*Lambda, *Fixture.Enclosing, 1) == asEMetadataResult::InvalidSignature));
		ASSERT_THAT(IsTrue(Fixture.Image->SetLambdaOrigin(*Lambda, *Fixture.Enclosing, 2) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->FinalizeLayouts() == asEMetadataResult::Succeeded && Fixture.Image->Freeze() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Fixture.Image->ValidateFrozenDefinitions()));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Fixture.Image) == asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(Engine->FindMetadataFunction(Key) == Lambda));
		ASSERT_THAT(IsTrue(Lambda->GetMetadataEnclosingFunction() == Fixture.Enclosing));
		ASSERT_THAT(AreEqual(asUINT(2), Lambda->GetMetadataLambdaOrdinal()));
		ASSERT_THAT(IsTrue(Fixture.Image->SetLambdaOrigin(*Lambda, *Fixture.Enclosing, 2) == asEMetadataResult::InvalidState));
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
