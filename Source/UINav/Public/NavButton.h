#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NavButton.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNavButtonPressed);

UCLASS(Abstract)
class UINAV_API UNavButton : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Nav Button")
	void Press();

	UPROPERTY(BlueprintAssignable, Category = "Nav Button")
	FOnNavButtonPressed OnPressed;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Nav Button")
	void OnFocusVisualChanged(bool bFocused);

	virtual void NativeOnInitialized() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Nav Button", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button;
};
