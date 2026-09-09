#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RuntimeBindingMixinTestTypes.generated.h"

UCLASS(BlueprintType)
class URuntimeBindingMixinTarget : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 Value = 0;
};

UCLASS()
class URuntimeBindingStaticLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	static void SetSelected(URuntimeBindingMixinTarget* Selected, int32 Value);

	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"))
	static bool ObserveWorld(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, meta=(DeterminesOutputType="RequestedClass"))
	static URuntimeBindingMixinTarget* SelectType(UClass* RequestedClass);
};

UCLASS(meta=(ScriptMixin="URuntimeBindingMixinTarget"))
class URuntimeBindingMixinLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	static void AddToSelected(URuntimeBindingMixinTarget* Target, int32 Delta);
};
