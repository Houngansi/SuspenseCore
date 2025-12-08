// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseInventoryAmmoState.generated.h"

/**
 * Структура для хранения состояния патронов в оружии
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseInventoryAmmoState
{
    GENERATED_BODY()

    /**
     * Текущее количество патронов в магазине
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
    float CurrentAmmo = 0.0f;

    /**
     * Оставшееся количество патронов (вне магазина)
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
    float RemainingAmmo = 0.0f;

    /**
     * Тип используемых патронов
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
    FGameplayTag AmmoType;

    /**
     * Флаг, указывающий, что данные о патронах действительны
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
    bool bHasAmmoState = false;

    /**
     * Конструктор по умолчанию
     */
    FSuspenseInventoryAmmoState()
        : CurrentAmmo(0.0f)
        , RemainingAmmo(0.0f)
        , AmmoType(FGameplayTag::EmptyTag)
        , bHasAmmoState(false)
    {
    }

    /**
     * Конструктор с параметрами
     */
    FSuspenseInventoryAmmoState(float InCurrentAmmo, float InRemainingAmmo, const FGameplayTag& InAmmoType)
        : CurrentAmmo(InCurrentAmmo)
        , RemainingAmmo(InRemainingAmmo)
        , AmmoType(InAmmoType)
        , bHasAmmoState(true)
    {
    }

    /**
     * Сбрасывает состояние патронов к начальным значениям
     */
    void Reset()
    {
        CurrentAmmo = 0.0f;
        RemainingAmmo = 0.0f;
        AmmoType = FGameplayTag::EmptyTag;
        bHasAmmoState = false;
    }

    /**
     * Проверка на полностью пустое состояние патронов
     */
    bool IsEmpty() const
    {
        return CurrentAmmo <= 0.0f && RemainingAmmo <= 0.0f;
    }

    /**
     * Проверка, полностью ли заполнен магазин
     */
    bool IsMagazineFull(float MagazineSize) const
    {
        return CurrentAmmo >= MagazineSize;
    }

    /**
     * Общее количество патронов
     */
    float GetTotalAmmo() const
    {
        return CurrentAmmo + RemainingAmmo;
    }
};
