#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "UObject/Interface.h"
#include "UINavConsumer.generated.h"

UINTERFACE(BlueprintType)
class UUINavConsumer : public UInterface
{
	GENERATED_BODY()
};

class UINAV_API IUINavConsumer
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI")
	bool ConsumeNavigation(EUINavigation Direction);
};
