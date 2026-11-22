#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/DataTable.h"
#include "InputCoreTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Types/SlateEnums.h"
#include "UIManager.generated.h"

class UDataTable;
class UEnhancedInputComponent;
class UTexture2D;
class UInputAction;
class UInputMappingContext;
class UUIScreenBase;
struct FInputActionInstance;

UENUM(BlueprintType)
enum class EUILayer : uint8
{
	HUD,
	GameMenu,
	Menu,
	Modal
};

USTRUCT()
struct FUIScreenStack
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UUIScreenBase>> Screens;
};

/** Row name is the key name, e.g. Gamepad_FaceButton_Bottom. */
USTRUCT(BlueprintType)
struct FUIKeyIconRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSoftObjectPtr<UTexture2D> Icon;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIPromptsChanged);

UCLASS(Config = Game)
class UINAV_API UUIManager : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UI")
	UUIScreenBase* PushScreen(EUILayer Layer, TSubclassOf<UUIScreenBase> ScreenClass);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void PushScreenAsync(EUILayer Layer, TSoftClassPtr<UUIScreenBase> ScreenClass);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void PopScreen(UUIScreenBase* Screen);

	UFUNCTION(BlueprintPure, Category = "UI")
	UUIScreenBase* GetInputOwner() const;

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsUsingGamepad() const { return bUsingGamepad; }

	UFUNCTION(BlueprintPure, Category = "UI")
	FKey GetPromptKey(const UInputAction* Action) const;

	UFUNCTION(BlueprintPure, Category = "UI")
	TSoftObjectPtr<UTexture2D> GetPromptIcon(const UInputAction* Action) const;

	/** Fires when the device type changes or the player remaps keys. Prompt widgets refresh on this. */
	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnUIPromptsChanged OnPromptsChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

private:
	UFUNCTION()
	void BroadcastPromptsChanged();

	void OnHardwareDeviceChanged(FPlatformUserId UserId, FInputDeviceId DeviceId);
	bool TickFocusWatchdog(float DeltaTime);
	void RecalculateInputOwner();
	void RestoreFocus(UUIScreenBase* Screen);
	void Navigate(EUINavigation Direction);
	void BindForward(UEnhancedInputComponent& Input, const UInputMappingContext* Context, TSet<const UInputAction*>& Seen, TArray<uint32>* OutHandles);
	void OnAction(const FInputActionInstance& Instance);
	void OnNavigate(const FInputActionInstance& Instance);
	void OnConfirm(const FInputActionInstance& Instance);
	void OnBack(const FInputActionInstance& Instance);
	void OnPause(const FInputActionInstance& Instance);

	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> GameplayContext;

	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> GlobalContext;

	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> HUDContext;

	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> MenuContext;

	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> NavigateAction;

	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> ConfirmAction;

	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> BackAction;

	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> PauseAction;

	UPROPERTY(Config)
	TSoftClassPtr<UUIScreenBase> PauseScreenClass;

	UPROPERTY(Config)
	TSoftObjectPtr<UDataTable> KeyIconTable;

	UPROPERTY(Config)
	float NavigateRepeatDelay = 0.4f;

	UPROPERTY(Config)
	float NavigateRepeatInterval = 0.1f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> LoadedAssets;

	UPROPERTY(Transient)
	FUIScreenStack Layers[4];

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputMappingContext>> AppliedExtraContexts;

	TWeakObjectPtr<UUIScreenBase> CurrentOwner;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	TSet<const UInputAction*> BaseActions;
	TArray<uint32> ExtraHandles;
	EUINavigation RepeatDirection = EUINavigation::Invalid;
	double NextRepeatTime = 0.0;
	bool bUsingGamepad = false;
	bool bFocusRestorePending = false;
	FTSTicker::FDelegateHandle FocusWatchdogHandle;
};
