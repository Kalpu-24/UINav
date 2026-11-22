#pragma once

#include "CoreMinimal.h"
#include "UINav/Public/NavButton.h"
#include "UINav/Public/UINavConsumer.h"
#include "NavOptionButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNavOptionChanged, int32, Value);

/** A NavButton whose value changes with left/right. Up/down still navigates. */
UCLASS(Abstract)
class UINAV_API UNavOptionButton : public UNavButton, public IUINavConsumer
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Nav Option")
	void SetValue(int32 NewValue);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nav Option")
	int32 Value = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nav Option")
	int32 MinValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nav Option")
	int32 MaxValue = 10;

	UPROPERTY(BlueprintAssignable, Category = "Nav Option")
	FOnNavOptionChanged OnValueChanged;

protected:
	virtual bool ConsumeNavigation_Implementation(EUINavigation Direction) override;
};
