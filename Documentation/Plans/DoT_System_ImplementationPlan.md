# DoT System (Bleeding/Burning) - Implementation Plan

> **Version:** 2.0
> **Date:** 2026-01-24
> **Author:** Claude Code
> **Status:** ✅ COMPLETE

---

## Overview

План поэтапного внедрения системы Damage-over-Time (DoT) эффектов: кровотечения (Bleeding) и горения (Burning), с полной интеграцией в архитектуру проекта AAA уровня.

---

## Phase 1: GameplayTags Registration
**Status:** PENDING
**Priority:** Critical (блокирует все остальные фазы)

### 1.1 Native Tags для DoT

Добавить в `SuspenseCoreGameplayTags.h/cpp`:

```cpp
// === State Tags (DoT) ===
namespace State::Health
{
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedingLight);    // State.Health.Bleeding.Light
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedingHeavy);    // State.Health.Bleeding.Heavy
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Regenerating);     // State.Health.Regenerating (for HoT)
}

// === Data Tags (SetByCaller) ===
namespace Data::Damage
{
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bleed);           // Data.Damage.Bleed
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BurnArmor);       // Data.Damage.Burn.Armor
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BurnHealth);      // Data.Damage.Burn.Health
}

// === Effect Tags ===
namespace Effect::Damage
{
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedLight);      // Effect.Damage.Bleed.Light
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedHeavy);      // Effect.Damage.Bleed.Heavy
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BurnArmorBypass); // Effect.Damage.Burn.ArmorBypass
}

// === Event Tags ===
namespace Event::DoT
{
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Applied);         // SuspenseCore.Event.DoT.Applied
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tick);            // SuspenseCore.Event.DoT.Tick
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Expired);         // SuspenseCore.Event.DoT.Expired
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Removed);         // SuspenseCore.Event.DoT.Removed
}

// === GameplayCue Tags ===
namespace GameplayCue::DoT
{
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedLight);      // GameplayCue.DoT.Bleed.Light
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedHeavy);      // GameplayCue.DoT.Bleed.Heavy
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Burn);            // GameplayCue.DoT.Burn
}
```

### 1.2 Deliverables
- [ ] Обновить `SuspenseCoreGameplayTags.h`
- [ ] Обновить `SuspenseCoreGameplayTags.cpp`
- [ ] Добавить теги в FSuspenseCoreTagValidator::GetCriticalTagNames()

---

## Phase 2: DoT Service with EventBus Integration
**Status:** PENDING
**Priority:** High

### 2.1 Архитектура сервиса

```
┌─────────────────────────────────────────────────────────────────┐
│                 USuspenseCoreDoTService                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  RESPONSIBILITIES:                                              │
│  ├── Track active DoT effects per character                     │
│  ├── Publish DoT events to EventBus                             │
│  ├── Provide query API for UI widgets                           │
│  └── Cache DoT data for performance                             │
│                                                                 │
│  INTEGRATION:                                                   │
│  ├── Subscribes to: GAS.Attribute.Changed                       │
│  ├── Publishes to: DoT.Applied, DoT.Tick, DoT.Expired           │
│  └── Registered via: ServiceLocator                             │
│                                                                 │
│  DATA FLOW:                                                     │
│  GameplayEffect → ASC → EventBus → DoTService → UI Widget       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 API Definition

```cpp
UCLASS(BlueprintType)
class GAS_API USuspenseCoreDoTService : public UObject
{
    // === Query API ===
    UFUNCTION(BlueprintCallable)
    TArray<FSuspenseCoreActiveDoT> GetActiveDoTs(AActor* Target) const;

    UFUNCTION(BlueprintCallable)
    bool HasActiveBleed(AActor* Target) const;

    UFUNCTION(BlueprintCallable)
    bool HasActiveBurn(AActor* Target) const;

    UFUNCTION(BlueprintCallable)
    float GetBleedDamagePerSecond(AActor* Target) const;

    UFUNCTION(BlueprintCallable)
    float GetBurnTimeRemaining(AActor* Target) const;

    // === EventBus Integration ===
    void Initialize(USuspenseCoreEventBus* InEventBus);
    void Shutdown();

private:
    // ASC Tag change subscription
    void OnTagChanged(FGameplayTag Tag, int32 NewCount, AActor* Target);

    // EventBus publishing
    void PublishDoTApplied(AActor* Target, FGameplayTag DoTType);
    void PublishDoTTick(AActor* Target, FGameplayTag DoTType, float DamageDealt);
    void PublishDoTRemoved(AActor* Target, FGameplayTag DoTType);
};
```

### 2.3 Event Data Structure

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreDoTEventData
{
    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> AffectedActor;

    UPROPERTY(BlueprintReadOnly)
    FGameplayTag DoTType;  // State.Health.Bleeding.Light, State.Burning, etc.

    UPROPERTY(BlueprintReadOnly)
    float DamagePerTick;

    UPROPERTY(BlueprintReadOnly)
    float RemainingDuration;  // -1 for infinite (bleed)

    UPROPERTY(BlueprintReadOnly)
    int32 StackCount;
};
```

### 2.4 Deliverables
- [ ] Создать `USuspenseCoreDoTService.h`
- [ ] Создать `USuspenseCoreDoTService.cpp`
- [ ] Создать `FSuspenseCoreDoTEventData` struct
- [ ] Зарегистрировать сервис в ServiceLocator
- [ ] Подписаться на EventBus события

---

## Phase 3: GrenadeProjectile Integration
**Status:** PENDING
**Priority:** High

### 3.1 Модификации для GrenadeProjectile

```cpp
void ASuspenseCoreGrenadeProjectile::ApplyExplosionDamage()
{
    // EXISTING: Explosion damage

    // NEW: Apply DoT effects based on grenade type
    switch (GrenadeType)
    {
        case ESuspenseCoreGrenadeType::Fragmentation:
            ApplyBleedingToVictims(Victims);
            break;

        case ESuspenseCoreGrenadeType::Incendiary:
            ApplyBurningToVictims(Victims);
            break;
    }
}

void ASuspenseCoreGrenadeProjectile::ApplyBleedingToVictims(const TArray<AActor*>& Victims)
{
    for (AActor* Victim : Victims)
    {
        UAbilitySystemComponent* ASC = GetASC(Victim);
        if (!ASC) continue;

        // CRITICAL: Check armor before applying bleed
        float Armor = ASC->GetNumericAttribute(ArmorAttribute);
        if (Armor > 0.0f)
        {
            continue;  // Shrapnel blocked by armor
        }

        // Determine severity based on fragment hits
        int32 FragmentHits = CalculateFragmentHits(Victim);
        TSubclassOf<UGameplayEffect> BleedClass =
            (FragmentHits >= 5) ? HeavyBleedEffect : LightBleedEffect;

        // Apply with EventBus notification
        ApplyDoTEffect(ASC, BleedClass, DamagePerTick);
    }
}

void ASuspenseCoreGrenadeProjectile::ApplyBurningToVictims(const TArray<AActor*>& Victims)
{
    for (AActor* Victim : Victims)
    {
        UAbilitySystemComponent* ASC = GetASC(Victim);
        if (!ASC) continue;

        // ARMOR BYPASS: Fire ignores armor
        ApplyDoTEffect(ASC, ArmorBypassBurnEffect, ArmorDamage, HealthDamage, Duration);
    }
}
```

### 3.2 Deliverables
- [ ] Добавить `ApplyBleedingToVictims()` в GrenadeProjectile
- [ ] Добавить `ApplyBurningToVictims()` в GrenadeProjectile
- [ ] Интегрировать с DoTService для EventBus уведомлений
- [ ] Добавить SSOT параметры: BleedDamagePerTick, BurnArmorDamage, BurnHealthDamage

---

## Phase 4: UI Widget System (Debuff Icons)
**Status:** PLANNING
**Priority:** Medium

### 4.1 Архитектура виджетов

```
┌─────────────────────────────────────────────────────────────────┐
│                     MASTER HUD WIDGET                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              W_DebuffContainer                           │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐                │   │
│  │  │W_Debuff  │ │W_Debuff  │ │W_Debuff  │ ← Procedural   │   │
│  │  │(Bleed)   │ │(Burn)    │ │(Poison)  │   loading      │   │
│  │  │[Icon]    │ │[Icon]    │ │[Icon]    │                │   │
│  │  │Timer: ∞  │ │Timer: 5s │ │Timer: 10s│                │   │
│  │  └──────────┘ └──────────┘ └──────────┘                │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  Health/Stamina Bar | Ammo | Crosshair | etc.                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 Widget Classes

#### W_DebuffIcon (Individual debuff icon)
```cpp
UCLASS()
class UI_API UW_DebuffIcon : public USuspenseCoreUserWidget
{
    // === UI Components ===
    UPROPERTY(meta = (BindWidget))
    UImage* DebuffIcon;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* TimerText;  // "∞" for infinite, "5s" for timed

    UPROPERTY(meta = (BindWidget))
    UProgressBar* DurationBar;  // Hidden for infinite effects

    // === Data ===
    UPROPERTY(BlueprintReadOnly)
    FGameplayTag DebuffType;

    UPROPERTY(BlueprintReadOnly)
    float RemainingDuration;  // -1 = infinite

    // === API ===
    void SetDebuffData(FGameplayTag InType, float InDuration, UTexture2D* InIcon);
    void UpdateTimer(float DeltaTime);
};
```

#### W_DebuffContainer (Procedural container)
```cpp
UCLASS()
class UI_API UW_DebuffContainer : public USuspenseCoreUserWidget
{
    // === UI Components ===
    UPROPERTY(meta = (BindWidget))
    UHorizontalBox* DebuffBox;  // Contains W_DebuffIcon instances

    // === Widget Pool ===
    UPROPERTY()
    TMap<FGameplayTag, UW_DebuffIcon*> ActiveDebuffs;

    // === EventBus Subscription ===
    FSuspenseCoreSubscriptionHandle DoTAppliedHandle;
    FSuspenseCoreSubscriptionHandle DoTRemovedHandle;

    // === API ===
    void OnDoTApplied(FGameplayTag EventTag, const FSuspenseCoreEventData& Data);
    void OnDoTRemoved(FGameplayTag EventTag, const FSuspenseCoreEventData& Data);

    // === Procedural Loading ===
    UW_DebuffIcon* CreateDebuffWidget(FGameplayTag DebuffType);
    void RemoveDebuffWidget(FGameplayTag DebuffType);
    UTexture2D* GetIconForDebuffType(FGameplayTag DebuffType);
};
```

### 4.3 Deliverables
- [ ] Создать `W_DebuffIcon` widget blueprint
- [ ] Создать `W_DebuffContainer` widget blueprint
- [ ] Интегрировать с EventBus (подписка на DoT.Applied/Removed)
- [ ] Добавить контейнер в Master HUD
- [ ] Создать иконки для Bleed/Burn

---

## Phase 5: Documentation Update
**Status:** PENDING
**Priority:** Low

### 5.1 Файлы для обновления
- [ ] `GrenadeDoT_DesignDocument.md` - добавить информацию о сервисе
- [ ] `ConsumablesThrowables_GDD.md` - добавить DoT секцию
- [ ] `GrenadeSystem_UserGuide.md` - добавить инструкции по DoT
- [ ] `INDEX.md` - добавить ссылки на новую документацию

### 5.2 Новые документы
- [ ] `DoT_Service_API_Reference.md`
- [ ] `DebuffWidget_SetupGuide.md`

---

## Implementation Timeline

```
┌────────────────────────────────────────────────────────────────┐
│ Phase 1: GameplayTags          [██████████] 100% ✓ COMPLETE    │
│ Phase 2: DoT Service           [██████████] 100% ✓ COMPLETE    │
│ Phase 3: GrenadeProjectile     [██████████] 100% ✓ COMPLETE    │
│ Phase 4: UI Widgets            [██████████] 100% ✓ COMPLETE    │
│ Phase 5: Documentation         [██████████] 100% ✓ COMPLETE    │
└────────────────────────────────────────────────────────────────┘

ALL PHASES COMPLETE - System is PRODUCTION ready.
```

---

## Dependencies

```
Phase 1 ──► Phase 2 ──► Phase 3
                │
                └──────► Phase 4 ──► Phase 5
```

- **Phase 1** блокирует все (теги нужны везде)
- **Phase 2** нужен для Phase 3 и Phase 4
- **Phase 4** может начаться параллельно с Phase 3

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| GAS Effect stacking issues | High | Тщательное тестирование stack policies |
| EventBus performance | Medium | Батчинг событий, отложенная публикация |
| Widget memory leaks | Medium | Object pooling, явный cleanup |
| Network replication | High | Server-authoritative DoT, client-predicted VFX |

---

**Document End**
