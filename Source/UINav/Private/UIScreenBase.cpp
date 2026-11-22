#include "UINav/Public/UIScreenBase.h"

#include "Components/Widget.h"
#include "Containers/Ticker.h"

void UUIScreenBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Navigation never leaves the screen: at the edge of a list it stops instead of
	// escaping, losing focus and being pulled back by the watchdog (a visible flash).
	for (const EUINavigation Direction : { EUINavigation::Up, EUINavigation::Down, EUINavigation::Left, EUINavigation::Right })
	{
		SetNavigationRuleBase(Direction, EUINavigationRule::Stop);
	}
}

UWidget* UUIScreenBase::GetDesiredFocusTarget_Implementation() const
{
	return nullptr;
}

bool UUIScreenBase::HandleUIAction_Implementation(const UInputAction* Action, ETriggerEvent Event)
{
	return false;
}

bool UUIScreenBase::HandleBack_Implementation()
{
	return false;
}

void UUIScreenBase::NotifyChildFocused(UWidget* Widget)
{
	LastFocused = Widget;
}
