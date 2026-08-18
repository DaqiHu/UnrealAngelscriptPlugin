#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"

#if AS_CAN_GENERATE_JIT

#include "AngelscriptEngine.h"

void FAngelscriptStaticJITGenerationProfile::ApplyToEngineConfig(
	FAngelscriptEngineConfig& InOutConfig) const
{
	InOutConfig.Purpose = EAngelscriptEnginePurpose::StaticJITGeneration;
	InOutConfig.bCollectStaticJITCompatibilityBinds = true;
	InOutConfig.bCaptureTypedSemanticIR =
		RequiresTypedSemanticIRCapture();
}

#endif
