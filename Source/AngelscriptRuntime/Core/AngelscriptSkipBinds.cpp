#include "CoreMinimal.h"
#include "AngelscriptBinds.h"

namespace
{
	void BindDefaultSkipConfiguration(FAngelscriptBinds& Binds)
	{
		Binds.AddSkipFunctionEffect(
			FName(TEXT("StaticMesh")),
			FName(TEXT("GetMinLODForQualityLevels")));
		Binds.AddSkipFunctionEffect(
			FName(TEXT("StaticMesh")),
			FName(TEXT("SetMinLODForQualityLevels")));
		Binds.AddSkipFunctionEffect(
			FName(TEXT("SkeletalMesh")),
			FName(TEXT("GetMinLODForQualityLevels")));
		Binds.AddSkipFunctionEffect(
			FName(TEXT("SkeletalMesh")),
			FName(TEXT("SetMinLODForQualityLevels")));
		Binds.AddSkipFunctionEffect(
			FName(TEXT("SourceEffectEQPreset")),
			FName(TEXT("SetSettings")));
		Binds.AddSkipClassEffect(FName(TEXT("ClothingSimulationInteractorNv")));
		Binds.AddSkipClassEffect(FName(TEXT("NiagaraPreviewGrid")));
		Binds.AddSkipClassEffect(FName(TEXT("GameplayCamerasSubsystem")));
		Binds.AddSkipClassEffect(FName(TEXT("AsyncAction_PerformTargeting")));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_Skip(
	TEXT("SkipBinds.Defaults"),
	EAngelscriptBindPhase::ExplicitBindings,
	&BindDefaultSkipConfiguration);
