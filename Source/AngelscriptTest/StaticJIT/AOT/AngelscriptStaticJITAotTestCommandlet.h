#pragma once

#include "Commandlets/Commandlet.h"

#include "AngelscriptStaticJITAotTestCommandlet.generated.h"

UCLASS()
class UAngelscriptTestJITCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
