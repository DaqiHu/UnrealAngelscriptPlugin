#include "CQTest.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptEngine.h"
#include "Engine/Engine.h"
#include "Engine/TimerHandle.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "as_callfunc.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FHostServicesBundle
	{
		int32 Num() const { return 0; }
	};

	struct FHostServicesTimer
	{
		double Interval = 0.25;
		void SetInterval(double Value) { Interval = Value; }
		double GetInterval() const { return Interval; }
	};

	void HostServicesRegister(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		FAngelscriptBinds Bundle = Binds.ValueClassForTarget<FHostServicesBundle>("HostBundle", Flags);
		Bundle.Method("int Num() const", &FHostServicesBundle::Num);
		FAngelscriptBinds Timer = Binds.ValueClassForTarget<FHostServicesTimer>("HostTimer", Flags);
		Timer.Method("void SetInterval(float64 Value)", &FHostServicesTimer::SetInterval);
		Timer.Method("float64 GetInterval() const", &FHostServicesTimer::GetInterval);
	}

	void HostServicesRegisterMissing(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionDirectForTarget("void Missing()", asSFuncPtr(), asCALL_CDECL, ASAutoCaller::FunctionCaller::Make());
	}

	FAngelscriptBindRecord HostServicesMakeRecord(FName BindName, EAngelscriptBindPhase Phase, FAngelscriptBindCallback Callback, int32 Line)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("AngelscriptTest");
		Record.BindName = BindName;
		Record.Phase = Phase;
		Record.SourceFile = "RuntimeBindingHostServicesTests.cpp";
		Record.SourceLine = Line;
		Record.Callback = Callback;
		return Record;
	}

	struct FHostServicesEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FHostServicesEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
	};

	struct FHostServicesSite
	{
		const TCHAR* Source;
		const TCHAR* Symbol;
		const TCHAR* Evidence;
		const TCHAR* Reason;
	};

	const FHostServicesSite GHostServicesSites[] = {
#include "RuntimeBindingHostServicesSites.inl"
	};

	asCScriptFunction* HostServicesFindMethod(asCObjectType* Type, const char* Name)
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

	asIScriptFunction* HostServicesFindGlobal(asIScriptEngine* Engine, const char* Name)
	{
		if (!Engine || !Name) return nullptr;
		for (asUINT Index = 0; Index < Engine->GetGlobalFunctionCount(); ++Index)
		{
			asIScriptFunction* Function = Engine->GetGlobalFunctionByIndex(Index);
			if (Function && Function->GetName() && FCStringAnsi::Strcmp(Function->GetName(), Name) == 0)
			{
				return Function;
			}
		}
		return nullptr;
	}

	struct FHostServicesTransientWorld
	{
		UWorld* World = nullptr;
		FHostServicesTransientWorld()
		{
			World = UWorld::CreateWorld(
				EWorldType::Game,
				false,
				FName(*FString::Printf(TEXT("HostServicesWorld_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))),
				GetTransientPackage());
			if (World && GEngine)
			{
				GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			}
		}
		~FHostServicesTransientWorld()
		{
			if (World)
			{
				World->GetTimerManager().ClearAllTimersForObject(World);
				World->DestroyWorld(false);
				if (GEngine) GEngine->DestroyWorldContext(World);
			}
		}
	};

	void HostServicesCall(asCScriptFunction& Function, void** Args, void* Return)
	{
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound()) return;
		if (Interface->caller.type == 2) Interface->caller.MethodCaller(Interface->method, Args, Return);
		else Interface->caller.FunctionCaller(Interface->func, Args, Return);
	}
}

TEST_CLASS_WITH_FLAGS(HostServices, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ServicesBoundBehavior)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Append(HostServicesMakeRecord(TEXT("HostServices.Members"), EAngelscriptBindPhase::TypeDeclarations, &HostServicesRegister, 10), Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));
		asCObjectType* BundleType = static_cast<asCObjectType*>(Collection.GetHostDefinitions()->FindType("HostBundle"));
		asCObjectType* TimerType = static_cast<asCObjectType*>(Collection.GetHostDefinitions()->FindType("HostTimer"));
		FHostServicesBundle Bundle;
		int32 Count = -1;
		void* BundleArgs[1] = { &Bundle };
		HostServicesCall(*HostServicesFindMethod(BundleType, "Num"), BundleArgs, &Count);
		ASSERT_THAT(AreEqual(0, Count));
		FHostServicesTimer Timer;
		double Interval = 0.0;
		void* TimerArgs[1] = { &Timer };
		HostServicesCall(*HostServicesFindMethod(TimerType, "GetInterval"), TimerArgs, &Interval);
		ASSERT_THAT(IsTrue(Interval == 0.25));
		double Next = 0.25;
		void* SetArgs[2] = { &Timer, &Next };
		HostServicesCall(*HostServicesFindMethod(TimerType, "SetInterval"), SetArgs, nullptr);
		HostServicesCall(*HostServicesFindMethod(TimerType, "GetInterval"), TimerArgs, &Interval);
		ASSERT_THAT(IsTrue(Interval == 0.25));
		FHostServicesEngineOwner A;
		FHostServicesEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
	}

	TEST_METHOD(TimerOrSubsystemHostCall)
	{
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		TUniquePtr<FAngelscriptEngine> Owner = FAngelscriptEngine::CreateForBindings(BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			Owner.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		ASSERT_THAT(IsNotNull(Owner->Engine));
		FHostServicesTransientWorld W;
		ASSERT_THAT(IsNotNull(W.World));
		FAngelscriptEngineScope Scope(*Owner, W.World);
		int32 Calls = 0;
		FTimerHandle Handle;
		W.World->GetTimerManager().SetTimer(Handle, [&Calls]() { ++Calls; }, 0.01f, true);
		W.World->GetTimerManager().Tick(0.0f);
		++GFrameCounter;
		W.World->GetTimerManager().Tick(0.011f);
		ASSERT_THAT(AreEqual(1, Calls));
		asIScriptFunction* Clear = HostServicesFindGlobal(Owner->Engine, "ClearAndInvalidateTimerHandle");
		ASSERT_THAT(IsNotNull(Clear));
		asIScriptContext* Context = Owner->Engine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Clear)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &Handle)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute()));
		Context->Release();
		ASSERT_THAT(IsFalse(Handle.IsValid()));
		++GFrameCounter;
		W.World->GetTimerManager().Tick(0.05f);
		ASSERT_THAT(AreEqual(1, Calls));
	}

	TEST_METHOD(CompleteFamilyAccounting)
	{
		ASSERT_THAT(AreEqual(51, static_cast<int32>(UE_ARRAY_COUNT(GHostServicesSites))));
		int32 Executable = 0, MetadataOnly = 0, Companion = 0;
		for (const FHostServicesSite& Site : GHostServicesSites)
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
		ASSERT_THAT(AreEqual(40, MetadataOnly));
		ASSERT_THAT(AreEqual(11, Companion));
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Collection.Append(HostServicesMakeRecord(TEXT("HostServices.Members"), EAngelscriptBindPhase::TypeDeclarations, &HostServicesRegister, 10), Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));
		const int32 Before = Collection.GetCallbackInvocationCount(TEXT("HostServices.Members"));
		FHostServicesEngineOwner A;
		FHostServicesEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(Before, Collection.GetCallbackInvocationCount(TEXT("HostServices.Members"))));
	}

	TEST_METHOD(OwnerAndFailureBoundaries)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Healthy;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Healthy.Append(HostServicesMakeRecord(TEXT("HostServices.Members"), EAngelscriptBindPhase::TypeDeclarations, &HostServicesRegister, 10), Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.ExecuteToHost(Diagnostic), *Diagnostic));
		FHostServicesBundle Bundle;
		int32 Count = -1;
		void* Args[1] = { &Bundle };
		HostServicesCall(*HostServicesFindMethod(static_cast<asCObjectType*>(Healthy.GetHostDefinitions()->FindType("HostBundle")), "Num"), Args, &Count);
		ASSERT_THAT(AreEqual(0, Count));
		UE::Angelscript::Private::FAngelscriptBindCollection Failed;
		ASSERT_THAT(IsTrue(Failed.Append(HostServicesMakeRecord(TEXT("HostServices.Missing"), EAngelscriptBindPhase::ExplicitBindings, &HostServicesRegisterMissing, 20), Diagnostic)));
		ASSERT_THAT(IsTrue(Failed.Finalize(Diagnostic)));
		ASSERT_THAT(IsFalse(Failed.ExecuteToHost(Diagnostic)));
		HostServicesCall(*HostServicesFindMethod(static_cast<asCObjectType*>(Healthy.GetHostDefinitions()->FindType("HostBundle")), "Num"), Args, &Count);
		ASSERT_THAT(AreEqual(0, Count));
	}
};

#endif
