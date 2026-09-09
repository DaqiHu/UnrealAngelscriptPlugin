#include "CoreMinimal.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#if WITH_EDITOR

/**
 * Reflected deprecation contribution; this registrar adds metadata and declares no new callable.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | NiagaraComponent.SetNiagaraVariable*(...);                                               | Marks the legacy typed-name Niagara setters deprecated; use the corresponding SetVariable(FName, ...) APIs.        |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_Deprecations(
	TEXT("Deprecations"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		static const TCHAR* Paths[] = {
			TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableLinearColor"), TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableVec4"),
			TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableQuat"), TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableMatrix"),
			TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableVec3"), TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariablePosition"),
			TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableVec2"), TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableFloat"),
			TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableInt"), TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableBool"),
			TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableActor"), TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableObject")};
		for (const TCHAR* Path : Paths)
			Binds.AddDeprecationEffect(Path, TEXT("Use the SetVariable variant that takes FName instead"));
	});

#endif
