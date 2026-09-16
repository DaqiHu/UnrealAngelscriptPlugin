#include "CQTest.h"
#include "Core/AngelscriptBindWriteWorkers.h"
#include "Core/AngelscriptBindsInternal.h"
#include "as_callfunc.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FHostBlueprintParent
	{
		int32 X = 7;
	};

	struct FHostBlueprintChild : FHostBlueprintParent
	{
		int32 Y = 11;
	};

	int32 GHostBlueprintPings = 0;
	void HostBlueprintPing() { ++GHostBlueprintPings; }
	int32 HostBlueprintIfaceCount(int32* Self) { return Self ? 1 : 0; }

	void HostBlueprintRegisterTypes(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FHostBlueprintChild>("C", Flags);
		Binds.ValueClassForTarget<FHostBlueprintParent>("P", Flags);
		Binds.ValueClassForTarget<int32>("HostIface", Flags);
	}

	void HostBlueprintRegisterMembers(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds Child = Binds.ExistingClassForTarget("C");
		Child.SetHostBaseTypeForTarget("P");
		Child.Property("int Y", &FHostBlueprintChild::Y);
		Binds.ExistingClassForTarget("P").Property("int X", &FHostBlueprintParent::X);
		Binds.ExistingClassForTarget("HostIface").Method("int Num() const", &HostBlueprintIfaceCount);
		Binds.BindGlobalFunctionForTarget("void HostBlueprintPing()", &HostBlueprintPing);
		FAngelscriptBindWriteWorkers::NoteGlobalWrite();
	}

	FAngelscriptBindRecord HostBlueprintMakeRecord(FName BindName, EAngelscriptBindPhase Phase, FAngelscriptBindCallback Callback, int32 Line)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("AngelscriptTest");
		Record.BindName = BindName;
		Record.Phase = Phase;
		Record.SourceFile = "RuntimeBindingHostBlueprintWritesTests.cpp";
		Record.SourceLine = Line;
		Record.Callback = Callback;
		return Record;
	}

	struct FHostBlueprintEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FHostBlueprintEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
	};

	struct FHostBlueprintFingerprint
	{
		int32 PropertyX = 0;
		int32 PropertyY = 0;
		int32 IfaceNum = 0;
		int32 Ping = 0;
		bool bDerived = false;
	};

	int32 HostBlueprintCountProperty(asCObjectType* Type, const char* Name)
	{
		int32 Count = 0;
		if (!Type || !Name) return 0;
		for (asUINT Index = 0; Index < Type->GetPropertyCount(); ++Index)
		{
			const char* PropertyName = nullptr;
			Type->GetProperty(Index, &PropertyName, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
			if (PropertyName && FCStringAnsi::Strcmp(PropertyName, Name) == 0) ++Count;
		}
		return Count;
	}

	asCScriptFunction* HostBlueprintFind(asCObjectType* Type, const char* Name)
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

	asCScriptFunction* HostBlueprintFindGlobal(asCDefinitions& Graph, const char* Name)
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

	void HostBlueprintCall(asCScriptFunction& Function, void** Args, void* Return)
	{
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound()) return;
		if (Interface->caller.type == 2) Interface->caller.MethodCaller(Interface->method, Args, Return);
		else Interface->caller.FunctionCaller(Interface->func, Args, Return);
	}

	bool HostBlueprintAppend(UE::Angelscript::Private::FAngelscriptBindCollection& Collection, FString& Diagnostic)
	{
		return Collection.Append(HostBlueprintMakeRecord(TEXT("HostBlueprint.Types"), EAngelscriptBindPhase::TypeDeclarations, &HostBlueprintRegisterTypes, 10), Diagnostic)
			&& Collection.Append(HostBlueprintMakeRecord(TEXT("HostBlueprint.Members"), EAngelscriptBindPhase::ExplicitBindings, &HostBlueprintRegisterMembers, 20), Diagnostic);
	}

	bool HostBlueprintCapture(int32 Workers, FHostBlueprintFingerprint& Out, FString& Diagnostic)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("as.Bind.WriteWorkers"));
		if (!CVar) { Diagnostic = TEXT("missing as.Bind.WriteWorkers"); return false; }
		CVar->Set(Workers, ECVF_SetByCode);
		if (FAngelscriptBindWriteWorkers::ResolveCount() != FMath::Max(1, Workers))
		{
			Diagnostic = TEXT("WriteWorkers resolve mismatch");
			return false;
		}
		GHostBlueprintPings = 0;
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		if (!HostBlueprintAppend(Collection, Diagnostic) || !Collection.Finalize(Diagnostic) || !Collection.ExecuteToHost(Diagnostic))
		{
			return false;
		}
		asCDefinitions* Graph = const_cast<asCDefinitions*>(Collection.GetHostDefinitions());
		asCObjectType* Parent = static_cast<asCObjectType*>(Graph->FindType("P"));
		asCObjectType* Child = static_cast<asCObjectType*>(Graph->FindType("C"));
		asCObjectType* Iface = static_cast<asCObjectType*>(Graph->FindType("HostIface"));
		Out.bDerived = Child && Parent && Child->derivedFrom == Parent;
		Out.PropertyX = HostBlueprintCountProperty(Child, "X");
		Out.PropertyY = HostBlueprintCountProperty(Child, "Y");
		int32 Num = 0;
		int32 Dummy = 0;
		void* IfaceArgs[1] = { &Dummy };
		HostBlueprintCall(*HostBlueprintFind(Iface, "Num"), IfaceArgs, &Num);
		Out.IfaceNum = Num;
		asCScriptFunction* Ping = HostBlueprintFindGlobal(*Graph, "HostBlueprintPing");
		void* NoArgs[1] = { nullptr };
		HostBlueprintCall(*Ping, NoArgs, nullptr);
		Out.Ping = GHostBlueprintPings;
		return Out.bDerived && Out.PropertyX == 1 && Out.PropertyY == 1 && Out.IfaceNum == 1 && Out.Ping == 1;
	}
}

TEST_CLASS_WITH_FLAGS(HostBlueprintWrites, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SerialParallelEquivalence)
	{
		FHostBlueprintFingerprint First;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostBlueprintCapture(0, First, Diagnostic), *Diagnostic));
		const int32 Workers[] = { 1, 2, 4 };
		for (int32 Count : Workers)
		{
			FHostBlueprintFingerprint Next;
			ASSERT_THAT(IsTrue(HostBlueprintCapture(Count, Next, Diagnostic), *Diagnostic));
			ASSERT_THAT(AreEqual(First.PropertyX, Next.PropertyX));
			ASSERT_THAT(AreEqual(First.PropertyY, Next.PropertyY));
			ASSERT_THAT(AreEqual(First.IfaceNum, Next.IfaceNum));
			ASSERT_THAT(AreEqual(First.Ping, Next.Ping));
			ASSERT_THAT(IsTrue(First.bDerived && Next.bDerived));
		}
		IConsoleManager::Get().FindConsoleVariable(TEXT("as.Bind.WriteWorkers"))->Set(1, ECVF_SetByCode);
	}

	TEST_METHOD(ActualClassOverlap)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("as.Bind.WriteWorkers"));
		ASSERT_THAT(IsNotNull(CVar));
		CVar->Set(1, ECVF_SetByCode);
		FAngelscriptBindWriteWorkers::ResetProbe();
		FAngelscriptBindWriteWorkers::SetHoldForOverlap(true);
		ASSERT_THAT(IsTrue(FAngelscriptBindWriteWorkers::RunClassWave(2, [](int32) { FPlatformProcess::Sleep(0.01f); })));
		ASSERT_THAT(AreEqual(1, FAngelscriptBindWriteWorkers::GetMaxActiveWorkers()));
		ASSERT_THAT(AreEqual(2, FAngelscriptBindWriteWorkers::GetClaimedItems()));
		CVar->Set(2, ECVF_SetByCode);
		FAngelscriptBindWriteWorkers::ResetProbe();
		FAngelscriptBindWriteWorkers::SetHoldForOverlap(true);
		ASSERT_THAT(IsTrue(FAngelscriptBindWriteWorkers::RunClassWave(2, [](int32) { FPlatformProcess::Sleep(0.01f); })));
		ASSERT_THAT(AreEqual(2, FAngelscriptBindWriteWorkers::GetMaxActiveWorkers()));
		ASSERT_THAT(AreEqual(2, FAngelscriptBindWriteWorkers::GetClaimedItems()));
		CVar->Set(1, ECVF_SetByCode);
		FAngelscriptBindWriteWorkers::ResetProbe();
	}

	TEST_METHOD(BaseOrderAndFailure)
	{
		FHostBlueprintFingerprint Fingerprint;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostBlueprintCapture(1, Fingerprint, Diagnostic), *Diagnostic));
		ASSERT_THAT(AreEqual(1, Fingerprint.PropertyX));
		ASSERT_THAT(AreEqual(1, Fingerprint.PropertyY));
		UE::Angelscript::Private::FAngelscriptBindCollection Healthy;
		ASSERT_THAT(IsTrue(HostBlueprintAppend(Healthy, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.ExecuteToHost(Diagnostic), *Diagnostic));
		asCObjectType* Child = static_cast<asCObjectType*>(Healthy.GetHostDefinitions()->FindType("C"));
		ASSERT_THAT(AreEqual(1, HostBlueprintCountProperty(Child, "X")));
		FAngelscriptBindWriteWorkers::ResetProbe();
		FAngelscriptBindWriteWorkers::SetFailAtIndex(0);
		IConsoleManager::Get().FindConsoleVariable(TEXT("as.Bind.WriteWorkers"))->Set(2, ECVF_SetByCode);
		ASSERT_THAT(IsFalse(FAngelscriptBindWriteWorkers::RunClassWave(2, [](int32) {})));
		ASSERT_THAT(AreEqual(1, HostBlueprintCountProperty(Child, "Y")));
		FAngelscriptBindWriteWorkers::ResetProbe();
		IConsoleManager::Get().FindConsoleVariable(TEXT("as.Bind.WriteWorkers"))->Set(1, ECVF_SetByCode);
		ASSERT_THAT(AreEqual(0, FAngelscriptBindWriteWorkers::GetGlobalWritesDuringActiveWorkers()));
	}
};

#endif
