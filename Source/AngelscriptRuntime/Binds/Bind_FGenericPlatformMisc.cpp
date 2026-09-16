#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptBinds.h"

namespace
{
	void RequestExit(bool bForce)
	{
		FGenericPlatformMisc::RequestExit(bForce, nullptr);
	}
}

/**
 * Generic platform process control.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void FGenericPlatformMisc::RequestExit(bool Force);                                      | Requests process shutdown; Force selects immediate rather than graceful exit.                                      |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FGenericPlatformMisc(
	TEXT("FGenericPlatformMisc"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		if (Binds.IsHostTarget()) return;
		FAngelscriptBinds::FNamespace Namespace(Binds, "FGenericPlatformMisc");
		Binds.BindGlobalFunctionForTarget("void RequestExit(bool Force)", &RequestExit);
	});
