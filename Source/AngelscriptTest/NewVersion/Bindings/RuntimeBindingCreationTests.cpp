#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptRuntimeModule.h"
#include "Core/AngelscriptSubsystem.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
using namespace RuntimeBindingTestSupport;

TEST_CLASS_WITH_FLAGS(Creation, "Angelscript.UnitTest.RuntimeBindings.Engine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExplicitSnapshotCreatesCallableOwner)
	{
		FAngelscriptBindingDiagnostic Diagnostic; auto Store = RecordFixture();
		auto Owner = FAngelscriptEngine::CreateForBindings(Store, Diagnostic);
		ASSERT_THAT(IsTrue(Owner.IsValid())); ASSERT_THAT(IsTrue(Owner->IsBindingEngine()));
		ASSERT_THAT(IsTrue(Diagnostic.Stage == EAngelscriptBindingStage::None)); ASSERT_THAT(IsTrue(Diagnostic.Message.IsEmpty()));
		ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetByScriptEngine(Owner->Engine) == Owner.Get()));
		ASSERT_THAT(IsNotNull(Owner->GetTypeDatabase())); ASSERT_THAT(IsNotNull(Owner->GetBindState()));
		ASSERT_THAT(IsNotNull(Owner->GetBindingInstallation()->FindType(TEXT("Pair"))));
		auto* Context = Owner->GetBindingContext(); ASSERT_THAT(IsNotNull(Context)); ASSERT_THAT(AreEqual(1, Owner->GetBindingContextCount()));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*Owner, TEXT("int Add(int A, int B)")))));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 2))); ASSERT_THAT(AreEqual(0, Context->SetArgDWord(1, 3)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute())); ASSERT_THAT(AreEqual(asDWORD(5), Context->GetReturnDWord()));
		FPair Value; ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*Owner, TEXT("int Sum() const"))))); ASSERT_THAT(AreEqual(0, Context->SetObject(&Value)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute())); ASSERT_THAT(AreEqual(asDWORD(7), Context->GetReturnDWord()));
	}
	TEST_METHOD(ExplicitCreationKeepsDefaultRuntimeDormant)
	{
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
		FAngelscriptBindingDiagnostic Diagnostic; auto Owner = FAngelscriptEngine::CreateForBindings(RecordFixture(), Diagnostic);
		ASSERT_THAT(IsTrue(Owner.IsValid())); ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
		ASSERT_THAT(IsFalse(FAngelscriptRuntimeModule::IsLegacyRuntimeEnabled()));
		auto* Subsystem = UAngelscriptSubsystem::Get(); ASSERT_THAT(IsNotNull(Subsystem)); ASSERT_THAT(IsNull(Subsystem->GetEngine()));
		ASSERT_THAT(IsFalse(Subsystem->IsAllowedToTick())); ASSERT_THAT(IsNull(Owner->GetCacheService()));
		ASSERT_THAT(IsNull(Owner->JITCoordinator)); ASSERT_THAT(IsFalse(Owner->bHotReloadThreadStarted));
		ASSERT_THAT(IsNull(Owner->GetSourceProvider())); ASSERT_THAT(AreEqual(0, Owner->GetActiveModules().Num()));
		Owner.Reset(); ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine())); ASSERT_THAT(IsNull(Subsystem->GetEngine()));
	}
	TEST_METHOD(MissingNativeTargetReportsSourceAndStageWithoutOwner)
	{
		FAngelscriptBindingDiagnostic Diagnostic; TSharedPtr<FAngelscriptTypeBindInfoStore> Store = RecordFixture(true);
		TWeakPtr<FAngelscriptTypeBindInfoStore> WeakStore = Store;
		auto Owner = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), Diagnostic); Store.Reset();
		ASSERT_THAT(IsFalse(Owner.IsValid())); ASSERT_THAT(IsTrue(Diagnostic.Stage == EAngelscriptBindingStage::Native));
		ASSERT_THAT(IsTrue(Diagnostic.Message.Contains(TEXT("Missing")))); ASSERT_THAT(IsTrue(Diagnostic.Message.Contains(TEXT("FixtureProvider"))));
		ASSERT_THAT(IsTrue(Diagnostic.Message.Contains(TEXT("Fixture/BindingEngine.cpp:42")))); ASSERT_THAT(IsFalse(WeakStore.IsValid()));
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
	}
	TEST_METHOD(UnsealedSnapshotFailsAtRecordingStage)
	{
		FAngelscriptBindingDiagnostic Diagnostic; auto Owner = FAngelscriptEngine::CreateForBindings(RecordFixture(false, false), Diagnostic);
		ASSERT_THAT(IsFalse(Owner.IsValid())); ASSERT_THAT(IsTrue(Diagnostic.Stage == EAngelscriptBindingStage::Recording));
		ASSERT_THAT(IsFalse(Diagnostic.Message.IsEmpty())); ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
	}
	TEST_METHOD(UnresolvedTypeFailsBeforeCallablePublication)
	{
		auto Store = RecordFixture(false, false); FAngelscriptBinds Binds(*Store);
		Binds.BindGlobalFunctionForTarget("int Wrong(MissingType Value)", &Add); FString SealDiagnostic; ASSERT_THAT(IsTrue(Store->Seal(SealDiagnostic)));
		FAngelscriptBindingDiagnostic Diagnostic; auto Owner = FAngelscriptEngine::CreateForBindings(Store, Diagnostic);
		ASSERT_THAT(IsFalse(Owner.IsValid())); ASSERT_THAT(IsTrue(Diagnostic.Stage == EAngelscriptBindingStage::Members));
		ASSERT_THAT(IsTrue(Diagnostic.Message.Contains(TEXT("MissingType")))); ASSERT_THAT(IsTrue(Diagnostic.Message.Contains(TEXT("FixtureProvider"))));
	}
	TEST_METHOD(InvalidNativeLayoutReportsTypeStageAndReleasesSnapshot)
	{
		TSharedPtr<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
		FAngelscriptTypeBindDeclaration D; D.Name = TEXT("Broken"); D.NativeSize = 8; D.NativeAlignment = 3;
		D.ObjectFlags = asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS;
		D.Source = {TEXT("FixtureModule"), TEXT("FixtureProvider"), TEXT("Fixture/BindingEngine.cpp"), 42};
		FAngelscriptTypeBindHandle Handle; FString RecordDiagnostic; ASSERT_THAT(IsTrue(Store->RecordType(D, Handle, RecordDiagnostic)));
		ASSERT_THAT(IsTrue(Store->Seal(RecordDiagnostic))); TWeakPtr<FAngelscriptTypeBindInfoStore> WeakStore = Store;
		FAngelscriptBindingDiagnostic Diagnostic; auto Owner = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), Diagnostic); Store.Reset();
		ASSERT_THAT(IsFalse(Owner.IsValid())); ASSERT_THAT(IsTrue(Diagnostic.Stage == EAngelscriptBindingStage::Types));
		ASSERT_THAT(IsTrue(Diagnostic.Message.Contains(TEXT("Broken")))); ASSERT_THAT(IsTrue(Diagnostic.Message.Contains(TEXT("FixtureProvider"))));
		ASSERT_THAT(IsFalse(WeakStore.IsValid()));
	}
	TEST_METHOD(RepeatedDestructionReleasesActualContextsEngineAndImage)
	{
		FCleanupCounts Counts; FAngelscriptBindingDiagnostic Diagnostic;
		for (int32 Iteration = 0; Iteration < 5; ++Iteration)
		{
			TSharedPtr<FAngelscriptTypeBindInfoStore> Store = RecordFixture(); TWeakPtr<FAngelscriptTypeBindInfoStore> WeakStore = Store;
			auto Owner = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), Diagnostic); Store.Reset(); ASSERT_THAT(IsTrue(Owner.IsValid()));
			ASSERT_THAT(IsTrue(WeakStore.IsValid())); ObserveCleanup(*Owner, Counts);
			std::weak_ptr<AS_NAMESPACE_QUALIFIER asCMetadataImage> WeakImage = Owner->GetBindingInstallation()->GetImage();
			auto* First = Owner->GetBindingContext(); auto* Second = Owner->CreateBindingContext();
			ASSERT_THAT(IsNotNull(Second)); ASSERT_THAT(IsTrue(First != Second)); ASSERT_THAT(AreEqual(2, Owner->GetBindingContextCount()));
			for (auto* Context : {First, Second})
			{
				Context->SetUserData(&Counts, CleanupKey); ASSERT_THAT(AreEqual(0, Context->Prepare(Function(*Owner, TEXT("int Add(int A, int B)")))));
			}
			Owner.Reset(); ASSERT_THAT(AreEqual((Iteration + 1) * 2, Counts.Contexts)); ASSERT_THAT(AreEqual(Iteration + 1, Counts.Engines));
			ASSERT_THAT(IsFalse(WeakStore.IsValid())); ASSERT_THAT(IsTrue(WeakImage.expired()));
		}
	}
	TEST_METHOD(ExplicitShutdownIsIdempotentAndStopsContextCreation)
	{
		FCleanupCounts Counts; FAngelscriptBindingDiagnostic Diagnostic; auto Owner = FAngelscriptEngine::CreateForBindings(RecordFixture(), Diagnostic);
		ASSERT_THAT(IsTrue(Owner.IsValid())); ObserveCleanup(*Owner, Counts); Owner->GetBindingContext()->SetUserData(&Counts, CleanupKey);
		Owner->Shutdown(); ASSERT_THAT(AreEqual(1, Counts.Contexts)); ASSERT_THAT(AreEqual(1, Counts.Engines));
		ASSERT_THAT(IsNull(Owner->Engine)); ASSERT_THAT(IsNull(Owner->GetTypeDatabase())); ASSERT_THAT(IsNull(Owner->GetBindingInstallation()));
		ASSERT_THAT(AreEqual(0, Owner->GetBindingContextCount())); ASSERT_THAT(IsNull(Owner->CreateBindingContext()));
		Owner->Shutdown(); Owner.Reset(); ASSERT_THAT(AreEqual(1, Counts.Contexts)); ASSERT_THAT(AreEqual(1, Counts.Engines));
	}
	TEST_METHOD(FullRuntimeOverloadPublishesCompleteExplicitOwner)
	{
		FAngelscriptBindingDiagnostic Diagnostic; auto Owner = FAngelscriptEngine::CreateForBindings(Diagnostic);
		ASSERT_THAT(IsTrue(Owner.IsValid(), *Diagnostic.Message)); ASSERT_THAT(IsTrue(Diagnostic.Stage == EAngelscriptBindingStage::None));
		ASSERT_THAT(IsTrue(Diagnostic.Message.IsEmpty())); ASSERT_THAT(IsTrue(Owner->IsBindingEngine()));
		ASSERT_THAT(IsNotNull(Owner->GetBindingInstallation())); ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
	}
};
#endif
