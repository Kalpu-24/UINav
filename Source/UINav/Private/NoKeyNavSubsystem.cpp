#include "UINav/Public/NoKeyNavSubsystem.h"

#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"

void UNoKeyNavSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	FSlateApplication& Slate = FSlateApplication::Get();
	PreviousConfig = Slate.GetNavigationConfig();
	
	TSharedRef<FNavigationConfig> Config = MakeShared<FNullNavigationConfig>();
	Config->KeyEventRules.Empty();
	Config->KeyActionRules.Empty();
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
