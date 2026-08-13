#pragma once

#include "Commandlets/Commandlet.h"

#include "AngelscriptJITCommandlet.generated.h"

UCLASS()
class ANGELSCRIPTEDITOR_API UAngelscriptJITCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
