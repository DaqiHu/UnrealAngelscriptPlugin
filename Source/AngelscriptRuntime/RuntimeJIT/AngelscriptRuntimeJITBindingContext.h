#pragma once

#include "CoreMinimal.h"

#include "RuntimeJIT/AngelscriptRuntimeJITState.h"

/**
 * Engine-local immutable Runtime binding payload. The published code lease
 * remains alive until the maintained fork releases the last active reader of
 * the retired Binding.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITBindingContext final
{
public:
	explicit FAngelscriptRuntimeJITBindingContext(
		const FAngelscriptRuntimeJITPublishedCode& InPublished);
	~FAngelscriptRuntimeJITBindingContext();

	bool IsValid() const;
	asSJITFunctionBinding BuildBinding() const;

	const FAngelscriptRuntimeJITPublishedCode& GetPublished() const
	{
		return Published;
	}

	uint64 GetExecutionCount() const
	{
		return ExecutionCount.Load();
	}

	static const FAngelscriptRuntimeJITBindingContext* FromBindingUserData(
		const void* UserData);

private:
	static void InvokeRuntimeEntry(
		FScriptExecution& Execution,
		asDWORD* FramePointer,
		asQWORD* OutValue);

	static constexpr uint64 LiveMagic = 0x415352544A495442ull; // ASRTJITB
	uint64 Magic = LiveMagic;
	FAngelscriptRuntimeJITPublishedCode Published;
	mutable TAtomic<uint64> ExecutionCount { 0 };
};
