#include "Runtime/AngelscriptStandaloneAllocator.h"

#include "ClassGenerator/ASClass.h"
#include "CoreMinimal.h"
#include "angelscript.h"

#include <iostream>
#include <string>

namespace
{
	struct FInstructionAbortObservation
	{
		int BeforeCount = 0;
		int AfterCount = 0;
	};

	constexpr asPWORD RawObjectTeardownProbeSlot =
		static_cast<asPWORD>(0x5241575445415244ull);
	bool GRawObjectRegistryWasAliveDuringTypeCleanup = false;

	bool Require(bool Condition, const char* What)
	{
		if (!Condition)
		{
			std::cerr << "FAILED: " << What << '\n';
		}
		return Condition;
	}

	void FailBeforeFirstInstruction(
		asIScriptContext* Context,
		const asSVMInstructionInfo* Info,
		void* UserData)
	{
		auto* Observation =
			static_cast<FInstructionAbortObservation*>(UserData);
		if (Info->Phase == asVM_BEFORE_INSTRUCTION)
		{
			++Observation->BeforeCount;
			Context->SetException("stop before instruction");
		}
		else
		{
			++Observation->AfterCount;
		}
	}

	void ObserveRawObjectRegistryDuringTypeCleanup(asITypeInfo* ScriptType)
	{
		void* ScriptObject = ScriptType->GetUserData(RawObjectTeardownProbeSlot);
		GRawObjectRegistryWasAliveDuringTypeCleanup =
			ScriptObject != nullptr
			&& UASClass::GetRawScriptObjectType(ScriptObject) == ScriptType;
		UASClass::UnregisterRawScriptObject(ScriptObject);
		FMemory::Free(ScriptObject);
	}

	bool TestHardMemoryLimitRejectsBeforeAllocation()
	{
		using namespace AngelscriptStandalone;

		FCountingAllocator Allocator(64);
		bool bPassed = Require(
			Allocator.Install(),
			"hard-limit allocator installation");
		void* First = asAllocMem(32);
		bPassed &= Require(First != nullptr, "below-limit allocation");
		bPassed &= Require(
			Allocator.GetCurrentBytes() == 32,
			"below-limit allocation accounting");
		void* Rejected = asAllocMem(40);
		bPassed &= Require(
			Rejected == nullptr,
			"over-limit allocation must return null");
		bPassed &= Require(
			Allocator.GetCurrentBytes() == 32,
			"rejected allocation must not change current bytes");
		bPassed &= Require(
			Allocator.GetPeakBytes() <= 64,
			"rejected allocation must not exceed peak limit");
		bPassed &= Require(
			Allocator.GetRejectedAllocationCount() == 1,
			"rejected allocation count");
		asFreeMem(Rejected);
		asFreeMem(First);
		bPassed &= Require(
			Allocator.GetCurrentBytes() == 0,
			"hard-limit allocator cleanup");
		Allocator.Uninstall();
		return bPassed;
	}

	bool TestZeroSizeAllocationCanOutliveAllocatorOwner()
	{
		using namespace AngelscriptStandalone;

		void* DelayedFree = nullptr;
		bool bPassed = true;
		{
			FCountingAllocator Allocator(64);
			bPassed &= Require(
				Allocator.Install(),
				"zero-size allocator installation");
			DelayedFree = asAllocMem(0);
			bPassed &= Require(
				DelayedFree != nullptr,
				"zero-size allocation must retain a releasable address");
			bPassed &= Require(
				Allocator.GetCurrentBytes() == 0,
				"zero-size allocation must not charge the byte budget");
		}

		asFreeMem(DelayedFree);

		FCountingAllocator NextAllocator(64);
		bPassed &= Require(
			NextAllocator.Install(),
			"allocator installation after delayed zero-size free");
		void* NextAllocation = asAllocMem(16);
		bPassed &= Require(
			NextAllocation != nullptr,
			"allocation after delayed zero-size free");
		asFreeMem(NextAllocation);
		bPassed &= Require(
			NextAllocator.GetCurrentBytes() == 0,
			"allocator accounting after delayed zero-size free");
		NextAllocator.Uninstall();
		return bPassed;
	}

	bool TestAbortBeforeInstructionHasNoAfterEvent()
	{
		asIScriptEngine* Engine = asCreateScriptEngine();
		bool bPassed = Require(Engine != nullptr, "instruction callback engine");
		if (Engine == nullptr)
		{
			return false;
		}

		asIScriptModule* Module =
			Engine->GetModule("instruction-abort", asGM_ALWAYS_CREATE);
		const char* Script = "int main() { return 42; }";
		bPassed &= Require(
			Module != nullptr
				&& Module->AddScriptSection("instruction-abort.as", Script) >= 0
				&& Module->Build() >= 0,
			"instruction callback fixture build");
		asIScriptFunction* Function =
			Module != nullptr ? Module->GetFunctionByDecl("int main()") : nullptr;
		asIScriptContext* Context = Engine->CreateContext();
		FInstructionAbortObservation Observation;
		bPassed &= Require(
			Function != nullptr
				&& Context != nullptr
				&& Context->Prepare(Function) >= 0
				&& Context->SetInstructionCallback(
					&FailBeforeFirstInstruction,
					&Observation) >= 0,
			"instruction callback fixture prepare");
		if (Context != nullptr && Function != nullptr)
		{
			const int ExecutionResult = Context->Execute();
			if (ExecutionResult != asEXECUTION_EXCEPTION
				|| Observation.BeforeCount != 1
				|| Observation.AfterCount != 0)
			{
				std::cerr
					<< "instruction callback observation: result="
					<< ExecutionResult
					<< ", before=" << Observation.BeforeCount
					<< ", after=" << Observation.AfterCount << '\n';
			}
			bPassed &= Require(
				ExecutionResult == asEXECUTION_EXCEPTION,
				"instruction callback exception result");
			bPassed &= Require(
				Observation.BeforeCount == 1,
				"instruction callback before count");
			bPassed &= Require(
				Observation.AfterCount == 0,
				"rejected instruction must not emit an after event");
		}
		if (Context != nullptr)
		{
			Context->Release();
		}
		Engine->ShutDownAndRelease();
		asThreadCleanup();
		return bPassed;
	}

	bool TestDirectEngineReleaseDrainsRawScriptObjects()
	{
		using namespace AngelscriptStandalone;

		FCountingAllocator Allocator(64ull * 1024ull * 1024ull);
		bool bPassed = Require(
			Allocator.Install(),
			"direct-release allocator installation");
		asIScriptEngine* Engine = asCreateScriptEngine();
		bPassed &= Require(Engine != nullptr, "direct-release engine");
		if (Engine != nullptr)
		{
			GRawObjectRegistryWasAliveDuringTypeCleanup = false;
			Engine->SetTypeInfoUserDataCleanupCallback(
				&ObserveRawObjectRegistryDuringTypeCleanup,
				RawObjectTeardownProbeSlot);
			asIScriptModule* Module =
				Engine->GetModule("direct-release", asGM_ALWAYS_CREATE);
			const char* Script =
				"class FReleaseProbe { int Value; }";
			bPassed &= Require(
				Module != nullptr
					&& Module->AddScriptSection("direct-release.as", Script) >= 0
					&& Module->Build() >= 0,
				"direct-release raw-object fixture build");
			asITypeInfo* ScriptType = Module != nullptr
				? Module->GetTypeInfoByDecl("FReleaseProbe")
				: nullptr;
			void* ScriptObject = FMemory::Malloc(sizeof(void*), alignof(void*));
			bPassed &= Require(
				ScriptType != nullptr && ScriptObject != nullptr,
				"direct-release raw-object teardown probe allocation");
			if (ScriptType != nullptr && ScriptObject != nullptr)
			{
				UASClass::RegisterRawScriptObject(ScriptObject, ScriptType);
				ScriptType->SetUserData(
					ScriptObject,
					RawObjectTeardownProbeSlot);
			}
			Engine->Release();
		}
		bPassed &= Require(
			GRawObjectRegistryWasAliveDuringTypeCleanup,
			"direct Engine::Release must retain raw-object type lookup through shutdown");
		bPassed &= Require(
			asThreadCleanup() == asSUCCESS,
			"direct-release thread cleanup");
		bPassed &= Require(
			Allocator.GetCurrentBytes() == 0,
			"direct Engine::Release must drain raw script objects");
		Allocator.Uninstall();
		return bPassed;
	}
}

int main()
{
	using namespace AngelscriptStandalone;

	bool Passed = true;
	Passed &= TestHardMemoryLimitRejectsBeforeAllocation();
	Passed &= TestZeroSizeAllocationCanOutliveAllocatorOwner();
	Passed &= TestAbortBeforeInstructionHasNoAfterEvent();
	Passed &= TestDirectEngineReleaseDrainsRawScriptObjects();
	return Passed ? 0 : 1;
}
