# Debuff Widget System - Implementation Plan

> **Version:** 1.0
> **Date:** 2026-01-22
> **Author:** Claude Code
> **Status:** PLANNING
> **Related:** DoT_System_ImplementationPlan.md

---

## Overview

План внедрения процедурной системы виджетов для отображения дебафов (кровотечение, горение, и др.) в Master HUD. Система следует паттернам AAA игр: EventBus подписка, object pooling, data-driven UI.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         MASTER HUD WIDGET                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │                    W_DebuffContainer                                │ │
│  │  ┌───────────────────────────────────────────────────────────────┐ │ │
│  │  │                  UHorizontalBox                                │ │ │
│  │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐             │ │ │
│  │  │  │W_Debuff │ │W_Debuff │ │W_Debuff │ │ (Pool)  │  ← Dynamic  │ │ │
│  │  │  │Icon #1  │ │Icon #2  │ │Icon #3  │ │ Hidden  │    loading  │ │ │
│  │  │  │[Bleed]  │ │[Burn]   │ │[Poison] │ │         │             │ │ │
│  │  │  │Timer: ∞ │ │Timer:5s │ │Timer:10s│ │         │             │ │ │
│  │  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘             │ │ │
│  │  └───────────────────────────────────────────────────────────────┘ │ │
│  └────────────────────────────────────────────────────────────────────┘ │
│                                                                          │
│  [Health Bar]  [Stamina Bar]  [Ammo Counter]  [Crosshair]               │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Data Flow

```
┌──────────────────┐    ┌─────────────────┐    ┌────────────────────┐
│ GrenadeProjectile │───►│   DoTService    │───►│     EventBus       │
│ ApplyDoTEffects() │    │ RegisterApplied │    │ Publish(DoT.Event) │
└──────────────────┘    └─────────────────┘    └─────────┬──────────┘
                                                         │
                                                         ▼
┌──────────────────┐    ┌─────────────────┐    ┌────────────────────┐
│   W_DebuffIcon   │◄───│W_DebuffContainer│◄───│   EventBus Sub     │
│ UpdateDisplay()  │    │OnDoTApplied()   │    │ OnDoTApplied/Removed│
└──────────────────┘    └─────────────────┘    └────────────────────┘
```

---

## Phase 4.1: W_DebuffIcon Widget

### 4.1.1 Class Definition (C++)

**File:** `Source/UI/Public/SuspenseCore/Widgets/HUD/W_DebuffIcon.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Widgets/SuspenseCoreUserWidget.h"
#include "GameplayTagContainer.h"
#include "W_DebuffIcon.generated.h"

class UImage;
class UTextBlock;
class UProgressBar;
class UWidgetAnimation;

/**
 * Individual debuff icon widget
 *
 * FEATURES:
 * - Displays debuff icon with appropriate visual
 * - Shows timer text (∞ for infinite, seconds for timed)
 * - Optional duration progress bar
 * - Pulse animation when critical
 * - Stack count display
 *
 * USAGE:
 * Created dynamically by W_DebuffContainer
 * Not intended for direct placement in HUD
 */
UCLASS()
class UI_API UW_DebuffIcon : public USuspenseCoreUserWidget
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════
    // UI COMPONENTS
    // ═══════════════════════════════════════════════════════════════

    /** Debuff icon image */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> DebuffImage;

    /** Timer text (∞ or seconds) */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TimerText;

    /** Duration progress bar (hidden for infinite effects) */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> DurationBar;

    /** Stack count text (hidden if StackCount <= 1) */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StackText;

    // ═══════════════════════════════════════════════════════════════
    // ANIMATIONS (Optional - set in Blueprint)
    // ═══════════════════════════════════════════════════════════════

    /** Pulse animation for critical state */
    UPROPERTY(meta = (BindWidgetAnimOptional))
    TObjectPtr<UWidgetAnimation> PulseAnimation;

    /** Fade in animation */
    UPROPERTY(meta = (BindWidgetAnimOptional))
    TObjectPtr<UWidgetAnimation> FadeInAnimation;

    /** Fade out animation */
    UPROPERTY(meta = (BindWidgetAnimOptional))
    TObjectPtr<UWidgetAnimation> FadeOutAnimation;

    // ═══════════════════════════════════════════════════════════════
    // CONFIGURATION
    // ═══════════════════════════════════════════════════════════════

    /** Icon textures mapped by debuff type */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff")
    TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>> DebuffIcons;

    /** Tint color for critical state (low health + bleeding) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff")
    FLinearColor CriticalTintColor = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);

    /** Duration threshold for "critical" warning (seconds) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff")
    float CriticalDurationThreshold = 3.0f;

    // ═══════════════════════════════════════════════════════════════
    // PUBLIC API
    // ═══════════════════════════════════════════════════════════════

    /**
     * Initialize debuff display
     * @param InDoTType Type tag (State.Health.Bleeding.Light, etc.)
     * @param InDuration Duration (-1 for infinite)
     * @param InStackCount Number of stacks
     */
    UFUNCTION(BlueprintCallable, Category = "Debuff")
    void SetDebuffData(FGameplayTag InDoTType, float InDuration, int32 InStackCount = 1);

    /**
     * Update timer display
     * @param RemainingDuration Remaining seconds (-1 for infinite)
     */
    UFUNCTION(BlueprintCallable, Category = "Debuff")
    void UpdateTimer(float RemainingDuration);

    /**
     * Update stack count
     * @param NewStackCount New stack count
     */
    UFUNCTION(BlueprintCallable, Category = "Debuff")
    void UpdateStackCount(int32 NewStackCount);

    /**
     * Play removal animation and notify when complete
     */
    UFUNCTION(BlueprintCallable, Category = "Debuff")
    void PlayRemovalAnimation();

    /**
     * Get debuff type tag
     */
    UFUNCTION(BlueprintPure, Category = "Debuff")
    FGameplayTag GetDebuffType() const { return DoTType; }

    /**
     * Check if this is an infinite duration debuff
     */
    UFUNCTION(BlueprintPure, Category = "Debuff")
    bool IsInfinite() const { return TotalDuration < 0.0f; }

    // ═══════════════════════════════════════════════════════════════
    // DELEGATES
    // ═══════════════════════════════════════════════════════════════

    /** Called when removal animation completes (for pooling) */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRemovalComplete, UW_DebuffIcon*, Icon);
    UPROPERTY(BlueprintAssignable, Category = "Debuff")
    FOnRemovalComplete OnRemovalComplete;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /**
     * Update visual state (icon, colors)
     */
    void UpdateVisuals();

    /**
     * Format duration for display
     * @param Duration Seconds (-1 for infinite)
     * @return Formatted string ("∞" or "5s")
     */
    FText FormatDuration(float Duration) const;

    /**
     * Load icon texture for debuff type
     */
    void LoadIcon();

private:
    /** Current debuff type */
    UPROPERTY()
    FGameplayTag DoTType;

    /** Total duration (-1 = infinite) */
    float TotalDuration = -1.0f;

    /** Current remaining duration */
    float RemainingDuration = -1.0f;

    /** Current stack count */
    int32 StackCount = 1;

    /** Is in critical state */
    bool bIsCritical = false;

    /** Async load handle for icon */
    TSharedPtr<FStreamableHandle> IconLoadHandle;
};
```

### 4.1.2 Implementation (C++)

**File:** `Source/UI/Private/SuspenseCore/Widgets/HUD/W_DebuffIcon.cpp`

```cpp
// Implementation notes - key methods:

void UW_DebuffIcon::SetDebuffData(FGameplayTag InDoTType, float InDuration, int32 InStackCount)
{
    DoTType = InDoTType;
    TotalDuration = InDuration;
    RemainingDuration = InDuration;
    StackCount = InStackCount;

    UpdateVisuals();
    UpdateTimer(RemainingDuration);
    UpdateStackCount(StackCount);

    // Play fade in
    if (FadeInAnimation)
    {
        PlayAnimation(FadeInAnimation);
    }

    // Hide duration bar for infinite effects
    if (DurationBar)
    {
        DurationBar->SetVisibility(IsInfinite() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }
}

FText UW_DebuffIcon::FormatDuration(float Duration) const
{
    if (Duration < 0.0f)
    {
        // Infinite symbol
        return FText::FromString(TEXT("∞"));
    }

    if (Duration >= 60.0f)
    {
        // Show minutes:seconds for long durations
        int32 Minutes = FMath::FloorToInt(Duration / 60.0f);
        int32 Seconds = FMath::FloorToInt(FMath::Fmod(Duration, 60.0f));
        return FText::Format(NSLOCTEXT("Debuff", "MinSec", "{0}:{1:02}"), Minutes, Seconds);
    }

    // Show seconds with one decimal
    return FText::Format(NSLOCTEXT("Debuff", "Seconds", "{0:.1f}s"), Duration);
}
```

### 4.1.3 Blueprint Setup

**Blueprint:** `WBP_DebuffIcon` (derives from W_DebuffIcon)

```
Widget Hierarchy:
├── [Root] CanvasPanel
│   ├── BackgroundBorder (9-slice for icon frame)
│   │   └── DebuffImage (Image - BindWidget)
│   ├── TimerText (TextBlock - BindWidget)
│   ├── DurationBar (ProgressBar - BindWidgetOptional)
│   └── StackText (TextBlock - BindWidgetOptional)

Animations:
├── PulseAnimation: Scale 1.0 → 1.15 → 1.0 (loop)
├── FadeInAnimation: Opacity 0 → 1, Scale 0.5 → 1.0
└── FadeOutAnimation: Opacity 1 → 0, Scale 1.0 → 0.8
```

---

## Phase 4.2: W_DebuffContainer Widget

### 4.2.1 Class Definition (C++)

**File:** `Source/UI/Public/SuspenseCore/Widgets/HUD/W_DebuffContainer.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Widgets/SuspenseCoreUserWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventTypes.h"
#include "GameplayTagContainer.h"
#include "W_DebuffContainer.generated.h"

class UHorizontalBox;
class UW_DebuffIcon;
class USuspenseCoreEventBus;

/**
 * Container widget for procedural debuff icon management
 *
 * FEATURES:
 * - EventBus subscription for DoT events
 * - Object pooling for icon widgets
 * - Automatic add/remove/update of debuff icons
 * - Support for multiple debuff types
 *
 * USAGE:
 * 1. Add to Master HUD Widget
 * 2. Widget auto-subscribes to EventBus on NativeConstruct
 * 3. Icons appear/disappear as DoT effects are applied/removed
 *
 * PERFORMANCE:
 * - Widget pooling prevents GC churn
 * - Batched updates via timer
 * - Lazy icon texture loading
 */
UCLASS()
class UI_API UW_DebuffContainer : public USuspenseCoreUserWidget
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════
    // UI COMPONENTS
    // ═══════════════════════════════════════════════════════════════

    /** Container for debuff icons */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UHorizontalBox> DebuffBox;

    // ═══════════════════════════════════════════════════════════════
    // CONFIGURATION
    // ═══════════════════════════════════════════════════════════════

    /** Widget class for debuff icons */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff")
    TSubclassOf<UW_DebuffIcon> DebuffIconClass;

    /** Maximum number of visible debuff icons */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff", meta = (ClampMin = "1", ClampMax = "20"))
    int32 MaxVisibleDebuffs = 10;

    /** Pool size for widget recycling */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff", meta = (ClampMin = "1"))
    int32 IconPoolSize = 15;

    /** Update interval for duration timers (seconds) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff", meta = (ClampMin = "0.05"))
    float UpdateInterval = 0.1f;

    // ═══════════════════════════════════════════════════════════════
    // PUBLIC API
    // ═══════════════════════════════════════════════════════════════

    /**
     * Manually refresh all debuff icons from DoTService
     * Useful for initialization or sync after disconnect
     */
    UFUNCTION(BlueprintCallable, Category = "Debuff")
    void RefreshFromDoTService();

    /**
     * Clear all active debuff icons
     */
    UFUNCTION(BlueprintCallable, Category = "Debuff")
    void ClearAllDebuffs();

    /**
     * Set target actor to display debuffs for (default: local player)
     */
    UFUNCTION(BlueprintCallable, Category = "Debuff")
    void SetTargetActor(AActor* NewTarget);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // ═══════════════════════════════════════════════════════════════
    // EVENTBUS HANDLERS
    // ═══════════════════════════════════════════════════════════════

    /** Called when DoT applied event received */
    UFUNCTION()
    void OnDoTApplied(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    /** Called when DoT removed event received */
    UFUNCTION()
    void OnDoTRemoved(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    /** Called when DoT tick event received (for damage numbers) */
    UFUNCTION()
    void OnDoTTick(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    // ═══════════════════════════════════════════════════════════════
    // ICON MANAGEMENT
    // ═══════════════════════════════════════════════════════════════

    /**
     * Add or update debuff icon
     * @param DoTType Debuff type tag
     * @param Duration Duration (-1 for infinite)
     * @param StackCount Stack count
     */
    void AddOrUpdateDebuff(FGameplayTag DoTType, float Duration, int32 StackCount);

    /**
     * Remove debuff icon (with animation)
     * @param DoTType Debuff type to remove
     */
    void RemoveDebuff(FGameplayTag DoTType);

    /**
     * Get icon from pool or create new
     */
    UW_DebuffIcon* AcquireIcon();

    /**
     * Return icon to pool
     */
    void ReleaseIcon(UW_DebuffIcon* Icon);

    /**
     * Initialize widget pool
     */
    void InitializePool();

    /**
     * Subscribe to EventBus events
     */
    void SubscribeToEvents();

    /**
     * Unsubscribe from EventBus events
     */
    void UnsubscribeFromEvents();

    /**
     * Update all active icon timers
     */
    void UpdateTimers(float DeltaTime);

    /**
     * Check if event is for our target actor
     */
    bool IsEventForTarget(const FSuspenseCoreEventData& EventData) const;

private:
    /** Map of active debuffs: DoTType → Icon */
    UPROPERTY()
    TMap<FGameplayTag, TObjectPtr<UW_DebuffIcon>> ActiveDebuffs;

    /** Pool of available icon widgets */
    UPROPERTY()
    TArray<TObjectPtr<UW_DebuffIcon>> IconPool;

    /** EventBus subscription handles */
    FSuspenseCoreSubscriptionHandle DoTAppliedHandle;
    FSuspenseCoreSubscriptionHandle DoTRemovedHandle;
    FSuspenseCoreSubscriptionHandle DoTTickHandle;

    /** Target actor to display debuffs for */
    UPROPERTY()
    TWeakObjectPtr<AActor> TargetActor;

    /** Cached EventBus reference */
    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

    /** Timer accumulator for periodic updates */
    float UpdateTimer = 0.0f;
};
```

### 4.2.2 EventBus Subscription Pattern

```cpp
void UW_DebuffContainer::SubscribeToEvents()
{
    // Get EventBus from EventManager (project standard pattern)
    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (!Manager)
    {
        return;
    }

    EventBus = Manager->GetEventBus();
    if (!EventBus.IsValid())
    {
        return;
    }

    // Subscribe to DoT events
    DoTAppliedHandle = EventBus->Subscribe(
        SuspenseCoreTags::Event::DoT::Applied,
        FOnSuspenseCoreEvent::CreateUObject(this, &UW_DebuffContainer::OnDoTApplied)
    );

    DoTRemovedHandle = EventBus->Subscribe(
        SuspenseCoreTags::Event::DoT::Removed,
        FOnSuspenseCoreEvent::CreateUObject(this, &UW_DebuffContainer::OnDoTRemoved)
    );

    DoTTickHandle = EventBus->Subscribe(
        SuspenseCoreTags::Event::DoT::Tick,
        FOnSuspenseCoreEvent::CreateUObject(this, &UW_DebuffContainer::OnDoTTick)
    );
}
```

### 4.2.3 Blueprint Setup

**Blueprint:** `WBP_DebuffContainer` (derives from W_DebuffContainer)

```
Widget Hierarchy:
├── [Root] SizeBox (constrain height)
│   └── DebuffBox (HorizontalBox - BindWidget)
│       └── [Dynamic children added at runtime]

Default Values:
├── DebuffIconClass: WBP_DebuffIcon
├── MaxVisibleDebuffs: 10
├── IconPoolSize: 15
└── UpdateInterval: 0.1
```

---

## Phase 4.3: Master HUD Integration

### 4.3.1 Placement in Master HUD

```
┌─────────────────────────────────────────────────────────────────┐
│ MASTER HUD (W_MasterHUD)                                        │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────┐  ┌───────────────────────┐   │
│  │ TopLeftAnchor                │  │ TopRightAnchor        │   │
│  │ ├── W_DebuffContainer  ← NEW │  │ └── W_Minimap         │   │
│  │ └── W_HealthStamina          │  │                       │   │
│  └──────────────────────────────┘  └───────────────────────┘   │
│                                                                  │
│                    ┌─────────────┐                               │
│                    │ W_Crosshair │                               │
│                    └─────────────┘                               │
│                                                                  │
│  ┌──────────────────────────────┐  ┌───────────────────────┐   │
│  │ BottomLeftAnchor             │  │ BottomRightAnchor     │   │
│  │ └── W_Inventory              │  │ └── W_AmmoCounter     │   │
│  └──────────────────────────────┘  └───────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 4.3.2 HUD Controller Changes

```cpp
// In W_MasterHUD.h
UPROPERTY(meta = (BindWidget))
TObjectPtr<UW_DebuffContainer> DebuffContainer;

// In W_MasterHUD.cpp NativeConstruct()
if (DebuffContainer)
{
    // Set target to local player
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            DebuffContainer->SetTargetActor(Pawn);
        }
    }
}
```

---

## Phase 4.4: Icon Assets

### 4.4.1 Required Textures

| Debuff Type | File | Description |
|-------------|------|-------------|
| State.Health.Bleeding.Light | `T_Debuff_BleedLight.png` | Small blood drop, red |
| State.Health.Bleeding.Heavy | `T_Debuff_BleedHeavy.png` | Large blood drop with pulse, dark red |
| Effect.DoT.Burn | `T_Debuff_Burning.png` | Flame icon, orange |
| (Future) Poison | `T_Debuff_Poison.png` | Skull/vial, green |
| (Future) Fracture | `T_Debuff_Fracture.png` | Broken bone, white |

### 4.4.2 Icon Specifications

- **Size:** 64x64 pixels
- **Format:** PNG with alpha
- **Style:** Flat/minimal, high contrast
- **Colors:**
  - Bleeding: `#FF3333` (light), `#CC0000` (heavy)
  - Burning: `#FF6600`
  - Background: Semi-transparent black

---

## Deliverables Checklist

### Phase 4.1: W_DebuffIcon
- [ ] Create `W_DebuffIcon.h`
- [ ] Create `W_DebuffIcon.cpp`
- [ ] Create `WBP_DebuffIcon` Blueprint
- [ ] Add animations (Pulse, FadeIn, FadeOut)
- [ ] Test individual icon functionality

### Phase 4.2: W_DebuffContainer
- [ ] Create `W_DebuffContainer.h`
- [ ] Create `W_DebuffContainer.cpp`
- [ ] Create `WBP_DebuffContainer` Blueprint
- [ ] Implement EventBus subscription
- [ ] Implement widget pooling
- [ ] Test procedural loading

### Phase 4.3: Master HUD Integration
- [ ] Add DebuffContainer to `WBP_MasterHUD`
- [ ] Configure positioning
- [ ] Test with local player targeting

### Phase 4.4: Icon Assets
- [ ] Create `T_Debuff_BleedLight.png`
- [ ] Create `T_Debuff_BleedHeavy.png`
- [ ] Create `T_Debuff_Burning.png`
- [ ] Configure soft references in DebuffIconClass

---

## Testing Plan

### Unit Tests
1. Icon displays correct texture for each debuff type
2. Timer updates correctly (infinite shows ∞)
3. Duration bar hides for infinite effects
4. Stack count displays when > 1

### Integration Tests
1. Apply bleeding via grenade → icon appears
2. Heal bleeding → icon disappears with animation
3. Apply burning → icon appears with timer
4. Burning expires → icon auto-removes
5. Multiple debuffs → all show correctly

### Performance Tests
1. Widget pooling prevents allocation spikes
2. 10+ debuffs don't impact frame rate
3. EventBus subscription doesn't leak

---

## Future Considerations

1. **Buff System:** Same architecture can support positive effects (healing, speed boost)
2. **Enemy Debuffs:** Target switching for viewing enemy status
3. **Tooltip:** Hover to see detailed effect info
4. **Sound Feedback:** Audio cue on debuff application/removal
5. **Screen Effects:** Vignette overlay for critical debuffs

---

**Document End**
