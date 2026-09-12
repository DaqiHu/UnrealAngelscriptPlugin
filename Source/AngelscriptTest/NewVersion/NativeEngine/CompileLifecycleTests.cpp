#include "CQTest.h"
#include "NativeEngineTestSupport.h"
#include "as_builder.h"
#include "as_compile_output.h"
#include "as_engine_compile_registration.h"
#include "as_module_definition_set.h"
#include "as_module_definition_set.h"
#include "frontend/Compile/as_definition_consumer.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"
#include "frontend/AST/as_type_context.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(CompileLifecycle, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FInput
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		AS_NAMESPACE_QUALIFIER asCSourceManager Sources{Snapshot};
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics{Sources};
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;

		explicit FInput(const FString& Source)
		{
			const auto Input = AngelscriptNativeEngineTest::FSourceInput::FromText(TEXT("Main.as"), Source);
			Snapshot->AddFile(TEXT("Main.as"), Input.GetUtf8Bytes());
			Snapshot->Freeze();
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

		static asCScriptFunction* FindNamedFunction(asCModuleDefinitionSet& Set, const char* Name)
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
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		ASSERT_THAT(IsNotNull(Set.Get()));
		asCTypeInfo* Unit = Set->FindType("Unit");
		ASSERT_THAT(IsNotNull(Unit));
		ASSERT_THAT(IsNull(Unit->GetEngine()));
		ASSERT_THAT(IsTrue(Unit->GetTypeId() == -1));
	}

	TEST_METHOD(TakeSetMovesUniquePtr)
	{
		FInput Input(TEXT("class Unit { int Value; void Set(int V) { Value = V; } }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		ASSERT_THAT(IsNotNull(Set.Get()));
		ASSERT_THAT(IsNull(Builder.GetModuleDefinitionSet()));
		ASSERT_THAT(IsNull(Builder.TakeModuleDefinitionSet().Get()));
	}

	TEST_METHOD(TakeSetSecondBuilderResolvesFirst)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> SharedContext =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();

		FInput FirstInput(TEXT("class First { int Value; }"));
		FirstInput.TypeContext = SharedContext;
		FirstInput.Options.TypeContext = SharedContext;
		asCBuilder FirstBuilder(FirstInput.Snapshot, FirstInput.Diagnostics, FirstInput.Options);
		ASSERT_THAT(IsTrue(FirstBuilder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), FirstInput.DescribeFailure(FirstBuilder)));
		TUniquePtr<asCModuleDefinitionSet> FirstSet = FirstBuilder.TakeModuleDefinitionSet();
		ASSERT_THAT(IsNotNull(FirstSet.Get()));
		ASSERT_THAT(IsNotNull(FirstSet->FindType("First")));

		FInput SecondInput(TEXT("class Second { First@ Field; }"));
		SecondInput.TypeContext = SharedContext;
		SecondInput.Options.TypeContext = SharedContext;
		SecondInput.Options.Dependencies.Add(FirstSet.Get());
		asCBuilder SecondBuilder(SecondInput.Snapshot, SecondInput.Diagnostics, SecondInput.Options);
		ASSERT_THAT(IsTrue(SecondBuilder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), SecondInput.DescribeFailure(SecondBuilder)));
		TUniquePtr<asCModuleDefinitionSet> SecondSet = SecondBuilder.TakeModuleDefinitionSet();
		ASSERT_THAT(IsNotNull(SecondSet.Get()));
		ASSERT_THAT(IsNotNull(SecondSet->FindType("Second")));
		ASSERT_THAT(IsNull(SecondSet->FindType("First")));
	}

	TEST_METHOD(TakeSetDestroyDeletesTypes)
	{
		asCTypeInfo* FirstPointer = nullptr;
		{
			FInput Input(TEXT("class Gone { }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
			TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
			ASSERT_THAT(IsNotNull(Set.Get()));
			FirstPointer = Set->FindType("Gone");
			ASSERT_THAT(IsNotNull(FirstPointer));
			Set.Reset();
		}
		FInput Again(TEXT("class Gone { }"));
		asCBuilder Second(Again.Snapshot, Again.Diagnostics, Again.Options);
		ASSERT_THAT(IsTrue(Second.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Again.DescribeFailure(Second)));
		TUniquePtr<asCModuleDefinitionSet> AgainSet = Second.TakeModuleDefinitionSet();
		ASSERT_THAT(IsNotNull(AgainSet.Get()));
		asCTypeInfo* SecondPointer = AgainSet->FindType("Gone");
		ASSERT_THAT(IsNotNull(SecondPointer));
		ASSERT_THAT(IsTrue(SecondPointer != FirstPointer));
	}

	TEST_METHOD(TakeSetFailedRunThroughYieldsNothing)
	{
		FInput Input(TEXT("class {"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)));
		ASSERT_THAT(IsNull(Builder.TakeModuleDefinitionSet().Get()));
	}

	TEST_METHOD(TakeSetMutableDependencyRejected)
	{
		FInput FirstInput(TEXT("class First { int Value; }"));
		asCBuilder FirstBuilder(FirstInput.Snapshot, FirstInput.Diagnostics, FirstInput.Options);
		ASSERT_THAT(IsTrue(FirstBuilder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsBuilt), FirstInput.DescribeFailure(FirstBuilder)));
		const asCModuleDefinitionSet* Unfrozen = FirstBuilder.GetModuleDefinitionSet();
		ASSERT_THAT(IsNotNull(Unfrozen));
		ASSERT_THAT(IsFalse(Unfrozen->IsImmutable()));

		FInput SecondInput(TEXT("class Second { First@ Field; }"));
		SecondInput.Options.TypeContext = FirstInput.TypeContext;
		SecondInput.Options.Dependencies.Add(const_cast<asCModuleDefinitionSet*>(Unfrozen));
		asCBuilder SecondBuilder(SecondInput.Snapshot, SecondInput.Diagnostics, SecondInput.Options);
		ASSERT_THAT(IsFalse(SecondBuilder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)));
		ASSERT_THAT(IsNull(SecondBuilder.TakeModuleDefinitionSet().Get()));
	}

	TEST_METHOD(CompileOutputReportsClassName)
	{
		FInput Input(TEXT("class Widget { int A; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
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
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		const asCCompileOutput* Output = Builder.GetCompileOutput();
		ASSERT_THAT(IsNotNull(Output));
		const TSharedRef<FAngelscriptClassDesc>& Class = Output->GetDefinitionOutput().GetModules()[0]->Classes[0];
		ASSERT_THAT(IsNull(Class->ScriptType));
		for (const TSharedRef<FAngelscriptFunctionDesc>& Method : Class->Methods)
		{
			ASSERT_THAT(IsNull(Method->ScriptFunction));
		}
	}

	TEST_METHOD(CompileOutputHasNoTypeInfo)
	{
		FInput Input(TEXT("class Widget { int A; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		ASSERT_THAT(IsNotNull(Builder.GetCompileOutput()));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		ASSERT_THAT(IsNotNull(Set.Get()));
		ASSERT_THAT(IsNotNull(Set->FindType("Widget")));
	}

	TEST_METHOD(CompileOutputTakeMovesBag)
	{
		FInput Input(TEXT("class Widget { int A; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		TUniquePtr<asCCompileOutput> Output = Builder.TakeCompileOutput();
		ASSERT_THAT(IsNotNull(Output.Get()));
		ASSERT_THAT(IsNull(Builder.GetCompileOutput()));
	}

	TEST_METHOD(CompileOutputFailedKeepsDiagnostics)
	{
		FInput Input(TEXT("class {"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)));
		const asCCompileOutput* Output = Builder.GetCompileOutput();
		ASSERT_THAT(IsNotNull(Output));
		ASSERT_THAT(IsTrue(Output->GetDiagnostics().Num() > 0));
		ASSERT_THAT(IsNull(Builder.TakeModuleDefinitionSet().Get()));
	}

	TEST_METHOD(EmitWritesStableOnFunction)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::ByteCodeEmitted), Input.DescribeFailure(Builder)));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		ASSERT_THAT(IsNotNull(Set.Get()));
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		ASSERT_THAT(IsTrue(Identity->GetStableByteCode().Num() > 0));
	}

	TEST_METHOD(RunThroughDefaultEmits)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		ASSERT_THAT(IsTrue(Builder.GetStage() == AS_NAMESPACE_QUALIFIER asEBuilderStage::ByteCodeEmitted));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		ASSERT_THAT(IsNotNull(Set.Get()));
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		ASSERT_THAT(IsTrue(Identity->GetStableByteCode().Num() > 0));
	}

	TEST_METHOD(EmitWithoutEngine)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		ASSERT_THAT(IsNull(Identity->GetEngine()));
		ASSERT_THAT(IsTrue(Identity->GetStableByteCode().Num() > 0));
	}

	TEST_METHOD(EmitNativeSystemEmptyStable)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCModuleDefinitionSet> Set = asCModuleDefinitionSet::Create();
		ASSERT_THAT(IsTrue(Set->SetIdentityContext(Context) == asEMetadataResult::Succeeded));
		asSFunctionIdentityDescriptor Identity;
		Identity.StableScope = TEXT("compile-lifecycle");
		Identity.Name = TEXT("NativeAdd");
		Identity.Kind = asEFunctionIdentityKind::Global;
		Context->InternPrimitiveTypeUse(asEPrimitiveType::Int32, Identity.ReturnType);
		asSStableKey Key;
		ASSERT_THAT(IsTrue(Context->GetIdentityRegistry().InternFunction(Identity, Key) == asETypeIdentityStatus::Succeeded));
		asCScriptFunction* Native = nullptr;
		ASSERT_THAT(IsTrue(Set->CreateFunction(Key, "NativeAdd", "", asFUNC_SYSTEM, Native) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsNotNull(Native));
		ASSERT_THAT(IsTrue(Native->GetStableByteCode().Num() == 0));
	}

	TEST_METHOD(EmitStopAtFrozenSkipsStable)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen), Input.DescribeFailure(Builder)));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		ASSERT_THAT(IsTrue(Identity->GetStableByteCode().Num() == 0));
	}

	TEST_METHOD(RegisterInstallsEngineAndTypeId)
	{
		FInput Input(TEXT("class Unit { int Value; int Get() { return Value; } }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		asCTypeInfo* Unit = Set->FindType("Unit");
		ASSERT_THAT(IsNotNull(Unit));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCModuleDefinitionSet>> Units;
		Units.Add(MoveTemp(Set));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units)) == asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(Unit->GetEngine() == Engine));
		ASSERT_THAT(IsTrue(Unit->GetTypeId() > asTYPEID_LAST_PRIMITIVE));
	}

	TEST_METHOD(RegisterWritesRuntimeBytecode)
	{
		FInput Input(TEXT("class Unit { int Value; int Get() { return Value; } }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		asCScriptFunction* Get = FInput::FindNamedFunction(*Set, "Get");
		ASSERT_THAT(IsNotNull(Get));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCModuleDefinitionSet>> Units;
		Units.Add(MoveTemp(Set));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units)) == asEMetadataRegistrationResult::Succeeded));
		asUINT Length = 0;
		Get->GetByteCode(&Length);
		ASSERT_THAT(IsTrue(Length > 0));
	}

	TEST_METHOD(PrepareBeforeRegisterReturnsNoFunction)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asIScriptContext* Context = Engine->CreateContext();
		ON_SCOPE_EXIT { if (Context) Context->Release(); };
		ASSERT_THAT(IsTrue(Context->Prepare(Identity) == asNO_FUNCTION));
	}

	TEST_METHOD(PrepareAfterRegisterExecutes)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCModuleDefinitionSet>> Units;
		Units.Add(MoveTemp(Set));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units)) == asEMetadataRegistrationResult::Succeeded));
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
		asCBuilder FirstBuilder(FirstInput.Snapshot, FirstInput.Diagnostics, FirstInput.Options);
		ASSERT_THAT(IsTrue(FirstBuilder.RunThrough(), FirstInput.DescribeFailure(FirstBuilder)));
		TUniquePtr<asCModuleDefinitionSet> FirstSet = FirstBuilder.TakeModuleDefinitionSet();
		FInput SecondInput(TEXT("class Unit { int Other; }"));
		asCBuilder SecondBuilder(SecondInput.Snapshot, SecondInput.Diagnostics, SecondInput.Options);
		ASSERT_THAT(IsTrue(SecondBuilder.RunThrough(), SecondInput.DescribeFailure(SecondBuilder)));
		TUniquePtr<asCModuleDefinitionSet> SecondSet = SecondBuilder.TakeModuleDefinitionSet();
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asITypeInfo* Existing = Engine->GetTypeInfoByName("$obj");
		ASSERT_THAT(IsNotNull(Existing));
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCModuleDefinitionSet>> Units;
		Units.Add(MoveTemp(FirstSet));
		Units.Add(MoveTemp(SecondSet));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units)) != asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(IsNull(Engine->FindMetadataType("", "Unit")));
		ASSERT_THAT(IsTrue(Engine->GetTypeInfoByName("$obj") == Existing));
	}

	TEST_METHOD(RegisterDoesNotUseImage)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCModuleDefinitionSet>> Units;
		Units.Add(MoveTemp(Set));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units)) == asEMetadataRegistrationResult::Succeeded));
	}

	TEST_METHOD(DefinitionSetOwnsWithoutImage)
	{
		TUniquePtr<asCModuleDefinitionSet> Set = MakeUnique<asCModuleDefinitionSet>();
		asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = TEXT("compile-lifecycle");
		Descriptor.Name = TEXT("OwnedNode");
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		ASSERT_THAT(IsTrue(Set->SetIdentityContext(Context) == asEMetadataResult::Succeeded));
		asSStableKey Key;
		ASSERT_THAT(IsTrue(Context->GetIdentityRegistry().InternNominalType(Descriptor, Key) == asETypeIdentityStatus::Succeeded));
		asCObjectType* Owned = nullptr;
		ASSERT_THAT(IsTrue(Set->CreateObjectType(Key, "OwnedNode", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Owned) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsNotNull(Owned));
		ASSERT_THAT(IsTrue(Set->FindType("OwnedNode") == Owned));
		ASSERT_THAT(IsTrue(Set->GetTypes().Num() >= 1));
		ASSERT_THAT(IsTrue(Owned->GetDefinitionSet() == Set.Get()));
		ASSERT_THAT(IsNull(Owned->GetEngine()));
		ASSERT_THAT(IsTrue(Owned->GetTypeId() == -1));
	}

	TEST_METHOD(ConsumerFillsSetNotImage)
	{
		FInput Input(TEXT("class Node { Node@ Next; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::ASTVerified), Input.DescribeFailure(Builder)));
		auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Builder.GetCompilationSession());
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsNotNull(Result.Definitions.Get()));
		asCTypeInfo* Node = Result.Definitions->FindType("Node");
		ASSERT_THAT(IsNotNull(Node));
		ASSERT_THAT(IsTrue(Node->GetDefinitionSet() == Result.Definitions.Get()));
		ASSERT_THAT(IsNull(Node->GetEngine()));
		ASSERT_THAT(IsTrue(Node->GetTypeId() == -1));
	}

	TEST_METHOD(RegistrationInstallsWithoutRegisterMetadataImage)
	{
		FInput Input(TEXT("int Identity(int V) { return V; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), Input.DescribeFailure(Builder)));
		TUniquePtr<asCModuleDefinitionSet> Set = Builder.TakeModuleDefinitionSet();
		asCScriptFunction* Identity = FInput::FindNamedFunction(*Set, "Identity");
		ASSERT_THAT(IsNotNull(Identity));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		asCEngineCompileRegistration Registration(*Engine);
		TArray<TUniquePtr<asCModuleDefinitionSet>> Units;
		Units.Add(MoveTemp(Set));
		ASSERT_THAT(IsTrue(Registration.Register(MoveTemp(Units)) == asEMetadataRegistrationResult::Succeeded));
		asIScriptContext* Context = Engine->CreateContext();
		ON_SCOPE_EXIT { if (Context) Context->Release(); };
		ASSERT_THAT(IsTrue(Context->Prepare(Identity) >= 0));
		Context->SetArgDWord(0, 7);
		ASSERT_THAT(IsTrue(Context->Execute() == asEXECUTION_FINISHED));
		ASSERT_THAT(IsTrue(Context->GetReturnDWord() == 7));
	}
};

#endif
