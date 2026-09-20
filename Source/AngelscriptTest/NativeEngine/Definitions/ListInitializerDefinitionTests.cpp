#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"
#include "as_builder.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "frontend/AST/as_ast_visitor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(ListInitializerDefinitions, "Angelscript.UnitTest.NativeEngine.Definitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FFixture
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCDefinitions> Host = MakeUnique<asCDefinitions>();
		asCObjectType* Type = nullptr;
		asCScriptFunction* Factory = nullptr;
		bool Initialize(bool Generic = false)
		{
if (Host->SetIdentityContext(Context) != asEDefinitionResult::Succeeded) return false;
			asSNominalTypeDescriptor Decl; Decl.StableScope = TEXT("list-host"); Decl.Kind = asETypeDeclKind::Class;
			Decl.Name = TEXT("Values"); Decl.GenericArity = Generic ? 1 : 0;
			asSStableKey Declaration, Return;
			if (Context->GetIdentityRegistry().InternNominalType(Decl, Declaration) != asETypeIdentityStatus::Succeeded ||
				Host->CreateObjectType(Declaration, "Values", "", asOBJ_REF | (Generic ? asOBJ_TEMPLATE : 0), Type) != asEDefinitionResult::Succeeded ||
				Host->SetTypeDeclarationFacts(*Type, asETypeDeclKind::Class, Decl.GenericArity) != asEDefinitionResult::Succeeded) return false;
			if ((Generic ? Host->SetFixedTemplateLayout(*Type, 16, 8) : Host->SetNativeLayout(*Type, 16, 8)) != asEDefinitionResult::Succeeded) return false;
			asSStableTypeUseDescriptor Use; Use.Kind = asETypeUseKind::Nominal; Use.Declaration = Declaration; Use.Qualifiers = asETypeUseQualifiers::Handle;
			if (Context->GetIdentityRegistry().InternTypeUse(Use, Return) != asETypeIdentityStatus::Succeeded) return false;
			asSFunctionIdentityDescriptor Function; Function.StableScope = Decl.StableScope; Function.Owner = Declaration;
			Function.Kind = asEFunctionIdentityKind::Factory; Function.Name = TEXT("ListFactory"); Function.ReturnType = Return;
			asSStableKey Key;
			if (Context->GetIdentityRegistry().InternFunction(Function, Key) != asETypeIdentityStatus::Succeeded ||
				Host->CreateFunction(Key, "ListFactory", "", asFUNC_SYSTEM, Factory) != asEDefinitionResult::Succeeded) return false;
			auto ReturnType = asCDataType::CreateType(Type, false); if (ReturnType.MakeHandle(true, true) < 0) return false;
			return Host->DefineFunction(*Factory, ReturnType, {}) == asEDefinitionResult::Succeeded &&
				Host->SetFunctionDeclarationFacts(*Factory, asEFunctionIdentityKind::Factory, Type) == asEDefinitionResult::Succeeded;
		}
		asSMetadataListPattern Pattern(bool Generic = false) const
		{
			asSMetadataListPattern List, Repeat, Value;
			Repeat.Kind = asEMetadataListPatternKind::Repeat; Value.Kind = asEMetadataListPatternKind::Value;
			if (Generic) Value.TemplateParameterIndex = 0; else Value.ValueType = Host->Primitive(ttInt);
			Repeat.Children.Add(Value); List.Children.Add(Repeat); return List;
		}
		bool Freeze(const asSMetadataListPattern& Pattern)
		{
			return Host->SetListInitializer(*Type, *Factory, Pattern) == asEDefinitionResult::Succeeded &&
				Host->FinalizeLayouts() == asEDefinitionResult::Succeeded && Host->Freeze() == asEDefinitionResult::Succeeded && Host->ValidateFrozenDefinitions();
		}
		bool Build(const FString& Text, int& Lists, int& Defaults)
		{
			TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources =
				AngelscriptNativeEngineTest::MakeSourceList(TEXT("List.as"), Text);
			asSBuilderOptions Options; Options.StableScope = TEXT("list-tests"); Options.TypeContext = Context; Options.Dependencies.Add(Host.Get());
			asCBuilder Builder(Sources, Options);
			if (!Builder.RunThrough()) return false;
			Lists = 0; Defaults = 0;
			asCASTVisitor::Walk(Builder.GetCompilationSession()->GetTranslationUnit(), [&](const asCASTNode& Node)
			{
				if (const auto* List = dyn_cast<asCInitListExpr>(&Node)) { if (List->GetInitializerIdentity() == Factory->GetStableKey()) ++Lists; }
				if (isa<asCValueInitExpr>(&Node)) ++Defaults;
			});
			return Builder.GetDefinitions()->ValidateFrozenDefinitions();
		}
	};
public:
	TEST_METHOD(RealFactoryAndPatternSurviveFreezeWithoutAnEngine)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		ASSERT_THAT(IsNull(Fixture.Host->GetListInitializer(*Fixture.Type)));
		ASSERT_THAT(IsTrue(Fixture.Freeze(Fixture.Pattern())));
		const auto* Result = Fixture.Host->GetListInitializer(*Fixture.Type);
		ASSERT_THAT(IsNotNull(Result)); ASSERT_THAT(IsTrue(Result->Factory == Fixture.Factory));
		ASSERT_THAT(IsTrue(Fixture.Host->SetListInitializer(*Fixture.Type, *Fixture.Factory, Fixture.Pattern()) == asEDefinitionResult::InvalidState));
		ASSERT_THAT(IsNull(Fixture.Host->GetBoundEngine()));
	}
	TEST_METHOD(InvalidRepetitionAndTemplateSlotsDoNotPublishAContract)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize(true)));
		auto Pattern = Fixture.Pattern(true); Pattern.Children[0].Children[0].TemplateParameterIndex = 1;
		ASSERT_THAT(IsTrue(Fixture.Host->SetListInitializer(*Fixture.Type, *Fixture.Factory, Pattern) == asEDefinitionResult::InvalidSignature));
		Pattern = Fixture.Pattern(true); Pattern.Children.Add(Pattern.Children[0].Children[0]);
		ASSERT_THAT(IsTrue(Fixture.Host->SetListInitializer(*Fixture.Type, *Fixture.Factory, Pattern) == asEDefinitionResult::InvalidSignature));
		Pattern = Fixture.Pattern(true);
		const asSMetadataListPattern ExtraValue = Pattern.Children[0].Children[0];
		Pattern.Children[0].Children.Add(ExtraValue);
		ASSERT_THAT(IsTrue(Fixture.Host->SetListInitializer(*Fixture.Type, *Fixture.Factory, Pattern) == asEDefinitionResult::InvalidSignature));
		ASSERT_THAT(IsTrue(Fixture.Freeze(Fixture.Pattern(true))));
	}
	TEST_METHOD(InitializerListsKeepAnActualFactoryAndExplicitOmissionNodes)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize() && Fixture.Freeze(Fixture.Pattern())));
		int Lists = 0, Defaults = 0;
		ASSERT_THAT(IsTrue(Fixture.Build(TEXT("void Use() { Values Data = {1,,3}; }"), Lists, Defaults)));
		ASSERT_THAT(AreEqual(1, Lists)); ASSERT_THAT(AreEqual(1, Defaults));
	}
	TEST_METHOD(TemplateListSlotsUseConcreteArgumentsNotTheContainerName)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize(true) && Fixture.Freeze(Fixture.Pattern(true))));
		int Lists = 0, Defaults = 0;
		ASSERT_THAT(IsTrue(Fixture.Build(TEXT("void Use() { Values<int> Data = {1, 2, 3}; }"), Lists, Defaults)));
		ASSERT_THAT(AreEqual(1, Lists)); ASSERT_THAT(AreEqual(0, Defaults));
	}
	TEST_METHOD(TrailingCommaRetainsAnOmittedASSlotRatherThanCppListSemantics)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize() && Fixture.Freeze(Fixture.Pattern())));
		int Lists = 0, Defaults = 0;
		ASSERT_THAT(IsTrue(Fixture.Build(TEXT("void Use() { Values Data = {1,}; }"), Lists, Defaults)));
		ASSERT_THAT(AreEqual(1, Lists)); ASSERT_THAT(AreEqual(1, Defaults));
		ASSERT_THAT(IsTrue(Fixture.Build(TEXT("void Use() { Values Data = {,}; }"), Lists, Defaults)));
		ASSERT_THAT(AreEqual(1, Lists)); ASSERT_THAT(AreEqual(2, Defaults));
		ASSERT_THAT(IsTrue(Fixture.Build(TEXT("void Use() { Values Data = {}; }"), Lists, Defaults)));
		ASSERT_THAT(AreEqual(1, Lists)); ASSERT_THAT(AreEqual(0, Defaults));
	}
	TEST_METHOD(NestedListShapeAndRepeatSameAreSemanticContracts)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize()));
		auto Pattern = Fixture.Pattern();
		asSMetadataListPattern Inner = Pattern; Inner.Children[0].Kind = asEMetadataListPatternKind::RepeatSame;
		Pattern.Children[0].Children = {Inner};
		ASSERT_THAT(IsTrue(Fixture.Freeze(Pattern)));
		int Lists = 0, Defaults = 0;
		ASSERT_THAT(IsTrue(Fixture.Build(TEXT("void Use() { Values Data = {{1,2}, {3,4}}; }"), Lists, Defaults)));
		ASSERT_THAT(IsFalse(Fixture.Build(TEXT("void Use() { Values Data = {{1,2}, {3}}; }"), Lists, Defaults)));
	}
	TEST_METHOD(OrdinaryRecordsAreNotImplicitlyTreatedAsCppAggregates)
	{
		FFixture Fixture; ASSERT_THAT(IsTrue(Fixture.Initialize() && Fixture.Freeze(Fixture.Pattern())));
		int Lists = 0, Defaults = 0;
		ASSERT_THAT(IsFalse(Fixture.Build(TEXT("class Record { int X; }; void Use() { Record Data = {1}; }"), Lists, Defaults)));
		ASSERT_THAT(IsFalse(Fixture.Build(TEXT("void Use() { Values Data = {null}; }"), Lists, Defaults)));
	}
};
#endif
