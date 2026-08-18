#include "AngelscriptTestJITProbes.h"

#include "StaticJIT/StaticJITHeader.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"

#include "Misc/ScopeLock.h"

#include <atomic>

namespace AngelscriptTestJITProbes
{
	namespace
	{
		std::atomic<int32> CallCount{0};
		std::atomic<int32> LastValue{0};
		std::atomic<int32> TypedASTScalarBranchProbeCount{0};
		std::atomic<FTypedASTScalarBranchProbe> TypedASTScalarBranchProbe{nullptr};
		std::atomic<int32> TypedASTMutationPowerProbeCount{0};
		std::atomic<FTypedASTMutationProbe> TypedASTMutationProbe{nullptr};
		std::atomic<FTypedASTPowerFloatProbe> TypedASTPowerFloatProbe{nullptr};
		std::atomic<FTypedASTPowerDoubleProbe> TypedASTPowerDoubleProbe{nullptr};
		std::atomic<FTypedASTPowerDoubleIntProbe> TypedASTPowerDoubleIntProbe{nullptr};
		std::atomic<int32> TypedASTControlFlowProbeCount{0};
		std::atomic<FTypedASTStructuredControlFlowProbe>
			TypedASTStructuredControlFlowProbe{nullptr};
		std::atomic<FTypedASTExhaustiveEnumProbe>
			TypedASTExhaustiveEnumProbe{nullptr};
		std::atomic<asJITFunction> TypedASTScalarProviderVMEntry{nullptr};
		std::atomic<asJITFunction_Raw> TypedASTScalarProviderRawEntry{nullptr};
		std::atomic<asJITFunction_ParmsEntry>
			TypedASTScalarProviderParmsEntry{nullptr};
		std::atomic<asJITFunction> TypedASTCapabilityProviderVMEntry{nullptr};
		std::atomic<asJITFunction_Raw>
			TypedASTCapabilityProviderRawEntry{nullptr};
		std::atomic<asJITFunction_ParmsEntry>
			TypedASTCapabilityProviderParmsEntry{nullptr};

		struct FIsolatedDifferentialEntrySet
		{
			std::atomic<asJITFunction> VMEntry{nullptr};
			std::atomic<asJITFunction_Raw> RawEntry{nullptr};
			std::atomic<asJITFunction_ParmsEntry> ParmsEntry{nullptr};
			std::atomic<const TCHAR*> SymbolPrefix{nullptr};
			std::atomic<const TCHAR*> StableFunctionKey{nullptr};
		};

		FIsolatedDifferentialEntrySet IsolatedBytecodeDifferentialEntries;
		FIsolatedDifferentialEntrySet IsolatedTypedASTDifferentialEntries;

		FCriticalSection& GetIsolatedDifferentialRegistryMutex()
		{
			static FCriticalSection Mutex;
			return Mutex;
		}

		TArray<FIsolatedDifferentialEntryView>&
			GetIsolatedDifferentialRegistry()
		{
			static TArray<FIsolatedDifferentialEntryView> Registry;
			return Registry;
		}

		using FTypedASTScalarProviderRawEntry = int32(*)(
			FScriptExecution&, asDWORD, asDWORD);
		using FTypedASTCapabilityProviderRawEntry = int32(*)(
			FScriptExecution&, asDWORD, asDWORD, asBYTE, asBYTE);
		using FIsolatedDifferentialRawEntry = int32(*)(
			FScriptExecution&, void*, asDWORD, asDWORD);

		bool HasIsolatedDifferentialEntries(
			const FIsolatedDifferentialEntrySet& Entries)
		{
			return Entries.VMEntry.load(std::memory_order_acquire) != nullptr
				&& Entries.RawEntry.load(std::memory_order_acquire) != nullptr
				&& Entries.ParmsEntry.load(std::memory_order_acquire) != nullptr
				&& Entries.SymbolPrefix.load(std::memory_order_acquire) != nullptr
				&& Entries.StableFunctionKey.load(std::memory_order_acquire) != nullptr;
		}

		FString GetIsolatedDifferentialSymbolPrefix(
			const FIsolatedDifferentialEntrySet& Entries)
		{
			const TCHAR* const Prefix = Entries.SymbolPrefix.load(
				std::memory_order_acquire);
			return Prefix != nullptr ? FString(Prefix) : FString();
		}

		FString GetIsolatedDifferentialFunctionKey(
			const FIsolatedDifferentialEntrySet& Entries)
		{
			const TCHAR* const FunctionKey = Entries.StableFunctionKey.load(
				std::memory_order_acquire);
			return FunctionKey != nullptr ? FString(FunctionKey) : FString();
		}

		int32 ExecuteIsolatedDifferentialRaw(
			const FIsolatedDifferentialEntrySet& Entries,
			const int32 A,
			const int32 B)
		{
			const asJITFunction_Raw UntypedEntry = Entries.RawEntry.load(
				std::memory_order_acquire);
			checkf(UntypedEntry != nullptr,
				TEXT("The isolated differential raw entry is not registered."));
			const FIsolatedDifferentialRawEntry Entry =
				reinterpret_cast<FIsolatedDifferentialRawEntry>(UntypedEntry);
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			return Entry(
				Execution,
				nullptr,
				static_cast<asDWORD>(A),
				static_cast<asDWORD>(B));
		}

		int32 ExecuteIsolatedDifferentialVM(
			const FIsolatedDifferentialEntrySet& Entries,
			const int32 A,
			const int32 B)
		{
			const asJITFunction Entry = Entries.VMEntry.load(
				std::memory_order_acquire);
			checkf(Entry != nullptr,
				TEXT("The isolated differential VM entry is not registered."));
			struct FFrame
			{
				void* Object;
				asDWORD A;
				asDWORD B;
			};
			FFrame Frame{nullptr, static_cast<asDWORD>(A), static_cast<asDWORD>(B)};
			asQWORD OutValue = 0;
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			Entry(Execution, reinterpret_cast<asDWORD*>(&Frame), &OutValue);
			return static_cast<int32>(static_cast<asDWORD>(OutValue));
		}

		int32 ExecuteIsolatedDifferentialParms(
			const FIsolatedDifferentialEntrySet& Entries,
			const int32 A,
			const int32 B)
		{
			const asJITFunction_ParmsEntry Entry = Entries.ParmsEntry.load(
				std::memory_order_acquire);
			checkf(Entry != nullptr,
				TEXT("The isolated differential Parms entry is not registered."));
			struct FParms
			{
				int32 A;
				int32 B;
				int32 ReturnValue;
			};
			FParms Parms{A, B, 0};
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			Entry(Execution, nullptr, &Parms);
			return Parms.ReturnValue;
		}

		void RegisterIsolatedDifferentialEntries(
			FIsolatedDifferentialEntrySet& Entries,
			const asJITFunction VMEntry,
			const asJITFunction_Raw RawEntry,
			const asJITFunction_ParmsEntry ParmsEntry,
			const TCHAR* const SymbolPrefix,
			const TCHAR* const StableFunctionKey)
		{
			Entries.RawEntry.store(RawEntry, std::memory_order_relaxed);
			Entries.ParmsEntry.store(ParmsEntry, std::memory_order_relaxed);
			Entries.SymbolPrefix.store(SymbolPrefix, std::memory_order_relaxed);
			Entries.StableFunctionKey.store(
				StableFunctionKey, std::memory_order_relaxed);
			Entries.VMEntry.store(VMEntry, std::memory_order_release);
		}
	}

	const FAngelscriptJITProviderId& GetStableProviderId()
	{
		static const FAngelscriptJITProviderId ProviderId{
			FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT(
				"7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44")))}};
		return ProviderId;
	}

	void Reset()
	{
		LastValue.store(0, std::memory_order_relaxed);
		CallCount.store(0, std::memory_order_release);
	}

	void Record(const int32 Value)
	{
		LastValue.store(Value, std::memory_order_relaxed);
		CallCount.fetch_add(1, std::memory_order_release);
	}

	int32 GetCallCount()
	{
		return CallCount.load(std::memory_order_acquire);
	}

	int32 GetLastValue()
	{
		return LastValue.load(std::memory_order_acquire);
	}

	int32 ExecuteTypedASTScalarBranch(const int32 A, const int32 B)
	{
		const FTypedASTScalarBranchProbe Probe =
			TypedASTScalarBranchProbe.load(std::memory_order_acquire);
		checkf(Probe != nullptr,
			TEXT("The generated TypedASTJIT scalar probe is not registered."));
		TypedASTScalarBranchProbeCount.fetch_add(
			1, std::memory_order_release);
		return Probe(A, B);
	}

	void ResetTypedASTScalarBranchProbeCount()
	{
		TypedASTScalarBranchProbeCount.store(0, std::memory_order_release);
	}

	int32 GetTypedASTScalarBranchProbeCount()
	{
		return TypedASTScalarBranchProbeCount.load(std::memory_order_acquire);
	}

	void RegisterTypedASTScalarBranchProbe(
		const FTypedASTScalarBranchProbe Probe)
	{
		TypedASTScalarBranchProbe.store(Probe, std::memory_order_release);
	}

	bool HasTypedASTMutationPowerProbes()
	{
		return TypedASTMutationProbe.load(std::memory_order_acquire) != nullptr
			&& TypedASTPowerFloatProbe.load(std::memory_order_acquire) != nullptr
			&& TypedASTPowerDoubleProbe.load(std::memory_order_acquire) != nullptr
			&& TypedASTPowerDoubleIntProbe.load(std::memory_order_acquire) != nullptr;
	}

	void ResetTypedASTMutationPowerProbeCount()
	{
		TypedASTMutationPowerProbeCount.store(0, std::memory_order_release);
	}

	int32 GetTypedASTMutationPowerProbeCount()
	{
		return TypedASTMutationPowerProbeCount.load(std::memory_order_acquire);
	}

	int32 ExecuteTypedASTMutation(const int32 Input, const int32 Source)
	{
		const FTypedASTMutationProbe Probe =
			TypedASTMutationProbe.load(std::memory_order_acquire);
		checkf(Probe != nullptr,
			TEXT("The generated TypedASTJIT mutation probe is not registered."));
		TypedASTMutationPowerProbeCount.fetch_add(1, std::memory_order_release);
		return Probe(Input, Source);
	}

	float ExecuteTypedASTPowerFloat(const float Base, const float Exponent)
	{
		const FTypedASTPowerFloatProbe Probe =
			TypedASTPowerFloatProbe.load(std::memory_order_acquire);
		checkf(Probe != nullptr,
			TEXT("The generated TypedASTJIT POWf probe is not registered."));
		TypedASTMutationPowerProbeCount.fetch_add(1, std::memory_order_release);
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		return Probe(Execution, Base, Exponent);
	}

	double ExecuteTypedASTPowerDouble(const double Base, const double Exponent)
	{
		const FTypedASTPowerDoubleProbe Probe =
			TypedASTPowerDoubleProbe.load(std::memory_order_acquire);
		checkf(Probe != nullptr,
			TEXT("The generated TypedASTJIT POWd probe is not registered."));
		TypedASTMutationPowerProbeCount.fetch_add(1, std::memory_order_release);
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		return Probe(Execution, Base, Exponent);
	}

	double ExecuteTypedASTPowerDoubleInt(
		const double Base, const int32 Exponent)
	{
		const FTypedASTPowerDoubleIntProbe Probe =
			TypedASTPowerDoubleIntProbe.load(std::memory_order_acquire);
		checkf(Probe != nullptr,
			TEXT("The generated TypedASTJIT POWdi probe is not registered."));
		TypedASTMutationPowerProbeCount.fetch_add(1, std::memory_order_release);
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		return Probe(Execution, Base, Exponent);
	}

	void RegisterTypedASTMutationPowerProbes(
		const FTypedASTMutationProbe MutationProbe,
		const FTypedASTPowerFloatProbe PowerFloatProbe,
		const FTypedASTPowerDoubleProbe PowerDoubleProbe,
		const FTypedASTPowerDoubleIntProbe PowerDoubleIntProbe)
	{
		TypedASTMutationProbe.store(MutationProbe, std::memory_order_relaxed);
		TypedASTPowerFloatProbe.store(PowerFloatProbe, std::memory_order_relaxed);
		TypedASTPowerDoubleProbe.store(PowerDoubleProbe, std::memory_order_relaxed);
		TypedASTPowerDoubleIntProbe.store(
			PowerDoubleIntProbe, std::memory_order_release);
	}

	bool HasTypedASTControlFlowProbes()
	{
		return TypedASTStructuredControlFlowProbe.load(
				std::memory_order_acquire) != nullptr
			&& TypedASTExhaustiveEnumProbe.load(
				std::memory_order_acquire) != nullptr;
	}

	void ResetTypedASTControlFlowProbeCount()
	{
		TypedASTControlFlowProbeCount.store(0, std::memory_order_release);
	}

	int32 GetTypedASTControlFlowProbeCount()
	{
		return TypedASTControlFlowProbeCount.load(std::memory_order_acquire);
	}

	int32 ExecuteTypedASTStructuredControlFlow(
		const int32 Limit,
		const int32 Selector)
	{
		const FTypedASTStructuredControlFlowProbe Probe =
			TypedASTStructuredControlFlowProbe.load(std::memory_order_acquire);
		checkf(Probe != nullptr,
			TEXT("The generated TypedASTJIT structured-control-flow probe is not registered."));
		TypedASTControlFlowProbeCount.fetch_add(1, std::memory_order_release);
		return Probe(Limit, Selector);
	}

	int32 ExecuteTypedASTExhaustiveEnum(
		const int32 Mode,
		bool& bOutExceptionThrown)
	{
		const FTypedASTExhaustiveEnumProbe Probe =
			TypedASTExhaustiveEnumProbe.load(std::memory_order_acquire);
		checkf(Probe != nullptr,
			TEXT("The generated TypedASTJIT exhaustive-enum probe is not registered."));
		TypedASTControlFlowProbeCount.fetch_add(1, std::memory_order_release);
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		const int32 Result = Probe(Execution, Mode);
		bOutExceptionThrown = Execution.bExceptionThrown;
		return Result;
	}

	void RegisterTypedASTControlFlowProbes(
		const FTypedASTStructuredControlFlowProbe StructuredControlFlowProbe,
		const FTypedASTExhaustiveEnumProbe ExhaustiveEnumProbe)
	{
		TypedASTStructuredControlFlowProbe.store(
			StructuredControlFlowProbe, std::memory_order_relaxed);
		TypedASTExhaustiveEnumProbe.store(
			ExhaustiveEnumProbe, std::memory_order_release);
	}

	bool HasTypedASTScalarProviderEntries()
	{
		return TypedASTScalarProviderVMEntry.load(std::memory_order_acquire)
			!= nullptr
			&& TypedASTScalarProviderRawEntry.load(std::memory_order_acquire)
				!= nullptr
			&& TypedASTScalarProviderParmsEntry.load(std::memory_order_acquire)
				!= nullptr;
	}

	int32 ExecuteTypedASTScalarProviderRaw(
		const int32 A, const int32 B)
	{
		const asJITFunction_Raw UntypedEntry =
			TypedASTScalarProviderRawEntry.load(std::memory_order_acquire);
		checkf(UntypedEntry != nullptr,
			TEXT("The generated TypedASTJIT scalar raw entry is not registered."));
		const FTypedASTScalarProviderRawEntry Entry =
			reinterpret_cast<FTypedASTScalarProviderRawEntry>(UntypedEntry);
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		return Entry(Execution, static_cast<asDWORD>(A),
			static_cast<asDWORD>(B));
	}

	int32 ExecuteTypedASTScalarProviderVM(
		const int32 A, const int32 B)
	{
		const asJITFunction Entry =
			TypedASTScalarProviderVMEntry.load(std::memory_order_acquire);
		checkf(Entry != nullptr,
			TEXT("The generated TypedASTJIT scalar VM entry is not registered."));
		asDWORD Frame[] = {
			static_cast<asDWORD>(A),
			static_cast<asDWORD>(B),
		};
		asQWORD OutValue = 0;
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Entry(Execution, Frame, &OutValue);
		return static_cast<int32>(static_cast<asDWORD>(OutValue));
	}

	int32 ExecuteTypedASTScalarProviderParms(
		const int32 A, const int32 B)
	{
		const asJITFunction_ParmsEntry Entry =
			TypedASTScalarProviderParmsEntry.load(std::memory_order_acquire);
		checkf(Entry != nullptr,
			TEXT("The generated TypedASTJIT scalar Parms entry is not registered."));
		struct FParms
		{
			int32 A;
			int32 B;
			int32 ReturnValue;
		};
		FParms Parms{A, B, 0};
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Entry(Execution, nullptr, &Parms);
		return Parms.ReturnValue;
	}

	void RegisterTypedASTScalarProviderEntries(
		const asJITFunction VMEntry,
		const asJITFunction_Raw RawEntry,
		const asJITFunction_ParmsEntry ParmsEntry)
	{
		TypedASTScalarProviderRawEntry.store(
			RawEntry, std::memory_order_relaxed);
		TypedASTScalarProviderParmsEntry.store(
			ParmsEntry, std::memory_order_relaxed);
		TypedASTScalarProviderVMEntry.store(
			VMEntry, std::memory_order_release);
	}

	bool HasTypedASTCapabilityProviderEntries()
	{
		return TypedASTCapabilityProviderVMEntry.load(std::memory_order_acquire)
				!= nullptr
			&& TypedASTCapabilityProviderRawEntry.load(std::memory_order_acquire)
				!= nullptr
			&& TypedASTCapabilityProviderParmsEntry.load(std::memory_order_acquire)
				!= nullptr;
	}

	int32 ExecuteTypedASTCapabilityProviderRaw(
		const int32 A,
		const int32 B,
		const bool bEnabled,
		const bool bExtraCheck)
	{
		const asJITFunction_Raw UntypedEntry =
			TypedASTCapabilityProviderRawEntry.load(std::memory_order_acquire);
		checkf(UntypedEntry != nullptr,
			TEXT("The generated TypedASTJIT capability raw entry is not registered."));
		const FTypedASTCapabilityProviderRawEntry Entry =
			reinterpret_cast<FTypedASTCapabilityProviderRawEntry>(UntypedEntry);
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		return Entry(
			Execution,
			static_cast<asDWORD>(A),
			static_cast<asDWORD>(B),
			static_cast<asBYTE>(bEnabled),
			static_cast<asBYTE>(bExtraCheck));
	}

	int32 ExecuteTypedASTCapabilityProviderVM(
		const int32 A,
		const int32 B,
		const bool bEnabled,
		const bool bExtraCheck)
	{
		const asJITFunction Entry =
			TypedASTCapabilityProviderVMEntry.load(std::memory_order_acquire);
		checkf(Entry != nullptr,
			TEXT("The generated TypedASTJIT capability VM entry is not registered."));
		asDWORD Frame[] = {
			static_cast<asDWORD>(A),
			static_cast<asDWORD>(B),
			static_cast<asDWORD>(bEnabled),
			static_cast<asDWORD>(bExtraCheck),
		};
		asQWORD OutValue = 0;
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Entry(Execution, Frame, &OutValue);
		return static_cast<int32>(static_cast<asDWORD>(OutValue));
	}

	int32 ExecuteTypedASTCapabilityProviderParms(
		const int32 A,
		const int32 B,
		const bool bEnabled,
		const bool bExtraCheck)
	{
		const asJITFunction_ParmsEntry Entry =
			TypedASTCapabilityProviderParmsEntry.load(std::memory_order_acquire);
		checkf(Entry != nullptr,
			TEXT("The generated TypedASTJIT capability Parms entry is not registered."));
		struct FParms
		{
			int32 A;
			int32 B;
			bool bEnabled;
			bool bExtraCheck;
			void* GeneratedWorldContext;
			int32 ReturnValue;
		};
		FParms Parms{
			A, B, bEnabled, bExtraCheck, nullptr, 0};
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Entry(Execution, nullptr, &Parms);
		return Parms.ReturnValue;
	}

	void RegisterTypedASTCapabilityProviderEntries(
		const asJITFunction VMEntry,
		const asJITFunction_Raw RawEntry,
		const asJITFunction_ParmsEntry ParmsEntry)
	{
		TypedASTCapabilityProviderRawEntry.store(
			RawEntry, std::memory_order_relaxed);
		TypedASTCapabilityProviderParmsEntry.store(
			ParmsEntry, std::memory_order_relaxed);
		TypedASTCapabilityProviderVMEntry.store(
			VMEntry, std::memory_order_release);
	}

	bool HasIsolatedBytecodeDifferentialEntries()
	{
		return HasIsolatedDifferentialEntries(
			IsolatedBytecodeDifferentialEntries);
	}

	bool HasIsolatedTypedASTDifferentialEntries()
	{
		return HasIsolatedDifferentialEntries(
			IsolatedTypedASTDifferentialEntries);
	}

	FString GetIsolatedBytecodeDifferentialSymbolPrefix()
	{
		return GetIsolatedDifferentialSymbolPrefix(
			IsolatedBytecodeDifferentialEntries);
	}

	FString GetIsolatedTypedASTDifferentialSymbolPrefix()
	{
		return GetIsolatedDifferentialSymbolPrefix(
			IsolatedTypedASTDifferentialEntries);
	}

	FString GetIsolatedBytecodeDifferentialFunctionKey()
	{
		return GetIsolatedDifferentialFunctionKey(
			IsolatedBytecodeDifferentialEntries);
	}

	FString GetIsolatedTypedASTDifferentialFunctionKey()
	{
		return GetIsolatedDifferentialFunctionKey(
			IsolatedTypedASTDifferentialEntries);
	}

	bool FindIsolatedDifferentialEntry(
		const EIsolatedDifferentialBackend Backend,
		const FStringView CanonicalDeclaration,
		FIsolatedDifferentialEntryView& OutEntry)
	{
		FScopeLock Lock(&GetIsolatedDifferentialRegistryMutex());
		const FIsolatedDifferentialEntryView* const Entry =
			GetIsolatedDifferentialRegistry().FindByPredicate(
				[Backend, CanonicalDeclaration](const auto& Candidate)
				{
					return Candidate.Backend == Backend
						&& FStringView(Candidate.CanonicalDeclaration)
							.Equals(CanonicalDeclaration,
								ESearchCase::CaseSensitive);
				});
		if (Entry == nullptr)
		{
			OutEntry = {};
			return false;
		}
		OutEntry = *Entry;
		return true;
	}

	void RegisterIsolatedDifferentialEntry(
		const EIsolatedDifferentialBackend Backend,
		const asJITFunction VMEntry,
		const asJITFunction_Raw RawEntry,
		const asJITFunction_ParmsEntry ParmsEntry,
		const TCHAR* const SymbolPrefix,
		const TCHAR* const StableFunctionKey,
		const TCHAR* const CanonicalDeclaration,
		const TCHAR* const SourceSection,
		const uint32 SourceLine,
		const uint32 SourceColumn,
		const EIsolatedDifferentialCleanupPlanState CleanupPlanState,
		const uint32 CleanupActionCount,
		const FAngelscriptJITReferenceSlot* const ReferenceSlots,
		const uint32 ReferenceSlotCount)
	{
		check(VMEntry != nullptr);
		check(RawEntry != nullptr);
		check(SymbolPrefix != nullptr);
		check(StableFunctionKey != nullptr);
		check(CanonicalDeclaration != nullptr);
		check(SourceSection != nullptr);
		check(CleanupPlanState
			== EIsolatedDifferentialCleanupPlanState::NotCaptured
			|| CleanupPlanState
				== EIsolatedDifferentialCleanupPlanState::VerifiedEmpty);
		check(CleanupPlanState
			!= EIsolatedDifferentialCleanupPlanState::VerifiedEmpty
			|| CleanupActionCount == 0);
		check((ReferenceSlots == nullptr) == (ReferenceSlotCount == 0));
		if (FCString::Strcmp(
				CanonicalDeclaration,
				TEXT("int DifferentialScalarValue(const int, const int)")) == 0)
		{
			check(ParmsEntry != nullptr);
			FIsolatedDifferentialEntrySet& CompatibilityEntries =
				Backend == EIsolatedDifferentialBackend::BytecodeJIT
					? IsolatedBytecodeDifferentialEntries
					: IsolatedTypedASTDifferentialEntries;
			RegisterIsolatedDifferentialEntries(
				CompatibilityEntries,
				VMEntry,
				RawEntry,
				ParmsEntry,
				SymbolPrefix,
				StableFunctionKey);
		}

		FScopeLock Lock(&GetIsolatedDifferentialRegistryMutex());
		TArray<FIsolatedDifferentialEntryView>& Registry =
			GetIsolatedDifferentialRegistry();
		FIsolatedDifferentialEntryView* const Existing =
			Registry.FindByPredicate(
				[Backend, StableFunctionKey](const auto& Candidate)
				{
					return Candidate.Backend == Backend
						&& Candidate.StableFunctionKey
							.Equals(StableFunctionKey,
								ESearchCase::CaseSensitive);
				});
		if (Existing != nullptr)
		{
			check(Existing->VMEntry == VMEntry);
			check(Existing->RawEntry == RawEntry);
			check(Existing->ParmsEntry == ParmsEntry);
			check(Existing->SymbolPrefix.Equals(
				SymbolPrefix, ESearchCase::CaseSensitive));
			check(Existing->CanonicalDeclaration.Equals(
				CanonicalDeclaration, ESearchCase::CaseSensitive));
			check(Existing->SourceSection.Equals(
				SourceSection, ESearchCase::CaseSensitive));
			check(Existing->SourceLine == SourceLine);
			check(Existing->SourceColumn == SourceColumn);
			check(Existing->CleanupPlanState == CleanupPlanState);
			check(Existing->CleanupActionCount == CleanupActionCount);
			check(Existing->ReferenceSlots.Num()
				== static_cast<int32>(ReferenceSlotCount));
			for (uint32 ReferenceIndex = 0;
				ReferenceIndex < ReferenceSlotCount; ++ReferenceIndex)
			{
				const FAngelscriptJITReferenceSlot& ExistingSlot =
					Existing->ReferenceSlots[ReferenceIndex];
				const FAngelscriptJITReferenceSlot& IncomingSlot =
					ReferenceSlots[ReferenceIndex];
				check(ExistingSlot.SlotIndex == IncomingSlot.SlotIndex);
				check(ExistingSlot.Flags == IncomingSlot.Flags);
				check(ExistingSlot.Reference == IncomingSlot.Reference);
			}
			return;
		}

		FIsolatedDifferentialEntryView& Entry = Registry.AddDefaulted_GetRef();
		Entry.Backend = Backend;
		Entry.VMEntry = VMEntry;
		Entry.RawEntry = RawEntry;
		Entry.ParmsEntry = ParmsEntry;
		Entry.SymbolPrefix = SymbolPrefix;
		Entry.StableFunctionKey = StableFunctionKey;
		Entry.CanonicalDeclaration = CanonicalDeclaration;
		Entry.SourceSection = SourceSection;
		Entry.SourceLine = SourceLine;
		Entry.SourceColumn = SourceColumn;
		Entry.CleanupPlanState = CleanupPlanState;
		Entry.CleanupActionCount = CleanupActionCount;
		if (ReferenceSlotCount != 0)
		{
			Entry.ReferenceSlots.Append(
				ReferenceSlots, static_cast<int32>(ReferenceSlotCount));
		}
	}

	asJITFunction_Raw GetIsolatedBytecodeDifferentialRawEntry()
	{
		return IsolatedBytecodeDifferentialEntries.RawEntry.load(
			std::memory_order_acquire);
	}

	asJITFunction_Raw GetIsolatedTypedASTDifferentialRawEntry()
	{
		return IsolatedTypedASTDifferentialEntries.RawEntry.load(
			std::memory_order_acquire);
	}

	int32 ExecuteIsolatedBytecodeDifferentialRaw(
		const int32 A, const int32 B)
	{
		return ExecuteIsolatedDifferentialRaw(
			IsolatedBytecodeDifferentialEntries, A, B);
	}

	int32 ExecuteIsolatedBytecodeDifferentialVM(
		const int32 A, const int32 B)
	{
		return ExecuteIsolatedDifferentialVM(
			IsolatedBytecodeDifferentialEntries, A, B);
	}

	int32 ExecuteIsolatedBytecodeDifferentialParms(
		const int32 A, const int32 B)
	{
		return ExecuteIsolatedDifferentialParms(
			IsolatedBytecodeDifferentialEntries, A, B);
	}

	int32 ExecuteIsolatedTypedASTDifferentialRaw(
		const int32 A, const int32 B)
	{
		return ExecuteIsolatedDifferentialRaw(
			IsolatedTypedASTDifferentialEntries, A, B);
	}

	int32 ExecuteIsolatedTypedASTDifferentialVM(
		const int32 A, const int32 B)
	{
		return ExecuteIsolatedDifferentialVM(
			IsolatedTypedASTDifferentialEntries, A, B);
	}

	int32 ExecuteIsolatedTypedASTDifferentialParms(
		const int32 A, const int32 B)
	{
		return ExecuteIsolatedDifferentialParms(
			IsolatedTypedASTDifferentialEntries, A, B);
	}

	void RegisterIsolatedBytecodeDifferentialEntries(
		const asJITFunction VMEntry,
		const asJITFunction_Raw RawEntry,
		const asJITFunction_ParmsEntry ParmsEntry,
		const TCHAR* const SymbolPrefix,
		const TCHAR* const StableFunctionKey,
		const TCHAR* const CanonicalDeclaration)
	{
		RegisterIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::BytecodeJIT,
			VMEntry, RawEntry, ParmsEntry, SymbolPrefix,
			StableFunctionKey, CanonicalDeclaration);
		RegisterIsolatedDifferentialEntries(
			IsolatedBytecodeDifferentialEntries,
			VMEntry, RawEntry, ParmsEntry, SymbolPrefix, StableFunctionKey);
	}

	void RegisterIsolatedTypedASTDifferentialEntries(
		const asJITFunction VMEntry,
		const asJITFunction_Raw RawEntry,
		const asJITFunction_ParmsEntry ParmsEntry,
		const TCHAR* const SymbolPrefix,
		const TCHAR* const StableFunctionKey,
		const TCHAR* const CanonicalDeclaration)
	{
		RegisterIsolatedDifferentialEntry(
			EIsolatedDifferentialBackend::TypedASTJIT,
			VMEntry, RawEntry, ParmsEntry, SymbolPrefix,
			StableFunctionKey, CanonicalDeclaration);
		RegisterIsolatedDifferentialEntries(
			IsolatedTypedASTDifferentialEntries,
			VMEntry, RawEntry, ParmsEntry, SymbolPrefix, StableFunctionKey);
	}

	int32 ExecuteTypedASTNativeBridge(
		FScriptExecution& Execution,
		const FAngelscriptTypedASTJITBoundCallSite& CallSite,
		const int32 Left,
		const int32 Right)
	{
		// This template instantiation lives in AngelscriptTestJIT.dll. Its fixed
		// non-template core is imported from AngelscriptRuntime.dll.
		return AngelscriptTypedASTJIT::InvokeBound<int32, int32, int32>(
			Execution, CallSite, Left, Right);
	}
}
