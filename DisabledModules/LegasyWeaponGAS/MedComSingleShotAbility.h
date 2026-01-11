#pragma once

#include "CoreMinimal.h"
#include "Core/AbilitySystem/Abilities/WeaponAbilities/MedComBaseFireAbility.h"
#include "MedComSingleShotAbility.generated.h"

/**
 * Способность одиночного выстрела с сетевой репликацией.
 * - Клиент формирует FMedComShotRequest
 * - Отправляет запрос на сервер через RPC
 * - Сервер проверяет валидацию (дистанция, угол, время)
 * - Сервер применяет урон и отправляет результат клиенту через RPC
 */
UCLASS()
class MEDCOMCORE_API UMedComSingleShotAbility : public UMedComBaseFireAbility
{
    GENERATED_BODY()

public:
    UMedComSingleShotAbility();

protected:
    /** Реализация метода выполнения выстрела из базового класса */
    virtual void FireNextShot() override;
};