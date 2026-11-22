#include "UINav/Public/UIManager.h"

#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Containers/Ticker.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Slate/SObjectWidget.h"
#include "UINav/Public/NavButton.h"
#include "UINav/Public/UINavConsumer.h"
#include "UINav/Public/UIScreenBase.h"

namespace
{
	constexpr int32 GameplayPriority = 0;
	constexpr int32 GlobalPriority = 50;
	constexpr int32 UIPriority = 100;
	constexpr int32 ExtraPriority = 110;
	constexpr double NavigateThreshold = 0.5;

	EUINavigation ToDirection(const FVector2D& Value)
	{
		if (FMath::Max(FMath::Abs(Value.X), FMath::Abs(Value.Y)) < NavigateThreshold)
		{
			return EUINavigation::Invalid;
		}
		if (FMath::Abs(Value.X) > FMath::Abs(Value.Y))
		{
			return Value.X > 0 ? EUINavigation::Right : EUINavigation::Left;
		}
		return Value.Y > 0 ? EUINavigation::Up : EUINavigation::Down;
	}

	// Nearest focusable widget under Root within ~60° of Direction, scored so that well-aligned
	// targets beat closer but more sideways ones. Uses last-painted geometry.
	TSharedPtr<SWidget> FindNearestInDirection(const TSharedRef<SWidget>& Root, const TSharedRef<SWidget>& From, EUINavigation Direction)
	{
		const FVector2f Dir = Direction == EUINavigation::Up ? FVector2f(0.f, -1.f)
			: Direction == EUINavigation::Down ? FVector2f(0.f, 1.f)
			: Direction == EUINavigation::Left ? FVector2f(-1.f, 0.f)
			: FVector2f(1.f, 0.f);
		const FGeometry& FromGeometry = From->GetPaintSpaceGeometry();
		const FVector2f Origin = FromGeometry.LocalToAbsolute(FromGeometry.GetLocalSize() * 0.5f);

		TSharedPtr<SWidget> Best;
		float BestScore = TNumericLimits<float>::Max();
		TFunction<void(const TSharedRef<SWidget>&)> Visit = [&](const TSharedRef<SWidget>& Widget)
		{
			if (!Widget->GetVisibility().IsVisible())
			{
				return; // skips collapsed/hidden subtrees, e.g. inactive tab pages
			}
			if (Widget != From && Widget != Root && Widget->SupportsKeyboardFocus())
			{
				if (Widget->IsEnabled())
				{
					const FGeometry& Geometry = Widget->GetPaintSpaceGeometry();
					const FVector2f Delta = FVector2f(Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f)) - Origin;
					const float Along = FVector2f::DotProduct(Delta, Dir);
					const float Across = FMath::Abs(FVector2f::CrossProduct(Delta, Dir));
					if (Along > 1.f && Across <= Along * 1.75f)
					{
						const float Score = Along + Across * 2.f;
						if (Score < BestScore)
						{
							BestScore = Score;
							Best = Widget;
						}
					}
				}
				return; // a focusable widget is one stop; don't descend into its parts
			}
			if (FChildren* Children = Widget->GetChildren())
			{
				for (int32 Index = 0; Index < Children->Num(); ++Index)
				{
					Visit(Children->GetChildAt(Index));
				}
			}
		};
		Visit(Root);
		return Best;
	}

	// Visible and enabled all the way up, so a button on a collapsed tab page doesn't count.
	bool IsUsable(const UWidget* Widget)
	{
		if (!Widget || !Widget->GetIsEnabled())
		{
			return false;
		}
		for (const UWidget* Current = Widget; Current;)
		{
			if (!Current->IsVisible())
			{
				return false;
			}
			const UWidget* Parent = Current->GetParent();
			Current = Parent ? Parent : Current->GetTypedOuter<UUserWidget>();
		}
		return true;
	}

	// Walks up from a Slate widget to the first UUserWidget that matches, stopping at Stop.
	UUserWidget* FindUserWidget(TSharedPtr<SWidget> Widget, const TSharedPtr<SWidget>& Stop, TFunctionRef<bool(UUserWidget&)> Match)
	{
		static const FName ObjectWidgetType(TEXT("SObjectWidget"));
		for (; Widget; Widget = Widget->GetParentWidget())
		{
			if (Widget->GetType() == ObjectWidgetType)
			{
				UUserWidget* UserWidget = StaticCastSharedPtr<SObjectWidget>(Widget)->GetWidgetObject();
				if (UserWidget && Match(*UserWidget))
				{
					return UserWidget;
				}
			}
			if (Widget == Stop)
			{
				break;
			}
		}
		return nullptr;
	}

	// bIgnoreHeld applies to the whole mapping rebuild, not just this context: every held key,
	// in every context, is ignored until released. Defaults to true in FModifyContextOptions.
	void SetContext(UEnhancedInputLocalPlayerSubsystem& Input, const UInputMappingContext* Context, bool bWanted, int32 Priority, bool bIgnoreHeld)
	{
		if (!Context || bWanted == Input.HasMappingContext(Context))
		{
			return;
		}
		FModifyContextOptions Options;
		Options.bIgnoreAllPressedKeysUntilRelease = bIgnoreHeld;
		if (bWanted)
		{
			Input.AddMappingContext(Context, Priority, Options);
		}
		else
		{
			Input.RemoveMappingContext(Context, Options);
		}
	}
}

UUIScreenBase* UUIManager::PushScreen(EUILayer Layer, TSubclassOf<UUIScreenBase> ScreenClass)
{
	APlayerController* PC = GetLocalPlayer()->PlayerController;
	if (!ScreenClass || !PC)
	{
		return nullptr;
	}

	UUIScreenBase* Screen = CreateWidget<UUIScreenBase>(PC, ScreenClass);
	TArray<TObjectPtr<UUIScreenBase>>& Stack = Layers[static_cast<int32>(Layer)].Screens;
	if (Stack.Num() > 0)
	{
		Stack.Last()->SetVisibility(ESlateVisibility::Collapsed);
	}
	Stack.Add(Screen);
	Screen->AddToPlayerScreen(static_cast<int32>(Layer) * 100 + Stack.Num());

	RecalculateInputOwner();
	return Screen;
}

void UUIManager::PushScreenAsync(EUILayer Layer, TSoftClassPtr<UUIScreenBase> ScreenClass)
{
	UAssetManager::GetStreamableManager().RequestAsyncLoad(ScreenClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, Layer, ScreenClass]
		{
			PushScreen(Layer, ScreenClass.Get());
		}));
}

void UUIManager::PopScreen(UUIScreenBase* Screen)
{
	for (FUIScreenStack& Layer : Layers)
	{
		if (Layer.Screens.Remove(Screen) == 0)
		{
			continue;
		}
		Screen->RemoveFromParent();
		if (Layer.Screens.Num() > 0)
		{
			Layer.Screens.Last()->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		RecalculateInputOwner();
		return;
	}
}

UUIScreenBase* UUIManager::GetInputOwner() const
{
	for (int32 Index = UE_ARRAY_COUNT(Layers) - 1; Index >= 0; --Index)
	{
		const TArray<TObjectPtr<UUIScreenBase>>& Stack = Layers[Index].Screens;
		if (Stack.Num() > 0 && Stack.Last()->InputPolicy != EUIInputPolicy::GameOnly)
		{
			return Stack.Last();
		}
	}
	return nullptr;
}

void UUIManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UInputDeviceSubsystem* Devices = UInputDeviceSubsystem::Get())
	{
		const FPlatformUserId User = GetLocalPlayer()->GetPlatformUserId();
		bUsingGamepad = Devices->GetMostRecentlyUsedHardwareDevice(User).PrimaryDeviceType == EHardwareDevicePrimaryType::Gamepad;
		Devices->OnInputHardwareDeviceChangedNative.AddUObject(this, &UUIManager::OnHardwareDeviceChanged);
	}
	if (UEnhancedInputLocalPlayerSubsystem* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		Input->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &UUIManager::BroadcastPromptsChanged);
	}

	// Polled, not driven by FSlateApplication::OnFocusChanging: when the focused widget is
	// destroyed, Slate drops the focus path without broadcasting anything.
	FocusWatchdogHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UUIManager::TickFocusWatchdog));
}

void UUIManager::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(FocusWatchdogHandle);
	Super::Deinitialize();
}

void UUIManager::OnHardwareDeviceChanged(FPlatformUserId UserId, FInputDeviceId DeviceId)
{
	UInputDeviceSubsystem* Devices = UInputDeviceSubsystem::Get();
	if (!Devices || UserId != GetLocalPlayer()->GetPlatformUserId())
	{
		return;
	}

	const bool bGamepad = Devices->GetInputDeviceHardwareIdentifier(DeviceId).PrimaryDeviceType == EHardwareDevicePrimaryType::Gamepad;
	if (bGamepad == bUsingGamepad)
	{
		return;
	}
	bUsingGamepad = bGamepad;

	RecalculateInputOwner();
	if (UUIScreenBase* Owner = GetInputOwner(); Owner && bUsingGamepad)
	{
		RestoreFocus(Owner);
	}
	BroadcastPromptsChanged();
}

void UUIManager::BroadcastPromptsChanged()
{
	OnPromptsChanged.Broadcast();
}

FKey UUIManager::GetPromptKey(const UInputAction* Action) const
{
	const UEnhancedInputLocalPlayerSubsystem* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Input || !Action)
	{
		return EKeys::Invalid;
	}

	const TArray<FKey> Keys = Input->QueryKeysMappedToAction(Action);
	const FKey* Match = Keys.FindByPredicate([this](const FKey& Key) { return Key.IsGamepadKey() == bUsingGamepad; });
	return Match ? *Match : EKeys::Invalid;
}

TSoftObjectPtr<UTexture2D> UUIManager::GetPromptIcon(const UInputAction* Action) const
{
	const UDataTable* Table = KeyIconTable.LoadSynchronous();
	const FKey Key = GetPromptKey(Action);
	const FUIKeyIconRow* Row = Table && Key.IsValid() ? Table->FindRow<FUIKeyIconRow>(Key.GetFName(), TEXT("UI prompt")) : nullptr;
	if (!Row)
	{
		return nullptr;
	}
	return Row->Icon;
}

void UUIManager::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	// Screens belonged to the old controller's world.
	for (FUIScreenStack& Layer : Layers)
	{
		for (UUIScreenBase* Screen : Layer.Screens)
		{
			Screen->RemoveFromParent();
		}
		Layer.Screens.Reset();
	}
	CurrentOwner = nullptr;
	ExtraHandles.Reset();
	BaseActions.Reset();

	LoadedAssets = { GameplayContext.LoadSynchronous(), GlobalContext.LoadSynchronous(), HUDContext.LoadSynchronous(),
		MenuContext.LoadSynchronous(), NavigateAction.LoadSynchronous(), ConfirmAction.LoadSynchronous(),
		BackAction.LoadSynchronous(), PauseAction.LoadSynchronous() };

	BoundInput = NewPlayerController ? Cast<UEnhancedInputComponent>(NewPlayerController->InputComponent) : nullptr;
	if (UEnhancedInputComponent* Input = BoundInput.Get())
	{
		auto Bind = [this, Input](const UInputAction* Action, ETriggerEvent Event, void (UUIManager::*Handler)(const FInputActionInstance&))
		{
			if (Action)
			{
				Input->BindAction(Action, Event, this, Handler);
				BaseActions.Add(Action);
			}
		};
		Bind(NavigateAction.Get(), ETriggerEvent::Triggered, &UUIManager::OnNavigate);
		Bind(NavigateAction.Get(), ETriggerEvent::Completed, &UUIManager::OnNavigate);
		Bind(ConfirmAction.Get(), ETriggerEvent::Started, &UUIManager::OnConfirm);
		Bind(BackAction.Get(), ETriggerEvent::Started, &UUIManager::OnBack);
		Bind(PauseAction.Get(), ETriggerEvent::Started, &UUIManager::OnPause);
		for (const UInputMappingContext* Context : { GlobalContext.Get(), HUDContext.Get(), MenuContext.Get() })
		{
			BindForward(*Input, Context, BaseActions, nullptr);
		}
	}

	if (UEnhancedInputLocalPlayerSubsystem* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		SetContext(*Input, GlobalContext.Get(), true, GlobalPriority, true);
	}
	RecalculateInputOwner();
}

void UUIManager::RecalculateInputOwner()
{
	UUIScreenBase* Owner = GetInputOwner();
	const EUIInputPolicy Policy = Owner ? Owner->InputPolicy : EUIInputPolicy::GameOnly;

	if (UEnhancedInputLocalPlayerSubsystem* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		// Only crossing between gameplay and a menu ignores held keys (the B that closed the menu
		// mustn't make the character act). GameAndMenu screens like the radial keep gameplay, and
		// ignoring there would cancel the very key holding them open.
		const bool bWantGameplay = Policy != EUIInputPolicy::MenuOnly;
		const bool bIgnoreHeld = GameplayContext.Get() && bWantGameplay != Input->HasMappingContext(GameplayContext.Get());

		SetContext(*Input, GameplayContext.Get(), bWantGameplay, GameplayPriority, bIgnoreHeld);
		SetContext(*Input, HUDContext.Get(), Policy == EUIInputPolicy::GameAndMenu, UIPriority, bIgnoreHeld);
		SetContext(*Input, MenuContext.Get(), Policy == EUIInputPolicy::MenuOnly, UIPriority, bIgnoreHeld);

		const TArray<TObjectPtr<UInputMappingContext>> WantedExtras = Owner ? Owner->ExtraContexts : TArray<TObjectPtr<UInputMappingContext>>();
		for (UInputMappingContext* Context : AppliedExtraContexts)
		{
			SetContext(*Input, Context, WantedExtras.Contains(Context), ExtraPriority, bIgnoreHeld);
		}
		for (UInputMappingContext* Context : WantedExtras)
		{
			SetContext(*Input, Context, true, ExtraPriority, bIgnoreHeld);
		}
		AppliedExtraContexts = WantedExtras;
	}

	if (APlayerController* PC = GetLocalPlayer()->PlayerController)
	{
		const bool bShowCursor = Owner && Owner->bShowCursor && !bUsingGamepad;
		if (Owner)
		{
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(!bShowCursor);
			Mode.SetLockMouseToViewportBehavior(bShowCursor ? EMouseLockMode::DoNotLock : EMouseLockMode::LockAlways);
			PC->SetInputMode(Mode);
		}
		else
		{
			PC->SetInputMode(FInputModeGameOnly());
		}
		PC->SetShowMouseCursor(bShowCursor);
	}

	if (Owner == CurrentOwner.Get())
	{
		return;
	}
	CurrentOwner = Owner;
	RepeatDirection = EUINavigation::Invalid;

	if (UEnhancedInputComponent* Input = BoundInput.Get())
	{
		for (const uint32 Handle : ExtraHandles)
		{
			Input->RemoveBindingByHandle(Handle);
		}
		ExtraHandles.Reset();
		if (Owner)
		{
			TSet<const UInputAction*> Seen = BaseActions;
			for (const UInputMappingContext* Context : Owner->ExtraContexts)
			{
				BindForward(*Input, Context, Seen, &ExtraHandles);
			}
		}
	}

	if (Owner)
	{
		RestoreFocus(Owner);
	}
}

void UUIManager::RestoreFocus(UUIScreenBase* Screen)
{
	if (bFocusRestorePending)
	{
		return;
	}
	bFocusRestorePending = true;

	// Next frame, so widgets created this frame have geometry. Core ticker, not the world
	// timer manager: it still runs while the game is paused.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, WeakScreen = TWeakObjectPtr<UUIScreenBase>(Screen)](float)
	{
		bFocusRestorePending = false;
		const UUIScreenBase* Pinned = WeakScreen.Get();
		if (!Pinned)
		{
			return false;
		}
		UWidget* Last = Pinned->LastFocused.Get();
		UWidget* Target = Pinned->bRestoreLastFocus && IsUsable(Last) ? Last : Pinned->GetDesiredFocusTarget();
		if (Target)
		{
			Target->SetFocus();
		}
		return false;
	}));
}

bool UUIManager::TickFocusWatchdog(float)
{
	UUIScreenBase* Owner = GetInputOwner();
	APlayerController* PC = GetLocalPlayer()->PlayerController;
	if (bUsingGamepad && Owner && PC && !Owner->HasUserFocusedDescendants(PC))
	{
		RestoreFocus(Owner);
	}
	return true;
}

void UUIManager::Navigate(EUINavigation Direction)
{
	UUIScreenBase* Owner = GetInputOwner();
	TSharedPtr<FSlateUser> User = GetLocalPlayer()->GetSlateUser();
	if (!Owner || !User)
	{
		return;
	}

	FSlateApplication& Slate = FSlateApplication::Get();
	const TSharedPtr<SWidget> Focused = User->GetFocusedWidget();
	const TSharedPtr<SWidget> OwnerWidget = Owner->GetCachedWidget();
	FWidgetPath Path;
	if (!Focused || !OwnerWidget || !Slate.FindPathToWidget(Focused.ToSharedRef(), Path) || !Path.ContainsWidget(OwnerWidget.Get()))
	{
		RestoreFocus(Owner);
		return;
	}

	UUserWidget* Consumer = FindUserWidget(Focused, OwnerWidget, [](UUserWidget& Widget) { return Widget.Implements<UUINavConsumer>(); });
	if (Consumer && IUINavConsumer::Execute_ConsumeNavigation(Consumer, Direction))
	{
		return;
	}

	// Not FSlateApplication::NavigateFromWidget: it ignores widget navigation rules and searches
	// the whole window, so at a dead end focus can land on the viewport (and flash back).
	// Same steps Slate uses for its own navigation, but the search never leaves the owner screen.
	const uint32 UserIndex = User->GetUserIndex();
	const FNavigationEvent Event(Slate.GetModifierKeys(), UserIndex, Direction, ENavigationGenesis::Controller);
	FNavigationReply Reply = FNavigationReply::Stop();
	int32 BoundaryIndex = Path.Widgets.Num() - 1;
	for (; BoundaryIndex >= 0; --BoundaryIndex)
	{
		const FArrangedWidget& Arranged = Path.Widgets[BoundaryIndex];
		const FNavigationReply WidgetReply = Arranged.Widget->OnNavigation(Arranged.Geometry, Event);
		if (WidgetReply.GetBoundaryRule() != EUINavigationRule::Escape)
		{
			Reply = WidgetReply;
			break;
		}
		if (Arranged.Widget == OwnerWidget)
		{
			break; // the screen edge is always a boundary; Reply stays Stop
		}
	}

	TSharedPtr<SWidget> Destination;
	switch (Reply.GetBoundaryRule())
	{
	case EUINavigationRule::Explicit:
		Destination = Reply.GetFocusRecipient();
		break;
	case EUINavigationRule::Custom:
		if (Reply.GetFocusDelegate().IsBound())
		{
			Destination = Reply.GetFocusDelegate().Execute(Direction);
		}
		break;
	default:
		Destination = Path.GetDeepestWindow()->GetHittestGrid().FindNextFocusableWidget(Path.Widgets.Last(), Direction, Reply, Path.Widgets[BoundaryIndex], UserIndex);
		// Slate only searches a straight band in front of the focused widget. Offset layouts
		// (radials, staggered rows) have nothing in that band, so fall back to a cone search.
		if (!Destination && Reply.GetBoundaryRule() == EUINavigationRule::Stop)
		{
			Destination = FindNearestInDirection(Path.Widgets[BoundaryIndex].Widget, Focused.ToSharedRef(), Direction);
		}
		break;
	}

	if (Destination && Destination->IsEnabled() && Destination->SupportsKeyboardFocus())
	{
		Slate.SetUserFocus(UserIndex, Destination, EFocusCause::Navigation);
	}
}

void UUIManager::BindForward(UEnhancedInputComponent& Input, const UInputMappingContext* Context, TSet<const UInputAction*>& Seen, TArray<uint32>* OutHandles)
{
	if (!Context)
	{
		return;
	}
	for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
	{
		bool bAlreadySeen = true;
		if (Mapping.Action)
		{
			Seen.Add(Mapping.Action, &bAlreadySeen);
		}
		if (bAlreadySeen)
		{
			continue;
		}
		for (const ETriggerEvent Event : { ETriggerEvent::Started, ETriggerEvent::Triggered, ETriggerEvent::Completed })
		{
			const uint32 Handle = Input.BindAction(Mapping.Action, Event, this, &UUIManager::OnAction).GetHandle();
			if (OutHandles)
			{
				OutHandles->Add(Handle);
			}
		}
	}
}

void UUIManager::OnAction(const FInputActionInstance& Instance)
{
	if (UUIScreenBase* Owner = GetInputOwner())
	{
		Owner->HandleUIAction(Instance.GetSourceAction(), Instance.GetTriggerEvent());
	}
}

void UUIManager::OnNavigate(const FInputActionInstance& Instance)
{
	const EUINavigation Direction = Instance.GetTriggerEvent() == ETriggerEvent::Completed
		? EUINavigation::Invalid
		: ToDirection(Instance.GetValue().Get<FVector2D>());
	const double Now = FPlatformTime::Seconds();

	if (Direction == EUINavigation::Invalid)
	{
		RepeatDirection = EUINavigation::Invalid;
	}
	else if (Direction != RepeatDirection)
	{
		RepeatDirection = Direction;
		NextRepeatTime = Now + NavigateRepeatDelay;
		Navigate(Direction);
	}
	else if (Now >= NextRepeatTime)
	{
		NextRepeatTime = Now + NavigateRepeatInterval;
		Navigate(Direction);
	}
}

void UUIManager::OnConfirm(const FInputActionInstance& Instance)
{
	UUIScreenBase* Owner = GetInputOwner();
	TSharedPtr<FSlateUser> User = GetLocalPlayer()->GetSlateUser();
	if (!Owner || !User || Owner->HandleUIAction(Instance.GetSourceAction(), Instance.GetTriggerEvent()))
	{
		return;
	}

	UUserWidget* Focused = FindUserWidget(User->GetFocusedWidget(), Owner->GetCachedWidget(), [](UUserWidget& Widget) { return Widget.IsA<UNavButton>(); });
	if (UNavButton* Button = Cast<UNavButton>(Focused))
	{
		Button->Press();
	}
}

void UUIManager::OnBack(const FInputActionInstance&)
{
	if (UUIScreenBase* Owner = GetInputOwner())
	{
		if (!Owner->HandleBack() && Owner->bCloseOnBack)
		{
			PopScreen(Owner);
		}
	}
}

void UUIManager::OnPause(const FInputActionInstance& Instance)
{
	if (UUIScreenBase* Owner = GetInputOwner())
	{
		Owner->HandleUIAction(Instance.GetSourceAction(), Instance.GetTriggerEvent());
	}
	else if (!PauseScreenClass.IsNull())
	{
		PushScreenAsync(EUILayer::Menu, PauseScreenClass);
	}
}
