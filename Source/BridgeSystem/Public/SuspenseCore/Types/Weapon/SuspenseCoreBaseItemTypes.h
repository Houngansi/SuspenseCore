// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseBaseItemTypes.generated.h"

/**
 * Структура для хранения информации о сокетах оружия для разных видов камеры
 *
 * Эта структура остается, так как DataTable содержит только базовые сокеты
 * (MuzzleSocket, SightSocket и т.д.), но не учитывает различия между видами.
 *
 * Используется когда оружие должно по-разному крепиться в зависимости от:
 * - Вида от первого лица (более детализированная модель)
 * - Вида от третьего лица (упрощенная модель)
 * - Использования ботами (может требовать особого расположения)
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FWeaponSocketData
{
    GENERATED_BODY()

    /** Сокет для вида от первого лица */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sockets")
    FName FirstPersonSocket = FName("GripPoint");

    /** Сокет для вида от третьего лица */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sockets")
    FName ThirdPersonSocket = FName("GripPoint");

    /** Сокет для ботов */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sockets")
    FName BotSocket = FName("GripPoint");

    /**
     * Получить подходящий сокет в зависимости от контекста
     * @param bIsFirstPerson - используется ли вид от первого лица
     * @param bIsBot - является ли владелец ботом
     * @return Имя сокета для использования
     */
    FName GetSocketForContext(bool bIsFirstPerson, bool bIsBot) const
    {
        if (bIsBot)
        {
            return BotSocket;
        }

        return bIsFirstPerson ? FirstPersonSocket : ThirdPersonSocket;
    }

    /**
     * Проверка валидности данных
     * @return true если все сокеты заданы
     */
    bool IsValid() const
    {
        return !FirstPersonSocket.IsNone() &&
               !ThirdPersonSocket.IsNone() &&
               !BotSocket.IsNone();
    }
};

/**
 * Дополнительные типы для совместимости и расширения
 *
 * Эти определения могут использоваться для специфичных случаев,
 * которые не покрываются основной системой DataTable
 */
namespace MedComItemTypes
{
    /** Типы способностей для категоризации */
    enum class EAbilityType : uint8
    {
        Fire = 0,      // Способности стрельбы
        Utility = 1,   // Утилитарные способности
        Special = 2,   // Специальные способности
        Passive = 3,   // Пассивные способности
        Custom = 4     // Пользовательские способности
    };
}
