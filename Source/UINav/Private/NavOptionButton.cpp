#include "UINav/Public/NavOptionButton.h"

void UNavOptionButton::SetValue(int32 NewValue)
{
	NewValue = FMath::Clamp(NewValue, MinValue, MaxValue);
	if (NewValue != Value)
	{
		Value = NewValue;
		OnValueChanged.Broadcast(Value);
	}
}

bool UNavOptionButton::ConsumeNavigation_Implementation(EUINavigation Direction)
{
	if (Direction == EUINavigation::Left || Direction == EUINavigation::Right)
	{
		SetValue(Value + (Direction == EUINavigation::Right ? 1 : -1));
		return true;
	}
	return false;
}
