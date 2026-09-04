#pragma once

#include "CoreMinimal.h"

// Narrow, reviewed Runtime callables that may be imported by generated
// StaticJIT consumer modules. Binding registration must use these exact
// functions so VM and generated C++ share one implementation and symbol.
namespace AngelscriptStaticJITNativeCallables
{
	ANGELSCRIPTRUNTIME_API void Print(
		const FString& Text,
		float Duration,
		FLinearColor Color);

#if WITH_ANGELSCRIPT_UNITTESTS
	/** Test-only exported wrapper used to exercise the RuntimeThunk AOT route. */
	ANGELSCRIPTRUNTIME_API int32 RuntimeThunkProbe(const FString& Text);
	ANGELSCRIPTRUNTIME_API void ResetRuntimeThunkProbeCountForTesting();
	ANGELSCRIPTRUNTIME_API int32 GetRuntimeThunkProbeCountForTesting();
	ANGELSCRIPTRUNTIME_API void ResetPrintCallCountForTesting();
	ANGELSCRIPTRUNTIME_API int32 GetPrintCallCountForTesting();
#else
	// Generated legacy TestJIT artifacts retain this narrow probe ABI even when
	// their owning tests are compiled out. Keep inert inline definitions so the
	// generated sources remain buildable without rewriting generated files.
	inline int32 RuntimeThunkProbe(const FString& Text)
	{
		return Text.Len() * 3 + 5;
	}

	inline void ResetRuntimeThunkProbeCountForTesting()
	{
	}

	inline int32 GetRuntimeThunkProbeCountForTesting()
	{
		return 0;
	}

	inline void ResetPrintCallCountForTesting()
	{
	}

	inline int32 GetPrintCallCountForTesting()
	{
		return 0;
	}
#endif
}
