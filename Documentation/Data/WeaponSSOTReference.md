# Weapon SSOT DataTable Reference

## Обзор

Single Source of Truth (SSOT) - единственный источник правды для данных оружия, магазинов и патронов. Все данные хранятся в DataTable и доступны через `USuspenseCoreDataManager`.

---

## DataManager API

### Получение DataManager

```cpp
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"

// Через GameInstance
USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(WorldContextObject);

// Или через ServiceProvider
if (USuspenseCoreServiceProvider* Provider = GI->GetSubsystem<USuspenseCoreServiceProvider>())
{
    USuspenseCoreDataManager* DataManager = Provider->GetDataManager();
}
```

### Получение данных оружия

```cpp
// По ItemID
FSuspenseCoreWeaponData WeaponData;
if (DataManager->GetWeaponData(ItemID, WeaponData))
{
    // WeaponData.DisplayName    - Отображаемое имя
    // WeaponData.AmmoType       - FGameplayTag калибра (КРИТИЧНО для валидации!)
    // WeaponData.MagazineSize   - Размер магазина
    // WeaponData.FireRate       - Скорострельность
    // WeaponData.Damage         - Базовый урон
    // WeaponData.WeaponClass    - Класс оружия (Rifle, Pistol, etc.)
}
```

### Получение данных магазина

```cpp
FSuspenseCoreMagazineData MagazineData;
if (DataManager->GetMagazineData(MagazineItemID, MagazineData))
{
    // MagazineData.Caliber      - FGameplayTag калибра
    // MagazineData.Capacity     - Вместимость
    // MagazineData.AmmoType     - Тип патронов
}
```

---

## Структуры данных

### FSuspenseCoreWeaponData

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreWeaponData : public FTableRowBase
{
    GENERATED_BODY()

    // ========================================
    // Идентификация
    // ========================================

    /** Уникальный ID (совпадает с Row Name в DataTable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ItemID;

    /** Отображаемое имя */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    /** Описание */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Description;

    // ========================================
    // Калибр и патроны (КРИТИЧНО!)
    // ========================================

    /** Тип патронов/калибр - используется для валидации магазинов!
     *  Пример: "Ammo.Caliber.545x39", "Ammo.Caliber.9x19"
     *  ДОЛЖЕН совпадать с Magazine.Caliber для совместимости!
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Ammo.Caliber"))
    FGameplayTag AmmoType;

    /** Размер магазина по умолчанию */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MagazineSize = 30;

    // ========================================
    // Характеристики стрельбы
    // ========================================

    /** Скорострельность (выстрелов в минуту) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireRate = 600.0f;

    /** Базовый урон */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseDamage = 25.0f;

    /** Доступные режимы стрельбы */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<ESuspenseCoreFireMode> AvailableFireModes;

    // ========================================
    // Визуал и анимации
    // ========================================

    /** Класс актора оружия */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftClassPtr<AActor> WeaponActorClass;

    /** Тип оружия для анимаций */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Weapon.Type"))
    FGameplayTag WeaponArchetype;

    /** ID стойки для AnimBP */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GripID = 0;

    /** ID позы прицеливания */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AimPose = 0;

    // ========================================
    // ADS Camera
    // ========================================

    /** FOV при прицеливании */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ADSFOV = 60.0f;

    /** Длительность перехода в ADS */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ADSTransitionDuration = 0.2f;
};
```

### FSuspenseCoreMagazineData

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreMagazineData : public FTableRowBase
{
    GENERATED_BODY()

    /** Уникальный ID магазина */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ItemID;

    /** Отображаемое имя */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    /** Калибр магазина - ДОЛЖЕН совпадать с Weapon.AmmoType!
     *  Пример: "Ammo.Caliber.545x39"
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Ammo.Caliber"))
    FGameplayTag Caliber;

    /** Вместимость магазина */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Capacity = 30;

    /** Тип патронов (для разных типов урона) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Ammo.Type"))
    FGameplayTag AmmoType;
};
```

### FSuspenseCoreMagazineInstance

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreMagazineInstance
{
    GENERATED_BODY()

    /** ID типа магазина (ссылка на DataTable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName MagazineID;

    /** Калибр (копия из MagazineData для быстрого доступа) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTag Caliber;

    /** Текущее количество патронов */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentAmmo = 0;

    /** Максимальная вместимость */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxAmmo = 30;

    /** ID загруженных патронов */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName LoadedAmmoID;

    /** Валидный ли экземпляр */
    bool IsValid() const { return !MagazineID.IsNone() && Caliber.IsValid(); }
};
```

---

## Калибры (Gameplay Tags)

### Стандартные калибры

```
Ammo.Caliber
├── Ammo.Caliber.545x39      // АК-74, АКС-74У
├── Ammo.Caliber.762x39      // АКМ, СКС
├── Ammo.Caliber.556x45      // M4A1, M16, HK416
├── Ammo.Caliber.762x51      // M14, SR-25
├── Ammo.Caliber.9x19        // Glock, MP5, Vector
├── Ammo.Caliber.9x18        // ПМ, АПС
├── Ammo.Caliber.45ACP       // M1911, UMP45
├── Ammo.Caliber.12Gauge     // Shotguns
└── Ammo.Caliber.338Lapua    // AWM, CheyTac
```

### Добавление нового калибра

1. Добавить в `SuspenseCoreGameplayTags.cpp`:

```cpp
namespace SuspenseCoreTags::Ammo::Caliber
{
    UE_DEFINE_GAMEPLAY_TAG(Cal_300BLK, "Ammo.Caliber.300BLK");
}
```

2. Экспортировать в хедере `SuspenseCoreGameplayTags.h`:

```cpp
namespace SuspenseCoreTags::Ammo::Caliber
{
    extern BRIDGESYSTEM_API FGameplayTag Cal_300BLK;
}
```

---

## Валидация калибра

### Где происходит валидация

```
QuickSlot Use → MagazineSwapHandler → MagazineComponent::IsMagazineCompatible()
                                              ↓
                                     Magazine.Caliber == CachedWeaponCaliber?
                                              ↓
                                        (из WeaponData.AmmoType)
```

### Код валидации (MagazineComponent)

```cpp
bool USuspenseCoreMagazineComponent::IsMagazineCompatible(
    const FSuspenseCoreMagazineInstance& Magazine) const
{
    // Если калибр оружия не установлен - разрешаем (для совместимости)
    if (!CachedWeaponCaliber.IsValid())
    {
        UE_LOG(LogMagazineComponent, Warning,
            TEXT("IsMagazineCompatible: No weapon caliber cached, allowing by default"));
        return true;
    }

    // Проверяем совпадение калибров
    bool bCompatible = Magazine.Caliber.MatchesTagExact(CachedWeaponCaliber);

    UE_LOG(LogMagazineComponent, Log,
        TEXT("IsMagazineCompatible: Magazine=%s (Caliber=%s), Weapon Caliber=%s, Compatible=%s"),
        *Magazine.MagazineID.ToString(),
        *Magazine.Caliber.ToString(),
        *CachedWeaponCaliber.ToString(),
        bCompatible ? TEXT("YES") : TEXT("NO"));

    return bCompatible;
}
```

### Инициализация калибра оружия

```cpp
// В SuspenseCoreWeaponActor::InitializeEquipmentComponents()

if (MagazineComponent)
{
    MagazineComponent->Initialize(GetOwner(), CachedASC);
    MagazineComponent->InitializeFromWeapon(this);  // Устанавливает CachedWeaponCaliber!
}
```

```cpp
// В SuspenseCoreMagazineComponent::InitializeFromWeapon()

bool USuspenseCoreMagazineComponent::InitializeFromWeapon(ISuspenseCoreWeapon* Weapon)
{
    if (!Weapon) return false;

    FSuspenseCoreWeaponData WeaponData = Weapon->GetWeaponData();

    // КРИТИЧНО: Кэшируем калибр оружия для валидации магазинов
    CachedWeaponCaliber = WeaponData.AmmoType;

    if (CachedWeaponCaliber.IsValid())
    {
        UE_LOG(LogMagazineComponent, Log,
            TEXT("InitializeFromWeapon: Cached caliber = %s"),
            *CachedWeaponCaliber.ToString());
    }
    else
    {
        UE_LOG(LogMagazineComponent, Warning,
            TEXT("InitializeFromWeapon: AmmoType is INVALID - caliber validation DISABLED!"));
    }

    return true;
}
```

---

## Пример DataTable записей

### DT_Weapons

| Row Name | DisplayName | AmmoType | MagazineSize | FireRate | WeaponArchetype |
|----------|-------------|----------|--------------|----------|-----------------|
| AK74M | АК-74М | Ammo.Caliber.545x39 | 30 | 650 | Weapon.Type.AssaultRifle |
| M4A1 | M4A1 Carbine | Ammo.Caliber.556x45 | 30 | 800 | Weapon.Type.AssaultRifle |
| Glock17 | Glock 17 | Ammo.Caliber.9x19 | 17 | 400 | Weapon.Type.Pistol |
| MP5 | HK MP5 | Ammo.Caliber.9x19 | 30 | 800 | Weapon.Type.SMG |

### DT_Magazines

| Row Name | DisplayName | Caliber | Capacity |
|----------|-------------|---------|----------|
| Mag_AK74_30 | АК-74 магазин (30) | Ammo.Caliber.545x39 | 30 |
| Mag_AK74_45 | АК-74 магазин (45) | Ammo.Caliber.545x39 | 45 |
| Mag_M4_30 | STANAG магазин (30) | Ammo.Caliber.556x45 | 30 |
| Mag_Glock_17 | Glock магазин (17) | Ammo.Caliber.9x19 | 17 |
| Mag_Glock_33 | Glock магазин (33) | Ammo.Caliber.9x19 | 33 |

---

## Частые ошибки

### 1. AmmoType не установлен в DataTable

**Симптом**: Любой магазин подходит к любому оружию

**Причина**: `WeaponData.AmmoType` пустой → `CachedWeaponCaliber` = Invalid

**Решение**: Убедиться что каждое оружие имеет AmmoType в DataTable

### 2. InitializeFromWeapon не вызван

**Симптом**: Лог показывает "No weapon caliber cached, allowing by default"

**Причина**: `MagazineComponent->InitializeFromWeapon()` не вызван

**Решение**: Добавить вызов в `InitializeEquipmentComponents()`

### 3. Несовпадение тегов калибра

**Симптом**: Совместимый магазин отклоняется

**Причина**: Разные теги (Ammo.Caliber.545x39 vs Ammo.Caliber.5.45x39)

**Решение**: Использовать точно одинаковые теги в Weapon и Magazine

---

## Диагностика

### Логи для отладки

```cpp
// Включить подробные логи:
UE_LOG(LogMagazineComponent, Log, TEXT("..."));

// В Output Log искать:
// [MagazineComponent] InitializeFromWeapon: Cached caliber = Ammo.Caliber.545x39
// [MagazineComponent] IsMagazineCompatible: Magazine=Mag_AK74_30 (Caliber=Ammo.Caliber.545x39), Weapon Caliber=Ammo.Caliber.545x39, Compatible=YES
```

### Консольные команды

```
// Показать текущее оружие и его калибр
SuspenseCore.Debug.CurrentWeapon

// Показать содержимое QuickSlots
SuspenseCore.Debug.QuickSlots

// Показать DataTable записи
SuspenseCore.Debug.WeaponData <ItemID>
```
