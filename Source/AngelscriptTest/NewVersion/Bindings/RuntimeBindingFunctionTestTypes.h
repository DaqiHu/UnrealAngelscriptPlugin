#pragma once

#include "CoreMinimal.h"
#include "RuntimeBindingFunctionTestTypes.generated.h"

USTRUCT(BlueprintType)
struct FRuntimeBindingFunctionPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString Text;
};

UCLASS()
class URuntimeBindingFunctionArgument : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class URuntimeBindingFunctionFixtureParent : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	int32 Add(int32 A, int32 B = 3) const { return A + B; }

	UFUNCTION(BlueprintCallable)
	FString MakeString(const FString& Prefix) const { return Prefix + TEXT("-owned"); }

	UFUNCTION(BlueprintCallable)
	FRuntimeBindingFunctionPayload MakePayload(const FString& Text) const { return {Text}; }

	UFUNCTION(BlueprintCallable)
	void Fill(int32& OutValue, UPARAM(ref) int32& InOutValue) const { OutValue = 7; InOutValue += 5; }

	UFUNCTION(BlueprintCallable)
	bool AcceptArgument(URuntimeBindingFunctionArgument* Argument) const { return Argument != nullptr; }
};

UCLASS()
class URuntimeBindingFunctionFixtureChild : public URuntimeBindingFunctionFixtureParent
{
	GENERATED_BODY()
};
