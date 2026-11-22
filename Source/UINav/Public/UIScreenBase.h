#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputTriggers.h"
#include "UIScreenBase.generated.h"

class UInputAction;
class UInputMappingContext;

UENUM(BlueprintType)
enum class EUIInputPolicy : uint8
{
	GameOnly,
	GameAndMenu,
	MenuOnly
};

UCLASS(Abstract)
class UINAV_API UUIScreenBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Screen")
	EUIInputPolicy InputPolicy = EUIInputPolicy::MenuOnly;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Screen")
	bool bShowCursor = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Screen")
	bool bCloseOnBack = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Screen")
	bool bRestoreLastFocus = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Screen")
	TArray<TObjectPtr<UInputMappingContext>> ExtraContexts;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Screen")
	UWidget* GetDesiredFocusTarget() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Screen")
	bool HandleUIAction(const UInputAction* Action, ETriggerEvent Event);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Screen")
	bool HandleBack();

	UFUNCTION(BlueprintCallable, Category = "Screen")
	void NotifyChildFocused(UWidget* Widget);

protected:
	friend class UUIManager;

	virtual void NativeOnInitialized() override;

	TWeakObjectPtr<UWidget> LastFocused;
};
