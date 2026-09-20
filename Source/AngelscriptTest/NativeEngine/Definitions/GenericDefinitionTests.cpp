#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "NativeEngine/NativeEngineTestSupport.h"
#include "NativeEngine/SourceExecution/NativeSourceExecutionTestSupport.h"
#include "as_builder.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "frontend/AST/as_type_context.h"
#include "frontend/Compile/as_compilation_session.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(GenericDefinitions, "Angelscript.UnitTest.NativeEngine.Definitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FFixture
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCDefinitions> Host = MakeUnique<asCDefinitions>();
		asCObjectType* Template = nullptr;
		AS_NAMESPACE_QUALIFIER asSStableKey Declaration;
		bool Initialize(bool Fixed = true)
		{
if (Host->SetIdentityContext(Context) != asEDefinitionResult::Succeeded) return false;
			asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = TEXT("generic-host"); Descriptor.Kind = asETypeDeclKind::Struct;
			Descriptor.Name = TEXT("array"); Descriptor.GenericArity = 1;
			if (Context->GetIdentityRegistry().InternNominalType(Descriptor, Declaration) != asETypeIdentityStatus::Succeeded) return false;
			if (Host->CreateObjectType(Declaration, "array", "", asOBJ_VALUE | asOBJ_TEMPLATE, Template) != asEDefinitionResult::Succeeded) return false;
			if (Host->SetTypeDeclarationFacts(*Template, asETypeDeclKind::Struct, 1) != asEDefinitionResult::Succeeded) return false;
			const auto Layout = Fixed ? Host->SetFixedTemplateLayout(*Template, 16, 8) : Host->SetNativeLayout(*Template, 16, 8);
			return Layout == asEDefinitionResult::Succeeded && Host->FinalizeLayouts() == asEDefinitionResult::Succeeded && Host->Freeze() == asEDefinitionResult::Succeeded;
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
		TUniquePtr<asCDefinitions> Draft()
		{
			auto Set = MakeUnique<asCDefinitions>();
			Set->SetIdentityContext(Context);
			Set->AddDependency(Host.Get());
			return Set;
		}
	};
public:
	TEST_METHOD(ConcreteArgumentsProduceDistinctActualObjectTypes)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Set = Fixture.Draft();
		asCObjectType* Integers = nullptr; asCObjectType* Floats = nullptr;
		ASSERT_THAT(IsTrue(Set->CreateTemplateInstance(*Fixture.Template, Fixture.Use(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32), {Set->Primitive(ttInt)}, Integers) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Set->CreateTemplateInstance(*Fixture.Template, Fixture.Use(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32), {Set->Primitive(ttFloat32)}, Floats) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Integers != Floats && Integers->GetStableKey() != Floats->GetStableKey()));
		ASSERT_THAT(IsTrue(Integers->GetTemplateDefinition() == Fixture.Template));
		ASSERT_THAT(IsTrue(Integers->templateSubTypes[0].GetTokenType() == ttInt));
		ASSERT_THAT(IsTrue(Floats->templateSubTypes[0].GetTokenType() == ttFloat32));
		ASSERT_THAT(IsTrue(Set->FinalizeLayouts() == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Set->Freeze() == asEDefinitionResult::Succeeded));
		ASSERT_THAT(AreEqual(asUINT(16), Integers->GetSize()));
		ASSERT_THAT(IsTrue(Set->ValidateFrozenDefinitions()));
	}
	TEST_METHOD(TemplateLayoutMustBeExplicitNotGuessedFromOneInstantiation)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize(false)));
		auto Set = Fixture.Draft(); asCObjectType* Rejected = nullptr;
		ASSERT_THAT(IsTrue(Set->CreateTemplateInstance(*Fixture.Template, Fixture.Use(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32), {Set->Primitive(ttInt)}, Rejected) == asEDefinitionResult::IncompleteDefinition));
		ASSERT_THAT(IsNull(Rejected)); ASSERT_THAT(AreEqual(0, Set->GetTypes().Num()));
	}
	TEST_METHOD(ArgumentKeyMismatchDoesNotPublishASpecialization)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Set = Fixture.Draft(); asCObjectType* Rejected = nullptr;
		ASSERT_THAT(IsTrue(Set->CreateTemplateInstance(*Fixture.Template, Fixture.Use(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32), {Set->Primitive(ttFloat32)}, Rejected) == asEDefinitionResult::InvalidSignature));
		ASSERT_THAT(IsNull(Rejected)); ASSERT_THAT(AreEqual(0, Set->GetTypes().Num()));
	}
	TEST_METHOD(RepeatedSpecializationPreservesPointerAndRejectsArgumentMutation)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Set = Fixture.Draft(); asCObjectType* First = nullptr; asCObjectType* Second = nullptr;
		const auto Key = Fixture.Use(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
		ASSERT_THAT(IsTrue(Set->CreateTemplateInstance(*Fixture.Template, Key, {Set->Primitive(ttInt)}, First) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Set->CreateTemplateInstance(*Fixture.Template, Key, {Set->Primitive(ttInt)}, Second) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(First == Second));
		ASSERT_THAT(IsTrue(Set->AddTemplateArgument(*First, Set->Primitive(ttInt)) == asEDefinitionResult::InvalidState));
		ASSERT_THAT(AreEqual(1, Set->GetTypes().Num()));
	}
	TEST_METHOD(CorruptFrozenEnvironmentIsRejectedBeforeSourceResolution)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		AS_NAMESPACE_QUALIFIER asCSourceManager Sources;
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Sources);
		AS_NAMESPACE_QUALIFIER asCCompilationSession Session(Sources, Diagnostics, TEXT("host-validation"), Fixture.Context);
		Fixture.Template->name = "tampered";
		ASSERT_THAT(IsFalse(Session.AddExternalDefinitions(Fixture.Host.Get())));
	}
	TEST_METHOD(DetachedBuilderResolvesExplicitAndArrayShorthandToOneConcreteType)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources =
			AngelscriptNativeEngineTest::MakeSourceList(TEXT("Generic.as"), TEXT("class Values { array<int> Explicit; int[] Shorthand; };"));
		asSBuilderOptions Options; Options.StableScope = TEXT("generic-tests"); Options.TypeContext = Fixture.Context; Options.Dependencies.Add(Fixture.Host.Get());
		asCBuilder Builder(Sources, Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		const asCObjectType* Record = nullptr;
		for (const auto* Type : Builder.GetDefinitions()->GetTypes()) if (Type->name == "Values") Record = static_cast<const asCObjectType*>(Type);
		ASSERT_THAT(IsNotNull(Record)); ASSERT_THAT(AreEqual(asUINT(2), Record->GetPropertyCount()));
		ASSERT_THAT(IsTrue(Record->properties[0]->type.GetTypeInfo() == Record->properties[1]->type.GetTypeInfo()));
		ASSERT_THAT(IsTrue(Record->properties[0]->type.GetTypeInfo()->GetDefinitions() == Builder.GetDefinitions()));
		ASSERT_THAT(IsNull(Builder.GetDefinitions()->GetBoundEngine()));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine, MoveTemp(Fixture.Host))));
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine, Builder)));
		const auto* Concrete = Record->properties[0]->type.GetTypeInfo();
		ASSERT_THAT(IsTrue(Engine->FindType(Concrete->GetStableKey()) == Concrete));
		ASSERT_THAT(IsTrue(Engine->FindType("", "array") == Fixture.Template));
	}
	TEST_METHOD(TransitiveHostEnvironmentAndNestedTemplateClosersAreRetained)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Intermediate = Fixture.Draft();
		ASSERT_THAT(IsTrue(Intermediate->FinalizeLayouts() == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsTrue(Intermediate->Freeze() == asEDefinitionResult::Succeeded));
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources =
			AngelscriptNativeEngineTest::MakeSourceList(TEXT("Nested.as"), TEXT("struct Grid { array<array<int>> Explicit; int[][] Shorthand; };"));
		asSBuilderOptions Options; Options.StableScope = TEXT("nested-tests"); Options.TypeContext = Fixture.Context; Options.Dependencies.Add(Intermediate.Get());
		asCBuilder Builder(Sources, Options);
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
