# Debuff Widget System - Implementation Guide

> **Version:** 2.1
> **Date:** 2026-01-24
> **Author:** Claude Code
> **Status:** ✅ PRODUCTION
> **Related:** DoT_System_ImplementationPlan.md, StatusEffect_SSOT_System.md, StatusEffects_DeveloperGuide.md

---

## Overview

Процедурная система виджетов для отображения дебафов (кровотечение, горение, и др.) в Master HUD. Система следует паттернам AAA игр: EventBus подписка, object pooling, SSOT data-driven UI.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         WBP_MasterHUD                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │              DebuffContainerWidget (W_DebuffContainer)              │ │
│  │  ┌───────────────────────────────────────────────────────────────┐ │ │
│  │  │                  DebuffBox (UHorizontalBox)                    │ │ │
│  │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐             │ │ │
│  │  │  │W_Debuff │ │W_Debuff │ │W_Debuff │ │ (Pool)  │  ← Dynamic  │ │ │
│  │  │  │Icon #1  │ │Icon #2  │ │Icon #3  │ │ Hidden  │    loading  │ │ │
│  │  │  │[Bleed]  │ │[Burn]   │ │[Poison] │ │         │             │ │ │
│  │  │  │Timer: ∞ │ │Timer:5s │ │Timer:10s│ │         │             │ │ │
│  │  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘             │ │ │
│  │  └───────────────────────────────────────────────────────────────┘ │ │
│  └────────────────────────────────────────────────────────────────────┘ │
│                                                                          │
│  [VitalsWidget]  [AmmoCounterWidget]  [QuickSlotsWidget]  [Crosshair]   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Data Flow

```
┌──────────────────┐    ┌─────────────────┐    ┌────────────────────┐
│ GrenadeProjectile │───►│   DoTService    │───►│     EventBus       │
│ ApplyDoTEffects() │    │ PublishDoTEvent │    │ Publish(DoT.Event) │
└──────────────────┘    └─────────────────┘    └─────────┬──────────┘
                                                         │
                        ┌────────────────────────────────┘
                        ▼
┌──────────────────┐    ┌─────────────────┐    ┌────────────────────┐
│DataManager(SSOT) │◄───│W_DebuffContainer│◄───│   EventBus Sub     │
│GetVisualsByTag() │    │OnDoTApplied()   │    │ OnDoTApplied/Removed│
└────────┬─────────┘    └────────┬────────┘    └────────────────────┘
         │                       │
         ▼                       ▼
┌──────────────────┐    ┌─────────────────┐
│ DT_StatusEffects │    │   W_DebuffIcon  │
│ (Visuals SSOT)   │    │ SetDebuffData() │
└──────────────────┘    └─────────────────┘
```

---

## SSOT Integration

### StatusEffectVisualsDataTable

Визуальные данные дебафов загружаются из DataTable через DataManager.

**Настройка в Project Settings → Game → SuspenseCore:**
```
StatusEffectVisualsDataTable: DT_StatusEffects
```

**Row Structure:** `FSuspenseCoreStatusEffectVisualRow`

| Field | Type | Description |
|-------|------|-------------|
| EffectID | FName | Уникальный ID эффекта |
| EffectTypeTag | FGameplayTag | Тег типа (State.Health.Bleeding.Heavy) |
| DisplayName | FText | Локализованное имя |
| Category | Enum | Debuff / Buff / Neutral |
| Icon | TSoftObjectPtr<UTexture2D> | Иконка для UI |
| IconTint | FLinearColor | Цвет иконки (нормальное состояние) |
| CriticalIconTint | FLinearColor | Цвет иконки (критическое состояние) |

**Пример DataTable row:**
```json
{
  "Name": "BleedingHeavy",
  "EffectID": "BleedingHeavy",
  "EffectTypeTag": "(TagName=\"State.Health.Bleeding.Heavy\")",
  "DisplayName": "Heavy Bleeding",
  "Category": "Debuff",
  "Icon": "/Game/UI/Icons/StatusEffects/T_Icon_Bleeding_Heavy",
  "IconTint": "(R=0.8, G=0.0, B=0.0, A=1.0)"
}
```

---

## Widget Classes

### W_DebuffIcon

**Source:** `Source/UISystem/Public/SuspenseCore/Widgets/HUD/W_DebuffIcon.h`

Индивидуальная иконка дебафа.

#### Bound Widgets (Blueprint)

| Widget | Type | Meta | Description |
|--------|------|------|-------------|
| DebuffImage | UImage | BindWidget | Иконка дебафа |
| TimerText | UTextBlock | BindWidget | Таймер (∞ или секунды) |
| DurationBar | UProgressBar | BindWidgetOptional | Прогресс-бар длительности |
| StackText | UTextBlock | BindWidgetOptional | Счётчик стаков |

#### Blueprint Hierarchy (WBP_DebuffIcon)

```
[WBP_DebuffIcon]
└── SizeContainer (SizeBox) ← ROOT, Min/Max: 64x64
    └── [Overlay]
        ├── ValidityBorder
        ├── HighlightBorder
        ├── DebuffImage (Image)
        ├── [TimerText] (TextBlock)
        ├── [StackText] (TextBlock)
        └── DurationBar (ProgressBar)
```

#### Key API

```cpp
// Установить данные дебафа (вызывать ПОСЛЕ добавления в parent!)
void SetDebuffData(FGameplayTag InDoTType, float InDuration, int32 InStackCount = 1);

// Обновить таймер
void UpdateTimer(float RemainingDuration);

// Обновить стаки
void UpdateStackCount(int32 NewStackCount);

// Проиграть анимацию удаления
void PlayRemovalAnimation();

// Сбросить для пула
void ResetToDefault();
```

#### SSOT Icon Loading

```cpp
bool UW_DebuffIcon::LoadIconFromSSOT()
{
    USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(this);
    if (!DataManager || !DataManager->IsStatusEffectVisualsReady())
        return false;

    FSuspenseCoreStatusEffectVisualRow VisualData;
    if (!DataManager->GetStatusEffectVisualsByTag(DoTType, VisualData))
        return false;

    // Cache icon path and tints from SSOT
    SSOTIconPath = VisualData.Icon;
    NormalTintColor = VisualData.IconTint;
    CriticalTintColor = VisualData.CriticalIconTint;

    // Async load texture
    if (SSOTIconPath.IsValid())
    {
        DebuffImage->SetBrushFromTexture(SSOTIconPath.Get());
    }
    else
    {
        // Async load
        FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
        IconLoadHandle = StreamableManager.RequestAsyncLoad(
            SSOTIconPath.ToSoftObjectPath(),
            FStreamableDelegate::CreateUObject(this, &UW_DebuffIcon::OnIconLoaded)
        );
    }
    return true;
}
```

---

### W_DebuffContainer

**Source:** `Source/UISystem/Public/SuspenseCore/Widgets/HUD/W_DebuffContainer.h`

Контейнер для процедурного управления иконками дебафов.

#### Bound Widgets (Blueprint)

| Widget | Type | Meta | Description |
|--------|------|------|-------------|
| DebuffBox | UHorizontalBox | BindWidget | Контейнер для иконок |

#### Blueprint Hierarchy (WBP_DebuffContainer)

```
[WBP_DebuffContainer]
└── DebuffBox (HorizontalBox) ← ROOT
    └── [Dynamic children - W_DebuffIcon instances]
```

#### Configuration

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| DebuffIconClass | TSubclassOf | WBP_DebuffIcon | Класс иконки |
| MaxVisibleDebuffs | int32 | 10 | Макс. видимых иконок |
| IconPoolSize | int32 | 15 | Размер пула |
| bAutoTargetLocalPlayer | bool | true | Автотаргет на игрока |

#### Key API

```cpp
// Добавить/обновить дебаф
void AddOrUpdateDebuff(FGameplayTag DoTType, float Duration, int32 StackCount);

// Удалить дебаф (с анимацией)
void RemoveDebuff(FGameplayTag DoTType);

// Установить целевого актора
void SetTargetActor(AActor* NewTarget);

// Очистить все дебафы
void ClearAllDebuffs();
```

---

## Critical Implementation Details

### ⚠️ Icon Initialization Order

**ВАЖНО:** Порядок инициализации иконки критичен!

```cpp
void UW_DebuffContainer::AddOrUpdateDebuff(FGameplayTag DoTType, float Duration, int32 StackCount)
{
    UW_DebuffIcon* NewIcon = AcquireIcon();

    // 1. СНАЧАЛА добавить в parent (вызовет NativeConstruct → Collapsed)
    UHorizontalBoxSlot* IconSlot = DebuffBox->AddChildToHorizontalBox(NewIcon);
    IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

    // 2. ПОТОМ установить данные (установит HitTestInvisible)
    NewIcon->SetDebuffData(DoTType, Duration, StackCount);

    // 3. Обновить layout
    DebuffBox->InvalidateLayoutAndVolatility();
}
```

**Причина:** `NativeConstruct()` устанавливает `Collapsed`, а `SetDebuffData()` устанавливает `HitTestInvisible`. Если вызвать `SetDebuffData` до добавления в parent, `NativeConstruct` перезапишет visibility на `Collapsed`.

### HorizontalBoxSlot Configuration

```cpp
IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
IconSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
IconSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
IconSlot->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 0.0f));
```

---

## EventBus Integration

### Subscribed Events

| Event Tag | Handler | Description |
|-----------|---------|-------------|
| SuspenseCore.Event.DoT.Applied | OnDoTApplied | Дебаф применён |
| SuspenseCore.Event.DoT.Removed | OnDoTRemoved | Дебаф снят |
| SuspenseCore.Event.DoT.Tick | OnDoTTick | Тик урона (опционально) |

### Event Handler Example

```cpp
void UW_DebuffContainer::OnDoTApplied(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    // Проверить что событие для нашего target актора
    if (!IsEventForTarget(EventData))
        return;

    // Извлечь данные из события
    FGameplayTag DoTType = EventData.Tags.First();
    float Duration = EventData.GetFloat(FName("Duration"), -1.0f);
    int32 Stacks = EventData.GetInt32(FName("Stacks"), 1);

    // Добавить/обновить иконку
    AddOrUpdateDebuff(DoTType, Duration, Stacks);
}
```

---

## Master HUD Integration

### WBP_MasterHUD Hierarchy

```
[WBP_MasterHUD]
└── RootCanvas
    ├── VitalsWidget
    ├── AmmoCounterWidget
    ├── QuickSlotsWidget
    ├── ReloadProgressWidget
    ├── CrosshairWidget
    └── DebuffContainerWidget ← W_DebuffContainer
```

### C++ Binding (Optional)

```cpp
// В W_MasterHUD.h
UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UW_DebuffContainer> DebuffContainerWidget;
```

---

## Testing Checklist

### ✅ Completed Tests

- [x] Icon displays when DoT applied via grenade explosion
- [x] Icon loads texture from SSOT DataTable
- [x] Multiple debuffs display simultaneously
- [x] Widget pooling works (icons recycled)
- [x] Container correctly positioned in MasterHUD
- [x] EventBus subscription active

### Manual Test Steps

1. Запустить PIE
2. Бросить гранату RGD-5 рядом с персонажем
3. После взрыва должна появиться иконка "Heavy Bleeding"
4. Иконка должна отображать настроенную текстуру из DataTable

---

## Troubleshooting

### Иконки не появляются

1. **Проверить логи:**
   ```
   LogDebuffContainer: OnDoTApplied: Type=State.Health.Bleeding.Heavy
   LogDebuffIcon: SetDebuffData: Type=State.Health.Bleeding.Heavy
   ```

2. **Проверить visibility:**
   ```
   Icon visibility: 3 (HitTestInvisible = OK)
   Icon visibility: 1 (Collapsed = ПРОБЛЕМА!)
   ```

3. **Проверить DebuffBox:**
   - Убедиться что `DebuffBox` имеет `BindWidget` мета
   - Убедиться что WBP_DebuffContainer содержит HorizontalBox с именем "DebuffBox"

### Иконка белая/placeholder

Проверить `StatusEffectVisualsDataTable` в Project Settings:
- Должен указывать на DataTable с корректными данными
- В DataTable должны быть правильные пути к текстурам

---

## Files Reference

| File | Description |
|------|-------------|
| `Source/UISystem/Public/SuspenseCore/Widgets/HUD/W_DebuffIcon.h` | Icon widget header |
| `Source/UISystem/Private/SuspenseCore/Widgets/HUD/W_DebuffIcon.cpp` | Icon widget impl |
| `Source/UISystem/Public/SuspenseCore/Widgets/HUD/W_DebuffContainer.h` | Container header |
| `Source/UISystem/Private/SuspenseCore/Widgets/HUD/W_DebuffContainer.cpp` | Container impl |
| `Source/BridgeSystem/Public/SuspenseCore/Settings/SuspenseCoreSettings.h` | Settings with DataTable refs |
| `Source/BridgeSystem/Private/SuspenseCore/Data/SuspenseCoreDataManager.cpp` | SSOT loading |

---

## Recent Commits & Fixes (2026-01-24)

### Commit History

| Commit | Description | Impact |
|--------|-------------|--------|
| `feat(Bleeding): Enable Tarkov-style stacking (up to 5x damage)` | StackLimitCount changed from 1 to 5 | Stacks now multiply damage |
| `balance(Bleeding): Separate Light/Heavy bleeding damage, reduce by 10x` | Split damage values, 5.0→0.5/1.5 | Better survival balance |
| `fix(DebuffContainer): Add icon to parent BEFORE calling SetDebuffData` | Fixed widget initialization order | Icons now visible |
| `debug(DebuffIcon): Add widget binding validation logging` | Added debug logging | Easier troubleshooting |

### Stacking System (Tarkov-Style)

**GameplayEffect Configuration:**
```cpp
// GE_BleedingEffect.cpp
StackingType = EGameplayEffectStackingType::AggregateByTarget;
StackLimitCount = 5;  // Max 5 stacks
```

**Damage Scaling:**

| Effect | 1 Stack | 3 Stacks | 5 Stacks (Max) |
|--------|---------|----------|----------------|
| Light Bleeding | 0.5 DPS | 1.5 DPS | 2.5 DPS |
| Heavy Bleeding | 1.5 DPS | 4.5 DPS | 7.5 DPS |

### Critical Initialization Order

```cpp
// W_DebuffContainer.cpp - AddOrUpdateDebuff()

// ✅ CORRECT ORDER:
// 1. FIRST add to parent (triggers NativeConstruct → Collapsed)
UHorizontalBoxSlot* IconSlot = DebuffBox->AddChildToHorizontalBox(NewIcon);

// 2. THEN setup data (sets HitTestInvisible)
NewIcon->SetDebuffData(DoTType, Duration, StackCount);

// 3. Force layout update
DebuffBox->InvalidateLayoutAndVolatility();
```

**Why this order matters:**
- `NativeConstruct()` sets widget to `Collapsed` by default
- `SetDebuffData()` sets widget to `HitTestInvisible` (visible)
- If SetDebuffData called BEFORE AddChild, NativeConstruct overwrites visibility

---

## Related Documentation

- **[StatusEffects_DeveloperGuide.md](../GAS/StatusEffects_DeveloperGuide.md)** - Full developer guide
- **[StatusEffect_SSOT_System.md](./StatusEffect_SSOT_System.md)** - SSOT architecture
- **[StatusEffect_System_GDD.md](../GameDesign/StatusEffect_System_GDD.md)** - Game design document

---

**Document End**
