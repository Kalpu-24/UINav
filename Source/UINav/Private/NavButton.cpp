#include "UINav/Public/NavButton.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "UINav/Public/UIScreenBase.h"

void UNavButton::Press()
{
	if (GetIsEnabled())
	{
		OnPressed.Broadcast();
	}
}

void UNavButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);
	// Slate's navigation searches the hit-test grid; the UserWidget default (SelfHitTestInvisible) isn't in it.
	if (GetVisibility() == ESlateVisibility::SelfHitTestInvisible)
	{
		SetVisibility(ESlateVisibility::Visible);
	}
	if (Button)
	{
		UE_CLOG(Button->GetIsFocusable(), LogTemp, Warning, TEXT("%s: untick Is Focusable on the inner Button so focus stays on the NavButton."), *GetPathName());
		Button->OnClicked.AddDynamic(this, &UNavButton::Press);
	}
}

void UNavButton::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (GetIsEnabled())
	{
		SetFocus();
	}
}

FReply UNavButton::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Border variant: no UButton to click, so the NavButton handles the click itself.
	if (!Button && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Press();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UNavButton::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);

	OnFocusVisualChanged(true);

	if (UUIScreenBase* Screen = GetTypedOuter<UUIScreenBase>())
	{
		Screen->NotifyChildFocused(this);
	}

	for (UWidget* Widget = this; Widget;)
	{
		if (UScrollBox* ScrollBox = Cast<UScrollBox>(Widget->GetParent()))
		{
			ScrollBox->ScrollWidgetIntoView(this);
			break;
		}

		UWidget* Parent = Widget->GetParent();
		if (!Parent)
		{
			UUserWidget* OwningUserWidget = Widget->GetTypedOuter<UUserWidget>();
			if (OwningUserWidget == Widget)
			{
				break;
			}
			Parent = OwningUserWidget;
		}
		Widget = Parent;
	}
}

void UNavButton::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);

	OnFocusVisualChanged(false);
}
