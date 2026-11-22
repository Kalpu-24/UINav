#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NoKeyNavSubsystem.generated.h"

class FNavigationConfig;

UCLASS()
class UNoKeyNavSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	TSharedPtr<FNavigationConfig> PreviousConfig;
};
