#include "CQTest.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptEngine.h"
#include "Misc/ScopeExit.h"
#include "NativeEngine/NativeEngineTestSupport.h"
#include "as_builder.h"
#include "as_callfunc.h"
#include "as_definitions.h"
#include "as_engine_compile_registration.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"
#include "frontend/AST/as_type_context.h"
#include "frontend/Sema/as_type_identity.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FHostSchemePair
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Sum() const { return X + Y; }
	};

	void HostSchemeRegisterPair(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FHostSchemePair>("Pair", Flags);
	}

	void HostSchemeRegisterSum(FAngelscriptBinds& Binds)
	{
		Binds.ExistingClassForTarget("Pair").Method("int Sum() const", &FHostSchemePair::Sum);
	}

	void HostSchemeRegisterFailedInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionDirectForTarget(
			"void Missing()",
			asSFuncPtr(),
			asCALL_CDECL,
			ASAutoCaller::FunctionCaller::Make());
	}

	FAngelscriptBindRecord HostSchemeMakeRecord(
		FName BindName,
		EAngelscriptBindPhase Phase,
		FAngelscriptBindCallback Callback,
		int32 Line)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("AngelscriptTest");
		Record.BindName = BindName;
		Record.Phase = Phase;
		Record.SourceFile = "RuntimeBindingHostSchemeTests.cpp";
		Record.SourceLine = Line;
		Record.Callback = Callback;
		return Record;
	}

	bool HostSchemeAppendNamed(
		UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
		FName BindName,
		FString& Diagnostic)
	{
		for (const FAngelscriptBindRecord& Record : FAngelscriptBind::GetRegisteredCollection().GetRecords())
		{
			if (Record.BindName == BindName)
			{
				return Collection.Append(Record, Diagnostic);
			}
		}

		Diagnostic = FString::Printf(TEXT("missing production bind %s"), *BindName.ToString());
		return false;
	}

	bool HostSchemeHasBindName(
		TConstArrayView<FAngelscriptBindRecord> Records,
		FName BindName)
	{
		for (const FAngelscriptBindRecord& Record : Records)
		{
			if (Record.BindName == BindName)
			{
				return true;
			}
		}

		return false;
	}
}

TEST_CLASS_WITH_FLAGS(HostScheme, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ProcessCollectionCopiesEveryRegisteredRecord)
	{
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic), *Diagnostic));

		ASSERT_THAT(IsTrue(
			FAngelscriptBind::EnsureProcessHostCollection(Diagnostic),
			Diagnostic.IsEmpty() ? TEXT("<empty host capture diagnostic>") : *Diagnostic));

		const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Host));
		const TConstArrayView<FAngelscriptBindRecord> HostRecords = Host->GetRecords();
		const TConstArrayView<FAngelscriptBindRecord> RegisteredRecords =
			FAngelscriptBind::GetRegisteredCollection().GetRecords();
		ASSERT_THAT(AreEqual(RegisteredRecords.Num(), HostRecords.Num()));
		ASSERT_THAT(IsTrue(HostSchemeHasBindName(HostRecords, TEXT("FString.TypeInfrastructure"))));
		ASSERT_THAT(IsTrue(HostSchemeHasBindName(HostRecords, TEXT("BlueprintType.ReflectionBindings"))));
	}

	TEST_METHOD(SevenPhasesInvokeInfrastructureAndReflection)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostSchemeAppendNamed(Collection, TEXT("FString.TypeDeclarations"), Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(HostSchemeAppendNamed(Collection, TEXT("FString.TypeInfrastructure"), Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(HostSchemeAppendNamed(Collection, TEXT("FString.ExplicitBindings"), Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(HostSchemeAppendNamed(Collection, TEXT("BlueprintType.ReflectionBindings"), Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));

		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));

		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("FString.TypeInfrastructure"))));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("BlueprintType.ReflectionBindings"))));
		const asCDefinitions* Graph = Collection.GetHostDefinitions();
		ASSERT_THAT(IsNotNull(Graph));
		ASSERT_THAT(IsTrue(Graph->GetState() == asEDefinitionState::Frozen));
	}

	TEST_METHOD(LocalPairSumStillFreezes)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Append(
			HostSchemeMakeRecord(TEXT("Pair"), EAngelscriptBindPhase::TypeDeclarations, &HostSchemeRegisterPair, 10),
			Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Append(
			HostSchemeMakeRecord(TEXT("PairSum"), EAngelscriptBindPhase::ExplicitBindings, &HostSchemeRegisterSum, 20),
			Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));

		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));

		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("Pair"))));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("PairSum"))));
		const asCDefinitions* Graph = Collection.GetHostDefinitions();
		ASSERT_THAT(IsNotNull(Graph));
		ASSERT_THAT(IsNotNull(Graph->FindType("Pair")));
		asCScriptFunction* Sum = nullptr;
		for (asCScriptFunction* Function : Graph->GetFunctions())
		{
			if (Function && Function->GetName() && FCStringAnsi::Strcmp(Function->GetName(), "Sum") == 0)
			{
				Sum = Function;
			}
		}
		ASSERT_THAT(IsNotNull(Sum));
		ASSERT_THAT(IsTrue(Graph->GetState() == asEDefinitionState::Frozen));
	}

	TEST_METHOD(FailedInfrastructurePublishesNoGraph)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Append(
			HostSchemeMakeRecord(
				TEXT("FailedInfrastructure"),
				EAngelscriptBindPhase::TypeInfrastructure,
				&HostSchemeRegisterFailedInfrastructure,
				30),
			Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));

		ASSERT_THAT(IsFalse(Collection.ExecuteToHost(Diagnostic)));

		ASSERT_THAT(IsFalse(Diagnostic.IsEmpty()));
		const asCDefinitions* Graph = Collection.GetHostDefinitions();
		ASSERT_THAT(IsTrue(Graph == nullptr || Graph->GetState() != asEDefinitionState::Frozen));
	}

	TEST_METHOD(BindScriptTypesInjectsWithoutReplay)
	{
		FString PrepareDiagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::PrepareForEngineInitialization(PrepareDiagnostic), *PrepareDiagnostic));

		FAngelscriptEngineConfig Config = FAngelscriptEngineConfig::FromCurrentProcess();
		Config.bSkipInitialCompile = true;
		Config.bOverrideCacheV2Enablement = true;
		Config.bEnableCacheV2 = false;
		TUniquePtr<FAngelscriptEngine> Owner = FAngelscriptEngine::Create(
			Config,
			FAngelscriptEngineDependencies::CreateDefault());
		ASSERT_THAT(IsNotNull(Owner.Get(), TEXT("BindScriptTypes should publish a local engine")));
		ASSERT_THAT(IsTrue(Owner->BindScriptTypes(), TEXT("inject-only BindScriptTypes should succeed")));

		const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Host));
		const asCDefinitions* Graph = Host->GetHostDefinitions();
		ASSERT_THAT(IsNotNull(Graph));
		ASSERT_THAT(IsTrue(Graph->GetState() == asEDefinitionState::Frozen));
		ASSERT_THAT(IsNull(Owner->GetBindingInstallation()));
		ASSERT_THAT(IsNotNull(Owner->GetBindState()));
		ASSERT_THAT(AreEqual(0, Owner->GetBindState()->DirectCallbackExecutionCountForTesting));
	}

	TEST_METHOD(NoArgCreateForBindingsInjectsCollection)
	{
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::EnsureProcessHostCollection(Diagnostic), *Diagnostic));
		const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Host));
		const asCDefinitions* Graph = Host->GetHostDefinitions();
		ASSERT_THAT(IsNotNull(Graph));
		asCTypeInfo* HostString = Graph->FindType("FString");
		ASSERT_THAT(IsNotNull(HostString));

		FAngelscriptBindingDiagnostic BindingDiagnostic;
		TUniquePtr<FAngelscriptEngine> Owner = FAngelscriptEngine::CreateForBindings(BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			Owner.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		ASSERT_THAT(IsNull(Owner->GetBindingInstallation()));
		ASSERT_THAT(IsNotNull(Owner->Engine));
		ASSERT_THAT(IsTrue(Owner->Engine->GetTypeInfoByName("FString") == HostString));
	}

	TEST_METHOD(NoArgCreateForBindingsHasNoInstallation)
	{
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::EnsureProcessHostCollection(Diagnostic), *Diagnostic));
		const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Host));
		asCTypeInfo* HostString = Host->GetHostDefinitions()->FindType("FString");
		ASSERT_THAT(IsNotNull(HostString));
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		TUniquePtr<FAngelscriptEngine> Owner = FAngelscriptEngine::CreateForBindings(BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			Owner.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		ASSERT_THAT(IsNull(Owner->GetBindingInstallation()));
		ASSERT_THAT(IsTrue(Owner->Engine->GetTypeInfoByName("FString") == HostString));
	}

	TEST_METHOD(NoArgCreateForBindingsSharesFString)
	{
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		TUniquePtr<FAngelscriptEngine> OwnerA = FAngelscriptEngine::CreateForBindings(BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			OwnerA.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty OwnerA CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		TUniquePtr<FAngelscriptEngine> OwnerB = FAngelscriptEngine::CreateForBindings(BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			OwnerB.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty OwnerB CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		ASSERT_THAT(IsNull(OwnerA->GetBindingInstallation()));
		ASSERT_THAT(IsNull(OwnerB->GetBindingInstallation()));
		ASSERT_THAT(IsNotNull(OwnerA->Engine));
		ASSERT_THAT(IsNotNull(OwnerB->Engine));
		asITypeInfo* StringA = OwnerA->Engine->GetTypeInfoByName("FString");
		asITypeInfo* StringB = OwnerB->Engine->GetTypeInfoByName("FString");
		ASSERT_THAT(IsNotNull(StringA));
		ASSERT_THAT(IsTrue(StringA == StringB));
	}

	TEST_METHOD(CaptureFailureFailsBindScriptTypes)
	{
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::EnsureProcessHostCollection(Diagnostic), *Diagnostic));
		const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Host));

		FAngelscriptBindingDiagnostic BindingDiagnostic;
		TUniquePtr<FAngelscriptEngine> Owner = FAngelscriptEngine::CreateForBindings(*Host, BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			Owner.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty Collection CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		ASSERT_THAT(IsNotNull(Owner->GetBindState()));
		const int32 BeforeCount = Owner->GetBindState()->DirectCallbackExecutionCountForTesting;

		FAngelscriptEngine::SetForcedProcessHostCaptureFailureForTesting(TEXT("forced capture failure"));
		ON_SCOPE_EXIT
		{
			FAngelscriptEngine::SetForcedProcessHostCaptureFailureForTesting(FString());
		};
		TestRunner->AddExpectedError(
			TEXT("Process host capture failed: forced capture failure"),
			EAutomationExpectedErrorFlags::Contains,
			1);

		ASSERT_THAT(IsFalse(Owner->BindScriptTypes()));
		ASSERT_THAT(AreEqual(BeforeCount, Owner->GetBindState()->DirectCallbackExecutionCountForTesting));
	}

	TEST_METHOD(InjectedScriptCompilesHostTypes)
	{
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::EnsureProcessHostCollection(Diagnostic), *Diagnostic));
		const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Host));
		const asCDefinitions* Graph = Host->GetHostDefinitions();
		ASSERT_THAT(IsNotNull(Graph));
		ASSERT_THAT(IsTrue(Graph->GetState() == asEDefinitionState::Frozen));

		FAngelscriptBindingDiagnostic BindingDiagnostic;
		TUniquePtr<FAngelscriptEngine> Owner = FAngelscriptEngine::CreateForBindings(*Host, BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			Owner.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty Collection CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		ASSERT_THAT(IsNotNull(Owner->Engine));
		asITypeInfo* InjectedString = Owner->Engine->GetTypeInfoByName("FString");
		ASSERT_THAT(IsNotNull(InjectedString));
		ASSERT_THAT(IsTrue(InjectedString == Graph->FindType("FString")));
		ASSERT_THAT(IsTrue(Graph->GetIdentityContext().IsValid()));
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor StringDecl;
		ASSERT_THAT(IsTrue(
			Graph->GetIdentityContext()->GetIdentityRegistry().ResolveNominalType(
				static_cast<asCTypeInfo*>(InjectedString)->GetStableKey(),
				StringDecl)
			== AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
			TEXT("injected FString is not interned in the HostProcess identity context")));

		TSharedPtr<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		if (TSharedPtr<const AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> HostContext =
			Graph->GetIdentityContext())
		{
			TypeContext = TSharedPtr<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>(
				HostContext,
				const_cast<AS_NAMESPACE_QUALIFIER asCTypeContext*>(HostContext.Get()));
		}

		const TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources =
			AngelscriptNativeEngineTest::MakeSourceList(
				TEXT("HostTypes.as"),
				TEXT("void UseHostTypes() { FString Name; FVector Position; TArray<int> Values; }"));
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		Options.StableScope = TEXT("host-scheme-injected-compile");
		Options.TypeContext = TypeContext;
		Options.Dependencies.Add(const_cast<asCDefinitions*>(Graph));
		asCBuilder Builder(Sources, Options);
		const bool Built = Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen);
		FString BuilderFailure = TEXT("asCBuilder failed");
		for (uint8 Stage = 0; Stage < uint8(AS_NAMESPACE_QUALIFIER asEBuilderStage::Failed); ++Stage)
		{
			if (const auto* Result = Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage(Stage));
				Result && !Result->Succeeded)
			{
				BuilderFailure = FString::Printf(TEXT("Stage %d: %s"), int32(Stage), *Result->Error);
				if (!Result->Json.IsEmpty())
				{
					BuilderFailure += TEXT(" ") + Result->Json.Left(512);
				}
				break;
			}
		}
		ASSERT_THAT(IsTrue(Built, *BuilderFailure));

		TUniquePtr<asCDefinitions> Compiled = Builder.TakeDefinitions();
		ASSERT_THAT(IsNotNull(Compiled.Get()));
		asCEngineCompileRegistration Registration(*Owner->Engine);
		TArray<TUniquePtr<asCDefinitions>> Units;
		Units.Add(MoveTemp(Compiled));
		const asERegistrationResult Registered = Registration.Register(MoveTemp(Units));
		ASSERT_THAT(IsTrue(
			Registered == asERegistrationResult::Succeeded,
			*FString::Printf(TEXT("register=%d"), int32(Registered))));
		ASSERT_THAT(IsTrue(Owner->Engine->GetTypeInfoByName("FString") == InjectedString));
	}

	TEST_METHOD(TwoEnginesShareHostPointers)
	{
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::EnsureProcessHostCollection(Diagnostic), *Diagnostic));
		const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Host));
		const asCDefinitions* Graph = Host->GetHostDefinitions();
		ASSERT_THAT(IsNotNull(Graph));
		ASSERT_THAT(IsTrue(Graph->GetState() == asEDefinitionState::Frozen));

		FAngelscriptBindingDiagnostic BindingDiagnostic;
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptEngine::CreateForBindings(*Host, BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			EngineA.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty EngineA CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptEngine::CreateForBindings(*Host, BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			EngineB.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty EngineB CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		ASSERT_THAT(IsNotNull(EngineA->Engine));
		ASSERT_THAT(IsNotNull(EngineB->Engine));

		asITypeInfo* StringA = EngineA->Engine->GetTypeInfoByName("FString");
		asITypeInfo* StringB = EngineB->Engine->GetTypeInfoByName("FString");
		ASSERT_THAT(IsNotNull(StringA));
		ASSERT_THAT(IsTrue(StringA == StringB));
		ASSERT_THAT(AreEqual(StringA->GetTypeId(), StringB->GetTypeId()));
		ASSERT_THAT(IsNull(StringA->GetEngine()));
		ASSERT_THAT(IsNull(StringB->GetEngine()));

		asITypeInfo* VectorA = EngineA->Engine->GetTypeInfoByName("FVector");
		asITypeInfo* VectorB = EngineB->Engine->GetTypeInfoByName("FVector");
		ASSERT_THAT(IsNotNull(VectorA));
		ASSERT_THAT(IsTrue(VectorA == VectorB));
		ASSERT_THAT(AreEqual(VectorA->GetTypeId(), VectorB->GetTypeId()));
		ASSERT_THAT(IsNull(VectorA->GetEngine()));
		ASSERT_THAT(IsNull(VectorB->GetEngine()));
	}

	TEST_METHOD(FrozenHostRejectsLaterMember)
	{
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::EnsureProcessHostCollection(Diagnostic), *Diagnostic));
		const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Host));

		FAngelscriptBindingDiagnostic BindingDiagnostic;
		TUniquePtr<FAngelscriptEngine> Owner = FAngelscriptEngine::CreateForBindings(*Host, BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			Owner.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty Collection CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		ASSERT_THAT(IsNotNull(Owner->Engine));
		asITypeInfo* StringType = Owner->Engine->GetTypeInfoByName("FString");
		ASSERT_THAT(IsNotNull(StringType));
		const asUINT MethodCount = StringType->GetMethodCount();

		struct FMustNotAttach
		{
			void MustNotAttach() {}
		};
		FAngelscriptBinds Binds(*Owner);
		FAngelscriptBinds StringBinds = Binds.ExistingClassForTarget("FString");
		const FAngelscriptBoundFunction Bound = StringBinds.Method(
			"void MustNotAttach()",
			&FMustNotAttach::MustNotAttach);
		ASSERT_THAT(IsTrue(Bound.GetFunctionId() < 0));
		ASSERT_THAT(IsTrue(StringBinds.HasRegistrationFailure()));
		ASSERT_THAT(IsFalse(StringBinds.GetRegistrationFailureDiagnostic().IsEmpty()));
		ASSERT_THAT(AreEqual(MethodCount, StringType->GetMethodCount()));
	}

	TEST_METHOD(FailedPhasePublishesNoGraph)
	{
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::EnsureProcessHostCollection(Diagnostic), *Diagnostic));
		const UE::Angelscript::Private::FAngelscriptBindCollection* Healthy =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Healthy));
		const asCDefinitions* HealthyGraph = Healthy->GetHostDefinitions();
		ASSERT_THAT(IsNotNull(HealthyGraph));
		asCTypeInfo* HealthyString = HealthyGraph->FindType("FString");
		ASSERT_THAT(IsNotNull(HealthyString));
		const int32 CaptureCount = FAngelscriptBind::GetProcessHostCaptureCountForTesting();

		UE::Angelscript::Private::FAngelscriptBindCollection Replacement;
		ASSERT_THAT(IsTrue(Replacement.Append(
			HostSchemeMakeRecord(
				TEXT("FailedExplicitBindings"),
				EAngelscriptBindPhase::ExplicitBindings,
				&HostSchemeRegisterFailedInfrastructure,
				40),
			Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Replacement.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsFalse(Replacement.ExecuteToHost(Diagnostic)));
		ASSERT_THAT(IsFalse(Diagnostic.IsEmpty()));
		const asCDefinitions* ReplacementGraph = Replacement.GetHostDefinitions();
		ASSERT_THAT(IsTrue(ReplacementGraph == nullptr || ReplacementGraph->GetState() != asEDefinitionState::Frozen));
		ASSERT_THAT(IsTrue(FAngelscriptBind::GetProcessHostCollection() == Healthy));
		ASSERT_THAT(IsTrue(Healthy->GetHostDefinitions() == HealthyGraph));
		ASSERT_THAT(IsTrue(HealthyGraph->FindType("FString") == HealthyString));
		ASSERT_THAT(AreEqual(CaptureCount, FAngelscriptBind::GetProcessHostCaptureCountForTesting()));
	}
};

#endif
