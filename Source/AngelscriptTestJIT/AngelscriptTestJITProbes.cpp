#include "AngelscriptTestJITProbes.h"

#include <atomic>

namespace AngelscriptTestJITProbes
{
	namespace
	{
		std::atomic<int32> CallCount{0};
		std::atomic<int32> LastValue{0};
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
}
