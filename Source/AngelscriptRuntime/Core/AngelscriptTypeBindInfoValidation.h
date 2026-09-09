#pragma once

#include "AngelscriptTypeBindInfo.h"

class FAngelscriptTypeBindInfoStore;

/** Deterministic, read-only validation result for one sealed Store. */
struct FAngelscriptTypeBindValidationResult
{
	bool bValid = false;
	EAngelscriptBindingStage Stage = EAngelscriptBindingStage::None;
	FString Message;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptTypeBindInfoValidation final
{
public:
	static FAngelscriptTypeBindValidationResult ValidateSealed(
		TSharedRef<const FAngelscriptTypeBindInfoStore> Store);
};
