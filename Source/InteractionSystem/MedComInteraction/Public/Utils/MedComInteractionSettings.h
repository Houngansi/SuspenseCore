// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MedComInteractionSettings.generated.h"

/**
 * Настройки взаимодействия для проекта MedCom
 * Позволяет задать параметры трассировки и другие опции
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "MedCom Interaction Settings"))
class MEDCOMINTERACTION_API UMedComInteractionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMedComInteractionSettings()
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