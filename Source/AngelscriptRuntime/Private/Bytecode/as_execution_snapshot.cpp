#include "as_execution_snapshot.h"
#include "as_scriptengine.h"
#include "as_typeinfo.h"

BEGIN_AS_NAMESPACE

asCExecutableFunction::asCExecutableFunction() = default;

asCExecutableFunction::~asCExecutableFunction()
{
	if (scriptData)
	{
		typedef asCScriptFunction::ScriptFunctionData ScriptData;
		asDELETE(scriptData, ScriptData);
		scriptData = nullptr;
	}
	if (sysFuncIntf)
	{
		asDELETE(sysFuncIntf, asSSystemFunctionInterface);
		sysFuncIntf = nullptr;
	}
	if (Declaration)
	{
		Declaration->Release();
		Declaration = nullptr;
	}
}

void asCExecutableFunction::AllocateScriptData()
{
	if (scriptData) return;
	typedef asCScriptFunction::ScriptFunctionData ScriptData;
	scriptData = asNEW(ScriptData);
	scriptData->stackNeeded = 0;
	scriptData->variableSpace = 0;
	scriptData->scriptSectionIdx = -1;
	scriptData->declaredAt = 0;
}

asCExecutableSnapshot::~asCExecutableSnapshot()
{
	asCScriptEngine* Publisher = Engine.exchange(nullptr);
	if (Publisher)
	{
		Publisher->DetachPublishedSnapshot(this);
		Executables.Reset();
		for (auto& Pair : Functions)
		{
			if (Pair.Value) Pair.Value->Release();
		}
		for (auto& Pair : Types)
		{
			if (Pair.Value) Pair.Value->Release();
		}
	}
	else
	{
		// Retirement already dropped the publisher and may have asDELETE'd
		// declaration objects. Do not Release them again.
		for (auto& Pair : Executables)
		{
			if (Pair.Value)
			{
				Pair.Value->Declaration = nullptr;
			}
		}
		Executables.Reset();
	}
	Functions.Empty();
	Types.Empty();
}

END_AS_NAMESPACE
