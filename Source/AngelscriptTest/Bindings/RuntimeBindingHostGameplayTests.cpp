#include "CQTest.h"
#include "Core/AngelscriptBindsInternal.h"
#include "as_callfunc.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FHostGameplayActor
	{
		FVector Location = FVector::ZeroVector;
		FVector GetActorLocation() const { return Location; }
	};

	struct FHostGameplayHit
	{
		bool bBlockingHit = true;
		FVector Location = FVector(9.0, 9.0, 9.0);
	};

	int32 GHostGameplaySubscriptions = 0;

	FVector HostGameplayGetLocation(FHostGameplayActor* Self)
	{
		return Self ? Self->GetActorLocation() : FVector::ZeroVector;
	}

	bool HostGameplayTraceEmpty(FHostGameplayHit& OutHit)
	{
		OutHit.bBlockingHit = false;
		OutHit.Location = FVector::ZeroVector;
		return false;
	}

	void HostGameplayBindInput() { ++GHostGameplaySubscriptions; }
	void HostGameplayUnbindInput()
	{
		if (GHostGameplaySubscriptions > 0) --GHostGameplaySubscriptions;
	}

	void HostGameplayRegister(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FVector>("FVector", Flags);
		Binds.ValueClassForTarget<FHostGameplayActor>("HostActor", Flags)
			.Method("FVector GetActorLocation() const", &HostGameplayGetLocation);
		Binds.ValueClassForTarget<FHostGameplayHit>("HostHit", Flags);
		Binds.BindGlobalFunctionForTarget("bool TraceEmpty(HostHit& OutHit)", &HostGameplayTraceEmpty);
		Binds.BindGlobalFunctionForTarget("void BindInput()", &HostGameplayBindInput);
		Binds.BindGlobalFunctionForTarget("void UnbindInput()", &HostGameplayUnbindInput);
	}

	void HostGameplayRegisterMissing(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionDirectForTarget("void Missing()", asSFuncPtr(), asCALL_CDECL, ASAutoCaller::FunctionCaller::Make());
	}

	FAngelscriptBindRecord HostGameplayMakeRecord(FName BindName, EAngelscriptBindPhase Phase, FAngelscriptBindCallback Callback, int32 Line)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("AngelscriptTest");
		Record.BindName = BindName;
		Record.Phase = Phase;
		Record.SourceFile = "RuntimeBindingHostGameplayTests.cpp";
		Record.SourceLine = Line;
		Record.Callback = Callback;
		return Record;
	}

	struct FHostGameplayEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FHostGameplayEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
	};

	struct FHostGameplaySite
	{
		const TCHAR* Source;
		const TCHAR* Symbol;
		const TCHAR* Evidence;
		const TCHAR* Reason;
	};

	const FHostGameplaySite GHostGameplaySites[] = {
#include "RuntimeBindingHostGameplaySites.inl"
	};

	asCScriptFunction* HostGameplayFindMethod(asCObjectType* Type, const char* Name)
	{
		if (!Type || !Name) return nullptr;
		for (asUINT Index = 0; Index < Type->GetMethodCount(); ++Index)
		{
			asIScriptFunction* Method = Type->GetMethodByIndex(Index);
			if (Method && Method->GetName() && FCStringAnsi::Strcmp(Method->GetName(), Name) == 0)
			{
				return static_cast<asCScriptFunction*>(Method);
			}
		}
		return nullptr;
	}

	asCScriptFunction* HostGameplayFindGlobal(asCDefinitions& Graph, const char* Name)
	{
		for (asCScriptFunction* Function : Graph.GetFunctions())
		{
			if (Function && Function->GetName() && FCStringAnsi::Strcmp(Function->GetName(), Name) == 0
				&& Function->GetObjectType() == nullptr)
			{
				return Function;
			}
		}
		return nullptr;
	}

	void HostGameplayCall(asCScriptFunction& Function, void** Args, void* Return)
	{
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound()) return;
		if (Interface->caller.type == 2) Interface->caller.MethodCaller(Interface->method, Args, Return);
		else Interface->caller.FunctionCaller(Interface->func, Args, Return);
	}
}

TEST_CLASS_WITH_FLAGS(HostGameplay, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GameplayBoundBehavior)
	{
		GHostGameplaySubscriptions = 0;
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Append(HostGameplayMakeRecord(TEXT("HostGameplay.Members"), EAngelscriptBindPhase::TypeDeclarations, &HostGameplayRegister, 10), Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));
		asCDefinitions* Graph = const_cast<asCDefinitions*>(Collection.GetHostDefinitions());
		asCScriptFunction* GetLocation = HostGameplayFindMethod(static_cast<asCObjectType*>(Graph->FindType("HostActor")), "GetActorLocation");
		asCScriptFunction* Trace = HostGameplayFindGlobal(*Graph, "TraceEmpty");
		asCScriptFunction* Bind = HostGameplayFindGlobal(*Graph, "BindInput");
		asCScriptFunction* Unbind = HostGameplayFindGlobal(*Graph, "UnbindInput");
		ASSERT_THAT(IsNotNull(GetLocation));
		ASSERT_THAT(IsNotNull(Trace));
		FHostGameplayActor Actor;
		Actor.Location = FVector(1.0, 2.0, 3.0);
		FVector Location(ForceInitToZero);
		void* ActorArgs[1] = { &Actor };
		HostGameplayCall(*GetLocation, ActorArgs, &Location);
		ASSERT_THAT(IsTrue(Location.Equals(FVector(1.0, 2.0, 3.0))));
		FHostGameplayHit Hit;
		void* TraceArgs[1] = { &Hit };
		bool bHit = true;
		HostGameplayCall(*Trace, TraceArgs, &bHit);
		ASSERT_THAT(IsFalse(bHit));
		ASSERT_THAT(IsFalse(Hit.bBlockingHit));
		ASSERT_THAT(IsTrue(Hit.Location.Equals(FVector::ZeroVector)));
		void* NoArgs[1] = { nullptr };
		HostGameplayCall(*Bind, NoArgs, nullptr);
		ASSERT_THAT(AreEqual(1, GHostGameplaySubscriptions));
		HostGameplayCall(*Unbind, NoArgs, nullptr);
		ASSERT_THAT(AreEqual(0, GHostGameplaySubscriptions));
		FHostGameplayEngineOwner A;
		FHostGameplayEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
	}

	TEST_METHOD(CompleteFamilyAccounting)
	{
		ASSERT_THAT(AreEqual(63, static_cast<int32>(UE_ARRAY_COUNT(GHostGameplaySites))));
		int32 Executable = 0, MetadataOnly = 0, Companion = 0;
		for (const FHostGameplaySite& Site : GHostGameplaySites)
		{
			ASSERT_THAT(IsTrue(Site.Reason && *Site.Reason));
			if (FCString::Strcmp(Site.Evidence, TEXT("executable")) == 0) ++Executable;
			else if (FCString::Strcmp(Site.Evidence, TEXT("metadata_only")) == 0) ++MetadataOnly;
			else
			{
				ASSERT_THAT(AreEqual(FString(TEXT("companion_no_output")), FString(Site.Evidence)));
				++Companion;
			}
		}
		ASSERT_THAT(AreEqual(0, Executable));
		ASSERT_THAT(AreEqual(52, MetadataOnly));
		ASSERT_THAT(AreEqual(11, Companion));
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Append(HostGameplayMakeRecord(TEXT("HostGameplay.Members"), EAngelscriptBindPhase::TypeDeclarations, &HostGameplayRegister, 10), Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));
		FHostGameplayEngineOwner A;
		FHostGameplayEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		const int32 Before = Collection.GetCallbackInvocationCount(TEXT("HostGameplay.Members"));
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(Before, Collection.GetCallbackInvocationCount(TEXT("HostGameplay.Members"))));
	}

	TEST_METHOD(OwnerAndFailureBoundaries)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Healthy;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Healthy.Append(HostGameplayMakeRecord(TEXT("HostGameplay.Members"), EAngelscriptBindPhase::TypeDeclarations, &HostGameplayRegister, 10), Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.ExecuteToHost(Diagnostic), *Diagnostic));
		FHostGameplayActor Actor;
		Actor.Location = FVector(1.0, 2.0, 3.0);
		FVector Location(ForceInitToZero);
		void* Args[1] = { &Actor };
		HostGameplayCall(*HostGameplayFindMethod(static_cast<asCObjectType*>(Healthy.GetHostDefinitions()->FindType("HostActor")), "GetActorLocation"), Args, &Location);
		ASSERT_THAT(IsTrue(Location.Equals(FVector(1.0, 2.0, 3.0))));
		UE::Angelscript::Private::FAngelscriptBindCollection Failed;
		ASSERT_THAT(IsTrue(Failed.Append(HostGameplayMakeRecord(TEXT("HostGameplay.Missing"), EAngelscriptBindPhase::ExplicitBindings, &HostGameplayRegisterMissing, 20), Diagnostic)));
		ASSERT_THAT(IsTrue(Failed.Finalize(Diagnostic)));
		ASSERT_THAT(IsFalse(Failed.ExecuteToHost(Diagnostic)));
		HostGameplayCall(*HostGameplayFindMethod(static_cast<asCObjectType*>(Healthy.GetHostDefinitions()->FindType("HostActor")), "GetActorLocation"), Args, &Location);
		ASSERT_THAT(IsTrue(Location.Equals(FVector(1.0, 2.0, 3.0))));
	}
};

#endif
