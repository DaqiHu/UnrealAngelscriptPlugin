#include "CQTest.h"
#include "Core/AngelscriptBindsInternal.h"
#include "as_callfunc.h"
#include "as_definitions.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FHostCollectionPair
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Sum() const { return X + Y; }
	};

	int32 GHostCollectionCounter = 7;
	int32 GHostCollectionPairCallbacks = 0;
	int32 GHostCollectionSumCallbacks = 0;

	void HostCollectionRegisterPair(FAngelscriptBinds& Binds)
	{
		++GHostCollectionPairCallbacks;
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FHostCollectionPair>("Pair", Flags);
	}

	void HostCollectionRegisterSum(FAngelscriptBinds& Binds)
	{
		++GHostCollectionSumCallbacks;
		Binds.ExistingClassForTarget("Pair").Method("int Sum() const", &FHostCollectionPair::Sum);
	}

	void HostCollectionRegisterCounter(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds, "N");
		Binds.BindGlobalVariableForTarget("int Counter", &GHostCollectionCounter);
	}

	void HostCollectionRegisterMissing(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionDirectForTarget("void Missing()", asSFuncPtr(), asCALL_CDECL, ASAutoCaller::FunctionCaller::Make());
	}

	FAngelscriptBindRecord MakeRecord(FName BindName, EAngelscriptBindPhase Phase, FAngelscriptBindCallback Callback, int32 Line)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("AngelscriptTest");
		Record.BindName = BindName;
		Record.Phase = Phase;
		Record.SourceFile = "RuntimeBindingHostCollectionTests.cpp";
		Record.SourceLine = Line;
		Record.Callback = Callback;
		return Record;
	}

	struct FEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
	};
}

TEST_CLASS_WITH_FLAGS(HostCollection, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OneCallbackOneGraph)
	{
		GHostCollectionPairCallbacks = 0;
		GHostCollectionSumCallbacks = 0;
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Append(MakeRecord(TEXT("Pair"), EAngelscriptBindPhase::TypeDeclarations, &HostCollectionRegisterPair, 10), Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Append(MakeRecord(TEXT("PairSum"), EAngelscriptBindPhase::ExplicitBindings, &HostCollectionRegisterSum, 20), Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic)));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("Pair"))));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("PairSum"))));
		ASSERT_THAT(AreEqual(1, GHostCollectionPairCallbacks));
		ASSERT_THAT(AreEqual(1, GHostCollectionSumCallbacks));
		const asCDefinitions* Graph = Collection.GetHostDefinitions();
		ASSERT_THAT(IsNotNull(Graph));
		asCTypeInfo* Pair = Graph->FindType("Pair");
		asCScriptFunction* Sum = nullptr;
		for (asCScriptFunction* Function : Graph->GetFunctions())
		{
			if (Function && Function->GetName() && FCStringAnsi::Strcmp(Function->GetName(), "Sum") == 0) Sum = Function;
		}
		ASSERT_THAT(IsNotNull(Pair));
		ASSERT_THAT(IsNotNull(Sum));
		ASSERT_THAT(IsTrue(Pair->GetTypeInfoKind() == asETypeInfoKind::HostProcess));
		ASSERT_THAT(IsNull(Pair->GetEngine()));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic)));
		ASSERT_THAT(AreEqual(1, GHostCollectionPairCallbacks));
		FEngineOwner A;
		FEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(1, GHostCollectionPairCallbacks));
		ASSERT_THAT(AreEqual(1, GHostCollectionSumCallbacks));
		FHostCollectionPair Value{20, 22};
		const auto Interface = Sum->GetProcessInterface();
		ASSERT_THAT(IsTrue(Interface && Interface->caller.IsBound()));
		int32 Result = 0;
		void* Args[1] = { &Value };
		if (Interface->caller.type == 2)
		{
			Interface->caller.MethodCaller(Interface->method, Args, &Result);
		}
		else
		{
			Interface->caller.FunctionCaller(Interface->func, Args, &Result);
		}
		ASSERT_THAT(AreEqual(42, Result));
	}

	TEST_METHOD(OrderingAndDiagnostics)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Sealed;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Sealed.Append(MakeRecord(TEXT("Counter"), EAngelscriptBindPhase::ExplicitBindings, &HostCollectionRegisterCounter, 30), Diagnostic)));
		ASSERT_THAT(IsTrue(Sealed.Finalize(Diagnostic)));
		ASSERT_THAT(IsTrue(Sealed.ExecuteToHost(Diagnostic), Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		TArray<FAngelscriptHostDeclarationView> Before;
		Sealed.InspectHostDeclarations(Before);
		UE::Angelscript::Private::FAngelscriptBindCollection Early;
		ASSERT_THAT(IsTrue(Early.Append(MakeRecord(TEXT("PairSum"), EAngelscriptBindPhase::TypeDeclarations, &HostCollectionRegisterSum, 40), Diagnostic)));
		ASSERT_THAT(IsTrue(Early.Append(MakeRecord(TEXT("Pair"), EAngelscriptBindPhase::ExplicitBindings, &HostCollectionRegisterPair, 50), Diagnostic)));
		ASSERT_THAT(IsTrue(Early.Finalize(Diagnostic)));
		ASSERT_THAT(IsFalse(Early.ExecuteToHost(Diagnostic)));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("host object method target")) || Diagnostic.Contains(TEXT("source="))));
		ASSERT_THAT(IsNull(Early.GetHostDefinitions()));
		TArray<FAngelscriptHostDeclarationView> After;
		Sealed.InspectHostDeclarations(After);
		ASSERT_THAT(AreEqual(Before.Num(), After.Num()));
		UE::Angelscript::Private::FAngelscriptBindCollection Missing;
		ASSERT_THAT(IsTrue(Missing.Append(MakeRecord(TEXT("Missing"), EAngelscriptBindPhase::ExplicitBindings, &HostCollectionRegisterMissing, 60), Diagnostic)));
		ASSERT_THAT(IsTrue(Missing.Finalize(Diagnostic)));
		ASSERT_THAT(IsFalse(Missing.ExecuteToHost(Diagnostic)));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("native target"))));
		ASSERT_THAT(IsNull(Missing.GetHostDefinitions()));
	}

	TEST_METHOD(LateRegistrationAndInspection)
	{
		GHostCollectionCounter = 7;
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Append(MakeRecord(TEXT("Counter"), EAngelscriptBindPhase::ExplicitBindings, &HostCollectionRegisterCounter, 70), Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic)));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		TArray<FAngelscriptHostDeclarationView> Views;
		Collection.InspectHostDeclarations(Views);
		bool bFound = false;
		for (const FAngelscriptHostDeclarationView& View : Views)
		{
			ASSERT_THAT(IsFalse(View.Name.Contains(TEXT("0x"))));
			if (View.Name == TEXT("Counter") && View.Kind == TEXT("Global"))
			{
				bFound = true;
				ASSERT_THAT(AreEqual(TEXT("N"), View.Namespace));
				ASSERT_THAT(AreEqual(7, View.IntValue));
			}
		}
		ASSERT_THAT(IsTrue(bFound));
		ASSERT_THAT(IsFalse(Collection.Append(MakeRecord(TEXT("Later"), EAngelscriptBindPhase::ExplicitBindings, &HostCollectionRegisterPair, 80), Diagnostic)));
		UE::Angelscript::Private::FAngelscriptBindCollection Later;
		ASSERT_THAT(IsTrue(Later.Append(MakeRecord(TEXT("Pair"), EAngelscriptBindPhase::TypeDeclarations, &HostCollectionRegisterPair, 90), Diagnostic)));
		ASSERT_THAT(IsTrue(Later.Finalize(Diagnostic)));
		ASSERT_THAT(IsTrue(Later.ExecuteToHost(Diagnostic)));
		ASSERT_THAT(IsNotNull(Later.GetHostDefinitions()->FindType("Pair")));
		ASSERT_THAT(IsNull(Collection.GetHostDefinitions()->FindType("Pair")));
	}
};

#endif
