#include "CQTest.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoRecorder.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(Providers, "Angelscript.UnitTest.RuntimeBindings.Recording",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FCollection = UE::Angelscript::Private::FAngelscriptBindCollection;
	struct FPair { int32 X; int32 Y; };
	inline static TArray<FString> Order;
	inline static int32 Invocations = 0;
	static int32 Zero() { return 0; }
	static void Declare(FAngelscriptBinds& Binds)
	{
		++Invocations;
		Order.Add(TEXT("Declare"));
		Binds.ValueClassForTarget<FPair>("Pair", {});
	}
	static void Extend(FAngelscriptBinds& Binds)
	{
		++Invocations;
		Order.Add(TEXT("Extend"));
		auto Pair = Binds.ValueClassForTarget<FPair>("Pair", {});
		Pair.Property("int X", &FPair::X);
	}
	static void Global(FAngelscriptBinds& Binds)
	{
		++Invocations;
		Order.Add(TEXT("Global"));
		Binds.BindGlobalFunctionForTarget("int Global()", &Zero);
	}
	static void Editor(FAngelscriptBinds& Binds)
	{
		++Invocations;
		Binds.BindGlobalFunctionForTarget("int Editor()", &Zero);
	}
	static void Runtime(FAngelscriptBinds& Binds)
	{
		++Invocations;
		Binds.BindGlobalFunctionForTarget("int Runtime()", &Zero);
	}
	static void Conflict(FAngelscriptBinds& Binds)
	{
		++Invocations;
		Binds.ValueClassForTarget<int64>("Pair", {});
	}
	static bool InEditor(const FAngelscriptBindRecordingPolicy& Policy) { return Policy.bEditor; }
	static bool InRuntime(const FAngelscriptBindRecordingPolicy& Policy) { return !Policy.bEditor; }
	static FAngelscriptBindRecord Provider(const TCHAR* Name, EAngelscriptBindPhase Phase,
		FAngelscriptBindCallback Callback, const ANSICHAR* File = "Fixture/Providers.cpp", int32 Line = 17)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("FixtureModule");
		Record.BindName = FName(Name);
		Record.Phase = Phase;
		Record.SourceFile = File;
		Record.SourceLine = Line;
		Record.Callback = Callback;
		return Record;
	}
	void ResetCounters() { Invocations = 0; Order.Reset(); }

public:
	TEST_METHOD(OutOfOrderProvidersPreservePhaseOrderAndContributions)
	{
		ResetCounters();
		FCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Append(Provider(TEXT("Z.Global"), EAngelscriptBindPhase::ExplicitBindings, &Global), Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Append(Provider(TEXT("Z.Declare"), EAngelscriptBindPhase::TypeDeclarations, &Declare), Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Append(Provider(TEXT("A.Extend"), EAngelscriptBindPhase::ExplicitBindings, &Extend), Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic)));
		TSharedPtr<const FAngelscriptTypeBindInfoStore> Store;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoRecorder::Capture(Collection, {}, Store, Diagnostic)));
		ASSERT_THAT(IsTrue(Store.IsValid()));
		ASSERT_THAT(IsTrue(Store->IsSealed()));
		ASSERT_THAT(AreEqual(3, Invocations));
		ASSERT_THAT(AreEqual(3, Order.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("Declare")), Order[0]));
		ASSERT_THAT(AreEqual(FString(TEXT("Extend")), Order[1]));
		ASSERT_THAT(AreEqual(FString(TEXT("Global")), Order[2]));
		const auto* Pair = Store->GetType(Store->FindType(TEXT("Pair")));
		ASSERT_THAT(IsNotNull(Pair));
		ASSERT_THAT(AreEqual(2, Pair->Sources.Num()));
		ASSERT_THAT(AreEqual(1, Pair->Members.Num()));
		ASSERT_THAT(IsTrue(Pair->Declaration.Source.Phase == EAngelscriptBindPhase::TypeDeclarations));
		ASSERT_THAT(AreEqual(FName(TEXT("A.Extend")), Pair->Members[0].Source.Provider));
		ASSERT_THAT(IsTrue(Pair->Members[0].Source.Phase == EAngelscriptBindPhase::ExplicitBindings));
		ASSERT_THAT(AreEqual(FString(TEXT("Fixture/Providers.cpp")), Pair->Members[0].Source.File));
		ASSERT_THAT(AreEqual(17, Pair->Members[0].Source.Line));
	}

	TEST_METHOD(ReusingSnapshotNeverReexecutesProviders)
	{
		ResetCounters();
		TSharedPtr<const FAngelscriptTypeBindInfoStore> Store;
		FString Diagnostic;
		{
			FCollection Collection;
			ASSERT_THAT(IsTrue(Collection.Append(Provider(TEXT("Global"), EAngelscriptBindPhase::ExplicitBindings, &Global), Diagnostic)));
			ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic)));
			ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoRecorder::Capture(Collection, {}, Store, Diagnostic)));
		}
		for (int32 Index = 0; Index != 8; ++Index)
		{
			auto Consumer = Store;
			const auto* Scope = Consumer->GetType(Consumer->FindScope());
			ASSERT_THAT(IsNotNull(Scope));
			ASSERT_THAT(AreEqual(1, Scope->Members.Num()));
			ASSERT_THAT(IsTrue(Consumer.Get() == Store.Get()));
		}
		ASSERT_THAT(AreEqual(1, Invocations));
		ASSERT_THAT(AreEqual(FName(TEXT("Global")), Store->GetProviders()[0].Source.Provider));
	}

	TEST_METHOD(RecordingFailureReportsSourceAndPublishesNothing)
	{
		ResetCounters();
		FCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Append(Provider(TEXT("Declare"), EAngelscriptBindPhase::TypeDeclarations, &Declare), Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Append(Provider(TEXT("Conflict"), EAngelscriptBindPhase::ExplicitBindings, &Conflict, "Fixture/Conflict.cpp", 42), Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Append(Provider(TEXT("Later"), EAngelscriptBindPhase::Finalization, &Global), Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic)));
		TSharedPtr<const FAngelscriptTypeBindInfoStore> Store;
		ASSERT_THAT(IsFalse(FAngelscriptTypeBindInfoRecorder::Capture(Collection, {}, Store, Diagnostic)));
		ASSERT_THAT(IsFalse(Store.IsValid()));
		ASSERT_THAT(AreEqual(2, Invocations));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Conflict.cpp:42"))));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Conflict"))));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Declare"))));
	}

	TEST_METHOD(TargetConditionsAndEffectivePolicyAreCaptured)
	{
		ResetCounters();
		FCollection Collection;
		FString Diagnostic;
		auto EditorProvider = Provider(TEXT("Editor"), EAngelscriptBindPhase::ExplicitBindings, &Editor);
		EditorProvider.Condition = &InEditor;
		auto RuntimeProvider = Provider(TEXT("Runtime"), EAngelscriptBindPhase::ExplicitBindings, &Runtime);
		RuntimeProvider.Condition = &InRuntime;
		ASSERT_THAT(IsTrue(Collection.Append(EditorProvider, Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Append(RuntimeProvider, Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Append(Provider(TEXT("Global"), EAngelscriptBindPhase::ExplicitBindings, &Global), Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic)));
		FAngelscriptBindRecordingPolicy Policy;
		Policy.bEditor = false;
		Policy.bTest = true;
		Policy.bLogging = false;
		Policy.Platform = TEXT("FixtureTarget");
		TSharedPtr<const FAngelscriptTypeBindInfoStore> Store;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoRecorder::Capture(Collection, Policy, Store, Diagnostic)));
		Policy.bEditor = true;
		ASSERT_THAT(AreEqual(2, Invocations));
		ASSERT_THAT(IsTrue(!Store->GetPolicy().bEditor && Store->GetPolicy().bTest && !Store->GetPolicy().bLogging));
		ASSERT_THAT(AreEqual(FName(TEXT("FixtureTarget")), Store->GetPolicy().Platform));
		ASSERT_THAT(AreEqual(3, Store->GetProviders().Num()));
		ASSERT_THAT(IsFalse(Store->GetProviders()[0].bEligible));
		ASSERT_THAT(AreEqual(FName(TEXT("Editor")), Store->GetProviders()[0].Source.Provider));
		ASSERT_THAT(IsTrue(Store->GetProviders()[1].bEligible && Store->GetProviders()[2].bEligible));
		const auto* Scope = Store->GetType(Store->FindScope());
		ASSERT_THAT(IsNotNull(Scope));
		ASSERT_THAT(AreEqual(2, Scope->Members.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("int Global()")), Scope->Members[0].Declaration));
		ASSERT_THAT(AreEqual(FString(TEXT("int Runtime()")), Scope->Members[1].Declaration));
	}

	TEST_METHOD(UnsealedCollectionCannotPublishASnapshot)
	{
		ResetCounters();
		FCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Append(Provider(TEXT("Global"), EAngelscriptBindPhase::ExplicitBindings, &Global), Diagnostic)));
		TSharedPtr<const FAngelscriptTypeBindInfoStore> Store;
		ASSERT_THAT(IsFalse(FAngelscriptTypeBindInfoRecorder::Capture(Collection, {}, Store, Diagnostic)));
		ASSERT_THAT(IsFalse(Store.IsValid()));
		ASSERT_THAT(AreEqual(0, Invocations));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("sealed"))));
	}

	TEST_METHOD(DuplicateIdentityNamesBothSourcesBeforeExecution)
	{
		ResetCounters();
		FCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Append(Provider(TEXT("Duplicate"), EAngelscriptBindPhase::ExplicitBindings, &Global, "Fixture/First.cpp", 11), Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Append(Provider(TEXT("Duplicate"), EAngelscriptBindPhase::ExplicitBindings, &Global, "Fixture/Second.cpp", 22), Diagnostic)));
		ASSERT_THAT(IsFalse(Collection.Finalize(Diagnostic)));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("First.cpp:11")) && Diagnostic.Contains(TEXT("Second.cpp:22"))));
		ASSERT_THAT(IsFalse(Collection.IsSealed()));
		TSharedPtr<const FAngelscriptTypeBindInfoStore> Store;
		ASSERT_THAT(IsFalse(FAngelscriptTypeBindInfoRecorder::Capture(Collection, {}, Store, Diagnostic)));
		ASSERT_THAT(IsFalse(Store.IsValid()));
		ASSERT_THAT(AreEqual(0, Invocations));
	}

	TEST_METHOD(LateProviderInvalidatesCaptureWithProvenance)
	{
		ResetCounters();
		FCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic)));
		ASSERT_THAT(IsFalse(Collection.Append(Provider(TEXT("Late"), EAngelscriptBindPhase::ExplicitBindings, &Global, "Fixture/Late.cpp", 81), Diagnostic)));
		Diagnostic.Reset();
		TSharedPtr<const FAngelscriptTypeBindInfoStore> Store;
		ASSERT_THAT(IsFalse(FAngelscriptTypeBindInfoRecorder::Capture(Collection, {}, Store, Diagnostic)));
		ASSERT_THAT(IsFalse(Store.IsValid()));
		ASSERT_THAT(AreEqual(0, Invocations));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Late.cpp:81"))));
	}

	TEST_METHOD(EmptySealedCollectionProducesAnEmptySealedStore)
	{
		ResetCounters();
		FCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic)));
		TSharedPtr<const FAngelscriptTypeBindInfoStore> Store;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoRecorder::Capture(Collection, {}, Store, Diagnostic)));
		ASSERT_THAT(IsTrue(Store.IsValid() && Store->IsSealed()));
		ASSERT_THAT(AreEqual(0, Store->GetTypes().Num()));
		ASSERT_THAT(AreEqual(0, Store->GetProviders().Num()));
		ASSERT_THAT(AreEqual(0, Invocations));
	}
};

#endif
