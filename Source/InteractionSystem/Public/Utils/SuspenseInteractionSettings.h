// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SuspenseInteractionSettings.generated.h"

/**
 * Настройки взаимодействия для проекта MedCom
 * Позволяет задать параметры трассировки и другие опции
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Suspense Interaction Settings"))
class INTERACTIONSYSTEM_API USuspenseInteractionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USuspenseInteractionSettings()
		: DefaultTraceDistance(300.0f)
		, DefaultTraceChannel(ECC_Visibility)
		, bEnableDebugDraw(false)
	{
	}

	/** Базовая дистанция трассировки для взаимодействия */
	UPROPERTY(config, EditAnywhere, Category = "Interaction", meta = (ClampMin = "10.0", ClampMax = "5000.0"))
	float DefaultTraceDistance;

	/** Канал трассировки для взаимодействия */
	UPROPERTY(config, EditAnywhere, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> DefaultTraceChannel;

	/** Включить отладочную отрисовку трассировки */
	UPROPERTY(config, EditAnywhere, Category = "Interaction|Debug")
	bool bEnableDebugDraw;
};
