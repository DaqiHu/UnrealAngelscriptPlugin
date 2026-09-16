#pragma once

#include "CoreMinimal.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptBindWriteWorkers
{
	static int32 ResolveCount();
	static bool RunClassWave(int32 ItemCount, TFunctionRef<void(int32 Index)> Work);

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetProbe();
	static int32 GetMaxActiveWorkers();
	static int32 GetClaimedItems();
	static int32 GetActiveWorkers();
	static int32 GetGlobalWritesDuringActiveWorkers();
	static void NoteGlobalWrite();
	static void SetFailAtIndex(int32 Index);
	static void SetHoldForOverlap(bool bHold);
#endif
};
