#include "AngelscriptBinds.h"

#include "CoreGlobals.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallDescriptor.h"
#include "UObject/Class.h"

namespace
{
#if AS_CAN_GENERATE_JIT
	static void AttachReviewedHeaderInlineScalarCall(
		FAngelscriptBoundFunction& BoundFunction,
		const TCHAR* const Symbol,
		const TCHAR* const Include,
		const FName OwningModule)
	{
		if (!BoundFunction.IsValid()
			|| BoundFunction.GetFunction() == nullptr
			|| !BoundFunction.GetTargetEngine().
				bCollectStaticJITCompatibilityBinds)
		{
			return;
		}

		FAngelscriptStaticJITNativeCallDescriptor Descriptor;
		Descriptor.Linkage =
			EAngelscriptStaticJITNativeCallLinkage::HeaderInline;
		Descriptor.Visibility =
			EAngelscriptStaticJITNativeCallVisibility::HeaderInline;
		Descriptor.Symbol = Symbol;
		Descriptor.Include = Include;
		Descriptor.OwningModule = OwningModule;
		Descriptor.CppCallableSignature = TEXT("bool()");
		Descriptor.ABIDomain =
			EAngelscriptStaticJITNativeCallABIDomain::Scalar;
		Descriptor.ExceptionPolicy =
			EAngelscriptStaticJITNativeCallExceptionPolicy::DoesNotSetScriptException;
		Descriptor.RouteFlags =
			EAngelscriptStaticJITNativeCallRouteFlags::DirectCall;
		Descriptor.LifetimeFlags =
			EAngelscriptStaticJITNativeCallLifetimeFlags::ByValueScalarOnly
			| EAngelscriptStaticJITNativeCallLifetimeFlags::NoRetainedReferences
			| EAngelscriptStaticJITNativeCallLifetimeFlags::NoEngineLocalCapture;
		if (TryBuildAngelscriptStaticJITNativeABIIdentity(
				*BoundFunction.GetFunction(), Descriptor.NativeABI)
			&& TryBuildAngelscriptStaticJITScalarABIIdentity(
				*BoundFunction.GetFunction(), Descriptor.ScalarABI))
		{
			BoundFunction.ExternalNativeCall(Descriptor);
		}
	}
#endif
}

/**
 * Unreal process-mode globals.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | bool IsRunningCommandlet();                                                              | Reports whether the process is executing any commandlet.                                                           |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | bool IsRunningCookCommandlet();                                                          | Reports whether the active commandlet is cooking content.                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | bool IsRunningDLCCookCommandlet();                                                       | Reports whether the active cook is producing DLC content.                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | UClass GetRunningCommandletClass();                                                      | Returns the running commandlet class, or null outside commandlet execution.                                        |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_CoreGlobals(
	TEXT("CoreGlobals"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		if (Binds.IsHostTarget()) return;
		FAngelscriptBoundFunction IsRunningCommandletFunction =
			Binds.BindGlobalFunctionForTarget(
				"bool IsRunningCommandlet() no_discard",
				&IsRunningCommandlet)
			.NativeFunction("IsRunningCommandlet", true);
#if AS_CAN_GENERATE_JIT
		AttachReviewedHeaderInlineScalarCall(
			IsRunningCommandletFunction,
			TEXT("IsRunningCommandlet"),
			TEXT("CoreGlobals.h"),
			TEXT("Core"));
#endif
		Binds.BindGlobalFunctionForTarget("bool IsRunningCookCommandlet() no_discard", &IsRunningCookCommandlet)
			.NativeFunction("IsRunningCookCommandlet", true);
		Binds.BindGlobalFunctionForTarget("bool IsRunningDLCCookCommandlet() no_discard", &IsRunningDLCCookCommandlet)
			.NativeFunction("IsRunningDLCCookCommandlet", true);
		Binds.BindGlobalFunctionForTarget("UClass GetRunningCommandletClass() no_discard", &GetRunningCommandletClass)
			.NativeFunction("GetRunningCommandletClass", true);
	});
