#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"
#include "NativeEngine/SourceExecution/NativeSourceExecutionTestSupport.h"
#include "as_builder.h"
#include "as_compile_output.h"
#include "as_engine_compile_registration.h"
#include "as_definitions.h"
#include "as_definitions.h"
#include "frontend/Compile/as_definition_consumer.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"
#include "frontend/AST/as_type_context.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(CompileLifecycle, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FInput
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;

		explicit FInput(const FString& Source)
		{
			Sources = AngelscriptNativeEngineTest::MakeSourceList(TEXT("Main.as"), Source);
			Options.StableScope = TEXT("compile-lifecycle");
			Options.TypeContext = TypeContext;
		}

		explicit FInput(TArray<TPair<FString, FString>> Files)
		{
			for (const TPair<FString, FString>& File : Files)
			{
				Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(File.Key, File.Value));
			}
			Options.StableScope = TEXT("compile-lifecycle");
			Options.TypeContext = TypeContext;
		}

		FString DescribeFailure(const asCBuilder& Builder) const
		{
			for (uint8 Stage = 0; Stage < uint8(AS_NAMESPACE_QUALIFIER asEBuilderStage::Failed); ++Stage)
			{
				if (const auto* Result = Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage(Stage)); Result && !Result->Succeeded)
				{
					return FString::Printf(TEXT("Stage %d: %s"), int32(Stage), *Result->Error);
				}
			}
			return TEXT("No failed stage observation");
		}

		static asCScriptFunction* FindNamedFunction(asCDefinitions& Set, const char* Name)
		{
			asCScriptFunction* Fallback = nullptr;
			for (asCScriptFunction* Function : Set.GetFunctions())
			{
				if (!Function || Function->name != Name) continue;
				if (!Function->objectType) return Function;
				Fallback = Function;
			}
			return Fallback;
		}
	};

public:
	TEST_METHOD(TakeSetReportsNullEngine)
	{
		FInput Input(TEXT("class Unit { int Value; void Set(int V) { Value = V; } }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		ASSERT_THAT(IsNotNull(Set.Get()));
		asCTypeInfo* Unit = Set->FindType("Unit");
		ASSERT_THAT(IsNotNull(Unit));
		ASSERT_THAT(IsNull(Unit->GetEngine()));
		ASSERT_THAT(IsTrue(Unit->GetTypeId() == -1));
	}

	TEST_METHOD(TakeSetMovesUniquePtr)
	{
		FInput Input(TEXT("class Unit { int Value; void Set(int V) { Value = V; } }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		ASSERT_THAT(IsNotNull(Set.Get()));
		ASSERT_THAT(IsNull(Builder.GetDefinitions()));
		ASSERT_THAT(IsNull(Builder.TakeDefinitions().Get()));
	}

	TEST_METHOD(TakeSetSecondBuilderResolvesFirst)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> SharedContext =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();

		FInput FirstInput(TEXT("class First { int Value; }"));
		FirstInput.TypeContext = SharedContext;
		FirstInput.Options.TypeContext = SharedContext;
		asCBuilder FirstBuilder(FirstInput.Sources, FirstInput.Options);
		ASSERT_THAT(IsTrue(FirstBuilder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), FirstInput.DescribeFailure(FirstBuilder)));
		TUniquePtr<asCDefinitions> FirstSet = FirstBuilder.TakeDefinitions();
		ASSERT_THAT(IsNotNull(FirstSet.Get()));
		ASSERT_THAT(IsNotNull(FirstSet->FindType("First")));

		FInput SecondInput(TEXT("class Second { First@ Field; }"));
		SecondInput.TypeContext = SharedContext;
		SecondInput.Options.TypeContext = SharedContext;
		SecondInput.Options.Dependencies.Add(FirstSet.Get());
		asCBuilder SecondBuilder(SecondInput.Sources, SecondInput.Options);
		ASSERT_THAT(IsTrue(SecondBuilder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), SecondInput.DescribeFailure(SecondBuilder)));
		TUniquePtr<asCDefinitions> SecondSet = SecondBuilder.TakeDefinitions();
		ASSERT_THAT(IsNotNull(SecondSet.Get()));
		ASSERT_THAT(IsNotNull(SecondSet->FindType("Second")));
		ASSERT_THAT(IsNull(SecondSet->FindType("First")));
	}

	TEST_METHOD(TakeSetDestroyDeletesTypes)
	{
		asSDefinitionDestroyCounters Counters;
		{
			FInput Input(TEXT("class Gone { }"));
			asCBuilder Builder(Input.Sources, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
			TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
			ASSERT_THAT(IsNotNull(Set.Get()));
			ASSERT_THAT(IsNotNull(Set->FindType("Gone")));
			Set->SetDestroyCounters(&Counters);
			Set.Reset();
		}
		ASSERT_THAT(IsTrue(Counters.Types.load() >= 1));
		FInput Again(TEXT("class Gone { }"));
		asCBuilder Second(Again.Sources, Again.Options);
		ASSERT_THAT(IsTrue(Second.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Again.DescribeFailure(Second)));
		TUniquePtr<asCDefinitions> AgainSet = Second.TakeDefinitions();
		ASSERT_THAT(IsNotNull(AgainSet.Get()));
		ASSERT_THAT(IsNotNull(AgainSet->FindType("Gone")));
	}

	TEST_METHOD(TakeSetFailedRunThroughYieldsNothing)
	{
		FInput Input(TEXT("class {"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)));
		ASSERT_THAT(IsNull(Builder.TakeDefinitions().Get()));
	}

	TEST_METHOD(TakeSetMutableDependencyRejected)
	{
		FInput FirstInput(TEXT("class First { int Value; }"));
		asCBuilder FirstBuilder(FirstInput.Sources, FirstInput.Options);
		ASSERT_THAT(IsTrue(FirstBuilder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsBuilt), FirstInput.DescribeFailure(FirstBuilder)));
		const asCDefinitions* Unfrozen = FirstBuilder.GetDefinitions();
		ASSERT_THAT(IsNotNull(Unfrozen));
		ASSERT_THAT(IsFalse(Unfrozen->IsImmutable()));

		FInput SecondInput(TEXT("class Second { First@ Field; }"));
		SecondInput.Options.TypeContext = FirstInput.TypeContext;
		SecondInput.Options.Dependencies.Add(const_cast<asCDefinitions*>(Unfrozen));
		asCBuilder SecondBuilder(SecondInput.Sources, SecondInput.Options);
		ASSERT_THAT(IsFalse(SecondBuilder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)));
		ASSERT_THAT(IsNull(SecondBuilder.TakeDefinitions().Get()));
	}

	TEST_METHOD(CompileOutputReportsClassName)
	{
		FInput Input(TEXT("class Widget { int A; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		const asCCompileOutput* Output = Builder.GetCompileOutput();
		ASSERT_THAT(IsNotNull(Output));
		ASSERT_THAT(IsTrue(Output->GetDefinitionOutput().GetModules().Num() > 0));
		ASSERT_THAT(IsTrue(Output->GetDefinitionOutput().GetModules()[0]->Classes.Num() > 0));
		ASSERT_THAT(AreEqual(FString(TEXT("Widget")), Output->GetDefinitionOutput().GetModules()[0]->Classes[0]->ClassName));
	}

	TEST_METHOD(CompileOutputLeavesScriptTypeNull)
	{
		FInput Input(TEXT("class Widget { int A; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		const asCCompileOutput* Output = Builder.GetCompileOutput();
		ASSERT_THAT(IsNotNull(Output));
		const TSharedRef<FAngelscriptClassDesc>& Class = Output->GetDefinitionOutput().GetModules()[0]->Classes[0];
		ASSERT_THAT(IsNull(Class->ScriptType));
		ASSERT_THAT(IsNull(Output->GetDefinitionOutput().GetModules()[0]->ScriptModule));
		for (const TSharedRef<FAngelscriptFunctionDesc>& Method : Class->Methods)
		{
			ASSERT_THAT(IsNull(Method->ScriptFunction));
		}
	}

	TEST_METHOD(ProjectedUStructSurvivesDefinitionsFrozen)
	{
		FInput Input(TEXT("USTRUCT() struct FPoint { UPROPERTY() int X; };"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		const asCCompileOutput* Output = Builder.GetCompileOutput();
		ASSERT_THAT(IsNotNull(Output));
		ASSERT_THAT(IsTrue(Output->GetDefinitionOutput().GetModules().Num() > 0));
		ASSERT_THAT(IsTrue(Output->GetDefinitionOutput().GetModules()[0]->Classes.Num() > 0));
		const TSharedRef<FAngelscriptClassDesc>& Class = Output->GetDefinitionOutput().GetModules()[0]->Classes[0];
		ASSERT_THAT(AreEqual(FString(TEXT("FPoint")), Class->ClassName));
		ASSERT_THAT(IsTrue(Class->bIsStruct));
		ASSERT_THAT(AreEqual(1, Class->Properties.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("X")), Class->Properties[0]->PropertyName));
		ASSERT_THAT(IsNull(Class->ScriptType));
	}

	TEST_METHOD(ProjectedUStructSurvivesByteCodeEmitted)
	{
		FInput Input(TEXT("USTRUCT() struct FPoint { UPROPERTY() int X; };"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		const asCCompileOutput* Output = Builder.GetCompileOutput();
		ASSERT_THAT(IsNotNull(Output));
		const TSharedRef<FAngelscriptClassDesc>& Class = Output->GetDefinitionOutput().GetModules()[0]->Classes[0];
		ASSERT_THAT(AreEqual(FString(TEXT("FPoint")), Class->ClassName));
		ASSERT_THAT(IsTrue(Class->bIsStruct));
		ASSERT_THAT(AreEqual(1, Class->Properties.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("X")), Class->Properties[0]->PropertyName));
		ASSERT_THAT(IsNull(Class->ScriptType));
	}

	TEST_METHOD(CompileOutputHasNoTypeInfo)
	{
		FInput Input(TEXT("class Widget { int A; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		ASSERT_THAT(IsNotNull(Builder.GetCompileOutput()));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		ASSERT_THAT(IsNotNull(Set.Get()));
		ASSERT_THAT(IsNotNull(Set->FindType("Widget")));
	}

	TEST_METHOD(CompileOutputTakeMovesBag)
	{
		FInput Input(TEXT("class Widget { int A; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		TUniquePtr<asCCompileOutput> Output = Builder.TakeCompileOutput();
		ASSERT_THAT(IsNotNull(Output.Get()));
		ASSERT_THAT(IsNull(Builder.GetCompileOutput()));
	}

	TEST_METHOD(CompileOutputFailedKeepsDiagnostics)
	{
		FInput Input(TEXT("class {"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)));
		const asCCompileOutput* Output = Builder.GetCompileOutput();
		ASSERT_THAT(IsNotNull(Output));
		ASSERT_THAT(IsTrue(Output->GetDiagnostics().Num() > 0));
		ASSERT_THAT(IsNull(Builder.TakeDefinitions().Get()));
	}

	TEST_METHOD(EmitWritesStableOnFunction)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::ByteCodeEmitted), Input.DescribeFailure(Builder)));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		ASSERT_THAT(IsNotNull(Set.Get()));
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		ASSERT_THAT(IsTrue(Identity->GetStableByteCode().Num() > 0));
	}

	TEST_METHOD(RunThroughDefaultEmits)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		ASSERT_THAT(IsTrue(Builder.GetStage() == AS_NAMESPACE_QUALIFIER asEBuilderStage::ByteCodeEmitted));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		ASSERT_THAT(IsNotNull(Set.Get()));
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		ASSERT_THAT(IsTrue(Identity->GetStableByteCode().Num() > 0));
	}

	TEST_METHOD(EmitWithoutEngine)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		ASSERT_THAT(IsNull(Identity->GetEngine()));
		ASSERT_THAT(IsTrue(Identity->GetStableByteCode().Num() > 0));
	}

	TEST_METHOD(EmitNativeSystemEmptyStable)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCDefinitions> Set = asCDefinitions::Create();
		ASSERT_THAT(IsTrue(Set->SetIdentityContext(Context) == asEDefinitionResult::Succeeded));
		asSFunctionIdentityDescriptor Identity;
		Identity.StableScope = TEXT("compile-lifecycle");
		Identity.Name = TEXT("NativeAdd");
		Identity.Kind = asEFunctionIdentityKind::Global;
		Context->InternPrimitiveTypeUse(asEPrimitiveType::Int32, Identity.ReturnType);
		asSStableKey Key;
		ASSERT_THAT(IsTrue(Context->GetIdentityRegistry().InternFunction(Identity, Key) == asETypeIdentityStatus::Succeeded));
		asCScriptFunction* Native = nullptr;
		ASSERT_THAT(IsTrue(Set->CreateFunction(Key, "NativeAdd", "", asFUNC_SYSTEM, Native) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsNotNull(Native));
		ASSERT_THAT(IsTrue(Native->GetStableByteCode().Num() == 0));
	}

	TEST_METHOD(EmitStopAtFrozenSkipsStable)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		ASSERT_THAT(IsTrue(Identity->GetStableByteCode().Num() == 0));
	}

	TEST_METHOD(RegisterInstallsEngineAndTypeId)
	{
		FInput Input(TEXT("class Unit { int Value; int Get() { return Value; } }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		asCTypeInfo* Unit = Set->FindType("Unit");
		ASSERT_THAT(IsNotNull(Unit));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCDefinitions>> Units;
		Units.Add(MoveTemp(Set));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units)) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(Unit->GetEngine() == Engine));
		ASSERT_THAT(IsTrue(Unit->GetTypeId() > asTYPEID_LAST_PRIMITIVE));
		ASSERT_THAT(IsNull(Engine->GetModule("Main.as")));
	}

	TEST_METHOD(RegisterCreatesPerFileModules)
	{
		FInput Input(TArray<TPair<FString, FString>>{
			{TEXT("First.as"), TEXT("class First {}")},
			{TEXT("Second.as"), TEXT("class Second {}")}});
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		const asCCompileOutput* Output = Builder.GetCompileOutput();
		ASSERT_THAT(IsNotNull(Output));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		asCTypeInfo* First = Set->FindType("First");
		asCTypeInfo* Second = Set->FindType("Second");
		ASSERT_THAT(IsNotNull(First));
		ASSERT_THAT(IsNotNull(Second));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCDefinitions>> Units;
		Units.Add(MoveTemp(Set));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units), *Output) == asERegistrationResult::Succeeded));
		asIScriptModule* FirstModule = Engine->GetModule("First.as");
		asIScriptModule* SecondModule = Engine->GetModule("Second.as");
		ASSERT_THAT(IsNotNull(FirstModule));
		ASSERT_THAT(IsNotNull(SecondModule));
		ASSERT_THAT(IsTrue(FirstModule != SecondModule));
		ASSERT_THAT(IsTrue(Engine->GetModuleCount() >= 2));
		ASSERT_THAT(IsTrue(First->module == FirstModule));
		ASSERT_THAT(IsTrue(Output->GetDefinitionOutput().GetModules()[0]->ScriptModule == FirstModule
			|| Output->GetDefinitionOutput().GetModules()[1]->ScriptModule == FirstModule));
		ASSERT_THAT(IsNull(Engine->GetModule("Other.as")));
		ASSERT_THAT(IsNotNull(Engine->GetModule("First.as")));
		ASSERT_THAT(IsNotNull(Engine->GetModule("Second.as")));
	}

	TEST_METHOD(RegisterCompiledDefinitionsForwardsOutput)
	{
		FInput Input(TArray<TPair<FString, FString>>{
			{TEXT("First.as"), TEXT("class First {}")},
			{TEXT("Second.as"), TEXT("class Second {}")}});
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		ASSERT_THAT(IsTrue(RegisterCompiledDefinitions(*Engine, Builder) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsNotNull(Engine->GetModule("First.as")));
	}

	TEST_METHOD(RegisterWritesRuntimeBytecode)
	{
		FInput Input(TEXT("class Unit { int Value; int Get() { return Value; } }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		asCScriptFunction* Get = FInput::FindNamedFunction(*Set, "Get");
		ASSERT_THAT(IsNotNull(Get));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCDefinitions>> Units;
		Units.Add(MoveTemp(Set));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units)) == asERegistrationResult::Succeeded));
		asUINT Length = 0;
		Get->GetByteCode(&Length);
		ASSERT_THAT(IsTrue(Length > 0));
	}

	TEST_METHOD(PrepareBeforeRegisterReturnsNoFunction)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asIScriptContext* Context = Engine->CreateContext();
		ON_SCOPE_EXIT { if (Context) Context->Release(); };
		ASSERT_THAT(IsTrue(Context->Prepare(Identity) == asINVALID_ARG));
	}

	TEST_METHOD(PrepareAfterRegisterExecutes)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCDefinitions>> Units;
		Units.Add(MoveTemp(Set));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units)) == asERegistrationResult::Succeeded));
		asIScriptContext* Context = Engine->CreateContext();
		ON_SCOPE_EXIT { if (Context) Context->Release(); };
		ASSERT_THAT(IsTrue(Context->Prepare(Identity) >= 0, TEXT("Prepare after Register")));
		Context->SetArgDWord(0, 7);
		ASSERT_THAT(IsTrue(Context->Execute() == asEXECUTION_FINISHED));
		ASSERT_THAT(IsTrue(Context->GetReturnDWord() == 7));
	}

	TEST_METHOD(RegisterAllOrNothing)
	{
		FInput FirstInput(TEXT("class Unit { int Value; }"));
		asCBuilder FirstBuilder(FirstInput.Sources, FirstInput.Options);
		ASSERT_THAT(IsTrue(FirstBuilder.RunThrough(), FirstInput.DescribeFailure(FirstBuilder)));
		TUniquePtr<asCDefinitions> FirstSet = FirstBuilder.TakeDefinitions();
		FInput SecondInput(TEXT("class Unit { int Other; }"));
		asCBuilder SecondBuilder(SecondInput.Sources, SecondInput.Options);
		ASSERT_THAT(IsTrue(SecondBuilder.RunThrough(), SecondInput.DescribeFailure(SecondBuilder)));
		TUniquePtr<asCDefinitions> SecondSet = SecondBuilder.TakeDefinitions();
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asITypeInfo* Existing = Engine->GetTypeInfoByName("$obj");
		ASSERT_THAT(IsNotNull(Existing));
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCDefinitions>> Units;
		Units.Add(MoveTemp(FirstSet));
		Units.Add(MoveTemp(SecondSet));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units)) != asERegistrationResult::Succeeded));
		ASSERT_THAT(IsNull(Engine->FindType("", "Unit")));
		ASSERT_THAT(IsTrue(Engine->GetTypeInfoByName("$obj") == Existing));
	}

	TEST_METHOD(RegisterDoesNotUseImage)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCDefinitions>> Units;
		Units.Add(MoveTemp(Set));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units)) == asERegistrationResult::Succeeded));
	}

	TEST_METHOD(DefinitionSetOwnsWithoutImage)
	{
		TUniquePtr<asCDefinitions> Set = MakeUnique<asCDefinitions>();
		asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = TEXT("compile-lifecycle");
		Descriptor.Name = TEXT("OwnedNode");
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		ASSERT_THAT(IsTrue(Set->SetIdentityContext(Context) == asEDefinitionResult::Succeeded));
		asSStableKey Key;
		ASSERT_THAT(IsTrue(Context->GetIdentityRegistry().InternNominalType(Descriptor, Key) == asETypeIdentityStatus::Succeeded));
		asCObjectType* Owned = nullptr;
		ASSERT_THAT(IsTrue(Set->CreateObjectType(Key, "OwnedNode", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Owned) == asEDefinitionResult::Succeeded));
		ASSERT_THAT(IsNotNull(Owned));
		ASSERT_THAT(IsTrue(Set->FindType("OwnedNode") == Owned));
		ASSERT_THAT(IsTrue(Set->GetTypes().Num() >= 1));
		ASSERT_THAT(IsTrue(Owned->GetDefinitions() == Set.Get()));
		ASSERT_THAT(IsNull(Owned->GetEngine()));
		ASSERT_THAT(IsTrue(Owned->GetTypeId() == -1));
	}

	TEST_METHOD(ConsumerFillsSetNotImage)
	{
		FInput Input(TEXT("class Node { Node@ Next; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::ASTVerified), Input.DescribeFailure(Builder)));
		auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Builder.GetCompilationSession());
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsNotNull(Result.Definitions.Get()));
		asCTypeInfo* Node = Result.Definitions->FindType("Node");
		ASSERT_THAT(IsNotNull(Node));
		ASSERT_THAT(IsTrue(Node->GetDefinitions() == Result.Definitions.Get()));
		ASSERT_THAT(IsNull(Node->GetEngine()));
		ASSERT_THAT(IsTrue(Node->GetTypeId() == -1));
	}

	TEST_METHOD(RegistrationInstallsWithoutRegisterMetadataImage)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCDefinitions> Set = Builder.TakeDefinitions();
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCDefinitions>> Units;
		Units.Add(MoveTemp(Set));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units)) == asERegistrationResult::Succeeded));
		asIScriptContext* Context = Engine->CreateContext();
		ON_SCOPE_EXIT { if (Context) Context->Release(); };
		ASSERT_THAT(IsTrue(Context->Prepare(Identity) >= 0));
		Context->SetArgDWord(0, 7);
		ASSERT_THAT(IsTrue(Context->Execute() == asEXECUTION_FINISHED));
		ASSERT_THAT(IsTrue(Context->GetReturnDWord() == 7));
	}
};

#endif
