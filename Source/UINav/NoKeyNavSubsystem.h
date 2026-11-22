#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NoKeyNavSubsystem.generated.h"

class FNavigationConfig;

/** Turns off Slate's built-in key/tab/analog navigation so every UI key goes through Enhanced Input. */
UCLASS()
class UNoKeyNavSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// Restored on shutdown so the editor gets its navigation back after PIE.
	TSharedPtr<FNavigationConfig> PreviousConfig;
};
