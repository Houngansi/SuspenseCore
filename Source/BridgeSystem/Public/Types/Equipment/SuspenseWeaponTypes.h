// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/Weapon/SuspenseInventoryAmmoState.h"
#include "SuspenseWeaponTypes.generated.h"

/**
 * Битовые флаги состояния оружия для оптимизированного хранения
 * Используется для сетевой репликации и сохранения состояния
 */
UENUM(BlueprintType, meta = (Bitflags))
enum class EWeaponStateMask : uint8
{
    None        = 0,
    Initialized = 1 << 0,  // Оружие инициализировано
    Equipped    = 1 << 1,  // Оружие экипировано
    Reloading   = 1 << 2,  // Идёт перезарядка
    Firing      = 1 << 3,  // Идёт стрельба
    Jammed      = 1 << 4,  // Оружие заклинило
    Overheated  = 1 << 5,  // Перегрев
    OutOfAmmo   = 1 << 6,  // Нет патронов
    Disabled    = 1 << 7   // Оружие отключено/сломано
};

ENUM_CLASS_FLAGS(EWeaponStateMask);

/**
 * Структура для работы с битовыми флагами состояния оружия
 * Более эффективная версия для сетевой репликации
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FWeaponStateMask
{
    GENERATED_BODY()

    // Битовая маска флагов (8 бит = 8 состояний)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|State")
    uint8 Flags;

    // Конструктор по умолчанию
    FWeaponStateMask()
        : Flags(0)
    {
    }

    // Конструктор из перечисления
    FWeaponStateMask(EWeaponStateMask InFlags)
        : Flags(static_cast<uint8>(InFlags))
    {
    }

    /**
     * Проверка наличия флагов
     * @param FlagsToCheck Флаги для проверки
     * @return true если все указанные флаги установлены
     */
    bool HasFlags(EWeaponStateMask FlagsToCheck) const
    {
        return (Flags & static_cast<uint8>(FlagsToCheck)) == static_cast<uint8>(FlagsToCheck);
    }

    /**
     * Проверка наличия любого из флагов
     * @param FlagsToCheck Флаги для проверки
     * @return true если хотя бы один флаг установлен
     */
    bool HasAnyFlags(EWeaponStateMask FlagsToCheck) const
    {
        return (Flags & static_cast<uint8>(FlagsToCheck)) != 0;
    }

    /**
     * Установка флагов
     * @param FlagsToSet Флаги для установки
     * @param bEnabled true для установки, false для снятия
     */
    void SetFlags(EWeaponStateMask FlagsToSet, bool bEnabled)
    {
        if (bEnabled)
        {
            Flags |= static_cast<uint8>(FlagsToSet);
        }
        else
        {
            Flags &= ~static_cast<uint8>(FlagsToSet);
        }
    }

    /**
     * Переключение флагов
     * @param FlagsToToggle Флаги для переключения
     */
    void ToggleFlags(EWeaponStateMask FlagsToToggle)
    {
        Flags ^= static_cast<uint8>(FlagsToToggle);
    }

    /**
     * Очистка всех флагов
     */
    void Clear()
    {
        Flags = 0;
    }

    /**
     * Конвертация в EWeaponStateMask
     */
    EWeaponStateMask ToEnum() const
    {
        return static_cast<EWeaponStateMask>(Flags);
    }

    /**
     * Проверка на пустоту
     */
    bool IsEmpty() const
    {
        return Flags == 0;
    }

    /**
     * Получение строкового представления для отладки
     */
    FString ToString() const
    {
        FString Result;

        if (HasFlags(EWeaponStateMask::Initialized)) Result += TEXT("Initialized ");
        if (HasFlags(EWeaponStateMask::Equipped)) Result += TEXT("Equipped ");
        if (HasFlags(EWeaponStateMask::Reloading)) Result += TEXT("Reloading ");
        if (HasFlags(EWeaponStateMask::Firing)) Result += TEXT("Firing ");
        if (HasFlags(EWeaponStateMask::Jammed)) Result += TEXT("Jammed ");
        if (HasFlags(EWeaponStateMask::Overheated)) Result += TEXT("Overheated ");
        if (HasFlags(EWeaponStateMask::OutOfAmmo)) Result += TEXT("OutOfAmmo ");
        if (HasFlags(EWeaponStateMask::Disabled)) Result += TEXT("Disabled ");

        return Result.IsEmpty() ? TEXT("None") : Result.TrimEnd();
    }
};

/**
 * Расширенные параметры выстрела из оружия
 * Включает дополнительную информацию для баллистики и эффектов
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FWeaponShotParams
{
    GENERATED_BODY()

    // Начальная точка выстрела (обычно дуло оружия)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Shot")
    FVector StartLocation;

    // Направление выстрела (нормализованный вектор)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Shot")
    FVector Direction;

    // Текущий разброс оружия в градусах
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Shot", meta = (ClampMin = "0.0", ClampMax = "45.0"))
    float SpreadAngle;

    // Базовый урон выстрела
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Shot", meta = (ClampMin = "0.0"))
    float BaseDamage;

    // Множитель урона (от модификаторов, критов и т.д.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Shot", meta = (ClampMin = "0.0"))
    float DamageMultiplier;

    // Дальность выстрела
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Shot", meta = (ClampMin = "0.0"))
    float Range;

    // Актор, совершающий выстрел
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Shot")
    AActor* Instigator;

    // Результат трассировки (заполняется после выстрела)
    UPROPERTY(BlueprintReadWrite, Category = "Weapon|Shot")
    FHitResult HitResult;

    // Теги для идентификации типа выстрела
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Shot")
    FGameplayTagContainer ShotTags;

    // Тип патрона (для разных эффектов)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Shot")
    FGameplayTag AmmoType;

    // Номер выстрела в очереди (для автоматического огня)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Shot")
    int32 ShotNumber;

    // Время выстрела (для расчёта трассеров и эффектов)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Shot")
    float Timestamp;

    // Конструктор по умолчанию
    FWeaponShotParams()
        : StartLocation(FVector::ZeroVector)
        , Direction(FVector::ForwardVector)
        , SpreadAngle(0.0f)
        , BaseDamage(0.0f)
        , DamageMultiplier(1.0f)
        , Range(10000.0f)
        , Instigator(nullptr)
        , ShotNumber(0)
        , Timestamp(0.0f)
    {
    }

    /**
     * Расчёт финального урона с учётом множителя
     */
    float GetFinalDamage() const
    {
        return BaseDamage * DamageMultiplier;
    }

    /**
     * Применение разброса к направлению
     * @param RandomStream Генератор случайных чисел для консистентности
     * @return Направление с применённым разбросом
     */
    FVector GetDirectionWithSpread(const FRandomStream& RandomStream) const
    {
        if (SpreadAngle <= 0.0f)
        {
            return Direction;
        }

        // Конвертируем угол разброса в радианы
        const float SpreadRadians = FMath::DegreesToRadians(SpreadAngle);

        // Генерируем случайный угол в конусе разброса
        const float ConeHalfAngle = SpreadRadians * 0.5f;
        const float RandomConeAngle = RandomStream.FRandRange(0.0f, ConeHalfAngle);
        const float RandomRotAngle = RandomStream.FRandRange(0.0f, 2.0f * PI);

        // Создаём вектор с разбросом
        FVector SpreadDirection = Direction.RotateAngleAxis(RandomConeAngle * 180.0f / PI, FVector::RightVector);
        SpreadDirection = SpreadDirection.RotateAngleAxis(RandomRotAngle * 180.0f / PI, Direction);

        return SpreadDirection.GetSafeNormal();
    }
};

/**
 * Данные о попадании для расчёта урона и эффектов
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FWeaponHitData
{
    GENERATED_BODY()

    // Параметры выстрела, который привёл к попаданию
    UPROPERTY(BlueprintReadWrite, Category = "Weapon|Hit")
    FWeaponShotParams ShotParams;

    // Компонент, в который попали
    UPROPERTY(BlueprintReadWrite, Category = "Weapon|Hit")
    UPrimitiveComponent* HitComponent;

    // Актор, в которого попали
    UPROPERTY(BlueprintReadWrite, Category = "Weapon|Hit")
    AActor* HitActor;

    // Точка попадания в мировых координатах
    UPROPERTY(BlueprintReadWrite, Category = "Weapon|Hit")
    FVector ImpactPoint;

    // Нормаль поверхности в точке попадания
    UPROPERTY(BlueprintReadWrite, Category = "Weapon|Hit")
    FVector ImpactNormal;

    // Название кости, если попали в скелетный меш
    UPROPERTY(BlueprintReadWrite, Category = "Weapon|Hit")
    FName BoneName;

    // Физический материал поверхности
    UPROPERTY(BlueprintReadWrite, Category = "Weapon|Hit")
    TWeakObjectPtr<UPhysicalMaterial> PhysMaterial;

    // Дистанция от точки выстрела до попадания
    UPROPERTY(BlueprintReadWrite, Category = "Weapon|Hit")
    float Distance;

    // Модификатор урона от типа попадания (голова, тело и т.д.)
    UPROPERTY(BlueprintReadWrite, Category = "Weapon|Hit")
    float HitZoneDamageMultiplier;

    FWeaponHitData()
        : HitComponent(nullptr)
        , HitActor(nullptr)
        , ImpactPoint(FVector::ZeroVector)
        , ImpactNormal(FVector::UpVector)
        , BoneName(NAME_None)
        , Distance(0.0f)
        , HitZoneDamageMultiplier(1.0f)
    {
    }
};
