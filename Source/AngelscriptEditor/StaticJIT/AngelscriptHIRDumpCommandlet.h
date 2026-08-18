#pragma once

#include "Commandlets/Commandlet.h"

#include "AngelscriptHIRDumpCommandlet.generated.h"

/** Dedicated developer/test HIR snapshot entry point; not a StaticJIT mode. */
UCLASS()
class ANGELSCRIPTEDITOR_API UAngelscriptHIRDumpCommandlet final
	: public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
