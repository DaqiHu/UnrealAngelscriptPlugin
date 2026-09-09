#pragma once

#include "CoreMinimal.h"
#include "RuntimeBindingDelegateTestTypes.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FRuntimeBindingSingleDelegate, int32, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRuntimeBindingMulticastDelegate, int32, Value);

USTRUCT()
struct FRuntimeBindingDelegatePayload
{
	GENERATED_BODY()

	static int32 LiveCopies;
	static int32 Destructions;

	UPROPERTY()
	int32 Value = 0;

	FRuntimeBindingDelegatePayload() = default;
	explicit FRuntimeBindingDelegatePayload(int32 InValue) : Value(InValue) {}
	FRuntimeBindingDelegatePayload(const FRuntimeBindingDelegatePayload& Other) : Value(Other.Value) { ++LiveCopies; }
	~FRuntimeBindingDelegatePayload() { ++Destructions; }
};

UCLASS()
class URuntimeBindingDelegateReceiver : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FRuntimeBindingSingleDelegate Single;

	UPROPERTY()
	FRuntimeBindingMulticastDelegate Multicast;

	UPROPERTY()
	int32 Total = 0;

	UPROPERTY()
	int32 Calls = 0;

	UFUNCTION()
	void OnValue(int32 Value) { Total += Value; ++Calls; }

	UFUNCTION()
	void OnPayload(FRuntimeBindingDelegatePayload Payload) { Total += Payload.Value; ++Calls; }

	UFUNCTION()
	void WrongSignature(float Value) { Total += FMath::RoundToInt(Value); }

	UFUNCTION(BlueprintNativeEvent)
	void ReflectedEvent(int32 Input, int32& Output);
	virtual void ReflectedEvent_Implementation(int32 Input, int32& Output) { Output = Input + 1; }
};
