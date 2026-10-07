#include "AngelscriptBinds.h"
#include "Core/FunctionCallers.h"

#include "FunctionLibraries/InputComponentScriptMixinLibrary.h"

/**
 * UPlayerInput generated-binding overrides.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void PlayerInput.AddActionMapping(const FInputActionKeyMapping& Mapping);                | Adds one action-key mapping. @param Mapping Action name, key, and modifier-key settings.                           |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void PlayerInput.AddAxisMapping(const FInputAxisKeyMapping& Mapping);                    | Adds one axis-key mapping. @param Mapping Axis name, key, and scale.                                               |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void PlayerInput.RemoveActionMapping(const FInputActionKeyMapping& Mapping);             | Removes the matching action-key mapping.                                                                           |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void PlayerInput.RemoveAxisMapping(const FInputAxisKeyMapping& Mapping);                 | Removes the matching axis-key mapping.                                                                             |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_InputComponentScriptMixins(
	TEXT("InputComponentScriptMixins.GeneratedOverrides"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		// UHT marks these wrappers overloaded-unresolved. Install the exact signatures
		// in the selected engine before GeneratedBindings falls back to reflection.
		Binds.RegisterFunctionBindingForTarget(
			UPlayerInputScriptMixinLibrary::StaticClass(),
			"AddActionMapping",
			{TDPLUGIN_ERASE_FUNCTION_PTR(UPlayerInputScriptMixinLibrary::AddActionMapping, (UPlayerInput*, const FInputActionKeyMapping&), TDPLUGIN_ERASE_ARGUMENT_PACK(void))});
		Binds.RegisterFunctionBindingForTarget(
			UPlayerInputScriptMixinLibrary::StaticClass(),
			"AddAxisMapping",
			{TDPLUGIN_ERASE_FUNCTION_PTR(UPlayerInputScriptMixinLibrary::AddAxisMapping, (UPlayerInput*, const FInputAxisKeyMapping&), TDPLUGIN_ERASE_ARGUMENT_PACK(void))});
		Binds.RegisterFunctionBindingForTarget(
			UPlayerInputScriptMixinLibrary::StaticClass(),
			"RemoveActionMapping",
			{TDPLUGIN_ERASE_FUNCTION_PTR(UPlayerInputScriptMixinLibrary::RemoveActionMapping, (UPlayerInput*, const FInputActionKeyMapping&), TDPLUGIN_ERASE_ARGUMENT_PACK(void))});
		Binds.RegisterFunctionBindingForTarget(
			UPlayerInputScriptMixinLibrary::StaticClass(),
			"RemoveAxisMapping",
			{TDPLUGIN_ERASE_FUNCTION_PTR(UPlayerInputScriptMixinLibrary::RemoveAxisMapping, (UPlayerInput*, const FInputAxisKeyMapping&), TDPLUGIN_ERASE_ARGUMENT_PACK(void))});
	});
