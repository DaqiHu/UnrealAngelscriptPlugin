#pragma once

/**
 * Standalone keeps the maintained fork's original VM-entry behavior without
 * importing the UE Runtime StaticJIT execution scope or reference resolver.
 * The caller already installed the current Engine, function, and retained
 * binding snapshot in FScriptExecution before reaching this compatibility
 * seam.
 */
class FAngelscriptJITExecutionContext final
{
public:
	static bool InvokeVM(
		FScriptExecution& Execution,
		asIScriptEngine*,
		asIScriptFunction* Function,
		const asSJITFunctionBinding& Binding,
		asDWORD* FramePointer,
		asQWORD* OutValue)
	{
		if (Function == nullptr || Binding.VMEntry == nullptr)
		{
			return false;
		}

		Binding.VMEntry(Execution, FramePointer, OutValue);
		return true;
	}
};
