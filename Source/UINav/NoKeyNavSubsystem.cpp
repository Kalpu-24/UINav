#include "NoKeyNavSubsystem.h"

#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"

void UNoKeyNavSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!FSlateApplication::IsInitialized()) // dedicated server / commandlet
	{
		return;
	}

	FSlateApplication& Slate = FSlateApplication::Get();
	PreviousConfig = Slate.GetNavigationConfig();

	// FNullNavigationConfig already clears bTab/bKey/bAnalogNavigation.
	TSharedRef<FNavigationConfig> Config = MakeShared<FNullNavigationConfig>();
	Config->KeyEventRules.Empty();
	Config->KeyActionRules.Empty(); // Slate no longer turns keys into Accept/Back
	Slate.SetNavigationConfig(Config);
}

void UNoKeyNavSubsystem::Deinitialize()
{
	if (PreviousConfig.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetNavigationConfig(PreviousConfig.ToSharedRef());
	}
	PreviousConfig.Reset();

	Super::Deinitialize();
}
