#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptType.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoRecorder.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingIsolationFixture
{
using namespace RuntimeBindingTestSupport;
struct FAdapter : FAngelscriptType
{
	int32 Bias;
	explicit FAdapter(int32 InBias) : Bias(InBias) {}
	FString GetAngelscriptTypeName() const override { return TEXT("Pair"); }
	bool CanQueryPropertyType() const override { return false; }
	void CopyValue(const FAngelscriptTypeUsage&, void* Source, void* Destination) const override
	{
		*static_cast<FPair*>(Destination) = *static_cast<FPair*>(Source);
		static_cast<FPair*>(Destination)->X += Bias;
	}
};
static TArray<FAngelscriptEngine*> ObservedOwners;
static int32 ObserveOwner()
{
	auto* Owner = FAngelscriptEngine::TryGetCurrentEngine(); ObservedOwners.Add(Owner);
	if (!Owner) return -1;
	auto Adapter = FAngelscriptType::GetByAngelscriptTypeName(TEXT("Pair"));
	return Adapter.IsValid() ? static_cast<FAdapter*>(Adapter.Get())->Bias : -2;
}
static void GenericOwner(asIScriptGeneric* Call) { Call->SetReturnDWord(ObserveOwner()); }
struct FAuxiliary
{
	int32 Value = 13;
	asIScriptContext* NestedContext = nullptr;
	asIScriptFunction* NestedFunction = nullptr;
	asCScriptEngine* RebindEngine = nullptr;
	asSStableKey RebindKey;
	FAuxiliary* Replacement = nullptr;
	void* AfterNested = nullptr;
	FAngelscriptEngine* OwnerBefore = nullptr;
	FAngelscriptEngine* OwnerAfter = nullptr;
	bool Rebound = false;
	int32 NestedResult = -1;
};
static int32 ObserveAuxiliary()
{
	auto* State = static_cast<FAuxiliary*>(FAngelscriptEngine::GetCurrentFunctionUserDataPtr());
	if (!State) return -1;
	State->OwnerBefore = FAngelscriptEngine::TryGetCurrentEngine();
	if (State->NestedContext && State->NestedContext->Prepare(State->NestedFunction) == 0)
	{
		if (State->NestedContext->Execute() == asEXECUTION_FINISHED) State->NestedResult = State->NestedContext->GetReturnDWord();
	}
	if (State->RebindEngine)
	{
		auto Current = State->RebindEngine->AcquireSystemInterface(static_cast<asCScriptFunction*>(asGetActiveFunction()));
		State->Rebound = Current && State->RebindEngine->BindNativeFunction(State->RebindKey,
			asFUNCTION(ObserveAuxiliary), asCALL_CDECL, Current->caller, State->Replacement) == asEByteCodeLinkStatus::Succeeded;
	}
	State->AfterNested = FAngelscriptEngine::GetCurrentFunctionUserDataPtr();
	State->OwnerAfter = FAngelscriptEngine::TryGetCurrentEngine();
	return State->Value;
}
static void GenericAuxiliary(asIScriptGeneric* Call) { Call->SetReturnDWord(ObserveAuxiliary()); }
static int32 ProviderCount = 0;
static void Provider(FAngelscriptBinds& Binds)
{
	++ProviderCount; FBindFlags Flags; Flags.bPOD = true;
	Binds.ValueClassForTarget<FPair>("Pair", Flags).Method("int Sum() const", &FPair::Sum);
	Binds.BindGlobalFunctionForTarget("int Add(int A, int B)", &Add);
}
static void AddAdapter(FAngelscriptEngine& Owner, int32 Bias)
{
	FAngelscriptType::Register(*Owner.GetTypeDatabase(), MakeShared<FAdapter>(Bias));
}
static TSharedRef<FAngelscriptTypeBindInfoStore> CallStore(FAuxiliary* State = nullptr)
{
	auto Store = RecordFixture(false, false); FAngelscriptBinds Binds(*Store);
	Binds.BindGlobalFunctionForTarget("int Owner()", &ObserveOwner);
	Binds.BindGlobalGenericFunctionForTarget("int GenericOwner()", &GenericOwner);
	Binds.BindGlobalFunctionForTarget("int Auxiliary()", &ObserveAuxiliary, State);
	Binds.BindGlobalGenericFunctionForTarget("int GenericAuxiliary()", &GenericAuxiliary, State);
	FString Diagnostic; check(Store->Seal(Diagnostic)); return Store;
}
}

using namespace RuntimeBindingIsolationFixture;
TEST_CLASS_WITH_FLAGS(Isolation, "Angelscript.UnitTest.RuntimeBindings.Engine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OneCapturedProviderBuildsDistinctOwnerMetadata)
	{
		ProviderCount = 0; UE::Angelscript::Private::FAngelscriptBindCollection Collection; FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("FixtureModule"); Record.BindName = TEXT("FixtureProvider"); Record.Phase = EAngelscriptBindPhase::ExplicitBindings;
		Record.SourceFile = "Fixture/Isolation.cpp"; Record.SourceLine = 12; Record.Callback = &Provider;
		FString Error; ASSERT_THAT(IsTrue(Collection.Append(Record, Error))); ASSERT_THAT(IsTrue(Collection.Finalize(Error)));
		TSharedPtr<const FAngelscriptTypeBindInfoStore> Store; ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoRecorder::Capture(Collection, {}, Store, Error)));
		FAngelscriptBindingDiagnostic Diagnostic; auto A = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), Diagnostic);
		auto B = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), Diagnostic); ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid()));
		ASSERT_THAT(AreEqual(1, ProviderCount)); ASSERT_THAT(IsTrue(A->Engine != B->Engine));
		ASSERT_THAT(IsTrue(A->GetBindingInstallation()->GetImage() != B->GetBindingInstallation()->GetImage()));
		ASSERT_THAT(IsTrue(A->GetBindingInstallation()->FindType(TEXT("Pair")) != B->GetBindingInstallation()->FindType(TEXT("Pair"))));
		ASSERT_THAT(IsTrue(Function(*A, TEXT("int Add(int A, int B)")) != Function(*B, TEXT("int Add(int A, int B)"))));
		ASSERT_THAT(IsTrue(A->GetBindingContext()->GetEngine() == A->Engine)); ASSERT_THAT(IsTrue(B->GetBindingContext()->GetEngine() == B->Engine));
	}
	TEST_METHOD(AdaptersAndMutableDatabaseBelongToExactOwner)
	{
		auto Store = RecordFixture(); FAngelscriptBindingDiagnostic Diagnostic;
		auto A = FAngelscriptEngine::CreateForBindings(Store, Diagnostic); auto B = FAngelscriptEngine::CreateForBindings(Store, Diagnostic);
		ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid())); AddAdapter(*A, 10); AddAdapter(*B, 20);
		auto AdapterA = FAngelscriptType::GetByAngelscriptTypeName(*A->GetTypeDatabase(), TEXT("Pair"));
		auto AdapterB = FAngelscriptType::GetByAngelscriptTypeName(*B->GetTypeDatabase(), TEXT("Pair"));
		ASSERT_THAT(IsTrue(AdapterA.Get() != AdapterB.Get())); FPair Input, OutA, OutB;
		AdapterA->CopyValue(FAngelscriptTypeUsage::DefaultUsage, &Input, &OutA); AdapterB->CopyValue(FAngelscriptTypeUsage::DefaultUsage, &Input, &OutB);
		ASSERT_THAT(AreEqual(13, OutA.X)); ASSERT_THAT(AreEqual(23, OutB.X)); ASSERT_THAT(AreEqual(3, Input.X));
		FAngelscriptType::RegisterAlias(*A->GetTypeDatabase(), TEXT("OnlyA"), AdapterA.ToSharedRef());
		ASSERT_THAT(IsFalse(FAngelscriptType::GetByAngelscriptTypeName(*B->GetTypeDatabase(), TEXT("OnlyA")).IsValid()));
	}
	TEST_METHOD(DestroyingOneOwnerLeavesOtherCallableAndReleasesBothImages)
	{
		FCleanupCounts Counts; TSharedPtr<FAngelscriptTypeBindInfoStore> Store = RecordFixture(); TWeakPtr<FAngelscriptTypeBindInfoStore> WeakStore = Store;
		FAngelscriptBindingDiagnostic Diagnostic; auto A = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), Diagnostic);
		auto B = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), Diagnostic); Store.Reset(); ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid()));
		ObserveCleanup(*A, Counts); ObserveCleanup(*B, Counts); A->GetBindingContext()->SetUserData(&Counts, CleanupKey); B->GetBindingContext()->SetUserData(&Counts, CleanupKey);
		A.Reset(); ASSERT_THAT(AreEqual(1, Counts.Engines));
		auto* Context = B->GetBindingContext(); ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*B, TEXT("int Add(int A, int B)")))));
		Context->SetArgDWord(0, 2); Context->SetArgDWord(1, 3); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute()));
		ASSERT_THAT(AreEqual(asDWORD(5), Context->GetReturnDWord())); ASSERT_THAT(IsNotNull(B->GetBindingInstallation()->FindType(TEXT("Pair"))));
		B.Reset(); ASSERT_THAT(IsFalse(WeakStore.IsValid())); ASSERT_THAT(AreEqual(2, Counts.Contexts)); ASSERT_THAT(AreEqual(2, Counts.Engines));
	}
	TEST_METHOD(ForeignFunctionsAreRejectedByBothContexts)
	{
		auto Store = RecordFixture(); FAngelscriptBindingDiagnostic Diagnostic; auto A = FAngelscriptEngine::CreateForBindings(Store, Diagnostic);
		auto B = FAngelscriptEngine::CreateForBindings(Store, Diagnostic); ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid()));
		ASSERT_THAT(AreEqual(int(asINVALID_ARG), A->GetBindingContext()->Prepare(Function(*B, TEXT("int Add(int A, int B)")))));
		ASSERT_THAT(AreEqual(int(asINVALID_ARG), B->GetBindingContext()->Prepare(Function(*A, TEXT("int Add(int A, int B)")))));
		ASSERT_THAT(IsTrue(A->GetBindingContext()->GetEngine() == A->Engine)); ASSERT_THAT(IsTrue(B->GetBindingContext()->GetEngine() == B->Engine));
	}
	TEST_METHOD(NativeCallbacksResolveTheExecutingOwnerAndAdapter)
	{
		ObservedOwners.Reset(); auto Store = CallStore(); FAngelscriptBindingDiagnostic Diagnostic;
		auto A = FAngelscriptEngine::CreateForBindings(Store, Diagnostic); auto B = FAngelscriptEngine::CreateForBindings(Store, Diagnostic);
		ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid())); AddAdapter(*A, 10); AddAdapter(*B, 20);
		for (auto* Owner : {A.Get(), B.Get()})
		{
			auto* Context = Owner->GetBindingContext(); ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*Owner, TEXT("int Owner()")))));
			ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute())); ASSERT_THAT(IsTrue(ObservedOwners.Last() == Owner));
			ASSERT_THAT(AreEqual(asDWORD(Owner == A.Get() ? 10 : 20), Context->GetReturnDWord()));
		}
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
	}
	TEST_METHOD(GenericExecutionOverridesAmbientScopeAndRestoresIt)
	{
		ObservedOwners.Reset(); auto Store = CallStore(); FAngelscriptBindingDiagnostic Diagnostic;
		auto A = FAngelscriptEngine::CreateForBindings(Store, Diagnostic); auto B = FAngelscriptEngine::CreateForBindings(Store, Diagnostic);
		ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid())); AddAdapter(*A, 10); AddAdapter(*B, 20);
		{
			FAngelscriptEngineScope Scope(*A); ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == A.Get()));
			auto* Context = B->GetBindingContext(); ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*B, TEXT("int GenericOwner()")))));
			ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute())); ASSERT_THAT(IsTrue(ObservedOwners.Last() == B.Get()));
			ASSERT_THAT(AreEqual(asDWORD(20), Context->GetReturnDWord())); ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == A.Get()));
		}
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
	}
	TEST_METHOD(NativeAuxiliaryAndOwnerRestoreAfterNestedOtherEngineCall)
	{
		FAuxiliary State; auto Store = CallStore(&State); FAngelscriptBindingDiagnostic Diagnostic;
		auto A = FAngelscriptEngine::CreateForBindings(Store, Diagnostic); auto B = FAngelscriptEngine::CreateForBindings(Store, Diagnostic);
		ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid())); AddAdapter(*B, 20); ObservedOwners.Reset();
		State.NestedContext = B->GetBindingContext(); State.NestedFunction = Function(*B, TEXT("int Owner()"));
		auto* Context = A->GetBindingContext(); ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*A, TEXT("int Auxiliary()")))));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute())); ASSERT_THAT(AreEqual(asDWORD(13), Context->GetReturnDWord()));
		ASSERT_THAT(AreEqual(20, State.NestedResult)); ASSERT_THAT(IsTrue(State.OwnerBefore == A.Get() && State.OwnerAfter == A.Get()));
		ASSERT_THAT(IsTrue(State.AfterNested == &State)); ASSERT_THAT(IsTrue(ObservedOwners.Last() == B.Get()));
		ASSERT_THAT(IsNull(FAngelscriptEngine::GetCurrentFunctionUserDataPtr())); ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
	}
	TEST_METHOD(GenericRuntimeHelperReadsTheCapturedAuxiliary)
	{
		FAuxiliary State; FAngelscriptBindingDiagnostic Diagnostic; auto Owner = FAngelscriptEngine::CreateForBindings(CallStore(&State), Diagnostic);
		ASSERT_THAT(IsTrue(Owner.IsValid())); auto* Context = Owner->GetBindingContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*Owner, TEXT("int GenericAuxiliary()")))));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute())); ASSERT_THAT(AreEqual(asDWORD(13), Context->GetReturnDWord()));
		ASSERT_THAT(IsTrue(State.AfterNested == &State)); ASSERT_THAT(IsTrue(State.OwnerBefore == Owner.Get()));
	}
	TEST_METHOD(RebindingOneOwnerKeepsActiveAuxiliaryGenerationAndOtherOwner)
	{
		FAuxiliary State, Next; Next.Value = 29; auto Store = CallStore(&State); FAngelscriptBindingDiagnostic Diagnostic;
		auto A = FAngelscriptEngine::CreateForBindings(Store, Diagnostic); auto B = FAngelscriptEngine::CreateForBindings(Store, Diagnostic);
		ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid())); auto* FunctionA = Function(*A, TEXT("int Auxiliary()"));
		State.RebindEngine = A->Engine; State.RebindKey = FunctionA->GetStableKey(); State.Replacement = &Next;
		auto* Context = A->GetBindingContext(); ASSERT_THAT(AreEqual(0, Context->Prepare(FunctionA)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute())); ASSERT_THAT(AreEqual(asDWORD(13), Context->GetReturnDWord()));
		ASSERT_THAT(IsTrue(State.Rebound)); ASSERT_THAT(IsTrue(State.AfterNested == &State)); State.RebindEngine = nullptr;
		Context->Unprepare(); ASSERT_THAT(AreEqual(0, Context->Prepare(FunctionA))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute()));
		ASSERT_THAT(AreEqual(asDWORD(29), Context->GetReturnDWord())); ASSERT_THAT(IsTrue(Next.AfterNested == &Next));
		Context = B->GetBindingContext(); ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*B, TEXT("int Auxiliary()")))));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute())); ASSERT_THAT(AreEqual(asDWORD(13), Context->GetReturnDWord()));
		ASSERT_THAT(IsTrue(State.OwnerBefore == B.Get()));
	}
};

TEST_CLASS_WITH_FLAGS(RuntimeBindingIsolation, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PrepareOnceTwoEngines)
	{
		ProviderCount = 0;
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("FixtureModule");
		Record.BindName = TEXT("FixtureProvider");
		Record.Phase = EAngelscriptBindPhase::ExplicitBindings;
		Record.SourceFile = "Fixture/Isolation.cpp";
		Record.SourceLine = 12;
		Record.Callback = &Provider;
		FString Error;
		ASSERT_THAT(IsTrue(Collection.Append(Record, Error)));
		ASSERT_THAT(IsTrue(Collection.Finalize(Error)));
		TSharedPtr<const FAngelscriptTypeBindInfoStore> Store;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoRecorder::Capture(Collection, {}, Store, Error)));
		FAngelscriptBindingDiagnostic Diagnostic;
		auto A = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), Diagnostic);
		auto B = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), Diagnostic);
		ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid(), *Diagnostic.Message));
		ASSERT_THAT(AreEqual(1, ProviderCount));
		ASSERT_THAT(IsTrue(A->Engine != B->Engine));
		const FAngelscriptTypeBindHandle PairHandle = Store->FindType(TEXT("Pair"));
		const int PublishedTypeId = Store->GetPublishedTypeId(PairHandle);
		ASSERT_THAT(IsTrue(PublishedTypeId > asTYPEID_LAST_PRIMITIVE));
		asCTypeInfo* TypeA = A->GetBindingInstallation()->FindType(TEXT("Pair"));
		asCTypeInfo* TypeB = B->GetBindingInstallation()->FindType(TEXT("Pair"));
		ASSERT_THAT(IsNotNull(TypeA));
		ASSERT_THAT(IsNotNull(TypeB));
		ASSERT_THAT(IsTrue(TypeA != TypeB));
		ASSERT_THAT(AreEqual(PublishedTypeId, TypeA->GetTypeId()));
		ASSERT_THAT(AreEqual(PublishedTypeId, TypeB->GetTypeId()));
		ASSERT_THAT(IsTrue(TypeA->GetEngine() == A->Engine));
		ASSERT_THAT(IsTrue(TypeB->GetEngine() == B->Engine));
		ASSERT_THAT(IsTrue(A->Engine->GetTypeInfoById(PublishedTypeId) == TypeA));
		ASSERT_THAT(IsTrue(B->Engine->GetTypeInfoById(PublishedTypeId) == TypeB));
		ASSERT_THAT(IsNull(A->GetBindingInstallation()->GetImage()->GetBoundEngine()));
		ASSERT_THAT(IsNull(B->GetBindingInstallation()->GetImage()->GetBoundEngine()));
		int PublishedFunctionId = -1;
		if (const FAngelscriptTypeBindInfo* PairInfo = Store->GetType(PairHandle))
		{
			for (int32 Index = 0; Index < PairInfo->Members.Num(); ++Index)
			{
				if (PairInfo->Members[Index].Declaration.Equals(TEXT("int Sum() const"), ESearchCase::CaseSensitive))
				{
					PublishedFunctionId = Store->GetPublishedFunctionId({PairHandle, Index});
					break;
				}
			}
		}
		ASSERT_THAT(IsTrue(PublishedFunctionId > 0));
		asIScriptFunction* SumA = A->Engine->GetFunctionById(PublishedFunctionId);
		asIScriptFunction* SumB = B->Engine->GetFunctionById(PublishedFunctionId);
		ASSERT_THAT(IsNotNull(SumA));
		ASSERT_THAT(IsNotNull(SumB));
		ASSERT_THAT(IsTrue(SumA != SumB));
		ASSERT_THAT(AreEqual(PublishedFunctionId, SumA->GetId()));
		ASSERT_THAT(AreEqual(PublishedFunctionId, SumB->GetId()));
		auto* Context = A->GetBindingContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*A, TEXT("int Add(int A, int B)")))));
		Context->SetArgDWord(0, 2);
		Context->SetArgDWord(1, 3);
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute()));
		ASSERT_THAT(AreEqual(asDWORD(5), Context->GetReturnDWord()));
		FPair Value;
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*A, TEXT("int Sum() const")))));
		ASSERT_THAT(AreEqual(0, Context->SetObject(&Value)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute()));
		ASSERT_THAT(AreEqual(asDWORD(7), Context->GetReturnDWord()));
	}

	TEST_METHOD(DestroyOne)
	{
		TSharedPtr<FAngelscriptTypeBindInfoStore> Store = RecordFixture();
		TWeakPtr<FAngelscriptTypeBindInfoStore> WeakStore = Store;
		FAngelscriptBindingDiagnostic Diagnostic;
		auto A = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), Diagnostic);
		auto B = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), Diagnostic);
		ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid(), *Diagnostic.Message));
		const int PublishedTypeId = Store->GetPublishedTypeId(Store->FindType(TEXT("Pair")));
		asCTypeInfo* TypeB = B->GetBindingInstallation()->FindType(TEXT("Pair"));
		ASSERT_THAT(IsNotNull(TypeB));
		ASSERT_THAT(AreEqual(PublishedTypeId, TypeB->GetTypeId()));
		Store.Reset();
		A.Reset();
		ASSERT_THAT(IsTrue(B->Engine->GetTypeInfoById(PublishedTypeId) == TypeB));
		auto* Context = B->GetBindingContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*B, TEXT("int Add(int A, int B)")))));
		Context->SetArgDWord(0, 2);
		Context->SetArgDWord(1, 3);
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute()));
		ASSERT_THAT(AreEqual(asDWORD(5), Context->GetReturnDWord()));
		FPair Value;
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*B, TEXT("int Sum() const")))));
		ASSERT_THAT(AreEqual(0, Context->SetObject(&Value)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute()));
		ASSERT_THAT(AreEqual(asDWORD(7), Context->GetReturnDWord()));
		B.Reset();
		ASSERT_THAT(IsFalse(WeakStore.IsValid()));
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
	}
};
#endif
