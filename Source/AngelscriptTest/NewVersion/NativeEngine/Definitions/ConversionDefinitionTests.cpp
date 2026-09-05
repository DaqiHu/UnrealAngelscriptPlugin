#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "as_builder.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "frontend/as_type_context.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(ConversionDefinitions, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FFixture
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		std::shared_ptr<asCMetadataImage> Image = asCMetadataImage::Create();
		asCObjectType* Owner = nullptr;
		asCScriptFunction* ToInt = nullptr;
		asCScriptFunction* ToFloat = nullptr;
		AS_NAMESPACE_QUALIFIER asSStableKey IntType;
		AS_NAMESPACE_QUALIFIER asSStableKey FloatType;
		bool Initialize()
		{
if (Image->SetIdentityContext(Context) != asEMetadataResult::Succeeded) return false;
			Context->InternPrimitiveTypeUse(asEPrimitiveType::Int32, IntType);
			Context->InternPrimitiveTypeUse(asEPrimitiveType::Float64, FloatType);
			asSNominalTypeDescriptor Nominal;
			Nominal.StableScope = TEXT("conversion-definitions");
			Nominal.Name = TEXT("Source");
			Nominal.Kind = asETypeDeclKind::Class;
			asSStableKey OwnerKey;
			return Context->GetIdentityRegistry().InternNominalType(Nominal, OwnerKey) == asETypeIdentityStatus::Succeeded
				&& Image->CreateObjectType(OwnerKey, "Source", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Owner) == asEMetadataResult::Succeeded
				&& AddConversion("opImplConv", IntType, ttInt, ToInt)
				&& AddConversion("opImplConv", FloatType, ttFloat64, ToFloat);
		}
		bool AddConversion(const char* Name, const AS_NAMESPACE_QUALIFIER asSStableKey& Destination, eTokenType Token, asCScriptFunction*& Out)
		{
asSFunctionIdentityDescriptor Descriptor;
			Descriptor.StableScope = TEXT("conversion-definitions");
			Descriptor.Name = UTF8_TO_TCHAR(Name);
			Descriptor.Kind = asEFunctionIdentityKind::Conversion;
			Descriptor.Owner = Owner->GetStableKey();
			Descriptor.ReturnType = Destination;
			Descriptor.ConversionDestination = Destination;
			asSStableKey Key;
			return Context->GetIdentityRegistry().InternFunction(Descriptor, Key) == asETypeIdentityStatus::Succeeded
				&& Image->CreateFunction(Key, Name, "", asFUNC_SCRIPT, Out) == asEMetadataResult::Succeeded
				&& Image->DefineFunction(*Out, Image->Primitive(Token), {}, Owner) == asEMetadataResult::Succeeded
				&& Image->SetFunctionDeclarationFacts(*Out, asEFunctionIdentityKind::Conversion, Owner) == asEMetadataResult::Succeeded;
		}
		bool Freeze()
		{
			return Image->FinalizeLayouts() == asEMetadataResult::Succeeded
				&& Image->Freeze() == asEMetadataResult::Succeeded
				&& Image->ValidateFrozenDefinitions();
		}
	};
	struct FBuilderInput
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot = MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		AS_NAMESPACE_QUALIFIER asCSourceManager Sources{Snapshot};
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics{Sources};
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		explicit FBuilderInput(const FString& Source)
		{
			const auto Input = AngelscriptNativeEngineTest::FSourceInput::FromText(TEXT("Main.as"), Source);
			Snapshot->AddFile(TEXT("Main.as"), Input.GetUtf8Bytes());
			Snapshot->Freeze();
			Options.StableScope = TEXT("conversion-definitions.builder");
		}
	};
public:
	TEST_METHOD(DestinationDistinctConversionsPublishAuthenticatedFunctions)
	{
FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Initialize()));
		ASSERT_THAT(IsFalse(Fixture.ToInt->GetStableKey() == Fixture.ToFloat->GetStableKey()));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		asSFunctionIdentityDescriptor IntIdentity;
		asSFunctionIdentityDescriptor FloatIdentity;
		auto& Registry = Fixture.Context->GetIdentityRegistry();
		ASSERT_THAT(AreEqual(asETypeIdentityStatus::Succeeded, Registry.ResolveFunction(Fixture.ToInt->GetStableKey(), IntIdentity)));
		ASSERT_THAT(AreEqual(asETypeIdentityStatus::Succeeded, Registry.ResolveFunction(Fixture.ToFloat->GetStableKey(), FloatIdentity)));
		ASSERT_THAT(AreEqual(asEFunctionIdentityKind::Conversion, IntIdentity.Kind));
		ASSERT_THAT(AreEqual(asEFunctionIdentityKind::Conversion, FloatIdentity.Kind));
		ASSERT_THAT(IsTrue(IntIdentity.ConversionDestination == Fixture.IntType));
		ASSERT_THAT(IsTrue(FloatIdentity.ConversionDestination == Fixture.FloatType));
		ASSERT_THAT(IsTrue(IntIdentity.ReturnType == IntIdentity.ConversionDestination));
		ASSERT_THAT(IsTrue(FloatIdentity.ReturnType == FloatIdentity.ConversionDestination));
		ASSERT_THAT(IsNull(Fixture.ToInt->GetEngine()));
	}

	TEST_METHOD(TamperedConversionReturnOrKindCannotFreezeOrRegister)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Initialize()));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		const asCDataType Original = Fixture.ToInt->returnType;
		Fixture.ToInt->returnType = Fixture.Image->Primitive(ttBool);
		ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Fixture.Image) != asEMetadataRegistrationResult::Succeeded));
		Fixture.ToInt->returnType = Original;
		ASSERT_THAT(IsTrue(Fixture.Image->ValidateFrozenDefinitions()));
		Fixture.ToInt->traits.SetTrait(asTRAIT_CONSTRUCTOR, true);
		ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
		Fixture.ToInt->traits.SetTrait(asTRAIT_CONSTRUCTOR, false);
		ASSERT_THAT(IsTrue(Fixture.Image->ValidateFrozenDefinitions()));
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Fixture.Image) == asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(Engine->FindMetadataFunction(Fixture.ToInt->GetStableKey()) == Fixture.ToInt));
	}

	TEST_METHOD(ExposedConversionObjectTypeMustMatchDeclarationOwner)
	{
		FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Initialize()));
		ASSERT_THAT(IsTrue(Fixture.Freeze()));
		asCObjectType* Original = Fixture.ToInt->objectType;
		Fixture.ToInt->objectType = nullptr;
		ASSERT_THAT(IsFalse(Fixture.Image->ValidateFrozenDefinitions()));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Fixture.Image) != asEMetadataRegistrationResult::Succeeded));
		Fixture.ToInt->objectType = Original;
		ASSERT_THAT(IsTrue(Fixture.Image->ValidateFrozenDefinitions()));
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Fixture.Image) == asEMetadataRegistrationResult::Succeeded));
	}

	TEST_METHOD(ConversionDeclarationFactsRejectVoidAndParameterizedShapes)
	{
FFixture Fixture;
		ASSERT_THAT(IsTrue(Fixture.Initialize()));
		asCScriptFunction* Parameterized = nullptr;
		asSFunctionIdentityDescriptor Descriptor;
		Descriptor.StableScope = TEXT("conversion-definitions");
		Descriptor.Name = TEXT("opConv");
		Descriptor.Kind = asEFunctionIdentityKind::Method;
		Descriptor.Owner = Fixture.Owner->GetStableKey();
		Descriptor.ReturnType = Fixture.IntType;
		asSStableParameter Parameter;
		Parameter.Type = Fixture.IntType;
		Descriptor.Parameters.Add(Parameter);
		asSStableKey Key;
		ASSERT_THAT(AreEqual(asETypeIdentityStatus::Succeeded,
			Fixture.Context->GetIdentityRegistry().InternFunction(Descriptor, Key)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->CreateFunction(Key, "opConv", "", asFUNC_SCRIPT, Parameterized)));
		asSMetadataParameter Argument;
		Argument.Name = "Extra";
		Argument.Type = Fixture.Image->Primitive(ttInt);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->DefineFunction(*Parameterized, Fixture.Image->Primitive(ttInt), {Argument}, Fixture.Owner)));
		ASSERT_THAT(AreEqual(asEMetadataResult::InvalidSignature,
			Fixture.Image->SetFunctionDeclarationFacts(*Parameterized, asEFunctionIdentityKind::Conversion, Fixture.Owner)));
	}

	TEST_METHOD(BuilderPublishesDestinationDistinctConversionsAndRejectsVoidShape)
	{
		FBuilderInput Valid(TEXT("class Source { int opImplConv() const; float64 opImplConv() const; }"));
		asCBuilder Builder(Valid.Snapshot, Valid.Diagnostics, Valid.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		std::shared_ptr<asCMetadataImage> Image = Builder.GetDefinitions();
		ASSERT_THAT(IsTrue(bool(Image)));
		ASSERT_THAT(IsTrue(Image->ValidateFrozenDefinitions()));
		TArray<const asCScriptFunction*> Conversions;
		for (const asCScriptFunction* Function : Image->GetFunctions())
			if (Function->name == "opImplConv") Conversions.Add(Function);
		ASSERT_THAT(AreEqual(2, Conversions.Num()));
		ASSERT_THAT(IsFalse(Conversions[0]->GetStableKey() == Conversions[1]->GetStableKey()));
		auto Context = Image->GetIdentityContext();
		ASSERT_THAT(IsTrue(Context.IsValid()));
		for (const asCScriptFunction* Function : Conversions)
		{
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Identity;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context->GetIdentityRegistry().ResolveFunction(Function->GetStableKey(), Identity)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Conversion, Identity.Kind));
			ASSERT_THAT(IsTrue(Identity.ConversionDestination == Identity.ReturnType));
		}

		FBuilderInput Invalid(TEXT("class Source { void opConv(); int Later(); }"));
		asCBuilder Rejected(Invalid.Snapshot, Invalid.Diagnostics, Invalid.Options);
		ASSERT_THAT(IsFalse(Rejected.RunThrough()));
		ASSERT_THAT(IsFalse(bool(Rejected.GetDefinitions())));
	}
};
#endif
