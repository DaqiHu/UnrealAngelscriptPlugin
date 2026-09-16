#include "AngelscriptBindWriteWorkers.h"

#include "Async/ParallelFor.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

#include <atomic>

static TAutoConsoleVariable<int32> CVarBindWriteWorkers(
	TEXT("as.Bind.WriteWorkers"),
	1,
	TEXT("Creation/member write-wave worker count. Default 1. 0 is treated as 1. ")
	TEXT("N>=2 runs a fixed worker group that claims one UClass at a time with atomic Next++. ")
	TEXT("as.Bind.ParallelPrepare remains an independent prepare switch."),
	ECVF_Default);

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	std::atomic<int32> GActiveWorkers{0};
	std::atomic<int32> GMaxActiveWorkers{0};
	std::atomic<int32> GClaimedItems{0};
	std::atomic<int32> GFailAtIndex{-1};
	std::atomic<int32> GGlobalWritesDuringActive{0};
	std::atomic<bool> GHoldForOverlap{false};
}
#endif

int32 FAngelscriptBindWriteWorkers::ResolveCount()
{
	const int32 Requested = CVarBindWriteWorkers.GetValueOnAnyThread();
	return FMath::Max(1, Requested);
}

bool FAngelscriptBindWriteWorkers::RunClassWave(int32 ItemCount, TFunctionRef<void(int32 Index)> Work)
{
	if (ItemCount <= 0)
	{
		return true;
	}

	const int32 WorkerCount = ResolveCount();
	std::atomic<int32> Next{0};
	std::atomic<bool> bFailed{false};
#if WITH_DEV_AUTOMATION_TESTS
	std::atomic<int32> Arrived{0};
#endif

	auto WorkerBody = [&](int32)
	{
		for (;;)
		{
			const int32 Index = Next.fetch_add(1, std::memory_order_relaxed);
			if (Index >= ItemCount)
			{
				return;
			}
#if WITH_DEV_AUTOMATION_TESTS
			if (GFailAtIndex.load(std::memory_order_relaxed) == Index)
			{
				bFailed.store(true, std::memory_order_relaxed);
				return;
			}
			const int32 NowActive = GActiveWorkers.fetch_add(1, std::memory_order_relaxed) + 1;
			int32 ObservedMax = GMaxActiveWorkers.load(std::memory_order_relaxed);
			while (NowActive > ObservedMax
				&& !GMaxActiveWorkers.compare_exchange_weak(ObservedMax, NowActive, std::memory_order_relaxed))
			{
			}
			if (GHoldForOverlap.load(std::memory_order_relaxed))
			{
				const int32 Gate = FMath::Min(2, WorkerCount);
				Arrived.fetch_add(1, std::memory_order_relaxed);
				const double Deadline = FPlatformTime::Seconds() + 2.0;
				while (Arrived.load(std::memory_order_relaxed) < Gate
					&& FPlatformTime::Seconds() < Deadline)
				{
					FPlatformProcess::Sleep(0.0f);
				}
			}
			GClaimedItems.fetch_add(1, std::memory_order_relaxed);
#endif
			Work(Index);
#if WITH_DEV_AUTOMATION_TESTS
			GActiveWorkers.fetch_sub(1, std::memory_order_relaxed);
#endif
		}
	};

	if (WorkerCount <= 1 || ItemCount == 1)
	{
		WorkerBody(0);
	}
	else
	{
		ParallelFor(WorkerCount, WorkerBody, EParallelForFlags::Unbalanced);
	}
	return !bFailed.load(std::memory_order_relaxed);
}

#if WITH_DEV_AUTOMATION_TESTS
void FAngelscriptBindWriteWorkers::ResetProbe()
{
	GActiveWorkers.store(0);
	GMaxActiveWorkers.store(0);
	GClaimedItems.store(0);
	GFailAtIndex.store(-1);
	GGlobalWritesDuringActive.store(0);
	GHoldForOverlap.store(false);
}

int32 FAngelscriptBindWriteWorkers::GetMaxActiveWorkers()
{
	return GMaxActiveWorkers.load();
}

int32 FAngelscriptBindWriteWorkers::GetClaimedItems()
{
	return GClaimedItems.load();
}

int32 FAngelscriptBindWriteWorkers::GetActiveWorkers()
{
	return GActiveWorkers.load();
}

int32 FAngelscriptBindWriteWorkers::GetGlobalWritesDuringActiveWorkers()
{
	return GGlobalWritesDuringActive.load();
}

void FAngelscriptBindWriteWorkers::NoteGlobalWrite()
{
	if (GActiveWorkers.load(std::memory_order_relaxed) > 0)
	{
		GGlobalWritesDuringActive.fetch_add(1, std::memory_order_relaxed);
	}
}

void FAngelscriptBindWriteWorkers::SetFailAtIndex(int32 Index)
{
	GFailAtIndex.store(Index);
}

void FAngelscriptBindWriteWorkers::SetHoldForOverlap(bool bHold)
{
	GHoldForOverlap.store(bHold);
}
#endif
