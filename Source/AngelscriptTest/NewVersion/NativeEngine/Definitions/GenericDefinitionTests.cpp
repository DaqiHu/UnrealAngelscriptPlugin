#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "../NativeEngineTestSupport.h"
#include "as_builder.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "frontend/as_type_context.h"
#include "frontend/as_compilation_session.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(GenericDefinitions, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FFixture
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		std::shared_ptr<asCMetadataImage> Host = asCMetadataImage::Create();
		asCObjectType* Template = nullptr;
		AS_NAMESPACE_QUALIFIER asSStableKey Declaration;
		bool Initialize(bool Fixed = true)
		{
if (Host->SetIdentityContext(Context) != asEMetadataResult::Succeeded) return false;
			asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = TEXT("generic-host"); Descriptor.Kind = asETypeDeclKind::Struct;
			Descriptor.Name = TEXT("array"); Descriptor.GenericArity = 1;
			if (Context->GetIdentityRegistry().InternNominalType(Descriptor, Declaration) != asETypeIdentityStatus::Succeeded) return false;
			if (Host->CreateObjectType(Declaration, "array", "", asOBJ_VALUE | asOBJ_TEMPLATE, Template) != asEMetadataResult::Succeeded) return false;
			if (Host->SetTypeDeclarationFacts(*Template, asETypeDeclKind::Struct, 1) != asEMetadataResult::Succeeded) return false;
			const auto Layout = Fixed ? Host->SetFixedTemplateLayout(*Template, 16, 8) : Host->SetNativeLayout(*Template, 16, 8);
			return Layout == asEMetadataResult::Succeeded && Host->FinalizeLayouts() == asEMetadataResult::Succeeded && Host->Freeze() == asEMetadataResult::Succeeded;
		}
		AS_NAMESPACE_QUALIFIER asSStableKey Use(AS_NAMESPACE_QUALIFIER asEPrimitiveType Primitive)
		{
asSStableKey Argument, Result;
			Context->InternPrimitiveTypeUse(Primitive, Argument);
			asSStableTypeUseDescriptor Descriptor;
			Descriptor.Kind = asETypeUseKind::Nominal; Descriptor.Declaration = Declaration; Descriptor.Arguments.Add(Argument);
			Context->GetIdentityRegistry().InternTypeUse(Descriptor, Result);
			return Result;
		}
		std::shared_ptr<asCMetadataImage> Draft()
		{
			auto Image = asCMetadataImage::Create(); Image->SetIdentityContext(Context); Image->AddDependency(Host); return Image;
		}
	};
public:
	TEST_METHOD(ConcreteArgumentsProduceDistinctActualObjectTypes)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Image = Fixture.Draft();
		asCObjectType* Integers = nullptr; asCObjectType* Floats = nullptr;
		ASSERT_THAT(IsTrue(Image->CreateTemplateInstance(*Fixture.Template, Fixture.Use(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32), {Image->Primitive(ttInt)}, Integers) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Image->CreateTemplateInstance(*Fixture.Template, Fixture.Use(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32), {Image->Primitive(ttFloat32)}, Floats) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Integers != Floats && Integers->GetStableKey() != Floats->GetStableKey()));
		ASSERT_THAT(IsTrue(Integers->GetMetadataTemplateDefinition() == Fixture.Template));
		ASSERT_THAT(IsTrue(Integers->templateSubTypes[0].GetTokenType() == ttInt));
		ASSERT_THAT(IsTrue(Floats->templateSubTypes[0].GetTokenType() == ttFloat32));
		ASSERT_THAT(IsTrue(Image->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Image->Freeze() == asEMetadataResult::Succeeded));
		ASSERT_THAT(AreEqual(asUINT(16), Integers->GetSize()));
		ASSERT_THAT(IsTrue(Image->ValidateFrozenDefinitions()));
	}
	TEST_METHOD(TemplateLayoutMustBeExplicitNotGuessedFromOneInstantiation)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize(false)));
		auto Image = Fixture.Draft(); asCObjectType* Rejected = nullptr;
		ASSERT_THAT(IsTrue(Image->CreateTemplateInstance(*Fixture.Template, Fixture.Use(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32), {Image->Primitive(ttInt)}, Rejected) == asEMetadataResult::IncompleteDefinition));
		ASSERT_THAT(IsNull(Rejected)); ASSERT_THAT(AreEqual(0, Image->GetTypes().Num()));
	}
	TEST_METHOD(ArgumentKeyMismatchDoesNotPublishASpecialization)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Image = Fixture.Draft(); asCObjectType* Rejected = nullptr;
		ASSERT_THAT(IsTrue(Image->CreateTemplateInstance(*Fixture.Template, Fixture.Use(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32), {Image->Primitive(ttFloat32)}, Rejected) == asEMetadataResult::InvalidSignature));
		ASSERT_THAT(IsNull(Rejected)); ASSERT_THAT(AreEqual(0, Image->GetTypes().Num()));
	}
	TEST_METHOD(RepeatedSpecializationPreservesPointerAndRejectsArgumentMutation)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Image = Fixture.Draft(); asCObjectType* First = nullptr; asCObjectType* Second = nullptr;
		const auto Key = Fixture.Use(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
		ASSERT_THAT(IsTrue(Image->CreateTemplateInstance(*Fixture.Template, Key, {Image->Primitive(ttInt)}, First) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Image->CreateTemplateInstance(*Fixture.Template, Key, {Image->Primitive(ttInt)}, Second) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(First == Second));
		ASSERT_THAT(IsTrue(Image->AddTemplateArgument(*First, Image->Primitive(ttInt)) == asEMetadataResult::InvalidState));
		ASSERT_THAT(AreEqual(1, Image->GetTypes().Num()));
	}
	TEST_METHOD(CorruptFrozenEnvironmentIsRejectedBeforeSourceResolution)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Snapshot = MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		Snapshot->Freeze();
		AS_NAMESPACE_QUALIFIER asCSourceManager Sources(Snapshot); AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Sources);
		AS_NAMESPACE_QUALIFIER asCCompilationSession Session(Snapshot, Diagnostics, TEXT("host-validation"), Fixture.Context);
		Fixture.Template->name = "tampered";
		ASSERT_THAT(IsFalse(Session.AddExternalDefinitions(Fixture.Host)));
	}
	TEST_METHOD(DetachedBuilderResolvesExplicitAndArrayShorthandToOneConcreteType)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
auto Snapshot = MakeShared<asCSourceSnapshot, ESPMode::ThreadSafe>();
		const auto Source = AngelscriptNativeEngineTest::FSourceInput::FromText(TEXT("Generic.as"), TEXT("class Values { array<int> Explicit; int[] Shorthand; };"));
		Snapshot->AddFile(TEXT("Generic.as"), Source.GetUtf8Bytes()); Snapshot->Freeze();
		AS_NAMESPACE_QUALIFIER asCSourceManager Sources(Snapshot); asCDiagnosticsEngine Diagnostics(Sources);
		asSBuilderOptions Options; Options.StableScope = TEXT("generic-tests"); Options.TypeContext = Fixture.Context; Options.Dependencies.Add(Fixture.Host);
		asCBuilder Builder(Snapshot, Diagnostics, Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		const asCObjectType* Record = nullptr;
		for (const auto* Type : Builder.GetDefinitions()->GetTypes()) if (Type->name == "Values") Record = static_cast<const asCObjectType*>(Type);
		ASSERT_THAT(IsNotNull(Record)); ASSERT_THAT(AreEqual(asUINT(2), Record->GetPropertyCount()));
		ASSERT_THAT(IsTrue(Record->properties[0]->type.GetTypeInfo() == Record->properties[1]->type.GetTypeInfo()));
		ASSERT_THAT(IsTrue(Record->properties[0]->type.GetTypeInfo()->GetMetadataImage() == Builder.GetDefinitions().get()));
		ASSERT_THAT(IsNull(Builder.GetDefinitions()->GetBoundEngine()));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		ASSERT_THAT(IsTrue(Engine->RegisterMetadataImage(Builder.GetDefinitions()) == asEMetadataRegistrationResult::Succeeded));
		const auto* Concrete = Record->properties[0]->type.GetTypeInfo();
		ASSERT_THAT(IsTrue(Engine->FindMetadataType(Concrete->GetStableKey()) == Concrete));
		ASSERT_THAT(IsTrue(Engine->FindMetadataType("", "array") == Fixture.Template));
	}
	TEST_METHOD(TransitiveHostEnvironmentAndNestedTemplateClosersAreRetained)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Intermediate = Fixture.Draft();
		ASSERT_THAT(IsTrue(Intermediate->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Intermediate->Freeze() == asEMetadataResult::Succeeded));
auto Snapshot = MakeShared<asCSourceSnapshot, ESPMode::ThreadSafe>();
		const auto Source = AngelscriptNativeEngineTest::FSourceInput::FromText(TEXT("Nested.as"), TEXT("struct Grid { array<array<int>> Explicit; int[][] Shorthand; };"));
		Snapshot->AddFile(TEXT("Nested.as"), Source.GetUtf8Bytes()); Snapshot->Freeze();
		AS_NAMESPACE_QUALIFIER asCSourceManager Sources(Snapshot); asCDiagnosticsEngine Diagnostics(Sources);
		asSBuilderOptions Options; Options.StableScope = TEXT("nested-tests"); Options.TypeContext = Fixture.Context; Options.Dependencies.Add(Intermediate);
		asCBuilder Builder(Snapshot, Diagnostics, Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		const asCObjectType* Grid = nullptr;
		for (const auto* Type : Builder.GetDefinitions()->GetTypes()) if (Type->name == "Grid") Grid = static_cast<const asCObjectType*>(Type);
		ASSERT_THAT(IsNotNull(Grid)); ASSERT_THAT(AreEqual(asUINT(2), Grid->GetPropertyCount()));
		ASSERT_THAT(IsTrue(Grid->properties[0]->type.GetTypeInfo() == Grid->properties[1]->type.GetTypeInfo()));
		const auto* Outer = static_cast<const asCObjectType*>(Grid->properties[0]->type.GetTypeInfo());
		ASSERT_THAT(IsTrue(Outer->templateSubTypes[0].GetTypeInfo() != nullptr));
		ASSERT_THAT(IsTrue(Builder.GetDefinitions()->ValidateFrozenDefinitions()));
	}
};
#endif
