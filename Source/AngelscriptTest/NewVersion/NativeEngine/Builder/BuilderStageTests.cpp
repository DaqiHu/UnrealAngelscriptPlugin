#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "as_builder.h"
#include "as_module_definition_set.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "frontend/AST/as_ast_cast.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace AngelscriptBuilderStageTests
{
	TEST_CLASS_WITH_FLAGS(Builder, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
	private:
		struct FInput
		{
			TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot = MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
			AS_NAMESPACE_QUALIFIER asCSourceManager Sources{Snapshot};
			AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics{Sources};
			AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
			explicit FInput(const FString& Source, int32 Workers = 1)
			{
				const auto Input = AngelscriptNativeEngineTest::FSourceInput::FromText(TEXT("Main.as"), Source);
				Snapshot->AddFile(TEXT("Main.as"), Input.GetUtf8Bytes());
				Snapshot->Freeze();
				Options.StableScope = TEXT("builder.tests");
				Options.WorkerCount = Workers;
			}
			FString DescribeFailure(const asCBuilder& Builder) const
			{
				for (uint8 Stage = 0; Stage < uint8(AS_NAMESPACE_QUALIFIER asEBuilderStage::Failed); ++Stage)
					if (const auto* Result = Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage(Stage)); Result && !Result->Succeeded)
						return FString::Printf(TEXT("Stage %d: %s\n%s"), int32(Stage), *Result->Error,
							*AS_NAMESPACE_QUALIFIER asCDiagnosticRenderer::SerializeStable(Result->Diagnostics));
				return TEXT("No failed stage observation");
			}
		};
	public:
		TEST_METHOD(DirectSessionRecoveryCannotPublishAfterDeclarationFailure)
		{
			AngelscriptNativeEngineTest::FFrontendSessionRun Run({{TEXT("Bad.as"), TEXT("auto First = Second; auto Second = First; int OtherwiseValid() { return 1; }")}});
			ASSERT_THAT(IsFalse(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(8)));
			ASSERT_THAT(IsNotNull(Run.Function(TEXT("OtherwiseValid"))->GetBody()));
			ASSERT_THAT(IsFalse(Run.Session->AreBodiesAnalyzed()));
			ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
		}

		TEST_METHOD(ExplicitGlobalBaseNameDoesNotBindANamespacedShadow)
		{
			FInput Input(TEXT("class Base {} namespace N { class Base {} class Derived : ::Base {} }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			const asCObjectType* Derived = nullptr;
			for (const auto* Type : Builder.GetModuleDefinitionSet()->GetTypes()) if (Type->name == "Derived") Derived = static_cast<const asCObjectType*>(Type);
			ASSERT_THAT(IsNotNull(Derived)); ASSERT_THAT(IsNotNull(Derived->derivedFrom));
			ASSERT_THAT(AreEqual(FString{}, FString(UTF8_TO_TCHAR(Derived->derivedFrom->GetNamespace()))));
		}

		TEST_METHOD(ClassDefaultStatementsShareOneActualLexicalBodyScope)
		{
			FInput Input(TEXT("class Item { int Value; default int Initial = 7; default Value = Initial; }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			const bool Built = Builder.RunThrough();
			ASSERT_THAT(IsTrue(Built, Input.DescribeFailure(Builder)));
			ASSERT_THAT(AreEqual(1, Builder.GetCompilationSession()->GetBodyFragments().Num()));
			ASSERT_THAT(AreEqual(2, Builder.GetCompilationSession()->GetBodyFragments()[0].GetBody()->GetStatements().Num()));
			ASSERT_THAT(AreEqual(1, Builder.GetModuleDefinitionSet()->GetFunctions().Num()));
		}

		TEST_METHOD(MaintainedSuffixFactsSurviveActualFunctionMaterialization)
		{
			struct FCase { const TCHAR* Spelling; asEFuncTrait Trait; };
			const FCase Cases[] = {
				{TEXT("accept_temporary_this"), asTRAIT_ACCEPT_TEMPORARY_OBJECT}, {TEXT("external_implicit_this"), asTRAIT_EXTERNAL_IMPLICIT_THIS},
				{TEXT("no_discard"), asTRAIT_NODISCARD}, {TEXT("allow_discard"), asTRAIT_ALLOWDISCARD},
				{TEXT("__generated"), asTRAIT_GENERATED_FUNCTION}, {TEXT("deprecated"), asTRAIT_DEPRECATED},
				{TEXT("defaults"), asTRAIT_DEFAULTS_ONLY}, {TEXT("unsafe_during_construction"), asTRAIT_UNSAFE_DURING_CONSTRUCTION},
			};
			for (const auto& Case : Cases)
			{
				FInput Input(FString::Printf(TEXT("class C { int F() %s { return 42; } }"), Case.Spelling));
				asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
				ASSERT_THAT(IsTrue(Builder.RunThrough())); ASSERT_THAT(AreEqual(1, Builder.GetModuleDefinitionSet()->GetFunctions().Num()));
				auto* Function = Builder.GetModuleDefinitionSet()->GetFunctions()[0]; ASSERT_THAT(IsTrue(Function->traits.GetTrait(Case.Trait)));
				ASSERT_THAT(IsTrue(Builder.GetModuleDefinitionSet()->ValidateFrozenDefinitions()));
				Function->traits.SetTrait(Case.Trait, false);
				ASSERT_THAT(IsFalse(Builder.GetModuleDefinitionSet()->ValidateFrozenDefinitions()));
			}
		}

		TEST_METHOD(RecordInheritanceAndOverrideRulesFailAtDeclarationResolution)
		{
			for (const TCHAR* Source : {
				TEXT("class A : B {} class B : A {}"), TEXT("class A : int {}"), TEXT("class A {} class B : A, A {}"),
				TEXT("class A {} class B {} class C : A, B {}"), TEXT("class A { int F() override { return 0; } }"),
				TEXT("class A { int F() final { return 1; } } class B : A { int F() { return 2; } }"),
				TEXT("class A { int F() { return 1; } } class B : A { float64 F() { return 2.0; } }")})
			{
				FInput Input(Source); asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
				ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsCollected)));
				ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
				ASSERT_THAT(IsNull(Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
			}
		}
		TEST_METHOD(ExactBaseOverrideAndDistinctOverloadsProduceRealDefinitions)
		{
			FInput Input(TEXT("class A { int F(int Value) { return Value; } } class B : A { int F(int Value) override { return Value + 1; } int F() { return 0; } }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough())); ASSERT_THAT(AreEqual(3, Builder.GetModuleDefinitionSet()->GetFunctions().Num()));
			bool FoundOverride = false;
			for (const auto* Function : Builder.GetModuleDefinitionSet()->GetFunctions()) FoundOverride |= Function->IsOverride();
			ASSERT_THAT(IsTrue(FoundOverride)); ASSERT_THAT(IsTrue(Builder.GetModuleDefinitionSet()->ValidateFrozenDefinitions()));
		}

		TEST_METHOD(NamedFunctionRetainsItsImageAndNamespaceAfterBuilderRelease)
		{
			asCScriptFunction* Retained = nullptr;
			{
				FInput Input(TEXT("namespace N { int AddOne(int Value) { return Value + 1; } int Read() { return AddOne(41); } }"));
				asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
				ASSERT_THAT(IsTrue(Builder.RunThrough()));
				ASSERT_THAT(AreEqual(2, Builder.GetModuleDefinitionSet()->GetFunctions().Num()));
				for (auto* Function : Builder.GetModuleDefinitionSet()->GetFunctions())
					if (Function->name == "AddOne") Retained = Function;
				ASSERT_THAT(IsNotNull(Retained));
				ASSERT_THAT(AreEqual(FString(TEXT("N")), FString(UTF8_TO_TCHAR(Retained->GetNamespace()))));
				ASSERT_THAT(IsTrue(Retained->name == "AddOne"));
				ASSERT_THAT(IsTrue(Builder.GetModuleDefinitionSet()->ValidateFrozenDefinitions()));
				Retained->AddRef();
			}
			ASSERT_THAT(IsTrue(Retained->name == "AddOne"));
			ASSERT_THAT(IsNull(Retained->GetEngine()));
			Retained->Release();
		}

		TEST_METHOD(ParallelNamedBodiesKeepTheSameActualFunctionKeysAndObservations)
		{
			FString Source = TEXT("int AddOne(int Value) { return Value + 1; }\n");
			for (int32 Index = 0; Index < 16; ++Index)
				Source += FString::Printf(TEXT("int Read%d() { return AddOne(%d); }\n"), Index, Index);
			FInput SerialInput(Source, 1), ParallelInput(Source, 8);
			asCBuilder Serial(SerialInput.Snapshot, SerialInput.Diagnostics, SerialInput.Options), Parallel(ParallelInput.Snapshot, ParallelInput.Diagnostics, ParallelInput.Options);
			ASSERT_THAT(IsTrue(Serial.RunThrough())); ASSERT_THAT(IsTrue(Parallel.RunThrough()));
			ASSERT_THAT(AreEqual(17, Parallel.GetModuleDefinitionSet()->GetFunctions().Num()));
			ASSERT_THAT(AreEqual(8, Parallel.GetCompilationSession()->GetBodyAnalysisStatistics().StartedWorkers));
			ASSERT_THAT(AreEqual(Serial.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::ASTVerified)->Json, Parallel.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::ASTVerified)->Json));
			ASSERT_THAT(AreEqual(Serial.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)->Text, Parallel.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)->Text));
		}

		TEST_METHOD(AutoGlobalsInferOnceThroughForwardInitializerDependencies)
		{
			FInput Input(TEXT("const auto Answer = Base + 2; const auto Base = 40; enum E { Value = Answer } int Read() { return Answer; }"), 4);
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
			const auto* Variable = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCVarDecl>(Builder.GetCompilationSession()->Lookup(TEXT("Answer"))[0]);
			ASSERT_THAT(IsTrue(Variable->GetResolvedQualType().IsConst()));
			const auto* Initializer = Variable->GetInitializer(); ASSERT_THAT(IsNotNull(Initializer));
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			ASSERT_THAT(IsTrue(Variable->GetInitializer() == Initializer));
			const auto* Global = Builder.GetModuleDefinitionSet()->FindGlobalProperty(Variable->GetStableIdentity());
			ASSERT_THAT(IsNotNull(Global)); ASSERT_THAT(IsTrue(Global->type.GetTokenType() == ttInt && Global->type.IsReadOnly()));
		}

		TEST_METHOD(AutoInferenceCyclesAndMissingInitializersFailWithoutDefinitions)
		{
			for (const TCHAR* Source : {TEXT("auto First = Second; auto Second = First;"), TEXT("auto Missing;"), TEXT("auto Invalid = null;"), TEXT("auto Invalid = {1, 2};")})
			{
				FInput Input(Source); asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
				ASSERT_THAT(IsFalse(Builder.RunThrough())); ASSERT_THAT(IsFalse(bool(Builder.GetModuleDefinitionSet())));
				ASSERT_THAT(IsFalse(Builder.GetCompilationSession()->IsPublishable()));
			}
		}

		TEST_METHOD(FunctionDefinitionModifiersReachActualTraitsAndFrozenAuthentication)
		{
			FInput Input(TEXT("local int Hidden() { return 1; } mixin int Twice(int Value) { return Value * 2; } class C { int Read() final { return 1; } C(int Value) {} }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			asCScriptFunction* Hidden = nullptr; asCScriptFunction* Twice = nullptr; asCScriptFunction* Read = nullptr; asCScriptFunction* Constructor = nullptr;
			for (auto* Function : Builder.GetModuleDefinitionSet()->GetFunctions())
			{
				if (Function->name == "Hidden") Hidden = Function;
				if (Function->name == "Twice") Twice = Function;
				if (Function->name == "Read") Read = Function;
				if (Function->name == "C") Constructor = Function;
			}
			ASSERT_THAT(IsNotNull(Hidden)); ASSERT_THAT(IsNotNull(Twice)); ASSERT_THAT(IsNotNull(Read)); ASSERT_THAT(IsNotNull(Constructor));
			ASSERT_THAT(IsTrue(Hidden->IsMetadataLocal() && Twice->IsMixin() && Read->IsFinal()));
			ASSERT_THAT(IsTrue(Builder.GetModuleDefinitionSet()->ValidateFrozenDefinitions()));
			Twice->SetMixin(false);
			ASSERT_THAT(IsFalse(Builder.GetModuleDefinitionSet()->ValidateFrozenDefinitions()));
		}

		TEST_METHOD(EnumExplicitFloatConversionRetainsTypedConstantSemantics)
		{
			FInput Input(TEXT("const float64 Scale = 2.0; enum E { Answer = int(21.25 * Scale), Selected = 1.0 < 2.0 ? Answer : 0 }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			const bool Built = Builder.RunThrough();
			ASSERT_THAT(IsTrue(Built, Input.DescribeFailure(Builder)));
			const auto* Enum = static_cast<const asCEnumType*>(Builder.GetModuleDefinitionSet()->GetTypes()[0]);
			int First = 0, Second = 0; Enum->GetEnumValueByIndex(0, &First); Enum->GetEnumValueByIndex(1, &Second);
			ASSERT_THAT(AreEqual(42, First)); ASSERT_THAT(AreEqual(42, Second));
		}

		TEST_METHOD(ParsedClassDefaultsProduceOneImplicitActualFunctionInSourceOrder)
		{
			FInput Input(TEXT("class Settings { int A; int B; default A = 20; default B = A + 22; }"), 4);
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			const bool Built = Builder.RunThrough();
			ASSERT_THAT(IsTrue(Built, Input.DescribeFailure(Builder)));
			const auto* Type = static_cast<const asCObjectType*>(Builder.GetModuleDefinitionSet()->GetTypes()[0]);
			ASSERT_THAT(AreEqual(asUINT(0), Type->GetMethodCount()));
			ASSERT_THAT(AreEqual(1, Builder.GetModuleDefinitionSet()->GetFunctions().Num()));
			const auto* Function = Builder.GetModuleDefinitionSet()->GetFunctions()[0];
			ASSERT_THAT(IsTrue(Function->GetMetadataDeclarationKind() == AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::ClassDefault));
			ASSERT_THAT(IsTrue(Function->name.GetLength() == 0 && Function->objectType == Type));
			const auto* ASTFunction = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Builder.GetCompilationSession()->GetBodyFragments()[0].GetWorkItem().Function);
			ASSERT_THAT(AreEqual(2, ASTFunction->GetDeferredStatementRanges().Num()));
			ASSERT_THAT(IsTrue(ASTFunction->GetDeferredStatementRanges()[0].Begin.ByteOffset < ASTFunction->GetDeferredStatementRanges()[1].Begin.ByteOffset));
			ASSERT_THAT(IsTrue(Builder.GetModuleDefinitionSet()->ValidateFrozenDefinitions()));
		}

		TEST_METHOD(GlobalInitializerAndDefaultArgumentsAreAnalyzedBeforeBodies)
		{
			FInput Input(TEXT("namespace N { const int Answer = 6 * 7; int Add(int Value = 42) { return Value; } int Use() { return Add(); } }"), 4);
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
			const auto* Session = Builder.GetCompilationSession();
			const auto* Global = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCVarDecl>(Session->Lookup(TEXT("Answer"))[0]);
			ASSERT_THAT(IsTrue(Global->HasDeferredInitializer()));
			ASSERT_THAT(IsNull(Global->GetInitializer()));
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			ASSERT_THAT(IsNotNull(Global->GetInitializer()));
			ASSERT_THAT(IsTrue(Global->GetInitializer()->IsSealed()));
			const auto* Definition = Builder.GetModuleDefinitionSet()->FindGlobalProperty(Global->GetStableIdentity());
			ASSERT_THAT(IsNotNull(Definition));
			ASSERT_THAT(IsTrue(Definition->type.IsReadOnly()));
			const auto* Add = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Session->Lookup(TEXT("Add"))[0]);
			const auto* Function = Builder.GetModuleDefinitionSet()->FindFunction(Add->GetStableIdentity());
			const char* Default = nullptr;
			ASSERT_THAT(AreEqual(0, Function->GetParam(0, nullptr, nullptr, nullptr, &Default)));
			ASSERT_THAT(AreEqual(FString(TEXT("42")), FString(UTF8_TO_TCHAR(Default))));
			ASSERT_THAT(IsTrue(Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)->Text.Contains(TEXT("global|"))));
		}

		TEST_METHOD(InvalidDefaultInitializerCannotPublishDefinitions)
		{
			FInput Input(TEXT("int Add(int Value = Missing()) { return Value; }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(IsFalse(bool(Builder.GetModuleDefinitionSet())));
			ASSERT_THAT(IsFalse(Builder.GetCompilationSession()->IsASTVerified()));
		}

		TEST_METHOD(TypedefKeepsItsEntityIdentityButCanonicalizesFunctionTypeUses)
		{
			FInput Input(TEXT("typedef int Count; Count Read(Count Value) { return Value; }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			const auto* Session = Builder.GetCompilationSession();
			const auto* Alias = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCTypeAliasDecl>(Session->Lookup(TEXT("Count"))[0]);
			const auto* Read = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Session->Lookup(TEXT("Read"))[0]);
			ASSERT_THAT(IsTrue(Alias->GetResolvedAliasedType() == Read->GetResolvedReturnType()));
			const auto* Definition = Builder.GetModuleDefinitionSet()->FindType(Alias->GetStableIdentity());
			ASSERT_THAT(IsNotNull(Definition));
			ASSERT_THAT(IsTrue((Definition->GetFlags() & asOBJ_TYPEDEF) != 0));
			ASSERT_THAT(IsTrue(static_cast<const asCTypedefType*>(Definition)->aliasForType.GetTokenType() == ttInt));
		}

		TEST_METHOD(CyclicAliasesFailBeforeBodyAnalysis)
		{
			FInput Input(TEXT("typedef Other First; typedef First Other; First Read();"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsCollected)));
			ASSERT_THAT(IsFalse(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
			ASSERT_THAT(IsTrue(Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)->Error.Contains(TEXT("cyclic-type-alias"))));
			ASSERT_THAT(IsFalse(bool(Builder.GetModuleDefinitionSet())));
		}

		TEST_METHOD(AuthoredAccessPoliciesReachFrozenDefinitionsAndOutliveTheBuilder)
		{
			TUniquePtr<asCModuleDefinitionSet> Definitions;
			asCObjectType* Type = nullptr;
			asCObjectProperty* Property = nullptr;
			asCScriptFunction* Method = nullptr;
			{
				FInput Input(TEXT("class Item { access Friends = private, Reader(readonly), *(editdefaults); access:Friends int Value; access:Friends int Read() const; }"));
				asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
				const bool Built = Builder.RunThrough();
				ASSERT_THAT(IsTrue(Built, Input.DescribeFailure(Builder)));
				Definitions = Builder.TakeModuleDefinitionSet();
				ASSERT_THAT(IsTrue(bool(Definitions)));
				asCModuleDefinitionSet* Set = Definitions.Get();
				for (auto* Candidate : Set->GetTypes())
					if (Candidate->name == "Item") Type = static_cast<asCObjectType*>(Candidate);
				ASSERT_THAT(IsNotNull(Type));
				asSAccessSpecifier* Policy = Type->GetAccessSpecifier("Friends");
				ASSERT_THAT(IsNotNull(Policy));
				ASSERT_THAT(IsTrue(Policy->bIsPrivate));
				ASSERT_THAT(IsTrue(Policy->bAnyEditDefaults));
				ASSERT_THAT(AreEqual(asUINT(1), Policy->permissions.GetLength()));
				ASSERT_THAT(IsTrue(Policy->permissions[0].accessName == "Reader"));
				ASSERT_THAT(IsTrue(Policy->permissions[0].bReadOnly));
				for (asUINT Index = 0; Index < Type->GetPropertyCount(); ++Index)
					if (Type->properties[Index]->name == "Value") Property = Type->properties[Index];
				for (const auto* Function : Set->GetFunctions())
					if (Function->name == "Read") Method = const_cast<asCScriptFunction*>(Function);
				ASSERT_THAT(IsNotNull(Property));
				ASSERT_THAT(IsTrue(Property->accessSpecifier == Policy));
				ASSERT_THAT(IsNotNull(Method));
				ASSERT_THAT(IsTrue(Method->accessSpecifier == Policy));
				Type->AddRef();
			}
			ASSERT_THAT(IsTrue(bool(Definitions)));
			ASSERT_THAT(IsNotNull(Type->GetAccessSpecifier("Friends")));
			ASSERT_THAT(IsTrue(Property->accessSpecifier == Type->GetAccessSpecifier("Friends")));
			Type->Release();
		}

		TEST_METHOD(InvalidAccessPolicyDoesNotPublishDefinitions)
		{
			FInput Input(TEXT("class Item { access:Missing int Value; } int Later() { return 1; }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(IsFalse(bool(Builder.GetModuleDefinitionSet())));
		}

		TEST_METHOD(MemberAccessAndConstructionReachActualDefinitionObjects)
		{
			FInput Input(TEXT("class Value { int Number; Value(int Initial) { Number = Initial; } int Read() const { return Number; } }; int Use() { Value Local(7); return Local.Read(); }"), 4);
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			const bool Built = Builder.RunThrough();
			ASSERT_THAT(IsTrue(Built, Input.DescribeFailure(Builder)));
			const auto* Type = static_cast<const asCObjectType*>(Builder.GetModuleDefinitionSet()->GetTypes()[0]);
			ASSERT_THAT(AreEqual(asUINT(1), Type->GetPropertyCount()));
			bool FoundConstructor = false;
			for (const auto* Function : Builder.GetModuleDefinitionSet()->GetFunctions())
				if (Function->name == "Value") { FoundConstructor = true; ASSERT_THAT(IsTrue(Function->objectType == Type && Function->traits.GetTrait(asTRAIT_CONSTRUCTOR))); }
			ASSERT_THAT(IsTrue(FoundConstructor));
			ASSERT_THAT(IsNull(Builder.GetModuleDefinitionSet()->GetBoundEngine()));
		}

		TEST_METHOD(IndependentStagesPublishActualDefinitionsOnlyAfterExplicitFreeze)
		{
			FInput Input(TEXT("class Payload { int Value; int Read() { return 7; } };"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsNull(Builder.engine));
			ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
			ASSERT_THAT(IsFalse(Builder.GetCompilationSession()->GetASTContext().IsSealed()));
			ASSERT_THAT(IsFalse(bool(Builder.GetModuleDefinitionSet())));
			ASSERT_THAT(IsTrue(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::ASTVerified)));
			ASSERT_THAT(IsTrue(Builder.GetCompilationSession()->IsASTVerified()));
			ASSERT_THAT(IsTrue(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsBuilt)));
			asCModuleDefinitionSet* Definitions = Builder.GetModuleDefinitionSet();
			ASSERT_THAT(IsNotNull(Definitions));
			ASSERT_THAT(AreEqual(asEDefinitionSetState::Building, Definitions->GetState()));
			ASSERT_THAT(AreEqual(1, Definitions->GetTypes().Num()));
			ASSERT_THAT(AreEqual(1, Definitions->GetFunctions().Num()));
			ASSERT_THAT(IsNull(Definitions->GetBoundEngine()));
			ASSERT_THAT(IsTrue(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::LayoutsFinalized)));
			ASSERT_THAT(AreEqual(asEDefinitionSetState::Building, Definitions->GetState()));
			ASSERT_THAT(IsTrue(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)));
			ASSERT_THAT(AreEqual(asEDefinitionSetState::Frozen, Definitions->GetState()));
			ASSERT_THAT(IsNull(Definitions->GetBoundEngine()));
		}

		TEST_METHOD(FullAndSplitRunsProduceIdenticalStageObservations)
		{
			FInput FullInput(TEXT("int First() { return 1; } int Second() { return 2; }"));
			FInput SplitInput(TEXT("int First() { return 1; } int Second() { return 2; }"), 4);
			asCBuilder Full(FullInput.Snapshot, FullInput.Diagnostics, FullInput.Options);
			asCBuilder Split(SplitInput.Snapshot, SplitInput.Diagnostics, SplitInput.Options);
			ASSERT_THAT(IsTrue(Full.RunThrough()));
			for (uint8 Step = 1; Step < uint8(AS_NAMESPACE_QUALIFIER asEBuilderStage::Failed); ++Step)
			{
				const auto Stage = static_cast<AS_NAMESPACE_QUALIFIER asEBuilderStage>(Step);
				ASSERT_THAT(IsTrue(Split.RunStage(Stage)));
				ASSERT_THAT(AreEqual(Full.GetStageResult(Stage)->Text, Split.GetStageResult(Stage)->Text));
				ASSERT_THAT(AreEqual(Full.GetStageResult(Stage)->Json, Split.GetStageResult(Stage)->Json));
				TSharedPtr<FJsonObject> Parsed;
				ASSERT_THAT(IsTrue(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Split.GetStageResult(Stage)->Json), Parsed)));
			}
			ASSERT_THAT(AreEqual(2, Split.GetCompilationSession()->GetBodyAnalysisStatistics().StartedWorkers));
			ASSERT_THAT(AreEqual(2, Split.GetCompilationSession()->GetBodyAnalysisStatistics().AnalyzedFragments));
		}

		TEST_METHOD(EqualFunctionSignaturesShareTheCanonicalTypeButNotFunctionDefinitions)
		{
			FInput Input(TEXT("int First(int Left) { return Left; } int Second(int Right) { return Right; }"), 2);
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			const auto* Session = Builder.GetCompilationSession();
			const auto* First = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Session->Lookup(TEXT("First"))[0]);
			const auto* Second = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Session->Lookup(TEXT("Second"))[0]);
			ASSERT_THAT(AreEqual(First->GetResolvedSignature(), Second->GetResolvedSignature()));
			ASSERT_THAT(IsFalse(First->GetStableIdentity() == Second->GetStableIdentity()));
			TUniquePtr<asCModuleDefinitionSet> Definitions = Builder.TakeModuleDefinitionSet();
			ASSERT_THAT(IsNotNull(Definitions.Get()));
			ASSERT_THAT(IsNotNull(Definitions->FindFunction(First->GetStableIdentity())));
			ASSERT_THAT(IsNotNull(Definitions->FindFunction(Second->GetStableIdentity())));
			ASSERT_THAT(IsFalse(Definitions->FindFunction(First->GetStableIdentity()) == Definitions->FindFunction(Second->GetStableIdentity())));
		}

		TEST_METHOD(IllegalOrderDoesNotConsumeOrPoisonTheCurrentStage)
		{
			FInput Input(TEXT("int Value() { return 3; }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsFalse(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBuilderStage::SourceReady, Builder.GetStage()));
			ASSERT_THAT(IsTrue(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
			ASSERT_THAT(IsFalse(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
		}

		TEST_METHOD(PreprocessorSelectionIsRetainedAndDoesNotConstructDeclarations)
		{
			FInput Input(TEXT("#if ENABLED\nint Selected() { return 1; }\n#else\ninvalid @@@\n#endif\n"));
			Input.Options.PreprocessorFlags.Add(TEXT("ENABLED"), true);
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Preprocessed)));
			ASSERT_THAT(IsNull(Builder.GetCompilationSession()));
			ASSERT_THAT(AreEqual(1, Builder.GetPreprocessedSources().Num()));
			ASSERT_THAT(IsTrue(Builder.GetPreprocessedSources()[0].IsValid()));
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			ASSERT_THAT(AreEqual(1, Builder.GetModuleDefinitionSet()->GetFunctions().Num()));
		}

		TEST_METHOD(RecoveryStopsDefinitionPublicationAndKeepsFailureObservation)
		{
			FInput Input(TEXT("int Broken() { return Missing; }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBuilderStage::Failed, Builder.GetStage()));
			ASSERT_THAT(IsFalse(bool(Builder.GetModuleDefinitionSet())));
			const auto* Attempt = Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed);
			ASSERT_THAT(IsNotNull(Attempt));
			ASSERT_THAT(IsFalse(Attempt->Succeeded));
			ASSERT_THAT(IsFalse(Attempt->Error.IsEmpty()));
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
		}

		TEST_METHOD(ForwardDefaultDependenciesAreIndependentOfDeclarationOrder)
		{
			FInput Forward(TEXT("int First(int Value=Second()) { return Value; } int Second(int Value=7) { return Value; } int Use() { return First(); }"), 4);
			FInput Reverse(TEXT("int Second(int Value=7) { return Value; } int First(int Value=Second()) { return Value; } int Use() { return First(); }"), 1);
			asCBuilder A(Forward.Snapshot, Forward.Diagnostics, Forward.Options);
			asCBuilder B(Reverse.Snapshot, Reverse.Diagnostics, Reverse.Options);
			ASSERT_THAT(IsTrue(A.RunThrough())); ASSERT_THAT(IsTrue(B.RunThrough()));
			ASSERT_THAT(AreEqual(A.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)->Text, B.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)->Text));
		}

		TEST_METHOD(CyclicDefaultDependenciesCannotPublishDefinitions)
		{
			FInput Input(TEXT("int First(int Value=Second()) { return Value; } int Second(int Value=First()) { return Value; }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(IsFalse(bool(Builder.GetModuleDefinitionSet())));
			ASSERT_THAT(IsTrue(Builder.GetCompilationSession()->SerializeStableBodyDiagnostics().Contains(TEXT("initializer-dependency-cycle"))));
		}

		TEST_METHOD(EnumExpressionsResolveBeforeHostDeclarationOutput)
		{
			FInput Input(TEXT("enum Flags { A=6*7, B=A+1, C=1<<4, D=C|2, E };"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
			auto* Enumeration = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Builder.GetCompilationSession()->Lookup(TEXT("Flags"))[0]);
			const int64 Expected[] = {42, 43, 16, 18, 19};
			int Index = 0;
			for (auto* Declaration : Enumeration->GetDecls())
			{
				auto* Constant = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCEnumConstantDecl>(Declaration);
				ASSERT_THAT(IsTrue(Constant->HasResolvedValue()));
				ASSERT_THAT(AreEqual(Expected[Index++], Constant->GetValue()));
			}
			const bool Built = Builder.RunThrough();
			ASSERT_THAT(IsTrue(Built, Input.DescribeFailure(Builder)));
			ASSERT_THAT(AreEqual(asUINT(5), Builder.GetModuleDefinitionSet()->GetTypes()[0]->GetEnumValueCount()));
		}

		TEST_METHOD(EnumForwardAndGlobalConstantDependenciesUseOriginalTypedExpressions)
		{
			FInput Input(TEXT("enum Answer { Result=Answer::Base+Offset, Base=40 }; const int Offset=1+1;"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			const bool Built = Builder.RunThrough();
			ASSERT_THAT(IsTrue(Built, Input.DescribeFailure(Builder)));
			const auto* Result = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCEnumConstantDecl>(Builder.GetCompilationSession()->Lookup(TEXT("Result"))[0]);
			ASSERT_THAT(AreEqual(int64(42), Result->GetValue()));
			const auto* Offset = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCVarDecl>(Builder.GetCompilationSession()->Lookup(TEXT("Offset"))[0]);
			ASSERT_THAT(IsNotNull(Offset->GetInitializer()));
			ASSERT_THAT(IsTrue(Offset->GetInitializer()->IsSealed()));
		}

		TEST_METHOD(EnumShortCircuitDoesNotEvaluateUnselectedConstantFailure)
		{
			FInput Input(TEXT("enum Choice { Value=false ? 1/0 : 42 };"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			ASSERT_THAT(AreEqual(int64(42), AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCEnumConstantDecl>(Builder.GetCompilationSession()->Lookup(TEXT("Value"))[0])->GetValue()));
		}

		TEST_METHOD(InvalidEnumConstantsFailBeforeDefinitionPublication)
		{
			for (const TCHAR* Source : {TEXT("enum Bad { A=1/0 };"), TEXT("enum Bad { A=2147483647, B };"),
				TEXT("enum Bad { A=1<<32 };"), TEXT("enum Bad { A=B, B=A };"), TEXT("int Get(){return 1;} enum Bad { A=Get() };"), TEXT("enum Bad { A=1.5 }; ")})
			{
				FInput Input(Source); asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
				ASSERT_THAT(IsFalse(Builder.RunThrough()));
				ASSERT_THAT(IsFalse(bool(Builder.GetModuleDefinitionSet())));
				ASSERT_THAT(IsFalse(Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)->Succeeded));
			}
		}

		TEST_METHOD(InvalidOptionsRejectBeforeLexing)
		{
			FInput Input(TEXT("void Valid() {}"), 0);
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBuilderStage::Failed, Builder.GetStage()));
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(IsNull(Builder.GetCompilationSession()));
		}

		TEST_METHOD(EmptyTranslationUnitHasAValidVerifiedSourceAnchor)
		{
			FInput Input(TEXT(""));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			ASSERT_THAT(AreEqual(0, Builder.GetModuleDefinitionSet()->GetTypes().Num()));
			ASSERT_THAT(AreEqual(0, Builder.GetModuleDefinitionSet()->GetFunctions().Num()));
			ASSERT_THAT(AreEqual(0, Builder.GetCompilationSession()->GetBodyAnalysisStatistics().StartedWorkers));
		}

		TEST_METHOD(ForeignDiagnosticSnapshotRejectsTheBuilderInput)
		{
			FInput Input(TEXT("void Work() {}"));
			FInput Foreign(TEXT("void Elsewhere() {}"));
			asCBuilder Builder(Input.Snapshot, Foreign.Diagnostics, Input.Options);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBuilderStage::Failed, Builder.GetStage()));
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(IsNull(Builder.GetCompilationSession()));
		}

		TEST_METHOD(BodyLookupUsesItsNamespaceBeforeUnrelatedSameNameDeclarations)
		{
			FInput Input(TEXT("namespace A { class Item {}; int Read() { Item Local; return 1; } int Use() { return Read(); } } "
				"namespace B { class Item {}; int Read() { Item Local; return 2; } int Use() { return Read(); } }"), 4);
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			const auto* Session = Builder.GetCompilationSession();
			ASSERT_THAT(AreEqual(4, Session->GetBodyFragments().Num()));
			ASSERT_THAT(AreEqual(2, Builder.GetModuleDefinitionSet()->GetTypes().Num()));
			ASSERT_THAT(AreEqual(4, Builder.GetModuleDefinitionSet()->GetFunctions().Num()));
		}

		TEST_METHOD(UnrelatedNamespaceDeclarationsAreNotImplicitlyVisible)
		{
			FInput Input(TEXT("namespace A { int Hidden() { return 1; } } namespace B { int Use() { return Hidden(); } }"), 2);
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(IsFalse(Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)->Succeeded));
			ASSERT_THAT(IsFalse(bool(Builder.GetModuleDefinitionSet())));
		}

		TEST_METHOD(UnknownConditionalFlagStopsBeforeDeclarationCollection)
		{
			FInput Input(TEXT("#if MISSING\nvoid Work() {}\n#endif\n"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(IsNull(Builder.GetCompilationSession()));
			const auto* Attempt = Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::Preprocessed);
			ASSERT_THAT(IsNotNull(Attempt));
			ASSERT_THAT(IsFalse(Attempt->Succeeded));
			ASSERT_THAT(IsFalse(Attempt->Diagnostics.IsEmpty()));
			ASSERT_THAT(IsFalse(bool(Builder.GetModuleDefinitionSet())));
		}

		TEST_METHOD(ManyParallelBodiesRetainTheSerialCanonicalProjection)
		{
			FString Source;
			for (int32 Index = 0; Index < 64; ++Index)
				Source += FString::Printf(TEXT("int Function%d() { return %d; }\n"), Index, Index);
			FInput SerialInput(Source, 1);
			FInput ParallelInput(Source, 8);
			asCBuilder Serial(SerialInput.Snapshot, SerialInput.Diagnostics, SerialInput.Options);
			asCBuilder Parallel(ParallelInput.Snapshot, ParallelInput.Diagnostics, ParallelInput.Options);
			ASSERT_THAT(IsTrue(Serial.RunThrough()));
			ASSERT_THAT(IsTrue(Parallel.RunThrough()));
			ASSERT_THAT(AreEqual(8, Parallel.GetCompilationSession()->GetBodyAnalysisStatistics().StartedWorkers));
			ASSERT_THAT(AreEqual(64, Parallel.GetCompilationSession()->GetBodyAnalysisStatistics().AnalyzedFragments));
			ASSERT_THAT(AreEqual(Serial.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::ASTVerified)->Json,
				Parallel.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::ASTVerified)->Json));
			ASSERT_THAT(AreEqual(Serial.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)->Text,
				Parallel.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)->Text));
		}

		TEST_METHOD(FrozenImageOutlivesTheBuilderAndRetainsIdentityContext)
		{
			TUniquePtr<asCModuleDefinitionSet> Definitions;
			{
				FInput Input(TEXT("class Kept {};"));
				asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
				ASSERT_THAT(IsTrue(Builder.RunThrough()));
				Definitions = Builder.TakeModuleDefinitionSet();
			}
			ASSERT_THAT(IsTrue(Definitions->GetIdentityContext().IsValid()));
			ASSERT_THAT(AreEqual(1, Definitions->GetTypes().Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("Kept")), FString(UTF8_TO_TCHAR(Definitions->GetTypes()[0]->GetName()))));
		}

		TEST_METHOD(DefinitionDumpDistinguishesSameSizeFieldsWithDifferentTypes)
		{
			FInput IntegerInput(TEXT("struct Payload { int Value; };"));
			FInput FloatInput(TEXT("struct Payload { float32 Value; };"));
			asCBuilder IntegerBuilder(IntegerInput.Snapshot, IntegerInput.Diagnostics, IntegerInput.Options);
			asCBuilder FloatBuilder(FloatInput.Snapshot, FloatInput.Diagnostics, FloatInput.Options);
			ASSERT_THAT(IsTrue(IntegerBuilder.RunThrough()));
			ASSERT_THAT(IsTrue(FloatBuilder.RunThrough()));
			ASSERT_THAT(AreEqual(IntegerBuilder.GetModuleDefinitionSet()->GetTypes()[0]->GetSize(), FloatBuilder.GetModuleDefinitionSet()->GetTypes()[0]->GetSize()));
			ASSERT_THAT(AreNotEqual(IntegerBuilder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)->Text,
				FloatBuilder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)->Text));
		}

		TEST_METHOD(LayoutFailureRetainsExactReasonAndDoesNotFreezeTheDraft)
		{
			FInput Input(TEXT("struct Left { Right Next; }; struct Right { Left Previous; };"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsBuilt)));
			auto Draft = Builder.GetModuleDefinitionSet();
			ASSERT_THAT(IsFalse(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::LayoutsFinalized)));
			const auto* Attempt = Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::LayoutsFinalized);
			ASSERT_THAT(IsTrue(Attempt->MetadataStatus.IsSet()));
			ASSERT_THAT(AreEqual(asEMetadataResult::RecursiveValueType, Attempt->MetadataStatus.GetValue()));
			ASSERT_THAT(IsFalse(bool(Builder.GetModuleDefinitionSet())));
			ASSERT_THAT(AreEqual(asEDefinitionSetState::Building, Draft->GetState()));
			ASSERT_THAT(IsFalse(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)));
		}
	};
}
#endif
